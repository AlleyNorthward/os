#include "os.h"
.section .boot, "ax"
.code16
.global _start

_start:
  cli

  mov $0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $_start, %esp

  sti

read_stage2:
  mov $_start_32, %bx
  mov $0x0002, %cx
  mov $0x0240, %ax
  mov $0x0080, %dx
  int $0x0013

  jc read_stage2

  cli
  lgdt [gdt_desc]

  mov $1, %eax
  lmsw %ax
  jmp $KERNEL_CODE_SEG, $_start_32

hang:
  jmp hang

.org 0x01fe
.byte 0x55, 0xaa

.section .text32, "ax"
.code32
.global _start_32

_start_32:
  mov $KERNEL_DATA_SEG, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %gs
  mov %ax, %fs
  mov $_start, %esp

  call os_init

  mov $pg_dir, %eax
  mov %eax, %cr3

  mov %cr4, %eax
  orl $(1 << 4), %eax
  mov %eax, %cr4

  mov %cr0, %eax
  orl $(1 << 31), %eax
  mov %eax, %cr0
  jmp .

.section .data
.align 8

gdt_table:
  .quad 0x0000000000000000
  .quad 0x00cf9a000000ffff
  .quad 0x00cf92000000ffff
gdt_end:

gdt_desc:
.word gdt_end - gdt_table - 1
.long gdt_table


.align 4096
.global pg_dir

pg_dir:
.long 0 + PDE_P + PDE_W + PDE_U + PDE_PS
.fill 1023, 4, 0
