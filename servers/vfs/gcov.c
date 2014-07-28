
#include "fs.h"
#include "file.h"

int gcov_flush(cp_grant_id_t grantid, size_t size );

/*===========================================================================*
 *				do_gcov_flush				*
 *===========================================================================*/
int do_gcov_flush()
{
/* A userland tool has requested the gcov data from another
 * process (possibly vfs itself). Grant the target process
 * access to the supplied buffer, and perform the call that
 * makes the target copy its buffer to the caller (incl vfs
 * itself).
 */
  struct fproc *rfp;
  ssize_t size;
  cp_grant_id_t grantid;
  int r, n;
  pid_t target;
  message m;
  vir_bytes buf;

  size = job_m_in.m_lc_vfs_gcov.buff_sz;
  target = job_m_in.m_lc_vfs_gcov.pid;
  buf = job_m_in.m_lc_vfs_gcov.buff_p;

  /* If the wrong process is sent to, the system hangs; so make this root-only.
   */

  if (!super_user) return(EPERM);

  /* Find target gcov process. */
  for(n = 0; n < NR_PROCS; n++) {
	if(fproc[n].fp_endpoint != NONE && fproc[n].fp_pid == target)
		 break;
  }
  if(n >= NR_PROCS) {
	printf("VFS: gcov process %d not found\n", target);
	return(ESRCH);
  }
  rfp = &fproc[n];

  /* Grant target process to requestor's buffer. */
  if ((grantid = cpf_grant_magic(rfp->fp_endpoint, who_e, buf,
				 size, CPF_WRITE)) < 0) {
	printf("VFS: gcov_flush: grant failed\n");
	return(ENOMEM);
  }

  if (rfp->fp_endpoint == VFS_PROC_NR) {
	/* Request is for VFS itself. */
	r = gcov_flush(grantid, size);
  } else {
	/* Perform generic GCOV request. */
	m.m_lc_vfs_gcov.grant = grantid;
	m.m_lc_vfs_gcov.buff_sz = size;
	r = _taskcall(rfp->fp_endpoint, COMMON_REQ_GCOV_DATA, &m);
  }

  cpf_revoke(grantid);

  return(r);
}
