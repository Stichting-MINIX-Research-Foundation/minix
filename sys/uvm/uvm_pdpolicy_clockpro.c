/*	$NetBSD: uvm_pdpolicy_clockpro.c,v 1.17 2011/06/20 23:18:58 yamt Exp $	*/

/*-
 * Copyright (c)2005, 2006 YAMAMOTO Takashi,
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
 * CLOCK-Pro replacement policy:
 *	http://www.cs.wm.edu/hpcs/WWW/HTML/publications/abs05-3.html
 *
 * approximation of the list of non-resident pages using hash:
 *	http://linux-mm.org/ClockProApproximation
 */

/* #define	CLOCKPRO_DEBUG */

#if defined(PDSIM)

#include "pdsim.h"

#else /* defined(PDSIM) */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pdpolicy_clockpro.c,v 1.17 2011/06/20 23:18:58 yamt Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/hash.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdaemon.h>	/* for uvmpd_trylockowner */
#include <uvm/uvm_pdpolicy.h>
#include <uvm/uvm_pdpolicy_impl.h>

#if ((__STDC_VERSION__ - 0) >= 199901L)
#define	DPRINTF(...)	/* nothing */
#define	WARN(...)	printf(__VA_ARGS__)
#else /* ((__STDC_VERSION__ - 0) >= 199901L) */
#define	DPRINTF(a...)	/* nothing */	/* GCC */
#define	WARN(a...)	printf(a)
#endif /* ((__STDC_VERSION__ - 0) >= 199901L) */

#define	dump(a)		/* nothing */

#undef	USEONCE2
#define	LISTQ
#undef	ADAPTIVE

#endif /* defined(PDSIM) */

#if !defined(CLOCKPRO_COLDPCT)
#define	CLOCKPRO_COLDPCT	10
#endif /* !defined(CLOCKPRO_COLDPCT) */

#define	CLOCKPRO_COLDPCTMAX	90

#if !defined(CLOCKPRO_HASHFACTOR)
#define	CLOCKPRO_HASHFACTOR	2
#endif /* !defined(CLOCKPRO_HASHFACTOR) */

#define	CLOCKPRO_NEWQMIN	((1024 * 1024) >> PAGE_SHIFT)	/* XXX */

int clockpro_hashfactor = CLOCKPRO_HASHFACTOR;

PDPOL_EVCNT_DEFINE(nresrecordobj)
PDPOL_EVCNT_DEFINE(nresrecordanon)
PDPOL_EVCNT_DEFINE(nreslookupobj)
PDPOL_EVCNT_DEFINE(nreslookupanon)
PDPOL_EVCNT_DEFINE(nresfoundobj)
PDPOL_EVCNT_DEFINE(nresfoundanon)
PDPOL_EVCNT_DEFINE(nresanonfree)
PDPOL_EVCNT_DEFINE(nresconflict)
PDPOL_EVCNT_DEFINE(nresoverwritten)
PDPOL_EVCNT_DEFINE(nreshandhot)

PDPOL_EVCNT_DEFINE(hhottakeover)
PDPOL_EVCNT_DEFINE(hhotref)
PDPOL_EVCNT_DEFINE(hhotunref)
PDPOL_EVCNT_DEFINE(hhotcold)
PDPOL_EVCNT_DEFINE(hhotcoldtest)

PDPOL_EVCNT_DEFINE(hcoldtakeover)
PDPOL_EVCNT_DEFINE(hcoldref)
PDPOL_EVCNT_DEFINE(hcoldunref)
PDPOL_EVCNT_DEFINE(hcoldreftest)
PDPOL_EVCNT_DEFINE(hcoldunreftest)
PDPOL_EVCNT_DEFINE(hcoldunreftestspeculative)
PDPOL_EVCNT_DEFINE(hcoldhot)

PDPOL_EVCNT_DEFINE(speculativeenqueue)
PDPOL_EVCNT_DEFINE(speculativehit1)
PDPOL_EVCNT_DEFINE(speculativehit2)
PDPOL_EVCNT_DEFINE(speculativemiss)

PDPOL_EVCNT_DEFINE(locksuccess)
PDPOL_EVCNT_DEFINE(lockfail)

#define	PQ_REFERENCED	PQ_PRIVATE1
#define	PQ_HOT		PQ_PRIVATE2
#define	PQ_TEST		PQ_PRIVATE3
#define	PQ_INITIALREF	PQ_PRIVATE4
#if PQ_PRIVATE6 != PQ_PRIVATE5 * 2 || PQ_PRIVATE7 != PQ_PRIVATE6 * 2
#error PQ_PRIVATE
#endif
#define	PQ_QMASK	(PQ_PRIVATE5|PQ_PRIVATE6|PQ_PRIVATE7)
#define	PQ_QFACTOR	PQ_PRIVATE5
#define	PQ_SPECULATIVE	PQ_PRIVATE8

#define	CLOCKPRO_NOQUEUE	0
#define	CLOCKPRO_NEWQ		1	/* small queue to clear initial ref. */
#if defined(LISTQ)
#define	CLOCKPRO_COLDQ		2
#define	CLOCKPRO_HOTQ		3
#else /* defined(LISTQ) */
#define	CLOCKPRO_COLDQ		(2 + coldqidx)	/* XXX */
#define	CLOCKPRO_HOTQ		(3 - coldqidx)	/* XXX */
#endif /* defined(LISTQ) */
#define	CLOCKPRO_LISTQ		4
#define	CLOCKPRO_NQUEUE		4

