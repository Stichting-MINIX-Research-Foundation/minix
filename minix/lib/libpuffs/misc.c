/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
void fs_sync(void)
{
/* Perform the sync() system call.  Flush all the tables.
 * The order in which the various tables are flushed is critical.
 */
  int r;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (is_readonly_fs)
	return; /* nothing to sync */

  r = global_pu->pu_ops.puffs_fs_sync(global_pu, MNT_WAIT, pcr);
  if (r) {
	lpuffs_debug("Warning: sync failed!\n");
  }
}
