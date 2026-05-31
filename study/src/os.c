#include "os.h"
#include "tinyfs.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define PDE_P (1 << 0)
#define PDE_W (1 << 1)
#define PDE_U (1 << 2)
#define PDE_PS (1 << 7)

#define MAP_ADDR 0x80000000

struct {
  u16 limit_l;
  u16 base_l;
  u16 basehl_attr;
  u16 base_limit;
} task0_ldt_table[256] __attribute__((aligned(8))) = {
    [TASK_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [TASK_DATA_SEG / 8] = {0xffff, 0x0000, 0xf300, 0x00cf}};

struct {
  u16 limit_l;
  u16 base_l;
  u16 basehl_attr;
  u16 base_limit;
} task1_ldt_table[256] __attribute__((aligned(8))) = {
    [TASK_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [TASK_DATA_SEG / 8] = {0xffff, 0x0000, 0xf300, 0x00cf}};

void do_syscall(int func, char *str, char color) {
  static int row = 0;
  if (func == 2) {
    u16 *dest = (u16 *)0xb8000 + 80 * row;
    while (*str) {
      *dest++ = *str++ | (color << 8);
    }

    row = (row >= 25) ? 0 : row + 1;
    for (int i = 0; i < 0xFFFFFF; ++i)
      ;
  }
}

void sys_show(char *str, char color) {
  u32 addr[] = {0, SYSCALL_SEG};

  __asm__ __volatile__(
      "push %[color]; push %[str]; push %[id]; lcalll *(%[a])" ::[a] "r"(addr),
      [color] "m"(color), [str] "m"(str), [id] "r"(2));
}

static void task_show_file(char *path, char color) {
  char buf[80];
  int n = tinyfs_read(path, buf, sizeof(buf) - 1);

  sys_show("tinyfs read:", color);
  sys_show(path, color);
  if (n < 0) {
    sys_show("read failed", color);
    return;
  }

  buf[n] = 0;
  sys_show(buf, color);
}

static void task_show_root(char color) {
  struct tinyfs_dir_view entries[8];
  int i;
  int n = tinyfs_list_root(entries, 8);

  sys_show("tinyfs root:", color);
  if (n < 0) {
    sys_show("list failed", color);
    return;
  }

  for (i = 0; i < n; ++i) {
    sys_show(entries[i].name, color);
  }
}

void task0(void) {
  char *str = "task a: 1234";
  u8 color = 0;
  int shown = 0;
  for (;;) {
    if (!shown) {
      task_show_file("/hello", 0x0e);
      task_show_file("/note", 0x0f);
      shown = 1;
    }
    sys_show(str, color++);
  }
}

void task1(void) {
  char *str = "task b: 5678";
  u8 color = 0xff;
  int shown = 0;
  for (;;) {
    if (!shown) {
      task_show_root(0x0a);
      shown = 1;
    }
    sys_show(str, color--);
  }
}

extern void timer_init(void);
extern void syscall_handler(void);

u8 map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

static u32 page_table[1024] __attribute__((aligned(4096))) = {PDE_U};
u32 pg_dir[1024] __attribute__((aligned(4096))) = {[0] = (0) | PDE_P | PDE_W |
                                                         PDE_U | PDE_PS};

u32 task0_dpl0_stack[1024];
u32 task0_dpl3_stack[1024];
u32 task1_dpl0_stack[1024];
u32 task1_dpl3_stack[1024];

u32 task0_tss[] = {
    0,
    (u32)task0_dpl0_stack + 4 * 1024,
    KERNEL_DATA_SEG,
    0x0,
    0x0,
    0x0,
    0x0,
    (u32)pg_dir,
    (u32)task0,
    0x202,
    0xa,
    0xc,
    0xd,
    0xb,
    (u32)task0_dpl3_stack + 4 * 1024,
    0x1,
    0x2,
    0x3,
    TASK_DATA_SEG,
    TASK_CODE_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK0_LDT_SEG,
    0x0,

};

u32 task1_tss[] = {
    0,
    (u32)task1_dpl0_stack + 4 * 1024,
    KERNEL_DATA_SEG,
    0x0,
    0x0,
    0x0,
    0x0,
    (u32)pg_dir,
    (u32)task1,
    0x202,
    0xa,
    0xc,
    0xd,
    0xb,
    (u32)task1_dpl3_stack + 4 * 1024,
    0x1,
    0x2,
    0x3,
    TASK_DATA_SEG,
    TASK_CODE_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK_DATA_SEG,
    TASK1_LDT_SEG,
    0x0,
};

struct {
  u16 limit_l;
  u16 base_l;
  u16 basehl_attr;
  u16 base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {
    [KERNEL_CODE_SEG / 8] = {0xffff, 0x0000, 0x9a00, 0x00cf},
    [KERNEL_DATA_SEG / 8] = {0xffff, 0x0000, 0x9200, 0x00cf},
    [APP_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [APP_DATA_SEG / 8] = {0xffff, 0x0000, 0xf300, 0x00cf},
    [TASK0_TSS_SEG / 8] = {0x68, 0, 0xe900, 0x0},
    [TASK1_TSS_SEG / 8] = {0x68, 0, 0xe900, 0x0},
    [SYSCALL_SEG / 8] = {0x0000, KERNEL_CODE_SEG, 0xec03, 0},
    [TASK0_LDT_SEG / 8] = {sizeof(task0_ldt_table) - 1, 0x0, 0xe200, 0x00cf},
    [TASK1_LDT_SEG / 8] = {sizeof(task1_ldt_table) - 1, 0x0, 0xe200, 0x00cf}};

struct {
  u16 offset_l;
  u16 selector;
  u16 attr;
  u16 offset_h;
} idt_table[256] __attribute__((aligned(8))) = {1};

void outb(u8 data, u16 port) {
  __asm__ __volatile__("outb %[v], %[p]" ::[p] "d"(port), [v] "a"(data));
}

void task_sched(void) {
  static int task_tss = TASK0_TSS_SEG;
  task_tss = (task_tss == TASK0_TSS_SEG) ? TASK1_TSS_SEG : TASK0_TSS_SEG;

  u32 addr[] = {0, task_tss};
  __asm__ __volatile__("ljmpl *(%[a])" ::[a] "r"(addr));
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

  gdt_table[TASK0_TSS_SEG / 8].base_l = (u16)(u32)task0_tss;
  gdt_table[TASK1_TSS_SEG / 8].base_l = (u16)(u32)task1_tss;
  gdt_table[SYSCALL_SEG / 8].limit_l = (u16)(u32)syscall_handler;
  gdt_table[TASK0_LDT_SEG / 8].base_l = (u32)task0_ldt_table;
  gdt_table[TASK1_LDT_SEG / 8].base_l = (u32)task1_ldt_table;

  pg_dir[MAP_ADDR >> 22] = (u32)page_table | PDE_P | PDE_W | PDE_U;
  page_table[(MAP_ADDR >> 12) & 0x3FF] =
      (u32)map_phy_buffer | PDE_P | PDE_W | PDE_U;
}
