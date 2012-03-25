#include "fs.h"


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
int fs_sync(message *fs_m_in, message *fs_m_out)
{
/* Perform the sync() system call.  No-op on this FS. */

  return(OK);		/* sync() can't fail */
}
