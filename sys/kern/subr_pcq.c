/*	$NetBSD: subr_pcq.c,v 1.9 2015/01/08 23:39:57 riastradh Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Lockless producer/consumer queue.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_pcq.c,v 1.9 2015/01/08 23:39:57 riastradh Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/kmem.h>

#include <sys/pcq.h>

/*
 * Internal producer-consumer queue structure.  Note: providing a separate
 * cache-line both for pcq_t::pcq_pc and pcq_t::pcq_items.
 */
struct pcq {
	u_int			pcq_nitems;
	uint8_t			pcq_pad1[COHERENCY_UNIT - sizeof(u_int)];
	volatile uint32_t	pcq_pc;
	uint8_t			pcq_pad2[COHERENCY_UNIT - sizeof(uint32_t)];
	void * volatile		pcq_items[];
};

/*
 * Producer (p) - stored in the lower 16 bits of pcq_t::pcq_pc.
 * Consumer (c) - in the higher 16 bits.
 *
 * We have a limitation of 16 bits i.e. 0xffff items in the queue.
 * The PCQ_MAXLEN constant is set accordingly.
 */

static inline void
pcq_split(uint32_t v, u_int *p, u_int *c)
{

	*p = v & 0xffff;
	*c = v >> 16;
}

static inline uint32_t
pcq_combine(u_int p, u_int c)
{

	return p | (c << 16);
}

static inline u_int
pcq_advance(pcq_t *pcq, u_int pc)
{

	if (__predict_false(++pc == pcq->pcq_nitems)) {
		return 0;
	}
	return pc;
}

/*
 * pcq_put: place an item at the end of the queue.
 */
bool
pcq_put(pcq_t *pcq, void *item)
{
	uint32_t v, nv;
	u_int op, p, c;

	KASSERT(item != NULL);

	do {
		v = pcq->pcq_pc;
		pcq_split(v, &op, &c);
		p = pcq_advance(pcq, op);
		if (p == c) {
			/* Queue is full. */
			return false;
		}
		nv = pcq_combine(p, c);
	} while (atomic_cas_32(&pcq->pcq_pc, v, nv) != v);

	/*
	 * Ensure that the update to pcq_pc is globally visible before the
	 * data item.  See pcq_get().  This also ensures that any changes
	 * that the caller made to the data item are globally visible
	 * before we put it onto the list.
	 */
#ifndef __HAVE_ATOMIC_AS_MEMBAR
	membar_producer();
#endif
	pcq->pcq_items[op] = item;

	/*
	 * Synchronization activity to wake up the consumer will ensure
	 * that the update to pcq_items[] is visible before the wakeup
	 * arrives.  So, we do not need an additonal memory barrier here.
	 */
	return true;
}

/*
 * pcq_peek: return the next item from the queue without removal.
 */
void *
pcq_peek(pcq_t *pcq)
{
	const uint32_t v = pcq->pcq_pc;
	u_int p, c;

	pcq_split(v, &p, &c);

	/* See comment on race below in pcq_get(). */
	return (p == c) ? NULL :
	    (membar_datadep_consumer(), pcq->pcq_items[c]);
}

/*
 * pcq_get: remove and return the next item for consumption or NULL if empty.
 *
 * => The caller must prevent concurrent gets from occuring.
 */
void *
pcq_get(pcq_t *pcq)
{
	uint32_t v, nv;
	u_int p, c;
	void *item;

	v = pcq->pcq_pc;
	pcq_split(v, &p, &c);
	if (p == c) {
		/* Queue is empty: nothing to return. */
		return NULL;
	}
	/* Make sure we read pcq->pcq_pc before pcq->pcq_items[c].  */
	membar_datadep_consumer();
	item = pcq->pcq_items[c];
	if (item == NULL) {
		/*
		 * Raced with sender: we rely on a notification (e.g. softint
		 * or wakeup) being generated after the producer's pcq_put(),
		 * causing us to retry pcq_get() later.
		 */
		return NULL;
	}
	pcq->pcq_items[c] = NULL;
	c = pcq_advance(pcq, c);
	nv = pcq_combine(p, c);

	/*
	 * Ensure that update to pcq_items[] becomes globally visible
	 * before the update to pcq_pc.  If it were reodered to occur
	 * after it, we could in theory wipe out a modification made
	 * to pcq_items[] by pcq_put().
	 */
#ifndef __HAVE_ATOMIC_AS_MEMBAR
	membar_producer();
#endif
	while (__predict_false(atomic_cas_32(&pcq->pcq_pc, v, nv) != v)) {
		v = pcq->pcq_pc;
		pcq_split(v, &p, &c);
		c = pcq_advance(pcq, c);
		nv = pcq_combine(p, c);
	}
	return item;
}

pcq_t *
pcq_create(size_t nitems, km_flag_t kmflags)
{
	pcq_t *pcq;

	KASSERT(nitems > 0 || nitems <= PCQ_MAXLEN);

	pcq = kmem_zalloc(offsetof(pcq_t, pcq_items[nitems]), kmflags);
	if (pcq == NULL) {
		return NULL;
	}
	pcq->pcq_nitems = nitems;
	return pcq;
}

void
pcq_destroy(pcq_t *pcq)
{

	kmem_free(pcq, offsetof(pcq_t, pcq_items[pcq->pcq_nitems]));
}

size_t
pcq_maxitems(pcq_t *pcq)
{

	return pcq->pcq_nitems;
}
