/*	$NetBSD: kern_sig.c,v 1.320 2015/10/02 16:54:15 christos Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
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
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_sig.c	8.14 (Berkeley) 5/14/95
 */

/*
 * Signal subsystem.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_sig.c,v 1.320 2015/10/02 16:54:15 christos Exp $");

#include "opt_ptrace.h"
#include "opt_dtrace.h"
#include "opt_compat_sunos.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#include "opt_pax.h"

#define	SIGPROP		/* include signal properties table */
#include <sys/param.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/syslog.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/pool.h>
#include <sys/ucontext.h>
#include <sys/exec.h>
#include <sys/kauth.h>
#include <sys/acct.h>
#include <sys/callout.h>
#include <sys/atomic.h>
#include <sys/cpu.h>
#include <sys/module.h>
#include <sys/sdt.h>

#ifdef PAX_SEGVGUARD
#include <sys/pax.h>
#endif /* PAX_SEGVGUARD */

#include <uvm/uvm_extern.h>

static pool_cache_t	sigacts_cache	__read_mostly;
static pool_cache_t	ksiginfo_cache	__read_mostly;
static callout_t	proc_stop_ch	__cacheline_aligned;

sigset_t		contsigmask	__cacheline_aligned;
static sigset_t		stopsigmask	__cacheline_aligned;
sigset_t		sigcantmask	__cacheline_aligned;

static void	ksiginfo_exechook(struct proc *, void *);
static void	proc_stop_callout(void *);
static int	sigchecktrace(void);
static int	sigpost(struct lwp *, sig_t, int, int);
static void	sigput(sigpend_t *, struct proc *, ksiginfo_t *);
static int	sigunwait(struct proc *, const ksiginfo_t *);
static void	sigswitch(bool, int, int);

static void	sigacts_poolpage_free(struct pool *, void *);
static void	*sigacts_poolpage_alloc(struct pool *, int);

void (*sendsig_sigcontext_vec)(const struct ksiginfo *, const sigset_t *);
int (*coredump_vec)(struct lwp *, const char *) =
    (int (*)(struct lwp *, const char *))enosys;

/*
 * DTrace SDT provider definitions
 */
SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE3(proc, kernel, , signal__send,
    "struct lwp *", 	/* target thread */
    "struct proc *", 	/* target process */
    "int");		/* signal */
SDT_PROBE_DEFINE3(proc, kernel, , signal__discard,
    "struct lwp *",	/* target thread */
    "struct proc *",	/* target process */
    "int");  		/* signal */
SDT_PROBE_DEFINE3(proc, kernel, , signal__handle,
    "int", 		/* signal */
    "ksiginfo_t *", 	/* signal info */
    "void (*)(void)");	/* handler address */


static struct pool_allocator sigactspool_allocator = {
	.pa_alloc = sigacts_poolpage_alloc,
	.pa_free = sigacts_poolpage_free
};

#ifdef DEBUG
int	kern_logsigexit = 1;
#else
int	kern_logsigexit = 0;
#endif

static const char logcoredump[] =
    "pid %d (%s), uid %d: exited on signal %d (core dumped)\n";
static const char lognocoredump[] =
    "pid %d (%s), uid %d: exited on signal %d (core not dumped, err = %d)\n";

static kauth_listener_t signal_listener;

static int
signal_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	struct proc *p;
	int result, signum;

	result = KAUTH_RESULT_DEFER;
	p = arg0;
	signum = (int)(unsigned long)arg1;

	if (action != KAUTH_PROCESS_SIGNAL)
		return result;

	if (kauth_cred_uidmatch(cred, p->p_cred) ||
	    (signum == SIGCONT && (curproc->p_session == p->p_session)))
		result = KAUTH_RESULT_ALLOW;

	return result;
}

/*
 * signal_init:
 *
 *	Initialize global signal-related data structures.
 */
void
signal_init(void)
{

	sigactspool_allocator.pa_pagesz = (PAGE_SIZE)*2;

	sigacts_cache = pool_cache_init(sizeof(struct sigacts), 0, 0, 0,
	    "sigacts", sizeof(struct sigacts) > PAGE_SIZE ?
	    &sigactspool_allocator : NULL, IPL_NONE, NULL, NULL, NULL);
	ksiginfo_cache = pool_cache_init(sizeof(ksiginfo_t), 0, 0, 0,
	    "ksiginfo", NULL, IPL_VM, NULL, NULL, NULL);

	exechook_establish(ksiginfo_exechook, NULL);

	callout_init(&proc_stop_ch, CALLOUT_MPSAFE);
	callout_setfunc(&proc_stop_ch, proc_stop_callout, NULL);

	signal_listener = kauth_listen_scope(KAUTH_SCOPE_PROCESS,
	    signal_listener_cb, NULL);
}

/*
 * sigacts_poolpage_alloc:
 *
 *	Allocate a page for the sigacts memory pool.
 */
static void *
sigacts_poolpage_alloc(struct pool *pp, int flags)
{

	return (void *)uvm_km_alloc(kernel_map,
	    PAGE_SIZE * 2, PAGE_SIZE * 2,
	    ((flags & PR_WAITOK) ? 0 : UVM_KMF_NOWAIT | UVM_KMF_TRYLOCK)
	    | UVM_KMF_WIRED);
}

/*
 * sigacts_poolpage_free:
 *
 *	Free a page on behalf of the sigacts memory pool.
 */
static void
sigacts_poolpage_free(struct pool *pp, void *v)
{

	uvm_km_free(kernel_map, (vaddr_t)v, PAGE_SIZE * 2, UVM_KMF_WIRED);
}

/*
 * sigactsinit:
 *
 *	Create an initial sigacts structure, using the same signal state
 *	as of specified process.  If 'share' is set, share the sigacts by
 *	holding a reference, otherwise just copy it from parent.
 */
struct sigacts *
sigactsinit(struct proc *pp, int share)
{
	struct sigacts *ps = pp->p_sigacts, *ps2;

	if (__predict_false(share)) {
		atomic_inc_uint(&ps->sa_refcnt);
		return ps;
	}
	ps2 = pool_cache_get(sigacts_cache, PR_WAITOK);
	mutex_init(&ps2->sa_mutex, MUTEX_DEFAULT, IPL_SCHED);
	ps2->sa_refcnt = 1;

	mutex_enter(&ps->sa_mutex);
	memcpy(ps2->sa_sigdesc, ps->sa_sigdesc, sizeof(ps2->sa_sigdesc));
	mutex_exit(&ps->sa_mutex);
	return ps2;
}

/*
 * sigactsunshare:
 *
 *	Make this process not share its sigacts, maintaining all signal state.
 */
void
sigactsunshare(struct proc *p)
{
	struct sigacts *ps, *oldps = p->p_sigacts;

	if (__predict_true(oldps->sa_refcnt == 1))
		return;

	ps = pool_cache_get(sigacts_cache, PR_WAITOK);
	mutex_init(&ps->sa_mutex, MUTEX_DEFAULT, IPL_SCHED);
	memcpy(ps->sa_sigdesc, oldps->sa_sigdesc, sizeof(ps->sa_sigdesc));
	ps->sa_refcnt = 1;

	p->p_sigacts = ps;
	sigactsfree(oldps);
}

/*
 * sigactsfree;
 *
 *	Release a sigacts structure.
 */
void
sigactsfree(struct sigacts *ps)
{

	if (atomic_dec_uint_nv(&ps->sa_refcnt) == 0) {
		mutex_destroy(&ps->sa_mutex);
		pool_cache_put(sigacts_cache, ps);
	}
}

/*
 * siginit:
 *
 *	Initialize signal state for process 0; set to ignore signals that
 *	are ignored by default and disable the signal stack.  Locking not
 *	required as the system is still cold.
 */
