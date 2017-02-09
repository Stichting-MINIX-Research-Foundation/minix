/*	$NetBSD: usb_subr.c,v 1.202 2015/07/09 07:20:57 skrll Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_subr.c,v 1.18 1999/11/17 22:33:47 n_hibma Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: usb_subr.c,v 1.202 2015/07/09 07:20:57 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_usb.h"
#include "opt_usbverbose.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/module.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/usb_verbose.h>

#include "locators.h"

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

MALLOC_DEFINE(M_USB, "USB", "USB misc. memory");
MALLOC_DEFINE(M_USBDEV, "USB device", "USB device driver");
MALLOC_DEFINE(M_USBHC, "USB HC", "USB host controller");

Static usbd_status usbd_set_config(usbd_device_handle, int);
Static void usbd_devinfo(usbd_device_handle, int, char *, size_t);
Static void usbd_devinfo_vp(usbd_device_handle, char *, size_t, char *, size_t,
    int, int);
Static int usbd_getnewaddr(usbd_bus_handle);
Static int usbd_print(void *, const char *);
Static int usbd_ifprint(void *, const char *);
Static void usbd_free_iface_data(usbd_device_handle, int);

uint32_t usb_cookie_no = 0;

Static const char * const usbd_error_strs[] = {
	"NORMAL_COMPLETION",
	"IN_PROGRESS",
	"PENDING_REQUESTS",
	"NOT_STARTED",
	"INVAL",
	"NOMEM",
	"CANCELLED",
	"BAD_ADDRESS",
	"IN_USE",
	"NO_ADDR",
	"SET_ADDR_FAILED",
	"NO_POWER",
	"TOO_DEEP",
	"IOERROR",
	"NOT_CONFIGURED",
	"TIMEOUT",
	"SHORT_XFER",
	"STALLED",
	"INTERRUPTED",
	"XXX",
};

DEV_VERBOSE_DEFINE(usb);

const char *
usbd_errstr(usbd_status err)
{
	static char buffer[5];

	if (err < USBD_ERROR_MAX) {
		return usbd_error_strs[err];
	} else {
		snprintf(buffer, sizeof buffer, "%d", err);
		return buffer;
	}
}

usbd_status
usbd_get_string_desc(usbd_device_handle dev, int sindex, int langid,
		     usb_string_descriptor_t *sdesc, int *sizep)
{
	usb_device_request_t req;
	usbd_status err;
	int actlen;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_STRING, sindex);
	USETW(req.wIndex, langid);
	USETW(req.wLength, 2);	/* only size byte first */
	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
		&actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen < 2)
		return (USBD_SHORT_XFER);

	USETW(req.wLength, sdesc->bLength);	/* the whole string */
 	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
 		&actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen != sdesc->bLength) {
		DPRINTFN(-1, ("usbd_get_string_desc: expected %d, got %d\n",
		    sdesc->bLength, actlen));
	}

	*sizep = actlen;
	return (USBD_NORMAL_COMPLETION);
}

static void
usbd_trim_spaces(char *p)
{
	char *q, *e;

	q = e = p;
	while (*q == ' ')		/* skip leading spaces */
		q++;
	while ((*p = *q++))		/* copy string */
		if (*p++ != ' ')	/* remember last non-space */
			e = p;
	*e = '\0';			/* kill trailing spaces */
}

Static void
usbd_devinfo_vp(usbd_device_handle dev, char *v, size_t vl, char *p,
    size_t pl, int usedev, int useencoded)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	if (dev == NULL)
		return;

	v[0] = p[0] = '\0';

	if (usedev) {
		if (usbd_get_string0(dev, udd->iManufacturer, v, useencoded) ==
		    USBD_NORMAL_COMPLETION)
			usbd_trim_spaces(v);
		if (usbd_get_string0(dev, udd->iProduct, p, useencoded) ==
		    USBD_NORMAL_COMPLETION)
			usbd_trim_spaces(p);
	}
	if (v[0] == '\0')
		usb_findvendor(v, vl, UGETW(udd->idVendor));
	if (p[0] == '\0')
		usb_findproduct(p, pl, UGETW(udd->idVendor),
		    UGETW(udd->idProduct));
}

int
usbd_printBCD(char *cp, size_t l, int bcd)
{
	return snprintf(cp, l, "%x.%02x", bcd >> 8, bcd & 0xff);
}

Static void
usbd_devinfo(usbd_device_handle dev, int showclass, char *cp, size_t l)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char *vendor, *product;
	int bcdDevice, bcdUSB;
	char *ep;

	vendor = malloc(USB_MAX_ENCODED_STRING_LEN * 2, M_USB, M_NOWAIT);
	if (vendor == NULL) {
		*cp = '\0';
		return;
	}
	product = &vendor[USB_MAX_ENCODED_STRING_LEN];

	ep = cp + l;

	usbd_devinfo_vp(dev, vendor, USB_MAX_ENCODED_STRING_LEN,
	    product, USB_MAX_ENCODED_STRING_LEN, 1, 1);
	cp += snprintf(cp, ep - cp, "%s %s", vendor, product);
	if (showclass)
		cp += snprintf(cp, ep - cp, ", class %d/%d",
		    udd->bDeviceClass, udd->bDeviceSubClass);
	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);
	cp += snprintf(cp, ep - cp, ", rev ");
	cp += usbd_printBCD(cp, ep - cp, bcdUSB);
	*cp++ = '/';
	cp += usbd_printBCD(cp, ep - cp, bcdDevice);
	cp += snprintf(cp, ep - cp, ", addr %d", dev->address);
	*cp = 0;
	free(vendor, M_USB);
}

char *
usbd_devinfo_alloc(usbd_device_handle dev, int showclass)
{
	char *devinfop;

	devinfop = malloc(DEVINFOSIZE, M_TEMP, M_WAITOK);
	usbd_devinfo(dev, showclass, devinfop, DEVINFOSIZE);
	return devinfop;
}

void
usbd_devinfo_free(char *devinfop)
{
	free(devinfop, M_TEMP);
}

/* Delay for a certain number of ms */
void
usb_delay_ms_locked(usbd_bus_handle bus, u_int ms, kmutex_t *lock)
{
	/* Wait at least two clock ticks so we know the time has passed. */
	if (bus->use_polling || cold)
		delay((ms+1) * 1000);
	else
		kpause("usbdly", false, (ms*hz+999)/1000 + 1, lock);
}

void
usb_delay_ms(usbd_bus_handle bus, u_int ms)
{
	usb_delay_ms_locked(bus, ms, NULL);
}

