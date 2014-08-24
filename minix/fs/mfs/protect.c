#include "fs.h"
#include "inode.h"
#include "super.h"


/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
int fs_chmod(ino_t ino_nr, mode_t *mode)
{
/* Perform the chmod(name, mode) system call. */
  register struct inode *rip;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);
 
  if(rip->i_sp->s_rd_only) {
  	put_inode(rip);
	return EROFS;
  }

  /* Now make the change. Clear setgid bit if file is not in caller's grp */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (*mode & ALL_MODES);
  rip->i_update |= CTIME;
  IN_MARKDIRTY(rip);

  /* Return full new mode to caller. */
  *mode = rip->i_mode;

  put_inode(rip);
  return(OK);
}


/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode)
{
  register struct inode *rip;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);

  rip->i_uid = uid;
  rip->i_gid = gid;
  rip->i_mode &= ~(I_SET_UID_BIT | I_SET_GID_BIT);
  rip->i_update |= CTIME;
  IN_MARKDIRTY(rip);

  /* Update caller on current mode, as it may have changed. */
  *mode = rip->i_mode;
  put_inode(rip);
  
  return(OK);
}
