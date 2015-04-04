/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include "inode.h"
#include "super.h"
#include <assert.h>

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
void fs_sync(void)
{
/* Perform the sync() system call.  Flush all the tables.
 * The order in which the various tables are flushed is critical.  The
 * blocks must be flushed last, since rw_inode() leaves its results in
 * the block cache.
 */
  struct inode *rip;

  if (superblock->s_rd_only)
	return; /* nothing to sync */

  /* Write all the dirty inodes to the disk. */
  for(rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if(rip->i_count > 0 && rip->i_dirt == IN_DIRTY) rw_inode(rip, WRITING);

  lmfs_flushall();

  if (superblock->s_dev != NO_DEV) {
	superblock->s_wtime = clock_time(NULL);
	write_super(superblock);
  }
}
