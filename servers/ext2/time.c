/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
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
  if( (rip = get_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);

  /*
   * Only the owner of a file or the super_user can change the timestamps.
   * Here we assume VFS did that check before.
   */

  r = OK;
  if(read_only(rip) != OK) r = EROFS;	/* not even su can touch if R/O */
  if(r == OK) {
	rip->i_update = CTIME;	/* discard any stale ATIME and MTIME flags */
	switch(fs_m_in.REQ_ACNSEC) {
	case UTIME_NOW:
		rip->i_update |= ATIME;
		break;
	case UTIME_OMIT: /* do not touch */
		break;
	default:
		/*
		 * cases fs_m_in.REQ_ACNSEC < 0 || fs_m_in.REQ_ACNSEC >= 1E9
		 * are caught by VFS to cooperate with old instances of EXT2
		 */
		rip->i_atime = fs_m_in.REQ_ACTIME;
		/*
		 * Ext2FS does not support better than second resolution,
		 * so we discard REQ_ACNSEC to round down
		 */
		break;
	}

	switch(fs_m_in.REQ_MODNSEC) {
	case UTIME_NOW:
		rip->i_update |= MTIME;
		break;
	case UTIME_OMIT: /* do not touch */
		break;
	default:
		/*
		 * cases fs_m_in.REQ_MODNSEC < 0 || fs_m_in.REQ_MODNSEC >= 1E9
		 * are caught by VFS to cooperate with old instances of EXT2
		 */
		rip->i_mtime = fs_m_in.REQ_MODTIME;
		/*
		 * Ext2FS does not support better than second resolution,
		 * so we discard REQ_MODNSEC to round down
		 */
		break;
	}

	rip->i_dirt = IN_DIRTY;
  }

  put_inode(rip);
  return(r);
}
