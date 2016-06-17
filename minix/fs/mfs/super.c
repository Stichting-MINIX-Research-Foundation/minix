/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   read_super:      read a superblock
 */

#include "fs.h"
#include <string.h>
#include <assert.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <minix/bdev.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include "const.h"

/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
bit_t alloc_bit(sp, map, origin)
struct super_block *sp;		/* the filesystem to allocate from */
int map;			/* IMAP (inode map) or ZMAP (zone map) */
bit_t origin;			/* number of bit to start searching at */
{
/* Allocate a bit from a bit map and return its bit number. */

  block_t start_block;		/* first bit block */
  block_t block;
  bit_t map_bits;		/* how many bits are there in the bit map? */
  short bit_blocks;		/* how many blocks are there in the bit map? */
  unsigned word, bcount;
  struct buf *bp;
  bitchunk_t *wptr, *wlim, k;
  bit_t i, b;

  if (sp->s_rd_only)
	panic("can't allocate bit on read-only filesys");

  if (map == IMAP) {
	start_block = START_BLOCK;
	map_bits = (bit_t) (sp->s_ninodes + 1);
	bit_blocks = sp->s_imap_blocks;
  } else {
	start_block = START_BLOCK + sp->s_imap_blocks;
	map_bits = (bit_t) (sp->s_zones - (sp->s_firstdatazone - 1));
	bit_blocks = sp->s_zmap_blocks;
  }

  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin >= map_bits) origin = 0;	/* for robustness */

  /* Locate the starting place. */
  block = (block_t) (origin / FS_BITS_PER_BLOCK(sp->s_block_size));
  word = (origin % FS_BITS_PER_BLOCK(sp->s_block_size)) / FS_BITCHUNK_BITS;

  /* Iterate over all blocks plus one, because we start in the middle. */
  bcount = bit_blocks + 1;
  do {
	bp = get_block(sp->s_dev, start_block + block, NORMAL);
	wlim = &b_bitmap(bp)[FS_BITMAP_CHUNKS(sp->s_block_size)];

	/* Iterate over the words in block. */
	for (wptr = &b_bitmap(bp)[word]; wptr < wlim; wptr++) {

		/* Does this word contain a free bit? */
		if (*wptr == (bitchunk_t) ~0) continue;

		/* Find and allocate the free bit. */
		k = (bitchunk_t) conv4(sp->s_native, (int) *wptr);
		for (i = 0; (k & (1 << i)) != 0; ++i) {}

		/* Bit number from the start of the bit map. */
		b = ((bit_t) block * FS_BITS_PER_BLOCK(sp->s_block_size))
		    + (wptr - &b_bitmap(bp)[0]) * FS_BITCHUNK_BITS
		    + i;

		/* Don't allocate bits beyond the end of the map. */
		if (b >= map_bits) break;

		/* Allocate and return bit number. */
		k |= 1 << i;
		*wptr = (bitchunk_t) conv4(sp->s_native, (int) k);
		MARKDIRTY(bp);
		put_block(bp);
		if(map == ZMAP) {
			used_zones++;
			lmfs_change_blockusage(1);
		}
		return(b);
	}
	put_block(bp);
	if (++block >= (unsigned int) bit_blocks) /* last block, wrap around */
		block = 0;
	word = 0;
  } while (--bcount > 0);
  return(NO_BIT);		/* no bit could be allocated */
}

/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
void free_bit(sp, map, bit_returned)
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
	panic("can't free bit on read-only filesys");

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

  k = (bitchunk_t) conv4(sp->s_native, (int) b_bitmap(bp)[word]);
  if (!(k & mask)) {
  	if (map == IMAP) panic("tried to free unused inode");
  	else panic("tried to free unused block: %u", bit_returned);
  }

  k &= ~mask;
  b_bitmap(bp)[word] = (bitchunk_t) conv4(sp->s_native, (int) k);
  MARKDIRTY(bp);

  put_block(bp);

  if(map == ZMAP) {
	used_zones--;
	lmfs_change_blockusage(-1);
  }
}

