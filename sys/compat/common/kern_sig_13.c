/*	$NetBSD: kern_sig_13.c,v 1.20 2011/01/19 10:21:16 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: kern_sig_13.c,v 1.20 2011/01/19 10:21:16 tsutsui Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <sys/syscallargs.h>

#include <machine/limits.h>

#include <compat/sys/signal.h>
#include <compat/sys/signalvar.h>
#include <compat/common/compat_util.h>
#include <compat/common/compat_sigaltstack.h>

void
native_sigset13_to_sigset(const sigset13_t *oss, sigset_t *ss)
{

	ss->__bits[0] = *oss;
	ss->__bits[1] = 0;
	ss->__bits[2] = 0;
	ss->__bits[3] = 0;
}

void
native_sigset_to_sigset13(const sigset_t *ss, sigset13_t *oss)
{

	*oss = ss->__bits[0];
}

void
native_sigaction13_to_sigaction(const struct sigaction13 *osa, struct sigaction *sa)
{

	sa->sa_handler = osa->osa_handler;
	native_sigset13_to_sigset(&osa->osa_mask, &sa->sa_mask);
	sa->sa_flags = osa->osa_flags;
}

void
native_sigaction_to_sigaction13(const struct sigaction *sa, struct sigaction13 *osa)
{

	osa->osa_handler = sa->sa_handler;
	native_sigset_to_sigset13(&sa->sa_mask, &osa->osa_mask);
	osa->osa_flags = sa->sa_flags;
}

int
compat_13_sys_sigaltstack(struct lwp *l, const struct compat_13_sys_sigaltstack_args *uap, register_t *retval)
{
	/* {
		syscallarg(const struct sigaltstack13 *) nss;
		syscallarg(struct sigaltstack13 *) oss;
	} */
	compat_sigaltstack(uap, sigaltstack13, SS_ONSTACK, SS_DISABLE);
}

int
compat_13_sys_sigaction(struct lwp *l, const struct compat_13_sys_sigaction_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) signum;
		syscallarg(const struct sigaction13 *) nsa;
		syscallarg(struct sigaction13 *) osa;
	} */
	struct sigaction13 nesa, oesa;
	struct sigaction nbsa, obsa;
	int error;

	if (SCARG(uap, nsa)) {
		error = copyin(SCARG(uap, nsa), &nesa, sizeof(nesa));
		if (error)
			return (error);
		native_sigaction13_to_sigaction(&nesa, &nbsa);
	}
	error = sigaction1(l, SCARG(uap, signum),
	    SCARG(uap, nsa) ? &nbsa : 0, SCARG(uap, osa) ? &obsa : 0,
	    NULL, 0);
	if (error)
		return (error);
	if (SCARG(uap, osa)) {
		native_sigaction_to_sigaction13(&obsa, &oesa);
		error = copyout(&oesa, SCARG(uap, osa), sizeof(oesa));
		if (error)
			return (error);
	}
	return (0);
}

int
compat_13_sys_sigprocmask(struct lwp *l, const struct compat_13_sys_sigprocmask_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) how;
		syscallarg(int) mask;
	} */
	struct proc *p = l->l_proc;
	sigset13_t ness, oess;
	sigset_t nbss, obss;
	int error;

	ness = SCARG(uap, mask);
	native_sigset13_to_sigset(&ness, &nbss);
	mutex_enter(p->p_lock);
	error = sigprocmask1(l, SCARG(uap, how), &nbss, &obss);
	mutex_exit(p->p_lock);
	if (error)
		return (error);
	native_sigset_to_sigset13(&obss, &oess);
	*retval = oess;
	return (0);
}

int
compat_13_sys_sigpending(struct lwp *l, const void *v, register_t *retval)
{
	sigset13_t ess;
	sigset_t bss;

	sigpending1(l, &bss);
	native_sigset_to_sigset13(&bss, &ess);
	*retval = ess;
	return (0);
}

int
compat_13_sys_sigsuspend(struct lwp *l, const struct compat_13_sys_sigsuspend_args *uap, register_t *retval)
{
	/* {
		syscallarg(sigset13_t) mask;
	} */
	sigset13_t ess;
	sigset_t bss;

	ess = SCARG(uap, mask);
	native_sigset13_to_sigset(&ess, &bss);
	return (sigsuspend1(l, &bss));
}
