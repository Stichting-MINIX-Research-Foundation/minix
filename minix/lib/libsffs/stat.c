/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   get_mode		return a file's mode
 *   do_stat		perform the STAT file system call
 *   do_chmod		perform the CHMOD file system call
 *   do_utime		perform the UTIME file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				get_mode				     *
 *===========================================================================*/
mode_t get_mode(struct inode *ino, int mode)
{
/* Return the mode for an inode, given the inode and the retrieved mode.
 */

  mode &= S_IRWXU;
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(ino))
	mode = S_IFDIR | (mode & sffs_params->p_dir_mask);
  else
	mode = S_IFREG | (mode & sffs_params->p_file_mask);

  if (read_only)
	mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

  return mode;
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
int do_stat(ino_t ino_nr, struct stat *st)
{
/* Retrieve inode status.
 */
  struct inode *ino;
  struct sffs_attr attr;
  char path[PATH_MAX];
  int r;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE | SFFS_ATTR_CRTIME |
		SFFS_ATTR_ATIME | SFFS_ATTR_MTIME | SFFS_ATTR_CTIME;

  if ((r = verify_inode(ino, path, &attr)) != OK)
	return r;

  st->st_mode = get_mode(ino, attr.a_mode);
  st->st_uid = sffs_params->p_uid;
  st->st_gid = sffs_params->p_gid;
  st->st_rdev = NO_DEV;
  st->st_size = attr.a_size;
  st->st_atimespec = attr.a_atime;
  st->st_mtimespec = attr.a_mtime;
  st->st_ctimespec = attr.a_ctime;
  st->st_birthtimespec = attr.a_crtime;

  st->st_blocks = st->st_size / S_BLKSIZE;
  if (st->st_size % S_BLKSIZE != 0)
	st->st_blocks += 1;

  st->st_blksize = BLOCK_SIZE;

  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
  st->st_nlink = 0;
  if (ino->i_parent != NULL) st->st_nlink++;
  if (IS_DIR(ino)) {
	st->st_nlink++;
	if (HAS_CHILDREN(ino)) st->st_nlink++;
  }

  return OK;
}

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
int do_chmod(ino_t ino_nr, mode_t *mode)
{
/* Change file mode.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct sffs_attr attr;
  int r;

  if (read_only)
	return EROFS;

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  /* Set the new file mode. */
  attr.a_mask = SFFS_ATTR_MODE;
  attr.a_mode = *mode; /* no need to convert in this direction */

  if ((r = sffs_table->t_setattr(path, &attr)) != OK)
	return r;

  /* We have no idea what really happened. Query for the mode again. */
  if ((r = verify_path(path, ino, &attr, NULL)) != OK)
	return r;

  *mode = get_mode(ino, attr.a_mode);

  return OK;
}

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
int do_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime)
{
/* Set file times.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct sffs_attr attr;
  int r;

  if (read_only)
	return EROFS;

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  attr.a_mask = 0;

  switch (atime->tv_nsec) {
  case UTIME_OMIT: /* do not touch */
	break;
  case UTIME_NOW:
	/* XXX VFS should have time() into ACTIME, for compat; we trust it! */
	atime->tv_nsec = 0;
	/*FALLTHROUGH*/
  default:
	attr.a_atime = *atime;
	attr.a_mask |= SFFS_ATTR_ATIME;
	break;
  }

  switch (mtime->tv_nsec) {
  case UTIME_OMIT: /* do not touch */
	break;
  case UTIME_NOW:
	/* XXX VFS should have time() into MODTIME, for compat; we trust it! */
	mtime->tv_nsec = 0;
	/*FALLTHROUGH*/
  default:
	attr.a_mtime = *mtime;
	attr.a_mask |= SFFS_ATTR_MTIME;
	break;
  }

  return sffs_table->t_setattr(path, &attr);
}