void
siginit(struct proc *p)
{
	struct lwp *l;
	struct sigacts *ps;
	int signo, prop;

	ps = p->p_sigacts;
	sigemptyset(&contsigmask);
	sigemptyset(&stopsigmask);
	sigemptyset(&sigcantmask);
	for (signo = 1; signo < NSIG; signo++) {
		prop = sigprop[signo];
		if (prop & SA_CONT)
			sigaddset(&contsigmask, signo);
		if (prop & SA_STOP)
			sigaddset(&stopsigmask, signo);
		if (prop & SA_CANTMASK)
			sigaddset(&sigcantmask, signo);
		if (prop & SA_IGNORE && signo != SIGCONT)
			sigaddset(&p->p_sigctx.ps_sigignore, signo);
		sigemptyset(&SIGACTION_PS(ps, signo).sa_mask);
		SIGACTION_PS(ps, signo).sa_flags = SA_RESTART;
	}
	sigemptyset(&p->p_sigctx.ps_sigcatch);
	p->p_sflag &= ~PS_NOCLDSTOP;

	ksiginfo_queue_init(&p->p_sigpend.sp_info);
	sigemptyset(&p->p_sigpend.sp_set);

	/*
	 * Reset per LWP state.
	 */
	l = LIST_FIRST(&p->p_lwps);
	l->l_sigwaited = NULL;
	l->l_sigstk.ss_flags = SS_DISABLE;
	l->l_sigstk.ss_size = 0;
	l->l_sigstk.ss_sp = 0;
	ksiginfo_queue_init(&l->l_sigpend.sp_info);
	sigemptyset(&l->l_sigpend.sp_set);

	/* One reference. */
	ps->sa_refcnt = 1;
}

/*
 * execsigs:
 *
 *	Reset signals for an exec of the specified process.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps;
	struct lwp *l;
	int signo, prop;
	sigset_t tset;
	ksiginfoq_t kq;

	KASSERT(p->p_nlwps == 1);

	sigactsunshare(p);
	ps = p->p_sigacts;

	/*
	 * Reset caught signals.  Held signals remain held through
	 * l->l_sigmask (unless they were caught, and are now ignored
	 * by default).
	 *
	 * No need to lock yet, the process has only one LWP and
	 * at this point the sigacts are private to the process.
	 */
	sigemptyset(&tset);
	for (signo = 1; signo < NSIG; signo++) {
		if (sigismember(&p->p_sigctx.ps_sigcatch, signo)) {
			prop = sigprop[signo];
			if (prop & SA_IGNORE) {
				if ((prop & SA_CONT) == 0)
					sigaddset(&p->p_sigctx.ps_sigignore,
					    signo);
				sigaddset(&tset, signo);
			}
			SIGACTION_PS(ps, signo).sa_handler = SIG_DFL;
		}
		sigemptyset(&SIGACTION_PS(ps, signo).sa_mask);
		SIGACTION_PS(ps, signo).sa_flags = SA_RESTART;
	}
	ksiginfo_queue_init(&kq);

	mutex_enter(p->p_lock);
	sigclearall(p, &tset, &kq);
	sigemptyset(&p->p_sigctx.ps_sigcatch);

	/*
	 * Reset no zombies if child dies flag as Solaris does.
	 */
	p->p_flag &= ~(PK_NOCLDWAIT | PK_CLDSIGIGN);
	if (SIGACTION_PS(ps, SIGCHLD).sa_handler == SIG_IGN)
		SIGACTION_PS(ps, SIGCHLD).sa_handler = SIG_DFL;

	/*
	 * Reset per-LWP state.
	 */
	l = LIST_FIRST(&p->p_lwps);
	l->l_sigwaited = NULL;
	l->l_sigstk.ss_flags = SS_DISABLE;
	l->l_sigstk.ss_size = 0;
	l->l_sigstk.ss_sp = 0;
	ksiginfo_queue_init(&l->l_sigpend.sp_info);
	sigemptyset(&l->l_sigpend.sp_set);
	mutex_exit(p->p_lock);

	ksiginfo_queue_drain(&kq);
}

/*
 * ksiginfo_exechook:
 *
 *	Free all pending ksiginfo entries from a process on exec.
 *	Additionally, drain any unused ksiginfo structures in the
 *	system back to the pool.
 *
 *	XXX This should not be a hook, every process has signals.
 */
static void
ksiginfo_exechook(struct proc *p, void *v)
{
	ksiginfoq_t kq;

	ksiginfo_queue_init(&kq);

	mutex_enter(p->p_lock);
	sigclearall(p, NULL, &kq);
	mutex_exit(p->p_lock);

	ksiginfo_queue_drain(&kq);
}

/*
 * ksiginfo_alloc:
 *
 *	Allocate a new ksiginfo structure from the pool, and optionally copy
 *	an existing one.  If the existing ksiginfo_t is from the pool, and
 *	has not been queued somewhere, then just return it.  Additionally,
 *	if the existing ksiginfo_t does not contain any information beyond
 *	the signal number, then just return it.
 */
ksiginfo_t *
ksiginfo_alloc(struct proc *p, ksiginfo_t *ok, int flags)
{
	ksiginfo_t *kp;

	if (ok != NULL) {
		if ((ok->ksi_flags & (KSI_QUEUED | KSI_FROMPOOL)) ==
		    KSI_FROMPOOL)
			return ok;
		if (KSI_EMPTY_P(ok))
			return ok;
	}

	kp = pool_cache_get(ksiginfo_cache, flags);
	if (kp == NULL) {
#ifdef DIAGNOSTIC
		printf("Out of memory allocating ksiginfo for pid %d\n",
		    p->p_pid);
#endif
		return NULL;
	}

	if (ok != NULL) {
		memcpy(kp, ok, sizeof(*kp));
		kp->ksi_flags &= ~KSI_QUEUED;
	} else
		KSI_INIT_EMPTY(kp);

	kp->ksi_flags |= KSI_FROMPOOL;

	return kp;
}

/*
 * ksiginfo_free:
 *
 *	If the given ksiginfo_t is from the pool and has not been queued,
 *	then free it.
 */
void
ksiginfo_free(ksiginfo_t *kp)
{

	if ((kp->ksi_flags & (KSI_QUEUED | KSI_FROMPOOL)) != KSI_FROMPOOL)
		return;
	pool_cache_put(ksiginfo_cache, kp);
}

/*
 * ksiginfo_queue_drain:
 *
 *	Drain a non-empty ksiginfo_t queue.
 */
void
ksiginfo_queue_drain0(ksiginfoq_t *kq)
{
	ksiginfo_t *ksi;

	KASSERT(!TAILQ_EMPTY(kq));

	while (!TAILQ_EMPTY(kq)) {
		ksi = TAILQ_FIRST(kq);
		TAILQ_REMOVE(kq, ksi, ksi_list);
		pool_cache_put(ksiginfo_cache, ksi);
	}
}

static bool
siggetinfo(sigpend_t *sp, ksiginfo_t *out, int signo)
{
	ksiginfo_t *ksi;

	if (sp == NULL)
		goto out;

	/* Find siginfo and copy it out. */
	TAILQ_FOREACH(ksi, &sp->sp_info, ksi_list) {
		if (ksi->ksi_signo != signo)
			continue;
		TAILQ_REMOVE(&sp->sp_info, ksi, ksi_list);
		KASSERT((ksi->ksi_flags & KSI_FROMPOOL) != 0);
		KASSERT((ksi->ksi_flags & KSI_QUEUED) != 0);
		ksi->ksi_flags &= ~KSI_QUEUED;
		if (out != NULL) {
			memcpy(out, ksi, sizeof(*out));
			out->ksi_flags &= ~(KSI_FROMPOOL | KSI_QUEUED);
		}
		ksiginfo_free(ksi);	/* XXXSMP */
		return true;
	}

out:
	/* If there is no siginfo, then manufacture it. */
	if (out != NULL) {
		KSI_INIT(out);
		out->ksi_info._signo = signo;
		out->ksi_info._code = SI_NOINFO;
	}
	return false;
}

/*
 * sigget:
 *
 *	Fetch the first pending signal from a set.  Optionally, also fetch
 *	or manufacture a ksiginfo element.  Returns the number of the first
 *	pending signal, or zero.
 */ 