/* Delay given a device handle. */
void
usbd_delay_ms_locked(usbd_device_handle dev, u_int ms, kmutex_t *lock)
{
	usb_delay_ms_locked(dev->bus, ms, lock);
}

/* Delay given a device handle. */
void
usbd_delay_ms(usbd_device_handle dev, u_int ms)
{
	usb_delay_ms_locked(dev->bus, ms, NULL);
}

usbd_status
usbd_reset_port(usbd_device_handle dev, int port, usb_port_status_t *ps)
{
	usb_device_request_t req;
	usbd_status err;
	int n;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UHF_PORT_RESET);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);
	DPRINTFN(1,("usbd_reset_port: port %d reset done, error=%s\n",
		    port, usbd_errstr(err)));
	if (err)
		return (err);
	n = 10;
	do {
		/* Wait for device to recover from reset. */
		usbd_delay_ms(dev, USB_PORT_RESET_DELAY);
		err = usbd_get_port_status(dev, port, ps);
		if (err) {
			DPRINTF(("usbd_reset_port: get status failed %d\n",
				 err));
			return (err);
		}
		/* If the device disappeared, just give up. */
		if (!(UGETW(ps->wPortStatus) & UPS_CURRENT_CONNECT_STATUS))
			return (USBD_NORMAL_COMPLETION);
	} while ((UGETW(ps->wPortChange) & UPS_C_PORT_RESET) == 0 && --n > 0);
	if (n == 0)
		return (USBD_TIMEOUT);
	err = usbd_clear_port_feature(dev, port, UHF_C_PORT_RESET);
#ifdef USB_DEBUG
	if (err)
		DPRINTF(("usbd_reset_port: clear port feature failed %d\n",
			 err));
#endif

	/* Wait for the device to recover from reset. */
	usbd_delay_ms(dev, USB_PORT_RESET_RECOVERY);
	return (err);
}

usb_interface_descriptor_t *
usbd_find_idesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx)
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	int curidx, lastidx, curaidx = 0;

	for (curidx = lastidx = -1; p < end; ) {
		d = (usb_interface_descriptor_t *)p;
		DPRINTFN(4,("usbd_find_idesc: idx=%d(%d) altidx=%d(%d) len=%d "
			    "type=%d\n",
			    ifaceidx, curidx, altidx, curaidx,
			    d->bLength, d->bDescriptorType));
		if (d->bLength == 0) /* bad descriptor */
			break;
		p += d->bLength;
		if (p <= end && d->bDescriptorType == UDESC_INTERFACE) {
			if (d->bInterfaceNumber != lastidx) {
				lastidx = d->bInterfaceNumber;
				curidx++;
				curaidx = 0;
			} else
				curaidx++;
			if (ifaceidx == curidx && altidx == curaidx)
				return (d);
		}
	}
	return (NULL);
}

usb_endpoint_descriptor_t *
usbd_find_edesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx,
		int endptidx)
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	usb_endpoint_descriptor_t *e;
	int curidx;

	d = usbd_find_idesc(cd, ifaceidx, altidx);
	if (d == NULL)
		return (NULL);
	if (endptidx >= d->bNumEndpoints) /* quick exit */
		return (NULL);

	curidx = -1;
	for (p = (char *)d + d->bLength; p < end; ) {
		e = (usb_endpoint_descriptor_t *)p;
		if (e->bLength == 0) /* bad descriptor */
			break;
		p += e->bLength;
		if (p <= end && e->bDescriptorType == UDESC_INTERFACE)
			return (NULL);
		if (p <= end && e->bDescriptorType == UDESC_ENDPOINT) {
			curidx++;
			if (curidx == endptidx)
				return (e);
		}
	}
	return (NULL);
}

usbd_status
usbd_fill_iface_data(usbd_device_handle dev, int ifaceidx, int altidx)
{
	usbd_interface_handle ifc = &dev->ifaces[ifaceidx];
	usb_interface_descriptor_t *idesc;
	char *p, *end;
	int endpt, nendpt;

	DPRINTFN(4,("usbd_fill_iface_data: ifaceidx=%d altidx=%d\n",
		    ifaceidx, altidx));
	idesc = usbd_find_idesc(dev->cdesc, ifaceidx, altidx);
	if (idesc == NULL)
		return (USBD_INVAL);
	ifc->device = dev;
	ifc->idesc = idesc;
	ifc->index = ifaceidx;
	ifc->altindex = altidx;
	nendpt = ifc->idesc->bNumEndpoints;
	DPRINTFN(4,("usbd_fill_iface_data: found idesc nendpt=%d\n", nendpt));
	if (nendpt != 0) {
		ifc->endpoints = malloc(nendpt * sizeof(struct usbd_endpoint),
					M_USB, M_NOWAIT);
		if (ifc->endpoints == NULL)
			return (USBD_NOMEM);
	} else
		ifc->endpoints = NULL;
	ifc->priv = NULL;
	p = (char *)ifc->idesc + ifc->idesc->bLength;
	end = (char *)dev->cdesc + UGETW(dev->cdesc->wTotalLength);
#define ed ((usb_endpoint_descriptor_t *)p)
	for (endpt = 0; endpt < nendpt; endpt++) {
		DPRINTFN(10,("usbd_fill_iface_data: endpt=%d\n", endpt));
		for (; p < end; p += ed->bLength) {
			DPRINTFN(10,("usbd_fill_iface_data: p=%p end=%p "
				     "len=%d type=%d\n",
				 p, end, ed->bLength, ed->bDescriptorType));
			if (p + ed->bLength <= end && ed->bLength != 0 &&
			    ed->bDescriptorType == UDESC_ENDPOINT)
				goto found;
			if (ed->bLength == 0 ||
			    ed->bDescriptorType == UDESC_INTERFACE)
				break;
		}
		/* passed end, or bad desc */
		printf("usbd_fill_iface_data: bad descriptor(s): %s\n",
		       ed->bLength == 0 ? "0 length" :
		       ed->bDescriptorType == UDESC_INTERFACE ? "iface desc":
		       "out of data");
		goto bad;
	found:
		ifc->endpoints[endpt].edesc = ed;
		if (dev->speed == USB_SPEED_HIGH) {
			u_int mps;
			/* Control and bulk endpoints have max packet limits. */
			switch (UE_GET_XFERTYPE(ed->bmAttributes)) {
			case UE_CONTROL:
				mps = USB_2_MAX_CTRL_PACKET;
				goto check;
			case UE_BULK:
				mps = USB_2_MAX_BULK_PACKET;
			check:
				if (UGETW(ed->wMaxPacketSize) != mps) {
					USETW(ed->wMaxPacketSize, mps);
#ifdef DIAGNOSTIC
					printf("usbd_fill_iface_data: bad max "
					       "packet size\n");
#endif
				}
				break;
			default:
				break;
			}
		}
		ifc->endpoints[endpt].refcnt = 0;
		ifc->endpoints[endpt].datatoggle = 0;
		p += ed->bLength;
	}
#undef ed
	LIST_INIT(&ifc->pipes);
	return (USBD_NORMAL_COMPLETION);

 bad:
	if (ifc->endpoints != NULL) {
		free(ifc->endpoints, M_USB);
		ifc->endpoints = NULL;
	}
	return (USBD_INVAL);
}

