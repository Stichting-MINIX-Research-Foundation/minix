/*
 * Copyright (c) 2005 Jesse Off.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * The matrix keypad is a primitive type of keying device
 * commonly used in systems as a small, cheap, easy-to-build and rugged
 * way to get user input in a variety of embedded environments.  This
 * driver can work for any size of keypad.  A one key keypad (aka
 * button) can also be used.  The theory of operation is described
 * thusly:
 *
 * 	1) The keypad is connected to the NetBSD embedded system
 * 	with digital I/O (DIO) pins connected to each column of
 * 	the keypad and also to each row of the keypad.
 *
 * 	2) When a button is pressed, a short is made between a
 * 	column line and the intersecting row line.
 *
 * 	3) Software is responsible to poll each row/column individually
 * 	and also to debounce any key presses.
 *
 * To correctly wire up such a thing requires the input DIO
 * lines to have pull-up resistors, otherwise an input may be read as a random
 * value if not currently being shorted by a button press.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: matrixkp_subr.c,v 1.7 2007/10/19 11:59:55 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <machine/autoconf.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/ic/matrixkpvar.h>

#define TV_ELAPSED_US(x, y)	(((x).tv_sec - (y).tv_sec) * 1000000 + \
	((x).tv_usec - (y).tv_usec))

const struct wskbd_accessops mxkp_accessops = {
	mxkp_enable,
	mxkp_set_leds,
	mxkp_ioctl,
};

void
mxkp_attach(struct matrixkp_softc *sc)
{
	u_int32_t i;

	callout_init(&sc->sc_callout, 0);
	callout_setfunc(&sc->sc_callout, mxkp_poll, sc);
	if (sc->poll_freq > hz || sc->poll_freq == 0)
		sc->poll_freq = hz;
	sc->sc_enabled = 0;
	if (sc->debounce_stable_ms == 0)
		sc->sc_flags |= MXKP_NODEBOUNCE;
	if (sc->mxkp_event == NULL)
		sc->mxkp_event = mxkp_wskbd_event;
	FOR_KEYS(i, sc->mxkp_pressed[i] = 0);
}

void
mxkp_poll(void *arg)
{
	struct matrixkp_softc *sc = (struct matrixkp_softc *)arg;
	u_int32_t i, anychanged;
	u_int32_t scanned[(MAXNKEYS + 31) / 32];
	u_int32_t changed[(MAXNKEYS + 31) / 32];
	u_int32_t set[(MAXNKEYS + 31) / 32];
	u_int32_t cleared[(MAXNKEYS + 31) / 32];

rescan:
	anychanged = 0;
	FOR_KEYS(i, scanned[i] = 0);
	sc->mxkp_scankeys(sc, scanned);
	FOR_KEYS(i, changed[i] = sc->mxkp_pressed[i] ^ scanned[i]);
	FOR_KEYS(i, anychanged |= changed[i]);

	if (!(sc->sc_flags & MXKP_NODEBOUNCE) && anychanged) {
		mxkp_debounce(sc, changed, scanned);
		anychanged = 0;
		FOR_KEYS(i, changed[i] &= sc->mxkp_pressed[i] ^ scanned[i]);
		FOR_KEYS(i, anychanged |= changed[i]);
	}
	if (anychanged) {
		FOR_KEYS(i, set[i] = changed[i] & scanned[i]);
		FOR_KEYS(i, cleared[i] = changed[i] & sc->mxkp_pressed[i]);
		sc->mxkp_event(sc, set, cleared);
		FOR_KEYS(i, sc->mxkp_pressed[i] &= ~cleared[i]);
		FOR_KEYS(i, sc->mxkp_pressed[i] |= set[i]);
		goto rescan;
	}
	if (sc->sc_enabled)
		callout_schedule(&sc->sc_callout, hz / sc->poll_freq);
}

/*
 * debounce will return when masked keys have been stable
 * for sc->debounce_stable_ms
 */
void
mxkp_debounce(struct matrixkp_softc *sc, u_int32_t *mask, u_int32_t *scan) {
	struct timeval verystart, start, now;
	u_int32_t last_val[(MAXNKEYS + 31) / 32];
	u_int32_t anyset, i;

	FOR_KEYS(i, last_val[i] = scan[i]);
	microtime(&verystart);
	start = verystart;
	do {
		FOR_KEYS(i, scan[i] = 0);
		sc->mxkp_scankeys(sc, scan);
		microtime(&now);
		anyset = 0;
		FOR_KEYS(i, anyset |= (scan[i] ^ last_val[i]) & mask[i]);
		if (anyset) /* bounce detected */
			start = now;
		FOR_KEYS(i, last_val[i] = scan[i]);
	} while (TV_ELAPSED_US(now, start) <= (sc->debounce_stable_ms * 1000));
}

void
mxkp_wskbd_event(struct matrixkp_softc *sc, u_int32_t *on, u_int32_t *off)
{
	unsigned int i;

	for(i = 0; i < sc->mxkp_nkeys; i++) {
		if (off[i / 32] & (1 << (i % 32))) {
			wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_UP, i);
		}
	}
	for(i = 0; i < sc->mxkp_nkeys; i++) {
		if (on[i / 32] & (1 << (i % 32))) {
			wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_DOWN, i);
		}
	}
}

int
mxkp_enable(void *v, int on)
{
	struct matrixkp_softc *sc = v;

	if (on) {
		if (sc->sc_enabled)
			return EBUSY;

		sc->sc_enabled = 1;
		callout_schedule(&sc->sc_callout, hz / sc->poll_freq);
	} else {
		sc->sc_enabled = 0;
	}

	return 0;
}

void
mxkp_set_leds(void *v, int leds)
{
}

int
mxkp_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_MATRIXKP;
		return 0;
	case WSKBDIO_SETLEDS:
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;
		return 0;
	case WSKBDIO_COMPLEXBELL:
		return 0;
	}
	return EPASSTHROUGH;
}
