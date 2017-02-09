/* $NetBSD: ixgbe_netbsd.c,v 1.3 2015/02/04 09:05:53 msaitoh Exp $ */
/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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
#include <sys/param.h>

#include <sys/atomic.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/workqueue.h>

#include "ixgbe_netbsd.h"

void
ixgbe_dma_tag_destroy(ixgbe_dma_tag_t *dt)
{
	kmem_free(dt, sizeof(*dt));
}

int
ixgbe_dma_tag_create(bus_dma_tag_t dmat, bus_size_t alignment,
    bus_size_t boundary, bus_size_t maxsize, int nsegments,
    bus_size_t maxsegsize, int flags, ixgbe_dma_tag_t **dtp)
{
	ixgbe_dma_tag_t *dt;

	*dtp = NULL;

	if ((dt = kmem_zalloc(sizeof(*dt), KM_SLEEP)) == NULL)
		return ENOMEM;

	dt->dt_dmat = dmat;
	dt->dt_alignment = alignment;
	dt->dt_boundary = boundary;
	dt->dt_maxsize = maxsize;
	dt->dt_nsegments = nsegments;
	dt->dt_maxsegsize = maxsegsize;
	dt->dt_flags = flags;
	*dtp = dt;

	return 0;
}

void
ixgbe_dmamap_destroy(ixgbe_dma_tag_t *dt, bus_dmamap_t dmam)
{
	bus_dmamap_destroy(dt->dt_dmat, dmam);
}

void
ixgbe_dmamap_sync(ixgbe_dma_tag_t *dt, bus_dmamap_t dmam, int ops)
{
        bus_dmamap_sync(dt->dt_dmat, dmam, 0, dt->dt_maxsize, ops);
}

void
ixgbe_dmamap_unload(ixgbe_dma_tag_t *dt, bus_dmamap_t dmam)
{
	bus_dmamap_unload(dt->dt_dmat, dmam);
}

int
ixgbe_dmamap_create(ixgbe_dma_tag_t *dt, int flags, bus_dmamap_t *dmamp)
{
	return bus_dmamap_create(dt->dt_dmat, dt->dt_maxsize, dt->dt_nsegments,
	    dt->dt_maxsegsize, dt->dt_boundary, flags, dmamp);
}

static void
ixgbe_putext(ixgbe_extmem_t *em)
{
	ixgbe_extmem_head_t *eh = em->em_head;

	mutex_enter(&eh->eh_mtx);

	TAILQ_INSERT_HEAD(&eh->eh_freelist, em, em_link);

	mutex_exit(&eh->eh_mtx);

	return;
}

static ixgbe_extmem_t *
ixgbe_getext(ixgbe_extmem_head_t *eh, size_t size)
{
	ixgbe_extmem_t *em;

	mutex_enter(&eh->eh_mtx);

	TAILQ_FOREACH(em, &eh->eh_freelist, em_link) {
		if (em->em_size >= size)
			break;
	}

	if (em != NULL)
		TAILQ_REMOVE(&eh->eh_freelist, em, em_link);

	mutex_exit(&eh->eh_mtx);

	return em;
}

static ixgbe_extmem_t *
ixgbe_newext(ixgbe_extmem_head_t *eh, bus_dma_tag_t dmat, size_t size)
{
	ixgbe_extmem_t *em;
	int nseg, rc;

	em = kmem_zalloc(sizeof(*em), KM_SLEEP);

	if (em == NULL)
		return NULL;

	rc = bus_dmamem_alloc(dmat, size, PAGE_SIZE, 0, &em->em_seg, 1, &nseg,
	    BUS_DMA_WAITOK);

	if (rc != 0)
		goto post_zalloc_err;

	rc = bus_dmamem_map(dmat, &em->em_seg, 1, size, &em->em_vaddr,
	    BUS_DMA_WAITOK);

	if (rc != 0)
		goto post_dmamem_err;

	em->em_dmat = dmat;
	em->em_size = size;
	em->em_head = eh;

	return em;
post_dmamem_err:
	bus_dmamem_free(dmat, &em->em_seg, 1);
post_zalloc_err:
	kmem_free(em, sizeof(*em));
	return NULL;
}

void
ixgbe_jcl_reinit(ixgbe_extmem_head_t *eh, bus_dma_tag_t dmat, int nbuf,
    size_t size)
{
	int i;
	ixgbe_extmem_t *em;

	if (!eh->eh_initialized) {
		TAILQ_INIT(&eh->eh_freelist);
		mutex_init(&eh->eh_mtx, MUTEX_DEFAULT, IPL_NET);
		eh->eh_initialized = true;
	}

	while ((em = ixgbe_getext(eh, 0)) != NULL) {
		KASSERT(em->em_vaddr != NULL);
		bus_dmamem_unmap(dmat, em->em_vaddr, em->em_size);
		bus_dmamem_free(dmat, &em->em_seg, 1);
		memset(em, 0, sizeof(*em));
		kmem_free(em, sizeof(*em));
	}

	for (i = 0; i < nbuf; i++) {
		if ((em = ixgbe_newext(eh, dmat, size)) == NULL) {
			printf("%s: only %d of %d jumbo buffers allocated\n",
			    __func__, i, nbuf);
			break;
		}
		ixgbe_putext(em);
	}
}

static void
ixgbe_jcl_free(struct mbuf *m, void *buf, size_t size, void *arg)
{
	ixgbe_extmem_t *em = arg;

	KASSERT(em->em_size == size);

	ixgbe_putext(em);
	/* this is an abstraction violation, but it does not lead to a
	 * double-free
	 */
	if (__predict_true(m != NULL)) {
		KASSERT(m->m_type != MT_FREE);
		m->m_type = MT_FREE;
		pool_cache_put(mb_cache, m);
	}
}

/* XXX need to wait for the system to finish with each jumbo mbuf and
 * free it before detaching the driver from the device.
 */
struct mbuf *
ixgbe_getjcl(ixgbe_extmem_head_t *eh, int nowait /* M_DONTWAIT */,
    int type /* MT_DATA */, int flags /* M_PKTHDR */, size_t size)
{
	ixgbe_extmem_t *em;
	struct mbuf *m;

	if ((flags & M_PKTHDR) != 0)
		m = m_gethdr(nowait, type);
	else
		m = m_get(nowait, type);

	if (m == NULL)
		return NULL;

	em = ixgbe_getext(eh, size);
	if (em == NULL) {
		m_freem(m);
		return NULL;
	}

	MEXTADD(m, em->em_vaddr, em->em_size, M_DEVBUF, &ixgbe_jcl_free, em);

	if ((m->m_flags & M_EXT) == 0) {
		ixgbe_putext(em);
		m_freem(m);
		return NULL;
	}

	return m;
}
