/*	$NetBSD: kern_syscall.c,v 1.11 2015/05/09 05:56:36 pgoyette Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_syscall.c,v 1.11 2015/05/09 05:56:36 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_modular.h"
#include "opt_syscall_debug.h"
#include "opt_ktrace.h"
#include "opt_ptrace.h"
#include "opt_dtrace.h"
#endif

/* XXX To get syscall prototypes. */
#define SYSVSHM
#define SYSVSEM
#define SYSVMSG

#include <sys/param.h>
#include <sys/module.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>
#include <sys/systm.h>
#include <sys/xcall.h>
#include <sys/ktrace.h>
#include <sys/ptrace.h>

int
sys_nomodule(struct lwp *l, const void *v, register_t *retval)
{
#ifdef MODULAR
#include <kern/syscalls_autoload.c>

	const struct sysent *sy;
	const struct emul *em;
	int code, i;

	/*
	 * Restart the syscall if we interrupted a module unload that
	 * failed.  Acquiring kernconfig_lock delays us until any unload
	 * has been completed or rolled back.
	 */
	kernconfig_lock();
	sy = l->l_sysent;
	if (sy->sy_call != sys_nomodule) {
		kernconfig_unlock();
		return ERESTART;
	}
	/*
	 * Try to autoload a module to satisfy the request.  If it 
	 * works, retry the request.
	 */
	em = l->l_proc->p_emul;
	if (em == &emul_netbsd) {
		code = sy - em->e_sysent;
		for (i = 0; i < __arraycount(syscalls_autoload); i++) {
			if (syscalls_autoload[i].al_code != code) {
				continue;
			}
			if (module_autoload(syscalls_autoload[i].al_module,
			    MODULE_CLASS_ANY) != 0 ||
			    sy->sy_call == sys_nomodule) {
			    	break;
			}
			kernconfig_unlock();
			return ERESTART;
		}
	}
	kernconfig_unlock();
#endif	/* MODULAR */

	return sys_nosys(l, v, retval);
}

int
syscall_establish(const struct emul *em, const struct syscall_package *sp)
{
	struct sysent *sy;
	int i;

	KASSERT(kernconfig_is_held());

	if (em == NULL) {
		em = &emul_netbsd;
	}
	sy = em->e_sysent;

	/*
	 * Ensure that all preconditions are valid, since this is
	 * an all or nothing deal.  Once a system call is entered,
	 * it can become busy and we could be unable to remove it
	 * on error.
	 */
	for (i = 0; sp[i].sp_call != NULL; i++) {
		if (sy[sp[i].sp_code].sy_call != sys_nomodule) {
#ifdef DIAGNOSTIC
			printf("syscall %d is busy\n", sp[i].sp_code);
#endif
			return EBUSY;
		}
	}
	/* Everything looks good, patch them in. */
	for (i = 0; sp[i].sp_call != NULL; i++) {
		sy[sp[i].sp_code].sy_call = sp[i].sp_call;
	}

	return 0;
}

