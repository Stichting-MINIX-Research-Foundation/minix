/*	$NetBSD: 3c523reg.h,v 1.2 2008/04/28 20:23:53 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 * The 3c523 i/o space is 8 bytes long. The first 6 bytes is the ethernet
 * address, followed by control & status register and card's revision number.
 */
#define	ELMC_IOADDR_BASE	0x300
#define	ELMC_IOADDR_SIZE	8
#define ELMC_CTRL		6	/* control & status register */
#define ELMC_REVISION		7	/* revision, first 4 bits only */
#define ELMC_REVISION_MASK	0xf

/*
 * The following define the bits for the control & status register.
 *
 * The bank select registers can be used if there is more than 16KB
 * of memory on the card. Bank 3 is the one for the bottom 16KB
 * (i.e. the common size), but the card defaults to bank 0. Hence we
 * need to set the bank to 3 to make the card operate at all.
 */
#define ELMC_CTRL_BS0		0x01	/* RW bank select */
#define ELMC_CTRL_BS1		0x02	/* RW bank select */
#define ELMC_CTRL_BS3		(ELMC_CTRL_BS0|ELMC_CTRL_BS1)	/* shortcut */
#define ELMC_CTRL_INT		0x04	/* RW interrupt enable, assert high */
#define ELMC_CTRL_LOOP		0x20	/* RW loopback enable, assert high */
#define ELMC_CTRL_CHA		0x40	/* RW channel attention, assert high */
#define ELMC_CTRL_RST		0x80	/* RW 82586 reset, assert low */

/*
 * The base memory space address is 0xc0000 (see 3c523 docs). The card
 * has actually 24KB of memory, but we only use first 16KB -
 * the upper 8KB is card's BIOS.
 */
#define ELMC_MADDR_BASE		0x0c0000
#define ELMC_MADDR_SIZE		0x4000		/* use only 16K of shared mem */

