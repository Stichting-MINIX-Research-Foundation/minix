/*	$NetBSD: vm.c,v 1.167 2015/06/02 14:07:48 pooka Exp $	*/

/*
 * Copyright (c) 2007-2011 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by
 * The Finnish Cultural Foundation and the Research Foundation of
 * The Helsinki University of Technology.
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

/*
 * Virtual memory emulation routines.
 */

/*
 * XXX: we abuse pg->uanon for the virtual address of the storage
 * for each page.  phys_addr would fit the job description better,
 * except that it will create unnecessary lossage on some platforms
 * due to not being a pointer type.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vm.c,v 1.167 2015/06/02 14:07:48 pooka Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/null.h>
#include <sys/vnode.h>

#include <machine/pmap.h>

#include <rump/rumpuser.h>

#include <uvm/uvm.h>
#include <uvm/uvm_ddb.h>
#include <uvm/uvm_pdpolicy.h>
#include <uvm/uvm_prot.h>
#include <uvm/uvm_readahead.h>
#include <uvm/uvm_device.h>

#include "rump_private.h"

#include "rump_vfs_private.h"

kmutex_t uvm_pageqlock; /* non-free page lock */
kmutex_t uvm_fpageqlock; /* free page lock, non-gpl license */
kmutex_t uvm_swap_data_lock;

struct uvmexp uvmexp;
struct uvm uvm;

#ifdef __uvmexp_pagesize
const int * const uvmexp_pagesize = &uvmexp.pagesize;
const int * const uvmexp_pagemask = &uvmexp.pagemask;
const int * const uvmexp_pageshift = &uvmexp.pageshift;
#endif

static struct vm_map kernel_map_store;
struct vm_map *kernel_map = &kernel_map_store;

static struct vm_map module_map_store;
extern struct vm_map *module_map;

static struct pmap pmap_kernel;
struct pmap rump_pmap_local;
struct pmap *const kernel_pmap_ptr = &pmap_kernel;

vmem_t *kmem_arena;
vmem_t *kmem_va_arena;

static unsigned int pdaemon_waiters;
static kmutex_t pdaemonmtx;
static kcondvar_t pdaemoncv, oomwait;

/* all local non-proc0 processes share this vmspace */
struct vmspace *rump_vmspace_local;

unsigned long rump_physmemlimit = RUMPMEM_UNLIMITED;
static unsigned long pdlimit = RUMPMEM_UNLIMITED; /* page daemon memlimit */
static unsigned long curphysmem;
static unsigned long dddlim;		/* 90% of memory limit used */
#define NEED_PAGEDAEMON() \
    (rump_physmemlimit != RUMPMEM_UNLIMITED && curphysmem > dddlim)
#define PDRESERVE (2*MAXPHYS)

/*
 * Try to free two pages worth of pages from objects.
 * If this succesfully frees a full page cache page, we'll
 * free the released page plus PAGE_SIZE/sizeof(vm_page).
 */
#define PAGEDAEMON_OBJCHUNK (2*PAGE_SIZE / sizeof(struct vm_page))

/*
 * Keep a list of least recently used pages.  Since the only way a
 * rump kernel can "access" a page is via lookup, we put the page
 * at the back of queue every time a lookup for it is done.  If the
 * page is in front of this global queue and we're short of memory, 
 * it's a candidate for pageout.
 */
static struct pglist vmpage_lruqueue;
static unsigned vmpage_onqueue;

static int
pg_compare_key(void *ctx, const void *n, const void *key)
{
	voff_t a = ((const struct vm_page *)n)->offset;
	voff_t b = *(const voff_t *)key;

	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	else
		return 0;
}

static int
pg_compare_nodes(void *ctx, const void *n1, const void *n2)
{

	return pg_compare_key(ctx, n1, &((const struct vm_page *)n2)->offset);
}

const rb_tree_ops_t uvm_page_tree_ops = {
	.rbto_compare_nodes = pg_compare_nodes,
	.rbto_compare_key = pg_compare_key,
	.rbto_node_offset = offsetof(struct vm_page, rb_node),
	.rbto_context = NULL
};

/*
 * vm pages 
 */

static int
pgctor(void *arg, void *obj, int flags)
{
	struct vm_page *pg = obj;

	memset(pg, 0, sizeof(*pg));
	pg->uanon = rump_hypermalloc(PAGE_SIZE, PAGE_SIZE,
	    (flags & PR_WAITOK) == PR_WAITOK, "pgalloc");
	return pg->uanon == NULL;
}

