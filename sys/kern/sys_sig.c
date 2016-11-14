/*	$NetBSD: sys_sig.c,v 1.45 2015/10/02 16:54:15 christos Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_sig.c,v 1.45 2015/10/02 16:54:15 christos Exp $");

#include "opt_dtrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/wait.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/sdt.h>

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE2(proc, kernel, , signal__clear,
    "int", 		/* signal */
    "ksiginfo_t *");	/* signal-info */

int
sys___sigaction_sigtramp(struct lwp *l,
    const struct sys___sigaction_sigtramp_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				signum;
		syscallarg(const struct sigaction *)	nsa;
		syscallarg(struct sigaction *)		osa;
		syscallarg(void *)			tramp;
		syscallarg(int)				vers;
	} */
	struct sigaction nsa, osa;
	int error;

	if (SCARG(uap, nsa)) {
		error = copyin(SCARG(uap, nsa), &nsa, sizeof(nsa));
		if (error)
			return (error);
	}
	error = sigaction1(l, SCARG(uap, signum),
	    SCARG(uap, nsa) ? &nsa : 0, SCARG(uap, osa) ? &osa : 0,
	    SCARG(uap, tramp), SCARG(uap, vers));
	if (error)
		return (error);
	if (SCARG(uap, osa)) {
		error = copyout(&osa, SCARG(uap, osa), sizeof(osa));
		if (error)
			return (error);
	}
	return 0;
}

/*
 * Manipulate signal mask.  Note that we receive new mask, not pointer, and
 * return old mask as return value; the library stub does the rest.
 */
int
sys___sigprocmask14(struct lwp *l, const struct sys___sigprocmask14_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int)			how;
		syscallarg(const sigset_t *)	set;
		syscallarg(sigset_t *)		oset;
	} */
	struct proc	*p = l->l_proc;
	sigset_t	nss, oss;
	int		error;

	if (SCARG(uap, set)) {
		error = copyin(SCARG(uap, set), &nss, sizeof(nss));
		if (error)
			return error;
	}
	mutex_enter(p->p_lock);
	error = sigprocmask1(l, SCARG(uap, how),
	    SCARG(uap, set) ? &nss : 0, SCARG(uap, oset) ? &oss : 0);
	mutex_exit(p->p_lock);
	if (error)
		return error;
	if (SCARG(uap, oset)) {
		error = copyout(&oss, SCARG(uap, oset), sizeof(oss));
		if (error)
			return error;
	}
	return 0;
}

int
sys___sigpending14(struct lwp *l, const struct sys___sigpending14_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(sigset_t *)	set;
	} */
	sigset_t ss;

	sigpending1(l, &ss);
	return copyout(&ss, SCARG(uap, set), sizeof(ss));
}

/*
 * Suspend process until signal, providing mask to be set in the meantime. 
 * Note nonstandard calling convention: libc stub passes mask, not pointer,
 * to save a copyin.
 */
int
sys___sigsuspend14(struct lwp *l, const struct sys___sigsuspend14_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const sigset_t *)	set;
	} */
	sigset_t	ss;
	int		error;

	if (SCARG(uap, set)) {
		error = copyin(SCARG(uap, set), &ss, sizeof(ss));
		if (error)
			return error;
	}
	return sigsuspend1(l, SCARG(uap, set) ? &ss : 0);
}

int
sys___sigaltstack14(struct lwp *l, const struct sys___sigaltstack14_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const struct sigaltstack *)	nss;
		syscallarg(struct sigaltstack *)	oss;
	} */
	struct sigaltstack	nss, oss;
	int			error;

	if (SCARG(uap, nss)) {
		error = copyin(SCARG(uap, nss), &nss, sizeof(nss));
		if (error)
			return error;
	}
	error = sigaltstack1(l,
	    SCARG(uap, nss) ? &nss : 0, SCARG(uap, oss) ? &oss : 0);
	if (error)
		return error;
	if (SCARG(uap, oss)) {
		error = copyout(&oss, SCARG(uap, oss), sizeof(oss));
		if (error)
			return error;
	}
	return 0;
}

