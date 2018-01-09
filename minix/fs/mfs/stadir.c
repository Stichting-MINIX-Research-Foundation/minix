#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *				estimate_blocks				     *
 *===========================================================================*/
static blkcnt_t estimate_blocks(struct inode *rip)
{
/* Return the number of 512-byte blocks used by this file. This includes space
 * used by data zones and indirect blocks (actually also zones). Reading in all
 * indirect blocks is too costly for a stat call, so we disregard holes and
 * return a conservative estimation.
 */
  blkcnt_t zones, sindirs, dindirs, nr_indirs, sq_indirs;
  unsigned int zone_size;

  /* Compute the number of zones used by the file. */
  zone_size = rip->i_sp->s_block_size << rip->i_sp->s_log_zone_size;

  zones = (blkcnt_t) ((rip->i_size + zone_size - 1) / zone_size);

  /* Compute the number of indirect blocks needed for that zone count. */
  nr_indirs = (blkcnt_t) rip->i_nindirs;
  sq_indirs = nr_indirs * nr_indirs;

  sindirs = (zones - (blkcnt_t) rip->i_ndzones + nr_indirs - 1) / nr_indirs;
  dindirs = (sindirs - 1 + sq_indirs - 1) / sq_indirs;

  /* Return the number of 512-byte blocks corresponding to the number of data
   * zones and indirect blocks.
   */
  return (zones + sindirs + dindirs) * (blkcnt_t) (zone_size / 512);
}


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

  statbuf->st_mode = (mode_t) rip->i_mode;
  statbuf->st_nlink = (nlink_t) rip->i_nlinks;
  statbuf->st_uid = rip->i_uid;
  statbuf->st_gid = rip->i_gid;
  statbuf->st_rdev = (s ? (dev_t)rip->i_zone[0] : NO_DEV);
  statbuf->st_size = rip->i_size;
  statbuf->st_atime = rip->i_atime;
  statbuf->st_mtime = rip->i_mtime;
  statbuf->st_ctime = rip->i_ctime;
  statbuf->st_blksize = lmfs_fs_block_size();
  statbuf->st_blocks = estimate_blocks(rip);

  put_inode(rip);		/* release the inode */

  return(OK);
}


/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs(struct statvfs *st)
{
  struct super_block *sp;
  int scale;

  sp = &superblock;

  scale = sp->s_log_zone_size;

  st->f_blocks = sp->s_zones;
  st->f_bfree = sp->s_zones - used_zones;
  st->f_bavail = st->f_bfree;

  st->f_bsize = sp->s_block_size << scale;
  st->f_frsize = sp->s_block_size;
  st->f_iosize = st->f_frsize;
  st->f_files = sp->s_ninodes;
  st->f_ffree = count_free_bits(sp, IMAP);
  st->f_favail = st->f_ffree;
  st->f_namemax = MFS_DIRSIZ;

  return(OK);
}
