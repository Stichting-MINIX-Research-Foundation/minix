/*	$NetBSD: vfs_bio.c,v 1.256 2015/08/24 22:50:32 pooka Exp $	*/

/*-
 * Copyright (c) 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, and by Wasabi Systems, Inc.
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

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)vfs_bio.c	8.6 (Berkeley) 1/11/94
 */

/*-
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *	@(#)vfs_bio.c	8.6 (Berkeley) 1/11/94
 */

/*
 * The buffer cache subsystem.
 *
 * Some references:
 *	Bach: The Design of the UNIX Operating System (Prentice Hall, 1986)
 *	Leffler, et al.: The Design and Implementation of the 4.3BSD
 *		UNIX Operating System (Addison Welley, 1989)
 *
 * Locking
 *
 * There are three locks:
 * - bufcache_lock: protects global buffer cache state.
 * - BC_BUSY: a long term per-buffer lock.
 * - buf_t::b_objlock: lock on completion (biowait vs biodone).
 *
 * For buffers associated with vnodes (a most common case) b_objlock points
 * to the vnode_t::v_interlock.  Otherwise, it points to generic buffer_lock.
 *
 * Lock order:
 *	bufcache_lock ->
 *		buf_t::b_objlock
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_bio.c,v 1.256 2015/08/24 22:50:32 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_bufcache.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/fstrans.h>
#include <sys/intr.h>
#include <sys/cpu.h>
#include <sys/wapbl.h>
#include <sys/bitops.h>
#include <sys/cprng.h>

#include <uvm/uvm.h>	/* extern struct uvm uvm */

#include <miscfs/specfs/specdev.h>

#ifndef	BUFPAGES
# define BUFPAGES 0
#endif

#ifdef BUFCACHE
# if (BUFCACHE < 5) || (BUFCACHE > 95)
#  error BUFCACHE is not between 5 and 95
# endif
#else
# define BUFCACHE 15
#endif

u_int	nbuf;			/* desired number of buffer headers */
u_int	bufpages = BUFPAGES;	/* optional hardwired count */
u_int	bufcache = BUFCACHE;	/* max % of RAM to use for buffer cache */

/* Function prototypes */
struct bqueue;

static void buf_setwm(void);
static int buf_trim(void);
static void *bufpool_page_alloc(struct pool *, int);
static void bufpool_page_free(struct pool *, void *);
static buf_t *bio_doread(struct vnode *, daddr_t, int, int);
static buf_t *getnewbuf(int, int, int);
static int buf_lotsfree(void);
static int buf_canrelease(void);
static u_long buf_mempoolidx(u_long);
static u_long buf_roundsize(u_long);
static void *buf_alloc(size_t);
static void buf_mrelease(void *, size_t);
static void binsheadfree(buf_t *, struct bqueue *);
static void binstailfree(buf_t *, struct bqueue *);
#ifdef DEBUG
static int checkfreelist(buf_t *, struct bqueue *, int);
#endif
static void biointr(void *);
static void biodone2(buf_t *);
static void bref(buf_t *);
static void brele(buf_t *);
static void sysctl_kern_buf_setup(void);
static void sysctl_vm_buf_setup(void);

/*
 * Definitions for the buffer hash lists.
 */
#define	BUFHASH(dvp, lbn)	\
	(&bufhashtbl[(((long)(dvp) >> 8) + (int)(lbn)) & bufhash])
LIST_HEAD(bufhashhdr, buf) *bufhashtbl, invalhash;
u_long	bufhash;
struct bqueue bufqueues[BQUEUES];

static kcondvar_t needbuffer_cv;

/*
 * Buffer queue lock.
 */
kmutex_t bufcache_lock;
kmutex_t buffer_lock;

/* Software ISR for completed transfers. */
static void *biodone_sih;

/* Buffer pool for I/O buffers. */
static pool_cache_t buf_cache;
static pool_cache_t bufio_cache;

#define MEMPOOL_INDEX_OFFSET (ilog2(DEV_BSIZE))	/* smallest pool is 512 bytes */
#define NMEMPOOLS (ilog2(MAXBSIZE) - MEMPOOL_INDEX_OFFSET + 1)
__CTASSERT((1 << (NMEMPOOLS + MEMPOOL_INDEX_OFFSET - 1)) == MAXBSIZE);

/* Buffer memory pools */
static struct pool bmempools[NMEMPOOLS];

static struct vm_map *buf_map;

/*
 * Buffer memory pool allocator.
 */
static void *
bufpool_page_alloc(struct pool *pp, int flags)
{

	return (void *)uvm_km_alloc(buf_map,
	    MAXBSIZE, MAXBSIZE,
	    ((flags & PR_WAITOK) ? 0 : UVM_KMF_NOWAIT|UVM_KMF_TRYLOCK)
	    | UVM_KMF_WIRED);
}

static void
bufpool_page_free(struct pool *pp, void *v)
{

	uvm_km_free(buf_map, (vaddr_t)v, MAXBSIZE, UVM_KMF_WIRED);
}

static struct pool_allocator bufmempool_allocator = {
	.pa_alloc = bufpool_page_alloc,
	.pa_free = bufpool_page_free,
	.pa_pagesz = MAXBSIZE,
};

/* Buffer memory management variables */
u_long bufmem_valimit;
u_long bufmem_hiwater;
u_long bufmem_lowater;
u_long bufmem;

/*
 * MD code can call this to set a hard limit on the amount
 * of virtual memory used by the buffer cache.
 */
int
buf_setvalimit(vsize_t sz)
{

	/* We need to accommodate at least NMEMPOOLS of MAXBSIZE each */
	if (sz < NMEMPOOLS * MAXBSIZE)
		return EINVAL;

	bufmem_valimit = sz;
	return 0;
}

static void
buf_setwm(void)
{

	bufmem_hiwater = buf_memcalc();
	/* lowater is approx. 2% of memory (with bufcache = 15) */
#define	BUFMEM_WMSHIFT	3
#define	BUFMEM_HIWMMIN	(64 * 1024 << BUFMEM_WMSHIFT)
	if (bufmem_hiwater < BUFMEM_HIWMMIN)
		/* Ensure a reasonable minimum value */
		bufmem_hiwater = BUFMEM_HIWMMIN;
	bufmem_lowater = bufmem_hiwater >> BUFMEM_WMSHIFT;
}

#ifdef DEBUG
int debug_verify_freelist = 0;
static int
checkfreelist(buf_t *bp, struct bqueue *dp, int ison)
{
	buf_t *b;

	if (!debug_verify_freelist)
		return 1;

	TAILQ_FOREACH(b, &dp->bq_queue, b_freelist) {
		if (b == bp)
			return ison ? 1 : 0;
	}

	return ison ? 0 : 1;
}
#endif

/*
 * Insq/Remq for the buffer hash lists.
 * Call with buffer queue locked.
 */
static void
binsheadfree(buf_t *bp, struct bqueue *dp)
{

	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(bp->b_freelistindex == -1);
	TAILQ_INSERT_HEAD(&dp->bq_queue, bp, b_freelist);
	dp->bq_bytes += bp->b_bufsize;
	bp->b_freelistindex = dp - bufqueues;
}

static void
binstailfree(buf_t *bp, struct bqueue *dp)
{

	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(bp->b_freelistindex == -1);
	TAILQ_INSERT_TAIL(&dp->bq_queue, bp, b_freelist);
	dp->bq_bytes += bp->b_bufsize;
	bp->b_freelistindex = dp - bufqueues;
}

