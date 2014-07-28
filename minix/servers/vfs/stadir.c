/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   do_chdir:	perform the CHDIR system call
 *   do_chroot:	perform the CHROOT system call
 *   do_lstat:  perform the LSTAT system call
 *   do_stat:	perform the STAT system call
 *   do_fstat:	perform the FSTAT system call
 *   do_statvfs:    perform the STATVFS1 system call
 *   do_fstatvfs:   perform the FSTATVFS1 system call
 *   do_getvfsstat: perform the GETVFSSTAT system call
 */

#include "fs.h"
#include <sys/stat.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <string.h>
#include "file.h"
#include "path.h"
#include <minix/vfsif.h>
#include <minix/callnr.h>
#include "vnode.h"
#include "vmnt.h"

static int change_into(struct vnode **iip, struct vnode *vp);

/*===========================================================================*
 *				do_fchdir				     *
 *===========================================================================*/
int do_fchdir(void)
{
  /* Change directory on already-opened fd. */
  struct filp *rfilp;
  int r, rfd;

  rfd = job_m_in.m_lc_vfs_fchdir.fd;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);
  r = change_into(&fp->fp_wd, rfilp->filp_vno);
  unlock_filp(rfilp);
  return(r);
}

/*===========================================================================*
 *				do_chdir				     *
 *===========================================================================*/
int do_chdir(void)
{
/* Perform the chdir(name) system call.
 * syscall might provide 'name' embedded in the message.
 */

  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  if (copy_path(fullpath, sizeof(fullpath)) != OK)
	return(err_code);

  /* Try to open the directory */
  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  r = change_into(&fp->fp_wd, vp);

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);

  return(r);
}

/*===========================================================================*
 *				do_chroot				     *
 *===========================================================================*/
int do_chroot(void)
{
/* Perform the chroot(name) system call.
 * syscall might provide 'name' embedded in the message.
 */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;

  if (!super_user) return(EPERM);	/* only su may chroot() */

  if (copy_path(fullpath, sizeof(fullpath)) != OK)
	return(err_code);

  /* Try to open the directory */
  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  r = change_into(&fp->fp_rd, vp);

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);

  return(r);
}

/*===========================================================================*
 *				change_into				     *
 *===========================================================================*/
static int change_into(struct vnode **result, struct vnode *vp)
{
  int r;

  if (*result == vp) return(OK);	/* Nothing to do */

  /* It must be a directory and also be searchable */
  if (!S_ISDIR(vp->v_mode))
	r = ENOTDIR;
  else
	r = forbidden(fp, vp, X_BIT);	/* Check if dir is searchable*/
  if (r != OK) return(r);

  /* Everything is OK.  Make the change. */
  put_vnode(*result);		/* release the old directory */
  dup_vnode(vp);
  *result = vp;			/* acquire the new one */
  return(OK);
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
int do_stat(void)
{
/* Perform the stat(name, buf) system call. */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = job_m_in.m_lc_vfs_stat.name;
  vname1_length = job_m_in.m_lc_vfs_stat.len;
  statbuf = job_m_in.m_lc_vfs_stat.buf;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, statbuf);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return r;
}

/*===========================================================================*
 *				do_fstat				     *
 *===========================================================================*/
int do_fstat(void)
{
/* Perform the fstat(fd, buf) system call. */
  register struct filp *rfilp;
  int r, rfd;
  vir_bytes statbuf;

  statbuf = job_m_in.m_lc_vfs_fstat.buf;
  rfd = job_m_in.m_lc_vfs_fstat.fd;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);

  r = req_stat(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
	       who_e, statbuf);

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *				update_statvfs				     *
 *===========================================================================*/
