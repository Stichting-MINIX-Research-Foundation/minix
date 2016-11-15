/*	$NetBSD: pciide_sl82c105_reg.h,v 1.6 2008/04/28 20:23:55 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Register definitions for the Symphony Labs 82C105 PCI IDE
 * interface.  This 82C105 is also found embedded in the Winbond
 * 83C553 Southbridge.
 */

/* PCI configuration space registers */

#define	SYMPH_PORT0_P	(PCI_MAPREG_START + 0x00)	/* port 0 primary */
#define	SYMPH_PORT0_S	(PCI_MAPREG_START + 0x04)	/* port 0 secondary */
#define	SYMPH_PORT1_P	(PCI_MAPREG_START + 0x08)	/* port 1 primary */
#define	SYMPH_PORT1_S	(PCI_MAPREG_START + 0x0c)	/* port 1 secondary */
#define	SYMPH_BMIDER	(PCI_MAPREG_START + 0x10)	/* bus master regs */

#define	SYMPH_IDECSR	0x40		/* IDE control/status */
#define	SYMPH_P0D0CR	0x44		/* port 0 drive 0 control */
#define	SYMPH_P0D1CR	0x48		/* port 0 drive 1 control */
#define	SYMPH_P1D0CR	0x4c		/* port 1 drive 0 control */
#define	SYMPH_P1D1CR	0x50		/* port 1 drive 1 control */

#define	IDECR_IDE_IRQB	(1U << 30)	/* IDE_IRQB signal */
#define	IDECR_IDE_IRQA	(1U << 28)	/* IDE_IRQA signal */
#define	IDECR_RA_SHIFT	16		/* read-ahead duration */
#define	IDECR_RA_MASK	(0x7ff << IDECR_RA_SHIFT)
#define	IDECR_LEGIRQ	(1U << 11)	/* don't use legacy IRQ mode */
#define	IDECR_P1F16	(1U << 5)	/* port 1 fast 16 */
#define	IDECR_P1EN	(1U << 4)	/* port 1 enable */
#define	IDECR_P0F16	(1U << 1)	/* port 0 fast 16 */
#define	IDECR_P0EN	(1U << 0)	/* port 0 enable */

#define	PxDx_USR_SHIFT	16		/* user defined bits */
#define	PxDx_USR_MASK	(0xff << PxDx_USR_SHIFT)
#define	PxDx_CMD_ON_SHIFT 8		/* CMD ON time */
#define	PxDx_CMD_ON_MASK (0x1f << PxDx_CMD_ON_SHIFT)
#define	PxDx_PWEN	(1U << 7)	/* posted write enable */
#define	PxDx_RDYEN	(1U << 6)	/* IOCHRDY enable */
#define	PxDx_RAEN	(1U << 5)	/* read-ahead enable */
#define	PxDx_CMD_OFF_MASK (0x1f)	/* CMD OFF time */

/*
 * IDE CMD ON and CMD OFF times for a 33MHz PCI bus clock.
 *
 * These come from Table 4-4 of the 83c553 manual.
 */
struct symph_cmdtime {
	int	cmd_on;		/* cmd on time */
	int	cmd_off;	/* cmd off time */
};

static const struct symph_cmdtime symph_pio_times[]
    __unused = {
/*        programmed               actual       */
	{ 5, 13 },		/* 6, 14 */
	{ 4, 7 },		/* 5, 8 */
	{ 3, 4 },		/* 4, 5 */
	{ 2, 2 },		/* 3, 3 */
	{ 2, 0 },		/* 3, 1 */
	{ 1, 0 },		/* 2, 1 */
};

static const struct symph_cmdtime symph_sw_dma_times[]
    __unused = {
/*        programmed               actual       */
	{ 15, 15 },		/* 16, 16 */
};

static const struct symph_cmdtime symph_mw_dma_times[]
     __unused = {
/*        programmed               actual       */
	{ 7, 7 },		/* 8, 8 */
	{ 2, 1 },		/* 3, 2 */
	{ 2, 0 },		/* 3, 1 */
	{ 1, 0 },		/* 2, 1 */
};
