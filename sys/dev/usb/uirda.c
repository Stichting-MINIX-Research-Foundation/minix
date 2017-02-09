/*	$NetBSD: uirda.c,v 1.38 2013/09/15 15:43:20 martin Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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
__KERNEL_RCSID(0, "$NetBSD: uirda.c,v 1.38 2013/09/15 15:43:20 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/proc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/ir/ir.h>
#include <dev/ir/irdaio.h>
#include <dev/ir/irframevar.h>

#include <dev/usb/uirdavar.h>

#ifdef UIRDA_DEBUG
#define DPRINTF(x)	if (uirdadebug) printf x
#define DPRINTFN(n,x)	if (uirdadebug>(n)) printf x
int	uirdadebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


/* Class specific requests */
#define UR_IRDA_RECEIVING		0x01	/* Receive in progress? */
#define UR_IRDA_CHECK_MEDIA_BUSY	0x03
#define UR_IRDA_SET_RATE_SNIFF		0x04	/* opt */
#define UR_IRDA_SET_UNICAST_LIST	0x05	/* opt */
#define UR_IRDA_GET_DESC		0x06

#define UIRDA_NEBOFS 8
static struct {
	int count;
	int mask;
	int header;
} uirda_ebofs[UIRDA_NEBOFS] = {
	{ 0, UI_EB_0, UIRDA_EB_0 },
	{ 1, UI_EB_1, UIRDA_EB_1 },
	{ 2, UI_EB_2, UIRDA_EB_2 },
	{ 3, UI_EB_3, UIRDA_EB_3 },
	{ 6, UI_EB_6, UIRDA_EB_6 },
	{ 12, UI_EB_12, UIRDA_EB_12 },
	{ 24, UI_EB_24, UIRDA_EB_24 },
	{ 48, UI_EB_48, UIRDA_EB_48 }
};

#define UIRDA_NSPEEDS 9
static struct {
	int speed;
	int mask;
	int header;
} uirda_speeds[UIRDA_NSPEEDS] = {
	{ 4000000, UI_BR_4000000, UIRDA_4000000 },
	{ 1152000, UI_BR_1152000, UIRDA_1152000 },
	{ 576000, UI_BR_576000, UIRDA_576000 },
	{ 115200, UI_BR_115200, UIRDA_115200 },
	{ 57600, UI_BR_57600, UIRDA_57600 },
	{ 38400, UI_BR_38400, UIRDA_38400 },
	{ 19200, UI_BR_19200, UIRDA_19200 },
	{ 9600, UI_BR_9600, UIRDA_9600 },
	{ 2400, UI_BR_2400, UIRDA_2400 },
};



int uirda_open(void *h, int flag, int mode, struct lwp *l);
int uirda_close(void *h, int flag, int mode, struct lwp *l);
int uirda_read(void *h, struct uio *uio, int flag);
int uirda_write(void *h, struct uio *uio, int flag);
int uirda_set_params(void *h, struct irda_params *params);
int uirda_get_speeds(void *h, int *speeds);
int uirda_get_turnarounds(void *h, int *times);
int uirda_poll(void *h, int events, struct lwp *l);
int uirda_kqfilter(void *h, struct knote *kn);

struct irframe_methods uirda_methods = {
	uirda_open, uirda_close, uirda_read, uirda_write, uirda_poll,
	uirda_kqfilter, uirda_set_params, uirda_get_speeds,
	uirda_get_turnarounds
};

void uirda_rd_cb(usbd_xfer_handle xfer,	usbd_private_handle priv,
		 usbd_status status);
usbd_status uirda_start_read(struct uirda_softc *sc);

/*
 * These devices don't quite follow the spec.  Speed changing is broken
 * and they don't handle windows.
 * But we change speed in a safe way, and don't use windows now.
 * Some devices also seem to have an interrupt pipe that can be ignored.
 *
 * Table information taken from Linux driver.
 */
