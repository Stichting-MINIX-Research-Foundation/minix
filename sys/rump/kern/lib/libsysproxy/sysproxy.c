/*	$NetBSD: sysproxy.c,v 1.3 2015/04/18 15:49:18 pooka Exp $	*/

/*
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
__KERNEL_RCSID(0, "$NetBSD: sysproxy.c,v 1.3 2015/04/18 15:49:18 pooka Exp $");

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/kmem.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/systm.h>
#include <sys/xcall.h>

#define _RUMP_SYSPROXY
#include <rump/rumpuser.h>

#include "rump_private.h"

int
rump_init_server(const char *url)
{

	return rumpuser_sp_init(url, ostype, osrelease, MACHINE);
}

static pid_t
hyp_getpid(void)
{

	return curproc->p_pid;
}

static int
hyp_syscall(int num, void *arg, long *retval)
{
	register_t regrv[2] = {0, 0};
	struct lwp *l;
	struct sysent *callp;
	int rv;

	if (__predict_false(num >= SYS_NSYSENT))
		return ENOSYS;

	/* XXX: always uses native syscall vector */
	callp = rump_sysent + num;
	l = curlwp;
	rv = sy_invoke(callp, l, (void *)arg, regrv, num);
	retval[0] = regrv[0];
	retval[1] = regrv[1];

	return rv;
}

static struct pmap remotepmap;

static int
hyp_rfork(void *priv, int flags, const char *comm)
{
	struct rump_spctl *spctl;
	struct vmspace *vm;
	struct proc *p;
	struct lwp *l;
	int error;
	bool initfds;

	/*
	 * If we are forking off of pid 1, initialize file descriptors.
	 */
	l = curlwp;
	if (l->l_proc->p_pid == 1) {
		KASSERT(flags == RUMP_RFFD_CLEAR);
		initfds = true;
	} else {
		initfds = false;
	}

	/*
	 * Since it's a proxy proc, we create a vmspace for it.
	 */
	spctl = kmem_zalloc(sizeof(*spctl), KM_SLEEP);
	vm = &spctl->spctl_vm;
	uvmspace_init(vm, &remotepmap, 0, 0, false);
	spctl->spctl = priv;

	if ((error = rump_lwproc_rfork_vmspace(vm, flags)) != 0) {
		kmem_free(vm, sizeof(*vm));
		return error;
	}

	/*
	 * We forked in this routine, so cannot use curlwp (const)
	 */
	l = rump_lwproc_curlwp();
	p = l->l_proc;

	if (comm)
		strlcpy(p->p_comm, comm, sizeof(p->p_comm));
	if (initfds)
		rump_consdev_init();

	return 0;
}

/*
 * Order all lwps in a process to exit.  does *not* wait for them to drain.
 */
static void
hyp_lwpexit(void)
{
	struct proc *p = curproc;
	uint64_t where;
	struct lwp *l;

	mutex_enter(p->p_lock);
	/*
	 * First pass: mark all lwps in the process with LW_RUMP_QEXIT
	 * so that they know they should exit.
	 */
	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		if (l == curlwp)
			continue;
		l->l_flag |= LW_RUMP_QEXIT;
	}
	mutex_exit(p->p_lock);

	/*
	 * Next, make sure everyone on all CPUs sees our status
	 * update.  This keeps threads inside cv_wait() and makes
	 * sure we don't access a stale cv pointer later when
	 * we wake up the threads.
	 */

	where = xc_broadcast(0, (xcfunc_t)nullop, NULL, NULL);
	xc_wait(where);

	/*
	 * Ok, all lwps are either:
	 *  1) not in the cv code
	 *  2) sleeping on l->l_private
	 *  3) sleeping on p->p_waitcv
	 *
	 * Either way, l_private is stable until we set PS_RUMP_LWPEXIT
	 * in p->p_sflag.
	 */

	mutex_enter(p->p_lock);
	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		if (l->l_private)
			cv_broadcast(l->l_private);
	}
	p->p_sflag |= PS_RUMP_LWPEXIT;
	cv_broadcast(&p->p_waitcv);
	mutex_exit(p->p_lock);
}

/*
 * Notify process that all threads have been drained and exec is complete.
 */
static void
hyp_execnotify(const char *comm)
{
	struct proc *p = curproc;

	fd_closeexec();
	mutex_enter(p->p_lock);
	KASSERT(p->p_nlwps == 1 && p->p_sflag & PS_RUMP_LWPEXIT);
	p->p_sflag &= ~PS_RUMP_LWPEXIT;
	mutex_exit(p->p_lock);
	strlcpy(p->p_comm, comm, sizeof(p->p_comm));
}

/*
 * Initialize interface pointers since component is present.
 */
RUMP_COMPONENT(RUMP_COMPONENT_KERN)
{

	rump_sysproxy_ops.rspo_copyin		= rumpuser_sp_copyin;
	rump_sysproxy_ops.rspo_copyinstr	= rumpuser_sp_copyinstr;
	rump_sysproxy_ops.rspo_copyout		= rumpuser_sp_copyout;
	rump_sysproxy_ops.rspo_copyoutstr	= rumpuser_sp_copyoutstr;
	rump_sysproxy_ops.rspo_anonmmap		= rumpuser_sp_anonmmap;
	rump_sysproxy_ops.rspo_raise		= rumpuser_sp_raise;
	rump_sysproxy_ops.rspo_fini		= rumpuser_sp_fini;

	rump_sysproxy_ops.rspo_hyp_getpid	= hyp_getpid;
	rump_sysproxy_ops.rspo_hyp_syscall	= hyp_syscall;
	rump_sysproxy_ops.rspo_hyp_rfork	= hyp_rfork;
	rump_sysproxy_ops.rspo_hyp_lwpexit	= hyp_lwpexit;
	rump_sysproxy_ops.rspo_hyp_execnotify	= hyp_execnotify;
}