int
sigget(sigpend_t *sp, ksiginfo_t *out, int signo, const sigset_t *mask)
{
	sigset_t tset;

	/* If there's no pending set, the signal is from the debugger. */
	if (sp == NULL)
		goto out;

	/* Construct mask from signo, and 'mask'. */
	if (signo == 0) {
		if (mask != NULL) {
			tset = *mask;
			__sigandset(&sp->sp_set, &tset);
		} else
			tset = sp->sp_set;

		/* If there are no signals pending - return. */
		if ((signo = firstsig(&tset)) == 0)
			goto out;
	} else {
		KASSERT(sigismember(&sp->sp_set, signo));
	}

	sigdelset(&sp->sp_set, signo);
out:
	(void)siggetinfo(sp, out, signo);
	return signo;
}

/*
 * sigput:
 *
 *	Append a new ksiginfo element to the list of pending ksiginfo's.
 */
static void
sigput(sigpend_t *sp, struct proc *p, ksiginfo_t *ksi)
{
	ksiginfo_t *kp;

	KASSERT(mutex_owned(p->p_lock));
	KASSERT((ksi->ksi_flags & KSI_QUEUED) == 0);

	sigaddset(&sp->sp_set, ksi->ksi_signo);

	/*
	 * If there is no siginfo, we are done.
	 */
	if (KSI_EMPTY_P(ksi))
		return;

	KASSERT((ksi->ksi_flags & KSI_FROMPOOL) != 0);

#ifdef notyet	/* XXX: QUEUING */
	if (ksi->ksi_signo < SIGRTMIN)
#endif
	{
		TAILQ_FOREACH(kp, &sp->sp_info, ksi_list) {
			if (kp->ksi_signo == ksi->ksi_signo) {
				KSI_COPY(ksi, kp);
				kp->ksi_flags |= KSI_QUEUED;
				return;
			}
		}
	}

	ksi->ksi_flags |= KSI_QUEUED;
	TAILQ_INSERT_TAIL(&sp->sp_info, ksi, ksi_list);
}

/*
 * sigclear:
 *
 *	Clear all pending signals in the specified set.
 */
void
sigclear(sigpend_t *sp, const sigset_t *mask, ksiginfoq_t *kq)
{
	ksiginfo_t *ksi, *next;

	if (mask == NULL)
		sigemptyset(&sp->sp_set);
	else
		sigminusset(mask, &sp->sp_set);

	TAILQ_FOREACH_SAFE(ksi, &sp->sp_info, ksi_list, next) {
		if (mask == NULL || sigismember(mask, ksi->ksi_signo)) {
			TAILQ_REMOVE(&sp->sp_info, ksi, ksi_list);
			KASSERT((ksi->ksi_flags & KSI_FROMPOOL) != 0);
			KASSERT((ksi->ksi_flags & KSI_QUEUED) != 0);
			TAILQ_INSERT_TAIL(kq, ksi, ksi_list);
		}
	}
}

/*
 * sigclearall:
 *
 *	Clear all pending signals in the specified set from a process and
 *	its LWPs.
 */
void
sigclearall(struct proc *p, const sigset_t *mask, ksiginfoq_t *kq)
{
	struct lwp *l;

	KASSERT(mutex_owned(p->p_lock));

	sigclear(&p->p_sigpend, mask, kq);

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		sigclear(&l->l_sigpend, mask, kq);
	}
}

/*
 * sigispending:
 *
 *	Return the first signal number if there are pending signals for the
 *	current LWP.  May be called unlocked provided that LW_PENDSIG is set,
 *	and that the signal has been posted to the appopriate queue before
 *	LW_PENDSIG is set.
 */ 
int
sigispending(struct lwp *l, int signo)
{
	struct proc *p = l->l_proc;
	sigset_t tset;

	membar_consumer();

	tset = l->l_sigpend.sp_set;
	sigplusset(&p->p_sigpend.sp_set, &tset);
	sigminusset(&p->p_sigctx.ps_sigignore, &tset);
	sigminusset(&l->l_sigmask, &tset);

	if (signo == 0) {
		return firstsig(&tset);
	}
	return sigismember(&tset, signo) ? signo : 0;
}

void
getucontext(struct lwp *l, ucontext_t *ucp)
{
	struct proc *p = l->l_proc;

	KASSERT(mutex_owned(p->p_lock));

	ucp->uc_flags = 0;
	ucp->uc_link = l->l_ctxlink;
	ucp->uc_sigmask = l->l_sigmask;
	ucp->uc_flags |= _UC_SIGMASK;

	/*
	 * The (unsupplied) definition of the `current execution stack'
	 * in the System V Interface Definition appears to allow returning
	 * the main context stack.
	 */
	if ((l->l_sigstk.ss_flags & SS_ONSTACK) == 0) {
		ucp->uc_stack.ss_sp = (void *)l->l_proc->p_stackbase;
		ucp->uc_stack.ss_size = ctob(l->l_proc->p_vmspace->vm_ssize);
		ucp->uc_stack.ss_flags = 0;	/* XXX, def. is Very Fishy */
	} else {
		/* Simply copy alternate signal execution stack. */
		ucp->uc_stack = l->l_sigstk;
	}
	ucp->uc_flags |= _UC_STACK;
	mutex_exit(p->p_lock);
	cpu_getmcontext(l, &ucp->uc_mcontext, &ucp->uc_flags);
	mutex_enter(p->p_lock);
}

int
setucontext(struct lwp *l, const ucontext_t *ucp)
{
	struct proc *p = l->l_proc;
	int error;

	KASSERT(mutex_owned(p->p_lock));

	if ((ucp->uc_flags & _UC_SIGMASK) != 0) {
		error = sigprocmask1(l, SIG_SETMASK, &ucp->uc_sigmask, NULL);
		if (error != 0)
			return error;
	}

	mutex_exit(p->p_lock);
	error = cpu_setmcontext(l, &ucp->uc_mcontext, ucp->uc_flags);
	mutex_enter(p->p_lock);
	if (error != 0)
		return (error);

	l->l_ctxlink = ucp->uc_link;

	/*
	 * If there was stack information, update whether or not we are
	 * still running on an alternate signal stack.
	 */
	if ((ucp->uc_flags & _UC_STACK) != 0) {
		if (ucp->uc_stack.ss_flags & SS_ONSTACK)
			l->l_sigstk.ss_flags |= SS_ONSTACK;
		else
			l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	}

	return 0;
}

/*
 * killpg1: common code for kill process group/broadcast kill.
 */
int
killpg1(struct lwp *l, ksiginfo_t *ksi, int pgid, int all)
{
	struct proc	*p, *cp;
	kauth_cred_t	pc;
	struct pgrp	*pgrp;
	int		nfound;
	int		signo = ksi->ksi_signo;

	cp = l->l_proc;
	pc = l->l_cred;
	nfound = 0;

	mutex_enter(proc_lock);
	if (all) {
		/*
		 * Broadcast.
		 */
		PROCLIST_FOREACH(p, &allproc) {
			if (p->p_pid <= 1 || p == cp ||
			    (p->p_flag & PK_SYSTEM) != 0)
				continue;
			mutex_enter(p->p_lock);
			if (kauth_authorize_process(pc,
			    KAUTH_PROCESS_SIGNAL, p, KAUTH_ARG(signo), NULL,
			    NULL) == 0) {
				nfound++;
				if (signo)
					kpsignal2(p, ksi);
			}
			mutex_exit(p->p_lock);
		}
	} else {
		if (pgid == 0)
			/* Zero pgid means send to my process group. */
			pgrp = cp->p_pgrp;
		else {
			pgrp = pgrp_find(pgid);
			if (pgrp == NULL)
				goto out;
		}
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			if (p->p_pid <= 1 || p->p_flag & PK_SYSTEM)
				continue;
			mutex_enter(p->p_lock);
			if (kauth_authorize_process(pc, KAUTH_PROCESS_SIGNAL,
			    p, KAUTH_ARG(signo), NULL, NULL) == 0) {
				nfound++;
				if (signo && P_ZOMBIE(p) == 0)
					kpsignal2(p, ksi);
			}
			mutex_exit(p->p_lock);
		}
	}
out:
	mutex_exit(proc_lock);
	return nfound ? 0 : ESRCH;
}

