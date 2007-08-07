/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:		perform the UTIME system call
 *   do_stime:		PM informs FS about STIME system call
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
  uid_t uid;
  time_t actime, modtime;
  struct vnode *vp;
  
  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1.
   */
  len = m_in.utime_length;
  if (len == 0) len = m_in.utime_strlen;

  if (fetch_name(m_in.utime_file, len, M1) != OK) return(err_code);
  
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp)) != OK) return r;

  /* Fill in request fields.*/
  if (m_in.utime_length == 0) {
        actime = modtime = clock_time();
  } else {
        actime = m_in.utime_actime;
        modtime = m_in.utime_modtime;
  }

  uid= fp->fp_effuid;

  r= OK;
  if (vp->v_uid != uid && uid != SU_UID) r = EPERM;
  if (m_in.utime_length == 0 && r != OK)
  {
	/* With a null times pointer, updating the times (to the current time)
	 * is allow if the object is writable. 
	 */
	r = forbidden(vp, W_BIT, 0 /*!use_realuid*/);
  }

  if (r != OK)
  {
	put_vnode(vp);
	return r;
  }
  
  /* Issue request */
  r= req_utime(vp->v_fs_e, vp->v_inode_nr, actime, modtime);
  put_vnode(vp);
  return r;
}

