/*	$NetBSD: mpc106reg.h,v 1.4 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c) 2001,2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus J. Klein and Tim Rightnour
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

#ifndef _DEV_IC_MPC106REG_H_
#define _DEV_IC_MPC106REG_H_

/*
 * Register definitions for the Motorola MPC106 PCI Bridge/Memory
 * Controller (PCIB/MC), as found in:
 *
 * MPC106 PCI Bridge/Memory Controller User's Manual,
 * Motorola Publication Number MPC106UM/AD.
 */

#define	MPC106_PMCR1		0x70	/* Power Management configuration */
#define MPC106_PMCR2		0x72	/* PMC register 2 */
#define MPC106_ODCR		0x73	/* Output Driver Control Register */
#define	MPC106_MEMSTARTADDR1	0x80	/* Memory starting address 1 */
#define	MPC106_MEMSTARTADDR2	0x84	/* Memory starting address 2 */
#define	MPC106_EXTMEMSTARTADDR1	0x88	/* Extd. memory starting address 1 */
#define	MPC106_EXTMEMSTARTADDR2	0x8c	/* Extd. memory starting address 2 */
#define	MPC106_MEMENDADDR1	0x90	/* Memory ending address 1 */
#define	MPC106_MEMENDADDR2	0x94	/* Memory ending address 2 */
#define	MPC106_EXTMEMENDADDR1	0x98	/* Extd. memory ending address 1 */
#define	MPC106_EXTMEMENDADDR2	0x9c	/* Extd. memory ending address 2 */
#define	MPC106_MEMEN		0xa0	/* Memory enable */
#define	MPC106_PICR1		0xa8	/* Processor Interface Config 1 */
#define	 MPC106_PICR1_CBA_MASK	 __BITS(31,24)	/* Copy-back addr mask */
#define	 MPC106_PICR1_BREAD_WS	 __BITS(23,22)	/* Burst read wait states: */
#define	 MPC106_PICR1_BREAD_WS0	 0x00000000	/*  0 wait states */
#define	 MPC106_PICR1_BREAD_WS1	 0x00400000	/*  1 wait state */
#define	 MPC106_PICR1_BREAD_WS2	 0x00800000	/*  2 wait states */
#define	 MPC106_PICR1_BREAD_WS3	 0x00c00000	/*  3 wait states */
#define	 MPC106_PICR1_CACHE_1G	 __BIT(21)	/* Cache 0-1G only */
#define	 MPC106_PICR1_RCS0	 __BIT(20)	/* ROM on 0:PCI, 1:60x bus */
#define	 MPC106_PICR1_XIO_MODE	 __BIT(19)	/* 0:Contig, 1:Discontig mode */
#define	 MPC106_PICR1_PROC_TYPE	 __BITS(18,17)	/* Processor type */
#define	 MPC106_PICR1_PROC_TYPE_601	0x00000000
#define	 MPC106_PICR1_PROC_TYPE_RSVD	0x00020000
#define	 MPC106_PICR1_PROC_TYPE_603	0x00040000 /* also 740/750 */
#define	 MPC106_PICR1_PROC_TYPE_604	0x00060000
#define	 MPC106_PICR1_XATS	 __BIT(16)	/* Address map 0:B, 1:A */
#define	 MPC106_PICR1_MP_ID	 __BITS(15,14)	/* Multiprocessor identifier */
	/* 2 bits describe which proc is reading this reg, 0-3 */
