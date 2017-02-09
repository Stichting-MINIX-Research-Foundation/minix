/*	$NetBSD: memc.c,v 1.11 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * Support for the MEMECC and MEMC40 memory controllers on MVME68K
 * and MVME88K boards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: memc.c,v 1.11 2012/10/27 17:18:27 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/mvme/memcvar.h>
#include <dev/mvme/memcreg.h>
#include <dev/mvme/pcctwovar.h>
#include <dev/mvme/pcctworeg.h>

#include <dev/vme/vmevar.h>
#include <dev/mvme/mvmebus.h>
#include <dev/mvme/vme_twovar.h>
#include <dev/mvme/vme_tworeg.h>


static struct memc_softc	*memc_softcs[MEMC_NDEVS];
static int memc_softc_count;

static void memc040_attach(struct memc_softc *);
static void memecc_attach(struct memc_softc *);
static void memc_hook_error_intr(struct memc_softc *, int (*)(void *));

static int  memecc_err_intr(void *);
static void memecc_log_error(struct memc_softc *, u_int8_t, int, int);

#define MEMECC_SCRUBBER_PERIOD	86400	/* ~24 hours */

/*
 * The following stuff is used to decode the ECC syndrome code so
 * that we can figure out exactly which address/bit needed to be
 * corrected.
 */
#define MEMECC_SYN_BIT_MASK		0x0fu
#define MEMECC_SYN_BANK_A		(0x00u << 4)
#define MEMECC_SYN_BANK_B		(0x01u << 4)
#define MEMECC_SYN_BANK_C		(0x02u << 4)
#define MEMECC_SYN_BANK_D		(0x03u << 4)
#define MEMECC_SYN_BANK_SHIFT		4
#define MEMECC_SYN_BANK_MASK		0x03u
#define MEMECC_SYN_CHECKBIT_ERR		0x80u
#define MEMECC_SYN_INVALID		0xffu

