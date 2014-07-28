/*	svrctl() - special server control functions.	Author: Kees J. Bot
 *								24 Apr 1994
 */
#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <sys/svrctl.h>

int svrctl(int request, void *argp)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_svrctl.request = request;
	m.m_lsys_svrctl.arg = argp;

	switch ((request >> 8) & 0xFF) {
	case 'M':
	case 'S':
		/* PM handles calls for itself and the kernel. */
		return _syscall(PM_PROC_NR, PM_SVRCTL, &m);
	case 'F':
	case 'I':
		/* VFS handles calls for itself and inet. */
		return _syscall(VFS_PROC_NR, VFS_SVRCTL, &m);
	default:
		errno = EINVAL;
		return -1;
	}
}
