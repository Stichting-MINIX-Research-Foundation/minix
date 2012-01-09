/*	$NetBSD: wd80x3.c,v 1.10 2008/12/14 18:46:33 christos Exp $	*/

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
 * Device driver for the Western Digital/SMC 8003 and 8013 series,
 * and the SMC Elite Ultra (8216).
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
#include "dp8390.h"
#include <dev/ic/wereg.h>

#ifndef BASEREG
#define BASEREG 0x240
#define BASEMEM 0xd0000
#endif

#define	WD_BASEREG BASEREG
#define	WD_BASEMEM BASEMEM

#ifndef _STANDALONE
extern int mapio(void);
#endif

u_char eth_myaddr[6];

static uint8_t we_type;
static int we_is16bit;

#ifdef _STANDALONE
static struct btinfo_netif bi_netif;
#endif

const char *
we_params(void)
{
	const char *typestr;

	dp8390_memsize = 8192;

	we_type = inb(WD_BASEREG + WE_CARD_ID);
	switch (we_type) {
#ifdef SUPPORT_WD80X3
	case WE_TYPE_WD8003S:
		typestr = "WD8003S";
		break;
	case WE_TYPE_WD8003E:
		typestr = "WD8003E";
		break;
	case WE_TYPE_WD8003EB:
		typestr = "WD8003EB";
		break;
	case WE_TYPE_WD8003W:
		typestr = "WD8003W";
		break;
	case WE_TYPE_WD8013EBT:
		typestr = "WD8013EBT";
		dp8390_memsize = 16384;
		we_is16bit = 1;
		break;
	case WE_TYPE_WD8013W:
		typestr = "WD8013W";
		dp8390_memsize = 16384;
		we_is16bit = 1;
		break;
	case WE_TYPE_WD8013EP:		/* also WD8003EP */
		if (inb(WD_BASEREG + WE_ICR) & WE_ICR_16BIT) {
			we_is16bit = 1;
			dp8390_memsize = 16384;
			typestr = "WD8013EP";
		} else
			typestr = "WD8003EP";
		break;
	case WE_TYPE_WD8013WC:
		typestr = "WD8013WC";
		dp8390_memsize = 16384;
		we_is16bit = 1;
		break;
	case WE_TYPE_WD8013EBP:
		typestr = "WD8013EBP";
		dp8390_memsize = 16384;
		we_is16bit = 1;
		break;
	case WE_TYPE_WD8013EPC:
		typestr = "WD8013EPC";
		dp8390_memsize = 16384;
		we_is16bit = 1;
		break;
#endif
#ifdef SUPPORT_SMC_ULTRA
	case WE_TYPE_SMC8216C:
	case WE_TYPE_SMC8216T:
	    {
		uint8_t hwr;

		typestr = (we_type == WE_TYPE_SMC8216C) ?
		    "SMC8216/SMC8216C" : "SMC8216T";

		hwr = inb(WD_BASEREG + WE790_HWR);
		outb(WD_BASEREG + WE790_HWR, hwr | WE790_HWR_SWH);
		switch (inb(WD_BASEREG + WE790_RAR) & WE790_RAR_SZ64) {
		case WE790_RAR_SZ64:
			dp8390_memsize = 65536;
			break;
		case WE790_RAR_SZ32:
			dp8390_memsize = 32768;
			break;
		case WE790_RAR_SZ16:
			dp8390_memsize = 16384;
			break;
		case WE790_RAR_SZ8:
			/* 8216 has 16K shared mem -- 8416 has 8K */
			typestr = (we_type == WE_TYPE_SMC8216C) ?
			    "SMC8416C/SMC8416BT" : "SMC8416T";
			dp8390_memsize = 8192;
			break;
		}
		outb(WD_BASEREG + WE790_HWR, hwr);

		we_is16bit = 1;
#ifdef SUPPORT_WD80X3
		dp8390_is790 = 1;
#endif
		break;
	    }
#endif
	default:
		/* Not one we recognize. */
		return NULL;
	}

	/*
	 * Make some adjustments to initial values depending on what is
	 * found in the ICR.
	 */
	if (we_is16bit && (we_type != WE_TYPE_WD8013EBT) &&
	    (inb(WD_BASEREG + WE_ICR) & WE_ICR_16BIT) == 0) {
		we_is16bit = 0;
		dp8390_memsize = 8192;
	}

#ifdef WE_DEBUG
	{
		int i;

		printf("we_params: type = 0x%x, typestr = %s, is16bit = %d, "
		    "memsize = %d\n", we_type, typestr, we_is16bit, dp8390_memsize);
		for (i = 0; i < 8; i++)
			printf("     %d -> 0x%x\n", i,
			    inb(WD_BASEREG + i));
	}
#endif

	return typestr;
}

