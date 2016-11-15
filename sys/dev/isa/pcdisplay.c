/* $NetBSD: pcdisplay.c,v 1.41 2012/10/27 17:18:25 chs Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcdisplay.c,v 1.41 2012/10/27 17:18:25 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/isa/pcdisplayvar.h>

#include <dev/ic/pcdisplay.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include "pcweasel.h"
#if NPCWEASEL > 0
#include <dev/isa/weaselreg.h>
#include <dev/isa/weaselvar.h>
#endif

struct pcdisplay_config {
	struct pcdisplayscreen pcs;
	struct pcdisplay_handle dc_ph;
	int mono;
};

struct pcdisplay_softc {
	struct pcdisplay_config *sc_dc;
	int nscreens;
#if NPCWEASEL > 0
	struct weasel_handle sc_weasel;
#endif
};

static int pcdisplayconsole, pcdisplay_console_attached;
static struct pcdisplay_config pcdisplay_console_dc;

int	pcdisplay_match(device_t, cfdata_t, void *);
void	pcdisplay_attach(device_t, device_t, void *);

static int pcdisplay_is_console(bus_space_tag_t);
static int pcdisplay_probe_col(bus_space_tag_t, bus_space_tag_t);
static int pcdisplay_probe_mono(bus_space_tag_t, bus_space_tag_t);
static void pcdisplay_init(struct pcdisplay_config *,
			     bus_space_tag_t, bus_space_tag_t,
			     int);
static int pcdisplay_allocattr(void *, int, int, int, long *);

CFATTACH_DECL_NEW(pcdisplay, sizeof(struct pcdisplay_softc),
    pcdisplay_match, pcdisplay_attach, NULL, NULL);

const struct wsdisplay_emulops pcdisplay_emulops = {
	pcdisplay_cursor,
	pcdisplay_mapchar,
	pcdisplay_putchar,
	pcdisplay_copycols,
	pcdisplay_erasecols,
	pcdisplay_copyrows,
	pcdisplay_eraserows,
	pcdisplay_allocattr,
	NULL,	/* replaceattr */
};

const struct wsscreen_descr pcdisplay_scr = {
	"80x25", 80, 25,
	&pcdisplay_emulops,
	0, 0, /* no font support */
	WSSCREEN_REVERSE, /* that's minimal... */
	NULL, /* modecookie */
};

const struct wsscreen_descr *_pcdisplay_scrlist[] = {
	&pcdisplay_scr,
};

const struct wsscreen_list pcdisplay_screenlist = {
	sizeof(_pcdisplay_scrlist) / sizeof(struct wsscreen_descr *),
	_pcdisplay_scrlist
};

static int pcdisplay_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t pcdisplay_mmap(void *, void *, off_t, int);
static int pcdisplay_alloc_screen(void *, const struct wsscreen_descr *,
				       void **, int *, int *, long *);
static void pcdisplay_free_screen(void *, void *);
static int pcdisplay_show_screen(void *, void *, int,
				      void (*) (void *, int, int), void *);

const struct wsdisplay_accessops pcdisplay_accessops = {
	pcdisplay_ioctl,
	pcdisplay_mmap,
	pcdisplay_alloc_screen,
	pcdisplay_free_screen,
	pcdisplay_show_screen,
	NULL, /* load_font */
	NULL, /* pollc */
	NULL, /* scroll */
};

static int
pcdisplay_probe_col(bus_space_tag_t iot, bus_space_tag_t memt)
{
	bus_space_handle_t memh, ioh_6845;
	u_int16_t oldval, val;

	if (bus_space_map(memt, 0xb8000, 0x8000, 0, &memh))
		return (0);
	oldval = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	val = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, oldval);
	bus_space_unmap(memt, memh, 0x8000);
	if (val != 0xa55a)
		return (0);

	if (bus_space_map(iot, 0x3d0, 0x10, 0, &ioh_6845))
		return (0);
	bus_space_unmap(iot, ioh_6845, 0x10);

	return (1);
}

