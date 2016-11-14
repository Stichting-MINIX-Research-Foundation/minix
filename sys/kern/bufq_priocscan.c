/*	$NetBSD: bufq_priocscan.c,v 1.18 2014/01/28 12:50:54 martin Exp $	*/

/*-
 * Copyright (c)2004,2005,2006,2008,2009,2011,2012 YAMAMOTO Takashi,
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bufq_priocscan.c,v 1.18 2014/01/28 12:50:54 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/bufq_impl.h>
#include <sys/kmem.h>
#include <sys/rbtree.h>

#undef	PRIOCSCAN_USE_GLOBAL_POSITION

/*
 * Cyclical scan (CSCAN)
 */

struct cscan_key {
	daddr_t	k_rawblkno;
	int k_cylinder;
};

struct cscan_queue {
	rb_tree_t cq_buffers;		/* ordered list of buffers */
#if !defined(PRIOCSCAN_USE_GLOBAL_POSITION)
	struct cscan_key cq_lastkey;	/* key of last request */
#endif /* !defined(PRIOCSCAN_USE_GLOBAL_POSITION) */
	int cq_sortby;			/* BUFQ_SORT_MASK */
	rb_tree_ops_t cq_ops;
};

static signed int
buf_cmp(const struct buf *b1, const struct buf *b2, int sortby)
{

	if (buf_inorder(b2, b1, sortby)) {
		return 1;	/* b1 > b2 */
	}
	if (buf_inorder(b1, b2, sortby)) {
		return -1;	/* b1 < b2 */
	}
	return 0;
}

/* return positive if n1 > n2 */
static signed int
cscan_tree_compare_nodes(void *context, const void *n1, const void *n2)
{
	const struct cscan_queue * const q = context;
	const struct buf * const b1 = n1;
	const struct buf * const b2 = n2;
	const int sortby = q->cq_sortby;
	const int diff = buf_cmp(b1, b2, sortby);

	/*
	 * XXX rawblkno/cylinder might not be unique.  eg. unbuffered i/o
	 */

	if (diff != 0) {
		return diff;
	}

	/*
	 * XXX rawblkno/cylinder might not be unique.  eg. unbuffered i/o
	 */
	if (b1 > b2) {
		return 1;
	}
	if (b1 < b2) {
		return -1;
	}
	return 0;
}

/* return positive if n1 > k2 */
static signed int
cscan_tree_compare_key(void *context, const void *n1, const void *k2)
{
	const struct cscan_queue * const q = context;
	const struct buf * const b1 = n1;
	const struct cscan_key * const key = k2;
	const struct buf tmp = {
		.b_rawblkno = key->k_rawblkno,
		.b_cylinder = key->k_cylinder,
	};
	const struct buf *b2 = &tmp;
	const int sortby = q->cq_sortby;

	return buf_cmp(b1, b2, sortby);
}

static void __unused
cscan_dump(struct cscan_queue *cq)
{
	const int sortby = cq->cq_sortby;
	struct buf *bp;

	RB_TREE_FOREACH(bp, &cq->cq_buffers) {
		if (sortby == BUFQ_SORT_RAWBLOCK) {
			printf(" %jd", (intmax_t)bp->b_rawblkno);
		} else {
			printf(" %jd/%jd",
			    (intmax_t)bp->b_cylinder, (intmax_t)bp->b_rawblkno);
		}
	}
}

static inline bool
cscan_empty(struct cscan_queue *q)
{

	/* XXX this might do more work than necessary */
	return rb_tree_iterate(&q->cq_buffers, NULL, RB_DIR_LEFT) == NULL;
}

static void
cscan_put(struct cscan_queue *q, struct buf *bp)
{
	struct buf *obp __diagused;

	obp = rb_tree_insert_node(&q->cq_buffers, bp);
	KASSERT(obp == bp); /* see cscan_tree_compare_nodes */
}

