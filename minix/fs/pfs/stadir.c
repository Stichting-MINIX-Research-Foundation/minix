#include "fs.h"


/*===========================================================================*
 *				fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
  struct inode *rip;
  mode_t type;
  u32_t blocks; /* The unit of this is 512 */
  int s;

  if ((rip = find_inode(ino_nr)) == NULL) return(EINVAL);

  type = rip->i_mode & I_TYPE;
  s = (type == I_CHAR_SPECIAL || type == I_BLOCK_SPECIAL);

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  blocks = rip->i_size / S_BLKSIZE;
  if (rip->i_size % S_BLKSIZE != 0)
	blocks += 1;

  statbuf->st_dev = rip->i_dev;
  statbuf->st_ino = rip->i_num;
  statbuf->st_mode = rip->i_mode;
  statbuf->st_nlink = rip->i_nlinks;
  statbuf->st_uid = rip->i_uid;
  statbuf->st_gid = (short int) rip->i_gid;
  statbuf->st_rdev = (s ? rip->i_rdev : NO_DEV);
  statbuf->st_size = rip->i_size;
  if (!s) statbuf->st_mode &= ~I_REGULAR;/* wipe out I_REGULAR bit for pipes */
  statbuf->st_atime = rip->i_atime;
  statbuf->st_mtime = rip->i_mtime;
  statbuf->st_ctime = rip->i_ctime;
  statbuf->st_blksize = PIPE_BUF;
  statbuf->st_blocks = blocks;

  return(OK);
}
