#ifndef _VIRTIO_GPU_H
#define _VIRTIO_GPU_H

#include <config.h>
#include <stdint.h>


struct VirtIOGPUConfig {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
} __attribute__((packed));

enum VirtIOGPUCtrlType {
    VIRTIO_GPU_UNDEFINED = 0,

    /* 2d commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,

    /* 3d commands */
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
    VIRTIO_GPU_CMD_SUBMIT_3D,

    /* cursor commands */
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    /* success responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,

    /* error responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

// These are in big endian order; BGRX is qemu's default format.
enum VirtIOGPUFormats {
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,

    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,

    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
};

struct VirtIOGPURect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct VirtIOGPUCtrlHdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed));

struct VirtIOGPUResourceCreate2D {
    struct VirtIOGPUCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct VirtIOGPUMemEntry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed));

struct VirtIOGPUResourceAttachBacking {
    struct VirtIOGPUCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct VirtIOGPUMemEntry entries[];
} __attribute__((packed));

struct VirtIOGPUSetScanout {
    struct VirtIOGPUCtrlHdr hdr;
    struct VirtIOGPURect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed));

struct VirtIOGPUResourceFlush {
    struct VirtIOGPUCtrlHdr hdr;
    struct VirtIOGPURect r;
    uint32_t resource_id;
    uint32_t padding;
};

struct VirtIOGPUTransferToHost2D {
    struct VirtIOGPUCtrlHdr hdr;
    struct VirtIOGPURect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};

#define VIRTIO_GPU_MAX_SCANOUTS 16
struct VirtIOGPUDisplayInfo {
    struct VirtIOGPUCtrlHdr hdr;
    struct {
        struct VirtIOGPURect r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed));

union VirtIOGPUCommand {
    struct VirtIOGPUCtrlHdr hdr;
    struct VirtIOGPUResourceCreate2D res_create_2d;
    struct VirtIOGPUResourceAttachBacking res_attach_backing;
    struct VirtIOGPUSetScanout set_scanout;
    struct VirtIOGPUResourceFlush res_flush;
    struct VirtIOGPUTransferToHost2D transfer_to_host_2d;

    uint8_t padding[PAGESIZE];
};

union VirtIOGPUResponse {
    struct VirtIOGPUCtrlHdr hdr;
    struct VirtIOGPUDisplayInfo display_info;

    uint8_t padding[PAGESIZE];
};

struct VirtIOControlRegs;


void init_virtio_gpu(struct VirtIOControlRegs *regs);

#endif
