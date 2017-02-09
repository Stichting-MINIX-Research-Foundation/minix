/*	$NetBSD: uhidev.c,v 1.64 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 2001, 2012 The NetBSD Foundation, Inc.
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

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uhidev.c,v 1.64 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/rndsource.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uhidev.h>

/* Report descriptor for broken Wacom Graphire */
#include <dev/usb/ugraphire_rdesc.h>
/* Report descriptor for game controllers in "XInput" mode */
#include <dev/usb/xinput_rdesc.h>
/* Report descriptor for Xbox One controllers */
#include <dev/usb/x1input_rdesc.h>

#include "locators.h"

#ifdef UHIDEV_DEBUG
#define DPRINTF(x)	if (uhidevdebug) printf x
#define DPRINTFN(n,x)	if (uhidevdebug>(n)) printf x
int	uhidevdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static void uhidev_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static int uhidev_maxrepid(void *, int);
Static int uhidevprint(void *, const char *);

int uhidev_match(device_t, cfdata_t, void *);
void uhidev_attach(device_t, device_t, void *);
void uhidev_childdet(device_t, device_t);
int uhidev_detach(device_t, int);
int uhidev_activate(device_t, enum devact);
extern struct cfdriver uhidev_cd;
CFATTACH_DECL2_NEW(uhidev, sizeof(struct uhidev_softc), uhidev_match,
    uhidev_attach, uhidev_detach, uhidev_activate, NULL, uhidev_childdet);

int
uhidev_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	/* Game controllers in "XInput" mode */
	if (USBIF_IS_XINPUT(uaa))
		return UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO;
	/* Xbox One controllers */
 	if (USBIF_IS_X1INPUT(uaa) && uaa->ifaceno == 0)
		return UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO;

	if (uaa->class != UICLASS_HID)
		return (UMATCH_NONE);
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_HID_IGNORE)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_GENERIC);
}

void
uhidev_attach(device_t parent, device_t self, void *aux)
{
	struct uhidev_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct uhidev_attach_arg uha;
	device_t dev;
	struct uhidev *csc;
	int maxinpktsize, size, nrepid, repid, repsz;
	int *repsizes;
	int i;
	void *desc;
	const void *descptr;
	usbd_status err;
	char *devinfop;
	int locs[UHIDBUSCF_NLOCS];

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;

	aprint_naive("\n");
	aprint_normal("\n");

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_USB);

	id = usbd_get_interface_descriptor(iface);

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s, iclass %d/%d\n",
	       devinfop, id->bInterfaceClass, id->bInterfaceSubClass);
	usbd_devinfo_free(devinfop);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	(void)usbd_set_idle(iface, 0, 0);
#if 0

	qflags = usbd_get_quirks(sc->sc_udev)->uq_flags;
	if ((qflags & UQ_NO_SET_PROTO) == 0 &&
	    id->bInterfaceSubClass != UISUBCLASS_BOOT)
		(void)usbd_set_protocol(iface, 1);