static void
pgdtor(void *arg, void *obj)
{
	struct vm_page *pg = obj;

	rump_hyperfree(pg->uanon, PAGE_SIZE);
}

static struct pool_cache pagecache;

/*
 * Called with the object locked.  We don't support anons.
 */
struct vm_page *
uvm_pagealloc_strat(struct uvm_object *uobj, voff_t off, struct vm_anon *anon,
	int flags, int strat, int free_list)
{
	struct vm_page *pg;

	KASSERT(uobj && mutex_owned(uobj->vmobjlock));
	KASSERT(anon == NULL);

	pg = pool_cache_get(&pagecache, PR_NOWAIT);
	if (__predict_false(pg == NULL)) {
		return NULL;
	}

	pg->offset = off;
	pg->uobject = uobj;

	pg->flags = PG_CLEAN|PG_BUSY|PG_FAKE;
	if (flags & UVM_PGA_ZERO) {
		uvm_pagezero(pg);
	}

	TAILQ_INSERT_TAIL(&uobj->memq, pg, listq.queue);
	(void)rb_tree_insert_node(&uobj->rb_tree, pg);

	/*
	 * Don't put anons on the LRU page queue.  We can't flush them
	 * (there's no concept of swap in a rump kernel), so no reason
	 * to bother with them.
	 */
	if (!UVM_OBJ_IS_AOBJ(uobj)) {
		atomic_inc_uint(&vmpage_onqueue);
		mutex_enter(&uvm_pageqlock);
		TAILQ_INSERT_TAIL(&vmpage_lruqueue, pg, pageq.queue);
		mutex_exit(&uvm_pageqlock);
	}

	uobj->uo_npages++;

	return pg;
}

/*
 * Release a page.
 *
 * Called with the vm object locked.
 */
void
uvm_pagefree(struct vm_page *pg)
{
	struct uvm_object *uobj = pg->uobject;

	KASSERT(mutex_owned(&uvm_pageqlock));
	KASSERT(mutex_owned(uobj->vmobjlock));

	if (pg->flags & PG_WANTED)
		wakeup(pg);

	TAILQ_REMOVE(&uobj->memq, pg, listq.queue);

	uobj->uo_npages--;
	rb_tree_remove_node(&uobj->rb_tree, pg);

	if (!UVM_OBJ_IS_AOBJ(uobj)) {
		TAILQ_REMOVE(&vmpage_lruqueue, pg, pageq.queue);
		atomic_dec_uint(&vmpage_onqueue);
	}

	pool_cache_put(&pagecache, pg);
}

void
uvm_pagezero(struct vm_page *pg)
{

	pg->flags &= ~PG_CLEAN;
	memset((void *)pg->uanon, 0, PAGE_SIZE);
}

/*
 * uvm_page_locked_p: return true if object associated with page is
 * locked.  this is a weak check for runtime assertions only.
 */

bool
uvm_page_locked_p(struct vm_page *pg)
{

	return mutex_owned(pg->uobject->vmobjlock);
}

/*
 * Misc routines
 */

static kmutex_t pagermtx;

