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
mode_t get_mode(ino, mode)
struct inode *ino;
int mode;
{
/* Return the mode for an inode, given the inode and the retrieved mode.
 */

  mode &= S_IRWXU;
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(ino))
	mode = S_IFDIR | (mode & sffs_params->p_dir_mask);
  else
	mode = S_IFREG | (mode & sffs_params->p_file_mask);

  if (state.s_read_only)
	mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

  return mode;
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
int do_stat()
{
/* Retrieve inode status.
 */
  struct inode *ino;
  struct sffs_attr attr;
  struct stat stat;
  char path[PATH_MAX];
  ino_t ino_nr;
  int r;

  ino_nr = m_in.REQ_INODE_NR;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE | SFFS_ATTR_CRTIME |
		SFFS_ATTR_ATIME | SFFS_ATTR_MTIME | SFFS_ATTR_CTIME;

  if ((r = verify_inode(ino, path, &attr)) != OK)
	return r;

  memset(&stat, 0, sizeof(struct stat));

  stat.st_dev = state.s_dev;
  stat.st_ino = ino_nr;
  stat.st_mode = get_mode(ino, attr.a_mode);
  stat.st_uid = sffs_params->p_uid;
  stat.st_gid = sffs_params->p_gid;
  stat.st_rdev = NO_DEV;
  if (cmp64u(attr.a_size, LONG_MAX) > 0)
	stat.st_size = LONG_MAX;
  else
	stat.st_size = ex64lo(attr.a_size);
  stat.st_atimespec = attr.a_atime;
  stat.st_mtimespec = attr.a_mtime;
  stat.st_ctimespec = attr.a_ctime;
  stat.st_birthtimespec = attr.a_crtime;

  stat.st_blocks = stat.st_size / S_BLKSIZE;
  if (stat.st_size % S_BLKSIZE != 0)
	stat.st_blocks += 1;

  stat.st_blksize = BLOCK_SIZE;

  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
  stat.st_nlink = 0;
  if (ino->i_parent != NULL) stat.st_nlink++;
  if (IS_DIR(ino)) {
	stat.st_nlink++;
	if (HAS_CHILDREN(ino)) stat.st_nlink++;
  }

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &stat, sizeof(stat));
}

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
int do_chmod()
{
/* Change file mode.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct sffs_attr attr;
  int r;

  if (state.s_read_only)
	return EROFS;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  /* Set the new file mode. */
  attr.a_mask = SFFS_ATTR_MODE;
  attr.a_mode = m_in.REQ_MODE; /* no need to convert in this direction */

  if ((r = sffs_table->t_setattr(path, &attr)) != OK)
	return r;

  /* We have no idea what really happened. Query for the mode again. */
  if ((r = verify_path(path, ino, &attr, NULL)) != OK)
	return r;

  m_out.RES_MODE = get_mode(ino, attr.a_mode);

  return OK;
}

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
int do_utime()
{
/* Set file times.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct sffs_attr attr;
  int r;

  if (state.s_read_only)
	return EROFS;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  attr.a_mask = 0;

  switch(m_in.REQ_ACNSEC) {
  case UTIME_OMIT: /* do not touch */
	break;
  case UTIME_NOW:
	/* XXX VFS should have time() into ACTIME, for compat; we trust it! */
	m_in.REQ_ACNSEC = 0;
	/*FALLTHROUGH*/
  default:
	/* cases m_in.REQ_ACNSEC < 0 || m_in.REQ_ACNSEC >= 1E9
	 * are caught by VFS to cooperate with old instances of EXT2
	 */
	attr.a_atime.tv_sec = m_in.REQ_ACTIME;
	attr.a_atime.tv_nsec = m_in.REQ_ACNSEC;
	attr.a_mask |= SFFS_ATTR_ATIME;
	break;
  }
  switch(m_in.REQ_MODNSEC) {
  case UTIME_OMIT: /* do not touch */
	break;
  case UTIME_NOW:
	/* XXX VFS should have time() into MODTIME, for compat; we trust it! */
	m_in.REQ_MODNSEC = 0;
	/*FALLTHROUGH*/
  default:
	/* cases m_in.REQ_MODNSEC < 0 || m_in.REQ_MODNSEC >= 1E9
	 * are caught by VFS to cooperate with old instances
	 */
	attr.a_mtime.tv_sec = m_in.REQ_MODTIME;
	attr.a_mtime.tv_nsec = m_in.REQ_MODNSEC;
	attr.a_mask |= SFFS_ATTR_MTIME;
	break;
  }
  return sffs_table->t_setattr(path, &attr);
}
