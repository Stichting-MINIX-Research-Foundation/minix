/* $NetBSD: atppcreg.h,v 1.5 2005/12/11 12:21:25 christos Exp $ */

/*-
 * Copyright (c) 2001 Alcove - Nicolas Souchu
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/isa/ppcreg.h,v 1.10.2.4 2001/10/02 05:21:45 nsouch Exp
 *
 */

#ifndef __ATPPCREG_H
#define __ATPPCREG_H

/* Generic register definitions as offsets from a base address */

#define ATPPC_SPP_DTR 		0	/* SPP data register */
#define ATPPC_ECP_A_FIFO	0	/* ECP Address fifo register */
#define ATPPC_SPP_STR		1	/* SPP status register */
#define ATPPC_SPP_CTR		2	/* SPP control register */
#define ATPPC_EPP_ADDR		3	/* EPP address register (8 bit) */
#define ATPPC_EPP_DATA		4	/* EPP data register (8, 16, 32 bit) */
#define ATPPC_ECP_D_FIFO	0x400	/* ECP Data fifo register */
#define ATPPC_ECP_CNFGA		0x400	/* Configuration register A */
#define ATPPC_ECP_CNFGB		0x401	/* Configuration register B */
#define ATPPC_ECP_ECR		0x402	/* ECP extended control register */

/* ECP control register commands/modes */
#define ATPPC_FIFO_EMPTY	0x1	/* ecr register - bit 0 */
#define ATPPC_FIFO_FULL		0x2	/* ecr register - bit 1 */
#define ATPPC_SERVICE_INTR 	0x4	/* ecr register - bit 2 */
#define ATPPC_ENABLE_DMA	0x8	/* ecr register - bit 3 */
#define ATPPC_nFAULT_INTR	0x10	/* ecr register - bit 4 */
/* ecr register - bits 5 through 7 */
#define ATPPC_ECR_STD		0x00	/* Standard mode */
#define ATPPC_ECR_PS2		0x20	/* Bidirectional mode */
#define ATPPC_ECR_FIFO		0x40	/* Fast Centronics mode */
#define ATPPC_ECR_ECP		0x60	/* ECP mode */
#define ATPPC_ECR_EPP		0x80	/* EPP mode */
#define ATPPC_ECR_TST		0xd0	/* Test mode*/
#define ATPPC_ECR_CFG		0xe0	/* Test mode*/



/* To set "inverted" flags, do AND. Otherwise, do OR */
/* 0 & x = 0, 1 | x = 1 */

/* Clear flags: n(var) is equivalent to var = 0.
#define n(flags) (~(flags) & (flags))*/

/* Invert flags
#define inv(flags) (~(flags))*/

/* SPP mode control register bit positions. */
#define STROBE		0x01
#define AUTOFEED	0x02
#define nINIT		0x04
#define SELECTIN	0x08
#define IRQENABLE	0x10
#define PCD             0x20

/*
#define nSTROBE		inv(STROBE)
#define nAUTOFEED	inv(AUTOFEED)
#define INIT		inv(nINIT)
#define nSELECTIN	inv(SELECTIN)
#define nPCD		inv(PCD)
*/

/* SPP status register bit positions. */
#define TIMEOUT         0x01
#define nFAULT          0x08
#define SELECT          0x10
#define PERROR          0x20
#define nACK            0x40
#define nBUSY           0x80

/* Flags indicating ready condition */
#define SPP_READY (SELECT | nFAULT | nBUSY)
#define SPP_MASK (SELECT | nFAULT | PERROR | nBUSY)

/* Byte mode signals */
#define HOSTCLK		STROBE /* Also ECP mode signal */
#define HOSTBUSY	AUTOFEED
#define ACTIVE1284	SELECTIN /* Also ECP mode signal */
#define PTRCLK		nACK
#define PTRBUSY		nBUSY
#define ACKDATAREQ	PERROR
#define XFLAG		SELECT /* Also ECP mode signal */
#define nDATAVAIL	nFAULT

/* ECP mode signals */
#define HOSTACK		AUTOFEED
#define nREVREQ		nINIT
#define PERICLK		nACK
#define PERIACK		nBUSY
#define nACKREV		PERROR
#define nPERIREQ	nFAULT

