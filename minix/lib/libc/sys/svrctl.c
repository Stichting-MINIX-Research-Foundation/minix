/*	svrctl() - special server control functions.	Author: Kees J. Bot
 *								24 Apr 1994
 */
#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <sys/svrctl.h>

int svrctl(unsigned long request, void *argp)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_svrctl.request = request;
	m.m_lc_svrctl.arg = (vir_bytes)argp;

	switch (IOCGROUP(request)) {
	case 'M': /* old, phasing out */
	case 'P': /* to PM */
		return _syscall(PM_PROC_NR, PM_SVRCTL, &m);
	case 'F': /* to VFS */
		return _syscall(VFS_PROC_NR, VFS_SVRCTL, &m);
	default:
		errno = EINVAL;
		return -1;
	}
}
