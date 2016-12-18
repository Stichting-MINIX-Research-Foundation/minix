/*	$NetBSD: hifn7751reg.h,v 1.8 2005/12/11 12:22:49 christos Exp $	*/
/*	$OpenBSD: hifn7751reg.h,v 1.43 2003/06/02 15:58:41 deraadt Exp $	*/

/*
 * Invertex AEON / Hifn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000-2001 Network Security Technologies, Inc.
 *			http://www.netsec.net
 *
 * Please send any comments, feedback, bug-fixes, or feature requests to
 * software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */
#ifndef __DEV_PCI_HIFN7751REG_H__
#define __DEV_PCI_HIFN7751REG_H__

#include <machine/endian.h>

/*
 * Some PCI configuration space offset defines.  The names were made
 * identical to the names used by the Linux kernel.
 */
#define HIFN_BAR0		(PCI_MAPREG_START + 0)	/* PUC register map */
#define HIFN_BAR1		(PCI_MAPREG_START + 4)	/* DMA register map */
#define	HIFN_RETRY_TIMEOUT	0x41
#define	HIFN_TRDY_TIMEOUT	0x40

/*
 * The values below should multiple of 4 -- and be large enough to handle
 * any command the driver implements.
 *
 * MAX_COMMAND = base command + mac command + encrypt command +
 *			mac-key + rc4-key
 * MAX_RESULT  = base result + comp result + mac result + mac + encrypt result
 *
 *
 */
#define	HIFN_MAX_COMMAND	(8 + 8 + 8 + 64 + 260)
#define	HIFN_MAX_RESULT		(8 + 4 + 4 + 20 + 4)

/*
 * hifn_desc_t
 *
 * Holds an individual descriptor for any of the rings.
 */
struct hifn_desc {
	volatile u_int32_t l;		/* length and status bits */
	volatile u_int32_t p;
};

/*
 * Masks for the "length" field of struct hifn_desc.
 */
#define	HIFN_D_LENGTH		0x0000ffff	/* length bit mask */
#define	HIFN_D_MASKDONEIRQ	0x02000000	/* mask the done interrupt */
#define	HIFN_D_DESTOVER		0x04000000	/* destination overflow */
#define	HIFN_D_OVER		0x08000000	/* overflow */
#define	HIFN_D_LAST		0x20000000	/* last descriptor in chain */
#define	HIFN_D_JUMP		0x40000000	/* jump descriptor */
#define	HIFN_D_VALID		0x80000000	/* valid bit */

/*
 * Processing Unit Registers (offset from BASEREG0)
 */
#define	HIFN_0_PUDATA		0x00	/* Processing Unit Data */
#define	HIFN_0_PUCTRL		0x04	/* Processing Unit Control */
#define	HIFN_0_PUISR		0x08	/* Processing Unit Interrupt Status */
#define	HIFN_0_PUCNFG		0x0c	/* Processing Unit Configuration */
#define	HIFN_0_PUIER		0x10	/* Processing Unit Interrupt Enable */
#define	HIFN_0_PUSTAT		0x14	/* Processing Unit Status/Chip ID */
#define	HIFN_0_FIFOSTAT		0x18	/* FIFO Status */
#define	HIFN_0_FIFOCNFG		0x1c	/* FIFO Configuration */
#define	HIFN_0_SPACESIZE	0x20	/* Register space size */

/* Processing Unit Control Register (HIFN_0_PUCTRL) */
#define	HIFN_PUCTRL_CLRSRCFIFO	0x0010	/* clear source fifo */
#define	HIFN_PUCTRL_STOP	0x0008	/* stop pu */
#define	HIFN_PUCTRL_LOCKRAM	0x0004	/* lock ram */
#define	HIFN_PUCTRL_DMAENA	0x0002	/* enable dma */
#define	HIFN_PUCTRL_RESET	0x0001	/* Reset processing unit */

