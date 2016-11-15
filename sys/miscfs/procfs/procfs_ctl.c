/*	$NetBSD: procfs_ctl.c,v 1.47 2009/10/21 21:12:06 rmind Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_ctl.c	8.4 (Berkeley) 6/15/94
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)procfs_ctl.c	8.4 (Berkeley) 6/15/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_ctl.c,v 1.47 2009/10/21 21:12:06 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <miscfs/procfs/procfs.h>

#define PROCFS_CTL_ATTACH	1
#define PROCFS_CTL_DETACH	2
#define PROCFS_CTL_STEP		3
#define PROCFS_CTL_RUN		4
#define PROCFS_CTL_WAIT		5

static const vfs_namemap_t ctlnames[] = {
	/* special /proc commands */
	{ "attach",	PROCFS_CTL_ATTACH },
	{ "detach",	PROCFS_CTL_DETACH },
	{ "step",	PROCFS_CTL_STEP },
	{ "run",	PROCFS_CTL_RUN },
	{ "wait",	PROCFS_CTL_WAIT },
	{ NULL,		0 },
};

static const vfs_namemap_t signames[] = {
	/* regular signal names */
	{ "hup",	SIGHUP },	{ "int",	SIGINT },
	{ "quit",	SIGQUIT },	{ "ill",	SIGILL },
	{ "trap",	SIGTRAP },	{ "abrt",	SIGABRT },
	{ "iot",	SIGIOT },	{ "emt",	SIGEMT },
	{ "fpe",	SIGFPE },	{ "kill",	SIGKILL },
	{ "bus",	SIGBUS },	{ "segv",	SIGSEGV },
	{ "sys",	SIGSYS },	{ "pipe",	SIGPIPE },
	{ "alrm",	SIGALRM },	{ "term",	SIGTERM },
	{ "urg",	SIGURG },	{ "stop",	SIGSTOP },
	{ "tstp",	SIGTSTP },	{ "cont",	SIGCONT },
	{ "chld",	SIGCHLD },	{ "ttin",	SIGTTIN },
	{ "ttou",	SIGTTOU },	{ "io",		SIGIO },
	{ "xcpu",	SIGXCPU },	{ "xfsz",	SIGXFSZ },
	{ "vtalrm",	SIGVTALRM },	{ "prof",	SIGPROF },
	{ "winch",	SIGWINCH },	{ "info",	SIGINFO },
	{ "usr1",	SIGUSR1 },	{ "usr2",	SIGUSR2 },
	{ NULL,		0 },
};

static int procfs_control(struct lwp *, struct lwp *, int, int,
    struct pfsnode *);

