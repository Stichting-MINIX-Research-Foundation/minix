/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <minix/vfsif.h>

#include "puffs.h"
#include "puffs_priv.h"

static int in_group(gid_t grp);


/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
int fs_chmod(void)
{
/* Perform the chmod(name, mode) system call. */
  struct puffs_node *pn;
  mode_t mode;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return(EINVAL);

  mode = fs_m_in.m_vfs_fs_chmod.mode;

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.m_vfs_fs_chmod.inode)) == NULL)
	return(EINVAL);
   
  puffs_vattr_null(&va);
  /* Clear setgid bit if file is not in caller's grp */
  va.va_mode = (pn->pn_va.va_mode & ~ALL_MODES) | (mode & ALL_MODES);
  va.va_ctime = clock_timespec();

  if (global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr) != 0)
	return(EINVAL);

  /* Return full new mode to caller. */
  fs_m_out.m_fs_vfs_chmod.mode = pn->pn_va.va_mode;

  return(OK);
}


/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
int fs_chown(void)
{
  struct puffs_node *pn;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.m_vfs_fs_chown.inode)) == NULL)
	return(EINVAL);

  /* Not permitted to change the owner of a file on a read-only file sys. */
  if (!is_readonly_fs) {
	puffs_vattr_null(&va);
	va.va_uid = fs_m_in.m_vfs_fs_chown.uid;
	va.va_gid = fs_m_in.m_vfs_fs_chown.gid;
	va.va_mode = pn->pn_va.va_mode & ~(I_SET_UID_BIT | I_SET_GID_BIT);
	va.va_ctime = clock_timespec();

	if (global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr) != 0)
		return(EINVAL);
  }

  /* Update caller on current mode, as it may have changed. */
  fs_m_out.m_fs_vfs_chown.mode = pn->pn_va.va_mode;

  return(OK);
}


/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
int forbidden(register struct puffs_node *pn, mode_t access_desired)
{
/* Given a pointer to an pnode, 'pn', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register mode_t bits, perm_bits;
  int r, shift;

  /* Isolate the relevant rwx bits from the mode. */
  bits = pn->pn_va.va_mode;
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
	if (caller_uid == pn->pn_va.va_uid) shift = 6;	/* owner */
	else if (caller_gid == pn->pn_va.va_gid) shift = 3;	/* group */
	else if (in_group(pn->pn_va.va_gid) == OK) shift = 3;	/* other groups */
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
		r = is_readonly_fs ? EROFS : OK;
	}
  }

  return(r);
}


/*===========================================================================*
 *				in_group				     *
 *===========================================================================*/
static int in_group(gid_t grp)
{
  int i;
  for(i = 0; i < credentials.vu_ngroups; i++)
	if (credentials.vu_sgroups[i] == grp)
		return(OK);

  return(EINVAL);
}