/* EPP mode signals */
#define nWRITE		STROBE
#define nDATASTB	AUTOFEED
#define nADDRSTB	SELECTIN
#define nWAIT		nBUSY
#define nRESET		nINIT
#define nINTR		nACK


/*
 * Useful macros for reading/writing registers.
 */

/* Reading macros */
#define atppc_r_dtr(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh, \
	ATPPC_SPP_DTR)
#define atppc_r_str(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh, \
	ATPPC_SPP_STR)
#define atppc_r_ctr(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh, \
	ATPPC_SPP_CTR)
#define atppc_r_eppA(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh,\
	ATPPC_EPP_ADDR)
#define atppc_r_eppD(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh,\
	ATPPC_EPP_DATA)
#define atppc_r_eppD_multi(atppc, buf, count) bus_space_read_multi_1( \
	(atppc)->sc_iot, (atppc)->sc_ioh, ATPPC_EPP_DATA, (buf), (count))
#define atppc_r_cnfgA(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh,\
	ATPPC_ECP_CNFGA)
#define atppc_r_cnfgB(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh,\
	ATPPC_ECP_CNFGB)
#define atppc_r_ecr(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh, \
	ATPPC_ECP_ECR)
#define atppc_r_fifo(atppc) bus_space_read_1((atppc)->sc_iot, (atppc)->sc_ioh, \
	ATPPC_ECP_D_FIFO)
#define atppc_r_fifo_multi(atppc, buf, count) bus_space_read_multi_1( \
	(atppc)->sc_iot, (atppc)->sc_ioh, ATPPC_ECP_D_FIFO, (buf), (count))

/* Writing macros */
#define atppc_w_dtr(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_SPP_DTR, (byte))
#define atppc_w_str(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_SPP_STR, (byte))
#define atppc_w_ctr(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_SPP_CTR, (byte))
#define atppc_w_eppA(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_EPP_ADDR, (byte))
#define atppc_w_eppD(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_EPP_DATA, (byte))
#define atppc_w_eppD_multi(atppc, buf, count) bus_space_write_multi_1( \
	(atppc)->sc_iot, (atppc)->sc_ioh, ATPPC_EPP_DATA, (buf), (count))
#define atppc_w_cnfgA(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_ECP_CNFGA, (byte))
#define atppc_w_cnfgB(atppc, byte) bus_space_read_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_ECP_CNFGB, (byte))
#define atppc_w_ecr(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_ECP_ECR, (byte))
#define atppc_w_fifo(atppc, byte) bus_space_write_1((atppc)->sc_iot, \
	(atppc)->sc_ioh, ATPPC_ECP_D_FIFO, (byte))
#define atppc_w_fifo_multi(atppc, buf, count) bus_space_write_multi_1( \
	(atppc)->sc_iot, (atppc)->sc_ioh, ATPPC_ECP_D_FIFO, (buf), (count))

/* Barrier macros for reads/writes */
#define atppc_barrier_r(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_READ)
#define atppc_barrier_w(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_WRITE)
#define atppc_barrier(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_WRITE | \
	BUS_SPACE_BARRIER_READ)

/* These are defined in man pages but don't actually exist for all acrhs */
#define atppc_barrier_rr(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_READ_BEFORE_READ)
#define atppc_barrier_rw(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_READ_BEFORE_WRITE)
#define atppc_barrier_rb(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_READ_BEFORE_READ | \
	BUS_SPACE_BARRIER_READ_BEFORE_WRITE)
#define atppc_barrier_wr(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_WRITE_BEFORE_READ)
#define atppc_barrier_ww(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_WRITE_BEFORE_WRITE)
#define atppc_barrier_wb(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_WRITE_BEFORE_READ | \
	BUS_SPACE_BARRIER_WRITE_BEFORE_WRITE)
#define atppc_barrier_sync(atppc) bus_space_barrier((atppc)->sc_iot, \
	(atppc)->sc_ioh, 0, IO_LPTSIZE, BUS_SPACE_BARRIER_SYNC)

