#ifndef OS_H
#define OS_H

#define KERNEL_CODE_SEG 1 << 3
#define KERNEL_DATA_SEG 1 << 4

#define PDE_P   (1 << 0) // 存在位
#define PDE_W   (1 << 1) // 读写位
#define PDE_U   (1 << 2) // 权限位
#define PDE_PS  (1 << 7) // 开启4M读取

#endif // OS_H
