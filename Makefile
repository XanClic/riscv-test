AS = riscv64-linux-gnu-gcc
CC = riscv64-linux-gnu-gcc
LD = riscv64-linux-gnu-ld
RM = rm -f

CFLAGS = -ffreestanding -nostdinc -nodefaultlibs -Wall -Wextra -pedantic -Wshadow -std=c11 -O3 -mcmodel=medany -mstrict-align -Iinclude -g2
ASFLAGS = -ffreestanding -nodefaultlibs -Wall -Wextra

# CFLAGS += -DSERIAL_IS_SOUND

OBJECTS = $(patsubst %.S,%.o,$(wildcard *.S)) $(patsubst %.c,%.o,$(wildcard *.c))

kernel: $(OBJECTS)
	$(LD) -T link.ld $^ -o $@

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
