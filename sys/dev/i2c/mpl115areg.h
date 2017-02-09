/*	$NetBSD: mpl115areg.h,v 1.1 2013/09/08 14:59:42 rkujawa Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Radoslaw Kujawa.
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

#ifndef _MPL115AREG_H_
#define _MPL115AREG_H_

#define MPL115A_ADDR			0x60

#define MPL115A_PADC_MSB		0x0
#define MPL115A_PADC_LSB		0x1
#define MPL115A_TADC_MSB		0x2
#define MPL115A_TADC_LSB		0x3
#define MPL115A_A0_MSB			0x4
#define MPL115A_A0_LSB			0x5
#define MPL115A_B1_MSB			0x6
#define MPL115A_B1_LSB			0x7
#define MPL115A_B2_MSB			0x8
#define MPL115A_B2_LSB			0x9
#define MPL115A_C12_MSB			0xA
#define MPL115A_C12_LSB			0xB
#define MPL115A_CONVERT			0x12

#endif /* _MPL115AREG_H_ */
