#include <assert.h>
#include <nonstddef.h>
#include <platform.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <virtio.h>
#include <virtio-input.h>


enum Device {
    KEYBOARD,
    MOUSE,
    TABLET,

    DEVICE_COUNT
};

static const char *const dev_name[DEVICE_COUNT] = {
    [KEYBOARD]  = "keyboard",
    [MOUSE]     = "mouse",
    [TABLET]    = "tablet",
};


#define QUEUE_SIZE 8


static _Alignas(4096) struct {
    _Alignas(4096) uint8_t vq_storage[VirtQTotalSize(QUEUE_SIZE)];
    _Alignas(16) struct VirtIOInputEvent evt[QUEUE_SIZE];
    VirtQ vq;
} devs[DEVICE_COUNT];

static int pointing_x, pointing_y;
static int screen_w, screen_h;
static int tablet_min[2], tablet_max[2];


static void init_device(struct VirtIOControlRegs *regs, enum Device dev);

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
    printf("[virtio-input] Found device @%p\n", (void *)regs);

    uint64_t features = FF_ANY_LAYOUT | FF_VERSION_1;
    int ret = virtio_basic_negotiate(regs, &features);
    if (ret < 0) {
        puts("[virtio-input] FATAL: Failed to negotiate device features");
        return;
    }

    select_config(regs, VIRTIO_INPUT_CFG_ID_NAME, 0);

    printf("[virtio-input] %s", regs->input.string);

    select_config(regs, VIRTIO_INPUT_CFG_EV_BITS, VIRTIO_INPUT_CESS_KEY);

    int keys = 0, axes = 0;
    bool mouse_axes = false, tablet_axes = false;

    if (regs->input.select) {
        for (int i = 0; i < regs->input.size * 8; i++) {
            if (regs->input.bitmap[i / 8] & (1 << (i % 8))) {
                keys++;
            }
        }
        printf(", %i keys", keys);
    }

    select_config(regs, VIRTIO_INPUT_CFG_EV_BITS, VIRTIO_INPUT_CESS_REL);

    if (regs->input.select) {
        for (int i = 0; i < regs->input.size * 8; i++) {
            if (regs->input.bitmap[i / 8] & (1 << (i % 8))) {
                axes++;
            }
        }
        printf(", %i rel. axes", axes);

        if (axes) {
            mouse_axes = (regs->input.bitmap[0] & 3) == 3;
        }
    }

    select_config(regs, VIRTIO_INPUT_CFG_EV_BITS, VIRTIO_INPUT_CESS_ABS);

    if (regs->input.select) {
        for (int i = 0; i < regs->input.size * 8; i++) {
            if (regs->input.bitmap[i / 8] & (1 << (i % 8))) {
                axes++;
            }
        }
        printf(", %i abs. axes", axes);

        if (axes) {
            tablet_axes = (regs->input.bitmap[0] & 3) == 3;
        }
    }

    putchar('\n');


    if (!axes && keys >= 80) {
        init_device(regs, KEYBOARD);
    } else if (mouse_axes) {
        init_device(regs, MOUSE);
    } else if (tablet_axes) {
        init_device(regs, TABLET);
    } else {
        puts("[virtio-input] Ignoring this unrecognized device");
    }
}


static void limit_pointing_device(int w, int h);
static bool get_keyboard_event(int *key, bool *up);
static bool get_pointing_event(int *x, int *y, bool *has_button, int *button,
                               bool *button_up);
static bool has_absolute_pointing_device(void);

static void init_device(struct VirtIOControlRegs *regs, enum Device dev)
{
    if (devs[dev].vq.queue_size) {
        printf("[virtio-input] Ignoring additional %s\n", dev_name[dev]);
        goto fail;
    }

    if (dev == MOUSE && devs[TABLET].vq.queue_size) {
        printf("[virtio-input] Already have a tablet, ignoring this mouse\n");
        goto fail;
    }

    if (dev == TABLET) {
        for (int axis = 0; axis < 2; axis++) {
            select_config(regs, VIRTIO_INPUT_CFG_ABS_INFO, axis);
            if (!regs->input.select) {
                printf("[virtio-input] FATAL: Failed to get %c axis info\n",
                       axis ? 'Y' : 'X');
                goto fail;
            }

            tablet_min[axis] = regs->input.abs.min;
            tablet_max[axis] = regs->input.abs.max;
        }
    }

    select_config(regs, VIRTIO_INPUT_CFG_UNSET, 0);

    if (!vq_init(&devs[dev].vq, 0, &devs[dev].vq_storage, QUEUE_SIZE, regs)) {
        printf("[virtio-input] FATAL: Failed to initialize %s vq\n",
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
            puts("[virtio-input] Offering pointing device");
            platform_funcs.limit_pointing_device = limit_pointing_device;
            platform_funcs.get_pointing_event = get_pointing_event;
            platform_funcs.has_absolute_pointing_device =
                has_absolute_pointing_device;
            break;

        case TABLET:
            puts("[virtio-input] Offering pointing device");
            platform_funcs.limit_pointing_device = limit_pointing_device;
            platform_funcs.get_pointing_event = get_pointing_event;
            platform_funcs.has_absolute_pointing_device =
                has_absolute_pointing_device;
            break;

        default:
            assert(0);
    }

    vq_exec(&devs[dev].vq);
    return;

fail:
    devs[dev].vq.queue_size = 0;
}


static void limit_pointing_device(int w, int h)
{
    screen_w = w;
    screen_h = h;

    pointing_x = w / 2;
    pointing_y = h / 2;
}


static bool has_absolute_pointing_device(void)
{
    return devs[TABLET].vq.queue_size;
}


static int saturated_add(int x, int y, int min, int max)
{
    // Warning: Relies on undefined overflow behavior.
    x += y;
    return MIN(MAX(x, min), max);
}


static bool get_device_event(enum Device dev, struct VirtIOInputEvent *evt)
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


static bool get_pointing_event(int *x, int *y, bool *has_button, int *button,
                               bool *button_up)
{
    struct VirtIOInputEvent evt;
    enum Device dev;

    if (devs[TABLET].vq.queue_size) {
        dev = TABLET;
    } else {
        dev = MOUSE;
    }

    if (!get_device_event(dev, &evt)) {
        return false;
    }

    *has_button = false;
    *button = 0;
    *button_up = false;

    if (evt.type == VIRTIO_INPUT_CESS_KEY) {
        *has_button = true;
        *button = evt.code;
        *button_up = !evt.value;
    } else if (dev == TABLET && evt.type == VIRTIO_INPUT_CESS_ABS) {
        if (evt.code == 0) {
            unsigned range = tablet_max[0] - tablet_min[0];
            pointing_x = (screen_w * (evt.value - tablet_min[0]) + range / 2)
                         / range;
        } else if (evt.code == 1) {
            unsigned range = tablet_max[1] - tablet_min[1];
            pointing_y = (screen_h * (evt.value - tablet_min[1]) + range / 2)
                         / range;
        } else {
            return false;
        }
    } else if (dev == MOUSE && evt.type == VIRTIO_INPUT_CESS_REL) {
        if (evt.code == 0) {
            pointing_x = saturated_add(pointing_x, (int32_t)evt.value,
                                       0, screen_w);
        } else if (evt.code == 1) {
            pointing_y = saturated_add(pointing_y, (int32_t)evt.value,
                                       0, screen_h);
        } else {
            return false;
        }
    } else {
        return false;
    }

    *x = pointing_x;
    *y = pointing_y;

    return true;
}
