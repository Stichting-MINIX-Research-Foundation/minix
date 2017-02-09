/*	$NetBSD: rs5c372reg.h,v 1.3 2012/01/21 19:44:30 nonaka Exp $	*/

/*-
 * Copyright (C) 2005 NONAKA Kimihiro <nonaka@netbsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_I2C_RS5C372REG_H_
#define _DEV_I2C_RS5C372REG_H_

/*
 * RS5C372[AB] Real-Time Clock
 */

#define	RS5C372_ADDR		0x32	/* Fixed I2C Slave Address */

#define RS5C372_SECONDS		0
#define RS5C372_MINUTES		1
#define RS5C372_HOURS		2
#define RS5C372_DAY		3
#define RS5C372_DATE		4
#define RS5C372_MONTH		5
#define RS5C372_YEAR		6
#define RS5C372_CLOCK_CORRECT	7
#define RS5C372_ALARMA_MIN	8
#define RS5C372_ALARMA_HOUR	9
#define RS5C372_ALARMA_DATE	10
#define RS5C372_ALARMB_MIN	11
#define RS5C372_ALARMB_HOUR	12
#define RS5C372_ALARMB_DATE	13
#define RS5C372_CONTROL1	14
#define RS5C372_CONTROL2	15
#define	RS5C372_NREGS		16
#define	RS5C372_NRTC_REGS	7

/*
 * Bit definitions.
 */
#define	RS5C372_SECONDS_MASK	0x7f
#define	RS5C372_MINUTES_MASK	0x7f
#define	RS5C372_HOURS_12HRS_PM	(1u << 5)	/* If 12 hr mode, set = PM */
#define	RS5C372_HOURS_12MASK	0x1f
#define	RS5C372_HOURS_24MASK	0x3f
#define	RS5C372_DAY_MASK	0x07
#define	RS5C372_DATE_MASK	0x3f
#define	RS5C372_MONTH_MASK	0x1f
#define	RS5C372_CONTROL2_24HRS	(1u << 5)
#define	RS5C372_CONTROL2_XSTP	(1u << 4)	/* read */
#define	RS5C372_CONTROL2_ADJ	(1u << 4)	/* write */
#define	RS5C372_CONTROL2_NCLEN	(1u << 3)
#define	RS5C372_CONTROL2_CTFG	(1u << 2)
#define	RS5C372_CONTROL2_AAFG	(1u << 1)
#define	RS5C372_CONTROL2_BAFG	(1u << 0)

#endif /* _DEV_I2C_RS5C372REG_H_ */
