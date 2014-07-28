/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

static int in_group(gid_t grp);


/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
int fs_chmod()
{
/* Perform the chmod(name, mode) system call. */

  register struct inode *rip;
  mode_t mode;

  mode = fs_m_in.m_vfs_fs_chmod.mode;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_chmod.inode)) == NULL)
	  return(EINVAL);

  /* Now make the change. Clear setgid bit if file is not in caller's grp */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (mode & ALL_MODES);
  rip->i_update |= CTIME;
  rip->i_dirt = IN_DIRTY;

  /* Return full new mode to caller. */
  fs_m_out.m_fs_vfs_chmod.mode = rip->i_mode;

  put_inode(rip);
  return(OK);
}


/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
int fs_chown()
{
  register struct inode *rip;
  register int r;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_chown.inode)) == NULL)
	  return(EINVAL);

  /* Not permitted to change the owner of a file on a read-only file sys. */
  r = read_only(rip);
  if (r == OK) {
	  rip->i_uid = fs_m_in.m_vfs_fs_chown.uid;
	  rip->i_gid = fs_m_in.m_vfs_fs_chown.gid;
	  rip->i_mode &= ~(I_SET_UID_BIT | I_SET_GID_BIT);
	  rip->i_update |= CTIME;
	  rip->i_dirt = IN_DIRTY;
  }

  /* Update caller on current mode, as it may have changed. */
  fs_m_out.m_fs_vfs_chown.mode = rip->i_mode;
  put_inode(rip);

  return(r);
}


/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
int forbidden(struct inode *rip, mode_t access_desired)
{
/* Given a pointer to an inode, 'rip', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register struct inode *old_rip = rip;
  mode_t bits, perm_bits;
  int r, shift;

  /* Isolate the relevant rwx bits from the mode. */
  bits = rip->i_mode;
  if (caller_uid == SU_UID) {
	/* Grant read and write permission.  Grant search permission for
	 * directories.  Grant execute permission (for non-directories) if
	 * and only if one of the 'X' bits is set.
	 */
	if ( (bits & I_TYPE) == I_DIRECTORY ||
	     bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
		perm_bits = R_BIT | W_BIT | X_BIT;
	else
		perm_bits = R_BIT | W_BIT;
  } else {
	if (caller_uid == rip->i_uid) shift = 6;	/* owner */
	else if (caller_gid == rip->i_gid) shift = 3;	/* group */
	else if (in_group(rip->i_gid) == OK) shift = 3;	/* other groups */
	else shift = 0;					/* other */
	perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT);
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
  if ((perm_bits | access_desired) != perm_bits) r = EACCES;

  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  if (r == OK) {
	if (access_desired & W_BIT) {
		r = read_only(rip);
	}
  }

  if (rip != old_rip) put_inode(rip);

  return(r);
}


/*===========================================================================*
 *				in_group				     *
 *===========================================================================*/
static int in_group(gid_t grp)
{
  int i;

  if (credentials.vu_ngroups > NGROUPS_MAX)
	return(EINVAL);

  for (i = 0; i < credentials.vu_ngroups; i++)
	if (credentials.vu_sgroups[i] == grp)
		return(OK);

  return(EINVAL);
}


/*===========================================================================*
 *				read_only				     *
 *===========================================================================*/
int read_only(ip)
struct inode *ip;		/* ptr to inode whose file sys is to be cked */
{
/* Check to see if the file system on which the inode 'ip' resides is mounted
 * read only.  If so, return EROFS, else return OK.
 */

  register struct super_block *sp;

  sp = ip->i_sp;
  return(sp->s_rd_only ? EROFS : OK);
}