void
bremfree(buf_t *bp)
{
	struct bqueue *dp;
	int bqidx = bp->b_freelistindex;

	KASSERT(mutex_owned(&bufcache_lock));

	KASSERT(bqidx != -1);
	dp = &bufqueues[bqidx];
	KDASSERT(checkfreelist(bp, dp, 1));
	KASSERT(dp->bq_bytes >= bp->b_bufsize);
	TAILQ_REMOVE(&dp->bq_queue, bp, b_freelist);
	dp->bq_bytes -= bp->b_bufsize;

	/* For the sysctl helper. */
	if (bp == dp->bq_marker)
		dp->bq_marker = NULL;

#if defined(DIAGNOSTIC)
	bp->b_freelistindex = -1;
#endif /* defined(DIAGNOSTIC) */
}

/*
 * Add a reference to an buffer structure that came from buf_cache.
 */
static inline void
bref(buf_t *bp)
{

	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(bp->b_refcnt > 0);

	bp->b_refcnt++;
}

/*
 * Free an unused buffer structure that came from buf_cache.
 */
static inline void
brele(buf_t *bp)
{

	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(bp->b_refcnt > 0);

	if (bp->b_refcnt-- == 1) {
		buf_destroy(bp);
#ifdef DEBUG
		memset((char *)bp, 0, sizeof(*bp));
#endif
		pool_cache_put(buf_cache, bp);
	}
}

/*
 * note that for some ports this is used by pmap bootstrap code to
 * determine kva size.
 */
u_long
buf_memcalc(void)
{
	u_long n;
	vsize_t mapsz = 0;

	/*
	 * Determine the upper bound of memory to use for buffers.
	 *
	 *	- If bufpages is specified, use that as the number
	 *	  pages.
	 *
	 *	- Otherwise, use bufcache as the percentage of
	 *	  physical memory.
	 */
	if (bufpages != 0) {
		n = bufpages;
	} else {
		if (bufcache < 5) {
			printf("forcing bufcache %d -> 5", bufcache);
			bufcache = 5;
		}
		if (bufcache > 95) {
			printf("forcing bufcache %d -> 95", bufcache);
			bufcache = 95;
		}
		if (buf_map != NULL)
			mapsz = vm_map_max(buf_map) - vm_map_min(buf_map);
		n = calc_cache_size(mapsz, bufcache,
		    (buf_map != kernel_map) ? 100 : BUFCACHE_VA_MAXPCT)
		    / PAGE_SIZE;
	}

	n <<= PAGE_SHIFT;
	if (bufmem_valimit != 0 && n > bufmem_valimit)
		n = bufmem_valimit;

	return (n);
}

/*
 * Initialize buffers and hash links for buffers.
 */
void
bufinit(void)
{
	struct bqueue *dp;
	int use_std;
	u_int i;
	extern void (*biodone_vfs)(buf_t *);

	biodone_vfs = biodone;

	mutex_init(&bufcache_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&buffer_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&needbuffer_cv, "needbuf");

	if (bufmem_valimit != 0) {
		vaddr_t minaddr = 0, maxaddr;
		buf_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
					  bufmem_valimit, 0, false, 0);
		if (buf_map == NULL)
			panic("bufinit: cannot allocate submap");
	} else
		buf_map = kernel_map;

	/*
	 * Initialize buffer cache memory parameters.
	 */
	bufmem = 0;
	buf_setwm();

	/* On "small" machines use small pool page sizes where possible */
	use_std = (physmem < atop(16*1024*1024));

	/*
	 * Also use them on systems that can map the pool pages using
	 * a direct-mapped segment.
	 */
#ifdef PMAP_MAP_POOLPAGE
	use_std = 1;
#endif

	buf_cache = pool_cache_init(sizeof(buf_t), 0, 0, 0,
	    "bufpl", NULL, IPL_SOFTBIO, NULL, NULL, NULL);
	bufio_cache = pool_cache_init(sizeof(buf_t), 0, 0, 0,
	    "biopl", NULL, IPL_BIO, NULL, NULL, NULL);

	for (i = 0; i < NMEMPOOLS; i++) {
		struct pool_allocator *pa;
		struct pool *pp = &bmempools[i];
		u_int size = 1 << (i + MEMPOOL_INDEX_OFFSET);
		char *name = kmem_alloc(8, KM_SLEEP); /* XXX: never freed */
		if (__predict_false(size >= 1048576))
			(void)snprintf(name, 8, "buf%um", size / 1048576);
		else if (__predict_true(size >= 1024))
			(void)snprintf(name, 8, "buf%uk", size / 1024);
		else
			(void)snprintf(name, 8, "buf%ub", size);
		pa = (size <= PAGE_SIZE && use_std)
			? &pool_allocator_nointr
			: &bufmempool_allocator;
		pool_init(pp, size, 0, 0, 0, name, pa, IPL_NONE);
		pool_setlowat(pp, 1);
		pool_sethiwat(pp, 1);
	}

	/* Initialize the buffer queues */
	for (dp = bufqueues; dp < &bufqueues[BQUEUES]; dp++) {
		TAILQ_INIT(&dp->bq_queue);
		dp->bq_bytes = 0;
	}

	/*
	 * Estimate hash table size based on the amount of memory we
	 * intend to use for the buffer cache. The average buffer
	 * size is dependent on our clients (i.e. filesystems).
	 *
	 * For now, use an empirical 3K per buffer.
	 */
	nbuf = (bufmem_hiwater / 1024) / 3;
	bufhashtbl = hashinit(nbuf, HASH_LIST, true, &bufhash);

	sysctl_kern_buf_setup();
	sysctl_vm_buf_setup();
}

void
bufinit2(void)
{

	biodone_sih = softint_establish(SOFTINT_BIO | SOFTINT_MPSAFE, biointr,
	    NULL);
	if (biodone_sih == NULL)
		panic("bufinit2: can't establish soft interrupt");
}

static int
buf_lotsfree(void)
{
	u_long guess;

	/* Always allocate if less than the low water mark. */
	if (bufmem < bufmem_lowater)
		return 1;

	/* Never allocate if greater than the high water mark. */
	if (bufmem > bufmem_hiwater)
		return 0;

	/* If there's anything on the AGE list, it should be eaten. */
	if (TAILQ_FIRST(&bufqueues[BQ_AGE].bq_queue) != NULL)
		return 0;

	/*
	 * The probabily of getting a new allocation is inversely
	 * proportional  to the current size of the cache above
	 * the low water mark.  Divide the total first to avoid overflows
	 * in the product.
	 */
	guess = cprng_fast32() % 16;

	if ((bufmem_hiwater - bufmem_lowater) / 16 * guess >=
	    (bufmem - bufmem_lowater))
		return 1;

	/* Otherwise don't allocate. */
	return 0;
}

/*
 * Return estimate of bytes we think need to be
 * released to help resolve low memory conditions.
 *
 * => called with bufcache_lock held.
 */
static int
buf_canrelease(void)
{
	int pagedemand, ninvalid = 0;

	KASSERT(mutex_owned(&bufcache_lock));

	if (bufmem < bufmem_lowater)
		return 0;

	if (bufmem > bufmem_hiwater)
		return bufmem - bufmem_hiwater;

	ninvalid += bufqueues[BQ_AGE].bq_bytes;

	pagedemand = uvmexp.freetarg - uvmexp.free;
	if (pagedemand < 0)
		return ninvalid;
	return MAX(ninvalid, MIN(2 * MAXBSIZE,
	    MIN((bufmem - bufmem_lowater) / 16, pagedemand * PAGE_SIZE)));
}

