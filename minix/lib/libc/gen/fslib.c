/* fslib.c - routines needed by fs and fs utilities */

#include <minix/config.h>	/* for unused stuff in <minix/type.h> :-( */
#include <limits.h>
#include <dirent.h>
#include <assert.h>
#include <stdlib.h>		/* for abort() */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>		/* for unshort :-( */
#include <minix/sysutil.h>
#include <minix/minlib.h>
#include "mfs/const.h"		/* depends of -I flag in Makefile */
#include "mfs/type.h"		/* ditto */
#include "mfs/inode.h"		/* ditto */
#include "mfs/super.h"
#include <minix/fslib.h>
#include <sys/stat.h>

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
int bitmapsize(nr_bits, block_size)
bit_t nr_bits;
int block_size;
{
  int nr_blocks;

  nr_blocks = (int) (nr_bits / FS_BITS_PER_BLOCK(block_size));
  if (((bit_t) nr_blocks * FS_BITS_PER_BLOCK(block_size)) < nr_bits) ++nr_blocks;
  return(nr_blocks);
}

uint8_t fs_mode_to_type(mode_t mode)
{
	if(S_ISREG(mode)) return DT_REG;
	else if(S_ISDIR(mode)) return DT_DIR;
	else if(S_ISLNK(mode)) return DT_LNK;
	else if(S_ISCHR(mode)) return DT_CHR;
	else if(S_ISBLK(mode)) return DT_BLK;
	else if(S_ISFIFO(mode)) return DT_FIFO;
	else if(S_ISSOCK(mode)) return DT_SOCK;

	assert(0 && "unknown type");

	/* assert()s are removed on NDEBUG builds. */
	abort();
}