/* Processing Unit Interrupt Status Register (HIFN_0_PUISR) */
#define	HIFN_PUISR_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUISR_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUISR_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUISR_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUISR_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUISR_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUISR_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUISR_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUISR_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUISR_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Configuration Register (HIFN_0_PUCNFG) */
#define	HIFN_PUCNFG_DRAMMASK	0xe000	/* DRAM size mask */
#define	HIFN_PUCNFG_DSZ_256K	0x0000	/* 256k dram */
#define	HIFN_PUCNFG_DSZ_512K	0x2000	/* 512k dram */
#define	HIFN_PUCNFG_DSZ_1M	0x4000	/* 1m dram */
#define	HIFN_PUCNFG_DSZ_2M	0x6000	/* 2m dram */
#define	HIFN_PUCNFG_DSZ_4M	0x8000	/* 4m dram */
#define	HIFN_PUCNFG_DSZ_8M	0xa000	/* 8m dram */
#define	HIFN_PUNCFG_DSZ_16M	0xc000	/* 16m dram */
#define	HIFN_PUCNFG_DSZ_32M	0xe000	/* 32m dram */
#define	HIFN_PUCNFG_DRAMREFRESH	0x1800	/* DRAM refresh rate mask */
#define	HIFN_PUCNFG_DRFR_512	0x0000	/* 512 divisor of ECLK */
#define	HIFN_PUCNFG_DRFR_256	0x0800	/* 256 divisor of ECLK */
#define	HIFN_PUCNFG_DRFR_128	0x1000	/* 128 divisor of ECLK */
#define	HIFN_PUCNFG_TCALLPHASES	0x0200	/* your guess is as good as mine... */
#define	HIFN_PUCNFG_TCDRVTOTEM	0x0100	/* your guess is as good as mine... */
#define	HIFN_PUCNFG_BIGENDIAN	0x0080	/* DMA big endian mode */
#define	HIFN_PUCNFG_BUS32	0x0040	/* Bus width 32bits */
#define	HIFN_PUCNFG_BUS16	0x0000	/* Bus width 16 bits */
#define	HIFN_PUCNFG_CHIPID	0x0020	/* Allow chipid from PUSTAT */
#define	HIFN_PUCNFG_DRAM	0x0010	/* Context RAM is DRAM */
#define	HIFN_PUCNFG_SRAM	0x0000	/* Context RAM is SRAM */
#define	HIFN_PUCNFG_COMPSING	0x0004	/* Enable single compression context */
#define	HIFN_PUCNFG_ENCCNFG	0x0002	/* Encryption configuration */

/* Processing Unit Interrupt Enable Register (HIFN_0_PUIER) */
#define	HIFN_PUIER_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUIER_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUIER_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUIER_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUIER_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUIER_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUIER_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUIER_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUIER_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUIER_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Status Register/Chip ID (HIFN_0_PUSTAT) */
#define	HIFN_PUSTAT_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUSTAT_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUSTAT_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUSTAT_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUSTAT_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUSTAT_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUSTAT_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUSTAT_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUSTAT_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUSTAT_DSTRESULT	0x0004	/* Destination result interrupt */
#define	HIFN_PUSTAT_CHIPREV	0x00ff	/* Chip revision mask */
#define	HIFN_PUSTAT_CHIPENA	0xff00	/* Chip enabled mask */
#define	HIFN_PUSTAT_ENA_2	0x1100	/* Level 2 enabled */
#define	HIFN_PUSTAT_ENA_1	0x1000	/* Level 1 enabled */
#define	HIFN_PUSTAT_ENA_0	0x3000	/* Level 0 enabled */
#define	HIFN_PUSTAT_REV_2	0x0020	/* 7751 PT6/2 */
#define	HIFN_PUSTAT_REV_3	0x0030	/* 7751 PT6/3 */

/* FIFO Status Register (HIFN_0_FIFOSTAT) */
#define	HIFN_FIFOSTAT_SRC	0x7f00	/* Source FIFO available */
#define	HIFN_FIFOSTAT_DST	0x007f	/* Destination FIFO available */

/* FIFO Configuration Register (HIFN_0_FIFOCNFG) */
#define	HIFN_FIFOCNFG_THRESHOLD	0x0400	/* must be written as 1 */

/*
 * DMA Interface Registers (offset from BASEREG1)
 */
