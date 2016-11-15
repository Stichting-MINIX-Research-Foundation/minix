/*	$NetBSD: ubt.c,v 1.51 2014/05/20 18:25:54 rmind Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and
 * David Sainty (David.Sainty@dtsp.co.nz).
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
 * This driver originally written by Lennart Augustsson and David Sainty,
 * but was mostly rewritten for the NetBSD Bluetooth protocol stack by
 * Iain Hibbert for Itronix, Inc using the FreeBSD ng_ubt.c driver as a
 * reference.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ubt.c,v 1.51 2014/05/20 18:25:54 rmind Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

/*******************************************************************************
 *
 *	debugging stuff
 */
#undef DPRINTF
#undef DPRINTFN

#ifdef UBT_DEBUG
int	ubt_debug = 0;

#define DPRINTF(...)		do {		\
	if (ubt_debug) {			\
		printf("%s: ", __func__);	\
		printf(__VA_ARGS__);		\
	}					\
} while (/* CONSTCOND */0)

#define DPRINTFN(n, ...)	do {		\
	if (ubt_debug > (n)) {			\
		printf("%s: ", __func__);	\
		printf(__VA_ARGS__);		\
	}					\
} while (/* CONSTCOND */0)

SYSCTL_SETUP(sysctl_hw_ubt_debug_setup, "sysctl hw.ubt_debug setup")
{

	sysctl_createv(NULL, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "ubt_debug",
		SYSCTL_DESCR("ubt debug level"),
		NULL, 0,
		&ubt_debug, sizeof(ubt_debug),
		CTL_HW, CTL_CREATE, CTL_EOL);
}
#else
#define DPRINTF(...)
#define DPRINTFN(...)
#endif

/*******************************************************************************
 *
 *	ubt softc structure
 *
 */

/* buffer sizes */
/*
 * NB: although ACL packets can extend to 65535 bytes, most devices
 * have max_acl_size at much less (largest I have seen is 384)
 */
#define UBT_BUFSIZ_CMD		(HCI_CMD_PKT_SIZE - 1)
#define UBT_BUFSIZ_ACL		(2048 - 1)
#define UBT_BUFSIZ_EVENT	(HCI_EVENT_PKT_SIZE - 1)

/* Transmit timeouts */
#define UBT_CMD_TIMEOUT		USBD_DEFAULT_TIMEOUT
#define UBT_ACL_TIMEOUT		USBD_DEFAULT_TIMEOUT

/*
 * ISOC transfers
 *
 * xfer buffer size depends on the frame size, and the number
 * of frames per transfer is fixed, as each frame should be
 * 1ms worth of data. This keeps the rate that xfers complete
 * fairly constant. We use multiple xfers to keep the hardware
 * busy
 */
#define UBT_NXFERS		3	/* max xfers to queue */
#define UBT_NFRAMES		10	/* frames per xfer */

struct ubt_isoc_xfer {
	struct ubt_softc	*softc;
	usbd_xfer_handle	 xfer;
	uint8_t			*buf;
	uint16_t		 size[UBT_NFRAMES];
	int			 busy;
};

struct ubt_softc {
	device_t		 sc_dev;
	usbd_device_handle	 sc_udev;
	int			 sc_refcnt;
	int			 sc_dying;
	int			 sc_enabled;

	/* Control Interface */
	usbd_interface_handle	 sc_iface0;

	/* Commands (control) */
	usbd_xfer_handle	 sc_cmd_xfer;
	uint8_t			*sc_cmd_buf;
	int			 sc_cmd_busy;	/* write active */
	MBUFQ_HEAD()		 sc_cmd_queue;	/* output queue */

	/* Events (interrupt) */
	int			 sc_evt_addr;	/* endpoint address */
	usbd_pipe_handle	 sc_evt_pipe;
	uint8_t			*sc_evt_buf;

	/* ACL data (in) */
	int			 sc_aclrd_addr;	/* endpoint address */
	usbd_pipe_handle	 sc_aclrd_pipe;	/* read pipe */
	usbd_xfer_handle	 sc_aclrd_xfer;	/* read xfer */
	uint8_t			*sc_aclrd_buf;	/* read buffer */
	int			 sc_aclrd_busy;	/* reading */

	/* ACL data (out) */
	int			 sc_aclwr_addr;	/* endpoint address */
	usbd_pipe_handle	 sc_aclwr_pipe;	/* write pipe */
	usbd_xfer_handle	 sc_aclwr_xfer;	/* write xfer */
	uint8_t			*sc_aclwr_buf;	/* write buffer */
	int			 sc_aclwr_busy;	/* write active */
	MBUFQ_HEAD()		 sc_aclwr_queue;/* output queue */

	/* ISOC interface */
	usbd_interface_handle	 sc_iface1;	/* ISOC interface */
	struct sysctllog	*sc_log;	/* sysctl log */
	int			 sc_config;	/* current config no */
	int			 sc_alt_config;	/* no of alternates */

	/* SCO data (in) */
	int			 sc_scord_addr;	/* endpoint address */
	usbd_pipe_handle	 sc_scord_pipe;	/* read pipe */
	int			 sc_scord_size;	/* frame length */
	struct ubt_isoc_xfer	 sc_scord[UBT_NXFERS];
	struct mbuf		*sc_scord_mbuf;	/* current packet */

	/* SCO data (out) */
	int			 sc_scowr_addr;	/* endpoint address */
	usbd_pipe_handle	 sc_scowr_pipe;	/* write pipe */
	int			 sc_scowr_size;	/* frame length */
	struct ubt_isoc_xfer	 sc_scowr[UBT_NXFERS];
	struct mbuf		*sc_scowr_mbuf;	/* current packet */
	int			 sc_scowr_busy;	/* write active */
	MBUFQ_HEAD()		 sc_scowr_queue;/* output queue */

