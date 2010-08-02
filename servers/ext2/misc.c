/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <assert.h>
#include <minix/vfsif.h>
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
PUBLIC int fs_sync()
{
/* Perform the sync() system call.  Flush all the tables.
 * The order in which the various tables are flushed is critical.  The
 * blocks must be flushed last, since rw_inode() leaves its results in
 * the block cache.
 */
  struct inode *rip;
  struct buf *bp;
  int r;

  assert(nr_bufs > 0);
  assert(buf);

  if (superblock->s_rd_only)
	return(OK); /* nothing to sync */

  /* Write all the dirty inodes to the disk. */
  for(rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if(rip->i_count > 0 && rip->i_dirt == DIRTY) rw_inode(rip, WRITING);

  /* Write all the dirty blocks to the disk, one drive at a time. */
  for(bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if(bp->b_dev != NO_DEV && bp->b_dirt == DIRTY)
		flushall(bp->b_dev);

  if (superblock->s_dev != NO_DEV) {
	superblock->s_wtime = clock_time();
	write_super(superblock);
  }

  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				fs_flush				     *
 *===========================================================================*/
PUBLIC int fs_flush()
{
/* Flush the blocks of a device from the cache after writing any dirty blocks
 * to disk.
 */
  dev_t dev = (dev_t) fs_m_in.REQ_DEV;

  if(dev == fs_dev) return(EBUSY);

  flushall(dev);
  invalidate(dev);

  return(OK);
}
