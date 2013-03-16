/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in fs_readwrite().
 *
 * The entry points into this file are
 *   write_map:    write a new zone into an inode
 *   clear_zone:   erase a zone in the middle of a file
 *   new_block:    acquire a new block
 *   zero_block:   overwrite a block with zeroes
 *
 */

#include "fs.h"
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include "buf.h"
#include "inode.h"
#include "super.h"


static void wr_indir(struct buf *bp, int index, zone_t zone);
static int empty_indir(struct buf *, struct super_block *);


/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
int write_map(rip, position, new_zone, op)
struct inode *rip;		/* pointer to inode to be changed */
off_t position;			/* file address to be mapped */
zone_t new_zone;		/* zone # to be inserted */
int op;				/* special actions */
{
/* Write a new zone into an inode.
 *
 * If op includes WMAP_FREE, free the data zone corresponding to that position
 * in the inode ('new_zone' is ignored then). Also free the indirect block
 * if that was the last entry in the indirect block.
 * Also free the double indirect block if that was the last entry in the
 * double indirect block.
 */
  int scale, ind_ex = 0, new_ind, new_dbl, 
        zones, nr_indirects, single, zindex, ex;
  zone_t z, z1, z2 = NO_ZONE, old_zone;
  register block_t b;
  long excess, zone;
  struct buf *bp_dindir = NULL, *bp = NULL;

  IN_MARKDIRTY(rip);
  scale = rip->i_sp->s_log_zone_size;		/* for zone-block conversion */
  	/* relative zone # to insert */
  zone = (position/rip->i_sp->s_block_size) >> scale;
  zones = rip->i_ndzones;	/* # direct zones in the inode */
  nr_indirects = rip->i_nindirs;/* # indirect zones per indirect block */

  /* Is 'position' to be found in the inode itself? */
  if (zone < zones) {
	zindex = (int) zone;	/* we need an integer here */
	if(rip->i_zone[zindex] != NO_ZONE && (op & WMAP_FREE)) {
		free_zone(rip->i_dev, rip->i_zone[zindex]);
		rip->i_zone[zindex] = NO_ZONE;
	} else {
		rip->i_zone[zindex] = new_zone;
	}
	return(OK);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - zones;	/* first Vx_NR_DZONES don't count */
  new_ind = FALSE;
  new_dbl = FALSE;

  if (excess < nr_indirects) {
	/* 'position' can be located via the single indirect block. */
	z1 = rip->i_zone[zones];	/* single indirect zone */
	single = TRUE;
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z2 = z = rip->i_zone[zones+1]) == NO_ZONE &&
	    !(op & WMAP_FREE)) {
		/* Create the double indirect block. */
		if ( (z = alloc_zone(rip->i_dev, rip->i_zone[0])) == NO_ZONE)
			return(err_code);
		rip->i_zone[zones+1] = z;
		new_dbl = TRUE;	/* set flag for later */
	}

	/* 'z' is zone number for double indirect block, either old
	 * or newly created.
	 * If there wasn't one and WMAP_FREE is set, 'z' is NO_ZONE.
	 */
	excess -= nr_indirects;	/* single indirect doesn't count */
	ind_ex = (int) (excess / nr_indirects);
	excess = excess % nr_indirects;
	if (ind_ex >= nr_indirects) return(EFBIG);

	if(z == NO_ZONE && (op & WMAP_FREE)) {
		/* WMAP_FREE and no double indirect block - then no
		 * single indirect block either.
		 */
		z1 = NO_ZONE;
	} else {
		b = (block_t) z << scale;
		bp_dindir = get_block(rip->i_dev, b,
			(new_dbl?NO_READ:NORMAL));
		if (new_dbl) zero_block(bp_dindir);
		z1 = rd_indir(bp_dindir, ind_ex);
	}
	single = FALSE;
  }

  /* z1 is now single indirect zone, or NO_ZONE; 'excess' is index.
   * We have to create the indirect zone if it's NO_ZONE. Unless
   * we're freeing (WMAP_FREE).
   */
  if (z1 == NO_ZONE && !(op & WMAP_FREE)) {
	z1 = alloc_zone(rip->i_dev, rip->i_zone[0]);
	if (single)
		rip->i_zone[zones] = z1; /* update inode w. single indirect */
	else
		wr_indir(bp_dindir, ind_ex, z1);	/* update dbl indir */

	new_ind = TRUE;
	/* If double ind, it is dirty. */
	if (bp_dindir != NULL) MARKDIRTY(bp_dindir);
	if (z1 == NO_ZONE) {
		/* Release dbl indirect blk. */
		put_block(bp_dindir, INDIRECT_BLOCK);
		return(err_code);	/* couldn't create single ind */
	}
  }

  /* z1 is indirect block's zone number (unless it's NO_ZONE when we're
   * freeing).
   */
  if(z1 != NO_ZONE) {
  	ex = (int) excess;			/* we need an int here */
	b = (block_t) z1 << scale;
	bp = get_block(rip->i_dev, b, (new_ind ? NO_READ : NORMAL) );
	if (new_ind) zero_block(bp);
	if(op & WMAP_FREE) {
		if((old_zone = rd_indir(bp, ex)) != NO_ZONE) {
			free_zone(rip->i_dev, old_zone);
			wr_indir(bp, ex, NO_ZONE);
		}

		/* Last reference in the indirect block gone? Then
		 * free the indirect block.
		 */
		if(empty_indir(bp, rip->i_sp)) {
			free_zone(rip->i_dev, z1);
			z1 = NO_ZONE;
			/* Update the reference to the indirect block to
			 * NO_ZONE - in the double indirect block if there
			 * is one, otherwise in the inode directly.
			 */
			if(single) {
				rip->i_zone[zones] = z1;
			} else {
				wr_indir(bp_dindir, ind_ex, z1);
				MARKDIRTY(bp_dindir);
			}
		}
	} else {
		wr_indir(bp, ex, new_zone);
	}
	/* z1 equals NO_ZONE only when we are freeing up the indirect block. */
	if(z1 == NO_ZONE) { MARKCLEAN(bp); } else { MARKDIRTY(bp); }
	put_block(bp, INDIRECT_BLOCK);
  }

  /* If the single indirect block isn't there (or was just freed),
   * see if we have to keep the double indirect block, if any.
   * If we don't have to keep it, don't bother writing it out.
   */
  if(z1 == NO_ZONE && !single && z2 != NO_ZONE &&
     empty_indir(bp_dindir, rip->i_sp)) {
     	MARKCLEAN(bp_dindir);
	free_zone(rip->i_dev, z2);
	rip->i_zone[zones+1] = NO_ZONE;
  }

  put_block(bp_dindir, INDIRECT_BLOCK);	/* release double indirect blk */

  return(OK);
}


