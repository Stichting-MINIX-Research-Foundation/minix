/*	$NetBSD: btms.c,v 1.12 2014/12/13 19:28:55 nonaka Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * based on dev/usb/ums.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btms.c,v 1.12 2014/12/13 19:28:55 nonaka Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>

#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#ifdef BTMS_DEBUG
int btms_debug = 0;
#define	BTMSDBG(s)	if (btms_debug) printf s
#define	BTMSDBGN(n,s)	if (btms_debug > (n)) printf s
#else
#define	BTMSDBG(s)
#define	BTMSDBGN(n,s)
#endif

#define MAX_BUTTONS	31
#define BUTTON(n)	(1 << (((n) == 1 || (n) == 2) ? 3 - (n) : (n)))
#define NOTMOUSE(f)	(((f) & (HIO_CONST | HIO_RELATIVE)) != HIO_RELATIVE)

struct btms_softc {
	struct bthidev		 sc_hidev;	/* device+ */

	device_t		 sc_wsmouse;	/* child */
	int			 sc_enabled;
	uint16_t		 sc_flags;

	/* locators */
	struct hid_location	 sc_loc_x;
	struct hid_location	 sc_loc_y;
	struct hid_location	 sc_loc_z;
	struct hid_location	 sc_loc_w;
	struct hid_location	 sc_loc_button[MAX_BUTTONS];

	int			 sc_num_buttons;
	uint32_t		 sc_buttons;
};

/* sc_flags */
#define BTMS_REVZ		(1 << 0)	/* reverse Z direction */
#define BTMS_HASZ		(1 << 1)	/* has Z direction */
#define BTMS_HASW		(1 << 2)	/* has W direction */

/* autoconf(9) methods */
static int	btms_match(device_t, cfdata_t, void *);
static void	btms_attach(device_t, device_t, void *);
static int	btms_detach(device_t, int);

CFATTACH_DECL_NEW(btms, sizeof(struct btms_softc),
    btms_match, btms_attach, btms_detach, NULL);

/* wsmouse(4) accessops */
static int	btms_wsmouse_enable(void *);
static int	btms_wsmouse_ioctl(void *, unsigned long, void *, int, struct lwp *);
static void	btms_wsmouse_disable(void *);

static const struct wsmouse_accessops btms_wsmouse_accessops = {
	btms_wsmouse_enable,
	btms_wsmouse_ioctl,
	btms_wsmouse_disable,
};

/* bthid methods */
static void btms_input(struct bthidev *, uint8_t *, int);

#ifdef BTMS_DEBUG
static void	btms_print_device(struct btms_softc *);
#endif

/*
 * quirks
 */
static const struct btms_quirk {
	int		vendor;
	int		product;

	uint32_t	flags;
#define	BTMS_QUIRK_ELECOM	__BIT(0)
} btms_quirk_table[] = {
	/* ELECOM M-XG2BB */
	{ 0x056e, 0x00d2, BTMS_QUIRK_ELECOM },
};

static uint32_t
btms_lookup_quirk_flags(int vendor, int product)
{
	const struct btms_quirk *q;
	int i;

	for (i = 0; i < __arraycount(btms_quirk_table); ++i) {
		q = &btms_quirk_table[i];
		if (vendor == q->vendor && product == q->product)
			return q->flags;
	}
	return 0;
}

static void btms_fixup_elecom(struct bthidev_attach_args *,struct btms_softc *);

/*****************************************************************************
 *
 *	btms autoconf(9) routines
 */

