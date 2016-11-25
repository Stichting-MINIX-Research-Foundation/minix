/*	$NetBSD: ugenhc.c,v 1.22 2014/08/02 12:38:01 skrll Exp $	*/

/*
 * Copyright (c) 2009, 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
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

/*
 * This rump driver attaches ugen as a kernel usb host controller.
 * It's still somewhat under the hammer ....
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ugenhc.c,v 1.22 2014/08/02 12:38:01 skrll Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mutex.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usbroothub_subr.h>

#include <rump/rumpuser.h>

#include "ugenhc_user.h"

#include "rump_private.h"
#include "rump_dev_private.h"

#define UGEN_NEPTS 16
#define UGEN_EPT_CTRL 0 /* ugenx.00 is the control endpoint */

struct ugenhc_softc {
	struct usbd_bus sc_bus;
	int sc_devnum;

	int sc_ugenfd[UGEN_NEPTS];
	int sc_fdmodes[UGEN_NEPTS];

	int sc_port_status;
	int sc_port_change;
	int sc_addr;
	int sc_conf;

	struct lwp *sc_rhintr;
	usbd_xfer_handle sc_intrxfer;

	kmutex_t sc_lock;
};

static int	ugenhc_probe(device_t, cfdata_t, void *);
static void	ugenhc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ugenhc, sizeof(struct ugenhc_softc),
	ugenhc_probe, ugenhc_attach, NULL, NULL);

struct rusb_xfer {
	struct usbd_xfer rusb_xfer;
	int rusb_status; /* now this is a cheap trick */
};
#define RUSB(x) ((struct rusb_xfer *)x)

#define UGENDEV_BASESTR "/dev/ugen"
#define UGENDEV_BUFSIZE 32
static void
makeugendevstr(int devnum, int endpoint, char *buf, size_t len)
{

	CTASSERT(UGENDEV_BUFSIZE > sizeof(UGENDEV_BASESTR)+sizeof("0.00")+1);
	snprintf(buf, len, "%s%d.%02d", UGENDEV_BASESTR, devnum, endpoint);
}

/*
 * Our fictional hubbie.
 */

static const usb_device_descriptor_t rumphub_udd = {
	.bLength		= USB_DEVICE_DESCRIPTOR_SIZE,
	.bDescriptorType	= UDESC_DEVICE,
	.bDeviceClass		= UDCLASS_HUB,
	.bDeviceSubClass	= UDSUBCLASS_HUB,
	.bDeviceProtocol	= UDPROTO_FSHUB,
	.bMaxPacketSize		= 64,
	.idVendor		= { 0x75, 0x72 },
	.idProduct		= { 0x70, 0x6d },
	.bNumConfigurations	= 1,
};

static const usb_config_descriptor_t rumphub_ucd = {
	.bLength		= USB_CONFIG_DESCRIPTOR_SIZE,
	.bDescriptorType	= UDESC_CONFIG,
	.wTotalLength		= { USB_CONFIG_DESCRIPTOR_SIZE
				  + USB_INTERFACE_DESCRIPTOR_SIZE
				  + USB_ENDPOINT_DESCRIPTOR_SIZE },
	.bNumInterface		= 1,
	.bmAttributes		= UC_SELF_POWERED | UC_ATTR_MBO,
};

static const usb_interface_descriptor_t rumphub_uid = {
	.bLength		= USB_INTERFACE_DESCRIPTOR_SIZE,
	.bDescriptorType	= UDESC_INTERFACE,
	.bInterfaceNumber	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= UICLASS_HUB,
	.bInterfaceSubClass	= UISUBCLASS_HUB,
	.bInterfaceProtocol	= UIPROTO_FSHUB,
};

static const usb_endpoint_descriptor_t rumphub_epd = {
	.bLength		= USB_ENDPOINT_DESCRIPTOR_SIZE,
	.bDescriptorType	= UDESC_ENDPOINT,
	.bmAttributes		= UE_INTERRUPT,
	.wMaxPacketSize		= {64, 0},
};

static const usb_hub_descriptor_t rumphub_hdd = {
	.bDescLength		= USB_HUB_DESCRIPTOR_SIZE,
	.bDescriptorType	= UDESC_HUB,
	.bNbrPorts		= 1,
};

