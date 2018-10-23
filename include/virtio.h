#ifndef _VIRTIO_H
#define _VIRTIO_H

#include <config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <virtio-gpu.h>
#include <virtio-input.h>


struct VirtIOControlRegs {
    uint32_t magic;     // "virt"
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t device_features;
    uint32_t device_features_sel;
    uint8_t reserved0[8];
    uint32_t driver_features;
    uint32_t driver_features_sel;
    uint32_t legacy_guest_page_size;
    uint8_t reserved1[4];
    uint32_t queue_sel;
    uint32_t queue_num_max;
    uint32_t queue_num;
    uint32_t legacy_queue_align;
    uint32_t legacy_queue_pfn;
    uint32_t queue_ready;
    uint8_t reserved2[8];
    uint32_t queue_notify;
    uint8_t reserved3[12];
    uint32_t interrupt_status;
    uint32_t interrupt_ack;
    uint8_t reserved4[8];
    uint32_t status;
    uint8_t reserved5[12];
    uint32_t queue_desc_lo, queue_desc_hi;
    uint8_t reserved6[8];
    uint32_t queue_avail_lo, queue_avail_hi;
    uint8_t reserved7[8];
    uint32_t queue_used_lo, queue_used_hi;
    uint8_t reserved8[8];

    uint8_t reserved9[0x40];

    uint8_t reserved10[12];
    uint32_t config_generation;

    union {
        uint8_t padding[0xf00];
        struct VirtIOGPUConfig gpu;
        struct VirtIOInputConfig input;
    };
} __attribute__((packed, aligned(4096)));

enum VirtIODeviceType {
    DEVID_RESERVED              = 0,
    DEVID_NETWORK               = 1,
    DEVID_BLOCK                 = 2,
    DEVID_CONSOLE               = 3,
    DEVID_ENTROPY_SOURCE        = 4,
    DEVID_MEMORY_BALLOON_LEGACY = 5,
    DEVID_IOMEMORY              = 6,
    DEVID_RPMSG                 = 7,
    DEVID_SCSI_HOST             = 8,
    DEVID_9P                    = 9,
    DEVID_80211                 = 10,
    DEVID_RPROC_SERIAL          = 11,
    DEVID_CAIF                  = 12,
    DEVID_MEMORY_BALLOON        = 13,
    DEVID_GPU                   = 16,
    DEVID_TIMER                 = 17,
    DEVID_INPUT                 = 18,
};

enum VirtIODeviceStatus {
    DEV_STATUS_RESET                = 0,

    DEV_STATUS_ACKNOWLEDGE          = 1,
    DEV_STATUS_DRIVER               = 2,
    DEV_STATUS_DRIVER_OK            = 4,
    DEV_STATUS_FEATURES_OK          = 8,
    DEV_STATUS_DEVICE_NEEDS_RESET   = 64,
    DEV_STATUS_FAILED               = 128,
};

enum VirtIOFeatureFlags {
    FF_NOTIFY_ON_EMPTY      = (1ull << 24),
    FF_ANY_LAYOUT           = (1ull << 27),
    FF_RING_INDIRECT_LAYOUT = (1ull << 28),
    FF_RING_EVENT_IDX       = (1ull << 29),
    FF_VERSION_1            = (1ull << 30),
};

enum VirtQDescFlags {
    VQDF_NEXT       = (1 << 0),
    VQDF_WRITE      = (1 << 1),
    VQDF_INDIRECT   = (1 << 2),
};

enum VirtQAvailFlags {
    VQAF_NO_INTERRUPT   = (1 << 0),
};

enum VirtQUsedFlags {
    VQUF_NO_NOTIFY  = (1 << 0),
};

struct VirtQDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed, aligned(16)));

#define VirtQAvail(QUEUE_SIZE) \
    struct { \
        uint16_t flags; \
        uint16_t idx; \
        uint16_t ring[QUEUE_SIZE]; \
        uint16_t used_event; \
    } __attribute__((packed, aligned(2))) \

#define VirtQUsed(QUEUE_SIZE) \
    struct { \
        uint16_t flags; \
        uint16_t idx; \
        struct { \
            uint32_t id; \
            uint32_t len; \
        } __attribute__((packed, aligned(4))) ring[QUEUE_SIZE]; \
        uint16_t avail_event; \
    } __attribute__((packed, aligned(4))) \

#ifndef ROUND_UP
#define ROUND_UP(x, y) (((x) + (y) - 1) & -(y))
#endif

#define VirtQTotalSize(QUEUE_SIZE) \
    (ROUND_UP(sizeof(struct VirtQDesc) * QUEUE_SIZE + \
              sizeof(VirtQAvail(QUEUE_SIZE)), PAGESIZE) + \
     ROUND_UP(sizeof(VirtQUsed(QUEUE_SIZE)), PAGESIZE))

typedef struct VirtQ {
    void *base; // Point to something of VirtQTotalSize(queue_size)
    int queue_index;
    int queue_size;
    int desc_i, avail_i, used_i;
    volatile struct VirtIOControlRegs *regs;
} VirtQ;


void init_virtio_device(struct VirtIOControlRegs *regs);

int virtio_basic_negotiate(struct VirtIOControlRegs *regs, uint64_t *features);

bool vq_init(VirtQ *vq, int queue_index, void *base, int queue_size,
             volatile struct VirtIOControlRegs *regs);
void vq_push_descriptor(VirtQ *vq, void *ptr, size_t length,
                        bool write, bool first, bool last);
void vq_exec(VirtQ *vq);
uint16_t vq_wait_used(VirtQ *vq);
int vq_single_poll_used(VirtQ *vq);
void vq_wait_settled(VirtQ *vq);

#endif