static u_int8_t memc_syn_decode[256] = {
	MEMECC_SYN_INVALID,			/* 0x00 */
	MEMECC_SYN_CHECKBIT_ERR | 0,		/* 0x01: Checkbit 0 */
	MEMECC_SYN_CHECKBIT_ERR | 1,		/* 0x02: Checkbit 1 */
	MEMECC_SYN_INVALID,			/* 0x03 */
	MEMECC_SYN_CHECKBIT_ERR | 2,		/* 0x04: Checkbit 2 */
	MEMECC_SYN_INVALID,			/* 0x05 */
	MEMECC_SYN_INVALID,			/* 0x06 */
	MEMECC_SYN_BANK_C | 10,			/* 0x07: Bank C 10/26 */
	MEMECC_SYN_CHECKBIT_ERR | 3,		/* 0x08: Checkbit 3 */
	MEMECC_SYN_INVALID,			/* 0x09 */
	MEMECC_SYN_INVALID,			/* 0x0a */
	MEMECC_SYN_BANK_C | 13,			/* 0x0b: Bank C 13/29 */
	MEMECC_SYN_INVALID,			/* 0x0c */
	MEMECC_SYN_BANK_D | 1,			/* 0x0d: Bank D 1/17 */
	MEMECC_SYN_BANK_D | 2,			/* 0x0e: Bank D 2/18 */
	MEMECC_SYN_INVALID,			/* 0x0f */
	MEMECC_SYN_CHECKBIT_ERR | 4,		/* 0x10: Checkbit 4 */
	MEMECC_SYN_INVALID,			/* 0x11 */
	MEMECC_SYN_INVALID,			/* 0x12 */
	MEMECC_SYN_BANK_C | 14,			/* 0x13: Bank C 14/30 */
	MEMECC_SYN_INVALID,			/* 0x14 */
	MEMECC_SYN_BANK_D | 4,			/* 0x15: Bank D 4/20 */
	MEMECC_SYN_BANK_D | 5,			/* 0x16: Bank D 5/21 */
	MEMECC_SYN_INVALID,			/* 0x17 */
	MEMECC_SYN_INVALID,			/* 0x18 */
	MEMECC_SYN_BANK_D | 8,			/* 0x19: Bank D 8/24 */
	MEMECC_SYN_BANK_D | 9,			/* 0x1a: Bank D 9/25 */
	MEMECC_SYN_INVALID,			/* 0x1b */
	MEMECC_SYN_BANK_D | 10,			/* 0x1c: Bank D 10/26 */
	MEMECC_SYN_INVALID,			/* 0x1d */
	MEMECC_SYN_INVALID,			/* 0x1e */
	MEMECC_SYN_INVALID,			/* 0x1f */
	MEMECC_SYN_CHECKBIT_ERR | 5,		/* 0x20: Checkbit 5 */
	MEMECC_SYN_INVALID,			/* 0x21 */
	MEMECC_SYN_INVALID,			/* 0x22 */
	MEMECC_SYN_BANK_C | 0,			/* 0x23: Bank C 0/16 */
	MEMECC_SYN_INVALID,			/* 0x24 */
	MEMECC_SYN_BANK_D | 7,			/* 0x25: Bank D 7/23 */
	MEMECC_SYN_BANK_D | 6,			/* 0x26: Bank D 6/22 */
	MEMECC_SYN_INVALID,			/* 0x27 */
	MEMECC_SYN_INVALID,			/* 0x28 */
	MEMECC_SYN_BANK_A | 15,			/* 0x29: Bank A 15/31 */
	MEMECC_SYN_BANK_D | 12,			/* 0x2a: Bank D 12/28 */
	MEMECC_SYN_INVALID,			/* 0x2b */
	MEMECC_SYN_BANK_D | 13,			/* 0x2c: Bank D 13/29 */
	MEMECC_SYN_INVALID,			/* 0x2d */
	MEMECC_SYN_INVALID,			/* 0x2e */
	MEMECC_SYN_INVALID,			/* 0x2f */
	MEMECC_SYN_INVALID,			/* 0x30 */
	MEMECC_SYN_BANK_A | 14,			/* 0x31: Bank A 14/30 */
	MEMECC_SYN_BANK_A | 0,			/* 0x32: Bank A 0/16 */
	MEMECC_SYN_INVALID,			/* 0x33 */
	MEMECC_SYN_BANK_A | 1,			/* 0x34: Bank A 1/17 */
	MEMECC_SYN_INVALID,			/* 0x35 */
	MEMECC_SYN_INVALID,			/* 0x36 */
	MEMECC_SYN_INVALID,			/* 0x37 */
	MEMECC_SYN_BANK_A | 2,			/* 0x38: Bank A 2/18 */
	MEMECC_SYN_INVALID,			/* 0x39 */
	MEMECC_SYN_INVALID,			/* 0x3a */
	MEMECC_SYN_INVALID,			/* 0x3b */
	MEMECC_SYN_INVALID,			/* 0x3c */
	MEMECC_SYN_BANK_C | 3,			/* 0x3d: Bank C 3/19 */
	MEMECC_SYN_INVALID,			/* 0x3e */
	MEMECC_SYN_INVALID,			/* 0x3f */
	MEMECC_SYN_CHECKBIT_ERR | 6,		/* 0x40: Checkbit 6 */
	MEMECC_SYN_INVALID,			/* 0x41 */
	MEMECC_SYN_INVALID,			/* 0x42 */
	MEMECC_SYN_BANK_C | 1,			/* 0x43: Bank C 1/17 */
	MEMECC_SYN_INVALID,			/* 0x44 */
	MEMECC_SYN_BANK_C | 4,			/* 0x45: Bank C 4/20 */
	MEMECC_SYN_BANK_C | 8,			/* 0x46: Bank C 8/24 */
	MEMECC_SYN_INVALID,			/* 0x47 */
	MEMECC_SYN_INVALID,			/* 0x48 */
	MEMECC_SYN_BANK_C | 7,			/* 0x49: Bank C 7/23 */
	MEMECC_SYN_BANK_D | 15,			/* 0x4a: Bank D 15/31 */
	MEMECC_SYN_INVALID,			/* 0x4b */
	MEMECC_SYN_BANK_D | 14,			/* 0x4c: Bank D 14/30 */
	MEMECC_SYN_INVALID,			/* 0x4d */
	MEMECC_SYN_INVALID,			/* 0x4e */
	MEMECC_SYN_BANK_B | 3,			/* 0x4f: Bank B 3/19 */
	MEMECC_SYN_INVALID,			/* 0x50 */
	MEMECC_SYN_BANK_B | 4,			/* 0x51: Bank B 4/20 */
	MEMECC_SYN_BANK_B | 7,			/* 0x52: Bank B 7/23 */
	MEMECC_SYN_INVALID,			/* 0x53 */
	MEMECC_SYN_BANK_A | 4,			/* 0x54: Bank A 4/20 */
	MEMECC_SYN_INVALID,			/* 0x55 */
	MEMECC_SYN_INVALID,			/* 0x56 */
	MEMECC_SYN_INVALID,			/* 0x57 */
	MEMECC_SYN_BANK_A | 5,			/* 0x58: Bank A 5/21 */
	MEMECC_SYN_INVALID,			/* 0x59 */
	MEMECC_SYN_INVALID,			/* 0x5a */
	MEMECC_SYN_INVALID,			/* 0x5b */
	MEMECC_SYN_INVALID,			/* 0x5c */
	MEMECC_SYN_INVALID,			/* 0x5d */
	MEMECC_SYN_INVALID,			/* 0x5e */
	MEMECC_SYN_INVALID,			/* 0x5f */
	MEMECC_SYN_INVALID,			/* 0x60 */
	MEMECC_SYN_BANK_B | 5,			/* 0x61: Bank B 5/21 */
	MEMECC_SYN_BANK_B | 6,			/* 0x62: Bank B 6/22 */
	MEMECC_SYN_INVALID,			/* 0x63 */
	MEMECC_SYN_BANK_A | 8,			/* 0x64: Bank A 8/24 */
	MEMECC_SYN_INVALID,			/* 0x65 */
	MEMECC_SYN_INVALID,			/* 0x66 */
	MEMECC_SYN_INVALID,			/* 0x67 */
	MEMECC_SYN_BANK_A | 9,			/* 0x68: Bank A 9/25 */
	MEMECC_SYN_INVALID,			/* 0x69 */
	MEMECC_SYN_INVALID,			/* 0x6a */
	MEMECC_SYN_INVALID,			/* 0x6b */
	MEMECC_SYN_INVALID,			/* 0x6c */
	MEMECC_SYN_INVALID,			/* 0x6d */
	MEMECC_SYN_INVALID,			/* 0x6e */
	MEMECC_SYN_INVALID,			/* 0x6f */
	MEMECC_SYN_BANK_A | 10,			/* 0x70: Bank A 10/26 */
	MEMECC_SYN_INVALID,			/* 0x71 */
	MEMECC_SYN_INVALID,			/* 0x72 */
	MEMECC_SYN_INVALID,			/* 0x73 */
	MEMECC_SYN_INVALID,			/* 0x74 */
	MEMECC_SYN_INVALID,			/* 0x75 */
	MEMECC_SYN_INVALID,			/* 0x76 */
	MEMECC_SYN_INVALID,			/* 0x77 */
	MEMECC_SYN_INVALID,			/* 0x78 */
	MEMECC_SYN_INVALID,			/* 0x79 */
	MEMECC_SYN_BANK_C | 11,			/* 0x7a: Bank C 11/27 */
	MEMECC_SYN_INVALID,			/* 0x7b */
	MEMECC_SYN_INVALID,			/* 0x7c */
	MEMECC_SYN_INVALID,			/* 0x7d */
	MEMECC_SYN_INVALID,			/* 0x7e */
	MEMECC_SYN_INVALID,			/* 0x7f */
	MEMECC_SYN_CHECKBIT_ERR | 7,		/* 0x80: Checkbit 7 */
	MEMECC_SYN_INVALID,			/* 0x81 */
	MEMECC_SYN_INVALID,			/* 0x82 */
	MEMECC_SYN_BANK_C | 2,			/* 0x83: Bank C 2/18 */
	MEMECC_SYN_INVALID,			/* 0x84 */
	MEMECC_SYN_BANK_C | 5,			/* 0x85: Bank C 5/21 */
	MEMECC_SYN_BANK_C | 9,			/* 0x86: Bank C 9/25 */
	MEMECC_SYN_INVALID,			/* 0x87 */
	MEMECC_SYN_INVALID,			/* 0x88 */
	MEMECC_SYN_BANK_C | 6,			/* 0x89: Bank C 6/22 */
	MEMECC_SYN_BANK_C | 12,			/* 0x8a: Bank C 12/28 */
	MEMECC_SYN_INVALID,			/* 0x8b */
	MEMECC_SYN_BANK_D | 0,			/* 0x8c: Bank D 0/16 */
	MEMECC_SYN_INVALID,			/* 0x8d */
	MEMECC_SYN_INVALID,			/* 0x8e */
	MEMECC_SYN_INVALID,			/* 0x8f */
	MEMECC_SYN_INVALID,			/* 0x90 */
	MEMECC_SYN_BANK_B | 8,			/* 0x91: Bank B 8/24 */
	MEMECC_SYN_BANK_C | 15,			/* 0x92: Bank C 15/31 */
	MEMECC_SYN_INVALID,			/* 0x93 */
	MEMECC_SYN_BANK_A | 7,			/* 0x94: Bank A 7/23 */
	MEMECC_SYN_INVALID,			/* 0x95 */
	MEMECC_SYN_INVALID,			/* 0x96 */
	MEMECC_SYN_INVALID,			/* 0x97 */
	MEMECC_SYN_BANK_A | 6,			/* 0x98: Bank A 6/22 */
	MEMECC_SYN_INVALID,			/* 0x99 */
	MEMECC_SYN_INVALID,			/* 0x9a */
	MEMECC_SYN_INVALID,			/* 0x9b */
	MEMECC_SYN_INVALID,			/* 0x9c */
	MEMECC_SYN_INVALID,			/* 0x9d */
	MEMECC_SYN_BANK_B | 11,			/* 0x9e: Bank B 11/27 */
	MEMECC_SYN_INVALID,			/* 0x9f */
	MEMECC_SYN_INVALID,			/* 0xa0 */
	MEMECC_SYN_BANK_B | 9,			/* 0xa1: Bank B 9/25 */
	MEMECC_SYN_BANK_B | 12,			/* 0xa2: Bank B 12/28 */
	MEMECC_SYN_INVALID,			/* 0xa3 */
	MEMECC_SYN_BANK_B | 15,			/* 0xa4: Bank B 15/31 */
	MEMECC_SYN_INVALID,			/* 0xa5 */
	MEMECC_SYN_INVALID,			/* 0xa6 */
	MEMECC_SYN_BANK_A | 11,			/* 0xa7: Bank A 11/27 */
	MEMECC_SYN_BANK_A | 12,			/* 0xa8: Bank A 12/28 */
	MEMECC_SYN_INVALID,			/* 0xa9 */
	MEMECC_SYN_INVALID,			/* 0xaa */
	MEMECC_SYN_INVALID,			/* 0xab */
	MEMECC_SYN_INVALID,			/* 0xac */
	MEMECC_SYN_INVALID,			/* 0xad */
	MEMECC_SYN_INVALID,			/* 0xae */
	MEMECC_SYN_INVALID,			/* 0xaf */
	MEMECC_SYN_BANK_A | 13,			/* 0xb0: Bank A 13/29 */
	MEMECC_SYN_INVALID,			/* 0xb1 */
	MEMECC_SYN_INVALID,			/* 0xb2 */
	MEMECC_SYN_INVALID,			/* 0xb3 */
	MEMECC_SYN_INVALID,			/* 0xb4 */
	MEMECC_SYN_INVALID,			/* 0xb5 */
	MEMECC_SYN_INVALID,			/* 0xb6 */
	MEMECC_SYN_INVALID,			/* 0xb7 */
	MEMECC_SYN_INVALID,			/* 0xb8 */
	MEMECC_SYN_INVALID,			/* 0xb9 */
	MEMECC_SYN_INVALID,			/* 0xba */
	MEMECC_SYN_INVALID,			/* 0xbb */
	MEMECC_SYN_INVALID,			/* 0xbc */
	MEMECC_SYN_INVALID,			/* 0xbd */
	MEMECC_SYN_INVALID,			/* 0xbe */
	MEMECC_SYN_INVALID,			/* 0xbf */
	MEMECC_SYN_INVALID,			/* 0xc0 */
	MEMECC_SYN_BANK_B | 10,			/* 0xc1: Bank B 10/26 */
	MEMECC_SYN_BANK_B | 13,			/* 0xc2: Bank B 13/29 */
	MEMECC_SYN_INVALID,			/* 0xc3 */
	MEMECC_SYN_BANK_B | 14,			/* 0xc4: Bank B 14/30 */
	MEMECC_SYN_INVALID,			/* 0xc5 */
	MEMECC_SYN_INVALID,			/* 0xc6 */
	MEMECC_SYN_INVALID,			/* 0xc7 */
	MEMECC_SYN_BANK_B | 0,			/* 0xc8: Bank B 0/16 */
	MEMECC_SYN_INVALID,			/* 0xc9 */
	MEMECC_SYN_INVALID,			/* 0xca */
	MEMECC_SYN_INVALID,			/* 0xcb */
	MEMECC_SYN_INVALID,			/* 0xcc */
	MEMECC_SYN_INVALID,			/* 0xcd */
	MEMECC_SYN_INVALID,			/* 0xce */
	MEMECC_SYN_INVALID,			/* 0xcf */
	MEMECC_SYN_BANK_B | 1,			/* 0xd0: Bank B 1/17 */
	MEMECC_SYN_INVALID,			/* 0xd1 */
	MEMECC_SYN_INVALID,			/* 0xd2 */
	MEMECC_SYN_BANK_A | 3,			/* 0xd3: Bank A 3/19 */
	MEMECC_SYN_INVALID,			/* 0xd4 */
	MEMECC_SYN_INVALID,			/* 0xd5 */
	MEMECC_SYN_INVALID,			/* 0xd6 */
	MEMECC_SYN_INVALID,			/* 0xd7 */
	MEMECC_SYN_INVALID,			/* 0xd8 */
	MEMECC_SYN_INVALID,			/* 0xd9 */
	MEMECC_SYN_INVALID,			/* 0xda */
	MEMECC_SYN_INVALID,			/* 0xdb */
	MEMECC_SYN_INVALID,			/* 0xdc */
	MEMECC_SYN_INVALID,			/* 0xdd */
	MEMECC_SYN_INVALID,			/* 0xde */
	MEMECC_SYN_INVALID,			/* 0xdf */
	MEMECC_SYN_BANK_B | 2,			/* 0xe0: Bank B 2/18 */
	MEMECC_SYN_INVALID,			/* 0xe1 */
	MEMECC_SYN_INVALID,			/* 0xe2 */
	MEMECC_SYN_INVALID,			/* 0xe3 */
	MEMECC_SYN_INVALID,			/* 0xe4 */
	MEMECC_SYN_INVALID,			/* 0xe5 */
	MEMECC_SYN_INVALID,			/* 0xe6 */
	MEMECC_SYN_INVALID,			/* 0xe7 */
	MEMECC_SYN_INVALID,			/* 0xe8 */
	MEMECC_SYN_BANK_D | 11,			/* 0xe9: Bank D 11/27 */
	MEMECC_SYN_INVALID,			/* 0xea */
	MEMECC_SYN_INVALID,			/* 0xeb */
	MEMECC_SYN_INVALID,			/* 0xec */
	MEMECC_SYN_INVALID,			/* 0xed */
	MEMECC_SYN_INVALID,			/* 0xee */
	MEMECC_SYN_INVALID,			/* 0xef */
	MEMECC_SYN_INVALID,			/* 0xf0 */
	MEMECC_SYN_INVALID,			/* 0xf1 */
	MEMECC_SYN_INVALID,			/* 0xf2 */
	MEMECC_SYN_INVALID,			/* 0xf3 */
	MEMECC_SYN_BANK_D | 3,			/* 0xf4: Bank D 3/19 */
	MEMECC_SYN_INVALID,			/* 0xf5 */
	MEMECC_SYN_INVALID,			/* 0xf6 */
	MEMECC_SYN_INVALID,			/* 0xf7 */
	MEMECC_SYN_INVALID,			/* 0xf8 */
	MEMECC_SYN_INVALID,			/* 0xf9 */
	MEMECC_SYN_INVALID,			/* 0xfa */
	MEMECC_SYN_INVALID,			/* 0xfb */
	MEMECC_SYN_INVALID,			/* 0xfc */
	MEMECC_SYN_INVALID,			/* 0xfd */
	MEMECC_SYN_INVALID,			/* 0xfe */
	MEMECC_SYN_INVALID			/* 0xff */
};


