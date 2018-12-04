AS = riscv64-linux-gnu-gcc
CC = riscv64-linux-gnu-gcc
LD = riscv64-linux-gnu-ld
OBJCP = riscv64-linux-gnu-objcopy
RM = rm -f

CFLAGS = -ffreestanding -nostdinc -nodefaultlibs -Wall -Wextra -pedantic -Wshadow -Werror -std=c11 -O3 -mcmodel=medany -mstrict-align -Iinclude -Ilibogg-1.3.2/include -ITremor -Izlib-1.2.11 -Ilibpng-1.6.35 -g2 -DUSE_LOCKS=0
ASFLAGS = -ffreestanding -nodefaultlibs -Wall -Wextra

ifneq ($(NOSOUND),1)
	# Signify that the serial output is fed to something that plays
	# RIFF WAVE with little delay so that we can actually use sound
	CFLAGS += -DSERIAL_IS_SOUND
endif

# Resample all sound from 16 to 8 bit
# CFLAGS += -DSAMPLE_8BIT

OBJECTS = $(patsubst %.S,%.o,$(shell find -name '*.S')) \
          $(patsubst %.c,%.o,$(shell find -name '*.c')) \
          $(patsubst %.npf,%-npf.o,$(wildcard assets/*.npf)) \
          $(patsubst %.ogg,%-ogg.o,$(wildcard assets/*.ogg)) \
          $(patsubst %.png,%-png.o,$(wildcard assets/*.png))

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

zlib-1.2.11/%.o: zlib-1.2.11/%.c
	$(CC) $(CFLAGS) -Wno-implicit-fallthrough -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%-npf.o: %.npf
	$(OBJCP) -I binary -O elf64-little -B riscv $< $@
	@echo Setting RISC-V architecture:
	echo -ne '\xf3' | dd of=$@ bs=1 seek=18 conv=notrunc status=none
	@echo Setting machine flags:
	echo -ne '\x05' | dd of=$@ bs=1 seek=48 conv=notrunc status=none

%-ogg.o: %.ogg
	$(OBJCP) -I binary -O elf64-little -B riscv $< $@
	@echo Setting RISC-V architecture:
	echo -ne '\xf3' | dd of=$@ bs=1 seek=18 conv=notrunc status=none
	@echo Setting machine flags:
	echo -ne '\x05' | dd of=$@ bs=1 seek=48 conv=notrunc status=none

%-png.o: %.png
	$(OBJCP) -I binary -O elf64-little -B riscv $< $@
	@echo Setting RISC-V architecture:
	echo -ne '\xf3' | dd of=$@ bs=1 seek=18 conv=notrunc status=none
	@echo Setting machine flags:
	echo -ne '\x05' | dd of=$@ bs=1 seek=48 conv=notrunc status=none

clean:
	$(RM) $(OBJECTS)
