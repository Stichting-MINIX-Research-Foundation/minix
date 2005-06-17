/* fslib.c - routines needed by fs and fs utilities */

#include <minix/config.h>	/* for unused stuff in <minix/type.h> :-( */
#include <ansi.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>		/* for unshort :-( */
#include "fs/const.h"		/* depends of -I flag in Makefile */
#include "fs/type.h"		/* ditto */
#include "fs/inode.h"		/* ditto */
#include "fs/super.h"
#include <minix/fslib.h>

/* The next routine is copied from fsck.c and mkfs.c...  (Re)define some
 * things for consistency.  Some things should be done better.
 */

/* Convert from bit count to a block count. The usual expression
 *
 *	(nr_bits + (1 << BITMAPSHIFT) - 1) >> BITMAPSHIFT
 *
 * doesn't work because of overflow.
 *
 * Other overflow bugs, such as the expression for N_ILIST overflowing when
 * s_inodes is just over V*_INODES_PER_BLOCK less than the maximum+1, are not
 * fixed yet, because that number of inodes is silly.
 */
/* The above comment doesn't all apply now bit_t is long.  Overflow is now
 * unlikely, but negative bit counts are now possible (though unlikely)
 * and give silly results.
 */ 
PUBLIC int bitmapsize(nr_bits, block_size)
bit_t nr_bits;
int block_size;
{
  int nr_blocks;

  nr_blocks = (int) (nr_bits / FS_BITS_PER_BLOCK(block_size));
  if (((bit_t) nr_blocks * FS_BITS_PER_BLOCK(block_size)) < nr_bits) ++nr_blocks;
  return(nr_blocks);
}


/*===========================================================================*
 *				conv2					     *
 *===========================================================================*/
PUBLIC unsigned conv2(norm, w)
int norm;			/* TRUE if no swap, FALSE for byte swap */
int w;				/* promotion of 16-bit word to be swapped */
{
/* Possibly swap a 16-bit word between 8086 and 68000 byte order. */

  if (norm) return( (unsigned) w & 0xFFFF);
  return( ((w&BYTE) << 8) | ( (w>>8) & BYTE));
}


/*===========================================================================*
 *				conv4					     *
 *===========================================================================*/
PUBLIC long conv4(norm, x)
int norm;			/* TRUE if no swap, FALSE for byte swap */
long x;				/* 32-bit long to be byte swapped */
{
/* Possibly swap a 32-bit long between 8086 and 68000 byte order. */

  unsigned lo, hi;
  long l;
  
  if (norm) return(x);			/* byte order was already ok */
  lo = conv2(FALSE, (int) x & 0xFFFF);	/* low-order half, byte swapped */
  hi = conv2(FALSE, (int) (x>>16) & 0xFFFF);	/* high-order half, swapped */
  l = ( (long) lo <<16) | hi;
  return(l);
}


/*===========================================================================*
 *				conv_inode				     *
 *===========================================================================*/
PUBLIC void conv_inode(rip, dip, dip2, rw_flag, magic)
register struct inode *rip;	/* pointer to the in-core inode struct */
register d1_inode *dip;		/* pointer to the V1 on-disk inode struct */
register d2_inode *dip2;	/* pointer to the V2 on-disk inode struct */
int rw_flag;			/* READING or WRITING */
int magic;			/* magic number of file system */
{ 
/* Copy the inode from the disk block to the in-core table or vice versa.
 * If the fourth parameter below is FALSE, the bytes are swapped.
 */
  switch (magic) {
	case SUPER_MAGIC:	old_icopy(rip, dip,  rw_flag, TRUE);	break;
	case SUPER_REV:		old_icopy(rip, dip,  rw_flag, FALSE);	break;
	case SUPER_V3:
	case SUPER_V2:		new_icopy(rip, dip2, rw_flag, TRUE);	break;
	case SUPER_V2_REV:	new_icopy(rip, dip2, rw_flag, FALSE);	break;
  } 
}


/*===========================================================================*
 *				old_icopy				     *
 *===========================================================================*/
PUBLIC void old_icopy(rip, dip, direction, norm)
register struct inode *rip;	/* pointer to the in-core inode struct */
register d1_inode *dip;		/* pointer to the d1_inode inode struct */
int direction;			/* READING (from disk) or WRITING (to disk) */
int norm;			/* TRUE = do not swap bytes; FALSE = swap */

