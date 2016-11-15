/*	$NetBSD: udsir.c,v 1.1 2013/05/28 12:03:26 kiyohara Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Sainty <David.Sainty@dtsp.co.nz>
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
__KERNEL_RCSID(0, "$NetBSD: udsir.c,v 1.1 2013/05/28 12:03:26 kiyohara Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/ir/ir.h>
#include <dev/ir/irdaio.h>
#include <dev/ir/irframevar.h>
#include <dev/ir/sir.h>

#ifdef UDSIR_DEBUG
#define DPRINTFN(n,x)	if (udsirdebug > (n)) printf x
int	udsirdebug = 0;
#else
#define DPRINTFN(n,x)
#endif

/* Max size with framing. */
#define MAX_UDSIR_OUTPUT_FRAME	(2 * IRDA_MAX_FRAME_SIZE + IRDA_MAX_EBOFS + 4)

struct udsir_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	uint8_t			*sc_ur_buf; /* Unencapsulated frame */
	u_int			sc_ur_framelen;

	uint8_t			*sc_rd_buf; /* Raw incoming data stream */
	int			sc_rd_maxpsz;
	size_t			sc_rd_index;
	int			sc_rd_addr;
	usbd_pipe_handle	sc_rd_pipe;
	usbd_xfer_handle	sc_rd_xfer;
	u_int			sc_rd_count;
	int			sc_rd_readinprogress;
	int			sc_rd_expectdataticks;
	u_char			sc_rd_err;
	struct framestate	sc_framestate;
	struct lwp		*sc_thread;
	struct selinfo		sc_rd_sel;

	uint8_t			*sc_wr_buf;
	int			sc_wr_maxpsz;
	int			sc_wr_addr;
	int			sc_wr_stalewrite;
	usbd_xfer_handle	sc_wr_xfer;
	usbd_pipe_handle	sc_wr_pipe;
	struct selinfo		sc_wr_sel;

	enum {
		udir_input, /* Receiving data */
		udir_output, /* Transmitting data */
		udir_stalled, /* Error preventing data flow */
		udir_idle /* Neither receiving nor transmitting */
	} sc_direction;

	device_t		sc_child;
	struct irda_params	sc_params;

	int			sc_refcnt;
	char			sc_closing;
	char			sc_dying;
};

/* True if we cannot safely read data from the device */
#define UDSIR_BLOCK_RX_DATA(sc) ((sc)->sc_ur_framelen != 0)

#define UDSIR_WR_TIMEOUT 200

static int udsir_match(device_t, cfdata_t, void *);
static void udsir_attach(device_t, device_t, void *);
static int udsir_detach(device_t, int);
static void udsir_childdet(device_t, device_t);
static int udsir_activate(device_t, enum devact);

static int udsir_open(void *, int, int, struct lwp *);
static int udsir_close(void *, int, int, struct lwp *);
static int udsir_read(void *, struct uio *, int);
static int udsir_write(void *, struct uio *, int);
static int udsir_poll(void *, int, struct lwp *);
static int udsir_kqfilter(void *, struct knote *);
static int udsir_set_params(void *, struct irda_params *);
static int udsir_get_speeds(void *, int *);
static int udsir_get_turnarounds(void *, int *);

static void filt_udsirrdetach(struct knote *);
static int filt_udsirread(struct knote *, long);
static void filt_udsirwdetach(struct knote *);
static int filt_udsirwrite(struct knote *, long);

static void udsir_thread(void *);

#ifdef UDSIR_DEBUG
static void udsir_dumpdata(uint8_t const *, size_t, char const *);
#endif
static int deframe_rd_ur(struct udsir_softc *);
static void udsir_periodic(struct udsir_softc *);
static void udsir_rd_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static usbd_status udsir_start_read(struct udsir_softc *);

CFATTACH_DECL2_NEW(udsir, sizeof(struct udsir_softc),
    udsir_match, udsir_attach, udsir_detach,
    udsir_activate, NULL, udsir_childdet);

static struct irframe_methods const udsir_methods = {
    udsir_open, udsir_close, udsir_read, udsir_write, udsir_poll,
    udsir_kqfilter, udsir_set_params, udsir_get_speeds, udsir_get_turnarounds,
};

static int
udsir_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	DPRINTFN(50, ("udsir_match\n"));

	if (uaa->vendor == USB_VENDOR_KINGSUN &&
	    uaa->product == USB_PRODUCT_KINGSUN_IRDA)
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