void
usbd_free_iface_data(usbd_device_handle dev, int ifcno)
{
	usbd_interface_handle ifc = &dev->ifaces[ifcno];
	if (ifc->endpoints)
		free(ifc->endpoints, M_USB);
}

Static usbd_status
usbd_set_config(usbd_device_handle dev, int conf)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_CONFIG;
	USETW(req.wValue, conf);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_config_no(usbd_device_handle dev, int no, int msg)
{
	int index;
	usb_config_descriptor_t cd;
	usbd_status err;

	if (no == USB_UNCONFIG_NO)
		return (usbd_set_config_index(dev, USB_UNCONFIG_INDEX, msg));

	DPRINTFN(5,("usbd_set_config_no: %d\n", no));
	/* Figure out what config index to use. */
	for (index = 0; index < dev->ddesc.bNumConfigurations; index++) {
		err = usbd_get_config_desc(dev, index, &cd);
		if (err)
			return (err);
		if (cd.bConfigurationValue == no)
			return (usbd_set_config_index(dev, index, msg));
	}
	return (USBD_INVAL);
}

usbd_status
usbd_set_config_index(usbd_device_handle dev, int index, int msg)
{
	usb_config_descriptor_t cd, *cdp;
	usbd_status err;
	int i, ifcidx, nifc, len, selfpowered, power;

	DPRINTFN(5,("usbd_set_config_index: dev=%p index=%d\n", dev, index));

	if (index >= dev->ddesc.bNumConfigurations &&
	    index != USB_UNCONFIG_INDEX) {
		/* panic? */
		printf("usbd_set_config_index: illegal index\n");
		return (USBD_INVAL);
	}

	/* XXX check that all interfaces are idle */
	if (dev->config != USB_UNCONFIG_NO) {
		DPRINTF(("usbd_set_config_index: free old config\n"));
		/* Free all configuration data structures. */
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB);
		free(dev->cdesc, M_USB);
		dev->ifaces = NULL;
		dev->cdesc = NULL;
		dev->config = USB_UNCONFIG_NO;
	}

	if (index == USB_UNCONFIG_INDEX) {
		/* We are unconfiguring the device, so leave unallocated. */
		DPRINTF(("usbd_set_config_index: set config 0\n"));
		err = usbd_set_config(dev, USB_UNCONFIG_NO);
		if (err) {
			DPRINTF(("usbd_set_config_index: setting config=0 "
				 "failed, error=%s\n", usbd_errstr(err)));
		}
		return (err);
	}

	/* Get the short descriptor. */
	err = usbd_get_config_desc(dev, index, &cd);
	if (err) {
		DPRINTF(("usbd_set_config_index: get_config_desc=%d\n", err));
		return (err);
	}
	len = UGETW(cd.wTotalLength);
	cdp = malloc(len, M_USB, M_NOWAIT);
	if (cdp == NULL)
		return (USBD_NOMEM);

	/* Get the full descriptor.  Try a few times for slow devices. */
	for (i = 0; i < 3; i++) {
		err = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdp);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
	}
	if (err) {
		DPRINTF(("usbd_set_config_index: get_desc=%d\n", err));
		goto bad;
	}
	if (cdp->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_set_config_index: bad desc %d\n",
			     cdp->bDescriptorType));
		err = USBD_INVAL;
		goto bad;
	}

	/*
	 * Figure out if the device is self or bus powered.
	 */
#if 0 /* XXX various devices don't report the power state correctly */
	selfpowered = 0;
	err = usbd_get_device_status(dev, &ds);
	if (!err && (UGETW(ds.wStatus) & UDS_SELF_POWERED))
		selfpowered = 1;
#endif
	/*
	 * Use the power state in the configuration we are going
	 * to set. This doesn't necessarily reflect the actual
	 * power state of the device; the driver can control this
	 * by choosing the appropriate configuration.
	 */
	selfpowered = !!(cdp->bmAttributes & UC_SELF_POWERED);

	DPRINTF(("usbd_set_config_index: (addr %d) cno=%d attr=0x%02x, "
		 "selfpowered=%d, power=%d\n",
		 cdp->bConfigurationValue, dev->address, cdp->bmAttributes,
		 selfpowered, cdp->bMaxPower * 2));

	/* Check if we have enough power. */
#if 0 /* this is a no-op, see above */
	if ((cdp->bmAttributes & UC_SELF_POWERED) && !selfpowered) {
		if (msg)
			printf("%s: device addr %d (config %d): "
				 "can't set self powered configuration\n",
			       device_xname(dev->bus->bdev), dev->address,
			       cdp->bConfigurationValue);
		err = USBD_NO_POWER;
		goto bad;
	}
#endif
#ifdef USB_DEBUG
	if (dev->powersrc == NULL) {
		DPRINTF(("usbd_set_config_index: No power source?\n"));
		err = USBD_IOERROR;
		goto bad;
	}
#endif
	power = cdp->bMaxPower * 2;
	if (power > dev->powersrc->power) {
		DPRINTF(("power exceeded %d %d\n", power,dev->powersrc->power));
		/* XXX print nicer message. */
		if (msg)
			printf("%s: device addr %d (config %d) exceeds power "
				 "budget, %d mA > %d mA\n",
			       device_xname(dev->bus->usbctl), dev->address,
			       cdp->bConfigurationValue,
			       power, dev->powersrc->power);
		err = USBD_NO_POWER;
		goto bad;
	}
	dev->power = power;
	dev->self_powered = selfpowered;

	/* Set the actual configuration value. */
	DPRINTF(("usbd_set_config_index: set config %d\n",
		 cdp->bConfigurationValue));
	err = usbd_set_config(dev, cdp->bConfigurationValue);
	if (err) {
		DPRINTF(("usbd_set_config_index: setting config=%d failed, "
			 "error=%s\n",
			 cdp->bConfigurationValue, usbd_errstr(err)));
		goto bad;
	}

	/* Allocate and fill interface data. */
	nifc = cdp->bNumInterface;
	dev->ifaces = malloc(nifc * sizeof(struct usbd_interface),
			     M_USB, M_NOWAIT);
	if (dev->ifaces == NULL) {
		err = USBD_NOMEM;
		goto bad;
	}
	DPRINTFN(5,("usbd_set_config_index: dev=%p cdesc=%p\n", dev, cdp));
	dev->cdesc = cdp;
	dev->config = cdp->bConfigurationValue;
	for (ifcidx = 0; ifcidx < nifc; ifcidx++) {
		err = usbd_fill_iface_data(dev, ifcidx, 0);
		if (err) {
			while (--ifcidx >= 0)
				usbd_free_iface_data(dev, ifcidx);
			goto bad;
		}
	}

	return (USBD_NORMAL_COMPLETION);

 bad:
	free(cdp, M_USB);
	return (err);
}