static inline void
clockpro_setq(struct vm_page *pg, int qidx)
{
	KASSERT(qidx >= CLOCKPRO_NOQUEUE);
	KASSERT(qidx <= CLOCKPRO_NQUEUE);

	pg->pqflags = (pg->pqflags & ~PQ_QMASK) | (qidx * PQ_QFACTOR);
}

static inline int
clockpro_getq(struct vm_page *pg)
{
	int qidx;

	qidx = (pg->pqflags & PQ_QMASK) / PQ_QFACTOR;
	KASSERT(qidx >= CLOCKPRO_NOQUEUE);
	KASSERT(qidx <= CLOCKPRO_NQUEUE);
	return qidx;
}

typedef struct {
	struct pglist q_q;
	int q_len;
} pageq_t;

struct clockpro_state {
	int s_npages;
	int s_coldtarget;
	int s_ncold;

	int s_newqlenmax;
	pageq_t s_q[CLOCKPRO_NQUEUE];

	struct uvm_pctparam s_coldtargetpct;
};

static pageq_t *
clockpro_queue(struct clockpro_state *s, int qidx)
{

	KASSERT(CLOCKPRO_NOQUEUE < qidx);
	KASSERT(qidx <= CLOCKPRO_NQUEUE);

	return &s->s_q[qidx - 1];
}

#if !defined(LISTQ)

static int coldqidx;

static void
clockpro_switchqueue(void)
{

	coldqidx = 1 - coldqidx;
}

#endif /* !defined(LISTQ) */

static struct clockpro_state clockpro;
static struct clockpro_scanstate {
	int ss_nscanned;
} scanstate;

/* ---------------------------------------- */

static void
pageq_init(pageq_t *q)
{

	TAILQ_INIT(&q->q_q);
	q->q_len = 0;
}

static int
pageq_len(const pageq_t *q)
{

	return q->q_len;
}

static struct vm_page *
pageq_first(const pageq_t *q)
{

	return TAILQ_FIRST(&q->q_q);
}

static void
pageq_insert_tail(pageq_t *q, struct vm_page *pg)
{

	TAILQ_INSERT_TAIL(&q->q_q, pg, pageq.queue);
	q->q_len++;
}

#if defined(LISTQ)
static void
pageq_insert_head(pageq_t *q, struct vm_page *pg)
{

	TAILQ_INSERT_HEAD(&q->q_q, pg, pageq.queue);
	q->q_len++;
}
#endif

static void
pageq_remove(pageq_t *q, struct vm_page *pg)
{

#if 1
	KASSERT(clockpro_queue(&clockpro, clockpro_getq(pg)) == q);
#endif
	KASSERT(q->q_len > 0);
	TAILQ_REMOVE(&q->q_q, pg, pageq.queue);
	q->q_len--;
}

static struct vm_page *
pageq_remove_head(pageq_t *q)
{
	struct vm_page *pg;

	pg = TAILQ_FIRST(&q->q_q);
	if (pg == NULL) {
		KASSERT(q->q_len == 0);
		return NULL;
	}
	pageq_remove(q, pg);
	return pg;
}

/* ---------------------------------------- */

static void
clockpro_insert_tail(struct clockpro_state *s, int qidx, struct vm_page *pg)
{
	pageq_t *q = clockpro_queue(s, qidx);
	
	clockpro_setq(pg, qidx);
	pageq_insert_tail(q, pg);
}

#if defined(LISTQ)
static void
clockpro_insert_head(struct clockpro_state *s, int qidx, struct vm_page *pg)
{
	pageq_t *q = clockpro_queue(s, qidx);
	
	clockpro_setq(pg, qidx);
	pageq_insert_head(q, pg);
}

#endif
/* ---------------------------------------- */

typedef uint32_t nonres_cookie_t;
#define	NONRES_COOKIE_INVAL	0

typedef uintptr_t objid_t;

/*
 * XXX maybe these hash functions need reconsideration,
 * given that hash distribution is critical here.
 */

static uint32_t
pageidentityhash1(objid_t obj, off_t idx)
{
	uint32_t hash = HASH32_BUF_INIT;

#if 1
	hash = hash32_buf(&idx, sizeof(idx), hash);
	hash = hash32_buf(&obj, sizeof(obj), hash);
#else
	hash = hash32_buf(&obj, sizeof(obj), hash);
	hash = hash32_buf(&idx, sizeof(idx), hash);
#endif
	return hash;
}

static uint32_t
pageidentityhash2(objid_t obj, off_t idx)
{
	uint32_t hash = HASH32_BUF_INIT;

	hash = hash32_buf(&obj, sizeof(obj), hash);
	hash = hash32_buf(&idx, sizeof(idx), hash);
	return hash;
}

static nonres_cookie_t
calccookie(objid_t obj, off_t idx)
{
	uint32_t hash = pageidentityhash2(obj, idx);
	nonres_cookie_t cookie = hash;

	if (__predict_false(cookie == NONRES_COOKIE_INVAL)) {
		cookie++; /* XXX */
	}
	return cookie;
}

#define	BUCKETSIZE	14
struct bucket {
	int cycle;
	int cur;
	nonres_cookie_t pages[BUCKETSIZE];
};
static int cycle_target;
static int cycle_target_frac;

static struct bucket static_bucket;
static struct bucket *buckets = &static_bucket;
static size_t hashsize = 1;

static int coldadj;
#define	COLDTARGET_ADJ(d)	coldadj += (d)

#if defined(PDSIM)

static void *
clockpro_hashalloc(int n)
{
	size_t allocsz = sizeof(*buckets) * n;

	return malloc(allocsz);
}

