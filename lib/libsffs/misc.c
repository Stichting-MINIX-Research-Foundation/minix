/* This file contains miscellaneous file system call handlers.
 *
 * The entry points into this file are:
 *   do_fstatfs		perform the FSTATFS file system call
 *   do_statvfs		perform the STATVFS file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <sys/statfs.h>
#include <sys/statvfs.h>

/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
int do_fstatfs()
{
/* Retrieve file system statistics.
 */
  struct statfs statfs;

  statfs.f_bsize = BLOCK_SIZE; /* arbitrary block size constant */

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &statfs, sizeof(statfs));
}

/*===========================================================================*
 *				do_statvfs				     *
 *===========================================================================*/
int do_statvfs()
{
/* Retrieve file system statistics.
 */
  struct statvfs statvfs;
  struct inode *ino;
  char path[PATH_MAX];
  u64_t free, total;
  int r;

  /* Unfortunately, we cannot be any more specific than this, because we are
   * not given an inode number. Statistics of individual shared folders can
   * only be obtained by making sure that the root of the file system is an
   * actual share, and not a list of available shares.
   */
  if ((ino = find_inode(ROOT_INODE_NR)) == NULL)
	return EINVAL;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  if ((r = sffs_table->t_queryvol(path, &free, &total)) != OK)
	return r;

  memset(&statvfs, 0, sizeof(statvfs));

  /* Returning zero for unknown values seems to be the convention. However, we
   * do have to use a nonzero block size, even though it is entirely arbitrary.
   */
  statvfs.f_bsize = BLOCK_SIZE;
  statvfs.f_frsize = BLOCK_SIZE;
  statvfs.f_blocks = div64u(total, BLOCK_SIZE);
  statvfs.f_bfree = div64u(free, BLOCK_SIZE);
  statvfs.f_bavail = statvfs.f_bfree;
  statvfs.f_files = 0;
  statvfs.f_ffree = 0;
  statvfs.f_favail = 0;
  statvfs.f_fsid = state.s_dev;
  statvfs.f_flag = state.s_read_only ? ST_RDONLY : 0;
  statvfs.f_flag |= ST_NOTRUNC;
  statvfs.f_namemax = NAME_MAX;

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &statvfs, sizeof(statvfs));
}
