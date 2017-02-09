/*	$NetBSD: signals.c,v 1.15 2015/04/18 15:49:18 pooka Exp $	*/

/*-
 * Copyright (c) 2010, 2011 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: signals.c,v 1.15 2015/04/18 15:49:18 pooka Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/signal.h>

#include <rump/rump.h>
#include <rump/rumpuser.h>

#include "rump_private.h"
#include "rumpkern_if_priv.h"

const struct filterops sig_filtops = {
	.f_attach = (void *)eopnotsupp,
};

sigset_t sigcantmask;

static void
pgrp_apply(struct pgrp *pgrp, int signo, void (*apply)(struct proc *p, int))
{
	struct proc *p;

	KASSERT(mutex_owned(proc_lock));

	LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
		mutex_enter(p->p_lock);
		apply(p, signo);
		mutex_exit(p->p_lock);
	}
}

/* RUMP_SIGMODEL_PANIC */

static void
rumpsig_panic(struct proc *p, int signo)
{

	switch (signo) {
	case SIGSYS:
	case SIGPIPE:
		break;
	default:
		panic("unhandled signal %d", signo);
	}
}

/* RUMP_SIGMODEL_IGNORE */

static void
rumpsig_ignore(struct proc *p, int signo)
{

	return;
}

/* RUMP_SIGMODEL_RAISE */

static void
rumpsig_raise(struct proc *p, int signo)
{

	if (RUMP_LOCALPROC_P(p)) {
		rumpuser_kill(p->p_pid, signo);
	} else {
		rump_sysproxy_raise(RUMP_SPVM2CTL(p->p_vmspace), signo);
	}
}

static void
rumpsig_record(struct proc *p, int signo)
{

	if (!sigismember(&p->p_sigctx.ps_sigignore, signo)) {
		sigaddset(&p->p_sigpend.sp_set, signo);
	}
}

typedef void (*rumpsig_fn)(struct proc *, int);

static rumpsig_fn rumpsig = rumpsig_raise;

/*
 * Set signal delivery model.  It would be nice if we could
 * take a functional argument.  But then we'd need some
 * OS independent way to represent a signal number and also
 * a method for easy processing (parsing is boring).
 *
 * Plus, upcalls from the rump kernel into process space except
 * via rumpuser is a somewhat gray area now.
 */
void
rump_boot_setsigmodel(enum rump_sigmodel model)
{

	switch (model) {
	case RUMP_SIGMODEL_PANIC:
		rumpsig = rumpsig_panic;
		break;
	case RUMP_SIGMODEL_IGNORE:
		rumpsig = rumpsig_ignore;
		break;
	case RUMP_SIGMODEL_RAISE:
		rumpsig = rumpsig_raise;
		break;
	case RUMP_SIGMODEL_RECORD:
		rumpsig = rumpsig_record;
		break;

	/* for compat, though I doubt anyone is using it */
	case RUMP_SIGMODEL__HOST_NOTANYMORE:
		rumpsig = rumpsig_raise;
		break;
	}
}

void
psignal(struct proc *p, int sig)
{

	rumpsig(p, sig);
}

void
pgsignal(struct pgrp *pgrp, int sig, int checktty)
{

	pgrp_apply(pgrp, sig, rumpsig);
}

void
kpsignal(struct proc *p, ksiginfo_t *ksi, void *data)
{

	rumpsig(p, ksi->ksi_signo);
}

void
kpgsignal(struct pgrp *pgrp, ksiginfo_t *ksi, void *data, int checkctty)
{

	pgrp_apply(pgrp, ksi->ksi_signo, rumpsig);
}

int
sigispending(struct lwp *l, int signo)
{
	struct proc *p = l->l_proc;
	sigset_t tset;

	tset = p->p_sigpend.sp_set;

	if (signo == 0) {
		if (firstsig(&tset) != 0)
			return EINTR;
	} else if (sigismember(&tset, signo))
		return EINTR;
	return 0;
}

void
sigpending1(struct lwp *l, sigset_t *ss)
{
	struct proc *p = l->l_proc;

	mutex_enter(p->p_lock);
	*ss = l->l_proc->p_sigpend.sp_set;
	mutex_exit(p->p_lock);
}

int
sigismasked(struct lwp *l, int sig)
{

	return sigismember(&l->l_proc->p_sigctx.ps_sigignore, sig);
}

void
sigclear(sigpend_t *sp, const sigset_t *mask, ksiginfoq_t *kq)
{

	if (mask == NULL)
		sigemptyset(&sp->sp_set);
	else
		sigminusset(mask, &sp->sp_set);
}

void
sigclearall(struct proc *p, const sigset_t *mask, ksiginfoq_t *kq)
{

	/* don't assert proc lock, hence callable from user context */
	sigclear(&p->p_sigpend, mask, kq);
}

void
ksiginfo_queue_drain0(ksiginfoq_t *kq)
{

	if (!(TAILQ_EMPTY(kq)))
		panic("how did that get there?");
}

int
sigprocmask1(struct lwp *l, int how, const sigset_t *nss, sigset_t *oss)
{
	sigset_t *mask = &l->l_proc->p_sigctx.ps_sigignore;

	KASSERT(mutex_owned(l->l_proc->p_lock));

	if (oss)
		*oss = *mask;

	if (nss) {
		switch (how) {
		case SIG_BLOCK:
			sigplusset(nss, mask);
			break;
		case SIG_UNBLOCK:
			sigminusset(nss, mask);
			break;
		case SIG_SETMASK:
			*mask = *nss;
			break;
		default:
			return EINVAL;
		}
	}

	return 0;
}

void
siginit(struct proc *p)
{

	sigemptyset(&p->p_sigctx.ps_sigignore);
	sigemptyset(&p->p_sigpend.sp_set);
}

void
sigsuspendsetup(struct lwp *l, const sigset_t *ss)
{
	/* XXX: Partial copy of kernel code, remove and use the kernel code. */
	struct proc *p = l->l_proc;

	mutex_enter(p->p_lock);
	l->l_sigrestore = 1;
	l->l_sigoldmask = l->l_sigmask;
	l->l_sigmask = *ss;
	sigminusset(&sigcantmask, &l->l_sigmask);
	mutex_exit(p->p_lock);
}

void
sigsuspendteardown(struct lwp *l)
{
	/* XXX: Copy of kernel code, remove and use the kernel code. */
	struct proc *p = l->l_proc;

	mutex_enter(p->p_lock);
	if (l->l_sigrestore) {
		l->l_sigrestore = 0;
		l->l_sigmask = l->l_sigoldmask;
	}
	mutex_exit(p->p_lock);
}