	/* Protocol structure */
	struct hci_unit		*sc_unit;
	struct bt_stats		 sc_stats;

	/* Successfully attached */
	int			 sc_ok;
};

/*******************************************************************************
 *
 * Bluetooth unit/USB callback routines
 *
 */
static int ubt_enable(device_t);
static void ubt_disable(device_t);

static void ubt_xmit_cmd(device_t, struct mbuf *);
static void ubt_xmit_cmd_start(struct ubt_softc *);
static void ubt_xmit_cmd_complete(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_xmit_acl(device_t, struct mbuf *);
static void ubt_xmit_acl_start(struct ubt_softc *);
static void ubt_xmit_acl_complete(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_xmit_sco(device_t, struct mbuf *);
static void ubt_xmit_sco_start(struct ubt_softc *);
static void ubt_xmit_sco_start1(struct ubt_softc *, struct ubt_isoc_xfer *);
static void ubt_xmit_sco_complete(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_recv_event(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_recv_acl_start(struct ubt_softc *);
static void ubt_recv_acl_complete(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_recv_sco_start1(struct ubt_softc *, struct ubt_isoc_xfer *);
static void ubt_recv_sco_complete(usbd_xfer_handle,
				usbd_private_handle, usbd_status);

static void ubt_stats(device_t, struct bt_stats *, int);

static const struct hci_if ubt_hci = {
	.enable = ubt_enable,
	.disable = ubt_disable,
	.output_cmd = ubt_xmit_cmd,
	.output_acl = ubt_xmit_acl,
	.output_sco = ubt_xmit_sco,
	.get_stats = ubt_stats,
	.ipl = IPL_USB,		/* IPL_SOFTUSB ??? */
};

/*******************************************************************************
 *
 * USB Autoconfig stuff
 *
 */

int             ubt_match(device_t, cfdata_t, void *);
void            ubt_attach(device_t, device_t, void *);
int             ubt_detach(device_t, int);
int             ubt_activate(device_t, enum devact);
extern struct cfdriver ubt_cd;
CFATTACH_DECL_NEW(ubt, sizeof(struct ubt_softc), ubt_match, ubt_attach, ubt_detach, ubt_activate);

static int ubt_set_isoc_config(struct ubt_softc *);
static int ubt_sysctl_config(SYSCTLFN_PROTO);
static void ubt_abortdealloc(struct ubt_softc *);

/*
 * To match or ignore forcibly, add
 *
 *	{ { VendorID, ProductID } , UMATCH_VENDOR_PRODUCT|UMATCH_NONE }
 *
 * to the ubt_dev list.
 */
const struct ubt_devno {
	struct usb_devno	devno;
	int			match;
} ubt_dev[] = {
	{ { USB_VENDOR_BROADCOM, USB_PRODUCT_BROADCOM_BCM2033NF },
	  UMATCH_NONE },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_BLUETOOTH_HOST_C },
	  UMATCH_VENDOR_PRODUCT },
};
#define ubt_lookup(vendor, product) \
	((const struct ubt_devno *)usb_lookup(ubt_dev, vendor, product))

int 
ubt_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	const struct ubt_devno *dev;

	DPRINTFN(50, "ubt_match\n");

	if ((dev = ubt_lookup(uaa->vendor, uaa->product)) != NULL)
		return dev->match;

	if (uaa->class == UDCLASS_WIRELESS
	    && uaa->subclass == UDSUBCLASS_RF
	    && uaa->proto == UDPROTO_BLUETOOTH)
		return UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO;

	return UMATCH_NONE;
}

void 
ubt_attach(device_t parent, device_t self, void *aux)
{
	struct ubt_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usb_config_descriptor_t *cd;
	usb_endpoint_descriptor_t *ed;
	const struct sysctlnode *node;
	char *devinfop;
	int err;
	uint8_t count, i;

	DPRINTFN(50, "ubt_attach: sc=%p\n", sc);

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	MBUFQ_INIT(&sc->sc_cmd_queue);
	MBUFQ_INIT(&sc->sc_aclwr_queue);
	MBUFQ_INIT(&sc->sc_scowr_queue);

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/*
	 * Move the device into the configured state
	 */
	err = usbd_set_config_index(sc->sc_udev, 0, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration idx 0: %s\n",
		    usbd_errstr(err));

		return;
	}

	/*
	 * Interface 0 must have 3 endpoints
	 *	1) Interrupt endpoint to receive HCI events
	 *	2) Bulk IN endpoint to receive ACL data
	 *	3) Bulk OUT endpoint to send ACL data
	 */
	err = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface0);
	if (err) {
		aprint_error_dev(self, "Could not get interface 0 handle %s (%d)\n",
				usbd_errstr(err), err);

		return;
	}

	sc->sc_evt_addr = -1;
	sc->sc_aclrd_addr = -1;
	sc->sc_aclwr_addr = -1;

	count = 0;
	(void)usbd_endpoint_count(sc->sc_iface0, &count);

	for (i = 0 ; i < count ; i++) {
		int dir, type;

		ed = usbd_interface2endpoint_descriptor(sc->sc_iface0, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "could not read endpoint descriptor %d\n", i);

			return;
		}

		dir = UE_GET_DIR(ed->bEndpointAddress);
		type = UE_GET_XFERTYPE(ed->bmAttributes);

		if (dir == UE_DIR_IN && type == UE_INTERRUPT)
			sc->sc_evt_addr = ed->bEndpointAddress;
		else if (dir == UE_DIR_IN && type == UE_BULK)
			sc->sc_aclrd_addr = ed->bEndpointAddress;
		else if (dir == UE_DIR_OUT && type == UE_BULK)
			sc->sc_aclwr_addr = ed->bEndpointAddress;
	}

	if (sc->sc_evt_addr == -1) {
		aprint_error_dev(self,
		    "missing INTERRUPT endpoint on interface 0\n");

		return;
	}
	if (sc->sc_aclrd_addr == -1) {
		aprint_error_dev(self,
		    "missing BULK IN endpoint on interface 0\n");

		return;
	}
	if (sc->sc_aclwr_addr == -1) {
		aprint_error_dev(self,
		    "missing BULK OUT endpoint on interface 0\n");

		return;
	}

	/*
	 * Interface 1 must have 2 endpoints
	 *	1) Isochronous IN endpoint to receive SCO data
	 *	2) Isochronous OUT endpoint to send SCO data
	 *
	 * and will have several configurations, which can be selected
	 * via a sysctl variable. We select config 0 to start, which
	 * means that no SCO data will be available.
	 */
	err = usbd_device2interface_handle(sc->sc_udev, 1, &sc->sc_iface1);
	if (err) {
		aprint_error_dev(self,
		    "Could not get interface 1 handle %s (%d)\n",
		    usbd_errstr(err), err);

		return;
	}

	cd = usbd_get_config_descriptor(sc->sc_udev);
	if (cd == NULL) {
		aprint_error_dev(self, "could not get config descriptor\n");

		return;
	}

	sc->sc_alt_config = usbd_get_no_alts(cd, 1);

	/* set initial config */
	err = ubt_set_isoc_config(sc);
	if (err) {
		aprint_error_dev(self, "ISOC config failed\n");

		return;
	}

	/* Attach HCI */
	sc->sc_unit = hci_attach_pcb(&ubt_hci, sc->sc_dev, 0);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	/* sysctl set-up for alternate configs */
	sysctl_createv(&sc->sc_log, 0, NULL, &node,
		0,
		CTLTYPE_NODE, device_xname(sc->sc_dev),
		SYSCTL_DESCR("ubt driver information"),
		NULL, 0,
		NULL, 0,
		CTL_HW,
		CTL_CREATE, CTL_EOL);

	if (node != NULL) {
		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "config",
			SYSCTL_DESCR("configuration number"),
			ubt_sysctl_config, 0,
			(void *)sc, 0,
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READONLY,
			CTLTYPE_INT, "alt_config",
			SYSCTL_DESCR("number of alternate configurations"),
			NULL, 0,
			&sc->sc_alt_config, sizeof(sc->sc_alt_config),
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READONLY,
			CTLTYPE_INT, "sco_rxsize",
			SYSCTL_DESCR("max SCO receive size"),
			NULL, 0,
			&sc->sc_scord_size, sizeof(sc->sc_scord_size),
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READONLY,
			CTLTYPE_INT, "sco_txsize",
			SYSCTL_DESCR("max SCO transmit size"),
			NULL, 0,
			&sc->sc_scowr_size, sizeof(sc->sc_scowr_size),
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);
	}

	sc->sc_ok = 1;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