static int
btms_match(device_t parent, cfdata_t match, void *aux)
{
	struct bthidev_attach_args *ba = aux;

	if (hid_is_collection(ba->ba_desc, ba->ba_dlen, ba->ba_id,
			    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		return 1;

	return 0;
}

static void
btms_attach(device_t parent, device_t self, void *aux)
{
	struct btms_softc *sc = device_private(self);
	struct bthidev_attach_args *ba = aux;
	struct wsmousedev_attach_args wsma;
	struct hid_location *zloc;
	uint32_t flags, quirks;
	int i, hl;

	ba->ba_input = btms_input;

	quirks = btms_lookup_quirk_flags(ba->ba_vendor, ba->ba_product);

	/* control the horizontal */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_x,
			&flags);

	if (hl == 0 || NOTMOUSE(flags)) {
		aprint_error("X report 0x%04x not supported\n", flags);
		return;
	}

	/* control the vertical */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_y,
			&flags);

	if (hl == 0 || NOTMOUSE(flags)) {
		aprint_error("Y report 0x%04x not supported\n", flags);
		return;
	}

	/* Try the wheel first as the Z activator since it's tradition. */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_z,
			&flags);

	zloc = &sc->sc_loc_z;
	if (hl) {
		if (NOTMOUSE(flags)) {
			aprint_error("Wheel report 0x%04x ignored\n", flags);

			/* ignore Bad Z coord */
			sc->sc_loc_z.size = 0;
		} else {
			sc->sc_flags |= BTMS_HASZ;
			/* Wheels need the Z axis reversed. */
			sc->sc_flags ^= BTMS_REVZ;
			/* Put Z on the W coordinate */
			zloc = &sc->sc_loc_w;
		}
	}

	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
			ba->ba_id,
			hid_input,
			zloc,
			&flags);

	/*
	 * The horizontal component of the scrollball can also be given by
	 * Application Control Pan in the Consumer page, so if we didnt see
	 * any Z then check that.
	 */
	if (!hl) {
		hl = hid_locate(ba->ba_desc,
				ba->ba_dlen,
				HID_USAGE2(HUP_CONSUMER, HUC_AC_PAN),
				ba->ba_id,
				hid_input,
				zloc,
				&flags);
	}

	if (hl) {
		if (NOTMOUSE(flags))
			zloc->size = 0;	/* ignore Z */
		else {
			if (sc->sc_flags & BTMS_HASZ)
				sc->sc_flags |= BTMS_HASW;
			else
				sc->sc_flags |= BTMS_HASZ;
		}
	}

	for (i = 1 ; i <= MAX_BUTTONS ; i++) {
		hl = hid_locate(ba->ba_desc,
				ba->ba_dlen,
				HID_USAGE2(HUP_BUTTON, i),
				ba->ba_id,
				hid_input,
				&sc->sc_loc_button[i - 1],
				NULL);

		if (hl == 0)
			break;
	}
	sc->sc_num_buttons = i - 1;

	if (ISSET(quirks, BTMS_QUIRK_ELECOM))
		btms_fixup_elecom(ba, sc);

	aprint_normal(": %d button%s%s%s%s.\n",
			sc->sc_num_buttons,
			sc->sc_num_buttons == 1 ? "" : "s",
			sc->sc_flags & BTMS_HASW ? ", W" : "",
			sc->sc_flags & BTMS_HASZ ? " and Z dir" : "",
			sc->sc_flags & BTMS_HASW ? "s" : "");
#ifdef BTMS_DEBUG
	if (btms_debug)
		btms_print_device(sc);
#endif

	wsma.accessops = &btms_wsmouse_accessops;
	wsma.accesscookie = sc;

	sc->sc_wsmouse = config_found(self, &wsma, wsmousedevprint);

	pmf_device_register(self, NULL, NULL);
}

static int
btms_detach(device_t self, int flags)
{
	struct btms_softc *sc = device_private(self);
	int err = 0;

	pmf_device_deregister(self);

	if (sc->sc_wsmouse != NULL) {
		err = config_detach(sc->sc_wsmouse, flags);
		sc->sc_wsmouse = NULL;
	}

	return err;
}

/*****************************************************************************
 *
 *	wsmouse(4) accessops
 */

static int
btms_wsmouse_enable(void *cookie)
{
	struct btms_softc *sc = cookie;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	return 0;
}

