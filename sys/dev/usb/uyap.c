/*	$NetBSD: uyap.c,v 1.19 2011/12/23 00:51:50 jakllsch Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by  Lennart Augustsson <lennart@augustsson.net>.
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
__KERNEL_RCSID(0, "$NetBSD: uyap.c,v 1.19 2011/12/23 00:51:50 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ezload.h>

const struct ezdata uyap_firmware[] = {
#include "dev/usb/uyap_firmware.h"
};
const struct ezdata *uyap_firmwares[] = { uyap_firmware, NULL };

struct uyap_softc {
	device_t		sc_dev;		/* base device */
};

int             uyap_match(device_t, cfdata_t, void *);
void            uyap_attach(device_t, device_t, void *);
int             uyap_detach(device_t, int);
int             uyap_activate(device_t, enum devact);
extern struct cfdriver uyap_cd;
CFATTACH_DECL_NEW(uyap, sizeof(struct uyap_softc), uyap_match, uyap_attach, uyap_detach, uyap_activate);

int 
uyap_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	/* Match the boot device. */
	if (uaa->vendor == USB_VENDOR_SILICONPORTALS &&
	    uaa->product == USB_PRODUCT_SILICONPORTALS_YAPPH_NF)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void 
uyap_attach(device_t parent, device_t self, void *aux)
{
	struct uyap_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	char *devinfop;

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	aprint_verbose_dev(self, "downloading firmware\n");

	err = ezload_downloads_and_reset(dev, uyap_firmwares);
	if (err) {
		aprint_error_dev(self, "download ezdata error: %s\n",
		    usbd_errstr(err));
		return;
	}

	aprint_verbose_dev(self,
	    "firmware download complete, disconnecting.\n");
	return;
}

int 
uyap_detach(device_t self, int flags)
{
#if 0
	struct uyap_softc *sc = device_private(self);
#endif

	return (0);
}

int
uyap_activate(device_t self, enum devact act)
{
	return 0;
}