/*
 * Buffer memory allocation helper functions
 */
static u_long
buf_mempoolidx(u_long size)
{
	u_int n = 0;

	size -= 1;
	size >>= MEMPOOL_INDEX_OFFSET;
	while (size) {
		size >>= 1;
		n += 1;
	}
	if (n >= NMEMPOOLS)
		panic("buf mem pool index %d", n);
	return n;
}

static u_long
buf_roundsize(u_long size)
{
	/* Round up to nearest power of 2 */
	return (1 << (buf_mempoolidx(size) + MEMPOOL_INDEX_OFFSET));
}

static void *
buf_alloc(size_t size)
{
	u_int n = buf_mempoolidx(size);
	void *addr;

	while (1) {
		addr = pool_get(&bmempools[n], PR_NOWAIT);
		if (addr != NULL)
			break;

		/* No memory, see if we can free some. If so, try again */
		mutex_enter(&bufcache_lock);
		if (buf_drain(1) > 0) {
			mutex_exit(&bufcache_lock);
			continue;
		}

		if (curlwp == uvm.pagedaemon_lwp) {
			mutex_exit(&bufcache_lock);
			return NULL;
		}

		/* Wait for buffers to arrive on the LRU queue */
		cv_timedwait(&needbuffer_cv, &bufcache_lock, hz / 4);
		mutex_exit(&bufcache_lock);
	}

	return addr;
}

static void
buf_mrelease(void *addr, size_t size)
{

	pool_put(&bmempools[buf_mempoolidx(size)], addr);
}

/*
 * bread()/breadn() helper.
 */
static buf_t *
bio_doread(struct vnode *vp, daddr_t blkno, int size, int async)
{
	buf_t *bp;
	struct mount *mp;

	bp = getblk(vp, blkno, size, 0, 0);

	/*
	 * getblk() may return NULL if we are the pagedaemon.
	 */
	if (bp == NULL) {
		KASSERT(curlwp == uvm.pagedaemon_lwp);
		return NULL;
	}

	/*
	 * If buffer does not have data valid, start a read.
	 * Note that if buffer is BC_INVAL, getblk() won't return it.
	 * Therefore, it's valid if its I/O has completed or been delayed.
	 */
	if (!ISSET(bp->b_oflags, (BO_DONE | BO_DELWRI))) {
		/* Start I/O for the buffer. */
		SET(bp->b_flags, B_READ | async);
		if (async)
			BIO_SETPRIO(bp, BPRIO_TIMELIMITED);
		else
			BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
		VOP_STRATEGY(vp, bp);

		/* Pay for the read. */
		curlwp->l_ru.ru_inblock++;
	} else if (async)
		brelse(bp, 0);

	if (vp->v_type == VBLK)
		mp = spec_node_getmountedfs(vp);
	else
		mp = vp->v_mount;

	/*
	 * Collect statistics on synchronous and asynchronous reads.
	 * Reads from block devices are charged to their associated
	 * filesystem (if any).
	 */
	if (mp != NULL) {
		if (async == 0)
			mp->mnt_stat.f_syncreads++;
		else
			mp->mnt_stat.f_asyncreads++;
	}

	return (bp);
}

/*
 * Read a disk block.
 * This algorithm described in Bach (p.54).
 */
int
bread(struct vnode *vp, daddr_t blkno, int size, int flags, buf_t **bpp)
{
	buf_t *bp;
	int error;

	/* Get buffer for block. */
	bp = *bpp = bio_doread(vp, blkno, size, 0);
	if (bp == NULL)
		return ENOMEM;

	/* Wait for the read to complete, and return result. */
	error = biowait(bp);
	if (error == 0 && (flags & B_MODIFY) != 0)
		error = fscow_run(bp, true);
	if (error) {
		brelse(bp, 0);
		*bpp = NULL;
	}

	return error;
}

/*
 * Read-ahead multiple disk blocks. The first is sync, the rest async.
 * Trivial modification to the breada algorithm presented in Bach (p.55).
 */
int
breadn(struct vnode *vp, daddr_t blkno, int size, daddr_t *rablks,
    int *rasizes, int nrablks, int flags, buf_t **bpp)
{
	buf_t *bp;
	int error, i;

	bp = *bpp = bio_doread(vp, blkno, size, 0);
	if (bp == NULL)
		return ENOMEM;

	/*
	 * For each of the read-ahead blocks, start a read, if necessary.
	 */
	mutex_enter(&bufcache_lock);
	for (i = 0; i < nrablks; i++) {
		/* If it's in the cache, just go on to next one. */
		if (incore(vp, rablks[i]))
			continue;

		/* Get a buffer for the read-ahead block */
		mutex_exit(&bufcache_lock);
		(void) bio_doread(vp, rablks[i], rasizes[i], B_ASYNC);
		mutex_enter(&bufcache_lock);
	}
	mutex_exit(&bufcache_lock);

	/* Otherwise, we had to start a read for it; wait until it's valid. */
	error = biowait(bp);
	if (error == 0 && (flags & B_MODIFY) != 0)
		error = fscow_run(bp, true);
	if (error) {
		brelse(bp, 0);
		*bpp = NULL;
	}

	return error;
}

/*
 * Block write.  Described in Bach (p.56)
 */
int
bwrite(buf_t *bp)
{
	int rv, sync, wasdelayed;
	struct vnode *vp;
	struct mount *mp;

	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(!cv_has_waiters(&bp->b_done));

	vp = bp->b_vp;
	if (vp != NULL) {
		KASSERT(bp->b_objlock == vp->v_interlock);
		if (vp->v_type == VBLK)
			mp = spec_node_getmountedfs(vp);
		else
			mp = vp->v_mount;
	} else {
		mp = NULL;
	}

	if (mp && mp->mnt_wapbl) {
		if (bp->b_iodone != mp->mnt_wapbl_op->wo_wapbl_biodone) {
			bdwrite(bp);
			return 0;
		}
	}

	/*
	 * Remember buffer type, to switch on it later.  If the write was
	 * synchronous, but the file system was mounted with MNT_ASYNC,
	 * convert it to a delayed write.
	 * XXX note that this relies on delayed tape writes being converted
	 * to async, not sync writes (which is safe, but ugly).
	 */
	sync = !ISSET(bp->b_flags, B_ASYNC);
	if (sync && mp != NULL && ISSET(mp->mnt_flag, MNT_ASYNC)) {
		bdwrite(bp);
		return (0);
	}

	/*
	 * Collect statistics on synchronous and asynchronous writes.
	 * Writes to block devices are charged to their associated
	 * filesystem (if any).
	 */
	if (mp != NULL) {
		if (sync)
			mp->mnt_stat.f_syncwrites++;
		else
			mp->mnt_stat.f_asyncwrites++;
	}

	/*
	 * Pay for the I/O operation and make sure the buf is on the correct
	 * vnode queue.
	 */
	bp->b_error = 0;
	wasdelayed = ISSET(bp->b_oflags, BO_DELWRI);
	CLR(bp->b_flags, B_READ);
	if (wasdelayed) {
		mutex_enter(&bufcache_lock);
		mutex_enter(bp->b_objlock);
		CLR(bp->b_oflags, BO_DONE | BO_DELWRI);
		reassignbuf(bp, bp->b_vp);
		mutex_exit(&bufcache_lock);
	} else {
		curlwp->l_ru.ru_oublock++;
		mutex_enter(bp->b_objlock);
		CLR(bp->b_oflags, BO_DONE | BO_DELWRI);
	}
	if (vp != NULL)
		vp->v_numoutput++;
	mutex_exit(bp->b_objlock);

	/* Initiate disk write. */
	if (sync)
		BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
	else
		BIO_SETPRIO(bp, BPRIO_TIMELIMITED);

	VOP_STRATEGY(vp, bp);

	if (sync) {
		/* If I/O was synchronous, wait for it to complete. */
		rv = biowait(bp);

		/* Release the buffer. */
		brelse(bp, 0);

		return (rv);
	} else {
		return (0);
	}
}

