/*	$NetBSD: kern_ras.c,v 1.36 2013/09/14 20:46:27 martin Exp $	*/

/*-
 * Copyright (c) 2002, 2006, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry, and by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: kern_ras.c,v 1.36 2013/09/14 20:46:27 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/ras.h>
#include <sys/xcall.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#define MAX_RAS_PER_PROC	16

u_int ras_per_proc = MAX_RAS_PER_PROC;

#ifdef DEBUG
int ras_debug = 0;
#define DPRINTF(x)	if (ras_debug) printf x
#else
#define DPRINTF(x)	/* nothing */
#endif

/*
 * Force all CPUs through cpu_switchto(), waiting until complete.
 * Context switching will drain the write buffer on the calling
 * CPU.
 */
static void
ras_sync(void)
{

	/* No need to sync if exiting or single threaded. */
	if (curproc->p_nlwps > 1 && ncpu > 1) {
#ifdef NO_SOFTWARE_PATENTS
		uint64_t where;
		where = xc_broadcast(0, (xcfunc_t)nullop, NULL, NULL);
		xc_wait(where);
#else
		/*
		 * Assumptions:
		 *
		 * o preemption is disabled by the thread in
		 *   ras_lookup().
		 * o proc::p_raslist is only inspected with
		 *   preemption disabled.
		 * o ras_lookup() plus loads reordered in advance
		 *   will take no longer than 1/8s to complete.
		 */
		const int delta = hz >> 3;
		int target = hardclock_ticks + delta;
		do {
			kpause("ras", false, delta, NULL);
		} while (hardclock_ticks < target);
#endif
	}
}

/*
 * Check the specified address to see if it is within the
 * sequence.  If it is found, we return the restart address,
 * otherwise we return -1.  If we do perform a restart, we
 * mark the sequence as hit.
 *
 * No locking required: we disable preemption and ras_sync()
 * guarantees that individual entries are valid while we still
 * have visibility of them.
 */
void *
ras_lookup(struct proc *p, void *addr)
{
	struct ras *rp;
	void *startaddr;
	lwp_t *l;

	startaddr = (void *)-1;
	l = curlwp;

	KPREEMPT_DISABLE(l);
	for (rp = p->p_raslist; rp != NULL; rp = rp->ras_next) {
		if (addr > rp->ras_startaddr && addr < rp->ras_endaddr) {
			startaddr = rp->ras_startaddr;
			DPRINTF(("RAS hit: p=%p %p\n", p, addr));
			break;
		}
	}
	KPREEMPT_ENABLE(l);

	return startaddr;
}

/*
 * During a fork, we copy all of the sequences from parent p1 to
 * the child p2.
 *
 * No locking required as the parent must be paused.
 */
int
ras_fork(struct proc *p1, struct proc *p2)
{
	struct ras *rp, *nrp;

	for (rp = p1->p_raslist; rp != NULL; rp = rp->ras_next) {
		nrp = kmem_alloc(sizeof(*nrp), KM_SLEEP);
		nrp->ras_startaddr = rp->ras_startaddr;
		nrp->ras_endaddr = rp->ras_endaddr;
		nrp->ras_next = p2->p_raslist;
		p2->p_raslist = nrp;
	}

	DPRINTF(("ras_fork: p1=%p, p2=%p\n", p1, p2));

	return 0;
}

/*
 * Nuke all sequences for this process.
 */
int
ras_purgeall(void)
{
	struct ras *rp, *nrp;
	proc_t *p;

	p = curproc;

	if (p->p_raslist == NULL)
		return 0;

	mutex_enter(&p->p_auxlock);
	if ((rp = p->p_raslist) != NULL) {
		p->p_raslist = NULL;
		ras_sync();
		for(; rp != NULL; rp = nrp) {
			nrp = rp->ras_next;
			kmem_free(rp, sizeof(*rp));
		}
	}
	mutex_exit(&p->p_auxlock);

	return 0;
}

