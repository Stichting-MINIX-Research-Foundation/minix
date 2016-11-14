/*	$NetBSD: pktqueue.c,v 1.8 2014/07/04 01:50:22 ozaki-r Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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
 * The packet queue (pktqueue) interface is a lockless IP input queue
 * which also abstracts and handles network ISR scheduling.  It provides
 * a mechanism to enable receiver-side packet steering (RPS).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pktqueue.c,v 1.8 2014/07/04 01:50:22 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/cpu.h>
#include <sys/pcq.h>
#include <sys/intr.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/percpu.h>

#include <net/pktqueue.h>

/*
 * WARNING: update this if struct pktqueue changes.
 */
#define	PKTQ_CLPAD	\
    MAX(COHERENCY_UNIT, COHERENCY_UNIT - sizeof(kmutex_t) - sizeof(u_int))

struct pktqueue {
	/*
	 * The lock used for a barrier mechanism.  The barrier counter,
	 * as well as the drop counter, are managed atomically though.
	 * Ensure this group is in a separate cache line.
	 */
	kmutex_t	pq_lock;
	volatile u_int	pq_barrier;
	uint8_t		_pad[PKTQ_CLPAD];

	/* The size of the queue, counters and the interrupt handler. */
	u_int		pq_maxlen;
	percpu_t *	pq_counters;
	void *		pq_sih;

	/* Finally, per-CPU queues. */
	pcq_t *		pq_queue[];
};

/* The counters of the packet queue. */
#define	PQCNT_ENQUEUE	0
#define	PQCNT_DEQUEUE	1
#define	PQCNT_DROP	2
#define	PQCNT_NCOUNTERS	3

typedef struct {
	uint64_t	count[PQCNT_NCOUNTERS];
} pktq_counters_t;

/* Special marker value used by pktq_barrier() mechanism. */
#define	PKTQ_MARKER	((void *)(~0ULL))

/*
 * The total size of pktqueue_t which depends on the number of CPUs.
 */
#define	PKTQUEUE_STRUCT_LEN(ncpu)	\
    roundup2(offsetof(pktqueue_t, pq_queue[ncpu]), coherency_unit)

pktqueue_t *
pktq_create(size_t maxlen, void (*intrh)(void *), void *sc)
{
	const u_int sflags = SOFTINT_NET | SOFTINT_MPSAFE | SOFTINT_RCPU;
	const size_t len = PKTQUEUE_STRUCT_LEN(ncpu);
	pktqueue_t *pq;
	percpu_t *pc;
	void *sih;

	if ((pc = percpu_alloc(sizeof(pktq_counters_t))) == NULL) {
		return NULL;
	}
	if ((sih = softint_establish(sflags, intrh, sc)) == NULL) {
		percpu_free(pc, sizeof(pktq_counters_t));
		return NULL;
	}

	pq = kmem_zalloc(len, KM_SLEEP);
	for (u_int i = 0; i < ncpu; i++) {
		pq->pq_queue[i] = pcq_create(maxlen, KM_SLEEP);
	}
	mutex_init(&pq->pq_lock, MUTEX_DEFAULT, IPL_NONE);
	pq->pq_maxlen = maxlen;
	pq->pq_counters = pc;
	pq->pq_sih = sih;

	return pq;
}

void
pktq_destroy(pktqueue_t *pq)
{
	const size_t len = PKTQUEUE_STRUCT_LEN(ncpu);

	for (u_int i = 0; i < ncpu; i++) {
		pcq_t *q = pq->pq_queue[i];
		KASSERT(pcq_peek(q) == NULL);
		pcq_destroy(q);
	}
	percpu_free(pq->pq_counters, sizeof(pktq_counters_t));
	softint_disestablish(pq->pq_sih);
	mutex_destroy(&pq->pq_lock);
	kmem_free(pq, len);
}

/*
 * - pktq_inc_counter: increment the counter given an ID.
 * - pktq_collect_counts: handler to sum up the counts from each CPU.
 * - pktq_getcount: return the effective count given an ID.
 */

static inline void
pktq_inc_count(pktqueue_t *pq, u_int i)
{
	percpu_t *pc = pq->pq_counters;
	pktq_counters_t *c;

	c = percpu_getref(pc);
	c->count[i]++;
	percpu_putref(pc);
}

static void
pktq_collect_counts(void *mem, void *arg, struct cpu_info *ci)
{
	const pktq_counters_t *c = mem;
	pktq_counters_t *sum = arg;

	for (u_int i = 0; i < PQCNT_NCOUNTERS; i++) {
		sum->count[i] += c->count[i];
	}
}

uint64_t
pktq_get_count(pktqueue_t *pq, pktq_count_t c)
{
	pktq_counters_t sum;

	if (c != PKTQ_MAXLEN) {
		memset(&sum, 0, sizeof(sum));
		percpu_foreach(pq->pq_counters, pktq_collect_counts, &sum);
	}
	switch (c) {
	case PKTQ_NITEMS:
		return sum.count[PQCNT_ENQUEUE] - sum.count[PQCNT_DEQUEUE];
	case PKTQ_DROPS:
		return sum.count[PQCNT_DROP];
	case PKTQ_MAXLEN:
		return pq->pq_maxlen;
	}
	return 0;
}

uint32_t
pktq_rps_hash(const struct mbuf *m __unused)
{
	/*
	 * XXX: No distribution yet; the softnet_lock contention
	 * XXX: must be eliminated first.
	 */
	return 0;
}

/*
 * pktq_enqueue: inject the packet into the end of the queue.
 *
 * => Must be called from the interrupt or with the preemption disabled.
 * => Consumes the packet and returns true on success.
 * => Returns false on failure; caller is responsible to free the packet.
 */