int 
ubt_detach(device_t self, int flags)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	DPRINTF("sc=%p flags=%d\n", sc, flags);

	pmf_device_deregister(self);

	sc->sc_dying = 1;

	if (!sc->sc_ok)
		return 0;

	/* delete sysctl nodes */
	sysctl_teardown(&sc->sc_log);

	/* Detach HCI interface */
	if (sc->sc_unit) {
		hci_detach_pcb(sc->sc_unit);
		sc->sc_unit = NULL;
	}

	/*
	 * Abort all pipes. Causes processes waiting for transfer to wake.
	 *
	 * Actually, hci_detach_pcb() above will call ubt_disable() which
	 * may call ubt_abortdealloc(), but lets be sure since doing it
	 * twice wont cause an error.
	 */
	ubt_abortdealloc(sc);

	/* wait for all processes to finish */
	s = splusb();
	if (sc->sc_refcnt-- > 0)
		usb_detach_waitold(sc->sc_dev);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	DPRINTFN(1, "driver detached\n");

	return 0;
}

int
ubt_activate(device_t self, enum devact act)
{
	struct ubt_softc *sc = device_private(self);

	DPRINTFN(1, "sc=%p, act=%d\n", sc, act);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/* set ISOC configuration */
static int
ubt_set_isoc_config(struct ubt_softc *sc)
{
	usb_endpoint_descriptor_t *ed;
	int rd_addr, wr_addr, rd_size, wr_size;
	uint8_t count, i;
	int err;

	err = usbd_set_interface(sc->sc_iface1, sc->sc_config);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, 
		    "Could not set config %d on ISOC interface. %s (%d)\n",
		    sc->sc_config, usbd_errstr(err), err);

		return err == USBD_IN_USE ? EBUSY : EIO;
	}

	/*
	 * We wont get past the above if there are any pipes open, so no
	 * need to worry about buf/xfer/pipe deallocation. If we get an
	 * error after this, the frame quantities will be 0 and no SCO
	 * data will be possible.
	 */

	sc->sc_scord_size = rd_size = 0;
	sc->sc_scord_addr = rd_addr = -1;

	sc->sc_scowr_size = wr_size = 0;
	sc->sc_scowr_addr = wr_addr = -1;

	count = 0;
	(void)usbd_endpoint_count(sc->sc_iface1, &count);

	for (i = 0 ; i < count ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface1, i);
		if (ed == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not read endpoint descriptor %d\n", i);

			return EIO;
		}

		DPRINTFN(5, "%s: endpoint type %02x (%02x) addr %02x (%s)\n",
			device_xname(sc->sc_dev),
			UE_GET_XFERTYPE(ed->bmAttributes),
			UE_GET_ISO_TYPE(ed->bmAttributes),
			ed->bEndpointAddress,
			UE_GET_DIR(ed->bEndpointAddress) ? "in" : "out");

		if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) {
			rd_addr = ed->bEndpointAddress;
			rd_size = UGETW(ed->wMaxPacketSize);
		} else {
			wr_addr = ed->bEndpointAddress;
			wr_size = UGETW(ed->wMaxPacketSize);
		}
	}

	if (rd_addr == -1) {
		aprint_error_dev(sc->sc_dev,
		    "missing ISOC IN endpoint on interface config %d\n",
		    sc->sc_config);

		return ENOENT;
	}
	if (wr_addr == -1) {
		aprint_error_dev(sc->sc_dev,
		    "missing ISOC OUT endpoint on interface config %d\n",
		    sc->sc_config);

		return ENOENT;
	}

	if (rd_size > MLEN) {
		aprint_error_dev(sc->sc_dev, "rd_size=%d exceeds MLEN\n",
		    rd_size);

		return EOVERFLOW;
	}

	if (wr_size > MLEN) {
		aprint_error_dev(sc->sc_dev, "wr_size=%d exceeds MLEN\n",
		    wr_size);

		return EOVERFLOW;
	}

	sc->sc_scord_size = rd_size;
	sc->sc_scord_addr = rd_addr;

	sc->sc_scowr_size = wr_size;
	sc->sc_scowr_addr = wr_addr;

	return 0;
}

