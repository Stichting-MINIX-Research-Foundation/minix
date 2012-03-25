/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/com.h>
#include <sys/stat.h>
#include <minix/ds.h>
#include <minix/vfsif.h>

#include "puffs_priv.h"

/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
int fs_readsuper()
{
  struct vattr *root_va;

  fs_dev    = fs_m_in.REQ_DEV;
  is_readonly_fs  = (fs_m_in.REQ_FLAGS & REQ_RDONLY) ? 1 : 0;
  is_root_fs    = (fs_m_in.REQ_FLAGS & REQ_ISROOT) ? 1 : 0;

  /* Open root pnode */
  global_pu->pu_pn_root->pn_count = 1;

  /* Root pnode properties */
  root_va = &global_pu->pu_pn_root->pn_va;
  fs_m_out.RES_INODE_NR = root_va->va_fileid;
  fs_m_out.RES_MODE = root_va->va_mode;
  fs_m_out.RES_FILE_SIZE_LO = root_va->va_size;
  fs_m_out.RES_UID = root_va->va_uid;
  fs_m_out.RES_GID = root_va->va_gid;
  fs_m_out.RES_CONREQS = 1;

  return(OK);
}


/*===========================================================================*
 *				fs_mountpoint				     *
 *===========================================================================*/
int fs_mountpoint()
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the pnode or not.
 */
  int r = OK;
  struct puffs_node *pn;
  mode_t bits;

  /*
   * XXX: we assume that lookup was done first, so pnode can be found with
   * puffs_pn_nodewalk.
   */
  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);


  if (pn->pn_mountpoint) r = EBUSY;

  /* It may not be special. */
  bits = pn->pn_va.va_mode & I_TYPE;
  if(bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL) r = ENOTDIR;

  if (r == OK)
	pn->pn_mountpoint = TRUE;

  return(r);
}


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
int fs_unmount()
{
  int error;

  /* XXX there is no information about flags, 0 should be safe enough */
  error = global_pu->pu_ops.puffs_fs_unmount(global_pu, 0);
  if (error) {
        /* XXX we can't return any error to VFS */
  	lpuffs_debug("user handler failed to unmount filesystem!\
		Force unmount!\n");
  } 
  
  fs_sync();

  /* Finish off the unmount. */
  PU_SETSTATE(global_pu, PUFFS_STATE_UNMOUNTED);
  unmountdone = TRUE;
  global_pu->pu_pn_root->pn_count--;

  return(OK);
}