#define	HIFN_1_DMA_CRAR		0x0c	/* DMA Command Ring Address */
#define	HIFN_1_DMA_SRAR		0x1c	/* DMA Source Ring Address */
#define	HIFN_1_DMA_RRAR		0x2c	/* DMA Result Ring Address */
#define	HIFN_1_DMA_DRAR		0x3c	/* DMA Destination Ring Address */
#define	HIFN_1_DMA_CSR		0x40	/* DMA Status and Control */
#define	HIFN_1_DMA_IER		0x44	/* DMA Interrupt Enable */
#define	HIFN_1_DMA_CNFG		0x48	/* DMA Configuration */
#define	HIFN_1_PLL		0x4c	/* 795x: PLL config */
#define	HIFN_1_7811_RNGENA	0x60	/* 7811: rng enable */
#define	HIFN_1_7811_RNGCFG	0x64	/* 7811: rng config */
#define	HIFN_1_7811_RNGDAT	0x68	/* 7811: rng data */
#define	HIFN_1_7811_RNGSTS	0x6c	/* 7811: rng status */
#define	HIFN_1_7811_MIPSRST	0x94	/* 7811: MIPS reset */
#define	HIFN_1_REVID		0x98	/* Revision ID */
#define	HIFN_1_UNLOCK_SECRET1	0xf4
#define	HIFN_1_UNLOCK_SECRET2	0xfc
#define	HIFN_1_PUB_RESET	0x204	/* Public/RNG Reset */
#define	HIFN_1_PUB_BASE		0x300	/* Public Base Address */
#define	HIFN_1_PUB_OPLEN	0x304	/* Public Operand Length */
#define	HIFN_1_PUB_OP		0x308	/* Public Operand */
#define	HIFN_1_PUB_STATUS	0x30c	/* Public Status */
#define	HIFN_1_PUB_IEN		0x310	/* Public Interrupt enable */
#define	HIFN_1_RNG_CONFIG	0x314	/* RNG config */
#define	HIFN_1_RNG_DATA		0x318	/* RNG data */
#define	HIFN_1_PUB_MEM		0x400	/* start of Public key memory */
#define	HIFN_1_PUB_MEMEND	0xbff	/* end of Public key memory */

/* DMA Status and Control Register (HIFN_1_DMA_CSR) */
#define	HIFN_DMACSR_D_CTRLMASK	0xc0000000	/* Destinition Ring Control */
#define	HIFN_DMACSR_D_CTRL_NOP	0x00000000	/* Dest. Control: no-op */
#define	HIFN_DMACSR_D_CTRL_DIS	0x40000000	/* Dest. Control: disable */
#define	HIFN_DMACSR_D_CTRL_ENA	0x80000000	/* Dest. Control: enable */
#define	HIFN_DMACSR_D_ABORT	0x20000000	/* Destinition Ring PCIAbort */
#define	HIFN_DMACSR_D_DONE	0x10000000	/* Destinition Ring Done */
#define	HIFN_DMACSR_D_LAST	0x08000000	/* Destinition Ring Last */
#define	HIFN_DMACSR_D_WAIT	0x04000000	/* Destinition Ring Waiting */
#define	HIFN_DMACSR_D_OVER	0x02000000	/* Destinition Ring Overflow */
#define	HIFN_DMACSR_R_CTRL	0x00c00000	/* Result Ring Control */
#define	HIFN_DMACSR_R_CTRL_NOP	0x00000000	/* Result Control: no-op */
#define	HIFN_DMACSR_R_CTRL_DIS	0x00400000	/* Result Control: disable */
#define	HIFN_DMACSR_R_CTRL_ENA	0x00800000	/* Result Control: enable */
#define	HIFN_DMACSR_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	HIFN_DMACSR_R_DONE	0x00100000	/* Result Ring Done */
#define	HIFN_DMACSR_R_LAST	0x00080000	/* Result Ring Last */
#define	HIFN_DMACSR_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	HIFN_DMACSR_R_OVER	0x00020000	/* Result Ring Overflow */
#define	HIFN_DMACSR_S_CTRL	0x0000c000	/* Source Ring Control */
#define	HIFN_DMACSR_S_CTRL_NOP	0x00000000	/* Source Control: no-op */
#define	HIFN_DMACSR_S_CTRL_DIS	0x00004000	/* Source Control: disable */
#define	HIFN_DMACSR_S_CTRL_ENA	0x00008000	/* Source Control: enable */
#define	HIFN_DMACSR_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	HIFN_DMACSR_S_DONE	0x00001000	/* Source Ring Done */
#define	HIFN_DMACSR_S_LAST	0x00000800	/* Source Ring Last */
#define	HIFN_DMACSR_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	HIFN_DMACSR_ILLW	0x00000200	/* Illegal write (7811 only) */
#define	HIFN_DMACSR_ILLR	0x00000100	/* Illegal read (7811 only) */
#define	HIFN_DMACSR_C_CTRL	0x000000c0	/* Command Ring Control */
#define	HIFN_DMACSR_C_CTRL_NOP	0x00000000	/* Command Control: no-op */
#define	HIFN_DMACSR_C_CTRL_DIS	0x00000040	/* Command Control: disable */
#define	HIFN_DMACSR_C_CTRL_ENA	0x00000080	/* Command Control: enable */
#define	HIFN_DMACSR_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	HIFN_DMACSR_C_DONE	0x00000010	/* Command Ring Done */
#define	HIFN_DMACSR_C_LAST	0x00000008	/* Command Ring Last */
#define	HIFN_DMACSR_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	HIFN_DMACSR_PUBDONE	0x00000002	/* Public op done (7951 only) */
#define	HIFN_DMACSR_ENGINE	0x00000001	/* Command Ring Engine IRQ */

