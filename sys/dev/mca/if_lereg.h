/*	$NetBSD: if_lereg.h,v 1.2 2008/04/28 20:23:53 martin Exp $	*/

/*-
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

#define LE_MCA_MEMSIZE	0x4000
#define LE_PROMOFF	0x3fc0		/* Offset of PROM within the memory */
#define LE_MCA_RAMSIZE	LE_PROMOFF	/* Size of RAM memory */
#define LE_LANCEREG	0x3ff0		/* Offset of LANCE register (2bytes) */
#define LE_PORT		0x3ff2		/* Offset of port register */
#define LE_REGIO	0x3ff3		/* Offset of regio register */

/* Bits in Status / Control */
#define	REGREQ		0x20	/* I/O command pending */
#define	IRQ		0x10	/* IRQ pending */
#define RESET		0x08	/* Reset board */
#define REGRW		0x02	/* Read/Write Register */
#define REGADDR		0x01	/* Register address */

#define REGREAD		REGRW	/* Read Register */
#define REGWRITE	0	/* Write Register */
#define RAP		REGADDR	/* Address RAP */
#define RDATA		0	/* Address Data */

/* Bits in REGIO */
#define	REGDO		0x80	/* Do transfer */
