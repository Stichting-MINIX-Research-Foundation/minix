/*	$NetBSD: uhmodem.c,v 1.13 2011/12/23 00:51:46 jakllsch Exp $	*/

/*
 * Copyright (c) 2008 Yojiro UO <yuo@nui.org>.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
__KERNEL_RCSID(0, "$NetBSD: uhmodem.c,v 1.13 2011/12/23 00:51:46 jakllsch Exp $");

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
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/usbcdc.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/ucomvar.h>
#include <dev/usb/ubsavar.h>

/* vendor specific bRequest */
#define	UHMODEM_REGWRITE	0x20
#define	UHMODEM_REGREAD		0x21
#define UHMODEM_SETUP		0x22

#define UHMODEMIBUFSIZE	4096
#define UHMODEMOBUFSIZE	4096

#ifdef UHMODEM_DEBUG
Static int	uhmodemdebug = 0;
#define DPRINTFN(n, x)  do { \
				if (uhmodemdebug > (n)) \
					printf x; \
			} while (0)
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

Static int uhmodem_open(void *, int);
Static  usbd_status e220_modechange_request(usbd_device_handle);
Static	usbd_status uhmodem_endpointhalt(struct ubsa_softc *, int);
Static	usbd_status uhmodem_regwrite(usbd_device_handle, uint8_t *, size_t);
Static	usbd_status uhmodem_regread(usbd_device_handle, uint8_t *, size_t);
Static  usbd_status a2502_init(usbd_device_handle);
#if 0
Static	usbd_status uhmodem_regsetup(usbd_device_handle, uint16_t);
Static  usbd_status e220_init(usbd_device_handle);
#endif

struct	uhmodem_softc {
	struct ubsa_softc	sc_ubsa;	
};

struct	ucom_methods uhmodem_methods = {
	ubsa_get_status,
	ubsa_set,
	ubsa_param,
	NULL,
	uhmodem_open,
	ubsa_close,
	NULL,
	NULL
};

struct uhmodem_type {
	struct usb_devno	uhmodem_dev;
	u_int16_t		uhmodem_coms;	/* number of serial interfaces on the device */
	u_int16_t		uhmodem_flags;
#define	E220	0x0001
#define	A2502	0x0002
#define	E620	0x0004		/* XXX */
				/* Whether or not it is a device different from E220 is not clear. */
};

Static const struct uhmodem_type uhmodem_devs[] = {
	/* HUAWEI E220 / Emobile D0[12]HW */
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220 }, 2,	E220},
	/* ANYDATA / NTT DoCoMo A2502 */
	{{ USB_VENDOR_ANYDATA, USB_PRODUCT_ANYDATA_A2502 }, 3,	A2502},
	/* HUAWEI E620 */
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE }, 3,   E620},
};
#define uhmodem_lookup(v, p) ((const struct uhmodem_type *)usb_lookup(uhmodem_devs, v, p))

int uhmodem_match(device_t, cfdata_t, void *);
void uhmodem_attach(device_t, device_t, void *);
void uhmodem_childdet(device_t, device_t);
int uhmodem_detach(device_t, int);
int uhmodem_activate(device_t, enum devact);
extern struct cfdriver uhmodem_cd;
CFATTACH_DECL2_NEW(uhmodem, sizeof(struct uhmodem_softc), uhmodem_match,
    uhmodem_attach, uhmodem_detach, uhmodem_activate, NULL, uhmodem_childdet);

int 
uhmodem_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	if (uhmodem_lookup(uaa->vendor, uaa->product) != NULL)
		/* XXX interface# 0,1 provide modem function, but this driver
		   handles all modem in single device.  */
		if (uaa->ifaceno == 0)
			return (UMATCH_VENDOR_PRODUCT);
	return (UMATCH_NONE);
}