void
uvm_init(void)
{
	char buf[64];

	if (rumpuser_getparam("RUMP_MEMLIMIT", buf, sizeof(buf)) == 0) {
		unsigned long tmp;
		char *ep;
		int mult;

		tmp = strtoul(buf, &ep, 10);
		if (strlen(ep) > 1)
			panic("uvm_init: invalid RUMP_MEMLIMIT: %s", buf);

		/* mini-dehumanize-number */
		mult = 1;
		switch (*ep) {
		case 'k':
			mult = 1024;
			break;
		case 'm':
			mult = 1024*1024;
			break;
		case 'g':
			mult = 1024*1024*1024;
			break;
		case 0:
			break;
		default:
			panic("uvm_init: invalid RUMP_MEMLIMIT: %s", buf);
		}
		rump_physmemlimit = tmp * mult;

		if (rump_physmemlimit / mult != tmp)
			panic("uvm_init: RUMP_MEMLIMIT overflow: %s", buf);

		/* reserve some memory for the pager */
		if (rump_physmemlimit <= PDRESERVE)
			panic("uvm_init: system reserves %d bytes of mem, "
			    "only %lu bytes given",
			    PDRESERVE, rump_physmemlimit);
		pdlimit = rump_physmemlimit;
		rump_physmemlimit -= PDRESERVE;

		if (pdlimit < 1024*1024)
			printf("uvm_init: WARNING: <1MB RAM limit, "
			    "hope you know what you're doing\n");

#define HUMANIZE_BYTES 9
		CTASSERT(sizeof(buf) >= HUMANIZE_BYTES);
		format_bytes(buf, HUMANIZE_BYTES, rump_physmemlimit);
#undef HUMANIZE_BYTES
		dddlim = 9 * (rump_physmemlimit / 10);
	} else {
		strlcpy(buf, "unlimited (host limit)", sizeof(buf));
	}
	aprint_verbose("total memory = %s\n", buf);

	TAILQ_INIT(&vmpage_lruqueue);

	if (rump_physmemlimit == RUMPMEM_UNLIMITED) {
		uvmexp.npages = physmem;
	} else {
		uvmexp.npages = pdlimit >> PAGE_SHIFT;
		uvmexp.reserve_pagedaemon = PDRESERVE >> PAGE_SHIFT;
		uvmexp.freetarg = (rump_physmemlimit-dddlim) >> PAGE_SHIFT;
	}
	/*
	 * uvmexp.free is not used internally or updated.  The reason is
	 * that the memory hypercall allocator is allowed to allocate
	 * non-page sized chunks.  We use a byte count in curphysmem
	 * instead.
	 */
	uvmexp.free = uvmexp.npages;

#ifndef __uvmexp_pagesize
	uvmexp.pagesize = PAGE_SIZE;
	uvmexp.pagemask = PAGE_MASK;
	uvmexp.pageshift = PAGE_SHIFT;
#else
#define FAKE_PAGE_SHIFT 12
	uvmexp.pageshift = FAKE_PAGE_SHIFT;
	uvmexp.pagesize = 1<<FAKE_PAGE_SHIFT;
	uvmexp.pagemask = (1<<FAKE_PAGE_SHIFT)-1;
#undef FAKE_PAGE_SHIFT
#endif

	mutex_init(&pagermtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&uvm_pageqlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&uvm_swap_data_lock, MUTEX_DEFAULT, IPL_NONE);

	/* just to appease linkage */
	mutex_init(&uvm_fpageqlock, MUTEX_SPIN, IPL_VM);

	mutex_init(&pdaemonmtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&pdaemoncv, "pdaemon");
	cv_init(&oomwait, "oomwait");

	module_map = &module_map_store;

	kernel_map->pmap = pmap_kernel();

	pool_subsystem_init();

	kmem_arena = vmem_create("kmem", 0, 1024*1024, PAGE_SIZE,
	    NULL, NULL, NULL,
	    0, VM_NOSLEEP | VM_BOOTSTRAP, IPL_VM);

	vmem_subsystem_init(kmem_arena);

	kmem_va_arena = vmem_create("kva", 0, 0, PAGE_SIZE,
	    vmem_alloc, vmem_free, kmem_arena,
	    8 * PAGE_SIZE, VM_NOSLEEP | VM_BOOTSTRAP, IPL_VM);

	pool_cache_bootstrap(&pagecache, sizeof(struct vm_page), 0, 0, 0,
	    "page$", NULL, IPL_NONE, pgctor, pgdtor, NULL);

	/* create vmspace used by local clients */
	rump_vmspace_local = kmem_zalloc(sizeof(*rump_vmspace_local), KM_SLEEP);
	uvmspace_init(rump_vmspace_local, &rump_pmap_local, 0, 0, false);
}

void
uvmspace_init(struct vmspace *vm, struct pmap *pmap, vaddr_t vmin, vaddr_t vmax,
    bool topdown)
{

	vm->vm_map.pmap = pmap;
	vm->vm_refcnt = 1;
}

void
uvm_pagewire(struct vm_page *pg)
{

	/* nada */
}

void
uvm_pageunwire(struct vm_page *pg)
{

	/* nada */
}

/* where's your schmonz now? */
#define PUNLIMIT(a)	\
p->p_rlimit[a].rlim_cur = p->p_rlimit[a].rlim_max = RLIM_INFINITY;
void
uvm_init_limits(struct proc *p)
{

#ifndef DFLSSIZ
#define DFLSSIZ (16*1024*1024)
#endif
	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	PUNLIMIT(RLIMIT_DATA);
	PUNLIMIT(RLIMIT_RSS);
	PUNLIMIT(RLIMIT_AS);
	/* nice, cascade */
}
#undef PUNLIMIT

