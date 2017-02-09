/*	$NetBSD: si470x_reg.h,v 1.1 2013/01/13 01:15:02 jakllsch Exp $ */

/*
 * Copyright (c) 2012 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_SI470X_REG_H_
#define _DEV_IC_SI470X_REG_H_

#define __BIT16(x)		((uint16_t)__BIT(x))
#define __BITS16(x, y)		((uint16_t)__BITS((x), (y)))

#define SI470X_DEVICEID		0x00
#define SI470X_PN		__BITS16(15, 12)
#define SI470X_MFGID		__BITS16(11, 0)

#define SI470X_CHIPID		0x01
#define SI470X_REV		__BITS16(15, 10)
#define SI470X_DEV		__BITS16(9, 6)
#define SI470X_FIRMWARE		__BITS16(5, 0)

#define SI470X_POWERCFG		0x02
#define SI470X_DSMUTE		__BIT16(15)
#define SI470X_DMUTE		__BIT16(14)
#define SI470X_MONO		__BIT16(13)
#define SI470X_RDSM		__BIT16(11)
#define SI470X_SKMODE		__BIT16(10)
#define SI470X_SEEKUP		__BIT16(9)
#define SI470X_SEEK		__BIT16(8)
#define SI470X_DISABLE		__BIT16(6)
#define SI470X_ENABLE		__BIT16(0)

#define SI470X_CHANNEL		0x03
#define SI470X_TUNE		__BIT16(15)
#define SI470X_CHAN		__BITS16(9, 0)

#define SI470X_SYSCONFIG1	0x04
#define SI470X_RDSIEN		__BIT16(15)
#define SI470X_STCIEN		__BIT16(14)
#define SI470X_RDS		__BIT16(12)
#define SI470X_DE		__BIT16(11)
#define SI470X_AGCD		__BIT16(10)
#define SI470X_BLNDADJ		__BITS16(7, 6)
#define SI470X_GPIO3		__BITS16(5, 4)
#define SI470X_GPIO2		__BITS16(3, 2)
#define SI470X_GPIO1		__BITS16(1, 0)

#define SI470X_SYSCONFIG2	0x05
#define SI470X_SEEKTH		__BITS16(15, 8)
#define SI470X_BAND		__BITS16(7, 6)
#define SI470X_SPACE		__BITS16(5, 4)
#define SI470X_VOLUME		__BITS16(3, 0)

#define SI470X_SYSCONFIG3	0x06
#define SI470X_SMUTER		__BITS16(15, 14)
#define SI470X_SMUTEA		__BITS16(13, 12)
#define SI470X_VOLEXT		__BIT16(8)
#define SI470X_SKSNR		__BITS16(7, 4)
#define SI470X_SKCNT		__BITS16(3, 0)

#define SI470X_TEST1		0x07
#define SI470X_XOSCEN		__BIT16(15)
#define SI470X_AHIZEN		__BIT16(14)

#define SI470X_TEST2		0x08

#define SI470X_BOOTCONFIG	0x09

#define SI470X_STATUSRSSI	0x0a
#define SI470X_RDSR		__BIT16(15)
#define SI470X_STC		__BIT16(14)
#define SI470X_SF_BL		__BIT16(13)
#define SI470X_AFCRL		__BIT16(12)
#define SI470X_RDSS		__BIT16(11)
#define SI470X_BLERA		__BITS16(9, 10)
#define SI470X_ST		__BIT16(8)
#define SI470X_RSSI		__BITS16(7, 0)

#define SI470X_READCHANNEL	0x0b
#define SI470X_BLERB		__BITS16(15, 14)
#define SI470X_BLERC		__BITS16(13, 12)
#define SI470X_BLERD		__BITS16(11, 10)
#define SI470X_READCHAN		__BITS16(9, 0)

#define SI470X_RDSA		0x0c
#define SI470X_RDSB		0x0d
#define SI470X_RDSC		0x0e
#define SI470X_RDSD		0x0f

#endif /* _DEV_IC_SI470X_REG_H_ */
