/* Prototypes for -lminixfs. */

#ifndef _MINIX_FSLIB_H
#define _MINIX_FSLIB_H

#include <minix/safecopies.h>
#include <minix/sef.h>
#include <minix/vfsif.h>

struct buf {
  /* Data portion of the buffer. */
  void *data;

  /* Header portion of the buffer - internal to libminixfs. */
  struct buf *lmfs_next;       /* used to link all free bufs in a chain */
  struct buf *lmfs_prev;       /* used to link all free bufs the other way */
  struct buf *lmfs_hash;       /* used to link bufs on hash chains */
  block_t lmfs_blocknr;        /* block number of its (minor) device */
  dev_t lmfs_dev;              /* major | minor device where block resides */
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

int fs_lookup_credentials(vfs_ucred_t *credentials,
        uid_t *caller_uid, gid_t *caller_gid, cp_grant_id_t grant2, size_t cred_size);
u32_t fs_bufs_heuristic(int minbufs, u32_t btotal, u32_t bfree,
	int blocksize, dev_t majordev);

void lmfs_markdirty(struct buf *bp);
void lmfs_markclean(struct buf *bp);
int lmfs_isclean(struct buf *bp);
dev_t lmfs_dev(struct buf *bp);
int lmfs_bytes(struct buf *bp);
int lmfs_bufs_in_use(void);
int lmfs_nr_bufs(void);
void lmfs_flushall(void);
int lmfs_fs_block_size(void);
void lmfs_may_use_vmcache(int); 
void lmfs_set_blocksize(int blocksize, int major); 
void lmfs_reset_rdwt_err(void); 
int lmfs_rdwt_err(void); 
void lmfs_buf_pool(int new_nr_bufs);
struct buf *lmfs_get_block(dev_t dev, block_t block,int only_search);
struct buf *lmfs_get_block_ino(dev_t dev, block_t block,int only_search,
	ino_t ino, u64_t off);
void lmfs_invalidate(dev_t device);
void lmfs_put_block(struct buf *bp, int block_type);
void lmfs_rw_scattered(dev_t, struct buf **, int, int);
void lmfs_setquiet(int q);
int lmfs_do_bpeek(message *);
void lmfs_cache_reevaluate(dev_t dev);
void lmfs_blockschange(dev_t dev, int delta);

/* calls that libminixfs does into fs */
void fs_blockstats(u32_t *blocks, u32_t *free, u32_t *used);
int fs_sync(void);

/* get_block arguments */
#define NORMAL             0    /* forces get_block to do disk read */
#define NO_READ            1    /* prevents get_block from doing disk read */
#define PREFETCH           2    /* tells get_block not to read or mark dev */

/* When a block is released, the type of usage is passed to put_block(). */
#define ONE_SHOT      0200 /* set if block not likely to be needed soon */

#define INODE_BLOCK        0                             /* inode block */
#define DIRECTORY_BLOCK    1                             /* directory block */
#define INDIRECT_BLOCK     2                             /* pointer block */
#define MAP_BLOCK          3                             /* bit map */
#define FULL_DATA_BLOCK    5                             /* data, fully used */
#define PARTIAL_DATA_BLOCK 6                             /* data, partly used*/

#define END_OF_FILE   (-104)        /* eof detected */

#endif /* _MINIX_FSLIB_H */

