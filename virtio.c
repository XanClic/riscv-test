#include <assert.h>
#include <config.h>
#include <kprintf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <virtio.h>
#include <virtio-gpu.h>


#define STR_TO_U32(str) (*(uint32_t *)str)


void init_virtio_device(struct VirtIOControlRegs *regs)
{
    assert(regs->magic == STR_TO_U32("virt"));

    if (regs->version != 1 && regs->version != 2) {
        return;
    }

    if (regs->device_id == DEVID_RESERVED) {
        return;
    }

    switch (regs->device_id) {
        case DEVID_GPU:
            init_virtio_gpu(regs);
            break;
    }
}


int virtio_basic_negotiate(struct VirtIOControlRegs *regs, uint64_t *features)
{
    regs->status = DEV_STATUS_RESET;
    __sync_synchronize();

    regs->status |= DEV_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    regs->status |= DEV_STATUS_DRIVER;
    __sync_synchronize();

    uint64_t offered_features;

    regs->device_features_sel = 0;
    __sync_synchronize();
    offered_features = regs->device_features;
    __sync_synchronize();
    regs->device_features_sel = 1;
    __sync_synchronize();
    offered_features |= (uint64_t)regs->device_features << 32;

    *features &= offered_features;

    regs->device_features_sel = 0;
    __sync_synchronize();
    regs->device_features = (uint32_t)*features;
    __sync_synchronize();
    regs->device_features_sel = 1;
    __sync_synchronize();
    regs->device_features = (uint32_t)(*features >> 32);
    __sync_synchronize();

    if (regs->version < 2 || !(*features & FF_VERSION_1)) {
        regs->legacy_guest_page_size = PAGESIZE;
    } else {
        regs->status |= DEV_STATUS_FEATURES_OK;

        __sync_synchronize();

        if (!(regs->status & DEV_STATUS_FEATURES_OK)) {
            return -1;
        }
    }

    return 0;
}


bool vq_init(VirtQ *vq, int queue_index, void *base, int queue_size,
             volatile struct VirtIOControlRegs *regs)
{
    *vq = (VirtQ){
        .base = base,
        .queue_index = queue_index,
        .queue_size = queue_size,
        .regs = regs,
    };

    uint64_t features;

    regs->device_features_sel = 0;
    features = regs->device_features;
    regs->device_features_sel = 1;
    features |= (uint64_t)regs->device_features << 32;

    bool legacy = regs->version < 2 || !(features & FF_VERSION_1);

    regs->queue_sel = queue_index;

    if (legacy ? regs->legacy_queue_pfn : regs->queue_ready) {
        puts("[virtio] virtqueue is already in use");
        return false;
    }

    if ((int)regs->queue_num_max < queue_size) {
        puts("[virtio] virtqueue max size is too small");
        return false;
    }

    regs->queue_num = queue_size;

    VirtQAvail(1) *avail = (void *)((uintptr_t)base +
                                    sizeof(struct VirtQDesc) * queue_size);
    avail->flags = VQAF_NO_INTERRUPT;

    if (legacy) {
        regs->legacy_queue_align = PAGESIZE;
        regs->legacy_queue_pfn = (uintptr_t)base / PAGESIZE;
    } else {
        uintptr_t vq_desc = (uintptr_t)base;
        regs->queue_desc_lo = (uint32_t)(uintptr_t)&vq_desc;
        regs->queue_desc_lo = (uint32_t)((uintptr_t)&vq_desc >> 32);

        uintptr_t vq_avail = (uintptr_t)avail;
        regs->queue_avail_lo = (uint32_t)(uintptr_t)&vq_avail;
        regs->queue_avail_lo = (uint32_t)((uintptr_t)&vq_avail >> 32);

        uintptr_t vq_used = (uintptr_t)base +
            ROUND_UP(sizeof(struct VirtQDesc) * queue_size +
                     sizeof(VirtQAvail(1)) + sizeof(uint16_t) * queue_size,
                     PAGESIZE);
        regs->queue_used_lo = (uint32_t)(uintptr_t)&vq_used;
        regs->queue_used_lo = (uint32_t)((uintptr_t)&vq_used >> 32);
    }

    return true;
}


void vq_push_descriptor(VirtQ *vq, void *ptr, size_t length,
                        bool write, bool first, bool last)
{
    struct VirtQDesc *vqdesc = vq->base;
    uint16_t next_i = vq->desc_i + 1;

    vqdesc[vq->desc_i % vq->queue_size] = (struct VirtQDesc){
        .addr = (uintptr_t)ptr,
        .len = length,
        .flags = !last ? VQDF_NEXT : 0
               | write ? VQDF_WRITE : 0,
        .next = !last ? next_i % vq->queue_size : 0,
    };

    if (first) {
        uint16_t *avail_ring =
            (void *)((uintptr_t)vq->base +
                     sizeof(struct VirtQDesc) * vq->queue_size +
                     offsetof(VirtQAvail(1), ring));

        avail_ring[vq->avail_i++ % vq->queue_size] = vq->desc_i % vq->queue_size;
    }

    vq->desc_i = next_i;
}


void vq_exec(VirtQ *vq)
{
    __sync_synchronize();

    VirtQAvail(1) *avail = (void *)((uintptr_t)vq->base +
                                    sizeof(struct VirtQDesc) * vq->queue_size);
    avail->idx = vq->avail_i;

    __sync_synchronize();
    __asm__ __volatile__ ("" ::: "memory");

    VirtQUsed(1) *used = (void *)((uintptr_t)vq->base +
            ROUND_UP(sizeof(struct VirtQDesc) * vq->queue_size +
                     sizeof(VirtQAvail(1)) + sizeof(uint16_t) * vq->queue_size,
                     PAGESIZE));

    if (!(used->flags & VQUF_NO_NOTIFY)) {
        vq->regs->queue_notify = vq->queue_index;
    }
}


uint16_t vq_wait_used(VirtQ *vq)
{
    VirtQUsed(1) *used = (void *)((uintptr_t)vq->base +
            ROUND_UP(sizeof(struct VirtQDesc) * vq->queue_size +
                     sizeof(VirtQAvail(1)) + sizeof(uint16_t) * vq->queue_size,
                     PAGESIZE));

    uint16_t next = vq->used_i;

    while (used->idx == vq->used_i) {
        __asm__ __volatile__ ("" ::: "memory");
    }

    while (used->idx != ++vq->used_i);

    __sync_synchronize();

    return next;
}


int vq_single_poll_used(VirtQ *vq)
{
    VirtQUsed(1) *used = (void *)((uintptr_t)vq->base +
            ROUND_UP(sizeof(struct VirtQDesc) * vq->queue_size +
                     sizeof(VirtQAvail(1)) + sizeof(uint16_t) * vq->queue_size,
                     PAGESIZE));

    uint16_t next = vq->used_i;

    if (used->idx == vq->used_i) {
        return -1;
    }

    vq->used_i++;

    __sync_synchronize();

    return next;
}
