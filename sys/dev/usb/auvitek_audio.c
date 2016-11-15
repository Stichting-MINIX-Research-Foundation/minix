/* $NetBSD: auvitek_audio.c,v 1.1 2010/12/27 15:42:11 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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
 * Auvitek AU0828 USB controller - audio support
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek_audio.c,v 1.1 2010/12/27 15:42:11 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

#include "locators.h"

static int	auvitek_ifprint(void *, const char *);

int
auvitek_audio_attach(struct auvitek_softc *sc)
{
	struct usbif_attach_arg uiaa;
	usbd_device_handle udev = sc->sc_udev;
	usb_device_descriptor_t *dd = &udev->ddesc;
	usbd_interface_handle *ifaces;
	int ilocs[USBIFIFCF_NLOCS], nifaces, i;

	nifaces = udev->cdesc->bNumInterface;
	ifaces = kmem_zalloc(nifaces * sizeof(*ifaces), KM_SLEEP);
	if (ifaces == NULL) {
		aprint_error_dev(sc->sc_dev, "audio attach: no memory\n");
		return ENOMEM;
	}
	for (i = 0; i < nifaces; i++) {
		ifaces[i] = &udev->ifaces[i];
	}

	uiaa.device = udev;
	uiaa.port = sc->sc_uport;
	uiaa.vendor = UGETW(dd->idVendor);
	uiaa.product = UGETW(dd->idProduct);
	uiaa.release = UGETW(dd->bcdDevice);
	uiaa.configno = udev->cdesc->bConfigurationValue;
	uiaa.ifaces = ifaces;
	uiaa.nifaces = nifaces;
	ilocs[USBIFIFCF_PORT] = uiaa.port;
	ilocs[USBIFIFCF_VENDOR] = uiaa.vendor;
	ilocs[USBIFIFCF_PRODUCT] = uiaa.product;
	ilocs[USBIFIFCF_RELEASE] = uiaa.release;
	ilocs[USBIFIFCF_CONFIGURATION] = uiaa.configno;

	for (i = 0; i < nifaces; i++) {
		if (!ifaces[i])
			continue;
		uiaa.class = ifaces[i]->idesc->bInterfaceClass;
		uiaa.subclass = ifaces[i]->idesc->bInterfaceSubClass;
		if (uiaa.class != UICLASS_AUDIO)
			continue;
		if (uiaa.subclass != UISUBCLASS_AUDIOCONTROL)
			continue;
		uiaa.iface = ifaces[i];
		uiaa.proto = ifaces[i]->idesc->bInterfaceProtocol;
		uiaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;
		ilocs[USBIFIFCF_INTERFACE] = uiaa.ifaceno;
		sc->sc_audiodev = config_found_sm_loc(sc->sc_dev, "usbifif",
		    ilocs, &uiaa, auvitek_ifprint, config_stdsubmatch);
		if (sc->sc_audiodev)
			break;
	}

	kmem_free(ifaces, nifaces * sizeof(*ifaces));

	return 0;
}

int
auvitek_audio_detach(struct auvitek_softc *sc, int flags)
{
	if (sc->sc_audiodev != NULL) {
		config_detach(sc->sc_audiodev, flags);
		sc->sc_audiodev = NULL;
	}

	return 0;
}

void
auvitek_audio_childdet(struct auvitek_softc *sc, device_t child)
{
	if (sc->sc_audiodev == child)
		sc->sc_audiodev = NULL;
}

static int
auvitek_ifprint(void *opaque, const char *pnp)
{
	struct usbif_attach_arg *uaa = opaque;

	if (pnp)
		return QUIET;

	aprint_normal(" port %d", uaa->port);
	aprint_normal(" configuration %d", uaa->configno);
	aprint_normal(" interface %d", uaa->ifaceno);

	return UNCONF;
}
