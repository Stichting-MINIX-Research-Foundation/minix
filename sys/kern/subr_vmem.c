/*	$NetBSD: subr_vmem.c,v 1.93 2015/08/24 22:50:32 pooka Exp $	*/

/*-
 * Copyright (c)2006,2007,2008,2009 YAMAMOTO Takashi,
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
 * reference:
 * -	Magazines and Vmem: Extending the Slab Allocator
 *	to Many CPUs and Arbitrary Resources
 *	http://www.usenix.org/event/usenix01/bonwick.html
 *
 * locking & the boundary tag pool:
 * - 	A pool(9) is used for vmem boundary tags
 * - 	During a pool get call the global vmem_btag_refill_lock is taken,
 *	to serialize access to the allocation reserve, but no other
 *	vmem arena locks.
 * -	During pool_put calls no vmem mutexes are locked.
 * - 	pool_drain doesn't hold the pool's mutex while releasing memory to
 * 	its backing therefore no interferance with any vmem mutexes.
 * -	The boundary tag pool is forced to put page headers into pool pages
 *  	(PR_PHINPAGE) and not off page to avoid pool recursion.
 *  	(due to sizeof(bt_t) it should be the case anyway)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_vmem.c,v 1.93 2015/08/24 22:50:32 pooka Exp $");

#if defined(_KERNEL) && defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif /* defined(_KERNEL) && defined(_KERNEL_OPT) */

#include <sys/param.h>
#include <sys/hash.h>
#include <sys/queue.h>
#include <sys/bitops.h>

#if defined(_KERNEL)
#include <sys/systm.h>
#include <sys/kernel.h>	/* hz */
#include <sys/callout.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/vmem.h>
#include <sys/vmem_impl.h>
#include <sys/workqueue.h>
#include <sys/atomic.h>
#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>
#include <uvm/uvm_km.h>
#include <uvm/uvm_page.h>
#include <uvm/uvm_pdaemon.h>
#else /* defined(_KERNEL) */
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../sys/vmem.h"
#include "../sys/vmem_impl.h"
#endif /* defined(_KERNEL) */


#if defined(_KERNEL)
#include <sys/evcnt.h>
#define VMEM_EVCNT_DEFINE(name) \
struct evcnt vmem_evcnt_##name = EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, \
    "vmem", #name); \
EVCNT_ATTACH_STATIC(vmem_evcnt_##name);
#define VMEM_EVCNT_INCR(ev)	vmem_evcnt_##ev.ev_count++
#define VMEM_EVCNT_DECR(ev)	vmem_evcnt_##ev.ev_count--

VMEM_EVCNT_DEFINE(static_bt_count)
VMEM_EVCNT_DEFINE(static_bt_inuse)

#define	VMEM_CONDVAR_INIT(vm, wchan)	cv_init(&vm->vm_cv, wchan)
#define	VMEM_CONDVAR_DESTROY(vm)	cv_destroy(&vm->vm_cv)
#define	VMEM_CONDVAR_WAIT(vm)		cv_wait(&vm->vm_cv, &vm->vm_lock)
#define	VMEM_CONDVAR_BROADCAST(vm)	cv_broadcast(&vm->vm_cv)

#else /* defined(_KERNEL) */

#define VMEM_EVCNT_INCR(ev)	/* nothing */
#define VMEM_EVCNT_DECR(ev)	/* nothing */

#define	VMEM_CONDVAR_INIT(vm, wchan)	/* nothing */
#define	VMEM_CONDVAR_DESTROY(vm)	/* nothing */
#define	VMEM_CONDVAR_WAIT(vm)		/* nothing */
#define	VMEM_CONDVAR_BROADCAST(vm)	/* nothing */

#define	UNITTEST
#define	KASSERT(a)		assert(a)
#define	mutex_init(a, b, c)	/* nothing */
#define	mutex_destroy(a)	/* nothing */
#define	mutex_enter(a)		/* nothing */
#define	mutex_tryenter(a)	true
#define	mutex_exit(a)		/* nothing */
#define	mutex_owned(a)		/* nothing */
#define	ASSERT_SLEEPABLE()	/* nothing */
#define	panic(...)		printf(__VA_ARGS__); abort()
#endif /* defined(_KERNEL) */

#if defined(VMEM_SANITY)
static void vmem_check(vmem_t *);
#else /* defined(VMEM_SANITY) */
#define vmem_check(vm)	/* nothing */
#endif /* defined(VMEM_SANITY) */

#define	VMEM_HASHSIZE_MIN	1	/* XXX */
#define	VMEM_HASHSIZE_MAX	65536	/* XXX */
#define	VMEM_HASHSIZE_INIT	1

#define	VM_FITMASK	(VM_BESTFIT | VM_INSTANTFIT)

#if defined(_KERNEL)
static bool vmem_bootstrapped = false;
static kmutex_t vmem_list_lock;
static LIST_HEAD(, vmem) vmem_list = LIST_HEAD_INITIALIZER(vmem_list);
#endif /* defined(_KERNEL) */

/* ---- misc */

#define	VMEM_LOCK(vm)		mutex_enter(&vm->vm_lock)
#define	VMEM_TRYLOCK(vm)	mutex_tryenter(&vm->vm_lock)
#define	VMEM_UNLOCK(vm)		mutex_exit(&vm->vm_lock)
#define	VMEM_LOCK_INIT(vm, ipl)	mutex_init(&vm->vm_lock, MUTEX_DEFAULT, ipl)
#define	VMEM_LOCK_DESTROY(vm)	mutex_destroy(&vm->vm_lock)
#define	VMEM_ASSERT_LOCKED(vm)	KASSERT(mutex_owned(&vm->vm_lock))

#define	VMEM_ALIGNUP(addr, align) \
	(-(-(addr) & -(align)))

#define	VMEM_CROSS_P(addr1, addr2, boundary) \
	((((addr1) ^ (addr2)) & -(boundary)) != 0)

#define	ORDER2SIZE(order)	((vmem_size_t)1 << (order))
#define	SIZE2ORDER(size)	((int)ilog2(size))

#if !defined(_KERNEL)
#define	xmalloc(sz, flags)	malloc(sz)
#define	xfree(p, sz)		free(p)
#define	bt_alloc(vm, flags)	malloc(sizeof(bt_t))
#define	bt_free(vm, bt)		free(bt)
#else /* defined(_KERNEL) */

#define	xmalloc(sz, flags) \
    kmem_alloc(sz, ((flags) & VM_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
#define	xfree(p, sz)		kmem_free(p, sz);

/*
 * BT_RESERVE calculation:
 * we allocate memory for boundry tags with vmem, therefor we have
 * to keep a reserve of bts used to allocated memory for bts. 
 * This reserve is 4 for each arena involved in allocating vmems memory.
 * BT_MAXFREE: don't cache excessive counts of bts in arenas
 */
#define STATIC_BT_COUNT 200
#define BT_MINRESERVE 4
#define BT_MAXFREE 64

static struct vmem_btag static_bts[STATIC_BT_COUNT];
static int static_bt_count = STATIC_BT_COUNT;

static struct vmem kmem_va_meta_arena_store;
vmem_t *kmem_va_meta_arena;
static struct vmem kmem_meta_arena_store;
vmem_t *kmem_meta_arena = NULL;

static kmutex_t vmem_btag_refill_lock;
static kmutex_t vmem_btag_lock;
static LIST_HEAD(, vmem_btag) vmem_btag_freelist;
static size_t vmem_btag_freelist_count = 0;
static struct pool vmem_btag_pool;

/* ---- boundary tag */

static int bt_refill(vmem_t *vm, vm_flag_t flags);

static void *
pool_page_alloc_vmem_meta(struct pool *pp, int flags)
{
	const vm_flag_t vflags = (flags & PR_WAITOK) ? VM_SLEEP: VM_NOSLEEP;
	vmem_addr_t va;
	int ret;

	ret = vmem_alloc(kmem_meta_arena, pp->pr_alloc->pa_pagesz,
	    (vflags & ~VM_FITMASK) | VM_INSTANTFIT | VM_POPULATING, &va);

	return ret ? NULL : (void *)va;
}

static void
pool_page_free_vmem_meta(struct pool *pp, void *v)
{

	vmem_free(kmem_meta_arena, (vmem_addr_t)v, pp->pr_alloc->pa_pagesz);
}

/* allocator for vmem-pool metadata */
struct pool_allocator pool_allocator_vmem_meta = {
	.pa_alloc = pool_page_alloc_vmem_meta,
	.pa_free = pool_page_free_vmem_meta,
	.pa_pagesz = 0
};

static int
bt_refill(vmem_t *vm, vm_flag_t flags)
{
	bt_t *bt;

	KASSERT(flags & VM_NOSLEEP);

	VMEM_LOCK(vm);
	if (vm->vm_nfreetags > BT_MINRESERVE) {
		VMEM_UNLOCK(vm);
		return 0;
	}

	mutex_enter(&vmem_btag_lock);
	while (!LIST_EMPTY(&vmem_btag_freelist) &&
	    vm->vm_nfreetags <= BT_MINRESERVE) {
		bt = LIST_FIRST(&vmem_btag_freelist);
		LIST_REMOVE(bt, bt_freelist);
		LIST_INSERT_HEAD(&vm->vm_freetags, bt, bt_freelist);
		vm->vm_nfreetags++;
		vmem_btag_freelist_count--;
		VMEM_EVCNT_INCR(static_bt_inuse);
	}
	mutex_exit(&vmem_btag_lock);

	while (vm->vm_nfreetags <= BT_MINRESERVE) {
		VMEM_UNLOCK(vm);
		mutex_enter(&vmem_btag_refill_lock);
		bt = pool_get(&vmem_btag_pool, PR_NOWAIT);
		mutex_exit(&vmem_btag_refill_lock);
		VMEM_LOCK(vm);
		if (bt == NULL)
			break;
		LIST_INSERT_HEAD(&vm->vm_freetags, bt, bt_freelist);
		vm->vm_nfreetags++;
	}

	if (vm->vm_nfreetags <= BT_MINRESERVE) {
		VMEM_UNLOCK(vm);
		return ENOMEM;
	}

	VMEM_UNLOCK(vm);

	if (kmem_meta_arena != NULL) {
		bt_refill(kmem_arena, (flags & ~VM_FITMASK)
		    | VM_INSTANTFIT | VM_POPULATING);
		bt_refill(kmem_va_meta_arena, (flags & ~VM_FITMASK)
		    | VM_INSTANTFIT | VM_POPULATING);
		bt_refill(kmem_meta_arena, (flags & ~VM_FITMASK)
		    | VM_INSTANTFIT | VM_POPULATING);
	}

	return 0;
}

static bt_t *
bt_alloc(vmem_t *vm, vm_flag_t flags)
{
	bt_t *bt;
	VMEM_LOCK(vm);
	while (vm->vm_nfreetags <= BT_MINRESERVE && (flags & VM_POPULATING) == 0) {
		VMEM_UNLOCK(vm);
		if (bt_refill(vm, VM_NOSLEEP | VM_INSTANTFIT)) {
			return NULL;
		}
		VMEM_LOCK(vm);
	}
	bt = LIST_FIRST(&vm->vm_freetags);
	LIST_REMOVE(bt, bt_freelist);
	vm->vm_nfreetags--;
	VMEM_UNLOCK(vm);

	return bt;
}

static void
bt_free(vmem_t *vm, bt_t *bt)
{

	VMEM_LOCK(vm);
	LIST_INSERT_HEAD(&vm->vm_freetags, bt, bt_freelist);
	vm->vm_nfreetags++;
	VMEM_UNLOCK(vm);
}

static void
bt_freetrim(vmem_t *vm, int freelimit)
{
	bt_t *t;
	LIST_HEAD(, vmem_btag) tofree;

	LIST_INIT(&tofree);

	VMEM_LOCK(vm);
	while (vm->vm_nfreetags > freelimit) {
		bt_t *bt = LIST_FIRST(&vm->vm_freetags);
		LIST_REMOVE(bt, bt_freelist);
		vm->vm_nfreetags--;
		if (bt >= static_bts
		    && bt < &static_bts[STATIC_BT_COUNT]) {
			mutex_enter(&vmem_btag_lock);
			LIST_INSERT_HEAD(&vmem_btag_freelist, bt, bt_freelist);
			vmem_btag_freelist_count++;
			mutex_exit(&vmem_btag_lock);
			VMEM_EVCNT_DECR(static_bt_inuse);
		} else {
			LIST_INSERT_HEAD(&tofree, bt, bt_freelist);
		}
	}

	VMEM_UNLOCK(vm);
	while (!LIST_EMPTY(&tofree)) {
		t = LIST_FIRST(&tofree);
		LIST_REMOVE(t, bt_freelist);
		pool_put(&vmem_btag_pool, t);
	}
}
#endif	/* defined(_KERNEL) */

/*
 * freelist[0] ... [1, 1]
 * freelist[1] ... [2, 3]
 * freelist[2] ... [4, 7]
 * freelist[3] ... [8, 15]
 *  :
 * freelist[n] ... [(1 << n), (1 << (n + 1)) - 1]
 *  :
 */

static struct vmem_freelist *
bt_freehead_tofree(vmem_t *vm, vmem_size_t size)
{
	const vmem_size_t qsize = size >> vm->vm_quantum_shift;
	const int idx = SIZE2ORDER(qsize);

	KASSERT(size != 0 && qsize != 0);
	KASSERT((size & vm->vm_quantum_mask) == 0);
	KASSERT(idx >= 0);
	KASSERT(idx < VMEM_MAXORDER);

	return &vm->vm_freelist[idx];
}

/*
 * bt_freehead_toalloc: return the freelist for the given size and allocation
 * strategy.
 *
 * for VM_INSTANTFIT, return the list in which any blocks are large enough
 * for the requested size.  otherwise, return the list which can have blocks
 * large enough for the requested size.
 */

static struct vmem_freelist *
bt_freehead_toalloc(vmem_t *vm, vmem_size_t size, vm_flag_t strat)
{
	const vmem_size_t qsize = size >> vm->vm_quantum_shift;
	int idx = SIZE2ORDER(qsize);

	KASSERT(size != 0 && qsize != 0);
	KASSERT((size & vm->vm_quantum_mask) == 0);

	if (strat == VM_INSTANTFIT && ORDER2SIZE(idx) != qsize) {
		idx++;
		/* check too large request? */
	}
	KASSERT(idx >= 0);
	KASSERT(idx < VMEM_MAXORDER);

	return &vm->vm_freelist[idx];
}

/* ---- boundary tag hash */

static struct vmem_hashlist *
bt_hashhead(vmem_t *vm, vmem_addr_t addr)
{
	struct vmem_hashlist *list;
	unsigned int hash;

	hash = hash32_buf(&addr, sizeof(addr), HASH32_BUF_INIT);
	list = &vm->vm_hashlist[hash % vm->vm_hashsize];

	return list;
}

static bt_t *
bt_lookupbusy(vmem_t *vm, vmem_addr_t addr)
{
	struct vmem_hashlist *list;
	bt_t *bt;

	list = bt_hashhead(vm, addr); 
	LIST_FOREACH(bt, list, bt_hashlist) {
		if (bt->bt_start == addr) {
			break;
		}
	}

	return bt;
}

static void
bt_rembusy(vmem_t *vm, bt_t *bt)
{

	KASSERT(vm->vm_nbusytag > 0);
	vm->vm_inuse -= bt->bt_size;
	vm->vm_nbusytag--;
	LIST_REMOVE(bt, bt_hashlist);
}

static void
bt_insbusy(vmem_t *vm, bt_t *bt)
{
	struct vmem_hashlist *list;

	KASSERT(bt->bt_type == BT_TYPE_BUSY);

	list = bt_hashhead(vm, bt->bt_start);
	LIST_INSERT_HEAD(list, bt, bt_hashlist);
	vm->vm_nbusytag++;
	vm->vm_inuse += bt->bt_size;
}

/* ---- boundary tag list */

static void
bt_remseg(vmem_t *vm, bt_t *bt)
{

	TAILQ_REMOVE(&vm->vm_seglist, bt, bt_seglist);
}

static void
bt_insseg(vmem_t *vm, bt_t *bt, bt_t *prev)
{

	TAILQ_INSERT_AFTER(&vm->vm_seglist, prev, bt, bt_seglist);
}

static void
bt_insseg_tail(vmem_t *vm, bt_t *bt)
{

	TAILQ_INSERT_TAIL(&vm->vm_seglist, bt, bt_seglist);
}

static void
bt_remfree(vmem_t *vm, bt_t *bt)
{

	KASSERT(bt->bt_type == BT_TYPE_FREE);

	LIST_REMOVE(bt, bt_freelist);
}

static void
bt_insfree(vmem_t *vm, bt_t *bt)
{
	struct vmem_freelist *list;

	list = bt_freehead_tofree(vm, bt->bt_size);
	LIST_INSERT_HEAD(list, bt, bt_freelist);
}

/* ---- vmem internal functions */

#if defined(QCACHE)
static inline vm_flag_t
prf_to_vmf(int prflags)
{
	vm_flag_t vmflags;

	KASSERT((prflags & ~(PR_LIMITFAIL | PR_WAITOK | PR_NOWAIT)) == 0);
	if ((prflags & PR_WAITOK) != 0) {
		vmflags = VM_SLEEP;
	} else {
		vmflags = VM_NOSLEEP;
	}
	return vmflags;
}

static inline int
vmf_to_prf(vm_flag_t vmflags)
{
	int prflags;

	if ((vmflags & VM_SLEEP) != 0) {
		prflags = PR_WAITOK;
	} else {
		prflags = PR_NOWAIT;
	}
	return prflags;
}

static size_t
qc_poolpage_size(size_t qcache_max)
{
	int i;

	for (i = 0; ORDER2SIZE(i) <= qcache_max * 3; i++) {
		/* nothing */
	}
	return ORDER2SIZE(i);
}

static void *
qc_poolpage_alloc(struct pool *pool, int prflags)
{
	qcache_t *qc = QC_POOL_TO_QCACHE(pool);
	vmem_t *vm = qc->qc_vmem;
	vmem_addr_t addr;

	if (vmem_alloc(vm, pool->pr_alloc->pa_pagesz,
	    prf_to_vmf(prflags) | VM_INSTANTFIT, &addr) != 0)
		return NULL;
	return (void *)addr;
}

static void
qc_poolpage_free(struct pool *pool, void *addr)
{
	qcache_t *qc = QC_POOL_TO_QCACHE(pool);
	vmem_t *vm = qc->qc_vmem;

	vmem_free(vm, (vmem_addr_t)addr, pool->pr_alloc->pa_pagesz);
}

static void
qc_init(vmem_t *vm, size_t qcache_max, int ipl)
{
	qcache_t *prevqc;
	struct pool_allocator *pa;
	int qcache_idx_max;
	int i;

	KASSERT((qcache_max & vm->vm_quantum_mask) == 0);
	if (qcache_max > (VMEM_QCACHE_IDX_MAX << vm->vm_quantum_shift)) {
		qcache_max = VMEM_QCACHE_IDX_MAX << vm->vm_quantum_shift;
	}
	vm->vm_qcache_max = qcache_max;
	pa = &vm->vm_qcache_allocator;
	memset(pa, 0, sizeof(*pa));
	pa->pa_alloc = qc_poolpage_alloc;
	pa->pa_free = qc_poolpage_free;
	pa->pa_pagesz = qc_poolpage_size(qcache_max);

	qcache_idx_max = qcache_max >> vm->vm_quantum_shift;
	prevqc = NULL;
	for (i = qcache_idx_max; i > 0; i--) {
		qcache_t *qc = &vm->vm_qcache_store[i - 1];
		size_t size = i << vm->vm_quantum_shift;
		pool_cache_t pc;

		qc->qc_vmem = vm;
		snprintf(qc->qc_name, sizeof(qc->qc_name), "%s-%zu",
		    vm->vm_name, size);

		pc = pool_cache_init(size,
		    ORDER2SIZE(vm->vm_quantum_shift), 0,
		    PR_NOALIGN | PR_NOTOUCH | PR_RECURSIVE /* XXX */,
		    qc->qc_name, pa, ipl, NULL, NULL, NULL);

		KASSERT(pc);

		qc->qc_cache = pc;
		KASSERT(qc->qc_cache != NULL);	/* XXX */
		if (prevqc != NULL &&
		    qc->qc_cache->pc_pool.pr_itemsperpage ==
		    prevqc->qc_cache->pc_pool.pr_itemsperpage) {
			pool_cache_destroy(qc->qc_cache);
			vm->vm_qcache[i - 1] = prevqc;
			continue;
		}
		qc->qc_cache->pc_pool.pr_qcache = qc;
		vm->vm_qcache[i - 1] = qc;
		prevqc = qc;
	}
}

static void
qc_destroy(vmem_t *vm)
{
	const qcache_t *prevqc;
	int i;
	int qcache_idx_max;

	qcache_idx_max = vm->vm_qcache_max >> vm->vm_quantum_shift;
	prevqc = NULL;
	for (i = 0; i < qcache_idx_max; i++) {
		qcache_t *qc = vm->vm_qcache[i];

		if (prevqc == qc) {
			continue;
		}
		pool_cache_destroy(qc->qc_cache);
		prevqc = qc;
	}
}
#endif

#if defined(_KERNEL)
static void
vmem_bootstrap(void)
{

	mutex_init(&vmem_list_lock, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&vmem_btag_lock, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&vmem_btag_refill_lock, MUTEX_DEFAULT, IPL_VM);

	while (static_bt_count-- > 0) {
		bt_t *bt = &static_bts[static_bt_count];
		LIST_INSERT_HEAD(&vmem_btag_freelist, bt, bt_freelist);
		VMEM_EVCNT_INCR(static_bt_count);
		vmem_btag_freelist_count++;
	}
	vmem_bootstrapped = TRUE;
}

void
vmem_subsystem_init(vmem_t *vm)
{

	kmem_va_meta_arena = vmem_init(&kmem_va_meta_arena_store, "vmem-va",
	    0, 0, PAGE_SIZE, vmem_alloc, vmem_free, vm,
	    0, VM_NOSLEEP | VM_BOOTSTRAP | VM_LARGEIMPORT,
	    IPL_VM);

	kmem_meta_arena = vmem_init(&kmem_meta_arena_store, "vmem-meta",
	    0, 0, PAGE_SIZE,
	    uvm_km_kmem_alloc, uvm_km_kmem_free, kmem_va_meta_arena,
	    0, VM_NOSLEEP | VM_BOOTSTRAP, IPL_VM);

	pool_init(&vmem_btag_pool, sizeof(bt_t), 0, 0, PR_PHINPAGE,
		    "vmembt", &pool_allocator_vmem_meta, IPL_VM);
}
#endif /* defined(_KERNEL) */

static int
vmem_add1(vmem_t *vm, vmem_addr_t addr, vmem_size_t size, vm_flag_t flags,
    int spanbttype)
{
	bt_t *btspan;
	bt_t *btfree;

	KASSERT((flags & (VM_SLEEP|VM_NOSLEEP)) != 0);
	KASSERT((~flags & (VM_SLEEP|VM_NOSLEEP)) != 0);
	KASSERT(spanbttype == BT_TYPE_SPAN ||
	    spanbttype == BT_TYPE_SPAN_STATIC);

	btspan = bt_alloc(vm, flags);
	if (btspan == NULL) {
		return ENOMEM;
	}
	btfree = bt_alloc(vm, flags);
	if (btfree == NULL) {
		bt_free(vm, btspan);
		return ENOMEM;
	}

	btspan->bt_type = spanbttype;
	btspan->bt_start = addr;
	btspan->bt_size = size;

	btfree->bt_type = BT_TYPE_FREE;
	btfree->bt_start = addr;
	btfree->bt_size = size;

	VMEM_LOCK(vm);
	bt_insseg_tail(vm, btspan);
	bt_insseg(vm, btfree, btspan);
	bt_insfree(vm, btfree);
	vm->vm_size += size;
	VMEM_UNLOCK(vm);

	return 0;
}

static void
vmem_destroy1(vmem_t *vm)
{

#if defined(QCACHE)
	qc_destroy(vm);
#endif /* defined(QCACHE) */
	if (vm->vm_hashlist != NULL) {
		int i;

		for (i = 0; i < vm->vm_hashsize; i++) {
			bt_t *bt;

			while ((bt = LIST_FIRST(&vm->vm_hashlist[i])) != NULL) {
				KASSERT(bt->bt_type == BT_TYPE_SPAN_STATIC);
				bt_free(vm, bt);
			}
		}
		if (vm->vm_hashlist != &vm->vm_hash0) {
			xfree(vm->vm_hashlist,
			    sizeof(struct vmem_hashlist *) * vm->vm_hashsize);
		}
	}

	bt_freetrim(vm, 0);

	VMEM_CONDVAR_DESTROY(vm);
	VMEM_LOCK_DESTROY(vm);
	xfree(vm, sizeof(*vm));
}

static int
vmem_import(vmem_t *vm, vmem_size_t size, vm_flag_t flags)
{
	vmem_addr_t addr;
	int rc;

	if (vm->vm_importfn == NULL) {
		return EINVAL;
	}

	if (vm->vm_flags & VM_LARGEIMPORT) {
		size *= 16;
	}

	if (vm->vm_flags & VM_XIMPORT) {
		rc = ((vmem_ximport_t *)vm->vm_importfn)(vm->vm_arg, size,
		    &size, flags, &addr);
	} else {
		rc = (vm->vm_importfn)(vm->vm_arg, size, flags, &addr);
	}
	if (rc) {
		return ENOMEM;
	}

	if (vmem_add1(vm, addr, size, flags, BT_TYPE_SPAN) != 0) {
		(*vm->vm_releasefn)(vm->vm_arg, addr, size);
		return ENOMEM;
	}

	return 0;
}

static int
vmem_rehash(vmem_t *vm, size_t newhashsize, vm_flag_t flags)
{
	bt_t *bt;
	int i;
	struct vmem_hashlist *newhashlist;
	struct vmem_hashlist *oldhashlist;
	size_t oldhashsize;

	KASSERT(newhashsize > 0);

	newhashlist =
	    xmalloc(sizeof(struct vmem_hashlist *) * newhashsize, flags);
	if (newhashlist == NULL) {
		return ENOMEM;
	}
	for (i = 0; i < newhashsize; i++) {
		LIST_INIT(&newhashlist[i]);
	}

	if (!VMEM_TRYLOCK(vm)) {
		xfree(newhashlist,
		    sizeof(struct vmem_hashlist *) * newhashsize);
		return EBUSY;
	}
	oldhashlist = vm->vm_hashlist;
	oldhashsize = vm->vm_hashsize;
	vm->vm_hashlist = newhashlist;
	vm->vm_hashsize = newhashsize;
	if (oldhashlist == NULL) {
		VMEM_UNLOCK(vm);
		return 0;
	}
	for (i = 0; i < oldhashsize; i++) {
		while ((bt = LIST_FIRST(&oldhashlist[i])) != NULL) {
			bt_rembusy(vm, bt); /* XXX */
			bt_insbusy(vm, bt);
		}
	}
	VMEM_UNLOCK(vm);

	if (oldhashlist != &vm->vm_hash0) {
		xfree(oldhashlist,
		    sizeof(struct vmem_hashlist *) * oldhashsize);
	}

	return 0;
}

/*
 * vmem_fit: check if a bt can satisfy the given restrictions.
 *
 * it's a caller's responsibility to ensure the region is big enough
 * before calling us.
 */

static int
vmem_fit(const bt_t *bt, vmem_size_t size, vmem_size_t align,
    vmem_size_t phase, vmem_size_t nocross,
    vmem_addr_t minaddr, vmem_addr_t maxaddr, vmem_addr_t *addrp)
{
	vmem_addr_t start;
	vmem_addr_t end;

	KASSERT(size > 0);
	KASSERT(bt->bt_size >= size); /* caller's responsibility */

	/*
	 * XXX assumption: vmem_addr_t and vmem_size_t are
	 * unsigned integer of the same size.
	 */

	start = bt->bt_start;
	if (start < minaddr) {
		start = minaddr;
	}
	end = BT_END(bt);
	if (end > maxaddr) {
		end = maxaddr;
	}
	if (start > end) {
		return ENOMEM;
	}

	start = VMEM_ALIGNUP(start - phase, align) + phase;
	if (start < bt->bt_start) {
		start += align;
	}
	if (VMEM_CROSS_P(start, start + size - 1, nocross)) {
		KASSERT(align < nocross);
		start = VMEM_ALIGNUP(start - phase, nocross) + phase;
	}
	if (start <= end && end - start >= size - 1) {
		KASSERT((start & (align - 1)) == phase);
		KASSERT(!VMEM_CROSS_P(start, start + size - 1, nocross));
		KASSERT(minaddr <= start);
		KASSERT(maxaddr == 0 || start + size - 1 <= maxaddr);
		KASSERT(bt->bt_start <= start);
		KASSERT(BT_END(bt) - start >= size - 1);
		*addrp = start;
		return 0;
	}
	return ENOMEM;
}

/* ---- vmem API */

/*
 * vmem_create_internal: creates a vmem arena.
 */

vmem_t *
vmem_init(vmem_t *vm, const char *name,
    vmem_addr_t base, vmem_size_t size, vmem_size_t quantum,
    vmem_import_t *importfn, vmem_release_t *releasefn,
    vmem_t *arg, vmem_size_t qcache_max, vm_flag_t flags, int ipl)
{
	int i;

	KASSERT((flags & (VM_SLEEP|VM_NOSLEEP)) != 0);
	KASSERT((~flags & (VM_SLEEP|VM_NOSLEEP)) != 0);
	KASSERT(quantum > 0);

#if defined(_KERNEL)
	/* XXX: SMP, we get called early... */
	if (!vmem_bootstrapped) {
		vmem_bootstrap();
	}
#endif /* defined(_KERNEL) */

	if (vm == NULL) {
		vm = xmalloc(sizeof(*vm), flags);
	}
	if (vm == NULL) {
		return NULL;
	}

	VMEM_CONDVAR_INIT(vm, "vmem");
	VMEM_LOCK_INIT(vm, ipl);
	vm->vm_flags = flags;
	vm->vm_nfreetags = 0;
	LIST_INIT(&vm->vm_freetags);
	strlcpy(vm->vm_name, name, sizeof(vm->vm_name));
	vm->vm_quantum_mask = quantum - 1;
	vm->vm_quantum_shift = SIZE2ORDER(quantum);
	KASSERT(ORDER2SIZE(vm->vm_quantum_shift) == quantum);
	vm->vm_importfn = importfn;
	vm->vm_releasefn = releasefn;
	vm->vm_arg = arg;
	vm->vm_nbusytag = 0;
	vm->vm_size = 0;
	vm->vm_inuse = 0;
#if defined(QCACHE)
	qc_init(vm, qcache_max, ipl);
#endif /* defined(QCACHE) */

	TAILQ_INIT(&vm->vm_seglist);
	for (i = 0; i < VMEM_MAXORDER; i++) {
		LIST_INIT(&vm->vm_freelist[i]);
	}
	memset(&vm->vm_hash0, 0, sizeof(struct vmem_hashlist));
	vm->vm_hashsize = 1;
	vm->vm_hashlist = &vm->vm_hash0;

	if (size != 0) {
		if (vmem_add(vm, base, size, flags) != 0) {
			vmem_destroy1(vm);
			return NULL;
		}
	}

#if defined(_KERNEL)
	if (flags & VM_BOOTSTRAP) {
		bt_refill(vm, VM_NOSLEEP);
	}

	mutex_enter(&vmem_list_lock);
	LIST_INSERT_HEAD(&vmem_list, vm, vm_alllist);
	mutex_exit(&vmem_list_lock);
#endif /* defined(_KERNEL) */

	return vm;
}



/*
 * vmem_create: create an arena.
 *
 * => must not be called from interrupt context.
 */

vmem_t *
vmem_create(const char *name, vmem_addr_t base, vmem_size_t size,
    vmem_size_t quantum, vmem_import_t *importfn, vmem_release_t *releasefn,
    vmem_t *source, vmem_size_t qcache_max, vm_flag_t flags, int ipl)
{

	KASSERT((flags & (VM_XIMPORT)) == 0);

	return vmem_init(NULL, name, base, size, quantum,
	    importfn, releasefn, source, qcache_max, flags, ipl);
}

/*
 * vmem_xcreate: create an arena takes alternative import func.
 *
 * => must not be called from interrupt context.
 */

vmem_t *
vmem_xcreate(const char *name, vmem_addr_t base, vmem_size_t size,
    vmem_size_t quantum, vmem_ximport_t *importfn, vmem_release_t *releasefn,
    vmem_t *source, vmem_size_t qcache_max, vm_flag_t flags, int ipl)
{

	KASSERT((flags & (VM_XIMPORT)) == 0);

	return vmem_init(NULL, name, base, size, quantum,
	    (vmem_import_t *)importfn, releasefn, source,
	    qcache_max, flags | VM_XIMPORT, ipl);
}

void
vmem_destroy(vmem_t *vm)
{

#if defined(_KERNEL)
	mutex_enter(&vmem_list_lock);
	LIST_REMOVE(vm, vm_alllist);
	mutex_exit(&vmem_list_lock);
#endif /* defined(_KERNEL) */

	vmem_destroy1(vm);
}

vmem_size_t
vmem_roundup_size(vmem_t *vm, vmem_size_t size)
{

	return (size + vm->vm_quantum_mask) & ~vm->vm_quantum_mask;
}

/*
 * vmem_alloc: allocate resource from the arena.
 */

int
vmem_alloc(vmem_t *vm, vmem_size_t size, vm_flag_t flags, vmem_addr_t *addrp)
{
	const vm_flag_t strat __diagused = flags & VM_FITMASK;

	KASSERT((flags & (VM_SLEEP|VM_NOSLEEP)) != 0);
	KASSERT((~flags & (VM_SLEEP|VM_NOSLEEP)) != 0);

	KASSERT(size > 0);
	KASSERT(strat == VM_BESTFIT || strat == VM_INSTANTFIT);
	if ((flags & VM_SLEEP) != 0) {
		ASSERT_SLEEPABLE();
	}

#if defined(QCACHE)
	if (size <= vm->vm_qcache_max) {
		void *p;
		int qidx = (size + vm->vm_quantum_mask) >> vm->vm_quantum_shift;
		qcache_t *qc = vm->vm_qcache[qidx - 1];

		p = pool_cache_get(qc->qc_cache, vmf_to_prf(flags));
		if (addrp != NULL)
			*addrp = (vmem_addr_t)p;
		return (p == NULL) ? ENOMEM : 0;
	}
#endif /* defined(QCACHE) */

	return vmem_xalloc(vm, size, 0, 0, 0, VMEM_ADDR_MIN, VMEM_ADDR_MAX,
	    flags, addrp);
}

int
vmem_xalloc(vmem_t *vm, const vmem_size_t size0, vmem_size_t align,
    const vmem_size_t phase, const vmem_size_t nocross,
    const vmem_addr_t minaddr, const vmem_addr_t maxaddr, const vm_flag_t flags,
    vmem_addr_t *addrp)
{
	struct vmem_freelist *list;
	struct vmem_freelist *first;
	struct vmem_freelist *end;
	bt_t *bt;
	bt_t *btnew;
	bt_t *btnew2;
	const vmem_size_t size = vmem_roundup_size(vm, size0);
	vm_flag_t strat = flags & VM_FITMASK;
	vmem_addr_t start;
	int rc;

	KASSERT(size0 > 0);
	KASSERT(size > 0);
	KASSERT(strat == VM_BESTFIT || strat == VM_INSTANTFIT);
	if ((flags & VM_SLEEP) != 0) {
		ASSERT_SLEEPABLE();
	}
	KASSERT((align & vm->vm_quantum_mask) == 0);
	KASSERT((align & (align - 1)) == 0);
	KASSERT((phase & vm->vm_quantum_mask) == 0);
	KASSERT((nocross & vm->vm_quantum_mask) == 0);
	KASSERT((nocross & (nocross - 1)) == 0);
	KASSERT((align == 0 && phase == 0) || phase < align);
	KASSERT(nocross == 0 || nocross >= size);
	KASSERT(minaddr <= maxaddr);
	KASSERT(!VMEM_CROSS_P(phase, phase + size - 1, nocross));

	if (align == 0) {
		align = vm->vm_quantum_mask + 1;
	}

	/*
	 * allocate boundary tags before acquiring the vmem lock.
	 */
	btnew = bt_alloc(vm, flags);
	if (btnew == NULL) {
		return ENOMEM;
	}
	btnew2 = bt_alloc(vm, flags); /* XXX not necessary if no restrictions */
	if (btnew2 == NULL) {
		bt_free(vm, btnew);
		return ENOMEM;
	}

	/*
	 * choose a free block from which we allocate.
	 */
retry_strat:
	first = bt_freehead_toalloc(vm, size, strat);
	end = &vm->vm_freelist[VMEM_MAXORDER];
retry:
	bt = NULL;
	VMEM_LOCK(vm);
	vmem_check(vm);
	if (strat == VM_INSTANTFIT) {
		/*
		 * just choose the first block which satisfies our restrictions.
		 *
		 * note that we don't need to check the size of the blocks
		 * because any blocks found on these list should be larger than
		 * the given size.
		 */
		for (list = first; list < end; list++) {
			bt = LIST_FIRST(list);
			if (bt != NULL) {
				rc = vmem_fit(bt, size, align, phase,
				    nocross, minaddr, maxaddr, &start);
				if (rc == 0) {
					goto gotit;
				}
				/*
				 * don't bother to follow the bt_freelist link
				 * here.  the list can be very long and we are
				 * told to run fast.  blocks from the later free
				 * lists are larger and have better chances to
				 * satisfy our restrictions.
				 */
			}
		}
	} else { /* VM_BESTFIT */
		/*
		 * we assume that, for space efficiency, it's better to
		 * allocate from a smaller block.  thus we will start searching
		 * from the lower-order list than VM_INSTANTFIT.
		 * however, don't bother to find the smallest block in a free
		 * list because the list can be very long.  we can revisit it
		 * if/when it turns out to be a problem.
		 *
		 * note that the 'first' list can contain blocks smaller than
		 * the requested size.  thus we need to check bt_size.
		 */
		for (list = first; list < end; list++) {
			LIST_FOREACH(bt, list, bt_freelist) {
				if (bt->bt_size >= size) {
					rc = vmem_fit(bt, size, align, phase,
					    nocross, minaddr, maxaddr, &start);
					if (rc == 0) {
						goto gotit;
					}
				}
			}
		}
	}
	VMEM_UNLOCK(vm);
#if 1
	if (strat == VM_INSTANTFIT) {
		strat = VM_BESTFIT;
		goto retry_strat;
	}
#endif
	if (align != vm->vm_quantum_mask + 1 || phase != 0 || nocross != 0) {

		/*
		 * XXX should try to import a region large enough to
		 * satisfy restrictions?
		 */

		goto fail;
	}
	/* XXX eeek, minaddr & maxaddr not respected */
	if (vmem_import(vm, size, flags) == 0) {
		goto retry;
	}
	/* XXX */

	if ((flags & VM_SLEEP) != 0) {
#if defined(_KERNEL)
		mutex_spin_enter(&uvm_fpageqlock);
		uvm_kick_pdaemon();
		mutex_spin_exit(&uvm_fpageqlock);
#endif
		VMEM_LOCK(vm);
		VMEM_CONDVAR_WAIT(vm);
		VMEM_UNLOCK(vm);
		goto retry;
	}
fail:
	bt_free(vm, btnew);
	bt_free(vm, btnew2);
	return ENOMEM;

gotit:
	KASSERT(bt->bt_type == BT_TYPE_FREE);
	KASSERT(bt->bt_size >= size);
	bt_remfree(vm, bt);
	vmem_check(vm);
	if (bt->bt_start != start) {
		btnew2->bt_type = BT_TYPE_FREE;
		btnew2->bt_start = bt->bt_start;
		btnew2->bt_size = start - bt->bt_start;
		bt->bt_start = start;
		bt->bt_size -= btnew2->bt_size;
		bt_insfree(vm, btnew2);
		bt_insseg(vm, btnew2, TAILQ_PREV(bt, vmem_seglist, bt_seglist));
		btnew2 = NULL;
		vmem_check(vm);
	}
	KASSERT(bt->bt_start == start);
	if (bt->bt_size != size && bt->bt_size - size > vm->vm_quantum_mask) {
		/* split */
		btnew->bt_type = BT_TYPE_BUSY;
		btnew->bt_start = bt->bt_start;
		btnew->bt_size = size;
		bt->bt_start = bt->bt_start + size;
		bt->bt_size -= size;
		bt_insfree(vm, bt);
		bt_insseg(vm, btnew, TAILQ_PREV(bt, vmem_seglist, bt_seglist));
		bt_insbusy(vm, btnew);
		vmem_check(vm);
		VMEM_UNLOCK(vm);
	} else {
		bt->bt_type = BT_TYPE_BUSY;
		bt_insbusy(vm, bt);
		vmem_check(vm);
		VMEM_UNLOCK(vm);
		bt_free(vm, btnew);
		btnew = bt;
	}
	if (btnew2 != NULL) {
		bt_free(vm, btnew2);
	}
	KASSERT(btnew->bt_size >= size);
	btnew->bt_type = BT_TYPE_BUSY;

	if (addrp != NULL)
		*addrp = btnew->bt_start;
	return 0;
}

/*
 * vmem_free: free the resource to the arena.
 */

void
vmem_free(vmem_t *vm, vmem_addr_t addr, vmem_size_t size)
{

	KASSERT(size > 0);

#if defined(QCACHE)
	if (size <= vm->vm_qcache_max) {
		int qidx = (size + vm->vm_quantum_mask) >> vm->vm_quantum_shift;
		qcache_t *qc = vm->vm_qcache[qidx - 1];

		pool_cache_put(qc->qc_cache, (void *)addr);
		return;
	}
#endif /* defined(QCACHE) */

	vmem_xfree(vm, addr, size);
}

void
vmem_xfree(vmem_t *vm, vmem_addr_t addr, vmem_size_t size)
{
	bt_t *bt;
	bt_t *t;
	LIST_HEAD(, vmem_btag) tofree;

	LIST_INIT(&tofree);

	KASSERT(size > 0);

	VMEM_LOCK(vm);

	bt = bt_lookupbusy(vm, addr);
	KASSERT(bt != NULL);
	KASSERT(bt->bt_start == addr);
	KASSERT(bt->bt_size == vmem_roundup_size(vm, size) ||
	    bt->bt_size - vmem_roundup_size(vm, size) <= vm->vm_quantum_mask);
	KASSERT(bt->bt_type == BT_TYPE_BUSY);
	bt_rembusy(vm, bt);
	bt->bt_type = BT_TYPE_FREE;

	/* coalesce */
	t = TAILQ_NEXT(bt, bt_seglist);
	if (t != NULL && t->bt_type == BT_TYPE_FREE) {
		KASSERT(BT_END(bt) < t->bt_start);	/* YYY */
		bt_remfree(vm, t);
		bt_remseg(vm, t);
		bt->bt_size += t->bt_size;
		LIST_INSERT_HEAD(&tofree, t, bt_freelist);
	}
	t = TAILQ_PREV(bt, vmem_seglist, bt_seglist);
	if (t != NULL && t->bt_type == BT_TYPE_FREE) {
		KASSERT(BT_END(t) < bt->bt_start);	/* YYY */
		bt_remfree(vm, t);
		bt_remseg(vm, t);
		bt->bt_size += t->bt_size;
		bt->bt_start = t->bt_start;
		LIST_INSERT_HEAD(&tofree, t, bt_freelist);
	}

	t = TAILQ_PREV(bt, vmem_seglist, bt_seglist);
	KASSERT(t != NULL);
	KASSERT(BT_ISSPAN_P(t) || t->bt_type == BT_TYPE_BUSY);
	if (vm->vm_releasefn != NULL && t->bt_type == BT_TYPE_SPAN &&
	    t->bt_size == bt->bt_size) {
		vmem_addr_t spanaddr;
		vmem_size_t spansize;

		KASSERT(t->bt_start == bt->bt_start);
		spanaddr = bt->bt_start;
		spansize = bt->bt_size;
		bt_remseg(vm, bt);
		LIST_INSERT_HEAD(&tofree, bt, bt_freelist);
		bt_remseg(vm, t);
		LIST_INSERT_HEAD(&tofree, t, bt_freelist);
		vm->vm_size -= spansize;
		VMEM_CONDVAR_BROADCAST(vm);
		VMEM_UNLOCK(vm);
		(*vm->vm_releasefn)(vm->vm_arg, spanaddr, spansize);
	} else {
		bt_insfree(vm, bt);
		VMEM_CONDVAR_BROADCAST(vm);
		VMEM_UNLOCK(vm);
	}

	while (!LIST_EMPTY(&tofree)) {
		t = LIST_FIRST(&tofree);
		LIST_REMOVE(t, bt_freelist);
		bt_free(vm, t);
	}

	bt_freetrim(vm, BT_MAXFREE);
}

/*
 * vmem_add:
 *
 * => caller must ensure appropriate spl,
 *    if the arena can be accessed from interrupt context.
 */

int
vmem_add(vmem_t *vm, vmem_addr_t addr, vmem_size_t size, vm_flag_t flags)
{

	return vmem_add1(vm, addr, size, flags, BT_TYPE_SPAN_STATIC);
}

/*
 * vmem_size: information about arenas size
 *
 * => return free/allocated size in arena
 */
vmem_size_t
vmem_size(vmem_t *vm, int typemask)
{

	switch (typemask) {
	case VMEM_ALLOC:
		return vm->vm_inuse;
	case VMEM_FREE:
		return vm->vm_size - vm->vm_inuse;
	case VMEM_FREE|VMEM_ALLOC:
		return vm->vm_size;
	default:
		panic("vmem_size");
	}
}

/* ---- rehash */

#if defined(_KERNEL)
static struct callout vmem_rehash_ch;
static int vmem_rehash_interval;
static struct workqueue *vmem_rehash_wq;
static struct work vmem_rehash_wk;

static void
vmem_rehash_all(struct work *wk, void *dummy)
{
	vmem_t *vm;

	KASSERT(wk == &vmem_rehash_wk);
	mutex_enter(&vmem_list_lock);
	LIST_FOREACH(vm, &vmem_list, vm_alllist) {
		size_t desired;
		size_t current;

		if (!VMEM_TRYLOCK(vm)) {
			continue;
		}
		desired = vm->vm_nbusytag;
		current = vm->vm_hashsize;
		VMEM_UNLOCK(vm);

		if (desired > VMEM_HASHSIZE_MAX) {
			desired = VMEM_HASHSIZE_MAX;
		} else if (desired < VMEM_HASHSIZE_MIN) {
			desired = VMEM_HASHSIZE_MIN;
		}
		if (desired > current * 2 || desired * 2 < current) {
			vmem_rehash(vm, desired, VM_NOSLEEP);
		}
	}
	mutex_exit(&vmem_list_lock);

	callout_schedule(&vmem_rehash_ch, vmem_rehash_interval);
}

static void
vmem_rehash_all_kick(void *dummy)
{

	workqueue_enqueue(vmem_rehash_wq, &vmem_rehash_wk, NULL);
}

void
vmem_rehash_start(void)
{
	int error;

	error = workqueue_create(&vmem_rehash_wq, "vmem_rehash",
	    vmem_rehash_all, NULL, PRI_VM, IPL_SOFTCLOCK, WQ_MPSAFE);
	if (error) {
		panic("%s: workqueue_create %d\n", __func__, error);
	}
	callout_init(&vmem_rehash_ch, CALLOUT_MPSAFE);
	callout_setfunc(&vmem_rehash_ch, vmem_rehash_all_kick, NULL);

	vmem_rehash_interval = hz * 10;
	callout_schedule(&vmem_rehash_ch, vmem_rehash_interval);
}
#endif /* defined(_KERNEL) */

/* ---- debug */

#if defined(DDB) || defined(UNITTEST) || defined(VMEM_SANITY)

static void bt_dump(const bt_t *, void (*)(const char *, ...)
    __printflike(1, 2));

static const char *
bt_type_string(int type)
{
	static const char * const table[] = {
		[BT_TYPE_BUSY] = "busy",
		[BT_TYPE_FREE] = "free",
		[BT_TYPE_SPAN] = "span",
		[BT_TYPE_SPAN_STATIC] = "static span",
	};

	if (type >= __arraycount(table)) {
		return "BOGUS";
	}
	return table[type];
}

static void
bt_dump(const bt_t *bt, void (*pr)(const char *, ...))
{

	(*pr)("\t%p: %" PRIu64 ", %" PRIu64 ", %d(%s)\n",
	    bt, (uint64_t)bt->bt_start, (uint64_t)bt->bt_size,
	    bt->bt_type, bt_type_string(bt->bt_type));
}

static void
vmem_dump(const vmem_t *vm , void (*pr)(const char *, ...) __printflike(1, 2))
{
	const bt_t *bt;
	int i;

	(*pr)("vmem %p '%s'\n", vm, vm->vm_name);
	TAILQ_FOREACH(bt, &vm->vm_seglist, bt_seglist) {
		bt_dump(bt, pr);
	}

	for (i = 0; i < VMEM_MAXORDER; i++) {
		const struct vmem_freelist *fl = &vm->vm_freelist[i];

		if (LIST_EMPTY(fl)) {
			continue;
		}

		(*pr)("freelist[%d]\n", i);
		LIST_FOREACH(bt, fl, bt_freelist) {
			bt_dump(bt, pr);
		}
	}
}

#endif /* defined(DDB) || defined(UNITTEST) || defined(VMEM_SANITY) */

#if defined(DDB)
static bt_t *
vmem_whatis_lookup(vmem_t *vm, uintptr_t addr)
{
	bt_t *bt;

	TAILQ_FOREACH(bt, &vm->vm_seglist, bt_seglist) {
		if (BT_ISSPAN_P(bt)) {
			continue;
		}
		if (bt->bt_start <= addr && addr <= BT_END(bt)) {
			return bt;
		}
	}

	return NULL;
}

void
vmem_whatis(uintptr_t addr, void (*pr)(const char *, ...))
{
	vmem_t *vm;

	LIST_FOREACH(vm, &vmem_list, vm_alllist) {
		bt_t *bt;

		bt = vmem_whatis_lookup(vm, addr);
		if (bt == NULL) {
			continue;
		}
		(*pr)("%p is %p+%zu in VMEM '%s' (%s)\n",
		    (void *)addr, (void *)bt->bt_start,
		    (size_t)(addr - bt->bt_start), vm->vm_name,
		    (bt->bt_type == BT_TYPE_BUSY) ? "allocated" : "free");
	}
}

void
vmem_printall(const char *modif, void (*pr)(const char *, ...))
{
	const vmem_t *vm;

	LIST_FOREACH(vm, &vmem_list, vm_alllist) {
		vmem_dump(vm, pr);
	}
}

void
vmem_print(uintptr_t addr, const char *modif, void (*pr)(const char *, ...))
{
	const vmem_t *vm = (const void *)addr;

	vmem_dump(vm, pr);
}
#endif /* defined(DDB) */

#if defined(_KERNEL)
#define vmem_printf printf
#else
#include <stdio.h>
#include <stdarg.h>

static void
vmem_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#endif

#if defined(VMEM_SANITY)

static bool
vmem_check_sanity(vmem_t *vm)
{
	const bt_t *bt, *bt2;

	KASSERT(vm != NULL);

	TAILQ_FOREACH(bt, &vm->vm_seglist, bt_seglist) {
		if (bt->bt_start > BT_END(bt)) {
			printf("corrupted tag\n");
			bt_dump(bt, vmem_printf);
			return false;
		}
	}
	TAILQ_FOREACH(bt, &vm->vm_seglist, bt_seglist) {
		TAILQ_FOREACH(bt2, &vm->vm_seglist, bt_seglist) {
			if (bt == bt2) {
				continue;
			}
			if (BT_ISSPAN_P(bt) != BT_ISSPAN_P(bt2)) {
				continue;
			}
			if (bt->bt_start <= BT_END(bt2) &&
			    bt2->bt_start <= BT_END(bt)) {
				printf("overwrapped tags\n");
				bt_dump(bt, vmem_printf);
				bt_dump(bt2, vmem_printf);
				return false;
			}
		}
	}

	return true;
}

static void
vmem_check(vmem_t *vm)
{

	if (!vmem_check_sanity(vm)) {
		panic("insanity vmem %p", vm);
	}
}

#endif /* defined(VMEM_SANITY) */

#if defined(UNITTEST)
int
main(void)
{
	int rc;
	vmem_t *vm;
	vmem_addr_t p;
	struct reg {
		vmem_addr_t p;
		vmem_size_t sz;
		bool x;
	} *reg = NULL;
	int nreg = 0;
	int nalloc = 0;
	int nfree = 0;
	vmem_size_t total = 0;
#if 1
	vm_flag_t strat = VM_INSTANTFIT;
#else
	vm_flag_t strat = VM_BESTFIT;
#endif

	vm = vmem_create("test", 0, 0, 1, NULL, NULL, NULL, 0, VM_SLEEP,
#ifdef _KERNEL
	    IPL_NONE
#else
	    0
#endif
	    );
	if (vm == NULL) {
		printf("vmem_create\n");
		exit(EXIT_FAILURE);
	}
	vmem_dump(vm, vmem_printf);

	rc = vmem_add(vm, 0, 50, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 100, 200, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 2000, 1, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 40000, 65536, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 10000, 10000, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 500, 1000, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_add(vm, 0xffffff00, 0x100, VM_SLEEP);
	assert(rc == 0);
	rc = vmem_xalloc(vm, 0x101, 0, 0, 0,
	    0xffffff00, 0xffffffff, strat|VM_SLEEP, &p);
	assert(rc != 0);
	rc = vmem_xalloc(vm, 50, 0, 0, 0, 0, 49, strat|VM_SLEEP, &p);
	assert(rc == 0 && p == 0);
	vmem_xfree(vm, p, 50);
	rc = vmem_xalloc(vm, 25, 0, 0, 0, 0, 24, strat|VM_SLEEP, &p);
	assert(rc == 0 && p == 0);
	rc = vmem_xalloc(vm, 0x100, 0, 0, 0,
	    0xffffff01, 0xffffffff, strat|VM_SLEEP, &p);
	assert(rc != 0);
	rc = vmem_xalloc(vm, 0x100, 0, 0, 0,
	    0xffffff00, 0xfffffffe, strat|VM_SLEEP, &p);
	assert(rc != 0);
	rc = vmem_xalloc(vm, 0x100, 0, 0, 0,
	    0xffffff00, 0xffffffff, strat|VM_SLEEP, &p);
	assert(rc == 0);
	vmem_dump(vm, vmem_printf);
	for (;;) {
		struct reg *r;
		int t = rand() % 100;

		if (t > 45) {
			/* alloc */
			vmem_size_t sz = rand() % 500 + 1;
			bool x;
			vmem_size_t align, phase, nocross;
			vmem_addr_t minaddr, maxaddr;

			if (t > 70) {
				x = true;
				/* XXX */
				align = 1 << (rand() % 15);
				phase = rand() % 65536;
				nocross = 1 << (rand() % 15);
				if (align <= phase) {
					phase = 0;
				}
				if (VMEM_CROSS_P(phase, phase + sz - 1,
				    nocross)) {
					nocross = 0;
				}
				do {
					minaddr = rand() % 50000;
					maxaddr = rand() % 70000;
				} while (minaddr > maxaddr);
				printf("=== xalloc %" PRIu64
				    " align=%" PRIu64 ", phase=%" PRIu64
				    ", nocross=%" PRIu64 ", min=%" PRIu64
				    ", max=%" PRIu64 "\n",
				    (uint64_t)sz,
				    (uint64_t)align,
				    (uint64_t)phase,
				    (uint64_t)nocross,
				    (uint64_t)minaddr,
				    (uint64_t)maxaddr);
				rc = vmem_xalloc(vm, sz, align, phase, nocross,
				    minaddr, maxaddr, strat|VM_SLEEP, &p);
			} else {
				x = false;
				printf("=== alloc %" PRIu64 "\n", (uint64_t)sz);
				rc = vmem_alloc(vm, sz, strat|VM_SLEEP, &p);
			}
			printf("-> %" PRIu64 "\n", (uint64_t)p);
			vmem_dump(vm, vmem_printf);
			if (rc != 0) {
				if (x) {
					continue;
				}
				break;
			}
			nreg++;
			reg = realloc(reg, sizeof(*reg) * nreg);
			r = &reg[nreg - 1];
			r->p = p;
			r->sz = sz;
			r->x = x;
			total += sz;
			nalloc++;
		} else if (nreg != 0) {
			/* free */
			r = &reg[rand() % nreg];
			printf("=== free %" PRIu64 ", %" PRIu64 "\n",
			    (uint64_t)r->p, (uint64_t)r->sz);
			if (r->x) {
				vmem_xfree(vm, r->p, r->sz);
			} else {
				vmem_free(vm, r->p, r->sz);
			}
			total -= r->sz;
			vmem_dump(vm, vmem_printf);
			*r = reg[nreg - 1];
			nreg--;
			nfree++;
		}
		printf("total=%" PRIu64 "\n", (uint64_t)total);
	}
	fprintf(stderr, "total=%" PRIu64 ", nalloc=%d, nfree=%d\n",
	    (uint64_t)total, nalloc, nfree);
	exit(EXIT_SUCCESS);
}
#endif /* defined(UNITTEST) */
