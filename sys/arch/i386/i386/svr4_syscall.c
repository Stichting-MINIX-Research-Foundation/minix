/*	$NetBSD: svr4_syscall.c,v 1.48 2015/03/07 18:50:01 christos Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: svr4_syscall.c,v 1.48 2015/03/07 18:50:01 christos Exp $");

#if defined(_KERNEL_OPT)
#include "opt_vm86.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/userret.h>

#include <compat/svr4/svr4_errno.h>
#include <compat/svr4/svr4_syscall.h>
#include <machine/svr4_machdep.h>

void svr4_syscall(struct trapframe *);
extern struct sysent svr4_sysent[];

void
svr4_syscall_intern(struct proc *p)
{

	p->p_md.md_syscall = svr4_syscall;
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
void
svr4_syscall(struct trapframe *frame)
{
	char *params;
	const struct sysent *callp;
	struct lwp *l;
	struct proc *p;
	int error;
	size_t argsize;
	register_t code, args[8], rval[2];

	l = curlwp;
	p = l->l_proc;
	LWP_CACHE_CREDS(l, p);

	code = frame->tf_eax;
	callp = svr4_sysent;
	params = (char *)frame->tf_esp + sizeof(int);

	switch (code) {
	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		break;
	default:
		break;
	}

	code &= (SVR4_SYS_NSYSENT - 1);
	callp += code;
	argsize = callp->sy_argsize;
	if (argsize) {
		error = copyin(params, (void *)args, argsize);
		if (error)
			goto bad;
	}

	if (!__predict_false(p->p_trace_enabled || KDTRACE_ENTRY(callp->sy_entry))
	    || (error = trace_enter(code, callp, args)) == 0) {
		rval[0] = 0;
		rval[1] = 0;
		error = sy_call(callp, l, args, rval);
	}

	switch (error) {
	case 0:
		frame->tf_eax = rval[0];
		frame->tf_edx = rval[1];
		frame->tf_eflags &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/*
		 * The offset to adjust the PC by depends on whether we entered
		 * the kernel through the trap or call gate.  We pushed the
		 * size of the instruction into tf_err on entry.
		 */
		frame->tf_eip -= frame->tf_err;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		error = native_to_svr4_errno[error];
		frame->tf_eax = error;
		frame->tf_eflags |= PSL_C;	/* carry bit */
		break;
	}

	if (__predict_false(p->p_trace_enabled || KDTRACE_ENTRY(callp->sy_return)))
		trace_exit(code, callp, args, rval, error);

	userret(l);
}
