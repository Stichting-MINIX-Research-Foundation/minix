#include "fs.h"
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
int fs_chmod(ino_t ino_nr, mode_t *mode)
{
/* Change the mode bits of an inode. */
  struct inode *rip;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (rip->i_sp->s_rd_only) {
	put_inode(rip);
	return(EROFS);
  }

  rip->i_din.di_mode = (rip->i_din.di_mode & ~ALL_MODES) | (*mode & ALL_MODES);
  rip->i_update |= CTIME;
  rip->i_dirt = IN_DIRTY;

  *mode = rip->i_din.di_mode;	/* return the full new mode */

  put_inode(rip);
  return(OK);
}

/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode)
{
/* Change the owner and group of an inode, clearing the set-uid/set-gid bits. */
  struct inode *rip;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (rip->i_sp->s_rd_only) {
	put_inode(rip);
	return(EROFS);
  }

  rip->i_din.di_uid = uid;
  rip->i_din.di_gid = gid;
  rip->i_din.di_mode &= ~(I_SET_UID_BIT | I_SET_GID_BIT);
  rip->i_update |= CTIME;
  rip->i_dirt = IN_DIRTY;

  *mode = rip->i_din.di_mode;

  put_inode(rip);
  return(OK);
}
