AS = riscv64-linux-gnu-gcc
CC = riscv64-linux-gnu-gcc
LD = riscv64-linux-gnu-ld
RM = rm -f

CFLAGS = -ffreestanding -nostdinc -nodefaultlibs -Wall -Wextra -pedantic -Wshadow -std=c11 -O3 -mcmodel=medany -mstrict-align -Iinclude -Ilibogg-1.3.2/include -ITremor -g2 -DUSE_LOCKS=0
ASFLAGS = -ffreestanding -nodefaultlibs -Wall -Wextra

CFLAGS += -DSERIAL_IS_SOUND

OBJECTS = $(patsubst %.S,%.o,$(wildcard *.S)) $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean

all: kernel

kernel: $(OBJECTS)
	$(LD) -T link.ld $^ -o $@

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

libogg-1.3.2/%.o: libogg-1.3.2/%.c
	$(CC) $(CFLAGS) -Wno-shadow -c $< -o $@

Tremor/%.o: Tremor/%.c
	$(CC) $(CFLAGS) -Wno-shadow -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
