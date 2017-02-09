/*	$NetBSD: if_iereg.h,v 1.2 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * Definitions for the MPU Port and Channel Attention registers
 * of the onboard i82596 Ethernet controller on MVME68K and MVME88K boards.
 */
#ifndef __mvme_if_iereg_h
#define __mvme_if_iereg_h

#define	IE_MPUREG_UPPER	0x00	/* Upper Command Word */
#define IE_MPUREG_LOWER	0x02	/* Lower Command Word */
#define IE_MPUREG_CA	0x04	/* Channel Attention. Dummy Rd or Wr */

#define IE_MPUREG_SIZE	0x08

/*
 * BUS_USE -> Interrupt Active High (edge-triggered),
 *            Lock function enabled,
 *            Internal bus throttle timer triggering,
 *            82586 operating mode.
 */
#define IE_BUS_USE	(IE_SYSBUS_596_INTHIGH	| \
			 IE_SYSBUS_596_LOCK	| \
			 IE_SYSBUS_596_TRGINT	| \
			 IE_SYSBUS_596_82586	| \
			 IE_SYSBUS_596_RSVD_SET)

#endif /* __mvme_if_iereg_h */
