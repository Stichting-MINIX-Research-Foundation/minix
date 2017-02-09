/*	$NetBSD: if_uba.c,v 1.31 2010/11/13 13:52:10 uebayasi Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	@(#)if_uba.c	7.16 (Berkeley) 12/16/90
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_uba.c,v 1.31 2010/11/13 13:52:10 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#include <sys/bus.h>

#include <dev/qbus/if_uba.h>
#include <dev/qbus/ubareg.h>
#include <dev/qbus/ubavar.h>

static	struct mbuf *getmcl(void);

/*
 * Routines supporting UNIBUS network interfaces.
 *
 * TODO:
 *	Support interfaces using only one BDP statically.
 */

/*
 * Init UNIBUS for interface whose headers of size hlen are to
 * end on a page boundary.  We allocate a UNIBUS map register for the page
 * with the header, and nmr more UNIBUS map registers for i/o on the adapter,
 * doing this once for each read and once for each write buffer.  We also
 * allocate page frames in the mbuffer pool for these pages.
 *
 * Recent changes:
 *	No special "header pages" anymore.
 *	Recv packets are always put in clusters.
 *	"size" is the maximum buffer size, may not be bigger than MCLBYTES.
 */
int
if_ubaminit(struct ifubinfo *ifu, struct uba_softc *uh, int size,
    struct ifrw *ifr, int nr, struct ifxmt *ifw, int nw)
{
	struct mbuf *m;
	int totsz, i, error, rseg, nm = nr;
	bus_dma_segment_t seg;
	void *vaddr;

#ifdef DIAGNOSTIC
	if (size > MCLBYTES)
		panic("if_ubaminit: size > MCLBYTES");
#endif
	ifu->iff_softc = uh;
	/*
	 * Get DMA memory for transmit buffers.
	 * Buffer size are rounded up to a multiple of the uba page size,
	 * then allocated contiguous.
	 */
	size = (size + UBA_PGOFSET) & ~UBA_PGOFSET;
	totsz = size * nw;
	if ((error = bus_dmamem_alloc(uh->uh_dmat, totsz, PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)))
		return error;
	if ((error = bus_dmamem_map(uh->uh_dmat, &seg, rseg, totsz, &vaddr,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT))) {
		bus_dmamem_free(uh->uh_dmat, &seg, rseg);
		return error;
	}

	/*
	 * Create receive and transmit maps.
	 * Alloc all resources now so we won't fail in the future.
	 */

	for (i = 0; i < nr; i++) {
		if ((error = bus_dmamap_create(uh->uh_dmat, size, 1,
		    size, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &ifr[i].ifrw_map))) {
			nr = i;
			nm = nw = 0;
			goto bad;
		}
	}
	for (i = 0; i < nw; i++) {
		if ((error = bus_dmamap_create(uh->uh_dmat, size, 1,
		    size, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &ifw[i].ifw_map))) {
			nw = i;
			nm = 0;
			goto bad;
		}
	}
	/*
	 * Preload the rx maps with mbuf clusters.
	 */
	for (i = 0; i < nm; i++) {
		if ((m = getmcl()) == NULL) {
			nm = i;
			goto bad;
		}
		ifr[i].ifrw_mbuf = m;
		bus_dmamap_load(uh->uh_dmat, ifr[i].ifrw_map,
		    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT);

	}
	/*
	 * Load the tx maps with DMA memory (common case).
	 */
	for (i = 0; i < nw; i++) {
		ifw[i].ifw_vaddr = (char *)vaddr + size * i;
		ifw[i].ifw_size = size;
		bus_dmamap_load(uh->uh_dmat, ifw[i].ifw_map,
		    ifw[i].ifw_vaddr, ifw[i].ifw_size, NULL, BUS_DMA_NOWAIT);
	}
	return 0;
bad:
	while (--nm >= 0) {
		bus_dmamap_unload(uh->uh_dmat, ifr[nw].ifrw_map);
		m_freem(ifr[nm].ifrw_mbuf);
	}
	while (--nw >= 0)
		bus_dmamap_destroy(uh->uh_dmat, ifw[nw].ifw_map);
	while (--nr >= 0)
		bus_dmamap_destroy(uh->uh_dmat, ifr[nw].ifrw_map);
	return (0);
}

struct mbuf *
getmcl(void)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return 0;
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return 0;
	}
	return m;
}

/*
 * Pull read data off a interface.
 * Totlen is length of data, with local net header stripped.
 * When full cluster sized units are present
 * on the interface on cluster boundaries we can get them more
 * easily by remapping, and take advantage of this here.
 * Save a pointer to the interface structure and the total length,
 * so that protocols can determine where incoming packets arrived.
 * Note: we may be called to receive from a transmit buffer by some
 * devices.  In that case, we must force normal mapping of the buffer,
 * so that the correct data will appear (only unibus maps are
 * changed when remapping the transmit buffers).
 */
struct mbuf *
if_ubaget(struct ifubinfo *ifu, struct ifrw *ifr, struct ifnet *ifp, int len)
{
	struct uba_softc *uh = ifu->iff_softc;
	struct mbuf *m, *mn;

	if ((mn = getmcl()) == NULL)
		return NULL;	/* Leave the old */

	bus_dmamap_unload(uh->uh_dmat, ifr->ifrw_map);
	m = ifr->ifrw_mbuf;
	ifr->ifrw_mbuf = mn;
	if ((bus_dmamap_load(uh->uh_dmat, ifr->ifrw_map,
	    mn->m_ext.ext_buf, mn->m_ext.ext_size, NULL, BUS_DMA_NOWAIT)))
		panic("if_ubaget"); /* Cannot happen */
	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len;
	return m;
}

/*
 * Called after a packet is sent. Releases hold resources.
 */
void
if_ubaend(struct ifubinfo *ifu, struct ifxmt *ifw)
{
	struct uba_softc *uh = ifu->iff_softc;

	if (ifw->ifw_flags & IFRW_MBUF) {
		bus_dmamap_unload(uh->uh_dmat, ifw->ifw_map);
		m_freem(ifw->ifw_mbuf);
		ifw->ifw_mbuf = NULL;
	}
}

/*
 * Map a chain of mbufs onto a network interface
 * in preparation for an i/o operation.
 * The argument chain of mbufs includes the local network
 * header which is copied to be in the mapped, aligned
 * i/o space.
 */
int
if_ubaput(struct ifubinfo *ifu, struct ifxmt *ifw, struct mbuf *m)
{
	struct uba_softc *uh = ifu->iff_softc;
	int len;

	if (/* m->m_next ==*/ 0) {
		/*
		 * Map the outgoing packet directly.
		 */
		if ((ifw->ifw_flags & IFRW_MBUF) == 0) {
			bus_dmamap_unload(uh->uh_dmat, ifw->ifw_map);
			ifw->ifw_flags |= IFRW_MBUF;
		}
		bus_dmamap_load(uh->uh_dmat, ifw->ifw_map, mtod(m, void *),
		    m->m_len, NULL, BUS_DMA_NOWAIT);
		ifw->ifw_mbuf = m;
		len = m->m_len;
	} else {
		if (ifw->ifw_flags & IFRW_MBUF) {
			bus_dmamap_load(uh->uh_dmat, ifw->ifw_map,
			    ifw->ifw_vaddr, ifw->ifw_size,NULL,BUS_DMA_NOWAIT);
			ifw->ifw_flags &= ~IFRW_MBUF;
		}
		len = m->m_pkthdr.len;
		m_copydata(m, 0, m->m_pkthdr.len, ifw->ifw_vaddr);
		m_freem(m);
	}
	return len;
}
