#include "tinyfs.h"

#define TINYFS_ROOT_BLOCK 4
#define TINYFS_DIR_ENTRIES (TINYFS_BLOCK_SIZE / sizeof(struct tinyfs_dir_entry))
#define TINYFS_HASH_SIZE 8
#define TINYFS_COLD_COOKIE 0x54465330u
#define TINYFS_READY_COOKIE 0x54465331u

struct tinyfs_hash_entry {
  tinyfs_u16 used;
  tinyfs_u16 slot;
  char name[TINYFS_NAME_LEN];
};

struct tinyfs_state {
  struct tinyfs_super_block super;
  tinyfs_u32 imap;
  tinyfs_u32 zmap;
  struct tinyfs_inode inodes[TINYFS_MAX_INODES];
  tinyfs_u8 blocks[TINYFS_MAX_BLOCKS][TINYFS_BLOCK_SIZE];
  struct tinyfs_hash_entry hash[TINYFS_HASH_SIZE];
};

static struct tinyfs_state fs;
static tinyfs_u32 fs_cookie = TINYFS_COLD_COOKIE;

static void ensure_ready(void) {
  if (fs_cookie != TINYFS_READY_COOKIE) {
    tinyfs_init();
  }
}

static void mem_zero(void *dst, tinyfs_u32 len) {
  tinyfs_u8 *p = (tinyfs_u8 *)dst;
  while (len--) {
    *p++ = 0;
  }
}

static void mem_copy(void *dst, const void *src, tinyfs_u32 len) {
  tinyfs_u8 *d = (tinyfs_u8 *)dst;
  const tinyfs_u8 *s = (const tinyfs_u8 *)src;
  while (len--) {
    *d++ = *s++;
  }
}

static int name_eq(const char *a, const char *b) {
  int i;
  for (i = 0; i < TINYFS_NAME_LEN; ++i) {
    if (a[i] != b[i]) {
      return 0;
    }
    if (a[i] == 0) {
      return 1;
    }
  }
  return 1;
}

static tinyfs_u32 name_hash(const char name[TINYFS_NAME_LEN]) {
  tinyfs_u32 hash = 5381;
  int i;
  for (i = 0; i < TINYFS_NAME_LEN && name[i]; ++i) {
    hash = ((hash << 5) + hash) + (tinyfs_u8)name[i];
  }
  return hash % TINYFS_HASH_SIZE;
}

static void name_copy(char dst[TINYFS_NAME_LEN], const char *src) {
  int i;
  for (i = 0; i < TINYFS_NAME_LEN; ++i) {
    dst[i] = src[i];
    if (src[i] == 0) {
      ++i;
      break;
    }
  }
  while (i < TINYFS_NAME_LEN) {
    dst[i++] = 0;
  }
}

static int parse_root_name(const char *path, char name[TINYFS_NAME_LEN]) {
  int i = 0;

  if (!path || path[0] != '/') {
    return TINYFS_ERR_INVAL;
  }

  ++path;
  if (!path[0]) {
    return TINYFS_ERR_ISDIR;
  }

  while (path[i]) {
    if (path[i] == '/') {
      return TINYFS_ERR_NOTDIR;
    }
    if (i >= TINYFS_NAME_LEN) {
      return TINYFS_ERR_NAMETOOLONG;
    }
    name[i] = path[i];
    ++i;
  }

  if (i == 0) {
    return TINYFS_ERR_INVAL;
  }
  while (i < TINYFS_NAME_LEN) {
    name[i++] = 0;
  }
  return TINYFS_OK;
}

static void bit_set(tinyfs_u32 *map, int bit) {
  *map |= (1u << bit);
}

static void bit_clear(tinyfs_u32 *map, int bit) {
  *map &= ~(1u << bit);
}

static int bit_test(tinyfs_u32 map, int bit) {
  return (map & (1u << bit)) != 0;
}

static struct tinyfs_dir_entry *root_dir(void) {
  return (struct tinyfs_dir_entry *)fs.blocks[fs.inodes[TINYFS_ROOT_INO].zone[0]];
}

static tinyfs_u16 alloc_inode(tinyfs_u16 mode) {
  tinyfs_u16 ino;
  for (ino = 2; ino < TINYFS_MAX_INODES; ++ino) {
    if (!bit_test(fs.imap, ino)) {
      bit_set(&fs.imap, ino);
      mem_zero(&fs.inodes[ino], sizeof(fs.inodes[ino]));
      fs.inodes[ino].mode = mode;
      fs.inodes[ino].nlinks = 1;
      return ino;
    }
  }
  return 0;
}

static tinyfs_u16 alloc_block(void) {
  tinyfs_u16 block;
  for (block = fs.super.firstdatazone + 1; block < TINYFS_MAX_BLOCKS; ++block) {
    if (!bit_test(fs.zmap, block)) {
      bit_set(&fs.zmap, block);
      mem_zero(fs.blocks[block], TINYFS_BLOCK_SIZE);
      return block;
    }
  }
  return 0;
}

