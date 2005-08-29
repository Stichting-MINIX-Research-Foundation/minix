/* Buffer (block) cache.  To acquire a block, a routine calls get_block(),
 * telling which block it wants.  The block is then regarded as "in use"
 * and has its 'b_count' field incremented.  All the blocks that are not
 * in use are chained together in an LRU list, with 'front' pointing
 * to the least recently used block, and 'rear' to the most recently used
 * block.  A reverse chain, using the field b_prev is also maintained.
 * Usage for LRU is measured by the time the put_block() is done.  The second
 * parameter to put_block() can violate the LRU order and put a block on the
 * front of the list, if it will probably not be needed soon.  If a block
 * is modified, the modifying routine must set b_dirt to DIRTY, so the block
 * will eventually be rewritten to the disk.
 */

#include <sys/dir.h>			/* need struct direct */
#include <dirent.h>

EXTERN struct buf {
  /* Data portion of the buffer. */
  union {
    char b__data[MAX_BLOCK_SIZE];		     /* ordinary user data */
/* directory block */
    struct direct b__dir[NR_DIR_ENTRIES(MAX_BLOCK_SIZE)];    
/* V1 indirect block */
    zone1_t b__v1_ind[V1_INDIRECTS];	     
/* V2 indirect block */
    zone_t  b__v2_ind[V2_INDIRECTS(MAX_BLOCK_SIZE)];	     
/* V1 inode block */
    d1_inode b__v1_ino[V1_INODES_PER_BLOCK]; 
/* V2 inode block */
    d2_inode b__v2_ino[V2_INODES_PER_BLOCK(MAX_BLOCK_SIZE)]; 
/* bit map block */
    bitchunk_t b__bitmap[FS_BITMAP_CHUNKS(MAX_BLOCK_SIZE)];  
  } b;

  /* Header portion of the buffer. */
  struct buf *b_next;		/* used to link all free bufs in a chain */
  struct buf *b_prev;		/* used to link all free bufs the other way */
  struct buf *b_hash;		/* used to link bufs on hash chains */
  block_t b_blocknr;		/* block number of its (minor) device */
  dev_t b_dev;			/* major | minor device where block resides */
  char b_dirt;			/* CLEAN or DIRTY */
  char b_count;			/* number of users of this buffer */
} buf[NR_BUFS];

/* A block is free if b_dev == NO_DEV. */

#define NIL_BUF ((struct buf *) 0)	/* indicates absence of a buffer */

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data   b.b__data
#define b_dir    b.b__dir
#define b_v1_ind b.b__v1_ind
#define b_v2_ind b.b__v2_ind
#define b_v1_ino b.b__v1_ino
#define b_v2_ino b.b__v2_ino
#define b_bitmap b.b__bitmap

EXTERN struct buf *buf_hash[NR_BUF_HASH];	/* the buffer hash table */

EXTERN struct buf *front;	/* points to least recently used free block */
EXTERN struct buf *rear;	/* points to most recently used free block */
EXTERN int bufs_in_use;		/* # bufs currently in use (not on free list)*/

/* When a block is released, the type of usage is passed to put_block(). */
#define WRITE_IMMED   0100 /* block should be written to disk now */
#define ONE_SHOT      0200 /* set if block not likely to be needed soon */

#define INODE_BLOCK        0				 /* inode block */
#define DIRECTORY_BLOCK    1				 /* directory block */
#define INDIRECT_BLOCK     2				 /* pointer block */
#define MAP_BLOCK          3				 /* bit map */
#define FULL_DATA_BLOCK    5		 	 	 /* data, fully used */
#define PARTIAL_DATA_BLOCK 6 				 /* data, partly used*/

#define HASH_MASK (NR_BUF_HASH - 1)	/* mask for hashing block numbers */
