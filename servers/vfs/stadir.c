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
 */

#include "fs.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <string.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

FORWARD _PROTOTYPE( int change, (struct vnode **iip, char *name_ptr, int len));
FORWARD _PROTOTYPE( int change_into, (struct vnode **iip, struct vnode *vp));


/*===========================================================================*
 *				do_fchdir				     *
 *===========================================================================*/
PUBLIC int do_fchdir()
{
  /* Change directory on already-opened fd. */
  struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  dup_vnode(rfilp->filp_vno);	/* Change into expects a reference. */
  return change_into(&fp->fp_wd, rfilp->filp_vno);
}


/*===========================================================================*
 *				do_chdir				     *
 *===========================================================================*/
PUBLIC int do_chdir()
{
/* Perform the chdir(name) system call. */

  return change(&fp->fp_wd, m_in.name, m_in.name_length);
}


/*===========================================================================*
 *				do_chroot				     *
 *===========================================================================*/
PUBLIC int do_chroot()
{
/* Perform the chroot(name) system call. */

  if (!super_user) return(EPERM);	/* only su may chroot() */
  return change(&fp->fp_rd, m_in.name, m_in.name_length);
}


/*===========================================================================*
 *				change					     *
 *===========================================================================*/
PRIVATE int change(iip, name_ptr, len)
struct vnode **iip;		/* pointer to the inode pointer for the dir */
char *name_ptr;			/* pointer to the directory name to change to */
int len;			/* length of the directory name string */
{
/* Do the actual work for chdir() and chroot(). */
  struct vnode *vp;
  int r;

  /* Try to open the directory */
  if (fetch_name(name_ptr, len, M3) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
  return change_into(iip, vp);
}


/*===========================================================================*
 *				change_into				     *
 *===========================================================================*/
PRIVATE int change_into(iip, vp)
struct vnode **iip;		/* pointer to the inode pointer for the dir */
struct vnode *vp;		/* this is what the inode has to become */
{
  int r;

  /* It must be a directory and also be searchable */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY)
  	r = ENOTDIR;
  else
	r = forbidden(vp, X_BIT);	/* Check if dir is searchable*/

  /* If error, return vnode */
  if (r != OK) {
  	put_vnode(vp);
  	return(r);
  }

  /* Everything is OK.  Make the change. */
  put_vnode(*iip);		/* release the old directory */
  *iip = vp;			/* acquire the new one */
  return(OK);
}


/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat()
{
/* Perform the stat(name, buf) system call. */
  int r;
  struct vnode *vp;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2, 0);

  put_vnode(vp);
  return r;
}


/*===========================================================================*
 *				do_fstat				     *
 *===========================================================================*/
PUBLIC int do_fstat()
{
/* Perform the fstat(fd, buf) system call. */
  register struct filp *rfilp;
  int pipe_pos = 0;

  /* Is the file descriptor valid? */
  if ((rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  
  /* If we read from a pipe, send position too */
  if (rfilp->filp_vno->v_pipe == I_PIPE) {
	if (rfilp->filp_mode & R_BIT) 
		if (ex64hi(rfilp->filp_pos) != 0) {
			panic(__FILE__, "do_fstat: bad position in pipe",
				NO_NUM);
		}
	pipe_pos = ex64lo(rfilp->filp_pos);
  }

  return req_stat(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
		  who_e, m_in.buffer, pipe_pos);
}


/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
PUBLIC int do_fstatfs()
{
/* Perform the fstatfs(fd, buf) system call. */
  struct filp *rfilp;

  /* Is the file descriptor valid? */
  if( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  return req_fstatfs(rfilp->filp_vno->v_fs_e, who_e, m_in.buffer);
}


/*===========================================================================*
 *                             do_lstat					     *
 *===========================================================================*/
PUBLIC int do_lstat()
{
/* Perform the lstat(name, buf) system call. */
  struct vnode *vp;
  int r;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_RET_SYMLINK)) == NIL_VNODE) return(err_code);
  r = req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2, 0);

  put_vnode(vp);
  return(r);
}


