/*	$NetBSD: hilms.c,v 1.2 2011/02/15 11:05:51 tsutsui Exp $	*/
/*	$OpenBSD: hilms.c,v 1.5 2007/04/10 22:37:17 miod Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <machine/autoconf.h>

#include <dev/hil/hilreg.h>
#include <dev/hil/hilvar.h>
#include <dev/hil/hildevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct hilms_softc {
	struct hildev_softc sc_hildev;

	int		sc_features;
	u_int		sc_buttons;
	u_int		sc_axes;
	int		sc_enabled;
	int		sc_buttonstate;

	device_t	sc_wsmousedev;
};

static int	hilmsprobe(device_t, cfdata_t, void *);
static void	hilmsattach(device_t, device_t, void *);
static int	hilmsdetach(device_t, int);

CFATTACH_DECL_NEW(hilms, sizeof(struct hilms_softc),
    hilmsprobe, hilmsattach, hilmsdetach, NULL);

static int	hilms_enable(void *);
static int	hilms_ioctl(void *, u_long, void *, int, struct lwp *);
static void	hilms_disable(void *);

static const struct wsmouse_accessops hilms_accessops = {
	hilms_enable,
	hilms_ioctl,
	hilms_disable,
};

static void	hilms_callback(struct hildev_softc *, u_int, uint8_t *);

int
hilmsprobe(device_t parent, cfdata_t cf, void *aux)
{
	struct hil_attach_args *ha = aux;

	if (ha->ha_type != HIL_DEVICE_MOUSE)
		return 0;

	/*
	 * Reject anything that has only buttons - they are handled as
	 * keyboards, really.
	 */
	if (ha->ha_infolen > 1 && (ha->ha_info[1] & HIL_AXMASK) == 0)
		return 0;

	return 1;
}

void
hilmsattach(device_t parent, device_t self, void *aux)
{
	struct hilms_softc *sc = device_private(self);
	struct hil_attach_args *ha = aux;
	struct wsmousedev_attach_args a;
	int iob, rx, ry;

	sc->sc_hildev.sc_dev = self;
	sc->hd_code = ha->ha_code;
	sc->hd_type = ha->ha_type;
	sc->hd_infolen = ha->ha_infolen;
	memcpy(sc->hd_info, ha->ha_info, ha->ha_infolen);
	sc->hd_fn = hilms_callback;

	/*
	 * Interpret the identification bytes, if any
	 */
	rx = ry = 0;
	if (ha->ha_infolen > 1) {
		sc->sc_features = ha->ha_info[1];
		sc->sc_axes = sc->sc_features & HIL_AXMASK;

		if (sc->sc_features & HIL_IOB) {
			/* skip resolution bytes */
			iob = 4;
			if (sc->sc_features & HIL_ABSOLUTE) {
				/* skip ranges */
				rx = ha->ha_info[4] | (ha->ha_info[5] << 8);
				if (sc->sc_axes > 1)
					ry = ha->ha_info[6] |
					    (ha->ha_info[7] << 8);
				iob += 2 * sc->sc_axes;
			}

			if (iob >= ha->ha_infolen) {
				sc->sc_features &= ~(HIL_IOB | HILIOB_PIO);
			} else {
				iob = ha->ha_info[iob];
				sc->sc_buttons = iob & HILIOB_BMASK;
				sc->sc_features |= (iob & HILIOB_PIO);
			}
		}
	}

	aprint_normal(", %d axes", sc->sc_axes);
	if (sc->sc_buttons == 1)
		aprint_normal(", 1 button");
	else if (sc->sc_buttons > 1)
		aprint_normal(", %d buttons", sc->sc_buttons);
	if (sc->sc_features & HILIOB_PIO)
		aprint_normal(", pressure sensor");
	if (sc->sc_features & HIL_ABSOLUTE) {
		aprint_normal("\n");
		aprint_normal_dev(self, "%d", rx);
		if (ry != 0)
			aprint_normal("x%d", ry);
		else
			aprint_normal(" linear");
		aprint_normal(" fixed area");
	}

	aprint_normal("\n");

	sc->sc_enabled = 0;

	a.accessops = &hilms_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
hilmsdetach(device_t self, int flags)
{
	struct hilms_softc *sc = device_private(self);

	if (sc->sc_wsmousedev != NULL)
		return config_detach(sc->sc_wsmousedev, flags);

	return 0;
}

int
hilms_enable(void *v)
{
	struct hilms_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	sc->sc_buttonstate = 0;

	return 0;
}

void
hilms_disable(void *v)
{
	struct hilms_softc *sc = v;

	sc->sc_enabled = 0;
}

int
hilms_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
#if 0
	struct hilms_softc *sc = v;
#endif

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(int *)data = WSMOUSE_TYPE_HIL;
		return 0;
	}

	return EPASSTHROUGH;
}

