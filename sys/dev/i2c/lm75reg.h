/*	$NetBSD: lm75reg.h,v 1.4 2013/08/07 19:38:45 soren Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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

#ifndef _DEV_I2C_LM75REG_H_
#define _DEV_I2C_LM75REG_H_

/*
 * LM75 temperature sensor I2C address:
 *
 *	100 1xxx
 */
#define	LM75_ADDRMASK		0x3f8
#define	LM75_ADDR		0x48
#define	LM77_ADDRMASK		0x3fc
#define	LM77_ADDR		0x48

/*
 * Temperature on the LM75 is represented by a 9-bit two's complement
 * integer in steps of 0.5C.  The following examples are taken from
 * the LM75 data sheet:
 *
 *	+125C	0 1111 1010	0x0fa
 *	+25C	0 0011 0010	0x032
 *	+0.5C	0 0000 0001	0x001
 *	0C	0 0000 0000	0x000
 *	-0.5C	1 1111 1111	0x1ff
 *	-25C	1 1100 1110	0x1ce
 *	-55C	1 1001 0010	0x192
 */

#define	LM75_REG_TEMP			0x00
#define	LM75_REG_CONFIG			0x01
#define	LM75_REG_THYST_SET_POINT	0x02
#define	LM75_REG_TOS_SET_POINT		0x03

#define	LM77_REG_TCRIT_SET_POINT	0x03
#define	LM77_REG_TLOW_SET_POINT		0x04
#define	LM77_REG_THIGH_SET_POINT	0x05


#define	LM75_TEMP_LEN			2	/* 2 data bytes */

#define	LM75_CONFIG_SHUTDOWN		0x01
#define	LM75_CONFIG_CMPINT		0x02
#define	LM75_CONFIG_OSPOLARITY		0x04
#define	LM75_CONFIG_FAULT_QUEUE_1	(0 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_2	(1 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_4	(2 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_6	(3 << 3)

#define	LM77_CONFIG_TCRITAPOLARITY	0x04
#define	LM77_CONFIG_INTPOLARITY		0x08
#define	LM77_CONFIG_FAULT_QUEUE		LM75_CONFIG_FAULT_QUEUE_4

#endif /* _DEV_I2C_LM75REG_H_ */
