/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   do_chdir:	perform the CHDIR system call
 *   do_chroot:	perform the CHROOT system call
 *   do_stat:	perform the STAT system call
 *   do_fstat:	perform the FSTAT system call
 *   do_fstatfs: perform the FSTATFS system call
 *   do_lstat:  perform the LSTAT system call
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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
  int r;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  /* Is it a dir? */
  if ((rfilp->filp_vno->v_mode & I_TYPE) != I_DIRECTORY)
      return ENOTDIR;
  
  /* Issue request and handle error */
  r = forbidden(rfilp->filp_vno, X_BIT, 0 /*!use_realuid*/);
  if (r != OK) return r;
  
  rfilp->filp_vno->v_ref_count++;	/* change_into expects a reference  */
  
  return change_into(&fp->fp_wd, rfilp->filp_vno);
}

/*===========================================================================*
 *				do_chdir				     *
 *===========================================================================*/
PUBLIC int do_chdir()
{
/* Change directory.  This function is  also called by MM to simulate a chdir
 * in order to do EXEC, etc.  It also changes the root directory, the uids and
 * gids, and the umask. 
 */
  int r;
  register struct fproc *rfp;

  if (who_e == PM_PROC_NR) {
	int slot;
	if(isokendpt(m_in.endpt1, &slot) != OK)
		return EINVAL;
	rfp = &fproc[slot];
        
        put_vnode(fp->fp_rd);
        dup_vnode(fp->fp_rd = rfp->fp_rd);
        put_vnode(fp->fp_wd);
        dup_vnode(fp->fp_wd = rfp->fp_wd);
        
	/* MM uses access() to check permissions.  To make this work, pretend
	 * that the user's real ids are the same as the user's effective ids.
	 * FS calls other than access() do not use the real ids, so are not
	 * affected.
	 */
	fp->fp_realuid =
	fp->fp_effuid = rfp->fp_effuid;
	fp->fp_realgid =
	fp->fp_effgid = rfp->fp_effgid;
	fp->fp_umask = rfp->fp_umask;
	return(OK);
  }

  /* Perform the chdir(name) system call. */
  r = change(&fp->fp_wd, m_in.name, m_in.name_length);
  return(r);
}

/*===========================================================================*
 *				do_chroot				     *
 *===========================================================================*/
PUBLIC int do_chroot()
{
/* Perform the chroot(name) system call. */

  register int r;

  if (!super_user) return(EPERM);	/* only su may chroot() */
  
  r = change(&fp->fp_rd, m_in.name, m_in.name_length);
  return(r);
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

  if (fetch_name(name_ptr, len, M3) != OK) return(err_code);
  
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp)) != OK) return r;

  /* Is it a dir? */
  if ((vp->v_mode & I_TYPE) != I_DIRECTORY)
  {
      put_vnode(vp);
      return ENOTDIR;
  }

  /* Access check */
  r = forbidden(vp, X_BIT, 0 /*!use_realuid*/);
  if (r != OK) {
        put_vnode(vp);
	return r;
  }

  return change_into(iip, vp);
}


/*===========================================================================*
 *				change_into				     *
 *===========================================================================*/
PRIVATE int change_into(iip, vp)
struct vnode **iip;		/* pointer to the inode pointer for the dir */
struct vnode *vp;		/* this is what the inode has to become */
{
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
  
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp)) != OK)
	return r;

  /* Issue request */
  r= req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2, 0);
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
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) {
	  return(err_code);
  }
  
  /* If we read from a pipe, send position too */
  pipe_pos= 0;
  if (rfilp->filp_vno->v_pipe == I_PIPE) {
	if (rfilp->filp_mode & R_BIT) 
		if (ex64hi(rfilp->filp_pos) != 0)
		{
			panic(__FILE__, "do_fstat: bad position in pipe",
				NO_NUM);
		}
		pipe_pos = ex64lo(rfilp->filp_pos);
  }

  /* Issue request */
  return req_stat(rfilp->filp_vno->v_fs_e, rfilp->filp_vno->v_inode_nr,
	who_e, m_in.buffer, pipe_pos);
}



/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
PUBLIC int do_fstatfs()
{
  /* Perform the fstatfs(fd, buf) system call. */
  register struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  /* Issue request */
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
  
  /* Request lookup */
  if ((r = lookup_vp(PATH_RET_SYMLINK, 0 /*!use_realuid*/, &vp)) != OK)
	return r;

  /* Issue request */
  r= req_stat(vp->v_fs_e, vp->v_inode_nr, who_e, m_in.name2, 0);

  put_vnode(vp);

  return r;
}