static int
pcdisplay_probe_mono(bus_space_tag_t iot, bus_space_tag_t memt)
{
	bus_space_handle_t memh, ioh_6845;
	u_int16_t oldval, val;

	if (bus_space_map(memt, 0xb0000, 0x8000, 0, &memh))
		return (0);
	oldval = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	val = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, oldval);
	bus_space_unmap(memt, memh, 0x8000);
	if (val != 0xa55a)
		return (0);

	if (bus_space_map(iot, 0x3b0, 0x10, 0, &ioh_6845))
		return (0);
	bus_space_unmap(iot, ioh_6845, 0x10);

	return (1);
}

static void
pcdisplay_init(struct pcdisplay_config *dc, bus_space_tag_t iot, bus_space_tag_t memt, int mono)
{
	struct pcdisplay_handle *ph = &dc->dc_ph;
	int cpos;

        ph->ph_iot = iot;
        ph->ph_memt = memt;
	dc->mono = mono;

	if (bus_space_map(memt, mono ? 0xb0000 : 0xb8000, 0x8000,
			  0, &ph->ph_memh))
		panic("pcdisplay_init: cannot map memory");
	if (bus_space_map(iot, mono ? 0x3b0 : 0x3d0, 0x10,
			  0, &ph->ph_ioh_6845))
		panic("pcdisplay_init: cannot map io");

	/*
	 * initialize the only screen
	 */
	dc->pcs.hdl = ph;
	dc->pcs.type = &pcdisplay_scr;
	dc->pcs.active = 1;
	dc->pcs.mem = NULL;

	cpos = pcdisplay_6845_read(ph, cursorh) << 8;
	cpos |= pcdisplay_6845_read(ph, cursorl);

	/* make sure we have a valid cursor position */
	if (cpos < 0 || cpos >= pcdisplay_scr.nrows * pcdisplay_scr.ncols)
		cpos = 0;

	dc->pcs.dispoffset = 0;

	dc->pcs.cursorrow = cpos / pcdisplay_scr.ncols;
	dc->pcs.cursorcol = cpos % pcdisplay_scr.ncols;
	pcdisplay_cursor_init(&dc->pcs, 1);
}

int
pcdisplay_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	int mono;

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* If values are hardwired to something that they can't be, punt. */
	if (ia->ia_nio < 1 ||
	    (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT &&
	     ia->ia_io[0].ir_addr != 0x3d0 &&
	     ia->ia_io[0].ir_addr != 0x3b0))
		return (0);

	if (ia->ia_niomem < 1 ||
	    (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
	     ia->ia_iomem[0].ir_addr != 0xb8000 &&
	     ia->ia_iomem[0].ir_addr != 0xb0000))
		return (0);
	if (ia->ia_iomem[0].ir_size != 0 &&
	    ia->ia_iomem[0].ir_size != 0x8000)
		return (0);

	if (ia->ia_nirq > 0 &&
	    ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ)
		return (0);

	if (ia->ia_ndrq > 0 &&
	    ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ)
		return (0);

	if (pcdisplay_is_console(ia->ia_iot))
		mono = pcdisplay_console_dc.mono;
	else if (ia->ia_io[0].ir_addr != 0x3b0 &&
		 ia->ia_iomem[0].ir_addr != 0xb0000 &&
		 pcdisplay_probe_col(ia->ia_iot, ia->ia_memt))
		mono = 0;
	else if (ia->ia_io[0].ir_addr != 0x3d0 &&
		 ia->ia_iomem[0].ir_addr != 0xb8000 &&
		 pcdisplay_probe_mono(ia->ia_iot, ia->ia_memt))
		mono = 1;
	else
		return (0);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = mono ? 0x3b0 : 0x3d0;
	ia->ia_io[0].ir_size = 0x10;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_size = mono ? 0xb0000 : 0xb8000;
	ia->ia_iomem[0].ir_size = 0x8000;

	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return (1);
}

void
pcdisplay_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct pcdisplay_softc *sc = device_private(self);
	int console;
	struct pcdisplay_config *dc;
	struct wsemuldisplaydev_attach_args aa;

	printf("\n");

	console = pcdisplay_is_console(ia->ia_iot);

	if (console) {
		dc = &pcdisplay_console_dc;
		sc->nscreens = 1;
		pcdisplay_console_attached = 1;
	} else {
		dc = malloc(sizeof(struct pcdisplay_config),
			    M_DEVBUF, M_WAITOK);
		if (ia->ia_io[0].ir_addr != 0x3b0 &&
		    ia->ia_iomem[0].ir_addr != 0xb0000 &&
		    pcdisplay_probe_col(ia->ia_iot, ia->ia_memt))
			pcdisplay_init(dc, ia->ia_iot, ia->ia_memt, 0);
		else if (ia->ia_io[0].ir_addr != 0x3d0 &&
			 ia->ia_iomem[0].ir_addr != 0xb8000 &&
			 pcdisplay_probe_mono(ia->ia_iot, ia->ia_memt))
			pcdisplay_init(dc, ia->ia_iot, ia->ia_memt, 1);
		else
			panic("pcdisplay_attach: display disappeared");
	}
	sc->sc_dc = dc;

#if NPCWEASEL > 0
	/*
	 * If the display is monochrome, check to see if we have
	 * a PC-Weasel, and initialize its special features.
	 */
	if (dc->mono) {
		sc->sc_weasel.wh_st = dc->dc_ph.ph_memt;
		sc->sc_weasel.wh_sh = dc->dc_ph.ph_memh;
		sc->sc_weasel.wh_parent = self;
		weasel_isa_init(&sc->sc_weasel);
	}
#endif /* NPCWEASEL > 0 */

	aa.console = console;
	aa.scrdata = &pcdisplay_screenlist;
	aa.accessops = &pcdisplay_accessops;
	aa.accesscookie = sc;

        config_found(self, &aa, wsemuldisplaydevprint);
}


int
pcdisplay_cnattach(bus_space_tag_t iot, bus_space_tag_t memt)
{
	int mono;

	if (pcdisplay_probe_col(iot, memt))
		mono = 0;
	else if (pcdisplay_probe_mono(iot, memt))
		mono = 1;
	else
		return (ENXIO);

	pcdisplay_init(&pcdisplay_console_dc, iot, memt, mono);

	wsdisplay_cnattach(&pcdisplay_scr, &pcdisplay_console_dc,
			   pcdisplay_console_dc.pcs.cursorcol,
			   pcdisplay_console_dc.pcs.cursorrow,
			   FG_LIGHTGREY | BG_BLACK);

	pcdisplayconsole = 1;
	return (0);
}

static int
pcdisplay_is_console(bus_space_tag_t iot)
{
	if (pcdisplayconsole &&
	    !pcdisplay_console_attached &&
	    bus_space_is_equal(iot, pcdisplay_console_dc.dc_ph.ph_iot))
		return (1);
	return (0);
}

static int
pcdisplay_ioctl(void *v, void *vs, u_long cmd,
    void *data, int flag, struct lwp *l)
{
	/*
	 * XXX "do something!"
	 */
	return (EPASSTHROUGH);
}

static paddr_t
pcdisplay_mmap(void *v, void *vs, off_t offset,
    int prot)
{
	return (-1);
}

static int
pcdisplay_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *defattrp)
{
	struct pcdisplay_softc *sc = v;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = sc->sc_dc;
	*curxp = 0;
	*curyp = 0;
	*defattrp = FG_LIGHTGREY | BG_BLACK;
	sc->nscreens++;
	return (0);
}

static void
pcdisplay_free_screen(void *v, void *cookie)
{
	struct pcdisplay_softc *sc = v;

	if (sc->sc_dc == &pcdisplay_console_dc)
		panic("pcdisplay_free_screen: console");

	sc->nscreens--;
}

static int
pcdisplay_show_screen(void *v, void *cookie,
    int waitok, void (*cb)(void *, int, int),
    void *cbarg)
{
#ifdef DIAGNOSTIC
	struct pcdisplay_softc *sc = v;

	if (cookie != sc->sc_dc)
		panic("pcdisplay_show_screen: bad screen");
#endif
	return (0);
}

static int
pcdisplay_allocattr(void *id, int fg, int bg,
    int flags, long *attrp)
{
	if (flags & WSATTR_REVERSE)
		*attrp = FG_BLACK | BG_LIGHTGREY;
	else
		*attrp = FG_LIGHTGREY | BG_BLACK;
	return (0);
}