static void
clockpro_hashfree(void *p, int n)
{

	free(p);
}

#else /* defined(PDSIM) */

static void *
clockpro_hashalloc(int n)
{
	size_t allocsz = round_page(sizeof(*buckets) * n);

	return (void *)uvm_km_alloc(kernel_map, allocsz, 0, UVM_KMF_WIRED);
}

static void
clockpro_hashfree(void *p, int n)
{
	size_t allocsz = round_page(sizeof(*buckets) * n);

	uvm_km_free(kernel_map, (vaddr_t)p, allocsz, UVM_KMF_WIRED);
}

#endif /* defined(PDSIM) */

static void
clockpro_hashinit(uint64_t n)
{
	struct bucket *newbuckets;
	struct bucket *oldbuckets;
	size_t sz;
	size_t oldsz;
	int i;

	sz = howmany(n, BUCKETSIZE);
	sz *= clockpro_hashfactor;
	newbuckets = clockpro_hashalloc(sz);
	if (newbuckets == NULL) {
		panic("%s: allocation failure", __func__);
	}
	for (i = 0; i < sz; i++) {
		struct bucket *b = &newbuckets[i];
		int j;

		b->cycle = cycle_target;
		b->cur = 0;
		for (j = 0; j < BUCKETSIZE; j++) {
			b->pages[j] = NONRES_COOKIE_INVAL;
		}
	}
	/* XXX lock */
	oldbuckets = buckets;
	oldsz = hashsize;
	buckets = newbuckets;
	hashsize = sz;
	/* XXX unlock */
	if (oldbuckets != &static_bucket) {
		clockpro_hashfree(oldbuckets, oldsz);
	}
}

static struct bucket *
nonresident_getbucket(objid_t obj, off_t idx)
{
	uint32_t hash;

	hash = pageidentityhash1(obj, idx);
	return &buckets[hash % hashsize];
}

static void
nonresident_rotate(struct bucket *b)
{
	const int target = cycle_target;
	const int cycle = b->cycle;
	int cur;
	int todo;

	todo = target - cycle;
	if (todo >= BUCKETSIZE * 2) {
		todo = (todo % BUCKETSIZE) + BUCKETSIZE;
	}
	cur = b->cur;
	while (todo > 0) {
		if (b->pages[cur] != NONRES_COOKIE_INVAL) {
			PDPOL_EVCNT_INCR(nreshandhot);
			COLDTARGET_ADJ(-1);
		}
		b->pages[cur] = NONRES_COOKIE_INVAL;
		cur++;
		if (cur == BUCKETSIZE) {
			cur = 0;
		}
		todo--;
	}
	b->cycle = target;
	b->cur = cur;
}

static bool
nonresident_lookupremove(objid_t obj, off_t idx)
{
	struct bucket *b = nonresident_getbucket(obj, idx);
	nonres_cookie_t cookie = calccookie(obj, idx);
	int i;

	nonresident_rotate(b);
	for (i = 0; i < BUCKETSIZE; i++) {
		if (b->pages[i] == cookie) {
			b->pages[i] = NONRES_COOKIE_INVAL;
			return true;
		}
	}
	return false;
}

static objid_t
pageobj(struct vm_page *pg)
{
	const void *obj;

	/*
	 * XXX object pointer is often freed and reused for unrelated object.
	 * for vnodes, it would be better to use something like
	 * a hash of fsid/fileid/generation.
	 */

	obj = pg->uobject;
	if (obj == NULL) {
		obj = pg->uanon;
		KASSERT(obj != NULL);
	}
	return (objid_t)obj;
}

static off_t
pageidx(struct vm_page *pg)
{

	KASSERT((pg->offset & PAGE_MASK) == 0);
	return pg->offset >> PAGE_SHIFT;
}

static bool
nonresident_pagelookupremove(struct vm_page *pg)
{
	bool found = nonresident_lookupremove(pageobj(pg), pageidx(pg));

	if (pg->uobject) {
		PDPOL_EVCNT_INCR(nreslookupobj);
	} else {
		PDPOL_EVCNT_INCR(nreslookupanon);
	}
	if (found) {
		if (pg->uobject) {
			PDPOL_EVCNT_INCR(nresfoundobj);
		} else {
			PDPOL_EVCNT_INCR(nresfoundanon);
		}
	}
	return found;
}

static void
nonresident_pagerecord(struct vm_page *pg)
{
	objid_t obj = pageobj(pg);
	off_t idx = pageidx(pg);
	struct bucket *b = nonresident_getbucket(obj, idx);
	nonres_cookie_t cookie = calccookie(obj, idx);

#if defined(DEBUG)
	int i;

	for (i = 0; i < BUCKETSIZE; i++) {
		if (b->pages[i] == cookie) {
			PDPOL_EVCNT_INCR(nresconflict);
		}
	}
#endif /* defined(DEBUG) */

	if (pg->uobject) {
		PDPOL_EVCNT_INCR(nresrecordobj);
	} else {
		PDPOL_EVCNT_INCR(nresrecordanon);
	}
	nonresident_rotate(b);
	if (b->pages[b->cur] != NONRES_COOKIE_INVAL) {
		PDPOL_EVCNT_INCR(nresoverwritten);
		COLDTARGET_ADJ(-1);
	}
	b->pages[b->cur] = cookie;
	b->cur = (b->cur + 1) % BUCKETSIZE;
}

/* ---------------------------------------- */

#if defined(CLOCKPRO_DEBUG)
static void
check_sanity(void)
{
}
#else /* defined(CLOCKPRO_DEBUG) */
#define	check_sanity()	/* nothing */
#endif /* defined(CLOCKPRO_DEBUG) */

