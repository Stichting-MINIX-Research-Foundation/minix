/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in fs_readwrite().
 *
 * The entry points into this file are
 *   write_map:    write a new block into an inode
 *   new_block:    acquire a new block
 *   zero_block:   overwrite a block with zeroes
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <string.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

static void wr_indir(struct buf *bp, int index, block_t block);
static int empty_indir(struct buf *, struct super_block *);

/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
int write_map(rip, position, new_block, op)
struct inode *rip;		/* pointer to inode to be changed */
off_t position;			/* file address to be mapped */
block_t new_block;		/* block # to be inserted */
int op;				/* special actions */
{
/* Write a new block into an inode.
 *
 * If op includes WMAP_FREE, free the block corresponding to that position
 * in the inode ('new_block' is ignored then). Also free the indirect block
 * if that was the last entry in the indirect block.
 * Also free the double/triple indirect block if that was the last entry in
 * the double/triple indirect block.
 * It's the only function which should take care about rip->i_blocks counter.
 */
  int index1 = 0, index2 = 0, index3 = 0; /* indexes in single..triple indirect blocks */
  long excess, block_pos;
  char new_ind = 0, new_dbl = 0, new_triple = 0;
  int single = 0, triple = 0;
  block_t old_block = NO_BLOCK, b1 = NO_BLOCK, b2 = NO_BLOCK, b3 = NO_BLOCK;
  struct buf *bp = NULL,
             *bp_dindir = NULL,
             *bp_tindir = NULL;
  static char first_time = TRUE;
  static long addr_in_block;
  static long addr_in_block2;
  static long doub_ind_s;
  static long triple_ind_s;
  static long out_range_s;

  if (first_time) {
	addr_in_block = rip->i_sp->s_block_size / BLOCK_ADDRESS_BYTES;
	addr_in_block2 = addr_in_block * addr_in_block;
	doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
	triple_ind_s = doub_ind_s + addr_in_block2;
	out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
	first_time = FALSE;
  }

  block_pos = position / rip->i_sp->s_block_size; /* relative blk # in file */
  rip->i_dirt = IN_DIRTY;		/* inode will be changed */

  /* Is 'position' to be found in the inode itself? */
  if (block_pos < EXT2_NDIR_BLOCKS) {
	if (rip->i_block[block_pos] != NO_BLOCK && (op & WMAP_FREE)) {
		free_block(rip->i_sp, rip->i_block[block_pos]);
		rip->i_block[block_pos] = NO_BLOCK;
		rip->i_blocks -= rip->i_sp->s_sectors_in_block;
	} else {
		rip->i_block[block_pos] = new_block;
		rip->i_blocks += rip->i_sp->s_sectors_in_block;
	}
	return(OK);
  }

  /* It is not in the inode, so it must be single, double or triple indirect */
  if (block_pos < doub_ind_s) {
      b1 = rip->i_block[EXT2_NDIR_BLOCKS]; /* addr of single indirect block */
      index1 = block_pos - EXT2_NDIR_BLOCKS;
      single = TRUE;
  } else if (block_pos >= out_range_s) { /* TODO: do we need it? */
	return(EFBIG);
  } else {
	/* double or triple indirect block. At first if it's triple,
	 * find double indirect block.
	 */
	excess = block_pos - doub_ind_s;
	b2 = rip->i_block[EXT2_DIND_BLOCK];
	if (block_pos >= triple_ind_s) {
		b3 = rip->i_block[EXT2_TIND_BLOCK];
		if (b3 == NO_BLOCK && !(op & WMAP_FREE)) {
		/* Create triple indirect block. */
			if ( (b3 = alloc_block(rip, rip->i_bsearch) ) == NO_BLOCK) {
				ext2_debug("failed to allocate tblock near %d\n", rip->i_block[0]);
				return(ENOSPC);
			}
			rip->i_block[EXT2_TIND_BLOCK] = b3;
			rip->i_blocks += rip->i_sp->s_sectors_in_block;
			new_triple = TRUE;
		}
		/* 'b3' is block number for triple indirect block, either old
		 * or newly created.
		 * If there wasn't one and WMAP_FREE is set, 'b3' is NO_BLOCK.
		 */
		if (b3 == NO_BLOCK && (op & WMAP_FREE)) {
		/* WMAP_FREE and no triple indirect block - then no
		 * double and single indirect blocks either.
		 */
			b1 = b2 = NO_BLOCK;
		} else {
			bp_tindir = get_block(rip->i_dev, b3, (new_triple ? NO_READ : NORMAL));
			if (new_triple) {
				zero_block(bp_tindir);
				lmfs_markdirty(bp_tindir);
			}
			excess = block_pos - triple_ind_s;
			index3 = excess / addr_in_block2;
			b2 = rd_indir(bp_tindir, index3);
			excess = excess % addr_in_block2;
		}
		triple = TRUE;
	}

	if (b2 == NO_BLOCK && !(op & WMAP_FREE)) {
	/* Create the double indirect block. */
		if ( (b2 = alloc_block(rip, rip->i_bsearch) ) == NO_BLOCK) {
			/* Release triple ind blk. */
			put_block(bp_tindir, INDIRECT_BLOCK);
			ext2_debug("failed to allocate dblock near %d\n", rip->i_block[0]);
			return(ENOSPC);
		}
		if (triple) {
			wr_indir(bp_tindir, index3, b2);  /* update triple indir */
			lmfs_markdirty(bp_tindir);
		} else {
			rip->i_block[EXT2_DIND_BLOCK] = b2;
		}
		rip->i_blocks += rip->i_sp->s_sectors_in_block;
		new_dbl = TRUE; /* set flag for later */
	}

	/* 'b2' is block number for double indirect block, either old
	 * or newly created.
	 * If there wasn't one and WMAP_FREE is set, 'b2' is NO_BLOCK.
	 */
	if (b2 == NO_BLOCK && (op & WMAP_FREE)) {
	/* WMAP_FREE and no double indirect block - then no
	 * single indirect block either.
	 */
		b1 = NO_BLOCK;
	} else {
		bp_dindir = get_block(rip->i_dev, b2, (new_dbl ? NO_READ : NORMAL));
		if (new_dbl) {
			zero_block(bp_dindir);
			lmfs_markdirty(bp_dindir);
		}
		index2 = excess / addr_in_block;
		b1 = rd_indir(bp_dindir, index2);
		index1 = excess % addr_in_block;
	}
	single = FALSE;
  }

  /* b1 is now single indirect block or NO_BLOCK; 'index' is index.
   * We have to create the indirect block if it's NO_BLOCK. Unless
   * we're freing (WMAP_FREE).
   */
  if (b1 == NO_BLOCK && !(op & WMAP_FREE)) {
	if ( (b1 = alloc_block(rip, rip->i_bsearch) ) == NO_BLOCK) {
		/* Release dbl and triple indirect blks. */
		put_block(bp_dindir, INDIRECT_BLOCK);
		put_block(bp_tindir, INDIRECT_BLOCK);
		ext2_debug("failed to allocate dblock near %d\n", rip->i_block[0]);
		return(ENOSPC);
	}
	if (single) {
		rip->i_block[EXT2_NDIR_BLOCKS] = b1; /* update inode single indirect */
	} else {
		wr_indir(bp_dindir, index2, b1);  /* update dbl indir */
		lmfs_markdirty(bp_dindir);
	}
	rip->i_blocks += rip->i_sp->s_sectors_in_block;
	new_ind = TRUE;
  }

  /* b1 is indirect block's number (unless it's NO_BLOCK when we're
   * freeing).
   */
  if (b1 != NO_BLOCK) {
	bp = get_block(rip->i_dev, b1, (new_ind ? NO_READ : NORMAL) );
	if (new_ind)
		zero_block(bp);
	if (op & WMAP_FREE) {
		if ((old_block = rd_indir(bp, index1)) != NO_BLOCK) {
			free_block(rip->i_sp, old_block);
			rip->i_blocks -= rip->i_sp->s_sectors_in_block;
			wr_indir(bp, index1, NO_BLOCK);
		}

		/* Last reference in the indirect block gone? Then
		 * free the indirect block.
		 */
		if (empty_indir(bp, rip->i_sp)) {
			free_block(rip->i_sp, b1);
			rip->i_blocks -= rip->i_sp->s_sectors_in_block;
			b1 = NO_BLOCK;
			/* Update the reference to the indirect block to
			 * NO_BLOCK - in the double indirect block if there
			 * is one, otherwise in the inode directly.
			 */
			if (single) {
				rip->i_block[EXT2_NDIR_BLOCKS] = b1;
			} else {
				wr_indir(bp_dindir, index2, b1);
				lmfs_markdirty(bp_dindir);
			}
		}
	} else {
		wr_indir(bp, index1, new_block);
		rip->i_blocks += rip->i_sp->s_sectors_in_block;
	}
	/* b1 equals NO_BLOCK only when we are freeing up the indirect block. */
	if(b1 == NO_BLOCK)
		lmfs_markclean(bp);
	else
		lmfs_markdirty(bp);
	put_block(bp, INDIRECT_BLOCK);
  }

  /* If the single indirect block isn't there (or was just freed),
   * see if we have to keep the double indirect block, if any.
   * If we don't have to keep it, don't bother writing it out.
   */
  if (b1 == NO_BLOCK && !single && b2 != NO_BLOCK &&
     empty_indir(bp_dindir, rip->i_sp)) {
	lmfs_markclean(bp_dindir);
	free_block(rip->i_sp, b2);
	rip->i_blocks -= rip->i_sp->s_sectors_in_block;
	b2 = NO_BLOCK;
	if (triple) {
		wr_indir(bp_tindir, index3, b2);  /* update triple indir */
		lmfs_markdirty(bp_tindir);
	} else {
		rip->i_block[EXT2_DIND_BLOCK] = b2;
	}
  }
  /* If the double indirect block isn't there (or was just freed),
   * see if we have to keep the triple indirect block, if any.
   * If we don't have to keep it, don't bother writing it out.
   */
  if (b2 == NO_BLOCK && triple && b3 != NO_BLOCK &&
     empty_indir(bp_tindir, rip->i_sp)) {
	lmfs_markclean(bp_tindir);
	free_block(rip->i_sp, b3);
	rip->i_blocks -= rip->i_sp->s_sectors_in_block;
	rip->i_block[EXT2_TIND_BLOCK] = NO_BLOCK;
  }

  put_block(bp_dindir, INDIRECT_BLOCK);	/* release double indirect blk */
  put_block(bp_tindir, INDIRECT_BLOCK);	/* release triple indirect blk */

  return(OK);
}


