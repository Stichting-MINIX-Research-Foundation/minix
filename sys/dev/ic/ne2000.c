/*	$NetBSD: ne2000.c,v 1.74 2013/08/11 12:34:16 rkujawa Exp $	*/

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
 * Common code shared by all NE2000-compatible Ethernet interfaces.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ne2000.c,v 1.74 2013/08/11 12:34:16 rkujawa Exp $");

#include "opt_ipkdb.h"

#include "rtl80x9.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#ifdef IPKDB_NE
#include <ipkdb/ipkdb.h>
#endif

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

#include <dev/ic/ax88190reg.h>

static int	ne2000_write_mbuf(struct dp8390_softc *, struct mbuf *, int);
static int	ne2000_ring_copy(struct dp8390_softc *, int, void *, u_short);
static void	ne2000_read_hdr(struct dp8390_softc *, int,
		    struct dp8390_ring *);
static int	ne2000_test_mem(struct dp8390_softc *);

static void	ne2000_writemem(bus_space_tag_t, bus_space_handle_t,
		    bus_space_tag_t, bus_space_handle_t, const uint8_t *, int,
		    size_t, int, int);
static void	ne2000_readmem(bus_space_tag_t, bus_space_handle_t,
		    bus_space_tag_t, bus_space_handle_t, int, uint8_t *,
		    size_t, int);

#ifdef NE2000_DETECT_8BIT
static bool	ne2000_detect_8bit(bus_space_tag_t, bus_space_handle_t,
		    bus_space_tag_t, bus_space_handle_t);
#endif

#define	ASIC_BARRIER(asict, asich) \
	bus_space_barrier((asict), (asich), 0, 0x10, \
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)

