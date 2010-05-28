#include "fs.h"


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
PUBLIC int fs_sync()
{
/* Perform the sync() system call.  No-op on this FS. */

  return(OK);		/* sync() can't fail */
}


