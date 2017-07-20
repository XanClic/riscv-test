AS = /usr/riscv/bin/riscv64-unknown-elf-gcc
CC = /usr/riscv/bin/riscv64-unknown-elf-gcc
LD = /usr/riscv/bin/riscv64-unknown-elf-ld
RM = rm -f

CFLAGS = -ffreestanding -nostdinc -nodefaultlibs -Wall -Wextra -pedantic -Wshadow -std=c11 -O3 -mcmodel=medany -Iinclude
ASFLAGS = -ffreestanding -nodefaultlibs -Wall -Wextra

OBJECTS = $(patsubst %.S,%.o,$(wildcard *.S)) $(patsubst %.c,%.o,$(wildcard *.c))

kernel: $(OBJECTS)
	$(LD) -T link.ld $^ -o $@

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