static void
clockpro_reinit(void)
{

	clockpro_hashinit(uvmexp.npages);
}

static void
clockpro_init(void)
{
	struct clockpro_state *s = &clockpro;
	int i;

	for (i = 0; i < CLOCKPRO_NQUEUE; i++) {
		pageq_init(&s->s_q[i]);
	}
	s->s_newqlenmax = 1;
	s->s_coldtarget = 1;
	uvm_pctparam_init(&s->s_coldtargetpct, CLOCKPRO_COLDPCT, NULL);
}

static void
clockpro_tune(void)
{
	struct clockpro_state *s = &clockpro;
	int coldtarget;

#if defined(ADAPTIVE)
	int coldmax = s->s_npages * CLOCKPRO_COLDPCTMAX / 100;
	int coldmin = 1;

	coldtarget = s->s_coldtarget;
	if (coldtarget + coldadj < coldmin) {
		coldadj = coldmin - coldtarget;
	} else if (coldtarget + coldadj > coldmax) {
		coldadj = coldmax - coldtarget;
	}
	coldtarget += coldadj;
#else /* defined(ADAPTIVE) */
	coldtarget = UVM_PCTPARAM_APPLY(&s->s_coldtargetpct, s->s_npages);
	if (coldtarget < 1) {
		coldtarget = 1;
	}
#endif /* defined(ADAPTIVE) */

	s->s_coldtarget = coldtarget;
	s->s_newqlenmax = coldtarget / 4;
	if (s->s_newqlenmax < CLOCKPRO_NEWQMIN) {
		s->s_newqlenmax = CLOCKPRO_NEWQMIN;
	}
}

static void
clockpro_movereferencebit(struct vm_page *pg, bool locked)
{
	kmutex_t *lock;
	bool referenced;

	KASSERT(!locked || uvm_page_locked_p(pg));
	if (!locked) {
		lock = uvmpd_trylockowner(pg);
		if (lock == NULL) {
			/*
			 * XXXuvmplock
			 */
			PDPOL_EVCNT_INCR(lockfail);
			return;
		}
		PDPOL_EVCNT_INCR(locksuccess);
	}
	referenced = pmap_clear_reference(pg);
	if (!locked) {
		mutex_exit(lock);
	}
	if (referenced) {
		pg->pqflags |= PQ_REFERENCED;
	}
}

static void
clockpro_clearreferencebit(struct vm_page *pg, bool locked)
{

	clockpro_movereferencebit(pg, locked);
	pg->pqflags &= ~PQ_REFERENCED;
}

static void
clockpro___newqrotate(int len)
{
	struct clockpro_state * const s = &clockpro;
	pageq_t * const newq = clockpro_queue(s, CLOCKPRO_NEWQ);
	struct vm_page *pg;

	while (pageq_len(newq) > len) {
		pg = pageq_remove_head(newq);
		KASSERT(pg != NULL);
		KASSERT(clockpro_getq(pg) == CLOCKPRO_NEWQ);
		if ((pg->pqflags & PQ_INITIALREF) != 0) {
			clockpro_clearreferencebit(pg, false);
			pg->pqflags &= ~PQ_INITIALREF;
		}
		/* place at the list head */
		clockpro_insert_tail(s, CLOCKPRO_COLDQ, pg);
	}
}

static void
clockpro_newqrotate(void)
{
	struct clockpro_state * const s = &clockpro;

	check_sanity();
	clockpro___newqrotate(s->s_newqlenmax);
	check_sanity();
}

static void
clockpro_newqflush(int n)
{

	check_sanity();
	clockpro___newqrotate(n);
	check_sanity();
}

static void
clockpro_newqflushone(void)
{
	struct clockpro_state * const s = &clockpro;

	clockpro_newqflush(
	    MAX(pageq_len(clockpro_queue(s, CLOCKPRO_NEWQ)) - 1, 0));
}

/*
 * our "tail" is called "list-head" in the paper.
 */

static void
clockpro___enqueuetail(struct vm_page *pg)
{
	struct clockpro_state * const s = &clockpro;

	KASSERT(clockpro_getq(pg) == CLOCKPRO_NOQUEUE);

	check_sanity();
#if !defined(USEONCE2)
	clockpro_insert_tail(s, CLOCKPRO_NEWQ, pg);
	clockpro_newqrotate();
#else /* !defined(USEONCE2) */
#if defined(LISTQ)
	KASSERT((pg->pqflags & PQ_REFERENCED) == 0);
#endif /* defined(LISTQ) */
	clockpro_insert_tail(s, CLOCKPRO_COLDQ, pg);
#endif /* !defined(USEONCE2) */
	check_sanity();
}