static int
btms_wsmouse_ioctl(void *cookie, unsigned long cmd, void *data,
    int flag, struct lwp *l)
{

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(uint *)data = WSMOUSE_TYPE_BLUETOOTH;
		break;

	default:
		return EPASSTHROUGH;
	}

	return 0;
}

static void
btms_wsmouse_disable(void *cookie)
{
	struct btms_softc *sc = cookie;

	sc->sc_enabled = 0;
}

/*****************************************************************************
 *
 *	btms input routine, called from our parent
 */

static void
btms_input(struct bthidev *hidev, uint8_t *data, int len)
{
	struct btms_softc *sc = (struct btms_softc *)hidev;
	int dx, dy, dz, dw;
	uint32_t buttons;
	int i, s;

	if (sc->sc_wsmouse == NULL || sc->sc_enabled == 0)
		return;

#ifdef BTMS_DEBUG
	if (btms_debug > 9) {
		printf("%s: data: ", __func__);
		for (i = 0; i < len; ++i) {
			printf("%02x", data[i]);
		}
		printf("\n");
	}
#endif

	dx =  hid_get_data(data, &sc->sc_loc_x);
	dy = -hid_get_data(data, &sc->sc_loc_y);
	dz =  hid_get_data(data, &sc->sc_loc_z);
	dw =  hid_get_data(data, &sc->sc_loc_w);

	if (sc->sc_flags & BTMS_REVZ)
		dz = -dz;

	buttons = 0;
	for (i = 0 ; i < sc->sc_num_buttons ; i++)
		if (hid_get_data(data, &sc->sc_loc_button[i]))
			buttons |= BUTTON(i);

	BTMSDBGN(9,("%s: dx=%d, dy=%d, dz=%d, dw=%d, buttons=0x%08x\n",
	    __func__, dx, dy, dz, dw, buttons));
	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 || buttons != sc->sc_buttons) {
		sc->sc_buttons = buttons;

		s = spltty();
		wsmouse_input(sc->sc_wsmouse,
				buttons,
				dx, dy, dz, dw,
				WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}

#ifdef BTMS_DEBUG
static void
btms_print_device(struct btms_softc *sc)
{
	int i;

	printf("btms: X: pos=%d, size=%d\n",
	    sc->sc_loc_x.pos, sc->sc_loc_x.size);
	printf("btms: Y: pos=%d, size=%d\n",
	    sc->sc_loc_y.pos, sc->sc_loc_y.size);
	if (sc->sc_flags & BTMS_HASZ) {
		printf("btms: Z: pos=%d, size=%d%s\n",
		    sc->sc_loc_z.pos, sc->sc_loc_z.size,
		    ((sc->sc_flags & BTMS_REVZ) ? ", REVZ" : ""));
	}
	if (sc->sc_flags & BTMS_HASW) {
		printf("btms: W: pos=%d, size=%d\n",
		    sc->sc_loc_w.pos, sc->sc_loc_w.size);
	}

	for (i = 0; i < sc->sc_num_buttons; ++i) {
		printf("btms: button%d: pos=%d, size=%d\n", i,
		    sc->sc_loc_button[i].pos, sc->sc_loc_button[i].size);
	}
}
#endif

/*****************************************************************************
 *
 * fixup routines
 */
static void
btms_fixup_elecom(struct bthidev_attach_args *ba, struct btms_softc *sc)
{

	switch (ba->ba_product) {
	case 0x00d2:	/* M-XG2BB */
		/* invalid Wheel and AC_Pan */
		BTMSDBG(("%s: fixup ELECOM M-XG2BB\n", __func__));
		sc->sc_loc_z.pos = 40;
		sc->sc_loc_z.size = 8;
		sc->sc_loc_w.pos = 0;
		sc->sc_loc_w.size = 0;
		sc->sc_flags = BTMS_HASZ | BTMS_REVZ;
		break;
	}
}