/*
 * This satisfies the "disgusting mmap hack" used by proplib.
 */
int
uvm_mmap_anon(struct proc *p, void **addrp, size_t size)
{
	int error;

	/* no reason in particular, but cf. uvm_default_mapaddr() */
	if (*addrp != NULL)
		panic("uvm_mmap() variant unsupported");

	if (RUMP_LOCALPROC_P(curproc)) {
		error = rumpuser_anonmmap(NULL, size, 0, 0, addrp);
	} else {
		error = rump_sysproxy_anonmmap(RUMP_SPVM2CTL(p->p_vmspace),
		    size, addrp);
	}
	return error;
}

/*
 * Stubs for things referenced from vfs_vnode.c but not used.
 */
const dev_t zerodev;

struct uvm_object *
udv_attach(dev_t device, vm_prot_t accessprot, voff_t off, vsize_t size)
{
	return NULL;
}

struct pagerinfo {
	vaddr_t pgr_kva;
	int pgr_npages;
	struct vm_page **pgr_pgs;
	bool pgr_read;

	LIST_ENTRY(pagerinfo) pgr_entries;
};
static LIST_HEAD(, pagerinfo) pagerlist = LIST_HEAD_INITIALIZER(pagerlist);

/*
 * Pager "map" in routine.  Instead of mapping, we allocate memory
 * and copy page contents there.  The reason for copying instead of
 * mapping is simple: we do not assume we are running on virtual
 * memory.  Even if we could emulate virtual memory in some envs
 * such as userspace, copying is much faster than trying to awkardly
 * cope with remapping (see "Design and Implementation" pp.95-98).
 * The downside of the approach is that the pager requires MAXPHYS
 * free memory to perform paging, but short of virtual memory or
 * making the pager do I/O in page-sized chunks we cannot do much
 * about that.
 */
vaddr_t
uvm_pagermapin(struct vm_page **pgs, int npages, int flags)
{
	struct pagerinfo *pgri;
	vaddr_t curkva;
	int i;

	/* allocate structures */
	pgri = kmem_alloc(sizeof(*pgri), KM_SLEEP);
	pgri->pgr_kva = (vaddr_t)kmem_alloc(npages * PAGE_SIZE, KM_SLEEP);
	pgri->pgr_npages = npages;
	pgri->pgr_pgs = kmem_alloc(sizeof(struct vm_page *) * npages, KM_SLEEP);
	pgri->pgr_read = (flags & UVMPAGER_MAPIN_READ) != 0;

	/* copy contents to "mapped" memory */
	for (i = 0, curkva = pgri->pgr_kva;
	    i < npages;
	    i++, curkva += PAGE_SIZE) {
		/*
		 * We need to copy the previous contents of the pages to
		 * the window even if we are reading from the
		 * device, since the device might not fill the contents of
		 * the full mapped range and we will end up corrupting
		 * data when we unmap the window.
		 */
		memcpy((void*)curkva, pgs[i]->uanon, PAGE_SIZE);
		pgri->pgr_pgs[i] = pgs[i];
	}

	mutex_enter(&pagermtx);
	LIST_INSERT_HEAD(&pagerlist, pgri, pgr_entries);
	mutex_exit(&pagermtx);

	return pgri->pgr_kva;
}

/*
 * map out the pager window.  return contents from VA to page storage
 * and free structures.
 *
 * Note: does not currently support partial frees
 */
void
uvm_pagermapout(vaddr_t kva, int npages)
{
	struct pagerinfo *pgri;
	vaddr_t curkva;
	int i;

	mutex_enter(&pagermtx);
	LIST_FOREACH(pgri, &pagerlist, pgr_entries) {
		if (pgri->pgr_kva == kva)
			break;
	}
	KASSERT(pgri);
	if (pgri->pgr_npages != npages)
		panic("uvm_pagermapout: partial unmapping not supported");
	LIST_REMOVE(pgri, pgr_entries);
	mutex_exit(&pagermtx);

	if (pgri->pgr_read) {
		for (i = 0, curkva = pgri->pgr_kva;
		    i < pgri->pgr_npages;
		    i++, curkva += PAGE_SIZE) {
			memcpy(pgri->pgr_pgs[i]->uanon,(void*)curkva,PAGE_SIZE);
		}
	}

	kmem_free(pgri->pgr_pgs, npages * sizeof(struct vm_page *));
	kmem_free((void*)pgri->pgr_kva, npages * PAGE_SIZE);
	kmem_free(pgri, sizeof(*pgri));
}

