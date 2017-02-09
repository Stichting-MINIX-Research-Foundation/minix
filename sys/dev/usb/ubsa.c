/*	$NetBSD: ubsa.c,v 1.30 2012/02/24 06:48:24 mrg Exp $	*/
/*-
 * Copyright (c) 2002, Alexander Kabaev <kan.FreeBSD.org>.
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
 */
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
__KERNEL_RCSID(0, "$NetBSD: ubsa.c,v 1.30 2012/02/24 06:48:24 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>
#include <dev/usb/ubsavar.h>

#ifdef UBSA_DEBUG
int		ubsadebug = 0;

#define	DPRINTFN(n, x)	do { \
				if (ubsadebug > (n)) \
					printf x; \
			} while (0)
#else
#define	DPRINTFN(n, x)
#endif
#define	DPRINTF(x) DPRINTFN(0, x)

struct	ucom_methods ubsa_methods = {
	ubsa_get_status,
	ubsa_set,
	ubsa_param,
	NULL,
	ubsa_open,
	ubsa_close,
	NULL,
	NULL
};

Static const struct usb_devno ubsa_devs[] = {
	/* BELKIN F5U103 */
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U103 },
	/* BELKIN F5U120 */
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U120 },
	/* GoHubs GO-COM232 */
	{ USB_VENDOR_ETEK, USB_PRODUCT_ETEK_1COM },
	/* GoHubs GO-COM232 */
	{ USB_VENDOR_GOHUBS, USB_PRODUCT_GOHUBS_GOCOM232 },
	/* Peracom */
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_SERIAL1 },
	/* Option N.V. */
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_MC3G },
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_QUADUMTS2 },
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_QUADUMTS },
	/* AnyDATA ADU-E100H */
	{ USB_VENDOR_ANYDATA, USB_PRODUCT_ANYDATA_ADU_E100H },
};
#define ubsa_lookup(v, p) usb_lookup(ubsa_devs, v, p)

int ubsa_match(device_t, cfdata_t, void *);
void ubsa_attach(device_t, device_t, void *);
void ubsa_childdet(device_t, device_t);
int ubsa_detach(device_t, int);
int ubsa_activate(device_t, enum devact);

extern struct cfdriver ubsa_cd;

CFATTACH_DECL2_NEW(ubsa, sizeof(struct ubsa_softc),
    ubsa_match, ubsa_attach, ubsa_detach, ubsa_activate, NULL, ubsa_childdet);

int 
ubsa_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (ubsa_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
ubsa_attach(device_t parent, device_t self, void *aux)
{
	struct ubsa_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	usbd_status err;
	struct ucom_attach_args uca;
	int i;

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

        sc->sc_udev = dev;
	sc->sc_config_index = UBSA_DEFAULT_CONFIG_INDEX;
	sc->sc_numif = 1; /* default device has one interface */

	/*
	 * initialize rts, dtr variables to something
	 * different from boolean 0, 1
	 */
	sc->sc_dtr = -1;
	sc->sc_rts = -1;

	/*
	 * Quad UMTS cards use different requests to
	 * control com settings and only some.
	 */
	sc->sc_quadumts = 0;
	if (uaa->vendor == USB_VENDOR_OPTIONNV) {
		switch (uaa->product) {
		case USB_PRODUCT_OPTIONNV_QUADUMTS:
		case USB_PRODUCT_OPTIONNV_QUADUMTS2:
			sc->sc_quadumts = 1;
			break;
		}
	}

	DPRINTF(("ubsa attach: sc = %p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, sc->sc_config_index, 1);
	if (err) {
		aprint_error_dev(self,
		    "failed to set configuration: %s\n",
		    usbd_errstr(err));
		sc->sc_dying = 1;
		goto error;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(sc->sc_udev);

	if (cdesc == NULL) {
		aprint_error_dev(self,
		    "failed to get configuration descriptor\n");
		sc->sc_dying = 1;
		goto error;
	}

	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* get the interfaces */
	err = usbd_device2interface_handle(dev, UBSA_IFACE_INDEX_OFFSET,
			 &sc->sc_iface[0]);
	if (err) {
		/* can not get main interface */
		sc->sc_dying = 1;
		goto error;
	}

	/* Find the endpoints */
	id = usbd_get_interface_descriptor(sc->sc_iface[0]);
	sc->sc_iface_number[0] = id->bInterfaceNumber;

	/* initialize endpoints */
	uca.bulkin = uca.bulkout = -1;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface[0], i);
		if (ed == NULL) {
			aprint_error_dev(self,
			     "no endpoint descriptor for %d\n", i);
			break;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkin = ed->bEndpointAddress;
			uca.ibufsize = UGETW(ed->wMaxPacketSize);
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkout = ed->bEndpointAddress;
			uca.obufsize = UGETW(ed->wMaxPacketSize);
		}
	} /* end of Endpoint loop */

	if (sc->sc_intr_number == -1) {
		aprint_error_dev(self, "Could not find interrupt in\n");
		sc->sc_dying = 1;
		goto error;
	}

	if (uca.bulkin == -1) {
		aprint_error_dev(self, "Could not find data bulk in\n");
		sc->sc_dying = 1;
		goto error;
	}

	if (uca.bulkout == -1) {
		aprint_error_dev(self, "Could not find data bulk out\n");
		sc->sc_dying = 1;
		goto error;
	}

	uca.portno = 0;
	/* bulkin, bulkout set above */
	uca.ibufsizepad = uca.ibufsize;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = sc->sc_iface[0];
	uca.methods = &ubsa_methods;
	uca.arg = sc;
	uca.info = NULL;
	DPRINTF(("ubsa: int#=%d, in = 0x%x, out = 0x%x, intr = 0x%x\n",
    		i, uca.bulkin, uca.bulkout, sc->sc_intr_number));
	sc->sc_subdevs[0] = config_found_sm_loc(self, "ucombus", NULL, &uca,
				    ucomprint, ucomsubmatch);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	return;

error:
	return;
}


void
ubsa_childdet(device_t self, device_t child)
{
	int i;
	struct ubsa_softc *sc = device_private(self);

	for (i = 0; i < sc->sc_numif; i++) {
		if (sc->sc_subdevs[i] == child)
			break;
	}
	KASSERT(i < sc->sc_numif);
		sc->sc_subdevs[i] = NULL;
}

int 
ubsa_detach(device_t self, int flags)
{
	struct ubsa_softc *sc = device_private(self);
	int i;
	int rv = 0;


	DPRINTF(("ubsa_detach: sc = %p\n", sc));

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

	sc->sc_dying = 1;
	for (i = 0; i < sc->sc_numif; i++) {
		if (sc->sc_subdevs[i] != NULL)
			rv |= config_detach(sc->sc_subdevs[i], flags);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}

int
ubsa_activate(device_t self, enum devact act)
{
	struct ubsa_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}
