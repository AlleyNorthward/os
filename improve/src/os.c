#include "os.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define MAP_ADDR 0x80000000

extern u32 pg_dir[1024];
extern void timer_init(void);

u8 map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x35};
static u32 page_table[1024] __attribute__((aligned(4096))) = {1};

struct {
  u16 offset_l;
  u16 selector;
  u16 attr;
  u16 offset_h;
} idt_table[256] __attribute__((aligned(8))) = {1};

void outb(u8 data, u16 port) {
  __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}

void os_init(void) {
  outb(0x11, 0x20);
  outb(0x11, 0xa0);
  outb(0x20, 0x21);
  outb(0x28, 0xa1);
  outb((1 << 2), 0x21);
  outb(2, 0xa1);
  outb(0x1, 0x21);
  outb(0x1, 0xa1);
  outb(0xfe, 0x21);
  outb(0xff, 0xa1);

  int tmo = 1193180 / 100;
  outb(0x36, 0x43);
  outb((u8)tmo, 0x40);
  outb(tmo >> 8, 0x40);

  idt_table[0x20].offset_l = (u32)timer_init & 0xffff;
  idt_table[0x20].offset_h = (u32)timer_init >> 16;
  idt_table[0x20].selector = KERNEL_CODE_SEG;
  idt_table[0x20].attr = 0x8e00;

  pg_dir[MAP_ADDR >> 22] = (u32)page_table | PDE_P | PDE_W | PDE_U;
  page_table[(MAP_ADDR >> 12) & 0x3FF] =
      (u32)map_phy_buffer | PDE_P | PDE_W | PDE_U;
}
