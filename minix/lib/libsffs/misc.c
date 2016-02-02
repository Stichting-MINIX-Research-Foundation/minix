/* This file contains miscellaneous file system call handlers.
 *
 * The entry points into this file are:
 *   do_statvfs		perform the STATVFS file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <sys/statvfs.h>

/*===========================================================================*
 *				do_statvfs				     *
 *===========================================================================*/
int do_statvfs(struct statvfs *st)
{
/* Retrieve file system statistics.
 */
  struct inode *ino;
  char path[PATH_MAX];
  u64_t bfree, btotal;
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

  if ((r = sffs_table->t_queryvol(path, &bfree, &btotal)) != OK)
	return r;

  /* Returning zero for unknown values seems to be the convention. However, we
   * do have to use a nonzero block size, even though it is entirely arbitrary.
   */
  st->f_flag = ST_NOTRUNC;
  st->f_bsize = BLOCK_SIZE;
  st->f_frsize = BLOCK_SIZE;
  st->f_iosize = BLOCK_SIZE;
  st->f_blocks = (fsblkcnt_t)(btotal / BLOCK_SIZE);
  st->f_bfree = (fsblkcnt_t)(bfree / BLOCK_SIZE);
  st->f_bavail = st->f_bfree;
  st->f_namemax = NAME_MAX;

  return OK;
}
