/*	$NetBSD: smc90cx6reg.h,v 1.10 2008/04/28 20:23:51 martin Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * chip offsets and bits for the SMC Arcnet chipset.
 */

#ifndef _SMC90CXVAR_H_
#define _SMC90CXVAR_H_

/* register offsets */

#define BAHSTAT	0
#define	BAHCMD	1

/* memory offsets */
#define BAHCHECKBYTE 0
#define BAHMACOFF 1

#define BAH_TXDIS	0x01
#define BAH_RXDIS	0x02
#define BAH_TX(x)	(0x03 | ((x)<<3))
#define BAH_RX(x)	(0x04 | ((x)<<3))
#define BAH_RXBC(x)	(0x84 | ((x)<<3))

#define BAH_CONF(x)  	(0x05 | (x))
#define CLR_POR		0x08
#define CLR_RECONFIG	0x10

#define BAH_CLR(x)	(0x06 | (x))
#define CONF_LONG	0x08
#define CONF_SHORT	0x00

/*
 * These are not in the COM90C65 docs. Derived from the arcnet.asm
 * packet driver by Philippe Prindeville and Russel Nelson.
 */

#define BAH_LDTST(x)	(0x07 | (x))
#define TEST_ON		0x08
#define TEST_OFF	0x00

#define BAH_TA		1	/* int mask also */
#define BAH_TMA		2
#define BAH_RECON	4	/* int mask also */
#define BAH_TEST	8	/* not in the COM90C65 docs (see above) */
#define BAH_POR		0x10	/* non maskable interrupt */
#define BAH_ET1		0x20	/* timeout value bits, normally 1 */
#define BAH_ET2		0x40	/* timeout value bits, normally 1 */
#define BAH_RI		0x80	/* int mask also */

#endif
