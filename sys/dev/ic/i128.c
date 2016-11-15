/*	$NetBSD: i128.c,v 1.4 2012/10/20 13:31:09 macallan Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i128.c,v 1.4 2012/10/20 13:31:09 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/ic/i128reg.h>
#include <dev/ic/i128var.h>

void
i128_init(bus_space_tag_t tag, bus_space_handle_t regh, int stride, int depth)
{
	/* initialize the i128's blitter */
	switch (depth) {
		case 8:
			bus_space_write_4(tag, regh, BUF_CTRL, BC_PSIZ_8B);
			break;
		case 16:
			bus_space_write_4(tag, regh, BUF_CTRL, BC_PSIZ_16B);
			break;
		case 32:
			bus_space_write_4(tag, regh, BUF_CTRL, BC_PSIZ_32B);
			break;
		default:
			aprint_error("i128: unsupported colour depth (%d)\n",
			    depth);
			return;
	}

	bus_space_write_4(tag, regh, DE_PGE, 0);
	bus_space_write_4(tag, regh, DE_SORG, 0);
	bus_space_write_4(tag, regh, DE_DORG, 0);
	bus_space_write_4(tag, regh, DE_MSRC, 0);
	bus_space_write_4(tag, regh, DE_WKEY, 0);
	bus_space_write_4(tag, regh, DE_SPTCH, stride);
	bus_space_write_4(tag, regh, DE_DPTCH, stride);
	bus_space_write_4(tag, regh, RMSK, 0);
	bus_space_write_4(tag, regh, XY4_ZM, ZOOM_NONE);
	bus_space_write_4(tag, regh, LPAT, 0xffffffff);
	bus_space_write_4(tag, regh, ACNTRL, 0);
	bus_space_write_4(tag, regh, INTM, 3);
	bus_space_write_4(tag, regh, CLPTL, 0x00000000);
	bus_space_write_4(tag, regh, CLPBR, 0x1fff0fff);
	bus_space_write_4(tag, regh, MASK, 0xffffffff);
}

void
i128_bitblt(bus_space_tag_t tag, bus_space_handle_t regh, int xs, int ys,
    int xd, int yd, int wi, int he, int rop)
{
	int dir = 0;

	if (xs < xd) {
		dir |= DIR_RL;
		xs += wi - 1;
		xd += wi - 1;
	}
	if (ys < yd) {
		dir |= DIR_BT;
		ys += he - 1;
		yd += he - 1;
	}

	I128_READY(tag, regh);
	bus_space_write_4(tag, regh, CMD,
	    (rop & 0xff) << 8 | CO_BITBLT);
	bus_space_write_4(tag, regh, XY3_DIR, dir);
	bus_space_write_4(tag, regh, XY2_WH, (wi << 16) | he);
	bus_space_write_4(tag, regh, XY0_SRC, (xs << 16) | ys);
	bus_space_write_4(tag, regh, XY1_DST, (xd << 16) | yd);
}

void
i128_rectfill(bus_space_tag_t tag, bus_space_handle_t regh, int x, int y,
    int wi, int he, uint32_t color)
{

	I128_READY(tag, regh);
	bus_space_write_4(tag, regh, CMD,
	    CS_SOLID << 16 | (CR_COPY) << 8 | CO_BITBLT);
	bus_space_write_4(tag, regh, FORE, color);
	bus_space_write_4(tag, regh, XY3_DIR, 0);
	bus_space_write_4(tag, regh, XY2_WH, (wi << 16) | he);
	bus_space_write_4(tag, regh, XY0_SRC, 0);
	bus_space_write_4(tag, regh, XY1_DST, (x << 16) | y);
}

void
i128_ready(bus_space_tag_t t, bus_space_handle_t h)
{
    I128_READY(t, h);
}

void
i128_sync(bus_space_tag_t t, bus_space_handle_t h)
{
    I128_DONE(t, h);
}
 
