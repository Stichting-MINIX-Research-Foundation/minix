/*	$NetBSD: linux_rump.c,v 1.2 2014/01/10 19:44:47 njoly Exp $	*/

#include <sys/param.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

#include "rump_linux_syscallargs.h"

int
rump_linux_sys_mknodat(struct lwp *l,
    const struct rump_linux_sys_mknodat_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(mode_t) mode;
		syscallarg(int) PAD;
		syscallarg(dev_t) dev;
	} */
	struct linux_sys_mknodat_args ua;

	SCARG(&ua, fd) = SCARG(uap, fd);
	SCARG(&ua, path) = SCARG(uap, path);
	SCARG(&ua, mode) = SCARG(uap, mode);
	SCARG(&ua, dev) = SCARG(uap, dev);

	return linux_sys_mknodat(l, &ua, retval);
}
