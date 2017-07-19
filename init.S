.global _start

.extern main


.section .text

_start:
la      sp, stack

call    main

// Disable interrupts
li      t0, 0x38
csrc    mstatus, t0

hang:
wfi
j       hang


.section .bss

.zero 4096
stack:
