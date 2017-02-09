/*	$NetBSD: max6900reg.h,v 1.3 2013/08/07 19:38:45 soren Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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

#ifndef _DEV_I2C_MAX6900REG_H_
#define _DEV_I2C_MAX6900REG_H_

/*
 * MAX6900 RTC I2C address:
 *
 *	101 0000
 */
#define	MAX6900_ADDRMASK		0x3ff
#define	MAX6900_ADDR			0x50

/*
 * Command/Addresses
 */
#define	MAX6900_CMD_WRITE		0x00
#define	MAX6900_CMD_READ		0x01

#define	MAX6900_REG_SECOND		0x80
#define	MAX6900_REG_MINUTE		0x82
#define	MAX6900_REG_HOUR		0x84
#define	MAX6900_REG_DATE		0x86
#define	MAX6900_REG_MONTH		0x88
#define	MAX6900_REG_DAY			0x8a
#define	MAX6900_REG_YEAR		0x8c
#define	MAX6900_REG_CONTROL		0x8e
#define	MAX6900_REG_CENTURY		0x92
#define	MAX6900_REG_CLK_BURST		0xbe

#define	MAX6900_REG_RAM(addr)		((addr << 1) + 0xc0)
#define	MAX6900_REG_RAM_BURST		0xfe

/*
 * Burst Read/Write register layout
 */
#define	MAX6900_BURST_SECOND		0
#define	MAX6900_BURST_MINUTE		1
#define	MAX6900_BURST_HOUR		2
#define	MAX6900_BURST_DATE		3
#define	MAX6900_BURST_MONTH		4
#define	MAX6900_BURST_WDAY		5
#define	MAX6900_BURST_YEAR		6
#define	MAX6900_BURST_CONTROL		7
#define	MAX6900_BURST_LEN		8

/*
 * Register bits
 */
#define	MAX6900_SECOND_MASK		0x7f
#define	MAX6900_MINUTE_MASK		0x7f
#define	MAX6900_HOUR_12HRS		(1u << 7)
#define	MAX6900_HOUR_12MASK		0x1f
#define	MAX6900_HOUR_12HRS_PM		(1u << 5)
#define	MAX6900_HOUR_24MASK		0x3f
#define	MAX6900_DATE_MASK		0x3f
#define	MAX6900_MONTH_MASK		0x1f
#define	MAX6900_CONTROL_WP		(1u << 7)

/*
 * NVRAM size
 */
#define	MAX6900_RAM_BYTES		31

#endif /* _DEV_I2C_MAX6900REG_H_ */