/*===========================================================================*
 *				get_block_size				     *
 *===========================================================================*/
unsigned int get_block_size(dev_t dev)
{
  if (dev == NO_DEV)
  	panic("request for block size of NO_DEV");

  return(lmfs_fs_block_size());
}


/*===========================================================================*
 *				rw_super				     *
 *===========================================================================*/
static int rw_super(struct super_block *sp, int writing)
{
/* Read/write a superblock. */
  dev_t save_dev = sp->s_dev;
  struct buf *bp;
  char *sbbuf;
  int r;

/* To keep the 1kb on disk clean, only read/write up to and including
 * this field.
 */
#define LAST_ONDISK_FIELD s_disk_version
  int ondisk_bytes = (int) ((char *) &sp->LAST_ONDISK_FIELD - (char *) sp)
  	+ sizeof(sp->LAST_ONDISK_FIELD);

  assert(ondisk_bytes > 0);
  assert(ondisk_bytes < PAGE_SIZE);
  assert(ondisk_bytes < sizeof(struct super_block));

  if (sp->s_dev == NO_DEV)
  	panic("request for super_block of NO_DEV");

  /* we rely on the cache blocksize, before reading the
   * superblock, being big enough that our complete superblock
   * is in block 0.
   *
   * copy between the disk block and the superblock buffer (depending
   * on direction). mark the disk block dirty if the copy is into the
   * disk block.
   */
  assert(lmfs_fs_block_size() >= sizeof(struct super_block) + SUPER_BLOCK_BYTES);
  assert(SUPER_BLOCK_BYTES >= sizeof(struct super_block));
  assert(SUPER_BLOCK_BYTES >= ondisk_bytes);

  /* Unlike accessing any other block, failure to read the superblock is a
   * somewhat legitimate use case: it may happen when trying to mount a
   * zero-sized partition.  In that case, we'd rather faily cleanly than
   * crash the MFS service.
   */
  if ((r = lmfs_get_block(&bp, sp->s_dev, 0, NORMAL)) != OK) {
	if (writing)
		panic("get_block of superblock failed: %d", r);
	else
		return r;
  }

  /* sbbuf points to the disk block at the superblock offset */
  sbbuf = (char *) b_data(bp) + SUPER_BLOCK_BYTES;

  if(writing) {
  	memset(b_data(bp), 0, lmfs_fs_block_size());
  	memcpy(sbbuf, sp, ondisk_bytes);
	lmfs_markdirty(bp);
  } else {
	memset(sp, 0, sizeof(*sp));
  	memcpy(sp, sbbuf, ondisk_bytes);
  	sp->s_dev = save_dev;
  }

  put_block(bp);
  lmfs_flushall();

  return OK;
}

/*===========================================================================*
 *				read_super				     *
 *===========================================================================*/
