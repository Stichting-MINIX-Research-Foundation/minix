/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:		perform the UTIME system call
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include "vnode.h"
#include <minix/vfsif.h>
#include "vmnt.h"

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
PUBLIC int do_utime()
{
/* Perform the utime(name, timep) system call. */
  register int len;
  int r;
  time_t actime, modtime;
  struct vnode *vp;
  
  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1 */
  len = m_in.utime_length;
  if(len == 0) len = m_in.utime_strlen;

  /* Temporarily open the file */
  if (fetch_name(m_in.utime_file, len, M1) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS, fp)) == NULL) return(err_code);

  /* Only the owner of a file or the super user can change its name. */  
  r = OK;
  if (vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID) r = EPERM;
  if (m_in.utime_length == 0 && r != OK) r = forbidden(vp, W_BIT);
  if (read_only(vp) != OK) r = EROFS; /* Not even su can touch if R/O */ 
  if (r == OK) {
	/* Issue request */
	if(m_in.utime_length == 0) {
		actime = modtime = clock_time();
	} else {
		actime = m_in.utime_actime;
		modtime = m_in.utime_modtime;
	}
	r = req_utime(vp->v_fs_e, vp->v_inode_nr, actime, modtime);
  }

  put_vnode(vp);
  return(r);
}

