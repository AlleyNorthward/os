#include "os.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

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