#define	 MPC106_PICR1_LBA_EN	 __BIT(13)	/* Local bus slave enable */
#define	 MPC106_PICR1_FLASHWR_EN __BIT(12)	/* Flash writes enable */
#define	 MPC106_PICR1_MCP_EN	 __BIT(11)	/* Machine check enable */
#define	 MPC106_PICR1_TEA_EN	 __BIT(10)	/* Transfer error enable */
#define	 MPC106_PICR1_DPARK	 __BIT(9)	/* Data bus park */
#define	 MPC106_PICR1_EXT_L2_EN	 __BIT(8)	/* external l2 enable */
#define	 MPC106_PICR1_NO_PORT_REGS __BIT(7)	/* Implement ext. conf regs */
#define	 MPC106_PICR1_ST_GATH_EN __BIT(6)	/* Store gathering enable */
#define	 MPC106_PICR1_LE_MODE	 __BIT(5)	/* 0:Big, 1:Little endian */
#define	 MPC106_PICR1_LOOP_SNOOP __BIT(4)	/* Snoop looping enable */
#define	 MPC106_PICR1_APARK	 __BIT(3)	/* Address bus park */
#define	 MPC106_PICR1_SPECREADS	 __BIT(2)	/* Speculative read enable */
#define	 MPC106_PICR1_L2_MP	 __BITS(1,0)	/* L2/multiproc config: */
	/* must be read with MPC106_PICR1_EXT_L2_EN :
	 * L2_EN	L2_MP	Meaning
	 * 0		00	Uniprocessor/none
	 * 0		01	internal conrol/write-through
	 * 0		10	internal control/write-back
	 * 0		11	Multiproc/none
	 * 1		00	Uniprocessor/external L2
	 * 1		11	Multiproc/external L2
	 */
#define	 MPC106_PICR1_L2_MP_NONE 0x00000000	/*  Uniprocessor/none */
#define	 MPC106_PICR1_L2_MP_WT	 0x00000001	/*  Write-through */
#define	 MPC106_PICR1_L2_MP_WB	 0x00000002	/*  Write-back */
#define	 MPC106_PICR1_L2_MP_MP	 0x00000003	/*  Multiprocessor */
#define	MPC106_PICR2		0xac	/* Processor Interface Config 2 */
#define	 MPC106_PICR2_L2_UPD_EN	 __BIT(31)	/* Service L2 cache misses */
#define	 MPC106_PICR2_L2_EN	 __BIT(30)	/* L2 cache internal enable */
	/* also available at 0x81C */
#define	 MPC106_PICR2_NOSERCFG	 __BIT(29)	/* serialized config writes */
#define	 MPC106_PICR2_FLUSH_L2	 __BIT(28)	/* 0->1: flush L2 cache */
#define	 MPC106_PICR2_NOSNOOP_EN __BIT(27)	/* snoop transactions */
#define  MPC106_PICR2_CF_FF0_LOC __BIT(26)	/* ROM remapping enable */
#define	 MPC106_PICR2_FLASH_LOCK __BIT(25)	/* Flash write lockout */
#define	 MPC106_PICR2_FAST_L2_MODE __BIT(24)	/* Fast L2 mode timing */
#define	 MPC106_PICR2_DATA_RAM_TYPE __BITS(23,22)	/* L2 data RAM type */
#define	 MPC106_PICR2_DATA_RAM_TYPE_SYNCBRST	0x00000000
#define	 MPC106_PICR2_DATA_RAM_TYPE_PIPEBRST	0x00400000
#define	 MPC106_PICR2_DATA_RAM_TYPE_ASYNC	0x00800000
#define	 MPC106_PICR2_DATA_RAM_TYPE_RSVD1	0x00c00000
#define	 MPC106_PICR2_WMODE	 __BITS(21,20)	/* SRAM write timing */
#define	 MPC106_PICR2_WMODE_NORMAL	0x00000000 /* norm w/o partial update */
#define	 MPC106_PICR2_WMODE_NORMAL_PART	0x00100000 /* norm with part upd. */
#define	 MPC106_PICR2_WMODE_DELAYED	0x00200000
#define	 MPC106_PICR2_WMODE_EARLY	0x00300000
#define	 MPC106_PICR2_SNOOP_WS	 __BITS(19,18)	/* Snoop wait states: */
#define	 MPC106_PICR2_SNOOP_WS0	 0x00000000	/*  0 wait states */
#define	 MPC106_PICR2_SNOOP_WS1	 0x00040000	/*  1 wait state */
#define	 MPC106_PICR2_SNOOP_WS2	 0x00080000	/*  2 wait states */
#define	 MPC106_PICR2_SNOOP_WS3	 0x000c0000	/*  3 wait states */
#define	 MPC106_PICR2_MOD_HIGH	 __BIT(17)	/* Cache modified polarity */
#define	 MPC106_PICR2_HIT_HIGH	 __BIT(16)	/* Cache hit polarity */
#define	 MPC106_PICR2_RSVD2	 __BIT(15)
#define	 MPC106_PICR2_ADDR_ONLY_DISABLE __BIT(14) /* L2 ignores CLEAN/
						      FLUSH/KILL ops */