/* ARGSUSED */
void
memc_init(struct memc_softc *sc)
{
	u_int8_t chipid;
	u_int8_t memcfg;

	if (memc_softc_count == MEMC_NDEVS)
		panic("memc_attach: too many memc devices!");

	memc_softcs[memc_softc_count++] = sc;

	chipid = memc_reg_read(sc, MEMC_REG_CHIP_ID);
	memcfg = memc_reg_read(sc, MEMC_REG_MEMORY_CONFIG);

	printf(": %dMB %s Memory Controller Chip (Rev %d)\n",
	    MEMC_MEMORY_CONFIG_2_MB(memcfg),
	    (chipid == MEMC_CHIP_ID_MEMC040) ? "Parity" : "ECC",
	    memc_reg_read(sc, MEMC_REG_CHIP_REVISION));

	printf("%s: Base Address: 0x%x, ", device_xname(sc->sc_dev),
	    MEMC_BASE_ADDRESS(memc_reg_read(sc, MEMC_REG_BASE_ADDRESS_HI),
			      memc_reg_read(sc, MEMC_REG_BASE_ADDRESS_LO)));

	printf("Fast RAM Read %sabled\n", (memc_reg_read(sc,
	    MEMC_REG_MEMORY_CONFIG) & MEMC_MEMORY_CONFIG_FSTRD) ?
	    "En" : "Dis");

	switch (chipid) {
	case MEMC_CHIP_ID_MEMC040:
		memc040_attach(sc);
		break;
	case MEMC_CHIP_ID_MEMECC:
		memecc_attach(sc);
		break;
	}
}