/* sysctl helper to set alternate configurations */
static int
ubt_sysctl_config(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct ubt_softc *sc;
	int t, error;

	node = *rnode;
	sc = node.sysctl_data;

	t = sc->sc_config;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 0 || t >= sc->sc_alt_config)
		return EINVAL;

	/* This may not change when the unit is enabled */
	if (sc->sc_enabled)
		return EBUSY;

	KERNEL_LOCK(1, curlwp);
	sc->sc_config = t;
	error = ubt_set_isoc_config(sc);
	KERNEL_UNLOCK_ONE(curlwp);
	return error;
}

static void
ubt_abortdealloc(struct ubt_softc *sc)
{
	int i;

	DPRINTFN(1, "sc=%p\n", sc);

	/* Abort all pipes */
	usbd_abort_default_pipe(sc->sc_udev);

	if (sc->sc_evt_pipe != NULL) {
		usbd_abort_pipe(sc->sc_evt_pipe);
		usbd_close_pipe(sc->sc_evt_pipe);
		sc->sc_evt_pipe = NULL;
	}

	if (sc->sc_aclrd_pipe != NULL) {
		usbd_abort_pipe(sc->sc_aclrd_pipe);
		usbd_close_pipe(sc->sc_aclrd_pipe);
		sc->sc_aclrd_pipe = NULL;
	}

	if (sc->sc_aclwr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_aclwr_pipe);
		usbd_close_pipe(sc->sc_aclwr_pipe);
		sc->sc_aclwr_pipe = NULL;
	}

	if (sc->sc_scord_pipe != NULL) {
		usbd_abort_pipe(sc->sc_scord_pipe);
		usbd_close_pipe(sc->sc_scord_pipe);
		sc->sc_scord_pipe = NULL;
	}

	if (sc->sc_scowr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_scowr_pipe);
		usbd_close_pipe(sc->sc_scowr_pipe);
		sc->sc_scowr_pipe = NULL;
	}

	/* Free event buffer */
	if (sc->sc_evt_buf != NULL) {
		free(sc->sc_evt_buf, M_USBDEV);
		sc->sc_evt_buf = NULL;
	}

	/* Free all xfers and xfer buffers (implicit) */
	if (sc->sc_cmd_xfer != NULL) {
		usbd_free_xfer(sc->sc_cmd_xfer);
		sc->sc_cmd_xfer = NULL;
		sc->sc_cmd_buf = NULL;
	}

	if (sc->sc_aclrd_xfer != NULL) {
		usbd_free_xfer(sc->sc_aclrd_xfer);
		sc->sc_aclrd_xfer = NULL;
		sc->sc_aclrd_buf = NULL;
	}

	if (sc->sc_aclwr_xfer != NULL) {
		usbd_free_xfer(sc->sc_aclwr_xfer);
		sc->sc_aclwr_xfer = NULL;
		sc->sc_aclwr_buf = NULL;
	}

	for (i = 0 ; i < UBT_NXFERS ; i++) {
		if (sc->sc_scord[i].xfer != NULL) {
			usbd_free_xfer(sc->sc_scord[i].xfer);
			sc->sc_scord[i].xfer = NULL;
			sc->sc_scord[i].buf = NULL;
		}

		if (sc->sc_scowr[i].xfer != NULL) {
			usbd_free_xfer(sc->sc_scowr[i].xfer);
			sc->sc_scowr[i].xfer = NULL;
			sc->sc_scowr[i].buf = NULL;
		}
	}

	/* Free partial SCO packets */
	if (sc->sc_scord_mbuf != NULL) {
		m_freem(sc->sc_scord_mbuf);
		sc->sc_scord_mbuf = NULL;
	}

	if (sc->sc_scowr_mbuf != NULL) {
		m_freem(sc->sc_scowr_mbuf);
		sc->sc_scowr_mbuf = NULL;
	}

	/* Empty mbuf queues */
	MBUFQ_DRAIN(&sc->sc_cmd_queue);
	MBUFQ_DRAIN(&sc->sc_aclwr_queue);
	MBUFQ_DRAIN(&sc->sc_scowr_queue);
}

/*******************************************************************************
 *
 * Bluetooth Unit/USB callbacks
 *
 */
