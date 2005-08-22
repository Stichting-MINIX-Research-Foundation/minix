/* Second level block cache to supplement the file system cache.  The block
 * cache of a 16-bit Minix system is very small, too small to prevent trashing.
 * A generic 32-bit system also doesn't have a very large cache to allow it
 * to run on systems with little memory.  On a system with lots of memory one
 * can use the RAM disk as a read-only second level cache.  Any blocks pushed
 * out of the primary cache are cached on the RAM disk.  This code manages the
 * second level cache.  The cache is a simple FIFO where old blocks are put
 * into and drop out at the other end.  Must be searched backwards.
 *
 * The entry points into this file are:
 *   init_cache2: initialize the second level cache
 *   get_block2:  get a block from the 2nd level cache
 *   put_block2:  store a block in the 2nd level cache
 *   invalidate2: remove all the cache blocks on some device
 */

#include "fs.h"
#include <minix/com.h>
#include "buf.h"

#if ENABLE_CACHE2

#define MAX_BUF2	(256 * sizeof(char *))

PRIVATE struct buf2 {	/* 2nd level cache per block administration */
  block_t b2_blocknr;		/* block number */
  dev_t b2_dev;			/* device number */
  u16_t b2_count;		/* count of in-cache block groups */
} buf2[MAX_BUF2];

PRIVATE unsigned nr_buf2;		/* actual cache size */
PRIVATE unsigned buf2_idx;		/* round-robin reuse index */

#define hash2(block)	((unsigned) ((block) & (MAX_BUF2 - 1)))

/*===========================================================================*
 *				init_cache2				     *
 *===========================================================================*/
PUBLIC void init_cache2(size)
unsigned long size;
{
/* Initialize the second level disk buffer cache of 'size' blocks. */

  nr_buf2 = size > MAX_BUF2 ? MAX_BUF2 : (unsigned) size;
}

/*===========================================================================*
 *				get_block2				     *
 *===========================================================================*/
PUBLIC int get_block2(bp, only_search)
struct buf *bp;			/* buffer to get from the 2nd level cache */
int only_search;		/* if NO_READ, do nothing, else act normal */
{
/* Fill a buffer from the 2nd level cache.  Return true iff block acquired. */
  unsigned b;
  struct buf2 *bp2;

  /* If the block wanted is in the RAM disk then our game is over. */
  if (bp->b_dev == DEV_RAM) nr_buf2 = 0;

  /* Cache enabled?  NO_READ?  Any blocks with the same hash key? */
  if (nr_buf2 == 0 || only_search == NO_READ
  			|| buf2[hash2(bp->b_blocknr)].b2_count == 0) return(0);

  /* Search backwards (there may be older versions). */
  b = buf2_idx;
  for (;;) {
	if (b == 0) b = nr_buf2;
	bp2 = &buf2[--b];
	if (bp2->b2_blocknr == bp->b_blocknr && bp2->b2_dev == bp->b_dev) break;
	if (b == buf2_idx) return(0);
  }

  /* Block is in the cache, get it. */
  if (dev_io(DEV_READ, DEV_RAM, FS_PROC_NR, bp->b_data,
			(off_t) b * BLOCK_SIZE, BLOCK_SIZE, 0) == BLOCK_SIZE) {
	return(1);
  }
  return(0);
}

/*===========================================================================*
 *				put_block2				     *
 *===========================================================================*/
PUBLIC void put_block2(bp)
struct buf *bp;			/* buffer to store in the 2nd level cache */
{
/* Store a buffer into the 2nd level cache. */
  unsigned b;
  struct buf2 *bp2;

  if (nr_buf2 == 0) return;	/* no 2nd level cache */

  b = buf2_idx++;
  if (buf2_idx == nr_buf2) buf2_idx = 0;

  bp2 = &buf2[b];

  if (dev_io(DEV_WRITE, DEV_RAM, FS_PROC_NR, bp->b_data,
			(off_t) b * BLOCK_SIZE, BLOCK_SIZE, 0) == BLOCK_SIZE) {
	if (bp2->b2_dev != NO_DEV) buf2[hash2(bp2->b2_blocknr)].b2_count--;
	bp2->b2_dev = bp->b_dev;
	bp2->b2_blocknr = bp->b_blocknr;
	buf2[hash2(bp2->b2_blocknr)].b2_count++;
  }
}

/*===========================================================================*
 *				invalidate2				     *
 *===========================================================================*/
PUBLIC void invalidate2(device)
dev_t device;
{
/* Invalidate all blocks from a given device in the 2nd level cache. */
  unsigned b;
  struct buf2 *bp2;

  for (b = 0; b < nr_buf2; b++) {
	bp2 = &buf2[b];
	if (bp2->b2_dev == device) {
		bp2->b2_dev = NO_DEV;
		buf2[hash2(bp2->b2_blocknr)].b2_count--;
	}
  }
}
#endif /* ENABLE_CACHE2 */