int
procfs_control(struct lwp *curl, struct lwp *l, int op, int sig, struct pfsnode *pfs)
{
	struct proc *curp = curl->l_proc;
	struct proc *p = l->l_proc;
	int error = 0;

	mutex_enter(proc_lock);
	mutex_enter(p->p_lock);

	switch (op) {
	/*
	 * Attach - attaches the target process for debugging
	 * by the calling process.
	 */
	case PROCFS_CTL_ATTACH:
		/*
		 * You can't attach to a process if:
		 *      (1) it's the process that's doing the attaching,
		 */
		if (p->p_pid == curp->p_pid) {
			error = EINVAL;
			break;
		}

		/*
		 *      (2) it's already being traced, or
		 */
		if (ISSET(p->p_slflag, PSL_TRACED)) {
			error = EBUSY;
			break;
		}

		/*
		 *      (3) the security model prevents it.
		 */
		if ((error = kauth_authorize_process(curl->l_cred,
		    KAUTH_PROCESS_PROCFS, p, pfs,
		    KAUTH_ARG(KAUTH_REQ_PROCESS_PROCFS_CTL), NULL)) != 0)
		    	break;

		break;

	/*
	 * Target process must be stopped, owned by (curp) and
	 * be set up for tracing (PSL_TRACED flag set).
	 * Allow DETACH to take place at any time for sanity.
	 * Allow WAIT any time, of course.
	 *
	 * Note that the semantics of DETACH are different here
	 * from ptrace(2).
	 */
	case PROCFS_CTL_DETACH:
	case PROCFS_CTL_WAIT:
		/*
		 * You can't do what you want to the process if:
		 *      (1) It's not being traced at all,
		 */
		if (!ISSET(p->p_slflag, PSL_TRACED)) {
			error = EPERM;
			break;
		}

		/*
		 *	(2) it's being traced by ptrace(2) (which has
		 *	    different signal delivery semantics), or
		 */
		if (!ISSET(p->p_slflag, PSL_FSTRACE)) {
			error = EBUSY;
			break;
		}

		/*
		 *      (3) it's not being traced by _you_.
		 */
		if (p->p_pptr != curp)
			error = EBUSY;

		break;

	default:
		/*
		 * You can't do what you want to the process if:
		 *      (1) It's not being traced at all,
		 */
		if (!ISSET(p->p_slflag, PSL_TRACED)) {
			error = EPERM;
			break;
		}

		/*
		 *	(2) it's being traced by ptrace(2) (which has
		 *	    different signal delivery semantics),
		 */
		if (!ISSET(p->p_slflag, PSL_FSTRACE)) {
			error = EBUSY;
			break;
		}

		/*
		 *      (3) it's not being traced by _you_, or
		 */
		if (p->p_pptr != curp) {
			error = EBUSY;
			break;
		}

		/*
		 *      (4) it's not currently stopped.
		 */
		if (p->p_stat != SSTOP || !p->p_waited /* XXXSMP */)
			error = EBUSY;

		break;
	}

	if (error != 0) {
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		return (error);
	}

	/* Do single-step fixup if needed. */
	FIX_SSTEP(p);

	switch (op) {
	case PROCFS_CTL_ATTACH:
		/*
		 * Go ahead and set the trace flag.
		 * Save the old parent (it's reset in
		 *   _DETACH, and also in kern_exit.c:wait4()
		 * Reparent the process so that the tracing
		 *   proc gets to see all the action.
		 * Stop the target.
		 */
		SET(p->p_slflag, PSL_TRACED|PSL_FSTRACE);
		p->p_opptr = p->p_pptr;
		if (p->p_pptr != curp) {
			if (p->p_pptr->p_lock < p->p_lock) {
				if (!mutex_tryenter(p->p_pptr->p_lock)) {
					mutex_exit(p->p_lock);
					mutex_enter(p->p_pptr->p_lock);
				}
			} else if (p->p_pptr->p_lock > p->p_lock) {
				mutex_enter(p->p_pptr->p_lock);
			}
			p->p_pptr->p_slflag |= PSL_CHTRACED;
			proc_reparent(p, curp);
			if (p->p_pptr->p_lock != p->p_lock)
				mutex_exit(p->p_pptr->p_lock);
		}
		sig = SIGSTOP;
		goto sendsig;

#ifdef PT_STEP
	case PROCFS_CTL_STEP:
		/*
		 * Step.  Let the target process execute a single instruction.
		 */
#endif
	case PROCFS_CTL_RUN:
	case PROCFS_CTL_DETACH:
#ifdef PT_STEP
		/* XXXAD locking? */
		error = process_sstep(l, op == PROCFS_CTL_STEP);
		if (error)
			break;
#endif

		if (op == PROCFS_CTL_DETACH) {
			/* give process back to original parent */
			if (p->p_opptr != p->p_pptr) {
				struct proc *pp = p->p_opptr;
				proc_reparent(p, pp ? pp : initproc);
			}

			/* not being traced any more */
			p->p_opptr = NULL;
			CLR(p->p_slflag, PSL_TRACED|PSL_FSTRACE);
			p->p_waited = 0;	/* XXXSMP */
		}

	sendsig:
		/* Finally, deliver the requested signal (or none). */
		lwp_lock(l);
		if (l->l_stat == LSSTOP) {
			p->p_xstat = sig;
			/* setrunnable() will release the lock. */
			setrunnable(l);
		} else {
			lwp_unlock(l);
			if (sig != 0) {
				mutex_exit(p->p_lock);
				psignal(p, sig);
				mutex_enter(p->p_lock);
			}
		}
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		return (error);

	case PROCFS_CTL_WAIT:
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);

		/*
		 * Wait for the target process to stop.
		 * XXXSMP WTF is this?
		 */
		while (l->l_stat != LSSTOP && P_ZOMBIE(p)) {
			error = tsleep(l, PWAIT|PCATCH, "procfsx", hz);
			if (error)
				break;
		}

		return (error);

	default:
		panic("procfs_ctl");
		/* NOTREACHED */
	}

	mutex_exit(p->p_lock);
	mutex_exit(proc_lock);
	return (error);
}

int
procfs_doctl(
    struct lwp *curl,
    struct lwp *l,
    struct pfsnode *pfs,
    struct uio *uio
)
{
	struct proc *p = l->l_proc;
	char msg[PROCFS_CTLLEN+1];
	const vfs_namemap_t *nm;
	int error;
	int xlen;

	if (uio->uio_rw != UIO_WRITE)
		return (EOPNOTSUPP);

	xlen = PROCFS_CTLLEN;
	error = vfs_getuserstr(uio, msg, &xlen);
	if (error)
		return (error);

	/*
	 * Map signal names into signal generation
	 * or debug control.  Unknown commands and/or signals
	 * return EOPNOTSUPP.
	 *
	 * Sending a signal while the process is being debugged
	 * also has the side effect of letting the target continue
	 * to run.  There is no way to single-step a signal delivery.
	 */
	error = EOPNOTSUPP;

	nm = vfs_findname(ctlnames, msg, xlen);
	if (nm) {
		error = procfs_control(curl, l, nm->nm_val, 0, pfs);
	} else {
		nm = vfs_findname(signames, msg, xlen);
		if (nm) {
			if (ISSET(p->p_slflag, PSL_TRACED) &&
			    p->p_pptr == p)
				error = procfs_control(curl, l, PROCFS_CTL_RUN,
				    nm->nm_val, pfs);
			else {
				psignal(p, nm->nm_val);
				error = 0;
			}
		}
	}

	return (error);
}