#endif

	maxinpktsize = 0;
	sc->sc_iep_addr = sc->sc_oep_addr = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "could not read endpoint descriptor\n");
			sc->sc_dying = 1;
			return;
		}

		DPRINTFN(10,("uhidev_attach: bLength=%d bDescriptorType=%d "
		    "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		    " bInterval=%d\n",
		    ed->bLength, ed->bDescriptorType,
		    ed->bEndpointAddress & UE_ADDR,
		    UE_GET_DIR(ed->bEndpointAddress)==UE_DIR_IN? "in" : "out",
		    ed->bmAttributes & UE_XFERTYPE,
		    UGETW(ed->wMaxPacketSize), ed->bInterval));

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			maxinpktsize = UGETW(ed->wMaxPacketSize);
			sc->sc_iep_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->sc_oep_addr = ed->bEndpointAddress;
		} else {
			aprint_verbose_dev(self, "endpoint %d: ignored\n", i);
		}
	}

	/*
	 * Check that we found an input interrupt endpoint. The output interrupt
	 * endpoint is optional
	 */
	if (sc->sc_iep_addr == -1) {
		aprint_error_dev(self, "no input interrupt endpoint\n");
		sc->sc_dying = 1;
		return;
	}

	/* XXX need to extend this */
	descptr = NULL;
	if (uaa->vendor == USB_VENDOR_WACOM) {
		static uByte reportbuf[] = {2, 2, 2};

		/* The report descriptor for the Wacom Graphire is broken. */
		switch (uaa->product) {
		case USB_PRODUCT_WACOM_GRAPHIRE:
		case USB_PRODUCT_WACOM_GRAPHIRE2:
		case USB_PRODUCT_WACOM_GRAPHIRE3_4X5:
		case USB_PRODUCT_WACOM_GRAPHIRE3_6X8:
		case USB_PRODUCT_WACOM_GRAPHIRE4_4X5: /* The 6x8 too? */
			/*
			 * The Graphire3 needs 0x0202 to be written to
			 * feature report ID 2 before it'll start
			 * returning digitizer data.
			 */
			usbd_set_report(uaa->iface, UHID_FEATURE_REPORT, 2,
			    &reportbuf, sizeof reportbuf);

			size = sizeof uhid_graphire3_4x5_report_descr;
			descptr = uhid_graphire3_4x5_report_descr;
			break;
		default:
			/* Keep descriptor */
			break;
		}
	}
	if (USBIF_IS_XINPUT(uaa)) {
		size = sizeof uhid_xinput_report_descr;
		descptr = uhid_xinput_report_descr;
	}
	if (USBIF_IS_X1INPUT(uaa)) {
		sc->sc_flags |= UHIDEV_F_XB1;
		size = sizeof uhid_x1input_report_descr;
		descptr = uhid_x1input_report_descr;
	}

	if (descptr) {
		desc = malloc(size, M_USBDEV, M_NOWAIT);
		if (desc == NULL)
			err = USBD_NOMEM;
		else {
			err = USBD_NORMAL_COMPLETION;
			memcpy(desc, descptr, size);
		}
	} else {
		desc = NULL;
		err = usbd_read_report_desc(uaa->iface, &desc, &size,
		    M_USBDEV);
	}
	if (err) {
		aprint_error_dev(self, "no report descriptor\n");
		sc->sc_dying = 1;
		return;
	}

	if (uaa->vendor == USB_VENDOR_HOSIDEN &&
	    uaa->product == USB_PRODUCT_HOSIDEN_PPP) {
		static uByte reportbuf[] = { 1 };
		/*
		 *  This device was sold by Konami with its ParaParaParadise
		 *  game for PlayStation2.  It needs to be "turned on"
		 *  before it will send any reports.
		 */

		usbd_set_report(uaa->iface, UHID_FEATURE_REPORT, 0,
		    &reportbuf, sizeof reportbuf);
	}

	if (uaa->vendor == USB_VENDOR_LOGITECH &&
	    uaa->product == USB_PRODUCT_LOGITECH_CBT44 && size == 0xb1) {
		uint8_t *data = desc;
		/*
		 * This device has a odd USAGE_MINIMUM value that would
		 * cause the multimedia keys to have their usage number
		 * shifted up one usage.  Adjust so the usages are sane.
		 */

		if (data[0x56] == 0x19 && data[0x57] == 0x01 &&
		    data[0x58] == 0x2a && data[0x59] == 0x8c)
			data[0x57] = 0x00;
	}

	/*
	 * Enable the Six Axis and DualShock 3 controllers.
	 * See http://ps3.jim.sh/sixaxis/usb/
	 */
	if (uaa->vendor == USB_VENDOR_SONY &&
	    uaa->product == USB_PRODUCT_SONY_PS3CONTROLLER) {
		usb_device_request_t req;
		char data[17];
		int actlen;

		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		req.bRequest = 1;
		USETW(req.wValue, 0x3f2);
		USETW(req.wIndex, 0);
		USETW(req.wLength, sizeof data);

		usbd_do_request_flags(sc->sc_udev, &req, data,
			USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT);
	}

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

	uha.uaa = uaa;
	nrepid = uhidev_maxrepid(desc, size);
	if (nrepid < 0)
		return;
	if (nrepid > 0)
		aprint_normal_dev(self, "%d report ids\n", nrepid);
	nrepid++;
	repsizes = malloc(nrepid * sizeof(*repsizes), M_TEMP, M_NOWAIT);
	if (repsizes == NULL)
		goto nomem;
	sc->sc_subdevs = malloc(nrepid * sizeof(device_t),
				M_USBDEV, M_NOWAIT | M_ZERO);
	if (sc->sc_subdevs == NULL) {
		free(repsizes, M_TEMP);
nomem:
		aprint_error_dev(self, "no memory\n");
		return;
	}

	/* Just request max packet size for the interrupt pipe */
	sc->sc_isize = maxinpktsize;
	sc->sc_nrepid = nrepid;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	for (repid = 0; repid < nrepid; repid++) {
		repsz = hid_report_size(desc, size, hid_input, repid);
		DPRINTF(("uhidev_match: repid=%d, repsz=%d\n", repid, repsz));
		repsizes[repid] = repsz;
	}

	DPRINTF(("uhidev_attach: isize=%d\n", sc->sc_isize));

	uha.parent = sc;
	for (repid = 0; repid < nrepid; repid++) {
		DPRINTF(("uhidev_match: try repid=%d\n", repid));
		if (hid_report_size(desc, size, hid_input, repid) == 0 &&
		    hid_report_size(desc, size, hid_output, repid) == 0 &&
		    hid_report_size(desc, size, hid_feature, repid) == 0) {
			;	/* already NULL in sc->sc_subdevs[repid] */
		} else {
			uha.reportid = repid;
			locs[UHIDBUSCF_REPORTID] = repid;

			dev = config_found_sm_loc(self,
				"uhidbus", locs, &uha,
				uhidevprint, config_stdsubmatch);
			sc->sc_subdevs[repid] = dev;
			if (dev != NULL) {
				csc = device_private(dev);
				csc->sc_in_rep_size = repsizes[repid];
#ifdef DIAGNOSTIC
				DPRINTF(("uhidev_match: repid=%d dev=%p\n",
					 repid, dev));
				if (csc->sc_intr == NULL) {
					free(repsizes, M_TEMP);
					aprint_error_dev(self,
					    "sc_intr == NULL\n");
					return;
				}
#endif
				rnd_attach_source(&csc->rnd_source,
						  device_xname(dev),
						  RND_TYPE_TTY,
						  RND_FLAG_DEFAULT);
			}
		}
	}
	free(repsizes, M_TEMP);

	return;
}

