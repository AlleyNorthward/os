#include "os.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define PDE_P  (1 << 0)
#define PDE_W  (1 << 1)
#define PDE_U  (1 << 2)
#define PDE_PS (1 << 7)

#define MAP_ADDR  0x80000000

u8 map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

static u32 page_table[1024] __attribute__((aligned(4096))) = {PDE_U};
u32 pg_dir[1024] __attribute__((aligned(4096))) = {
  [0] = (0) | PDE_P | PDE_W | PDE_U | PDE_PS
};

struct {
  u16 limit_l;
  u16 base_l;
  u16 basehl_attr;
  u16 base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {

  [KERNEL_CODE_SEG / 8] = {
    0xffff,
    0x0000,
    0x9a00,
    0x00cf
  },

  [KERNEL_DATA_SEG / 8] = {
    0xffff,
    0x0000,
    0x9200,
    0x00cf
  }
};

void os_init(void){
  pg_dir[MAP_ADDR >> 22] = (u32)page_table | PDE_P | PDE_W | PDE_U;
  page_table[(MAP_ADDR >> 12) & 0x3FF] = (u32)map_phy_buffer | PDE_P | PDE_W | PDE_U;
}
