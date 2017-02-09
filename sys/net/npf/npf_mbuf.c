/*	$NetBSD: npf_mbuf.c,v 1.14 2015/07/12 23:51:53 rmind Exp $	*/

/*-
 * Copyright (c) 2009-2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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
 * NPF network buffer management interface.
 *
 * Network buffer in NetBSD is mbuf.  Internal mbuf structures are
 * abstracted within this source.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_mbuf.c,v 1.14 2015/07/12 23:51:53 rmind Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>

#include "npf_impl.h"

#define	NBUF_ENSURE_ALIGN	(MAX(COHERENCY_UNIT, 64))
#define	NBUF_ENSURE_MASK	(NBUF_ENSURE_ALIGN - 1)
#define	NBUF_ENSURE_ROUNDUP(x)	(((x) + NBUF_ENSURE_ALIGN) & ~NBUF_ENSURE_MASK)

void
nbuf_init(nbuf_t *nbuf, struct mbuf *m, const ifnet_t *ifp)
{
	u_int ifid = npf_ifmap_getid(ifp);

	KASSERT((m->m_flags & M_PKTHDR) != 0);

	nbuf->nb_mbuf0 = m;
	nbuf->nb_ifp = ifp;
	nbuf->nb_ifid = ifid;
	nbuf_reset(nbuf);
}

void
nbuf_reset(nbuf_t *nbuf)
{
	struct mbuf *m = nbuf->nb_mbuf0;

	nbuf->nb_mbuf = m;
	nbuf->nb_nptr = mtod(m, void *);
}

void *
nbuf_dataptr(nbuf_t *nbuf)
{
	KASSERT(nbuf->nb_nptr);
	return nbuf->nb_nptr;
}

size_t
nbuf_offset(const nbuf_t *nbuf)
{
	const struct mbuf *m = nbuf->nb_mbuf;
	const u_int off = (uintptr_t)nbuf->nb_nptr - mtod(m, uintptr_t);
	const int poff = m_length(nbuf->nb_mbuf0) - m_length(m) + off;

	return poff;
}

struct mbuf *
nbuf_head_mbuf(nbuf_t *nbuf)
{
	return nbuf->nb_mbuf0;
}

bool
nbuf_flag_p(const nbuf_t *nbuf, int flag)
{
	return (nbuf->nb_flags & flag) != 0;
}

void
nbuf_unset_flag(nbuf_t *nbuf, int flag)
{
	nbuf->nb_flags &= ~flag;
}

/*
 * nbuf_advance: advance in nbuf or chain by specified amount of bytes and,
 * if requested, ensure that the area *after* advance is contiguous.
 *
 * => Returns new pointer to data in nbuf or NULL if offset is invalid.
 * => Current nbuf and the offset is stored in the nbuf metadata.
 */
void *
nbuf_advance(nbuf_t *nbuf, size_t len, size_t ensure)
{
	struct mbuf *m = nbuf->nb_mbuf;
	u_int off, wmark;
	uint8_t *d;

	/* Offset with amount to advance. */
	off = (uintptr_t)nbuf->nb_nptr - mtod(m, uintptr_t) + len;
	wmark = m->m_len;

	/* Find the mbuf according to offset. */
	while (__predict_false(wmark <= off)) {
		m = m->m_next;
		if (__predict_false(m == NULL)) {
			/*
			 * If end of the chain, then the offset is
			 * higher than packet length.
			 */
			return NULL;
		}
		wmark += m->m_len;
	}
	KASSERT(off < m_length(nbuf->nb_mbuf0));

	/* Offset in mbuf data. */
	d = mtod(m, uint8_t *);
	KASSERT(off >= (wmark - m->m_len));
	d += (off - (wmark - m->m_len));

	nbuf->nb_mbuf = m;
	nbuf->nb_nptr = d;

	if (ensure) {
		/* Ensure contiguousness (may change nbuf chain). */
		d = nbuf_ensure_contig(nbuf, ensure);
	}
	return d;
}

/*
 * nbuf_ensure_contig: check whether the specified length from the current
 * point in the nbuf is contiguous.  If not, rearrange the chain to be so.
 *
 * => Returns pointer to the data at the current offset in the buffer.
 * => Returns NULL on failure and nbuf becomes invalid.
 */