static struct buf *
cscan_get(struct cscan_queue *q, int remove, struct cscan_key *key)
{
	struct buf *bp;

	bp = rb_tree_find_node_geq(&q->cq_buffers, key);
	KDASSERT(bp == NULL || cscan_tree_compare_key(q, bp, key) >= 0);
	if (bp == NULL) {
		bp = rb_tree_iterate(&q->cq_buffers, NULL, RB_DIR_LEFT);
		KDASSERT(cscan_tree_compare_key(q, bp, key) < 0);
	}
	if (bp != NULL && remove) {
#if defined(DEBUG)
		struct buf *nbp;
#endif /* defined(DEBUG) */

		rb_tree_remove_node(&q->cq_buffers, bp);
		/*
		 * remember the head position.
		 */
		key->k_cylinder = bp->b_cylinder;
		key->k_rawblkno = bp->b_rawblkno + (bp->b_bcount >> DEV_BSHIFT);
#if defined(DEBUG)
		nbp = rb_tree_find_node_geq(&q->cq_buffers, key);
		if (nbp != NULL && cscan_tree_compare_nodes(q, nbp, bp) < 0) {
			panic("%s: wrong order %p < %p\n", __func__,
			    nbp, bp);
		}
#endif /* defined(DEBUG) */
	}
	return bp;
}

static void
cscan_init(struct cscan_queue *q, int sortby)
{
	static const rb_tree_ops_t cscan_tree_ops = {
		.rbto_compare_nodes = cscan_tree_compare_nodes,
		.rbto_compare_key = cscan_tree_compare_key,
		.rbto_node_offset = offsetof(struct buf, b_u.u_rbnode),
		.rbto_context = NULL,
	};

	q->cq_sortby = sortby;
	/* XXX copy ops to workaround rbtree.h API limitation */
	q->cq_ops = cscan_tree_ops;
	q->cq_ops.rbto_context = q;
	rb_tree_init(&q->cq_buffers, &q->cq_ops);
}

/*
 * Per-prioritiy CSCAN.
 *
 * XXX probably we should have a way to raise
 * priority of the on-queue requests.
 */
#define	PRIOCSCAN_NQUEUE	3

struct priocscan_queue {
	struct cscan_queue q_queue;
	unsigned int q_burst;
};

struct bufq_priocscan {
	struct priocscan_queue bq_queue[PRIOCSCAN_NQUEUE];

#if defined(PRIOCSCAN_USE_GLOBAL_POSITION)
	/*
	 * XXX using "global" head position can reduce positioning time
	 * when switching between queues.
	 * although it might affect against fairness.
	 */
	struct cscan_key bq_lastkey;
#endif
};

/*
 * how many requests to serve when having pending requests on other queues.
 *
 * XXX tune
 * be careful: while making these values larger likely
 * increases the total throughput, it can also increase latencies
 * for some workloads.
 */
const int priocscan_burst[] = {
	64, 16, 4
};

static void bufq_priocscan_init(struct bufq_state *);
static void bufq_priocscan_put(struct bufq_state *, struct buf *);
static struct buf *bufq_priocscan_get(struct bufq_state *, int);

BUFQ_DEFINE(priocscan, 40, bufq_priocscan_init);

static inline struct cscan_queue *bufq_priocscan_selectqueue(
    struct bufq_priocscan *, const struct buf *);

static inline struct cscan_queue *
bufq_priocscan_selectqueue(struct bufq_priocscan *q, const struct buf *bp)
{
	static const int priocscan_priomap[] = {
		[BPRIO_TIMENONCRITICAL] = 2,
		[BPRIO_TIMELIMITED] = 1,
		[BPRIO_TIMECRITICAL] = 0
	};

	return &q->bq_queue[priocscan_priomap[BIO_GETPRIO(bp)]].q_queue;
}

static void
bufq_priocscan_put(struct bufq_state *bufq, struct buf *bp)
{
	struct bufq_priocscan *q = bufq->bq_private;
	struct cscan_queue *cq;

	cq = bufq_priocscan_selectqueue(q, bp);
	cscan_put(cq, bp);
}

