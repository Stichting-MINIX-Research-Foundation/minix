/*	$NetBSD: ct65550var.h,v 1.2 2011/03/23 04:02:43 macallan Exp $	*/

/*
 * Copyright (c) 2006 Michael Lorenz
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

#ifndef CT65550VAR_H
#define CT65550VAR_H

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/i2c/i2cvar.h>

struct chipsfb_softc {
	device_t sc_dev;

	bus_space_tag_t sc_memt;
	bus_space_tag_t sc_iot;

	bus_space_handle_t sc_fbh;
	bus_space_handle_t sc_mmregh;
	bus_space_handle_t sc_ioregh;
	bus_addr_t sc_fb;
	bus_size_t sc_fbsize, sc_ioregsize;

	int (*sc_ioctl)(void *, void *, u_long, void *, int, struct lwp *);
	paddr_t	(*sc_mmap)(void *, void *, off_t, int);


	size_t memsize;

	int bits_per_pixel;
	int width, height, linebytes;

	int sc_mode;
	uint32_t sc_bg;

	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];
	int sc_dacw;

	/*
	 * I2C stuff
	 * DDC2 clock is on GPIO1, data on GPIO0
	 */
	struct i2c_controller sc_i2c;
	uint8_t sc_edid[1024];
	int sc_edidbytes;	/* number of bytes read from the monitor */

	struct vcons_data vd;
};

void chipsfb_do_attach(struct chipsfb_softc *sc);
uint32_t chipsfb_probe_vram(struct chipsfb_softc *sc);

#endif /* CT65550VAR_H */
