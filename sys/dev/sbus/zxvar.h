/*	$NetBSD: zxvar.h,v 1.6 2009/09/19 11:55:09 tsutsui Exp $	*/

/*
 *  Copyright (c) 2002 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Andrew Doran.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _DEV_SBUS_ZXVAR_H_
#define _DEV_SBUS_ZXVAR_H_

/*
 * Sun (and Linux) compatible offsets for mmap().
 */
#define ZX_FB0_VOFF		0x00000000
#define ZX_LC0_VOFF		0x00800000
#define ZX_LD0_VOFF		0x00801000
#define ZX_LX0_CURSOR_VOFF	0x00802000
#define ZX_FB1_VOFF		0x00803000
#define ZX_LC1_VOFF		0x01003000
#define ZX_LD1_VOFF		0x01004000
#define ZX_LX0_VERT_VOFF	0x01005000
#define ZX_LX_KRN_VOFF		0x01006000
#define ZX_LC0_KRN_VOFF		0x01007000
#define ZX_LC1_KRN_VOFF		0x01008000
#define ZX_LD_GBL_VOFF		0x01009000

#define	ZX_WID_SHARED_8	0
#define	ZX_WID_SHARED_24	1
#define	ZX_WID_DBL_8		2
#define	ZX_WID_DBL_24		3

/*
 * Per-instance data.
 */
struct zx_softc {
	device_t	sc_dv;
	struct fbdevice	sc_fb;
	bus_space_tag_t	sc_bt;

	bus_space_handle_t sc_bhzc;
	bus_space_handle_t sc_bhzx;
	bus_space_handle_t sc_bhzdss0;
	bus_space_handle_t sc_bhzdss1;
	bus_space_handle_t sc_bhzcu;

	int		sc_flags;
	uint8_t		*sc_cmap;
	uint32_t	*sc_pixels;
	bus_addr_t	sc_paddr;
	int		sc_shiftx;
	int		sc_shifty;

	struct fbcurpos	sc_curpos;
	struct fbcurpos	sc_curhot;
	struct fbcurpos sc_cursize;
	uint8_t		sc_curcmap[8];
	uint32_t	sc_curbits[2][32];

#if NWSDISPLAY > 0	
	uint32_t sc_width;
	uint32_t sc_height;	/* display width / height */
	uint32_t sc_stride;
	int sc_mode;
	uint32_t sc_bg;
	struct vcons_data vd;
#endif	
};
#define	ZX_BLANKED	0x01
#define	ZX_CURSOR	0x02

#endif	/* !_DEV_SBUS_ZXVAR_H_ */