static void
clockpro_pageenqueue(struct vm_page *pg)
{
	struct clockpro_state * const s = &clockpro;
	bool hot;
	bool speculative = (pg->pqflags & PQ_SPECULATIVE) != 0; /* XXX */

	KASSERT((~pg->pqflags & (PQ_INITIALREF|PQ_SPECULATIVE)) != 0);
	KASSERT(mutex_owned(&uvm_pageqlock));
	check_sanity();
	KASSERT(clockpro_getq(pg) == CLOCKPRO_NOQUEUE);
	s->s_npages++;
	pg->pqflags &= ~(PQ_HOT|PQ_TEST);
	if (speculative) {
		hot = false;
		PDPOL_EVCNT_INCR(speculativeenqueue);
	} else {
		hot = nonresident_pagelookupremove(pg);
		if (hot) {
			COLDTARGET_ADJ(1);
		}
	}

	/*
	 * consider mmap'ed file:
	 *
	 * - read-ahead enqueues a page.
	 *
	 * - on the following read-ahead hit, the fault handler activates it.
	 *
	 * - finally, the userland code which caused the above fault
	 *   actually accesses the page.  it makes its reference bit set.
	 *
	 * we want to count the above as a single access, rather than
	 * three accesses with short reuse distances.
	 */

#if defined(USEONCE2)
	pg->pqflags &= ~PQ_INITIALREF;
	if (hot) {
		pg->pqflags |= PQ_TEST;
	}
	s->s_ncold++;
	clockpro_clearreferencebit(pg, false);
	clockpro___enqueuetail(pg);
#else /* defined(USEONCE2) */
	if (speculative) {
		s->s_ncold++;
	} else if (hot) {
		pg->pqflags |= PQ_HOT;
	} else {
		pg->pqflags |= PQ_TEST;
		s->s_ncold++;
	}
	clockpro___enqueuetail(pg);
#endif /* defined(USEONCE2) */
	KASSERT(s->s_ncold <= s->s_npages);
}

static pageq_t *
clockpro_pagequeue(struct vm_page *pg)
{
	struct clockpro_state * const s = &clockpro;
	int qidx;

	qidx = clockpro_getq(pg);
	KASSERT(qidx != CLOCKPRO_NOQUEUE);

	return clockpro_queue(s, qidx);
}

static void
clockpro_pagedequeue(struct vm_page *pg)
{
	struct clockpro_state * const s = &clockpro;
	pageq_t *q;

	KASSERT(s->s_npages > 0);
	check_sanity();
	q = clockpro_pagequeue(pg);
	pageq_remove(q, pg);
	check_sanity();
	clockpro_setq(pg, CLOCKPRO_NOQUEUE);
	if ((pg->pqflags & PQ_HOT) == 0) {
		KASSERT(s->s_ncold > 0);
		s->s_ncold--;
	}
	KASSERT(s->s_npages > 0);
	s->s_npages--;
	check_sanity();
}

static void
clockpro_pagerequeue(struct vm_page *pg)
{
	struct clockpro_state * const s = &clockpro;
	int qidx;

	qidx = clockpro_getq(pg);
	KASSERT(qidx == CLOCKPRO_HOTQ || qidx == CLOCKPRO_COLDQ);
	pageq_remove(clockpro_queue(s, qidx), pg);
	check_sanity();
	clockpro_setq(pg, CLOCKPRO_NOQUEUE);

	clockpro___enqueuetail(pg);
}

static void
handhot_endtest(struct vm_page *pg)
{

	KASSERT((pg->pqflags & PQ_HOT) == 0);
	if ((pg->pqflags & PQ_TEST) != 0) {
		PDPOL_EVCNT_INCR(hhotcoldtest);
		COLDTARGET_ADJ(-1);
		pg->pqflags &= ~PQ_TEST;
	} else {
		PDPOL_EVCNT_INCR(hhotcold);
	}
}

static void
handhot_advance(void)
{
	struct clockpro_state * const s = &clockpro;
	struct vm_page *pg;
	pageq_t *hotq;
	int hotqlen;

	clockpro_tune();

	dump("hot called");
	if (s->s_ncold >= s->s_coldtarget) {
		return;
	}
	hotq = clockpro_queue(s, CLOCKPRO_HOTQ);
again:
	pg = pageq_first(hotq);
	if (pg == NULL) {
		DPRINTF("%s: HHOT TAKEOVER\n", __func__);
		dump("hhottakeover");
		PDPOL_EVCNT_INCR(hhottakeover);
#if defined(LISTQ)
		while (/* CONSTCOND */ 1) {
			pageq_t *coldq = clockpro_queue(s, CLOCKPRO_COLDQ);

			pg = pageq_first(coldq);
			if (pg == NULL) {
				clockpro_newqflushone();
				pg = pageq_first(coldq);
				if (pg == NULL) {
					WARN("hhot: no page?\n");
					return;
				}
			}
			KASSERT(clockpro_pagequeue(pg) == coldq);
			pageq_remove(coldq, pg);
			check_sanity();
			if ((pg->pqflags & PQ_HOT) == 0) {
				handhot_endtest(pg);
				clockpro_insert_tail(s, CLOCKPRO_LISTQ, pg);
			} else {
				clockpro_insert_head(s, CLOCKPRO_HOTQ, pg);
				break;
			}
		}
#else /* defined(LISTQ) */
		clockpro_newqflush(0); /* XXX XXX */
		clockpro_switchqueue();
		hotq = clockpro_queue(s, CLOCKPRO_HOTQ);
		goto again;
#endif /* defined(LISTQ) */
	}

	KASSERT(clockpro_pagequeue(pg) == hotq);

	/*
	 * terminate test period of nonresident pages by cycling them.
	 */

	cycle_target_frac += BUCKETSIZE;
	hotqlen = pageq_len(hotq);
	while (cycle_target_frac >= hotqlen) {
		cycle_target++;
		cycle_target_frac -= hotqlen;
	}

	if ((pg->pqflags & PQ_HOT) == 0) {
#if defined(LISTQ)
		panic("cold page in hotq: %p", pg);
#else /* defined(LISTQ) */
		handhot_endtest(pg);
		goto next;
#endif /* defined(LISTQ) */
	}
	KASSERT((pg->pqflags & PQ_TEST) == 0);
	KASSERT((pg->pqflags & PQ_INITIALREF) == 0);
	KASSERT((pg->pqflags & PQ_SPECULATIVE) == 0);

	/*
	 * once we met our target,
	 * stop at a hot page so that no cold pages in test period
	 * have larger recency than any hot pages.
	 */

	if (s->s_ncold >= s->s_coldtarget) {
		dump("hot done");
		return;
	}
	clockpro_movereferencebit(pg, false);
	if ((pg->pqflags & PQ_REFERENCED) == 0) {
		PDPOL_EVCNT_INCR(hhotunref);
		uvmexp.pddeact++;
		pg->pqflags &= ~PQ_HOT;
		clockpro.s_ncold++;
		KASSERT(s->s_ncold <= s->s_npages);
	} else {
		PDPOL_EVCNT_INCR(hhotref);
	}
	pg->pqflags &= ~PQ_REFERENCED;
#if !defined(LISTQ)
next:
#endif /* !defined(LISTQ) */
	clockpro_pagerequeue(pg);
	dump("hot");
	goto again;
}

