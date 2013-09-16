/*	$NetBSD: uvm_glue.c,v 1.160 2012/09/01 00:26:37 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 * from: Id: uvm_glue.c,v 1.1.2.8 1998/02/07 01:16:54 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_glue.c,v 1.160 2012/09/01 00:26:37 matt Exp $");

#include "opt_kgdb.h"
#include "opt_kstack.h"
#include "opt_uvmhist.h"

/*
 * uvm_glue.c: glue functions
 */

#include <sys/param.h>
#include <sys/kernel.h>

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/buf.h>
#include <sys/syncobj.h>
#include <sys/cpu.h>
#include <sys/atomic.h>
#include <sys/lwp.h>

#include <uvm/uvm.h>

/*
 * uvm_kernacc: test if kernel can access a memory region.
 *
 * => Currently used only by /dev/kmem driver (dev/mm.c).
 */
bool
uvm_kernacc(void *addr, size_t len, vm_prot_t prot)
{
	vaddr_t saddr = trunc_page((vaddr_t)addr);
	vaddr_t eaddr = round_page(saddr + len);
	bool rv;

	vm_map_lock_read(kernel_map);
	rv = uvm_map_checkprot(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);

	return rv;
}

#ifdef KGDB
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 *
 * We force the protection change at the pmap level.  If we were
 * to use vm_map_protect a change to allow writing would be lazily-
 * applied meaning we would still take a protection fault, something
 * we really don't want to do.  It would also fragment the kernel
 * map unnecessarily.  We cannot use pmap_protect since it also won't
 * enforce a write-enable request.  Using pmap_enter is the only way
 * we can ensure the change takes place properly.
 */
void
uvm_chgkprot(void *addr, size_t len, int rw)
{
	vm_prot_t prot;
	paddr_t pa;
	vaddr_t sva, eva;

	prot = rw == B_READ ? VM_PROT_READ : VM_PROT_READ|VM_PROT_WRITE;
	eva = round_page((vaddr_t)addr + len);
	for (sva = trunc_page((vaddr_t)addr); sva < eva; sva += PAGE_SIZE) {
		/*
		 * Extract physical address for the page.
		 */
		if (pmap_extract(pmap_kernel(), sva, &pa) == false)
			panic("%s: invalid page", __func__);
		pmap_enter(pmap_kernel(), sva, pa, prot, PMAP_WIRED);
	}
	pmap_update(pmap_kernel());
}
#endif

/*
 * uvm_vslock: wire user memory for I/O
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

int
uvm_vslock(struct vmspace *vs, void *addr, size_t len, vm_prot_t access_type)
{
	struct vm_map *map;
	vaddr_t start, end;
	int error;

	map = &vs->vm_map;
	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	error = uvm_fault_wire(map, start, end, access_type, 0);
	return error;
}

/*
 * uvm_vsunlock: unwire user memory wired by uvm_vslock()
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

void
uvm_vsunlock(struct vmspace *vs, void *addr, size_t len)
{
	uvm_fault_unwire(&vs->vm_map, trunc_page((vaddr_t)addr),
		round_page((vaddr_t)addr + len));
}

/*
 * uvm_proc_fork: fork a virtual address space
 *
 * - the address space is copied as per parent map's inherit values
 */
void
uvm_proc_fork(struct proc *p1, struct proc *p2, bool shared)
{

	if (shared == true) {
		p2->p_vmspace = NULL;
		uvmspace_share(p1, p2);
	} else {
		p2->p_vmspace = uvmspace_fork(p1->p_vmspace);
	}

	cpu_proc_fork(p1, p2);
}

/*
 * uvm_lwp_fork: fork a thread
 *
 * - a new PCB structure is allocated for the child process,
 *	and filled in by MD layer
 * - if specified, the child gets a new user stack described by
 *	stack and stacksize
 * - NOTE: the kernel stack may be at a different location in the child
 *	process, and thus addresses of automatic variables may be invalid
 *	after cpu_lwp_fork returns in the child process.  We do nothing here
 *	after cpu_lwp_fork returns.
 */
