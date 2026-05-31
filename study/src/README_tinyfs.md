# tinyfs 运行流程

tinyfs 是给当前 `study` 内核使用的教学文件系统。它不是完整 Linux 文件系统，也不读写真实硬盘；它把一块内存数组当作“磁盘块设备”，用 Linux 0.12 / Minix 文件系统的核心思想组织数据。

最重要的链路是：

```text
路径名 -> 目录项 -> inode -> 数据块号 -> 数据块内容
```

## 当前已经嵌入的位置

当前代码已经接入三处：

1. `start.S`

   `_start_32` 里先调用 `os_init`，然后调用 `tinyfs_boot`：

   ```asm
   call os_init
   call tinyfs_boot
   ```

   所以 tinyfs 会在进入第一个用户任务之前完成初始化。

2. `os.c`

   `os.c` 引入了：

   ```c
   #include "tinyfs.h"
   ```

   `task0()` 会读取 `/hello` 和 `/note` 并通过 `sys_show()` 打印出来。

   `task1()` 会列出根目录，并把目录项名字打印出来。

3. `Makefile`

   `tinyfs.c` 已经被编译为 `tinyfs.o`，并链接进 `os.elf`：

   ```makefile
   OBJS = $(BUILD)/start.o $(BUILD)/os.o $(BUILD)/tinyfs.o
   ```

   启动命令没有改，仍然是：

   ```bash
   make
   make run
   make debug
   make gdb
   ```

## 启动后的完整流程

整个系统启动时，流程如下：

```text
BIOS
  -> 加载 boot sector 到 0x7c00
  -> 执行 _start
  -> 读取后续扇区到内存
  -> 加载 GDT / IDT
  -> 进入保护模式
  -> 跳到 _start_32
  -> call os_init
  -> call tinyfs_boot
  -> 打开分页
  -> 加载 TR / LDTR
  -> iret 到 task0
  -> task0/task1 使用 tinyfs API
```

## 为什么这次不会卡在 iret 后

`iret` 到 task0 前，CPU 会依赖这些东西：

```text
TR  -> GDT 里的 TASK0_TSS 描述符
LDTR -> GDT 里的 TASK0_LDT 描述符
CS  -> LDT 里的 TASK_CODE_SEG 描述符
SS  -> LDT 里的 TASK_DATA_SEG 描述符
```

如果 TSS/LDT 描述符里的 base 只写低 16 位，那么只要 `task0_tss` 或 `task0_ldt_table` 的地址超过 `0xffff`，CPU 就会去错误位置读取 TSS/LDT。结果通常是：

```text
iret
  -> 加载用户态 CS/SS
  -> 描述符地址错误
  -> #GP 或 #TS
  -> 没有对应异常处理
  -> triple fault
  -> 虚拟机重启，看起来像又回到开头
```

所以现在 `os.c` 使用 `set_descriptor_base()` 写完整 32 位 base：

```text
base[15:0]   -> descriptor.base_l
base[23:16]  -> descriptor.basehl_attr 的低 8 位
base[31:24]  -> descriptor.base_limit 的高 8 位
```

系统调用使用的 call gate 也一样。`sys_show()` 会通过 `lcall` 进入 `syscall_handler`，call gate 里的 offset 也必须是完整 32 位：

```text
offset[15:0]  -> descriptor.limit_l
offset[31:16] -> descriptor.base_limit
```

否则 task0 已经 `iret` 成功了，但第一次 `sys_show()` 仍然可能跳到错误地址，让系统像“iret 后失败”一样崩掉。

其中 tinyfs 的初始化发生在：

```text
start.S:_start_32
  -> tinyfs_boot()
       -> tinyfs_init()
            -> tinyfs_format()
            -> tinyfs_write("/hello", ...)
            -> tinyfs_write("/note", ...)
```

## tinyfs_format 做了什么

`tinyfs_format()` 相当于格式化一个非常小的内存磁盘。

它会初始化这些结构：

```text
super block
inode bitmap
zone/block bitmap
inode table
data blocks
root directory
hash table
```

具体顺序：

1. 清空 `fs` 整个文件系统状态。
2. 设置超级块：

   ```text
   ninodes       = 16
   nzones        = 32
   block size    = 1024
   firstdatazone = 4
   magic         = 0x137f
   ```

3. 标记 inode 位图：

   ```text
   inode 0: 不使用
   inode 1: 根目录
   ```

4. 标记数据块位图：

   ```text
   block 0..4: 保留
   block 4: 根目录数据块
   ```

5. 创建根目录 inode：

   ```text
   inode number = 1
   mode         = directory
   nlinks       = 2
   zone[0]      = 4
   ```

6. 在根目录数据块里写入两个目录项：

   ```text
   "."  -> inode 1
   ".." -> inode 1
   ```

## tinyfs_init 做了什么

`tinyfs_init()` 会先格式化，然后创建两个演示文件：

```text
/hello
/note
```

创建 `/hello` 的流程是：

```text
tinyfs_write("/hello", data, len)
  -> 查根目录有没有 hello
  -> 没有，则 tinyfs_create("/hello")
       -> 分配一个空 inode
       -> 在根目录中添加目录项 hello -> inode
       -> 更新 hash 表
  -> 给该 inode 分配数据块
  -> 把 data 拷贝到数据块
  -> 更新 inode.size
```

## 读取文件时发生什么

以 `tinyfs_read("/hello", buf, len)` 为例：

```text
"/hello"
  -> 解析路径，得到文件名 "hello"
  -> 用 "hello" 查 hash 表
       -> 命中：直接得到目录项 slot
       -> 未命中或冲突：线性扫描根目录
  -> 目录项给出 inode number
  -> 根据 inode number 找 inode table
  -> inode.zone[] 给出数据块号
  -> 从 blocks[block] 拷贝数据到 buf
  -> 返回读取字节数
```

