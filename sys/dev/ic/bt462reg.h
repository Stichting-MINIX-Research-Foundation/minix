/*	$NetBSD: bt462reg.h,v 1.1 2010/06/24 03:30:36 macallan Exp $ */

/*-
 * Copyright (c) 2010 Michael Lorenz
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
 * register definitions for the Bt461/Bt462 RAMDAC
 *
 * There are four registers, how exactly to access them depends on the host so 
 * we don't provide constants for them. All registers are 8 bit wide:
 * - register 0 provides the lower 8 index bits
 * - register 1 provides the upper 2 index bits
 * - register 2 accesses the alternate colour map ( 256 entries ), overlay
 *   colour map ( 32 enttries ) and control registers
 * - register 3 accesses the main colour map ( 1024 entries )
 */
 
#ifndef BT462REG_H
#define BT462REG_H

/* offsets for register 2 */
#define BT462_ALT_CMAP		0x0000
#define BT462_OVL_CMAP		0x0100
#define BT462_ID		0x0200
	#define ID_BT461	0x4d
	#define	ID_BT462	0x4c

#define BT462_CMD_0		0x0201
	#define BT462_C0_BLINK_16_48	0x00
	#define BT462_C0_BLINK_16_16	0x04
	#define BT462_C0_BLINK_32_32	0x08
	#define BT462_C0_BLINK_64_64	0x0c
	#define BT462_C0_ALT_ENABLE	0x10
	#define BT462_C0_OVL_OPAQUE	0x20	/* use ovl 0, not cmap */
	#define BT462_C0_MULT_3_1	0x00
	#define BT462_C0_MULT_4_1	0x40
	#define BT462_C0_MULT_5_1	0xc0

#define BT462_CMD_1		0x0202
	#define BT462_C1_PAN_0		0x00
	#define BT462_C1_PAN_1		0x20
	#define BT462_C1_PAN_2		0x40
	#define BT462_C1_PAN_3		0x80
	#define BT462_C1_PAN_4		0xa0

#define BT462_CMD_2		0x0203
	#define BT462_C2_TEST_ENABLE	0x01
	#define BT462_C2_UNDERLAY_EN	0x04	/* Bt462 only */
	#define BT462_C2_PLL_USE_BLANK	0x08	/* SYNC otherwise */
	#define BT462_C2_LOAD_ALWAYS	0x00
	#define BT462_C2_LOAD_ON_RED	0x10
	#define BT462_C2_LOAD_ON_GREEN	0x20
	#define BT462_C2_LOAD_ON_BLUE	0x30
	#define BT462_C2_PEDESTAL_EN	0x40
	#define BT462_C2_SYNC_ENABLE	0x80

#define BT462_PIXEL_READ_MSK_L	0x0204
#define BT462_PIXEL_READ_MSK_H	0x0205
#define BT462_PIXEL_BLNK_MSK_L	0x0206
#define BT462_PIXEL_BLNK_MSK_H	0x0207
#define BT462_OVL_READ_MSK	0x0208
#define BT462_OVL_BLNK_MSK	0x0209
#define BT462_TEST		0x020c

#endif /* BT462REG_H */
