#include <errno.h>
#include <lib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ucred.h>

int 
getnucred(endpoint_t proc_ep, struct uucred *ucred)
{
	message m;
	pid_t pid;

	if (ucred == NULL) {
		errno = EFAULT;
		return -1;
	}

	m.m1_i1 = proc_ep;		/* search for this process */

	pid = _syscall(PM_PROC_NR, GETEPINFO, &m);
	if (pid < 0) {
		 return -1;
	}

	/* Only two fields are used for now, so ensure the rest is zeroed out. */
	memset(ucred, 0, sizeof(struct uucred));
	ucred->cr_uid = m.PM_NUID;
	ucred->cr_gid = m.PM_NGID;

	return 0;
}
