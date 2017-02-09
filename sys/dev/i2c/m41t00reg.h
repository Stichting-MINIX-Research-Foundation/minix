/*	$NetBSD: m41t00reg.h,v 1.2 2005/12/11 12:21:23 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen K. Briggs for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_I2C_M41T00REG_H_
#define	_DEV_I2C_M41T00REG_H_

#define M41T00_ADDR	0x68

#define M41T00_SEC		0	/* 00-59     -- BCD Seconds */
#define M41T00_MIN		1	/* 00-59     -- BCD Minutes */
#define M41T00_CENHR		2	/* 0-1/00-23 -- BCD Century/Hour */
#define M41T00_DAY		3	/* 01-07     -- BCD Day */
#define M41T00_DATE		4	/* 01-31     -- BCD Date */
#define M41T00_MONTH		5	/* 01-12     -- BCD Month */
#define M41T00_YEAR		6	/* 00-99     -- BCD Year */
#define M41T00_DATE_BYTES	7
#define M41T00_CONTROL		7	/* Control Register */
#define M41T00_NBYTES		8

#define M41T00_SEC_MASK		0x7f
#define M41T00_MIN_MASK		0x7f
#define M41T00_HOUR_MASK	0x3f
#define M41T00_DAY_MASK		0x07
#define M41T00_DATE_MASK	0x3f
#define M41T00_MONTH_MASK	0x1f
#define M41T00_YEAR_MASK	0xff

#endif /* _DEV_I2C_M41T00REG_H_ */
