.global _start


_start:
li      t0, 42          // addi x4, x0, 42

call    some_function   // jal x1, some_function

loop:
j       loop            // jal x0, loop


some_function:
li      t1, 66          // addi x5, x0, 66
ret                     // jr x1 == jalr x0, x1, x0
