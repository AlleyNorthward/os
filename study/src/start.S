#include "os.h"

.code16
.global _start, timer_init, syscall_handler

.text
_start:
  mov $0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $_start, %esp

read_self_all:
  mov $_start_32, %bx
  mov $0x2, %cx
  mov $0x0240, %ax
  mov $0x80, %dx
  int $0x0013
  jc read_self_all

  cli
  lgdt [gdt_desc]
  lidt [idt_desc]

  mov $1, %eax
  lmsw %ax
  jmp $KERNEL_CODE_SEG, $_start_32

.org 0x1fe
.byte 0x55, 0xaa

.code32
.text

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

  mov $TASK0_TSS_SEG, %ax
  ltr %ax

  mov $TASK0_LDT_SEG, %ax
  lldt %ax

  push $TASK_DATA_SEG
  push $task0_dpl3_stack + 1024 * 4
  push $0x202
  push $TASK_CODE_SEG
  push $task0_entry

  iret

timer_init:
  push %ds
  pusha
  mov $0x20, %al
  outb %al, $0x20

  mov $KERNEL_DATA_SEG, %ax
  mov %ax, %ds
  call task_sched

  popa
  pop %ds
  iret

syscall_handler:
  push %ds
  pusha
  mov $KERNEL_DATA_SEG, %ax
  mov %ax, %ds

  mov %esp, %ebp
  push 13 * 4(%ebp)
  push 12 * 4(%ebp)
  push 11 * 4(%ebp)
  call do_syscall
  add $(3*4), %esp

  popa
  pop %ds

  retf $(3 * 4)

task0_entry:
  mov %ss, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %gs
  mov %ax, %fs
  jmp task0


gdt_desc:
.word (256 * 8) - 1
.long gdt_table

idt_desc:
.word (256 * 8) - 1
.long idt_table