static void
memc040_attach(struct memc_softc *sc)
{

	/* XXX: TBD */
}

static void
memecc_attach(struct memc_softc *sc)
{
	u_int8_t rv;

	/*
	 * First, disable bus-error and interrupts on ECC errors.
	 * Also switch off SWAIT to enhance performance.
	 */
	rv = memc_reg_read(sc, MEMECC_REG_DRAM_CONTROL);
	rv &= ~(MEMECC_DRAM_CONTROL_NCEBEN |
	        MEMECC_DRAM_CONTROL_NCEIEN |
	        MEMECC_DRAM_CONTROL_SWAIT);
	rv |= MEMECC_DRAM_CONTROL_RAMEN;
	memc_reg_write(sc, MEMECC_REG_DRAM_CONTROL, rv);
	rv = memc_reg_read(sc, MEMECC_REG_SCRUB_CONTROL);
	rv &= ~(MEMECC_SCRUB_CONTROL_SCRBEN | MEMECC_SCRUB_CONTROL_SBEIEN);
	memc_reg_write(sc, MEMECC_REG_SCRUB_CONTROL, rv);

	/*
	 * Ensure error correction is enabled
	 */
	rv = memc_reg_read(sc, MEMECC_REG_DATA_CONTROL);
	rv &= ~MEMECC_DATA_CONTROL_DERC;
	memc_reg_write(sc, MEMECC_REG_DATA_CONTROL, rv);

	/*
	 * Clear any error currently in the logs
	 */
	rv = memc_reg_read(sc, MEMECC_REG_ERROR_LOGGER);
#ifdef DIAGNOSTIC
	if ((rv & MEMECC_ERROR_LOGGER_MASK) != 0)
		memecc_log_error(sc, rv, 0, 0);
#endif
	memc_reg_write(sc, MEMECC_REG_ERROR_LOGGER,
		    MEMECC_ERROR_LOGGER_ERRLOG);

	rv = memc_reg_read(sc, MEMECC_REG_ERROR_LOGGER + 2);
#ifdef DIAGNOSTIC
	if ((rv & MEMECC_ERROR_LOGGER_MASK) != 0)
		memecc_log_error(sc, rv, 2, 0);
#endif
	memc_reg_write(sc, MEMECC_REG_ERROR_LOGGER + 2,
		    MEMECC_ERROR_LOGGER_ERRLOG);

	/*
	 * Now hook the ECC error interrupt
	 */
	if (memc_softc_count == 1)
		memc_hook_error_intr(sc, memecc_err_intr);

	/*
	 * Enable bus-error and interrupt on uncorrectable ECC
	 */
	rv = memc_reg_read(sc, MEMECC_REG_DRAM_CONTROL);
	rv |= MEMECC_DRAM_CONTROL_NCEBEN | MEMECC_DRAM_CONTROL_NCEIEN;
	memc_reg_write(sc, MEMECC_REG_DRAM_CONTROL, rv);

	/*
	 * Set up the scrubber to run roughly once every 24 hours
	 * with minimal impact on the local bus. With these on/off
	 * time settings, a scrub of a 32MB DRAM board will take
	 * roughly half a minute.
	 */
	memc_reg_write(sc, MEMECC_REG_SCRUB_PERIOD_HI,
	    MEMECC_SCRUB_PERIOD_HI(MEMECC_SCRUBBER_PERIOD));
	memc_reg_write(sc, MEMECC_REG_SCRUB_PERIOD_LO,
	    MEMECC_SCRUB_PERIOD_LO(MEMECC_SCRUBBER_PERIOD));
	memc_reg_write(sc, MEMECC_REG_SCRUB_TIME_ONOFF,
	    MEMECC_SCRUB_TIME_ON_1 | MEMECC_SCRUB_TIME_OFF_16);

	/*
	 * Start the scrubber, and enable interrupts on Correctable errors
	 */
	memc_reg_write(sc, MEMECC_REG_SCRUB_CONTROL,
	    memc_reg_read(sc, MEMECC_REG_SCRUB_CONTROL) |
	    MEMECC_SCRUB_CONTROL_SCRBEN | MEMECC_SCRUB_CONTROL_SBEIEN);

	printf("%s: Logging ECC errors at ipl %d\n", device_xname(sc->sc_dev),
	    MEMC_IRQ_LEVEL);
}

