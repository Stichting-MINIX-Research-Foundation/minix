/*	$NetBSD: usbdi_util.c,v 1.64 2015/03/27 12:46:51 skrll Exp $	*/

/*
 * Copyright (c) 1998, 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology and Matthew R. Green (mrg@eterna.com.au).
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
__KERNEL_RCSID(0, "$NetBSD: usbdi_util.c,v 1.64 2015/03/27 12:46:51 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_usb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhist.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

usbd_status
usbd_get_desc(usbd_device_handle dev, int type, int index, int len, void *desc)
{
	usb_device_request_t req;

	DPRINTFN(3,("usbd_get_desc: type=%d, index=%d, len=%d\n",
		    type, index, len));

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, type, index);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, desc));
}

usbd_status
usbd_get_config_desc(usbd_device_handle dev, int confidx,
		     usb_config_descriptor_t *d)
{
	usbd_status err;

	DPRINTFN(3,("usbd_get_config_desc: confidx=%d\n", confidx));
	err = usbd_get_desc(dev, UDESC_CONFIG, confidx,
			    USB_CONFIG_DESCRIPTOR_SIZE, d);
	if (err)
		return (err);
	if (d->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_get_config_desc: confidx=%d, bad desc "
			     "len=%d type=%d\n",
			     confidx, d->bLength, d->bDescriptorType));
		return (USBD_INVAL);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_get_config_desc_full(usbd_device_handle dev, int conf, void *d, int size)
{
	DPRINTFN(3,("usbd_get_config_desc_full: conf=%d\n", conf));
	return (usbd_get_desc(dev, UDESC_CONFIG, conf, size, d));
}

usbd_status
usbd_get_bos_desc(usbd_device_handle dev, int confidx,
		     usb_bos_descriptor_t *d)
{
	usbd_status err;

	DPRINTFN(3,("usbd_get_bos_desc: confidx=%d\n", confidx));
	err = usbd_get_desc(dev, UDESC_BOS, confidx,
			    USB_BOS_DESCRIPTOR_SIZE, d);
	if (err)
		return (err);
	if (d->bDescriptorType != UDESC_BOS) {
		DPRINTFN(-1,("usbd_get_bos_desc: confidx=%d, bad desc "
			     "len=%d type=%d\n",
			     confidx, d->bLength, d->bDescriptorType));
		return (USBD_INVAL);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_get_bos_desc_full(usbd_device_handle dev, int conf, void *d, int size)
{
	DPRINTFN(3,("usbd_get_bos_desc_full: conf=%d\n", conf));
	return (usbd_get_desc(dev, UDESC_BOS, conf, size, d));
}

usbd_status
usbd_get_device_desc(usbd_device_handle dev, usb_device_descriptor_t *d)
{
	DPRINTFN(3,("usbd_get_device_desc:\n"));
	return (usbd_get_desc(dev, UDESC_DEVICE,
			     0, USB_DEVICE_DESCRIPTOR_SIZE, d));
}

usbd_status
usbd_get_device_status(usbd_device_handle dev, usb_status_t *st)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(usb_status_t));
	return (usbd_do_request(dev, &req, st));
}

usbd_status
usbd_get_hub_status(usbd_device_handle dev, usb_hub_status_t *st)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(usb_hub_status_t));
	return (usbd_do_request(dev, &req, st));
}

usbd_status
usbd_set_address(usbd_device_handle dev, int addr)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return usbd_do_request(dev, &req, 0);
}

usbd_status
usbd_get_port_status(usbd_device_handle dev, int port, usb_port_status_t *ps)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_CLASS_OTHER;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, port);
	USETW(req.wLength, sizeof *ps);
	return (usbd_do_request(dev, &req, ps));
}

usbd_status
usbd_clear_hub_feature(usbd_device_handle dev, int sel)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_hub_feature(usbd_device_handle dev, int sel)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_clear_port_feature(usbd_device_handle dev, int port, int sel)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_port_feature(usbd_device_handle dev, int port, int sel)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_get_protocol(usbd_interface_handle iface, u_int8_t *report)
{
	usb_interface_descriptor_t *id = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;

	DPRINTFN(4, ("usbd_get_protocol: iface=%p, endpt=%d\n",
		     iface, id->bInterfaceNumber));
	if (id == NULL)
		return (USBD_IOERROR);
	usbd_interface2device_handle(iface, &dev);
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_PROTOCOL;
	USETW(req.wValue, 0);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 1);
	return (usbd_do_request(dev, &req, report));
}

usbd_status
usbd_set_protocol(usbd_interface_handle iface, int report)
{
	usb_interface_descriptor_t *id = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;

	DPRINTFN(4, ("usbd_set_protocol: iface=%p, report=%d, endpt=%d\n",
		     iface, report, id->bInterfaceNumber));
	if (id == NULL)
		return (USBD_IOERROR);
	usbd_interface2device_handle(iface, &dev);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_PROTOCOL;
	USETW(req.wValue, report);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_report(usbd_interface_handle iface, int type, int id, void *data,
		int len)
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;

	DPRINTFN(4, ("usbd_set_report: len=%d\n", len));
	if (ifd == NULL)
		return (USBD_IOERROR);
	usbd_interface2device_handle(iface, &dev);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, data));
}

usbd_status
usbd_get_report(usbd_interface_handle iface, int type, int id, void *data,
		int len)
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;

	DPRINTFN(4, ("usbd_get_report: len=%d\n", len));
	if (ifd == NULL)
		return (USBD_IOERROR);
	usbd_interface2device_handle(iface, &dev);
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, data));
}

usbd_status
usbd_set_idle(usbd_interface_handle iface, int duration, int id)
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;

	DPRINTFN(4, ("usbd_set_idle: %d %d\n", duration, id));
	if (ifd == NULL)
		return (USBD_IOERROR);
	usbd_interface2device_handle(iface, &dev);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_IDLE;
	USETW2(req.wValue, duration, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_get_report_descriptor(usbd_device_handle dev, int ifcno,
			   int size, void *d)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_REPORT, 0); /* report id should be 0 */
	USETW(req.wIndex, ifcno);
	USETW(req.wLength, size);
	return (usbd_do_request(dev, &req, d));
}

