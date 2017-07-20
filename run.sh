#!/bin/sh
../qemu-riscv/build/riscv64-softmmu/qemu-system-riscv64 -kernel kernel -serial stdio $@