/* DMA Interrupt Enable Register (HIFN_1_DMA_IER) */
#define	HIFN_DMAIER_D_ABORT	0x20000000	/* Destination Ring PCIAbort */
#define	HIFN_DMAIER_D_DONE	0x10000000	/* Destination Ring Done */
#define	HIFN_DMAIER_D_LAST	0x08000000	/* Destination Ring Last */
#define	HIFN_DMAIER_D_WAIT	0x04000000	/* Destination Ring Waiting */
#define	HIFN_DMAIER_D_OVER	0x02000000	/* Destination Ring Overflow */
#define	HIFN_DMAIER_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	HIFN_DMAIER_R_DONE	0x00100000	/* Result Ring Done */
#define	HIFN_DMAIER_R_LAST	0x00080000	/* Result Ring Last */
#define	HIFN_DMAIER_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	HIFN_DMAIER_R_OVER	0x00020000	/* Result Ring Overflow */
#define	HIFN_DMAIER_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	HIFN_DMAIER_S_DONE	0x00001000	/* Source Ring Done */
#define	HIFN_DMAIER_S_LAST	0x00000800	/* Source Ring Last */
#define	HIFN_DMAIER_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	HIFN_DMAIER_ILLW	0x00000200	/* Illegal write (7811 only) */
#define	HIFN_DMAIER_ILLR	0x00000100	/* Illegal read (7811 only) */
#define	HIFN_DMAIER_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	HIFN_DMAIER_C_DONE	0x00000010	/* Command Ring Done */
#define	HIFN_DMAIER_C_LAST	0x00000008	/* Command Ring Last */
#define	HIFN_DMAIER_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	HIFN_DMAIER_PUBDONE	0x00000002	/* public op done (7951 only) */
#define	HIFN_DMAIER_ENGINE	0x00000001	/* Engine IRQ */

/* DMA Configuration Register (HIFN_1_DMA_CNFG) */
#define	HIFN_DMACNFG_BIGENDIAN	0x10000000	/* big endian mode */
#define	HIFN_DMACNFG_POLLFREQ	0x00ff0000	/* Poll frequency mask */
#define	HIFN_DMACNFG_UNLOCK	0x00000800
#define	HIFN_DMACNFG_POLLINVAL	0x00000700	/* Invalid Poll Scalar */
#define	HIFN_DMACNFG_LAST	0x00000010	/* Host control LAST bit */
#define	HIFN_DMACNFG_MODE	0x00000004	/* DMA mode */
#define	HIFN_DMACNFG_DMARESET	0x00000002	/* DMA Reset # */
#define	HIFN_DMACNFG_MSTRESET	0x00000001	/* Master Reset # */

/* 7811 RNG Enable Register (HIFN_1_7811_RNGENA) */
#define	HIFN_7811_RNGENA_ENA	0x00000001	/* enable RNG */

/* 7811 RNG Config Register (HIFN_1_7811_RNGCFG) */
#define	HIFN_7811_RNGCFG_PRE1	0x00000f00	/* first prescalar */
#define	HIFN_7811_RNGCFG_OPRE	0x00000080	/* output prescalar */
#define	HIFN_7811_RNGCFG_DEFL	0x00000f80	/* 2 words/ 1/100 sec */

/* 7811 RNG Status Register (HIFN_1_7811_RNGSTS) */
#define	HIFN_7811_RNGSTS_RDY	0x00004000	/* two numbers in FIFO */
#define	HIFN_7811_RNGSTS_UFL	0x00001000	/* rng underflow */