static void
memc_hook_error_intr(struct memc_softc *sc, int (*func)(void *))
{

#if 0
	evcnt_attach_dynamic(&sc->sc_evcnt, EVCNT_TYPE_INTR,
	    (*sc->sc_isrevcnt)(sc->sc_isrcookie, MEMC_IRQ_LEVEL),
	    "memory", "ecc errors");
#endif

	/*
	 * On boards without a VMEChip2, the interrupt is routed
	 * via the MCChip (mvme162/mvme172).
	 */
	if (vmetwo_not_present)
		pcctwointr_establish(MCCHIPV_PARITY_ERR, func, MEMC_IRQ_LEVEL,
		    sc, &sc->sc_evcnt);
	else
		vmetwo_local_intr_establish(MEMC_IRQ_LEVEL,
		    VME2_VEC_PARITY_ERROR, func, sc, &sc->sc_evcnt);
}

/* ARGSUSED */
static int
memecc_err_intr(void *arg)
{
	struct memc_softc *sc;
	u_int8_t rv;
	int i, j, cnt = 0;

	/*
	 * For each memory controller we found ...
	 */
	for (i = 0; i < memc_softc_count; i++) {
		sc = memc_softcs[i];

		/*
		 * There are two error loggers per controller, the registers of
		 * the 2nd are offset from the 1st by 2 bytes.
		 */
		for (j = 0; j <= 2; j += 2) {
			rv = memc_reg_read(sc, MEMECC_REG_ERROR_LOGGER + j);
			if ((rv & MEMECC_ERROR_LOGGER_MASK) != 0) {
				memecc_log_error(sc, rv, j, 1);
				memc_reg_write(sc, MEMECC_REG_ERROR_LOGGER + j,
				    MEMECC_ERROR_LOGGER_ERRLOG);
				cnt++;
			}
		}
	}

	return (cnt);
}