static struct vm_page *
handcold_advance(void)
{
	struct clockpro_state * const s = &clockpro;
	struct vm_page *pg;

	for (;;) {
#if defined(LISTQ)
		pageq_t *listq = clockpro_queue(s, CLOCKPRO_LISTQ);
#endif /* defined(LISTQ) */
		pageq_t *coldq;

		clockpro_newqrotate();
		handhot_advance();
#if defined(LISTQ)
		pg = pageq_first(listq);
		if (pg != NULL) {
			KASSERT(clockpro_getq(pg) == CLOCKPRO_LISTQ);
			KASSERT((pg->pqflags & PQ_TEST) == 0);
			KASSERT((pg->pqflags & PQ_HOT) == 0);
			KASSERT((pg->pqflags & PQ_INITIALREF) == 0);
			pageq_remove(listq, pg);
			check_sanity();
			clockpro_insert_head(s, CLOCKPRO_COLDQ, pg); /* XXX */
			goto gotcold;
		}
#endif /* defined(LISTQ) */
		check_sanity();
		coldq = clockpro_queue(s, CLOCKPRO_COLDQ);
		pg = pageq_first(coldq);
		if (pg == NULL) {
			clockpro_newqflushone();
			pg = pageq_first(coldq);
		}
		if (pg == NULL) {
			DPRINTF("%s: HCOLD TAKEOVER\n", __func__);
			dump("hcoldtakeover");
			PDPOL_EVCNT_INCR(hcoldtakeover);
			KASSERT(
			    pageq_len(clockpro_queue(s, CLOCKPRO_NEWQ)) == 0);
#if defined(LISTQ)
			KASSERT(
			    pageq_len(clockpro_queue(s, CLOCKPRO_HOTQ)) == 0);
#else /* defined(LISTQ) */
			clockpro_switchqueue();
			coldq = clockpro_queue(s, CLOCKPRO_COLDQ);
			pg = pageq_first(coldq);
#endif /* defined(LISTQ) */
		}
		if (pg == NULL) {
			WARN("hcold: no page?\n");
			return NULL;
		}
		KASSERT((pg->pqflags & PQ_INITIALREF) == 0);
		if ((pg->pqflags & PQ_HOT) != 0) {
			PDPOL_EVCNT_INCR(hcoldhot);
			pageq_remove(coldq, pg);
			clockpro_insert_tail(s, CLOCKPRO_HOTQ, pg);
			check_sanity();
			KASSERT((pg->pqflags & PQ_TEST) == 0);
			uvmexp.pdscans++;
			continue;
		}
#if defined(LISTQ)
gotcold:
#endif /* defined(LISTQ) */
		KASSERT((pg->pqflags & PQ_HOT) == 0);
		uvmexp.pdscans++;
		clockpro_movereferencebit(pg, false);
		if ((pg->pqflags & PQ_SPECULATIVE) != 0) {
			KASSERT((pg->pqflags & PQ_TEST) == 0);
			if ((pg->pqflags & PQ_REFERENCED) != 0) {
				PDPOL_EVCNT_INCR(speculativehit2);
				pg->pqflags &= ~(PQ_SPECULATIVE|PQ_REFERENCED);
				clockpro_pagedequeue(pg);
				clockpro_pageenqueue(pg);
				continue;
			}
			PDPOL_EVCNT_INCR(speculativemiss);
		}
		switch (pg->pqflags & (PQ_REFERENCED|PQ_TEST)) {
		case PQ_TEST:
			PDPOL_EVCNT_INCR(hcoldunreftest);
			nonresident_pagerecord(pg);
			goto gotit;
		case 0:
			PDPOL_EVCNT_INCR(hcoldunref);
gotit:
			KASSERT(s->s_ncold > 0);
			clockpro_pagerequeue(pg); /* XXX */
			dump("cold done");
			/* XXX "pg" is still in queue */
			handhot_advance();
			goto done;

		case PQ_REFERENCED|PQ_TEST:
			PDPOL_EVCNT_INCR(hcoldreftest);
			s->s_ncold--;
			COLDTARGET_ADJ(1);
			pg->pqflags |= PQ_HOT;
			pg->pqflags &= ~PQ_TEST;
			break;

		case PQ_REFERENCED:
			PDPOL_EVCNT_INCR(hcoldref);
			pg->pqflags |= PQ_TEST;
			break;
		}
		pg->pqflags &= ~PQ_REFERENCED;
		uvmexp.pdreact++;
		/* move to the list head */
		clockpro_pagerequeue(pg);
		dump("cold");
	}
done:;
	return pg;
}

