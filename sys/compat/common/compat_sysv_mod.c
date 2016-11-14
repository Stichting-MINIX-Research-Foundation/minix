/*	$NetBSD: compat_sysv_mod.c,v 1.3 2015/05/13 02:08:20 pgoyette Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Linkage for the compat module: spaghetti.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: compat_sysv_mod.c,v 1.3 2015/05/13 02:08:20 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/systm.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>
#include <sys/ipc.h>

MODULE(MODULE_CLASS_EXEC, compat_sysv, "sysv_ipc");

static const struct syscall_package compat_sysv_syscalls[] = {
#if defined(COMPAT_10) && !defined(_LP64)
# if defined(SYSVMSG)
	{ SYS_compat_10_omsgsys, 0, (sy_call_t *)compat_10_sys_msgsys },
# endif
# if defined(SYSVSEM)
	{ SYS_compat_10_osemsys, 0, (sy_call_t *)compat_10_sys_semsys },
# endif
# if defined(SYSVSHM)
	{ SYS_compat_10_oshmsys, 0, (sy_call_t *)compat_10_sys_shmsys },
# endif
#endif

#if defined(COMPAT_14)
# if defined(SYSVSEM)
	{ SYS_compat_14___semctl, 0, (sy_call_t *)compat_14_sys___semctl },
# endif
# if defined(SYSVMSG)
	{ SYS_compat_14_msgctl, 0, (sy_call_t *)compat_14_sys_msgctl },
# endif
# if defined(SYSVSHM)
	{ SYS_compat_14_shmctl, 0, (sy_call_t *)compat_14_sys_shmctl },
# endif
#endif

#if defined(COMPAT_50)
# if defined(SYSVSEM)
	{ SYS_compat_50_____semctl13, 0, (sy_call_t *)compat_50_sys_____semctl13 },
# endif
# if defined(SYSVMSG)
	{ SYS_compat_50___msgctl13, 0, (sy_call_t *)compat_50_sys___msgctl13 },
# endif
# if defined(SYSVSHM)
	{ SYS_compat_50___shmctl13, 0, (sy_call_t *)compat_50_sys___shmctl13 },
# endif
#endif
	{ 0, 0, NULL },
};

int sysctl_kern_sysvipc50(SYSCTLFN_ARGS);

static int
compat_sysv_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
		/* Link the system calls */
		error = syscall_establish(NULL, compat_sysv_syscalls);
#ifdef COMPAT_50
		sysvipc50_set_compat_sysctl(sysctl_kern_sysvipc50);
#endif
		return error;

	case MODULE_CMD_FINI:
		/* Unlink the system calls. */
		error = syscall_disestablish(NULL, compat_sysv_syscalls);
#ifdef COMPAT_50
		sysvipc50_set_compat_sysctl(NULL);
#endif
		return error;

	default:
		return ENOTTY;
	}
}
