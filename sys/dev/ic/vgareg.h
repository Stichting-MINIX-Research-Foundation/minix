/* $NetBSD: vgareg.h,v 1.9 2005/12/11 12:21:29 christos Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

struct reg_vgaattr { /* indexed via port 0x3c0 */
	u_int8_t palette[16];
	u_int8_t mode, overscan, colplen, horpixpan;
	u_int8_t colreset, misc;
};
#define VGA_ATC_NREGS	21
#define VGA_ATC_INDEX	0x0
#define VGA_ATC_DATAW	0x0
#define VGA_ATC_DATAR	0x1
#define VGA_ATC_OVERSCAN	0x11

struct reg_vgats { /* indexed via port 0x3c4 */
	u_int8_t syncreset, mode, wrplmask, fontsel, memmode;
};
#define VGA_TS_MODE_BLANK	0x20

#define VGA_TS_NREGS	5
#define VGA_TS_INDEX 	0x4
#define VGA_TS_DATA	0x5

struct reg_vgagdc { /* indexed via port 0x3ce */
	u_int8_t setres, ensetres, colorcomp, rotfunc;
	u_int8_t rdplanesel, mode, misc, colorcare;
	u_int8_t bitmask;
};
#define VGA_GDC_NREGS	9
#define VGA_GDC_INDEX	0xe
#define VGA_GDC_DATA	0xf

/*
 * CRTC registers are defined in sys/dev/ic/mc6845reg.h
 */

/* video DAC palette registers */
#define VGA_DAC_PELMASK	0x6
#define VGA_DAC_STATE	0x7
#define VGA_DAC_ADDRR	0x7
#define VGA_DAC_ADDRW	0x8
#define VGA_DAC_PALETTE	0x9

/* misc output register */
#define VGA_MISC_DATAR	0xc
#define VGA_MISC_DATAW	0x2
