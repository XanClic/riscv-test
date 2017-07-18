DRAM_BASE = 0x80000000;

ENTRY(_start)
SECTIONS {
    . = DRAM_BASE;

    .text : AT(ADDR(.text)) {
        *(.text)
    }
    .rodata ALIGN(4096) : AT(ADDR(.rodata)) {
        *(.rodata)
    }
    .data ALIGN(4096) : AT(ADDR(.data)) {
        *(.data)
    }
    .bss ALIGN(4096) : AT(ADDR(.bss)) {
        *(.bss)
    }
}
