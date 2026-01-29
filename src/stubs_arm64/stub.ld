ENTRY(_start)
SECTIONS
{
  . = 0x5000;
  .text   : { *(.text .text.*) }
  .rodata : { KEEP(*(.rodata.elfz_params)) *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) }
  .bss    : { *(.bss .bss.*) *(COMMON) }
}
