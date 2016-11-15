/*	$NetBSD: if_efreg.h,v 1.4 2008/04/28 20:23:52 martin Exp $	*/

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
 * Card-specific definitions and macros for the 3Com 3C507/3C507TP
 */

/*
 * Register offsets (from IOBASE)
 */

/* All banks */
#define	EF_CTRL		6	/* offset of general control register */
#define	EF_ICTRL	10	/* offset of intr. latch clear register */
#define	EF_ATTN 	11	/* offset of CA register */
#define	EF_MEDIA	13	/* offset of ROM & xcvr config register */
#define	EF_MADDR	14	/* offset of shared memory config register */
#define	EF_IRQ		15	/* offset of IRQ configuration register */

/* Bank 0 -- "*3COM*" signature */
#define EF_SIG		0	/* offset of ASCII signature -- "*3COM*" */

/* Bank 1 -- ethernet address */
#define EF_ADDR		0	/* offset of card's ethernet address */

/* Bank 2 -- card part #, revision, date of manufacture */
#define	EF_TYPE		0	/* offset of card part # */
#define	EF_TYPE_HI	0	/* offset of card part # -- high byte */
#define EF_TYPE_MID	1	/* offset of card part # -- middle byte */
#define EF_TYPE_LOW	2	/* offset of card part # -- low byte */
#define EF_REV		3	/* offset of card revision, in BCD */
#define EF_DOM_DAY	4	/* offset of date of manf: day in BCD */
#define EF_DOM_MY	4	/* offset of date of manf: month, year in BCD */

/*
 * Definitions for non-bankswitched registers
 */

/* General control register */
#define	EF_CTRL_BNK0	0x00	/* register bank 0 */
#define	EF_CTRL_BNK1	0x01	/* register bank 1 */
#define	EF_CTRL_BNK2	0x02	/* register bank 2 */
#define	EF_CTRL_IEN	0x04	/* interrupt enable */
#define	EF_CTRL_INTL	0x08	/* interrupt active latch */
#define	EF_CTRL_16BIT	0x10	/* bus width; clear = 8-bit, set = 16-bit */
#define	EF_CTRL_LOOP	0x20	/* loopback mode */
#define	EF_CTRL_NRST	0x80	/* turn off to reset */
#define	EF_CTRL_RESET	(EF_CTRL_LOOP)
#define	EF_CTRL_NORMAL	(EF_CTRL_NRST | EF_CTRL_IEN | EF_CTRL_BNK1)

/* ROM & media control register */
#define EF_MEDIA_MASK	0x80	/* m1 = (EF_MEDIA register) & EF_MEDIA_MASK */
#define EF_MEDIA_SHIFT	7	/* media index = m1 >> EF_MEDIA_SHIFT */

/* shared memory control register */
#define EF_MADDR_HIGH	0x20	/* memory mapping above 15Meg */
#define EF_MADDR_MASK	0x1c	/* m1 = (EF_MADDR register) & EF_MADDR_MASK */
#define EF_MADDR_SHIFT	12	/* m2 = m1 << EF_MADDR_SHIFT  */
#define EF_MADDR_BASE	0xc0000	/* maddr = m2 + EF_MADDR_BASE */
#define EF_MSIZE_MASK	0x03	/* m1 = (EF_MADDR register) & EF_MSIZE_MASK */
#define EF_MSIZE_STEP	16384	/* msize = (m1 + 1) * EF_MSIZE_STEP */

/* interrupt control register */
#define EF_IRQ_MASK	0x0f	/* irq = (EF_IRQ register) & EF_IRQ_MASK */

/*
 * Definitions for Bank 0 registers
 */
#define EF_SIG_LEN	6	/* signature length */
#define EF_SIGNATURE	"*3COM*"

/*
 * Definitions for Bank 1 registers
 */
#define EF_ADDR_LEN	6	/* ether address length */

/*
 * Definitions for Bank 2 registers
 */
#define EF_TYPE_LEN	3	/* card part # length */

/*
 * General card-specific macros and definitions
 */
#define EF_IOBASE_LOW	0x200
#define EF_IOBASE_HIGH	0x3e0
#define EF_IOSIZE	16

/*
 * XXX: It seems that the 3C507-TP is differentiated from AUI/BNC 3C507
 * by part numbers, but I'm not sure how accurate this test is, seeing
 * as it's based on the sample of 3 cards I own (2AUI/BNC, 1 TP).
 */
#define EF_IS_TP(type)	((type)[EF_TYPE_MID] > 0x70)

#define EF_CARD_BNC	0	/* regular AUI/BNC 3C507 */
#define EF_CARD_TP	1	/* 3C507-TP -- no AUI/BNC */

