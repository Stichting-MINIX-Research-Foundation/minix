/*      $NetBSD: msm6242breg.h,v 1.2 2013/02/04 17:19:17 phx Exp $ */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#ifndef _MSM6242BREG_H_
#define _MSM6242BREG_H_

#define MSM6242B_1SECOND	0x0
#define MSM6242B_10SECOND	0x1
#define MSM6242B_1MINUTE	0x2
#define MSM6242B_10MINUTE	0x3
#define MSM6242B_1HOUR		0x4

#define MSM6242B_10HOUR_PMAM	0x5
#define MSM6242B_10HOUR_MASK		0x3
#define MSM6242B_PMAM_BIT		__BIT(2)

#define MSM6242B_1DAY		0x6
#define MSM6242B_10DAY		0x7
#define MSM6242B_1MONTH		0x8
#define MSM6242B_10MONTH	0x9
#define MSM6242B_1YEAR		0xA
#define MSM6242B_10YEAR		0xB
#define MSM6242B_WEEK		0xC

#define	MSM6242B_CONTROL_D	0xD
#define MSM6242B_CONTROL_D_HOLD		__BIT(0)
#define MSM6242B_CONTROL_D_BUSY		__BIT(1)
#define MSM6242B_CONTROL_D_IRQF		__BIT(2)
#define MSM6242B_CONTROL_D_30SECADJ	__BIT(3)

#define	MSM6242B_CONTROL_E	0xE

#define	MSM6242B_CONTROL_F	0xF
#define	MSM6242B_CONTROL_F_24H		__BIT(2)

#define MSM6242B_MASK		0xF	/* 4 significant bits only */
#define MSM6242B_SIZE		0x10

#define MSM6242B_BASE_YEAR	1900

#endif /* _MSM6242BREG_H_ */
