#!/bin/sh
$QEMU \
    -kernel kernel -serial stdio -M virt \
    -device virtio-gpu-device,xres=1600,yres=900 \
    -device virtio-keyboard-device \
    -device virtio-mouse-device \
    $@
