/* Prototypes for -lminixfs. */

#ifndef _MINIX_FSLIB_H
#define _MINIX_FSLIB_H

#include <minix/fsdriver.h>

struct buf {
  /* Data portion of the buffer. */
  void *data;

  /* Header portion of the buffer - internal to libminixfs. */
  struct buf *lmfs_next;       /* used to link all free bufs in a chain */
  struct buf *lmfs_prev;       /* used to link all free bufs the other way */
  struct buf *lmfs_hash;       /* used to link bufs on hash chains */
  dev_t lmfs_dev;              /* major | minor device where block resides */
  block64_t lmfs_blocknr;      /* block number of its (minor) device */
  char lmfs_count;             /* number of users of this buffer */
  char lmfs_needsetcache;      /* to be identified to VM */
  unsigned int lmfs_bytes;     /* Number of bytes allocated in bp */
  u32_t lmfs_flags;            /* Flags shared between VM and FS */

  /* If any, which inode & offset does this block correspond to?
   * If none, VMC_NO_INODE
   */
  ino_t lmfs_inode;
  u64_t lmfs_inode_offset;
};

void lmfs_markdirty(struct buf *bp);
void lmfs_markclean(struct buf *bp);
int lmfs_isclean(struct buf *bp);
dev_t lmfs_dev(struct buf *bp);
int lmfs_bufs_in_use(void);
int lmfs_nr_bufs(void);
void lmfs_flushall(void);
void lmfs_flushdev(dev_t dev);
int lmfs_fs_block_size(void);
void lmfs_may_use_vmcache(int);
void lmfs_set_blocksize(int blocksize);
void lmfs_reset_rdwt_err(void);
int lmfs_rdwt_err(void);
void lmfs_buf_pool(int new_nr_bufs);
struct buf *lmfs_get_block(dev_t dev, block64_t block, int how);
struct buf *lmfs_get_block_ino(dev_t dev, block64_t block, int how, ino_t ino,
	u64_t off);
void lmfs_put_block(struct buf *bp);
void lmfs_free_block(dev_t dev, block64_t block);
void lmfs_zero_block_ino(dev_t dev, ino_t ino, u64_t off);
void lmfs_invalidate(dev_t device);
void lmfs_rw_scattered(dev_t, struct buf **, int, int);
void lmfs_setquiet(int q);
void lmfs_cache_reevaluate(void);
void lmfs_blockschange(int delta);

/* calls that libminixfs does into fs */
void fs_blockstats(u64_t *blocks, u64_t *free);

/* get_block arguments */
#define NORMAL             0    /* forces get_block to do disk read */
#define NO_READ            1    /* prevents get_block from doing disk read */
#define PREFETCH           2    /* tells get_block not to read or mark dev */
#define PEEK               3    /* returns NULL if not in cache or VM cache */

#define END_OF_FILE   (-104)        /* eof detected */

/* Block I/O helper functions. */
void lmfs_driver(dev_t dev, char *label);
ssize_t lmfs_bio(dev_t dev, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
void lmfs_bflush(dev_t dev);

#endif /* _MINIX_FSLIB_H */
