AS = /usr/riscv/bin/riscv64-unknown-elf-gcc
LD = /usr/riscv/bin/riscv64-unknown-elf-ld
RM = rm -f

OBJECTS = $(patsubst %.S,%.o,$(wildcard *.S))

kernel: $(OBJECTS)
	$(LD) -T link.ld $^ -o $@

%.o: %.S
	$(AS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
