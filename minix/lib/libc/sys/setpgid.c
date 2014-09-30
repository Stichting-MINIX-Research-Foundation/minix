#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

#include <unistd.h>

/*
 * "Smart" stub for now. This requires job control to be properly implemented.
 */
int setpgid(pid_t pid, pid_t pgid)
{
	pid_t _pid, _pgid, cpid;

	_pid = pid;
	_pgid = pgid;

	/* Who are we? */
	cpid = getpid();

	/* if zero, means current process. */
	if (_pid == 0) {
		_pid = cpid;
	}

	/* if zero, means given pid. */
	if (_pgid == 0) {
		_pgid = _pid;
	}

	/* right now we only support the equivalent of setsid(), which is
	 * setpgid(0,0) */
	if ((_pid != cpid) || (_pgid != cpid)) {
	    errno = EINVAL;
	    return -1;
	}

	if (setsid() == cpid) {
		return 0;
	} else {
		return -1;
	}
}