{
/* 4 different on-disk inode layouts are supported, one for each combination
 * of V1.x/V2.x * bytes-swapped/not-swapped.  When an inode is read or written
 * this routine handles the conversions so that the information in the inode
 * table is independent of the disk structure from which the inode came.
 * The old_icopy routine copies to and from V1 disks.
 */

  int i;

  if (direction == READING) {
	/* Copy V1.x inode to the in-core table, swapping bytes if need be. */
	rip->i_mode    = conv2(norm, dip->d1_mode);
	rip->i_uid     = conv2(norm,dip->d1_uid );
	rip->i_size    = conv4(norm,dip->d1_size);
	rip->i_mtime   = conv4(norm,dip->d1_mtime);
	rip->i_atime   = 0;
	rip->i_ctime   = 0;
	rip->i_nlinks  = (nlink_t) dip->d1_nlinks;	/* 1 char */
	rip->i_gid     = (gid_t) dip->d1_gid;		/* 1 char */
	rip->i_ndzones = V1_NR_DZONES;
	rip->i_nindirs = V1_INDIRECTS;
	for (i = 0; i < V1_NR_TZONES; i++)
		rip->i_zone[i] = conv2(norm, (int) dip->d1_zone[i]);
  } else {
	/* Copying V1.x inode to disk from the in-core table. */
	dip->d1_mode   = conv2(norm,rip->i_mode);
	dip->d1_uid    = conv2(norm,rip->i_uid );
	dip->d1_size   = conv4(norm,rip->i_size);
	dip->d1_mtime  = conv4(norm,rip->i_mtime);
	dip->d1_nlinks = (nlink_t) rip->i_nlinks;	/* 1 char */
	dip->d1_gid    = (gid_t) rip->i_gid;		/* 1 char */
	for (i = 0; i < V1_NR_TZONES; i++)
		dip->d1_zone[i] = conv2(norm, (int) rip->i_zone[i]);
  }
}


/*===========================================================================*
 *				new_icopy				     *
 *===========================================================================*/
PUBLIC void new_icopy(rip, dip, direction, norm)
register struct inode *rip;	/* pointer to the in-core inode struct */
register d2_inode *dip;	/* pointer to the d2_inode struct */
int direction;			/* READING (from disk) or WRITING (to disk) */
int norm;			/* TRUE = do not swap bytes; FALSE = swap */

{
/* Same as old_icopy, but to/from V2 disk layout. */

  int i;

  if (direction == READING) {
	/* Copy V2.x inode to the in-core table, swapping bytes if need be. */
	rip->i_mode    = conv2(norm,dip->d2_mode);
	rip->i_uid     = conv2(norm,dip->d2_uid );
	rip->i_nlinks  = conv2(norm,(int) dip->d2_nlinks);
	rip->i_gid     = conv2(norm,(int) dip->d2_gid );
	rip->i_size    = conv4(norm,dip->d2_size);
	rip->i_atime   = conv4(norm,dip->d2_atime);
	rip->i_ctime   = conv4(norm,dip->d2_ctime);
	rip->i_mtime   = conv4(norm,dip->d2_mtime);
	rip->i_ndzones = V2_NR_DZONES;
	rip->i_nindirs = V2_INDIRECTS(rip->i_sp->s_block_size);
	for (i = 0; i < V2_NR_TZONES; i++)
		rip->i_zone[i] = conv4(norm, (long) dip->d2_zone[i]);
  } else {
	/* Copying V2.x inode to disk from the in-core table. */
	dip->d2_mode   = conv2(norm,rip->i_mode);
	dip->d2_uid    = conv2(norm,rip->i_uid );
	dip->d2_nlinks = conv2(norm,rip->i_nlinks);
	dip->d2_gid    = conv2(norm,rip->i_gid );
	dip->d2_size   = conv4(norm,rip->i_size);
	dip->d2_atime  = conv4(norm,rip->i_atime);
	dip->d2_ctime  = conv4(norm,rip->i_ctime);
	dip->d2_mtime  = conv4(norm,rip->i_mtime);
	for (i = 0; i < V2_NR_TZONES; i++)
		dip->d2_zone[i] = conv4(norm, (long) rip->i_zone[i]);
  }
}