Static const struct usb_devno uirda_devs[] = {
	{ USB_VENDOR_ACTISYS, USB_PRODUCT_ACTISYS_IR2000U },
	{ USB_VENDOR_EXTENDED, USB_PRODUCT_EXTENDED_XTNDACCESS },
	{ USB_VENDOR_KAWATSU, USB_PRODUCT_KAWATSU_KC180 },
};
#define uirda_lookup(v, p) (usb_lookup(uirda_devs, v, p))

int uirda_match(device_t, cfdata_t, void *);
void uirda_attach(device_t, device_t, void *);
void uirda_childdet(device_t, device_t);
int uirda_detach(device_t, int);
int uirda_activate(device_t, enum devact);
extern struct cfdriver uirda_cd;
CFATTACH_DECL2_NEW(uirda, sizeof(struct uirda_softc), uirda_match,
    uirda_attach, uirda_detach, uirda_activate, NULL, uirda_childdet);

int 
uirda_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	DPRINTFN(50,("uirda_match\n"));

	if (uirda_lookup(uaa->vendor, uaa->product) != NULL)
		return (UMATCH_VENDOR_PRODUCT);

	if (uaa->class == UICLASS_APPL_SPEC &&
	    uaa->subclass == UISUBCLASS_IRDA &&
	    uaa->proto == UIPROTO_IRDA)
		return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
	return (UMATCH_NONE);
}

void 
uirda_attach(device_t parent, device_t self, void *aux)
{
	struct uirda_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface = uaa->iface;
	char			*devinfop;
	usb_endpoint_descriptor_t *ed;
	usbd_status		err;
	u_int8_t		epcount;
	u_int			specrev;
	int			i;
	struct ir_attach_args	ia;

	DPRINTFN(10,("uirda_attach: sc=%p\n", sc));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	if (sc->sc_hdszi == 0)
		sc->sc_hdszi = UIRDA_INPUT_HEADER_SIZE;

	epcount = 0;
	(void)usbd_endpoint_count(iface, &epcount);

	sc->sc_rd_addr = -1;
	sc->sc_wr_addr = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_rd_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_wr_addr = ed->bEndpointAddress;
		}
	}
	if (sc->sc_rd_addr == -1 || sc->sc_wr_addr == -1) {
		aprint_error_dev(self, "missing endpoint\n");
		return;
	}

	if (sc->sc_loadfw(sc) != 0) {
		return;
	}

	/* Get the IrDA descriptor */
	err = usbd_get_class_desc(sc->sc_udev, UDESC_IRDA, 0,
		USB_IRDA_DESCRIPTOR_SIZE, &sc->sc_irdadesc);
	aprint_error_dev(self, "error %d reading class desc\n", err);
	if (err) {
		err = usbd_get_desc(sc->sc_udev, UDESC_IRDA, 0,
		  USB_IRDA_DESCRIPTOR_SIZE, &sc->sc_irdadesc);
	}
	aprint_error_dev(self, "error %d reading desc\n", err);
	if (err) {
		/* maybe it's embedded in the config desc? */
		usbd_desc_iter_t iter;
		const usb_descriptor_t *d;
		usb_desc_iter_init(sc->sc_udev, &iter);
		for (;;) {
			d = usb_desc_iter_next(&iter);
			if (!d || d->bDescriptorType == UDESC_IRDA)
				break;
		}
		if (d == NULL) {
			aprint_error_dev(self,
			    "Cannot get IrDA descriptor\n");
			return;
		}
		memcpy(&sc->sc_irdadesc, d, USB_IRDA_DESCRIPTOR_SIZE);
	}
	DPRINTF(("uirda_attach: bDescriptorSize %d bDescriptorType 0x%x "
		 "bmDataSize=0x%02x bmWindowSize=0x%02x "
		 "bmMinTurnaroundTime=0x%02x wBaudRate=0x%04x "
		 "bmAdditionalBOFs=0x%02x bIrdaSniff=%d bMaxUnicastList=%d\n",
		 sc->sc_irdadesc.bLength,
		 sc->sc_irdadesc.bDescriptorType,
		 sc->sc_irdadesc.bmDataSize,
		 sc->sc_irdadesc.bmWindowSize,
		 sc->sc_irdadesc.bmMinTurnaroundTime,
		 UGETW(sc->sc_irdadesc.wBaudRate),
		 sc->sc_irdadesc.bmAdditionalBOFs,
		 sc->sc_irdadesc.bIrdaSniff,
		 sc->sc_irdadesc.bMaxUnicastList));

	specrev = UGETW(sc->sc_irdadesc.bcdSpecRevision);
	aprint_normal_dev(self, "USB-IrDA protocol version %x.%02x\n",
	    specrev >> 8, specrev & 0xff);

	DPRINTFN(10, ("uirda_attach: %p\n", sc->sc_udev));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	mutex_init(&sc->sc_wr_buf_lk, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_rd_buf_lk, MUTEX_DEFAULT, IPL_NONE);
	selinit(&sc->sc_rd_sel);
	selinit(&sc->sc_wr_sel);

	ia.ia_type = IR_TYPE_IRFRAME;
	ia.ia_methods = sc->sc_irm ? sc->sc_irm : &uirda_methods;
	ia.ia_handle = sc;

	sc->sc_child = config_found(self, &ia, ir_print);

	return;
}