/*
 * convert va in pager window to page structure.
 * XXX: how expensive is this (global lock, list traversal)?
 */
struct vm_page *
uvm_pageratop(vaddr_t va)
{
	struct pagerinfo *pgri;
	struct vm_page *pg = NULL;
	int i;

	mutex_enter(&pagermtx);
	LIST_FOREACH(pgri, &pagerlist, pgr_entries) {
		if (pgri->pgr_kva <= va
		    && va < pgri->pgr_kva + pgri->pgr_npages*PAGE_SIZE)
			break;
	}
	if (pgri) {
		i = (va - pgri->pgr_kva) >> PAGE_SHIFT;
		pg = pgri->pgr_pgs[i];
	}
	mutex_exit(&pagermtx);

	return pg;
}

/*
 * Called with the vm object locked.
 *
 * Put vnode object pages at the end of the access queue to indicate
 * they have been recently accessed and should not be immediate
 * candidates for pageout.  Do not do this for lookups done by
 * the pagedaemon to mimic pmap_kentered mappings which don't track
 * access information.
 */
struct vm_page *
uvm_pagelookup(struct uvm_object *uobj, voff_t off)
{
	struct vm_page *pg;
	bool ispagedaemon = curlwp == uvm.pagedaemon_lwp;

	pg = rb_tree_find_node(&uobj->rb_tree, &off);
	if (pg && !UVM_OBJ_IS_AOBJ(pg->uobject) && !ispagedaemon) {
		mutex_enter(&uvm_pageqlock);
		TAILQ_REMOVE(&vmpage_lruqueue, pg, pageq.queue);
		TAILQ_INSERT_TAIL(&vmpage_lruqueue, pg, pageq.queue);
		mutex_exit(&uvm_pageqlock);
	}

	return pg;
}

void
uvm_page_unbusy(struct vm_page **pgs, int npgs)
{
	struct vm_page *pg;
	int i;

	KASSERT(npgs > 0);
	KASSERT(mutex_owned(pgs[0]->uobject->vmobjlock));

	for (i = 0; i < npgs; i++) {
		pg = pgs[i];
		if (pg == NULL)
			continue;

		KASSERT(pg->flags & PG_BUSY);
		if (pg->flags & PG_WANTED)
			wakeup(pg);
		if (pg->flags & PG_RELEASED)
			uvm_pagefree(pg);
		else
			pg->flags &= ~(PG_WANTED|PG_BUSY);
	}
}

void
uvm_estimatepageable(int *active, int *inactive)
{

	/* XXX: guessing game */
	*active = 1024;
	*inactive = 1024;
}

bool
vm_map_starved_p(struct vm_map *map)
{

	if (map->flags & VM_MAP_WANTVA)
		return true;

	return false;
}

int
uvm_loan(struct vm_map *map, vaddr_t start, vsize_t len, void *v, int flags)
{

	panic("%s: unimplemented", __func__);
}

void
uvm_unloan(void *v, int npages, int flags)
{

	panic("%s: unimplemented", __func__);
}

int
uvm_loanuobjpages(struct uvm_object *uobj, voff_t pgoff, int orignpages,
	struct vm_page **opp)
{

	return EBUSY;
}

struct vm_page *
uvm_loanbreak(struct vm_page *pg)
{

	panic("%s: unimplemented", __func__);
}

void
ubc_purge(struct uvm_object *uobj)
{

}

vaddr_t
uvm_default_mapaddr(struct proc *p, vaddr_t base, vsize_t sz)
{

	return 0;
}

int
uvm_map_protect(struct vm_map *map, vaddr_t start, vaddr_t end,
	vm_prot_t prot, bool set_max)
{

	return EOPNOTSUPP;
}

/*
 * UVM km
 */