int
vn_bwrite(void *v)
{
	struct vop_bwrite_args *ap = v;

	return (bwrite(ap->a_bp));
}

/*
 * Delayed write.
 *
 * The buffer is marked dirty, but is not queued for I/O.
 * This routine should be used when the buffer is expected
 * to be modified again soon, typically a small write that
 * partially fills a buffer.
 *
 * NB: magnetic tapes cannot be delayed; they must be
 * written in the order that the writes are requested.
 *
 * Described in Leffler, et al. (pp. 208-213).
 */
void
bdwrite(buf_t *bp)
{

	KASSERT(bp->b_vp == NULL || bp->b_vp->v_tag != VT_UFS ||
	    bp->b_vp->v_type == VBLK || ISSET(bp->b_flags, B_COWDONE));
	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(!cv_has_waiters(&bp->b_done));

	/* If this is a tape block, write the block now. */
	if (bdev_type(bp->b_dev) == D_TAPE) {
		bawrite(bp);
		return;
	}

	if (wapbl_vphaswapbl(bp->b_vp)) {
		struct mount *mp = wapbl_vptomp(bp->b_vp);

		if (bp->b_iodone != mp->mnt_wapbl_op->wo_wapbl_biodone) {
			WAPBL_ADD_BUF(mp, bp);
		}
	}

	/*
	 * If the block hasn't been seen before:
	 *	(1) Mark it as having been seen,
	 *	(2) Charge for the write,
	 *	(3) Make sure it's on its vnode's correct block list.
	 */
	KASSERT(bp->b_vp == NULL || bp->b_objlock == bp->b_vp->v_interlock);

	if (!ISSET(bp->b_oflags, BO_DELWRI)) {
		mutex_enter(&bufcache_lock);
		mutex_enter(bp->b_objlock);
		SET(bp->b_oflags, BO_DELWRI);
		curlwp->l_ru.ru_oublock++;
		reassignbuf(bp, bp->b_vp);
		mutex_exit(&bufcache_lock);
	} else {
		mutex_enter(bp->b_objlock);
	}
	/* Otherwise, the "write" is done, so mark and release the buffer. */
	CLR(bp->b_oflags, BO_DONE);
	mutex_exit(bp->b_objlock);

	brelse(bp, 0);
}

/*
 * Asynchronous block write; just an asynchronous bwrite().
 */
void
bawrite(buf_t *bp)
{

	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(bp->b_vp != NULL);

	SET(bp->b_flags, B_ASYNC);
	VOP_BWRITE(bp->b_vp, bp);
}

/*
 * Release a buffer on to the free lists.
 * Described in Bach (p. 46).
 */
void
brelsel(buf_t *bp, int set)
{
	struct bqueue *bufq;
	struct vnode *vp;

	KASSERT(bp != NULL);
	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(!cv_has_waiters(&bp->b_done));
	KASSERT(bp->b_refcnt > 0);
	
	SET(bp->b_cflags, set);

	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(bp->b_iodone == NULL);

	/* Wake up any processes waiting for any buffer to become free. */
	cv_signal(&needbuffer_cv);

	/* Wake up any proceeses waiting for _this_ buffer to become */
	if (ISSET(bp->b_cflags, BC_WANTED))
		CLR(bp->b_cflags, BC_WANTED|BC_AGE);

	/* If it's clean clear the copy-on-write flag. */
	if (ISSET(bp->b_flags, B_COWDONE)) {
		mutex_enter(bp->b_objlock);
		if (!ISSET(bp->b_oflags, BO_DELWRI))
			CLR(bp->b_flags, B_COWDONE);
		mutex_exit(bp->b_objlock);
	}

	/*
	 * Determine which queue the buffer should be on, then put it there.
	 */

	/* If it's locked, don't report an error; try again later. */
	if (ISSET(bp->b_flags, B_LOCKED))
		bp->b_error = 0;

	/* If it's not cacheable, or an error, mark it invalid. */
	if (ISSET(bp->b_cflags, BC_NOCACHE) || bp->b_error != 0)
		SET(bp->b_cflags, BC_INVAL);

	if (ISSET(bp->b_cflags, BC_VFLUSH)) {
		/*
		 * This is a delayed write buffer that was just flushed to
		 * disk.  It is still on the LRU queue.  If it's become
		 * invalid, then we need to move it to a different queue;
		 * otherwise leave it in its current position.
		 */
		CLR(bp->b_cflags, BC_VFLUSH);
		if (!ISSET(bp->b_cflags, BC_INVAL|BC_AGE) &&
		    !ISSET(bp->b_flags, B_LOCKED) && bp->b_error == 0) {
			KDASSERT(checkfreelist(bp, &bufqueues[BQ_LRU], 1));
			goto already_queued;
		} else {
			bremfree(bp);
		}
	}

	KDASSERT(checkfreelist(bp, &bufqueues[BQ_AGE], 0));
	KDASSERT(checkfreelist(bp, &bufqueues[BQ_LRU], 0));
	KDASSERT(checkfreelist(bp, &bufqueues[BQ_LOCKED], 0));

	if ((bp->b_bufsize <= 0) || ISSET(bp->b_cflags, BC_INVAL)) {
		/*
		 * If it's invalid or empty, dissociate it from its vnode
		 * and put on the head of the appropriate queue.
		 */
		if (ISSET(bp->b_flags, B_LOCKED)) {
			if (wapbl_vphaswapbl(vp = bp->b_vp)) {
				struct mount *mp = wapbl_vptomp(vp);

				KASSERT(bp->b_iodone
				    != mp->mnt_wapbl_op->wo_wapbl_biodone);
				WAPBL_REMOVE_BUF(mp, bp);
			}
		}

		mutex_enter(bp->b_objlock);
		CLR(bp->b_oflags, BO_DONE|BO_DELWRI);
		if ((vp = bp->b_vp) != NULL) {
			KASSERT(bp->b_objlock == vp->v_interlock);
			reassignbuf(bp, bp->b_vp);
			brelvp(bp);
			mutex_exit(vp->v_interlock);
		} else {
			KASSERT(bp->b_objlock == &buffer_lock);
			mutex_exit(bp->b_objlock);
		}

		if (bp->b_bufsize <= 0)
			/* no data */
			goto already_queued;
		else
			/* invalid data */
			bufq = &bufqueues[BQ_AGE];
		binsheadfree(bp, bufq);
	} else  {
		/*
		 * It has valid data.  Put it on the end of the appropriate
		 * queue, so that it'll stick around for as long as possible.
		 * If buf is AGE, but has dependencies, must put it on last
		 * bufqueue to be scanned, ie LRU. This protects against the
		 * livelock where BQ_AGE only has buffers with dependencies,
		 * and we thus never get to the dependent buffers in BQ_LRU.
		 */
		if (ISSET(bp->b_flags, B_LOCKED)) {
			/* locked in core */
			bufq = &bufqueues[BQ_LOCKED];
		} else if (!ISSET(bp->b_cflags, BC_AGE)) {
			/* valid data */
			bufq = &bufqueues[BQ_LRU];
		} else {
			/* stale but valid data */
			bufq = &bufqueues[BQ_AGE];
		}
		binstailfree(bp, bufq);
	}
already_queued:
	/* Unlock the buffer. */
	CLR(bp->b_cflags, BC_AGE|BC_BUSY|BC_NOCACHE);
	CLR(bp->b_flags, B_ASYNC);
	cv_broadcast(&bp->b_busy);

	if (bp->b_bufsize <= 0)
		brele(bp);
}

