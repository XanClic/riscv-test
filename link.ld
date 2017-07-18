ENTRY(_start)
SECTIONS {
    . = 0x1000;

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