vaddr_t
uvm_km_alloc(struct vm_map *map, vsize_t size, vsize_t align, uvm_flag_t flags)
{
	void *rv, *desired = NULL;
	int alignbit, error;

#ifdef __x86_64__
	/*
	 * On amd64, allocate all module memory from the lowest 2GB.
	 * This is because NetBSD kernel modules are compiled
	 * with -mcmodel=kernel and reserve only 4 bytes for
	 * offsets.  If we load code compiled with -mcmodel=kernel
	 * anywhere except the lowest or highest 2GB, it will not
	 * work.  Since userspace does not have access to the highest
	 * 2GB, use the lowest 2GB.
	 * 
	 * Note: this assumes the rump kernel resides in
	 * the lowest 2GB as well.
	 *
	 * Note2: yes, it's a quick hack, but since this the only
	 * place where we care about the map we're allocating from,
	 * just use a simple "if" instead of coming up with a fancy
	 * generic solution.
	 */
	if (map == module_map) {
		desired = (void *)(0x80000000 - size);
	}
#endif

	if (__predict_false(map == module_map)) {
		alignbit = 0;
		if (align) {
			alignbit = ffs(align)-1;
		}
		error = rumpuser_anonmmap(desired, size, alignbit,
		    flags & UVM_KMF_EXEC, &rv);
	} else {
		error = rumpuser_malloc(size, align, &rv);
	}

	if (error) {
		if (flags & (UVM_KMF_CANFAIL | UVM_KMF_NOWAIT))
			return 0;
		else
			panic("uvm_km_alloc failed");
	}

	if (flags & UVM_KMF_ZERO)
		memset(rv, 0, size);

	return (vaddr_t)rv;
}

void
uvm_km_free(struct vm_map *map, vaddr_t vaddr, vsize_t size, uvm_flag_t flags)
{

	if (__predict_false(map == module_map))
		rumpuser_unmap((void *)vaddr, size);
	else
		rumpuser_free((void *)vaddr, size);
}

struct vm_map *
uvm_km_suballoc(struct vm_map *map, vaddr_t *minaddr, vaddr_t *maxaddr,
	vsize_t size, int pageable, bool fixed, struct vm_map *submap)
{

	return (struct vm_map *)417416;
}

int
uvm_km_kmem_alloc(vmem_t *vm, vmem_size_t size, vm_flag_t flags,
    vmem_addr_t *addr)
{
	vaddr_t va;
	va = (vaddr_t)rump_hypermalloc(size, PAGE_SIZE,
	    (flags & VM_SLEEP), "kmalloc");

	if (va) {
		*addr = va;
		return 0;
	} else {
		return ENOMEM;
	}
}

void
uvm_km_kmem_free(vmem_t *vm, vmem_addr_t addr, vmem_size_t size)
{

	rump_hyperfree((void *)addr, size);
}

/*
 * VM space locking routines.  We don't really have to do anything,
 * since the pages are always "wired" (both local and remote processes).
 */
int
uvm_vslock(struct vmspace *vs, void *addr, size_t len, vm_prot_t access)
{

	return 0;
}

void
uvm_vsunlock(struct vmspace *vs, void *addr, size_t len)
{

}

/*
 * For the local case the buffer mappers don't need to do anything.
 * For the remote case we need to reserve space and copy data in or
 * out, depending on B_READ/B_WRITE.
 */
int
vmapbuf(struct buf *bp, vsize_t len)
{
	int error = 0;

	bp->b_saveaddr = bp->b_data;

	/* remote case */
	if (!RUMP_LOCALPROC_P(curproc)) {
		bp->b_data = rump_hypermalloc(len, 0, true, "vmapbuf");
		if (BUF_ISWRITE(bp)) {
			error = copyin(bp->b_saveaddr, bp->b_data, len);
			if (error) {
				rump_hyperfree(bp->b_data, len);
				bp->b_data = bp->b_saveaddr;
				bp->b_saveaddr = 0;
			}
		}
	}

	return error;
}

