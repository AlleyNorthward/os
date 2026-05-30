#include "os.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define MAP_ADDR 0x80000000

extern u32 pg_dir[1024];

u8 map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x35};

static u32 page_table[1024] __attribute__((aligned(4096))) = {1};

void os_init(void) {
  pg_dir[MAP_ADDR >> 22] = (u32)page_table | PDE_P | PDE_W | PDE_U;
  page_table[(MAP_ADDR >> 12) & 0x3ff] =
      (u32)map_phy_buffer | PDE_P | PDE_W | PDE_U;
}
