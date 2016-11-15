/*	$NetBSD: cgthree_sbus.c,v 1.30 2010/09/14 18:28:18 macallan Exp $ */

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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgthree) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgthree_sbus.c,v 1.30 2010/09/14 18:28:18 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>
#include <dev/sun/cgthreereg.h>
#include <dev/sun/cgthreevar.h>

#include <dev/sbus/sbusvar.h>


/* autoconfiguration driver */
static int	cgthreematch_sbus(device_t, cfdata_t, void *);
static void	cgthreeattach_sbus(device_t, device_t, void *);

CFATTACH_DECL_NEW(cgthree_sbus, sizeof(struct cgthree_softc),
    cgthreematch_sbus, cgthreeattach_sbus, NULL, NULL);

/*
 * Match a cgthree.
 */
int
cgthreematch_sbus(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	if (strcmp(cf->cf_name, sa->sa_name) == 0)
		return 100;	/* beat genfb(4) */

	return 0;
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgthreeattach_sbus(device_t parent, device_t self, void *args)
{
	struct cgthree_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	struct fbdevice *fb = &sc->sc_fb;
	int node = sa->sa_node;
	int isconsole;
	const char *name;
	bus_space_handle_t bh;

	sc->sc_dev = self;

	/* Remember cookies for cgthree_mmap() */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	fb->fb_device = self;
	fb->fb_flags = device_cfdata(self)->cf_flags & FB_USERMASK;
	fb->fb_type.fb_type = FBTYPE_SUN3COLOR;

	fb->fb_type.fb_depth = 8;
	fb_setsize_obp(fb, fb->fb_type.fb_depth, 1152, 900, node);

	/*
	 * When the ROM has mapped in a cgthree display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + CG3REG_REG,
			 sizeof(struct fbcontrol),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map control registers\n");
		return;
	}
	sc->sc_fbc = (struct fbcontrol *)bus_space_vaddr(sa->sa_bustag, bh);

	isconsole = fb_is_console(node);
	name = prom_getpropstring(node, "model");
	if (name == NULL)
		name = "cgthree";

	if (sa->sa_npromvaddrs != 0)
		fb->fb_pixels = (void *)(u_long)sa->sa_promvaddrs[0];
	if (fb->fb_pixels == NULL) {
		int ramsize = fb->fb_type.fb_height * fb->fb_linebytes;
		if (sbus_bus_map(sa->sa_bustag,
				 sa->sa_slot,
				 sa->sa_offset + CG3REG_MEM,
				 ramsize,
				 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
			aprint_error_dev(self, "cannot map pixels\n");
			return;
		}
		fb->fb_pixels = (char *)bus_space_vaddr(sa->sa_bustag, bh);
	}

	cgthreeattach(sc, name, isconsole);
}
