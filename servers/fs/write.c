/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in read_write().
 *
 * The entry points into this file are
 *   do_write:     call read_write to perform the WRITE system call
 *   clear_zone:   erase a zone in the middle of a file
 *   new_block:    acquire a new block
 */

#include "fs.h"
#include <string.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "super.h"

FORWARD _PROTOTYPE( int write_map, (struct inode *rip, off_t position,
			zone_t new_zone)				);

FORWARD _PROTOTYPE( void wr_indir, (struct buf *bp, int index, zone_t zone) );

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
PUBLIC int do_write()
{
/* Perform the write(fd, buffer, nbytes) system call. */

  return(read_write(WRITING));
}

/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
PRIVATE int write_map(rip, position, new_zone)
register struct inode *rip;	/* pointer to inode to be changed */
off_t position;			/* file address to be mapped */
zone_t new_zone;		/* zone # to be inserted */
{
/* Write a new zone into an inode. */
  int scale, ind_ex, new_ind, new_dbl, zones, nr_indirects, single, zindex, ex;
  zone_t z, z1;
  register block_t b;
  long excess, zone;
  struct buf *bp;

  rip->i_dirt = DIRTY;		/* inode will be changed */
  bp = NIL_BUF;
  scale = rip->i_sp->s_log_zone_size;		/* for zone-block conversion */
  	/* relative zone # to insert */
  zone = (position/rip->i_sp->s_block_size) >> scale;
  zones = rip->i_ndzones;	/* # direct zones in the inode */
  nr_indirects = rip->i_nindirs;/* # indirect zones per indirect block */

  /* Is 'position' to be found in the inode itself? */
  if (zone < zones) {
	zindex = (int) zone;	/* we need an integer here */
	rip->i_zone[zindex] = new_zone;
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
	if ( (z = rip->i_zone[zones+1]) == NO_ZONE) {
		/* Create the double indirect block. */
		if ( (z = alloc_zone(rip->i_dev, rip->i_zone[0])) == NO_ZONE)
			return(err_code);
		rip->i_zone[zones+1] = z;
		new_dbl = TRUE;	/* set flag for later */
	}

	/* Either way, 'z' is zone number for double indirect block. */
	excess -= nr_indirects;	/* single indirect doesn't count */
	ind_ex = (int) (excess / nr_indirects);
	excess = excess % nr_indirects;
	if (ind_ex >= nr_indirects) return(EFBIG);
	b = (block_t) z << scale;
	bp = get_block(rip->i_dev, b, (new_dbl ? NO_READ : NORMAL));
	if (new_dbl) zero_block(bp);
	z1 = rd_indir(bp, ind_ex);
	single = FALSE;
  }

  /* z1 is now single indirect zone; 'excess' is index. */
  if (z1 == NO_ZONE) {
	/* Create indirect block and store zone # in inode or dbl indir blk. */
	z1 = alloc_zone(rip->i_dev, rip->i_zone[0]);
	if (single)
		rip->i_zone[zones] = z1;	/* update inode */
	else
		wr_indir(bp, ind_ex, z1);	/* update dbl indir */

	new_ind = TRUE;
	if (bp != NIL_BUF) bp->b_dirt = DIRTY;	/* if double ind, it is dirty*/
	if (z1 == NO_ZONE) {
		put_block(bp, INDIRECT_BLOCK);	/* release dbl indirect blk */
		return(err_code);	/* couldn't create single ind */
	}
  }
  put_block(bp, INDIRECT_BLOCK);	/* release double indirect blk */

  /* z1 is indirect block's zone number. */
  b = (block_t) z1 << scale;
  bp = get_block(rip->i_dev, b, (new_ind ? NO_READ : NORMAL) );
  if (new_ind) zero_block(bp);
  ex = (int) excess;			/* we need an int here */
  wr_indir(bp, ex, new_zone);
  bp->b_dirt = DIRTY;
  put_block(bp, INDIRECT_BLOCK);

  return(OK);
}

/*===========================================================================*
 *				wr_indir				     *
 *===========================================================================*/
PRIVATE void wr_indir(bp, index, zone)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
zone_t zone;			/* zone to write */
{
/* Given a pointer to an indirect block, write one entry. */

  struct super_block *sp;

  sp = get_super(bp->b_dev);	/* need super block to find file sys type */

  /* write a zone into an indirect block */
  if (sp->s_version == V1)
	bp->b_v1_ind[index] = (zone1_t) conv2(sp->s_native, (int)  zone);
  else
	bp->b_v2_ind[index] = (zone_t)  conv4(sp->s_native, (long) zone);
}

/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
PUBLIC void clear_zone(rip, pos, flag)
register struct inode *rip;	/* inode to clear */
off_t pos;			/* points to block to clear */
int flag;			/* 0 if called by read_write, 1 by new_block */
{
/* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
 * a byte in the first block to be zeroed.  Clearzone() is called from 
 * read_write and new_block().
 */

  register struct buf *bp;
  register block_t b, blo, bhi;
  register off_t next;
  register int scale;
  register zone_t zone_size;

  /* If the block size and zone size are the same, clear_zone() not needed. */
  scale = rip->i_sp->s_log_zone_size;
  if (scale == 0) return;

  zone_size = (zone_t) rip->i_sp->s_block_size << scale;
  if (flag == 1) pos = (pos/zone_size) * zone_size;
  next = pos + rip->i_sp->s_block_size - 1;

  /* If 'pos' is in the last block of a zone, do not clear the zone. */
  if (next/zone_size != pos/zone_size) return;
  if ( (blo = read_map(rip, next)) == NO_BLOCK) return;
  bhi = (  ((blo>>scale)+1) << scale)   - 1;

  /* Clear all the blocks between 'blo' and 'bhi'. */
  for (b = blo; b <= bhi; b++) {
	bp = get_block(rip->i_dev, b, NO_READ);
	zero_block(bp);
	put_block(bp, FULL_DATA_BLOCK);
  }
}

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
PUBLIC struct buf *new_block(rip, position)
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
  struct super_block *sp;

  /* Is another block available in the current zone? */
  if ( (b = read_map(rip, position)) == NO_BLOCK) {
	/* Choose first zone if possible. */
	/* Lose if the file is nonempty but the first zone number is NO_ZONE
	 * corresponding to a zone full of zeros.  It would be better to
	 * search near the last real zone.
	 */
	if (rip->i_zone[0] == NO_ZONE) {
		sp = rip->i_sp;
		z = sp->s_firstdatazone;
	} else {
		z = rip->i_zone[0];	/* hunt near first zone */
	}
	if ( (z = alloc_zone(rip->i_dev, z)) == NO_ZONE) return(NIL_BUF);
	if ( (r = write_map(rip, position, z)) != OK) {
		free_zone(rip->i_dev, z);
		err_code = r;
		return(NIL_BUF);
	}

	/* If we are not writing at EOF, clear the zone, just to be safe. */
	if ( position != rip->i_size) clear_zone(rip, position, 1);
	scale = rip->i_sp->s_log_zone_size;
	base_block = (block_t) z << scale;
	zone_size = (zone_t) rip->i_sp->s_block_size << scale;
	b = base_block + (block_t)((position % zone_size)/rip->i_sp->s_block_size);
  }

  bp = get_block(rip->i_dev, b, NO_READ);
  zero_block(bp);
  return(bp);
}

/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
PUBLIC void zero_block(bp)
register struct buf *bp;	/* pointer to buffer to zero */
{
/* Zero a block. */
  memset(bp->b_data, 0, MAX_BLOCK_SIZE);
  bp->b_dirt = DIRTY;
}
