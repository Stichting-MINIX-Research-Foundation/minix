/* Some misc functions */

#include "inc.h"
#include <fcntl.h>
#include <minix/vfsif.h>

/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
PUBLIC int fs_sync()		/* Calling of syncing the filesystem. No action
				 * is taken */
{
  return(OK);		/* sync() can't fail */
}
