/*	$NetBSD: uvm_emap.c,v 1.9 2012/04/13 15:33:38 yamt Exp $	*/

/*-
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius and Andrew Doran.
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
 * UVM ephemeral mapping interface.
 */

/*
 * Overview:
 *
 * On multiprocessor systems, frequent uses of pmap_kenter_pa/pmap_kremove
 * for ephemeral mappings are not desirable because they likely involve
 * TLB flush IPIs because that pmap_kernel() is shared among all LWPs.
 * This interface can be used instead, to reduce the number of IPIs.
 *
 * For a single-page mapping, PMAP_DIRECT_MAP is likely a better choice
 * if available.  (__HAVE_DIRECT_MAP)
 */

/*
 * How to use:
 *
 * Map pages at the address:
 *
 *	uvm_emap_enter(va, pgs, npages);
 *	gen = uvm_emap_produce();
 *
 * Read pages via the mapping:
 *
 *	uvm_emap_consume(gen);
 *	some_access(va);
 *
 * After finishing using the mapping:
 *
 *	uvm_emap_remove(va, len);
 */

/*
 * Notes for pmap developers:
 *
 * Generic (more expensive) stubs are implemented for architectures which
 * do not support pmap.
 *
 * Note that uvm_emap_update() is called from lower pmap(9) layer, while
 * other functions call to pmap(9).  Typical pattern of update in pmap:
 *
 *	u_int gen = uvm_emap_gen_return();
 *	tlbflush();
 *	uvm_emap_update();
 *
 * It is also used from IPI context, therefore functions must safe.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_emap.c,v 1.9 2012/04/13 15:33:38 yamt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/atomic.h>
#include <sys/lwp.h>
#include <sys/vmem.h>
#include <sys/types.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

/* XXX: Arbitrary. */
#ifdef _LP64
#define	UVM_EMAP_SIZE		(128 * 1024 * 1024)	/* 128 MB */
#else
#define	UVM_EMAP_SIZE		(32 * 1024 * 1024)	/*  32 MB */
#endif

static u_int		_uvm_emap_gen[COHERENCY_UNIT - sizeof(u_int)]
    __aligned(COHERENCY_UNIT);

#define	uvm_emap_gen	(_uvm_emap_gen[0])

u_int			uvm_emap_size = UVM_EMAP_SIZE;
static vaddr_t		uvm_emap_va;
static vmem_t *		uvm_emap_vmem;

/*
 * uvm_emap_init: initialize subsystem.
 */