static int
ubt_enable(device_t self)
{
	struct ubt_softc *sc = device_private(self);
	usbd_status err;
	int s, i, error;

	DPRINTFN(1, "sc=%p\n", sc);

	if (sc->sc_enabled)
		return 0;

	s = splusb();

	/* Events */
	sc->sc_evt_buf = malloc(UBT_BUFSIZ_EVENT, M_USBDEV, M_NOWAIT);
	if (sc->sc_evt_buf == NULL) {
		error = ENOMEM;
		goto bad;
	}
	err = usbd_open_pipe_intr(sc->sc_iface0,
				  sc->sc_evt_addr,
				  USBD_SHORT_XFER_OK,
				  &sc->sc_evt_pipe,
				  sc,
				  sc->sc_evt_buf,
				  UBT_BUFSIZ_EVENT,
				  ubt_recv_event,
				  USBD_DEFAULT_INTERVAL);
	if (err != USBD_NORMAL_COMPLETION) {
		error = EIO;
		goto bad;
	}

	/* Commands */
	sc->sc_cmd_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_cmd_xfer == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_cmd_buf = usbd_alloc_buffer(sc->sc_cmd_xfer, UBT_BUFSIZ_CMD);
	if (sc->sc_cmd_buf == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_cmd_busy = 0;

	/* ACL read */
	err = usbd_open_pipe(sc->sc_iface0, sc->sc_aclrd_addr,
				USBD_EXCLUSIVE_USE, &sc->sc_aclrd_pipe);
	if (err != USBD_NORMAL_COMPLETION) {
		error = EIO;
		goto bad;
	}
	sc->sc_aclrd_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_aclrd_xfer == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_aclrd_buf = usbd_alloc_buffer(sc->sc_aclrd_xfer, UBT_BUFSIZ_ACL);
	if (sc->sc_aclrd_buf == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_aclrd_busy = 0;
	ubt_recv_acl_start(sc);

	/* ACL write */
	err = usbd_open_pipe(sc->sc_iface0, sc->sc_aclwr_addr,
				USBD_EXCLUSIVE_USE, &sc->sc_aclwr_pipe);
	if (err != USBD_NORMAL_COMPLETION) {
		error = EIO;
		goto bad;
	}
	sc->sc_aclwr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_aclwr_xfer == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_aclwr_buf = usbd_alloc_buffer(sc->sc_aclwr_xfer, UBT_BUFSIZ_ACL);
	if (sc->sc_aclwr_buf == NULL) {
		error = ENOMEM;
		goto bad;
	}
	sc->sc_aclwr_busy = 0;

	/* SCO read */
	if (sc->sc_scord_size > 0) {
		err = usbd_open_pipe(sc->sc_iface1, sc->sc_scord_addr,
					USBD_EXCLUSIVE_USE, &sc->sc_scord_pipe);
		if (err != USBD_NORMAL_COMPLETION) {
			error = EIO;
			goto bad;
		}

		for (i = 0 ; i < UBT_NXFERS ; i++) {
			sc->sc_scord[i].xfer = usbd_alloc_xfer(sc->sc_udev);
			if (sc->sc_scord[i].xfer == NULL) {
				error = ENOMEM;
				goto bad;
			}
			sc->sc_scord[i].buf = usbd_alloc_buffer(sc->sc_scord[i].xfer,
						sc->sc_scord_size * UBT_NFRAMES);
			if (sc->sc_scord[i].buf == NULL) {
				error = ENOMEM;
				goto bad;
			}
			sc->sc_scord[i].softc = sc;
			sc->sc_scord[i].busy = 0;
			ubt_recv_sco_start1(sc, &sc->sc_scord[i]);
		}
	}

	/* SCO write */
	if (sc->sc_scowr_size > 0) {
		err = usbd_open_pipe(sc->sc_iface1, sc->sc_scowr_addr,
					USBD_EXCLUSIVE_USE, &sc->sc_scowr_pipe);
		if (err != USBD_NORMAL_COMPLETION) {
			error = EIO;
			goto bad;
		}

		for (i = 0 ; i < UBT_NXFERS ; i++) {
			sc->sc_scowr[i].xfer = usbd_alloc_xfer(sc->sc_udev);
			if (sc->sc_scowr[i].xfer == NULL) {
				error = ENOMEM;
				goto bad;
			}
			sc->sc_scowr[i].buf = usbd_alloc_buffer(sc->sc_scowr[i].xfer,
						sc->sc_scowr_size * UBT_NFRAMES);
			if (sc->sc_scowr[i].buf == NULL) {
				error = ENOMEM;
				goto bad;
			}
			sc->sc_scowr[i].softc = sc;
			sc->sc_scowr[i].busy = 0;
		}

		sc->sc_scowr_busy = 0;
	}

	sc->sc_enabled = 1;
	splx(s);
	return 0;

bad:
	ubt_abortdealloc(sc);
	splx(s);
	return error;
}

static void
ubt_disable(device_t self)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	DPRINTFN(1, "sc=%p\n", sc);

	if (sc->sc_enabled == 0)
		return;

	s = splusb();
	ubt_abortdealloc(sc);

	sc->sc_enabled = 0;
	splx(s);
}

static void
ubt_xmit_cmd(device_t self, struct mbuf *m)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	s = splusb();
	MBUFQ_ENQUEUE(&sc->sc_cmd_queue, m);

	if (sc->sc_cmd_busy == 0)
		ubt_xmit_cmd_start(sc);

	splx(s);
}