/*
 * Log an ECC error to the console.
 * Note: Since this usually runs at an elevated ipl (above clock), we
 * should probably schedule a soft interrupt to log the error details.
 * (But only for errors where we would not normally panic.)
 */
static void
memecc_log_error(struct memc_softc *sc, u_int8_t errlog, int off, int mbepanic)
{
	u_int32_t addr;
	u_int8_t rv, syndrome;
	const char *bm = "CPU";
	const char *rdwr;
	const char *etype;
	char syntext[32];

	/*
	 * Get the address associated with the error.
	 */
	rv = memc_reg_read(sc, MEMECC_REG_ERROR_ADDRESS_HIHI + off);
	addr = (u_int32_t)rv;
	rv = memc_reg_read(sc, MEMECC_REG_ERROR_ADDRESS_HI + off);
	addr = (addr << 8) | (u_int32_t)rv;
	rv = memc_reg_read(sc, MEMECC_REG_ERROR_ADDRESS_MID + off);
	addr = (addr << 8) | (u_int32_t)rv;
	rv = memc_reg_read(sc, MEMECC_REG_ERROR_ADDRESS_LO + off);
	addr = (addr << 8) | (u_int32_t)rv;

	/*
	 * And the Syndrome bits
	 */
	syndrome = memc_reg_read(sc, MEMECC_REG_ERROR_SYNDROME + off);

	rdwr = ((errlog & MEMECC_ERROR_LOGGER_ERD) != 0) ? " read" : " write";

	if ((errlog & MEMECC_ERROR_LOGGER_EALT) != 0)
		bm = "Peripheral Device";
	else
	if ((errlog & MEMECC_ERROR_LOGGER_ESCRB) != 0) {
		bm = "Scrubber";
		rdwr = "";
	}

	if ((errlog & MEMECC_ERROR_LOGGER_SBE) != 0) {
		int syncode, bank, bitnum;

		etype = "Correctable";
		syncode = memc_syn_decode[syndrome];
		bitnum = (syncode & MEMECC_SYN_BIT_MASK) + (off ? 16 : 0);
		bank = (syncode >> MEMECC_SYN_BANK_SHIFT) &MEMECC_SYN_BANK_MASK;

		if (syncode == MEMECC_SYN_INVALID)
			strcpy(syntext, "Invalid!");
		else
		if ((syncode & MEMECC_SYN_CHECKBIT_ERR) != 0)
			snprintf(syntext, sizeof(syntext),
			    "Checkbit#%d", bitnum);
		else {
			addr |= (u_int32_t) (bank << 2);
			snprintf(syntext, sizeof(syntext),
			    "DRAM Bank %c, Bit#%d", 'A' + bank, bitnum);
		}
	} else if ((errlog & MEMECC_ERROR_LOGGER_MBE) != 0)
		etype = "Uncorrectable";
	else
		etype = "Spurious";

	printf("%s: %s error on %s%s access to 0x%08x.\n",
	    device_xname(sc->sc_dev), etype, bm, rdwr, addr);

	if ((errlog & MEMECC_ERROR_LOGGER_SBE) != 0)
		printf("%s: ECC Syndrome 0x%02x (%s)\n", device_xname(sc->sc_dev),
		    syndrome, syntext);

	/*
	 * If an uncorrectable error was detected by an alternate
	 * bus master or the scrubber, panic immediately.
	 * We can't rely on the contents of memory at this point.
	 *
	 * Uncorrectable errors detected when the CPU was accessing
	 * DRAM will cause the CPU to take a bus error trap. Depending
	 * on whether the error was in kernel or user mode, the system
	 * with either panic or kill the affected process. Basically,
	 * we don't have to deal with it here.
	 *
	 * XXX: I'm not sure whether it's our responsibility to
	 * perform some dummy writes to the offending address in this
	 * case to re-generate a good ECC. Note that we'd have to write
	 * an entire block of 4 words since we can only narrow down the
	 * faulty address for correctable errors...
	 */
	if (mbepanic && (errlog & MEMECC_ERROR_LOGGER_MBE) &&
	    (errlog & (MEMECC_ERROR_LOGGER_ESCRB|MEMECC_ERROR_LOGGER_EALT))) {
		/*
		 * Ensure we don't get a Bus Error while panicking...
		 */
		rv = memc_reg_read(sc, MEMECC_REG_DRAM_CONTROL + off);
		rv &= ~(MEMECC_DRAM_CONTROL_NCEBEN |
		        MEMECC_DRAM_CONTROL_NCEIEN);
		memc_reg_write(sc, MEMECC_REG_DRAM_CONTROL + off, rv);
		rv = memc_reg_read(sc, MEMECC_REG_SCRUB_CONTROL + off);
		rv &= ~(MEMECC_SCRUB_CONTROL_SBEIEN |
			MEMECC_SCRUB_CONTROL_SCRBEN);
		memc_reg_write(sc, MEMECC_REG_SCRUB_CONTROL + off, rv);

		panic("%s: Halting system to preserve data integrity.",
		    device_xname(sc->sc_dev));
	}
}