int read_super(struct super_block *sp)
{
  unsigned int magic;
  block_t offset;
  int version, native, r;

  if((r=rw_super(sp, 0)) != OK)
  	return r;

  magic = sp->s_magic;		/* determines file system type */

  if(magic == SUPER_V2 || magic == SUPER_MAGIC) {
	printf("MFS: only supports V3 filesystems.\n");
	return EINVAL;
  }

  /* Get file system version and type - only support v3. */
  if(magic != SUPER_V3) {
	return EINVAL;
  }
  version = V3;
  native = 1;

  /* If the super block has the wrong byte order, swap the fields; the magic
   * number doesn't need conversion. */
  sp->s_ninodes =           (ino_t) conv4(native, (int) sp->s_ninodes);
  sp->s_nzones =          (zone1_t) conv2(native, (int) sp->s_nzones);
  sp->s_imap_blocks =       (short) conv2(native, (int) sp->s_imap_blocks);
  sp->s_zmap_blocks =       (short) conv2(native, (int) sp->s_zmap_blocks);
  sp->s_firstdatazone_old =(zone1_t)conv2(native,(int)sp->s_firstdatazone_old);
  sp->s_log_zone_size =     (short) conv2(native, (int) sp->s_log_zone_size);
  sp->s_max_size =          (off_t) conv4(native, sp->s_max_size);
  sp->s_zones =             (zone_t)conv4(native, sp->s_zones);

  /* Zones consisting of multiple blocks are longer supported, so fail as early
   * as possible. There is still a lot of code cleanup to do here, though.
   */
  if (sp->s_log_zone_size != 0) {
	printf("MFS: block and zone sizes are different\n");
	return EINVAL;
  }

  /* Calculate some other numbers that depend on the version here too, to
   * hide some of the differences.
   */
  assert(version == V3);
  sp->s_block_size = (unsigned short) conv2(native,(int) sp->s_block_size);
  if (sp->s_block_size < PAGE_SIZE) {
 	return EINVAL;
  }
  sp->s_inodes_per_block = V2_INODES_PER_BLOCK(sp->s_block_size);
  sp->s_ndzones = V2_NR_DZONES;
  sp->s_nindirs = V2_INDIRECTS(sp->s_block_size);

  /* For even larger disks, a similar problem occurs with s_firstdatazone.
   * If the on-disk field contains zero, we assume that the value was too
   * large to fit, and compute it on the fly.
   */
  if (sp->s_firstdatazone_old == 0) {
	offset = START_BLOCK + sp->s_imap_blocks + sp->s_zmap_blocks;
	offset += (sp->s_ninodes + sp->s_inodes_per_block - 1) /
		sp->s_inodes_per_block;

	sp->s_firstdatazone = (offset + (1 << sp->s_log_zone_size) - 1) >>
		sp->s_log_zone_size;
  } else {
	sp->s_firstdatazone = (zone_t) sp->s_firstdatazone_old;
  }

  if (sp->s_block_size < PAGE_SIZE) 
  	return(EINVAL);
  
  if ((sp->s_block_size % 512) != 0) 
  	return(EINVAL);
  
  if (SUPER_SIZE > sp->s_block_size) 
  	return(EINVAL);
  
  if ((sp->s_block_size % V2_INODE_SIZE) != 0) {
  	return(EINVAL);
  }

  /* Limit s_max_size to LONG_MAX */
  if ((unsigned long)sp->s_max_size > LONG_MAX) 
	sp->s_max_size = LONG_MAX;

  sp->s_isearch = 0;		/* inode searches initially start at 0 */
  sp->s_zsearch = 0;		/* zone searches initially start at 0 */
  sp->s_version = version;
  sp->s_native  = native;

  /* Make a few basic checks to see if super block looks reasonable. */
  if (sp->s_imap_blocks < 1 || sp->s_zmap_blocks < 1
				|| sp->s_ninodes < 1 || sp->s_zones < 1
				|| sp->s_firstdatazone <= 4
				|| sp->s_firstdatazone >= sp->s_zones
				|| (unsigned) sp->s_log_zone_size > 4) {
  	printf("not enough imap or zone map blocks, \n");
  	printf("or not enough inodes, or not enough zones, \n"
  		"or invalid first data zone, or zone size too large\n");
	return(EINVAL);
  }


  /* Check any flags we don't understand but are required to. Currently
   * these don't exist so all such unknown bits are fatal.
   */
  if(sp->s_flags & MFSFLAG_MANDATORY_MASK) {
  	printf("MFS: unsupported feature flags on this FS.\n"
		"Please use a newer MFS to mount it.\n");
	return(EINVAL);
  }

  return(OK);
}

/*===========================================================================*
 *				write_super				     *
 *===========================================================================*/
int write_super(struct super_block *sp)
{
  if(sp->s_rd_only)
  	panic("can't write superblock of readonly filesystem");
  return rw_super(sp, 1);
}
