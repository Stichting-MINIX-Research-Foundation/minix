/*	$NetBSD: ugensa.c,v 1.31 2013/08/03 16:04:30 soren Exp $	*/

/*
 * Copyright (c) 2004, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell <elric@netbsd.org>.
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
__KERNEL_RCSID(0, "$NetBSD: ugensa.c,v 1.31 2013/08/03 16:04:30 soren Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ucomvar.h>

/* XXXrcd: heh */
#define UGENSA_DEBUG 1

#ifdef UGENSA_DEBUG
#define DPRINTF(x)	if (ugensadebug) printf x
#define DPRINTFN(n,x)	if (ugensadebug>(n)) printf x
int ugensadebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct ugensa_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */
	usbd_interface_handle	sc_iface;	/* interface */

	device_t		sc_subdev;
	int			sc_numcon;

	u_char			sc_dying;
};

struct ucom_methods ugensa_methods = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

#define UGENSA_CONFIG_INDEX	0
#define UGENSA_IFACE_INDEX	0
#define UGENSA_BUFSIZE		1024

struct ugensa_type {
	struct usb_devno	ugensa_dev;
	u_int16_t		ugensa_flags;
#define UNTESTED		0x0001
};

static const struct ugensa_type ugensa_devs[] = {
	{{ USB_VENDOR_AIRPRIME, USB_PRODUCT_AIRPRIME_PC5220 }, 0 },
	{{ USB_VENDOR_DELL, USB_PRODUCT_DELL_HSDPA }, 0 },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_FLEXPACKGPS }, 0 },
	{{ USB_VENDOR_QUALCOMM_K, USB_PRODUCT_QUALCOMM_K_CDMA_MSM_K }, 0 },
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_USB305 }, 0 },
	{{ USB_VENDOR_ZTE, USB_PRODUCT_ZTE_AC8700 }, 0 },

	/*
	 * The following devices are untested, but they are purported to
	 * to work in similar device drivers on other OSes:
	 */

        {{ USB_VENDOR_ANYDATA, USB_PRODUCT_ANYDATA_ADU_500A }, UNTESTED },
        {{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_EXPRESSCARD }, UNTESTED },
	{{ USB_VENDOR_QUALCOMM, USB_PRODUCT_QUALCOMM_MSM_HSDPA }, UNTESTED },
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875 }, UNTESTED },
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625 }, UNTESTED },
};
#define ugensa_lookup(v, p) \
	((const struct ugensa_type *)usb_lookup(ugensa_devs, v, p))

int ugensa_match(device_t, cfdata_t, void *);
void ugensa_attach(device_t, device_t, void *);
void ugensa_childdet(device_t, device_t);
int ugensa_detach(device_t, int);
int ugensa_activate(device_t, enum devact);
extern struct cfdriver ugensa_cd;
CFATTACH_DECL2_NEW(ugensa, sizeof(struct ugensa_softc), ugensa_match,
    ugensa_attach, ugensa_detach, ugensa_activate, NULL, ugensa_childdet);

int 
ugensa_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(20,("ugensa: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

	return (ugensa_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
ugensa_attach(device_t parent, device_t self, void *aux)
{
	struct ugensa_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	const char *devname = device_xname(self);
	usbd_status err;
	struct ucom_attach_args uca;
	int i;

	DPRINTFN(10,("\nugensa_attach: sc=%p\n", sc));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UGENSA_CONFIG_INDEX, 1);
	if (err) {
		aprint_error("\n%s: failed to set configuration, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(dev, UGENSA_IFACE_INDEX, &iface);
	if (err) {
		aprint_error("\n%s: failed to get interface, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	if (ugensa_lookup(uaa->vendor, uaa->product)->ugensa_flags & UNTESTED)
		aprint_normal_dev(self, "WARNING: This device is marked as "
		    "untested. Please submit a report via send-pr(1).\n");

	id = usbd_get_interface_descriptor(iface);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	uca.info = "Generic Serial Device";
	uca.ibufsize = UGENSA_BUFSIZE;
	uca.obufsize = UGENSA_BUFSIZE;
	uca.ibufsizepad = UGENSA_BUFSIZE;
	uca.portno = UCOM_UNK_PORTNO;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = iface;
	uca.methods = &ugensa_methods;
	uca.arg = sc;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	uca.bulkin = uca.bulkout = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		int addr, dir, attr;

		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "could not read endpoint descriptor: %s\n",
			    usbd_errstr(err));
			goto bad;
		}

		addr = ed->bEndpointAddress;
		dir = UE_GET_DIR(ed->bEndpointAddress);
		attr = ed->bmAttributes & UE_XFERTYPE;
		if (attr == UE_BULK) {
			if (uca.bulkin == -1 && dir == UE_DIR_IN) {
				DPRINTF(("%s: Bulk in %d\n", devname, i));
				uca.bulkin = addr;
				continue;
			}
			if (uca.bulkout == -1 && dir == UE_DIR_OUT) {
				DPRINTF(("%s: Bulk out %d\n", devname, i));
				uca.bulkout = addr;
				continue;
			}
		}
		aprint_error_dev(self, "unexpected endpoint\n");
	}
	if (uca.bulkin == -1) {
		aprint_error_dev(self, "Could not find data bulk in\n");
		goto bad;
	}
	if (uca.bulkout == -1) {
		aprint_error_dev(self, "Could not find data bulk out\n");
		goto bad;
	}

	DPRINTF(("ugensa: in=0x%x out=0x%x\n", uca.bulkin, uca.bulkout));
	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
	return;

bad:
	DPRINTF(("ugensa_attach: ATTACH ERROR\n"));
	sc->sc_dying = 1;
	return;
}

void
ugensa_childdet(device_t self, device_t child)
{
	struct ugensa_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

int
ugensa_activate(device_t self, enum devact act)
{
	struct ugensa_softc *sc = device_private(self);

	DPRINTF(("ugensa_activate: sc=%p\n", sc));

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int 
ugensa_detach(device_t self, int flags)
{
	struct ugensa_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF(("ugensa_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	pmf_device_deregister(self);

	if (sc->sc_subdev != NULL)
		rv = config_detach(sc->sc_subdev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}
