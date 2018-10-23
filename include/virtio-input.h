#ifndef _VIRTIO_INPUT_H
#define _VIRTIO_INPUT_H

#include <stdint.h>


struct VirtIOInputAbsInfo {
    uint32_t min;
    uint32_t max;
    uint32_t fuzz;
    uint32_t flat;
    uint32_t res;
} __attribute__((packed));

struct VirtIOInputDevIDs {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
} __attribute__((packed));

struct VirtIOInputConfig {
    uint8_t select;
    uint8_t subsel;
    uint8_t size;
    uint8_t reserved[5];

    union {
        char string[128];
        uint8_t bitmap[128];
        struct VirtIOInputAbsInfo abs;
        struct VirtIOInputDevIDs ids;
    };
} __attribute__((packed));


enum VirtIOInputConfigSelect {
    VIRTIO_INPUT_CFG_UNSET      = 0x00,
    VIRTIO_INPUT_CFG_ID_NAME    = 0x01,
    VIRTIO_INPUT_CFG_ID_SERIAL  = 0x02,
    VIRTIO_INPUT_CFG_ID_DEVIDS  = 0x03,
    VIRTIO_INPUT_CFG_PROP_BITS  = 0x10,
    VIRTIO_INPUT_CFG_EV_BITS    = 0x11,
    VIRTIO_INPUT_CFG_ABS_INFO   = 0x12,
};

enum VirtIOInputConfigEvSubSel {
    VIRTIO_INPUT_CESS_KEY   = 0x01,
    VIRTIO_INPUT_CESS_REL   = 0x02,
};

struct VirtIOInputEvent {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed, aligned(16)));


void init_virtio_input(struct VirtIOControlRegs *regs);

#endif