/*
 * Send a signal to a process group.  If checktty is set, limit to members
 * which have a controlling terminal.
 */
void
pgsignal(struct pgrp *pgrp, int sig, int checkctty)
{
	ksiginfo_t ksi;

	KASSERT(!cpu_intr_p());
	KASSERT(mutex_owned(proc_lock));

	KSI_INIT_EMPTY(&ksi);
	ksi.ksi_signo = sig;
	kpgsignal(pgrp, &ksi, NULL, checkctty);
}

void
kpgsignal(struct pgrp *pgrp, ksiginfo_t *ksi, void *data, int checkctty)
{
	struct proc *p;

	KASSERT(!cpu_intr_p());
	KASSERT(mutex_owned(proc_lock));
	KASSERT(pgrp != NULL);

	LIST_FOREACH(p, &pgrp->pg_members, p_pglist)
		if (checkctty == 0 || p->p_lflag & PL_CONTROLT)
			kpsignal(p, ksi, data);
}

/*
 * Send a signal caused by a trap to the current LWP.  If it will be caught
 * immediately, deliver it with correct code.  Otherwise, post it normally.
 */
void
trapsignal(struct lwp *l, ksiginfo_t *ksi)
{
	struct proc	*p;
	struct sigacts	*ps;
	int signo = ksi->ksi_signo;
	sigset_t *mask;

	KASSERT(KSI_TRAP_P(ksi));

	ksi->ksi_lid = l->l_lid;
	p = l->l_proc;

	KASSERT(!cpu_intr_p());
	mutex_enter(proc_lock);
	mutex_enter(p->p_lock);
	mask = &l->l_sigmask;
	ps = p->p_sigacts;

	if ((p->p_slflag & PSL_TRACED) == 0 &&
	    sigismember(&p->p_sigctx.ps_sigcatch, signo) &&
	    !sigismember(mask, signo)) {
		mutex_exit(proc_lock);
		l->l_ru.ru_nsignals++;
		kpsendsig(l, ksi, mask);
		mutex_exit(p->p_lock);
		ktrpsig(signo, SIGACTION_PS(ps, signo).sa_handler, mask, ksi);
	} else {
		/* XXX for core dump/debugger */
		p->p_sigctx.ps_lwp = l->l_lid;
		p->p_sigctx.ps_signo = ksi->ksi_signo;
		p->p_sigctx.ps_code = ksi->ksi_trap;
		kpsignal2(p, ksi);
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
	}
}

/*
 * Fill in signal information and signal the parent for a child status change.
 */
void
child_psignal(struct proc *p, int mask)
{
	ksiginfo_t ksi;
	struct proc *q;
	int xstat;

	KASSERT(mutex_owned(proc_lock));
	KASSERT(mutex_owned(p->p_lock));

	xstat = p->p_xstat;

	KSI_INIT(&ksi);
	ksi.ksi_signo = SIGCHLD;
	ksi.ksi_code = (xstat == SIGCONT ? CLD_CONTINUED : CLD_STOPPED);
	ksi.ksi_pid = p->p_pid;
	ksi.ksi_uid = kauth_cred_geteuid(p->p_cred);
	ksi.ksi_status = xstat;
	ksi.ksi_utime = p->p_stats->p_ru.ru_utime.tv_sec;
	ksi.ksi_stime = p->p_stats->p_ru.ru_stime.tv_sec;

	q = p->p_pptr;

	mutex_exit(p->p_lock);
	mutex_enter(q->p_lock);

	if ((q->p_sflag & mask) == 0)
		kpsignal2(q, &ksi);

	mutex_exit(q->p_lock);
	mutex_enter(p->p_lock);
}

void
psignal(struct proc *p, int signo)
{
	ksiginfo_t ksi;

	KASSERT(!cpu_intr_p());
	KASSERT(mutex_owned(proc_lock));

	KSI_INIT_EMPTY(&ksi);
	ksi.ksi_signo = signo;
	mutex_enter(p->p_lock);
	kpsignal2(p, &ksi);
	mutex_exit(p->p_lock);
}

void
kpsignal(struct proc *p, ksiginfo_t *ksi, void *data)
{
	fdfile_t *ff;
	file_t *fp;
	fdtab_t *dt;

	KASSERT(!cpu_intr_p());
	KASSERT(mutex_owned(proc_lock));

	if ((p->p_sflag & PS_WEXIT) == 0 && data) {
		size_t fd;
		filedesc_t *fdp = p->p_fd;

		/* XXXSMP locking */
		ksi->ksi_fd = -1;
		dt = fdp->fd_dt;
		for (fd = 0; fd < dt->dt_nfiles; fd++) {
			if ((ff = dt->dt_ff[fd]) == NULL)
				continue;
			if ((fp = ff->ff_file) == NULL)
				continue;
			if (fp->f_data == data) {
				ksi->ksi_fd = fd;
				break;
			}
		}
	}
	mutex_enter(p->p_lock);
	kpsignal2(p, ksi);
	mutex_exit(p->p_lock);
}

/*
 * sigismasked:
 *
 *	Returns true if signal is ignored or masked for the specified LWP.
 */
int
sigismasked(struct lwp *l, int sig)
{
	struct proc *p = l->l_proc;

	return sigismember(&p->p_sigctx.ps_sigignore, sig) ||
	    sigismember(&l->l_sigmask, sig);
}

/*
 * sigpost:
 *
 *	Post a pending signal to an LWP.  Returns non-zero if the LWP may
 *	be able to take the signal.
 */
static int
sigpost(struct lwp *l, sig_t action, int prop, int sig)
{
	int rv, masked;
	struct proc *p = l->l_proc;

	KASSERT(mutex_owned(p->p_lock));

	/*
	 * If the LWP is on the way out, sigclear() will be busy draining all
	 * pending signals.  Don't give it more.
	 */
	if (l->l_refcnt == 0)
		return 0;

	SDT_PROBE(proc, kernel, , signal__send, l, p, sig, 0, 0);

	/*
	 * Have the LWP check for signals.  This ensures that even if no LWP
	 * is found to take the signal immediately, it should be taken soon.
	 */
	lwp_lock(l);
	l->l_flag |= LW_PENDSIG;

	/*
	 * SIGCONT can be masked, but if LWP is stopped, it needs restart.
	 * Note: SIGKILL and SIGSTOP cannot be masked.
	 */
	masked = sigismember(&l->l_sigmask, sig);
	if (masked && ((prop & SA_CONT) == 0 || l->l_stat != LSSTOP)) {
		lwp_unlock(l);
		return 0;
	}

	/*
	 * If killing the process, make it run fast.
	 */
	if (__predict_false((prop & SA_KILL) != 0) &&
	    action == SIG_DFL && l->l_priority < MAXPRI_USER) {
		KASSERT(l->l_class == SCHED_OTHER);
		lwp_changepri(l, MAXPRI_USER);
	}

	/*
	 * If the LWP is running or on a run queue, then we win.  If it's
	 * sleeping interruptably, wake it and make it take the signal.  If
	 * the sleep isn't interruptable, then the chances are it will get
	 * to see the signal soon anyhow.  If suspended, it can't take the
	 * signal right now.  If it's LWP private or for all LWPs, save it
	 * for later; otherwise punt.
	 */
	rv = 0;

	switch (l->l_stat) {
	case LSRUN:
	case LSONPROC:
		lwp_need_userret(l);
		rv = 1;
		break;

	case LSSLEEP:
		if ((l->l_flag & LW_SINTR) != 0) {
			/* setrunnable() will release the lock. */
			setrunnable(l);
			return 1;
		}
		break;

	case LSSUSPENDED:
		if ((prop & SA_KILL) != 0 && (l->l_flag & LW_WCORE) != 0) {
			/* lwp_continue() will release the lock. */
			lwp_continue(l);
			return 1;
		}
		break;

	case LSSTOP:
		if ((prop & SA_STOP) != 0)
			break;

		/*
		 * If the LWP is stopped and we are sending a continue
		 * signal, then start it again.
		 */
		if ((prop & SA_CONT) != 0) {
			if (l->l_wchan != NULL) {
				l->l_stat = LSSLEEP;
				p->p_nrlwps++;
				rv = 1;
				break;
			}
			/* setrunnable() will release the lock. */
			setrunnable(l);
			return 1;
		} else if (l->l_wchan == NULL || (l->l_flag & LW_SINTR) != 0) {
			/* setrunnable() will release the lock. */
			setrunnable(l);
			return 1;
		}
		break;

	default:
		break;
	}

	lwp_unlock(l);
	return rv;
}