也就是说，路径名本身不直接指向数据。路径名先通过目录项变成 inode，inode 再指向数据块。

## 目录项是什么

目录在文件系统里也是文件。根目录的数据块里放的是一组目录项：

```c
struct tinyfs_dir_entry {
  tinyfs_u16 inode;
  char name[14];
};
```

每个目录项表达一件事：

```text
这个名字 -> 这个 inode 号
```

例如：

```text
hello -> inode 2
note  -> inode 3
```

所以目录查找不是“打开文件内容”，而是“把名字翻译成 inode 号”。

## inode 是什么

inode 保存文件元数据和数据块索引：

```c
struct tinyfs_inode {
  tinyfs_u16 mode;
  tinyfs_u16 uid;
  tinyfs_u32 size;
  tinyfs_u32 mtime;
  tinyfs_u8 gid;
  tinyfs_u8 nlinks;
  tinyfs_u16 zone[7];
};
```

当前 tinyfs 只支持 7 个直接块：

```text
inode.zone[0] -> 第 1 个数据块
inode.zone[1] -> 第 2 个数据块
...
inode.zone[6] -> 第 7 个数据块
```

所以最大文件大小是：

```text
7 * 1024 = 7168 字节
```

Linux 0.12 / Minix 文件系统还会有一次间接块、二次间接块等结构。tinyfs 先不做这些，是为了让主链路更清楚。

## 位图是什么

tinyfs 有两个位图：

```text
imap: inode bitmap
zmap: zone/block bitmap
```

位图的意义是：

```text
bit = 0: 空闲
bit = 1: 已占用
```

分配 inode 时：

```text
从 imap 找第一个 0
把它置 1
返回 inode number
```

分配数据块时：

```text
从 zmap 找第一个 0
把它置 1
返回 block number
```

这就是 Linux 0.12 文件系统里 `new_inode`、`new_block` 一类操作背后的核心思想。

## 有没有散列表

有。

当前 tinyfs 里有一个很小的根目录查找 hash 表：

```c
#define TINYFS_HASH_SIZE 8

struct tinyfs_hash_entry {
  tinyfs_u16 used;
  tinyfs_u16 slot;
  char name[14];
};
```

它的作用是缓存：

```text
文件名 -> 根目录目录项 slot
```

查找 `/hello` 时：

```text
name_hash("hello") -> hash index
  -> 如果 hash entry 里就是 hello
       -> 直接得到目录项 slot
  -> 如果没命中
       -> 扫描根目录
       -> 找到后写回 hash 表
```

注意，这个 hash 表只是缓存，不是文件系统的真实数据来源。真实数据仍然是：

```text
根目录数据块里的 tinyfs_dir_entry
```

如果 hash 冲突，tinyfs 会回退到线性扫描目录，所以不会因为冲突找错文件。

Linux 0.12 里更典型的 hash 思路主要出现在缓冲区缓存等地方。现代 Linux 还有 dentry cache、inode cache 等机制。tinyfs 这里加这个小 hash，是为了让你直观看到“名字查找可以被缓存加速”。

## task0 会看到什么

`task0()` 启动后会先执行一次：

```text
tinyfs_read("/hello", ...)
tinyfs_read("/note", ...)
```

然后通过 `sys_show()` 打印：

```text
tinyfs read:
/hello
hello from tinyfs
tinyfs read:
/note
super block + inode + dir entry + zone bitmap
```

之后继续打印原来的：

```text
task a: 1234
```

## task1 会看到什么

`task1()` 启动后会先执行一次：

```text
tinyfs_list_root(...)
```

然后打印根目录里能看到的名字：

```text
tinyfs root:
.
..
hello
note
```

之后继续打印原来的：

```text
task b: 5678
```

## 当前为什么可以在 task 里直接调用 tinyfs

现在的教学 OS 里，用户任务的 LDT 代码段和数据段仍然是：

```text
base  = 0
limit = 4GB
DPL   = 3
```

分页里低 4MB 也设置了用户可访问位。所以 task0/task1 可以直接调用 `tinyfs_read()` 这类内核函数。

这对真实操作系统来说不安全，但对当前阶段很适合教学，因为你可以先把文件系统结构跑通。

更接近真实系统的下一步是：

```text
task
  -> call gate / int syscall
  -> kernel syscall handler
  -> tinyfs_read / tinyfs_write
  -> copy_to_user / copy_from_user
  -> 返回 task
```

也就是把 tinyfs API 包到系统调用里，而不是让 task 直接调用。

## API

```c
void tinyfs_init(void);
void tinyfs_boot(void);
void tinyfs_format(void);

int tinyfs_create(const char *path);
int tinyfs_write(const char *path, const char *data, tinyfs_u32 len);
int tinyfs_read(const char *path, char *buf, tinyfs_u32 len);
int tinyfs_list_root(struct tinyfs_dir_view *out, int max_entries);
int tinyfs_stat(const char *path, struct tinyfs_inode *out);

const struct tinyfs_super_block *tinyfs_super(void);
const struct tinyfs_inode *tinyfs_get_inode(tinyfs_u16 ino);
```

## 设计边界

当前 tinyfs 故意不做这些事：

```text
不读写真正硬盘
不实现多级目录
不实现间接块
不实现删除文件
不做权限检查
不做 buffer cache
不做 page cache
```

它的目标不是完整，而是清楚。先理解：

```text
目录项负责名字
inode 负责元数据和块索引
位图负责分配
数据块负责保存内容
hash 表负责加速名字查找
```

理解这条链路后，再把 `blocks[][]` 替换成真实块设备读写，就能继续向 Linux 0.12 的文件系统实现靠近。
