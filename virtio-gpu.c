#include <config.h>
#include <kmalloc.h>
#include <kprintf.h>
#include <platform.h>
#include <stdbool.h>
#include <stdint.h>
#include <virtio.h>
#include <virtio-gpu.h>


#define QUEUE_SIZE 4


static int fb_width, fb_height;

static uint8_t vq_storage[VirtQTotalSize(QUEUE_SIZE)];
static VirtQ vq;

static union VirtIOGPUCommand gpu_command __attribute__((aligned(4096)));
static union VirtIOGPUResponse gpu_response __attribute__((aligned(4096)));

static uint32_t *framebuffer;


static int basic_negotiate(struct VirtIOControlRegs *regs);
static struct VirtIOGPUDisplayInfo *get_display_info(void);
static uint32_t *setup_framebuffer(int scanout, int res_id,
                                   int width, int height);
static bool flush_framebuffer(int x, int y, int width, int height);
static size_t framebuffer_stride(void);
static uint32_t *get_framebuffer(void);
static int get_framebuffer_width(void);
static int get_framebuffer_height(void);

void init_virtio_gpu(struct VirtIOControlRegs *regs)
{
    if (platform_funcs()->framebuffer) {
        puts("[virtio-gpu] Ignoring further framebuffer device");
        return;
    }

    kprintf("[virtio-gpu] Found device @%p\n", (void *)regs);

    int ret = basic_negotiate(regs);
    if (ret < 0) {
        return;
    }

    if (regs->gpu.num_scanouts < 1) {
        puts("[virtio-gpu] FATAL: no scanout");
        return;
    }

    if (!vq_init(&vq, 0, &vq_storage, QUEUE_SIZE, regs)) {
        puts("[virtio-gpu] FATAL: initializing ctrl vq failed");
        return;
    }

    regs->status |= DEV_STATUS_DRIVER_OK;


    struct VirtIOGPUDisplayInfo *di = get_display_info();
    if (!di) {
        puts("[virtio-gpu] FATAL: Failed to get display info");
        return;
    }

    for (int i = 0; i < (int)regs->gpu.num_scanouts; i++) {
        kprintf("[virtio-gpu] Scanout %i%s: %ix%i:%ix%i\n",
                i, i ? " (unsupported)" : "",
                di->pmodes[i].r.x, di->pmodes[i].r.y,
                di->pmodes[i].r.width, di->pmodes[i].r.height);
    }


    framebuffer = setup_framebuffer(0, 1, di->pmodes[0].r.width,
                                    di->pmodes[0].r.height);
    if (!framebuffer) {
        puts("[virtio-gpu] FATAL: Failed setting up framebuffer");
    }

    fb_width = di->pmodes[0].r.width;
    fb_height = di->pmodes[0].r.height;


    kprintf("[virtio-gpu] Framebuffer set up @%p\n", (void *)framebuffer);


    platform_funcs()->framebuffer = get_framebuffer;
    platform_funcs()->fb_width = get_framebuffer_width;
    platform_funcs()->fb_height = get_framebuffer_height;
    platform_funcs()->fb_stride = framebuffer_stride;
    platform_funcs()->fb_flush = flush_framebuffer;
}


static int basic_negotiate(struct VirtIOControlRegs *regs)
{
    regs->status = DEV_STATUS_RESET;

    regs->status |= DEV_STATUS_ACKNOWLEDGE;
    regs->status |= DEV_STATUS_DRIVER;

    uint64_t features;

    regs->device_features_sel = 0;
    features = regs->device_features;
    regs->device_features_sel = 1;
    features |= (uint64_t)regs->device_features << 32;

    features &= (FF_ANY_LAYOUT | FF_VERSION_1);

    regs->device_features_sel = 0;
    regs->device_features = (uint32_t)features;
    regs->device_features_sel = 1;
    regs->device_features = (uint32_t)(features >> 32);

    if (regs->version < 2 || !(features & FF_VERSION_1)) {
        regs->legacy_guest_page_size = PAGESIZE;
    } else {
        regs->status |= DEV_STATUS_FEATURES_OK;

        __sync_synchronize();

        if (!(regs->status & DEV_STATUS_FEATURES_OK)) {
            puts("[virtio-gpu] Failed to negotiate device features");
            return -1;
        }
    }

    return 0;
}