/*
 * Notify an LWP that it has a pending signal.
 */
void
signotify(struct lwp *l)
{
	KASSERT(lwp_locked(l, NULL));

	l->l_flag |= LW_PENDSIG;
	lwp_need_userret(l);
}

/*
 * Find an LWP within process p that is waiting on signal ksi, and hand
 * it on.
 */
static int
sigunwait(struct proc *p, const ksiginfo_t *ksi)
{
	struct lwp *l;
	int signo;

	KASSERT(mutex_owned(p->p_lock));

	signo = ksi->ksi_signo;

	if (ksi->ksi_lid != 0) {
		/*
		 * Signal came via _lwp_kill().  Find the LWP and see if
		 * it's interested.
		 */
		if ((l = lwp_find(p, ksi->ksi_lid)) == NULL)
			return 0;
		if (l->l_sigwaited == NULL ||
		    !sigismember(&l->l_sigwaitset, signo))
			return 0;
	} else {
		/*
		 * Look for any LWP that may be interested.
		 */
		LIST_FOREACH(l, &p->p_sigwaiters, l_sigwaiter) {
			KASSERT(l->l_sigwaited != NULL);
			if (sigismember(&l->l_sigwaitset, signo))
				break;
		}
	}

	if (l != NULL) {
		l->l_sigwaited->ksi_info = ksi->ksi_info;
		l->l_sigwaited = NULL;
		LIST_REMOVE(l, l_sigwaiter);
		cv_signal(&l->l_sigcv);
		return 1;
	}

	return 0;
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 */
void
kpsignal2(struct proc *p, ksiginfo_t *ksi)
{
	int prop, signo = ksi->ksi_signo;
	struct sigacts *sa;
	struct lwp *l = NULL;
	ksiginfo_t *kp;
	lwpid_t lid;
	sig_t action;
	bool toall;

	KASSERT(!cpu_intr_p());
	KASSERT(mutex_owned(proc_lock));
	KASSERT(mutex_owned(p->p_lock));
	KASSERT((ksi->ksi_flags & KSI_QUEUED) == 0);
	KASSERT(signo > 0 && signo < NSIG);

	/*
	 * If the process is being created by fork, is a zombie or is
	 * exiting, then just drop the signal here and bail out.
	 */
	if (p->p_stat != SACTIVE && p->p_stat != SSTOP)
		return;

	/*
	 * Notify any interested parties of the signal.
	 */
	KNOTE(&p->p_klist, NOTE_SIGNAL | signo);

	/*
	 * Some signals including SIGKILL must act on the entire process.
	 */
	kp = NULL;
	prop = sigprop[signo];
	toall = ((prop & SA_TOALL) != 0);
	lid = toall ? 0 : ksi->ksi_lid;

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (p->p_slflag & PSL_TRACED) {
		action = SIG_DFL;

		if (lid == 0) {
			/*
			 * If the process is being traced and the signal
			 * is being caught, make sure to save any ksiginfo.
			 */
			if ((kp = ksiginfo_alloc(p, ksi, PR_NOWAIT)) == NULL)
				goto discard;
			sigput(&p->p_sigpend, p, kp);
		}
	} else {
		/*
		 * If the signal was the result of a trap and is not being
		 * caught, then reset it to default action so that the
		 * process dumps core immediately.
		 */
		if (KSI_TRAP_P(ksi)) {
			sa = p->p_sigacts;
			mutex_enter(&sa->sa_mutex);
			if (!sigismember(&p->p_sigctx.ps_sigcatch, signo)) {
				sigdelset(&p->p_sigctx.ps_sigignore, signo);
				SIGACTION(p, signo).sa_handler = SIG_DFL;
			}
			mutex_exit(&sa->sa_mutex);
		}

		/*
		 * If the signal is being ignored, then drop it.  Note: we
		 * don't set SIGCONT in ps_sigignore, and if it is set to
		 * SIG_IGN, action will be SIG_DFL here.
		 */
		if (sigismember(&p->p_sigctx.ps_sigignore, signo))
			goto discard;

		else if (sigismember(&p->p_sigctx.ps_sigcatch, signo))
			action = SIG_CATCH;
		else {
			action = SIG_DFL;

			/*
			 * If sending a tty stop signal to a member of an
			 * orphaned process group, discard the signal here if
			 * the action is default; don't stop the process below
			 * if sleeping, and don't clear any pending SIGCONT.
			 */
			if (prop & SA_TTYSTOP && p->p_pgrp->pg_jobc == 0)
				goto discard;

			if (prop & SA_KILL && p->p_nice > NZERO)
				p->p_nice = NZERO;
		}
	}

	/*
	 * If stopping or continuing a process, discard any pending
	 * signals that would do the inverse.
	 */
	if ((prop & (SA_CONT | SA_STOP)) != 0) {
		ksiginfoq_t kq;

		ksiginfo_queue_init(&kq);
		if ((prop & SA_CONT) != 0)
			sigclear(&p->p_sigpend, &stopsigmask, &kq);
		if ((prop & SA_STOP) != 0)
			sigclear(&p->p_sigpend, &contsigmask, &kq);
		ksiginfo_queue_drain(&kq);	/* XXXSMP */
	}

	/*
	 * If the signal doesn't have SA_CANTMASK (no override for SIGKILL,
	 * please!), check if any LWPs are waiting on it.  If yes, pass on
	 * the signal info.  The signal won't be processed further here.
	 */
	if ((prop & SA_CANTMASK) == 0 && !LIST_EMPTY(&p->p_sigwaiters) &&
	    p->p_stat == SACTIVE && (p->p_sflag & PS_STOPPING) == 0 &&
	    sigunwait(p, ksi))
		goto discard;

	/*
	 * XXXSMP Should be allocated by the caller, we're holding locks
	 * here.
	 */
	if (kp == NULL && (kp = ksiginfo_alloc(p, ksi, PR_NOWAIT)) == NULL)
		goto discard;

	/*
	 * LWP private signals are easy - just find the LWP and post
	 * the signal to it.
	 */
	if (lid != 0) {
		l = lwp_find(p, lid);
		if (l != NULL) {
			sigput(&l->l_sigpend, p, kp);
			membar_producer();
			(void)sigpost(l, action, prop, kp->ksi_signo);
		}
		goto out;
	}

	/*
	 * Some signals go to all LWPs, even if posted with _lwp_kill()
	 * or for an SA process.
	 */
	if (p->p_stat == SACTIVE && (p->p_sflag & PS_STOPPING) == 0) {
		if ((p->p_slflag & PSL_TRACED) != 0)
			goto deliver;

		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) != 0 && action == SIG_DFL)
			goto out;
	} else {
		/*
		 * Process is stopped or stopping.
		 * - If traced, then no action is needed, unless killing.
		 * - Run the process only if sending SIGCONT or SIGKILL.
		 */
		if ((p->p_slflag & PSL_TRACED) != 0 && signo != SIGKILL) {
			goto out;
		}
		if ((prop & SA_CONT) != 0 || signo == SIGKILL) {
			/*
			 * Re-adjust p_nstopchild if the process wasn't
			 * collected by its parent.
			 */
			p->p_stat = SACTIVE;
			p->p_sflag &= ~PS_STOPPING;
			if (!p->p_waited) {
				p->p_pptr->p_nstopchild--;
			}
			if (p->p_slflag & PSL_TRACED) {
				KASSERT(signo == SIGKILL);
				goto deliver;
			}
			/*
			 * Do not make signal pending if SIGCONT is default.
			 *
			 * If the process catches SIGCONT, let it handle the
			 * signal itself (if waiting on event - process runs,
			 * otherwise continues sleeping).
			 */
			if ((prop & SA_CONT) != 0 && action == SIG_DFL) {
				KASSERT(signo != SIGKILL);
				goto deliver;
			}
		} else if ((prop & SA_STOP) != 0) {
			/*
			 * Already stopped, don't need to stop again.
			 * (If we did the shell could get confused.)
			 */
			goto out;
		}
	}
	/*
	 * Make signal pending.
	 */
	KASSERT((p->p_slflag & PSL_TRACED) == 0);
	sigput(&p->p_sigpend, p, kp);

