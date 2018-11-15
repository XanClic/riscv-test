#include <assert.h>
#include <config.h>
#include <platform.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <virtio.h>
#include <virtio-gpu.h>


#define QUEUE_SIZE 4

// Chosen by virtio-gpu
#define CURSOR_W 64
#define CURSOR_H 64

enum {
    RESOURCE_FB = 1,
    RESOURCE_CURSOR,
};


static int fb_width, fb_height;

static _Alignas(4096) uint8_t vq_storage[VirtQTotalSize(QUEUE_SIZE)];
static VirtQ vq;

static _Alignas(4096) uint8_t cursor_vq_storage[VirtQTotalSize(QUEUE_SIZE)];
static VirtQ cursor_vq;

static _Alignas(16) union VirtIOGPUCommand gpu_command;
static _Alignas(16) union VirtIOGPUCommand gpu_command2;
static _Alignas(16) union VirtIOGPUResponse gpu_response;

static _Alignas(16) struct VirtIOGPUCursorCommand cursor_commands[QUEUE_SIZE];

static uint32_t *framebuffer;


static struct VirtIOGPUDisplayInfo *get_display_info(void);
static uint32_t *setup_framebuffer(int scanout, int res_id,
                                   int width, int height);
static void flush_framebuffer(int x, int y, int width, int height);
static size_t framebuffer_stride(void);
static uint32_t *get_framebuffer(void);
static int get_framebuffer_width(void);
static int get_framebuffer_height(void);

static bool setup_cursor(uint32_t *data, int width, int height,
                         int hot_x, int hot_y);
static void move_cursor(int x, int y);

static bool need_cursor_updates(void);

void init_virtio_gpu(struct VirtIOControlRegs *regs)
{
    if (platform_funcs.framebuffer) {
        puts("[virtio-gpu] Ignoring further framebuffer device");
        return;
    }

    printf("[virtio-gpu] Found device @%p\n", (void *)regs);

    uint64_t features = FF_ANY_LAYOUT | FF_VERSION_1;
    int ret = virtio_basic_negotiate(regs, &features);
    if (ret < 0) {
        puts("[virtio-gpu] FATAL: Failed to negotiate device features");
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

    if (!vq_init(&cursor_vq, 1, &cursor_vq_storage, QUEUE_SIZE, regs)) {
        puts("[virtio-gpu] FATAL: initializing cursor vq failed");
        return;
    }

    regs->status |= DEV_STATUS_DRIVER_OK;
    __sync_synchronize();

    for (int i = 0; i < QUEUE_SIZE; i++) {
        vq_push_descriptor(&cursor_vq, &cursor_commands[i],
                           sizeof(struct VirtIOGPUCursorCommand),
                           false, true, true);
    }
    // Those descriptors are not full, so reset avail_i
    cursor_vq.avail_i = 0;


    struct VirtIOGPUDisplayInfo *di = get_display_info();
    if (!di) {
        puts("[virtio-gpu] FATAL: Failed to get display info");
        return;
    }

    for (int i = 0; i < (int)regs->gpu.num_scanouts; i++) {
        printf("[virtio-gpu] Scanout %i%s: %ix%i:%ix%i\n",
                i, i ? " (unsupported)" : "",
                di->pmodes[i].r.x, di->pmodes[i].r.y,
                di->pmodes[i].r.width, di->pmodes[i].r.height);
    }


    framebuffer = setup_framebuffer(0, RESOURCE_FB, di->pmodes[0].r.width,
                                    di->pmodes[0].r.height);
    if (!framebuffer) {
        puts("[virtio-gpu] FATAL: Failed setting up framebuffer");
    }

    fb_width = di->pmodes[0].r.width;
    fb_height = di->pmodes[0].r.height;


    printf("[virtio-gpu] Framebuffer set up @%p\n", (void *)framebuffer);


    platform_funcs.framebuffer = get_framebuffer;
    platform_funcs.fb_width = get_framebuffer_width;
    platform_funcs.fb_height = get_framebuffer_height;
    platform_funcs.fb_stride = framebuffer_stride;
    platform_funcs.fb_flush = flush_framebuffer;

    platform_funcs.setup_cursor = setup_cursor;
    platform_funcs.move_cursor = move_cursor;
    platform_funcs.need_cursor_updates = need_cursor_updates;
}


static bool need_cursor_updates(void)
{
    return !platform_funcs.has_absolute_pointing_device ||
        !platform_funcs.has_absolute_pointing_device();
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
    vq_wait_settled(&vq);
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
    vq_wait_settled(&vq);

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
    vq_wait_settled(&vq);

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
    vq_wait_settled(&vq);

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


#define CALC_STRIDE(width, bpp) \
    ((((width) * (bpp) + 31) / 32) * sizeof(uint32_t))

static size_t calc_stride(int width, int bpp)
{
    return CALC_STRIDE(width, bpp);
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
    uint32_t *fb = memalign(PAGESIZE, height * stride);
    if (!resource_attach_backing(res_id, (uintptr_t)fb, height * stride)) {
        return NULL;
    }

    return fb;
}


static void flush_framebuffer(int x, int y, int width, int height)
{
    if (width <= 0) {
        width = fb_width;
    }
    if (height <= 0) {
        height = fb_height;
    }

    vq_wait_settled(&vq);

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
        .resource_id = RESOURCE_FB,
    };

    gpu_command2.res_flush = (struct VirtIOGPUResourceFlush){
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
        },
        .r = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        },
        .resource_id = RESOURCE_FB,
    };

    vq_push_descriptor(&vq, &gpu_command, sizeof(gpu_command),
                       false, true, false);

    vq_push_descriptor(&vq, &gpu_response, sizeof(gpu_response),
                       true, false, true);

    vq_push_descriptor(&vq, &gpu_command2, sizeof(gpu_command2),
                       false, true, false);

    // We don't care about the result anyway, might as well just
    // overwrite the previous one
    vq_push_descriptor(&vq, &gpu_response, sizeof(gpu_response),
                       true, false, true);

    vq_exec(&vq);
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