void
hilms_callback(struct hildev_softc *hdsc, u_int buflen, uint8_t *buf)
{
	struct hilms_softc *sc = device_private(hdsc->sc_dev);
	int type, flags;
	int dx, dy, dz, button;
#ifdef DIAGNOSTIC
	int minlen;
#endif

	/*
	 * Ignore packet if we don't need it
	 */
	if (sc->sc_enabled == 0)
		return;

	type = *buf++;

#ifdef DIAGNOSTIC
	/*
	 * Check that the packet contains all the expected data,
	 * ignore it if too short.
	 */
	minlen = 1;
	if (type & HIL_MOUSEMOTION) {
		minlen += sc->sc_axes <<
		    (sc->sc_features & HIL_16_BITS) ? 1 : 0;
	}
	if (type & HIL_MOUSEBUTTON)
		minlen++;

	if (minlen > buflen)
		return;
#endif

	/*
	 * The packet can contain both a mouse motion and a button event.
	 * In this case, the motion data comes first.
	 */

	if (type & HIL_MOUSEMOTION) {
		flags = sc->sc_features & HIL_ABSOLUTE ?
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z : WSMOUSE_INPUT_DELTA;
		if (sc->sc_features & HIL_16_BITS) {
			dx = *buf++;
			dx |= (*buf++) << 8;
			if (!(sc->sc_features & HIL_ABSOLUTE))
				dx = (int16_t)dx;
		} else {
			dx = *buf++;
			if (!(sc->sc_features & HIL_ABSOLUTE))
				dx = (int8_t)dx;
		}
		if (sc->sc_axes > 1) {
			if (sc->sc_features & HIL_16_BITS) {
				dy = *buf++;
				dy |= (*buf++) << 8;
				if (!(sc->sc_features & HIL_ABSOLUTE))
					dy = (int16_t)dy;
			} else {
				dy = *buf++;
				if (!(sc->sc_features & HIL_ABSOLUTE))
					dy = (int8_t)dy;
			}
			if (sc->sc_axes > 2) {
				if (sc->sc_features & HIL_16_BITS) {
					dz = *buf++;
					dz |= (*buf++) << 8;
					if (!(sc->sc_features & HIL_ABSOLUTE))
						dz = (int16_t)dz;
				} else {
					dz = *buf++;
					if (!(sc->sc_features & HIL_ABSOLUTE))
						dz = (int8_t)dz;
				}
			} else
				dz = 0;
		} else
			dy = dz = 0;

		/*
		 * Correct Y direction for button boxes.
		 */
		if ((sc->sc_features & HIL_ABSOLUTE) == 0 &&
		    sc->sc_buttons == 0)
			dy = -dy;
	} else
		dx = dy = dz = flags = 0;

	if (type & HIL_MOUSEBUTTON) {
		button = *buf;
		/*
		 * The pressure sensor is very primitive and only has
		 * a boolean behaviour, as an extra mouse button, which is
		 * down if there is pressure or the pen is near the tablet,
		 * and up if there is no pressure or the pen is far from the
		 * tablet - at least for Tablet id 0x94, P/N 46088B
		 *
		 * The corresponding codes are 0x8f and 0x8e. Convert them
		 * to a pseudo fourth button - even if the tablet never
		 * has three buttons.
		 */
		button = (button - 0x80) >> 1;
		if (button > 4)
			button = 4;

		if (*buf & 1) {
			/* Button released, or no pressure */
			sc->sc_buttonstate &= ~(1 << button);
		} else {
			/* Button pressed, or pressure */
			sc->sc_buttonstate |= (1 << button);
		}
		/* buf++; */
	}
	
	if (sc->sc_wsmousedev != NULL)
		wsmouse_input(sc->sc_wsmousedev,
		    sc->sc_buttonstate, dx, dy, dz, 0, flags);
}
