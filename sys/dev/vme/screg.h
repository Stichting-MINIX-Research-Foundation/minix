/*	$NetBSD: screg.h,v 1.2 2008/04/28 20:24:01 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
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
 * Register map for the Sun2 SCSI Interface (sc)
 */

#if __for_reference_only__
struct sunsc_regs {
	u_char	sunsc_data;	/* data register */
	u_char	sunsc_unused0;
	u_char	sunsc_cmd_stat;	/* command/status register */
	u_char	sunsc_unused1;
	u_short	sunsc_icr;	/* interface control register */
	u_short	sunsc_unused2;
	u_short	sunsc_dma_addr_h;	/* DMA address, high 16 */
	u_short	sunsc_dma_addr_l;	/* DMA address, low 16 */
	u_short	sunsc_dma_count;/* DMA count */
	u_char	sunsc_unused3;
	u_char	sunsc_intvec;	/* interrupt vector (VME only) */
};
#endif

/* Register offsets. */
#define	SCREG_DATA		(0)
#define	SCREG_CMD_STAT		(2)
#define	SCREG_ICR		(4)
#define	SCREG_DMA_ADDR_H	(8)
#define	SCREG_DMA_ADDR_L	(10)
#define	SCREG_DMA_COUNT		(12)
#define	SCREG_INTVEC		(15)
#define	SCREG_BANK_SZ		(16)

