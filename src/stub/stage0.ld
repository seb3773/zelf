/* stage0.ld
 * Linker script for stage0_nrv2b_le32.elf
 * Goal: ensure stage0 entry is at the very beginning of the binary payload,
 * and discard non-essential sections (notes, comments, unwind).
 */

ENTRY(stage0_nrv2b_entry)

SECTIONS
{
  . = 0;

  .text : ALIGN(16)
  {
    *(.text.stage0*)
    *(.text*)
  }

  .rodata : ALIGN(16)
  {
    *(.rodata*)
    *(.data.rel.ro*)
  }

  .data : ALIGN(16)
  {
    *(.data*)
  }

  .bss : ALIGN(16)
  {
    *(.bss*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.note*)
    *(.comment*)
    *(.eh_frame*)
    *(.eh_frame_hdr*)
    *(.gnu*)
  }
}





