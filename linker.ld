/* BleeOS linker script: flat 32-bit kernel loaded at 0x7E00 by the MBR. */
ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")
SECTIONS
{
    . = 0x7E00;
    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment*) *(.eh_frame*) *(.note*) }
}