int update_statvfs(struct vmnt *vmp, struct statvfs *buf)
{
/* Get statistics from a file system, and cache part of the results. */
  int r;

  if ((r = req_statvfs(vmp->m_fs_e, buf)) != OK)
	return r;

  vmp->m_stats.f_flag = buf->f_flag;
  vmp->m_stats.f_bsize = buf->f_bsize;
  vmp->m_stats.f_frsize = buf->f_frsize;
  vmp->m_stats.f_iosize = buf->f_iosize;

  vmp->m_stats.f_blocks = buf->f_blocks;
  vmp->m_stats.f_bfree = buf->f_bfree;
  vmp->m_stats.f_bavail = buf->f_bavail;
  vmp->m_stats.f_bresvd = buf->f_bresvd;

  vmp->m_stats.f_files = buf->f_files;
  vmp->m_stats.f_ffree = buf->f_ffree;
  vmp->m_stats.f_favail = buf->f_favail;
  vmp->m_stats.f_fresvd = buf->f_fresvd;

  vmp->m_stats.f_syncreads = buf->f_syncreads;
  vmp->m_stats.f_syncwrites = buf->f_syncwrites;

  vmp->m_stats.f_asyncreads = buf->f_asyncreads;
  vmp->m_stats.f_asyncwrites = buf->f_asyncwrites;

  vmp->m_stats.f_namemax = buf->f_namemax;

  return OK;
}

/*===========================================================================*
 *				fill_statvfs				     *
 *===========================================================================*/
static int fill_statvfs(struct vmnt *vmp, endpoint_t endpt, vir_bytes buf_addr,
	int flags)
{
/* Fill a statvfs structure in a userspace process.  First let the target file
 * server fill in most fields, or use the cached copy if ST_NOWAIT is given.
 * Then fill in some remaining fields with local information.  Finally, copy
 * the result to user space.
 */
  struct statvfs buf;

  if (!(flags & ST_NOWAIT)) {
	/* Get fresh statistics from the file system. */
	if (update_statvfs(vmp, &buf) != OK)
		return EIO;
  } else {
	/* Use the cached statistics. */
	memset(&buf, 0, sizeof(buf));

	buf.f_flag = vmp->m_stats.f_flag;
	buf.f_bsize = vmp->m_stats.f_bsize;
	buf.f_frsize = vmp->m_stats.f_frsize;
	buf.f_iosize = vmp->m_stats.f_iosize;

	buf.f_blocks = vmp->m_stats.f_blocks;
	buf.f_bfree = vmp->m_stats.f_bfree;
	buf.f_bavail = vmp->m_stats.f_bavail;
	buf.f_bresvd = vmp->m_stats.f_bresvd;

	buf.f_files = vmp->m_stats.f_files;
	buf.f_ffree = vmp->m_stats.f_ffree;
	buf.f_favail = vmp->m_stats.f_favail;
	buf.f_fresvd = vmp->m_stats.f_fresvd;

	buf.f_syncreads = vmp->m_stats.f_syncreads;
	buf.f_syncwrites = vmp->m_stats.f_syncwrites;

	buf.f_asyncreads = vmp->m_stats.f_asyncreads;
	buf.f_asyncwrites = vmp->m_stats.f_asyncwrites;

	buf.f_namemax = vmp->m_stats.f_namemax;
  }

  if (vmp->m_flags & VMNT_READONLY)
	buf.f_flag |= ST_RDONLY;

  buf.f_fsid = (unsigned long)vmp->m_dev;
  buf.f_fsidx.__fsid_val[0] = (long)vmp->m_dev; /* This is what is done on NetBSD */
  buf.f_fsidx.__fsid_val[1] = 0; /* Here they convert the FS type name into a number. */

  strlcpy(buf.f_fstypename, vmp->m_fstype, sizeof(buf.f_fstypename));
  strlcpy(buf.f_mntonname, vmp->m_mount_path, sizeof(buf.f_mntonname));
  strlcpy(buf.f_mntfromname, vmp->m_mount_dev, sizeof(buf.f_mntfromname));

  return sys_datacopy_wrapper(SELF, (vir_bytes) &buf,
	endpt, buf_addr, sizeof(buf));
}