static void free_inode(tinyfs_u16 ino) {
  if (ino > TINYFS_ROOT_INO && ino < TINYFS_MAX_INODES) {
    bit_clear(&fs.imap, ino);
    mem_zero(&fs.inodes[ino], sizeof(fs.inodes[ino]));
  }
}

static void free_file_blocks(struct tinyfs_inode *inode) {
  int i;
  for (i = 0; i < TINYFS_DIRECT_ZONES; ++i) {
    if (inode->zone[i]) {
      bit_clear(&fs.zmap, inode->zone[i]);
      mem_zero(fs.blocks[inode->zone[i]], TINYFS_BLOCK_SIZE);
      inode->zone[i] = 0;
    }
  }
  inode->size = 0;
}

static int find_dir_slot(const char name[TINYFS_NAME_LEN]) {
  struct tinyfs_dir_entry *dir = root_dir();
  tinyfs_u32 h = name_hash(name);
  int i;

  if (fs.hash[h].used && name_eq(fs.hash[h].name, name)) {
    tinyfs_u16 slot = fs.hash[h].slot;
    if (slot < TINYFS_DIR_ENTRIES && dir[slot].inode &&
        name_eq(dir[slot].name, name)) {
      return slot;
    }
  }

  for (i = 0; i < TINYFS_DIR_ENTRIES; ++i) {
    if (dir[i].inode && name_eq(dir[i].name, name)) {
      fs.hash[h].used = 1;
      fs.hash[h].slot = (tinyfs_u16)i;
      name_copy(fs.hash[h].name, name);
      return i;
    }
  }
  return -1;
}

static int find_free_dir_slot(void) {
  struct tinyfs_dir_entry *dir = root_dir();
  int i;
  for (i = 0; i < TINYFS_DIR_ENTRIES; ++i) {
    if (!dir[i].inode) {
      return i;
    }
  }
  return -1;
}

static int add_root_entry(const char name[TINYFS_NAME_LEN], tinyfs_u16 ino) {
  int slot = find_free_dir_slot();
  tinyfs_u32 h = name_hash(name);
  if (slot < 0) {
    return TINYFS_ERR_NOSPC;
  }
  root_dir()[slot].inode = ino;
  name_copy(root_dir()[slot].name, name);
  fs.hash[h].used = 1;
  fs.hash[h].slot = (tinyfs_u16)slot;
  name_copy(fs.hash[h].name, name);
  fs.inodes[TINYFS_ROOT_INO].size += sizeof(struct tinyfs_dir_entry);
  return TINYFS_OK;
}

void tinyfs_format(void) {
  struct tinyfs_dir_entry *dir;
  int i;

  mem_zero(&fs, sizeof(fs));
  fs.super.ninodes = TINYFS_MAX_INODES;
  fs.super.nzones = TINYFS_MAX_BLOCKS;
  fs.super.imap_blocks = 1;
  fs.super.zmap_blocks = 1;
  fs.super.firstdatazone = TINYFS_ROOT_BLOCK;
  fs.super.log_zone_size = 0;
  fs.super.max_size = TINYFS_MAX_FILE_SIZE;
  fs.super.magic = TINYFS_MAGIC;

  for (i = 0; i <= TINYFS_ROOT_BLOCK; ++i) {
    bit_set(&fs.zmap, i);
  }

  bit_set(&fs.imap, 0);
  bit_set(&fs.imap, TINYFS_ROOT_INO);
  fs.inodes[TINYFS_ROOT_INO].mode = TINYFS_S_IFDIR;
  fs.inodes[TINYFS_ROOT_INO].nlinks = 2;
  fs.inodes[TINYFS_ROOT_INO].zone[0] = TINYFS_ROOT_BLOCK;

  dir = root_dir();
  dir[0].inode = TINYFS_ROOT_INO;
  name_copy(dir[0].name, ".");
  dir[1].inode = TINYFS_ROOT_INO;
  name_copy(dir[1].name, "..");
  fs.inodes[TINYFS_ROOT_INO].size = 2 * sizeof(struct tinyfs_dir_entry);
  fs_cookie = TINYFS_READY_COOKIE;
}

void tinyfs_init(void) {
  static const char hello[] = "hello from tinyfs";
  static const char note[] = "super block + inode + dir entry + zone bitmap";

  if (fs_cookie == TINYFS_READY_COOKIE) {
    return;
  }

  tinyfs_format();
  tinyfs_write("/hello", hello, sizeof(hello) - 1);
  tinyfs_write("/note", note, sizeof(note) - 1);
}

void tinyfs_boot(void) {
  tinyfs_init();
}

int tinyfs_create(const char *path) {
  char name[TINYFS_NAME_LEN];
  tinyfs_u16 ino;
  int err;

  ensure_ready();
  err = parse_root_name(path, name);
  if (err) {
    return err;
  }
  if (find_dir_slot(name) >= 0) {
    return TINYFS_ERR_EXIST;
  }

  ino = alloc_inode(TINYFS_S_IFREG);
  if (!ino) {
    return TINYFS_ERR_NOSPC;
  }

  err = add_root_entry(name, ino);
  if (err) {
    free_inode(ino);
  }
  return err;
}