static bool setup_cursor(uint32_t *data, int width, int height,
                         int hot_x, int hot_y)
{
    static bool already_defined;
    if (already_defined) {
        return false;
    }
    already_defined = true;

    if (width > CURSOR_W || height > CURSOR_H) {
        return false;
    }

    if (!create_2d_resource(RESOURCE_CURSOR, VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
                            CURSOR_W, CURSOR_H))
    {
        return false;
    }

    size_t stride = CALC_STRIDE(CURSOR_W, 32);
    static uint32_t cursor_data[(CALC_STRIDE(CURSOR_W, 32) * CURSOR_H) /
                                sizeof(uint32_t)];
    for (int y = 0; y < height; y++) {
        memcpy(cursor_data + y * stride / sizeof(uint32_t), data + y * width,
               width * sizeof(uint32_t));
    }

    if (!resource_attach_backing(RESOURCE_CURSOR, (uintptr_t)cursor_data,
                                 height * stride))
    {
        return false;
    }

    vq_wait_settled(&vq);

    gpu_command.transfer_to_host_2d = (struct VirtIOGPUTransferToHost2D){
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
        },
        .r = {
            .width = CURSOR_W,
            .height = CURSOR_H,
        },
        .resource_id = RESOURCE_CURSOR,
    };

    if (!exec_command()) {
        return false;
    }

    vq_wait_settled(&cursor_vq);

    int desc_i = cursor_vq.avail_i++ % QUEUE_SIZE;
    cursor_commands[desc_i] = (struct VirtIOGPUCursorCommand){
        .hdr = {
            .type = VIRTIO_GPU_CMD_UPDATE_CURSOR,
        },
        .pos = {
            .scanout_id = 0,
        },
        .resource_id = RESOURCE_CURSOR,
        .hot_x = hot_x,
        .hot_y = hot_y,
    };

    vq_exec(&cursor_vq);

    return true;
}


static void move_cursor(int x, int y)
{
    vq_wait_settled(&cursor_vq);

    int desc_i = cursor_vq.avail_i++ % QUEUE_SIZE;
    cursor_commands[desc_i] = (struct VirtIOGPUCursorCommand){
        .hdr = {
            .type = VIRTIO_GPU_CMD_MOVE_CURSOR,
        },
        .pos = {
            .scanout_id = 0,
            .x = x,
            .y = y,
        },
        // I leave it up to you whether the "spec" (the documentation
        // in the reference header) or qemu's implementation is buggy,
        // but the latter definitely requires this ID even when just
        // moving the cursor
        .resource_id = RESOURCE_CURSOR,
    };

    vq_exec(&cursor_vq);
}