void *
nbuf_ensure_contig(nbuf_t *nbuf, size_t len)
{
	const struct mbuf * const n = nbuf->nb_mbuf;
	const size_t off = (uintptr_t)nbuf->nb_nptr - mtod(n, uintptr_t);

	KASSERT(off <= n->m_len);

	if (__predict_false(n->m_len < (off + len))) {
		struct mbuf *m = nbuf->nb_mbuf0;
		const size_t foff = nbuf_offset(nbuf);
		const size_t plen = m_length(m);
		const size_t mlen = m->m_len;
		size_t target;
		bool success;

		npf_stats_inc(NPF_STAT_NBUF_NONCONTIG);

		/* Attempt to round-up to NBUF_ENSURE_ALIGN bytes. */
		if ((target = NBUF_ENSURE_ROUNDUP(foff + len)) > plen) {
			target = foff + len;
		}

		/* Rearrange the chain to be contiguous. */
		KASSERT((m->m_flags & M_PKTHDR) != 0);
		success = m_ensure_contig(&m, target);
		KASSERT(m != NULL);

		/* If no change in the chain: return what we have. */
		if (m == nbuf->nb_mbuf0 && m->m_len == mlen) {
			return success ? nbuf->nb_nptr : NULL;
		}

		/*
		 * The mbuf chain was re-arranged.  Update the pointers
		 * accordingly and indicate that the references to the data
		 * might need a reset.
		 */
		KASSERT((m->m_flags & M_PKTHDR) != 0);
		nbuf->nb_mbuf0 = m;
		nbuf->nb_mbuf = m;

		KASSERT(foff < m->m_len && foff < m_length(m));
		nbuf->nb_nptr = mtod(m, uint8_t *) + foff;
		nbuf->nb_flags |= NBUF_DATAREF_RESET;

		if (!success) {
			npf_stats_inc(NPF_STAT_NBUF_CONTIG_FAIL);
			return NULL;
		}
	}
	return nbuf->nb_nptr;
}

void *
nbuf_ensure_writable(nbuf_t *nbuf, size_t len)
{
	struct mbuf *m = nbuf->nb_mbuf;
	const u_int off = (uintptr_t)nbuf->nb_nptr - mtod(m, uintptr_t);
	const int tlen = off + len;
	bool head_buf;

	KASSERT(off < m_length(nbuf->nb_mbuf0));

	if (!M_UNWRITABLE(m, tlen)) {
		return nbuf->nb_nptr;
	}
	head_buf = (nbuf->nb_mbuf0 == m);
	if (m_makewritable(&m, 0, tlen, M_NOWAIT)) {
		memset(nbuf, 0, sizeof(nbuf_t));
		return NULL;
	}
	if (head_buf) {
		KASSERT((m->m_flags & M_PKTHDR) != 0);
		KASSERT(off < m_length(m));
		nbuf->nb_mbuf0 = m;
	}
	nbuf->nb_mbuf = m;
	nbuf->nb_nptr = mtod(m, uint8_t *) + off;

	return nbuf->nb_nptr;
}

bool
nbuf_cksum_barrier(nbuf_t *nbuf, int di)
{
	struct mbuf *m;

	if (di != PFIL_OUT) {
		return false;
	}
	m = nbuf->nb_mbuf0;
	KASSERT((m->m_flags & M_PKTHDR) != 0);

	if (m->m_pkthdr.csum_flags & (M_CSUM_TCPv4 | M_CSUM_UDPv4)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~(M_CSUM_TCPv4 | M_CSUM_UDPv4);
		return true;
	}
	return false;
}

/*
 * nbuf_add_tag: add a tag to specified network buffer.
 *
 * => Returns 0 on success or errno on failure.
 */
int
nbuf_add_tag(nbuf_t *nbuf, uint32_t key, uint32_t val)
{
	struct mbuf *m = nbuf->nb_mbuf0;
	struct m_tag *mt;
	uint32_t *dat;

	KASSERT((m->m_flags & M_PKTHDR) != 0);

	mt = m_tag_get(PACKET_TAG_NPF, sizeof(uint32_t), M_NOWAIT);
	if (mt == NULL) {
		return ENOMEM;
	}
	dat = (uint32_t *)(mt + 1);
	*dat = val;
	m_tag_prepend(m, mt);
	return 0;
}

/*
 * nbuf_find_tag: find a tag in specified network buffer.
 *
 * => Returns 0 on success or errno on failure.
 */
int
nbuf_find_tag(nbuf_t *nbuf, uint32_t key, void **data)
{
	struct mbuf *m = nbuf->nb_mbuf0;
	struct m_tag *mt;

	KASSERT((m->m_flags & M_PKTHDR) != 0);

	mt = m_tag_find(m, PACKET_TAG_NPF, NULL);
	if (mt == NULL) {
		return EINVAL;
	}
	*data = (void *)(mt + 1);
	return 0;
}
