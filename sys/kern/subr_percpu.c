/*	$NetBSD: subr_percpu.c,v 1.17 2014/11/27 15:00:00 uebayasi Exp $	*/

/*-
 * Copyright (c)2007,2008 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * per-cpu storage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_percpu.c,v 1.17 2014/11/27 15:00:00 uebayasi Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/percpu.h>
#include <sys/rwlock.h>
#include <sys/vmem.h>
#include <sys/xcall.h>

#define	PERCPU_QUANTUM_SIZE	(ALIGNBYTES + 1)
#define	PERCPU_QCACHE_MAX	0
#define	PERCPU_IMPORT_SIZE	2048

#if defined(DIAGNOSTIC)
#define	MAGIC	0x50435055	/* "PCPU" */
#define	percpu_encrypt(pc)	((pc) ^ MAGIC)
#define	percpu_decrypt(pc)	((pc) ^ MAGIC)
#else /* defined(DIAGNOSTIC) */
#define	percpu_encrypt(pc)	(pc)
#define	percpu_decrypt(pc)	(pc)
#endif /* defined(DIAGNOSTIC) */

static krwlock_t	percpu_swap_lock	__cacheline_aligned;
static kmutex_t		percpu_allocation_lock	__cacheline_aligned;
static vmem_t *		percpu_offset_arena	__cacheline_aligned;
static unsigned int	percpu_nextoff		__cacheline_aligned;

static percpu_cpu_t *
cpu_percpu(struct cpu_info *ci)
{

	return &ci->ci_data.cpu_percpu;
}

static unsigned int
percpu_offset(percpu_t *pc)
{
	const unsigned int off = percpu_decrypt((uintptr_t)pc);

	KASSERT(off < percpu_nextoff);
	return off;
}

/*
 * percpu_cpu_swap: crosscall handler for percpu_cpu_enlarge
 */

static void
percpu_cpu_swap(void *p1, void *p2)
{
	struct cpu_info * const ci = p1;
	percpu_cpu_t * const newpcc = p2;
	percpu_cpu_t * const pcc = cpu_percpu(ci);

	KASSERT(ci == curcpu() || !mp_online);

	/*
	 * swap *pcc and *newpcc unless anyone has beaten us.
	 */
	rw_enter(&percpu_swap_lock, RW_WRITER);
	if (newpcc->pcc_size > pcc->pcc_size) {
		percpu_cpu_t tmp;
		int s;

		tmp = *pcc;

		/*
		 * block interrupts so that we don't lose their modifications.
		 */

		s = splhigh();

		/*
		 * copy data to new storage.
		 */

		memcpy(newpcc->pcc_data, pcc->pcc_data, pcc->pcc_size);

		/*
		 * this assignment needs to be atomic for percpu_getptr_remote.
		 */

		pcc->pcc_data = newpcc->pcc_data;

		splx(s);

		pcc->pcc_size = newpcc->pcc_size;
		*newpcc = tmp;
	}
	rw_exit(&percpu_swap_lock);
}

/*
 * percpu_cpu_enlarge: ensure that percpu_cpu_t of each cpus have enough space
 */

static void
percpu_cpu_enlarge(size_t size)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	for (CPU_INFO_FOREACH(cii, ci)) {
		percpu_cpu_t pcc;

		pcc.pcc_data = kmem_alloc(size, KM_SLEEP); /* XXX cacheline */
		pcc.pcc_size = size;
		if (!mp_online) {
			percpu_cpu_swap(ci, &pcc);
		} else {
			uint64_t where;

			where = xc_unicast(0, percpu_cpu_swap, ci, &pcc, ci);
			xc_wait(where);
		}
		KASSERT(pcc.pcc_size < size);
		if (pcc.pcc_data != NULL) {
			kmem_free(pcc.pcc_data, pcc.pcc_size);
		}
	}
}

/*
 * percpu_backend_alloc: vmem import callback for percpu_offset_arena
 */

static int
percpu_backend_alloc(vmem_t *dummy, vmem_size_t size, vmem_size_t *resultsize,
    vm_flag_t vmflags, vmem_addr_t *addrp)
{
	unsigned int offset;
	unsigned int nextoff;

	ASSERT_SLEEPABLE();
	KASSERT(dummy == NULL);

	if ((vmflags & VM_NOSLEEP) != 0)
		return ENOMEM;

	size = roundup(size, PERCPU_IMPORT_SIZE);
	mutex_enter(&percpu_allocation_lock);
	offset = percpu_nextoff;
	percpu_nextoff = nextoff = percpu_nextoff + size;
	mutex_exit(&percpu_allocation_lock);

	percpu_cpu_enlarge(nextoff);

	*resultsize = size;
	*addrp = (vmem_addr_t)offset;
	return 0;
}