int
ne2000_attach(struct ne2000_softc *nsc, uint8_t *myea)
{
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	bus_space_tag_t nict = dsc->sc_regt;
	bus_space_handle_t nich = dsc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	uint8_t romdata[16];
	int memstart, memsize, i, useword;

	/*
	 * Detect it again unless caller specified it; this gives us
	 * the memory size.
	 */
	if (nsc->sc_type == NE2000_TYPE_UNKNOWN)
		nsc->sc_type = ne2000_detect(nict, nich, asict, asich);

	/*
	 * 8k of memory for NE1000, 16k for NE2000 and 24k for the
	 * card uses DL10019.
	 */
	switch (nsc->sc_type) {
	case NE2000_TYPE_UNKNOWN:
	default:
		aprint_error_dev(dsc->sc_dev, "where did the card go?\n");
		return 1;
	case NE2000_TYPE_NE1000:
		memstart = 8192;
		memsize = 8192;
		useword = 0;
		break;
	case NE2000_TYPE_NE2000:
	case NE2000_TYPE_AX88190:		/* XXX really? */
	case NE2000_TYPE_AX88790:
	case NE2000_TYPE_AX88796:
#if NRTL80X9 > 0
	case NE2000_TYPE_RTL8019:
#endif
		memstart = 16384;
		memsize = 16384;
		useword = 1;

		if (
#ifdef NE2000_DETECT_8BIT
		    ne2000_detect_8bit(nict, nich, asict, asich) ||
#endif
		    (nsc->sc_quirk & NE2000_QUIRK_8BIT) != 0) {
			/* in 8 bit mode, only 8KB memory can be used */
			memsize = 8192;
			useword = 0;
		}
		break;
	case NE2000_TYPE_DL10019:
	case NE2000_TYPE_DL10022:
		memstart = 8192 * 3;
		memsize = 8192 * 3;
		useword = 1;
		break;
	}

	nsc->sc_useword = useword;
#if NRTL80X9 > 0
	if (nsc->sc_type == NE2000_TYPE_RTL8019) {
		dsc->init_card = rtl80x9_init_card;
		dsc->sc_media_init = rtl80x9_media_init;
		dsc->sc_mediachange = rtl80x9_mediachange;
		dsc->sc_mediastatus = rtl80x9_mediastatus;
	}
#endif

	dsc->cr_proto = ED_CR_RD2;
	if (nsc->sc_type == NE2000_TYPE_AX88190 ||
	    nsc->sc_type == NE2000_TYPE_AX88790) {
		dsc->rcr_proto = ED_RCR_INTT;
		dsc->sc_flags |= DP8390_DO_AX88190_WORKAROUND;
	} else
		dsc->rcr_proto = 0;

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * NE1000 gets byte-wide DMA, NE2000 gets word-wide DMA.
	 */
	dsc->dcr_reg = ED_DCR_FT1 | ED_DCR_LS | (useword ? ED_DCR_WTS : 0);

	dsc->test_mem = ne2000_test_mem;
	dsc->ring_copy = ne2000_ring_copy;
	dsc->write_mbuf = ne2000_write_mbuf;
	dsc->read_hdr = ne2000_read_hdr;

	/* Registers are linear. */
	for (i = 0; i < 16; i++)
		dsc->sc_reg_map[i] = i;

	/*
	 * NIC memory doens't start at zero on an NE board.
	 * The start address is tied to the bus width.
	 */
#ifdef GWETHER
	{
		int x;
		int8_t pbuf0[ED_PAGE_SIZE], pbuf[ED_PAGE_SIZE],
		    tbuf[ED_PAGE_SIZE];

		memstart = 0;
		for (i = 0; i < ED_PAGE_SIZE; i++)
			pbuf0[i] = 0;

		/* Search for the start of RAM. */
		for (x = 1; x < 256; x++) {
			ne2000_writemem(nict, nich, asict, asich, pbuf0,
			    x << ED_PAGE_SHIFT, ED_PAGE_SIZE, useword, 0);
			ne2000_readmem(nict, nich, asict, asich,
			    x << ED_PAGE_SHIFT, tbuf, ED_PAGE_SIZE, useword);
			if (memcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ne2000_writemem(nict, nich, asict, asich,
				    pbuf, x << ED_PAGE_SHIFT, ED_PAGE_SIZE,
				    useword, 0);
				ne2000_readmem(nict, nich, asict, asich,
				    x << ED_PAGE_SHIFT, tbuf, ED_PAGE_SIZE,
				    useword);
				if (memcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0) {
					memstart = x << ED_PAGE_SHIFT;
					memsize = ED_PAGE_SIZE;
					break;
				}
			}
		}

		if (memstart == 0) {
			aprint_error_dev(dsc->sc_dev,
			    "cannot find start of RAM\n");
			return 1;
		}

		/* Search for the end of RAM. */
		for (++x; x < 256; x++) {
			ne2000_writemem(nict, nich, asict, asich, pbuf0,
			    x << ED_PAGE_SHIFT, ED_PAGE_SIZE, useword, 0);
			ne2000_readmem(nict, nich, asict, asich,
			    x << ED_PAGE_SHIFT, tbuf, ED_PAGE_SIZE, useword);
			if (memcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ne2000_writemem(nict, nich, asict, asich,
				    pbuf, x << ED_PAGE_SHIFT, ED_PAGE_SIZE,
				    useword, 0);
				ne2000_readmem(nict, nich, asict, asich,
				    x << ED_PAGE_SHIFT, tbuf, ED_PAGE_SIZE,
				    useword);
				if (memcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0)
					memsize += ED_PAGE_SIZE;
				else
					break;
			} else
				break;
		}

		printf("%s: RAM start 0x%x, size %d\n",
		    device_xname(dsc->sc_dev), memstart, memsize);
	}
#endif /* GWETHER */
	dsc->mem_start = memstart;

	dsc->mem_size = memsize;

	if (myea == NULL) {
		/* Read the station address. */
		if (nsc->sc_type == NE2000_TYPE_AX88190 ||
		    nsc->sc_type == NE2000_TYPE_AX88790 ||
		    nsc->sc_type == NE2000_TYPE_AX88796) {
			/* Select page 0 registers. */
			NIC_BARRIER(nict, nich);
			bus_space_write_1(nict, nich, ED_P0_CR,
			    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
			NIC_BARRIER(nict, nich);
			/* Select word transfer. */
			bus_space_write_1(nict, nich, ED_P0_DCR,
			    useword ? ED_DCR_WTS : 0);
			NIC_BARRIER(nict, nich);
			ne2000_readmem(nict, nich, asict, asich,
			    AX88190_NODEID_OFFSET, dsc->sc_enaddr,
			    ETHER_ADDR_LEN, useword);
		} else {
			bool ne1000 = (nsc->sc_type == NE2000_TYPE_NE1000);

			ne2000_readmem(nict, nich, asict, asich, 0, romdata,
			    sizeof(romdata), useword);
			for (i = 0; i < ETHER_ADDR_LEN; i++)
				dsc->sc_enaddr[i] =
				    romdata[i * (ne1000 ? 1 : 2)];
		}
	} else
		memcpy(dsc->sc_enaddr, myea, sizeof(dsc->sc_enaddr));

	/* Clear any pending interrupts that might have occurred above. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_ISR, 0xff);
	NIC_BARRIER(nict, nich);

	if (dsc->sc_media_init == NULL)
		dsc->sc_media_init = dp8390_media_init;

	if (dp8390_config(dsc)) {
		aprint_error_dev(dsc->sc_dev, "setup failed\n");
		return 1;
	}

	return 0;
}

/*
 * Detect an NE-2000 or compatible.  Returns a model code.
 */
int
ne2000_detect(bus_space_tag_t nict, bus_space_handle_t nich,
    bus_space_tag_t asict, bus_space_handle_t asich)
{
	const uint8_t test_pattern[32] = "THIS is A memory TEST pattern";
	uint8_t test_buffer[32], tmp;
	int i, rv = NE2000_TYPE_UNKNOWN;
	int useword;

	/* Reset the board. */
#ifdef GWETHER
	bus_space_write_1(asict, asich, NE2000_ASIC_RESET, 0);
	ASIC_BARRIER(asict, asich);
	delay(200);
#endif /* GWETHER */
	tmp = bus_space_read_1(asict, asich, NE2000_ASIC_RESET);
	ASIC_BARRIER(asict, asich);
	delay(10000);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that a outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we do
	 * the invasive thing for now.  Yuck.]
	 */
	bus_space_write_1(asict, asich, NE2000_ASIC_RESET, tmp);
	ASIC_BARRIER(asict, asich);
	delay(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up).
	 * XXX - this makes the probe invasive!  Done against my better
	 * judgement.  -DLG
	 */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(nict, nich);

	delay(5000);

	/*
	 * Generic probe routine for testing for the existence of a DS8390.
	 * Must be performed  after the NIC has just been reset.  This
	 * works by looking at certain register values that are guaranteed
	 * to be initialized a certain way after power-up or reset.
	 *
	 * Specifically:
	 *
	 *	Register		reset bits	set bits
	 *	--------		----------	--------
	 *	CR			TXP, STA	RD2, STP
	 *	ISR					RST
	 *	IMR			<all>
	 *	DCR					LAS
	 *	TCR			LB1, LB0
	 *
	 * We only look at CR and ISR, however, since looking at the others
	 * would require changing register pages, which would be intrusive
	 * if this isn't an 8390.
	 */

	tmp = bus_space_read_1(nict, nich, ED_P0_CR);
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
	    (ED_CR_RD2 | ED_CR_STP))
		goto out;

	tmp = bus_space_read_1(nict, nich, ED_P0_ISR);
	if ((tmp & ED_ISR_RST) != ED_ISR_RST)
		goto out;

	bus_space_write_1(nict, nich,
	    ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	for (i = 0; i < 100; i++) {
		if ((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RST) ==
		    ED_ISR_RST) {
			/* Ack the reset bit. */
			bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RST);
			NIC_BARRIER(nict, nich);
			break;
		}
		delay(100);
	}

#if 0
	/* XXX */
	if (i == 100)
		goto out;
#endif

	/*
	 * Test the ability to read and write to the NIC memory.  This has
	 * the side effect of determining if this is an NE1000 or an NE2000.
	 */

	/*
	 * This prevents packets from being stored in the NIC memory when
	 * the readmem routine turns on the start bit in the CR.
	 */
	bus_space_write_1(nict, nich, ED_P0_RCR, ED_RCR_MON);
	NIC_BARRIER(nict, nich);

	/* Temporarily initialize DCR for byte operations. */
	bus_space_write_1(nict, nich, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	bus_space_write_1(nict, nich, ED_P0_PSTART, 8192 >> ED_PAGE_SHIFT);
	bus_space_write_1(nict, nich, ED_P0_PSTOP, 16384 >> ED_PAGE_SHIFT);

	/*
	 * Write a test pattern in byte mode.  If this fails, then there
	 * probably isn't any memory at 8k - which likely means that the
	 * board is an NE2000.
	 */
	ne2000_writemem(nict, nich, asict, asich, test_pattern, 8192,
	    sizeof(test_pattern), 0, 1);
	ne2000_readmem(nict, nich, asict, asich, 8192, test_buffer,
	    sizeof(test_buffer), 0);

	if (memcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
		/* We're an NE1000. */
		rv = NE2000_TYPE_NE1000;
		goto out;
	}

	/* not an NE1000 - try NE2000 */

	/* try 16 bit mode first */
	useword = 1;

#ifdef NE2000_DETECT_8BIT
	/*
	 * Check bus type in EEPROM first because some NE2000 compatible wedges
	 * on 16 bit DMA access if the chip is configured in 8 bit mode.
	 */
	if (ne2000_detect_8bit(nict, nich, asict, asich))
		useword = 0;
#endif
 again:
	bus_space_write_1(nict, nich, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS |
	    (useword ? ED_DCR_WTS : 0));
	bus_space_write_1(nict, nich, ED_P0_PSTART, 16384 >> ED_PAGE_SHIFT);
	bus_space_write_1(nict, nich, ED_P0_PSTOP,
	    (16384 + (useword ? 16384 : 8192)) >> ED_PAGE_SHIFT);

	/*
	 * Write the test pattern in word mode.  If this also fails,
	 * then we don't know what this board is.
	 */
	ne2000_writemem(nict, nich, asict, asich, test_pattern, 16384,
	    sizeof(test_pattern), useword, 1);
	ne2000_readmem(nict, nich, asict, asich, 16384, test_buffer,
	    sizeof(test_buffer), useword);

	if (memcmp(test_pattern, test_buffer, sizeof(test_pattern)) != 0) {
		if (useword == 1) {
			/* try 8 bit mode */
			useword = 0;
			goto again;
		}
		return NE2000_TYPE_UNKNOWN;	/* not an NE2000 either */
	}

	rv = NE2000_TYPE_NE2000;

#if NRTL80X9 > 0
	/* Check for a Realtek RTL8019. */
	if (bus_space_read_1(nict, nich, NERTL_RTL0_8019ID0) == RTL0_8019ID0 &&
	    bus_space_read_1(nict, nich, NERTL_RTL0_8019ID1) == RTL0_8019ID1)
		rv = NE2000_TYPE_RTL8019;
#endif

 out:
	/* Clear any pending interrupts that might have occurred above. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_ISR, 0xff);

	return rv;
}

#ifdef NE2000_DETECT_8BIT
static bool
ne2000_detect_8bit(bus_space_tag_t nict, bus_space_handle_t nich,
    bus_space_tag_t asict, bus_space_handle_t asich)
{
	bool is8bit;
	uint8_t romdata[32];

	is8bit = false;

	/* Set DCR for 8 bit DMA. */
	bus_space_write_1(nict, nich, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	/* Read PROM area. */
	ne2000_readmem(nict, nich, asict, asich, 0, romdata,
	    sizeof(romdata), 0);
	if (romdata[28] == 'B' && romdata[30] == 'B') {
		/* 'B' (0x42) in 8 bit mode, 'W' (0x57) in 16 bit mode */
		is8bit = true;
	} 
	if (!is8bit) {
		/* not in 8 bit mode; put back DCR setting for 16 bit DMA */
		bus_space_write_1(nict, nich, ED_P0_DCR,
		    ED_DCR_FT1 | ED_DCR_LS | ED_DCR_WTS);
	}

	return is8bit;
}
#endif

/*
 * Write an mbuf chain to the destination NIC memory address using programmed
 * I/O.
 */
int
ne2000_write_mbuf(struct dp8390_softc *sc, struct mbuf *m, int buf)
{
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	int savelen, padlen;
	int maxwait = 100;	/* about 120us */

	savelen = m->m_pkthdr.len;
	if (savelen < ETHER_MIN_LEN - ETHER_CRC_LEN) {
		padlen = ETHER_MIN_LEN - ETHER_CRC_LEN - savelen;
		savelen = ETHER_MIN_LEN - ETHER_CRC_LEN;
	} else
		padlen = 0;


	/* Select page 0 registers. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	/* Reset remote DMA complete flag. */
	bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RDC);
	NIC_BARRIER(nict, nich);

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, savelen);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, savelen >> 8);

	/* Set up destination address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, buf);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, buf >> 8);

	/* Set remote DMA write. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich,
	    ED_P0_CR, ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	/*
	 * Transfer the mbuf chain to the NIC memory.  NE2000 cards
	 * require that data be transferred as words, and only words,
	 * so that case requires some extra code to patch over odd-length
	 * mbufs.
	 */
	if (nsc->sc_useword == 0) {
		/* byte ops are easy. */
		for (; m != NULL; m = m->m_next) {
			if (m->m_len) {
				bus_space_write_multi_1(asict, asich,
				    NE2000_ASIC_DATA, mtod(m, uint8_t *),
				    m->m_len);
			}
		}
		if (padlen) {
			for(; padlen > 0; padlen--)
				bus_space_write_1(asict, asich,
				    NE2000_ASIC_DATA, 0);
		}
	} else {
		/* word ops are a bit trickier. */
		uint8_t *data, savebyte[2];
		int l, leftover;
#ifdef DIAGNOSTIC
		uint8_t *lim;
#endif
		/* Start out with no leftover data. */
		leftover = 0;
		savebyte[0] = savebyte[1] = 0;

		for (; m != NULL; m = m->m_next) {
			l = m->m_len;
			if (l == 0)
				continue;
			data = mtod(m, uint8_t *);
#ifdef DIAGNOSTIC
			lim = data + l;
#endif
			while (l > 0) {
				if (leftover) {
					/*
					 * Data left over (from mbuf or
					 * realignment).  Buffer the next
					 * byte, and write it and the
					 * leftover data out.
					 */
					savebyte[1] = *data++;
					l--;
					bus_space_write_stream_2(asict, asich,
					    NE2000_ASIC_DATA,
					    *(uint16_t *)savebyte);
					leftover = 0;
				} else if (BUS_SPACE_ALIGNED_POINTER(data,
					   uint16_t) == 0) {
					/*
					 * Unaligned data; buffer the next
					 * byte.
					 */
					savebyte[0] = *data++;
					l--;
					leftover = 1;
				} else {
					/*
					 * Aligned data; output contiguous
					 * words as much as we can, then
					 * buffer the remaining byte, if any.
					 */
					leftover = l & 1;
					l &= ~1;
					bus_space_write_multi_stream_2(asict,
					    asich, NE2000_ASIC_DATA,
					    (uint16_t *)data, l >> 1);
					data += l;
					if (leftover)
						savebyte[0] = *data++;
					l = 0;
				}
			}
			if (l < 0)
				panic("ne2000_write_mbuf: negative len");
#ifdef DIAGNOSTIC
			if (data != lim)
				panic("ne2000_write_mbuf: data != lim");
#endif
		}
		if (leftover) {
			savebyte[1] = 0;
			bus_space_write_stream_2(asict, asich, NE2000_ASIC_DATA,
			    *(uint16_t *)savebyte);
		}
		if (padlen) {
			for(; padlen > 1; padlen -= 2)
				bus_space_write_stream_2(asict, asich,
				    NE2000_ASIC_DATA, 0);
		}
	}
	NIC_BARRIER(nict, nich);

	/* some AX88796 doesn't seem to have remote DMA complete */
	if (sc->sc_flags & DP8390_NO_REMOTE_DMA_COMPLETE)
		return savelen;

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait) {
		(void)bus_space_read_1(nict, nich, ED_P0_CRDA1);
		(void)bus_space_read_1(nict, nich, ED_P0_CRDA0);
		NIC_BARRIER(nict, nich);
		DELAY(1);
	}

	if (maxwait == 0) {
		log(LOG_WARNING,
		    "%s: remote transmit DMA failed to complete\n",
		    device_xname(sc->sc_dev));
		dp8390_reset(sc);
	}

	return savelen;
}

