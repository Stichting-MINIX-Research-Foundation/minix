/*	$NetBSD: mx98905.c,v 1.15 2009/03/14 21:04:20 dsl Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * Special routines for the Macronix MX 98905.  For use with the "ne" driver.
 */

/*
 * <URL:http://mail-index.NetBSD.org/port-arm32/1996/06/23/0005.html>:
 * There are 2 types of etherh card.  One uses the macronics chipset MX98905
 * and that chipset has a bug in it, in that it the MSB remote DMA
 * register does not work.  There is a workaround for this which
 * should be around soon.  In fact, I think only the buffer ram test
 * ever transfers more than 256 bytes across the DMA channel, so diabling
 * it will make the mx stuff work.
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: mx98905.c,v 1.15 2009/03/14 21:04:20 dsl Exp $");

#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>
#include <dev/ic/mx98905var.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

static inline void mx98905_write_setup(struct dp8390_softc *, int, int);
static inline void mx98905_write_wait(struct dp8390_softc *);

void
mx98905_attach(struct dp8390_softc *sc)
{

	sc->ring_copy = mx98905_ring_copy;
	sc->write_mbuf = mx98905_write_mbuf;
	sc->read_hdr = mx98905_read_hdr;
}

static inline void
mx98905_write_setup(struct dp8390_softc *sc, int len, int buf)
{
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;

	/* Select page 0 registers. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	/* Reset remote DMA complete flag. */
	bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RDC);
	NIC_BARRIER(nict, nich);

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, len);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, len >> 8);

	/* Set up destination address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, buf);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, buf >> 8);

	/* Set remote DMA write. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich,
	    ED_P0_CR, ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);
}


static inline void
mx98905_write_wait(struct dp8390_softc *sc)
{
	int maxwait = 100;	/* about 120us */
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait) {
		bus_space_read_1(nict, nich, ED_P0_CRDA1);
		bus_space_read_1(nict, nich, ED_P0_CRDA0);
		NIC_BARRIER(nict, nich);
		DELAY(1);
	}

	if (maxwait == 0) {
		log(LOG_WARNING,
		    "%s: remote transmit DMA failed to complete\n",
		    device_xname(sc->sc_dev));
		dp8390_reset(sc);
	}
}

/*
 * Write an mbuf chain to the destination NIC memory address using programmed
 * I/O.
 */
int
mx98905_write_mbuf(struct dp8390_softc *sc, struct mbuf *m, int buf)
{
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	int savelen, dmalen, resid, len;
	u_int8_t *data, savebyte[2];
	int l, leftover;
#ifdef DIAGNOSTIC
	u_int8_t *lim;
#endif
	int i;

	resid = savelen = m->m_pkthdr.len;

	dmalen = min(resid, 254);

	mx98905_write_setup(sc, dmalen, buf);

	buf += dmalen;
	resid -= dmalen;

	/*
	 * Transfer the mbuf chain to the NIC memory.  NE2000 cards
	 * require that data be transferred as words, and only words,
	 * so that case requires some extra code to patch over odd-length
	 * mbufs.
	 */
	/* NE2000s are a bit trickier. */
	/* Start out with no leftover data. */
	leftover = 0;
	savebyte[0] = savebyte[1] = 0;

	for (; m != 0; m = m->m_next) {
		l = m->m_len;
		if (l == 0)
			continue;
		data = mtod(m, u_int8_t *);
#ifdef DIAGNOSTIC
		lim = data + l;
#endif
		while (l > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or
				 * realignment).  Buffer the next
				 * byte, and write it and the leftover
				 * data out.
				 */
				savebyte[1] = *data++;
				l--;
				bus_space_write_stream_2(asict, asich,
				    NE2000_ASIC_DATA, *(u_int16_t *)savebyte);
				dmalen -= 2;
				leftover = 0;
			} else if (BUS_SPACE_ALIGNED_POINTER(data,
			    u_int16_t) == 0) {
				/* Unaligned data; buffer the next byte. */
				savebyte[0] = *data++;
				l--;
				leftover = 1;
			} else {
				/*
				 * Aligned data; output contiguous
				 * words as much as we can, then
				 * buffer the remaining byte, if any.
				 */
				len = min(l, dmalen);
				leftover = len & 1;
				len &= ~1;
				bus_space_write_multi_stream_2(asict,
				    asich, NE2000_ASIC_DATA,
				    (u_int16_t *)data, len >> 1);
				dmalen -= len;
				data += len;
				if (leftover)
					savebyte[0] = *data++;
				l -= len + leftover;
			}
			if (dmalen == 0 && resid > 0) {
				mx98905_write_wait(sc);
				dmalen = min(resid, 254);

				mx98905_write_setup(sc, dmalen, buf);

				buf += dmalen;
				resid -= dmalen;
			}
		}
		if (l < 0)
			panic("mx98905_write_mbuf: negative len");
