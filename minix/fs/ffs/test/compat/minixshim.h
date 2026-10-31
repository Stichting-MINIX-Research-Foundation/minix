/* Master shim header for host-testing the MINIX FFS server source files.
 * Provides just enough of the MINIX/libminixfs/libfsdriver/libbdev surface for
 * balloc.c, ialloc.c, write.c, read.c, path.c, inode.c, super.c, open.c,
 * link.c, stadir.c, protect.c, time.c, misc.c, utility.c to compile and run on
 * a Linux host against an image file.
 */
#ifndef MINIXSHIM_H
#define MINIXSHIM_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- basic MINIX types ---- */
typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;
typedef int8_t   i8_t;
typedef int16_t  i16_t;
typedef int32_t  i32_t;
typedef int64_t  i64_t;
typedef uint32_t block_t;
typedef uint64_t block64_t;
typedef int32_t  bit_t;
typedef uint32_t bitchunk_t;
typedef unsigned long vir_bytes;

/* ---- basic constants ---- */
#ifndef OK
#define OK 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define EXTERN extern
#define UNUSED(x) x __attribute__((__unused__))
#define __UNCONST(a) ((void *)(unsigned long)(const void *)(a))

#define NO_DEV   ((dev_t) 0)
#define NO_BLOCK ((block_t) 0)
#define NO_ENTRY ((ino_t) 0)
#define NO_LINK  ((nlink_t) 0)

/* mode bits (match UFS/MINIX) */
#define I_TYPE          0170000
#define I_NAMED_PIPE    0010000
#define I_CHAR_SPECIAL  0020000
#define I_DIRECTORY     0040000
#define I_BLOCK_SPECIAL 0060000
#define I_REGULAR       0100000
#define I_SYMBOLIC_LINK 0120000
#define I_SET_UID_BIT   0004000
#define I_SET_GID_BIT   0002000
#define ALL_MODES       0007777
#define RWX_MODES       0000777

#define READING 0
#define WRITING 1

/* block get_block() how-values (also defined identically in the server const.h) */
#define NORMAL   0
#define NO_READ  1
#define PREFETCH 2

#ifndef MAXBSIZE
#define MAXBSIZE 65536
#endif
#ifndef DEV_BSHIFT
#define DEV_BSHIFT 9
#endif
#ifndef LINK_MAX
#define LINK_MAX 32767
#endif
#ifndef UTIME_NOW
#define UTIME_NOW  ((1l << 30) - 1)
#endif
#ifndef UTIME_OMIT
#define UTIME_OMIT ((1l << 30) - 2)
#endif

/* ---- panic / clock ---- */
void panic(const char *fmt, ...) __attribute__((noreturn));
time_t clock_time(clock_t *p);

/* ---- libminixfs (lmfs) ---- */
struct buf {
	char *data;		/* block data (b_data() casts this) */
	block64_t lmfs_blocknr;
	int dirty;
	int refcount;
};
#define PEEK 3			/* how value: return cached or NULL */

int  lmfs_get_block(struct buf **bpp, dev_t dev, block64_t block, int how);
int  lmfs_get_block_ino(struct buf **bpp, dev_t dev, block64_t block, int how,
	ino_t ino, u64_t off);
void lmfs_put_block(struct buf *bp);
void lmfs_markdirty(struct buf *bp);
void lmfs_set_blocksize(size_t blocksize);
unsigned int lmfs_fs_block_size(void);
void lmfs_set_blockusage(u64_t total, u64_t used);
void lmfs_flushall(void);
void lmfs_invalidate(dev_t dev);
void lmfs_zero_block_ino(dev_t dev, ino_t ino, u64_t off);
void lmfs_may_use_vmcache(int n);
void lmfs_buf_pool(int n);

/* ---- libbdev ---- */
#define BDEV_NOFLAGS 0
#define BDEV_R_BIT   1
#define BDEV_W_BIT   2
int     bdev_open(dev_t dev, int access);
int     bdev_close(dev_t dev);
ssize_t bdev_read(dev_t dev, u64_t pos, char *buf, size_t count, int flags);
ssize_t bdev_write(dev_t dev, u64_t pos, char *buf, size_t count, int flags);

/* ---- libfsdriver ---- */
#define REQ_RDONLY  0x1
#define RES_NOFLAGS 0x0
#define FSC_READ    1
#define FSC_WRITE   2
#define FSC_PEEK    3
#define FSC_UNLINK  4
#define FSC_RMDIR   5

struct fsdriver_node {
	ino_t  fn_ino_nr;
	mode_t fn_mode;
	off_t  fn_size;
	uid_t  fn_uid;
	gid_t  fn_gid;
	dev_t  fn_dev;
};

/* Represents a (here, local-memory) user buffer for read/write/getdents. */
struct fsdriver_data {
	char  *ptr;
	size_t len;
};

struct fsdriver;		/* incomplete; only referenced by table.c (unused) */

int fsdriver_copyin(struct fsdriver_data *data, size_t off, void *buf, size_t n);
int fsdriver_copyout(struct fsdriver_data *data, size_t off, void *buf, size_t n);
int fsdriver_zero(struct fsdriver_data *data, size_t off, size_t n);

/* getdents collector */
struct fsdriver_dentry {
	struct fsdriver_data *data;
	size_t bytes;
	char *buf;
	size_t bufsize;
};
void    fsdriver_dentry_init(struct fsdriver_dentry *dp, struct fsdriver_data *data,
	size_t bytes, char *buf, size_t bufsize);
ssize_t fsdriver_dentry_add(struct fsdriver_dentry *dp, ino_t ino_nr,
	const char *name, size_t namelen, unsigned int type);
ssize_t fsdriver_dentry_finish(struct fsdriver_dentry *dp);

/* ---- misc macros some files expect ---- */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef rounddown
#define rounddown(x, y) (((x) / (y)) * (y))
#endif
#ifndef roundup
#define roundup(x, y)   ((((x) + ((y) - 1)) / (y)) * (y))
#endif

#endif /* MINIXSHIM_H */
