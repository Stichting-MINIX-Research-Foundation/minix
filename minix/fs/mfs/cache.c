/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file contains some
 * related routines, but the cache is now in libminixfs.
 */

#include "fs.h"
#include <minix/u64.h>
#include <minix/bdev.h>
#include <sys/param.h>
#include <stdlib.h>
#include <assert.h>
#include <minix/libminixfs.h>
#include <math.h>
#include "buf.h"
#include "super.h"
#include "inode.h"

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
struct buf *get_block(dev_t dev, block_t block, int how)
{
/* Wrapper routine for lmfs_get_block(). This MFS implementation does not deal
 * well with block read errors pretty much anywhere. To prevent corruption due
 * to unchecked error conditions, we panic upon an I/O failure here.
 */
  struct buf *bp;
  int r;

  if ((r = lmfs_get_block(&bp, dev, block, how)) != OK && r != ENOENT)
	panic("MFS: error getting block (%llu,%u): %d", dev, block, r);

  assert(r == OK || how == PEEK);

  return (r == OK) ? bp : NULL;
}

/*===========================================================================*
 *				alloc_zone				     *
 *===========================================================================*/
zone_t alloc_zone(
  dev_t dev,			/* device where zone wanted */
  zone_t z			/* try to allocate new zone near this one */
)
{
/* Allocate a new zone on the indicated device and return its number. */

  bit_t b, bit;
  struct super_block *sp;
  static int print_oos_msg = 1;

  /* Note that the routine alloc_bit() returns 1 for the lowest possible
   * zone, which corresponds to sp->s_firstdatazone.  To convert a value
   * between the bit number, 'b', used by alloc_bit() and the zone number, 'z',
   * stored in the inode, use the formula:
   *     z = b + sp->s_firstdatazone - 1
   * Alloc_bit() never returns 0, since this is used for NO_BIT (failure).
   */
  sp = &superblock;

  /* If z is 0, skip initial part of the map known to be fully in use. */
  if (z == sp->s_firstdatazone) {
	bit = sp->s_zsearch;
  } else {
	bit = (bit_t) (z - (sp->s_firstdatazone - 1));
  }
  b = alloc_bit(sp, ZMAP, bit);
  if (b == NO_BIT) {
	err_code = ENOSPC;
	if (print_oos_msg)
		printf("No space on device %d/%d\n", major(sp->s_dev),
			minor(sp->s_dev));
	print_oos_msg = 0;	/* Don't repeat message */
	return(NO_ZONE);
  }
  print_oos_msg = 1;
  if (z == sp->s_firstdatazone) sp->s_zsearch = b;	/* for next time */
  return( (zone_t) (sp->s_firstdatazone - 1) + (zone_t) b);
}

/*===========================================================================*
 *				free_zone				     *
 *===========================================================================*/
void free_zone(
  dev_t dev,				/* device where zone located */
  zone_t numb				/* zone to be returned */
)
{
/* Return a zone. */

  register struct super_block *sp;
  bit_t bit;

  /* Locate the appropriate super_block and return bit. */
  sp = &superblock;
  if (numb < sp->s_firstdatazone || numb >= sp->s_zones) return;
  bit = (bit_t) (numb - (zone_t) (sp->s_firstdatazone - 1));
  free_bit(sp, ZMAP, bit);
  if (bit < sp->s_zsearch) sp->s_zsearch = bit;

  /* Also tell libminixfs, so that 1) if it has a block for this bit, it can
   * mark it as clean, thus reducing useless writes, and 2) it can tell VM that
   * any previous inode association is to be broken for this block, so that the
   * block will not be mapped in erroneously later on.
   */
  assert(sp->s_log_zone_size == 0); /* otherwise we need a loop here.. */
  lmfs_free_block(dev, (block_t)numb);
}