/* XXX add function for alternate settings */

usbd_status
usbd_setup_pipe(usbd_device_handle dev, usbd_interface_handle iface,
		struct usbd_endpoint *ep, int ival, usbd_pipe_handle *pipe)
{
	return usbd_setup_pipe_flags(dev, iface, ep, ival, pipe, 0);
}

usbd_status
usbd_setup_pipe_flags(usbd_device_handle dev, usbd_interface_handle iface,
    struct usbd_endpoint *ep, int ival, usbd_pipe_handle *pipe, uint8_t flags)
{
	usbd_pipe_handle p;
	usbd_status err;

	p = malloc(dev->bus->pipe_size, M_USB, M_NOWAIT);
	DPRINTFN(1,("usbd_setup_pipe: dev=%p iface=%p ep=%p pipe=%p\n",
		    dev, iface, ep, p));
	if (p == NULL)
		return (USBD_NOMEM);
	p->device = dev;
	p->iface = iface;
	p->endpoint = ep;
	ep->refcnt++;
	p->refcnt = 1;
	p->intrxfer = NULL;
	p->running = 0;
	p->aborting = 0;
	p->repeat = 0;
	p->interval = ival;
	p->flags = flags;
	SIMPLEQ_INIT(&p->queue);
	err = dev->bus->methods->open_pipe(p);
	if (err) {
		DPRINTFN(-1,("usbd_setup_pipe: endpoint=0x%x failed, error="
			 "%s\n",
			 ep->edesc->bEndpointAddress, usbd_errstr(err)));
		free(p, M_USB);
		return (err);
	}
	usb_init_task(&p->async_task, usbd_clear_endpoint_stall_task, p,
	    USB_TASKQ_MPSAFE);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

/* Abort the device control pipe. */
void
usbd_kill_pipe(usbd_pipe_handle pipe)
{
	usbd_abort_pipe(pipe);
	usbd_lock_pipe(pipe);
	pipe->methods->close(pipe);
	usbd_unlock_pipe(pipe);
	usb_rem_task(pipe->device, &pipe->async_task);
	pipe->endpoint->refcnt--;
	free(pipe, M_USB);
}

int
usbd_getnewaddr(usbd_bus_handle bus)
{
	int addr;

	for (addr = 1; addr < USB_MAX_DEVICES; addr++)
		if (bus->devices[addr] == NULL)
			return (addr);
	return (-1);
}

usbd_status
usbd_attach_roothub(device_t parent, usbd_device_handle dev)
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
	device_t dv;

	uaa.device = dev;
	uaa.usegeneric = 0;
	uaa.port = 0;
	uaa.vendor = UGETW(dd->idVendor);
	uaa.product = UGETW(dd->idProduct);
	uaa.release = UGETW(dd->bcdDevice);
	uaa.class = dd->bDeviceClass;
	uaa.subclass = dd->bDeviceSubClass;
	uaa.proto = dd->bDeviceProtocol;

	dv = config_found_ia(parent, "usbroothubif", &uaa, 0);
	if (dv) {
		dev->subdevs = malloc(sizeof dv, M_USB, M_NOWAIT);
		if (dev->subdevs == NULL)
			return (USBD_NOMEM);
		dev->subdevs[0] = dv;
		dev->subdevlen = 1;
	}
	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
usbd_attachwholedevice(device_t parent, usbd_device_handle dev, int port,
	int usegeneric)
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
	device_t dv;
	int dlocs[USBDEVIFCF_NLOCS];

	uaa.device = dev;
	uaa.usegeneric = usegeneric;
	uaa.port = port;
	uaa.vendor = UGETW(dd->idVendor);
	uaa.product = UGETW(dd->idProduct);
	uaa.release = UGETW(dd->bcdDevice);
	uaa.class = dd->bDeviceClass;
	uaa.subclass = dd->bDeviceSubClass;
	uaa.proto = dd->bDeviceProtocol;

	dlocs[USBDEVIFCF_PORT] = uaa.port;
	dlocs[USBDEVIFCF_VENDOR] = uaa.vendor;
	dlocs[USBDEVIFCF_PRODUCT] = uaa.product;
	dlocs[USBDEVIFCF_RELEASE] = uaa.release;
	/* the rest is historical ballast */
	dlocs[USBDEVIFCF_CONFIGURATION] = -1;
	dlocs[USBDEVIFCF_INTERFACE] = -1;

