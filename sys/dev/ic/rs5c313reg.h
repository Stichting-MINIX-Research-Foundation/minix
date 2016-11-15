/*	$NetBSD: rs5c313reg.h,v 1.3 2010/04/06 15:29:19 nonaka Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
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

#ifndef	_DEV_IC_RS5C313REG_H_
#define	_DEV_IC_RS5C313REG_H_

/*
 * RICOH RS5C3[12]x Real Time Clock
 */
				/* 5c313/5c314 don't have bank1 */
#define	RS5C313_SEC1	0	/* bank0 */
#define	RS5C313_SEC10	1	/* bank0 */
#define	RS5C313_MIN1	2	/* bank0 */
#define	RS5C313_MIN10	3	/* bank0 */
#define	RS5C313_HOUR1	4	/* bank0 */
#define	RS5C313_HOUR10	5	/* bank0 */
#define	RS5C313_WDAY	6	/* bank0 */
#define	RS5C313_TINT	7	/* bank0/1 (5c313/5c314/5c316/5c317) */
#define	RS5C313_SCRATCH	7	/* bank0/1 (5c321) */
#define	RS5C313_DAY1	8	/* bank0 */
#define	RS5C313_DAY10	9	/* bank0 */
#define	RS5C313_MON1	10	/* bank0 */
#define	RS5C313_MON10	11	/* bank0 */
#define	RS5C313_YEAR1	12	/* bank0 */
#define	RS5C313_YEAR10	13	/* bank0 */
#define	RS5C313_CTRL	14	/* bank0/1 */
#define	RS5C313_CTRL2	15	/* bank0/1 */

/* Alarm register (5c316/5c317) */
#define	RS5C313_AWOD1	0	/* bank1 */
#define	RS5C313_AWOD2	1	/* bank1 */
#define	RS5C313_AMIN1	2	/* bank1 */
#define	RS5C313_AMIN10	3	/* bank1 */
#define	RS5C313_AHOUR1	4	/* bank1 */
#define	RS5C313_AHOUR10	5	/* bank1 */

/* Timer register (5c317) */
#define	RS5C313_TMR	9	/* bank1 */

/* 32kHz control register (5c317/5c321) */
#define	RS5C313_32KHZ	10	/* bank1 */

/* TINT register */
#define	TINT_CT0		0x01
#define	TINT_CT1		0x02
#define	TINT_CT2		0x04
#define	TINT_CT3		0x08

/* CTRL register */
#define	CTRL_BSY		0x01	/* read */
#define	CTRL_ADJ		0x01	/* write */
#define	CTRL_XSTP		0x02	/* read */
#define	CTRL_WTEN		0x02	/* write */
#define	CTRL_24H		0x04	/* read/write (5c313/5c314) */
#define	CTRL_ALFG		0x04	/* read/write (5c316/5c317) */
#define	CTRL_CTFG		0x08	/* read/write */

/* CTRL2 register */
#define	CTRL2_NTEST		0x01
#define	CTRL2_BANK		0x02	/* (5c316/5c317/5c321) */
#define	CTRL2_TMR		0x04	/* (5c317) */
#define	CTRL2_24H		0x08	/* (5c316/5c317/5c321) */

#endif	/* _DEV_IC_RS5C313REG_H_ */