static void
percpu_zero_cb(void *vp, void *vp2, struct cpu_info *ci)
{
	size_t sz = (uintptr_t)vp2;

	memset(vp, 0, sz);
}

/*
 * percpu_zero: initialize percpu storage with zero.
 */

static void
percpu_zero(percpu_t *pc, size_t sz)
{

	percpu_foreach(pc, percpu_zero_cb, (void *)(uintptr_t)sz);
}

/*
 * percpu_init: subsystem initialization
 */

void
percpu_init(void)
{

	ASSERT_SLEEPABLE();
	rw_init(&percpu_swap_lock);
	mutex_init(&percpu_allocation_lock, MUTEX_DEFAULT, IPL_NONE);
	percpu_nextoff = PERCPU_QUANTUM_SIZE;

	percpu_offset_arena = vmem_xcreate("percpu", 0, 0, PERCPU_QUANTUM_SIZE,
	    percpu_backend_alloc, NULL, NULL, PERCPU_QCACHE_MAX, VM_SLEEP,
	    IPL_NONE);
}

/*
 * percpu_init_cpu: cpu initialization
 *
 * => should be called before the cpu appears on the list for CPU_INFO_FOREACH.
 */

void
percpu_init_cpu(struct cpu_info *ci)
{
	percpu_cpu_t * const pcc = cpu_percpu(ci);
	size_t size = percpu_nextoff; /* XXX racy */

	ASSERT_SLEEPABLE();
	pcc->pcc_size = size;
	if (size) {
		pcc->pcc_data = kmem_zalloc(pcc->pcc_size, KM_SLEEP);
	}
}

/*
 * percpu_alloc: allocate percpu storage
 *
 * => called in thread context.
 * => considered as an expensive and rare operation.
 * => allocated storage is initialized with zeros.
 */

percpu_t *
percpu_alloc(size_t size)
{
	vmem_addr_t offset;
	percpu_t *pc;

	ASSERT_SLEEPABLE();
	if (vmem_alloc(percpu_offset_arena, size, VM_SLEEP | VM_BESTFIT,
	    &offset) != 0)
		return NULL;
	pc = (percpu_t *)percpu_encrypt((uintptr_t)offset);
	percpu_zero(pc, size);
	return pc;
}

/*
 * percpu_free: free percpu storage
 *
 * => called in thread context.
 * => considered as an expensive and rare operation.
 */

void
percpu_free(percpu_t *pc, size_t size)
{

	ASSERT_SLEEPABLE();
	vmem_free(percpu_offset_arena, (vmem_addr_t)percpu_offset(pc), size);
}

/*
 * percpu_getref:
 *
 * => safe to be used in either thread or interrupt context
 * => disables preemption; must be bracketed with a percpu_putref()
 */

void *
percpu_getref(percpu_t *pc)
{

	kpreempt_disable();
	return percpu_getptr_remote(pc, curcpu());
}

/*
 * percpu_putref:
 *
 * => drops the preemption-disabled count after caller is done with per-cpu
 *    data
 */

void
percpu_putref(percpu_t *pc)
{

	kpreempt_enable();
}

/*
 * percpu_traverse_enter, percpu_traverse_exit, percpu_getptr_remote:
 * helpers to access remote cpu's percpu data.
 *
 * => called in thread context.
 * => percpu_traverse_enter can block low-priority xcalls.
 * => typical usage would be:
 *
 *	sum = 0;
 *	percpu_traverse_enter();
 *	for (CPU_INFO_FOREACH(cii, ci)) {
 *		unsigned int *p = percpu_getptr_remote(pc, ci);
 *		sum += *p;
 *	}
 *	percpu_traverse_exit();
 */

void
percpu_traverse_enter(void)
{

	ASSERT_SLEEPABLE();
	rw_enter(&percpu_swap_lock, RW_READER);
}

void
percpu_traverse_exit(void)
{

	rw_exit(&percpu_swap_lock);
}

void *
percpu_getptr_remote(percpu_t *pc, struct cpu_info *ci)
{

	return &((char *)cpu_percpu(ci)->pcc_data)[percpu_offset(pc)];
}

/*
 * percpu_foreach: call the specified callback function for each cpus.
 *
 * => called in thread context.
 * => caller should not rely on the cpu iteration order.
 * => the callback function should be minimum because it is executed with
 *    holding a global lock, which can block low-priority xcalls.
 *    eg. it's illegal for a callback function to sleep for memory allocation.
 */
void
percpu_foreach(percpu_t *pc, percpu_callback_t cb, void *arg)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	percpu_traverse_enter();
	for (CPU_INFO_FOREACH(cii, ci)) {
		(*cb)(percpu_getptr_remote(pc, ci), arg, ci);
	}
	percpu_traverse_exit();
}
