#!/bin/sh
if [ -z "$QEMU" ]; then
    echo '$QEMU environment variable not set!' >&2
    echo '(must point to qemu-system-riscv64)' 2>&1
    exit 1
fi

make clean || exit 1
NOSOUND=1 make -j 4 || exit 1

$QEMU \
    -kernel kernel -serial stdio -M virt \
    -device virtio-gpu-device,xres=1600,yres=900 \
    -device virtio-keyboard-device \
    -device virtio-tablet-device \
    $@