void 
uhmodem_attach(device_t parent, device_t self, void *aux)
{
	struct uhmodem_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	usbd_status err;
	struct ucom_attach_args uca;
	int i;
	int j;
	char comname[16];

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_ubsa.sc_dev = self;
        sc->sc_ubsa.sc_udev = dev;
	sc->sc_ubsa.sc_config_index = UBSA_DEFAULT_CONFIG_INDEX;
	sc->sc_ubsa.sc_numif = 1; /* defaut device has one interface */

	/* Hauwei E220 need special request to change its mode to modem */
	if ((uaa->ifaceno == 0) && (uaa->class != 255)) {
		err = e220_modechange_request(dev);
		if (err) {
			aprint_error_dev(self, "failed to change mode: %s\n", 
				usbd_errstr(err));
			sc->sc_ubsa.sc_dying = 1;
			goto error;
		}
		aprint_error_dev(self,
		    "mass storage only mode, reattach to enable modem\n");
		sc->sc_ubsa.sc_dying = 1;
		goto error;
	}

	/*
	 * initialize rts, dtr variables to something
	 * different from boolean 0, 1
	 */
	sc->sc_ubsa.sc_dtr = -1;
	sc->sc_ubsa.sc_rts = -1;

	sc->sc_ubsa.sc_quadumts = 1;
	sc->sc_ubsa.sc_config_index = 0;
	sc->sc_ubsa.sc_numif = uhmodem_lookup(uaa->vendor, uaa->product)->uhmodem_coms;
	sc->sc_ubsa.sc_devflags = uhmodem_lookup(uaa->vendor, uaa->product)->uhmodem_flags;

	DPRINTF(("uhmodem attach: sc = %p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, sc->sc_ubsa.sc_config_index, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration: %s\n",
		    usbd_errstr(err));
		sc->sc_ubsa.sc_dying = 1;
		goto error;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(sc->sc_ubsa.sc_udev);
	if (cdesc == NULL) {
		aprint_error_dev(self,
		    "failed to get configuration descriptor\n");
		sc->sc_ubsa.sc_dying = 1;
		goto error;
	}

	sc->sc_ubsa.sc_intr_number = -1;
	sc->sc_ubsa.sc_intr_pipe = NULL;

	/* get the interfaces */
	for (i = 0; i < sc->sc_ubsa.sc_numif; i++) {
		err = usbd_device2interface_handle(dev, UBSA_IFACE_INDEX_OFFSET+i,
				 &sc->sc_ubsa.sc_iface[i]);
		if (err) {
			if (i == 0){
				/* can not get main interface */
				sc->sc_ubsa.sc_dying = 1;
				goto error;
			} else
				break;
		}

		/* Find the endpoints */
		id = usbd_get_interface_descriptor(sc->sc_ubsa.sc_iface[i]);
		sc->sc_ubsa.sc_iface_number[i] = id->bInterfaceNumber;

		/* initialize endpoints */
		uca.bulkin = uca.bulkout = -1;

		for (j = 0; j < id->bNumEndpoints; j++) {
			ed = usbd_interface2endpoint_descriptor(
				sc->sc_ubsa.sc_iface[i], j);
			if (ed == NULL) {
				aprint_error_dev(self,
				    "no endpoint descriptor for %d "
				    "(interface: %d)\n", j, i);
				break;
			}

			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
				sc->sc_ubsa.sc_intr_number = ed->bEndpointAddress;
				sc->sc_ubsa.sc_isize = UGETW(ed->wMaxPacketSize);
			} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
				uca.bulkin = ed->bEndpointAddress;
			} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
				uca.bulkout = ed->bEndpointAddress;
			}
		} /* end of Endpoint loop */

		if (sc->sc_ubsa.sc_intr_number == -1) {
			aprint_error_dev(self, "HUAWEI E220 need to re-attach "
			    "to enable modem function\n");
			if (i == 0) {
				/* could not get intr for main tty */
				sc->sc_ubsa.sc_dying = 1;
				goto error;
			} else
				break;
		}
		if (uca.bulkin == -1) {
			aprint_error_dev(self,
			    "Could not find data bulk in\n");
			sc->sc_ubsa.sc_dying = 1;
			goto error;
		}

		if (uca.bulkout == -1) {
			aprint_error_dev(self,
			    "Could not find data bulk out\n");
			sc->sc_ubsa.sc_dying = 1;
			goto error;
		}

		switch (i) {
		case 0:
			snprintf(comname, sizeof(comname), "modem");
			break;
		case 1:
			snprintf(comname, sizeof(comname), "alt#1");
			break;
		case 2:
			snprintf(comname, sizeof(comname), "alt#2");
			break;
		default:
			snprintf(comname, sizeof(comname), "int#%d", i);
			break;
		}

		uca.portno = i;
		/* bulkin, bulkout set above */
		uca.ibufsize = UHMODEMIBUFSIZE;
		uca.obufsize = UHMODEMOBUFSIZE;
		uca.ibufsizepad = UHMODEMIBUFSIZE;
		uca.opkthdrlen = 0;
		uca.device = dev;
		uca.iface = sc->sc_ubsa.sc_iface[i];
		uca.methods = &uhmodem_methods;
		uca.arg = &sc->sc_ubsa;
		uca.info = comname;
		DPRINTF(("uhmodem: int#=%d, in = 0x%x, out = 0x%x, intr = 0x%x\n",
	    		i, uca.bulkin, uca.bulkout, sc->sc_ubsa.sc_intr_number));
		sc->sc_ubsa.sc_subdevs[i] = config_found_sm_loc(self, "ucombus", NULL,
				 &uca, ucomprint, ucomsubmatch);

		/* issue endpoint halt to each interface */
		err = uhmodem_endpointhalt(&sc->sc_ubsa, i);
		if (err) 
			aprint_error("%s: endpointhalt fail\n", __func__);
		else
			usbd_delay_ms(sc->sc_ubsa.sc_udev, 50);
	} /* end of Interface loop */

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_ubsa.sc_udev,
			   sc->sc_ubsa.sc_dev);

	return;

