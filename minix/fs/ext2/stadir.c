/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
  struct inode *rip;
  mode_t mo;
  int s;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf->st_mode = rip->i_mode;
  statbuf->st_nlink = rip->i_links_count;
  statbuf->st_uid = rip->i_uid;
  statbuf->st_gid = rip->i_gid;
  statbuf->st_rdev = (s ? (dev_t)rip->i_block[0] : NO_DEV);
  statbuf->st_size = rip->i_size;
  statbuf->st_atime = rip->i_atime;
  statbuf->st_mtime = rip->i_mtime;
  statbuf->st_ctime = rip->i_ctime;
  statbuf->st_blksize = rip->i_sp->s_block_size;
  statbuf->st_blocks = rip->i_blocks;

  put_inode(rip);		/* release the inode */

  return(OK);
}

/*===========================================================================*
 *                             fs_statvfs                                    *
 *===========================================================================*/
int fs_statvfs(struct statvfs *st)
{
  struct super_block *sp;

  sp = get_super(fs_dev);

  st->f_flag = ST_NOTRUNC;
  st->f_bsize =  sp->s_block_size;
  st->f_frsize = sp->s_block_size;
  st->f_iosize = sp->s_block_size;
  st->f_blocks = sp->s_blocks_count;
  st->f_bfree = sp->s_free_blocks_count;
  st->f_bavail = sp->s_free_blocks_count - sp->s_r_blocks_count;
  st->f_files = sp->s_inodes_count;
  st->f_ffree = sp->s_free_inodes_count;
  st->f_favail = sp->s_free_inodes_count;
  st->f_namemax = EXT2_NAME_MAX;

  return(OK);
}
