#include "fs.h"
#include "inode.h"
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

  /* Only the owner of a file or the super_user can change its time. */
  r = OK;
  if(read_only(rip) != OK) r = EROFS;	/* not even su can touch if R/O */
  if(r == OK) {
	  rip->i_atime = fs_m_in.REQ_ACTIME;
	  rip->i_mtime = fs_m_in.REQ_MODTIME;
	  rip->i_update = CTIME; /* discard any stale ATIME and MTIME flags */
	  IN_MARKDIRTY(rip);
  }

  put_inode(rip);
  return(r);
}

