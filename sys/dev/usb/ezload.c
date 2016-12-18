/*	$NetBSD: ezload.c,v 1.15 2013/01/05 23:34:16 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ezload.c,v 1.15 2013/01/05 23:34:16 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/ezload.h>

/*
 * Vendor specific request code for Anchor Upload/Download
 */

/* This one is implemented in the core */
#define ANCHOR_LOAD_INTERNAL	0xA0

/* This is the highest internal RAM address for the AN2131Q */
#define ANCHOR_MAX_INTERNAL_ADDRESS  0x1B3F

/*
 * EZ-USB Control and Status Register.  Bit 0 controls 8051 reset
 */
#define ANCHOR_CPUCS_REG	0x7F92
#define  ANCHOR_RESET		0x01

/*
 * Although USB does not limit you here, the Anchor docs
 * quote 64 as a limit, and Mato@activewireinc.com suggested
 * to use 16.
 */
#define ANCHOR_CHUNK 16

/*
 * This is a firmware loader for ezusb (Anchor) devices. When the firmware
 * has been downloaded the device will simulate a disconnect and when it
 * is next recognized by the USB software it will appear as another
 * device.
 */

#ifdef EZLOAD_DEBUG
#define DPRINTF(x)	if (ezloaddebug) printf x
#define DPRINTFN(n,x)	if (ezloaddebug>(n)) printf x
int ezloaddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

usbd_status
ezload_reset(usbd_device_handle dev, int reset)
{
	usb_device_request_t req;
	uByte rst;

	DPRINTF(("ezload_reset: reset=%d\n", reset));

	rst = reset ? ANCHOR_RESET : 0;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ANCHOR_LOAD_INTERNAL;
	USETW(req.wValue, ANCHOR_CPUCS_REG);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	return (usbd_do_request(dev, &req, &rst));
}

usbd_status
ezload_download(usbd_device_handle dev, const struct ezdata *rec)
{
	usb_device_request_t req;
	const struct ezdata *ptr;
	u_int len, offs;
	int err;

	DPRINTF(("ezload_down record=%p\n", rec));

	for (ptr = rec; ptr->length != 0; ptr++) {

#if 0
		if (ptr->address + ptr->length > ANCHOR_MAX_INTERNAL_ADDRESS)
			return (USBD_INVAL);
#endif

		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
		req.bRequest = ANCHOR_LOAD_INTERNAL;
		USETW(req.wIndex, 0);
		for (offs = 0; offs < ptr->length; offs += ANCHOR_CHUNK) {
			len = ptr->length - offs;
			if (len > ANCHOR_CHUNK)
				len = ANCHOR_CHUNK;
			USETW(req.wValue, ptr->address + offs);
			USETW(req.wLength, len);
			DPRINTFN(2,("ezload_download: addr=0x%x len=%d\n",
				    ptr->address + offs, len));
			/*XXXUNCONST*/
			err = usbd_do_request(dev, &req,
			    __UNCONST(ptr->data + offs));
			if (err)
				return (err);
		}
	}

	return (0);
}

usbd_status
ezload_downloads_and_reset(usbd_device_handle dev, const struct ezdata **recs)
{
	usbd_status err;

	/*(void)ezload_reset(dev, 1);*/
	err = ezload_reset(dev, 1);
	if (err)
		return (err);
	usbd_delay_ms(dev, 250);
	while (*recs != NULL) {
		err = ezload_download(dev, *recs++);
		if (err)
			return (err);
	}
	usbd_delay_ms(dev, 250);
	err = ezload_reset(dev, 0);
	usbd_delay_ms(dev, 250);
	return (err);
}
