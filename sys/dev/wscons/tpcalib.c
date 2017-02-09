/*	$NetBSD: tpcalib.c,v 1.11 2007/03/04 06:02:51 christos Exp $	*/

/*
 * Copyright (c) 1999-2003 TAKEMURA Shin All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tpcalib.c,v 1.11 2007/03/04 06:02:51 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_tpcalib.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/tpcalibvar.h>

#ifdef TPCALIBDEBUG
int	tpcalib_debug = 0;
#define	DPRINTF(arg) if (tpcalib_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

/* mra is defined in mra.c */
extern int mra_Y_AX1_BX2_C(const int *, int,
			   const int *, int, const int *, int, int, int,
			   int *, int *, int *);

#define SCALE	(1024*256)

int
tpcalib_init(struct tpcalib_softc *sc)
{
	tpcalib_reset(sc);
	return (0);
}

void
tpcalib_reset(struct tpcalib_softc *sc)
{
	/* This indicate 'raw mode'. No translation will be done. */
	sc->sc_saved.samplelen = WSMOUSE_CALIBCOORDS_RESET;
}

void
tpcalib_trans(struct tpcalib_softc *sc, int rawx, int rawy, int *x, int *y)
{
	if (sc->sc_saved.samplelen == WSMOUSE_CALIBCOORDS_RESET) {
		/* This indicate 'raw mode'. No translation will be done. */
		*x = rawx;
		*y = rawy;
	} else {
		*x = (sc->sc_ax * rawx + sc->sc_bx * rawy) / SCALE + sc->sc_cx;
		*y = (sc->sc_ay * rawx + sc->sc_by * rawy) / SCALE + sc->sc_cy;
		if (*x < sc->sc_minx) *x = sc->sc_minx;
		if (*y < sc->sc_miny) *y = sc->sc_miny;
		if (sc->sc_maxx < *x)
			*x = sc->sc_maxx;
		if (sc->sc_maxy < *y)
			*y = sc->sc_maxy;
	}
}

int
tpcalib_ioctl(struct tpcalib_softc *sc, u_long cmd, void *data,
    int flag, struct lwp *l)
{
	const struct wsmouse_calibcoords *d;
	int s;

	switch (cmd) {
	case WSMOUSEIO_SCALIBCOORDS:
		s = sizeof(struct wsmouse_calibcoord);
		d = (const struct wsmouse_calibcoords *)data;
		if (d->samplelen == WSMOUSE_CALIBCOORDS_RESET) {
			tpcalib_reset(sc);
		} else
			if (mra_Y_AX1_BX2_C(&d->samples[0].x, s,
				    &d->samples[0].rawx, s,
				    &d->samples[0].rawy, s,
				    d->samplelen, SCALE,
				    &sc->sc_ax, &sc->sc_bx, &sc->sc_cx) ||
			    mra_Y_AX1_BX2_C(&d->samples[0].y, s,
				    &d->samples[0].rawx, s,
				    &d->samples[0].rawy, s,
				    d->samplelen, SCALE,
				    &sc->sc_ay, &sc->sc_by, &sc->sc_cy)) {
				printf("tpcalib: MRA error");
				tpcalib_reset(sc);

				return (EINVAL);
			} else {
				sc->sc_minx = d->minx;
				sc->sc_maxx = d->maxx;
				sc->sc_miny = d->miny;
				sc->sc_maxy = d->maxy;
				sc->sc_saved = *d;
				DPRINTF(("tpcalib: x=%d~%d y=%d~%d\n",
				    sc->sc_minx, sc->sc_maxx,
				    sc->sc_miny, sc->sc_maxy));
				DPRINTF(("tpcalib: Ax=%d Bx=%d Cx=%d\n",
				    sc->sc_ax, sc->sc_bx, sc->sc_cx));
				DPRINTF(("tpcalib: Ay=%d By=%d Cy=%d\n",
				    sc->sc_ay, sc->sc_by, sc->sc_cy));
			}
		break;

	case WSMOUSEIO_GCALIBCOORDS:
		*(struct wsmouse_calibcoords *)data = sc->sc_saved;
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}