static bool exec_command(void)
{
    vq_push_descriptor(&vq, &gpu_command, sizeof(gpu_command),
                       false, true, false);

    vq_push_descriptor(&vq, &gpu_response, sizeof(gpu_response),
                       true, false, true);

    vq_exec(&vq);
    vq_wait_used(&vq);

    return gpu_response.hdr.type >= VIRTIO_GPU_RESP_OK_NODATA &&
           gpu_response.hdr.type <  VIRTIO_GPU_RESP_ERR_UNSPEC;
}


static struct VirtIOGPUDisplayInfo *get_display_info(void)
{
    gpu_command.hdr.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    exec_command();
    if (gpu_response.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        return NULL;
    }

    return &gpu_response.display_info;
}


static bool create_2d_resource(int id, enum VirtIOGPUFormats format,
                               int width, int height)
{
    gpu_command.res_create_2d = (struct VirtIOGPUResourceCreate2D){
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
        },
        .resource_id = id,
        .format = format,
        .width = width,
        .height = height,
    };

    return exec_command();
}


static bool resource_attach_backing(int id, uintptr_t address, size_t length)
{
    gpu_command.res_attach_backing = (struct VirtIOGPUResourceAttachBacking){
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
        },
        .resource_id = id,
        .nr_entries = 1,
    };

    gpu_command.res_attach_backing.entries[0] = (struct VirtIOGPUMemEntry){
        .addr = address,
        .length = length,
    };

    return exec_command();
}


static bool set_scanout(int scanout, int res_id, int width, int height)
{
    gpu_command.set_scanout = (struct VirtIOGPUSetScanout){
        .hdr = {
            .type = VIRTIO_GPU_CMD_SET_SCANOUT,
        },
        .r = {
            .width = width,
            .height = height,
        },
        .scanout_id = scanout,
        .resource_id = res_id,
    };

    return exec_command();
}


static size_t calc_stride(int width, int bpp)
{
    return ((width * bpp + 31) / 32) * sizeof(uint32_t);
}

static size_t framebuffer_stride(void)
{
    return calc_stride(fb_width, 32);
}


static uint32_t *setup_framebuffer(int scanout, int res_id,
                                   int width, int height)
{
    if (!create_2d_resource(res_id, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM,
                             width, height))
    {
        return NULL;
    }

    if (!set_scanout(scanout, res_id, width, height)) {
        return NULL;
    }

    size_t stride = calc_stride(width, 32);
    uint32_t *fb = kmalloc(height * stride);
    if (!resource_attach_backing(res_id, (uintptr_t)fb, height * stride)) {
        return NULL;
    }

    return fb;
}


static bool flush_framebuffer(int x, int y, int width, int height)
{
    if (width <= 0) {
        width = fb_width;
    }
    if (height <= 0) {
        height = fb_height;
    }

    __sync_synchronize();

    gpu_command.transfer_to_host_2d = (struct VirtIOGPUTransferToHost2D){
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
        },
        .r = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        },
        .offset = y * framebuffer_stride() + x * 4,
        .resource_id = 1,
    };

    if (!exec_command()) {
        return false;
    }

    gpu_command.res_flush = (struct VirtIOGPUResourceFlush){
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
        },
        .r = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        },
        .resource_id = 1,
    };

    return exec_command();
}


static uint32_t *get_framebuffer(void)
{
    return framebuffer;
}

static int get_framebuffer_width(void)
{
    return fb_width;
}

static int get_framebuffer_height(void)
{
    return fb_height;
}
