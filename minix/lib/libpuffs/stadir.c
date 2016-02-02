/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"

/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
  register struct puffs_node *pn;  /* target pnode */
  struct vattr va;
  mode_t mo;
  int s;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (global_pu->pu_ops.puffs_node_getattr == NULL) {
	lpuffs_debug("fs_stat: puffs_node_getattr is missing\n");
	return(EINVAL);
  }

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL) {
	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  if (global_pu->pu_ops.puffs_node_getattr(global_pu, pn, &va, pcr) != 0) {
	if (errno) {
		if (errno > 0) errno = -errno;
		return(errno);
	}
	return(EINVAL);
  }

  /* Fill in the statbuf struct. */
  mo = va.va_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf->st_mode = va.va_mode;
  statbuf->st_nlink = va.va_nlink;
  statbuf->st_uid = va.va_uid;
  statbuf->st_gid = va.va_gid;
  statbuf->st_rdev = (s ? va.va_rdev : NO_DEV);
  statbuf->st_size = va.va_size;
  statbuf->st_atimespec = va.va_atime;
  statbuf->st_mtimespec = va.va_mtime;
  statbuf->st_ctimespec = va.va_ctime;

  statbuf->st_birthtimespec = va.va_birthtime;
  statbuf->st_blksize = va.va_blocksize;
  statbuf->st_blocks = va.va_bytes / va.va_blocksize;
  statbuf->st_flags = va.va_flags;
  statbuf->st_gen = va.va_gen;

  return(OK);
}


/*===========================================================================*
 *                             fs_statvfs                                    *
 *===========================================================================*/
int fs_statvfs(struct statvfs *st)
{

  if (global_pu->pu_ops.puffs_fs_statvfs(global_pu, st) != 0) {
	lpuffs_debug("statvfs failed\n");
	return(EINVAL);
  }

  /* libpuffs doesn't truncate filenames */
  st->f_flag |= ST_NOTRUNC;

  return(OK);
}
