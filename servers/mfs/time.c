#include "fs.h"
#include "inode.h"
#include <sys/stat.h>
#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
int fs_utime()
{
  register struct inode *rip;
  register int r;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_utime.inode)) == NULL)
        return(EINVAL);

  /*
   * Only the owner of a file or the super_user can change the timestamps.
   * Here we assume VFS did that check before.
   */

  r = OK;
  if(read_only(rip) != OK) r = EROFS;	/* not even su can touch if R/O */
  if(r == OK) {
	rip->i_update = CTIME; /* discard any stale ATIME and MTIME flags */
	switch(fs_m_in.m_vfs_fs_utime.acnsec) {
	case UTIME_NOW:
		rip->i_update |= ATIME;
		break;
	case UTIME_OMIT: /* do not touch */
		break;
	default:
		/*
		 * cases fs_m_in.m_vfs_fs_utime.acnsec < 0 || fs_m_in.m_vfs_fs_utime.acnsec >= 1E9
		 * are caught by VFS to cooperate with old instances of MFS
		 */
		rip->i_atime = fs_m_in.m_vfs_fs_utime.actime;
		/*
		 * MFS does not support better than second resolution,
		 * so we discard ACNSEC to round down
		 */
		break;
	}

	switch(fs_m_in.m_vfs_fs_utime.modnsec) {
	case UTIME_NOW:
		rip->i_update |= MTIME;
		break;
	case UTIME_OMIT: /* do not touch */
		break;
	default:
		/*
		 * cases fs_m_in.m_vfs_fs_utime.modnsec < 0 || fs_m_in.m_vfs_fs_utime.modnsec >= 1E9
		 * are caught by VFS to cooperate with old instances of MFS
		 */
		rip->i_mtime = fs_m_in.m_vfs_fs_utime.modtime;
		/*
		 * MFS does not support better than second resolution,
		 * so we discard MODNSEC to round down
		 */
		break;
	}

	IN_MARKDIRTY(rip);
  }

  put_inode(rip);
  return(r);
}

