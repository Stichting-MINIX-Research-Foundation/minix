/* $NetBSD: daicreg.h,v 1.6 2008/04/28 20:23:49 martin Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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
 * Shared memory definitions for Diehl activ isdn adapters.
 */

/*
 * Values after reset, before isdn support is downloaded
 */
#define	DAIC_BOOT_CTRL		0	/* byte */
#define DAIC_BOOT_CARD		1	/* byte */
#define DAIC_BOOT_MSIZE		2	/* byte */
#define	DAIC_BOOT_EBIT		4	/* 2 byte */
#define DAIC_BOOT_ELOC		6	/* 4 byte */
#define DAIC_BOOT_SIGNATURE	30	/* 2 byte */
#define	DAIC_BOOT_BUF		256	/* 256 bytes */

#define	DAIC_SERNO	1008
#define	DAIC_SWID	1016
#define	DAIC_DOSIO	0x3e8
#define	DAIC_MEMFREE	0x3f4
#define	DAIC_SET_CARD	0x3fc
#define	DAIC_IRQ	0x3fe

#define	DAIC_BOOT_START		0
#define	DAIC_BOOT_CODE		0x200
#define	DAIC_BOOT_END		0x3ff
#define	DAIC_BOOT_SET_RESET	0x400	/* byte */
#define	DAIC_BOOT_CLR_RESET	0x401	/* byte */

/*
 * Parameters for the microcode
 */
#define	DAIC_BOOT_TEI	8
#define	DAIC_BOOT_NT2	9
#define	DAIC_BOOT_ZERO	10
#define	DAIC_BOOT_WATCHDOG	11
#define	DAIC_BOOT_PERMANENT	12
#define	DAIC_BOOT_XINTERFACE	13

/*
 * Response from microcode
 */
#define	DAIC_SIGNATURE_VALUE	0x4447

/*
 * Diagnostic interface
 */
#define	DAIC_DIAG_REQ		0x380	/* byte */
#define	DAIC_DIAG_RC		0x381	/* byte */
#define	DAIC_DIAG_MEM		0x382	/* 4 byte */
#define	DAIC_DIAG_LENGTH	0x386	/* 2 byte */
#define	DAIC_DIAG_PORT		0x388	/* 2 byte */
#define	DAIC_DIAG_DATA		0x390	/* 48 byte */
#define	DAIC_DIAG_DATA_SIZE	48
#define	DAIC_DIAG_MAXCMD	14

/*
 * Values after download, with software running on card
 */
#define	DAIC_COM_REQ		0	/* byte */
#define	DAIC_COM_REQID		1	/* byte */
#define	DAIC_COM_RC		2	/* byte */
#define DAIC_COM_RCID		3	/* byte */
#define DAIC_COM_IND		4	/* byte */
#define DAIC_COM_INDID		5	/* byte */
#define	DAIC_COM_IMASK		6	/* byte */
#define	DAIC_COM_RNR		7	/* byte */
#define	DAIC_COM_XLOCK		8	/* byte */
#define	DAIC_COM_INTERRUPT	9	/* byte */
#define	DAIC_COM_REQCH		10	/* byte */
#define	DAIC_COM_RCCH		11	/* byte */
#define	DAIC_COM_INDCH		12	/* byte */
#define	DAIC_COM_MIND		13	/* byte */
#define	DAIC_COM_MLENGTH	14	/* 2 byte */
#define	DAIC_COM_SIGNATURE	30	/* 2 byte */
#define	DAIC_COM_XBUFFER	32	/* 272 byte */
#define	DAIC_COM_RBUFFER	304	/* 272 byte */

/*
 * Diagnostic commands/results
 */
#define	DAIC_TEST_RDY	0
#define	DAIC_TEST_MEM	1
#define	DAIC_TEST_SKIP	2
#define	DAIC_TEST_BUSY	3

/*
 * ID's of global instances on the card
 */
#define	DAIC_GLOBALID_DCHAN	0
#define	DAIC_GLOBALID_ISO3	0x20
#define	DAIC_GLOBALID_ISO2	0x60
#define	DAIC_GLOBALID_TASKS	0x80
#define	DAIC_GLOBALID_TIMER	0xa0
#define	DAIC_GLOBALID_PHONE	0xc0

/*
 * REQUEST codes
 */
#define DAIC_REQ_ASSIGN		0x01
#define	DAIC_REQ_INDICATE	0x0a
#define	DAIC_REQ_CALL		0x01

/*
 * INDICATION codes
 */
#define	DAIC_IND_HANGUP		0x03
#define	DAIC_IND_INDICATE	0x0a
#define	DAIC_IND_INFO		0x0d

/*
 * return codes
 */
#define	DAIC_RC_UNKNOWN_COMMAND	0x01
#define	DAIC_RC_WRONG_COMMAND	0x02
#define	DAIC_RC_WRONG_ID	0x03
#define	DAIC_RC_WRONG_CH	0x04
#define	DAIC_RC_UNKNOWN_IE	0x05
#define	DAIC_RC_WRONG_IE	0x06
#define	DAIC_RC_OUT_OF_RESRCS	0x07
#define	DAIC_RC_ASSIGN_RC	0xe0
#define	DAIC_RC_ASSIGN_MASK	0xf0
#define	DAIC_RC_ERRMASK		0x0f
#define	DAIC_RC_ASSIGN_OK	0xef
#define	DAIC_RC_TIMER_INT	0xfe
#define	DAIC_RC_OK		0xff
