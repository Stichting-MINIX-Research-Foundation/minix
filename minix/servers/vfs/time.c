/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utimens:	perform the UTIMENS system call
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "file.h"
#include "path.h"
#include "vnode.h"
#include <minix/vfsif.h>
#include "vmnt.h"

#define	UTIMENS_STYLE	0	/* utimes(2)/utimensat(2) style, named file */
#define	FUTIMENS_STYLE	1	/* futimens(2)/futimes(2) style, file desc. */

/*===========================================================================*
 *				do_utimens				     *
 *===========================================================================*/
int do_utimens(void)
{
/* Perform the utimens(name, times, flag) system call, and its friends.
 * Implement a very large but not complete subset of the utimensat()
 * Posix:2008/XOpen-7 function.
 * Are handled all the following cases:
 * . utimensat(AT_FDCWD, "/some/absolute/path", , )
 * . utimensat(AT_FDCWD, "some/path", , )
 * . utimens("anything", ) really special case of the above two
 * . lutimens("anything", ) also really special case of the above
 * . utimensat(fd, "/some/absolute/path", , ) although fd is useless here
 * . futimens(fd, )
 * Are not handled the following cases:
 * . utimensat(fd, "some/path", , ) path to a file relative to some open fd
 */
  int r, kind, lookup_flags;
  struct vnode *vp;
  struct filp *filp = NULL; /* initialization required by clueless GCC */
  struct vmnt *vmp;
  struct timespec actim, modtim, now, newactim, newmodtim;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname;
  size_t vname_length;

  memset(&now, 0, sizeof(now));

  /* The case times==NULL is handled by the caller, replaced with UTIME_NOW */
  actim.tv_sec = job_m_in.m_vfs_utimens.atime;
  actim.tv_nsec = job_m_in.m_vfs_utimens.ansec;
  modtim.tv_sec = job_m_in.m_vfs_utimens.mtime;
  modtim.tv_nsec = job_m_in.m_vfs_utimens.mnsec;

  if (job_m_in.m_vfs_utimens.name != NULL) {
	kind = UTIMENS_STYLE;
	if (job_m_in.m_vfs_utimens.flags & ~AT_SYMLINK_NOFOLLOW)
		return EINVAL; /* unknown flag */
	/* Temporarily open the file */
	vname = (vir_bytes) job_m_in.m_vfs_utimens.name;
	vname_length = (size_t) job_m_in.m_vfs_utimens.len;
	if (job_m_in.m_vfs_utimens.flags & AT_SYMLINK_NOFOLLOW)
		lookup_flags = PATH_RET_SYMLINK;
	else
		lookup_flags = PATH_NOFLAGS;
	lookup_init(&resolve, fullpath, lookup_flags, &vmp, &vp);
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode_lock = VNODE_READ;
	/* Temporarily open the file */
	if (fetch_name(vname, vname_length, fullpath) != OK) return(err_code);
	if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  }
  else {
	kind = FUTIMENS_STYLE;
	/* Change timestamps on already-opened fd. Is it valid? */
	if (job_m_in.m_vfs_utimens.flags != 0)
		return EINVAL; /* unknown flag */
	if ((filp = get_filp(job_m_in.m_vfs_utimens.fd, VNODE_READ)) == NULL)
		return err_code;
	vp = filp->filp_vno;
  }

  r = OK;
  /* Only the owner of a file or the super user can change timestamps. */
  if (vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID) r = EPERM;
  /* Need write permission (or super user) to 'touch' the file */
  if (r != OK && actim.tv_nsec == UTIME_NOW
              && modtim.tv_nsec == UTIME_NOW) r = forbidden(fp, vp, W_BIT);
  if (read_only(vp) != OK) r = EROFS; /* Not even su can touch if R/O */

  if (r == OK) {
	/* Do we need to ask for current time? */
	if (actim.tv_nsec == UTIME_NOW
	 || actim.tv_nsec == UTIME_OMIT
	 || modtim.tv_nsec == UTIME_NOW
	 || modtim.tv_nsec == UTIME_OMIT) {
		(void)clock_time(&now);
	}

	/* Build the request */
	switch (actim.tv_nsec) {
	case UTIME_NOW:
		newactim = now;
		break;
	case UTIME_OMIT:
		newactim.tv_nsec = UTIME_OMIT;
		/* Be nice with old FS, put a sensible value in
		 * otherwise not used field for seconds
		 */
		newactim.tv_sec = now.tv_sec;
		break;
	default:
		if ( (unsigned)actim.tv_nsec >= 1000000000)
			r = EINVAL;
		else
			newactim = actim;
		break;
	}
	switch (modtim.tv_nsec) {
	case UTIME_NOW:
		newmodtim = now;
		break;
	case UTIME_OMIT:
		newmodtim.tv_nsec = UTIME_OMIT;
		/* Be nice with old FS, put a sensible value */
		newmodtim.tv_sec = now.tv_sec;
		break;
	default:
		if ( (unsigned)modtim.tv_nsec >= 1000000000)
			r = EINVAL;
		else
			newmodtim = modtim;
		break;
	}
  }

  if (r == OK)
	/* Issue request */
	r = req_utime(vp->v_fs_e, vp->v_inode_nr, &newactim, &newmodtim);

  if (kind == UTIMENS_STYLE) {
	/* Close the temporary */
	unlock_vnode(vp);
	unlock_vmnt(vmp);
	put_vnode(vp);
  }
  else { /* Change timestamps on opened fd. */
	unlock_filp(filp);
  }
  return r;
}