int
kill1(struct lwp *l, pid_t pid, ksiginfo_t *ksi, register_t *retval)
{
	int error;
	struct proc *p;

	if ((u_int)ksi->ksi_signo >= NSIG)
		return EINVAL;

	if (pid != l->l_proc->p_pid) {
		if (ksi->ksi_pid != l->l_proc->p_pid)
			return EPERM;

		if (ksi->ksi_uid != kauth_cred_geteuid(l->l_cred))
			return EPERM;

		switch (ksi->ksi_code) {
		case SI_USER:
		case SI_QUEUE:
			break;
		default:
			return EPERM;
		}
	}

	if (pid > 0) {
		/* kill single process */
		mutex_enter(proc_lock);
		p = proc_find_raw(pid);
		if (p == NULL || (p->p_stat != SACTIVE && p->p_stat != SSTOP)) {
			mutex_exit(proc_lock);
			/* IEEE Std 1003.1-2001: return success for zombies */
			return p ? 0 : ESRCH;
		}
		mutex_enter(p->p_lock);
		error = kauth_authorize_process(l->l_cred,
		    KAUTH_PROCESS_SIGNAL, p, KAUTH_ARG(ksi->ksi_signo),
		    NULL, NULL);
		if (!error && ksi->ksi_signo) {
			kpsignal2(p, ksi);
		}
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		return error;
	}

	switch (pid) {
	case -1:		/* broadcast signal */
		return killpg1(l, ksi, 0, 1);
	case 0:			/* signal own process group */
		return killpg1(l, ksi, 0, 0);
	default:		/* negative explicit process group */
		return killpg1(l, ksi, -pid, 0);
	}
	/* NOTREACHED */
}

int
sys_sigqueueinfo(struct lwp *l, const struct sys_sigqueueinfo_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(pid_t int)	pid;
		syscallarg(const siginfo_t *)	info;
	} */
	ksiginfo_t	ksi;
	int error;

	KSI_INIT(&ksi);

	if ((error = copyin(&SCARG(uap, info)->_info, &ksi.ksi_info,
	    sizeof(ksi.ksi_info))) != 0)
		return error;

	return kill1(l, SCARG(uap, pid), &ksi, retval);
}

int
sys_kill(struct lwp *l, const struct sys_kill_args *uap, register_t *retval)
{
	/* {
		syscallarg(pid_t)	pid;
		syscallarg(int)	signum;
	} */
	ksiginfo_t	ksi;

	KSI_INIT(&ksi);

	ksi.ksi_signo = SCARG(uap, signum);
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = l->l_proc->p_pid;
	ksi.ksi_uid = kauth_cred_geteuid(l->l_cred);

	return kill1(l, SCARG(uap, pid), &ksi, retval);
}

int
sys_getcontext(struct lwp *l, const struct sys_getcontext_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(struct __ucontext *) ucp;
	} */
	struct proc *p = l->l_proc;
	ucontext_t uc;

	memset(&uc, 0, sizeof(uc));

	mutex_enter(p->p_lock);
	getucontext(l, &uc);
	mutex_exit(p->p_lock);

	return copyout(&uc, SCARG(uap, ucp), sizeof (*SCARG(uap, ucp)));
}

int
sys_setcontext(struct lwp *l, const struct sys_setcontext_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const ucontext_t *) ucp;
	} */
	struct proc *p = l->l_proc;
	ucontext_t uc;
	int error;

	error = copyin(SCARG(uap, ucp), &uc, sizeof (uc));
	if (error)
		return error;
	if ((uc.uc_flags & _UC_CPU) == 0)
		return EINVAL;
	mutex_enter(p->p_lock);
	error = setucontext(l, &uc);
	mutex_exit(p->p_lock);
	if (error)
 		return error;

	return EJUSTRETURN;
}

/*
 * sigtimedwait(2) system call, used also for implementation
 * of sigwaitinfo() and sigwait().
 *
 * This only handles single LWP in signal wait. libpthread provides
 * its own sigtimedwait() wrapper to DTRT WRT individual threads.
 */
int
sys_____sigtimedwait50(struct lwp *l,
    const struct sys_____sigtimedwait50_args *uap, register_t *retval)
{

	return sigtimedwait1(l, uap, retval, copyin, copyout, copyin, copyout);
}

int
sigaction1(struct lwp *l, int signum, const struct sigaction *nsa,
	struct sigaction *osa, const void *tramp, int vers)
{
	struct proc *p;
	struct sigacts *ps;
	sigset_t tset;
	int prop, error;
	ksiginfoq_t kq;
	static bool v0v1valid;

	if (signum <= 0 || signum >= NSIG)
		return EINVAL;

	p = l->l_proc;
	error = 0;
	ksiginfo_queue_init(&kq);

