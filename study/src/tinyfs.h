#ifndef TINYFS_H
#define TINYFS_H

typedef unsigned char tinyfs_u8;
typedef unsigned short tinyfs_u16;
typedef unsigned int tinyfs_u32;

#define TINYFS_BLOCK_SIZE 1024
#define TINYFS_MAX_INODES 16
#define TINYFS_MAX_BLOCKS 32
#define TINYFS_NAME_LEN 14
#define TINYFS_DIRECT_ZONES 7
#define TINYFS_MAX_FILE_SIZE (TINYFS_BLOCK_SIZE * TINYFS_DIRECT_ZONES)

#define TINYFS_ROOT_INO 1
#define TINYFS_MAGIC 0x137f

#define TINYFS_S_IFREG 0100000
#define TINYFS_S_IFDIR 0040000

#define TINYFS_OK 0
#define TINYFS_ERR_INVAL -1
#define TINYFS_ERR_NOENT -2
#define TINYFS_ERR_EXIST -3
#define TINYFS_ERR_NOSPC -4
#define TINYFS_ERR_NAMETOOLONG -5
#define TINYFS_ERR_ISDIR -6
#define TINYFS_ERR_NOTDIR -7

struct tinyfs_super_block {
  tinyfs_u16 ninodes;
  tinyfs_u16 nzones;
  tinyfs_u16 imap_blocks;
  tinyfs_u16 zmap_blocks;
  tinyfs_u16 firstdatazone;
  tinyfs_u16 log_zone_size;
  tinyfs_u32 max_size;
  tinyfs_u16 magic;
};

struct tinyfs_inode {
  tinyfs_u16 mode;
  tinyfs_u16 uid;
  tinyfs_u32 size;
  tinyfs_u32 mtime;
  tinyfs_u8 gid;
  tinyfs_u8 nlinks;
  tinyfs_u16 zone[TINYFS_DIRECT_ZONES];
};

struct tinyfs_dir_entry {
  tinyfs_u16 inode;
  char name[TINYFS_NAME_LEN];
};

struct tinyfs_dir_view {
  tinyfs_u16 inode;
  char name[TINYFS_NAME_LEN + 1];
};

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

#endif
