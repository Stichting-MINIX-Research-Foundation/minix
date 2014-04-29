/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <assert.h>
#include <minix/vfsif.h>

#include "puffs.h"
#include "puffs_priv.h"

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
int fs_sync(void)
{
/* Perform the sync() system call.  Flush all the tables.
 * The order in which the various tables are flushed is critical.
 */
  int r;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (is_readonly_fs)
	return(OK); /* nothing to sync */

  r = global_pu->pu_ops.puffs_fs_sync(global_pu, MNT_WAIT, pcr);
  if (r) {
	lpuffs_debug("Warning: sync failed!\n");
  }

  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				fs_flush				     *
 *===========================================================================*/
int fs_flush(void)
{
/* Flush the blocks of a device from the cache after writing any dirty blocks
 * to disk.
 */
#if 0
  dev_t dev = fs_m_in.m_vfs_fs_flush.device;

  if(dev == fs_dev) return(EBUSY);

  flushall(dev);
  invalidate(dev);
#endif

  return(OK);
}


/*===========================================================================*
 *				fs_new_driver				     *
 *===========================================================================*/
int fs_new_driver(void)
{
/* Do not do anything. */

  return(OK);
}
