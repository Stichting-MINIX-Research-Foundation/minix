#include "fs.h"
#include "glo.h"


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
int fs_unmount(message *fs_m_in, message *fs_m_out)
{
/* Unmount Pipe File Server. */

  if (busy) return(EBUSY);	/* can't umount a busy file system */

  /* Finish off the unmount. */
  unmountdone = TRUE;

  return(OK);
}
