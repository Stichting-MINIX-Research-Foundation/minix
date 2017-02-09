/*	$NetBSD: if_aireg.h,v 1.3 2008/04/28 20:23:52 martin Exp $ 	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rafal K. Boni.
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
 * definitions for AT&T StarLAN 10 etc...
 */

/* register offsets in card's IO space */
#define AI_IOSIZE	16	/* card has 16 registers in IO space */

#define AI_RESET 	0	/* any write here resets the 586 */
#define AI_ATTN 	1	/* any write here sends a Chan attn */
#define AI_REVISION	6	/* read here to figure out this board */
#define AI_ATTRIB	7	/* more information about this board */

#define SL_BOARD(x) 	((x) & 0x0f)
#define SL_REV(x) 	((x) >> 4)

#define SL1_BOARD	0
#define SL10_BOARD	1
#define EN100_BOARD	2
#define SLFIBER_BOARD	3

#define SL_ATTR_WIDTH	0x04	/* bus width: clear -> 8-bit */
#define SL_ATTR_SPEED	0x08	/* medium speed: clear -> 10 Mbps */
#define SL_ATTR_CODING	0x10	/* encoding: clear -> Manchester */
#define SL_ATTR_HBW	0x20	/* host bus width: clear -> 16-bit */
#define SL_ATTR_TYPE	0x40	/* medium type: clear -> Ethernet */
#define SL_ATTR_BOOTROM	0x80	/* set -> boot ROM present */
