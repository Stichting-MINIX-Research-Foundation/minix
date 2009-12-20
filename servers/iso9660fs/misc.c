#include "inc.h"
#include <fcntl.h>
#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
PUBLIC int fs_sync()
{
  /* Always mounted read only, so nothing to sync */
  return(OK);		/* sync() can't fail */
}

