/*	$NetBSD: sireg.h,v 1.3 2008/04/28 20:24:01 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Register map for the VME SCSI-3 adpater (si)
 * The first part of this register map is an NCR5380
 * SCSI Bus Interface Controller (SBIC).  The rest is a
 * DMA controller and custom logic.
 */


#if __for_reference_only__
/*
 * Am5380 Register map (no padding). See dev/ic/ncr5380reg.h
 */
struct ncr5380regs {
	u_char r[8];
};

struct si_regs {
	struct ncr5380regs sci;

	/* DMA controller registers */
	u_short	dma_addrh;	/* DMA address (VME only) */
	u_short	dma_addrl;	/* (high word, low word)  */
	u_short	dma_counth;	/* DMA count   (VME only) */
	u_short	dma_countl;	/* (high word, low word)  */

	u_int	pad0;		/* no-existent register */

	u_short	fifo_data;	/* fifo data register */
	u_short	fifo_count;	/* fifo count register */
	u_short	si_csr;		/* si control/status */
	u_short	bprh;		/* VME byte pack high */
	u_short	bprl;		/* VME byte pack low */
	u_short	iv_am;		/* bits 0-7: intr vector */
				/* bits 8-13: addr modifier (VME only) */
				/* bits 14-15: unused */
	u_short	fifo_cnt_hi;	/* high part of fifo_count (VME only) */

	/* Whole thing repeats after 32 bytes. */
	u_short	_space[3];
};
#endif

/*
 * Size of NCR5380 registers located at the bottom of the register bank
 */
#define NCR5380REGS_SZ	8

/*
 * Register definition for the `si' VME controller
 */
#define SIREG_DMA_ADDRH	(NCR5380REGS_SZ + 0)	/* DMA address, high word */
#define SIREG_DMA_ADDRL	(NCR5380REGS_SZ + 2)	/* DMA address, low word */
#define SIREG_DMA_CNTH	(NCR5380REGS_SZ + 4)	/* DMA count, high word */
#define SIREG_DMA_CNTL	(NCR5380REGS_SZ + 6)	/* DMA count, low word */
#define SIREG_FIFO_DATA	(NCR5380REGS_SZ + 12)	/* FIFO data */
#define SIREG_FIFO_CNT	(NCR5380REGS_SZ + 14)	/* FIFO count, low word */
#define SIREG_CSR	(NCR5380REGS_SZ + 16)	/* Control/status register */
#define SIREG_BPRH	(NCR5380REGS_SZ + 18)	/* VME byte pack, high word */
#define SIREG_BPRL	(NCR5380REGS_SZ + 20)	/* VME byte pack, low word */
#define SIREG_IV_AM	(NCR5380REGS_SZ + 22)	/* bits 0-7: intr vector;
						   bits 8-13: addr modifier */
#define SIREG_FIFO_CNTH	(NCR5380REGS_SZ + 24)	/* FIFO count, high word */
#define SIREG_BANK_SZ	(NCR5380REGS_SZ + 26)

/*
 * Status Register.
 * Note:
 *	(r)	indicates bit is read only.
 *	(rw)	indicates bit is read or write.
 *	(v)	vme host adaptor interface only.
 *	(o)	sun3/50 onboard host adaptor interface only.
 *	(b)	both vme and sun3/50 host adaptor interfaces.
 *
 * Note 2: because of the historical connections of this VME driver
 * with the on-board SCSI interfaces found in sun3/50, sun3/60 and sun4/100
 * systems, the (v), (o) and (b) qualifications are left in for
 * cross-reference.
 */
#define SI_CSR_DMA_ACTIVE	0x8000	/* (r,o) DMA transfer active */
#define SI_CSR_DMA_CONFLICT	0x4000	/* (r,b) reg accessed while DMA'ing */
#define SI_CSR_DMA_BUS_ERR	0x2000	/* (r,b) bus error during DMA */
#define SI_CSR_ID		0x1000	/* (r,b) 0 for 3/50, 1 for SCSI-3, */
					/* 0 if SCSI-3 unmodified */
#define SI_CSR_FIFO_FULL	0x0800	/* (r,b) fifo full */
#define SI_CSR_FIFO_EMPTY	0x0400	/* (r,b) fifo empty */
#define SI_CSR_SBC_IP		0x0200	/* (r,b) sbc interrupt pending */
#define SI_CSR_DMA_IP		0x0100	/* (r,b) DMA interrupt pending */
#define SI_CSR_LOB		0x00c0	/* (r,v) number of leftover bytes */
#define   SI_CSR_LOB_THREE	0x00c0	/* (r,v) three leftover bytes */
#define   SI_CSR_LOB_TWO	0x0080	/* (r,v) two leftover bytes */
#define   SI_CSR_LOB_ONE	0x0040	/* (r,v) one leftover byte */
#define SI_CSR_BPCON		0x0020	/* (rw,v) byte packing control */
					/* DMA is in 0=longwords, 1=words */
#define SI_CSR_DMA_EN		0x0010	/* (rw,v) DMA/interrupt enable */
#define SI_CSR_SEND		0x0008	/* (rw,b) DMA dir, 1=to device */
#define SI_CSR_INTR_EN		0x0004	/* (rw,b) interrupts enable */
#define SI_CSR_FIFO_RES		0x0002	/* (rw,b) inits fifo, 0=reset */
#define SI_CSR_SCSI_RES		0x0001	/* (rw,b) reset sbc and udc, 0=reset */
