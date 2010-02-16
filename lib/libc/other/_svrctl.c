/*	svrctl() - special server control functions.	Author: Kees J. Bot
 *								24 Apr 1994
 */
#include <lib.h>
#include <stdio.h>
#define svrctl _svrctl
#include <sys/svrctl.h>

int svrctl(int request, void *argp)
{
	message m;

	m.m2_i1 = request;
	m.m2_p1 = argp;

	switch ((request >> 8) & 0xFF) {
	case 'M':
	case 'S':
		/* MM handles calls for itself and the kernel. */
		return _syscall(MM, SVRCTL, &m);
	case 'F':
	case 'I':
		/* FS handles calls for itself and inet. */
		return _syscall(FS, SVRCTL, &m);
	default:
		errno = EINVAL;
		return -1;
	}
}