error:
	return;
}

void
uhmodem_childdet(device_t self, device_t child)
{
	int i;
	struct uhmodem_softc *sc = device_private(self);

	for (i = 0; i < sc->sc_ubsa.sc_numif; i++) {
		if (sc->sc_ubsa.sc_subdevs[i] == child)
			break;
	}
	KASSERT(i < sc->sc_ubsa.sc_numif);
	sc->sc_ubsa.sc_subdevs[i] = NULL;
}

int 
uhmodem_detach(device_t self, int flags)
{
	struct uhmodem_softc *sc = device_private(self);
	int i;
	int rv = 0;

	DPRINTF(("uhmodem_detach: sc = %p\n", sc));

	if (sc->sc_ubsa.sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_ubsa.sc_intr_pipe);
		usbd_close_pipe(sc->sc_ubsa.sc_intr_pipe);
		free(sc->sc_ubsa.sc_intr_buf, M_USBDEV);
		sc->sc_ubsa.sc_intr_pipe = NULL;
	}

	sc->sc_ubsa.sc_dying = 1;
	for (i = 0; i < sc->sc_ubsa.sc_numif; i++) {
		if (sc->sc_ubsa.sc_subdevs[i] != NULL)
			rv |= config_detach(sc->sc_ubsa.sc_subdevs[i], flags);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_ubsa.sc_udev,
			   sc->sc_ubsa.sc_dev);

	return (rv);
}

int
uhmodem_activate(device_t self, enum devact act)
{
	struct uhmodem_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_ubsa.sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static int
uhmodem_open(void *addr, int portno)
{
	struct ubsa_softc *sc = addr;
	usbd_status err;

	if (sc->sc_dying)
		return (ENXIO);

	DPRINTF(("%s: sc = %p\n", __func__, sc));

	err = uhmodem_endpointhalt(sc, 0);
	if (err) 
		aprint_error("%s: endpointhalt fail\n", __func__);
	else
		usbd_delay_ms(sc->sc_udev, 50);

	if (sc->sc_devflags & A2502) {
		err = a2502_init(sc->sc_udev);
		if (err)
			aprint_error("%s: a2502init fail\n", __func__);
		else
			usbd_delay_ms(sc->sc_udev, 50);
	}
#if 0 /* currently disabled */
	if (sc->sc_devflags & E220) {
		err = e220_init(sc->sc_udev);
		if (err)
			aprint_error("%s: e220init fail\n", __func__);
		else
			usbd_delay_ms(sc->sc_udev, 50);
	}
#endif
	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		/* XXX only iface# = 0 has intr line */
		/* XXX E220 specific? need to check */
		err = usbd_open_pipe_intr(sc->sc_iface[0],
		    sc->sc_intr_number,
		    USBD_SHORT_XFER_OK,
		    &sc->sc_intr_pipe,
		    sc,
		    sc->sc_intr_buf,
		    sc->sc_isize,
		    ubsa_intr,
		    UBSA_INTR_INTERVAL);
		if (err) {
			aprint_error_dev(sc->sc_dev,
			    "cannot open interrupt pipe (addr %d)\n",
			    sc->sc_intr_number);
			return (EIO);
		}
	}

	return (0);
}

/*
 * Hauwei E220 needs special request to enable modem function.
 * -- DEVICE_REMOTE_WAKEUP ruquest to endpoint 2.
 */
Static  usbd_status 
e220_modechange_request(usbd_device_handle dev)
{
#define E220_MODE_CHANGE_REQUEST 0x2
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, E220_MODE_CHANGE_REQUEST);
	USETW(req.wLength, 0);

	DPRINTF(("%s: send e220 mode change request\n", __func__));
	err = usbd_do_request(dev, &req, 0);
	if (err) {
		DPRINTF(("%s: E220 mode change fail\n", __func__));
		return (EIO);
	}

	return (0);
