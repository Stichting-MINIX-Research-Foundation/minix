/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:		perform the UTIME system call
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <time.h>
#include "file.h"
#include "fproc.h"
#include "path.h"
#include "param.h"
#include "vnode.h"
#include <minix/vfsif.h>
#include "vmnt.h"

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
int do_utime()
{
/* Perform the utime(name, timep) system call. */
  int r;
  struct timespec actim, modtim, newactim, newmodtim;
  struct vnode *vp;
  struct vmnt *vmp;
  char fullpath[PATH_MAX];
  struct lookup resolve;
  vir_bytes vname;
  size_t vname_length, len;

  vname = (vir_bytes) job_m_in.utime_file;
  vname_length = (size_t) job_m_in.utime_length;
  actim.tv_sec = job_m_in.utime_actime;
  modtim.tv_sec = job_m_in.utime_modtime;
  actim.tv_nsec = modtim.tv_nsec = 0;

  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1 */
  len = vname_length;
  if (len == 0) len = (size_t) job_m_in.utime_strlen;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  /* Temporarily open the file */
  if (fetch_name(vname, len, fullpath) != OK) return(err_code);
  if ((vp = eat_path(&resolve, fp)) == NULL) return(err_code);

  /* Only the owner of a file or the super user can change timestamps. */
  r = OK;
  if (vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID) r = EPERM;
  if (vname_length == 0 && r != OK) r = forbidden(fp, vp, W_BIT);
  if (read_only(vp) != OK) r = EROFS; /* Not even su can touch if R/O */
  if (r == OK) {
	/* Issue request */
	if (vname_length == 0) {
		newactim = newmodtim = clock_timespec();
	} else {
		newactim = actim;
		newmodtim = modtim;
	}
	r = req_utime(vp->v_fs_e, vp->v_inode_nr, &newactim, &newmodtim);
  }

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return(r);
}