/*
 * Register defines for the PC873xx parts
 */

#define PC873_FER	0x00
#define PC873_PPENABLE	(1<<0)
#define PC873_FAR	0x01
#define PC873_PTR	0x02
#define PC873_CFGLOCK	(1<<6)
#define PC873_EPPRDIR	(1<<7)
#define PC873_EXTENDED	(1<<7)
#define PC873_LPTBIRQ7	(1<<3)
#define PC873_FCR	0x03
#define PC873_ZWS	(1<<5)
#define PC873_ZWSPWDN	(1<<6)
#define PC873_PCR	0x04
#define PC873_EPPEN	(1<<0)
#define PC873_EPP19	(1<<1)
#define PC873_ECPEN	(1<<2)
#define PC873_ECPCLK	(1<<3)
#define PC873_PMC	0x06
#define PC873_TUP	0x07
#define PC873_SID	0x08
#define PC873_PNP0	0x1b
#define PC873_PNP1	0x1c
#define PC873_LPTBA	0x19

/*
 * Register defines for the SMC FDC37C66xGT parts
 */

/* Init codes */
#define SMC665_iCODE	0x55
#define SMC666_iCODE	0x44

/* Base configuration ports */
#define SMC66x_CSR	0x3F0
#define SMC666_CSR	0x370		/* hard-configured value for 666 */

/* Bits */
#define SMC_CR1_ADDR	0x3		/* bit 0 and 1 */
#define SMC_CR1_MODE	(1<<3)		/* bit 3 */
#define SMC_CR4_EMODE	0x3		/* bits 0 and 1 */
#define SMC_CR4_EPPTYPE	(1<<6)		/* bit 6 */

/* Extended modes */
#define SMC_SPP		0x0		/* SPP */
#define SMC_EPPSPP	0x1		/* EPP and SPP */
#define SMC_ECP		0x2 		/* ECP */
#define SMC_ECPEPP	0x3		/* ECP and EPP */

/*
 * Register defines for the SMC FDC37C935 parts
 */

/* Configuration ports */
#define SMC935_CFG	0x370
#define SMC935_IND	0x370
#define SMC935_DAT	0x371

/* Registers */
#define SMC935_LOGDEV	0x7
#define SMC935_ID	0x20
#define SMC935_PORTHI	0x60
#define SMC935_PORTLO	0x61
#define SMC935_PPMODE	0xf0

/* Parallel port modes */
#define SMC935_SPP	0x38 + 0
#define SMC935_EPP19SPP	0x38 + 1
#define SMC935_ECP	0x38 + 2
#define SMC935_ECPEPP19	0x38 + 3
#define SMC935_CENT	0x38 + 4
#define SMC935_EPP17SPP	0x38 + 5
#define SMC935_UNUSED	0x38 + 6
#define SMC935_ECPEPP17	0x38 + 7

/*
 * Register defines for the Winbond W83877F parts
 */

#define WINB_W83877F_ID		0xa
#define WINB_W83877AF_ID	0xb

/* Configuration bits */
#define WINB_HEFERE	(1<<5)		/* CROC bit 5 */
#define WINB_HEFRAS	(1<<0)		/* CR16 bit 0 */

#define WINB_PNPCVS	(1<<2)		/* CR16 bit 2 */
#define WINB_CHIPID	0xf		/* CR9 bits 0-3 */

#define WINB_PRTMODS0	(1<<2)		/* CR0 bit 2 */
#define WINB_PRTMODS1	(1<<3)		/* CR0 bit 3 */
#define WINB_PRTMODS2	(1<<7)		/* CR9 bit 7 */

/* W83877F modes: CR9/bit7 | CR0/bit3 | CR0/bit2 */
#define WINB_W83757	0x0
#define WINB_EXTFDC	0x4
#define WINB_EXTADP	0x8
#define WINB_EXT2FDD	0xc
#define WINB_JOYSTICK	0x80

#define WINB_PARALLEL	0x80
#define WINB_EPP_SPP	0x4
#define WINB_ECP	0x8
#define WINB_ECP_EPP	0xc

#endif /* __ATPPCREG_H */