/*===========================================================================*
 *				wr_indir				     *
 *===========================================================================*/
static void wr_indir(bp, index, block)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
block_t block;			/* block to write */
{
/* Given a pointer to an indirect block, write one entry. */

  if(bp == NULL)
	panic("wr_indir() on NULL");

  /* write a block into an indirect block */
  b_ind(bp)[index] = conv4(le_CPU, block);
}


/*===========================================================================*
 *				empty_indir				     *
 *===========================================================================*/
static int empty_indir(bp, sb)
struct buf *bp;			/* pointer to indirect block */
struct super_block *sb;		/* superblock of device block resides on */
{
/* Return nonzero if the indirect block pointed to by bp contains
 * only NO_BLOCK entries.
 */
  long addr_in_block = sb->s_block_size/4; /* 4 bytes per addr */
  int i;
  for(i = 0; i < addr_in_block; i++)
	if(b_ind(bp)[i] != NO_BLOCK)
		return(0);
  return(1);
}

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
struct buf *new_block(rip, position)
register struct inode *rip;	/* pointer to inode */
off_t position;			/* file pointer */
{
/* Acquire a new block and return a pointer to it. */
  register struct buf *bp;
  int r;
  block_t b;

  /* Is another block available? */
  if ( (b = read_map(rip, position)) == NO_BLOCK) {
	/* Check if this position follows last allocated
	 * block.
	 */
	block_t goal = NO_BLOCK;
	if (rip->i_last_pos_bl_alloc != 0) {
		off_t position_diff = position - rip->i_last_pos_bl_alloc;
		if (rip->i_bsearch == 0) {
			/* Should never happen, but not critical */
			ext2_debug("warning, i_bsearch is 0, while\
					i_last_pos_bl_alloc is not!");
		}
		if (position_diff <= rip->i_sp->s_block_size) {
			goal = rip->i_bsearch + 1;
		} else {
			/* Non-sequential write operation,
			 * disable preallocation
			 * for this inode.
			 */
			rip->i_preallocation = 0;
			discard_preallocated_blocks(rip);
		}
	}

	if ( (b = alloc_block(rip, goal) ) == NO_BLOCK) {
		err_code = ENOSPC;
		return(NULL);
	}
	if ( (r = write_map(rip, position, b, 0)) != OK) {
		free_block(rip->i_sp, b);
		err_code = r;
		ext2_debug("write_map failed\n");
		return(NULL);
	}
	rip->i_last_pos_bl_alloc = position;
	if (position == 0) {
		/* rip->i_last_pos_bl_alloc points to the block position,
		 * and zero indicates first usage, thus just increment.
		 */
		rip->i_last_pos_bl_alloc++;
	}
  }

  bp = get_block(rip->i_dev, b, NO_READ);
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
  lmfs_markdirty(bp);
}