int
uhidev_maxrepid(void *buf, int len)
{
	struct hid_data *d;
	struct hid_item h;
	int maxid;

	maxid = -1;
	h.report_ID = 0;
	for (d = hid_start_parse(buf, len, hid_none); hid_get_item(d, &h); )
		if (h.report_ID > maxid)
			maxid = h.report_ID;
	hid_end_parse(d);
	return (maxid);
}

int
uhidevprint(void *aux, const char *pnp)
{
	struct uhidev_attach_arg *uha = aux;

	if (pnp)
		aprint_normal("uhid at %s", pnp);
	if (uha->reportid != 0)
		aprint_normal(" reportid %d", uha->reportid);
	return (UNCONF);
}

int
uhidev_activate(device_t self, enum devact act)
{
	struct uhidev_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
uhidev_childdet(device_t self, device_t child)
{
	int i;
	struct uhidev_softc *sc = device_private(self);

	for (i = 0; i < sc->sc_nrepid; i++) {
		if (sc->sc_subdevs[i] == child)
			break;
	}
	KASSERT(i < sc->sc_nrepid);
	sc->sc_subdevs[i] = NULL;
}

int
uhidev_detach(device_t self, int flags)
{
	struct uhidev_softc *sc = device_private(self);
	int i, rv;
	struct uhidev *csc;

	DPRINTF(("uhidev_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	if (sc->sc_ipipe != NULL)
		usbd_abort_pipe(sc->sc_ipipe);

	if (sc->sc_repdesc != NULL)
		free(sc->sc_repdesc, M_USBDEV);

	rv = 0;
	for (i = 0; i < sc->sc_nrepid; i++) {
		if (sc->sc_subdevs[i] != NULL) {
			csc = device_private(sc->sc_subdevs[i]);
			rnd_detach_source(&csc->rnd_source);
			rv |= config_detach(sc->sc_subdevs[i], flags);
		}
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	pmf_device_deregister(self);
	mutex_destroy(&sc->sc_lock);

	return (rv);
}

void
uhidev_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uhidev_softc *sc = addr;
	device_t cdev;
	struct uhidev *scd;
	u_char *p;
	u_int rep;
	u_int32_t cc;

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);

#ifdef UHIDEV_DEBUG
	if (uhidevdebug > 5) {
		u_int32_t i;

		DPRINTF(("uhidev_intr: status=%d cc=%d\n", status, cc));
		DPRINTF(("uhidev_intr: data ="));
		for (i = 0; i < cc; i++)
			DPRINTF((" %02x", sc->sc_ibuf[i]));
		DPRINTF(("\n"));
	}
#endif

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: interrupt status=%d\n", device_xname(sc->sc_dev),
			 status));
		usbd_clear_endpoint_stall_async(sc->sc_ipipe);
		return;
	}

	p = sc->sc_ibuf;
	if (sc->sc_nrepid != 1)
		rep = *p++, cc--;
	else
		rep = 0;
	if (rep >= sc->sc_nrepid) {
		printf("uhidev_intr: bad repid %d\n", rep);
		return;
	}
	cdev = sc->sc_subdevs[rep];
	if (!cdev)
		return;
	scd = device_private(cdev);
	DPRINTFN(5,("uhidev_intr: rep=%d, scd=%p state=0x%x\n",
		    rep, scd, scd ? scd->sc_state : 0));
	if (!(scd->sc_state & UHIDEV_OPEN))
		return;