void
uvm_lwp_fork(struct lwp *l1, struct lwp *l2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{

	/* Fill stack with magic number. */
	kstack_setup_magic(l2);

	/*
	 * cpu_lwp_fork() copy and update the pcb, and make the child ready
 	 * to run.  If this is a normal user fork, the child will exit
	 * directly to user mode via child_return() on its first time
	 * slice and will not return here.  If this is a kernel thread,
	 * the specified entry point will be executed.
	 */
	cpu_lwp_fork(l1, l2, stack, stacksize, func, arg);

	/* Inactive emap for new LWP. */
	l2->l_emap_gen = UVM_EMAP_INACTIVE;
}

#ifndef USPACE_ALIGN
#define	USPACE_ALIGN	0
#endif

static pool_cache_t uvm_uarea_cache;
#if defined(__HAVE_CPU_UAREA_ROUTINES)
static pool_cache_t uvm_uarea_system_cache;
#else
#define uvm_uarea_system_cache uvm_uarea_cache
#endif

static void *
uarea_poolpage_alloc(struct pool *pp, int flags)
{
#if defined(PMAP_MAP_POOLPAGE)
	if (USPACE == PAGE_SIZE && USPACE_ALIGN == 0) {
		struct vm_page *pg;
		vaddr_t va;

#if defined(PMAP_ALLOC_POOLPAGE)
		pg = PMAP_ALLOC_POOLPAGE(
		   ((flags & PR_WAITOK) == 0 ? UVM_KMF_NOWAIT : 0));
#else
		pg = uvm_pagealloc(NULL, 0, NULL,
		   ((flags & PR_WAITOK) == 0 ? UVM_KMF_NOWAIT : 0));
#endif
		if (pg == NULL)
			return NULL;
		va = PMAP_MAP_POOLPAGE(VM_PAGE_TO_PHYS(pg));
		if (va == 0)
			uvm_pagefree(pg);
		return (void *)va;
	}
#endif
#if defined(__HAVE_CPU_UAREA_ROUTINES)
	void *va = cpu_uarea_alloc(false);
	if (va)
		return (void *)va;
#endif
	return (void *)uvm_km_alloc(kernel_map, pp->pr_alloc->pa_pagesz,
	    USPACE_ALIGN, UVM_KMF_WIRED |
	    ((flags & PR_WAITOK) ? UVM_KMF_WAITVA :
	    (UVM_KMF_NOWAIT | UVM_KMF_TRYLOCK)));
}

static void
uarea_poolpage_free(struct pool *pp, void *addr)
{
#if defined(PMAP_MAP_POOLPAGE)
	if (USPACE == PAGE_SIZE && USPACE_ALIGN == 0) {
		paddr_t pa;

		pa = PMAP_UNMAP_POOLPAGE((vaddr_t) addr);
		KASSERT(pa != 0);
		uvm_pagefree(PHYS_TO_VM_PAGE(pa));
		return;
	}
#endif
#if defined(__HAVE_CPU_UAREA_ROUTINES)
	if (cpu_uarea_free(addr))
		return;
#endif
	uvm_km_free(kernel_map, (vaddr_t)addr, pp->pr_alloc->pa_pagesz,
	    UVM_KMF_WIRED);
}

static struct pool_allocator uvm_uarea_allocator = {
	.pa_alloc = uarea_poolpage_alloc,
	.pa_free = uarea_poolpage_free,
	.pa_pagesz = USPACE,
};

#if defined(__HAVE_CPU_UAREA_ROUTINES)
static void *
uarea_system_poolpage_alloc(struct pool *pp, int flags)
{
	void * const va = cpu_uarea_alloc(true);
	if (va != NULL)
		return va;

	return (void *)uvm_km_alloc(kernel_map, pp->pr_alloc->pa_pagesz,
	    USPACE_ALIGN, UVM_KMF_WIRED |
	    ((flags & PR_WAITOK) ? UVM_KMF_WAITVA :
	    (UVM_KMF_NOWAIT | UVM_KMF_TRYLOCK)));
}

static void
uarea_system_poolpage_free(struct pool *pp, void *addr)
{
	if (cpu_uarea_free(addr))
		return;

	uvm_km_free(kernel_map, (vaddr_t)addr, pp->pr_alloc->pa_pagesz,
	    UVM_KMF_WIRED);
}

static struct pool_allocator uvm_uarea_system_allocator = {
	.pa_alloc = uarea_system_poolpage_alloc,
	.pa_free = uarea_system_poolpage_free,
	.pa_pagesz = USPACE,
};
#endif /* __HAVE_CPU_UAREA_ROUTINES */

void
uvm_uarea_init(void)
{
	int flags = PR_NOTOUCH;

	/*
	 * specify PR_NOALIGN unless the alignment provided by
	 * the backend (USPACE_ALIGN) is sufficient to provide
	 * pool page size (UPSACE) alignment.
	 */

	if ((USPACE_ALIGN == 0 && USPACE != PAGE_SIZE) ||
	    (USPACE_ALIGN % USPACE) != 0) {
		flags |= PR_NOALIGN;
	}

	uvm_uarea_cache = pool_cache_init(USPACE, USPACE_ALIGN, 0, flags,
	    "uarea", &uvm_uarea_allocator, IPL_NONE, NULL, NULL, NULL);
#if defined(__HAVE_CPU_UAREA_ROUTINES)
	uvm_uarea_system_cache = pool_cache_init(USPACE, USPACE_ALIGN,
	    0, flags, "uareasys", &uvm_uarea_system_allocator,
	    IPL_NONE, NULL, NULL, NULL);
#endif
}

/*
 * uvm_uarea_alloc: allocate a u-area
 */

vaddr_t
uvm_uarea_alloc(void)
{

	return (vaddr_t)pool_cache_get(uvm_uarea_cache, PR_WAITOK);
}

vaddr_t
uvm_uarea_system_alloc(struct cpu_info *ci)
{
#ifdef __HAVE_CPU_UAREA_ALLOC_IDLELWP
	if (__predict_false(ci != NULL))
		return cpu_uarea_alloc_idlelwp(ci);
#endif

	return (vaddr_t)pool_cache_get(uvm_uarea_system_cache, PR_WAITOK);
}

/*
 * uvm_uarea_free: free a u-area
 */

void
uvm_uarea_free(vaddr_t uaddr)
{

	pool_cache_put(uvm_uarea_cache, (void *)uaddr);
}

void
uvm_uarea_system_free(vaddr_t uaddr)
{

	pool_cache_put(uvm_uarea_system_cache, (void *)uaddr);
}

vaddr_t
uvm_lwp_getuarea(lwp_t *l)
{

	return (vaddr_t)l->l_addr - UAREA_PCB_OFFSET;
}

void
uvm_lwp_setuarea(lwp_t *l, vaddr_t addr)
{

	l->l_addr = (void *)(addr + UAREA_PCB_OFFSET);
}

/*
 * uvm_proc_exit: exit a virtual address space
 *
 * - borrow proc0's address space because freeing the vmspace
 *   of the dead process may block.
 */

void
uvm_proc_exit(struct proc *p)
{
	struct lwp *l = curlwp; /* XXX */
	struct vmspace *ovm;

	KASSERT(p == l->l_proc);
	ovm = p->p_vmspace;
	KASSERT(ovm != NULL);

	if (__predict_false(ovm == proc0.p_vmspace))
		return;

	/*
	 * borrow proc0's address space.
	 */
	KPREEMPT_DISABLE(l);
	pmap_deactivate(l);
	p->p_vmspace = proc0.p_vmspace;
	pmap_activate(l);
	KPREEMPT_ENABLE(l);

	uvmspace_free(ovm);
}

void
uvm_lwp_exit(struct lwp *l)
{
	vaddr_t va = uvm_lwp_getuarea(l);
	bool system = (l->l_flag & LW_SYSTEM) != 0;

	if (system)
		uvm_uarea_system_free(va);
	else
		uvm_uarea_free(va);
#ifdef DIAGNOSTIC
	uvm_lwp_setuarea(l, (vaddr_t)NULL);
#endif
}

/*
 * uvm_init_limit: init per-process VM limits
 *
 * - called for process 0 and then inherited by all others.
 */

void
uvm_init_limits(struct proc *p)
{

	/*
	 * Set up the initial limits on process VM.  Set the maximum
	 * resident set size to be all of (reasonably) available memory.
	 * This causes any single, large process to start random page
	 * replacement once it fills memory.
	 */

	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = maxsmap;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = maxdmap;
	p->p_rlimit[RLIMIT_AS].rlim_cur = RLIM_INFINITY;
	p->p_rlimit[RLIMIT_AS].rlim_max = RLIM_INFINITY;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = MIN(
	    VM_MAXUSER_ADDRESS, ctob((rlim_t)uvmexp.free));
}

/*
 * uvm_scheduler: process zero main loop.
 */

extern struct loadavg averunnable;

void
uvm_scheduler(void)
{
	lwp_t *l = curlwp;

	lwp_lock(l);
	l->l_priority = PRI_VM;
	l->l_class = SCHED_FIFO;
	lwp_unlock(l);

	for (;;) {
		sched_pstats();
		(void)kpause("uvm", false, hz, NULL);
	}
}