void
uvmpdpol_pageactivate(struct vm_page *pg)
{

	if (!uvmpdpol_pageisqueued_p(pg)) {
		KASSERT((pg->pqflags & PQ_SPECULATIVE) == 0);
		pg->pqflags |= PQ_INITIALREF;
		clockpro_pageenqueue(pg);
	} else if ((pg->pqflags & PQ_SPECULATIVE)) {
		PDPOL_EVCNT_INCR(speculativehit1);
		pg->pqflags &= ~PQ_SPECULATIVE;
		pg->pqflags |= PQ_INITIALREF;
		clockpro_pagedequeue(pg);
		clockpro_pageenqueue(pg);
	}
	pg->pqflags |= PQ_REFERENCED;
}

void
uvmpdpol_pagedeactivate(struct vm_page *pg)
{

	clockpro_clearreferencebit(pg, true);
}

void
uvmpdpol_pagedequeue(struct vm_page *pg)
{

	if (!uvmpdpol_pageisqueued_p(pg)) {
		return;
	}
	clockpro_pagedequeue(pg);
	pg->pqflags &= ~(PQ_INITIALREF|PQ_SPECULATIVE);
}

void
uvmpdpol_pageenqueue(struct vm_page *pg)
{

#if 1
	if (uvmpdpol_pageisqueued_p(pg)) {
		return;
	}
	clockpro_clearreferencebit(pg, true);
	pg->pqflags |= PQ_SPECULATIVE;
	clockpro_pageenqueue(pg);
#else
	uvmpdpol_pageactivate(pg);
#endif
}

void
uvmpdpol_anfree(struct vm_anon *an)
{

	KASSERT(an->an_page == NULL);
	if (nonresident_lookupremove((objid_t)an, 0)) {
		PDPOL_EVCNT_INCR(nresanonfree);
	}
}

void
uvmpdpol_init(void)
{

	clockpro_init();
}

void
uvmpdpol_reinit(void)
{

	clockpro_reinit();
}

void
uvmpdpol_estimatepageable(int *active, int *inactive)
{
	struct clockpro_state * const s = &clockpro;

	if (active) {
		*active = s->s_npages - s->s_ncold;
	}
	if (inactive) {
		*inactive = s->s_ncold;
	}
}

bool
uvmpdpol_pageisqueued_p(struct vm_page *pg)
{

	return clockpro_getq(pg) != CLOCKPRO_NOQUEUE;
}

void
uvmpdpol_scaninit(void)
{
	struct clockpro_scanstate * const ss = &scanstate;

	ss->ss_nscanned = 0;
}

struct vm_page *
uvmpdpol_selectvictim(void)
{
	struct clockpro_state * const s = &clockpro;
	struct clockpro_scanstate * const ss = &scanstate;
	struct vm_page *pg;

	if (ss->ss_nscanned > s->s_npages) {
		DPRINTF("scan too much\n");
		return NULL;
	}
	pg = handcold_advance();
	ss->ss_nscanned++;
	return pg;
}

static void
clockpro_dropswap(pageq_t *q, int *todo)
{
	struct vm_page *pg;

	TAILQ_FOREACH_REVERSE(pg, &q->q_q, pglist, pageq.queue) {
		if (*todo <= 0) {
			break;
		}
		if ((pg->pqflags & PQ_HOT) == 0) {
			continue;
		}
		if ((pg->pqflags & PQ_SWAPBACKED) == 0) {
			continue;
		}
		if (uvmpd_trydropswap(pg)) {
			(*todo)--;
		}
	}
}

void
uvmpdpol_balancequeue(int swap_shortage)
{
	struct clockpro_state * const s = &clockpro;
	int todo = swap_shortage;

	if (todo == 0) {
		return;
	}

	/*
	 * reclaim swap slots from hot pages
	 */

	DPRINTF("%s: swap_shortage=%d\n", __func__, swap_shortage);

	clockpro_dropswap(clockpro_queue(s, CLOCKPRO_NEWQ), &todo);
	clockpro_dropswap(clockpro_queue(s, CLOCKPRO_COLDQ), &todo);
	clockpro_dropswap(clockpro_queue(s, CLOCKPRO_HOTQ), &todo);

	DPRINTF("%s: done=%d\n", __func__, swap_shortage - todo);
}

bool
uvmpdpol_needsscan_p(void)
{
	struct clockpro_state * const s = &clockpro;

	if (s->s_ncold < s->s_coldtarget) {
		return true;
	}
	return false;
}

void
uvmpdpol_tune(void)
{

	clockpro_tune();
}

#if !defined(PDSIM)

#include <sys/sysctl.h>	/* XXX SYSCTL_DESCR */

void
uvmpdpol_sysctlsetup(void)
{
#if !defined(ADAPTIVE)
	struct clockpro_state * const s = &clockpro;

	uvm_pctparam_createsysctlnode(&s->s_coldtargetpct, "coldtargetpct",
	    SYSCTL_DESCR("Percentage cold target queue of the entire queue"));
#endif /* !defined(ADAPTIVE) */
}

#endif /* !defined(PDSIM) */

#if defined(DDB)

#if 0 /* XXXuvmplock */
#define	_pmap_is_referenced(pg)	pmap_is_referenced(pg)
#else
#define	_pmap_is_referenced(pg)	false
#endif

void clockpro_dump(void);