/*===========================================================================*
 *				do_statvfs				     *
 *===========================================================================*/
int do_statvfs(void)
{
/* Perform the statvfs1(name, buf, flags) system call. */
  int r, flags;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = job_m_in.m_lc_vfs_statvfs1.name;
  vname1_length = job_m_in.m_lc_vfs_statvfs1.len;
  statbuf = job_m_in.m_lc_vfs_statvfs1.buf;
  flags = job_m_in.m_lc_vfs_statvfs1.flags;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = fill_statvfs(vp->v_vmnt, who_e, statbuf, flags);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return r;
}

/*===========================================================================*
 *				do_fstatvfs				     *
 *===========================================================================*/
int do_fstatvfs(void)
{
/* Perform the fstatvfs1(fd, buf, flags) system call. */
  register struct filp *rfilp;
  int r, rfd, flags;
  vir_bytes statbuf;

  rfd = job_m_in.m_lc_vfs_statvfs1.fd;
  statbuf = job_m_in.m_lc_vfs_statvfs1.buf;
  flags = job_m_in.m_lc_vfs_statvfs1.flags;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);
  r = fill_statvfs(rfilp->filp_vno->v_vmnt, who_e, statbuf, flags);

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *				do_getvfsstat				     *
 *===========================================================================*/
int do_getvfsstat(void)
{
/* Perform the getvfsstat(buf, bufsize, flags) system call. */
  struct vmnt *vmp;
  vir_bytes buf;
  size_t bufsize;
  int r, flags, count, do_lock;

  buf = job_m_in.m_lc_vfs_getvfsstat.buf;
  bufsize = job_m_in.m_lc_vfs_getvfsstat.len;
  flags = job_m_in.m_lc_vfs_getvfsstat.flags;

  count = 0;

  if (buf != 0) {
	/* We only need to lock target file systems if we are going to query
	 * them.  This will only happen if ST_NOWAIT is not given.  If we do
	 * not lock, we rely on the VMNT_CANSTAT flag to protect us from
	 * concurrent (un)mount operations.  Note that procfs relies on
	 * ST_NOWAIT calls being lock free, as it is a file system itself.
	 */
	do_lock = !(flags & ST_NOWAIT);

	for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
		/* If there is no more space, return the count so far. */
		if (bufsize < sizeof(struct statvfs))
			break;

		/* Lock the file system before checking any fields. */
		if (do_lock && (r = lock_vmnt(vmp, VMNT_READ)) != OK)
			return r;

		/* Obtain information for this file system, if it is in use and
		 * can be reported.  File systems that are being (un)mounted
		 * are skipped, as is PFS.  The fill call will block only if
		 * ST_NOWAIT was not given.
		 */
		if (vmp->m_dev != NO_DEV && (vmp->m_flags & VMNT_CANSTAT)) {
			if ((r = fill_statvfs(vmp, who_e, buf, flags)) != OK) {
				if (do_lock)
					unlock_vmnt(vmp);

				return r;
			}

			count++;
			buf += sizeof(struct statvfs);
			bufsize -= sizeof(struct statvfs);
		}

		if (do_lock)
			unlock_vmnt(vmp);
	}
  } else {
	/* Just report a file system count.  No need to lock, as above. */
	for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
		if (vmp->m_dev != NO_DEV && (vmp->m_flags & VMNT_CANSTAT))
			count++;
	}
  }

  return count;
}

/*===========================================================================*
 *                             do_lstat					     *
 *===========================================================================*/
int do_lstat(void)
{
/* Perform the lstat(name, buf) system call. */
  struct vnode *vp;
  struct vmnt *vmp;
  int r;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = job_m_in.m_lc_vfs_stat.name;
  vname1_length = job_m_in.m_lc_vfs_stat.len;
  statbuf = job_m_in.m_lc_vfs_stat.buf;

  lookup_init(&resolve, fullpath, PATH_RET_SYMLINK, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, statbuf);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return(r);
}
