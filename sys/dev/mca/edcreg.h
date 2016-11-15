/*	$NetBSD: edcreg.h,v 1.5 2008/05/04 13:11:14 martin Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * Driver for MCA ESDI controllers and disks.
 */

#define	ESDIC_IOPRM		0x3510
#define ESDIC_IOALT		0x3518
#define ESDIC_REG_NPORTS	8
#define ESDIC_IRQ		14		/* this is fixed */

/* pos2 */
#define IO_IS_ALT		0x02
#define DRQ_MASK		0x3c
#define	FAIRNESS_ENABLE		0x40

/* pos3 */
#define PACING_INT_MASK		0x30

/* pos4 */
#define	PACING_CTRL_DISABLE	0x01
#define RELEASE_2		0x02	/* lower bit of Time to Release */
#define RELEASE_1		0x04	/* higher bit of Time to Release */

/* controller registers */
#define SIFR			0	/* read Status Interface Register,
					   2 bytes, little endian */
#define SIFR_CMD_MASK		0x2f

#define CIFR			0	/* write - Command Interface Reg,
					   2 bytes, little endian */
#define CIFR_LONG_CMD		(1<<14) /* 4 word command */

/* Command Codes */
#define CMD_READ_DATA		0x01	/* uses DMA */
#define CMD_WRITE_DATA		0x02	/* uses DMA */
#define CMD_READ_VERIFY		0x03
#define CMD_WRITE_VERIFY	0x04	/* uses DMA */
#define CMD_SEEK		0x05
#define CMD_PARK_HEAD		0x06
#define CMD_GET_CMD_COMP_STATUS	0x07
#define CMD_GET_DEV_STATUS	0x08
#define CMD_GET_DEV_CONF	0x09
#define CMD_GET_POS_INFO	0x0A
#define CMD_TRANSLATE_RBA	0x0B
#define CMD_WRITE_ATTACH_BUFF	0x10	/* uses DMA */
#define CMD_READ_ATTACH_BUFF	0x11	/* uses DMA */
#define CMD_RUN_DIAG_TEST	0x12
#define CMD_GET_DIAG_STAT_BLOCK	0x14
#define CMD_GET_MFG_HEADER	0x15	/* uses DMA */
#define CMD_FORMAT_UNIT		0x16	/* uses DMA */
#define CMD_FORMAT_PREPARE	0x17
#define	CMD_SET_MAX_RBA		0x1A
#define CMD_SET_PWR_SAV_MODE	0x1B	/* optional */
#define CMD_POWER_CONS_CMD	0x1C	/* optional */

#define BCR			2	/* write */
#define BCR_INT_ENABLE		0x01
#define BCR_DMA_ENABLE		0x02
#define BCR_RESET		0x80

#define BSR			2	/* read */
#define BSR_DMA_ENABLED		0x80
#define BSR_INT_PENDING		0x40
#define BSR_CMD_INPROGRESS	0x20
#define BSR_BUSY		0x10
#define BSR_SIFR_FULL		0x08	/* also called STATUS OUT */
#define BSR_CIFR_FULL		0x04
#define BSR_TRANSFER_REQ	0x02
#define BSR_INTR		0x01

#define ISR			3	/* read, Interrupt Status Register */
#define ISR_DEV_SELECT_MASK	0xE0
#define ISR_ATTACH_ERR		0x10
#define ISR_INTR_ID_MASK	0x0F
#define ISR_COMPLETED		0x01
#define ISR_COMPLETED_WITH_ECC	0x03
#define ISR_COMPLETED_RETRIES	0x05
#define ISR_PARTIAL_FORMAT	0x06	/* Status available */
#define ISR_COMPLETED_WARNING	0x08
#define ISR_ABORT_COMPLETED	0x09
#define ISR_RESET_COMPLETED	0x0A
#define ISR_DATA_TRANSFER_RDY	0x0B	/* No Status Block */
#define ISR_CMD_FAILED		0x0C
#define ISR_DMA_ERROR		0x0D
#define ISR_CMD_BLOCK_ERROR	0x0E
#define ISR_ATTN_ERROR		0x0F

/* Macros to get info from command status block */
#define SB_GET_CMD_STATUS(sb)	(((sb)[1] & 0xff00) >> 8)
#define SB_RESBLKCNT_IDX	3

#define ATN			3	/* write, Attention register */
#define ATN_CMD_REQ		1
#define ATN_END_INT		2	/* End of Interrupt (EOI) */
#define ATN_ABORT_CMD		3
#define ATN_RESET_ATTACHMENT	4

#define DASD_DEVNO_CONTROLLER	7	/* Device number for controller */