static void
ubt_xmit_cmd_start(struct ubt_softc *sc)
{
	usb_device_request_t req;
	usbd_status status;
	struct mbuf *m;
	int len;

	if (sc->sc_dying)
		return;

	if (MBUFQ_FIRST(&sc->sc_cmd_queue) == NULL)
		return;

	MBUFQ_DEQUEUE(&sc->sc_cmd_queue, m);
	KASSERT(m != NULL);

	DPRINTFN(15, "%s: xmit CMD packet (%d bytes)\n",
			device_xname(sc->sc_dev), m->m_pkthdr.len);

	sc->sc_refcnt++;
	sc->sc_cmd_busy = 1;

	len = m->m_pkthdr.len - 1;
	m_copydata(m, 1, len, sc->sc_cmd_buf);
	m_freem(m);

	memset(&req, 0, sizeof(req));
	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	USETW(req.wLength, len);

	usbd_setup_default_xfer(sc->sc_cmd_xfer,
				sc->sc_udev,
				sc,
				UBT_CMD_TIMEOUT,
				&req,
				sc->sc_cmd_buf,
				len,
				USBD_NO_COPY | USBD_FORCE_SHORT_XFER,
				ubt_xmit_cmd_complete);

	status = usbd_transfer(sc->sc_cmd_xfer);

	KASSERT(status != USBD_NORMAL_COMPLETION);

	if (status != USBD_IN_PROGRESS) {
		DPRINTF("usbd_transfer status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_refcnt--;
		sc->sc_cmd_busy = 0;
	}
}

static void
ubt_xmit_cmd_complete(usbd_xfer_handle xfer,
			usbd_private_handle h, usbd_status status)
{
	struct ubt_softc *sc = h;
	uint32_t count;

	DPRINTFN(15, "%s: CMD complete status=%s (%d)\n",
			device_xname(sc->sc_dev), usbd_errstr(status), status);

	sc->sc_cmd_busy = 0;

	if (--sc->sc_refcnt < 0) {
		DPRINTF("sc_refcnt=%d\n", sc->sc_refcnt);
		usb_detach_wakeupold(sc->sc_dev);
		return;
	}

	if (sc->sc_dying) {
		DPRINTF("sc_dying\n");
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_stats.err_tx++;
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	sc->sc_stats.cmd_tx++;
	sc->sc_stats.byte_tx += count;

	ubt_xmit_cmd_start(sc);
}

static void
ubt_xmit_acl(device_t self, struct mbuf *m)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	s = splusb();
	MBUFQ_ENQUEUE(&sc->sc_aclwr_queue, m);

	if (sc->sc_aclwr_busy == 0)
		ubt_xmit_acl_start(sc);

	splx(s);
}

static void
ubt_xmit_acl_start(struct ubt_softc *sc)
{
	struct mbuf *m;
	usbd_status status;
	int len;

	if (sc->sc_dying)
		return;

	if (MBUFQ_FIRST(&sc->sc_aclwr_queue) == NULL)
		return;

	sc->sc_refcnt++;
	sc->sc_aclwr_busy = 1;

	MBUFQ_DEQUEUE(&sc->sc_aclwr_queue, m);
	KASSERT(m != NULL);

	DPRINTFN(15, "%s: xmit ACL packet (%d bytes)\n",
			device_xname(sc->sc_dev), m->m_pkthdr.len);

	len = m->m_pkthdr.len - 1;
	if (len > UBT_BUFSIZ_ACL) {
		DPRINTF("%s: truncating ACL packet (%d => %d)!\n",
			device_xname(sc->sc_dev), len, UBT_BUFSIZ_ACL);

		len = UBT_BUFSIZ_ACL;
	}

	m_copydata(m, 1, len, sc->sc_aclwr_buf);
	m_freem(m);

	sc->sc_stats.acl_tx++;
	sc->sc_stats.byte_tx += len;

	usbd_setup_xfer(sc->sc_aclwr_xfer,
			sc->sc_aclwr_pipe,
			sc,
			sc->sc_aclwr_buf,
			len,
			USBD_NO_COPY | USBD_FORCE_SHORT_XFER,
			UBT_ACL_TIMEOUT,
			ubt_xmit_acl_complete);

	status = usbd_transfer(sc->sc_aclwr_xfer);

	KASSERT(status != USBD_NORMAL_COMPLETION);

	if (status != USBD_IN_PROGRESS) {
		DPRINTF("usbd_transfer status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_refcnt--;
		sc->sc_aclwr_busy = 0;
	}
}

static void
ubt_xmit_acl_complete(usbd_xfer_handle xfer,
		usbd_private_handle h, usbd_status status)
{
	struct ubt_softc *sc = h;

	DPRINTFN(15, "%s: ACL complete status=%s (%d)\n",
		device_xname(sc->sc_dev), usbd_errstr(status), status);

	sc->sc_aclwr_busy = 0;

	if (--sc->sc_refcnt < 0) {
		usb_detach_wakeupold(sc->sc_dev);
		return;
	}

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_stats.err_tx++;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_aclwr_pipe);
		else
			return;
	}

	ubt_xmit_acl_start(sc);
}

static void
ubt_xmit_sco(device_t self, struct mbuf *m)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	s = splusb();
	MBUFQ_ENQUEUE(&sc->sc_scowr_queue, m);

	if (sc->sc_scowr_busy == 0)
		ubt_xmit_sco_start(sc);

	splx(s);
}

static void
ubt_xmit_sco_start(struct ubt_softc *sc)
{
	int i;

	if (sc->sc_dying || sc->sc_scowr_size == 0)
		return;

	for (i = 0 ; i < UBT_NXFERS ; i++) {
		if (sc->sc_scowr[i].busy)
			continue;

		ubt_xmit_sco_start1(sc, &sc->sc_scowr[i]);
	}
}