#ifdef UHIDEV_DEBUG
	if (scd->sc_in_rep_size != cc) {
		DPRINTF(("%s: expected %d bytes, got %d\n",
		       device_xname(sc->sc_dev), scd->sc_in_rep_size, cc));
	}
#endif
	if (cc == 0) {
		DPRINTF(("%s: 0-length input ignored\n",
			device_xname(sc->sc_dev)));
		return;
	}
	rnd_add_uint32(&scd->rnd_source, (uintptr_t)(sc->sc_ibuf));
	scd->sc_intr(scd, p, cc);
}

void
uhidev_get_report_desc(struct uhidev_softc *sc, void **desc, int *size)
{
	*desc = sc->sc_repdesc;
	*size = sc->sc_repdesc_size;
}

int
uhidev_open(struct uhidev *scd)
{
	struct uhidev_softc *sc = scd->sc_parent;
	usbd_status err;
	int error;

	DPRINTF(("uhidev_open: open pipe, state=%d\n", scd->sc_state));

	mutex_enter(&sc->sc_lock);
	if (scd->sc_state & UHIDEV_OPEN) {
		mutex_exit(&sc->sc_lock);
		return (EBUSY);
	}
	scd->sc_state |= UHIDEV_OPEN;
	if (sc->sc_refcnt++) {
		mutex_exit(&sc->sc_lock);
		return (0);
	}
	mutex_exit(&sc->sc_lock);

	if (sc->sc_isize == 0)
		return (0);

	sc->sc_ibuf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);

	/* Set up input interrupt pipe. */
	DPRINTF(("uhidev_open: isize=%d, ep=0x%02x\n", sc->sc_isize,
		 sc->sc_iep_addr));

	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_iep_addr,
		  USBD_SHORT_XFER_OK, &sc->sc_ipipe, sc, sc->sc_ibuf,
		  sc->sc_isize, uhidev_intr, USBD_DEFAULT_INTERVAL);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uhidopen: usbd_open_pipe_intr failed, "
		    "error=%d\n", err));
		error = EIO;
		goto out1;
	}

	/*
	 * Set up output interrupt pipe if an output interrupt endpoint
	 * exists.
	 */
	if (sc->sc_oep_addr != -1) {
		DPRINTF(("uhidev_open: oep=0x%02x\n", sc->sc_oep_addr));

		err = usbd_open_pipe(sc->sc_iface, sc->sc_oep_addr,
		    0, &sc->sc_opipe);

		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uhidev_open: usbd_open_pipe failed, "
			    "error=%d\n", err));
			error = EIO;
			goto out2;
		}
		DPRINTF(("uhidev_open: sc->sc_opipe=%p\n", sc->sc_opipe));

		sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer == NULL) {
			DPRINTF(("uhidev_open: couldn't allocate an xfer\n"));
			error = ENOMEM;
			goto out3;
		}

		if (sc->sc_flags & UHIDEV_F_XB1) {
			uint8_t init_data[] = { 0x05, 0x20 };
			int init_data_len = sizeof(init_data);
			err = usbd_intr_transfer(sc->sc_oxfer, sc->sc_opipe, 0,
			    USBD_NO_TIMEOUT, init_data, &init_data_len,
			    "uhidevxb1");
			if (err != USBD_NORMAL_COMPLETION) {
				DPRINTF(("uhidev_open: xb1 init failed, "
				    "error=%d\n", err));
				error = EIO;
				goto out4;
			}
		}
	}

	return (0);