/* 7811 MIPS Reset Register (HIFN_1_7811_MIPSRST) */
#define	HIFN_MIPSRST_BAR2SIZE	0xffff0000	/* sdram size */
#define	HIFN_MIPSRST_GPRAMINIT	0x00008000	/* gpram can be accessed */
#define	HIFN_MIPSRST_CRAMINIT	0x00004000	/* ctxram can be accessed */
#define	HIFN_MIPSRST_LED2	0x00000400	/* external LED2 */
#define	HIFN_MIPSRST_LED1	0x00000200	/* external LED1 */
#define	HIFN_MIPSRST_LED0	0x00000100	/* external LED0 */
#define	HIFN_MIPSRST_MIPSDIS	0x00000004	/* disable MIPS */
#define	HIFN_MIPSRST_MIPSRST	0x00000002	/* warm reset MIPS */
#define	HIFN_MIPSRST_MIPSCOLD	0x00000001	/* cold reset MIPS */

/* Revision ID */
#define	HIFN_REVID_7811_PB3_2	0x00000002	/* 7811PB3/2 */

/* Public key reset register (HIFN_1_PUB_RESET) */
#define	HIFN_PUBRST_RESET	0x00000001	/* reset public/rng unit */

/* Public base address register (HIFN_1_PUB_BASE) */
#define	HIFN_PUBBASE_ADDR	0x00003fff	/* base address */

/* Public operand length register (HIFN_1_PUB_OPLEN) */
#define	HIFN_PUBOPLEN_MOD_M	0x0000007f	/* modulus length mask */
#define	HIFN_PUBOPLEN_MOD_S	0		/* modulus length shift */
#define	HIFN_PUBOPLEN_EXP_M	0x0003ff80	/* exponent length mask */
#define	HIFN_PUBOPLEN_EXP_S	7		/* exponent length shift */
#define	HIFN_PUBOPLEN_RED_M	0x003c0000	/* reducend length mask */
#define	HIFN_PUBOPLEN_RED_S	18		/* reducend length shift */

/* Public operation register (HIFN_1_PUB_OP) */
#define	HIFN_PUBOP_AOFFSET_M	0x0000007f	/* A offset mask */
#define	HIFN_PUBOP_AOFFSET_S	0		/* A offset shift */
#define	HIFN_PUBOP_BOFFSET_M	0x00000f80	/* B offset mask */
#define	HIFN_PUBOP_BOFFSET_S	7		/* B offset shift */
#define	HIFN_PUBOP_MOFFSET_M	0x0003f000	/* M offset mask */
#define	HIFN_PUBOP_MOFFSET_S	12		/* M offset shift */
#define	HIFN_PUBOP_OP_MASK	0x003c0000	/* Opcode: */
#define	HIFN_PUBOP_OP_NOP	0x00000000	/*  NOP */
#define	HIFN_PUBOP_OP_ADD	0x00040000	/*  ADD */
#define	HIFN_PUBOP_OP_ADDC	0x00080000	/*  ADD w/carry */
#define	HIFN_PUBOP_OP_SUB	0x000c0000	/*  SUB */
#define	HIFN_PUBOP_OP_SUBC	0x00100000	/*  SUB w/carry */
#define	HIFN_PUBOP_OP_MODADD	0x00140000	/*  Modular ADD */
#define	HIFN_PUBOP_OP_MODSUB	0x00180000	/*  Modular SUB */
#define	HIFN_PUBOP_OP_INCA	0x001c0000	/*  INC A */
#define	HIFN_PUBOP_OP_DECA	0x00200000	/*  DEC A */
#define	HIFN_PUBOP_OP_MULT	0x00240000	/*  MULT */
#define	HIFN_PUBOP_OP_MODMULT	0x00280000	/*  Modular MULT */
#define	HIFN_PUBOP_OP_MODRED	0x002c0000	/*  Modular RED */
#define	HIFN_PUBOP_OP_MODEXP	0x00300000	/*  Modular EXP */

/* Public status register (HIFN_1_PUB_STATUS) */
#define	HIFN_PUBSTS_DONE	0x00000001	/* operation done */
#define	HIFN_PUBSTS_CARRY	0x00000002	/* carry */

