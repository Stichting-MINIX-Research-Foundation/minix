/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   do_chdir:	perform the CHDIR system call
 *   do_chroot:	perform the CHROOT system call
 *   do_lstat:  perform the LSTAT system call
 *   do_stat:	perform the STAT system call
 *   do_fstat:	perform the FSTAT system call
 *   do_fstatfs: perform the FSTATFS system call
 *   do_statvfs: perform the STATVFS system call
 *   do_fstatvfs: perform the FSTATVFS system call
 */

#include "fs.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <string.h>
#include "file.h"
#include "fproc.h"
#include "path.h"
#include "param.h"
#include <minix/vfsif.h>
#include <minix/callnr.h>
#include "vnode.h"
#include "vmnt.h"

static int change_into(struct vnode **iip, struct vnode *vp);

/*===========================================================================*
 *				do_fchdir				     *
 *===========================================================================*/
int do_fchdir()
{
  /* Change directory on already-opened fd. */
  struct filp *rfilp;
  int r, rfd;

  rfd = job_m_in.fd;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);
  r = change_into(&fp->fp_wd, rfilp->filp_vno);
  unlock_filp(rfilp);
  return(r);
}

/*===========================================================================*
 *				do_chdir				     *
 *===========================================================================*/
int do_chdir()
{
/* Perform the chdir(name) system call.
 * syscall might provide 'name' embedded in the message.
 */

  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname;
  size_t vname_length;

  vname = (vir_bytes) job_m_in.name;
  vname_length = (size_t) job_m_in.name_length;

  if (copy_name(vname_length, fullpath) != OK) {
	/* Direct copy failed, try fetching from user space */
	if (fetch_name(vname, vname_length, fullpath) != OK)
		return(err_code);
  }

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
int do_chroot()
{
/* Perform the chroot(name) system call.
 * syscall might provide 'name' embedded in the message.
 */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname;
  size_t vname_length;

  vname = (vir_bytes) job_m_in.name;
  vname_length = (size_t) job_m_in.name_length;

  if (!super_user) return(EPERM);	/* only su may chroot() */

  if (copy_name(vname_length, fullpath) != OK) {
	/* Direct copy failed, try fetching from user space */
	if (fetch_name(vname, vname_length, fullpath) != OK)
		return(err_code);
  }

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
int do_stat()
{
/* Perform the stat(name, buf) system call. */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  int old_stat = 0;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = (vir_bytes) job_m_in.name1;
  vname1_length = (size_t) job_m_in.name1_length;
  statbuf = (vir_bytes) job_m_in.m1_p2;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (job_call_nr == PREV_STAT)
	old_stat = 1;

  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, statbuf, old_stat);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return r;
}

/*===========================================================================*
 *				do_fstat				     *
 *===========================================================================*/
int do_fstat()
{
/* Perform the fstat(fd, buf) system call. */
  register struct filp *rfilp;
  int r, old_stat = 0, rfd;
  vir_bytes statbuf;

  statbuf = (vir_bytes) job_m_in.buffer;
  rfd = job_m_in.fd;

  if (job_call_nr == PREV_FSTAT)
	old_stat = 1;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);

  r = req_stat(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
	       who_e, statbuf, old_stat);

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
int do_fstatfs()
{
/* Perform the fstatfs(fd, buf) system call. */
  struct filp *rfilp;
  int r, rfd;
  vir_bytes statbuf;

  rfd = job_m_in.fd;
  statbuf = (vir_bytes) job_m_in.buffer;

  /* Is the file descriptor valid? */
  if( (rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);

  r = req_fstatfs(rfilp->filp_vno->v_fs_e, who_e, statbuf);

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *				do_statvfs				     *
 *===========================================================================*/
int do_statvfs()
{
/* Perform the stat(name, buf) system call. */
  int r;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = (vir_bytes) job_m_in.name1;
  vname1_length = (size_t) job_m_in.name1_length;
  statbuf = (vir_bytes) job_m_in.name2;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = req_statvfs(vp->v_fs_e, who_e, statbuf);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return r;
}

/*===========================================================================*
 *				do_fstatvfs				     *
 *===========================================================================*/
int do_fstatvfs()
{
/* Perform the fstat(fd, buf) system call. */
  register struct filp *rfilp;
  int r, rfd;
  vir_bytes statbuf;

  rfd = job_m_in.fd;
  statbuf = (vir_bytes) job_m_in.name2;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);
  r = req_statvfs(rfilp->filp_vno->v_fs_e, who_e, statbuf);

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *                             do_lstat					     *
 *===========================================================================*/
int do_lstat()
{
/* Perform the lstat(name, buf) system call. */
  struct vnode *vp;
  struct vmnt *vmp;
  int r;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  int old_stat = 0;
  vir_bytes vname1, statbuf;
  size_t vname1_length;

  vname1 = (vir_bytes) job_m_in.name1;
  vname1_length = (size_t) job_m_in.name1_length;
  statbuf = (vir_bytes) job_m_in.name2;

  lookup_init(&resolve, fullpath, PATH_RET_SYMLINK, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  if (job_call_nr == PREV_LSTAT)
	old_stat = 1;
  if (fetch_name(vname1, vname1_length, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, statbuf, old_stat);

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return(r);
}
