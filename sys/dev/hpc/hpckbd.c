/*	$NetBSD: hpckbd.c,v 1.30 2012/10/27 17:18:17 chs Exp $ */

/*-
 * Copyright (c) 1999-2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
__KERNEL_RCSID(0, "$NetBSD: hpckbd.c,v 1.30 2012/10/27 17:18:17 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/tty.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <machine/config_hook.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>

#include "opt_wsdisplay_compat.h"
#include "opt_pckbd_layout.h"
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/pckbport/wskbdmap_mfii.h>
#ifdef WSDISPLAY_COMPAT_RAWKBD
#include <dev/hpc/pckbd_encode.h>
#endif

#include <dev/hpc/hpckbdvar.h>
#include <dev/hpc/hpckbdkeymap.h>

struct hpckbd_softc;

#define NEVENTQ 32
struct hpckbd_eventq {
	u_int	hq_type;
	int	hq_data;
};

struct hpckbd_core {
	struct hpckbd_if	hc_if;
	struct hpckbd_ic_if	*hc_ic;
	const uint8_t		*hc_keymap;
	const int		*hc_special;
	int			hc_polling;
	int			hc_console;
#define NEVENTQ 32
	struct hpckbd_eventq	hc_eventq[NEVENTQ];
	struct hpckbd_eventq	*hc_head, *hc_tail;
	int			hc_nevents;
	int			hc_enabled;
	device_t		hc_wskbddev;
	struct hpckbd_softc	*hc_sc;	/* back link */
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int			hc_rawkbd;
#endif
};

struct hpckbd_softc {
	device_t		sc_dev;
	struct hpckbd_core	*sc_core;
	struct hpckbd_core	sc_coredata;
};

int	hpckbd_match(device_t, cfdata_t, void *);
void	hpckbd_attach(device_t, device_t, void *);

void	hpckbd_initcore(struct hpckbd_core *, struct hpckbd_ic_if *, int);
void	hpckbd_initif(struct hpckbd_core *);
int	hpckbd_getevent(struct hpckbd_core *, u_int *, int *);
int	hpckbd_putevent(struct hpckbd_core *, u_int, int);
void	hpckbd_keymap_lookup(struct hpckbd_core*);
void	hpckbd_keymap_setup(struct hpckbd_core *, const keysym_t *, int);
int	__hpckbd_input(void *, int, int);
void	__hpckbd_input_hook(void *);

CFATTACH_DECL_NEW(hpckbd, sizeof(struct hpckbd_softc),
    hpckbd_match, hpckbd_attach, NULL, NULL);

/* wskbd accessopts */
int	hpckbd_enable(void *, int);
void	hpckbd_set_leds(void *, int);
int	hpckbd_ioctl(void *, u_long, void *, int, struct lwp *);

/* consopts */
struct	hpckbd_core hpckbd_consdata;
void	hpckbd_cngetc(void *, u_int *, int*);
void	hpckbd_cnpollc(void *, int);

const struct wskbd_accessops hpckbd_accessops = {
	hpckbd_enable,
	hpckbd_set_leds,
	hpckbd_ioctl,
};

const struct wskbd_consops hpckbd_consops = {
	hpckbd_cngetc,
	hpckbd_cnpollc,
	NULL,
};

struct wskbd_mapdata hpckbd_keymapdata = {
	pckbd_keydesctab,
#ifdef PCKBD_LAYOUT
	PCKBD_LAYOUT
#else
	KB_US
#endif
};

int
hpckbd_match(device_t parent, cfdata_t cf, void *aux)
{
	return (1);
}

void
hpckbd_attach(device_t parent, device_t self, void *aux)
{
	struct hpckbd_attach_args *haa = aux;
	struct hpckbd_softc *sc = device_private(self);
	struct hpckbd_ic_if *ic = haa->haa_ic;
	struct wskbddev_attach_args wa;

	sc->sc_dev = self;

	/*
	 * Initialize core if it isn't console
	 */
	if (hpckbd_consdata.hc_ic == ic) {
		sc->sc_core = &hpckbd_consdata;
		/* The core has been initialized in hpckbd_cnattach. */
	} else {
		sc->sc_core = &sc->sc_coredata;
		hpckbd_initcore(sc->sc_core, ic, 0 /* not console */);
	}

	if (sc->sc_core->hc_keymap == default_keymap)
		printf(": no keymap.");

	printf("\n");

	/*
	 * setup hpckbd public interface for parent controller.
	 */
	hpckbd_initif(sc->sc_core);

	/*
	 * attach wskbd
	 */
	wa.console = sc->sc_core->hc_console;
	wa.keymap = &hpckbd_keymapdata;
	wa.accessops = &hpckbd_accessops;
	wa.accesscookie = sc->sc_core;
	sc->sc_core->hc_wskbddev = config_found(self, &wa, wskbddevprint);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "unable to establish power handler\n");
}

int
hpckbd_print(void *aux, const char *pnp)
{
	return (pnp ? QUIET : UNCONF);
}

void
hpckbd_initcore(struct hpckbd_core *hc, struct hpckbd_ic_if *ic, int console)
{
	hc->hc_polling = 0;
	hc->hc_console = console;
	hc->hc_ic = ic;

	/* setup event queue */
	hc->hc_head = hc->hc_tail = hc->hc_eventq;
	hc->hc_nevents = 0;

	hpckbd_keymap_lookup(hc);
}

void
hpckbd_initif(struct hpckbd_core *hc)
{
	struct hpckbd_if *kbdif = &hc->hc_if;

	kbdif->hi_ctx = hc;
	kbdif->hi_input = __hpckbd_input;
	kbdif->hi_input_hook = __hpckbd_input_hook;
	hpckbd_ic_establish(hc->hc_ic, &hc->hc_if);
}

int
hpckbd_putevent(struct hpckbd_core* hc, u_int type, int data)
{
	int s = spltty();

	if (hc->hc_nevents == NEVENTQ) {
		splx(s);
		return (0); /* queue is full */
	}

	hc->hc_nevents++;
	hc->hc_tail->hq_type = type;
	hc->hc_tail->hq_data = data;
	if (&hc->hc_eventq[NEVENTQ] <= ++hc->hc_tail)
		hc->hc_tail = hc->hc_eventq;
	splx(s);

	return (1);
}

int
hpckbd_getevent(struct hpckbd_core* hc, u_int *type, int *data)
{
	int s = spltty();

	if (hc->hc_nevents == 0) {
		splx(s);
		return (0); /* queue is empty */
	}

	*type = hc->hc_head->hq_type;
	*data = hc->hc_head->hq_data;
	hc->hc_nevents--;
	if (&hc->hc_eventq[NEVENTQ] <= ++hc->hc_head)
		hc->hc_head = hc->hc_eventq;
	splx(s);

	return (1);
}

void
hpckbd_keymap_setup(struct hpckbd_core *hc,
		    const keysym_t *map, int mapsize)
{
	int i;
	struct wscons_keydesc *desc;

	/* fix keydesc table */
	/* 
	 * XXX The way this is done is really wrong.  The __UNCONST()
	 * is a hint as to what is wrong.  This actually ends up modifying
	 * initialized data which is marked "const".
	 * The reason we get away with it here is apparently that text
	 * and read-only data gets mapped read/write on the platforms
	 * using this code.
	 */
	desc = (struct wscons_keydesc *)__UNCONST(hpckbd_keymapdata.keydesc);
	for (i = 0; desc[i].name != 0; i++) {
		if ((desc[i].name & KB_MACHDEP) && desc[i].map == NULL) {
			desc[i].map = map;
			desc[i].map_size = mapsize;
		}
	}

	return;
}

void
hpckbd_keymap_lookup(struct hpckbd_core *hc)
{
	const struct hpckbd_keymap_table *tab;
	platid_mask_t mask;

	for (tab = hpckbd_keymap_table; tab->ht_platform != NULL; tab++) {

		mask = PLATID_DEREF(tab->ht_platform);

		if (platid_match(&platid, &mask)) {
			hc->hc_keymap = tab->ht_keymap;
			hc->hc_special = tab->ht_special;
#if !defined(PCKBD_LAYOUT)
			hpckbd_keymapdata.layout = tab->ht_layout;
#endif
			if (tab->ht_cmdmap.map) {
				hpckbd_keymap_setup(hc, tab->ht_cmdmap.map,
				    tab->ht_cmdmap.size);
#if !defined(PCKBD_LAYOUT)
				hpckbd_keymapdata.layout |= KB_MACHDEP;
#endif
			} else {
				hpckbd_keymapdata.layout &= ~KB_MACHDEP;
			}
			return;
		}
	}

	/* no keymap. use default. */
	hc->hc_keymap = default_keymap;
	hc->hc_special = default_special_keymap;
#if !defined(PCKBD_LAYOUT)
	hpckbd_keymapdata.layout = KB_US;
#endif
}

void
__hpckbd_input_hook(void *arg)
{
#if 0
	struct hpckbd_core *hc = arg;

	if (hc->hc_polling) {
		hc->hc_type = WSCONS_EVENT_ALL_KEYS_UP;
	}
#endif
}

int
__hpckbd_input(void *arg, int flag, int scancode)
{
	struct hpckbd_core *hc = arg;
	int type, key;

	if (flag) {
		type = WSCONS_EVENT_KEY_DOWN;
	} else {
		type = WSCONS_EVENT_KEY_UP;
	}

	key = hc->hc_keymap[scancode];
	if (key == UNK) {
#ifdef DEBUG
		printf("hpckbd: unknown scan code %#x (%d, %d)\n",
		    scancode, scancode >> 3,
		    scancode - ((scancode >> 3) << 3));
#endif /* DEBUG */
		return (0);
	}

	if (key == IGN) {
		return (0);
	}

	if (key == SPL) {
		if (!flag)
			return (0);

		if (scancode == hc->hc_special[KEY_SPECIAL_OFF]) {
			config_hook_call(CONFIG_HOOK_BUTTONEVENT,
			    CONFIG_HOOK_BUTTONEVENT_POWER, (void *)1 /* on */);
		} else if (scancode == hc->hc_special[KEY_SPECIAL_LIGHT]) {
			static int onoff; /* XXX -uch */
			config_hook_call(CONFIG_HOOK_BUTTONEVENT,
			    CONFIG_HOOK_BUTTONEVENT_LIGHT,
			    (void *)(onoff ^= 1));
		} else {
#ifdef DEBUG
			printf("unknown special key %d\n", scancode);
#endif
		}

		return (0);
	}

	if (hc->hc_polling) {
		if (hpckbd_putevent(hc, type, key) == 0)
			printf("hpckbd: queue over flow\n");
	} else {
#ifdef WSDISPLAY_COMPAT_RAWKBD
		if (hc->hc_rawkbd) {
			int n;
			u_char data[16];
			n = pckbd_encode(type, key, data);
			wskbd_rawinput(hc->hc_wskbddev, data, n);
		} else
#endif
			wskbd_input(hc->hc_wskbddev, type, key);
	}

	return (0);
}

/*
 * console support routines
 */
int
hpckbd_cnattach(struct hpckbd_ic_if *ic)
{
	struct hpckbd_core *hc = &hpckbd_consdata;

	hpckbd_initcore(hc, ic, 1 /* console */);

	/* attach controller */
	hpckbd_initif(hc);

	/* attach wskbd */
	wskbd_cnattach(&hpckbd_consops, hc, &hpckbd_keymapdata);

	return (0);
}

void
hpckbd_cngetc(void *arg, u_int *type, int *data)
{
	struct hpckbd_core *hc = arg;

	if (!hc->hc_console || !hc->hc_polling || !hc->hc_ic)
		return;

	while (hpckbd_getevent(hc, type, data) == 0) /* busy loop */
		hpckbd_ic_poll(hc->hc_ic);
}

void
hpckbd_cnpollc(void *arg, int on)
{
	struct hpckbd_core *hc = arg;

	hc->hc_polling = on;
}

int
hpckbd_enable(void *arg, int on)
{
	struct hpckbd_core *hc = arg;

	if (on) {
		if (hc->hc_enabled)
			return (EBUSY);
		hc->hc_enabled = 1;
	} else {
		if (hc->hc_console)
			return (EBUSY);
		hc->hc_enabled = 0;
	}

	return (0);
}

void
hpckbd_set_leds(void *arg, int leds)
{
	/* Can you find any LED which tells you about keyboard? */
}

int
hpckbd_ioctl(void *arg, u_long cmd, void *data, int flag,
	     struct lwp *l)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct hpckbd_core *hc = arg;
#endif
	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_HPC_KBD;
		return (0);
	case WSKBDIO_SETLEDS:
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;	/* dummy for wsconsctl(8) */
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		hc->hc_rawkbd = (*(int *)data == WSKBD_RAW);
		return (0);
#endif
	}
	return (EPASSTHROUGH);
}
