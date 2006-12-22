


#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "inode.h"

#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
PUBLIC int fs_utime()
{
  register struct inode *rip;
  register int r;
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_utime() failed\n", SELF_E);
        return(EINVAL);
  }

  /* Only the owner of a file or the super_user can change its time. */
  r = OK;
  if (rip->i_uid != caller_uid && caller_uid != SU_UID) r = EPERM;
  if (fs_m_in.REQ_ACTIME == 0 && r != OK) r = forbidden(rip, W_BIT);
  if (read_only(rip) != OK) r = EROFS;	/* not even su can touch if R/O */
  if (r == OK) {
	if (fs_m_in.REQ_ACTIME == 0) {
                rip->i_atime = fs_m_in.REQ_MODTIME;
		rip->i_mtime = rip->i_atime;
	} else {
		rip->i_atime = fs_m_in.REQ_ACTIME;
		rip->i_mtime = fs_m_in.REQ_MODTIME;
	}
	rip->i_update = CTIME;	/* discard any stale ATIME and MTIME flags */
	rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}

PUBLIC int fs_stime()
{
  boottime = fs_m_in.REQ_BOOTTIME;
  return OK;
}