	dv = config_found_sm_loc(parent, "usbdevif", dlocs, &uaa, usbd_print,
				 config_stdsubmatch);
	if (dv) {
		dev->subdevs = malloc(sizeof dv, M_USB, M_NOWAIT);
		if (dev->subdevs == NULL)
			return (USBD_NOMEM);
		dev->subdevs[0] = dv;
		dev->subdevlen = 1;
		dev->nifaces_claimed = 1; /* XXX */
	}
	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
usbd_attachinterfaces(device_t parent, usbd_device_handle dev,
		      int port, const int *locators)
{
	struct usbif_attach_arg uiaa;
	int ilocs[USBIFIFCF_NLOCS];
	usb_device_descriptor_t *dd = &dev->ddesc;
	int nifaces;
	usbd_interface_handle *ifaces;
	int i, j, loc;
	device_t dv;

	nifaces = dev->cdesc->bNumInterface;
	ifaces = malloc(nifaces * sizeof(*ifaces), M_USB, M_NOWAIT|M_ZERO);
	if (!ifaces)
		return (USBD_NOMEM);
	for (i = 0; i < nifaces; i++) {
		if (!dev->subdevs[i]) {
			ifaces[i] = &dev->ifaces[i];
		}
		DPRINTF(("%s: interface %d %p\n", __func__, i, ifaces[i]));
	}

	uiaa.device = dev;
	uiaa.port = port;
	uiaa.vendor = UGETW(dd->idVendor);
	uiaa.product = UGETW(dd->idProduct);
	uiaa.release = UGETW(dd->bcdDevice);
	uiaa.configno = dev->cdesc->bConfigurationValue;
	uiaa.ifaces = ifaces;
	uiaa.nifaces = nifaces;
	ilocs[USBIFIFCF_PORT] = uiaa.port;
	ilocs[USBIFIFCF_VENDOR] = uiaa.vendor;
	ilocs[USBIFIFCF_PRODUCT] = uiaa.product;
	ilocs[USBIFIFCF_RELEASE] = uiaa.release;
	ilocs[USBIFIFCF_CONFIGURATION] = uiaa.configno;

	for (i = 0; i < nifaces; i++) {
		if (!ifaces[i]) {
			DPRINTF(("%s: interface %d claimed\n", __func__, i));
			continue; /* interface already claimed */
		}
		uiaa.iface = ifaces[i];
		uiaa.class = ifaces[i]->idesc->bInterfaceClass;
		uiaa.subclass = ifaces[i]->idesc->bInterfaceSubClass;
		uiaa.proto = ifaces[i]->idesc->bInterfaceProtocol;
		uiaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;

		DPRINTF(("%s: searching for interface %d "
		    "class %x subclass %x proto %x ifaceno %d\n", __func__, i,
		    uiaa.class, uiaa.subclass, uiaa.proto, uiaa.ifaceno));
		ilocs[USBIFIFCF_INTERFACE] = uiaa.ifaceno;
		if (locators != NULL) {
			loc = locators[USBIFIFCF_CONFIGURATION];
			if (loc != USBIFIFCF_CONFIGURATION_DEFAULT &&
			    loc != uiaa.configno)
				continue;
			loc = locators[USBIFIFCF_INTERFACE];
			if (loc != USBIFIFCF_INTERFACE_DEFAULT &&
			    loc != uiaa.ifaceno)
				continue;
		}
		dv = config_found_sm_loc(parent, "usbifif", ilocs, &uiaa,
					 usbd_ifprint, config_stdsubmatch);
		if (!dv)
			continue;
		/* claim */
		ifaces[i] = NULL;
		/* account for ifaces claimed by the driver behind our back */
		for (j = 0; j < nifaces; j++) {
			if (!ifaces[j] && !dev->subdevs[j]) {
				DPRINTF(("%s: interface %d claimed "
				    "behind our back\n", __func__, j));
				dev->subdevs[j] = dv;
				dev->nifaces_claimed++;
			}
		}
	}

	free(ifaces, M_USB);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_probe_and_attach(device_t parent, usbd_device_handle dev,
                      int port, int addr)
{
	usb_device_descriptor_t *dd = &dev->ddesc;
	int confi, nifaces;
	usbd_status err;

	/* First try with device specific drivers. */
	DPRINTF(("usbd_probe_and_attach: trying device specific drivers\n"));
	err = usbd_attachwholedevice(parent, dev, port, 0);
	if (dev->nifaces_claimed || err)
		return (err);
	DPRINTF(("usbd_probe_and_attach: no device specific driver found\n"));

	DPRINTF(("usbd_probe_and_attach: looping over %d configurations\n",
		 dd->bNumConfigurations));
	for (confi = 0; confi < dd->bNumConfigurations; confi++) {
		DPRINTFN(1,("usbd_probe_and_attach: trying config idx=%d\n",
			    confi));
		err = usbd_set_config_index(dev, confi, 1);
		if (err) {
#ifdef USB_DEBUG
			DPRINTF(("%s: port %d, set config at addr %d failed, "
			    "error=%s\n", device_xname(parent), port,
			    addr, usbd_errstr(err)));
#else
			printf("%s: port %d, set config at addr %d failed\n",
			    device_xname(parent), port, addr);
#endif
			return (err);
		}
		nifaces = dev->cdesc->bNumInterface;
		dev->subdevs = malloc(nifaces * sizeof(device_t), M_USB,
				      M_NOWAIT|M_ZERO);
		if (dev->subdevs == NULL)
			return (USBD_NOMEM);
		dev->subdevlen = nifaces;

		err = usbd_attachinterfaces(parent, dev, port, NULL);

		if (!dev->nifaces_claimed) {
			free(dev->subdevs, M_USB);
			dev->subdevs = 0;
			dev->subdevlen = 0;
		}
		if (dev->nifaces_claimed || err)
			return (err);
	}
	/* No interfaces were attached in any of the configurations. */

	if (dd->bNumConfigurations > 1) /* don't change if only 1 config */
		usbd_set_config_index(dev, 0, 0);

	DPRINTF(("usbd_probe_and_attach: no interface drivers found\n"));

	/* Finally try the generic driver. */
	err = usbd_attachwholedevice(parent, dev, port, 1);

	/*
	 * The generic attach failed, but leave the device as it is.
	 * We just did not find any drivers, that's all.  The device is
	 * fully operational and not harming anyone.
	 */
	DPRINTF(("usbd_probe_and_attach: generic attach failed\n"));
	return (USBD_NORMAL_COMPLETION);
}

/**
 * Called from uhub_rescan().  usbd_new_device() for the target dev must be
 * called before calling this.
 */
usbd_status
usbd_reattach_device(device_t parent, usbd_device_handle dev,
                     int port, const int *locators)
{
	int i, loc;

	if (locators != NULL) {
		loc = locators[USBIFIFCF_PORT];
		if (loc != USBIFIFCF_PORT_DEFAULT && loc != port)
			return USBD_NORMAL_COMPLETION;
		loc = locators[USBIFIFCF_VENDOR];
		if (loc != USBIFIFCF_VENDOR_DEFAULT &&
		    loc != UGETW(dev->ddesc.idVendor))
			return USBD_NORMAL_COMPLETION;
		loc = locators[USBIFIFCF_PRODUCT];
		if (loc != USBIFIFCF_PRODUCT_DEFAULT &&
		    loc != UGETW(dev->ddesc.idProduct))
			return USBD_NORMAL_COMPLETION;
		loc = locators[USBIFIFCF_RELEASE];
		if (loc != USBIFIFCF_RELEASE_DEFAULT &&
		    loc != UGETW(dev->ddesc.bcdDevice))
			return USBD_NORMAL_COMPLETION;
	}
	if (dev->subdevlen == 0) {
		/* XXX: check USBIFIFCF_CONFIGURATION and
		 * USBIFIFCF_INTERFACE too */
		return usbd_probe_and_attach(parent, dev, port, dev->address);
	} else if (dev->subdevlen != dev->cdesc->bNumInterface) {
		/* device-specific or generic driver is already attached. */
		return USBD_NORMAL_COMPLETION;
	}
	/* Does the device have unconfigured interfaces? */
	for (i = 0; i < dev->subdevlen; i++) {
		if (dev->subdevs[i] == NULL) {
			break;
		}
	}
	if (i >= dev->subdevlen)
		return USBD_NORMAL_COMPLETION;
	return usbd_attachinterfaces(parent, dev, port, locators);
}

/*
 * Get the first 8 bytes of the device descriptor.
 * Do as Windows does: try to read 64 bytes -- there are devices which
 * recognize the initial descriptor fetch (before the control endpoint's
 * MaxPacketSize is known by the host) by exactly this length.
 */
usbd_status
usbd_get_initial_ddesc(usbd_device_handle dev, usb_device_descriptor_t *desc)
{
	usb_device_request_t req;
	char buf[64];
	int res, actlen;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_DEVICE, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 64);
	res = usbd_do_request_flags(dev, &req, buf, USBD_SHORT_XFER_OK,
		&actlen, USBD_DEFAULT_TIMEOUT);
	if (res)
		return res;
	if (actlen < 8)
		return USBD_SHORT_XFER;
	memcpy(desc, buf, 8);
	return USBD_NORMAL_COMPLETION;
}

/*
 * Called when a new device has been put in the powered state,
 * but not yet in the addressed state.
 * Get initial descriptor, set the address, get full descriptor,
 * and attach a driver.
 */
usbd_status
usbd_new_device(device_t parent, usbd_bus_handle bus, int depth,
                int speed, int port, struct usbd_port *up)
{
	usbd_device_handle dev, adev;
	struct usbd_device *hub;
	usb_device_descriptor_t *dd;
	usb_port_status_t ps;
	usbd_status err;
	int addr;
	int i;
	int p;

	DPRINTF(("usbd_new_device bus=%p port=%d depth=%d speed=%d\n",
		 bus, port, depth, speed));

	if (bus->methods->new_device != NULL)
		return (bus->methods->new_device)(parent, bus, depth, speed,
		    port, up);

	addr = usbd_getnewaddr(bus);
	if (addr < 0) {
		printf("%s: No free USB addresses, new device ignored.\n",
		       device_xname(bus->usbctl));
		return (USBD_NO_ADDR);
	}

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT|M_ZERO);
	if (dev == NULL)
		return (USBD_NOMEM);

