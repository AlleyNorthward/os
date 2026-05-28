ENTRY(_start)

SECTIONS{
  . = 0x7c00;
  .boot :
  {
    KEEP(*(.boot))
  }

  ASSERT(SIZEOF(.boot) == 512, "boot sector must be exactly 512 bytes")

  . = 0x7e00;
  .protect32 :
  {
    KEEP(*(.text32))
    *(.text*)
  }

  .rodata :
  {
    *(.rodata*)
  }

  .data :
  {
    *(.data*)
  }

  .bss :
  {
    *(.bss*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.eh_frame)
    *(.comment)
    *(.note*)
    *(.gnu.property)
  }
}