static usbd_status
rumpusb_root_ctrl_start(usbd_xfer_handle xfer)
{
	usb_device_request_t *req = &xfer->request;
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	int len, totlen, value, curlen, err;
	uint8_t *buf = NULL;

	len = totlen = UGETW(req->wLength);
	if (len)
		buf = KERNADDR(&xfer->dmabuf, 0);
	value = UGETW(req->wValue);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {

	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*buf = sc->sc_conf;
			totlen = 1;
		}
		break;

	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		switch (value >> 8) {
		case UDESC_DEVICE:
			totlen = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &rumphub_udd, totlen);
			break;

		case UDESC_CONFIG:
			totlen = 0;
			curlen = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &rumphub_ucd, curlen);
			len -= curlen;
			buf += curlen;
			totlen += curlen;

			curlen = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			memcpy(buf, &rumphub_uid, curlen);
			len -= curlen;
			buf += curlen;
			totlen += curlen;

			curlen = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			memcpy(buf, &rumphub_epd, curlen);
			len -= curlen;
			buf += curlen;
			totlen += curlen;
			break;

		case UDESC_STRING:
#define sd ((usb_string_descriptor_t *)buf)
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = usb_makelangtbl(sd, len);
				break;
			case 1: /* Vendor */
				totlen = usb_makestrdesc(sd, len, "rod nevada");
				break;
			case 2: /* Product */
				totlen = usb_makestrdesc(sd, len,
				    "RUMPUSBHC root hub");
				break;
			}
#undef sd
			break;

		default:
			panic("unhandled read device request");
			break;
		}
		break;

	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;

	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;

	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		switch (value) {
		case UHF_PORT_RESET:
			sc->sc_port_change |= UPS_C_PORT_RESET;
			break;
		case UHF_PORT_POWER:
			break;
		default:
			panic("unhandled");
		}
		break;

	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		sc->sc_port_change &= ~value;
		break;

	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		totlen = min(len, USB_HUB_DESCRIPTOR_SIZE);
		memcpy(buf, &rumphub_hdd, totlen);
		break;

	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		/* huh?  other hc's do this */
		memset(buf, 0, len);
		totlen = len;
		break;

	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		{
		usb_port_status_t ps;

		USETW(ps.wPortStatus, sc->sc_port_status);
		USETW(ps.wPortChange, sc->sc_port_change);
		totlen = min(len, sizeof(ps));
		memcpy(buf, &ps, totlen);
		break;
		}

	default:
		panic("unhandled request");
		break;
	}
	err = USBD_NORMAL_COMPLETION;
	xfer->actlen = totlen;

ret:
	xfer->status = err;
	mutex_enter(&sc->sc_lock);
	usb_transfer_complete(xfer);
	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

static usbd_status
rumpusb_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	return (rumpusb_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static void
rumpusb_root_ctrl_abort(usbd_xfer_handle xfer)
{

}

static void
rumpusb_root_ctrl_close(usbd_pipe_handle pipe)
{

}

static void
rumpusb_root_ctrl_cleartoggle(usbd_pipe_handle pipe)
{

}

static void
rumpusb_root_ctrl_done(usbd_xfer_handle xfer)
{

}

static const struct usbd_pipe_methods rumpusb_root_ctrl_methods = {
	.transfer =	rumpusb_root_ctrl_transfer,
	.start =	rumpusb_root_ctrl_start,
	.abort =	rumpusb_root_ctrl_abort,
	.close =	rumpusb_root_ctrl_close,
	.cleartoggle =	rumpusb_root_ctrl_cleartoggle,
	.done =		rumpusb_root_ctrl_done,
};

static usbd_status
rumpusb_device_ctrl_start(usbd_xfer_handle xfer)
{
	usb_device_request_t *req = &xfer->request;
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	uint8_t *buf = NULL;
	int len, totlen;
	int value;
	int err = 0;
	int ru_error, mightfail = 0;

	len = totlen = UGETW(req->wLength);
	if (len)
		buf = KERNADDR(&xfer->dmabuf, 0);
	value = UGETW(req->wValue);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		switch (value>>8) {
		case UDESC_DEVICE:
			{
			usb_device_descriptor_t uddesc;
			totlen = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memset(buf, 0, totlen);
			if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
			    USB_GET_DEVICE_DESC, &uddesc, &ru_error) == -1) {
				err = EIO;
				goto ret;
			}
			memcpy(buf, &uddesc, totlen);
			}

			break;
		case UDESC_CONFIG:
			{
			struct usb_full_desc ufdesc;
			ufdesc.ufd_config_index = value & 0xff;
			ufdesc.ufd_size = len;
			ufdesc.ufd_data = buf;
			memset(buf, 0, len);
			if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
			    USB_GET_FULL_DESC, &ufdesc, &ru_error) == -1) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = ufdesc.ufd_size;
			}
			break;

		case UDESC_STRING:
			{
			struct usb_device_info udi;

			if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
			    USB_GET_DEVICEINFO, &udi, &ru_error) == -1) {
				printf("ugenhc: get dev info failed: %d\n",
				    ru_error);
				err = USBD_IOERROR;
				goto ret;
			}

			switch (value & 0xff) {
#define sd ((usb_string_descriptor_t *)buf)
			case 0: /* language table */
				break;
			case 1: /* vendor */
				totlen = usb_makestrdesc(sd, len,
				    udi.udi_vendor);
				break;
			case 2: /* product */
				totlen = usb_makestrdesc(sd, len,
				    udi.udi_product);
				break;
			}