	/*
	 * Trampoline ABI version 0 is reserved for the legacy kernel
	 * provided on-stack trampoline.  Conversely, if we are using a
	 * non-0 ABI version, we must have a trampoline.  Only validate the
	 * vers if a new sigaction was supplied and there was an actual
	 * handler specified (not SIG_IGN or SIG_DFL), which don't require
	 * a trampoline. Emulations use legacy kernel trampolines with
	 * version 0, alternatively check for that too.
	 *
	 * If version < 2, we try to autoload the compat module.  Note
	 * that we interlock with the unload check in compat_modcmd()
	 * using kernconfig_lock.  If the autoload fails, we don't try it
	 * again for this process.
	 */
	if (nsa != NULL && nsa->sa_handler != SIG_IGN
	    && nsa->sa_handler != SIG_DFL) {
		if (__predict_false(vers < 2)) {
			if (p->p_flag & PK_32)
				v0v1valid = true;
			else if ((p->p_lflag & PL_SIGCOMPAT) == 0) {
				kernconfig_lock();
				if (sendsig_sigcontext_vec == NULL) {
					(void)module_autoload("compat",
					    MODULE_CLASS_ANY);
				}
				if (sendsig_sigcontext_vec != NULL) {
					/*
					 * We need to remember if the
					 * sigcontext method may be useable,
					 * because libc may use it even
					 * if siginfo is available.
					 */
					v0v1valid = true;
				}
				mutex_enter(proc_lock);
				/*
				 * Prevent unload of compat module while
				 * this process remains.
				 */
				p->p_lflag |= PL_SIGCOMPAT;
				mutex_exit(proc_lock);
				kernconfig_unlock();
			}
		}

		switch (vers) {
		case 0:
			/* sigcontext, kernel supplied trampoline. */
			if (tramp != NULL || !v0v1valid) {
				return EINVAL;
			}
			break;
		case 1:
			/* sigcontext, user supplied trampoline. */
			if (tramp == NULL || !v0v1valid) {
				return EINVAL;
			}
			break;
		case 2:
		case 3:
			/* siginfo, user supplied trampoline. */
			if (tramp == NULL) {
				return EINVAL;
			}
			break;
		default:
			return EINVAL;
		}
	}

	mutex_enter(p->p_lock);

	ps = p->p_sigacts;
	if (osa)
		*osa = SIGACTION_PS(ps, signum);
	if (!nsa)
		goto out;

	prop = sigprop[signum];
	if ((nsa->sa_flags & ~SA_ALLBITS) || (prop & SA_CANTMASK)) {
		error = EINVAL;
		goto out;
	}

	SIGACTION_PS(ps, signum) = *nsa;
	ps->sa_sigdesc[signum].sd_tramp = tramp;
	ps->sa_sigdesc[signum].sd_vers = vers;
	sigminusset(&sigcantmask, &SIGACTION_PS(ps, signum).sa_mask);

	if ((prop & SA_NORESET) != 0)
		SIGACTION_PS(ps, signum).sa_flags &= ~SA_RESETHAND;

	if (signum == SIGCHLD) {
		if (nsa->sa_flags & SA_NOCLDSTOP)
			p->p_sflag |= PS_NOCLDSTOP;
		else
			p->p_sflag &= ~PS_NOCLDSTOP;
		if (nsa->sa_flags & SA_NOCLDWAIT) {
			/*
			 * Paranoia: since SA_NOCLDWAIT is implemented by
			 * reparenting the dying child to PID 1 (and trust
			 * it to reap the zombie), PID 1 itself is forbidden
			 * to set SA_NOCLDWAIT.
			 */
			if (p->p_pid == 1)
				p->p_flag &= ~PK_NOCLDWAIT;
			else
				p->p_flag |= PK_NOCLDWAIT;
		} else
			p->p_flag &= ~PK_NOCLDWAIT;

		if (nsa->sa_handler == SIG_IGN) {
			/*
			 * Paranoia: same as above.
			 */
			if (p->p_pid == 1)
				p->p_flag &= ~PK_CLDSIGIGN;
			else
				p->p_flag |= PK_CLDSIGIGN;
		} else
			p->p_flag &= ~PK_CLDSIGIGN;
	}

	if ((nsa->sa_flags & SA_NODEFER) == 0)
		sigaddset(&SIGACTION_PS(ps, signum).sa_mask, signum);
	else
		sigdelset(&SIGACTION_PS(ps, signum).sa_mask, signum);

	/*
	 * Set bit in p_sigctx.ps_sigignore for signals that are set to
	 * SIG_IGN, and for signals set to SIG_DFL where the default is to
	 * ignore. However, don't put SIGCONT in p_sigctx.ps_sigignore, as
	 * we have to restart the process.
	 */
	if (nsa->sa_handler == SIG_IGN ||
	    (nsa->sa_handler == SIG_DFL && (prop & SA_IGNORE) != 0)) {
		/* Never to be seen again. */
		sigemptyset(&tset);
		sigaddset(&tset, signum);
		sigclearall(p, &tset, &kq);
		if (signum != SIGCONT) {
			/* Easier in psignal */
			sigaddset(&p->p_sigctx.ps_sigignore, signum);
		}
		sigdelset(&p->p_sigctx.ps_sigcatch, signum);
	} else {
		sigdelset(&p->p_sigctx.ps_sigignore, signum);
		if (nsa->sa_handler == SIG_DFL)
			sigdelset(&p->p_sigctx.ps_sigcatch, signum);
		else
			sigaddset(&p->p_sigctx.ps_sigcatch, signum);
	}