#define	 MPC106_PICR2_HOLD	 __BIT(13)	/* L2 tag address hold */
#define	 MPC106_PICR2_INV_MODE	 __BIT(12)	/* L2 invalidate mode enable */
#define	 MPC106_PICR2_RWITM	 __BIT(11)	/* read with intent to modify
						   line-fill disable */
#define	 MPC106_PICR2_L2_HIT_DELAY	 __BITS(10,9)	/* L2 hit delay */
#define	 MPC106_PICR2_L2_HIT_DELAYRSVD	 0x0000000	/*  reserved */
#define	 MPC106_PICR2_L2_HIT_DELAY1	 0x0000200	/*  1 clock cycle */
#define	 MPC106_PICR2_L2_HIT_DELAY2	 0x0000400	/*  2 clock cycles */
#define	 MPC106_PICR2_L2_HIT_DELAY3	 0x0000600	/*  3 clock cycles */
#define	 MPC106_PICR2_BANKS	__BIT(8) 	 /* L2: nrof banks */
#define	 MPC106_PICR2_FAST_CASTOUT __BIT(7)	 /* L2 Fast castout timing */
#define	 MPC106_PICR2_TOE_WIDTH	__BIT(6)	 /* TOE 0:2, 1:3 clock cycles */
#define	 MPC106_PICR2_L2_SIZE	__BITS(5,4)	 /* L2 cache size */
#define	 MPC106_PICR2_L2_SIZE_256K	 0x00000000
#define	 MPC106_PICR2_L2_SIZE_512K	 0x00000010
#define	 MPC106_PICR2_L2_SIZE_1M	 0x00000020
#define	 MPC106_PICR2_L2_SIZE_RSVD	 0x00000030
#define	 MPC106_PICR2_APHASE_WS	 __BITS(3,2)	 /* Addr. phase wait states: */
#define	 MPC106_PICR2_APHASE_WS0 0x00000000	 /*  0 clock cycles */
#define	 MPC106_PICR2_APHASE_WS1 0x00000004	 /*  1 clock cycle */
#define	 MPC106_PICR2_APHASE_WS2 0x00000008	 /*  2 clock cycles */
#define	 MPC106_PICR2_APHASE_WS3 0x0000000c	 /*  3 clock cycles */
#define	 MPC106_PICR2_DOE	 __BIT(1)	 /* L2 first data read timing */
#define	 MPC106_PICR2_WDATA	 __BIT(0)	 /* L2 first data write timing*/
#define MPC106_ECC_SBECR	0xb8	/* ECC single bit err count reg */
#define MPC106_ECC_SBETR	0xb9	/* ECC single bit error trigger */
#define	MPC106_AOVPR1		0xba	/* Alt. OS-visible parameters 1 */
#define	MPC106_AOVPR2		0xbb	/* Alt. OS-visible parameters 2 */
#define	MPC106_ERRENR1		0xc0	/* Error Enabling ter 1 */
#define	MPC106_ERRDR1		0xc1	/* Error Detection Register 1 */
#define	MPC106_60xBUSERRSTATR	0xc3	/* 60x Bus Error Status Register */
#define	MPC106_ERRENR2		0xc4	/* Error Enabling Register 2 */
#define	MPC106_ERRDR2		0xc5	/* Error Detection Register 2 */
#define	MPC106_PCIBUSERRSTATR	0xc7	/* PCI Bus Error Status Register */
#define	MPC106_ERRADDRR		0xc8	/* 60x/PCI Error Address Register */
#define MPC106_ESCR1		0xe0	/* emulation support conf reg 1 */
#define MPC106_ESCR2		0xe8	/* emulation support conf reg 2 */
#define MPC106_MMSR1		0xe4	/* modified memory status reg 1 */
#define MPC106_MMSR2		0xec	/* modified memory status reg 2 */
#define	MPC106_MCCR1		0xf0	/* Memory control configuration 1 */
#define  MPC106_MCCR1_ROMNAL	__BITS(31,28)	/* burst mode rom reads */
#define  MPC106_MCCR1_ROMFAL	__BITS(27,23)	/* nonburst mode rom reads */
#define  MPC106_MCCR1_501_MODE	__BIT(22)	/* 501 mode conf signal */
#define  MPC106_MCCR1_8N64	__BIT(21)
#define  MPC106_MCCR1_BURST	__BIT(20)
#define  MPC106_MCCR1_MEMGO	__BIT(19)
#define  MPC106_MCCR1_SREN	__BIT(18)
#define  MPC106_MCCR1_RAMTYPE	__BIT(17)	/* 0:SDRAM 1:DRAM/EDO */
#define  MPC106_MCCR1_PCKEN	__BIT(16)	/* dis/enable parity */
/* bits 15-0 are ram bank row address bit counts */
#define	MPC106_MCCR2		0xf4	/* Memory control configuration 2 */
#define  MPC106_MCRR2_TSWAIT	__BITS(31,29)
#define  MPC106_MCRR2_RESV0	__BITS(28,22)	/* reserved */
#define  MPC106_MCRR2_BSTOPRE0	__BIT(21)
#define  MPC106_MCRR2_BSTOPRE1	__BIT(20)
#define  MPC106_MCRR2_EXT_ECM_PAR_EN	__BIT(19) /* ext. ECM parity check */
#define  MPC106_MCRR2_EXT_ECM_ECC_EN	__BIT(18) /* ext. ECM ECC enable */
#define  MPC106_MCRR2_ECC_EN	__BIT(17)	/* ECC enable */
#define  MPC106_MCRR2_EDO	__BIT(16)	/* EDO enable */
#define  MPC106_MCRR2_REFINT	__BITS(15,2)	/* refreash interval */
#define  MPC106_MCRR2_BUFMODE	__BIT(1)	/* buffer mode */
#define  MPC106_MCRR2_RMW_PAR	__BIT(0)	/* RMW parity enable */
#define	MPC106_MCCR3		0xf8	/* Memory control configuration 3 */
#define	MPC106_MCCR4		0xfc	/* Memory control configuration 4 */
#define  MPC106_MCRR4_PRETOACT	__BITS(31,28)
#define  MPC106_MCRR4_ACTOPRE	__BITS(27,24)
#define  MPC106_MCRR4_EXT_ECM_EN __BITS(23,22)	/* ext ecm enable */
						/* 00= disabled 11=enabled */

#define MPC106_ECR1	0x092	/* External config register 1 (LE mode) */
#define MPC106_ECR2	0x81C	/* External config register 2 */
#define MPC106_ECR2_L2_UPD	0x80	/* same as bit31 of PICR2 */
#define MPC106_ECR2_L2_EN	0x40	/* L2 cache enable */
#define MPC106_ECR2_TEA_EN	0x20	/* TEA enable */
#define MPC106_ECR2_L2_FLUSH	0x10	/* l2 cache flush */
#define MPC106_ECR3	0x850

#endif /* !_DEV_IC_MPC106REG_H_ */
