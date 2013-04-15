#include "fs.h"
#include "inode.h"


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
int fs_sync(message *fs_m_in, message *fs_m_out)
{
/* Perform the sync() system call.  No-op on this FS. */

  return(OK);		/* sync() can't fail */
}

/*===========================================================================*
 *                             fs_chmod					     *
 *===========================================================================*/
int fs_chmod(message *fs_m_in, message *fs_m_out)
{
  struct inode *rip;  /* target inode */
  mode_t mode = (mode_t) fs_m_in->REQ_MODE;

  if( (rip = find_inode(fs_m_in->REQ_INODE_NR)) == NULL) return(EINVAL);
  get_inode(rip->i_dev, rip->i_num);	/* mark inode in use */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (mode & ALL_MODES);
  put_inode(rip);			/* release the inode */
  return OK;
}
