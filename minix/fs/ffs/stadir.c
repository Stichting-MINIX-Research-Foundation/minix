#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
  struct inode *rip;
  struct ufs2_dinode *dip;
  mode_t mo;
  int s;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (rip->i_update)
	update_times(rip);	/* no-op on a read-only file system */

  dip = &rip->i_din;
  mo = dip->di_mode & I_TYPE;
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);	/* true iff special */

  statbuf->st_mode = dip->di_mode;
  statbuf->st_nlink = dip->di_nlink;
  statbuf->st_uid = dip->di_uid;
  statbuf->st_gid = dip->di_gid;
  statbuf->st_rdev = (s ? (dev_t) dip->di_rdev : NO_DEV);
  statbuf->st_size = dip->di_size;
  statbuf->st_atime = dip->di_atime;
  statbuf->st_atimensec = dip->di_atimensec;
  statbuf->st_mtime = dip->di_mtime;
  statbuf->st_mtimensec = dip->di_mtimensec;
  statbuf->st_ctime = dip->di_ctime;
  statbuf->st_ctimensec = dip->di_ctimensec;
  statbuf->st_birthtime = dip->di_birthtime;
  statbuf->st_birthtimensec = dip->di_birthnsec;
  statbuf->st_blksize = rip->i_sp->s_fs.fs_bsize;
  statbuf->st_blocks = dip->di_blocks;	/* in 512-byte units */

  put_inode(rip);

  return(OK);
}

/*===========================================================================*
 *                             fs_statvfs                                    *
 *===========================================================================*/
int fs_statvfs(struct statvfs *st)
{
  struct super_block *sp;
  struct fs *fs;
  int64_t bfree, bavail, reserved;

  sp = get_super(fs_dev);
  fs = &sp->s_fs;

  /* Free space, in fragments (the f_frsize unit). */
  bfree = fs->fs_cstotal.cs_nbfree * fs->fs_frag + fs->fs_cstotal.cs_nffree;
  reserved = fs->fs_dsize * fs->fs_minfree / 100;
  bavail = bfree - reserved;
  if (bavail < 0)
	bavail = 0;

  st->f_flag = ST_NOTRUNC;
  st->f_bsize = fs->fs_bsize;
  st->f_frsize = fs->fs_fsize;
  st->f_iosize = fs->fs_bsize;
  st->f_blocks = fs->fs_dsize;
  st->f_bfree = bfree;
  st->f_bavail = bavail;
  st->f_files = (uint64_t) fs->fs_ipg * fs->fs_ncg;
  st->f_ffree = fs->fs_cstotal.cs_nifree;
  st->f_favail = fs->fs_cstotal.cs_nifree;
  st->f_namemax = FFS_MAXNAMLEN;

  return(OK);
}

/*===========================================================================*
 *                             fs_rdlink                                     *
 *===========================================================================*/
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes)
{
  struct inode *rip;
  struct fs *fs;
  struct buf *bp = NULL;
  char *link_text;
  size_t link_len;
  int r;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  fs = &rip->i_sp->s_fs;
  link_len = (size_t) rip->i_din.di_size;

  if (rip->i_din.di_size < fs->fs_maxsymlinklen) {
	/* Fast symlink: the target is stored in the inode's block array. */
	link_text = (char *) rip->i_din.di_db;
	r = OK;
  } else {
	/* Slow symlink: the target lives in the first data block. */
	if ((bp = get_block_map(rip, 0)) == NULL) {
		r = EIO;
	} else {
		link_text = b_data(bp);
		r = OK;
	}
  }

  if (r == OK) {
	if (bytes > link_len)
		bytes = link_len;
	r = fsdriver_copyout(data, 0, link_text, bytes);
	if (bp != NULL)
		put_block(bp);
	if (r == OK)
		r = (int) bytes;
  }

  put_inode(rip);
  return(r);
}
