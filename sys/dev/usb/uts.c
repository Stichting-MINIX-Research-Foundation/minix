/*	$NetBSD: uts.c,v 1.3 2013/01/05 23:34:21 christos Exp $	*/

/*
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Pierre Pronchery (khorben@defora.org).
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
 *  USB generic Touch Screen driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uts.c,v 1.3 2013/01/05 23:34:21 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/tpcalibvar.h>

#ifdef UTS_DEBUG
#define DPRINTF(x)	if (utsdebug) printf x
#define DPRINTFN(n,x)	if (utsdebug>(n)) printf x
int	utsdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


struct uts_softc {
	struct uhidev sc_hdev;

	struct hid_location sc_loc_x, sc_loc_y, sc_loc_z;
	struct hid_location sc_loc_btn;

	int sc_enabled;

	int flags;		/* device configuration */
#define UTS_ABS		0x1	/* absolute position */

	u_int32_t		sc_buttons;	/* touchscreen button status */
	device_t		sc_wsmousedev;
	struct tpcalib_softc	sc_tpcalib;	/* calibration */
	struct wsmouse_calibcoords sc_calibcoords;

	char sc_dying;
};

#define TSCREEN_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)

Static void	uts_intr(struct uhidev *addr, void *ibuf, u_int len);

Static int	uts_enable(void *);
Static void	uts_disable(void *);
Static int	uts_ioctl(void *, u_long, void *, int, struct lwp *);

Static const struct wsmouse_accessops uts_accessops = {
	uts_enable,
	uts_ioctl,
	uts_disable,
};

Static int	uts_match(device_t, cfdata_t, void *);
Static void	uts_attach(device_t, device_t, void *);
Static void	uts_childdet(device_t, device_t);
Static int	uts_detach(device_t, int);
Static int	uts_activate(device_t, enum devact);

extern struct cfdriver uts_cd;

CFATTACH_DECL2_NEW(uts, sizeof(struct uts_softc), uts_match, uts_attach,
    uts_detach, uts_activate, NULL, uts_childdet);

Static int
uts_match(device_t parent, cfdata_t match, void *aux)
{
	struct uhidev_attach_arg *uha = aux;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCH_SCREEN)) &&
	    !hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_FINGER)))
		return UMATCH_NONE;

	return UMATCH_IFACECLASS;
}

Static void
uts_attach(device_t parent, device_t self, void *aux)
{
	struct uts_softc *sc = device_private(self);
	struct uhidev_attach_arg *uha = aux;
	struct wsmousedev_attach_args a;
	int size;
	void *desc;
	u_int32_t flags;
	struct hid_data * d;
	struct hid_item item;

	aprint_naive("\n");

	sc->sc_hdev.sc_dev = self;
	sc->sc_hdev.sc_intr = uts_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* requires HID usage Generic_Desktop:X */
	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		uha->reportid, hid_input, &sc->sc_loc_x, &flags)) {
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "touchscreen has no X report\n");
		return;
	}
	switch (flags & TSCREEN_FLAGS_MASK) {
	case 0:
		sc->flags |= UTS_ABS;
		break;
	case HIO_RELATIVE:
		break;
	default:
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "X report 0x%04x not supported\n", flags);
		return;
	}

	/* requires HID usage Generic_Desktop:Y */
	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		uha->reportid, hid_input, &sc->sc_loc_y, &flags)) {
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "touchscreen has no Y report\n");
		return;
	}
	switch (flags & TSCREEN_FLAGS_MASK) {
	case 0:
		sc->flags |= UTS_ABS;
		break;
	case HIO_RELATIVE:
		break;
	default:
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "Y report 0x%04x not supported\n", flags);
		return;
	}

	/* requires HID usage Digitizer:Tip_Switch */
	if (!hid_locate(desc, size, HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH),
	    uha->reportid, hid_input, &sc->sc_loc_btn, 0)) {
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "touchscreen has no tip switch report\n");
		return;
	}

	/* requires HID usage Digitizer:In_Range */
	if (!hid_locate(desc, size, HID_USAGE2(HUP_DIGITIZERS, HUD_IN_RANGE),
		uha->reportid, hid_input, &sc->sc_loc_z, &flags)) {
		aprint_error_dev(sc->sc_hdev.sc_dev,
		    "touchscreen has no range report\n");
		return;
	}

	/* multi-touch support would need HUD_CONTACTID and HUD_CONTACTMAX */

#ifdef UTS_DEBUG
	DPRINTF(("uts_attach: sc=%p\n", sc));
	DPRINTF(("uts_attach: X\t%d/%d\n",
		sc->sc_loc_x.pos, sc->sc_loc_x.size));
	DPRINTF(("uts_attach: Y\t%d/%d\n",
		sc->sc_loc_y.pos, sc->sc_loc_y.size));
	DPRINTF(("uts_attach: Z\t%d/%d\n",
		sc->sc_loc_z.pos, sc->sc_loc_z.size));