static struct buf *
bufq_priocscan_get(struct bufq_state *bufq, int remove)
{
	struct bufq_priocscan *q = bufq->bq_private;
	struct priocscan_queue *pq, *npq;
	struct priocscan_queue *first; /* highest priority non-empty queue */
	const struct priocscan_queue *epq;
	struct buf *bp;
	bool single; /* true if there's only one non-empty queue */

	/*
	 * find the highest priority non-empty queue.
	 */
	pq = &q->bq_queue[0];
	epq = pq + PRIOCSCAN_NQUEUE;
	for (; pq < epq; pq++) {
		if (!cscan_empty(&pq->q_queue)) {
			break;
		}
	}
	if (pq == epq) {
		/*
		 * all our queues are empty.  there's nothing to serve.
		 */
		return NULL;
	}
	first = pq;

	/*
	 * scan the rest of queues.
	 *
	 * if we have two or more non-empty queues, we serve the highest
	 * priority one with non-zero burst count.
	 */
	single = true;
	for (npq = pq + 1; npq < epq; npq++) {
		if (!cscan_empty(&npq->q_queue)) {
			/*
			 * we found another non-empty queue.
			 * it means that a queue needs to consume its burst
			 * count to be served.
			 */
			single = false;

			/*
			 * check if our current candidate queue has already
			 * exhausted its burst count.
			 */
			if (pq->q_burst > 0) {
				break;
			}
			pq = npq;
		}
	}
	if (single) {
		/*
		 * there's only a non-empty queue.
		 * just serve it without consuming its burst count.
		 */
		KASSERT(pq == first);
	} else {
		/*
		 * there are two or more non-empty queues.
		 */
		if (pq->q_burst == 0) {
			/*
			 * no queues can be served because they have already
			 * exhausted their burst count.
			 */
			unsigned int i;
#ifdef DEBUG
			for (i = 0; i < PRIOCSCAN_NQUEUE; i++) {
				pq = &q->bq_queue[i];
				if (!cscan_empty(&pq->q_queue) && pq->q_burst) {
					panic("%s: inconsist", __func__);
				}
			}
#endif /* DEBUG */
			/*
			 * reset burst counts.
			 */
			if (remove) {
				for (i = 0; i < PRIOCSCAN_NQUEUE; i++) {
					pq = &q->bq_queue[i];
					pq->q_burst = priocscan_burst[i];
				}
			}

			/*
			 * serve the highest priority non-empty queue.
			 */
			pq = first;
		}
		/*
		 * consume the burst count.
		 *
		 * XXX account only by number of requests.  is it good enough?
		 */
		if (remove) {
			KASSERT(pq->q_burst > 0);
			pq->q_burst--;
		}
	}

	/*
	 * finally, get a request from the selected queue.
	 */
	KDASSERT(!cscan_empty(&pq->q_queue));
	bp = cscan_get(&pq->q_queue, remove,
#if defined(PRIOCSCAN_USE_GLOBAL_POSITION)
	    &q->bq_lastkey
#else /* defined(PRIOCSCAN_USE_GLOBAL_POSITION) */
	    &pq->q_queue.cq_lastkey
#endif /* defined(PRIOCSCAN_USE_GLOBAL_POSITION) */
	    );
	KDASSERT(bp != NULL);
	KDASSERT(&pq->q_queue == bufq_priocscan_selectqueue(q, bp));

	return bp;
}

static struct buf *
bufq_priocscan_cancel(struct bufq_state *bufq, struct buf *bp)
{
	struct bufq_priocscan * const q = bufq->bq_private;
	unsigned int i;

	for (i = 0; i < PRIOCSCAN_NQUEUE; i++) {
		struct cscan_queue * const cq = &q->bq_queue[i].q_queue;
		struct buf *it;

		/*
		 * XXX probably could be faster but the cancel functionality
		 * is not widely used anyway.
		 */
		RB_TREE_FOREACH(it, &cq->cq_buffers) {
			if (it == bp) {
				rb_tree_remove_node(&cq->cq_buffers, bp);
				return bp;
			}
		}
	}
	return NULL;
}

static void
bufq_priocscan_fini(struct bufq_state *bufq)
{

	KASSERT(bufq->bq_private != NULL);
	kmem_free(bufq->bq_private, sizeof(struct bufq_priocscan));
}

static void
bufq_priocscan_init(struct bufq_state *bufq)
{
	struct bufq_priocscan *q;
	const int sortby = bufq->bq_flags & BUFQ_SORT_MASK;
	unsigned int i;

	bufq->bq_get = bufq_priocscan_get;
	bufq->bq_put = bufq_priocscan_put;
	bufq->bq_cancel = bufq_priocscan_cancel;
	bufq->bq_fini = bufq_priocscan_fini;
	bufq->bq_private = kmem_zalloc(sizeof(struct bufq_priocscan), KM_SLEEP);

	q = bufq->bq_private;
	for (i = 0; i < PRIOCSCAN_NQUEUE; i++) {
		struct cscan_queue *cq = &q->bq_queue[i].q_queue;

		cscan_init(cq, sortby);
	}
}