static void
udsir_attach(device_t parent, device_t self, void *aux)
{
	struct udsir_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface = uaa->iface;
	char *devinfop;
	usb_endpoint_descriptor_t *ed;
	uint8_t epcount;
	int i;
	struct ir_attach_args ia;

	DPRINTFN(10, ("udsir_attach: sc=%p\n", sc));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

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
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_rd_addr = ed->bEndpointAddress;
			sc->sc_rd_maxpsz = UGETW(ed->wMaxPacketSize);
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_wr_addr = ed->bEndpointAddress;
			sc->sc_wr_maxpsz = UGETW(ed->wMaxPacketSize);
		}
	}
	if (sc->sc_rd_addr == -1 || sc->sc_wr_addr == -1) {
		aprint_error_dev(self, "missing endpoint\n");
		return;
	}

	DPRINTFN(10, ("udsir_attach: %p\n", sc->sc_udev));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	ia.ia_type = IR_TYPE_IRFRAME;
	ia.ia_methods = &udsir_methods;
	ia.ia_handle = sc;

	sc->sc_child = config_found(self, &ia, ir_print);
	selinit(&sc->sc_rd_sel);
	selinit(&sc->sc_wr_sel);

	return;
}

static int
udsir_detach(device_t self, int flags)
{
	struct udsir_softc *sc = device_private(self);
	int s;
	int rv = 0;

	DPRINTFN(0, ("udsir_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_closing = sc->sc_dying = 1;

	wakeup(&sc->sc_thread);

	while (sc->sc_thread != NULL)
		tsleep(&sc->sc_closing, PWAIT, "usircl", 0);

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
	wakeup(&sc->sc_ur_framelen);
	wakeup(&sc->sc_wr_buf);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_waitold(sc->sc_dev);
	}
	splx(s);

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	seldestroy(&sc->sc_rd_sel);
	seldestroy(&sc->sc_wr_sel);

	return rv;
}

static void
udsir_childdet(device_t self, device_t child)
{
	struct udsir_softc *sc = device_private(self);

	KASSERT(sc->sc_child == child);
	sc->sc_child = NULL;
}

static int
udsir_activate(device_t self, enum devact act)
{
	struct udsir_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/* ARGSUSED */
static int
udsir_open(void *h, int flag, int mode, struct lwp *l)
{
	struct udsir_softc *sc = h;
	int error;
	usbd_status err;

	DPRINTFN(0, ("%s: sc=%p\n", __func__, sc));

	err = usbd_open_pipe(sc->sc_iface, sc->sc_rd_addr, 0, &sc->sc_rd_pipe);
	if (err != USBD_NORMAL_COMPLETION) {
		error = EIO;
		goto bad1;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_wr_addr, 0, &sc->sc_wr_pipe);
	if (err != USBD_NORMAL_COMPLETION) {
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
	sc->sc_rd_buf = usbd_alloc_buffer(sc->sc_rd_xfer, sc->sc_rd_maxpsz);
	if (sc->sc_rd_buf == NULL) {
		error = ENOMEM;
		goto bad5;
	}
	sc->sc_wr_buf = usbd_alloc_buffer(sc->sc_wr_xfer, IRDA_MAX_FRAME_SIZE);
	if (sc->sc_wr_buf == NULL) {
		error = ENOMEM;
		goto bad5;
	}
	sc->sc_ur_buf = malloc(IRDA_MAX_FRAME_SIZE, M_USBDEV, M_NOWAIT);
	if (sc->sc_ur_buf == NULL) {
		error = ENOMEM;
		goto bad5;
	}

	sc->sc_rd_index = sc->sc_rd_count = 0;
	sc->sc_closing = 0;
	sc->sc_rd_readinprogress = 0;
	sc->sc_rd_expectdataticks = 0;
	sc->sc_ur_framelen = 0;
	sc->sc_rd_err = 0;
	sc->sc_wr_stalewrite = 0;
	sc->sc_direction = udir_idle;
	sc->sc_params.speed = 0;
	sc->sc_params.ebofs = 0;
	sc->sc_params.maxsize = min(sc->sc_rd_maxpsz, sc->sc_wr_maxpsz);

	deframe_init(&sc->sc_framestate, sc->sc_ur_buf, IRDA_MAX_FRAME_SIZE);

	/* Increment reference for thread */
	sc->sc_refcnt++;

	error = kthread_create(PRI_NONE, 0, NULL, udsir_thread, sc,
	    &sc->sc_thread, "%s", device_xname(sc->sc_dev));
	if (error) {
		sc->sc_refcnt--;
		goto bad5;
	}

	return 0;

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
	return error;
}

/* ARGSUSED */
static int
udsir_close(void *h, int flag, int mode, struct lwp *l)
{
	struct udsir_softc *sc = h;

	DPRINTFN(0, ("%s: sc=%p\n", __func__, sc));

	sc->sc_refcnt++;

	sc->sc_rd_readinprogress = 1;
	sc->sc_closing = 1;

	wakeup(&sc->sc_thread);

	while (sc->sc_thread != NULL)
		tsleep(&sc->sc_closing, PWAIT, "usircl", 0);

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
	if (sc->sc_ur_buf != NULL) {
		free(sc->sc_ur_buf, M_USBDEV);
		sc->sc_ur_buf = NULL;
	}

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return 0;
}

/* ARGSUSED */
static int
udsir_read(void *h, struct uio *uio, int flag)
{
	struct udsir_softc *sc = h;
	int s;
	int error;
	u_int uframelen;

	DPRINTFN(1, ("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return EIO;

#ifdef DIAGNOSTIC
	if (sc->sc_rd_buf == NULL)
		return EINVAL;
#endif

	sc->sc_refcnt++;

	if (!sc->sc_rd_readinprogress && !UDSIR_BLOCK_RX_DATA(sc))
		/* Possibly wake up polling thread */
		wakeup(&sc->sc_thread);

	do {
		s = splusb();
		while (sc->sc_ur_framelen == 0) {
			DPRINTFN(5, ("%s: calling tsleep()\n", __func__));
			error = tsleep(&sc->sc_ur_framelen, PZERO | PCATCH,
				       "usirrd", 0);
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				splx(s);
				DPRINTFN(0, ("%s: tsleep() = %d\n",
					     __func__, error));
				goto ret;
			}
		}
		splx(s);

		uframelen = sc->sc_ur_framelen;
		DPRINTFN(1, ("%s: sc=%p framelen=%u, hdr=0x%02x\n",
			     __func__, sc, uframelen, sc->sc_ur_buf[0]));
		if (uframelen > uio->uio_resid)
			error = EINVAL;
		else
			error = uiomove(sc->sc_ur_buf, uframelen, uio);
		sc->sc_ur_framelen = 0;

		if (deframe_rd_ur(sc) == 0 && uframelen > 0) {
			/*
			 * Need to wait for another read to obtain a
			 * complete frame...  If we also obtained
			 * actual data, wake up the possibly sleeping
			 * thread immediately...
			 */
			wakeup(&sc->sc_thread);
		}
	} while (uframelen == 0);

	DPRINTFN(1, ("%s: return %d\n", __func__, error));

 ret:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	return error;
}

/* ARGSUSED */
static int
udsir_write(void *h, struct uio *uio, int flag)
{
	struct udsir_softc *sc = h;
	usbd_status err;
	uint32_t wrlen;
	int error, sirlength;
	uint8_t *wrbuf;
	int s;

	DPRINTFN(1, ("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return EIO;

#ifdef DIAGNOSTIC
	if (sc->sc_wr_buf == NULL)
		return EINVAL;
#endif

	wrlen = uio->uio_resid;
	if (wrlen > sc->sc_wr_maxpsz)
		return EINVAL;

	sc->sc_refcnt++;

	if (!UDSIR_BLOCK_RX_DATA(sc)) {
		/*
		 * If reads are not blocked, determine what action we
		 * should potentially take...
		 */
		if (sc->sc_direction == udir_output) {
			/*
			 * If the last operation was an output, wait for the
			 * polling thread to check for incoming data.
			 */
			sc->sc_wr_stalewrite = 1;
			wakeup(&sc->sc_thread);
		} else if (!sc->sc_rd_readinprogress &&
			   (sc->sc_direction == udir_idle ||
			    sc->sc_direction == udir_input)) {
			/* If idle, check for input before outputting */
			udsir_start_read(sc);
		}
	}

	s = splusb();
	while (sc->sc_wr_stalewrite ||
	       (sc->sc_direction != udir_output &&
		sc->sc_direction != udir_idle)) {
		DPRINTFN(5, ("%s: sc=%p stalewrite=%d direction=%d, "
			     "calling tsleep()\n",
			     __func__, sc, sc->sc_wr_stalewrite,
			     sc->sc_direction));
		error = tsleep(&sc->sc_wr_buf, PZERO | PCATCH, "usirwr", 0);
		if (sc->sc_dying)
			error = EIO;
		if (error) {
			splx(s);
			DPRINTFN(0, ("%s: tsleep() = %d\n", __func__, error));
			goto ret;
		}
	}
	splx(s);

	wrbuf = sc->sc_wr_buf;

	sirlength = irda_sir_frame(wrbuf, MAX_UDSIR_OUTPUT_FRAME,
	    uio, sc->sc_params.ebofs);
	if (sirlength < 0)
		error = -sirlength;
	else {
		uint32_t btlen;

		DPRINTFN(1, ("%s: transfer %u bytes\n",
			     __func__, (unsigned int)wrlen));

		btlen = sirlength;

		sc->sc_direction = udir_output;

#ifdef UDSIR_DEBUG
		if (udsirdebug >= 20)
			udsir_dumpdata(wrbuf, btlen, __func__);
#endif

		err = usbd_intr_transfer(sc->sc_wr_xfer, sc->sc_wr_pipe,
				USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
				UDSIR_WR_TIMEOUT, wrbuf, &btlen, "udsiwr");
		DPRINTFN(2, ("%s: err=%d\n", __func__, err));
		if (err != USBD_NORMAL_COMPLETION) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
		} else
			error = 0;
	}

 ret:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	DPRINTFN(1, ("%s: sc=%p done\n", __func__, sc));
	return error;
}

static int
udsir_poll(void *h, int events, struct lwp *l)
{
	struct udsir_softc *sc = h;
	int revents = 0;

	DPRINTFN(1, ("%s: sc=%p\n", __func__, sc));

	if (events & (POLLOUT | POLLWRNORM)) {
		if (sc->sc_direction != udir_input)
			revents |= events & (POLLOUT | POLLWRNORM);
		else {
			DPRINTFN(2, ("%s: recording write select\n", __func__));
			selrecord(l, &sc->sc_wr_sel);
		}
	}

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_ur_framelen != 0) {
			DPRINTFN(2, ("%s: have data\n", __func__));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			DPRINTFN(2, ("%s: recording read select\n", __func__));
			selrecord(l, &sc->sc_rd_sel);
		}
	}

	return revents;
}

static const struct filterops udsirread_filtops =
	{ 1, NULL, filt_udsirrdetach, filt_udsirread };
static const struct filterops udsirwrite_filtops =
	{ 1, NULL, filt_udsirwdetach, filt_udsirwrite };

static int
udsir_kqfilter(void *h, struct knote *kn)
{
	struct udsir_softc *sc = h;
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rd_sel.sel_klist;
		kn->kn_fop = &udsirread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->sc_wr_sel.sel_klist;
		kn->kn_fop = &udsirwrite_filtops;
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

static int
udsir_set_params(void *h, struct irda_params *p)
{
	struct udsir_softc *sc = h;

	DPRINTFN(0, ("%s: sc=%p, speed=%d ebofs=%d maxsize=%d\n",
		     __func__, sc, p->speed, p->ebofs, p->maxsize));

	if (sc->sc_dying)
		return EIO;

	if (p->speed != 9600)
		return EINVAL;

	if (p->maxsize != sc->sc_params.maxsize) {
		if (p->maxsize > min(sc->sc_rd_maxpsz, sc->sc_wr_maxpsz))
			return EINVAL;
		sc->sc_params.maxsize = p->maxsize;
	}

	sc->sc_params = *p;

	return 0;
}

static int
udsir_get_speeds(void *h, int *speeds)
{
	struct udsir_softc *sc = h;

	DPRINTFN(0, ("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return EIO;

	/* Support only 9600bps now. */
	*speeds = IRDA_SPEED_9600;

	return 0;
}

static int
udsir_get_turnarounds(void *h, int *turnarounds)
{
	struct udsir_softc *sc = h;

	DPRINTFN(0, ("%s: sc=%p\n", __func__, sc));

	if (sc->sc_dying)
		return EIO;

	/*
	 * Documentation is on the light side with respect to
	 * turnaround time for this device.
	 */
	*turnarounds = IRDA_TURNT_10000;

	return 0;
}

static void
filt_udsirrdetach(struct knote *kn)
{
	struct udsir_softc *sc = kn->kn_hook;
	int s;

	s = splusb();
	SLIST_REMOVE(&sc->sc_rd_sel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

/* ARGSUSED */
static int
filt_udsirread(struct knote *kn, long hint)
{
	struct udsir_softc *sc = kn->kn_hook;

	kn->kn_data = sc->sc_ur_framelen;
	return (kn->kn_data > 0);
}

static void
filt_udsirwdetach(struct knote *kn)
{
	struct udsir_softc *sc = kn->kn_hook;
	int s;

	s = splusb();
	SLIST_REMOVE(&sc->sc_wr_sel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

/* ARGSUSED */
static int
filt_udsirwrite(struct knote *kn, long hint)
{
	struct udsir_softc *sc = kn->kn_hook;

	kn->kn_data = 0;
	return (sc->sc_direction != udir_input);
}


static void
udsir_thread(void *arg)
{
	struct udsir_softc *sc = arg;
	int error;

	DPRINTFN(20, ("%s: starting polling thread\n", __func__));

	while (!sc->sc_closing) {
		if (!sc->sc_rd_readinprogress && !UDSIR_BLOCK_RX_DATA(sc))
			udsir_periodic(sc);

		if (!sc->sc_closing) {
			error = tsleep(&sc->sc_thread, PWAIT, "udsir", hz / 10);
			if (error == EWOULDBLOCK &&
			    sc->sc_rd_expectdataticks > 0)
				/*
				 * After a timeout decrement the tick
				 * counter within which time we expect
				 * data to arrive if we are receiving
				 * data...
				 */
				sc->sc_rd_expectdataticks--;
		}
	}

	DPRINTFN(20, ("%s: exiting polling thread\n", __func__));

	sc->sc_thread = NULL;

	wakeup(&sc->sc_closing);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	kthread_exit(0);
}

#ifdef UDSIR_DEBUG
static void
udsir_dumpdata(uint8_t const *data, size_t dlen, char const *desc)
{
	size_t bdindex;

	printf("%s: (%lx)", desc, (unsigned long)dlen);
	for (bdindex = 0; bdindex < dlen; bdindex++)
		printf(" %02x", (unsigned int)data[bdindex]);
	printf("\n");
}
#endif

/* Returns 0 if more data required, 1 if a complete frame was extracted */
static int
deframe_rd_ur(struct udsir_softc *sc)
{

	if (sc->sc_rd_index == 0) {
		KASSERT(sc->sc_rd_count == sc->sc_rd_maxpsz);
		/* valid count */
		sc->sc_rd_count = sc->sc_rd_buf[sc->sc_rd_index++] + 1;
		KASSERT(sc->sc_rd_count < sc->sc_rd_maxpsz);
	}

	while (sc->sc_rd_index < sc->sc_rd_count) {
		uint8_t const *buf;
		size_t buflen;
		enum frameresult fresult;

		buf = &sc->sc_rd_buf[sc->sc_rd_index];
		buflen = sc->sc_rd_count - sc->sc_rd_index;

		fresult = deframe_process(&sc->sc_framestate, &buf, &buflen);

		sc->sc_rd_index = sc->sc_rd_count - buflen;

		DPRINTFN(1,("%s: result=%d\n", __func__, (int)fresult));

		switch (fresult) {
		case FR_IDLE:
		case FR_INPROGRESS:
		case FR_FRAMEBADFCS:
		case FR_FRAMEMALFORMED:
		case FR_BUFFEROVERRUN:
			break;
		case FR_FRAMEOK:
			sc->sc_ur_framelen = sc->sc_framestate.bufindex;
			wakeup(&sc->sc_ur_framelen); /* XXX should use flag */
			selnotify(&sc->sc_rd_sel, 0, 0);
			return 1;
		}
	}

	/* Reset indices into USB-side buffer */
	sc->sc_rd_index = sc->sc_rd_count = 0;

	return 0;
}

/*
 * Direction transitions:
 *
 * udsir_periodic() can switch the direction from:
 *
 *	output -> idle
 *	output -> stalled
 *	stalled -> idle
 *	idle -> input
 *
 * udsir_rd_cb() can switch the direction from:
 *
 *	input -> stalled
 *	input -> idle
 *
 * udsir_write() can switch the direction from:
 *
 *	idle -> output
 */
static void
udsir_periodic(struct udsir_softc *sc)
{

	DPRINTFN(60, ("%s: direction = %d\n", __func__, sc->sc_direction));

	if (sc->sc_wr_stalewrite && sc->sc_direction == udir_idle) {
		/*
		 * In a stale write case, we need to check if the
		 * write has completed.  Once that has happened, the
		 * write is no longer stale.
		 *
		 * But note that we may immediately start a read poll...
		 */
		sc->sc_wr_stalewrite = 0;
		wakeup(&sc->sc_wr_buf);
	}

	if (!sc->sc_rd_readinprogress &&
	    (sc->sc_direction == udir_idle ||
	     sc->sc_direction == udir_input))
		/* Do a read poll if appropriate... */
		udsir_start_read(sc);
}

static void
udsir_rd_cb(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct udsir_softc *sc = priv;
	uint32_t size;

	DPRINTFN(60, ("%s: sc=%p\n", __func__, sc));

	/* Read is no longer in progress */
	sc->sc_rd_readinprogress = 0;

	if (status == USBD_CANCELLED || sc->sc_closing)	/* this is normal */
		return;
	if (status) {
		size = 0;
		sc->sc_rd_err = 1;

		if (sc->sc_direction == udir_input ||
		    sc->sc_direction == udir_idle) {
			/*
			 * Receive error, probably need to clear error
			 * condition.
			 */
			sc->sc_direction = udir_stalled;
		}
	} else
		usbd_get_xfer_status(xfer, NULL, NULL, &size, NULL);

	sc->sc_rd_index = 0;
	sc->sc_rd_count = size;

	DPRINTFN(((size > 0 || sc->sc_rd_err != 0) ? 20 : 60),
		 ("%s: sc=%p size=%u, err=%d\n",
		  __func__, sc, size, sc->sc_rd_err));

#ifdef UDSIR_DEBUG
	if (udsirdebug >= 20 && size > 0)
		udsir_dumpdata(sc->sc_rd_buf, size, __func__);
#endif

	if (deframe_rd_ur(sc) == 0) {
		if (!deframe_isclear(&sc->sc_framestate) && size == 0 &&
		    sc->sc_rd_expectdataticks == 0) {
			/*
			 * Expected data, but didn't get it
			 * within expected time...
			 */
			DPRINTFN(5,("%s: incoming packet timeout\n",
				    __func__));
			deframe_clear(&sc->sc_framestate);
		} else if (size > 0) {
			/*
			 * If we also received actual data, reset the
			 * data read timeout and wake up the possibly
			 * sleeping thread...
			 */
			sc->sc_rd_expectdataticks = 2;
			wakeup(&sc->sc_thread);
		}
	}

	/*
	 * Check if incoming data has stopped, or that we cannot
	 * safely read any more data.  In the case of the latter we
	 * must switch to idle so that a write will not block...
	 */
	if (sc->sc_direction == udir_input &&
	    ((size == 0 && sc->sc_rd_expectdataticks == 0) ||
	     UDSIR_BLOCK_RX_DATA(sc))) {
		DPRINTFN(8, ("%s: idling on packet timeout, "
			     "complete frame, or no data\n", __func__));
		sc->sc_direction = udir_idle;

		/* Wake up for possible output */
		wakeup(&sc->sc_wr_buf);
		selnotify(&sc->sc_wr_sel, 0, 0);
	}
}

static usbd_status
udsir_start_read(struct udsir_softc *sc)
{
	usbd_status err;

	DPRINTFN(60, ("%s: sc=%p, size=%d\n", __func__, sc, sc->sc_rd_maxpsz));

	if (sc->sc_dying)
		return USBD_IOERROR;

	if (UDSIR_BLOCK_RX_DATA(sc) || deframe_rd_ur(sc)) {
		/*
		 * Can't start reading just yet.  Since we aren't
		 * going to start a read, have to switch direction to
		 * idle.
		 */
		sc->sc_direction = udir_idle;
		return USBD_NORMAL_COMPLETION;
	}

	/* Starting a read... */
	sc->sc_rd_readinprogress = 1;
	sc->sc_direction = udir_input;

	if (sc->sc_rd_err) {
		sc->sc_rd_err = 0;
		DPRINTFN(0, ("%s: clear stall\n", __func__));
		usbd_clear_endpoint_stall(sc->sc_rd_pipe);
	}

	usbd_setup_xfer(sc->sc_rd_xfer, sc->sc_rd_pipe, sc, sc->sc_rd_buf,
	    sc->sc_rd_maxpsz, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, udsir_rd_cb);
	err = usbd_transfer(sc->sc_rd_xfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTFN(0, ("%s: err=%d\n", __func__, (int)err));
		return err;
	}
	return USBD_NORMAL_COMPLETION;
}