	dev->bus = bus;

	/* Set up default endpoint handle. */
	dev->def_ep.edesc = &dev->def_ep_desc;

	/* Set up default endpoint descriptor. */
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	/*
	 * temporary, will be fixed after first descriptor fetch
	 * (which uses 64 bytes so it shouldn't be less),
	 * highspeed devices must support 64 byte packets anyway
	 */
	if (speed == USB_SPEED_HIGH || speed == USB_SPEED_FULL)
		USETW(dev->def_ep_desc.wMaxPacketSize, 64);
	else
		USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);

	dev->def_ep_desc.bInterval = 0;

	/* doesn't matter, just don't let it uninitialized */
	dev->def_ep.datatoggle = 0;

	dev->quirks = &usbd_no_quirk;
	dev->address = USB_START_ADDR;
	dev->ddesc.bMaxPacketSize = 0;
	dev->depth = depth;
	dev->powersrc = up;
	dev->myhub = up->parent;

	up->device = dev;

	/* Locate port on upstream high speed hub */
	for (adev = dev, hub = up->parent;
	     hub != NULL && hub->speed != USB_SPEED_HIGH;
	     adev = hub, hub = hub->myhub)
		;
	if (hub) {
		for (p = 0; p < hub->hub->hubdesc.bNbrPorts; p++) {
			if (hub->hub->ports[p].device == adev) {
				dev->myhsport = &hub->hub->ports[p];
				goto found;
			}
		}
		panic("usbd_new_device: cannot find HS port");
	found:
		DPRINTFN(1,("usbd_new_device: high speed port %d\n", p));
	} else {
		dev->myhsport = NULL;
	}
	dev->speed = speed;
	dev->langid = USBD_NOLANG;
	dev->cookie.cookie = ++usb_cookie_no;

	/* Establish the default pipe. */
	err = usbd_setup_pipe_flags(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
			      &dev->default_pipe, USBD_MPSAFE);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	dd = &dev->ddesc;
	/* Try a few times in case the device is slow (i.e. outside specs.) */
	for (i = 0; i < 10; i++) {
		/* Get the first 8 bytes of the device descriptor. */
		err = usbd_get_initial_ddesc(dev, dd);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
		if ((i & 3) == 3)
			usbd_reset_port(up->parent, port, &ps);
	}
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting first desc "
			      "failed: %d\n", addr, err));
		usbd_remove_device(dev, up);
		return (err);
	}

	/* Windows resets the port here, do likewise */
	if (up->parent)
		usbd_reset_port(up->parent, port, &ps);

	if (speed == USB_SPEED_HIGH) {
		/* Max packet size must be 64 (sec 5.5.3). */
		if (dd->bMaxPacketSize != USB_2_MAX_CTRL_PACKET) {
#ifdef DIAGNOSTIC
			printf("usbd_new_device: addr=%d bad max packet "
			    "size=%d. adjusting to %d.\n",
			    addr, dd->bMaxPacketSize, USB_2_MAX_CTRL_PACKET);
#endif
			dd->bMaxPacketSize = USB_2_MAX_CTRL_PACKET;
		}
	}

	DPRINTF(("usbd_new_device: adding unit addr=%d, rev=%02x, class=%d, "
		 "subclass=%d, protocol=%d, maxpacket=%d, len=%d, speed=%d\n",
		 addr,UGETW(dd->bcdUSB), dd->bDeviceClass, dd->bDeviceSubClass,
		 dd->bDeviceProtocol, dd->bMaxPacketSize, dd->bLength,
		 dev->speed));

	if (dd->bDescriptorType != UDESC_DEVICE) {
		/* Illegal device descriptor */
		DPRINTFN(-1,("usbd_new_device: illegal descriptor %d\n",
			     dd->bDescriptorType));
		usbd_remove_device(dev, up);
		return (USBD_INVAL);
	}

	if (dd->bLength < USB_DEVICE_DESCRIPTOR_SIZE) {
		DPRINTFN(-1,("usbd_new_device: bad length %d\n", dd->bLength));
		usbd_remove_device(dev, up);
		return (USBD_INVAL);
	}

	USETW(dev->def_ep_desc.wMaxPacketSize, dd->bMaxPacketSize);

	/* Re-establish the default pipe with the new MPS. */
	usbd_kill_pipe(dev->default_pipe);
	err = usbd_setup_pipe_flags(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe, USBD_MPSAFE);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: setup default pipe failed\n"));
		usbd_remove_device(dev, up);
		return err;
	}

	/* Set the address */
	DPRINTFN(5, ("usbd_new_device: setting device address=%d\n", addr));
	err = usbd_set_address(dev, addr);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: set address %d failed\n", addr));
		err = USBD_SET_ADDR_FAILED;
		usbd_remove_device(dev, up);
		return err;
	}

	/* Allow device time to set new address */
	usbd_delay_ms(dev, USB_SET_ADDRESS_SETTLE);
	dev->address = addr;	/* new device address now */
	bus->devices[addr] = dev;

	/* Re-establish the default pipe with the new address. */
	usbd_kill_pipe(dev->default_pipe);
	err = usbd_setup_pipe_flags(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe, USBD_MPSAFE);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: setup default pipe failed\n"));
		usbd_remove_device(dev, up);
		return err;
	}

	err = usbd_reload_device_desc(dev);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting full desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (err);
	}

	/* Assume 100mA bus powered for now. Changed when configured. */
	dev->power = USB_MIN_POWER;
	dev->self_powered = 0;

	DPRINTF(("usbd_new_device: new dev (addr %d), dev=%p, parent=%p\n",
		 addr, dev, parent));

	usbd_add_dev_event(USB_EVENT_DEVICE_ATTACH, dev);

	if (port == 0) { /* root hub */
		KASSERT(addr == 1);
		usbd_attach_roothub(parent, dev);
		return (USBD_NORMAL_COMPLETION);
	}

	err = usbd_probe_and_attach(parent, dev, port, addr);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_reload_device_desc(usbd_device_handle dev)
{
	usbd_status err;

	/* Get the full device descriptor. */
	err = usbd_get_device_desc(dev, &dev->ddesc);
	if (err)
		return (err);

	/* Figure out what's wrong with this device. */
	dev->quirks = usbd_find_quirk(&dev->ddesc);

	return (USBD_NORMAL_COMPLETION);
}