int tinyfs_write(const char *path, const char *data, tinyfs_u32 len) {
  char name[TINYFS_NAME_LEN];
  struct tinyfs_inode *inode;
  tinyfs_u32 pos = 0;
  tinyfs_u32 left = len;
  int slot;
  int zone_index = 0;
  int err;

  ensure_ready();
  if (!data && len) {
    return TINYFS_ERR_INVAL;
  }
  err = parse_root_name(path, name);
  if (err) {
    return err;
  }
  if (len > TINYFS_MAX_FILE_SIZE) {
    return TINYFS_ERR_NOSPC;
  }

  slot = find_dir_slot(name);
  if (slot < 0) {
    err = tinyfs_create(path);
    if (err) {
      return err;
    }
    slot = find_dir_slot(name);
  }

  inode = &fs.inodes[root_dir()[slot].inode];
  if ((inode->mode & TINYFS_S_IFDIR) == TINYFS_S_IFDIR) {
    return TINYFS_ERR_ISDIR;
  }

  free_file_blocks(inode);
  while (left) {
    tinyfs_u32 n = left > TINYFS_BLOCK_SIZE ? TINYFS_BLOCK_SIZE : left;
    tinyfs_u16 block = alloc_block();
    if (!block) {
      free_file_blocks(inode);
      return TINYFS_ERR_NOSPC;
    }
    inode->zone[zone_index++] = block;
    mem_copy(fs.blocks[block], data + pos, n);
    pos += n;
    left -= n;
  }
  inode->size = len;
  return (int)len;
}

int tinyfs_read(const char *path, char *buf, tinyfs_u32 len) {
  char name[TINYFS_NAME_LEN];
  struct tinyfs_inode *inode;
  tinyfs_u32 pos = 0;
  tinyfs_u32 left;
  int slot;
  int zone_index = 0;
  int err;

  ensure_ready();
  if (!buf && len) {
    return TINYFS_ERR_INVAL;
  }
  err = parse_root_name(path, name);
  if (err) {
    return err;
  }
  slot = find_dir_slot(name);
  if (slot < 0) {
    return TINYFS_ERR_NOENT;
  }

  inode = &fs.inodes[root_dir()[slot].inode];
  if ((inode->mode & TINYFS_S_IFDIR) == TINYFS_S_IFDIR) {
    return TINYFS_ERR_ISDIR;
  }

  left = inode->size < len ? inode->size : len;
  while (left) {
    tinyfs_u32 n = left > TINYFS_BLOCK_SIZE ? TINYFS_BLOCK_SIZE : left;
    tinyfs_u16 block = inode->zone[zone_index++];
    mem_copy(buf + pos, fs.blocks[block], n);
    pos += n;
    left -= n;
  }
  return (int)pos;
}

int tinyfs_list_root(struct tinyfs_dir_view *out, int max_entries) {
  struct tinyfs_dir_entry *dir;
  int copied = 0;
  int i;

  ensure_ready();
  if (!out || max_entries <= 0) {
    return TINYFS_ERR_INVAL;
  }

  dir = root_dir();
  for (i = 0; i < TINYFS_DIR_ENTRIES && copied < max_entries; ++i) {
    int j;
    if (!dir[i].inode) {
      continue;
    }
    out[copied].inode = dir[i].inode;
    for (j = 0; j < TINYFS_NAME_LEN; ++j) {
      out[copied].name[j] = dir[i].name[j];
      if (!dir[i].name[j]) {
        break;
      }
    }
    out[copied].name[TINYFS_NAME_LEN] = 0;
    ++copied;
  }
  return copied;
}

int tinyfs_stat(const char *path, struct tinyfs_inode *out) {
  char name[TINYFS_NAME_LEN];
  int slot;
  int err;

  ensure_ready();
  if (!path || !out) {
    return TINYFS_ERR_INVAL;
  }
  if (path[0] == '/' && path[1] == 0) {
    *out = fs.inodes[TINYFS_ROOT_INO];
    return TINYFS_OK;
  }

  err = parse_root_name(path, name);
  if (err) {
    return err;
  }
  slot = find_dir_slot(name);
  if (slot < 0) {
    return TINYFS_ERR_NOENT;
  }
  *out = fs.inodes[root_dir()[slot].inode];
  return TINYFS_OK;
}

const struct tinyfs_super_block *tinyfs_super(void) {
  ensure_ready();
  return &fs.super;
}

const struct tinyfs_inode *tinyfs_get_inode(tinyfs_u16 ino) {
  ensure_ready();
  if (ino >= TINYFS_MAX_INODES || !bit_test(fs.imap, ino)) {
    return 0;
  }
  return &fs.inodes[ino];
}
