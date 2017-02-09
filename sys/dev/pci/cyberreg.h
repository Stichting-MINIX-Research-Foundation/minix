/*	$NetBSD: cyberreg.h,v 1.3 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frederick S. Bruckman.
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
 * These cards have various combinations of serial and parallel ports. All
 * varieties have up to 6 1-bit registers for extended capabilities, named
 * "Usr0, ..., Usr5". The functional registers are mapped to the proper
 * "Usr" register at attachment time. The only functional registers the
 * kernel currently deals with are the registers to enable or disable the
 * alternate clock, which permits speeds of the serial port all the way to
 * 960Kbps. (In the documentation, those registers are called "Clks0" and
 * "Clks1" on the "10x" series, and * "K0" and "K1" on the 20x series.)
 */

#ifndef _PCI_CYBERREG_H_
#define	_PCI_CYBERREG_H_

/* The "10x" series cards have 4 1-bit registers, spaced 3 bits apart. */
#define SIIG10x_USR_BASE	0x50
#define SIIG10x_USR0_MASK	(1 << 2 << 16)
#define SIIG10x_USR1_MASK	(1 << 5 << 16)
#define SIIG10x_USR2_MASK	(1 << 8 << 16)
#define SIIG10x_USR3_MASK	(1 << 11 << 16)

/* The "20x" series cards have 6 1-bit registers, spaced 32 bits apart. */
#define SIIG20x_USR0		0x6c
#define SIIG20x_USR1		0x70
#define SIIG20x_USR2		0x74
#define SIIG20x_USR3		0x78
#define SIIG20x_USR4		0x7c
#define SIIG20x_USR5		0x80
#define SIIG20x_USR_MASK	(1 << 28)

#endif /* !_PCI_CYBERREG_H_ */
