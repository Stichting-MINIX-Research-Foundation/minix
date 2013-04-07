/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <minix/vfsif.h>

#include "puffs.h"
#include "puffs_priv.h"


/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
int fs_fstatfs()
{
  int r;
  struct statvfs st_vfs;
  struct statfs st;

  if (global_pu->pu_ops.puffs_fs_statvfs(global_pu, &st_vfs) != 0) {
	lpuffs_debug("statfs failed\n");
	return(EINVAL);
  }

  st.f_bsize = st_vfs.f_bsize;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT,
		     (vir_bytes) 0, (vir_bytes) &st, (size_t) sizeof(st));

  return(r);
}


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat()
{
  register int r;              /* return value */
  register struct puffs_node *pn;  /* target pnode */
  struct vattr va;
  struct stat statbuf;
  mode_t mo;
  int s;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (global_pu->pu_ops.puffs_node_getattr == NULL) {
	lpuffs_debug("fs_stat: puffs_node_getattr is missing\n");
	return(EINVAL);
  }

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL) {
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

  statbuf.st_dev = fs_dev;
  statbuf.st_ino = va.va_fileid;
  statbuf.st_mode = va.va_mode;
  statbuf.st_nlink = va.va_nlink;
  statbuf.st_uid = va.va_uid;
  statbuf.st_gid = va.va_gid;
  statbuf.st_rdev = (s ? va.va_rdev : NO_DEV);
  statbuf.st_size = va.va_size;
  statbuf.st_atimespec = va.va_atime;
  statbuf.st_mtimespec = va.va_mtime;
  statbuf.st_ctimespec = va.va_ctime;

  statbuf.st_birthtimespec = va.va_birthtime;
  statbuf.st_blksize = va.va_blocksize;
  statbuf.st_blocks = va.va_bytes / va.va_blocksize;
  statbuf.st_flags = va.va_flags;
  statbuf.st_gen = va.va_gen;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT,
		     (vir_bytes) 0, (vir_bytes) &statbuf,
		     (size_t) sizeof(statbuf));

  return(r);
}


/*===========================================================================*
 *                             fs_statvfs                                    *
 *===========================================================================*/
int fs_statvfs()
{
  int r;
  struct statvfs st;

  if (global_pu->pu_ops.puffs_fs_statvfs(global_pu, &st) != 0) {
	lpuffs_debug("statvfs failed\n");
	return(EINVAL);
  }

  /* XXX libpuffs doesn't truncate filenames and returns ENAMETOOLONG,
   * though some servers would like to behave differently.
   * See subtest 2.18-19 of test23 and test/common.c:does_fs_truncate().
   */
  st.f_flag |= ST_NOTRUNC;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0, (vir_bytes) &st,
                    (phys_bytes) sizeof(st));

  return(r);
}
