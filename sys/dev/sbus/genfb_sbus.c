/*	$NetBSD: genfb_sbus.c,v 1.11 2014/07/24 21:35:13 riastradh Exp $ */

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

/* an SBus frontend for the generic fb console driver */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfb_sbus.c,v 1.11 2014/07/24 21:35:13 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>

#include <uvm/uvm.h>

#include <sys/bus.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/wsfb/genfbvar.h>

struct genfb_sbus_softc {
	struct genfb_softc sc_gen;
	bus_space_tag_t sc_tag;
	paddr_t sc_paddr;
};

static int	genfb_match_sbus(device_t, cfdata_t, void *);
static void	genfb_attach_sbus(device_t, device_t, void *);
static int	genfb_ioctl_sbus(void *, void *, u_long, void *, int,
				 struct lwp*);
static paddr_t	genfb_mmap_sbus(void *, void *, off_t, int);

CFATTACH_DECL_NEW(genfb_sbus, sizeof(struct genfb_sbus_softc),
    genfb_match_sbus, genfb_attach_sbus, NULL, NULL);

/*
 * Match a graphics device.
 */
static int
genfb_match_sbus(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	/* if there's no address property we can't use the device */
	if (prom_getpropint(sa->sa_node, "address", -1) == -1)
		return 0;
	if (strcmp("display", prom_getpropstring(sa->sa_node, "device_type"))
	    == 0)
		return 1;

	return 0;
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
static void
genfb_attach_sbus(device_t parent, device_t self, void *args)
{
	struct genfb_sbus_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	static const struct genfb_ops zero_ops;
	struct genfb_ops ops = zero_ops;
	prop_dictionary_t dict;
	bus_space_handle_t bh;
	paddr_t fbpa;
	vaddr_t fbva;
	int node = sa->sa_node;
	int isconsole;

	aprint_normal("\n");
	sc->sc_gen.sc_dev = self;
	/* Remember cookies for genfb_mmap_sbus() */
	sc->sc_tag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	/* read geometry information from the device tree */
	sc->sc_gen.sc_width = prom_getpropint(sa->sa_node, "width", 1152);
	sc->sc_gen.sc_height = prom_getpropint(sa->sa_node, "height", 900);
	sc->sc_gen.sc_depth = prom_getpropint(sa->sa_node, "depth", 8);
	sc->sc_gen.sc_stride = prom_getpropint(sa->sa_node, "linebytes",
	    (sc->sc_gen.sc_width * sc->sc_gen.sc_depth + 7) >> 3 );
	sc->sc_gen.sc_fbsize = sc->sc_gen.sc_height * sc->sc_gen.sc_stride;
	fbva = (uint32_t)prom_getpropint(sa->sa_node, "address", 0);
	if (fbva == 0)
		panic("this fb has no address property\n");
	aprint_normal_dev(self, "%d x %d at %d bit\n",
	    sc->sc_gen.sc_width, sc->sc_gen.sc_height, sc->sc_gen.sc_depth);

	pmap_extract(pmap_kernel(), fbva, &fbpa);
	sc->sc_gen.sc_fboffset = (fbpa & 0x01ffffff) - 
	    (sc->sc_paddr & 0x01ffffff);
	aprint_normal_dev(self, "framebuffer at offset 0x%x\n",
	    (uint32_t)sc->sc_gen.sc_fboffset);

#if notyet
	if (sc->sc_gen.sc_depth <= 8) {
		/* setup some ANSIish colour map */
		char boo[256];
		snprintf(boo, 256, "\" pal!\" %x %x %x %x %x call",
		    sa->sa_node, 0, 0xa0, 0xa0, 0);
		prom_interpret(boo);
	}
#endif

	isconsole = fb_is_console(node);
	dict = device_properties(self);
	prop_dictionary_set_bool(dict, "is_console", isconsole);
	
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + sc->sc_gen.sc_fboffset,
			 sc->sc_gen.sc_fbsize,
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map framebuffer\n");
		return;
	}
	sc->sc_gen.sc_fbaddr = (void *)bus_space_vaddr(sa->sa_bustag, bh);

	ops.genfb_ioctl = genfb_ioctl_sbus;
	ops.genfb_mmap = genfb_mmap_sbus;

	genfb_attach(&sc->sc_gen, &ops);
}

static int
genfb_ioctl_sbus(void *v, void *vs, u_long cmd, void *data, int flag,
    struct lwp *l)
{

	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			*(u_int *)data = WSDISPLAY_TYPE_GENFB;
			return 0;
	}

	return EPASSTHROUGH;
}

static paddr_t
genfb_mmap_sbus(void *v, void *vs, off_t offset, int prot)
{
	struct genfb_sbus_softc *sc = v;

	/* regular fb mapping at 0 */
	if ((offset >= 0) && (offset < sc->sc_gen.sc_fbsize)) {

		return bus_space_mmap(sc->sc_tag, sc->sc_paddr,
		    sc->sc_gen.sc_fboffset + offset, prot,
		    BUS_SPACE_MAP_LINEAR);
	}

	return -1;
}