static void
ubt_xmit_sco_start1(struct ubt_softc *sc, struct ubt_isoc_xfer *isoc)
{
	struct mbuf *m;
	uint8_t *buf;
	int num, len, size, space;

	space = sc->sc_scowr_size * UBT_NFRAMES;
	buf = isoc->buf;
	len = 0;

	/*
	 * Fill the request buffer with data from the queue,
	 * keeping any leftover packet on our private hook.
	 *
	 * Complete packets are passed back up to the stack
	 * for disposal, since we can't rely on the controller
	 * to tell us when it has finished with them.
	 */

	m = sc->sc_scowr_mbuf;
	while (space > 0) {
		if (m == NULL) {
			MBUFQ_DEQUEUE(&sc->sc_scowr_queue, m);
			if (m == NULL)
				break;

			m_adj(m, 1);	/* packet type */
		}

		if (m->m_pkthdr.len > 0) {
			size = MIN(m->m_pkthdr.len, space);

			m_copydata(m, 0, size, buf);
			m_adj(m, size);

			buf += size;
			len += size;
			space -= size;
		}

		if (m->m_pkthdr.len == 0) {
			sc->sc_stats.sco_tx++;
			if (!hci_complete_sco(sc->sc_unit, m))
				sc->sc_stats.err_tx++;

			m = NULL;
		}
	}
	sc->sc_scowr_mbuf = m;

	DPRINTFN(15, "isoc=%p, len=%d, space=%d\n", isoc, len, space);

	if (len == 0)	/* nothing to send */
		return;

	sc->sc_refcnt++;
	sc->sc_scowr_busy = 1;
	sc->sc_stats.byte_tx += len;
	isoc->busy = 1;

	/*
	 * calculate number of isoc frames and sizes
	 */

	for (num = 0 ; len > 0 ; num++) {
		size = MIN(sc->sc_scowr_size, len);

		isoc->size[num] = size;
		len -= size;
	}

	usbd_setup_isoc_xfer(isoc->xfer,
			     sc->sc_scowr_pipe,
			     isoc,
			     isoc->size,
			     num,
			     USBD_NO_COPY | USBD_FORCE_SHORT_XFER,
			     ubt_xmit_sco_complete);

	usbd_transfer(isoc->xfer);
}

static void
ubt_xmit_sco_complete(usbd_xfer_handle xfer,
		usbd_private_handle h, usbd_status status)
{
	struct ubt_isoc_xfer *isoc = h;
	struct ubt_softc *sc;
	int i;

	KASSERT(xfer == isoc->xfer);
	sc = isoc->softc;

	DPRINTFN(15, "isoc=%p, status=%s (%d)\n",
		isoc, usbd_errstr(status), status);

	isoc->busy = 0;

	for (i = 0 ; ; i++) {
		if (i == UBT_NXFERS) {
			sc->sc_scowr_busy = 0;
			break;
		}

		if (sc->sc_scowr[i].busy)
			break;
	}

	if (--sc->sc_refcnt < 0) {
		usb_detach_wakeupold(sc->sc_dev);
		return;
	}

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_stats.err_tx++;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_scowr_pipe);
		else
			return;
	}

	ubt_xmit_sco_start(sc);
}

/*
 * load incoming data into an mbuf with
 * leading type byte
 */
static struct mbuf *
ubt_mbufload(uint8_t *buf, int count, uint8_t type)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	*mtod(m, uint8_t *) = type;
	m->m_pkthdr.len = m->m_len = MHLEN;
	m_copyback(m, 1, count, buf);	// (extends if necessary)
	if (m->m_pkthdr.len != MAX(MHLEN, count + 1)) {
		m_free(m);
		return NULL;
	}

	m->m_pkthdr.len = count + 1;
	m->m_len = MIN(MHLEN, m->m_pkthdr.len);

	return m;
}

static void
ubt_recv_event(usbd_xfer_handle xfer, usbd_private_handle h, usbd_status status)
{
	struct ubt_softc *sc = h;
	struct mbuf *m;
	uint32_t count;
	void *buf;

	DPRINTFN(15, "sc=%p status=%s (%d)\n",
		    sc, usbd_errstr(status), status);

	if (status != USBD_NORMAL_COMPLETION || sc->sc_dying)
		return;

	usbd_get_xfer_status(xfer, NULL, &buf, &count, NULL);

	if (count < sizeof(hci_event_hdr_t) - 1) {
		DPRINTF("dumped undersized event (count = %d)\n", count);
		sc->sc_stats.err_rx++;
		return;
	}

	sc->sc_stats.evt_rx++;
	sc->sc_stats.byte_rx += count;

	m = ubt_mbufload(buf, count, HCI_EVENT_PKT);
	if (m == NULL || !hci_input_event(sc->sc_unit, m))
		sc->sc_stats.err_rx++;
}

static void
ubt_recv_acl_start(struct ubt_softc *sc)
{
	usbd_status status;

	DPRINTFN(15, "sc=%p\n", sc);

	if (sc->sc_aclrd_busy || sc->sc_dying) {
		DPRINTF("sc_aclrd_busy=%d, sc_dying=%d\n",
			sc->sc_aclrd_busy,
			sc->sc_dying);

		return;
	}

	sc->sc_refcnt++;
	sc->sc_aclrd_busy = 1;

	usbd_setup_xfer(sc->sc_aclrd_xfer,
			sc->sc_aclrd_pipe,
			sc,
			sc->sc_aclrd_buf,
			UBT_BUFSIZ_ACL,
			USBD_NO_COPY | USBD_SHORT_XFER_OK,
			USBD_NO_TIMEOUT,
			ubt_recv_acl_complete);

	status = usbd_transfer(sc->sc_aclrd_xfer);

	KASSERT(status != USBD_NORMAL_COMPLETION);

	if (status != USBD_IN_PROGRESS) {
		DPRINTF("usbd_transfer status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_refcnt--;
		sc->sc_aclrd_busy = 0;
	}
}

static void
ubt_recv_acl_complete(usbd_xfer_handle xfer,
		usbd_private_handle h, usbd_status status)
{
	struct ubt_softc *sc = h;
	struct mbuf *m;
	uint32_t count;
	void *buf;

	DPRINTFN(15, "sc=%p status=%s (%d)\n",
			sc, usbd_errstr(status), status);

	sc->sc_aclrd_busy = 0;

	if (--sc->sc_refcnt < 0) {
		DPRINTF("refcnt = %d\n", sc->sc_refcnt);
		usb_detach_wakeupold(sc->sc_dev);
		return;
	}

	if (sc->sc_dying) {
		DPRINTF("sc_dying\n");
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_stats.err_rx++;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_aclrd_pipe);
		else
			return;
	} else {
		usbd_get_xfer_status(xfer, NULL, &buf, &count, NULL);

		if (count < sizeof(hci_acldata_hdr_t) - 1) {
			DPRINTF("dumped undersized packet (%d)\n", count);
			sc->sc_stats.err_rx++;
		} else {
			sc->sc_stats.acl_rx++;
			sc->sc_stats.byte_rx += count;

			m = ubt_mbufload(buf, count, HCI_ACL_DATA_PKT);
			if (m == NULL || !hci_input_acl(sc->sc_unit, m))
				sc->sc_stats.err_rx++;
		}
	}

	/* and restart */
	ubt_recv_acl_start(sc);
}