deliver:
	/*
	 * Before we set LW_PENDSIG on any LWP, ensure that the signal is
	 * visible on the per process list (for sigispending()).  This
	 * is unlikely to be needed in practice, but...
	 */
	membar_producer();

	/*
	 * Try to find an LWP that can take the signal.
	 */
	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		if (sigpost(l, action, prop, kp->ksi_signo) && !toall)
			break;
	}
	signo = -1;
out:
	/*
	 * If the ksiginfo wasn't used, then bin it.  XXXSMP freeing memory
	 * with locks held.  The caller should take care of this.
	 */
	ksiginfo_free(kp);
	if (signo == -1)
		return;
discard:
	SDT_PROBE(proc, kernel, , signal__discard, l, p, signo, 0, 0);
}

void
kpsendsig(struct lwp *l, const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct proc *p = l->l_proc;

	KASSERT(mutex_owned(p->p_lock));
	(*p->p_emul->e_sendsig)(ksi, mask);
}

/*
 * Stop any LWPs sleeping interruptably.
 */
static void
proc_stop_lwps(struct proc *p)
{
	struct lwp *l;

	KASSERT(mutex_owned(p->p_lock));
	KASSERT((p->p_sflag & PS_STOPPING) != 0);

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		lwp_lock(l);
		if (l->l_stat == LSSLEEP && (l->l_flag & LW_SINTR) != 0) {
			l->l_stat = LSSTOP;
			p->p_nrlwps--;
		}
		lwp_unlock(l);
	}
}

/*
 * Finish stopping of a process.  Mark it stopped and notify the parent.
 *
 * Drop p_lock briefly if PS_NOTIFYSTOP is set and ppsig is true.
 */
static void
proc_stop_done(struct proc *p, bool ppsig, int ppmask)
{

	KASSERT(mutex_owned(proc_lock));
	KASSERT(mutex_owned(p->p_lock));
	KASSERT((p->p_sflag & PS_STOPPING) != 0);
	KASSERT(p->p_nrlwps == 0 || (p->p_nrlwps == 1 && p == curproc));

	p->p_sflag &= ~PS_STOPPING;
	p->p_stat = SSTOP;
	p->p_waited = 0;
	p->p_pptr->p_nstopchild++;
	if ((p->p_sflag & PS_NOTIFYSTOP) != 0) {
		if (ppsig) {
			/* child_psignal drops p_lock briefly. */
			child_psignal(p, ppmask);
		}
		cv_broadcast(&p->p_pptr->p_waitcv);
	}
}

/*
 * Stop the current process and switch away when being stopped or traced.
 */
static void
sigswitch(bool ppsig, int ppmask, int signo)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	int biglocks;

	KASSERT(mutex_owned(p->p_lock));
	KASSERT(l->l_stat == LSONPROC);
	KASSERT(p->p_nrlwps > 0);

	/*
	 * On entry we know that the process needs to stop.  If it's
	 * the result of a 'sideways' stop signal that has been sourced
	 * through issignal(), then stop other LWPs in the process too.
	 */
	if (p->p_stat == SACTIVE && (p->p_sflag & PS_STOPPING) == 0) {
		KASSERT(signo != 0);
		proc_stop(p, 1, signo);
		KASSERT(p->p_nrlwps > 0);
	}

	/*
	 * If we are the last live LWP, and the stop was a result of
	 * a new signal, then signal the parent.
	 */
	if ((p->p_sflag & PS_STOPPING) != 0) {
		if (!mutex_tryenter(proc_lock)) {
			mutex_exit(p->p_lock);
			mutex_enter(proc_lock);
			mutex_enter(p->p_lock);
		}

		if (p->p_nrlwps == 1 && (p->p_sflag & PS_STOPPING) != 0) {
			/*
			 * Note that proc_stop_done() can drop
			 * p->p_lock briefly.
			 */
			proc_stop_done(p, ppsig, ppmask);
		}

		mutex_exit(proc_lock);
	}

	/*
	 * Unlock and switch away.
	 */
	KERNEL_UNLOCK_ALL(l, &biglocks);
	if (p->p_stat == SSTOP || (p->p_sflag & PS_STOPPING) != 0) {
		p->p_nrlwps--;
		lwp_lock(l);
		KASSERT(l->l_stat == LSONPROC || l->l_stat == LSSLEEP);
		l->l_stat = LSSTOP;
		lwp_unlock(l);
	}

	mutex_exit(p->p_lock);
	lwp_lock(l);
	mi_switch(l);
	KERNEL_LOCK(biglocks, l);
	mutex_enter(p->p_lock);
}

/*
 * Check for a signal from the debugger.
 */
static int
sigchecktrace(void)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	int signo;

	KASSERT(mutex_owned(p->p_lock));

	/* If there's a pending SIGKILL, process it immediately. */
	if (sigismember(&p->p_sigpend.sp_set, SIGKILL))
		return 0;

	/*
	 * If we are no longer being traced, or the parent didn't
	 * give us a signal, or we're stopping, look for more signals.
	 */
	if ((p->p_slflag & PSL_TRACED) == 0 || p->p_xstat == 0 ||
	    (p->p_sflag & PS_STOPPING) != 0)
		return 0;

	/*
	 * If the new signal is being masked, look for other signals.
	 * `p->p_sigctx.ps_siglist |= mask' is done in setrunnable().
	 */
	signo = p->p_xstat;
	p->p_xstat = 0;
	if (sigismember(&l->l_sigmask, signo)) {
		signo = 0;
	}
	return signo;
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 *
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap.
 *
 * We will also return -1 if the process is exiting and the current LWP must
 * follow suit.
 */
