#include "fs.h"
#include "inode.h"
#include "super.h"
#include <assert.h>

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
void fs_sync(void)
{
/* Perform the sync() system call: flush all dirty inodes, blocks and the
 * superblock to disk. */
  struct inode *rip;

  if (superblock == NULL)
	return; /* not mounted (e.g. failed mount being torn down) */

  if (superblock->s_rd_only)
	return; /* nothing to sync */

  /* Write all the dirty inodes to the disk. */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dirt == IN_DIRTY)
		rw_inode(rip, WRITING);

  /* Flush the block cache, then commit the superblock and cg summary. */
  lmfs_flushall();

  if (superblock->s_dev != NO_DEV && superblock->s_fs.fs_fmod)
	write_super(superblock);
}