#ifdef DIAGNOSTIC
		if (data != lim)
			panic("mx98905_write_mbuf: data != lim");
#endif
	}
	if (leftover) {
		savebyte[1] = 0;
		bus_space_write_stream_2(asict, asich, NE2000_ASIC_DATA,
		    *(u_int16_t *)savebyte);
	}
	if (savelen < ETHER_MIN_LEN - ETHER_CRC_LEN) {
		for(i = 0; i < (ETHER_MIN_LEN - ETHER_CRC_LEN - savelen) >> 1;
		    i++)
			bus_space_write_stream_2(asict, asich,
			    NE2000_ASIC_DATA, 0);
		savelen = ETHER_MIN_LEN - ETHER_CRC_LEN;
	}
	NIC_BARRIER(nict, nich);

	mx98905_write_wait(sc);

	return (savelen);
}

/*
 * Given a source and destination address, copy 'amout' of a packet from
 * the ring buffer into a linear destination buffer.  Takes into account
 * ring-wrap.
 */
int
mx98905_ring_copy(struct dp8390_softc *sc, int src, void *vdst, u_short amount)
{
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;
	uint8_t *dst = vdst;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	u_short tmp_amount;
	int useword = nsc->sc_useword;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		mx98905_readmem(nict, nich, asict, asich, src,
		    (u_int8_t *)dst, tmp_amount, useword);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	mx98905_readmem(nict, nich, asict, asich, src, dst, amount, useword);

	return (src + amount);
}

void
mx98905_read_hdr(struct dp8390_softc *sc, int buf, struct dp8390_ring *hdr)
{
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;

	mx98905_readmem(sc->sc_regt, sc->sc_regh, nsc->sc_asict, nsc->sc_asich,
	    buf, (u_int8_t *)hdr, sizeof(struct dp8390_ring),
	    nsc->sc_useword);
#if BYTE_ORDER == BIG_ENDIAN
	hdr->count = bswap16(hdr->count);
#endif
}

static inline void
mx98905_read_setup(bus_space_tag_t nict, bus_space_handle_t nich,
    int len, int buf)
{

	/* Select page 0 registers. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, len);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, len >> 8);

	/* Set up source address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, buf);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, buf >> 8);

	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

	bus_space_barrier(nict, nich, 0, NE2000_NPORTS,
			  BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'amount' from NIC to host using programmed i/o.  The 'amount' is
 * rounded up to a word - ok as long as mbufs are word sized.
 */
void
mx98905_readmem(bus_space_tag_t nict, bus_space_handle_t nich, bus_space_tag_t asict, bus_space_handle_t asich, int src, u_int8_t *dst, size_t amount, int useword)
{
	int len, resid;

	resid = amount;
	/* Round up to a word. */
	if (resid & 1)
		++resid;

	while (resid > 0) {
		len = min(resid, 254);
		mx98905_read_setup(nict, nich, len, src);
		if (useword)
			bus_space_read_multi_stream_2(asict, asich,
			    NE2000_ASIC_DATA, (u_int16_t *)dst, len >> 1);
		else
			bus_space_read_multi_1(asict, asich, NE2000_ASIC_DATA,
			    dst, len);
		resid -= len;
		src += len;
		dst += len;
	}
}