#endif

	a.accessops = &uts_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

	/* calibrate the touchscreen */
	memset(&sc->sc_calibcoords, 0, sizeof(sc->sc_calibcoords));
	sc->sc_calibcoords.maxx = 4095;
	sc->sc_calibcoords.maxy = 4095;
	sc->sc_calibcoords.samplelen = WSMOUSE_CALIBCOORDS_RESET;
	d = hid_start_parse(desc, size, hid_input);
	while (hid_get_item(d, &item)) {
		if (item.kind != hid_input
		    || HID_GET_USAGE_PAGE(item.usage) != HUP_GENERIC_DESKTOP
		    || item.report_ID != sc->sc_hdev.sc_report_id)
			continue;
		if (HID_GET_USAGE(item.usage) == HUG_X) {
			sc->sc_calibcoords.minx = item.logical_minimum;
			sc->sc_calibcoords.maxx = item.logical_maximum;
		}
		if (HID_GET_USAGE(item.usage) == HUG_Y) {
			sc->sc_calibcoords.miny = item.logical_minimum;
			sc->sc_calibcoords.maxy = item.logical_maximum;
		}
	}
	hid_end_parse(d);

	tpcalib_init(&sc->sc_tpcalib);
	tpcalib_ioctl(&sc->sc_tpcalib, WSMOUSEIO_SCALIBCOORDS,
	    (void *)&sc->sc_calibcoords, 0, 0);

	return;
}

Static int
uts_detach(device_t self, int flags)
{
	struct uts_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF(("uts_detach: sc=%p flags=%d\n", sc, flags));

	if (sc->sc_wsmousedev != NULL)
		rv = config_detach(sc->sc_wsmousedev, flags);

	pmf_device_deregister(self);

	return rv;
}

Static void
uts_childdet(device_t self, device_t child)
{
	struct uts_softc *sc = device_private(self);

	KASSERT(sc->sc_wsmousedev == child);
	sc->sc_wsmousedev = NULL;
}

Static int
uts_activate(device_t self, enum devact act)
{
	struct uts_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static int
uts_enable(void *v)
{
	struct uts_softc *sc = v;

	DPRINTFN(1,("uts_enable: sc=%p\n", sc));

	if (sc->sc_dying)
		return EIO;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	return uhidev_open(&sc->sc_hdev);
}

Static void
uts_disable(void *v)
{
	struct uts_softc *sc = v;

	DPRINTFN(1,("uts_disable: sc=%p\n", sc));
#ifdef DIAGNOSTIC
	if (!sc->sc_enabled) {
		printf("uts_disable: not enabled\n");
		return;
	}
#endif

	sc->sc_enabled = 0;
	uhidev_close(&sc->sc_hdev);
}

Static int
uts_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct uts_softc *sc = v;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		if (sc->flags & UTS_ABS)
			*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		else
			*(u_int *)data = WSMOUSE_TYPE_USB;
		return 0;
	case WSMOUSEIO_SCALIBCOORDS:
	case WSMOUSEIO_GCALIBCOORDS:
		return tpcalib_ioctl(&sc->sc_tpcalib, cmd, data, flag, l);
	}

	return EPASSTHROUGH;
}

Static void
uts_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct uts_softc *sc = (struct uts_softc *)addr;
	int dx, dy, dz;
	u_int32_t buttons = 0;
	int flags, s;

	DPRINTFN(5,("uts_intr: len=%d\n", len));

	flags = WSMOUSE_INPUT_DELTA | WSMOUSE_INPUT_ABSOLUTE_Z;

	dx = hid_get_data(ibuf, &sc->sc_loc_x);
	if (sc->flags & UTS_ABS) {
		flags |= (WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y);
		dy = hid_get_data(ibuf, &sc->sc_loc_y);
		tpcalib_trans(&sc->sc_tpcalib, dx, dy, &dx, &dy);
	} else
		dy = -hid_get_data(ibuf, &sc->sc_loc_y);

	dz = hid_get_data(ibuf, &sc->sc_loc_z);

	if (hid_get_data(ibuf, &sc->sc_loc_btn))
		buttons |= 1;

	if (dx != 0 || dy != 0 || dz != 0 || buttons != sc->sc_buttons) {
		DPRINTFN(10,("uts_intr: x:%d y:%d z:%d buttons:0x%x\n",
		    dx, dy, dz, buttons));
		sc->sc_buttons = buttons;
		if (sc->sc_wsmousedev != NULL) {
			s = spltty();
			wsmouse_input(sc->sc_wsmousedev, buttons, dx, dy, dz, 0,
			    flags);
			splx(s);
		}
	}
}
