#include <assert.h>
#include <kmalloc.h>
#include <kprintf.h>
#include <platform.h>
#include <stdbool.h>
#include <stddef.h>
#include <virtio.h>
#include <virtio-input.h>


enum DEVICE {
    KEYBOARD,
    MOUSE,

    DEVICE_COUNT
};

static const char *const dev_name[DEVICE_COUNT] = {
    [KEYBOARD]  = "keyboard",
    [MOUSE]     = "mouse",
};


#define QUEUE_SIZE 8


static struct {
    uint8_t vq_storage[VirtQTotalSize(QUEUE_SIZE)] \
        __attribute__((aligned(4096)));
    struct VirtIOInputEvent evt[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtQ vq;
} devs[DEVICE_COUNT] __attribute__((aligned(4096)));


static void init_device(struct VirtIOControlRegs *regs, enum DEVICE dev);

static void select_config(struct VirtIOControlRegs *regs,
                          int select, int subsel)
{
    __sync_synchronize();
    regs->input.select = select;
    regs->input.subsel = subsel;
    __sync_synchronize();
}

void init_virtio_input(struct VirtIOControlRegs *regs)
{
    kprintf("[virtio-input] Found device @%p\n", (void *)regs);

    uint64_t features = FF_ANY_LAYOUT | FF_VERSION_1;
    int ret = virtio_basic_negotiate(regs, &features);
    if (ret < 0) {
        puts("[virtio-input] FATAL: Failed to negotiate device features");
        return;
    }

    select_config(regs, VIRTIO_INPUT_CFG_ID_NAME, 0);

    kprintf("[virtio-input] %s", regs->input.string);

    select_config(regs, VIRTIO_INPUT_CFG_EV_BITS, VIRTIO_INPUT_CESS_KEY);

    int keys = 0, axes = 0;
    bool mouse_axes = false;

    if (regs->input.select) {
        for (int i = 0; i < regs->input.size * 8; i++) {
            if (regs->input.bitmap[i / 8] & (1 << (i % 8))) {
                keys++;
            }
        }
        kprintf(", %i keys", keys);
    }

    select_config(regs, VIRTIO_INPUT_CFG_EV_BITS, VIRTIO_INPUT_CESS_REL);

    if (regs->input.select) {
        for (int i = 0; i < regs->input.size * 8; i++) {
            if (regs->input.bitmap[i / 8] & (1 << (i % 8))) {
                axes++;
            }
        }
        kprintf(", %i rel. axes", axes);

        if (axes) {
            mouse_axes = (regs->input.bitmap[0] & 3) == 3;
        }
    }

    putchar('\n');


    if (!axes && keys >= 80) {
        init_device(regs, KEYBOARD);
    } else if (mouse_axes) {
        init_device(regs, MOUSE);
    } else {
        puts("[virtio-input] Ignoring this unrecognized device");
    }
}


static bool get_keyboard_event(int *key, bool *up);
static bool get_mouse_event(int *dx, int *dy, bool *has_button, int *button,
                            bool *button_up);

static void init_device(struct VirtIOControlRegs *regs, enum DEVICE dev)
{
    if (devs[dev].vq.queue_size) {
        kprintf("[virtio-input] Ignoring additional %s\n", dev_name[dev]);
        goto fail;
    }

    if (!vq_init(&devs[dev].vq, 0, &devs[dev].vq_storage, QUEUE_SIZE, regs)) {
        kprintf("[virtio-input] FATAL: Failed to initialize %s vq\n",
                dev_name[dev]);
        goto fail;
    }

    regs->status |= DEV_STATUS_DRIVER_OK;
    __sync_synchronize();

    for (int i = 0; i < QUEUE_SIZE; i++) {
        vq_push_descriptor(&devs[dev].vq, &devs[dev].evt[i],
                           sizeof(struct VirtIOInputEvent), true, true, true);
    }

    switch (dev) {
        case KEYBOARD:
            puts("[virtio-input] Offering keyboard");
            platform_funcs.get_keyboard_event = get_keyboard_event;
            break;

        case MOUSE:
            puts("[virtio-input] Offering mouse");
            platform_funcs.get_mouse_event = get_mouse_event;
            break;

        default:
            assert(0);
    }

    vq_exec(&devs[dev].vq);
    return;

fail:
    devs[dev].vq.queue_size = 0;
}


static bool get_device_event(enum DEVICE dev, struct VirtIOInputEvent *evt)
{
    int ret = vq_single_poll_used(&devs[dev].vq);
    if (ret < 0) {
        return false;
    }

    *evt = devs[dev].evt[ret % QUEUE_SIZE];

    devs[dev].vq.avail_i++;
    vq_exec(&devs[dev].vq);

    return true;
}


static bool get_keyboard_event(int *key, bool *up)
{
    struct VirtIOInputEvent evt;

    if (!get_device_event(KEYBOARD, &evt)) {
        return false;
    }

    if (evt.type != VIRTIO_INPUT_CESS_KEY) {
        return false;
    }

    *key = evt.code;
    *up = !evt.value;

    return true;
}


static bool get_mouse_event(int *dx, int *dy, bool *has_button, int *button,
                            bool *button_up)
{
    struct VirtIOInputEvent evt;

    if (!get_device_event(MOUSE, &evt)) {
        return false;
    }

    *dx = 0;
    *dy = 0;
    *has_button = false;
    *button = 0;
    *button_up = false;

    switch (evt.type) {
        case VIRTIO_INPUT_CESS_KEY:
            *has_button = true;
            *button = evt.code;
            *button_up = !evt.value;
            break;

        case VIRTIO_INPUT_CESS_REL:
            if (evt.code == 0) {
                *dx = (int32_t)evt.value;
            } else if (evt.code == 1) {
                *dy = (int32_t)evt.value;
            } else {
                return false;
            }
            break;

        default:
            return false;
    }

    return true;
}
