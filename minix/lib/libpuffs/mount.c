/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <fcntl.h>
#include <minix/vfsif.h>

/*===========================================================================*
 *				fs_mount				     *
 *===========================================================================*/
int fs_mount(dev_t __unused dev, unsigned int flags,
	struct fsdriver_node *root_node, unsigned int *res_flags)
{
  struct vattr *root_va;

  is_readonly_fs = !!(flags & REQ_RDONLY);

  /* Open root pnode */
  global_pu->pu_pn_root->pn_count = 1;

  /* Root pnode properties */
  root_va = &global_pu->pu_pn_root->pn_va;
  root_node->fn_ino_nr = root_va->va_fileid;
  root_node->fn_mode = root_va->va_mode;
  root_node->fn_size = root_va->va_size;
  root_node->fn_uid = root_va->va_uid;
  root_node->fn_gid = root_va->va_gid;
  root_node->fn_dev = NO_DEV;

  *res_flags = RES_NOFLAGS;

  mounted = TRUE;

  return(OK);
}


/*===========================================================================*
 *				fs_mountpt				     *
 *===========================================================================*/
int fs_mountpt(ino_t ino_nr)
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the pnode or not.
 */
  int r = OK;
  struct puffs_node *pn;
  mode_t bits;

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL)
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
void fs_unmount(void)
{
  int error;

  /* Always force unmounting, as VFS will not tolerate failure. */
  error = global_pu->pu_ops.puffs_fs_unmount(global_pu, MNT_FORCE);
  if (error) {
	lpuffs_debug("user handler failed to unmount filesystem!\
		Force unmount!\n");
  }

  fs_sync();

  /* Finish off the unmount. */
  PU_SETSTATE(global_pu, PUFFS_STATE_UNMOUNTED);
  mounted = FALSE;
  global_pu->pu_pn_root->pn_count--;
}