int
EtherInit(unsigned char *myadr)
{
	const char *typestr;
	uint8_t x;
	int i;
	uint8_t laar_proto;
	uint8_t msr_proto;

	dp8390_iobase = WD_BASEREG + WE_NIC_OFFSET;
	dp8390_membase = WD_BASEMEM;

#ifndef _STANDALONE
	if (mapio()) {
		printf("no IO access\n");
		return 0;
	}
#endif

	for (x = 0, i = 0; i < 8; i++)
		x += inb(WD_BASEREG + WE_PROM + i);

	if (x != WE_ROM_CHECKSUM_TOTAL)
		return 0;

	/* reset the ethernet card */
	outb(WD_BASEREG + WE_MSR, WE_MSR_RST);
	delay(100);
	outb(WD_BASEREG + WE_MSR, inb(WD_BASEREG + WE_MSR) & ~WE_MSR_RST);
	delay(5000);

	typestr = we_params();
	if (!typestr)
		return 0;

	printf("Using %s board, port 0x%x, iomem 0x%x, iosiz %d\n",
	       typestr, WD_BASEREG, WD_BASEMEM, dp8390_memsize);

	/* get ethernet address */
	for (i = 0; i < 6; i++)
		eth_myaddr[i] = myadr[i]= inb(WD_BASEREG + WE_PROM + i);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory.
	 */
	if (dp8390_is790) {
		laar_proto = inb(WD_BASEREG + WE_LAAR) & ~WE_LAAR_M16EN;
		outb(WD_BASEREG + WE_LAAR, laar_proto |
		     (we_is16bit ? WE_LAAR_M16EN : 0));
	} else if ((we_type & WE_SOFTCONFIG) ||
		   (we_type == WE_TYPE_WD8013EBT)) {
		laar_proto = (WD_BASEMEM >> 19) & WE_LAAR_ADDRHI;
		if (we_is16bit)
			laar_proto |= WE_LAAR_L16EN;
		outb(WD_BASEREG + WE_LAAR, laar_proto |
		     (we_is16bit ? WE_LAAR_M16EN : 0));
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (dp8390_is790) {
		/* XXX MAGIC CONSTANTS XXX */
		x = inb(WD_BASEREG + 0x04);
		outb(WD_BASEREG + 0x04, x | 0x80);
		outb(WD_BASEREG + 0x0b,
		    ((WD_BASEMEM >> 13) & 0x0f) |
		    ((WD_BASEMEM >> 11) & 0x40) |
		    (inb(WD_BASEREG + 0x0b) & 0xb0));
		outb(WD_BASEREG + 0x04, x);
		msr_proto = 0x00;
		dp8390_cr_proto = 0x00;
	} else {
		msr_proto = (WD_BASEMEM >> 13) & WE_MSR_ADDR;
		dp8390_cr_proto = ED_CR_RD2;
	}

	outb(WD_BASEREG +  WE_MSR, msr_proto | WE_MSR_MENB);
	delay(2);

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * 16-bit cards also get word-wide DMA transfers.
	 */
	dp8390_dcr_reg = ED_DCR_FT1 | ED_DCR_LS | (we_is16bit ? ED_DCR_WTS : 0);

	if (dp8390_config())
		return 0;

#ifdef _STANDALONE
	strncpy(bi_netif.ifname, "we", sizeof(bi_netif.ifname));
	bi_netif.bus = BI_BUS_ISA;
	bi_netif.addr.iobase = WD_BASEREG;

	BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));
#endif
	return 1;
}

/*
 * Stop ethernet board
 */
void
EtherStop(void) {
	/* stop dp8390, followed by a board reset */
	dp8390_stop();
	outb(WD_BASEREG + WE_MSR, WE_MSR_RST);
	outb(WD_BASEREG + WE_MSR, 0);
}