int 
uirda_detach(device_t self, int flags)
{
	struct uirda_softc *sc = device_private(self);
	int s;
	int rv = 0;

	DPRINTF(("uirda_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_rd_pipe != NULL) {
		usbd_abort_pipe(sc->sc_rd_pipe);
		usbd_close_pipe(sc->sc_rd_pipe);
		sc->sc_rd_pipe = NULL;
	}
	if (sc->sc_wr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_wr_pipe);
		usbd_close_pipe(sc->sc_wr_pipe);
		sc->sc_wr_pipe = NULL;
	}
	wakeup(&sc->sc_rd_count);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_waitold(sc->sc_dev);
	}
	splx(s);

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	mutex_destroy(&sc->sc_wr_buf_lk);
	mutex_destroy(&sc->sc_rd_buf_lk);
	seldestroy(&sc->sc_rd_sel);
	seldestroy(&sc->sc_wr_sel);

	return (rv);
}

void
uirda_childdet(device_t self, device_t child)
{
	struct uirda_softc *sc = device_private(self);

	KASSERT(sc->sc_child == child);
	sc->sc_child = NULL;
}

int
uirda_activate(device_t self, enum devact act)
{
	struct uirda_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
uirda_open(void *h, int flag, int mode,
    struct lwp *l)
{
	struct uirda_softc *sc = h;
	int error;
	usbd_status err;

	DPRINTF(("%s: sc=%p\n", __func__, sc));

	err = usbd_open_pipe(sc->sc_iface, sc->sc_rd_addr, 0, &sc->sc_rd_pipe);
	if (err) {
		error = EIO;
		goto bad1;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_wr_addr, 0, &sc->sc_wr_pipe);
	if (err) {
		error = EIO;
		goto bad2;
	}
	sc->sc_rd_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_rd_xfer == NULL) {
		error = ENOMEM;
		goto bad3;
	}
	sc->sc_wr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_wr_xfer == NULL) {
		error = ENOMEM;
		goto bad4;
	}
	sc->sc_rd_buf = usbd_alloc_buffer(sc->sc_rd_xfer,
			    IRDA_MAX_FRAME_SIZE + sc->sc_hdszi);
	if (sc->sc_rd_buf == NULL) {
		error = ENOMEM;
		goto bad5;
	}
	sc->sc_wr_buf = usbd_alloc_buffer(sc->sc_wr_xfer,
			    IRDA_MAX_FRAME_SIZE + UIRDA_OUTPUT_HEADER_SIZE +
				2 + 1 /* worst case ST-UIRDA */);
	if (sc->sc_wr_buf == NULL) {
		error = ENOMEM;
		goto bad5;
	}
	sc->sc_rd_count = 0;
	sc->sc_rd_err = 0;
	sc->sc_params.speed = 0;
	sc->sc_params.ebofs = 0;
	sc->sc_params.maxsize = IRDA_MAX_FRAME_SIZE;
	sc->sc_wr_hdr = -1;

	err = uirda_start_read(sc);
	/* XXX check err */

	return (0);

