/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

#include "puffs.h"
#include "puffs_priv.h"


/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
int fs_chmod(ino_t ino_nr, mode_t *mode)
{
/* Perform the chmod(name, mode) system call. */
  struct puffs_node *pn;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return(EINVAL);

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL)
	return(EINVAL);

  puffs_vattr_null(&va);
  /* Clear setgid bit if file is not in caller's grp */
  va.va_mode = (pn->pn_va.va_mode & ~ALL_MODES) | (*mode & ALL_MODES);
  (void)clock_time(&va.va_ctime);

  if (global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr) != 0)
	return(EINVAL);

  /* Return full new mode to caller. */
  *mode = pn->pn_va.va_mode;

  return(OK);
}


/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode)
{
  struct puffs_node *pn;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL)
	return(EINVAL);

  puffs_vattr_null(&va);
  va.va_uid = uid;
  va.va_gid = gid;
  va.va_mode = pn->pn_va.va_mode & ~(I_SET_UID_BIT | I_SET_GID_BIT);
  (void)clock_time(&va.va_ctime);

  if (global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr) != 0)
	return(EINVAL);

  /* Update caller on current mode, as it may have changed. */
  *mode = pn->pn_va.va_mode;

  return(OK);
}
