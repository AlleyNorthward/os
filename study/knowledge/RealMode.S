#include "os.h"

.code16
.global _start

.text
_start:
# 位于 0x7c00，启动扇区共 512 字节
# 可用区域: 0x7c00 ~ 0x7dfd
# 启动标志: 0x7dfe ~ 0x7dff，必须为 0x55 0xaa

  mov $0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $_start, $esp 
  # 设置栈区, _start 地址是0x7c00, 向低地址是栈区, 高地址是自己编写的bootloader(512字节)以及操作系统

read_self_all:
# bios只会自动地将第一扇区内的代码(也就是这里写的代码)加载到0x7c00处
# 一个扇区512字节, 肯定是不够, 所以需要从磁盘中, 将相关代码(操作系统)引入内存,
# 下面int 之前的操作是相当于函数传参, 最后中断函数会在ROM中, 并不在RAM中
  mov $_start_32, %bx
  # 第一个是将待加载地址放入bx中
  mov $0x2, %cx
  # 第二个是从第二扇区开始
  mov $0x0240, %ax
  # 第三个2表示执行bios int 13中断, 40(10进制64)表示读取64个扇区
  mov $0x80, %dx
  # 第四个表示第一个块磁盘(只有一个)
  int $0x0013
  jc read_self_all

  jmp .

.org 0x01fe
.byte 0x55, 0xaa
# 非cpu指令, 而是操作磁盘的
# 相对当前段(.text, 也就是用户编译-Ttext传参, 即0x7c00)偏移0x1fe, 且中间未填充数据为0
# 然后在这里写入 0x55 0xaa
# 并不是顺序执行, 这是编译层面, 操作的是.o

_start_32:
.fill 64*1024, 1, 0x35
# 编译层面执行的, cpu不会执行, 磁盘初始默认值为这些, 但是未加载到内存时, 相关地址仍为0, 执行read_self_all后会显示0x35, 但要明确, 并不是因为cpu执行了这段代码后才显示的
