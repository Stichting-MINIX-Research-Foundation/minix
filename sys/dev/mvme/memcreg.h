/*	$NetBSD: memcreg.h,v 1.4 2008/04/28 20:23:54 martin Exp $	*/

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
 * Register definitions for the MEMECC and MEMC040 devices.
 */
#ifndef	_MVME_MEMCREG_H
#define	_MVME_MEMCREG_H

/*
 * Size, in bytes, of the memory controller's register set
 * (Actually, the MEMC040's register set is only 0x20 bytes in size, but
 * we go with the larger of the two).
 */
#define	MEMC_REGSIZE	0x80

/* Both memory controllers share some registers in common */
#define	MEMC_REG_CHIP_ID		0x00
#define  MEMC_CHIP_ID_MEMC040		0x80	/* It's a MEMC040 */
#define  MEMC_CHIP_ID_MEMECC		0x81	/* It's a MEMECC */

/* Revision of the ASIC */
#define	MEMC_REG_CHIP_REVISION		0x04

/* Configuration of the memory block controlled by this ASIC */
#define	MEMC_REG_MEMORY_CONFIG		0x08
#define  MEMC_MEMORY_CONFIG_2_BYTES(x)	(0x400000 << ((x) & 0x07))
#define  MEMC_MEMORY_CONFIG_2_MB(x)	(4 << ((x) & 0x07))
#define  MEMC040_MEMORY_CONFIG_EXTPEN	(1u << 3)  /* External parity enabled */
#define  MEMC040_MEMORY_CONFIG_WPB	(1u << 4)  /* Write Per Bit mode */
#define  MEMC_MEMORY_CONFIG_FSTRD	(1u << 5)  /* Fast RAM Read enabled */

/* Where, in the CPU's address space, does this memory appear? */
#define	MEMC_REG_BASE_ADDRESS_HI	0x14
#define	MEMC_REG_BASE_ADDRESS_LO	0x18
#define  MEMC_BASE_ADDRESS(hi,lo)	(((hi) << 24) | (((lo) & 0xc0) << 22))

/* Tells the memory controller what the board's Bus Clock frequency is */
#define	MEMC_REG_BUS_CLOCK		0x1c


/* Register offsets and definitions for the Parity Memory Controller */
#define	MEMC040_REG_ALT_STATUS		0x0c	/* Not used */
#define	MEMC040_REG_ALT_CONTROL		0x10	/* Not used */

/* Memory Control Register */
#define	MEMC040_REG_RAM_CONTROL		0x18
#define  MEMC040_RAM_CONTROL_RAMEN	(1u << 0)
#define  MEMC040_RAM_CONTROL_PAREN	(1u << 1)
#define  MEMC040_RAM_CONTROL_PARINT	(1u << 2)
#define  MEMC040_RAM_CONTROL_WWP	(1u << 3)
#define  MEMC040_RAM_CONTROL_SWAIT	(1u << 4)
#define  MEMC040_RAM_CONTROL_DMCTL	(1u << 5)


/* Register offsets and definitions for the ECC Memory Controller */
#define	MEMECC_REG_DRAM_CONTROL		0x18
#define  MEMECC_DRAM_CONTROL_RAMEN	(1u << 0)
#define  MEMECC_DRAM_CONTROL_NCEBEN	(1u << 1)
#define  MEMECC_DRAM_CONTROL_NCEIEN	(1u << 2)
#define  MEMECC_DRAM_CONTROL_RWB3	(1u << 3)
#define  MEMECC_DRAM_CONTROL_SWAIT	(1u << 4)
#define  MEMECC_DRAM_CONTROL_RWB5	(1u << 5)
#define  MEMECC_DRAM_CONTROL_BAD22	(1u << 6)
#define  MEMECC_DRAM_CONTROL_BAD23	(1u << 7)

#define	MEMECC_REG_DATA_CONTROL		0x20
#define  MEMECC_DATA_CONTROL_RWCKB	(1u << 3)
#define  MEMECC_DATA_CONTROL_ZFILL	(1u << 4)
#define  MEMECC_DATA_CONTROL_DERC	(1u << 5)

