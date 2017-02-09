/* $NetBSD: hpetreg.h,v 1.4 2011/10/31 12:47:15 yamt Exp $ */

/*
 * Copyright (c) 2006 Nicolas Joly
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
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

#ifndef _DEV_IC_HPETREG_H_
#define _DEV_IC_HPETREG_H_

#define	HPET_WINDOW_SIZE	0x400

#define HPET_INFO		0x00
#define HPET_INFO_64BITS 		0x2000
#define HPET_PERIOD		0x04
#define HPET_PERIOD_MAX			100000000
#define HPET_CONFIG		0x10
#define HPET_CONFIG_ENABLE		0x0001
#define HPET_CONFIG_LEGRTCNF		0x0002
#define HPET_MCOUNT_LO		0xf0
#define HPET_MCOUNT_HI		0xf4
#define HPET_INT_STATUS		0x20
#define HPET_INT_STS_TIMER0		0x00000001
#define HPET_INT_STS_TIMER1		0x00000002
#define HPET_INT_STS_TIMER2		0x00000004

#define HPET_TIMER_USE_LEVEL_INTR	0x00000002
#define HPET_TIMER_ENABLE_INTR		0x00000004
#define HPET_TIMER_IS_PERIODIC_INTR	0x00000008
#define HPET_TIMER_HAS_PERIODIC_INTR	0x00000010
#define	HPET_TIMER_INTR_SHIFT		9

#define HPET_TIMER0_CONFIG	0x0100
#define HPET_TIMER0_INTCAP	0x0104
#define HPET_TIMER0_CMPREG_LO	0x0108
#define HPET_TIMER0_CMPREG_HI	0x010C

#define HPET_TIMER1_CONFIG	0x0120
#define HPET_TIMER1_INTCAP	0x0124
#define HPET_TIMER1_CMPREG_LO	0x0128
#define HPET_TIMER1_CMPREG_HI	0x012C

#define HPET_TIMER2_CONFIG	0x0140
#define HPET_TIMER2_INTCAP	0x0144
#define HPET_TIMER2_CMPREG_LO	0x0148
#define HPET_TIMER2_CMPREG_HI	0x014C

#endif /* _DEV_IC_HPETREG_H_ */
