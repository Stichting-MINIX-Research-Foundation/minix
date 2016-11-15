/*	$NetBSD: cgsix_sbus.c,v 1.30 2009/09/17 16:28:12 tsutsui Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * color display (cgsix) driver; Sbus bus front-end.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgsix_sbus.c,v 1.30 2009/09/17 16:28:12 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>
#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>
#include <dev/sun/cgsixreg.h>
#include <dev/sun/cgsixvar.h>

/* autoconfiguration driver */
static int	cgsixmatch(device_t, cfdata_t, void *);
static void	cgsixattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cgsix_sbus, sizeof(struct cgsix_softc),
    cgsixmatch, cgsixattach, NULL, NULL);

/*
 * Match a cgsix.
 */
int
cgsixmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_name, sa->sa_name) == 0) ? 100 : 0;
}


/*
 * Attach a cgsix.
 */
void
cgsixattach(device_t parent, device_t self, void *aux)
{
	struct cgsix_softc *sc = device_private(self);
	struct sbus_attach_args *sa = aux;
	struct fbdevice *fb = &sc->sc_fb;
	int node, isconsole;
	const char *name;
	bus_space_handle_t bh;

	/* Remember cookies for cgsix_mmap() */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);
	sc->sc_dev = self;

	node = sa->sa_node;
	
	fb->fb_device = sc->sc_dev;
	fb->fb_type.fb_type = FBTYPE_SUNFAST_COLOR;
	fb->fb_flags = device_cfdata(sc->sc_dev)->cf_flags & FB_USERMASK;
	fb->fb_type.fb_depth = 8;

	fb_setsize_obp(fb, fb->fb_type.fb_depth, 1152, 900, node);

	/*
	 * Dunno what the PROM has mapped, though obviously it must have
	 * the video RAM mapped.  Just map what we care about for ourselves
	 * (the FHC, THC, and Brooktree registers).
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CGSIX_BT_OFFSET,
			 sizeof(*sc->sc_bt),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map brooktree registers\n");
		return;
	}
	sc->sc_bt = (struct bt_regs *)bus_space_vaddr(sa->sa_bustag, bh);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CGSIX_FHC_OFFSET,
			 sizeof(*sc->sc_fhc),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map FHC registers\n");
		return;
	}
	sc->sc_fhc = (int *)bus_space_vaddr(sa->sa_bustag, bh);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CGSIX_THC_OFFSET,
			 sizeof(*sc->sc_thc),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map THC registers\n");
		return;
	}
	sc->sc_thc = (struct cg6_thc *)bus_space_vaddr(sa->sa_bustag, bh);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CGSIX_TEC_OFFSET,
			 sizeof(*sc->sc_tec),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map TEC registers\n");
		return;
	}
	sc->sc_tec = (struct cg6_tec_xxx *)bus_space_vaddr(sa->sa_bustag, bh);

	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CGSIX_FBC_OFFSET,
			 sizeof(*sc->sc_fbc),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map FBC registers\n");
		return;
	}
	sc->sc_fbc = (struct cg6_fbc *)bus_space_vaddr(sa->sa_bustag, bh);

	name = prom_getpropstring(node, "model");

	isconsole = fb_is_console(node);

	/*
	 * we need the address of the framebuffer, no matter if we're console or
	 * not.
	 */
	sc->sc_ramsize = prom_getpropint(node, "fbmapped", 1024 * 1024);
	if (sbus_bus_map(sa->sa_bustag,
			sa->sa_slot,
			sa->sa_offset + CGSIX_RAM_OFFSET,
			sc->sc_ramsize,
			BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_LARGE,
			&bh) != 0) {
		aprint_error_dev(self, "cannot map pixels\n");
		return;
	}
	sc->sc_fb.fb_pixels = (void *)bus_space_vaddr(sa->sa_bustag, bh);

	cg6attach(sc, name, isconsole);
}