int
issignal(struct lwp *l)
{
	struct proc *p;
	int signo, prop;
	sigpend_t *sp;
	sigset_t ss;

	p = l->l_proc;
	sp = NULL;
	signo = 0;

	KASSERT(p == curproc);
	KASSERT(mutex_owned(p->p_lock));

	for (;;) {
		/* Discard any signals that we have decided not to take. */
		if (signo != 0) {
			(void)sigget(sp, NULL, signo, NULL);
		}

		/*
		 * If the process is stopped/stopping, then stop ourselves
		 * now that we're on the kernel/userspace boundary.  When
		 * we awaken, check for a signal from the debugger.
		 */
		if (p->p_stat == SSTOP || (p->p_sflag & PS_STOPPING) != 0) {
			sigswitch(true, PS_NOCLDSTOP, 0);
			signo = sigchecktrace();
		} else
			signo = 0;

		/* Signals from the debugger are "out of band". */
		sp = NULL;

		/*
		 * If the debugger didn't provide a signal, find a pending
		 * signal from our set.  Check per-LWP signals first, and
		 * then per-process.
		 */
		if (signo == 0) {
			sp = &l->l_sigpend;
			ss = sp->sp_set;
			if ((p->p_lflag & PL_PPWAIT) != 0)
				sigminusset(&stopsigmask, &ss);
			sigminusset(&l->l_sigmask, &ss);

			if ((signo = firstsig(&ss)) == 0) {
				sp = &p->p_sigpend;
				ss = sp->sp_set;
				if ((p->p_lflag & PL_PPWAIT) != 0)
					sigminusset(&stopsigmask, &ss);
				sigminusset(&l->l_sigmask, &ss);

				if ((signo = firstsig(&ss)) == 0) {
					/*
					 * No signal pending - clear the
					 * indicator and bail out.
					 */
					lwp_lock(l);
					l->l_flag &= ~LW_PENDSIG;
					lwp_unlock(l);
					sp = NULL;
					break;
				}
			}
		}

		/*
		 * We should see pending but ignored signals only if
		 * we are being traced.
		 */
		if (sigismember(&p->p_sigctx.ps_sigignore, signo) &&
		    (p->p_slflag & PSL_TRACED) == 0) {
			/* Discard the signal. */
			continue;
		}

		/*
		 * If traced, always stop, and stay stopped until released
		 * by the debugger.  If the our parent process is waiting
		 * for us, don't hang as we could deadlock.
		 */
		if ((p->p_slflag & PSL_TRACED) != 0 &&
		    (p->p_lflag & PL_PPWAIT) == 0 && signo != SIGKILL) {
			/*
			 * Take the signal, but don't remove it from the
			 * siginfo queue, because the debugger can send
			 * it later.
			 */
			if (sp)
				sigdelset(&sp->sp_set, signo);
			p->p_xstat = signo;

			/* Emulation-specific handling of signal trace */
			if (p->p_emul->e_tracesig == NULL ||
			    (*p->p_emul->e_tracesig)(p, signo) == 0)
				sigswitch(!(p->p_slflag & PSL_FSTRACE), 0,
				    signo);

			/* Check for a signal from the debugger. */
			if ((signo = sigchecktrace()) == 0)
				continue;

			/* Signals from the debugger are "out of band". */
			sp = NULL;
		}

		prop = sigprop[signo];

		/*
		 * Decide whether the signal should be returned.
		 */
		switch ((long)SIGACTION(p, signo).sa_handler) {
		case (long)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf_nolog("Process (pid %d) got sig %d\n",
				    p->p_pid, signo);
#endif
				continue;
			}

			/*
			 * If there is a pending stop signal to process with
			 * default action, stop here, then clear the signal. 
			 * However, if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				/*
				 * XXX Don't hold proc_lock for p_lflag,
				 * but it's not a big deal.
				 */
				if (p->p_slflag & PSL_TRACED ||
				    ((p->p_lflag & PL_ORPHANPG) != 0 &&
				    prop & SA_TTYSTOP)) {
					/* Ignore the signal. */
					continue;
				}
				/* Take the signal. */
				(void)sigget(sp, NULL, signo, NULL);
				p->p_xstat = signo;
				signo = 0;
				sigswitch(true, PS_NOCLDSTOP, p->p_xstat);
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				continue;
			}
			break;

		case (long)SIG_IGN:
#ifdef DEBUG_ISSIGNAL
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (p->p_slflag & PSL_TRACED) == 0)
				printf_nolog("issignal\n");
#endif
			continue;

		default:
			/*
			 * This signal has an action, let postsig() process
			 * it.
			 */
			break;
		}

		break;
	}

	l->l_sigpendset = sp;
	return signo;
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
postsig(int signo)
{
	struct lwp	*l;
	struct proc	*p;
	struct sigacts	*ps;
	sig_t		action;
	sigset_t	*returnmask;
	ksiginfo_t	ksi;

	l = curlwp;
	p = l->l_proc;
	ps = p->p_sigacts;

	KASSERT(mutex_owned(p->p_lock));
	KASSERT(signo > 0);

	/*
	 * Set the new mask value and also defer further occurrences of this
	 * signal.
	 *
	 * Special case: user has done a sigsuspend.  Here the current mask is
	 * not of interest, but rather the mask from before the sigsuspend is
	 * what we want restored after the signal processing is completed.
	 */
	if (l->l_sigrestore) {
		returnmask = &l->l_sigoldmask;
		l->l_sigrestore = 0;
	} else
		returnmask = &l->l_sigmask;

	/*
	 * Commit to taking the signal before releasing the mutex.
	 */
	action = SIGACTION_PS(ps, signo).sa_handler;
	l->l_ru.ru_nsignals++;
	if (l->l_sigpendset == NULL) {
		/* From the debugger */
		if (!siggetinfo(&l->l_sigpend, &ksi, signo))
			(void)siggetinfo(&p->p_sigpend, &ksi, signo);
	} else
		sigget(l->l_sigpendset, &ksi, signo, NULL);

	if (ktrpoint(KTR_PSIG)) {
		mutex_exit(p->p_lock);
		ktrpsig(signo, action, returnmask, &ksi);
		mutex_enter(p->p_lock);
	}

	SDT_PROBE(proc, kernel, , signal__handle, signo, &ksi, action, 0, 0);

	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(l, signo);
		return;
	}

	/*
	 * If we get here, the signal must be caught.
	 */
#ifdef DIAGNOSTIC
	if (action == SIG_IGN || sigismember(&l->l_sigmask, signo))
		panic("postsig action");
#endif

	kpsendsig(l, &ksi, returnmask);
}

/*
 * sendsig:
 *
 *	Default signal delivery method for NetBSD.
 */
void
sendsig(const struct ksiginfo *ksi, const sigset_t *mask)
{
	struct sigacts *sa;
	int sig;

	sig = ksi->ksi_signo;
	sa = curproc->p_sigacts;

	switch (sa->sa_sigdesc[sig].sd_vers)  {
	case 0:
	case 1:
		/* Compat for 1.6 and earlier. */
		if (sendsig_sigcontext_vec == NULL) {
			break;
		}
		(*sendsig_sigcontext_vec)(ksi, mask);
		return;
	case 2:
	case 3:
		sendsig_siginfo(ksi, mask);
		return;
	default:
		break;
	}

	printf("sendsig: bad version %d\n", sa->sa_sigdesc[sig].sd_vers);
	sigexit(curlwp, SIGILL);
}

/*
 * sendsig_reset:
 *
 *	Reset the signal action.  Called from emulation specific sendsig()
 *	before unlocking to deliver the signal.
 */
void
sendsig_reset(struct lwp *l, int signo)
{
	struct proc *p = l->l_proc;
	struct sigacts *ps = p->p_sigacts;

	KASSERT(mutex_owned(p->p_lock));

	p->p_sigctx.ps_lwp = 0;
	p->p_sigctx.ps_code = 0;
	p->p_sigctx.ps_signo = 0;

	mutex_enter(&ps->sa_mutex);
	sigplusset(&SIGACTION_PS(ps, signo).sa_mask, &l->l_sigmask);
	if (SIGACTION_PS(ps, signo).sa_flags & SA_RESETHAND) {
		sigdelset(&p->p_sigctx.ps_sigcatch, signo);
		if (signo != SIGCONT && sigprop[signo] & SA_IGNORE)
			sigaddset(&p->p_sigctx.ps_sigignore, signo);
		SIGACTION_PS(ps, signo).sa_handler = SIG_DFL;
	}
	mutex_exit(&ps->sa_mutex);
}

/*
 * Kill the current process for stated reason.
 */
