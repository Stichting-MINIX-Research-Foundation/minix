/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/vfsif.h>

#include "puffs.h"
#include "puffs_priv.h"


/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
int fs_utime()
{
  struct puffs_node *pn;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (is_readonly_fs)
	return(EROFS);

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return(EINVAL);

  if( (pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);
  
  puffs_vattr_null(&va);
  va.va_atime.tv_sec = fs_m_in.REQ_ACTIME;
  va.va_atime.tv_nsec = fs_m_in.REQ_ACNSEC;
  va.va_mtime.tv_sec = fs_m_in.REQ_MODTIME;
  va.va_mtime.tv_nsec = fs_m_in.REQ_MODNSEC;
  va.va_ctime = clock_timespec();

  if (global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr) != 0)
	return(EINVAL);

  return(OK);
}
