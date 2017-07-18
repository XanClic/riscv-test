.global _start

.extern main


.section .text

_start:
la      sp, stack

call    main

hang:
j       hang


.section .bss

.zero 4096
stack:
