/*	$NetBSD: hilkbd.c,v 1.3 2011/02/21 12:33:05 he Exp $	*/
/*	$OpenBSD: hilkbd.c,v 1.14 2009/01/21 21:53:59 grange Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "opt_wsdisplay_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <machine/autoconf.h>

#include <dev/hil/hilreg.h>
#include <dev/hil/hilvar.h>
#include <dev/hil/hildevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/hil/hilkbdmap.h>

struct hilkbd_softc {
	struct hildev_softc sc_hildev;

	int		sc_numleds;
	int		sc_ledstate;
	int		sc_enabled;
	int		sc_console;
	int		sc_lastarrow;

	device_t	sc_wskbddev;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int		sc_rawkbd;
	int		sc_nrep;
	char		sc_rep[HILBUFSIZE * 2];
	struct callout	sc_rawrepeat_ch;
#define	REP_DELAY1	400
#define	REP_DELAYN	100
#endif
};

static int	hilkbdprobe(device_t, cfdata_t, void *);
static void	hilkbdattach(device_t, device_t, void *);
static int	hilkbddetach(device_t, int);

CFATTACH_DECL_NEW(hilkbd, sizeof(struct hilkbd_softc),
    hilkbdprobe, hilkbdattach, hilkbddetach, NULL);

static int	hilkbd_enable(void *, int);
static void	hilkbd_set_leds(void *, int);
static int	hilkbd_ioctl(void *, u_long, void *, int, struct lwp *);

static const struct wskbd_accessops hilkbd_accessops = {
	hilkbd_enable,
	hilkbd_set_leds,
	hilkbd_ioctl,
};

static void	hilkbd_cngetc(void *, u_int *, int *);
static void	hilkbd_cnpollc(void *, int);
static void	hilkbd_cnbell(void *, u_int, u_int, u_int);

static const struct wskbd_consops hilkbd_consops = {
	hilkbd_cngetc,
	hilkbd_cnpollc,
	hilkbd_cnbell,
};

static struct wskbd_mapdata hilkbd_keymapdata = {
	hilkbd_keydesctab,
#ifdef HILKBD_LAYOUT
	HILKBD_LAYOUT,
#else
	KB_US,
#endif
};

static struct wskbd_mapdata hilkbd_keymapdata_ps2 = {
	hilkbd_keydesctab_ps2,
#ifdef HILKBD_LAYOUT
	HILKBD_LAYOUT,
#else
	KB_US,
#endif
};

static void	hilkbd_bell(struct hil_softc *, u_int, u_int, u_int);
static void	hilkbd_callback(struct hildev_softc *, u_int, uint8_t *);
static void	hilkbd_decode(struct hilkbd_softc *, uint8_t, u_int *, int *,
		    int);
static int	hilkbd_is_console(int);
#ifdef WSDISPLAY_COMPAT_RAWKBD
static void	hilkbd_rawrepeat(void *);
#endif

static int	seen_hilkbd_console;

int
hilkbdprobe(device_t parent, cfdata_t cf, void *aux)
{
	struct hil_attach_args *ha = aux;

	if (ha->ha_type != HIL_DEVICE_KEYBOARD &&
	    ha->ha_type != HIL_DEVICE_BUTTONBOX)
		return 0;

	return 1;
}

void
hilkbdattach(device_t parent, device_t self, void *aux)
{
	struct hilkbd_softc *sc = device_private(self);
	struct hil_attach_args *ha = aux;
	struct wskbddev_attach_args a;
	uint8_t layoutcode;
	int ps2;

	sc->sc_hildev.sc_dev = self;
	sc->hd_code = ha->ha_code;
	sc->hd_type = ha->ha_type;
	sc->hd_infolen = ha->ha_infolen;
	memcpy(sc->hd_info, ha->ha_info, ha->ha_infolen);
	sc->hd_fn = hilkbd_callback;

	if (ha->ha_type == HIL_DEVICE_KEYBOARD) {
		/*
		 * Determine the keyboard language configuration, but don't
		 * override a user-specified setting.
		 */
		layoutcode = ha->ha_id & (MAXHILKBDLAYOUT - 1);
#ifndef HILKBD_LAYOUT
		if (layoutcode < MAXHILKBDLAYOUT &&
		    hilkbd_layouts[layoutcode] != -1)
			hilkbd_keymapdata.layout =
			hilkbd_keymapdata_ps2.layout =
			    hilkbd_layouts[layoutcode];
#endif

		aprint_normal(", layout %x", layoutcode);
	}

	/*
	 * Interpret the identification bytes, if any
	 */
	if (ha->ha_infolen > 2 && (ha->ha_info[1] & HIL_IOB) != 0) {
		/* HILIOB_PROMPT is not always reported... */
		sc->sc_numleds = (ha->ha_info[2] & HILIOB_PMASK) >> 4;
		if (sc->sc_numleds != 0)
			aprint_normal(", %d leds", sc->sc_numleds);
	}

	aprint_normal("\n");

	/*
	 * Red lettered keyboards come in two flavours, the old one
	 * with only one control key, to the left of the escape key,
	 * and the modern one which has a PS/2 like layout, and leds.
	 *
	 * Unfortunately for us, they use the same device ID range.
	 * We'll differentiate them by looking at the leds property.
	 */
	ps2 = (sc->sc_numleds != 0);

#ifdef WSDISPLAY_COMPAT_RAWKBD
	callout_init(&sc->sc_rawrepeat_ch, 0);
	callout_setfunc(&sc->sc_rawrepeat_ch, hilkbd_rawrepeat, sc);
#endif

	/* Do not consider button boxes as console devices. */
	if (ha->ha_type == HIL_DEVICE_BUTTONBOX)
		a.console = 0;
	else
		a.console = hilkbd_is_console(ha->ha_console);
	a.keymap = ps2 ? &hilkbd_keymapdata_ps2 : &hilkbd_keymapdata;
	a.accessops = &hilkbd_accessops;
	a.accesscookie = sc;

	if (a.console) {
		sc->sc_console = sc->sc_enabled = 1;
		wskbd_cnattach(&hilkbd_consops, sc, a.keymap);
	} else {
		sc->sc_console = sc->sc_enabled = 0;
	}

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);

	/*
	 * If this is an old keyboard with a numeric pad but no ``num lock''
	 * key, simulate it being pressed so that the keyboard runs in
	 * numeric mode.
	 */
	if (!ps2 && sc->sc_wskbddev != NULL) {
		wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_DOWN, 80);
		wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_UP, 80);
	}
}

int
hilkbddetach(device_t self, int flags)
{
	struct hilkbd_softc *sc = device_private(self);

	/*
	 * Handle console keyboard for the best. It should have been set
	 * as the first device in the loop anyways.
	 */
	if (sc->sc_console) {
		wskbd_cndetach();
		seen_hilkbd_console = 0;
	}

	if (sc->sc_wskbddev != NULL)
		return config_detach(sc->sc_wskbddev, flags);

	return 0;
}

int
hilkbd_enable(void *v, int on)
{
	struct hilkbd_softc *sc = v;

	if (on) {
		if (sc->sc_enabled)
			return EBUSY;
	} else {
		if (sc->sc_console)
			return EBUSY;
	}

	sc->sc_enabled = on;

	return 0;
}

void
hilkbd_set_leds(void *v, int leds)
{
	struct hilkbd_softc *sc = v;
	struct hildev_softc *hdsc = &sc->sc_hildev;
	int changemask;

	if (sc->sc_numleds == 0)
		return;

	changemask = leds ^ sc->sc_ledstate;
	if (changemask == 0)
		return;

	/* We do not handle more than 3 leds here */
	if (changemask & WSKBD_LED_SCROLL)
		send_hildev_cmd(hdsc,
		    (leds & WSKBD_LED_SCROLL) ? HIL_PROMPT1 : HIL_ACK1,
		    NULL, NULL);
	if (changemask & WSKBD_LED_NUM)
		send_hildev_cmd(hdsc,
		    (leds & WSKBD_LED_NUM) ? HIL_PROMPT2 : HIL_ACK2,
		    NULL, NULL);
	if (changemask & WSKBD_LED_CAPS)
		send_hildev_cmd(hdsc,
		    (leds & WSKBD_LED_CAPS) ? HIL_PROMPT3 : HIL_ACK3,
		    NULL, NULL);

	sc->sc_ledstate = leds;
}

int
hilkbd_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct hilkbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_HIL;
		return 0;
	case WSKBDIO_SETLEDS:
		hilkbd_set_leds(v, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_ledstate;
		return 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		callout_stop(&sc->sc_rawrepeat_ch);
		return 0;
#endif
	case WSKBDIO_COMPLEXBELL:
#define	d ((struct wskbd_bell_data *)data)
		hilkbd_bell(device_private(device_parent(sc->sc_hildev.sc_dev)),
		    d->pitch, d->period, d->volume);
#undef d
		return 0;
	}

	return EPASSTHROUGH;
}

void
hilkbd_cngetc(void *v, u_int *type, int *data)
{
	struct hilkbd_softc *sc = v;
	struct hildev_softc *hdsc = &sc->sc_hildev;
	uint8_t c, stat;

	for (;;) {
		while (hil_poll_data(hdsc, &stat, &c) != 0)
			;

		/*
		 * Disregard keyboard data packet header.
		 * Note that no key generates it, so we're safe.
		 */
		if (c != HIL_KBDBUTTON)
			break;
	}

	hilkbd_decode(sc, c, type, data, HIL_KBDBUTTON);
}

void
hilkbd_cnpollc(void *v, int on)
{
	struct hilkbd_softc *sc = v;

	hil_set_poll(device_private(device_parent(sc->sc_hildev.sc_dev)), on);
}

void
hilkbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
	struct hilkbd_softc *sc = v;

	hilkbd_bell(device_private(device_parent(sc->sc_hildev.sc_dev)),
	    pitch, period, volume);
}

void
hilkbd_bell(struct hil_softc *sc, u_int pitch, u_int period, u_int volume)
{
	uint8_t buf[2];

	/* XXX there could be at least a pitch -> HIL pitch conversion here */
#define	BELLDUR		80	/* tone duration in msec (10-2560) */
#define	BELLFREQ	8	/* tone frequency (0-63) */
	buf[0] = ar_format(period - 10);
	buf[1] = BELLFREQ;
	send_hil_cmd(sc, HIL_SETTONE, buf, 2, NULL);
}

void
hilkbd_callback(struct hildev_softc *hdsc, u_int buflen, uint8_t *buf)
{
	struct hilkbd_softc *sc = device_private(hdsc->sc_dev);
	u_int type;
	int kbdtype, key;
	int i, s;

	/*
	 * Ignore packet if we don't need it
	 */
	if (sc->sc_enabled == 0)
		return;

	if (buflen == 0)
		return;
	switch ((kbdtype = *buf & HIL_KBDDATA)) {
	case HIL_BUTTONBOX:
	case HIL_KBDBUTTON:
		break;
	default:
		return;
	}

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		uint8_t cbuf[HILBUFSIZE * 2];
		int c, j, npress;

		npress = j = 0;
		for (i = 1, buf++; i < buflen; i++) {
			hilkbd_decode(sc, *buf++, &type, &key, kbdtype);
			c = hilkbd_raw[key];
			if (c == 0)
				continue;
			/* fake extended scancode if necessary */
			if (c & 0x80)
				cbuf[j++] = 0xe0;
			cbuf[j] = c & 0x7f;
			if (type == WSCONS_EVENT_KEY_UP)
				cbuf[j] |= 0x80;
			else {
				/* remember pressed keys for autorepeat */
				if (c & 0x80)
					sc->sc_rep[npress++] = 0xe0;
				sc->sc_rep[npress++] = c & 0x7f;
			}
			j++;
		}

		s = spltty();
		wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		splx(s);
		callout_stop(&sc->sc_rawrepeat_ch);
		sc->sc_nrep = npress;
		if (npress != 0) {
			callout_schedule(&sc->sc_rawrepeat_ch,
			    mstohz(REP_DELAY1));
		}
	} else
#endif
	{
		s = spltty();
		for (i = 1, buf++; i < buflen; i++) {
			hilkbd_decode(sc, *buf++, &type, &key, kbdtype);
			if (sc->sc_wskbddev != NULL)
				wskbd_input(sc->sc_wskbddev, type, key);
		}
		splx(s);
	}
}

void
hilkbd_decode(struct hilkbd_softc *sc, uint8_t data, u_int *type, int *key,
    int kbdtype)
{

	if (kbdtype == HIL_BUTTONBOX) {
		if (data == 0x02)	/* repeat arrow */
			data = sc->sc_lastarrow;
		else if (data >= 0xf8)
			sc->sc_lastarrow = data;
	}

	*type = (data & 1) ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*key = data >> 1;
}

int
hilkbd_is_console(int hil_is_console)
{

	/* if not first hil keyboard, then not the console */
	if (seen_hilkbd_console)
		return 0;

	/* if PDC console does not match hil bus path, then not the console */
	if (hil_is_console == 0)
		return 0;

	seen_hilkbd_console = 1;
	return 1;
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
hilkbd_rawrepeat(void *v)
{
	struct hilkbd_softc *sc = v;
	int s;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	splx(s);
	callout_schedule(&sc->sc_rawrepeat_ch, mstohz(REP_DELAYN));
}
#endif
