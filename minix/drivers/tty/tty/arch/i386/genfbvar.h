/*	$NetBSD: genfbvar.h,v 1.25 2017/02/25 01:11:55 nonaka Exp $ */

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

#ifndef GENFBVAR_H
#define GENFBVAR_H

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>

//#include <sys/bus.h>

#include "../../wscons/wsconsio.h"
#include "../../wscons/wsdisplayvar.h"
#include "../../rasops/rasops.h"

#include "../../wscons/wsdisplay_vconsvar.h"

struct genfb_softc;

struct genfb_ops {
	int (*genfb_ioctl)(void *, void *, u_long, void *, int);
	paddr_t	(*genfb_mmap)(void *, void *, off_t, int);
	//int (*genfb_borrow)(void *, bus_addr_t, bus_space_handle_t *);
	//int (*genfb_enable_polling)(void *);
	//int (*genfb_disable_polling)(void *);
};

#if 0
struct genfb_colormap_callback {
	void *gcc_cookie;
	void (*gcc_set_mapreg)(void *, int, int, int, int);
};

/*
 * Integer parameter provider.  Each callback shall return 0 on success,
 * and an error(2) number on failure.  The gpc_upd_parameter callback is
 * optional (i.e. it can be NULL).
 *
 * This structure is used for backlight and brightness control.  The
 * expected parameter range is:
 *
 *	[0, 1]		for backlight
 *	[0, 255]	for brightness
 */
struct genfb_parameter_callback {
	void *gpc_cookie;
	int (*gpc_get_parameter)(void *, int *);
	int (*gpc_set_parameter)(void *, int);
	int (*gpc_upd_parameter)(void *, int);
};

struct genfb_pmf_callback {
	bool (*gpc_suspend)(device_t, const pmf_qual_t *);
	bool (*gpc_resume)(device_t, const pmf_qual_t *);
};

struct genfb_mode_callback {
	bool (*gmc_setmode)(struct genfb_softc *, int);
};
#endif

struct genfb_softc {
	device_t sc_dev;
	struct vcons_data vd;
	struct genfb_ops sc_ops;
	struct vcons_screen sc_console_screen;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;
	//struct genfb_colormap_callback *sc_cmcb;
	//struct genfb_pmf_callback *sc_pmfcb;
	//struct genfb_parameter_callback *sc_backlight;
	//struct genfb_parameter_callback *sc_brightness;
	//struct genfb_mode_callback *sc_modecb;
	//int sc_backlight_level, sc_backlight_on;
	void *sc_fbaddr;	/* kva */
	void *sc_shadowfb;
	bool sc_enable_shadowfb;
	//bus_addr_t sc_fboffset;	/* bus address */
	int sc_width, sc_height, sc_stride, sc_depth;
	size_t sc_fbsize;
	int sc_rpos, sc_gpos, sc_bpos;
	int sc_rnum, sc_gnum, sc_bnum;
	int sc_mode;
	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];
	bool sc_want_clear;
	struct wsdisplay_accessops sc_accessops;
};

void	genfb_cnattach(void);
void	genfb_disable(void);
int	genfb_is_console(void);
int	genfb_is_enabled(void);
void	genfb_init(struct genfb_softc *);
int	genfb_attach(struct genfb_softc *, struct genfb_ops *);
//int	genfb_borrow(bus_addr_t, bus_space_handle_t *);
//void	genfb_restore_palette(struct genfb_softc *);
//void	genfb_enable_polling(device_t);
//void	genfb_disable_polling(device_t);

#endif /* GENFBVAR_H */
