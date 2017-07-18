.global _start
.global tohost
.global fromhost


.section .text

_start:
la      sp, stack

la      a0, hello_world
call    puts

loop:
j       loop


putchar:
/* just a single character */
la      t0, tohost
andi    a0, a0, 0xff
sw      a0, (t0)
/* device 1 (serial), function 1 (put string) */
li      a0, 0x01010000
sw      a0, 4(t0)

/* acknowledge result */
mv      a0, zero
sw      a0, 8(t0)
sw      a0, 12(t0)
ret


puts:
addi    sp, sp, -16
sd      ra, 8(sp)
sd      s0, 0(sp)

mv      s0, a0

puts_loop:
lb      a0, (s0)
addi    s0, s0, 1
beqz    a0, puts_loop_break

call    putchar

j       puts_loop
puts_loop_break:

ld      s0, 0(sp)
ld      ra, 8(sp)
addi    sp, sp, 16
ret


.section .data

tohost:
.type tohost, "object"
.size tohost, 8
.quad 0

fromhost:
.type fromhost, "object"
.size fromhost, 8
.quad 0


.section .rodata

hello_world:
.string "Hello, RISC-V world!\r\n\0"


.section .bss

.zero 4096
stack:
