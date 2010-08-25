
#include "fs.h"
#include "file.h"
#include "fproc.h"


/*===========================================================================*
 *				do_gcov_flush				*
 *===========================================================================*/
PUBLIC int do_gcov_flush()
{
/* A userland tool has requested the gcov data from another
 * process (possibly vfs itself). Grant the target process
 * access to the supplied buffer, and perform the call that
 * makes the target copy its buffer to the caller (incl vfs
 * itself).
 */
	int i;
	struct fproc *rfp;
	ssize_t size;
	cp_grant_id_t grantid;
	int r;
	int n;
	pid_t target;

	size = m_in.GCOV_BUFF_SZ;
	target = m_in.GCOV_PID;

	/* If the wrong process is sent to, the system hangs;
	 * so make this root-only.
	 */

	if (!super_user) return(EPERM);

	/* Find target gcov process. */

	for(n = 0; n < NR_PROCS; n++) {
		 if(fproc[n].fp_endpoint != NONE &&
			 fproc[n].fp_pid == target)
				 break;
	}

	if(n >= NR_PROCS) {
		 printf("VFS: gcov proccess %d not found.\n", target);
		 return ESRCH;
	}

	rfp = &fproc[n];

	/* Grant target process to requestor's buffer. */

	if((grantid = cpf_grant_magic(rfp->fp_endpoint,
		 who_e, (vir_bytes) m_in.GCOV_BUFF_P,
		 size, CPF_WRITE)) < 0) {
		 printf("VFS: gcov_flush: grant failed\n");
		 return ENOMEM;
	}

	if(target == getpid()) {
		 /* Request is for VFS itself. */
		r = gcov_flush(grantid, size);
	} else {
		/* Perform generic GCOV request. */
		m_out.GCOV_GRANT = grantid;
		m_out.GCOV_BUFF_SZ = size;
		r = _taskcall(rfp->fp_endpoint, COMMON_REQ_GCOV_DATA, &m_out);
	}

	cpf_revoke(grantid);

	return r;
}