void
usbd_remove_device(usbd_device_handle dev, struct usbd_port *up)
{

	DPRINTF(("usbd_remove_device: %p\n", dev));

	if (dev->default_pipe != NULL)
		usbd_kill_pipe(dev->default_pipe);
	up->device = NULL;
	dev->bus->devices[dev->address] = NULL;

	free(dev, M_USB);
}

int
usbd_print(void *aux, const char *pnp)
{
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(15, ("usbd_print dev=%p\n", uaa->device));
	if (pnp) {
#define USB_DEVINFO 1024
		char *devinfo;
		if (!uaa->usegeneric)
			return (QUIET);
		devinfo = malloc(USB_DEVINFO, M_TEMP, M_WAITOK);
		usbd_devinfo(uaa->device, 1, devinfo, USB_DEVINFO);
		aprint_normal("%s, %s", devinfo, pnp);
		free(devinfo, M_TEMP);
	}
	aprint_normal(" port %d", uaa->port);
#if 0
	/*
	 * It gets very crowded with these locators on the attach line.
	 * They are not really needed since they are printed in the clear
	 * by each driver.
	 */
	if (uaa->vendor != UHUB_UNK_VENDOR)
		aprint_normal(" vendor 0x%04x", uaa->vendor);
	if (uaa->product != UHUB_UNK_PRODUCT)
		aprint_normal(" product 0x%04x", uaa->product);
	if (uaa->release != UHUB_UNK_RELEASE)
		aprint_normal(" release 0x%04x", uaa->release);
#endif
	return (UNCONF);
}

int
usbd_ifprint(void *aux, const char *pnp)
{
	struct usbif_attach_arg *uaa = aux;

	DPRINTFN(15, ("usbd_print dev=%p\n", uaa->device));
	if (pnp)
		return (QUIET);
	aprint_normal(" port %d", uaa->port);
	aprint_normal(" configuration %d", uaa->configno);
	aprint_normal(" interface %d", uaa->ifaceno);
#if 0
	/*
	 * It gets very crowded with these locators on the attach line.
	 * They are not really needed since they are printed in the clear
	 * by each driver.
	 */
	if (uaa->vendor != UHUB_UNK_VENDOR)
		aprint_normal(" vendor 0x%04x", uaa->vendor);
	if (uaa->product != UHUB_UNK_PRODUCT)
		aprint_normal(" product 0x%04x", uaa->product);
	if (uaa->release != UHUB_UNK_RELEASE)
		aprint_normal(" release 0x%04x", uaa->release);
#endif
	return (UNCONF);
}

void
usbd_fill_deviceinfo(usbd_device_handle dev, struct usb_device_info *di,
		     int usedev)
{
	struct usbd_port *p;
	int i, j, err, s;

	di->udi_bus = device_unit(dev->bus->usbctl);
	di->udi_addr = dev->address;
	di->udi_cookie = dev->cookie;
	usbd_devinfo_vp(dev, di->udi_vendor, sizeof(di->udi_vendor),
	    di->udi_product, sizeof(di->udi_product), usedev, 1);
	usbd_printBCD(di->udi_release, sizeof(di->udi_release),
	    UGETW(dev->ddesc.bcdDevice));
	di->udi_serial[0] = 0;
	if (usedev)
		(void)usbd_get_string(dev, dev->ddesc.iSerialNumber,
				      di->udi_serial);
	di->udi_vendorNo = UGETW(dev->ddesc.idVendor);
	di->udi_productNo = UGETW(dev->ddesc.idProduct);
	di->udi_releaseNo = UGETW(dev->ddesc.bcdDevice);
	di->udi_class = dev->ddesc.bDeviceClass;
	di->udi_subclass = dev->ddesc.bDeviceSubClass;
	di->udi_protocol = dev->ddesc.bDeviceProtocol;
	di->udi_config = dev->config;
	di->udi_power = dev->self_powered ? 0 : dev->power;
	di->udi_speed = dev->speed;

	if (dev->subdevlen > 0) {
		for (i = 0, j = 0; i < dev->subdevlen &&
			     j < USB_MAX_DEVNAMES; i++) {
			if (!dev->subdevs[i])
				continue;
			strncpy(di->udi_devnames[j],
			    device_xname(dev->subdevs[i]), USB_MAX_DEVNAMELEN);
			di->udi_devnames[j][USB_MAX_DEVNAMELEN-1] = '\0';
			j++;
		}
	} else {
		j = 0;
	}
	for (/* j is set */; j < USB_MAX_DEVNAMES; j++)
		di->udi_devnames[j][0] = 0;                 /* empty */

	if (dev->hub) {
		for (i = 0;
		     i < sizeof(di->udi_ports) / sizeof(di->udi_ports[0]) &&
			     i < dev->hub->hubdesc.bNbrPorts;
		     i++) {
			p = &dev->hub->ports[i];
			if (p->device)
				err = p->device->address;
			else {
				s = UGETW(p->status.wPortStatus);
				if (s & UPS_PORT_ENABLED)
					err = USB_PORT_ENABLED;
				else if (s & UPS_SUSPEND)
					err = USB_PORT_SUSPENDED;
				else if (s & UPS_PORT_POWER)
					err = USB_PORT_POWERED;
				else
					err = USB_PORT_DISABLED;
			}
			di->udi_ports[i] = err;
		}
		di->udi_nports = dev->hub->hubdesc.bNbrPorts;
	} else
		di->udi_nports = 0;
}