usb_hid_descriptor_t *
usbd_get_hid_descriptor(usbd_interface_handle ifc)
{
	usb_interface_descriptor_t *idesc = usbd_get_interface_descriptor(ifc);
	usbd_device_handle dev;
	usb_config_descriptor_t *cdesc;
	usb_hid_descriptor_t *hd;
	char *p, *end;

	if (idesc == NULL)
		return (NULL);
	usbd_interface2device_handle(ifc, &dev);
	cdesc = usbd_get_config_descriptor(dev);

	p = (char *)idesc + idesc->bLength;
	end = (char *)cdesc + UGETW(cdesc->wTotalLength);

	for (; p < end; p += hd->bLength) {
		hd = (usb_hid_descriptor_t *)p;
		if (p + hd->bLength <= end && hd->bDescriptorType == UDESC_HID)
			return (hd);
		if (hd->bDescriptorType == UDESC_INTERFACE)
			break;
	}
	return (NULL);
}

usbd_status
usbd_read_report_desc(usbd_interface_handle ifc, void **descp, int *sizep,
		       struct malloc_type * mem)
{
	usb_interface_descriptor_t *id;
	usb_hid_descriptor_t *hid;
	usbd_device_handle dev;
	usbd_status err;

	usbd_interface2device_handle(ifc, &dev);
	id = usbd_get_interface_descriptor(ifc);
	if (id == NULL)
		return (USBD_INVAL);
	hid = usbd_get_hid_descriptor(ifc);
	if (hid == NULL)
		return (USBD_IOERROR);
	*sizep = UGETW(hid->descrs[0].wDescriptorLength);
	*descp = malloc(*sizep, mem, M_NOWAIT);
	if (*descp == NULL)
		return (USBD_NOMEM);
	err = usbd_get_report_descriptor(dev, id->bInterfaceNumber,
					 *sizep, *descp);
	if (err) {
		free(*descp, mem);
		*descp = NULL;
		return (err);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_get_config(usbd_device_handle dev, u_int8_t *conf)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_CONFIG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	return (usbd_do_request(dev, &req, conf));
}

usbd_status
usbd_bulk_transfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		   u_int16_t flags, u_int32_t timeout, void *buf,
		   u_int32_t *size, const char *lbl)
{
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	usbd_setup_xfer(xfer, pipe, 0, buf, *size, flags, timeout, NULL);
	DPRINTFN(1, ("usbd_bulk_transfer: start transfer %d bytes\n", *size));
	err = usbd_sync_transfer_sig(xfer);

	usbd_get_xfer_status(xfer, NULL, NULL, size, NULL);
	DPRINTFN(1,("usbd_bulk_transfer: transferred %d\n", *size));
	if (err) {
		DPRINTF(("usbd_bulk_transfer: error=%d\n", err));
		usbd_clear_endpoint_stall(pipe);
	}
	USBHIST_LOG(usbdebug, "<- done err %d", xfer, err, 0, 0);

	return (err);
}