#if defined(__HAVE_RAS)

#if __GNUC_PREREQ__(4, 8)
#define	__WARNING_PUSH_LESS_NULL_PTR	_Pragma("GCC diagnostic push") 	_Pragma("GCC diagnostic ignored \"-Wextra\"")
#define	__WARNING_POP_LESS_NULL_PTR	_Pragma("GCC diagnostic pop")
#else
#define	__WARNING_PUSH_LESS_NULL_PTR
#define	__WARNING_POP_LESS_NULL_PTR
#endif

/*
 * Install the new sequence.  If it already exists, return
 * an error.
 */
static int
ras_install(void *addr, size_t len)
{
	struct ras *rp;
	struct ras *newrp;
	void *endaddr;
	int nras, error;
	proc_t *p;

	endaddr = (char *)addr + len;

	/* do not warn about < NULL pointer comparision */
	__WARNING_PUSH_LESS_NULL_PTR
	if (addr < (void *)VM_MIN_ADDRESS ||
	    endaddr > (void *)VM_MAXUSER_ADDRESS)
		return (EINVAL);
	__WARNING_POP_LESS_NULL_PTR

	if (len <= 0)
		return (EINVAL);

	newrp = kmem_alloc(sizeof(*newrp), KM_SLEEP);
	newrp->ras_startaddr = addr;
	newrp->ras_endaddr = endaddr;
	error = 0;
	nras = 0;
	p = curproc;

	mutex_enter(&p->p_auxlock);
	for (rp = p->p_raslist; rp != NULL; rp = rp->ras_next) {
		if (++nras >= ras_per_proc) {
			error = EINVAL;
			break;
		}
		if (addr < rp->ras_endaddr && endaddr > rp->ras_startaddr) {
			error = EEXIST;
			break;
		}
	}
	if (rp == NULL) {
		newrp->ras_next = p->p_raslist;
		p->p_raslist = newrp;
		ras_sync();
	 	mutex_exit(&p->p_auxlock);
	} else {
	 	mutex_exit(&p->p_auxlock);
 		kmem_free(newrp, sizeof(*newrp));
	}

	return error;
}

/*
 * Nuke the specified sequence.  Both address and len must
 * match, otherwise we return an error.
 */
static int
ras_purge(void *addr, size_t len)
{
	struct ras *rp, **link;
	void *endaddr;
	proc_t *p;

	endaddr = (char *)addr + len;
	p = curproc;

	mutex_enter(&p->p_auxlock);
	link = &p->p_raslist;
	for (rp = *link; rp != NULL; link = &rp->ras_next, rp = *link) {
		if (addr == rp->ras_startaddr && endaddr == rp->ras_endaddr)
			break;
	}
	if (rp != NULL) {
		*link = rp->ras_next;
		ras_sync();
		mutex_exit(&p->p_auxlock);
		kmem_free(rp, sizeof(*rp));
		return 0;
	} else {
		mutex_exit(&p->p_auxlock);
		return ESRCH;
	}
}

#endif /* defined(__HAVE_RAS) */

/*ARGSUSED*/
int
sys_rasctl(struct lwp *l, const struct sys_rasctl_args *uap, register_t *retval)
{

#if defined(__HAVE_RAS)
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) op;
	} */
	void *addr;
	size_t len;
	int op;
	int error;

	/*
	 * first, extract syscall args from the uap.
	 */

	addr = (void *)SCARG(uap, addr);
	len = (size_t)SCARG(uap, len);
	op = SCARG(uap, op);

	DPRINTF(("sys_rasctl: p=%p addr=%p, len=%ld, op=0x%x\n",
	    curproc, addr, (long)len, op));

	switch (op) {
	case RAS_INSTALL:
		error = ras_install(addr, len);
		break;
	case RAS_PURGE:
		error = ras_purge(addr, len);
		break;
	case RAS_PURGE_ALL:
		error = ras_purgeall();
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);

#else

	return (EOPNOTSUPP);

#endif

}
