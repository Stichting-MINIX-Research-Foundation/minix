/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   get_super:       search the 'superblock' table for a device
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   read_super:      read a superblock
 */

#include "fs.h"
#include <string.h>
#include <minix/com.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include "const.h"

/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
PUBLIC bit_t alloc_bit(sp, map, origin)
struct super_block *sp;		/* the filesystem to allocate from */
int map;			/* IMAP (inode map) or ZMAP (zone map) */
bit_t origin;			/* number of bit to start searching at */
{
/* Allocate a bit from a bit map and return its bit number. */

  block_t start_block;		/* first bit block */
  bit_t map_bits;		/* how many bits are there in the bit map? */
  unsigned bit_blocks;		/* how many blocks are there in the bit map? */
  unsigned block, word, bcount;
  struct buf *bp;
  bitchunk_t *wptr, *wlim, k;
  bit_t i, b;

  if (sp->s_rd_only)
	panic(__FILE__,"can't allocate bit on read-only filesys.", NO_NUM);

  if (map == IMAP) {
	start_block = START_BLOCK;
	map_bits = sp->s_ninodes + 1;
	bit_blocks = sp->s_imap_blocks;
  } else {
	start_block = START_BLOCK + sp->s_imap_blocks;
	map_bits = sp->s_zones - (sp->s_firstdatazone - 1);
	bit_blocks = sp->s_zmap_blocks;
  }

  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin >= map_bits) origin = 0;	/* for robustness */

  /* Locate the starting place. */
  block = origin / FS_BITS_PER_BLOCK(sp->s_block_size);
  word = (origin % FS_BITS_PER_BLOCK(sp->s_block_size)) / FS_BITCHUNK_BITS;

  /* Iterate over all blocks plus one, because we start in the middle. */
  bcount = bit_blocks + 1;
  do {
	bp = get_block(sp->s_dev, start_block + block, NORMAL);
	wlim = &bp->b_bitmap[FS_BITMAP_CHUNKS(sp->s_block_size)];

	/* Iterate over the words in block. */
	for (wptr = &bp->b_bitmap[word]; wptr < wlim; wptr++) {

		/* Does this word contain a free bit? */
		if (*wptr == (bitchunk_t) ~0) continue;

		/* Find and allocate the free bit. */
		k = conv2(sp->s_native, (int) *wptr);
		for (i = 0; (k & (1 << i)) != 0; ++i) {}

		/* Bit number from the start of the bit map. */
		b = ((bit_t) block * FS_BITS_PER_BLOCK(sp->s_block_size))
		    + (wptr - &bp->b_bitmap[0]) * FS_BITCHUNK_BITS
		    + i;

		/* Don't allocate bits beyond the end of the map. */
		if (b >= map_bits) break;

		/* Allocate and return bit number. */
		k |= 1 << i;
		*wptr = conv2(sp->s_native, (int) k);
		bp->b_dirt = DIRTY;
		put_block(bp, MAP_BLOCK);
		return(b);
	}
	put_block(bp, MAP_BLOCK);
	if (++block >= bit_blocks) block = 0;	/* last block, wrap around */
	word = 0;
  } while (--bcount > 0);
  return(NO_BIT);		/* no bit could be allocated */
}

/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
PUBLIC void free_bit(sp, map, bit_returned)
struct super_block *sp;		/* the filesystem to operate on */
int map;			/* IMAP (inode map) or ZMAP (zone map) */
bit_t bit_returned;		/* number of bit to insert into the map */
{
/* Return a zone or inode by turning off its bitmap bit. */

  unsigned block, word, bit;
  struct buf *bp;
  bitchunk_t k, mask;
  block_t start_block;

  if (sp->s_rd_only)
	panic(__FILE__,"can't free bit on read-only filesys.", NO_NUM);

  if (map == IMAP) {
	start_block = START_BLOCK;
  } else {
	start_block = START_BLOCK + sp->s_imap_blocks;
  }
  block = bit_returned / FS_BITS_PER_BLOCK(sp->s_block_size);
  word = (bit_returned % FS_BITS_PER_BLOCK(sp->s_block_size))
  	 / FS_BITCHUNK_BITS;

  bit = bit_returned % FS_BITCHUNK_BITS;
  mask = 1 << bit;

  bp = get_block(sp->s_dev, start_block + block, NORMAL);

  k = conv2(sp->s_native, (int) bp->b_bitmap[word]);
  if (!(k & mask)) {
	panic(__FILE__,map == IMAP ? "tried to free unused inode" :
	      "tried to free unused block", NO_NUM);
  }

  k &= ~mask;
  bp->b_bitmap[word] = conv2(sp->s_native, (int) k);
  bp->b_dirt = DIRTY;

  put_block(bp, MAP_BLOCK);
}

/*===========================================================================*
 *				get_super				     *
 *===========================================================================*/
PUBLIC struct super_block *get_super(dev)
dev_t dev;			/* device number whose super_block is sought */
{
/* Search the superblock table for this device.  It is supposed to be there. */

  register struct super_block *sp;

  if (dev == NO_DEV)
  	panic(__FILE__,"request for super_block of NO_DEV", NO_NUM);

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(sp);

  /* Search failed.  Something wrong. */
  panic(__FILE__,"can't find superblock for device (in decimal)", (int) dev);

  return(NIL_SUPER);		/* to keep the compiler and lint quiet */
}

/*===========================================================================*
 *				get_block_size				     *
 *===========================================================================*/
PUBLIC int get_block_size(dev_t dev)
{
/* Search the superblock table for this device. */

  register struct super_block *sp;

  if (dev == NO_DEV)
  	panic(__FILE__,"request for block size of NO_DEV", NO_NUM);

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
	if (sp->s_dev == dev) {
		return(sp->s_block_size);
	}
  }

  /* no mounted filesystem? use this block size then. */
  return MIN_BLOCK_SIZE;
}