#undef E220_MODE_CHANGE_REQUEST
}

Static  usbd_status 
uhmodem_endpointhalt(struct ubsa_softc *sc, int iface)
{
	usb_device_request_t req;
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	usbd_status err;
	int i;

	/* Find the endpoints */
	id = usbd_get_interface_descriptor(sc->sc_iface[iface]);

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface[iface], i);
		if (ed == NULL)	
			return (EIO);

		if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			/* issue ENDPOINT_HALT request */
			req.bmRequestType = UT_WRITE_ENDPOINT;
			req.bRequest = UR_CLEAR_FEATURE;
			USETW(req.wValue, UF_ENDPOINT_HALT);
			USETW(req.wIndex, ed->bEndpointAddress);
			USETW(req.wLength, 0);
			err = usbd_do_request(sc->sc_udev, &req, 0);
			if (err) {
				DPRINTF(("%s: ENDPOINT_HALT to EP:%d fail\n", 
					__func__, ed->bEndpointAddress));
				return (EIO);
			}

		}
	} /* end of Endpoint loop */

	return (0);
}

Static usbd_status
uhmodem_regwrite(usbd_device_handle dev, uint8_t *data, size_t len)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UHMODEM_REGWRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, 0x0000);
	USETW(req.wLength, len);
	err = usbd_do_request(dev, &req, data);
	if (err) 
		return err;

	return 0;
}

Static usbd_status
uhmodem_regread(usbd_device_handle dev, uint8_t *data, size_t len)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UHMODEM_REGREAD;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, 0x0000);
	USETW(req.wLength, len);
	err = usbd_do_request(dev, &req, data);

	if (err)
		return err;

	return 0;
}

#if 0
Static usbd_status
uhmodem_regsetup(usbd_device_handle dev, uint16_t cmd)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UHMODEM_SETUP;
	USETW(req.wValue, cmd);
	USETW(req.wIndex, 0x0000);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);

	if (err)
		return err;

	return 0;
}
#endif

Static  usbd_status 
a2502_init(usbd_device_handle dev)
{
	uint8_t data[8];
	static uint8_t init_cmd[] = {0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x08};
#ifdef UHMODEM_DEBUG
	int i;
#endif
	if (uhmodem_regread(dev, data, 7)) {
		DPRINTF(("%s: read fail\n", __func__));
		return EIO;
	}
#ifdef UHMODEM_DEBUG
	printf("%s: readdata: ", __func__);
	for (i = 0; i < 7; i++)
		printf("0x%x ", data[i]);
#endif
	if (uhmodem_regwrite(dev, init_cmd, sizeof(init_cmd)) ) {
		DPRINTF(("%s: write fail\n", __func__));
		return EIO;
	}

	if (uhmodem_regread(dev, data, 7)) { 
		DPRINTF(("%s: read fail\n", __func__));
		return EIO;
	}
#ifdef UHMODEM_DEBUG
	printf("%s: readdata: ", __func__);
	printf(" => ");
	for (i = 0; i < 7; i++)
		printf("0x%x ", data[i]);
	printf("\n");
#endif
	return 0;
}


#if 0
/* 
 * Windows device driver send these sequens of USB requests.
 * However currently I can't understand what the messege is,
 * disable this code when I get more information about it.
 */ 
Static  usbd_status 
e220_init(usbd_device_handle dev)
{
	uint8_t data[8];
	usb_device_request_t req;
	int i;

	/* vendor specific unknown request */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = 0x02;
	USETW(req.wValue, 0x0001);
	USETW(req.wIndex, 0x0000);
	USETW(req.wLength, 2);
	data[0] = 0x0;
	data[1] = 0x0;
	if (usbd_do_request(dev, &req, data))
		goto error;

	/* vendor specific unknown sequence */
	if(uhmodem_regsetup(dev, 0x1)) 
		goto error;

	if (uhmodem_regread(dev, data, 7))
		goto error;

	data[1] = 0x8;
	data[2] = 0x7;
	if (uhmodem_regwrite(dev, data, sizeof(data)) )
		goto error;

	if (uhmodem_regread(dev, data, 7))
		goto error;
		/* XXX should verify the read data ? */

	if (uhmodem_regsetup(dev, 0x3))
		goto error;

	return (0);
error:
	DPRINTF(("%s: E220 init request fail\n", __func__));
	return (EIO);
}
#endif