	/*
	 * Previously held signals may now have become visible.  Ensure that
	 * we check for them before returning to userspace.
	 */
	if (sigispending(l, 0)) {
		lwp_lock(l);
		l->l_flag |= LW_PENDSIG;
		lwp_unlock(l);
	}
out:
	mutex_exit(p->p_lock);
	ksiginfo_queue_drain(&kq);

	return error;
}

int
sigprocmask1(struct lwp *l, int how, const sigset_t *nss, sigset_t *oss)
{
	sigset_t *mask = &l->l_sigmask;
	bool more;

	KASSERT(mutex_owned(l->l_proc->p_lock));

	if (oss) {
		*oss = *mask;
	}

	if (nss == NULL) {
		return 0;
	}

	switch (how) {
	case SIG_BLOCK:
		sigplusset(nss, mask);
		more = false;
		break;
	case SIG_UNBLOCK:
		sigminusset(nss, mask);
		more = true;
		break;
	case SIG_SETMASK:
		*mask = *nss;
		more = true;
		break;
	default:
		return EINVAL;
	}
	sigminusset(&sigcantmask, mask);
	if (more && sigispending(l, 0)) {
		/*
		 * Check for pending signals on return to user.
		 */
		lwp_lock(l);
		l->l_flag |= LW_PENDSIG;
		lwp_unlock(l);
	}
	return 0;
}

void
sigpending1(struct lwp *l, sigset_t *ss)
{
	struct proc *p = l->l_proc;

	mutex_enter(p->p_lock);
	*ss = l->l_sigpend.sp_set;
	sigplusset(&p->p_sigpend.sp_set, ss);
	mutex_exit(p->p_lock);
}

void
sigsuspendsetup(struct lwp *l, const sigset_t *ss)
{
	struct proc *p = l->l_proc;

	/*
	 * When returning from sigsuspend/pselect/pollts, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigctx structure
	 * to indicate this.
	 */
	mutex_enter(p->p_lock);
	l->l_sigrestore = 1;
	l->l_sigoldmask = l->l_sigmask;
	l->l_sigmask = *ss;
	sigminusset(&sigcantmask, &l->l_sigmask);

	/* Check for pending signals when sleeping. */
	if (sigispending(l, 0)) {
		lwp_lock(l);
		l->l_flag |= LW_PENDSIG;
		lwp_unlock(l);
	}
	mutex_exit(p->p_lock);
}

void
sigsuspendteardown(struct lwp *l)
{
	struct proc *p = l->l_proc;

	mutex_enter(p->p_lock);
	/* Check for pending signals when sleeping. */
	if (l->l_sigrestore) {
		if (sigispending(l, 0)) {
			lwp_lock(l);
			l->l_flag |= LW_PENDSIG;
			lwp_unlock(l);
		} else {
			l->l_sigrestore = 0;
			l->l_sigmask = l->l_sigoldmask;
		}
	}
	mutex_exit(p->p_lock);
}

int
sigsuspend1(struct lwp *l, const sigset_t *ss)
{

	if (ss)
		sigsuspendsetup(l, ss);

	while (kpause("pause", true, 0, NULL) == 0)
		;

	/* always return EINTR rather than ERESTART... */
	return EINTR;
}

int
sigaltstack1(struct lwp *l, const struct sigaltstack *nss,
    struct sigaltstack *oss)
{
	struct proc *p = l->l_proc;
	int error = 0;

	mutex_enter(p->p_lock);

	if (oss)
		*oss = l->l_sigstk;

	if (nss) {
		if (nss->ss_flags & ~SS_ALLBITS)
			error = EINVAL;
		else if (nss->ss_flags & SS_DISABLE) {
			if (l->l_sigstk.ss_flags & SS_ONSTACK)
				error = EINVAL;
		} else if (nss->ss_size < MINSIGSTKSZ)
			error = ENOMEM;

		if (!error)
			l->l_sigstk = *nss;
	}

	mutex_exit(p->p_lock);

	return error;
}