/*===========================================================================*
 *				wr_indir				     *
 *===========================================================================*/
static void wr_indir(bp, index, zone)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
zone_t zone;			/* zone to write */
{
/* Given a pointer to an indirect block, write one entry. */

  struct super_block *sp;

  if(bp == NULL)
	panic("wr_indir() on NULL");

  sp = get_super(lmfs_dev(bp));	/* need super block to find file sys type */

  /* write a zone into an indirect block */
  if (sp->s_version == V1)
	b_v1_ind(bp)[index] = (zone1_t) conv2(sp->s_native, (int)  zone);
  else
	b_v2_ind(bp)[index] = (zone_t)  conv4(sp->s_native, (long) zone);
}


/*===========================================================================*
 *				empty_indir				     *
 *===========================================================================*/
static int empty_indir(bp, sb)
struct buf *bp;			/* pointer to indirect block */
struct super_block *sb;		/* superblock of device block resides on */
{
/* Return nonzero if the indirect block pointed to by bp contains
 * only NO_ZONE entries.
 */
  unsigned int i;
  for(i = 0; i < V2_INDIRECTS(sb->s_block_size); i++)
	if( b_v2_ind(bp)[i] != NO_ZONE)
		return(0);

  return(1);
}


/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
void clear_zone(rip, pos, flag)
register struct inode *rip;	/* inode to clear */
off_t pos;			/* points to block to clear */
int flag;			/* 1 if called by new_block, 0 otherwise */
{
/* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
 * a byte in the first block to be zeroed.  Clearzone() is called from 
 * fs_readwrite(), truncate_inode(), and new_block().
 */
  int scale;

  /* If the block size and zone size are the same, clear_zone() not needed. */
  scale = rip->i_sp->s_log_zone_size;
  assert(scale == 0);
  return;
}


/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
struct buf *new_block(rip, position)
register struct inode *rip;	/* pointer to inode */
off_t position;			/* file pointer */
{
/* Acquire a new block and return a pointer to it.  Doing so may require
 * allocating a complete zone, and then returning the initial block.
 * On the other hand, the current zone may still have some unused blocks.
 */

  register struct buf *bp;
  block_t b, base_block;
  zone_t z;
  zone_t zone_size;
  int scale, r;

  /* Is another block available in the current zone? */
  if ( (b = read_map(rip, position, 0)) == NO_BLOCK) {
	if (rip->i_zsearch == NO_ZONE) {
		/* First search for this file. Start looking from
		 * the file's first data zone to prevent fragmentation
		 */
		if ( (z = rip->i_zone[0]) == NO_ZONE) {
		 	/* No first zone for file either, let alloc_zone
		 	 * decide. */
			z = (zone_t) rip->i_sp->s_firstdatazone;
		}
	} else {
		/* searched before, start from last find */
		z = rip->i_zsearch;
	}
	if ( (z = alloc_zone(rip->i_dev, z)) == NO_ZONE) return(NULL);
	rip->i_zsearch = z;	/* store for next lookup */
	if ( (r = write_map(rip, position, z, 0)) != OK) {
		free_zone(rip->i_dev, z);
		err_code = r;
		return(NULL);
	}

	/* If we are not writing at EOF, clear the zone, just to be safe. */
	if ( position != rip->i_size) clear_zone(rip, position, 1);
	scale = rip->i_sp->s_log_zone_size;
	base_block = (block_t) z << scale;
	zone_size = (zone_t) rip->i_sp->s_block_size << scale;
	b = base_block + (block_t)((position % zone_size)/rip->i_sp->s_block_size);
  }

  bp = lmfs_get_block_ino(rip->i_dev, b, NO_READ, rip->i_num,
  	rounddown(position, rip->i_sp->s_block_size));
  zero_block(bp);
  return(bp);
}


/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
void zero_block(bp)
register struct buf *bp;	/* pointer to buffer to zero */
{
/* Zero a block. */
  ASSERT(lmfs_bytes(bp) > 0);
  ASSERT(bp->data);
  memset(b_data(bp), 0, (size_t) lmfs_bytes(bp));
  MARKDIRTY(bp);
}

