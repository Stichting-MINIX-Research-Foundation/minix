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
PUBLIC mode_t get_mode(ino, mode)
struct inode *ino;
int mode;
{
/* Return the mode for an inode, given the inode and the HGFS retrieved mode.
 */

  mode &= S_IRWXU;
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(ino))
	mode = S_IFDIR | (mode & opt.dir_mask);
  else
	mode = S_IFREG | (mode & opt.file_mask);

  if (state.read_only)
	mode &= ~0222;

  return mode;
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat()
{
/* Retrieve inode statistics.
 */
  struct inode *ino;
  struct hgfs_attr attr;
  struct stat stat;
  char path[PATH_MAX];
  ino_t ino_nr;
  int r;

  ino_nr = m_in.REQ_INODE_NR;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((ino = find_inode(ino_nr)) == NIL_INODE)
	return EINVAL;

  attr.a_mask = HGFS_ATTR_MODE | HGFS_ATTR_SIZE | HGFS_ATTR_ATIME |
		HGFS_ATTR_MTIME | HGFS_ATTR_CTIME;

  if ((r = verify_inode(ino, path, &attr)) != OK)
	return r;

  stat.st_dev = state.dev;
  stat.st_ino = ino_nr;
  stat.st_mode = get_mode(ino, attr.a_mode);
  stat.st_uid = opt.uid;
  stat.st_gid = opt.gid;
  stat.st_rdev = NO_DEV;
  stat.st_size = ex64hi(attr.a_size) ? ULONG_MAX : ex64lo(attr.a_size);
  stat.st_atime = attr.a_atime;
  stat.st_mtime = attr.a_mtime;
  stat.st_ctime = attr.a_ctime;

  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
  stat.st_nlink = 0;
  if (ino->i_parent != NIL_INODE) stat.st_nlink++;
  if (IS_DIR(ino)) {
	stat.st_nlink++;
	if (HAS_CHILDREN(ino)) stat.st_nlink++;
  }

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &stat, sizeof(stat), D);
}

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod()
{
/* Change file mode.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct hgfs_attr attr;
  int r;

  if (state.read_only)
	return EROFS;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NIL_INODE)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  /* Set the new file mode. */
  attr.a_mask = HGFS_ATTR_MODE;
  attr.a_mode = m_in.REQ_MODE; /* no need to convert in this direction */

  if ((r = hgfs_setattr(path, &attr)) != OK)
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
PUBLIC int do_utime()
{
/* Set file times.
 */
  struct inode *ino;
  char path[PATH_MAX];
  struct hgfs_attr attr;
  int r;

  if (state.read_only)
	return EROFS;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NIL_INODE)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  attr.a_mask = HGFS_ATTR_ATIME | HGFS_ATTR_MTIME;
  attr.a_atime = m_in.REQ_ACTIME;
  attr.a_mtime = m_in.REQ_MODTIME;

  return hgfs_setattr(path, &attr);
}