int
sigtimedwait1(struct lwp *l, const struct sys_____sigtimedwait50_args *uap,
    register_t *retval, copyin_t fetchss, copyout_t storeinf, copyin_t fetchts,
    copyout_t storets)
{
	/* {
		syscallarg(const sigset_t *) set;
		syscallarg(siginfo_t *) info;
		syscallarg(struct timespec *) timeout;
	} */
	struct proc *p = l->l_proc;
	int error, signum, timo;
	struct timespec ts, tsstart, tsnow;
	ksiginfo_t ksi;

	/*
	 * Calculate timeout, if it was specified.
	 *
	 * NULL pointer means an infinite timeout.
	 * {.tv_sec = 0, .tv_nsec = 0} means do not block.
	 */
	if (SCARG(uap, timeout)) {
		error = (*fetchts)(SCARG(uap, timeout), &ts, sizeof(ts));
		if (error)
			return error;

		if ((error = itimespecfix(&ts)) != 0)
			return error;

		timo = tstohz(&ts);
		if (timo == 0) {
			if (ts.tv_sec == 0 && ts.tv_nsec == 0)
				timo = -1; /* do not block */
			else
				timo = 1; /* the shortest possible timeout */
		}

		/*
		 * Remember current uptime, it would be used in
		 * ECANCELED/ERESTART case.
		 */
		getnanouptime(&tsstart);
	} else {
		memset(&tsstart, 0, sizeof(tsstart)); /* XXXgcc */
		timo = 0; /* infinite timeout */
	}

	error = (*fetchss)(SCARG(uap, set), &l->l_sigwaitset,
	    sizeof(l->l_sigwaitset));
	if (error)
		return error;

	/*
	 * Silently ignore SA_CANTMASK signals. psignal1() would ignore
	 * SA_CANTMASK signals in waitset, we do this only for the below
	 * siglist check.
	 */
	sigminusset(&sigcantmask, &l->l_sigwaitset);

	mutex_enter(p->p_lock);

	/* Check for pending signals in the process, if no - then in LWP. */
	if ((signum = sigget(&p->p_sigpend, &ksi, 0, &l->l_sigwaitset)) == 0)
		signum = sigget(&l->l_sigpend, &ksi, 0, &l->l_sigwaitset);

	if (signum != 0) {
		/* If found a pending signal, just copy it out to the user. */
		mutex_exit(p->p_lock);
		goto out;
	}

	if (timo < 0) {
		/* If not allowed to block, return an error */
		mutex_exit(p->p_lock);
		return EAGAIN;
	}

	/*
	 * Set up the sigwait list and wait for signal to arrive.
	 * We can either be woken up or time out.
	 */
	l->l_sigwaited = &ksi;
	LIST_INSERT_HEAD(&p->p_sigwaiters, l, l_sigwaiter);
	error = cv_timedwait_sig(&l->l_sigcv, p->p_lock, timo);

	/*
	 * Need to find out if we woke as a result of _lwp_wakeup() or a
	 * signal outside our wait set.
	 */
	if (l->l_sigwaited != NULL) {
		if (error == EINTR) {
			/* Wakeup via _lwp_wakeup(). */
			error = ECANCELED;
		} else if (!error) {
			/* Spurious wakeup - arrange for syscall restart. */
			error = ERESTART;
		}
		l->l_sigwaited = NULL;
		LIST_REMOVE(l, l_sigwaiter);
	}
	mutex_exit(p->p_lock);

	/*
	 * If the sleep was interrupted (either by signal or wakeup), update
	 * the timeout and copyout new value back.  It would be used when
	 * the syscall would be restarted or called again.
	 */
	if (timo && (error == ERESTART || error == ECANCELED)) {
		getnanouptime(&tsnow);

		/* Compute how much time has passed since start. */
		timespecsub(&tsnow, &tsstart, &tsnow);

		/* Substract passed time from timeout. */
		timespecsub(&ts, &tsnow, &ts);

		if (ts.tv_sec < 0)
			error = EAGAIN;
		else {
			/* Copy updated timeout to userland. */
			error = (*storets)(&ts, SCARG(uap, timeout),
			    sizeof(ts));
		}
	}
out:
	/*
	 * If a signal from the wait set arrived, copy it to userland.
	 * Copy only the used part of siginfo, the padding part is
	 * left unchanged (userland is not supposed to touch it anyway).
	 */
	if (error == 0 && SCARG(uap, info)) {
		error = (*storeinf)(&ksi.ksi_info, SCARG(uap, info),
		    sizeof(ksi.ksi_info));
	}
	if (error == 0) {
		*retval = ksi.ksi_info._signo;
		SDT_PROBE(proc, kernel, , signal__clear, *retval,
		    &ksi, 0, 0, 0);
	}
	return error;
}
