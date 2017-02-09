/*	$NetBSD: sunscpalreg.h,v 1.3 2008/04/28 20:23:51 martin Exp $	*/

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
 * Macros for accessing the registers in the Sun "Sun-2" "sc"
 * PAL+logic sequencer chips.
 */

/*
 * Interface Control Register.
 *
 * This register pretty much just reflects the state of the SCSI
 * bus, directly.  For example, the _COMMAND_DATA, _MESSAGE,
 * and _INPUT_OUTPUT bits and their values correspond directly
 * to SCSI bus signals.
 *
 * Note:
 *	(r)	indicates bit is read only.
 *	(rw)	indicates bit is read or write.
 */
#define	SUNSCPAL_ICR_PARITY_ERROR	0x8000	/* (r) parity error */
#define	SUNSCPAL_ICR_BUS_ERROR		0x4000	/* (r) bus error */
#define	SUNSCPAL_ICR_ODD_LENGTH		0x2000	/* (r) odd length */
#define	SUNSCPAL_ICR_INTERRUPT_REQUEST	0x1000	/* (r) interrupt request */
#define	SUNSCPAL_ICR_REQUEST		0x0800	/* (r) request */
#define	SUNSCPAL_ICR_MESSAGE		0x0400	/* (r) message */
#define	SUNSCPAL_ICR_COMMAND_DATA	0x0200	/* (r) 1=command, 0=data */
#define	SUNSCPAL_ICR_INPUT_OUTPUT	0x0100	/* (r) 1=input (initiator should read), 0=output */
#define	SUNSCPAL_ICR_PARITY		0x0080	/* (r) parity */
#define	SUNSCPAL_ICR_BUSY		0x0040	/* (r) busy */
#define	SUNSCPAL_ICR_SELECT		0x0020	/* (rw) select */
#define	SUNSCPAL_ICR_RESET		0x0010	/* (rw) reset */
#define	SUNSCPAL_ICR_PARITY_ENABLE	0x0008	/* (rw) enable parity */
#define	SUNSCPAL_ICR_WORD_MODE		0x0004	/* (rw) word mode */
#define	SUNSCPAL_ICR_DMA_ENABLE		0x0002	/* (rw) enable DMA */
#define	SUNSCPAL_ICR_INTERRUPT_ENABLE	0x0001	/* (rw) enable interrupts */

#define	SUNSCPAL_ICR_BITS	"\20\1INTEN\2DMAEN\3WM\4PAREN\5RESET\6SEL\7BSY\10PAR\11INPUT\12CMD\13MSG\14REQ\15INTRQ\16ODD\17BUSERR\20PARERR"

/*
 * This chip keeps its DMA count with its bits flipped.  Normally, you
 * would just use the ~ operator everywhere.  However, apparently that
 * operator doesn't always work as you would expect, i.e.:
 *
 * #include <stdio.h>
 * int main() {
 *   unsigned short x;
 *   int y;
 *   x = 0;
 *   y = ~x;
 *   printf("%d\n", y);
 * }
 *
 * prints -1, not 65535.  So this macro does the bit-flipping
 * carefully for you.
 */
#define SUNSCPAL_DMA_COUNT_FLIP(x) (((unsigned short) (x)) ^ 0xFFFF)