void
vunmapbuf(struct buf *bp, vsize_t len)
{

	/* remote case */
	if (!RUMP_LOCALPROC_P(bp->b_proc)) {
		if (BUF_ISREAD(bp)) {
			bp->b_error = copyout_proc(bp->b_proc,
			    bp->b_data, bp->b_saveaddr, len);
		}
		rump_hyperfree(bp->b_data, len);
	}

	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

void
uvmspace_addref(struct vmspace *vm)
{

	/*
	 * No dynamically allocated vmspaces exist.
	 */
}

void
uvmspace_free(struct vmspace *vm)
{

	/* nothing for now */
}

/*
 * page life cycle stuff.  it really doesn't exist, so just stubs.
 */

void
uvm_pageactivate(struct vm_page *pg)
{

	/* nada */
}

void
uvm_pagedeactivate(struct vm_page *pg)
{

	/* nada */
}

void
uvm_pagedequeue(struct vm_page *pg)
{

	/* nada*/
}

void
uvm_pageenqueue(struct vm_page *pg)
{

	/* nada */
}

void
uvmpdpol_anfree(struct vm_anon *an)
{

	/* nada */
}

/*
 * Physical address accessors.
 */

struct vm_page *
uvm_phys_to_vm_page(paddr_t pa)
{

	return NULL;
}

paddr_t
uvm_vm_page_to_phys(const struct vm_page *pg)
{

	return 0;
}

vaddr_t
uvm_uarea_alloc(void)
{

	/* non-zero */
	return (vaddr_t)11;
}

void
uvm_uarea_free(vaddr_t uarea)
{

	/* nata, so creamy */
}

/*
 * Routines related to the Page Baroness.
 */

void
uvm_wait(const char *msg)
{

	if (__predict_false(rump_threads == 0))
		panic("pagedaemon missing (RUMP_THREADS = 0)");

	if (curlwp == uvm.pagedaemon_lwp) {
		/* is it possible for us to later get memory? */
		if (!uvmexp.paging)
			panic("pagedaemon out of memory");
	}

	mutex_enter(&pdaemonmtx);
	pdaemon_waiters++;
	cv_signal(&pdaemoncv);
	cv_wait(&oomwait, &pdaemonmtx);
	mutex_exit(&pdaemonmtx);
}

void
uvm_pageout_start(int npages)
{

	mutex_enter(&pdaemonmtx);
	uvmexp.paging += npages;
	mutex_exit(&pdaemonmtx);
}

void
uvm_pageout_done(int npages)
{

	if (!npages)
		return;

	mutex_enter(&pdaemonmtx);
	KASSERT(uvmexp.paging >= npages);
	uvmexp.paging -= npages;

	if (pdaemon_waiters) {
		pdaemon_waiters = 0;
		cv_broadcast(&oomwait);
	}
	mutex_exit(&pdaemonmtx);
}

static bool
processpage(struct vm_page *pg, bool *lockrunning)
{
	struct uvm_object *uobj;

	uobj = pg->uobject;
	if (mutex_tryenter(uobj->vmobjlock)) {
		if ((pg->flags & PG_BUSY) == 0) {
			mutex_exit(&uvm_pageqlock);
			uobj->pgops->pgo_put(uobj, pg->offset,
			    pg->offset + PAGE_SIZE,
			    PGO_CLEANIT|PGO_FREE);
			KASSERT(!mutex_owned(uobj->vmobjlock));
			return true;
		} else {
			mutex_exit(uobj->vmobjlock);
		}
	} else if (*lockrunning == false && ncpu > 1) {
		CPU_INFO_ITERATOR cii;
		struct cpu_info *ci;
		struct lwp *l;

		l = mutex_owner(uobj->vmobjlock);
		for (CPU_INFO_FOREACH(cii, ci)) {
			if (ci->ci_curlwp == l) {
				*lockrunning = true;
				break;
			}
		}
	}

	return false;
}

/*
 * The Diabolical pageDaemon Director (DDD).
 *
 * This routine can always use better heuristics.
 */
void
uvm_pageout(void *arg)
{
	struct vm_page *pg;
	struct pool *pp, *pp_first;
	int cleaned, skip, skipped;
	bool succ;
	bool lockrunning;

	mutex_enter(&pdaemonmtx);
	for (;;) {
		if (!NEED_PAGEDAEMON()) {
			kernel_map->flags &= ~VM_MAP_WANTVA;
		}

		if (pdaemon_waiters) {
			pdaemon_waiters = 0;
			cv_broadcast(&oomwait);
		}

		cv_wait(&pdaemoncv, &pdaemonmtx);
		uvmexp.pdwoke++;

		/* tell the world that we are hungry */
		kernel_map->flags |= VM_MAP_WANTVA;
		mutex_exit(&pdaemonmtx);

		/*
		 * step one: reclaim the page cache.  this should give
		 * us the biggest earnings since whole pages are released
		 * into backing memory.
		 */
		pool_cache_reclaim(&pagecache);
		if (!NEED_PAGEDAEMON()) {
			mutex_enter(&pdaemonmtx);
			continue;
		}

		/*
		 * Ok, so that didn't help.  Next, try to hunt memory
		 * by pushing out vnode pages.  The pages might contain
		 * useful cached data, but we need the memory.
		 */
		cleaned = 0;
		skip = 0;
		lockrunning = false;
 again:
		mutex_enter(&uvm_pageqlock);
		while (cleaned < PAGEDAEMON_OBJCHUNK) {
			skipped = 0;
			TAILQ_FOREACH(pg, &vmpage_lruqueue, pageq.queue) {

				/*
				 * skip over pages we _might_ have tried
				 * to handle earlier.  they might not be
				 * exactly the same ones, but I'm not too
				 * concerned.
				 */
				while (skipped++ < skip)
					continue;

				if (processpage(pg, &lockrunning)) {
					cleaned++;
					goto again;
				}

				skip++;
			}
			break;
		}
		mutex_exit(&uvm_pageqlock);

		/*
		 * Ok, someone is running with an object lock held.
		 * We want to yield the host CPU to make sure the
		 * thread is not parked on the host.  Since sched_yield()
		 * doesn't appear to do anything on NetBSD, nanosleep
		 * for the smallest possible time and hope we're back in
		 * the game soon.
		 */
		if (cleaned == 0 && lockrunning) {
			rumpuser_clock_sleep(RUMPUSER_CLOCK_RELWALL, 0, 1);

			lockrunning = false;
			skip = 0;

			/* and here we go again */
			goto again;
		}

		/*
		 * And of course we need to reclaim the page cache
		 * again to actually release memory.
		 */
		pool_cache_reclaim(&pagecache);
		if (!NEED_PAGEDAEMON()) {
			mutex_enter(&pdaemonmtx);
			continue;
		}

		/*
		 * And then drain the pools.  Wipe them out ... all of them.
		 */
		for (pp_first = NULL;;) {
			rump_vfs_drainbufs(10 /* XXX: estimate! */);

			succ = pool_drain(&pp);
			if (succ || pp == pp_first)
				break;

			if (pp_first == NULL)
				pp_first = pp;
		}

		/*
		 * Need to use PYEC on our bag of tricks.
		 * Unfortunately, the wife just borrowed it.
		 */

		mutex_enter(&pdaemonmtx);
		if (!succ && cleaned == 0 && pdaemon_waiters &&
		    uvmexp.paging == 0) {
			rumpuser_dprintf("pagedaemoness: failed to reclaim "
			    "memory ... sleeping (deadlock?)\n");
			kpause("pddlk", false, hz, &pdaemonmtx);
		}
	}

	panic("you can swap out any time you like, but you can never leave");
}

void
uvm_kick_pdaemon()
{

	/*
	 * Wake up the diabolical pagedaemon director if we are over
	 * 90% of the memory limit.  This is a complete and utter
	 * stetson-harrison decision which you are allowed to finetune.
	 * Don't bother locking.  If we have some unflushed caches,
	 * other waker-uppers will deal with the issue.
	 */
	if (NEED_PAGEDAEMON()) {
		cv_signal(&pdaemoncv);
	}
}

void *
rump_hypermalloc(size_t howmuch, int alignment, bool waitok, const char *wmsg)
{
	const unsigned long thelimit =
	    curlwp == uvm.pagedaemon_lwp ? pdlimit : rump_physmemlimit;
	unsigned long newmem;
	void *rv;
	int error;

	uvm_kick_pdaemon(); /* ouch */

	/* first we must be within the limit */
 limitagain:
	if (thelimit != RUMPMEM_UNLIMITED) {
		newmem = atomic_add_long_nv(&curphysmem, howmuch);
		if (newmem > thelimit) {
			newmem = atomic_add_long_nv(&curphysmem, -howmuch);
			if (!waitok) {
				return NULL;
			}
			uvm_wait(wmsg);
			goto limitagain;
		}
	}

	/* second, we must get something from the backend */
 again:
	error = rumpuser_malloc(howmuch, alignment, &rv);
	if (__predict_false(error && waitok)) {
		uvm_wait(wmsg);
		goto again;
	}

	return rv;
}

void
rump_hyperfree(void *what, size_t size)
{

	if (rump_physmemlimit != RUMPMEM_UNLIMITED) {
		atomic_add_long(&curphysmem, -size);
	}
	rumpuser_free(what, size);
}
