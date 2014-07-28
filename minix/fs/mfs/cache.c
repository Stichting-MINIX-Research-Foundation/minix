/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   get_block:	  request to fetch a block for reading or writing from cache
 *   put_block:	  return a block previously requested with get_block
 *   alloc_zone:  allocate a new zone (to increase the length of a file)
 *   free_zone:	  release a zone (when a file is removed)
 *   invalidate:  remove all the cache blocks on some device
 *
 * Private functions:
 *   read_block:    read or write a block from the disk itself
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
  sp = get_super(dev);

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
  sp = get_super(dev);
  if (numb < sp->s_firstdatazone || numb >= sp->s_zones) return;
  bit = (bit_t) (numb - (zone_t) (sp->s_firstdatazone - 1));
  free_bit(sp, ZMAP, bit);
  if (bit < sp->s_zsearch) sp->s_zsearch = bit;
}


