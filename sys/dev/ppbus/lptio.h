/* $NetBSD: lptio.h,v 1.9 2015/09/06 06:01:00 dholland Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gary Thorpe.
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

#ifndef __DEV_PPBUS_LPTIO_H_
#define __DEV_PPBUS_LPTIO_H_

#include <sys/ioccom.h>

/* Definitions for get status command */
enum lpt_mode_t {
	mode_unknown = -1,
	mode_standard = 1,
	mode_nibble = 2,
	mode_ps2 = 3,
	mode_fast = 4,
	mode_ecp = 5,
	mode_epp = 6
};

/* LPT ioctl commands */
#define	LPTGMODE	_IOR('L', 0, int)
#define	LPTSMODE	_IOW('L', 1, int)
#define	LPTGFLAGS	_IOR('L', 2, int)
#define	LPTSFLAGS	_IOW('L', 3, int)

/* flags for LPT[GS]FLAGS */
#define	LPT_DMA		0x01	/* enabled DMA */
#define LPT_IEEE	0x02	/* enabled IEEE 1284 negotiation */
#define	LPT_INTR	0x04	/* enabled interrupts (not polling) */
#define	LPT_PRIME	0x08	/* enabled printer initialization on open */
#define	LPT_AUTOLF	0x10	/* Automatic LF on CR */

#endif /* __DEV_PPBUS_LPTIO_H_ */