/*
 * Given a source and destination address, copy 'amout' of a packet from
 * the ring buffer into a linear destination buffer.  Takes into account
 * ring-wrap.
 */
int
ne2000_ring_copy(struct dp8390_softc *sc, int src, void *dstv, u_short amount)
{
	char *dst = dstv;
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;
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
		ne2000_readmem(nict, nich, asict, asich, src,
		    (uint8_t *)dst, tmp_amount, useword);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	ne2000_readmem(nict, nich, asict, asich, src, (uint8_t *)dst,
	    amount, useword);

	return src + amount;
}

void
ne2000_read_hdr(struct dp8390_softc *sc, int buf, struct dp8390_ring *hdr)
{
	struct ne2000_softc *nsc = (struct ne2000_softc *)sc;

	ne2000_readmem(sc->sc_regt, sc->sc_regh, nsc->sc_asict, nsc->sc_asich,
	    buf, (uint8_t *)hdr, sizeof(struct dp8390_ring),
	    nsc->sc_useword);
#if BYTE_ORDER == BIG_ENDIAN
	hdr->count = bswap16(hdr->count);
#endif
}

int
ne2000_test_mem(struct dp8390_softc *sc)
{

	/* Noop. */
	return 0;
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'amount' from NIC to host using programmed i/o.  The 'amount' is
 * rounded up to a word - ok as long as mbufs are word sized.
 */
void
ne2000_readmem(bus_space_tag_t nict, bus_space_handle_t nich,
    bus_space_tag_t asict, bus_space_handle_t asich,
    int src, uint8_t *dst, size_t amount, int useword)
{

	/* Select page 0 registers. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	/* Round up to a word. */
	amount = roundup2(amount, sizeof(uint16_t));

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, amount);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, amount >> 8);

	/* Set up source address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, src);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, src >> 8);

	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

	ASIC_BARRIER(asict, asich);
	if (useword)
		bus_space_read_multi_stream_2(asict, asich, NE2000_ASIC_DATA,
		    (uint16_t *)dst, amount >> 1);
	else
		bus_space_read_multi_1(asict, asich, NE2000_ASIC_DATA,
		    dst, amount);
}

/*
 * Stripped down routine for writing a linear buffer to NIC memory.  Only
 * used in the probe routine to test the memory.  'len' must be even.
 */