int
syscall_disestablish(const struct emul *em, const struct syscall_package *sp)
{
	struct sysent *sy;
	uint64_t where;
	lwp_t *l;
	int i;

	KASSERT(kernconfig_is_held());

	if (em == NULL) {
		em = &emul_netbsd;
	}
	sy = em->e_sysent;

	/*
	 * First, patch the system calls to sys_nomodule to gate further
	 * activity.
	 */
	for (i = 0; sp[i].sp_call != NULL; i++) {
		KASSERT(sy[sp[i].sp_code].sy_call == sp[i].sp_call);
		sy[sp[i].sp_code].sy_call = sys_nomodule;
	}

	/*
	 * Run a cross call to cycle through all CPUs.  This does two
	 * things: lock activity provides a barrier and makes our update
	 * of sy_call visible to all CPUs, and upon return we can be sure
	 * that we see pertinent values of l_sysent posted by remote CPUs.
	 */
	where = xc_broadcast(0, (xcfunc_t)nullop, NULL, NULL);
	xc_wait(where);

	/*
	 * Now it's safe to check l_sysent.  Run through all LWPs and see
	 * if anyone is still using the system call.
	 */
	for (i = 0; sp[i].sp_call != NULL; i++) {
		mutex_enter(proc_lock);
		LIST_FOREACH(l, &alllwp, l_list) {
			if (l->l_sysent == &sy[sp[i].sp_code]) {
				break;
			}
		}
		mutex_exit(proc_lock);
		if (l == NULL) {
			continue;
		}
		/*
		 * We lose: one or more calls are still in use.  Put back
		 * the old entrypoints and act like nothing happened.
		 * When we drop kernconfig_lock, any system calls held in
		 * sys_nomodule() will be restarted.
		 */
		for (i = 0; sp[i].sp_call != NULL; i++) {
			sy[sp[i].sp_code].sy_call = sp[i].sp_call;
		}
		return EBUSY;
	}

	return 0;
}

/*
 * Return true if system call tracing is enabled for the specified process.
 */
bool
trace_is_enabled(struct proc *p)
{
#ifdef SYSCALL_DEBUG
	return (true);
#endif
#ifdef KTRACE
	if (ISSET(p->p_traceflag, (KTRFAC_SYSCALL | KTRFAC_SYSRET)))
		return (true);
#endif
#ifdef PTRACE
	if (ISSET(p->p_slflag, PSL_SYSCALL))
		return (true);
#endif

	return (false);
}

/*
 * Start trace of particular system call. If process is being traced,
 * this routine is called by MD syscall dispatch code just before
 * a system call is actually executed.
 */
int
trace_enter(register_t code, const struct sysent *sy, const void *args)
{
	int error = 0;

#ifdef KDTRACE_HOOKS
	if (sy->sy_entry) {
		struct emul *e = curlwp->l_proc->p_emul;
		(*e->e_dtrace_syscall)(sy->sy_entry, code, sy, args, NULL, 0);
	}
#endif

#ifdef SYSCALL_DEBUG
	scdebug_call(code, args);
#endif /* SYSCALL_DEBUG */

	ktrsyscall(code, args, sy->sy_narg);

#ifdef PTRACE
	if ((curlwp->l_proc->p_slflag & (PSL_SYSCALL|PSL_TRACED)) ==
	    (PSL_SYSCALL|PSL_TRACED)) {
		process_stoptrace();
		if (curlwp->l_proc->p_slflag & PSL_SYSCALLEMU) {
			/* tracer will emulate syscall for us */
			error = EJUSTRETURN;
		}
	}
#endif
	return error;
}

/*
 * End trace of particular system call. If process is being traced,
 * this routine is called by MD syscall dispatch code just after
 * a system call finishes.
 * MD caller guarantees the passed 'code' is within the supported
 * system call number range for emulation the process runs under.
 */
void
trace_exit(register_t code, const struct sysent *sy, const void *args,
    register_t rval[], int error)
{
#if defined(PTRACE) || defined(KDTRACE_HOOKS)
	struct proc *p = curlwp->l_proc;
#endif

#ifdef KDTRACE_HOOKS
	if (sy->sy_return) {
		(*p->p_emul->e_dtrace_syscall)(sy->sy_return, code, sy, args,
		    rval, error);
	}
#endif

#ifdef SYSCALL_DEBUG
	scdebug_ret(code, error, rval);
#endif /* SYSCALL_DEBUG */

	ktrsysret(code, error, rval);
	
#ifdef PTRACE
	if ((p->p_slflag & (PSL_SYSCALL|PSL_TRACED|PSL_SYSCALLEMU)) ==
	    (PSL_SYSCALL|PSL_TRACED))
		process_stoptrace();
	CLR(p->p_slflag, PSL_SYSCALLEMU);
#endif
}
