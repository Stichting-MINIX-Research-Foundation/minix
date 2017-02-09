/*	$NetBSD: mpc105reg.h,v 1.3 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus J. Klein.
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

#ifndef _DEV_IC_MPC105REG_H_
#define _DEV_IC_MPC105REG_H_

/*
 * Register definitions for the Motorola MPC105 PCI Bridge/Memory
 * Controller (PCIB/MC), as found in:
 *
 * MPC105 PCI Bridge/Memory Controller User's Manual,
 * Motorola Publication Number MPC105UM/AD.
 */

#define	MPC105_PMCR		0x70	/* Power Management configuration */
#define	MPC105_MEMSTARTADDR1	0x80	/* Memory starting address 1 */
#define	MPC105_MEMSTARTADDR2	0x84	/* Memory starting address 2 */
#define	MPC105_EXTMEMSTARTADDR1	0x88	/* Extd. memory starting address 1 */
#define	MPC105_EXTMEMSTARTADDR2	0x8c	/* Extd. memory starting address 2 */
#define	MPC105_MEMENDADDR1	0x90	/* Memory ending address 1 */
#define	MPC105_MEMENDADDR2	0x94	/* Memory ending address 2 */
#define	MPC105_EXTMEMENDADDR1	0x98	/* Extd. memory ending address 1 */
#define	MPC105_EXTMEMENDADDR2	0x9c	/* Extd. memory ending address 2 */
#define	MPC105_MEMEN		0xa0	/* Memory enable */
#define	MPC105_PICR1		0xa8	/* Processor Interface Config 1 */
#define	 MPC105_PICR1_CBA_MASK	 0xff000000	/* Copy-back addr mask */
#define	 MPC105_PICR1_BREAD_WS	 0x00c00000	/* Burst read wait states: */
#define	 MPC105_PICR1_BREAD_WS0	 0x00000000	/*  0 wait states */
#define	 MPC105_PICR1_BREAD_WS1	 0x00400000	/*  1 wait state */
#define	 MPC105_PICR1_BREAD_WS2	 0x00800000	/*  2 wait states */
#define	 MPC105_PICR1_BREAD_WS3	 0x00c00000	/*  3 wait states */
#define	 MPC105_PICR1_CACHE_1G	 0x00200000	/* Cache 0-1G only */
#define	 MPC105_PICR1_RCS0	 0x00100000	/* ROM on 0:PCI, 1:60x bus */
#define	 MPC105_PICR1_XIO_MODE	 0x00080000	/* 0:Contig, 1:Discontig mode */
#define	 MPC105_PICR1_PROC_TYPE	 0x00060000	/* Processor type */
#define	 MPC105_PICR1_PROC_TYPE_601	0x00000000
#
#define	 MPC105_PICR1_PROC_TYPE_RSVD	0x00020000
#define	 MPC105_PICR1_PROC_TYPE_603	0x00040000
#define	 MPC105_PICR1_PROC_TYPE_604	0x00060000
#define	 MPC105_PICR1_XATS	 0x00010000	/* Address map 0:B, 1:A */
#define	 MPC105_PICR1_MP_ID	 0x00008000	/* Multiprocessor identifier */
#define	 MPC105_PICR1_RSVD0	 0x00004000
#define	 MPC105_PICR1_LBA_EN	 0x00002000	/* Local bus slave enable */
#define	 MPC105_PICR1_FLASHWR_EN 0x00001000	/* Flash writes enable */
#define	 MPC105_PICR1_MCP_EN	 0x00000800	/* Machine check enable */
#define	 MPC105_PICR1_TEA_EN	 0x00000400	/* Transfer error enable */
#define	 MPC105_PICR1_DPARK	 0x00000200	/* Data bus park */
#define	 MPC105_PICR1_RSVD1	 0x00000100
#define	 MPC105_PICR1_NO_PORT_REGS 0x00000080	/* Implement ext. conf regs */
#define	 MPC105_PICR1_ST_GATH_EN 0x00000040	/* Store gathering enable */
#define	 MPC105_PICR1_LE_MODE	 0x00000020	/* 0:Big, 1:Little endian */
#define	 MPC105_PICR1_LOOP_SNOOP 0x00000010	/* Snoop looping enable */
#define	 MPC105_PICR1_APARK	 0x00000008	/* Address bus park */
#define	 MPC105_PICR1_SPECREADS	 0x00000004	/* Speculative read enable */
#define	 MPC105_PICR1_L2_MP	 0x00000003	/* L2/multiproc config: */
#define	 MPC105_PICR1_L2_MP_NONE 0x00000000	/*  Uniprocessor/none */
#define	 MPC105_PICR1_L2_MP_WT	 0x00000001	/*  Write-through */
#define	 MPC105_PICR1_L2_MP_WB	 0x00000002	/*  Write-back */
#define	 MPC105_PICR1_L2_MP_MP	 0x00000003	/*  Multiprocessor */
#define	MPC105_PICR2		0xac	/* Processor Interface Config 2 */
#define	 MPC105_PICR2_L2_UPD_EN	 0x80000000	/* Service L2 cache misses */
#define	 MPC105_PICR2_L2_EN	 0x40000000	/* L2 cache enable */
#define	 MPC105_PICR2_RSVD0	 0x20000000
#define	 MPC105_PICR2_FLUSH_L2	 0x10000000	/* 0->1: flush L2 cache */
#define	 MPC105_PICR2_RSVD1	 0x0c000000
#define	 MPC105_PICR2_BYTE_DEC	 0x02000000	/* Do L2 byte-write decode */
#define	 MPC105_PICR2_FAST_L2_MODE 0x01000000	/* Fast L2 mode timing */
#define	 MPC105_PICR2_DATA_RAM_TYPE 0x00c00000	/* L2 data RAM type */
#define	 MPC105_PICR2_DATA_RAM_TYPE_SYNCBRST	0x00000000
#define	 MPC105_PICR2_DATA_RAM_TYPE_RSVD0	0x00400000
#define	 MPC105_PICR2_DATA_RAM_TYPE_ASYNC	0x00800000
#define	 MPC105_PICR2_DATA_RAM_TYPE_RSVD1	0x00c00000
#define	 MPC105_PICR2_WMODE	 0x00300000	/* SRAM write timing */
#define	 MPC105_PICR2_WMODE_RSVD	0x00000000
#define	 MPC105_PICR2_WMODE_NORMAL	0x00100000
#define	 MPC105_PICR2_WMODE_DELAYED	0x00200000
#define	 MPC105_PICR2_WMODE_EARLY	0x00300000
#define	 MPC105_PICR2_SNOOP_WS	 0x000c0000	/* Snoop wait states: */
#define	 MPC105_PICR2_SNOOP_WS0	 0x00000000	/*  0 clock cycles */
#define	 MPC105_PICR2_SNOOP_WS1	 0x00040000	/*  1 clock cycle */
#define	 MPC105_PICR2_SNOOP_WS2	 0x00080000	/*  2 clock cycles */
#define	 MPC105_PICR2_SNOOP_WS3	 0x000c0000	/*  3 clock cycles */
#define	 MPC105_PICR2_MOD_HIGH	 0x00020000	/* Cache modified polarity */
#define	 MPC105_PICR2_HIT_HIGH	 0x00010000	/* Cache hit polarity */
#define	 MPC105_PICR2_RSVD2	 0x00008000
#define	 MPC105_PICR2_ADDR_ONLY_DISABLE 0x00004000 /* L2 ignores CLEAN/
						      FLUSH/KILL ops */
