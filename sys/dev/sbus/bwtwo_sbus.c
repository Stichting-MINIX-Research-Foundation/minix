/*	$NetBSD: bwtwo_sbus.c,v 1.29 2009/09/19 04:52:44 tsutsui Exp $ */

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	@(#)bwtwo.c	8.1 (Berkeley) 6/11/93
 */

/*
 * black&white display (bwtwo) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * P4 and overlay plane support by Jason R. Thorpe <thorpej@NetBSD.org>.
 * Overlay plane handling hints and ideas provided by Brad Spencer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bwtwo_sbus.c,v 1.29 2009/09/19 04:52:44 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/eeprom.h>

#include <dev/sbus/sbusvar.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>
#include <dev/sun/btreg.h>
#include <dev/sun/bwtworeg.h>
#include <dev/sun/bwtwovar.h>
#include <dev/sun/pfourreg.h>

/* autoconfiguration driver */
static void	bwtwoattach_sbus (device_t, device_t, void *);
static int	bwtwomatch_sbus (device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(bwtwo_sbus, sizeof(struct bwtwo_softc),
    bwtwomatch_sbus, bwtwoattach_sbus, NULL, NULL);

static int	bwtwo_get_video (struct bwtwo_softc *);
static void	bwtwo_set_video (struct bwtwo_softc *, int);

/*
 * Match a bwtwo.
 */
static int
bwtwomatch_sbus(device_t parent, cfdata_t cf, void *aux)
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
bwtwoattach_sbus(device_t parent, device_t self, void *args)
{
	struct bwtwo_softc *sc = device_private(self);
	struct sbus_attach_args *sa = args;
	struct fbdevice *fb = &sc->sc_fb;
	bus_space_handle_t bh;
	int isconsole, node;
	const char *name;

	sc->sc_dev = self;
	node = sa->sa_node;

	/* Remember cookies for bwtwo_mmap() */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	fb->fb_flags = device_cfdata(self)->cf_flags;
	fb->fb_type.fb_depth = 1;
	fb_setsize_obp(fb, fb->fb_type.fb_depth, 1152, 900, node);

	/*
	 * When the ROM has mapped in a bwtwo display, the address
	 * maps only the video RAM, hence we always map the control
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset + BWREG_REG,
			 sizeof(struct fbcontrol),
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error_dev(self, "cannot map control registers\n");
		return;
	}
	sc->sc_reg = (struct fbcontrol *)bus_space_vaddr(sa->sa_bustag, bh);
	fb->fb_pfour = NULL;

	sc->sc_pixeloffset = BWREG_MEM;

	isconsole = fb_is_console(node);
	name = prom_getpropstring(node, "model");

	/* Assume `bwtwo at sbus' only happens at sun4c's */
	sc->sc_get_video = bwtwo_get_video;
	sc->sc_set_video = bwtwo_set_video;

	if (sa->sa_npromvaddrs != 0)
		sc->sc_fb.fb_pixels = (void *)(u_long)sa->sa_promvaddrs[0];
	if (isconsole && sc->sc_fb.fb_pixels == NULL) {
		int ramsize = fb->fb_type.fb_height * fb->fb_linebytes;
		if (sbus_bus_map(sa->sa_bustag, sa->sa_slot,
				 sa->sa_offset + sc->sc_pixeloffset,
				 ramsize,
				 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
			aprint_error_dev(self, "cannot map pixels\n");
			return;
		}
		sc->sc_fb.fb_pixels = (char *)bus_space_vaddr(sa->sa_bustag, bh);
	}

	bwtwoattach(sc, name, isconsole);
}

static void
bwtwo_set_video(struct bwtwo_softc *sc, int enable)
{

	if (enable)
		/*
		 * If the we're the console the PROM will have taken care
		 * of the FBC_TIMING bit and video control parameters. We
		 * simply turn on FBC_TIMING; in case other video parameters
		 * are necessary, here is a set read from an ELC:
		 * [0xbb,0x2b,0x4,0x14,0xae,0x3,0xa8,0x24,0x1,0x5,0xff,0x1]
		 */
		sc->sc_reg->fbc_ctrl |= FBC_VENAB | FBC_TIMING;
	else
		sc->sc_reg->fbc_ctrl &= ~FBC_VENAB;
}

static int
bwtwo_get_video(struct bwtwo_softc *sc)
{

	return ((sc->sc_reg->fbc_ctrl & FBC_VENAB) != 0);
}
