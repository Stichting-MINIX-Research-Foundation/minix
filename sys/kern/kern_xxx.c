/*	$NetBSD: kern_xxx.c,v 1.72 2011/09/01 18:25:44 matt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_xxx.c	8.3 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_xxx.c,v 1.72 2011/09/01 18:25:44 matt Exp $");

#ifdef _KERNEL_OPT
#include "opt_syscall_debug.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>

/* ARGSUSED */
int
sys_reboot(struct lwp *l, const struct sys_reboot_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) opt;
		syscallarg(char *) bootstr;
	} */
	int error;
	char *bootstr, bs[128];

	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_REBOOT,
	    0, NULL, NULL, NULL)) != 0)
		return (error);

	/*
	 * Only use the boot string if RB_STRING is set.
	 */
	if ((SCARG(uap, opt) & RB_STRING) &&
	    (error = copyinstr(SCARG(uap, bootstr), bs, sizeof(bs), 0)) == 0)
		bootstr = bs;
	else
		bootstr = NULL;
	/*
	 * Not all ports use the bootstr currently.
	 */
	KERNEL_LOCK(1, NULL);
	cpu_reboot(SCARG(uap, opt), bootstr);
	KERNEL_UNLOCK_ONE(NULL);
	return (0);
}

/*
 * Pull in the indirect syscall functions here.
 * They are only actually used if the ports syscall entry code
 * doesn't special-case SYS_SYSCALL and SYS___SYSCALL
 *
 * In some cases the generated code for the two functions is identical,
 * but there isn't a MI way of determining that - so we don't try.
 */

#define SYS_SYSCALL sys_syscall
#include "sys_syscall.c"
#undef SYS_SYSCALL

#define SYS_SYSCALL sys___syscall
#include "sys_syscall.c"
#undef SYS_SYSCALL

#ifdef SYSCALL_DEBUG
#define	SCDEBUG_CALLS		0x0001	/* show calls */
#define	SCDEBUG_RETURNS		0x0002	/* show returns */
#define	SCDEBUG_ALL		0x0004	/* even syscalls that are implemented */
#define	SCDEBUG_SHOWARGS	0x0008	/* show arguments to calls */

#if 0
int	scdebug = SCDEBUG_CALLS|SCDEBUG_RETURNS|SCDEBUG_SHOWARGS;
#else
int	scdebug = SCDEBUG_CALLS|SCDEBUG_RETURNS|SCDEBUG_SHOWARGS|SCDEBUG_ALL;
#endif

void
scdebug_call(register_t code, const register_t args[])
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	const struct sysent *sy;
	const struct emul *em;
	int i;

	if (!(scdebug & SCDEBUG_CALLS))
		return;

	em = p->p_emul;
	sy = &em->e_sysent[code];
	if (!(scdebug & SCDEBUG_ALL || (int)code < 0
#ifndef __HAVE_MINIMAL_EMUL
	    || code >= em->e_nsysent
#endif
	    || sy->sy_call == sys_nosys))
		return;

	printf("proc %d (%s): %s num ", p->p_pid, p->p_comm, em->e_name);
	if ((int)code < 0
#ifndef __HAVE_MINIMAL_EMUL
	    || code >= em->e_nsysent
#endif
	    )
		printf("OUT OF RANGE (%ld)", (long)code);
	else {
		printf("%ld call: %s", (long)code, em->e_syscallnames[code]);
		if (scdebug & SCDEBUG_SHOWARGS) {
			printf("(");
			for (i = 0; i < sy->sy_argsize/sizeof(register_t); i++)
				printf("%s0x%lx", i == 0 ? "" : ", ",
				    (long)args[i]);
			printf(")");
		}
	}
	printf("\n");
}

void
scdebug_ret(register_t code, int error, const register_t retval[])
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	const struct sysent *sy;
	const struct emul *em;

	if (!(scdebug & SCDEBUG_RETURNS))
		return;

	em = p->p_emul;
	sy = &em->e_sysent[code];
	if (!(scdebug & SCDEBUG_ALL || (int)code < 0
#ifndef __HAVE_MINIMAL_EMUL
	    || (int)code >= em->e_nsysent
#endif
	    || sy->sy_call == sys_nosys))
		return;

	printf("proc %d (%s): %s num ", p->p_pid, p->p_comm, em->e_name);
	if ((int)code < 0
#ifndef __HAVE_MINIMAL_EMUL
	    || code >= em->e_nsysent
#endif
	    )
		printf("OUT OF RANGE (%ld)", (long)code);
	else
		printf("%ld ret %s: err = %d, rv = 0x%lx,0x%lx", (long)code,
		    em->e_syscallnames[code], error,
		    (long)retval[0], (long)retval[1]);
	printf("\n");
}
#endif /* SYSCALL_DEBUG */