void
ne2000_writemem(bus_space_tag_t nict, bus_space_handle_t nich,
    bus_space_tag_t asict, bus_space_handle_t asich,
    const uint8_t *src, int dst, size_t len, int useword, int quiet)
{
	int maxwait = 100;	/* about 120us */

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
	bus_space_write_1(nict, nich, ED_P0_RSAR0, dst);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, dst >> 8);

	/* Set remote DMA write. */
	NIC_BARRIER(nict, nich);
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(nict, nich);

	ASIC_BARRIER(asict, asich);
	if (useword)
		bus_space_write_multi_stream_2(asict, asich, NE2000_ASIC_DATA,
		    (const uint16_t *)src, len >> 1);
	else
		bus_space_write_multi_1(asict, asich, NE2000_ASIC_DATA,
		    src, len);
	ASIC_BARRIER(asict, asich);

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait)
		DELAY(1);

	if (!quiet && maxwait == 0)
		printf("ne2000_writemem: failed to complete\n");
}

int
ne2000_detach(struct ne2000_softc *sc, int flags)
{

	return dp8390_detach(&sc->sc_dp8390, flags);
}

#ifdef IPKDB_NE
/*
 * This code is essentially the same as ne2000_attach above.
 */
int
ne2000_ipkdb_attach(struct ipkdb_if *kip)
{
	struct ne2000_softc *np = kip->port;
	struct dp8390_softc *dp = &np->sc_dp8390;
	bus_space_tag_t nict = dp->sc_regt;
	bus_space_handle_t nich = dp->sc_regh;
	bus_space_tag_t asict = np->sc_asict;
	bus_space_handle_t asich = np->sc_asich;
	int i, useword;

#ifdef GWETHER
	/* Not supported (yet?) */
	return -1;
#endif

	if (np->sc_type == NE2000_TYPE_UNKNOWN)
		np->sc_type = ne2000_detect(nict, nich, asict, asich);
	if (np->sc_type == NE2000_TYPE_UNKNOWN)
		return -1;

	switch (np->sc_type) {
	case NE2000_TYPE_NE1000:
		dp->mem_start = 8192;
		dp->mem_size = 8192;
		useword = 0;
		kip->name = "ne1000";
		break;
	case NE2000_TYPE_NE2000:
	case NE2000_TYPE_AX88190:
	case NE2000_TYPE_AX88790:
	case NE2000_TYPE_AX88796:
#if NRTL80X9 > 0
	case NE2000_TYPE_RTL8019:
#endif
		dp->mem_start = 16384;
		dp->mem_size = 16384;
		useword = 1;
		if (
#ifdef NE2000_DETECT_8BIT
		    ne2000_detect_8bit(nict, nich, asict, asich) ||
#endif
		    (np->sc_quirk & NE2000_QUIRK_8BIT) != 0) {
			/* in 8 bit mode, only 8KB memory can be used */
			dp->mem_size = 8192;
			useword = 0;
		}
		kip->name =
		    (np->sc_type == NE2000_TYPE_AX88190 ||
		     np->sc_type == NE2000_TYPE_AX88790) ?
		    "ax88190" : "ne2000";
		break;
	case NE2000_TYPE_DL10019:
	case NE2000_TYPE_DL10022:
		dp->mem_start = 8192 * 3;
		dp->mem_size = 8192 * 3;
		useword = 1;
		kip->name = (np->sc_type == NE2000_TYPE_DL10019) ?
		    "dl10022" : "dl10019";
		break;
	default:
		return -1;
		break;
	}

	np->sc_useword = useword;
#if NRTL80X9 > 0
	if (np->sc_type == NE2000_TYPE_RTL8019) {
		dp->init_card = rtl80x9_init_card;
		dp->sc_media_init = rtl80x9_media_init;
		dp->sc_mediachange = rtl80x9_mediachange;
		dp->sc_mediastatus = rtl80x9_mediastatus;
	}
#endif

	dp->cr_proto = ED_CR_RD2;
	if (np->sc_type == NE2000_TYPE_AX88190 ||
	    np->sc_type == NE2000_TYPE_AX88790) {
		dp->rcr_proto = ED_RCR_INTT;
		dp->sc_flags |= DP8390_DO_AX88190_WORKAROUND;
	} else
		dp->rcr_proto = 0;
	dp->dcr_reg = ED_DCR_FT1 | ED_DCR_LS | (useword ? ED_DCR_WTS : 0);

	dp->test_mem = ne2000_test_mem;
	dp->ring_copy = ne2000_ring_copy;
	dp->write_mbuf = ne2000_write_mbuf;
	dp->read_hdr = ne2000_read_hdr;

	for (i = 0; i < 16; i++)
		dp->sc_reg_map[i] = i;

	if (dp8390_ipkdb_attach(kip))
		return -1;

	if (!(kip->flags & IPKDB_MYHW)) {
		char romdata[16];

		/* Read the station address. */
		if (np->sc_type == NE2000_TYPE_AX88190 ||
		    np->sc_type == NE2000_TYPE_AX88790 ||
		    np->sc_type == NE2000_TYPE_AX88796) {
			/* Select page 0 registers. */
			NIC_BARRIER(nict, nich);
			bus_space_write_1(nict, nich, ED_P0_CR,
				ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
			NIC_BARRIER(nict, nich);
			/* Select word transfer */
			bus_space_write_1(nict, nich, ED_P0_DCR,
			    useword ? ED_DCR_WTS : 0);
			ne2000_readmem(nict, nich, asict, asich,
				AX88190_NODEID_OFFSET, kip->myenetaddr,
				ETHER_ADDR_LEN, useword);
		} else {
			bool ne1000 = (np->sc_type == NE2000_TYPE_NE1000);

			ne2000_readmem(nict, nich, asict, asich,
				0, romdata, sizeof romdata, useword);
			for (i = 0; i < ETHER_ADDR_LEN; i++)
				kip->myenetaddr[i] =
				    romdata[i * (ne1000 ? 1 : 2)];
		}
		kip->flags |= IPKDB_MYHW;

	}
	dp8390_stop(dp);

	return 0;
}
#endif

bool
ne2000_suspend(device_t self, const pmf_qual_t *qual)
{
	struct ne2000_softc *sc = device_private(self);
	struct dp8390_softc *dsc = &sc->sc_dp8390;
	int s;

	s = splnet();

	dp8390_stop(dsc);
	dp8390_disable(dsc);

	splx(s);
	return true;
}

bool
ne2000_resume(device_t self, const pmf_qual_t *qual)
{
	struct ne2000_softc *sc = device_private(self);
	struct dp8390_softc *dsc = &sc->sc_dp8390;
	struct ifnet *ifp = &dsc->sc_ec.ec_if;
	int s;

	s = splnet();

	if (ifp->if_flags & IFF_UP) {
		if (dp8390_enable(dsc) == 0)
			dp8390_init(dsc);
	}

	splx(s);
	return true;
}