void
uvm_emap_sysinit(void)
{
	struct uvm_cpu *ucpu;
	size_t qmax;
	u_int i;

	uvm_emap_size = roundup(uvm_emap_size, PAGE_SIZE);
	qmax = 16 * PAGE_SIZE;
#if 0
	uvm_emap_va = uvm_km_alloc(kernel_map, uvm_emap_size, 0,
	    UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	if (uvm_emap_va == 0) {
		panic("uvm_emap_init: KVA allocation failed");
	}

	uvm_emap_vmem = vmem_create("emap", uvm_emap_va, uvm_emap_size,
	    PAGE_SIZE, NULL, NULL, NULL, qmax, VM_SLEEP, IPL_NONE);
	if (uvm_emap_vmem == NULL) {
		panic("uvm_emap_init: vmem creation failed");
	}
#else
	uvm_emap_va = 0;
	uvm_emap_vmem = NULL;
#endif
	/* Initial generation value is 1. */
	uvm_emap_gen = 1;
	for (i = 0; i < maxcpus; i++) {
		ucpu = uvm.cpus[i];
		if (ucpu != NULL) {
			ucpu->emap_gen = 1;
		}
	}
}

/*
 * uvm_emap_alloc: allocate a window.
 */
vaddr_t
uvm_emap_alloc(vsize_t size, bool waitok)
{
	vmem_addr_t addr;

	KASSERT(size > 0);
	KASSERT(round_page(size) == size);

	if (vmem_alloc(uvm_emap_vmem, size,
	    VM_INSTANTFIT | (waitok ? VM_SLEEP : VM_NOSLEEP), &addr) == 0)
		return (vaddr_t)addr;

	return (vaddr_t)0;
}

/*
 * uvm_emap_free: free a window.
 */
void
uvm_emap_free(vaddr_t va, size_t size)
{

	KASSERT(va >= uvm_emap_va);
	KASSERT(size <= uvm_emap_size);
	KASSERT(va + size <= uvm_emap_va + uvm_emap_size);

	vmem_free(uvm_emap_vmem, va, size);
}

#ifdef __HAVE_PMAP_EMAP

/*
 * uvm_emap_enter: enter a new mapping, without TLB flush.
 */
void
uvm_emap_enter(vaddr_t va, struct vm_page **pgs, u_int npages)
{
	paddr_t pa;
	u_int n;

	for (n = 0; n < npages; n++, va += PAGE_SIZE) {
		pa = VM_PAGE_TO_PHYS(pgs[n]);
		pmap_emap_enter(va, pa, VM_PROT_READ);
	}
}

/*
 * uvm_emap_remove: remove a mapping.
 */
void
uvm_emap_remove(vaddr_t sva, vsize_t len)
{

	pmap_emap_remove(sva, len);
}

/*
 * uvm_emap_gen_return: get the global generation number.
 *
 * => can be called from IPI handler, therefore function must be safe.
 */
u_int
uvm_emap_gen_return(void)
{
	u_int gen;

	gen = uvm_emap_gen;
	if (__predict_false(gen == UVM_EMAP_INACTIVE)) {
		/*
		 * Instead of looping, just increase in our side.
		 * Other thread could race and increase it again,
		 * but without any negative effect.
		 */
		gen = atomic_inc_uint_nv(&uvm_emap_gen);
	}
	KASSERT(gen != UVM_EMAP_INACTIVE);
	return gen;
}

/*
 * uvm_emap_switch: if the CPU is 'behind' the LWP in emap visibility,
 * perform TLB flush and thus update the local view.  Main purpose is
 * to handle kernel preemption, while emap is in use.
 *
 * => called from mi_switch(), when LWP returns after block or preempt.
 */
void
uvm_emap_switch(lwp_t *l)
{
	struct uvm_cpu *ucpu;
	u_int curgen, gen;

	KASSERT(kpreempt_disabled());

	/* If LWP did not use emap, then nothing to do. */
	if (__predict_true(l->l_emap_gen == UVM_EMAP_INACTIVE)) {
		return;
	}

	/*
	 * No need to synchronise if generation number of current CPU is
	 * newer than the number of this LWP.
	 *
	 * This test assumes two's complement arithmetic and allows
	 * ~2B missed updates before it will produce bad results.
	 */
	ucpu = curcpu()->ci_data.cpu_uvm;
	curgen = ucpu->emap_gen;
	gen = l->l_emap_gen;
	if (__predict_true((signed int)(curgen - gen) >= 0)) {
		return;
	}

	/*
	 * See comments in uvm_emap_consume() about memory
	 * barriers and race conditions.
	 */
	curgen = uvm_emap_gen_return();
	pmap_emap_sync(false);
	ucpu->emap_gen = curgen;
}

/*
 * uvm_emap_consume: update the current CPU and LWP to the given generation
 * of the emap.  In a case of LWP migration to a different CPU after block
 * or preempt, uvm_emap_switch() will synchronise.
 *
 * => may be called from both interrupt and thread context.
 */
void
uvm_emap_consume(u_int gen)
{
	struct cpu_info *ci;
	struct uvm_cpu *ucpu;
	lwp_t *l = curlwp;
	u_int curgen;

	if (gen == UVM_EMAP_INACTIVE) {
		return;
	}

	/*
	 * No need to synchronise if generation number of current CPU is
	 * newer than the number of this LWP.
	 *
	 * This test assumes two's complement arithmetic and allows
	 * ~2B missed updates before it will produce bad results.
	 */
	KPREEMPT_DISABLE(l);
	ci = l->l_cpu;
	ucpu = ci->ci_data.cpu_uvm;
	if (__predict_true((signed int)(ucpu->emap_gen - gen) >= 0)) {
		l->l_emap_gen = ucpu->emap_gen;
		KPREEMPT_ENABLE(l);
		return;
	}

	/*
	 * Record the current generation _before_ issuing the TLB flush.
	 * No need for a memory barrier before, as reading a stale value
	 * for uvm_emap_gen is not a problem.
	 *
	 * pmap_emap_sync() must implicitly perform a full memory barrier,
	 * which prevents us from fetching a value from after the TLB flush
	 * has occurred (which would be bad).
	 *
	 * We can race with an interrupt on the current CPU updating the
	 * counter to a newer value.  This could cause us to set a stale
	 * value into ucpu->emap_gen, overwriting a newer update from the
	 * interrupt.  However, it does not matter since:
	 *  (1) Interrupts always run to completion or block.
	 *  (2) Interrupts will only ever install a newer value and,
	 *  (3) We will roll the value forward later.
	 */
	curgen = uvm_emap_gen_return();
	pmap_emap_sync(true);
	ucpu->emap_gen = curgen;
	l->l_emap_gen = curgen;
	KASSERT((signed int)(curgen - gen) >= 0);
	KPREEMPT_ENABLE(l);
}

/*
 * uvm_emap_produce: increment emap generation counter.
 *
 * => pmap updates must be globally visible.
 * => caller must have already entered mappings.
 * => may be called from both interrupt and thread context.
 */
u_int
uvm_emap_produce(void)
{
	u_int gen;
again:
	gen = atomic_inc_uint_nv(&uvm_emap_gen);
	if (__predict_false(gen == UVM_EMAP_INACTIVE)) {
		goto again;
	}
	return gen;
}

/*
 * uvm_emap_update: update global emap generation number for current CPU.
 *
 * Function is called by MD code (eg. pmap) to take advantage of TLB flushes
 * initiated for other reasons, that sync the emap as a side effect.  Note
 * update should be performed before the actual TLB flush, to avoid race
 * with newly generated number.
 *
 * => can be called from IPI handler, therefore function must be safe.
 * => should be called _after_ TLB flush.
 * => emap generation number should be taken _before_ TLB flush.
 * => must be called with preemption disabled.
 */
void
uvm_emap_update(u_int gen)
{
	struct uvm_cpu *ucpu;

	/*
	 * See comments in uvm_emap_consume() about memory barriers and
	 * race conditions.  Store is atomic if emap_gen size is word.
	 */
	CTASSERT(sizeof(ucpu->emap_gen) == sizeof(int));
	/* XXX: KASSERT(kpreempt_disabled()); */

	ucpu = curcpu()->ci_data.cpu_uvm;
	ucpu->emap_gen = gen;
}

#else

/*
 * Stubs for architectures which do not support emap.
 */

void
uvm_emap_enter(vaddr_t va, struct vm_page **pgs, u_int npages)
{
	paddr_t pa;
	u_int n;

	for (n = 0; n < npages; n++, va += PAGE_SIZE) {
		pa = VM_PAGE_TO_PHYS(pgs[n]);
		pmap_kenter_pa(va, pa, VM_PROT_READ, 0);
	}
	pmap_update(pmap_kernel());
}

void
uvm_emap_remove(vaddr_t sva, vsize_t len)
{

	pmap_kremove(sva, len);
	pmap_update(pmap_kernel());
}

#endif