void
brelse(buf_t *bp, int set)
{

	mutex_enter(&bufcache_lock);
	brelsel(bp, set);
	mutex_exit(&bufcache_lock);
}

/*
 * Determine if a block is in the cache.
 * Just look on what would be its hash chain.  If it's there, return
 * a pointer to it, unless it's marked invalid.  If it's marked invalid,
 * we normally don't return the buffer, unless the caller explicitly
 * wants us to.
 */
buf_t *
incore(struct vnode *vp, daddr_t blkno)
{
	buf_t *bp;

	KASSERT(mutex_owned(&bufcache_lock));

	/* Search hash chain */
	LIST_FOREACH(bp, BUFHASH(vp, blkno), b_hash) {
		if (bp->b_lblkno == blkno && bp->b_vp == vp &&
		    !ISSET(bp->b_cflags, BC_INVAL)) {
		    	KASSERT(bp->b_objlock == vp->v_interlock);
		    	return (bp);
		}
	}

	return (NULL);
}

/*
 * Get a block of requested size that is associated with
 * a given vnode and block offset. If it is found in the
 * block cache, mark it as having been found, make it busy
 * and return it. Otherwise, return an empty block of the
 * correct size. It is up to the caller to insure that the
 * cached blocks be of the correct size.
 */
buf_t *
getblk(struct vnode *vp, daddr_t blkno, int size, int slpflag, int slptimeo)
{
	int err, preserve;
	buf_t *bp;

	mutex_enter(&bufcache_lock);
 loop:
	bp = incore(vp, blkno);
	if (bp != NULL) {
		err = bbusy(bp, ((slpflag & PCATCH) != 0), slptimeo, NULL);
		if (err != 0) {
			if (err == EPASSTHROUGH)
				goto loop;
			mutex_exit(&bufcache_lock);
			return (NULL);
		}
		KASSERT(!cv_has_waiters(&bp->b_done));
#ifdef DIAGNOSTIC
		if (ISSET(bp->b_oflags, BO_DONE|BO_DELWRI) &&
		    bp->b_bcount < size && vp->v_type != VBLK)
			panic("getblk: block size invariant failed");
#endif
		bremfree(bp);
		preserve = 1;
	} else {
		if ((bp = getnewbuf(slpflag, slptimeo, 0)) == NULL)
			goto loop;

		if (incore(vp, blkno) != NULL) {
			/* The block has come into memory in the meantime. */
			brelsel(bp, 0);
			goto loop;
		}

		LIST_INSERT_HEAD(BUFHASH(vp, blkno), bp, b_hash);
		bp->b_blkno = bp->b_lblkno = bp->b_rawblkno = blkno;
		mutex_enter(vp->v_interlock);
		bgetvp(vp, bp);
		mutex_exit(vp->v_interlock);
		preserve = 0;
	}
	mutex_exit(&bufcache_lock);

	/*
	 * LFS can't track total size of B_LOCKED buffer (locked_queue_bytes)
	 * if we re-size buffers here.
	 */
	if (ISSET(bp->b_flags, B_LOCKED)) {
		KASSERT(bp->b_bufsize >= size);
	} else {
		if (allocbuf(bp, size, preserve)) {
			mutex_enter(&bufcache_lock);
			LIST_REMOVE(bp, b_hash);
			mutex_exit(&bufcache_lock);
			brelse(bp, BC_INVAL);
			return NULL;
		}
	}
	BIO_SETPRIO(bp, BPRIO_DEFAULT);
	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.
 */
buf_t *
geteblk(int size)
{
	buf_t *bp;
	int error __diagused;

	mutex_enter(&bufcache_lock);
	while ((bp = getnewbuf(0, 0, 0)) == NULL)
		;

	SET(bp->b_cflags, BC_INVAL);
	LIST_INSERT_HEAD(&invalhash, bp, b_hash);
	mutex_exit(&bufcache_lock);
	BIO_SETPRIO(bp, BPRIO_DEFAULT);
	error = allocbuf(bp, size, 0);
	KASSERT(error == 0);
	return (bp);
}

/*
 * Expand or contract the actual memory allocated to a buffer.
 *
 * If the buffer shrinks, data is lost, so it's up to the
 * caller to have written it out *first*; this routine will not
 * start a write.  If the buffer grows, it's the callers
 * responsibility to fill out the buffer's additional contents.
 */
int
allocbuf(buf_t *bp, int size, int preserve)
{
	void *addr;
	vsize_t oldsize, desired_size;
	int oldcount;
	int delta;

	desired_size = buf_roundsize(size);
	if (desired_size > MAXBSIZE)
		printf("allocbuf: buffer larger than MAXBSIZE requested");

	oldcount = bp->b_bcount;

	bp->b_bcount = size;

	oldsize = bp->b_bufsize;
	if (oldsize == desired_size) {
		/*
		 * Do not short cut the WAPBL resize, as the buffer length
		 * could still have changed and this would corrupt the
		 * tracking of the transaction length.
		 */
		goto out;
	}

	/*
	 * If we want a buffer of a different size, re-allocate the
	 * buffer's memory; copy old content only if needed.
	 */
	addr = buf_alloc(desired_size);
	if (addr == NULL)
		return ENOMEM;
	if (preserve)
		memcpy(addr, bp->b_data, MIN(oldsize,desired_size));
	if (bp->b_data != NULL)
		buf_mrelease(bp->b_data, oldsize);
	bp->b_data = addr;
	bp->b_bufsize = desired_size;

	/*
	 * Update overall buffer memory counter (protected by bufcache_lock)
	 */
	delta = (long)desired_size - (long)oldsize;

	mutex_enter(&bufcache_lock);
	if ((bufmem += delta) > bufmem_hiwater) {
		/*
		 * Need to trim overall memory usage.
		 */
		while (buf_canrelease()) {
			if (curcpu()->ci_schedstate.spc_flags &
			    SPCF_SHOULDYIELD) {
				mutex_exit(&bufcache_lock);
				preempt();
				mutex_enter(&bufcache_lock);
			}
			if (buf_trim() == 0)
				break;
		}
	}
	mutex_exit(&bufcache_lock);

 out:
	if (wapbl_vphaswapbl(bp->b_vp))
		WAPBL_RESIZE_BUF(wapbl_vptomp(bp->b_vp), bp, oldsize, oldcount);

	return 0;
}

/*
 * Find a buffer which is available for use.
 * Select something from a free list.
 * Preference is to AGE list, then LRU list.
 *
 * Called with the buffer queues locked.
 * Return buffer locked.
 */
buf_t *
getnewbuf(int slpflag, int slptimeo, int from_bufq)
{
	buf_t *bp;
	struct vnode *vp;

 start:
	KASSERT(mutex_owned(&bufcache_lock));

	/*
	 * Get a new buffer from the pool.
	 */
	if (!from_bufq && buf_lotsfree()) {
		mutex_exit(&bufcache_lock);
		bp = pool_cache_get(buf_cache, PR_NOWAIT);
		if (bp != NULL) {
			memset((char *)bp, 0, sizeof(*bp));
			buf_init(bp);
			SET(bp->b_cflags, BC_BUSY);	/* mark buffer busy */
			mutex_enter(&bufcache_lock);
#if defined(DIAGNOSTIC)
			bp->b_freelistindex = -1;
#endif /* defined(DIAGNOSTIC) */
			return (bp);
		}
		mutex_enter(&bufcache_lock);
	}

	KASSERT(mutex_owned(&bufcache_lock));
	if ((bp = TAILQ_FIRST(&bufqueues[BQ_AGE].bq_queue)) != NULL ||
	    (bp = TAILQ_FIRST(&bufqueues[BQ_LRU].bq_queue)) != NULL) {
	    	KASSERT(!ISSET(bp->b_cflags, BC_BUSY) || ISSET(bp->b_cflags, BC_VFLUSH));
		bremfree(bp);

		/* Buffer is no longer on free lists. */
		SET(bp->b_cflags, BC_BUSY);
	} else {
		/*
		 * XXX: !from_bufq should be removed.
		 */
		if (!from_bufq || curlwp != uvm.pagedaemon_lwp) {
			/* wait for a free buffer of any kind */
			if ((slpflag & PCATCH) != 0)
				(void)cv_timedwait_sig(&needbuffer_cv,
				    &bufcache_lock, slptimeo);
			else
				(void)cv_timedwait(&needbuffer_cv,
				    &bufcache_lock, slptimeo);
		}
		return (NULL);
	}

#ifdef DIAGNOSTIC
	if (bp->b_bufsize <= 0)
		panic("buffer %p: on queue but empty", bp);
#endif

	if (ISSET(bp->b_cflags, BC_VFLUSH)) {
		/*
		 * This is a delayed write buffer being flushed to disk.  Make
		 * sure it gets aged out of the queue when it's finished, and
		 * leave it off the LRU queue.
		 */
		CLR(bp->b_cflags, BC_VFLUSH);
		SET(bp->b_cflags, BC_AGE);
		goto start;
	}

	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(bp->b_refcnt > 0);
    	KASSERT(!cv_has_waiters(&bp->b_done));

	/*
	 * If buffer was a delayed write, start it and return NULL
	 * (since we might sleep while starting the write).
	 */
	if (ISSET(bp->b_oflags, BO_DELWRI)) {
		/*
		 * This buffer has gone through the LRU, so make sure it gets
		 * reused ASAP.
		 */
		SET(bp->b_cflags, BC_AGE);
		mutex_exit(&bufcache_lock);
		bawrite(bp);
		mutex_enter(&bufcache_lock);
		return (NULL);
	}

	vp = bp->b_vp;

	/* clear out various other fields */
	bp->b_cflags = BC_BUSY;
	bp->b_oflags = 0;
	bp->b_flags = 0;
	bp->b_dev = NODEV;
	bp->b_blkno = 0;
	bp->b_lblkno = 0;
	bp->b_rawblkno = 0;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;

	LIST_REMOVE(bp, b_hash);

	/* Disassociate us from our vnode, if we had one... */
	if (vp != NULL) {
		mutex_enter(vp->v_interlock);
		brelvp(bp);
		mutex_exit(vp->v_interlock);
	}

	return (bp);
}

/*
 * Attempt to free an aged buffer off the queues.
 * Called with queue lock held.
 * Returns the amount of buffer memory freed.
 */
static int
buf_trim(void)
{
	buf_t *bp;
	long size;

	KASSERT(mutex_owned(&bufcache_lock));

	/* Instruct getnewbuf() to get buffers off the queues */
	if ((bp = getnewbuf(PCATCH, 1, 1)) == NULL)
		return 0;

	KASSERT((bp->b_cflags & BC_WANTED) == 0);
	size = bp->b_bufsize;
	bufmem -= size;
	if (size > 0) {
		buf_mrelease(bp->b_data, size);
		bp->b_bcount = bp->b_bufsize = 0;
	}
	/* brelse() will return the buffer to the global buffer pool */
	brelsel(bp, 0);
	return size;
}

int
buf_drain(int n)
{
	int size = 0, sz;

	KASSERT(mutex_owned(&bufcache_lock));

	while (size < n && bufmem > bufmem_lowater) {
		sz = buf_trim();
		if (sz <= 0)
			break;
		size += sz;
	}

	return size;
}

/*
 * Wait for operations on the buffer to complete.
 * When they do, extract and return the I/O's error value.
 */
int
biowait(buf_t *bp)
{

	KASSERT(ISSET(bp->b_cflags, BC_BUSY));
	KASSERT(bp->b_refcnt > 0);

	mutex_enter(bp->b_objlock);
	while (!ISSET(bp->b_oflags, BO_DONE | BO_DELWRI))
		cv_wait(&bp->b_done, bp->b_objlock);
	mutex_exit(bp->b_objlock);

	return bp->b_error;
}

/*
 * Mark I/O complete on a buffer.
 *
 * If a callback has been requested, e.g. the pageout
 * daemon, do so. Otherwise, awaken waiting processes.
 *
 * [ Leffler, et al., says on p.247:
 *	"This routine wakes up the blocked process, frees the buffer
 *	for an asynchronous write, or, for a request by the pagedaemon
 *	process, invokes a procedure specified in the buffer structure" ]
 *
 * In real life, the pagedaemon (or other system processes) wants
 * to do async stuff to, and doesn't want the buffer brelse()'d.
 * (for swap pager, that puts swap buffers on the free lists (!!!),
 * for the vn device, that puts allocated buffers on the free lists!)
 */
void
biodone(buf_t *bp)
{
	int s;

	KASSERT(!ISSET(bp->b_oflags, BO_DONE));

	if (cpu_intr_p()) {
		/* From interrupt mode: defer to a soft interrupt. */
		s = splvm();
		TAILQ_INSERT_TAIL(&curcpu()->ci_data.cpu_biodone, bp, b_actq);
		softint_schedule(biodone_sih);
		splx(s);
	} else {
		/* Process now - the buffer may be freed soon. */
		biodone2(bp);
	}
}

static void
biodone2(buf_t *bp)
{
	void (*callout)(buf_t *);

	mutex_enter(bp->b_objlock);
	/* Note that the transfer is done. */
	if (ISSET(bp->b_oflags, BO_DONE))
		panic("biodone2 already");
	CLR(bp->b_flags, B_COWDONE);
	SET(bp->b_oflags, BO_DONE);
	BIO_SETPRIO(bp, BPRIO_DEFAULT);

	/* Wake up waiting writers. */
	if (!ISSET(bp->b_flags, B_READ))
		vwakeup(bp);

	if ((callout = bp->b_iodone) != NULL) {
		/* Note callout done, then call out. */
		KASSERT(!cv_has_waiters(&bp->b_done));
		KERNEL_LOCK(1, NULL);		/* XXXSMP */
		bp->b_iodone = NULL;
		mutex_exit(bp->b_objlock);
		(*callout)(bp);
		KERNEL_UNLOCK_ONE(NULL);	/* XXXSMP */
	} else if (ISSET(bp->b_flags, B_ASYNC)) {
		/* If async, release. */
		KASSERT(!cv_has_waiters(&bp->b_done));
		mutex_exit(bp->b_objlock);
		brelse(bp, 0);
	} else {
		/* Otherwise just wake up waiters in biowait(). */
		cv_broadcast(&bp->b_done);
		mutex_exit(bp->b_objlock);
	}
}

static void
biointr(void *cookie)
{
	struct cpu_info *ci;
	buf_t *bp;
	int s;

	ci = curcpu();

	while (!TAILQ_EMPTY(&ci->ci_data.cpu_biodone)) {
		KASSERT(curcpu() == ci);

		s = splvm();
		bp = TAILQ_FIRST(&ci->ci_data.cpu_biodone);
		TAILQ_REMOVE(&ci->ci_data.cpu_biodone, bp, b_actq);
		splx(s);

		biodone2(bp);
	}
}

/*
 * Wait for all buffers to complete I/O
 * Return the number of "stuck" buffers.
 */
int
buf_syncwait(void)
{
	buf_t *bp;
	int iter, nbusy, nbusy_prev = 0, ihash;

	for (iter = 0; iter < 20;) {
		mutex_enter(&bufcache_lock);
		nbusy = 0;
		for (ihash = 0; ihash < bufhash+1; ihash++) {
		    LIST_FOREACH(bp, &bufhashtbl[ihash], b_hash) {
			if ((bp->b_cflags & (BC_BUSY|BC_INVAL)) == BC_BUSY)
				nbusy += ((bp->b_flags & B_READ) == 0);
		    }
		}
		mutex_exit(&bufcache_lock);

		if (nbusy == 0)
			break;
		if (nbusy_prev == 0)
			nbusy_prev = nbusy;
		printf("%d ", nbusy);
		kpause("bflush", false, MAX(1, hz / 25 * iter), NULL);
		if (nbusy >= nbusy_prev) /* we didn't flush anything */
			iter++;
		else
			nbusy_prev = nbusy;
	}

	if (nbusy) {
#if defined(DEBUG) || defined(DEBUG_HALT_BUSY)
		printf("giving up\nPrinting vnodes for busy buffers\n");
		for (ihash = 0; ihash < bufhash+1; ihash++) {
		    LIST_FOREACH(bp, &bufhashtbl[ihash], b_hash) {
			if ((bp->b_cflags & (BC_BUSY|BC_INVAL)) == BC_BUSY &&
			    (bp->b_flags & B_READ) == 0)
				vprint(NULL, bp->b_vp);
		    }
		}
#endif
	}

	return nbusy;
}

static void
sysctl_fillbuf(buf_t *i, struct buf_sysctl *o)
{

	o->b_flags = i->b_flags | i->b_cflags | i->b_oflags;
	o->b_error = i->b_error;
	o->b_prio = i->b_prio;
	o->b_dev = i->b_dev;
	o->b_bufsize = i->b_bufsize;
	o->b_bcount = i->b_bcount;
	o->b_resid = i->b_resid;
	o->b_addr = PTRTOUINT64(i->b_data);
	o->b_blkno = i->b_blkno;
	o->b_rawblkno = i->b_rawblkno;
	o->b_iodone = PTRTOUINT64(i->b_iodone);
	o->b_proc = PTRTOUINT64(i->b_proc);
	o->b_vp = PTRTOUINT64(i->b_vp);
	o->b_saveaddr = PTRTOUINT64(i->b_saveaddr);
	o->b_lblkno = i->b_lblkno;
}

#define KERN_BUFSLOP 20
static int
sysctl_dobuf(SYSCTLFN_ARGS)
{
	buf_t *bp;
	struct buf_sysctl bs;
	struct bqueue *bq;
	char *dp;
	u_int i, op, arg;
	size_t len, needed, elem_size, out_size;
	int error, elem_count, retries;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (namelen != 4)
		return (EINVAL);

	retries = 100;
 retry:
	dp = oldp;
	len = (oldp != NULL) ? *oldlenp : 0;
	op = name[0];
	arg = name[1];
	elem_size = name[2];
	elem_count = name[3];
	out_size = MIN(sizeof(bs), elem_size);

	/*
	 * at the moment, these are just "placeholders" to make the
	 * API for retrieving kern.buf data more extensible in the
	 * future.
	 *
	 * XXX kern.buf currently has "netbsd32" issues.  hopefully
	 * these will be resolved at a later point.
	 */
	if (op != KERN_BUF_ALL || arg != KERN_BUF_ALL ||
	    elem_size < 1 || elem_count < 0)
		return (EINVAL);

	error = 0;
	needed = 0;
	sysctl_unlock();
	mutex_enter(&bufcache_lock);
	for (i = 0; i < BQUEUES; i++) {
		bq = &bufqueues[i];
		TAILQ_FOREACH(bp, &bq->bq_queue, b_freelist) {
			bq->bq_marker = bp;
			if (len >= elem_size && elem_count > 0) {
				sysctl_fillbuf(bp, &bs);
				mutex_exit(&bufcache_lock);
				error = copyout(&bs, dp, out_size);
				mutex_enter(&bufcache_lock);
				if (error)
					break;
				if (bq->bq_marker != bp) {
					/*
					 * This sysctl node is only for
					 * statistics.  Retry; if the
					 * queue keeps changing, then
					 * bail out.
					 */
					if (retries-- == 0) {
						error = EAGAIN;
						break;
					}
					mutex_exit(&bufcache_lock);
					sysctl_relock();
					goto retry;
				}
				dp += elem_size;
				len -= elem_size;
			}
			needed += elem_size;
			if (elem_count > 0 && elem_count != INT_MAX)
				elem_count--;
		}
		if (error != 0)
			break;
	}
	mutex_exit(&bufcache_lock);
	sysctl_relock();

	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += KERN_BUFSLOP * sizeof(buf_t);

	return (error);
}

static int
sysctl_bufvm_update(SYSCTLFN_ARGS)
{
	int error, rv;
	struct sysctlnode node;
	unsigned int temp_bufcache;
	unsigned long temp_water;

	/* Take a copy of the supplied node and its data */
	node = *rnode;
	if (node.sysctl_data == &bufcache) {
	    node.sysctl_data = &temp_bufcache;
	    temp_bufcache = *(unsigned int *)rnode->sysctl_data;
	} else {
	    node.sysctl_data = &temp_water;
	    temp_water = *(unsigned long *)rnode->sysctl_data;
	}

	/* Update the copy */
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (rnode->sysctl_data == &bufcache) {
		if (temp_bufcache > 100)
			return (EINVAL);
		bufcache = temp_bufcache;
		buf_setwm();
	} else if (rnode->sysctl_data == &bufmem_lowater) {
		if (bufmem_hiwater - temp_water < 16)
			return (EINVAL);
		bufmem_lowater = temp_water;
	} else if (rnode->sysctl_data == &bufmem_hiwater) {
		if (temp_water - bufmem_lowater < 16)
			return (EINVAL);
		bufmem_hiwater = temp_water;
	} else
		return (EINVAL);

	/* Drain until below new high water mark */
	sysctl_unlock();
	mutex_enter(&bufcache_lock);
	while (bufmem > bufmem_hiwater) {
		rv = buf_drain((bufmem - bufmem_hiwater) / (2 * 1024));
		if (rv <= 0)
			break;
	}
	mutex_exit(&bufcache_lock);
	sysctl_relock();

	return 0;
}

static struct sysctllog *vfsbio_sysctllog;

static void
sysctl_kern_buf_setup(void)
{

	sysctl_createv(&vfsbio_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "buf",
		       SYSCTL_DESCR("Kernel buffer cache information"),
		       sysctl_dobuf, 0, NULL, 0,
		       CTL_KERN, KERN_BUF, CTL_EOL);
}

static void
sysctl_vm_buf_setup(void)
{

	sysctl_createv(&vfsbio_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "bufcache",
		       SYSCTL_DESCR("Percentage of physical memory to use for "
				    "buffer cache"),
		       sysctl_bufvm_update, 0, &bufcache, 0,
		       CTL_VM, CTL_CREATE, CTL_EOL);
	sysctl_createv(&vfsbio_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_LONG, "bufmem",
		       SYSCTL_DESCR("Amount of kernel memory used by buffer "
				    "cache"),
		       NULL, 0, &bufmem, 0,
		       CTL_VM, CTL_CREATE, CTL_EOL);
	sysctl_createv(&vfsbio_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_LONG, "bufmem_lowater",
		       SYSCTL_DESCR("Minimum amount of kernel memory to "
				    "reserve for buffer cache"),
		       sysctl_bufvm_update, 0, &bufmem_lowater, 0,
		       CTL_VM, CTL_CREATE, CTL_EOL);
	sysctl_createv(&vfsbio_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_LONG, "bufmem_hiwater",
		       SYSCTL_DESCR("Maximum amount of kernel memory to use "
				    "for buffer cache"),
		       sysctl_bufvm_update, 0, &bufmem_hiwater, 0,
		       CTL_VM, CTL_CREATE, CTL_EOL);
}

#ifdef DEBUG
/*
 * Print out statistics on the current allocation of the buffer pool.
 * Can be enabled to print out on every ``sync'' by setting "syncprt"
 * in vfs_syscalls.c using sysctl.
 */
void
vfs_bufstats(void)
{
	int i, j, count;
	buf_t *bp;
	struct bqueue *dp;
	int counts[(MAXBSIZE / PAGE_SIZE) + 1];
	static const char *bname[BQUEUES] = { "LOCKED", "LRU", "AGE" };

	for (dp = bufqueues, i = 0; dp < &bufqueues[BQUEUES]; dp++, i++) {
		count = 0;
		for (j = 0; j <= MAXBSIZE/PAGE_SIZE; j++)
			counts[j] = 0;
		TAILQ_FOREACH(bp, &dp->bq_queue, b_freelist) {
			counts[bp->b_bufsize/PAGE_SIZE]++;
			count++;
		}
		printf("%s: total-%d", bname[i], count);
		for (j = 0; j <= MAXBSIZE/PAGE_SIZE; j++)
			if (counts[j] != 0)
				printf(", %d-%d", j * PAGE_SIZE, counts[j]);
		printf("\n");
	}
}
#endif /* DEBUG */

/* ------------------------------ */

buf_t *
getiobuf(struct vnode *vp, bool waitok)
{
	buf_t *bp;

	bp = pool_cache_get(bufio_cache, (waitok ? PR_WAITOK : PR_NOWAIT));
	if (bp == NULL)
		return bp;

	buf_init(bp);

	if ((bp->b_vp = vp) == NULL)
		bp->b_objlock = &buffer_lock;
	else
		bp->b_objlock = vp->v_interlock;
	
	return bp;
}

void
putiobuf(buf_t *bp)
{

	buf_destroy(bp);
	pool_cache_put(bufio_cache, bp);
}

/*
 * nestiobuf_iodone: b_iodone callback for nested buffers.
 */

void
nestiobuf_iodone(buf_t *bp)
{
	buf_t *mbp = bp->b_private;
	int error;
	int donebytes;

	KASSERT(bp->b_bcount <= bp->b_bufsize);
	KASSERT(mbp != bp);

	error = bp->b_error;
	if (bp->b_error == 0 &&
	    (bp->b_bcount < bp->b_bufsize || bp->b_resid > 0)) {
		/*
		 * Not all got transfered, raise an error. We have no way to
		 * propagate these conditions to mbp.
		 */
		error = EIO;
	}

	donebytes = bp->b_bufsize;

	putiobuf(bp);
	nestiobuf_done(mbp, donebytes, error);
}

/*
 * nestiobuf_setup: setup a "nested" buffer.
 *
 * => 'mbp' is a "master" buffer which is being divided into sub pieces.
 * => 'bp' should be a buffer allocated by getiobuf.
 * => 'offset' is a byte offset in the master buffer.
 * => 'size' is a size in bytes of this nested buffer.
 */

void
nestiobuf_setup(buf_t *mbp, buf_t *bp, int offset, size_t size)
{
	const int b_read = mbp->b_flags & B_READ;
	struct vnode *vp = mbp->b_vp;

	KASSERT(mbp->b_bcount >= offset + size);
	bp->b_vp = vp;
	bp->b_dev = mbp->b_dev;
	bp->b_objlock = mbp->b_objlock;
	bp->b_cflags = BC_BUSY;
	bp->b_flags = B_ASYNC | b_read;
	bp->b_iodone = nestiobuf_iodone;
	bp->b_data = (char *)mbp->b_data + offset;
	bp->b_resid = bp->b_bcount = size;
	bp->b_bufsize = bp->b_bcount;
	bp->b_private = mbp;
	BIO_COPYPRIO(bp, mbp);
	if (!b_read && vp != NULL) {
		mutex_enter(vp->v_interlock);
		vp->v_numoutput++;
		mutex_exit(vp->v_interlock);
	}
}

/*
 * nestiobuf_done: propagate completion to the master buffer.
 *
 * => 'donebytes' specifies how many bytes in the 'mbp' is completed.
 * => 'error' is an errno(2) that 'donebytes' has been completed with.
 */

void
nestiobuf_done(buf_t *mbp, int donebytes, int error)
{

	if (donebytes == 0) {
		return;
	}
	mutex_enter(mbp->b_objlock);
	KASSERT(mbp->b_resid >= donebytes);
	mbp->b_resid -= donebytes;
	if (error)
		mbp->b_error = error;
	if (mbp->b_resid == 0) {
		if (mbp->b_error)
			mbp->b_resid = mbp->b_bcount;
		mutex_exit(mbp->b_objlock);
		biodone(mbp);
	} else
		mutex_exit(mbp->b_objlock);
}

void
buf_init(buf_t *bp)
{

	cv_init(&bp->b_busy, "biolock");
	cv_init(&bp->b_done, "biowait");
	bp->b_dev = NODEV;
	bp->b_error = 0;
	bp->b_flags = 0;
	bp->b_cflags = 0;
	bp->b_oflags = 0;
	bp->b_objlock = &buffer_lock;
	bp->b_iodone = NULL;
	bp->b_refcnt = 1;
	bp->b_dev = NODEV;
	bp->b_vnbufs.le_next = NOLIST;
	BIO_SETPRIO(bp, BPRIO_DEFAULT);
}

void
buf_destroy(buf_t *bp)
{

	cv_destroy(&bp->b_done);
	cv_destroy(&bp->b_busy);
}

int
bbusy(buf_t *bp, bool intr, int timo, kmutex_t *interlock)
{
	int error;

	KASSERT(mutex_owned(&bufcache_lock));

	if ((bp->b_cflags & BC_BUSY) != 0) {
		if (curlwp == uvm.pagedaemon_lwp)
			return EDEADLK;
		bp->b_cflags |= BC_WANTED;
		bref(bp);
		if (interlock != NULL)
			mutex_exit(interlock);
		if (intr) {
			error = cv_timedwait_sig(&bp->b_busy, &bufcache_lock,
			    timo);
		} else {
			error = cv_timedwait(&bp->b_busy, &bufcache_lock,
			    timo);
		}
		brele(bp);
		if (interlock != NULL)
			mutex_enter(interlock);
		if (error != 0)
			return error;
		return EPASSTHROUGH;
	}
	bp->b_cflags |= BC_BUSY;

	return 0;
}