#ifdef COMPAT_30
void
usbd_fill_deviceinfo_old(usbd_device_handle dev, struct usb_device_info_old *di,
                         int usedev)
{
	struct usbd_port *p;
	int i, j, err, s;

	di->udi_bus = device_unit(dev->bus->usbctl);
	di->udi_addr = dev->address;
	di->udi_cookie = dev->cookie;
	usbd_devinfo_vp(dev, di->udi_vendor, sizeof(di->udi_vendor),
	    di->udi_product, sizeof(di->udi_product), usedev, 0);
	usbd_printBCD(di->udi_release, sizeof(di->udi_release),
	    UGETW(dev->ddesc.bcdDevice));
	di->udi_vendorNo = UGETW(dev->ddesc.idVendor);
	di->udi_productNo = UGETW(dev->ddesc.idProduct);
	di->udi_releaseNo = UGETW(dev->ddesc.bcdDevice);
	di->udi_class = dev->ddesc.bDeviceClass;
	di->udi_subclass = dev->ddesc.bDeviceSubClass;
	di->udi_protocol = dev->ddesc.bDeviceProtocol;
	di->udi_config = dev->config;
	di->udi_power = dev->self_powered ? 0 : dev->power;
	di->udi_speed = dev->speed;

	if (dev->subdevlen > 0) {
		for (i = 0, j = 0; i < dev->subdevlen &&
			     j < USB_MAX_DEVNAMES; i++) {
			if (!dev->subdevs[i])
				continue;
			strncpy(di->udi_devnames[j],
			    device_xname(dev->subdevs[i]), USB_MAX_DEVNAMELEN);
			di->udi_devnames[j][USB_MAX_DEVNAMELEN-1] = '\0';
			j++;
		}
	} else {
		j = 0;
	}
	for (/* j is set */; j < USB_MAX_DEVNAMES; j++)
		di->udi_devnames[j][0] = 0;		 /* empty */

	if (dev->hub) {
		for (i = 0;
		     i < sizeof(di->udi_ports) / sizeof(di->udi_ports[0]) &&
			     i < dev->hub->hubdesc.bNbrPorts;
		     i++) {
			p = &dev->hub->ports[i];
			if (p->device)
				err = p->device->address;
			else {
				s = UGETW(p->status.wPortStatus);
				if (s & UPS_PORT_ENABLED)
					err = USB_PORT_ENABLED;
				else if (s & UPS_SUSPEND)
					err = USB_PORT_SUSPENDED;
				else if (s & UPS_PORT_POWER)
					err = USB_PORT_POWERED;
				else
					err = USB_PORT_DISABLED;
			}
			di->udi_ports[i] = err;
		}
		di->udi_nports = dev->hub->hubdesc.bNbrPorts;
	} else
		di->udi_nports = 0;
}
#endif


void
usb_free_device(usbd_device_handle dev)
{
	int ifcidx, nifc;

	if (dev->default_pipe != NULL)
		usbd_kill_pipe(dev->default_pipe);
	if (dev->ifaces != NULL) {
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB);
	}
	if (dev->cdesc != NULL)
		free(dev->cdesc, M_USB);
	if (dev->subdevlen > 0) {
		free(dev->subdevs, M_USB);
		dev->subdevlen = 0;
	}
	free(dev, M_USB);
}

/*
 * The general mechanism for detaching drivers works as follows: Each
 * driver is responsible for maintaining a reference count on the
 * number of outstanding references to its softc (e.g.  from
 * processing hanging in a read or write).  The detach method of the
 * driver decrements this counter and flags in the softc that the
 * driver is dying and then wakes any sleepers.  It then sleeps on the
 * softc.  Each place that can sleep must maintain the reference
 * count.  When the reference count drops to -1 (0 is the normal value
 * of the reference count) the a wakeup on the softc is performed
 * signaling to the detach waiter that all references are gone.
 */

/*
 * Called from process context when we discover that a port has
 * been disconnected.
 */
int
usb_disconnect_port(struct usbd_port *up, device_t parent, int flags)
{
	usbd_device_handle dev = up->device;
	device_t subdev;
	char subdevname[16];
	const char *hubname = device_xname(parent);
	int i, rc;

	DPRINTFN(3,("uhub_disconnect: up=%p dev=%p port=%d\n",
		    up, dev, up->portno));

	if (dev == NULL) {
#ifdef DIAGNOSTIC
		printf("usb_disconnect_port: no device\n");
#endif
		return 0;
	}

	if (dev->subdevlen > 0) {
		DPRINTFN(3,("usb_disconnect_port: disconnect subdevs\n"));
		for (i = 0; i < dev->subdevlen; i++) {
			if ((subdev = dev->subdevs[i]) == NULL)
				continue;
			strlcpy(subdevname, device_xname(subdev),
			    sizeof(subdevname));
			if ((rc = config_detach(subdev, flags)) != 0)
				return rc;
			printf("%s: at %s", subdevname, hubname);
			if (up->portno != 0)
				printf(" port %d", up->portno);
			printf(" (addr %d) disconnected\n", dev->address);
		}
		KASSERT(!dev->nifaces_claimed);
	}

	usbd_add_dev_event(USB_EVENT_DEVICE_DETACH, dev);
	dev->bus->devices[dev->address] = NULL;
	up->device = NULL;
	usb_free_device(dev);
	return 0;
}
