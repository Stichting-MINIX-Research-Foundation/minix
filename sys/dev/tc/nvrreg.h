/* 	$NetBSD: nvrreg.h,v 1.4 2005/12/11 12:24:00 christos Exp $ 	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 */

#ifndef _TC_NVRREG_H_
#define _TC_NVRREG_H_ 1

/* Offsets within PrestoServe's slot */
#define	NVR_CSR 	0x00100000	/* Control and status register */
#define	NVR_DAR        	0x00100004	/* DMA address register */
#define NVR_BAT		0x00100008	/* Battery control register */
#define NVR_MAR		0x0010000C	/* ? */
#define NVR_BCR		0x00100010	/* DMA burst count register */

/* CSR register bit definitions (R/W) */
#define	NVR_CSR_DMA_GO		0x00000001
#define NVR_CSR_ANTIHOG		0x00000002
#define NVR_CSR_DMA_DONE	0x00000080
#define NVR_CSR_BURST_128_BYTES	0x00000800
#define NVR_CSR_ENBL_PARITY	0x00001000
#define NVR_CSR_ENBL_BAT_INT	0x00002000
#define NVR_CSR_ENBL_DONE_INT	0x00004000
#define NVR_CSR_ENBL_ERR_INT	0x00008000

/* CSR register bit definitions (R) */
#define	NVR_CSR_BAT_DISCONNECT	0x00100000
#define NVR_CSR_BAT_FAIL	0x00400000
#define NVR_CSR_TC_ERROR	0x01000000
#define	NVR_CSR_MEMORY_ERROR	0x02000000
#define NVR_CSR_TC_PARITY_ERR	0x04000000
#define NVR_CSR_TC_PROTCAL_ERR	0x08000000
#define NVR_CSR_ERROR_SUM	0x80000000

#endif	/* _TC_NVRREG_H_ */