/* Public interrupt enable register (HIFN_1_PUB_IEN) */
#define	HIFN_PUBIEN_DONE	0x00000001	/* operation done interrupt */

/* Random number generator config register (HIFN_1_RNG_CONFIG) */
#define	HIFN_RNGCFG_ENA		0x00000001	/* enable rng */

/*
 * PLL config register
 */
#define	HIFN_PLL_7956		0x00001d18	/* 7956 PLL config value */

/*********************************************************************
 * Structs for board commands
 *
 *********************************************************************/

/*
 * Structure to help build up the command data structure.
 */
struct hifn_base_command {
	volatile u_int16_t masks;
	volatile u_int16_t session_num;
	volatile u_int16_t total_source_count;
	volatile u_int16_t total_dest_count;
};

#define	HIFN_BASE_CMD_COMP		0x0100	/* enable compression engine */
#define	HIFN_BASE_CMD_PAD		0x0200	/* enable padding engine */
#define	HIFN_BASE_CMD_MAC		0x0400	/* enable MAC engine */
#define	HIFN_BASE_CMD_CRYPT		0x0800	/* enable crypt engine */
#define	HIFN_BASE_CMD_DECODE		0x2000
#define	HIFN_BASE_CMD_SRCLEN_M		0xc000
#define	HIFN_BASE_CMD_SRCLEN_S		14
#define	HIFN_BASE_CMD_DSTLEN_M		0x3000
#define	HIFN_BASE_CMD_DSTLEN_S		12
#define	HIFN_BASE_CMD_LENMASK_HI	0x30000
#define	HIFN_BASE_CMD_LENMASK_LO	0x0ffff

/*
 * Structure to help build up the command data structure.
 */
struct hifn_crypt_command {
	volatile u_int16_t masks;
	volatile u_int16_t header_skip;
	volatile u_int16_t source_count;
	volatile u_int16_t reserved;
};

#define	HIFN_CRYPT_CMD_ALG_MASK		0x0003		/* algorithm: */
#define	HIFN_CRYPT_CMD_ALG_DES		0x0000		/*   DES */
#define	HIFN_CRYPT_CMD_ALG_3DES		0x0001		/*   3DES */
#define	HIFN_CRYPT_CMD_ALG_RC4		0x0002		/*   RC4 */
#define	HIFN_CRYPT_CMD_ALG_AES		0x0003		/*   AES */
#define	HIFN_CRYPT_CMD_MODE_MASK	0x0018		/* Encrypt mode: */
#define	HIFN_CRYPT_CMD_MODE_ECB		0x0000		/*   ECB */
#define	HIFN_CRYPT_CMD_MODE_CBC		0x0008		/*   CBC */
#define	HIFN_CRYPT_CMD_MODE_CFB		0x0010		/*   CFB */
#define	HIFN_CRYPT_CMD_MODE_OFB		0x0018		/*   OFB */
#define	HIFN_CRYPT_CMD_CLR_CTX		0x0040		/* clear context */
#define	HIFN_CRYPT_CMD_NEW_KEY		0x0800		/* expect new key */
#define	HIFN_CRYPT_CMD_NEW_IV		0x1000		/* expect new iv */

#define	HIFN_CRYPT_CMD_SRCLEN_M		0xc000
#define	HIFN_CRYPT_CMD_SRCLEN_S		14


#define	HIFN_CRYPT_CMD_KSZ_MASK		0x0600		/* AES key size: */
#define	HIFN_CRYPT_CMD_KSZ_128		0x0000		/*   128 bit */
#define	HIFN_CRYPT_CMD_KSZ_192		0x0200		/*   192 bit */
#define	HIFN_CRYPT_CMD_KSZ_256		0x0400		/*   256 bit */

/*
 * Structure to help build up the command data structure.
 */
struct hifn_mac_command {
	volatile u_int16_t masks;
	volatile u_int16_t header_skip;
	volatile u_int16_t source_count;
	volatile u_int16_t reserved;
};