#undef sd
			}
			break;

		default:
			panic("not handled");
		}
		break;

	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		/* ignored, ugen won't let us */
		break;

	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
		    USB_SET_CONFIG, &value, &ru_error) == -1) {
			printf("ugenhc: set config failed: %d\n",
			    ru_error);
			err = USBD_IOERROR;
			goto ret;
		}
		break;

	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		{
		struct usb_alt_interface uai;

		totlen = 0;
		uai.uai_interface_index = UGETW(req->wIndex);
		uai.uai_alt_no = value;
		if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
		    USB_SET_ALTINTERFACE, &uai, &ru_error) == -1) {
			printf("ugenhc: set alt interface failed: %d\n",
			    ru_error);
			err = USBD_IOERROR;
			goto ret;
		}
		break;
		}

	/*
	 * This request might fail unknown reasons.  "EIO" doesn't
	 * give much help, and debugging the host ugen would be
	 * necessary.  However, since it doesn't seem to really
	 * affect anything, just let it fail for now.
	 */
	case C(0x00, UT_WRITE_CLASS_INTERFACE):
		mightfail = 1;
		/*FALLTHROUGH*/

	/*
	 * XXX: don't wildcard these yet.  I want to better figure
	 * out what to trap here.  This is kinda silly, though ...
	 */

	case C(0x01, UT_WRITE_VENDOR_DEVICE):
	case C(0x06, UT_WRITE_VENDOR_DEVICE):
	case C(0x07, UT_READ_VENDOR_DEVICE):
	case C(0x09, UT_READ_VENDOR_DEVICE):
	case C(0xfe, UT_READ_CLASS_INTERFACE):
	case C(0x01, UT_READ_CLASS_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
	case C(UR_GET_DESCRIPTOR, UT_READ_INTERFACE):
	case C(0xff, UT_WRITE_CLASS_INTERFACE):
	case C(0x20, UT_WRITE_CLASS_INTERFACE):
	case C(0x22, UT_WRITE_CLASS_INTERFACE):
	case C(0x0a, UT_WRITE_CLASS_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
	case C(0x00, UT_WRITE_CLASS_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
	case C(UR_SET_REPORT, UT_WRITE_CLASS_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		{
		struct usb_ctl_request ucr;

		memcpy(&ucr.ucr_request, req, sizeof(ucr.ucr_request));
		ucr.ucr_data = buf;
		if (rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[UGEN_EPT_CTRL],
		    USB_DO_REQUEST, &ucr, &ru_error) == -1) {
			if (!mightfail) {
				panic("request failed: %d", ru_error);
			} else {
				err = ru_error;
			}
		}
		}
		break;

	default:
		panic("unhandled request");
		break;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;

 ret:
	xfer->status = err;
	mutex_enter(&sc->sc_lock);
	usb_transfer_complete(xfer);
	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

static usbd_status
rumpusb_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	return (rumpusb_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static void
rumpusb_device_ctrl_abort(usbd_xfer_handle xfer)
{

}

static void
rumpusb_device_ctrl_close(usbd_pipe_handle pipe)
{

}

static void
rumpusb_device_ctrl_cleartoggle(usbd_pipe_handle pipe)
{

}

static void
rumpusb_device_ctrl_done(usbd_xfer_handle xfer)
{

}

static const struct usbd_pipe_methods rumpusb_device_ctrl_methods = {
	.transfer =	rumpusb_device_ctrl_transfer,
	.start =	rumpusb_device_ctrl_start,
	.abort =	rumpusb_device_ctrl_abort,
	.close =	rumpusb_device_ctrl_close,
	.cleartoggle =	rumpusb_device_ctrl_cleartoggle,
	.done =		rumpusb_device_ctrl_done,
};

static void
rhscintr(void *arg)
{
	char buf[UGENDEV_BUFSIZE];
	struct ugenhc_softc *sc = arg;
	usbd_xfer_handle xfer;
	int fd, error;

	makeugendevstr(sc->sc_devnum, 0, buf, sizeof(buf));

	for (;;) {
		/*
		 * Detect device attach.
		 */

		for (;;) {
			error = rumpuser_open(buf, RUMPUSER_OPEN_RDWR, &fd);
			if (error == 0)
				break;
			kpause("ugwait", false, hz/4, NULL);
		}

		sc->sc_ugenfd[UGEN_EPT_CTRL] = fd;
		sc->sc_port_status = UPS_CURRENT_CONNECT_STATUS
		    | UPS_PORT_ENABLED | UPS_PORT_POWER;
		sc->sc_port_change = UPS_C_CONNECT_STATUS | UPS_C_PORT_RESET;

		xfer = sc->sc_intrxfer;
		memset(xfer->buffer, 0xff, xfer->length);
		xfer->actlen = xfer->length;
		xfer->status = USBD_NORMAL_COMPLETION;

		mutex_enter(&sc->sc_lock);
		usb_transfer_complete(xfer);
		mutex_exit(&sc->sc_lock);

		kpause("ugwait2", false, hz, NULL);

		/*
		 * Detect device detach.
		 */

		for (;;) {
			fd = rumpuser_open(buf, RUMPUSER_OPEN_RDWR, &error);
			if (fd == -1)
				break;

			error = rumpuser_close(fd);
			kpause("ugwait2", false, hz/4, NULL);
		}

		sc->sc_port_status = ~(UPS_CURRENT_CONNECT_STATUS
		    | UPS_PORT_ENABLED | UPS_PORT_POWER);
		sc->sc_port_change = UPS_C_CONNECT_STATUS | UPS_C_PORT_RESET;

		error = rumpuser_close(sc->sc_ugenfd[UGEN_EPT_CTRL]);
		sc->sc_ugenfd[UGEN_EPT_CTRL] = -1;

		xfer = sc->sc_intrxfer;
		memset(xfer->buffer, 0xff, xfer->length);
		xfer->actlen = xfer->length;
		xfer->status = USBD_NORMAL_COMPLETION;
		mutex_enter(&sc->sc_lock);
		usb_transfer_complete(xfer);
		mutex_exit(&sc->sc_lock);

		kpause("ugwait3", false, hz, NULL);
	}

	kthread_exit(0);
}

static usbd_status
rumpusb_root_intr_start(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	int error;

	mutex_enter(&sc->sc_lock);
	sc->sc_intrxfer = xfer;
	if (!sc->sc_rhintr) {
		error = kthread_create(PRI_NONE, 0, NULL,
		    rhscintr, sc, &sc->sc_rhintr, "ugenrhi");
		if (error)
			xfer->status = USBD_IOERROR;
	}
	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

static usbd_status
rumpusb_root_intr_transfer(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	return (rumpusb_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static void
rumpusb_root_intr_abort(usbd_xfer_handle xfer)
{

}

static void
rumpusb_root_intr_close(usbd_pipe_handle pipe)
{

}

static void
rumpusb_root_intr_cleartoggle(usbd_pipe_handle pipe)
{

}

static void
rumpusb_root_intr_done(usbd_xfer_handle xfer)
{

}

static const struct usbd_pipe_methods rumpusb_root_intr_methods = {
	.transfer =	rumpusb_root_intr_transfer,
	.start =	rumpusb_root_intr_start,
	.abort =	rumpusb_root_intr_abort,
	.close =	rumpusb_root_intr_close,
	.cleartoggle =	rumpusb_root_intr_cleartoggle,
	.done =		rumpusb_root_intr_done,
};

static usbd_status
rumpusb_device_bulk_start(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	usb_endpoint_descriptor_t *ed = xfer->pipe->endpoint->edesc;
	size_t n, done;
	bool isread;
	int len, error, endpt;
	uint8_t *buf;
	int xfererr = USBD_NORMAL_COMPLETION;
	int shortval, i;

	ed = xfer->pipe->endpoint->edesc;
	endpt = ed->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	endpt = UE_GET_ADDR(endpt);
	KASSERT(endpt < UGEN_NEPTS);

	buf = KERNADDR(&xfer->dmabuf, 0);
	done = 0;
	if ((ed->bmAttributes & UE_XFERTYPE) == UE_ISOCHRONOUS) {
		for (i = 0, len = 0; i < xfer->nframes; i++)
			len += xfer->frlengths[i];
	} else {
		KASSERT(xfer->length);
		len = xfer->length;
	}
	shortval = (xfer->flags & USBD_SHORT_XFER_OK) != 0;

	while (RUSB(xfer)->rusb_status == 0) {
		if (isread) {
			struct rumpuser_iovec iov;

			rumpcomp_ugenhc_ioctl(sc->sc_ugenfd[endpt],
			    USB_SET_SHORT_XFER, &shortval, &error);
			iov.iov_base = buf+done;
			iov.iov_len = len-done;
			error = rumpuser_iovread(sc->sc_ugenfd[endpt], &iov, 1,
			    RUMPUSER_IOV_NOSEEK, &n);
			if (error) {
				n = 0;
				if (done == 0) {
					if (error == ETIMEDOUT)
						continue;
					xfererr = USBD_IOERROR;
					goto out;
				}
			}
			done += n;
			if (done == len)
				break;
		} else {
			struct rumpuser_iovec iov;

			iov.iov_base = buf;
			iov.iov_len = len;
			error = rumpuser_iovwrite(sc->sc_ugenfd[endpt], &iov, 1,
			    RUMPUSER_IOV_NOSEEK, &n);
			done = n;
			if (done == len)
				break;
			else if (!error)
				panic("short write");

			xfererr = USBD_IOERROR;
			goto out;
		}

		if (shortval) {
			/*
			 * Holy XXX, bitman.  I get >16byte interrupt
			 * transfers from ugen in 16 byte chunks.
			 * Don't know how to better fix this for now.
			 * Of course this hack will fail e.g. if someone
			 * sports other magic values or if the transfer
			 * happens to be an integral multiple of 16
			 * in size ....
			 */
			if ((ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT
			    && n == 16) {
				continue;
			} else {
				break;
			}
		}
	}

	if (RUSB(xfer)->rusb_status == 0) {
		xfer->actlen = done;
	} else {
		xfererr = USBD_CANCELLED;
		RUSB(xfer)->rusb_status = 2;
	}
 out:
	if ((ed->bmAttributes & UE_XFERTYPE) == UE_ISOCHRONOUS)
		if (done != len)
			panic("lazy bum");
	xfer->status = xfererr;
	mutex_enter(&sc->sc_lock);
	usb_transfer_complete(xfer);
	mutex_exit(&sc->sc_lock);
	return (USBD_IN_PROGRESS);
}

static void
doxfer_kth(void *arg)
{
	usbd_pipe_handle pipe = arg;
	struct ugenhc_softc *sc = pipe->device->bus->hci_private;

	mutex_enter(&sc->sc_lock);
	do {
		usbd_xfer_handle xfer = SIMPLEQ_FIRST(&pipe->queue);
		mutex_exit(&sc->sc_lock);
		rumpusb_device_bulk_start(xfer);
		mutex_enter(&sc->sc_lock);
	} while (!SIMPLEQ_EMPTY(&pipe->queue));
	mutex_exit(&sc->sc_lock);
	kthread_exit(0);
}

static usbd_status
rumpusb_device_bulk_transfer(usbd_xfer_handle xfer)
{
	struct ugenhc_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	if (!rump_threads) {
		/* XXX: lie about supporting async transfers */
		if ((xfer->flags & USBD_SYNCHRONOUS) == 0) {
			printf("non-threaded rump does not support "
			    "async transfers.\n");
			return USBD_IN_PROGRESS;
		}

		mutex_enter(&sc->sc_lock);
		err = usb_insert_transfer(xfer);
		mutex_exit(&sc->sc_lock);
		if (err)
			return err;

		return rumpusb_device_bulk_start(
		    SIMPLEQ_FIRST(&xfer->pipe->queue));
	} else {
		mutex_enter(&sc->sc_lock);
		err = usb_insert_transfer(xfer);
		mutex_exit(&sc->sc_lock);
		if (err)
			return err;
		kthread_create(PRI_NONE, 0, NULL, doxfer_kth, xfer->pipe, NULL,
		    "rusbhcxf");

		return USBD_IN_PROGRESS;
	}
}

/* wait for transfer to abort.  yea, this is cheesy (from a spray can) */
static void
rumpusb_device_bulk_abort(usbd_xfer_handle xfer)
{
	struct rusb_xfer *rx = RUSB(xfer);

	rx->rusb_status = 1;
	while (rx->rusb_status < 2) {
		kpause("jopo", false, hz/10, NULL);
	}
}

static void
rumpusb_device_bulk_close(usbd_pipe_handle pipe)
{
	struct ugenhc_softc *sc = pipe->device->bus->hci_private;
	int endpt = pipe->endpoint->edesc->bEndpointAddress;
	usbd_xfer_handle xfer;

	KASSERT(mutex_owned(&sc->sc_lock));

	endpt = UE_GET_ADDR(endpt);

	while ((xfer = SIMPLEQ_FIRST(&pipe->queue)) != NULL)
		rumpusb_device_bulk_abort(xfer);

	rumpuser_close(sc->sc_ugenfd[endpt]);
	sc->sc_ugenfd[endpt] = -1;
	sc->sc_fdmodes[endpt] = -1;
}

static void
rumpusb_device_bulk_cleartoggle(usbd_pipe_handle pipe)
{

}

static void
rumpusb_device_bulk_done(usbd_xfer_handle xfer)
{

}

static const struct usbd_pipe_methods rumpusb_device_bulk_methods = {
	.transfer =	rumpusb_device_bulk_transfer,
	.start =	rumpusb_device_bulk_start,
	.abort =	rumpusb_device_bulk_abort,
	.close =	rumpusb_device_bulk_close,
	.cleartoggle =	rumpusb_device_bulk_cleartoggle,
	.done =		rumpusb_device_bulk_done,
};

static usbd_status
ugenhc_open(struct usbd_pipe *pipe)
{
	usbd_device_handle dev = pipe->device;
	struct ugenhc_softc *sc = dev->bus->hci_private;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	u_int8_t addr = dev->address;
	u_int8_t xfertype = ed->bmAttributes & UE_XFERTYPE;
	char buf[UGENDEV_BUFSIZE];
	int endpt, oflags, error;
	int fd, val;

	if (addr == sc->sc_addr) {
		switch (xfertype) {
		case UE_CONTROL:
			pipe->methods = &rumpusb_root_ctrl_methods;
			break;
		case UE_INTERRUPT:
			pipe->methods = &rumpusb_root_intr_methods;
			break;
		default:
			panic("%d not supported", xfertype);
			break;
		}
	} else {
		switch (xfertype) {
		case UE_CONTROL:
			pipe->methods = &rumpusb_device_ctrl_methods;
			break;
		case UE_INTERRUPT:
		case UE_BULK:
		case UE_ISOCHRONOUS:
			pipe->methods = &rumpusb_device_bulk_methods;
			endpt = pipe->endpoint->edesc->bEndpointAddress;
			if (UE_GET_DIR(endpt) == UE_DIR_IN) {
				oflags = O_RDONLY;
			} else {
				oflags = O_WRONLY;
			}
			endpt = UE_GET_ADDR(endpt);

			if (oflags != O_RDONLY && xfertype == UE_ISOCHRONOUS) {
				printf("WARNING: faking isoc write open\n");
				oflags = O_RDONLY;
			}

			if (sc->sc_fdmodes[endpt] == oflags
			    || sc->sc_fdmodes[endpt] == O_RDWR)
				break;

			if (sc->sc_fdmodes[endpt] != -1) {
				/* XXX: closing from under someone? */
				error = rumpuser_close(sc->sc_ugenfd[endpt]);
				oflags = O_RDWR;
			}

			makeugendevstr(sc->sc_devnum, endpt, buf, sizeof(buf));
			/* XXX: theoretically should convert oflags */
			error = rumpuser_open(buf, oflags, &fd);
			if (error != 0) {
				return USBD_INVAL; /* XXX: no mapping */
			}
			val = 100;
			if (rumpcomp_ugenhc_ioctl(fd, USB_SET_TIMEOUT, &val,
			    &error) == -1)
				panic("timeout set failed");
			sc->sc_ugenfd[endpt] = fd;
			sc->sc_fdmodes[endpt] = oflags;

			break;
		default:
			panic("%d not supported", xfertype);
			break;

		}
	}
	return 0;
}

static void
ugenhc_softint(void *arg)
{

}

static void
ugenhc_poll(struct usbd_bus *ubus)
{

}

static usbd_status
ugenhc_allocm(struct usbd_bus *bus, usb_dma_t *dma, uint32_t size)
{
	struct ugenhc_softc *sc = bus->hci_private;

	return usb_allocmem(&sc->sc_bus, size, 0, dma);
}

static void
ugenhc_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	struct ugenhc_softc *sc = bus->hci_private;

	usb_freemem(&sc->sc_bus, dma);
}

static struct usbd_xfer *
ugenhc_allocx(struct usbd_bus *bus)
{
	usbd_xfer_handle xfer;

	xfer = kmem_zalloc(sizeof(struct usbd_xfer), KM_SLEEP);
	xfer->busy_free = XFER_BUSY;

	return xfer;
}

static void
ugenhc_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{

	kmem_free(xfer, sizeof(struct usbd_xfer));
}


static void
ugenhc_getlock(struct usbd_bus *bus, kmutex_t **lock)
{
	struct ugenhc_softc *sc = bus->hci_private;

	*lock = &sc->sc_lock;
}

struct ugenhc_pipe {
	struct usbd_pipe pipe;
};

static const struct usbd_bus_methods ugenhc_bus_methods = {
	.open_pipe =	ugenhc_open,
	.soft_intr =	ugenhc_softint,
	.do_poll =	ugenhc_poll,
	.allocm = 	ugenhc_allocm,
	.freem = 	ugenhc_freem,
	.allocx = 	ugenhc_allocx,
	.freex =	ugenhc_freex,
	.get_lock =	ugenhc_getlock
};

static int
ugenhc_probe(device_t parent, cfdata_t match, void *aux)
{
	char buf[UGENDEV_BUFSIZE];

	makeugendevstr(match->cf_unit, 0, buf, sizeof(buf));
	if (rumpuser_getfileinfo(buf, NULL, NULL) != 0)
		return 0;

	return 1;
}

static void
ugenhc_attach(device_t parent, device_t self, void *aux)
{
	struct mainbus_attach_args *maa = aux;
	struct ugenhc_softc *sc = device_private(self);

	aprint_normal("\n");

	memset(sc, 0, sizeof(*sc));
	memset(&sc->sc_ugenfd, -1, sizeof(sc->sc_ugenfd));
	memset(&sc->sc_fdmodes, -1, sizeof(sc->sc_fdmodes));

	sc->sc_bus.usbrev = USBREV_2_0;
	sc->sc_bus.methods = &ugenhc_bus_methods;
	sc->sc_bus.hci_private = sc;
	sc->sc_bus.pipe_size = sizeof(struct ugenhc_pipe);
	sc->sc_devnum = maa->maa_unit;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	config_found(self, &sc->sc_bus, usbctlprint);
}
