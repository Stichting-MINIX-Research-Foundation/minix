/* $NetBSD: ne.c,v 1.7 2008/12/14 18:46:33 christos Exp $ */

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
 * this code is mainly obtained from /sys/dev/ic/ne2000.c .
 */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>
#include <libi386.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#include <bootinfo.h>
#endif

#include "etherdrv.h"
#include <dev/ic/dp8390reg.h>
#include <dev/ic/ne2000reg.h>
#include "dp8390.h"
#include "ne.h"

#ifndef BASEREG
#define BASEREG 0x300
#endif

#define	NE_BASEREG BASEREG
#define NE_ASIC_BASEREG (NE_BASEREG+NE2000_ASIC_OFFSET)

#define NIC_PORT(x) (NE_BASEREG + (x))
#define NIC_INB(x) inb(NIC_PORT(x))
#define NIC_OUTB(x, b) outb(NIC_PORT(x), (b))

#define NE_16BIT

#define DELAY(x) delay(x)

#define ASIC_PORT(x) (NE_ASIC_BASEREG + (x))
#define ASIC_INB(x) inb(ASIC_PORT(x))
#define ASIC_INW(x) inw(ASIC_PORT(x))
#define ASIC_OUTB(x, b) outb(ASIC_PORT(x), (b))
#define ASIC_OUTW(x, b) outw(ASIC_PORT(x), (b))

u_char eth_myaddr[6];

#ifdef _STANDALONE
static struct btinfo_netif bi_netif;
#endif

int
EtherInit(unsigned char *myadr)
{
	uint8_t tmp;
	int i;

	printf("ne: trying iobase=0x%x\n", NE_BASEREG);

	dp8390_iobase = NE_BASEREG;
	dp8390_membase = dp8390_memsize = 8192*2;
	dp8390_cr_proto = ED_CR_RD2;
	dp8390_dcr_reg = ED_DCR_FT1 | ED_DCR_LS
#ifdef NE_16BIT
	| ED_DCR_WTS
#endif
	;

	/* reset */
	tmp = ASIC_INB(NE2000_ASIC_RESET);
	DELAY(10000);
	ASIC_OUTB(NE2000_ASIC_RESET, tmp);
	DELAY(5000);

	NIC_OUTB(ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
	DELAY(5000);

	tmp = NIC_INB(ED_P0_CR);
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
		(ED_CR_RD2 | ED_CR_STP)) {
		goto out;
	}

	tmp = NIC_INB(ED_P0_ISR);
	if ((tmp & ED_ISR_RST) != ED_ISR_RST) {
		goto out;
	}

	NIC_OUTB(ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	for (i = 0; i < 100; i++) {
		if ((NIC_INB(ED_P0_ISR) & ED_ISR_RST) ==
		    ED_ISR_RST) {
			/* Ack the reset bit. */
			NIC_OUTB(ED_P0_ISR, ED_ISR_RST);
			break;
		}
		DELAY(100);
	}

	printf("ne: found\n");

	/*
	 * This prevents packets from being stored in the NIC memory when
	 * the readmem routine turns on the start bit in the CR.
	 */
	NIC_OUTB(ED_P0_RCR, ED_RCR_MON);

	/* Temporarily initialize DCR for byte operations. */
	NIC_OUTB(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	NIC_OUTB(ED_P0_PSTART, 8192 >> ED_PAGE_SHIFT);
	NIC_OUTB(ED_P0_PSTOP, 16384 >> ED_PAGE_SHIFT);

#ifdef HWADDR
	for (i = 0; i < 6; i++)
		myadr[i] = eth_myaddr[i] = HWADDR[i];
#else
{
	uint8_t romdata[16];

	ne2000_readmem(0, romdata, 16);
	for (i = 0; i < 6; i++)
		myadr[i] = eth_myaddr[i] = romdata[i*2];
}
#endif

	if (dp8390_config())
		goto out;

#ifdef _STANDALONE
	strncpy(bi_netif.ifname, "ne", sizeof(bi_netif.ifname));
	bi_netif.bus = BI_BUS_ISA;
	bi_netif.addr.iobase = NE_BASEREG;

	BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));
#endif
	return 1;
out:
	return 0;
}

void
EtherStop(void) {
	uint8_t tmp;

	dp8390_stop();

	tmp = ASIC_INB(NE2000_ASIC_RESET);
	DELAY(10000);
	ASIC_OUTB(NE2000_ASIC_RESET, tmp);
	DELAY(5000);

	NIC_OUTB(ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
	DELAY(5000);
}

void
ne2000_writemem(uint8_t *src, int dst, size_t len)
{
	size_t i;
	int maxwait = 100;	/* about 120us */

	/* Select page 0 registers. */
	NIC_OUTB(ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	/* Reset remote DMA complete flag. */
	NIC_OUTB(ED_P0_ISR, ED_ISR_RDC);

	/* Set up DMA byte count. */
	NIC_OUTB(ED_P0_RBCR0, len);
	NIC_OUTB(ED_P0_RBCR1, len >> 8);

	/* Set up destination address in NIC mem. */
	NIC_OUTB(ED_P0_RSAR0, dst);
	NIC_OUTB(ED_P0_RSAR1, dst >> 8);

	/* Set remote DMA write. */
	NIC_OUTB(ED_P0_CR, ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

#ifdef NE_16BIT
	for (i = 0; i < len; i += 2, src += 2)
		ASIC_OUTW(NE2000_ASIC_DATA, *(uint16_t *)src);
#else
	for (i = 0; i < len; i++)
		ASIC_OUTB(NE2000_ASIC_DATA, *src++);
#endif

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((NIC_INB(ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait)
		DELAY(1);

	if (maxwait == 0)
		printf("ne2000_writemem: failed to complete\n");
}

void
ne2000_readmem(int src, uint8_t *dst, size_t amount)
{
	size_t i;

	/* Select page 0 registers. */
	NIC_OUTB(ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	/* Round up to a word. */
	if (amount & 1)
		++amount;

	/* Set up DMA byte count. */
	NIC_OUTB(ED_P0_RBCR0, amount);
	NIC_OUTB(ED_P0_RBCR1, amount >> 8);

	/* Set up source address in NIC mem. */
	NIC_OUTB(ED_P0_RSAR0, src);
	NIC_OUTB(ED_P0_RSAR1, src >> 8);

	NIC_OUTB(ED_P0_CR, ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

#ifdef NE_16BIT
	for (i = 0; i < amount; i += 2, dst += 2)
		*(uint16_t *)dst = ASIC_INW(NE2000_ASIC_DATA);
#else
	for (i = 0; i < amount; i++)
		*dst++ = ASIC_INB(NE2000_ASIC_DATA);
#endif
}