bool
pktq_enqueue(pktqueue_t *pq, struct mbuf *m, const u_int hash __unused)
{
#if defined(_RUMPKERNEL) || defined(_RUMP_NATIVE_ABI)
	const unsigned cpuid = curcpu()->ci_index;
#else
	const unsigned cpuid = hash % ncpu;
#endif

	KASSERT(kpreempt_disabled());

	if (__predict_false(!pcq_put(pq->pq_queue[cpuid], m))) {
		pktq_inc_count(pq, PQCNT_DROP);
		return false;
	}
	softint_schedule_cpu(pq->pq_sih, cpu_lookup(cpuid));
	pktq_inc_count(pq, PQCNT_ENQUEUE);
	return true;
}

/*
 * pktq_dequeue: take a packet from the queue.
 *
 * => Must be called with preemption disabled.
 * => Must ensure there are not concurrent dequeue calls.
 */
struct mbuf *
pktq_dequeue(pktqueue_t *pq)
{
	const struct cpu_info *ci = curcpu();
	const unsigned cpuid = cpu_index(ci);
	struct mbuf *m;

	m = pcq_get(pq->pq_queue[cpuid]);
	if (__predict_false(m == PKTQ_MARKER)) {
		/* Note the marker entry. */
		atomic_inc_uint(&pq->pq_barrier);
		return NULL;
	}
	if (__predict_true(m != NULL)) {
		pktq_inc_count(pq, PQCNT_DEQUEUE);
	}
	return m;
}

/*
 * pktq_barrier: waits for a grace period when all packets enqueued at
 * the moment of calling this routine will be processed.  This is used
 * to ensure that e.g. packets referencing some interface were drained.
 */
void
pktq_barrier(pktqueue_t *pq)
{
	u_int pending = 0;

	mutex_enter(&pq->pq_lock);
	KASSERT(pq->pq_barrier == 0);

	for (u_int i = 0; i < ncpu; i++) {
		pcq_t *q = pq->pq_queue[i];

		/* If the queue is empty - nothing to do. */
		if (pcq_peek(q) == NULL) {
			continue;
		}
		/* Otherwise, put the marker and entry. */
		while (!pcq_put(q, PKTQ_MARKER)) {
			kpause("pktqsync", false, 1, NULL);
		}
		kpreempt_disable();
		softint_schedule_cpu(pq->pq_sih, cpu_lookup(i));
		kpreempt_enable();
		pending++;
	}

	/* Wait for each queue to process the markers. */
	while (pq->pq_barrier != pending) {
		kpause("pktqsync", false, 1, NULL);
	}
	pq->pq_barrier = 0;
	mutex_exit(&pq->pq_lock);
}

/*
 * pktq_flush: free mbufs in all queues.
 *
 * => The caller must ensure there are no concurrent writers or flush calls.
 */
void
pktq_flush(pktqueue_t *pq)
{
	struct mbuf *m;

	for (u_int i = 0; i < ncpu; i++) {
		while ((m = pcq_get(pq->pq_queue[i])) != NULL) {
			pktq_inc_count(pq, PQCNT_DEQUEUE);
			m_freem(m);
		}
	}
}

/*
 * pktq_set_maxlen: create per-CPU queues using a new size and replace
 * the existing queues without losing any packets.
 */
int
pktq_set_maxlen(pktqueue_t *pq, size_t maxlen)
{
	const u_int slotbytes = ncpu * sizeof(pcq_t *);
	pcq_t **qs;

	if (!maxlen || maxlen > PCQ_MAXLEN)
		return EINVAL;
	if (pq->pq_maxlen == maxlen)
		return 0;

	/* First, allocate the new queues and replace them. */
	qs = kmem_zalloc(slotbytes, KM_SLEEP);
	for (u_int i = 0; i < ncpu; i++) {
		qs[i] = pcq_create(maxlen, KM_SLEEP);
	}
	mutex_enter(&pq->pq_lock);
	for (u_int i = 0; i < ncpu; i++) {
		/* Swap: store of a word is atomic. */
		pcq_t *q = pq->pq_queue[i];
		pq->pq_queue[i] = qs[i];
		qs[i] = q;
	}
	pq->pq_maxlen = maxlen;
	mutex_exit(&pq->pq_lock);

	/*
	 * At this point, the new packets are flowing into the new
	 * queues.  However, the old queues may have some packets
	 * present which are no longer being processed.  We are going
	 * to re-enqueue them.  This may change the order of packet
	 * arrival, but it is not considered an issue.
	 *
	 * There may be in-flight interrupts calling pktq_dequeue()
	 * which reference the old queues.  Issue a barrier to ensure
	 * that we are going to be the only pcq_get() callers on the
	 * old queues.
	 */
	pktq_barrier(pq);

	for (u_int i = 0; i < ncpu; i++) {
		struct mbuf *m;

		while ((m = pcq_get(qs[i])) != NULL) {
			while (!pcq_put(pq->pq_queue[i], m)) {
				kpause("pktqrenq", false, 1, NULL);
			}
		}
		pcq_destroy(qs[i]);
	}

	/* Well, that was fun. */
	kmem_free(qs, slotbytes);
	return 0;
}

int
sysctl_pktq_maxlen(SYSCTLFN_ARGS, pktqueue_t *pq)
{
	u_int nmaxlen = pktq_get_count(pq, PKTQ_MAXLEN);
	struct sysctlnode node = *rnode;
	int error;

	node.sysctl_data = &nmaxlen;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	return pktq_set_maxlen(pq, nmaxlen);
}

int
sysctl_pktq_count(SYSCTLFN_ARGS, pktqueue_t *pq, u_int count_id)
{
	int count = pktq_get_count(pq, count_id);
	struct sysctlnode node = *rnode;
	node.sysctl_data = &count;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}