/*===========================================================================*
 *				mounted					     *
 *===========================================================================*/
PUBLIC int mounted(rip)
register struct inode *rip;	/* pointer to inode */
{
/* Report on whether the given inode is on a mounted (or ROOT) file system. */

  register struct super_block *sp;
  register dev_t dev;

  dev = (dev_t) rip->i_zone[0];
  if (dev == root_dev) return(TRUE);	/* inode is on root file system */

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(TRUE);

  return(FALSE);
}

/*===========================================================================*
 *				read_super				     *
 *===========================================================================*/
PUBLIC int read_super(sp)
register struct super_block *sp; /* pointer to a superblock */
{
/* Read a superblock. */
  dev_t dev;
  int magic;
  int version, native, r;
  static char sbbuf[MIN_BLOCK_SIZE];

  dev = sp->s_dev;		/* save device (will be overwritten by copy) */
  if (dev == NO_DEV)
  	panic(__FILE__,"request for super_block of NO_DEV", NO_NUM);
  r = dev_io(DEV_READ, dev, FS_PROC_NR,
  	sbbuf, SUPER_BLOCK_BYTES, MIN_BLOCK_SIZE, 0);
  if (r != MIN_BLOCK_SIZE) {
  	return EINVAL;
  }
  memcpy(sp, sbbuf, sizeof(*sp));
  sp->s_dev = NO_DEV;		/* restore later */
  magic = sp->s_magic;		/* determines file system type */

  /* Get file system version and type. */
  if (magic == SUPER_MAGIC || magic == conv2(BYTE_SWAP, SUPER_MAGIC)) {
	version = V1;
	native  = (magic == SUPER_MAGIC);
  } else if (magic == SUPER_V2 || magic == conv2(BYTE_SWAP, SUPER_V2)) {
	version = V2;
	native  = (magic == SUPER_V2);
  } else if (magic == SUPER_V3) {
	version = V3;
  	native = 1;
  } else {
	return(EINVAL);
  }

  /* If the super block has the wrong byte order, swap the fields; the magic
   * number doesn't need conversion. */
  sp->s_ninodes =       conv4(native, sp->s_ninodes);
  sp->s_nzones =        conv2(native, (int) sp->s_nzones);
  sp->s_imap_blocks =   conv2(native, (int) sp->s_imap_blocks);
  sp->s_zmap_blocks =   conv2(native, (int) sp->s_zmap_blocks);
  sp->s_firstdatazone = conv2(native, (int) sp->s_firstdatazone);
  sp->s_log_zone_size = conv2(native, (int) sp->s_log_zone_size);
  sp->s_max_size =      conv4(native, sp->s_max_size);
  sp->s_zones =         conv4(native, sp->s_zones);

  /* In V1, the device size was kept in a short, s_nzones, which limited
   * devices to 32K zones.  For V2, it was decided to keep the size as a
   * long.  However, just changing s_nzones to a long would not work, since
   * then the position of s_magic in the super block would not be the same
   * in V1 and V2 file systems, and there would be no way to tell whether
   * a newly mounted file system was V1 or V2.  The solution was to introduce
   * a new variable, s_zones, and copy the size there.
   *
   * Calculate some other numbers that depend on the version here too, to
   * hide some of the differences.
   */
  if (version == V1) {
  	sp->s_block_size = STATIC_BLOCK_SIZE;
	sp->s_zones = sp->s_nzones;	/* only V1 needs this copy */
	sp->s_inodes_per_block = V1_INODES_PER_BLOCK;
	sp->s_ndzones = V1_NR_DZONES;
	sp->s_nindirs = V1_INDIRECTS;
  } else {
  	if (version == V2)
  		sp->s_block_size = STATIC_BLOCK_SIZE;
  	if (sp->s_block_size < MIN_BLOCK_SIZE)
  		return EINVAL;
	sp->s_inodes_per_block = V2_INODES_PER_BLOCK(sp->s_block_size);
	sp->s_ndzones = V2_NR_DZONES;
	sp->s_nindirs = V2_INDIRECTS(sp->s_block_size);
  }

  if (sp->s_block_size < MIN_BLOCK_SIZE) {
  	return EINVAL;
  }
  if (sp->s_block_size > MAX_BLOCK_SIZE) {
  	printf("Filesystem block size is %d kB; maximum filesystem\n"
 	"block size is %d kB. This limit can be increased by recompiling.\n",
  	sp->s_block_size/1024, MAX_BLOCK_SIZE/1024);
  	return EINVAL;
  }
  if ((sp->s_block_size % 512) != 0) {
  	return EINVAL;
  }
  if (SUPER_SIZE > sp->s_block_size) {
  	return EINVAL;
  }
  if ((sp->s_block_size % V2_INODE_SIZE) != 0 ||
     (sp->s_block_size % V1_INODE_SIZE) != 0) {
  	return EINVAL;
  }

  sp->s_isearch = 0;		/* inode searches initially start at 0 */
  sp->s_zsearch = 0;		/* zone searches initially start at 0 */
  sp->s_version = version;
  sp->s_native  = native;

  /* Make a few basic checks to see if super block looks reasonable. */
  if (sp->s_imap_blocks < 1 || sp->s_zmap_blocks < 1
				|| sp->s_ninodes < 1 || sp->s_zones < 1
				|| (unsigned) sp->s_log_zone_size > 4) {
  	printf("not enough imap or zone map blocks, \n");
  	printf("or not enough inodes, or not enough zones, "
  		"or zone size too large\n");
	return(EINVAL);
  }
  sp->s_dev = dev;		/* restore device number */
  return(OK);
}