void
killproc(struct proc *p, const char *why)
{

	KASSERT(mutex_owned(proc_lock));

	log(LOG_ERR, "pid %d was killed: %s\n", p->p_pid, why);
	uprintf_locked("sorry, pid %d was killed: %s\n", p->p_pid, why);
	psignal(p, SIGKILL);
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught
 * signals, allowing unrecoverable failures to terminate the process without
 * changing signal state.  Mark the accounting record with the signal
 * termination.  If dumping core, save the signal number for the debugger. 
 * Calls exit and does not return.
 */
void
sigexit(struct lwp *l, int signo)
{
	int exitsig, error, docore;
	struct proc *p;
	struct lwp *t;

	p = l->l_proc;

	KASSERT(mutex_owned(p->p_lock));
	KERNEL_UNLOCK_ALL(l, NULL);

	/*
	 * Don't permit coredump() multiple times in the same process.
	 * Call back into sigexit, where we will be suspended until
	 * the deed is done.  Note that this is a recursive call, but
	 * LW_WCORE will prevent us from coming back this way.
	 */
	if ((p->p_sflag & PS_WCORE) != 0) {
		lwp_lock(l);
		l->l_flag |= (LW_WCORE | LW_WEXIT | LW_WSUSPEND);
		lwp_unlock(l);
		mutex_exit(p->p_lock);
		lwp_userret(l);
		panic("sigexit 1");
		/* NOTREACHED */
	}

	/* If process is already on the way out, then bail now. */
	if ((p->p_sflag & PS_WEXIT) != 0) {
		mutex_exit(p->p_lock);
		lwp_exit(l);
		panic("sigexit 2");
		/* NOTREACHED */
	}

	/*
	 * Prepare all other LWPs for exit.  If dumping core, suspend them
	 * so that their registers are available long enough to be dumped.
 	 */
	if ((docore = (sigprop[signo] & SA_CORE)) != 0) {
		p->p_sflag |= PS_WCORE;
		for (;;) {
			LIST_FOREACH(t, &p->p_lwps, l_sibling) {
				lwp_lock(t);
				if (t == l) {
					t->l_flag &= ~LW_WSUSPEND;
					lwp_unlock(t);
					continue;
				}
				t->l_flag |= (LW_WCORE | LW_WEXIT);
				lwp_suspend(l, t);
			}

			if (p->p_nrlwps == 1)
				break;

			/*
			 * Kick any LWPs sitting in lwp_wait1(), and wait
			 * for everyone else to stop before proceeding.
			 */
			p->p_nlwpwait++;
			cv_broadcast(&p->p_lwpcv);
			cv_wait(&p->p_lwpcv, p->p_lock);
			p->p_nlwpwait--;
		}
	}

	exitsig = signo;
	p->p_acflag |= AXSIG;
	p->p_sigctx.ps_signo = signo;

	if (docore) {
		mutex_exit(p->p_lock);
		if ((error = (*coredump_vec)(l, NULL)) == 0)
			exitsig |= WCOREFLAG;

		if (kern_logsigexit) {
			int uid = l->l_cred ?
			    (int)kauth_cred_geteuid(l->l_cred) : -1;

			if (error)
				log(LOG_INFO, lognocoredump, p->p_pid,
				    p->p_comm, uid, signo, error);
			else
				log(LOG_INFO, logcoredump, p->p_pid,
				    p->p_comm, uid, signo);
		}

#ifdef PAX_SEGVGUARD
		pax_segvguard(l, p->p_textvp, p->p_comm, true);
#endif /* PAX_SEGVGUARD */
		/* Acquire the sched state mutex.  exit1() will release it. */
		mutex_enter(p->p_lock);
	}

	/* No longer dumping core. */
	p->p_sflag &= ~PS_WCORE;

	exit1(l, W_EXITCODE(0, exitsig));
	/* NOTREACHED */
}

/*
 * Put process 'p' into the stopped state and optionally, notify the parent.
 */
void
proc_stop(struct proc *p, int notify, int signo)
{
	struct lwp *l;

	KASSERT(mutex_owned(p->p_lock));

	/*
	 * First off, set the stopping indicator and bring all sleeping
	 * LWPs to a halt so they are included in p->p_nrlwps.  We musn't
	 * unlock between here and the p->p_nrlwps check below.
	 */
	p->p_sflag |= PS_STOPPING;
	if (notify)
		p->p_sflag |= PS_NOTIFYSTOP;
	else
		p->p_sflag &= ~PS_NOTIFYSTOP;
	membar_producer();

	proc_stop_lwps(p);

	/*
	 * If there are no LWPs available to take the signal, then we
	 * signal the parent process immediately.  Otherwise, the last
	 * LWP to stop will take care of it.
	 */

	if (p->p_nrlwps == 0) {
		proc_stop_done(p, true, PS_NOCLDSTOP);
	} else {
		/*
		 * Have the remaining LWPs come to a halt, and trigger
		 * proc_stop_callout() to ensure that they do.
		 */
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			sigpost(l, SIG_DFL, SA_STOP, signo);
		}
		callout_schedule(&proc_stop_ch, 1);
	}
}

/*
 * When stopping a process, we do not immediatly set sleeping LWPs stopped,
 * but wait for them to come to a halt at the kernel-user boundary.  This is
 * to allow LWPs to release any locks that they may hold before stopping.
 *
 * Non-interruptable sleeps can be long, and there is the potential for an
 * LWP to begin sleeping interruptably soon after the process has been set
 * stopping (PS_STOPPING).  These LWPs will not notice that the process is
 * stopping, and so complete halt of the process and the return of status
 * information to the parent could be delayed indefinitely.
 *
 * To handle this race, proc_stop_callout() runs once per tick while there
 * are stopping processes in the system.  It sets LWPs that are sleeping
 * interruptably into the LSSTOP state.
 *
 * Note that we are not concerned about keeping all LWPs stopped while the
 * process is stopped: stopped LWPs can awaken briefly to handle signals. 
 * What we do need to ensure is that all LWPs in a stopping process have
 * stopped at least once, so that notification can be sent to the parent
 * process.
 */
static void
proc_stop_callout(void *cookie)
{
	bool more, restart;
	struct proc *p;

	(void)cookie;

	do {
		restart = false;
		more = false;

		mutex_enter(proc_lock);
		PROCLIST_FOREACH(p, &allproc) {
			mutex_enter(p->p_lock);

			if ((p->p_sflag & PS_STOPPING) == 0) {
				mutex_exit(p->p_lock);
				continue;
			}

			/* Stop any LWPs sleeping interruptably. */
			proc_stop_lwps(p);
			if (p->p_nrlwps == 0) {
				/*
				 * We brought the process to a halt.
				 * Mark it as stopped and notify the
				 * parent.
				 */
				if ((p->p_sflag & PS_NOTIFYSTOP) != 0) {
					/*
					 * Note that proc_stop_done() will
					 * drop p->p_lock briefly.
					 * Arrange to restart and check
					 * all processes again.
					 */
					restart = true;
				}
				proc_stop_done(p, true, PS_NOCLDSTOP);
			} else
				more = true;

			mutex_exit(p->p_lock);
			if (restart)
				break;
		}
		mutex_exit(proc_lock);
	} while (restart);

	/*
	 * If we noted processes that are stopping but still have
	 * running LWPs, then arrange to check again in 1 tick.
	 */
	if (more)
		callout_schedule(&proc_stop_ch, 1);
}

/*
 * Given a process in state SSTOP, set the state back to SACTIVE and
 * move LSSTOP'd LWPs to LSSLEEP or make them runnable.
 */
void
proc_unstop(struct proc *p)
{
	struct lwp *l;
	int sig;

	KASSERT(mutex_owned(proc_lock));
	KASSERT(mutex_owned(p->p_lock));

	p->p_stat = SACTIVE;
	p->p_sflag &= ~PS_STOPPING;
	sig = p->p_xstat;

	if (!p->p_waited)
		p->p_pptr->p_nstopchild--;

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		lwp_lock(l);
		if (l->l_stat != LSSTOP) {
			lwp_unlock(l);
			continue;
		}
		if (l->l_wchan == NULL) {
			setrunnable(l);
			continue;
		}
		if (sig && (l->l_flag & LW_SINTR) != 0) {
			setrunnable(l);
			sig = 0;
		} else {
			l->l_stat = LSSLEEP;
			p->p_nrlwps++;
			lwp_unlock(l);
		}
	}
}

static int
filt_sigattach(struct knote *kn)
{
	struct proc *p = curproc;

	kn->kn_obj = p;
	kn->kn_flags |= EV_CLEAR;	/* automatically set */

	mutex_enter(p->p_lock);
	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);
	mutex_exit(p->p_lock);

	return 0;
}

static void
filt_sigdetach(struct knote *kn)
{
	struct proc *p = kn->kn_obj;

	mutex_enter(p->p_lock);
	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
	mutex_exit(p->p_lock);
}

/*
 * Signal knotes are shared with proc knotes, so we apply a mask to
 * the hint in order to differentiate them from process hints.  This
 * could be avoided by using a signal-specific knote list, but probably
 * isn't worth the trouble.
 */
static int
filt_signal(struct knote *kn, long hint)
{

	if (hint & NOTE_SIGNAL) {
		hint &= ~NOTE_SIGNAL;

		if (kn->kn_id == hint)
			kn->kn_data++;
	}
	return (kn->kn_data != 0);
}

const struct filterops sig_filtops = {
	0, filt_sigattach, filt_sigdetach, filt_signal
};