#define	MEMECC_REG_SCRUB_CONTROL	0x24
#define  MEMECC_SCRUB_CONTROL_IDIS	(1u << 0)
#define  MEMECC_SCRUB_CONTROL_SBEIEN	(1u << 1)
#define  MEMECC_SCRUB_CONTROL_SCRBEN	(1u << 3)
#define  MEMECC_SCRUB_CONTROL_SCRB	(1u << 4)
#define  MEMECC_SCRUB_CONTROL_HITDIS	(1u << 5)
#define  MEMECC_SCRUB_CONTROL_RADATA	(1u << 6)
#define  MEMECC_SCRUB_CONTROL_RACODE	(1u << 7)

#define	MEMECC_REG_SCRUB_PERIOD_HI	0x28
#define  MEMECC_SCRUB_PERIOD_HI(secs)	(((secs) / 2) >> 8)
#define	MEMECC_REG_SCRUB_PERIOD_LO	0x2c
#define  MEMECC_SCRUB_PERIOD_LO(secs)	(((secs) / 2) & 0xffu)

#define	MEMECC_REG_CHIP_PRESCALE	0x30

#define	MEMECC_REG_SCRUB_TIME_ONOFF	0x34
#define  MEMECC_SCRUB_TIME_ONOFF_MASK	0x07u
#define  MEMECC_SCRUB_TIME_OFF_0	0u
#define  MEMECC_SCRUB_TIME_OFF_16	1u
#define  MEMECC_SCRUB_TIME_OFF_32	2u
#define  MEMECC_SCRUB_TIME_OFF_64	3u
#define  MEMECC_SCRUB_TIME_OFF_128	4u
#define  MEMECC_SCRUB_TIME_OFF_256	5u
#define  MEMECC_SCRUB_TIME_OFF_512	6u
#define  MEMECC_SCRUB_TIME_OFF_NEVER	7u
#define  MEMECC_SCRUB_TIME_ON_1		(0u << 3)
#define  MEMECC_SCRUB_TIME_ON_16	(1u << 3)
#define  MEMECC_SCRUB_TIME_ON_32	(2u << 3)
#define  MEMECC_SCRUB_TIME_ON_64	(3u << 3)
#define  MEMECC_SCRUB_TIME_ON_128	(4u << 3)
#define  MEMECC_SCRUB_TIME_ON_256	(5u << 3)
#define  MEMECC_SCRUB_TIME_ON_512	(6u << 3)
#define  MEMECC_SCRUB_TIME_ON_ALWAYS	(7u << 3)
#define  MEMECC_SCRUB_TIME_SRDIS	(1u << 7)

#define	MEMECC_REG_SCRUB_PRESCALE_HI	0x38
#define	MEMECC_REG_SCRUB_PRESCALE_MID	0x3c
#define	MEMECC_REG_SCRUB_PRESCALE_LO	0x40

#define	MEMECC_REG_SCRUB_TIMER_HI	0x44
#define	MEMECC_REG_SCRUB_TIMER_LO	0x48

#define	MEMECC_REG_SCRUB_ADDR_CNTR_HIHI	0x4c
#define	MEMECC_REG_SCRUB_ADDR_CNTR_HI	0x50
#define	MEMECC_REG_SCRUB_ADDR_CNTR_MID	0x54
#define	MEMECC_REG_SCRUB_ADDR_CNTR_LO	0x58

#define	MEMECC_REG_ERROR_LOGGER		0x5c
#define  MEMECC_ERROR_LOGGER_MASK	0x7fu
#define  MEMECC_ERROR_LOGGER_SBE	(1u << 0)
#define  MEMECC_ERROR_LOGGER_MBE	(1u << 1)
#define  MEMECC_ERROR_LOGGER_EALT	(1u << 3)
#define  MEMECC_ERROR_LOGGER_ERA	(1u << 4)
#define  MEMECC_ERROR_LOGGER_ESCRB	(1u << 5)
#define  MEMECC_ERROR_LOGGER_ERD	(1u << 6)
#define  MEMECC_ERROR_LOGGER_ERRLOG	(1u << 7)

#define	MEMECC_REG_ERROR_ADDRESS_HIHI	0x60
#define	MEMECC_REG_ERROR_ADDRESS_HI	0x64
#define	MEMECC_REG_ERROR_ADDRESS_MID	0x68
#define	MEMECC_REG_ERROR_ADDRESS_LO	0x6c

#define	MEMECC_REG_ERROR_SYNDROME	0x70

#define	MEMECC_REG_DEFAULTS1		0x74
#define	MEMECC_REG_DEFAULTS2		0x78

#define	MEMECC_REG_SDRAM_CONFIG		0x7c

#endif	/* _MVME_MEMCREG_H */
