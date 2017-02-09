/*	$NetBSD: bppreg.h,v 1.2 2008/04/28 20:23:57 martin Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

/* Hardware Configuration Register */
#define BPP_HCR_DSS_MASK	0x003f	/* Data before strobe */
#define BPP_HCR_DSS_SHFT	0	/*   (in Sbus clocks)*/
#define BPP_HCR_DSW_MASK	0x7f00	/* Data Strobe Width */
#define BPP_HCR_DSW_SHFT	8	/*   (in Sbus clocks)*/
#define BPP_HCR_TEST		0x8000	/* */
#define BPP_HCR_BITS		"\177\020" \
				"f\0\6DSS\0f\10\7DSW\0b\17TEST\0"


/* Operation Configuration Register */
#define BPP_OCR_IDLE		0x0008	/* State machines are idle */
#define BPP_OCR_SRST		0x0080	/* Reset bit */
#define BPP_OCR_ACK_OP		0x0100	/* ACK handshake operation */
#define BPP_OCR_BUSY_OP		0x0200	/* BUSY handshake operation */
#define BPP_OCR_EN_DIAG		0x0400	/* */
#define BPP_OCR_ACK_DSEL	0x0800	/* ack line is bidirectional */
#define BPP_OCR_BUSY_DSEL	0x1000	/* busy line is bidirectional */
#define BPP_OCR_DS_DSEL		0x2000	/* data strobe line is bidirectional */
#define BPP_OCR_DATA_SRC	0x4000	/* Data source for `memory clear' */
#define BPP_OCR_MEM_SRC		0x8000	/* Enable `memory clear' */
#define BPP_OCR_BITS		"\177\020" \
				"b\3IDLE\0b\7SRST\0b\10ACK_OP\0b\11BUSY_OP\0" \
				"b\12EN_DIAG\0b\13ACK_DSEL\0b\14BUSY_DSEL\0" \
				"b\15DS_DSEL\0b\16DATA_SRC\0b\17MEM_SRC\0"
/* User settable bits */
#define BPP_OCR_USER	\
	(BPP_OCR_ACK_OP|BPP_OCR_BUSY_OP|BPP_OCR_ACK_DSEL|\
	 BPP_OCR_BUSY_DSEL|BPP_OCR_DS_DSEL)

/* Transfer Control Register */
#define BPP_TCR_DS		0x01	/* Data Strobe */
#define BPP_TCR_ACK		0x02	/* Acknowledge */
#define BPP_TCR_BUSY		0x04	/* Busy */
#define BPP_TCR_DIR		0x08	/* Direction control */
#define BPP_TCR_BITS		"\177\020" \
				"b\0DS\0b\1ACK\0b\2BUSY\0b\3DIR\0"
#define BPP_TCR_USER		(BPP_TCR_DS|BPP_TCR_ACK|BPP_TCR_BUSY)

/* Output Register */
#define BPP_OR_SLCTIN		0x01	/* Select */
#define BPP_OR_AFXN		0x02	/* Auto Feed */
#define BPP_OR_INIT		0x04	/* Initialize */
#define BPP_OR_BITS		"\177\020" \
				"b\0SLCTIN\0b\1AFXN\0b\2INIT\0"
#define BPP_OR_USER		(BPP_OR_SLCTIN|BPP_OR_AFXN)

/* Input Register (read-only) */
#define BPP_IR_ERR		0x01	/* Err input pin */
#define BPP_IR_SLCT		0x02	/* Select input pin */
#define BPP_IR_PE		0x04	/* Paper Out input pin */
#define BPP_IR_BITS		"\177\020" \
				"b\0ERR\0b\1SLCT\0b\2PE\0"

/* Interrupt Control Register */
#define BPP_ERR_IRQ_EN		0x0001	/* Error interrupt enable */
#define BPP_ERR_IRP		0x0002	/* ERR interrupt polarity */
#define BPP_SLCT_IRQ_EN		0x0004	/* Select interrupt enable */
#define BPP_SLCT_IRP		0x0008	/* Select interrupt polarity */
#define BPP_PE_IRQ_EN		0x0010	/* Paper Empty interrupt enable */
#define BPP_PE_IRP		0x0020	/* PE interrupt polarity */
#define BPP_BUSY_IRQ_EN		0x0040	/* BUSY interrupt enable */
#define BPP_BUSY_IRP		0x0080	/* BUSY interrupt polarity */
#define BPP_ACK_IRQ_EN		0x0100	/* ACK interrupt enable */
#define BPP_DS_IRQ_EN		0x0200	/* Data Strobe interrupt enable */
#define BPP_ERR_IRQ		0x0400	/* ERR interrupt pending */
#define BPP_SLCT_IRQ		0x0800	/* SLCT interrupt pending */
#define BPP_PE_IRQ		0x1000	/* PE interrupt pending */
#define BPP_BUSY_IRQ		0x2000	/* BUSY interrupt pending */
#define BPP_ACK_IRQ		0x4000	/* ACK interrupt pending */
#define BPP_DS_IRQ		0x8000	/* DS interrupt pending */

/* Define mask for each of all irq request, all polarity and all enable bits */
#define BPP_ALLIRQ	(BPP_ERR_IRQ|BPP_SLCT_IRQ|BPP_PE_IRQ|	\
			 BPP_BUSY_IRQ|BPP_ACK_IRQ|BPP_DS_IRQ)
#define BPP_ALLEN	(BPP_ERR_IRQ_EN|BPP_SLCT_IRQ_EN|	\
			 BPP_PE_IRQ_EN|BPP_BUSY_IRQ_EN|		\
			 BPP_ACK_IRQ_EN|BPP_DS_IRQ_EN)
#define BPP_ALLIRP	(BPP_ERR_IRP|BPP_PE_IRP|BPP_BUSY_IRP)
#define BPP_IRQ_USER	BPP_ALLIRP

#define BPP_IRQ_BITS	"\177\020"					\
			"b\0ERR_IRQ_EN\0b\1ERR_IRP\0b\2SLCT_IRQ_EN\0"	\
			"b\3SLCT_IRP\0b\4PE_IRQ_EN\0b\5PE_IRP\0"	\
			"b\6BUSY_IRQ_EN\0b\7BUSY_IRP\0b\10ACK_IRQ_EN\0"	\
			"b\11DS_IRQ_EN\0b\12ERR_IRQ\0b\13SLCT_IRQ\0"	\
			"b\14PE_IRQ\0b\15BUSY_IRQ\0b\16ACK_IRQ\0"	\
			"b\17DS_IRQ\0"