#define	HIFN_MAC_CMD_ALG_MASK		0x0001
#define	HIFN_MAC_CMD_ALG_SHA1		0x0000
#define	HIFN_MAC_CMD_ALG_MD5		0x0001
#define	HIFN_MAC_CMD_MODE_MASK		0x000c
#define	HIFN_MAC_CMD_MODE_HMAC		0x0000
#define	HIFN_MAC_CMD_MODE_SSL_MAC	0x0004
#define	HIFN_MAC_CMD_MODE_HASH		0x0008
#define	HIFN_MAC_CMD_MODE_FULL		0x0004
#define	HIFN_MAC_CMD_TRUNC		0x0010
#define	HIFN_MAC_CMD_RESULT		0x0020
#define	HIFN_MAC_CMD_APPEND		0x0040
#define	HIFN_MAC_CMD_SRCLEN_M		0xc000
#define	HIFN_MAC_CMD_SRCLEN_S		14

/*
 * MAC POS IPsec initiates authentication after encryption on encodes
 * and before decryption on decodes.
 */
#define	HIFN_MAC_CMD_POS_IPSEC		0x0200
#define	HIFN_MAC_CMD_NEW_KEY		0x0800

struct hifn_comp_command {
	volatile u_int16_t masks;
	volatile u_int16_t header_skip;
	volatile u_int16_t source_count;
	volatile u_int16_t reserved;
};

#define	HIFN_COMP_CMD_SRCLEN_M		0xc000
#define	HIFN_COMP_CMD_SRCLEN_S		14
#define	HIFN_COMP_CMD_ONE		0x0100	/* must be one */
#define	HIFN_COMP_CMD_CLEARHIST		0x0010	/* clear history */
#define	HIFN_COMP_CMD_UPDATEHIST	0x0008	/* update history */
#define	HIFN_COMP_CMD_LZS_STRIP0	0x0004	/* LZS: strip zero */
#define	HIFN_COMP_CMD_MPPC_RESTART	0x0004	/* MPPC: restart */
#define	HIFN_COMP_CMD_ALG_MASK		0x0001	/* compression mode: */
#define	HIFN_COMP_CMD_ALG_MPPC		0x0001	/*   MPPC */
#define	HIFN_COMP_CMD_ALG_LZS		0x0000	/*   LZS */

struct hifn_base_result {
	volatile u_int16_t flags;
	volatile u_int16_t session;
	volatile u_int16_t src_cnt;		/* 15:0 of source count */
	volatile u_int16_t dst_cnt;		/* 15:0 of dest count */
};

#define	HIFN_BASE_RES_DSTOVERRUN	0x0200	/* destination overrun */
#define	HIFN_BASE_RES_SRCLEN_M		0xc000	/* 17:16 of source count */
#define	HIFN_BASE_RES_SRCLEN_S		14
#define	HIFN_BASE_RES_DSTLEN_M		0x3000	/* 17:16 of dest count */
#define	HIFN_BASE_RES_DSTLEN_S		12

struct hifn_comp_result {
	volatile u_int16_t flags;
	volatile u_int16_t crc;
};

#define	HIFN_COMP_RES_LCB_M		0xff00	/* longitudinal check byte */
#define	HIFN_COMP_RES_LCB_S		8
#define	HIFN_COMP_RES_RESTART		0x0004	/* MPPC: restart */
#define	HIFN_COMP_RES_ENDMARKER		0x0002	/* LZS: end marker seen */
#define	HIFN_COMP_RES_SRC_NOTZERO	0x0001	/* source expired */

struct hifn_mac_result {
	volatile u_int16_t flags;
	volatile u_int16_t reserved;
	/* followed by 0, 6, 8, or 10 u_int16_t's of the MAC, then crypt */
};

#define	HIFN_MAC_RES_MISCOMPARE		0x0002	/* compare failed */
#define	HIFN_MAC_RES_SRC_NOTZERO	0x0001	/* source expired */

struct hifn_crypt_result {
	volatile u_int16_t flags;
	volatile u_int16_t reserved;
};

#define	HIFN_CRYPT_RES_SRC_NOTZERO	0x0001	/* source expired */

/*
 * The poll frequency and poll scalar defines are unshifted values used
 * to set fields in the DMA Configuration Register.
 */
#ifndef HIFN_POLL_FREQUENCY
#define HIFN_POLL_FREQUENCY	0x1
#endif

#ifndef HIFN_POLL_SCALAR
#define HIFN_POLL_SCALAR	0x0
#endif

#define	HIFN_MAX_SEGLEN 	0xffff		/* maximum dma segment len */
#define	HIFN_MAX_DMALEN		0x3ffff		/* maximum dma length */
#endif /* __DEV_PCI_HIFN7751REG_H__ */