static void
ubt_recv_sco_start1(struct ubt_softc *sc, struct ubt_isoc_xfer *isoc)
{
	int i;

	DPRINTFN(15, "sc=%p, isoc=%p\n", sc, isoc);

	if (isoc->busy || sc->sc_dying || sc->sc_scord_size == 0) {
		DPRINTF("%s%s%s\n",
			isoc->busy ? " busy" : "",
			sc->sc_dying ? " dying" : "",
			sc->sc_scord_size == 0 ? " size=0" : "");

		return;
	}

	sc->sc_refcnt++;
	isoc->busy = 1;

	for (i = 0 ; i < UBT_NFRAMES ; i++)
		isoc->size[i] = sc->sc_scord_size;

	usbd_setup_isoc_xfer(isoc->xfer,
			     sc->sc_scord_pipe,
			     isoc,
			     isoc->size,
			     UBT_NFRAMES,
			     USBD_NO_COPY | USBD_SHORT_XFER_OK,
			     ubt_recv_sco_complete);

	usbd_transfer(isoc->xfer);
}

static void
ubt_recv_sco_complete(usbd_xfer_handle xfer,
		usbd_private_handle h, usbd_status status)
{
	struct ubt_isoc_xfer *isoc = h;
	struct ubt_softc *sc;
	struct mbuf *m;
	uint32_t count;
	uint8_t *ptr, *frame;
	int i, size, got, want;

	KASSERT(isoc != NULL);
	KASSERT(isoc->xfer == xfer);

	sc = isoc->softc;
	isoc->busy = 0;

	if (--sc->sc_refcnt < 0) {
		DPRINTF("refcnt=%d\n", sc->sc_refcnt);
		usb_detach_wakeupold(sc->sc_dev);
		return;
	}

	if (sc->sc_dying) {
		DPRINTF("sc_dying\n");
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("status=%s (%d)\n",
			usbd_errstr(status), status);

		sc->sc_stats.err_rx++;

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(sc->sc_scord_pipe);
			goto restart;
		}

		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	if (count == 0)
		goto restart;

	DPRINTFN(15, "sc=%p, isoc=%p, count=%u\n",
			sc, isoc, count);

	sc->sc_stats.byte_rx += count;

	/*
	 * Extract SCO packets from ISOC frames. The way we have it,
	 * no SCO packet can be bigger than MHLEN. This is unlikely
	 * to actually happen, but if we ran out of mbufs and lost
	 * sync then we may get spurious data that makes it seem that
	 * way, so we discard data that wont fit. This doesnt really
	 * help with the lost sync situation alas.
	 */

	m = sc->sc_scord_mbuf;
	if (m != NULL) {
		sc->sc_scord_mbuf = NULL;
		ptr = mtod(m, uint8_t *) + m->m_pkthdr.len;
		got = m->m_pkthdr.len;
		want = sizeof(hci_scodata_hdr_t);
		if (got >= want)
			want += mtod(m, hci_scodata_hdr_t *)->length ;
	} else {
		ptr = NULL;
		got = 0;
		want = 0;
	}

	for (i = 0 ; i < UBT_NFRAMES ; i++) {
		frame = isoc->buf + (i * sc->sc_scord_size);

		while (isoc->size[i] > 0) {
			size = isoc->size[i];

			if (m == NULL) {
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					aprint_error_dev(sc->sc_dev,
					    "out of memory (xfer halted)\n");

					sc->sc_stats.err_rx++;
					return;		/* lost sync */
				}

				ptr = mtod(m, uint8_t *);
				*ptr++ = HCI_SCO_DATA_PKT;
				got = 1;
				want = sizeof(hci_scodata_hdr_t);
			}

			if (got + size > want)
				size = want - got;

			memcpy(ptr, frame, size);

			ptr += size;
			got += size;
			frame += size;

			if (got == want) {
				/*
				 * If we only got a header, add the packet
				 * length to our want count. Send complete
				 * packets up to protocol stack.
				 */
				if (want == sizeof(hci_scodata_hdr_t)) {
					uint32_t len =
					    mtod(m, hci_scodata_hdr_t *)->length;
					want += len;
					if (len == 0 || want > MHLEN) {
						aprint_error_dev(sc->sc_dev,
						    "packet too large %u "
						    "(lost sync)\n", len);
						sc->sc_stats.err_rx++;
						return;
					}
				}

				if (got == want) {
					m->m_pkthdr.len = m->m_len = got;
					sc->sc_stats.sco_rx++;
					if (!hci_input_sco(sc->sc_unit, m))
						sc->sc_stats.err_rx++;
						
					m = NULL;
				}
			}

			isoc->size[i] -= size;
		}
	}

	if (m != NULL) {
		m->m_pkthdr.len = m->m_len = got;
		sc->sc_scord_mbuf = m;
	}

restart: /* and restart */
	ubt_recv_sco_start1(sc, isoc);
}

void
ubt_stats(device_t self, struct bt_stats *dest, int flush)
{
	struct ubt_softc *sc = device_private(self);
	int s;

	s = splusb();
	memcpy(dest, &sc->sc_stats, sizeof(struct bt_stats));

	if (flush)
		memset(&sc->sc_stats, 0, sizeof(struct bt_stats));

	splx(s);
}