usbd_status
usbd_intr_transfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		   u_int16_t flags, u_int32_t timeout, void *buf,
		   u_int32_t *size, const char *lbl)
{
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	usbd_setup_xfer(xfer, pipe, 0, buf, *size, flags, timeout, NULL);

	DPRINTFN(1, ("usbd_intr_transfer: start transfer %d bytes\n", *size));
	err = usbd_sync_transfer_sig(xfer);

	usbd_get_xfer_status(xfer, NULL, NULL, size, NULL);

	DPRINTFN(1,("usbd_intr_transfer: transferred %d\n", *size));
	if (err) {
		DPRINTF(("usbd_intr_transfer: error=%d\n", err));
		usbd_clear_endpoint_stall(pipe);
	}
	USBHIST_LOG(usbdebug, "<- done err %d", xfer, err, 0, 0);

	return (err);
}

void
usb_detach_wait(device_t dv, kcondvar_t *cv, kmutex_t *lock)
{
	DPRINTF(("usb_detach_wait: waiting for %s\n", device_xname(dv)));
	if (cv_timedwait(cv, lock, hz * 60))	// dv, PZERO, "usbdet", hz * 60
		printf("usb_detach_wait: %s didn't detach\n",
		        device_xname(dv));
	DPRINTF(("usb_detach_wait: %s done\n", device_xname(dv)));
}

void
usb_detach_broadcast(device_t dv, kcondvar_t *cv)
{
	DPRINTF(("usb_detach_broadcast: for %s\n", device_xname(dv)));
	cv_broadcast(cv);
}

void
usb_detach_waitold(device_t dv)
{
	DPRINTF(("usb_detach_waitold: waiting for %s\n", device_xname(dv)));
	if (tsleep(dv, PZERO, "usbdet", hz * 60)) /* XXXSMP ok */
		printf("usb_detach_waitold: %s didn't detach\n",
		        device_xname(dv));
	DPRINTF(("usb_detach_waitold: %s done\n", device_xname(dv)));
}

void
usb_detach_wakeupold(device_t dv)
{
	DPRINTF(("usb_detach_wakeupold: for %s\n", device_xname(dv)));
	wakeup(dv); /* XXXSMP ok */
}

const usb_cdc_descriptor_t *
usb_find_desc(usbd_device_handle dev, int type, int subtype)
{
	usbd_desc_iter_t iter;
	const usb_cdc_descriptor_t *desc;

	usb_desc_iter_init(dev, &iter);
	for (;;) {
		desc = (const usb_cdc_descriptor_t *)usb_desc_iter_next(&iter);
		if (!desc || (desc->bDescriptorType == type &&
			      (subtype == USBD_CDCSUBTYPE_ANY ||
			       subtype == desc->bDescriptorSubtype)))
			break;
	}
	return desc;
}

/* same as usb_find_desc(), but searches only in the specified interface. */
const usb_cdc_descriptor_t *
usb_find_desc_if(usbd_device_handle dev, int type, int subtype,
		 usb_interface_descriptor_t *id)
{
	usbd_desc_iter_t iter;
	const usb_cdc_descriptor_t *desc;

	if (id == NULL)
		return usb_find_desc(dev, type, subtype);

	usb_desc_iter_init(dev, &iter);

	iter.cur = (void *)id;		/* start from the interface desc */
	usb_desc_iter_next(&iter);	/* and skip it */

	while ((desc = (const usb_cdc_descriptor_t *)usb_desc_iter_next(&iter))
	       != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			/* we ran into the next interface --- not found */
			return NULL;
		}
		if (desc->bDescriptorType == type &&
		    (subtype == USBD_CDCSUBTYPE_ANY ||
		     subtype == desc->bDescriptorSubtype))
			break;
	}
	return desc;
}