void
clockpro_dump(void)
{
	struct clockpro_state * const s = &clockpro;

	struct vm_page *pg;
	int ncold, nhot, ntest, nspeculative, ninitialref, nref;
	int newqlen, coldqlen, hotqlen, listqlen;

	newqlen = coldqlen = hotqlen = listqlen = 0;
	printf("npages=%d, ncold=%d, coldtarget=%d, newqlenmax=%d\n",
	    s->s_npages, s->s_ncold, s->s_coldtarget, s->s_newqlenmax);

#define	INITCOUNT()	\
	ncold = nhot = ntest = nspeculative = ninitialref = nref = 0

#define	COUNT(pg)	\
	if ((pg->pqflags & PQ_HOT) != 0) { \
		nhot++; \
	} else { \
		ncold++; \
		if ((pg->pqflags & PQ_TEST) != 0) { \
			ntest++; \
		} \
		if ((pg->pqflags & PQ_SPECULATIVE) != 0) { \
			nspeculative++; \
		} \
		if ((pg->pqflags & PQ_INITIALREF) != 0) { \
			ninitialref++; \
		} else if ((pg->pqflags & PQ_REFERENCED) != 0 || \
		    _pmap_is_referenced(pg)) { \
			nref++; \
		} \
	}

#define	PRINTCOUNT(name)	\
	printf("%s hot=%d, cold=%d, test=%d, speculative=%d, initialref=%d, " \
	    "nref=%d\n", \
	    (name), nhot, ncold, ntest, nspeculative, ninitialref, nref)

	INITCOUNT();
	TAILQ_FOREACH(pg, &clockpro_queue(s, CLOCKPRO_NEWQ)->q_q, pageq.queue) {
		if (clockpro_getq(pg) != CLOCKPRO_NEWQ) {
			printf("newq corrupt %p\n", pg);
		}
		COUNT(pg)
		newqlen++;
	}
	PRINTCOUNT("newq");

	INITCOUNT();
	TAILQ_FOREACH(pg, &clockpro_queue(s, CLOCKPRO_COLDQ)->q_q, pageq.queue) {
		if (clockpro_getq(pg) != CLOCKPRO_COLDQ) {
			printf("coldq corrupt %p\n", pg);
		}
		COUNT(pg)
		coldqlen++;
	}
	PRINTCOUNT("coldq");

	INITCOUNT();
	TAILQ_FOREACH(pg, &clockpro_queue(s, CLOCKPRO_HOTQ)->q_q, pageq.queue) {
		if (clockpro_getq(pg) != CLOCKPRO_HOTQ) {
			printf("hotq corrupt %p\n", pg);
		}
#if defined(LISTQ)
		if ((pg->pqflags & PQ_HOT) == 0) {
			printf("cold page in hotq: %p\n", pg);
		}
#endif /* defined(LISTQ) */
		COUNT(pg)
		hotqlen++;
	}
	PRINTCOUNT("hotq");

	INITCOUNT();
	TAILQ_FOREACH(pg, &clockpro_queue(s, CLOCKPRO_LISTQ)->q_q, pageq.queue) {
#if !defined(LISTQ)
		printf("listq %p\n", pg);
#endif /* !defined(LISTQ) */
		if (clockpro_getq(pg) != CLOCKPRO_LISTQ) {
			printf("listq corrupt %p\n", pg);
		}
		COUNT(pg)
		listqlen++;
	}
	PRINTCOUNT("listq");

	printf("newqlen=%d/%d, coldqlen=%d/%d, hotqlen=%d/%d, listqlen=%d/%d\n",
	    newqlen, pageq_len(clockpro_queue(s, CLOCKPRO_NEWQ)),
	    coldqlen, pageq_len(clockpro_queue(s, CLOCKPRO_COLDQ)),
	    hotqlen, pageq_len(clockpro_queue(s, CLOCKPRO_HOTQ)),
	    listqlen, pageq_len(clockpro_queue(s, CLOCKPRO_LISTQ)));
}

#endif /* defined(DDB) */

#if defined(PDSIM)
#if defined(DEBUG)
static void
pdsim_dumpq(int qidx)
{
	struct clockpro_state * const s = &clockpro;
	pageq_t *q = clockpro_queue(s, qidx);
	struct vm_page *pg;

	TAILQ_FOREACH(pg, &q->q_q, pageq.queue) {
		DPRINTF(" %" PRIu64 "%s%s%s%s%s%s",
		    pg->offset >> PAGE_SHIFT,
		    (pg->pqflags & PQ_HOT) ? "H" : "",
		    (pg->pqflags & PQ_TEST) ? "T" : "",
		    (pg->pqflags & PQ_REFERENCED) ? "R" : "",
		    _pmap_is_referenced(pg) ? "r" : "",
		    (pg->pqflags & PQ_INITIALREF) ? "I" : "",
		    (pg->pqflags & PQ_SPECULATIVE) ? "S" : ""
		    );
	}
}
#endif /* defined(DEBUG) */

void
pdsim_dump(const char *id)
{
#if defined(DEBUG)
	struct clockpro_state * const s = &clockpro;

	DPRINTF("  %s L(", id);
	pdsim_dumpq(CLOCKPRO_LISTQ);
	DPRINTF(" ) H(");
	pdsim_dumpq(CLOCKPRO_HOTQ);
	DPRINTF(" ) C(");
	pdsim_dumpq(CLOCKPRO_COLDQ);
	DPRINTF(" ) N(");
	pdsim_dumpq(CLOCKPRO_NEWQ);
	DPRINTF(" ) ncold=%d/%d, coldadj=%d\n",
	    s->s_ncold, s->s_coldtarget, coldadj);
#endif /* defined(DEBUG) */
}
#endif /* defined(PDSIM) */
