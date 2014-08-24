#include "fs.h"


/*===========================================================================*
 *                             fs_chmod					     *
 *===========================================================================*/
int fs_chmod(ino_t ino_nr, mode_t *mode)
{
  struct inode *rip;  /* target inode */

  if( (rip = find_inode(ino_nr)) == NULL) return(EINVAL);

  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (*mode & ALL_MODES);

  *mode = rip->i_mode;	/* return new mode */
  return OK;
}