bad5:
	usbd_free_xfer(sc->sc_wr_xfer);
	sc->sc_wr_xfer = NULL;
bad4:
	usbd_free_xfer(sc->sc_rd_xfer);
	sc->sc_rd_xfer = NULL;
bad3:
	usbd_close_pipe(sc->sc_wr_pipe);
	sc->sc_wr_pipe = NULL;
bad2:
	usbd_close_pipe(sc->sc_rd_pipe);
	sc->sc_rd_pipe = NULL;
bad1:
	return (error);
}

int
uirda_close(void *h, int flag, int mode,
    struct lwp *l)
{
	struct uirda_softc *sc = h;

	DPRINTF(("%s: sc=%p\n", __func__, sc));

	if (sc->sc_rd_pipe != NULL) {
		usbd_abort_pipe(sc->sc_rd_pipe);
		usbd_close_pipe(sc->sc_rd_pipe);
		sc->sc_rd_pipe = NULL;
	}
	if (sc->sc_wr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_wr_pipe);
		usbd_close_pipe(sc->sc_wr_pipe);
		sc->sc_wr_pipe = NULL;
	}
	if (sc->sc_rd_xfer != NULL) {
		usbd_free_xfer(sc->sc_rd_xfer);
		sc->sc_rd_xfer = NULL;
		sc->sc_rd_buf = NULL;
	}
	if (sc->sc_wr_xfer != NULL) {
		usbd_free_xfer(sc->sc_wr_xfer);
		sc->sc_wr_xfer = NULL;
		sc->sc_wr_buf = NULL;
	}

	return (0);
}

int
uirda_read(void *h, struct uio *uio, int flag)
{
	struct uirda_softc *sc = h;
	int s;
	int error;
	u_int n;

	DPRINTFN(1,("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return (EIO);

#ifdef DIAGNOSTIC
	if (sc->sc_rd_buf == NULL)
		return (EINVAL);
#endif

	sc->sc_refcnt++;

	do {
		s = splusb();
		while (sc->sc_rd_count == 0) {
			DPRINTFN(5,("uirda_read: calling tsleep()\n"));
			error = tsleep(&sc->sc_rd_count, PZERO | PCATCH,
				       "uirdrd", 0);
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				splx(s);
				DPRINTF(("uirda_read: tsleep() = %d\n", error));
				goto ret;
			}
		}
		splx(s);

		mutex_enter(&sc->sc_rd_buf_lk);
		n = sc->sc_rd_count - sc->sc_hdszi;
		DPRINTFN(1,("%s: sc=%p n=%u, hdr=0x%02x\n", __func__,
			    sc, n, sc->sc_rd_buf[0]));
		if (n > uio->uio_resid)
			error = EINVAL;
		else
			error = uiomove(sc->sc_rd_buf + sc->sc_hdszi, n, uio);
		sc->sc_rd_count = 0;
		mutex_exit(&sc->sc_rd_buf_lk);

		uirda_start_read(sc);
		/* XXX check uirda_start_read() return value */

	} while (n == 0);

	DPRINTFN(1,("uirda_read: return %d\n", error));

 ret:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	return (error);
}

int
uirda_write(void *h, struct uio *uio, int flag)
{
	struct uirda_softc *sc = h;
	usbd_status err;
	u_int32_t n;
	int error = 0;

	DPRINTFN(1,("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return (EIO);

#ifdef DIAGNOSTIC
	if (sc->sc_wr_buf == NULL)
		return (EINVAL);
#endif

	n = uio->uio_resid;
	if (n > sc->sc_params.maxsize)
		return (EINVAL);

	sc->sc_refcnt++;
	mutex_enter(&sc->sc_wr_buf_lk);

	sc->sc_wr_buf[0] = UIRDA_EB_NO_CHANGE | UIRDA_NO_SPEED;
	error = uiomove(sc->sc_wr_buf + UIRDA_OUTPUT_HEADER_SIZE, n, uio);
	if (!error) {
		DPRINTFN(1, ("uirdawrite: transfer %d bytes\n", n));

		n += UIRDA_OUTPUT_HEADER_SIZE;
		err = usbd_bulk_transfer(sc->sc_wr_xfer, sc->sc_wr_pipe,
			  USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
			  UIRDA_WR_TIMEOUT,
			  sc->sc_wr_buf, &n, "uirdawr");
		DPRINTFN(2, ("uirdawrite: err=%d\n", err));
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
		}
	}

	mutex_exit(&sc->sc_wr_buf_lk);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	DPRINTFN(1,("%s: sc=%p done\n", __func__, sc));
	return (error);
}

int
uirda_poll(void *h, int events, struct lwp *l)
{
	struct uirda_softc *sc = h;
	int revents = 0;
	int s;

	DPRINTFN(1,("%s: sc=%p\n", __func__, sc));

	s = splusb();
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_rd_count != 0) {
			DPRINTFN(2,("%s: have data\n", __func__));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			DPRINTFN(2,("%s: recording select\n", __func__));
			selrecord(l, &sc->sc_rd_sel);
		}
	}
	splx(s);

	return (revents);
}