#define	 MPC105_PICR2_HOLD	 0x00002000	/* L2 tag address hold */
#define	 MPC105_PICR2_INV_MODE	 0x00001000	/* L2 invalidate mode enable */
#define	 MPC105_PICR2_RSVD3	 0x00000800
#define	 MPC105_PICR2_L2_HIT_DELAY	 0x0000600	/* L2 hit delay */
#define	 MPC105_PICR2_L2_HIT_DELAYRSVD	 0x0000000	/*  reserved */
#define	 MPC105_PICR2_L2_HIT_DELAY1	 0x0000200	/*  1 clock cycle */
#define	 MPC105_PICR2_L2_HIT_DELAY2	 0x0000400	/*  2 clock cycles */
#define	 MPC105_PICR2_L2_HIT_DELAY3	 0x0000600	/*  3 clock cycles */
#define	 MPC105_PICR2_BURST_RATE 0x00000100	 /* L2: 0:1, 1:2 clocks */
#define	 MPC105_PICR2_FAST_CASTOUT 0x00000080	 /* L2 Fast castout timing */
#define	 MPC105_PICR2_TOE_WIDTH	 0x00000040	 /* TOE 0:2, 1:3 clock cycles */
#define	 MPC105_PICR2_L2_SIZE	 0x00000030	 /* L2 cache size */
#define	 MPC105_PICR2_L2_SIZE_256K	 0x00000000
#define	 MPC105_PICR2_L2_SIZE_512K	 0x00000010
#define	 MPC105_PICR2_L2_SIZE_1M	 0x00000020
#define	 MPC105_PICR2_L2_SIZE_RSVD	 0x00000030
#define	 MPC105_PICR2_APHASE_WS	 0x0000000c	 /* Addr. phase wait states: */
#define	 MPC105_PICR2_APHASE_WS0 0x00000000	 /*  0 clock cycles */
#define	 MPC105_PICR2_APHASE_WS1 0x00000004	 /*  1 clock cycle */
#define	 MPC105_PICR2_APHASE_WS2 0x00000008	 /*  2 clock cycles */
#define	 MPC105_PICR2_APHASE_WS3 0x0000000c	 /*  3 clock cycles */
#define	 MPC105_PICR2_DOE	 0x00000002	 /* L2 first data read timing */
#define	 MPC105_PICR2_WDATA	 0x00000001	 /* L2 first data write timing*/
#define	MPC105_AOVPR1		0xba	/* Alt. OS-visible parameters 1 */
#define	MPC105_AOVPR2		0xbb	/* Alt. OS-visible parameters 2 */
#define	MPC105_ERRENR1		0xc0	/* Error Enabling ter 1 */
#define	MPC105_ERRDR1		0xc1	/* Error Detection Register 1 */
#define	MPC105_60xBUSERRSTATR	0xc3	/* 60x Bus Error Status Register */
#define	MPC105_ERRENR2		0xc4	/* Error Enabling Register 2 */
#define	MPC105_ERRDR2		0xc5	/* Error Detection Register 2 */
#define	MPC105_PCIBUSERRSTATR	0xc7	/* PCI Bus Error Status Register */
#define	MPC105_ERRADDRR		0xc8	/* 60x/PCI Error Address Register */
#define	MPC105_MEMCTRLCR1	0xf0	/* Memory control configuration 1 */
#define	MPC105_MEMCTRLCR2	0xf4	/* Memory control configuration 2 */
#define	MPC105_MEMCTRLCR3	0xf8	/* Memory control configuration 3 */
#define	MPC105_MEMCTRLCR4	0xfc	/* Memory control configuration 4 */

#endif /* !_DEV_IC_MPC105REG_H_ */