out4:
	/* Free output xfer */
	if (sc->sc_oxfer != NULL)
		usbd_free_xfer(sc->sc_oxfer);
out3:
	/* Abort output pipe */
	usbd_close_pipe(sc->sc_opipe);
out2:
	/* Abort input pipe */
	usbd_close_pipe(sc->sc_ipipe);
out1:
	DPRINTF(("uhidev_open: failed in someway"));
	free(sc->sc_ibuf, M_USBDEV);
	mutex_enter(&sc->sc_lock);
	scd->sc_state &= ~UHIDEV_OPEN;
	sc->sc_refcnt = 0;
	sc->sc_ibuf = NULL;
	sc->sc_ipipe = NULL;
	sc->sc_opipe = NULL;
	sc->sc_oxfer = NULL;
	mutex_exit(&sc->sc_lock);
	return error;
}

void
uhidev_stop(struct uhidev *scd)
{
	struct uhidev_softc *sc = scd->sc_parent;

	/* Disable interrupts. */
	if (sc->sc_opipe != NULL) {
		usbd_abort_pipe(sc->sc_opipe);
		usbd_close_pipe(sc->sc_opipe);
		sc->sc_opipe = NULL;
	}

	if (sc->sc_ipipe != NULL) {
		usbd_abort_pipe(sc->sc_ipipe);
		usbd_close_pipe(sc->sc_ipipe);
		sc->sc_ipipe = NULL;
	}

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_ibuf = NULL;
	}
}

void
uhidev_close(struct uhidev *scd)
{
	struct uhidev_softc *sc = scd->sc_parent;

	mutex_enter(&sc->sc_lock);
	if (!(scd->sc_state & UHIDEV_OPEN)) {
		mutex_exit(&sc->sc_lock);
		return;
	}
	scd->sc_state &= ~UHIDEV_OPEN;
	if (--sc->sc_refcnt) {
		mutex_exit(&sc->sc_lock);
		return;
	}
	mutex_exit(&sc->sc_lock);

	DPRINTF(("uhidev_close: close pipe\n"));

	if (sc->sc_oxfer != NULL) {
		usbd_free_xfer(sc->sc_oxfer);
		sc->sc_oxfer = NULL;
	}

	/* Possibly redundant, but properly handled */
	uhidev_stop(scd);
}

usbd_status
uhidev_set_report(struct uhidev *scd, int type, void *data, int len)
{
	char *buf;
	usbd_status retstat;

	if (scd->sc_report_id == 0)
		return usbd_set_report(scd->sc_parent->sc_iface, type,
				       scd->sc_report_id, data, len);

	buf = malloc(len + 1, M_TEMP, M_WAITOK);
	buf[0] = scd->sc_report_id;
	memcpy(buf+1, data, len);

	retstat = usbd_set_report(scd->sc_parent->sc_iface, type,
				  scd->sc_report_id, buf, len + 1);

	free(buf, M_TEMP);

	return retstat;
}

usbd_status
uhidev_get_report(struct uhidev *scd, int type, void *data, int len)
{
	return usbd_get_report(scd->sc_parent->sc_iface, type,
			       scd->sc_report_id, data, len);
}

usbd_status
uhidev_write(struct uhidev_softc *sc, void *data, int len)
{

	DPRINTF(("uhidev_write: data=%p, len=%d\n", data, len));

	if (sc->sc_opipe == NULL)
		return USBD_INVAL;

#ifdef UHIDEV_DEBUG
	if (uhidevdebug > 50) {

		u_int32_t i;
		u_int8_t *d = data;

		DPRINTF(("uhidev_write: data ="));
		for (i = 0; i < len; i++)
			DPRINTF((" %02x", d[i]));
		DPRINTF(("\n"));
	}
#endif
	return usbd_intr_transfer(sc->sc_oxfer, sc->sc_opipe, 0,
	    USBD_NO_TIMEOUT, data, &len, "uhidevwi");
}