static void
filt_uirdardetach(struct knote *kn)
{
	struct uirda_softc *sc = kn->kn_hook;
	int s;

	s = splusb();
	SLIST_REMOVE(&sc->sc_rd_sel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_uirdaread(struct knote *kn, long hint)
{
	struct uirda_softc *sc = kn->kn_hook;

	kn->kn_data = sc->sc_rd_count;
	return (kn->kn_data > 0);
}

static void
filt_uirdawdetach(struct knote *kn)
{
	struct uirda_softc *sc = kn->kn_hook;
	int s;

	s = splusb();
	SLIST_REMOVE(&sc->sc_wr_sel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static const struct filterops uirdaread_filtops =
	{ 1, NULL, filt_uirdardetach, filt_uirdaread };
static const struct filterops uirdawrite_filtops =
	{ 1, NULL, filt_uirdawdetach, filt_seltrue };

int
uirda_kqfilter(void *h, struct knote *kn)
{
	struct uirda_softc *sc = kn->kn_hook;
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rd_sel.sel_klist;
		kn->kn_fop = &uirdaread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->sc_wr_sel.sel_klist;
		kn->kn_fop = &uirdawrite_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = sc;

	s = splusb();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

int
uirda_set_params(void *h, struct irda_params *p)
{
	struct uirda_softc *sc = h;
	usbd_status err;
	int i;
	u_int8_t hdr;
	u_int32_t n;
	u_int mask;

	DPRINTF(("%s: sc=%p, speed=%d ebofs=%d maxsize=%d\n", __func__,
		 sc, p->speed, p->ebofs, p->maxsize));

	if (sc->sc_dying)
		return (EIO);

	hdr = 0;
	if (p->ebofs != sc->sc_params.ebofs) {
		/* round up ebofs */
		mask = 1 /* sc->sc_irdadesc.bmAdditionalBOFs*/;
		DPRINTF(("u.s.p.: mask=0x%x, sc->ebofs=%d, p->ebofs=%d\n",
			mask, sc->sc_params.ebofs, p->ebofs));
		for (i = 0; i < UIRDA_NEBOFS; i++) {
			DPRINTF(("u.s.p.: u_e[%d].mask=0x%x, count=%d\n",
				i, uirda_ebofs[i].mask, uirda_ebofs[i].count));
			if ((mask & uirda_ebofs[i].mask) &&
			    uirda_ebofs[i].count >= p->ebofs) {
				hdr = uirda_ebofs[i].header;
				goto found1;
			}
		}
		for (i = 0; i < UIRDA_NEBOFS; i++) {
			DPRINTF(("u.s.p.: u_e[%d].mask=0x%x, count=%d\n",
				i, uirda_ebofs[i].mask, uirda_ebofs[i].count));
			if ((mask & uirda_ebofs[i].mask)) {
				hdr = uirda_ebofs[i].header;
				goto found1;
			}
		}
		/* no good value found */
		return (EINVAL);
	found1:
		DPRINTF(("uirda_set_params: ebofs hdr=0x%02x\n", hdr));
		;

	}
	if (hdr != 0 || p->speed != sc->sc_params.speed) {
		/* find speed */
		mask = UGETW(sc->sc_irdadesc.wBaudRate);
		for (i = 0; i < UIRDA_NSPEEDS; i++) {
			if ((mask & uirda_speeds[i].mask) &&
			    uirda_speeds[i].speed == p->speed) {
				hdr |= uirda_speeds[i].header;
				goto found2;
			}
		}
		/* no good value found */
		return (EINVAL);
	found2:
		DPRINTF(("uirda_set_params: speed hdr=0x%02x\n", hdr));
		;
	}
	if (p->maxsize != sc->sc_params.maxsize) {
		if (p->maxsize > IRDA_MAX_FRAME_SIZE)
			return (EINVAL);
		sc->sc_params.maxsize = p->maxsize;
#if 0
		DPRINTF(("%s: new buffers, old size=%d\n", __func__,
			 sc->sc_params.maxsize));
		if (p->maxsize > 10000 || p < 0) /* XXX */
			return (EINVAL);

		/* Change the write buffer */
		mutex_enter(&sc->sc_wr_buf_lk);
		if (sc->sc_wr_buf != NULL)
			usbd_free_buffer(sc->sc_wr_xfer);
		sc->sc_wr_buf = usbd_alloc_buffer(sc->sc_wr_xfer, p->maxsize+1);
		mutex_exit(&sc->sc_wr_buf_lk);
		if (sc->sc_wr_buf == NULL)
			return (ENOMEM);

		/* Change the read buffer */
		mutex_enter(&sc->sc_rd_buf_lk);
		usbd_abort_pipe(sc->sc_rd_pipe);
		if (sc->sc_rd_buf != NULL)
			usbd_free_buffer(sc->sc_rd_xfer);
		sc->sc_rd_buf = usbd_alloc_buffer(sc->sc_rd_xfer, p->maxsize+1);
		sc->sc_rd_count = 0;
		if (sc->sc_rd_buf == NULL) {
			mutex_exit(&sc->sc_rd_buf_lk);
			return (ENOMEM);
		}
		sc->sc_params.maxsize = p->maxsize;
		err = uirda_start_read(sc); /* XXX check */
		mutex_exit(&sc->sc_rd_buf_lk);
#endif
	}
	if (hdr != 0 && hdr != sc->sc_wr_hdr) {
		/*
		 * A change has occurred, transmit a 0 length frame with
		 * the new settings.  The 0 length frame is not sent to the
		 * device.
		 */
		DPRINTF(("%s: sc=%p setting header 0x%02x\n",
			 __func__, sc, hdr));
		sc->sc_wr_hdr = hdr;
		mutex_enter(&sc->sc_wr_buf_lk);
		sc->sc_wr_buf[0] = hdr;
		n = UIRDA_OUTPUT_HEADER_SIZE;
		err = usbd_bulk_transfer(sc->sc_wr_xfer, sc->sc_wr_pipe,
			  USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
			  UIRDA_WR_TIMEOUT, sc->sc_wr_buf, &n, "uirdast");
		if (err) {
			aprint_error_dev(sc->sc_dev, "set failed, err=%d\n",
			    err);
			usbd_clear_endpoint_stall(sc->sc_wr_pipe);
		}
		mutex_exit(&sc->sc_wr_buf_lk);
	}

	sc->sc_params = *p;

	return (0);
}

int
uirda_get_speeds(void *h, int *speeds)
{
	struct uirda_softc *sc = h;
	u_int isp;
	u_int usp;

	DPRINTF(("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return (EIO);

	usp = UGETW(sc->sc_irdadesc.wBaudRate);
	isp = 0;
	if (usp & UI_BR_4000000) isp |= IRDA_SPEED_4000000;
	if (usp & UI_BR_1152000) isp |= IRDA_SPEED_1152000;
	if (usp & UI_BR_576000)  isp |= IRDA_SPEED_576000;
	if (usp & UI_BR_115200)  isp |= IRDA_SPEED_115200;
	if (usp & UI_BR_57600)   isp |= IRDA_SPEED_57600;
	if (usp & UI_BR_38400)   isp |= IRDA_SPEED_38400;
	if (usp & UI_BR_19200)   isp |= IRDA_SPEED_19200;
	if (usp & UI_BR_9600)    isp |= IRDA_SPEED_9600;
	if (usp & UI_BR_2400)    isp |= IRDA_SPEED_2400;
	*speeds = isp;
	DPRINTF(("%s: speeds = 0x%x\n", __func__, isp));
	return (0);
}

int
uirda_get_turnarounds(void *h, int *turnarounds)
{
	struct uirda_softc *sc = h;
	u_int ita;
	u_int uta;

	DPRINTF(("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return (EIO);

	uta = sc->sc_irdadesc.bmMinTurnaroundTime;
	ita = 0;
	if (uta & UI_TA_0)     ita |= IRDA_TURNT_0;
	if (uta & UI_TA_10)    ita |= IRDA_TURNT_10;
	if (uta & UI_TA_50)    ita |= IRDA_TURNT_50;
	if (uta & UI_TA_100)   ita |= IRDA_TURNT_100;
	if (uta & UI_TA_500)   ita |= IRDA_TURNT_500;
	if (uta & UI_TA_1000)  ita |= IRDA_TURNT_1000;
	if (uta & UI_TA_5000)  ita |= IRDA_TURNT_5000;
	if (uta & UI_TA_10000) ita |= IRDA_TURNT_10000;
	*turnarounds = ita;
	return (0);
}

void
uirda_rd_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
	    usbd_status status)
{
	struct uirda_softc *sc = priv;
	u_int32_t size;

	DPRINTFN(1,("%s: sc=%p\n", __func__, sc));

	if (status == USBD_CANCELLED) /* this is normal */
		return;
	if (status) {
		size = sc->sc_hdszi;
		sc->sc_rd_err = 1;
	} else {
		usbd_get_xfer_status(xfer, NULL, NULL, &size, NULL);
	}
	DPRINTFN(1,("%s: sc=%p size=%u, err=%d\n", __func__, sc, size,
		    sc->sc_rd_err));
	sc->sc_rd_count = size;
	wakeup(&sc->sc_rd_count); /* XXX should use flag */
	selnotify(&sc->sc_rd_sel, 0, 0);
}

usbd_status
uirda_start_read(struct uirda_softc *sc)
{
	usbd_status err;

	DPRINTFN(1,("%s: sc=%p, size=%d\n", __func__, sc,
		    sc->sc_params.maxsize + UIRDA_INPUT_HEADER_SIZE));

	if (sc->sc_dying)
		return (USBD_IOERROR);

	if (sc->sc_rd_err) {
		sc->sc_rd_err = 0;
		DPRINTF(("uirda_start_read: clear stall\n"));
		usbd_clear_endpoint_stall(sc->sc_rd_pipe);
	}

	usbd_setup_xfer(sc->sc_rd_xfer, sc->sc_rd_pipe, sc, sc->sc_rd_buf,
			sc->sc_params.maxsize + sc->sc_hdszi,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, uirda_rd_cb);
	err = usbd_transfer(sc->sc_rd_xfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("uirda_start_read: err=%d\n", err));
		return (err);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_get_class_desc(usbd_device_handle dev, int type, int index, int len, void *desc)
{
	usb_device_request_t req;

	DPRINTFN(3,("usbd_get_desc: type=%d, index=%d, len=%d\n",
		    type, index, len));

	req.bmRequestType = 0xa1; /* XXX ? */
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, type, index);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, desc));
}
