/*	$NetBSD: ugen.c,v 1.126 2014/09/20 08:45:23 gson Exp $	*/

/*
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Copyright (c) 2006 BBN Technologies Corp.  All rights reserved.
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and the Department of the Interior National Business
 * Center under agreement number NBCHC050166.
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
__KERNEL_RCSID(0, "$NetBSD: ugen.c,v 1.126 2014/09/20 08:45:23 gson Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_usb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#ifdef UGEN_DEBUG
#define DPRINTF(x)	if (ugendebug) printf x
#define DPRINTFN(n,x)	if (ugendebug>(n)) printf x
int	ugendebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define	UGEN_CHUNK	128	/* chunk size for read */
#define	UGEN_IBSIZE	1020	/* buffer size */
#define	UGEN_BBSIZE	1024

#define UGEN_NISOREQS	4	/* number of outstanding xfer requests */
#define UGEN_NISORFRMS	8	/* number of transactions per req */
#define UGEN_NISOFRAMES	(UGEN_NISORFRMS * UGEN_NISOREQS)

#define UGEN_BULK_RA_WB_BUFSIZE	16384		/* default buffer size */
#define UGEN_BULK_RA_WB_BUFMAX	(1 << 20)	/* maximum allowed buffer */

struct isoreq {
	struct ugen_endpoint *sce;
	usbd_xfer_handle xfer;
	void *dmabuf;
	u_int16_t sizes[UGEN_NISORFRMS];
};

struct ugen_endpoint {
	struct ugen_softc *sc;
	usb_endpoint_descriptor_t *edesc;
	usbd_interface_handle iface;
	int state;
#define	UGEN_ASLP	0x02	/* waiting for data */
#define UGEN_SHORT_OK	0x04	/* short xfers are OK */
#define UGEN_BULK_RA	0x08	/* in bulk read-ahead mode */
#define UGEN_BULK_WB	0x10	/* in bulk write-behind mode */
#define UGEN_RA_WB_STOP	0x20	/* RA/WB xfer is stopped (buffer full/empty) */
	usbd_pipe_handle pipeh;
	struct clist q;
	u_char *ibuf;		/* start of buffer (circular for isoc) */
	u_char *fill;		/* location for input (isoc) */
	u_char *limit;		/* end of circular buffer (isoc) */
	u_char *cur;		/* current read location (isoc) */
	u_int32_t timeout;
	u_int32_t ra_wb_bufsize; /* requested size for RA/WB buffer */
	u_int32_t ra_wb_reqsize; /* requested xfer length for RA/WB */
	u_int32_t ra_wb_used;	 /* how much is in buffer */
	u_int32_t ra_wb_xferlen; /* current xfer length for RA/WB */
	usbd_xfer_handle ra_wb_xfer;
	struct isoreq isoreqs[UGEN_NISOREQS];
	/* Keep these last; we don't overwrite them in ugen_set_config() */
#define UGEN_ENDPOINT_NONZERO_CRUFT	offsetof(struct ugen_endpoint, rsel)
	struct selinfo rsel;
	kcondvar_t cv;
};

struct ugen_softc {
	device_t sc_dev;		/* base device */
	usbd_device_handle sc_udev;

	kmutex_t		sc_lock;
	kcondvar_t		sc_detach_cv;

	char sc_is_open[USB_MAX_ENDPOINTS];
	struct ugen_endpoint sc_endpoints[USB_MAX_ENDPOINTS][2];
#define OUT 0
#define IN  1

	int sc_refcnt;
	char sc_buffer[UGEN_BBSIZE];
	u_char sc_dying;
};

dev_type_open(ugenopen);
dev_type_close(ugenclose);
dev_type_read(ugenread);
dev_type_write(ugenwrite);
dev_type_ioctl(ugenioctl);
dev_type_poll(ugenpoll);
dev_type_kqfilter(ugenkqfilter);

const struct cdevsw ugen_cdevsw = {
	.d_open = ugenopen,
	.d_close = ugenclose,
	.d_read = ugenread,
	.d_write = ugenwrite,
	.d_ioctl = ugenioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = ugenpoll,
	.d_mmap = nommap,
	.d_kqfilter = ugenkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};

Static void ugenintr(usbd_xfer_handle xfer, usbd_private_handle addr,
		     usbd_status status);
Static void ugen_isoc_rintr(usbd_xfer_handle xfer, usbd_private_handle addr,
			    usbd_status status);
Static void ugen_bulkra_intr(usbd_xfer_handle xfer, usbd_private_handle addr,
			     usbd_status status);
Static void ugen_bulkwb_intr(usbd_xfer_handle xfer, usbd_private_handle addr,
			     usbd_status status);
Static int ugen_do_read(struct ugen_softc *, int, struct uio *, int);
Static int ugen_do_write(struct ugen_softc *, int, struct uio *, int);
Static int ugen_do_ioctl(struct ugen_softc *, int, u_long,
			 void *, int, struct lwp *);
Static int ugen_set_config(struct ugen_softc *sc, int configno);
Static usb_config_descriptor_t *ugen_get_cdesc(struct ugen_softc *sc,
					       int index, int *lenp);
Static usbd_status ugen_set_interface(struct ugen_softc *, int, int);
Static int ugen_get_alt_index(struct ugen_softc *sc, int ifaceidx);

#define UGENUNIT(n) ((minor(n) >> 4) & 0xf)
#define UGENENDPOINT(n) (minor(n) & 0xf)
#define UGENDEV(u, e) (makedev(0, ((u) << 4) | (e)))

int             ugen_match(device_t, cfdata_t, void *);
void            ugen_attach(device_t, device_t, void *);
int             ugen_detach(device_t, int);
int             ugen_activate(device_t, enum devact);
extern struct cfdriver ugen_cd;
CFATTACH_DECL_NEW(ugen, sizeof(struct ugen_softc), ugen_match, ugen_attach, ugen_detach, ugen_activate);

/* toggle to control attach priority. -1 means "let autoconf decide" */
int ugen_override = -1;

int
ugen_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	int override;

	if (ugen_override != -1)
		override = ugen_override;
	else
		override = match->cf_flags & 1;

	if (override)
		return (UMATCH_HIGHEST);
	else if (uaa->usegeneric)
		return (UMATCH_GENERIC);
	else
		return (UMATCH_NONE);
}

void
ugen_attach(device_t parent, device_t self, void *aux)
{
	struct ugen_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle udev;
	char *devinfop;
	usbd_status err;
	int i, dir, conf;

	aprint_naive("\n");
	aprint_normal("\n");

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_USB);
	cv_init(&sc->sc_detach_cv, "ugendet");

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;
	sc->sc_udev = udev = uaa->device;

	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			struct ugen_endpoint *sce;

			sce = &sc->sc_endpoints[i][dir];
			selinit(&sce->rsel);
			cv_init(&sce->cv, "ugensce");
		}
	}

	/* First set configuration index 0, the default one for ugen. */
	err = usbd_set_config_index(udev, 0, 0);
	if (err) {
		aprint_error_dev(self,
		    "setting configuration index 0 failed\n");
		sc->sc_dying = 1;
		return;
	}
	conf = usbd_get_config_descriptor(udev)->bConfigurationValue;

	/* Set up all the local state for this configuration. */
	err = ugen_set_config(sc, conf);
	if (err) {
		aprint_error_dev(self, "setting configuration %d failed\n",
		    conf);
		sc->sc_dying = 1;
		return;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

Static int
ugen_set_config(struct ugen_softc *sc, int configno)
{
	usbd_device_handle dev = sc->sc_udev;
	usb_config_descriptor_t *cdesc;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	struct ugen_endpoint *sce;
	u_int8_t niface, nendpt;
	int ifaceno, endptno, endpt;
	usbd_status err;
	int dir, i;

	DPRINTFN(1,("ugen_set_config: %s to configno %d, sc=%p\n",
		    device_xname(sc->sc_dev), configno, sc));

	/*
	 * We start at 1, not 0, because we don't care whether the
	 * control endpoint is open or not. It is always present.
	 */
	for (endptno = 1; endptno < USB_MAX_ENDPOINTS; endptno++)
		if (sc->sc_is_open[endptno]) {
			DPRINTFN(1,
			     ("ugen_set_config: %s - endpoint %d is open\n",
			      device_xname(sc->sc_dev), endptno));
			return (USBD_IN_USE);
		}

	/* Avoid setting the current value. */
	cdesc = usbd_get_config_descriptor(dev);
	if (!cdesc || cdesc->bConfigurationValue != configno) {
		err = usbd_set_config_no(dev, configno, 1);
		if (err)
			return (err);
	}

	err = usbd_interface_count(dev, &niface);
	if (err)
		return (err);

	/* Clear out the old info, but leave the selinfo and cv initialised. */
	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			sce = &sc->sc_endpoints[i][dir];
			memset(sce, 0, UGEN_ENDPOINT_NONZERO_CRUFT);
		}
	}

	for (ifaceno = 0; ifaceno < niface; ifaceno++) {
		DPRINTFN(1,("ugen_set_config: ifaceno %d\n", ifaceno));
		err = usbd_device2interface_handle(dev, ifaceno, &iface);
		if (err)
			return (err);
		err = usbd_endpoint_count(iface, &nendpt);
		if (err)
			return (err);
		for (endptno = 0; endptno < nendpt; endptno++) {
			ed = usbd_interface2endpoint_descriptor(iface,endptno);
			KASSERT(ed != NULL);
			endpt = ed->bEndpointAddress;
			dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
			sce = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
			DPRINTFN(1,("ugen_set_config: endptno %d, endpt=0x%02x"
				    "(%d,%d), sce=%p\n",
				    endptno, endpt, UE_GET_ADDR(endpt),
				    UE_GET_DIR(endpt), sce));
			sce->sc = sc;
			sce->edesc = ed;
			sce->iface = iface;
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

int
ugenopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct ugen_softc *sc;
	int unit = UGENUNIT(dev);
	int endpt = UGENENDPOINT(dev);
	usb_endpoint_descriptor_t *edesc;
	struct ugen_endpoint *sce;
	int dir, isize;
	usbd_status err;
	usbd_xfer_handle xfer;
	void *tbuf;
	int i, j;

	sc = device_lookup_private(&ugen_cd, unit);
	if (sc == NULL)
		return ENXIO;

	DPRINTFN(5, ("ugenopen: flag=%d, mode=%d, unit=%d endpt=%d\n",
		     flag, mode, unit, endpt));

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	/* The control endpoint allows multiple opens. */
	if (endpt == USB_CONTROL_ENDPOINT) {
		sc->sc_is_open[USB_CONTROL_ENDPOINT] = 1;
		return (0);
	}

	if (sc->sc_is_open[endpt])
		return (EBUSY);

	/* Make sure there are pipes for all directions. */
	for (dir = OUT; dir <= IN; dir++) {
		if (flag & (dir == OUT ? FWRITE : FREAD)) {
			sce = &sc->sc_endpoints[endpt][dir];
			if (sce == 0 || sce->edesc == 0)
				return (ENXIO);
		}
	}

	/* Actually open the pipes. */
	/* XXX Should back out properly if it fails. */
	for (dir = OUT; dir <= IN; dir++) {
		if (!(flag & (dir == OUT ? FWRITE : FREAD)))
			continue;
		sce = &sc->sc_endpoints[endpt][dir];
		sce->state = 0;
		sce->timeout = USBD_NO_TIMEOUT;
		DPRINTFN(5, ("ugenopen: sc=%p, endpt=%d, dir=%d, sce=%p\n",
			     sc, endpt, dir, sce));
		edesc = sce->edesc;
		switch (edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			if (dir == OUT) {
				err = usbd_open_pipe(sce->iface,
				    edesc->bEndpointAddress, 0, &sce->pipeh);
				if (err)
					return (EIO);
				break;
			}
			isize = UGETW(edesc->wMaxPacketSize);
			if (isize == 0)	/* shouldn't happen */
				return (EINVAL);
			sce->ibuf = malloc(isize, M_USBDEV, M_WAITOK);
			DPRINTFN(5, ("ugenopen: intr endpt=%d,isize=%d\n",
				     endpt, isize));
			if (clalloc(&sce->q, UGEN_IBSIZE, 0) == -1) {
				free(sce->ibuf, M_USBDEV);
				sce->ibuf = NULL;
				return (ENOMEM);
			}
			err = usbd_open_pipe_intr(sce->iface,
				  edesc->bEndpointAddress,
				  USBD_SHORT_XFER_OK, &sce->pipeh, sce,
				  sce->ibuf, isize, ugenintr,
				  USBD_DEFAULT_INTERVAL);
			if (err) {
				clfree(&sce->q);
				free(sce->ibuf, M_USBDEV);
				sce->ibuf = NULL;
				return (EIO);
			}
			DPRINTFN(5, ("ugenopen: interrupt open done\n"));
			break;
		case UE_BULK:
			err = usbd_open_pipe(sce->iface,
				  edesc->bEndpointAddress, 0, &sce->pipeh);
			if (err)
				return (EIO);
			sce->ra_wb_bufsize = UGEN_BULK_RA_WB_BUFSIZE;
			/*
			 * Use request size for non-RA/WB transfers
			 * as the default.
			 */
			sce->ra_wb_reqsize = UGEN_BBSIZE;
			break;
		case UE_ISOCHRONOUS:
			if (dir == OUT)
				return (EINVAL);
			isize = UGETW(edesc->wMaxPacketSize);
			if (isize == 0)	/* shouldn't happen */
				return (EINVAL);
			sce->ibuf = malloc(isize * UGEN_NISOFRAMES,
				M_USBDEV, M_WAITOK);
			sce->cur = sce->fill = sce->ibuf;
			sce->limit = sce->ibuf + isize * UGEN_NISOFRAMES;
			DPRINTFN(5, ("ugenopen: isoc endpt=%d, isize=%d\n",
				     endpt, isize));
			err = usbd_open_pipe(sce->iface,
				  edesc->bEndpointAddress, 0, &sce->pipeh);
			if (err) {
				free(sce->ibuf, M_USBDEV);
				sce->ibuf = NULL;
				return (EIO);
			}
			for(i = 0; i < UGEN_NISOREQS; ++i) {
				sce->isoreqs[i].sce = sce;
				xfer = usbd_alloc_xfer(sc->sc_udev);
				if (xfer == 0)
					goto bad;
				sce->isoreqs[i].xfer = xfer;
				tbuf = usbd_alloc_buffer
					(xfer, isize * UGEN_NISORFRMS);
				if (tbuf == 0) {
					i++;
					goto bad;
				}
				sce->isoreqs[i].dmabuf = tbuf;
				for(j = 0; j < UGEN_NISORFRMS; ++j)
					sce->isoreqs[i].sizes[j] = isize;
				usbd_setup_isoc_xfer
					(xfer, sce->pipeh, &sce->isoreqs[i],
					 sce->isoreqs[i].sizes,
					 UGEN_NISORFRMS, USBD_NO_COPY,
					 ugen_isoc_rintr);
				(void)usbd_transfer(xfer);
			}
			DPRINTFN(5, ("ugenopen: isoc open done\n"));
			break;
		bad:
			while (--i >= 0) /* implicit buffer free */
				usbd_free_xfer(sce->isoreqs[i].xfer);
			usbd_close_pipe(sce->pipeh);
			sce->pipeh = NULL;
			free(sce->ibuf, M_USBDEV);
			sce->ibuf = NULL;
			return (ENOMEM);
		case UE_CONTROL:
			sce->timeout = USBD_DEFAULT_TIMEOUT;
			return (EINVAL);
		}
	}
	sc->sc_is_open[endpt] = 1;
	return (0);
}

int
ugenclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	struct ugen_endpoint *sce;
	int dir;
	int i;

	sc = device_lookup_private(& ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	DPRINTFN(5, ("ugenclose: flag=%d, mode=%d, unit=%d, endpt=%d\n",
		     flag, mode, UGENUNIT(dev), endpt));

#ifdef DIAGNOSTIC
	if (!sc->sc_is_open[endpt]) {
		printf("ugenclose: not open\n");
		return (EINVAL);
	}
#endif

	if (endpt == USB_CONTROL_ENDPOINT) {
		DPRINTFN(5, ("ugenclose: close control\n"));
		sc->sc_is_open[endpt] = 0;
		return (0);
	}

	for (dir = OUT; dir <= IN; dir++) {
		if (!(flag & (dir == OUT ? FWRITE : FREAD)))
			continue;
		sce = &sc->sc_endpoints[endpt][dir];
		if (sce == NULL || sce->pipeh == NULL)
			continue;
		DPRINTFN(5, ("ugenclose: endpt=%d dir=%d sce=%p\n",
			     endpt, dir, sce));

		usbd_abort_pipe(sce->pipeh);
		usbd_close_pipe(sce->pipeh);
		sce->pipeh = NULL;

		switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			ndflush(&sce->q, sce->q.c_cc);
			clfree(&sce->q);
			break;
		case UE_ISOCHRONOUS:
			for (i = 0; i < UGEN_NISOREQS; ++i)
				usbd_free_xfer(sce->isoreqs[i].xfer);
			break;
		case UE_BULK:
			if (sce->state & (UGEN_BULK_RA | UGEN_BULK_WB))
				/* ibuf freed below */
				usbd_free_xfer(sce->ra_wb_xfer);
			break;
		default:
			break;
		}

		if (sce->ibuf != NULL) {
			free(sce->ibuf, M_USBDEV);
			sce->ibuf = NULL;
		}
	}
	sc->sc_is_open[endpt] = 0;

	return (0);
}

Static int
ugen_do_read(struct ugen_softc *sc, int endpt, struct uio *uio, int flag)
{
	struct ugen_endpoint *sce = &sc->sc_endpoints[endpt][IN];
	u_int32_t n, tn;
	usbd_xfer_handle xfer;
	usbd_status err;
	int error = 0;

	DPRINTFN(5, ("%s: ugenread: %d\n", device_xname(sc->sc_dev), endpt));

	if (sc->sc_dying)
		return (EIO);

	if (endpt == USB_CONTROL_ENDPOINT)
		return (ENODEV);

#ifdef DIAGNOSTIC
	if (sce->edesc == NULL) {
		printf("ugenread: no edesc\n");
		return (EIO);
	}
	if (sce->pipeh == NULL) {
		printf("ugenread: no pipe\n");
		return (EIO);
	}
#endif

	switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
		/* Block until activity occurred. */
		mutex_enter(&sc->sc_lock);
		while (sce->q.c_cc == 0) {
			if (flag & IO_NDELAY) {
				mutex_exit(&sc->sc_lock);
				return (EWOULDBLOCK);
			}
			sce->state |= UGEN_ASLP;
			DPRINTFN(5, ("ugenread: sleep on %p\n", sce));
			/* "ugenri" */
			error = cv_timedwait_sig(&sce->cv, &sc->sc_lock,
			    mstohz(sce->timeout));
			DPRINTFN(5, ("ugenread: woke, error=%d\n", error));
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				sce->state &= ~UGEN_ASLP;
				break;
			}
		}
		mutex_exit(&sc->sc_lock);

		/* Transfer as many chunks as possible. */
		while (sce->q.c_cc > 0 && uio->uio_resid > 0 && !error) {
			n = min(sce->q.c_cc, uio->uio_resid);
			if (n > sizeof(sc->sc_buffer))
				n = sizeof(sc->sc_buffer);

			/* Remove a small chunk from the input queue. */
			q_to_b(&sce->q, sc->sc_buffer, n);
			DPRINTFN(5, ("ugenread: got %d chars\n", n));

			/* Copy the data to the user process. */
			error = uiomove(sc->sc_buffer, n, uio);
			if (error)
				break;
		}
		break;
	case UE_BULK:
		if (sce->state & UGEN_BULK_RA) {
			DPRINTFN(5, ("ugenread: BULK_RA req: %zd used: %d\n",
				     uio->uio_resid, sce->ra_wb_used));
			xfer = sce->ra_wb_xfer;

			mutex_enter(&sc->sc_lock);
			if (sce->ra_wb_used == 0 && flag & IO_NDELAY) {
				mutex_exit(&sc->sc_lock);
				return (EWOULDBLOCK);
			}
			while (uio->uio_resid > 0 && !error) {
				while (sce->ra_wb_used == 0) {
					sce->state |= UGEN_ASLP;
					DPRINTFN(5,
						 ("ugenread: sleep on %p\n",
						  sce));
					/* "ugenrb" */
					error = cv_timedwait_sig(&sce->cv,
					    &sc->sc_lock, mstohz(sce->timeout));
					DPRINTFN(5,
						 ("ugenread: woke, error=%d\n",
						  error));
					if (sc->sc_dying)
						error = EIO;
					if (error) {
						sce->state &= ~UGEN_ASLP;
						break;
					}
				}

				/* Copy data to the process. */
				while (uio->uio_resid > 0
				       && sce->ra_wb_used > 0) {
					n = min(uio->uio_resid,
						sce->ra_wb_used);
					n = min(n, sce->limit - sce->cur);
					error = uiomove(sce->cur, n, uio);
					if (error)
						break;
					sce->cur += n;
					sce->ra_wb_used -= n;
					if (sce->cur == sce->limit)
						sce->cur = sce->ibuf;
				}

				/*
				 * If the transfers stopped because the
				 * buffer was full, restart them.
				 */
				if (sce->state & UGEN_RA_WB_STOP &&
				    sce->ra_wb_used < sce->limit - sce->ibuf) {
					n = (sce->limit - sce->ibuf)
					    - sce->ra_wb_used;
					usbd_setup_xfer(xfer,
					    sce->pipeh, sce, NULL,
					    min(n, sce->ra_wb_xferlen),
					    USBD_NO_COPY, USBD_NO_TIMEOUT,
					    ugen_bulkra_intr);
					sce->state &= ~UGEN_RA_WB_STOP;
					err = usbd_transfer(xfer);
					if (err != USBD_IN_PROGRESS)
						/*
						 * The transfer has not been
						 * queued.  Setting STOP
						 * will make us try
						 * again at the next read.
						 */
						sce->state |= UGEN_RA_WB_STOP;
				}
			}
			mutex_exit(&sc->sc_lock);
			break;
		}
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (ENOMEM);
		while ((n = min(UGEN_BBSIZE, uio->uio_resid)) != 0) {
			DPRINTFN(1, ("ugenread: start transfer %d bytes\n",n));
			tn = n;
			err = usbd_bulk_transfer(
				  xfer, sce->pipeh,
				  sce->state & UGEN_SHORT_OK ?
				      USBD_SHORT_XFER_OK : 0,
				  sce->timeout, sc->sc_buffer, &tn, "ugenrb");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
			DPRINTFN(1, ("ugenread: got %d bytes\n", tn));
			error = uiomove(sc->sc_buffer, tn, uio);
			if (error || tn < n)
				break;
		}
		usbd_free_xfer(xfer);
		break;
	case UE_ISOCHRONOUS:
		mutex_enter(&sc->sc_lock);
		while (sce->cur == sce->fill) {
			if (flag & IO_NDELAY) {
				mutex_exit(&sc->sc_lock);
				return (EWOULDBLOCK);
			}
			sce->state |= UGEN_ASLP;
			/* "ugenri" */
			DPRINTFN(5, ("ugenread: sleep on %p\n", sce));
			error = cv_timedwait_sig(&sce->cv, &sc->sc_lock,
			    mstohz(sce->timeout));
			DPRINTFN(5, ("ugenread: woke, error=%d\n", error));
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				sce->state &= ~UGEN_ASLP;
				break;
			}
		}

		while (sce->cur != sce->fill && uio->uio_resid > 0 && !error) {
			if(sce->fill > sce->cur)
				n = min(sce->fill - sce->cur, uio->uio_resid);
			else
				n = min(sce->limit - sce->cur, uio->uio_resid);

			DPRINTFN(5, ("ugenread: isoc got %d chars\n", n));

			/* Copy the data to the user process. */
			error = uiomove(sce->cur, n, uio);
			if (error)
				break;
			sce->cur += n;
			if (sce->cur >= sce->limit)
				sce->cur = sce->ibuf;
		}
		mutex_exit(&sc->sc_lock);
		break;


	default:
		return (ENXIO);
	}
	return (error);
}

int
ugenread(dev_t dev, struct uio *uio, int flag)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = device_lookup_private(& ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	mutex_enter(&sc->sc_lock);
	sc->sc_refcnt++;
	mutex_exit(&sc->sc_lock);

	error = ugen_do_read(sc, endpt, uio, flag);

	mutex_enter(&sc->sc_lock);
	if (--sc->sc_refcnt < 0)
		usb_detach_broadcast(sc->sc_dev, &sc->sc_detach_cv);
	mutex_exit(&sc->sc_lock);

	return (error);
}

Static int
ugen_do_write(struct ugen_softc *sc, int endpt, struct uio *uio,
	int flag)
{
	struct ugen_endpoint *sce = &sc->sc_endpoints[endpt][OUT];
	u_int32_t n;
	int error = 0;
	u_int32_t tn;
	char *dbuf;
	usbd_xfer_handle xfer;
	usbd_status err;

	DPRINTFN(5, ("%s: ugenwrite: %d\n", device_xname(sc->sc_dev), endpt));

	if (sc->sc_dying)
		return (EIO);

	if (endpt == USB_CONTROL_ENDPOINT)
		return (ENODEV);

#ifdef DIAGNOSTIC
	if (sce->edesc == NULL) {
		printf("ugenwrite: no edesc\n");
		return (EIO);
	}
	if (sce->pipeh == NULL) {
		printf("ugenwrite: no pipe\n");
		return (EIO);
	}
#endif

	switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_BULK:
		if (sce->state & UGEN_BULK_WB) {
			DPRINTFN(5, ("ugenwrite: BULK_WB req: %zd used: %d\n",
				     uio->uio_resid, sce->ra_wb_used));
			xfer = sce->ra_wb_xfer;

			mutex_enter(&sc->sc_lock);
			if (sce->ra_wb_used == sce->limit - sce->ibuf &&
			    flag & IO_NDELAY) {
				mutex_exit(&sc->sc_lock);
				return (EWOULDBLOCK);
			}
			while (uio->uio_resid > 0 && !error) {
				while (sce->ra_wb_used ==
				       sce->limit - sce->ibuf) {
					sce->state |= UGEN_ASLP;
					DPRINTFN(5,
						 ("ugenwrite: sleep on %p\n",
						  sce));
					/* "ugenwb" */
					error = cv_timedwait_sig(&sce->cv,
					    &sc->sc_lock, mstohz(sce->timeout));
					DPRINTFN(5,
						 ("ugenwrite: woke, error=%d\n",
						  error));
					if (sc->sc_dying)
						error = EIO;
					if (error) {
						sce->state &= ~UGEN_ASLP;
						break;
					}
				}

				/* Copy data from the process. */
				while (uio->uio_resid > 0 &&
				    sce->ra_wb_used < sce->limit - sce->ibuf) {
					n = min(uio->uio_resid,
						(sce->limit - sce->ibuf)
						 - sce->ra_wb_used);
					n = min(n, sce->limit - sce->fill);
					error = uiomove(sce->fill, n, uio);
					if (error)
						break;
					sce->fill += n;
					sce->ra_wb_used += n;
					if (sce->fill == sce->limit)
						sce->fill = sce->ibuf;
				}

				/*
				 * If the transfers stopped because the
				 * buffer was empty, restart them.
				 */
				if (sce->state & UGEN_RA_WB_STOP &&
				    sce->ra_wb_used > 0) {
					dbuf = (char *)usbd_get_buffer(xfer);
					n = min(sce->ra_wb_used,
						sce->ra_wb_xferlen);
					tn = min(n, sce->limit - sce->cur);
					memcpy(dbuf, sce->cur, tn);
					dbuf += tn;
					if (n - tn > 0)
						memcpy(dbuf, sce->ibuf,
						       n - tn);
					usbd_setup_xfer(xfer,
					    sce->pipeh, sce, NULL, n,
					    USBD_NO_COPY, USBD_NO_TIMEOUT,
					    ugen_bulkwb_intr);
					sce->state &= ~UGEN_RA_WB_STOP;
					err = usbd_transfer(xfer);
					if (err != USBD_IN_PROGRESS)
						/*
						 * The transfer has not been
						 * queued.  Setting STOP
						 * will make us try again
						 * at the next read.
						 */
						sce->state |= UGEN_RA_WB_STOP;
				}
			}
			mutex_exit(&sc->sc_lock);
			break;
		}
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (EIO);
		while ((n = min(UGEN_BBSIZE, uio->uio_resid)) != 0) {
			error = uiomove(sc->sc_buffer, n, uio);
			if (error)
				break;
			DPRINTFN(1, ("ugenwrite: transfer %d bytes\n", n));
			err = usbd_bulk_transfer(xfer, sce->pipeh, 0,
				  sce->timeout, sc->sc_buffer, &n,"ugenwb");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
		}
		usbd_free_xfer(xfer);
		break;
	case UE_INTERRUPT:
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (EIO);
		while ((n = min(UGETW(sce->edesc->wMaxPacketSize),
		    uio->uio_resid)) != 0) {
			error = uiomove(sc->sc_buffer, n, uio);
			if (error)
				break;
			DPRINTFN(1, ("ugenwrite: transfer %d bytes\n", n));
			err = usbd_intr_transfer(xfer, sce->pipeh, 0,
			    sce->timeout, sc->sc_buffer, &n, "ugenwi");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
		}
		usbd_free_xfer(xfer);
		break;
	default:
		return (ENXIO);
	}
	return (error);
}

int
ugenwrite(dev_t dev, struct uio *uio, int flag)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = device_lookup_private(& ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	mutex_enter(&sc->sc_lock);
	sc->sc_refcnt++;
	mutex_exit(&sc->sc_lock);

	error = ugen_do_write(sc, endpt, uio, flag);

	mutex_enter(&sc->sc_lock);
	if (--sc->sc_refcnt < 0)
		usb_detach_broadcast(sc->sc_dev, &sc->sc_detach_cv);
	mutex_exit(&sc->sc_lock);

	return (error);
}

int
ugen_activate(device_t self, enum devact act)
{
	struct ugen_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
ugen_detach(device_t self, int flags)
{
	struct ugen_softc *sc = device_private(self);
	struct ugen_endpoint *sce;
	int i, dir;
	int maj, mn;

	DPRINTF(("ugen_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	pmf_device_deregister(self);
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			sce = &sc->sc_endpoints[i][dir];
			if (sce && sce->pipeh)
				usbd_abort_pipe(sce->pipeh);
		}
	}

	mutex_enter(&sc->sc_lock);
	if (--sc->sc_refcnt >= 0) {
		/* Wake everyone */
		for (i = 0; i < USB_MAX_ENDPOINTS; i++)
			cv_signal(&sc->sc_endpoints[i][IN].cv);
		/* Wait for processes to go away. */
		usb_detach_wait(sc->sc_dev, &sc->sc_detach_cv, &sc->sc_lock);
	}
	mutex_exit(&sc->sc_lock);

	/* locate the major number */
	maj = cdevsw_lookup_major(&ugen_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = device_unit(self) * USB_MAX_ENDPOINTS;
	vdevgone(maj, mn, mn + USB_MAX_ENDPOINTS - 1, VCHR);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			sce = &sc->sc_endpoints[i][dir];
			seldestroy(&sce->rsel);
			cv_destroy(&sce->cv);
		}
	}

	cv_destroy(&sc->sc_detach_cv);
	mutex_destroy(&sc->sc_lock);

	return (0);
}

Static void
ugenintr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct ugen_endpoint *sce = addr;
	struct ugen_softc *sc = sce->sc;
	u_int32_t count;
	u_char *ibuf;

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ugenintr: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sce->pipeh);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	ibuf = sce->ibuf;

	DPRINTFN(5, ("ugenintr: xfer=%p status=%d count=%d\n",
		     xfer, status, count));
	DPRINTFN(5, ("          data = %02x %02x %02x\n",
		     ibuf[0], ibuf[1], ibuf[2]));

	(void)b_to_q(ibuf, count, &sce->q);

	mutex_enter(&sc->sc_lock);
	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_intr: waking %p\n", sce));
		cv_signal(&sce->cv);
	}
	mutex_exit(&sc->sc_lock);
	selnotify(&sce->rsel, 0, 0);
}

Static void
ugen_isoc_rintr(usbd_xfer_handle xfer, usbd_private_handle addr,
		usbd_status status)
{
	struct isoreq *req = addr;
	struct ugen_endpoint *sce = req->sce;
	struct ugen_softc *sc = sce->sc;
	u_int32_t count, n;
	int i, isize;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("ugen_isoc_rintr: xfer %ld, count=%d\n",
	    (long)(req - sce->isoreqs), count));

	/* throw away oldest input if the buffer is full */
	if(sce->fill < sce->cur && sce->cur <= sce->fill + count) {
		sce->cur += count;
		if(sce->cur >= sce->limit)
			sce->cur = sce->ibuf + (sce->limit - sce->cur);
		DPRINTFN(5, ("ugen_isoc_rintr: throwing away %d bytes\n",
			     count));
	}

	isize = UGETW(sce->edesc->wMaxPacketSize);
	for (i = 0; i < UGEN_NISORFRMS; i++) {
		u_int32_t actlen = req->sizes[i];
		char const *tbuf = (char const *)req->dmabuf + isize * i;

		/* copy data to buffer */
		while (actlen > 0) {
			n = min(actlen, sce->limit - sce->fill);
			memcpy(sce->fill, tbuf, n);

			tbuf += n;
			actlen -= n;
			sce->fill += n;
			if(sce->fill == sce->limit)
				sce->fill = sce->ibuf;
		}

		/* setup size for next transfer */
		req->sizes[i] = isize;
	}

	usbd_setup_isoc_xfer(xfer, sce->pipeh, req, req->sizes, UGEN_NISORFRMS,
			     USBD_NO_COPY, ugen_isoc_rintr);
	(void)usbd_transfer(xfer);

	mutex_enter(&sc->sc_lock);
	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_isoc_rintr: waking %p\n", sce));
		cv_signal(&sce->cv);
	}
	mutex_exit(&sc->sc_lock);
	selnotify(&sce->rsel, 0, 0);
}

Static void
ugen_bulkra_intr(usbd_xfer_handle xfer, usbd_private_handle addr,
		 usbd_status status)
{
	struct ugen_endpoint *sce = addr;
	struct ugen_softc *sc = sce->sc;
	u_int32_t count, n;
	char const *tbuf;
	usbd_status err;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ugen_bulkra_intr: status=%d\n", status));
		sce->state |= UGEN_RA_WB_STOP;
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sce->pipeh);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);

	/* Keep track of how much is in the buffer. */
	sce->ra_wb_used += count;

	/* Copy data to buffer. */
	tbuf = (char const *)usbd_get_buffer(sce->ra_wb_xfer);
	n = min(count, sce->limit - sce->fill);
	memcpy(sce->fill, tbuf, n);
	tbuf += n;
	count -= n;
	sce->fill += n;
	if (sce->fill == sce->limit)
		sce->fill = sce->ibuf;
	if (count > 0) {
		memcpy(sce->fill, tbuf, count);
		sce->fill += count;
	}

	/* Set up the next request if necessary. */
	n = (sce->limit - sce->ibuf) - sce->ra_wb_used;
	if (n > 0) {
		usbd_setup_xfer(xfer, sce->pipeh, sce, NULL,
		    min(n, sce->ra_wb_xferlen), USBD_NO_COPY,
		    USBD_NO_TIMEOUT, ugen_bulkra_intr);
		err = usbd_transfer(xfer);
		if (err != USBD_IN_PROGRESS) {
			printf("usbd_bulkra_intr: error=%d\n", err);
			/*
			 * The transfer has not been queued.  Setting STOP
			 * will make us try again at the next read.
			 */
			sce->state |= UGEN_RA_WB_STOP;
		}
	}
	else
		sce->state |= UGEN_RA_WB_STOP;

	mutex_enter(&sc->sc_lock);
	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_bulkra_intr: waking %p\n", sce));
		cv_signal(&sce->cv);
	}
	mutex_exit(&sc->sc_lock);
	selnotify(&sce->rsel, 0, 0);
}

Static void
ugen_bulkwb_intr(usbd_xfer_handle xfer, usbd_private_handle addr,
		 usbd_status status)
{
	struct ugen_endpoint *sce = addr;
	struct ugen_softc *sc = sce->sc;
	u_int32_t count, n;
	char *tbuf;
	usbd_status err;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ugen_bulkwb_intr: status=%d\n", status));
		sce->state |= UGEN_RA_WB_STOP;
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sce->pipeh);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);

	/* Keep track of how much is in the buffer. */
	sce->ra_wb_used -= count;

	/* Update buffer pointers. */
	sce->cur += count;
	if (sce->cur >= sce->limit)
		sce->cur = sce->ibuf + (sce->cur - sce->limit);

	/* Set up next request if necessary. */
	if (sce->ra_wb_used > 0) {
		/* copy data from buffer */
		tbuf = (char *)usbd_get_buffer(sce->ra_wb_xfer);
		count = min(sce->ra_wb_used, sce->ra_wb_xferlen);
		n = min(count, sce->limit - sce->cur);
		memcpy(tbuf, sce->cur, n);
		tbuf += n;
		if (count - n > 0)
			memcpy(tbuf, sce->ibuf, count - n);

		usbd_setup_xfer(xfer, sce->pipeh, sce, NULL,
		    count, USBD_NO_COPY, USBD_NO_TIMEOUT, ugen_bulkwb_intr);
		err = usbd_transfer(xfer);
		if (err != USBD_IN_PROGRESS) {
			printf("usbd_bulkwb_intr: error=%d\n", err);
			/*
			 * The transfer has not been queued.  Setting STOP
			 * will make us try again at the next write.
			 */
			sce->state |= UGEN_RA_WB_STOP;
		}
	}
	else
		sce->state |= UGEN_RA_WB_STOP;

	mutex_enter(&sc->sc_lock);
	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_bulkwb_intr: waking %p\n", sce));
		cv_signal(&sce->cv);
	}
	mutex_exit(&sc->sc_lock);
	selnotify(&sce->rsel, 0, 0);
}

Static usbd_status
ugen_set_interface(struct ugen_softc *sc, int ifaceidx, int altno)
{
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;
	struct ugen_endpoint *sce;
	u_int8_t niface, nendpt, endptno, endpt;
	int dir;

	DPRINTFN(15, ("ugen_set_interface %d %d\n", ifaceidx, altno));

	err = usbd_interface_count(sc->sc_udev, &niface);
	if (err)
		return (err);
	if (ifaceidx < 0 || ifaceidx >= niface)
		return (USBD_INVAL);

	err = usbd_device2interface_handle(sc->sc_udev, ifaceidx, &iface);
	if (err)
		return (err);
	err = usbd_endpoint_count(iface, &nendpt);
	if (err)
		return (err);
	/* XXX should only do this after setting new altno has succeeded */
	for (endptno = 0; endptno < nendpt; endptno++) {
		ed = usbd_interface2endpoint_descriptor(iface,endptno);
		endpt = ed->bEndpointAddress;
		dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
		sce = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
		sce->sc = 0;
		sce->edesc = 0;
		sce->iface = 0;
	}

	/* change setting */
	err = usbd_set_interface(iface, altno);
	if (err)
		return (err);

	err = usbd_endpoint_count(iface, &nendpt);
	if (err)
		return (err);
	for (endptno = 0; endptno < nendpt; endptno++) {
		ed = usbd_interface2endpoint_descriptor(iface,endptno);
		KASSERT(ed != NULL);
		endpt = ed->bEndpointAddress;
		dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
		sce = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
		sce->sc = sc;
		sce->edesc = ed;
		sce->iface = iface;
	}
	return (0);
}

/* Retrieve a complete descriptor for a certain device and index. */
Static usb_config_descriptor_t *
ugen_get_cdesc(struct ugen_softc *sc, int index, int *lenp)
{
	usb_config_descriptor_t *cdesc, *tdesc, cdescr;
	int len;
	usbd_status err;

	if (index == USB_CURRENT_CONFIG_INDEX) {
		tdesc = usbd_get_config_descriptor(sc->sc_udev);
		len = UGETW(tdesc->wTotalLength);
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		memcpy(cdesc, tdesc, len);
		DPRINTFN(5,("ugen_get_cdesc: current, len=%d\n", len));
	} else {
		err = usbd_get_config_desc(sc->sc_udev, index, &cdescr);
		if (err)
			return (0);
		len = UGETW(cdescr.wTotalLength);
		DPRINTFN(5,("ugen_get_cdesc: index=%d, len=%d\n", index, len));
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		err = usbd_get_config_desc_full(sc->sc_udev, index, cdesc, len);
		if (err) {
			free(cdesc, M_TEMP);
			return (0);
		}
	}
	return (cdesc);
}

Static int
ugen_get_alt_index(struct ugen_softc *sc, int ifaceidx)
{
	usbd_interface_handle iface;
	usbd_status err;

	err = usbd_device2interface_handle(sc->sc_udev, ifaceidx, &iface);
	if (err)
		return (-1);
	return (usbd_get_interface_altindex(iface));
}

Static int
ugen_do_ioctl(struct ugen_softc *sc, int endpt, u_long cmd,
	      void *addr, int flag, struct lwp *l)
{
	struct ugen_endpoint *sce;
	usbd_status err;
	usbd_interface_handle iface;
	struct usb_config_desc *cd;
	usb_config_descriptor_t *cdesc;
	struct usb_interface_desc *id;
	usb_interface_descriptor_t *idesc;
	struct usb_endpoint_desc *ed;
	usb_endpoint_descriptor_t *edesc;
	struct usb_alt_interface *ai;
	struct usb_string_desc *si;
	u_int8_t conf, alt;

	DPRINTFN(5, ("ugenioctl: cmd=%08lx\n", cmd));
	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		return (0);
	case USB_SET_SHORT_XFER:
		if (endpt == USB_CONTROL_ENDPOINT)
			return (EINVAL);
		/* This flag only affects read */
		sce = &sc->sc_endpoints[endpt][IN];
		if (sce == NULL || sce->pipeh == NULL)
			return (EINVAL);
		if (*(int *)addr)
			sce->state |= UGEN_SHORT_OK;
		else
			sce->state &= ~UGEN_SHORT_OK;
		return (0);
	case USB_SET_TIMEOUT:
		sce = &sc->sc_endpoints[endpt][IN];
		if (sce == NULL
		    /* XXX this shouldn't happen, but the distinction between
		       input and output pipes isn't clear enough.
		       || sce->pipeh == NULL */
			)
			return (EINVAL);
		sce->timeout = *(int *)addr;
		return (0);
	case USB_SET_BULK_RA:
		if (endpt == USB_CONTROL_ENDPOINT)
			return (EINVAL);
		sce = &sc->sc_endpoints[endpt][IN];
		if (sce == NULL || sce->pipeh == NULL)
			return (EINVAL);
		edesc = sce->edesc;
		if ((edesc->bmAttributes & UE_XFERTYPE) != UE_BULK)
			return (EINVAL);

		if (*(int *)addr) {
			/* Only turn RA on if it's currently off. */
			if (sce->state & UGEN_BULK_RA)
				return (0);

			if (sce->ra_wb_bufsize == 0 || sce->ra_wb_reqsize == 0)
				/* shouldn't happen */
				return (EINVAL);
			sce->ra_wb_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (sce->ra_wb_xfer == NULL)
				return (ENOMEM);
			sce->ra_wb_xferlen = sce->ra_wb_reqsize;
			/*
			 * Set up a dmabuf because we reuse the xfer with
			 * the same (max) request length like isoc.
			 */
			if (usbd_alloc_buffer(sce->ra_wb_xfer,
					      sce->ra_wb_xferlen) == 0) {
				usbd_free_xfer(sce->ra_wb_xfer);
				return (ENOMEM);
			}
			sce->ibuf = malloc(sce->ra_wb_bufsize,
					   M_USBDEV, M_WAITOK);
			sce->fill = sce->cur = sce->ibuf;
			sce->limit = sce->ibuf + sce->ra_wb_bufsize;
			sce->ra_wb_used = 0;
			sce->state |= UGEN_BULK_RA;
			sce->state &= ~UGEN_RA_WB_STOP;
			/* Now start reading. */
			usbd_setup_xfer(sce->ra_wb_xfer, sce->pipeh, sce,
			    NULL,
			    min(sce->ra_wb_xferlen, sce->ra_wb_bufsize),
			    USBD_NO_COPY, USBD_NO_TIMEOUT,
			    ugen_bulkra_intr);
			err = usbd_transfer(sce->ra_wb_xfer);
			if (err != USBD_IN_PROGRESS) {
				sce->state &= ~UGEN_BULK_RA;
				free(sce->ibuf, M_USBDEV);
				sce->ibuf = NULL;
				usbd_free_xfer(sce->ra_wb_xfer);
				return (EIO);
			}
		} else {
			/* Only turn RA off if it's currently on. */
			if (!(sce->state & UGEN_BULK_RA))
				return (0);

			sce->state &= ~UGEN_BULK_RA;
			usbd_abort_pipe(sce->pipeh);
			usbd_free_xfer(sce->ra_wb_xfer);
			/*
			 * XXX Discard whatever's in the buffer, but we
			 * should keep it around and drain the buffer
			 * instead.
			 */
			free(sce->ibuf, M_USBDEV);
			sce->ibuf = NULL;
		}
		return (0);
	case USB_SET_BULK_WB:
		if (endpt == USB_CONTROL_ENDPOINT)
			return (EINVAL);
		sce = &sc->sc_endpoints[endpt][OUT];
		if (sce == NULL || sce->pipeh == NULL)
			return (EINVAL);
		edesc = sce->edesc;
		if ((edesc->bmAttributes & UE_XFERTYPE) != UE_BULK)
			return (EINVAL);

		if (*(int *)addr) {
			/* Only turn WB on if it's currently off. */
			if (sce->state & UGEN_BULK_WB)
				return (0);

			if (sce->ra_wb_bufsize == 0 || sce->ra_wb_reqsize == 0)
				/* shouldn't happen */
				return (EINVAL);
			sce->ra_wb_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (sce->ra_wb_xfer == NULL)
				return (ENOMEM);
			sce->ra_wb_xferlen = sce->ra_wb_reqsize;
			/*
			 * Set up a dmabuf because we reuse the xfer with
			 * the same (max) request length like isoc.
			 */
			if (usbd_alloc_buffer(sce->ra_wb_xfer,
					      sce->ra_wb_xferlen) == 0) {
				usbd_free_xfer(sce->ra_wb_xfer);
				return (ENOMEM);
			}
			sce->ibuf = malloc(sce->ra_wb_bufsize,
					   M_USBDEV, M_WAITOK);
			sce->fill = sce->cur = sce->ibuf;
			sce->limit = sce->ibuf + sce->ra_wb_bufsize;
			sce->ra_wb_used = 0;
			sce->state |= UGEN_BULK_WB | UGEN_RA_WB_STOP;
		} else {
			/* Only turn WB off if it's currently on. */
			if (!(sce->state & UGEN_BULK_WB))
				return (0);

			sce->state &= ~UGEN_BULK_WB;
			/*
			 * XXX Discard whatever's in the buffer, but we
			 * should keep it around and keep writing to
			 * drain the buffer instead.
			 */
			usbd_abort_pipe(sce->pipeh);
			usbd_free_xfer(sce->ra_wb_xfer);
			free(sce->ibuf, M_USBDEV);
			sce->ibuf = NULL;
		}
		return (0);
	case USB_SET_BULK_RA_OPT:
	case USB_SET_BULK_WB_OPT:
	{
		struct usb_bulk_ra_wb_opt *opt;

		if (endpt == USB_CONTROL_ENDPOINT)
			return (EINVAL);
		opt = (struct usb_bulk_ra_wb_opt *)addr;
		if (cmd == USB_SET_BULK_RA_OPT)
			sce = &sc->sc_endpoints[endpt][IN];
		else
			sce = &sc->sc_endpoints[endpt][OUT];
		if (sce == NULL || sce->pipeh == NULL)
			return (EINVAL);
		if (opt->ra_wb_buffer_size < 1 ||
		    opt->ra_wb_buffer_size > UGEN_BULK_RA_WB_BUFMAX ||
		    opt->ra_wb_request_size < 1 ||
		    opt->ra_wb_request_size > opt->ra_wb_buffer_size)
			return (EINVAL);
		/*
		 * XXX These changes do not take effect until the
		 * next time RA/WB mode is enabled but they ought to
		 * take effect immediately.
		 */
		sce->ra_wb_bufsize = opt->ra_wb_buffer_size;
		sce->ra_wb_reqsize = opt->ra_wb_request_size;
		return (0);
	}
	default:
		break;
	}

	if (endpt != USB_CONTROL_ENDPOINT)
		return (EINVAL);

	switch (cmd) {
#ifdef UGEN_DEBUG
	case USB_SETDEBUG:
		ugendebug = *(int *)addr;
		break;
#endif
	case USB_GET_CONFIG:
		err = usbd_get_config(sc->sc_udev, &conf);
		if (err)
			return (EIO);
		*(int *)addr = conf;
		break;
	case USB_SET_CONFIG:
		if (!(flag & FWRITE))
			return (EPERM);
		err = ugen_set_config(sc, *(int *)addr);
		switch (err) {
		case USBD_NORMAL_COMPLETION:
			break;
		case USBD_IN_USE:
			return (EBUSY);
		default:
			return (EIO);
		}
		break;
	case USB_GET_ALTINTERFACE:
		ai = (struct usb_alt_interface *)addr;
		err = usbd_device2interface_handle(sc->sc_udev,
			  ai->uai_interface_index, &iface);
		if (err)
			return (EINVAL);
		idesc = usbd_get_interface_descriptor(iface);
		if (idesc == NULL)
			return (EIO);
		ai->uai_alt_no = idesc->bAlternateSetting;
		break;
	case USB_SET_ALTINTERFACE:
		if (!(flag & FWRITE))
			return (EPERM);
		ai = (struct usb_alt_interface *)addr;
		err = usbd_device2interface_handle(sc->sc_udev,
			  ai->uai_interface_index, &iface);
		if (err)
			return (EINVAL);
		err = ugen_set_interface(sc, ai->uai_interface_index,
		    ai->uai_alt_no);
		if (err)
			return (EINVAL);
		break;
	case USB_GET_NO_ALT:
		ai = (struct usb_alt_interface *)addr;
		cdesc = ugen_get_cdesc(sc, ai->uai_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		idesc = usbd_find_idesc(cdesc, ai->uai_interface_index, 0);
		if (idesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		ai->uai_alt_no = usbd_get_no_alts(cdesc,
		    idesc->bInterfaceNumber);
		free(cdesc, M_TEMP);
		break;
	case USB_GET_DEVICE_DESC:
		*(usb_device_descriptor_t *)addr =
			*usbd_get_device_descriptor(sc->sc_udev);
		break;
	case USB_GET_CONFIG_DESC:
		cd = (struct usb_config_desc *)addr;
		cdesc = ugen_get_cdesc(sc, cd->ucd_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		cd->ucd_desc = *cdesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_INTERFACE_DESC:
		id = (struct usb_interface_desc *)addr;
		cdesc = ugen_get_cdesc(sc, id->uid_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		if (id->uid_config_index == USB_CURRENT_CONFIG_INDEX &&
		    id->uid_alt_index == USB_CURRENT_ALT_INDEX)
			alt = ugen_get_alt_index(sc, id->uid_interface_index);
		else
			alt = id->uid_alt_index;
		idesc = usbd_find_idesc(cdesc, id->uid_interface_index, alt);
		if (idesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		id->uid_desc = *idesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_ENDPOINT_DESC:
		ed = (struct usb_endpoint_desc *)addr;
		cdesc = ugen_get_cdesc(sc, ed->ued_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		if (ed->ued_config_index == USB_CURRENT_CONFIG_INDEX &&
		    ed->ued_alt_index == USB_CURRENT_ALT_INDEX)
			alt = ugen_get_alt_index(sc, ed->ued_interface_index);
		else
			alt = ed->ued_alt_index;
		edesc = usbd_find_edesc(cdesc, ed->ued_interface_index,
					alt, ed->ued_endpoint_index);
		if (edesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		ed->ued_desc = *edesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_FULL_DESC:
	{
		int len;
		struct iovec iov;
		struct uio uio;
		struct usb_full_desc *fd = (struct usb_full_desc *)addr;
		int error;

		cdesc = ugen_get_cdesc(sc, fd->ufd_config_index, &len);
		if (cdesc == NULL)
			return (EINVAL);
		if (len > fd->ufd_size)
			len = fd->ufd_size;
		iov.iov_base = (void *)fd->ufd_data;
		iov.iov_len = len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = len;
		uio.uio_offset = 0;
		uio.uio_rw = UIO_READ;
		uio.uio_vmspace = l->l_proc->p_vmspace;
		error = uiomove((void *)cdesc, len, &uio);
		free(cdesc, M_TEMP);
		return (error);
	}
	case USB_GET_STRING_DESC: {
		int len;
		si = (struct usb_string_desc *)addr;
		err = usbd_get_string_desc(sc->sc_udev, si->usd_string_index,
			  si->usd_language_id, &si->usd_desc, &len);
		if (err)
			return (EINVAL);
		break;
	}
	case USB_DO_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)addr;
		int len = UGETW(ur->ucr_request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		usbd_status xerr;
		int error = 0;

		if (!(flag & FWRITE))
			return (EPERM);
		/* Avoid requests that would damage the bus integrity. */
		if ((ur->ucr_request.bmRequestType == UT_WRITE_DEVICE &&
		     ur->ucr_request.bRequest == UR_SET_ADDRESS) ||
		    (ur->ucr_request.bmRequestType == UT_WRITE_DEVICE &&
		     ur->ucr_request.bRequest == UR_SET_CONFIG) ||
		    (ur->ucr_request.bmRequestType == UT_WRITE_INTERFACE &&
		     ur->ucr_request.bRequest == UR_SET_INTERFACE))
			return (EINVAL);

		if (len < 0 || len > 32767)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (void *)ur->ucr_data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_rw =
				ur->ucr_request.bmRequestType & UT_READ ?
				UIO_READ : UIO_WRITE;
			uio.uio_vmspace = l->l_proc->p_vmspace;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		sce = &sc->sc_endpoints[endpt][IN];
		xerr = usbd_do_request_flags(sc->sc_udev, &ur->ucr_request,
			  ptr, ur->ucr_flags, &ur->ucr_actlen, sce->timeout);
		if (xerr) {
			error = EIO;
			goto ret;
		}
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
	ret:
		if (ptr)
			free(ptr, M_TEMP);
		return (error);
	}
	case USB_GET_DEVICEINFO:
		usbd_fill_deviceinfo(sc->sc_udev,
				     (struct usb_device_info *)addr, 0);
		break;
#ifdef COMPAT_30
	case USB_GET_DEVICEINFO_OLD:
		usbd_fill_deviceinfo_old(sc->sc_udev,
					 (struct usb_device_info_old *)addr, 0);

		break;
#endif
	default:
		return (EINVAL);
	}
	return (0);
}

int
ugenioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = device_lookup_private(& ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	sc->sc_refcnt++;
	error = ugen_do_ioctl(sc, endpt, cmd, addr, flag, l);
	if (--sc->sc_refcnt < 0)
		usb_detach_broadcast(sc->sc_dev, &sc->sc_detach_cv);
	return (error);
}

int
ugenpoll(dev_t dev, int events, struct lwp *l)
{
	struct ugen_softc *sc;
	struct ugen_endpoint *sce_in, *sce_out;
	int revents = 0;

	sc = device_lookup_private(&ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (POLLHUP);

	if (UGENENDPOINT(dev) == USB_CONTROL_ENDPOINT)
		return ENODEV;

	sce_in = &sc->sc_endpoints[UGENENDPOINT(dev)][IN];
	sce_out = &sc->sc_endpoints[UGENENDPOINT(dev)][OUT];
	if (sce_in == NULL && sce_out == NULL)
		return (POLLERR);
#ifdef DIAGNOSTIC
	if (!sce_in->edesc && !sce_out->edesc) {
		printf("ugenpoll: no edesc\n");
		return (POLLERR);
	}
	/* It's possible to have only one pipe open. */
	if (!sce_in->pipeh && !sce_out->pipeh) {
		printf("ugenpoll: no pipe\n");
		return (POLLERR);
	}
#endif

	mutex_enter(&sc->sc_lock);
	if (sce_in && sce_in->pipeh && (events & (POLLIN | POLLRDNORM)))
		switch (sce_in->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			if (sce_in->q.c_cc > 0)
				revents |= events & (POLLIN | POLLRDNORM);
			else
				selrecord(l, &sce_in->rsel);
			break;
		case UE_ISOCHRONOUS:
			if (sce_in->cur != sce_in->fill)
				revents |= events & (POLLIN | POLLRDNORM);
			else
				selrecord(l, &sce_in->rsel);
			break;
		case UE_BULK:
			if (sce_in->state & UGEN_BULK_RA) {
				if (sce_in->ra_wb_used > 0)
					revents |= events &
					    (POLLIN | POLLRDNORM);
				else
					selrecord(l, &sce_in->rsel);
				break;
			}
			/*
			 * We have no easy way of determining if a read will
			 * yield any data or a write will happen.
			 * Pretend they will.
			 */
			 revents |= events & (POLLIN | POLLRDNORM);
			 break;
		default:
			break;
		}
	if (sce_out && sce_out->pipeh && (events & (POLLOUT | POLLWRNORM)))
		switch (sce_out->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
		case UE_ISOCHRONOUS:
			/* XXX unimplemented */
			break;
		case UE_BULK:
			if (sce_out->state & UGEN_BULK_WB) {
				if (sce_out->ra_wb_used <
				    sce_out->limit - sce_out->ibuf)
					revents |= events &
					    (POLLOUT | POLLWRNORM);
				else
					selrecord(l, &sce_out->rsel);
				break;
			}
			/*
			 * We have no easy way of determining if a read will
			 * yield any data or a write will happen.
			 * Pretend they will.
			 */
			 revents |= events & (POLLOUT | POLLWRNORM);
			 break;
		default:
			break;
		}

	mutex_exit(&sc->sc_lock);

	return (revents);
}

static void
filt_ugenrdetach(struct knote *kn)
{
	struct ugen_endpoint *sce = kn->kn_hook;
	struct ugen_softc *sc = sce->sc;

	mutex_enter(&sc->sc_lock);
	SLIST_REMOVE(&sce->rsel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(&sc->sc_lock);
}

static int
filt_ugenread_intr(struct knote *kn, long hint)
{
	struct ugen_endpoint *sce = kn->kn_hook;

	kn->kn_data = sce->q.c_cc;
	return (kn->kn_data > 0);
}

static int
filt_ugenread_isoc(struct knote *kn, long hint)
{
	struct ugen_endpoint *sce = kn->kn_hook;

	if (sce->cur == sce->fill)
		return (0);

	if (sce->cur < sce->fill)
		kn->kn_data = sce->fill - sce->cur;
	else
		kn->kn_data = (sce->limit - sce->cur) +
		    (sce->fill - sce->ibuf);

	return (1);
}

static int
filt_ugenread_bulk(struct knote *kn, long hint)
{
	struct ugen_endpoint *sce = kn->kn_hook;

	if (!(sce->state & UGEN_BULK_RA))
		/*
		 * We have no easy way of determining if a read will
		 * yield any data or a write will happen.
		 * So, emulate "seltrue".
		 */
		return (filt_seltrue(kn, hint));

	if (sce->ra_wb_used == 0)
		return (0);

	kn->kn_data = sce->ra_wb_used;

	return (1);
}

static int
filt_ugenwrite_bulk(struct knote *kn, long hint)
{
	struct ugen_endpoint *sce = kn->kn_hook;

	if (!(sce->state & UGEN_BULK_WB))
		/*
		 * We have no easy way of determining if a read will
		 * yield any data or a write will happen.
		 * So, emulate "seltrue".
		 */
		return (filt_seltrue(kn, hint));

	if (sce->ra_wb_used == sce->limit - sce->ibuf)
		return (0);

	kn->kn_data = (sce->limit - sce->ibuf) - sce->ra_wb_used;

	return (1);
}

static const struct filterops ugenread_intr_filtops =
	{ 1, NULL, filt_ugenrdetach, filt_ugenread_intr };

static const struct filterops ugenread_isoc_filtops =
	{ 1, NULL, filt_ugenrdetach, filt_ugenread_isoc };

static const struct filterops ugenread_bulk_filtops =
	{ 1, NULL, filt_ugenrdetach, filt_ugenread_bulk };

static const struct filterops ugenwrite_bulk_filtops =
	{ 1, NULL, filt_ugenrdetach, filt_ugenwrite_bulk };

int
ugenkqfilter(dev_t dev, struct knote *kn)
{
	struct ugen_softc *sc;
	struct ugen_endpoint *sce;
	struct klist *klist;

	sc = device_lookup_private(&ugen_cd, UGENUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (ENXIO);

	if (UGENENDPOINT(dev) == USB_CONTROL_ENDPOINT)
		return ENODEV;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		sce = &sc->sc_endpoints[UGENENDPOINT(dev)][IN];
		if (sce == NULL)
			return (EINVAL);

		klist = &sce->rsel.sel_klist;
		switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			kn->kn_fop = &ugenread_intr_filtops;
			break;
		case UE_ISOCHRONOUS:
			kn->kn_fop = &ugenread_isoc_filtops;
			break;
		case UE_BULK:
			kn->kn_fop = &ugenread_bulk_filtops;
			break;
		default:
			return (EINVAL);
		}
		break;

	case EVFILT_WRITE:
		sce = &sc->sc_endpoints[UGENENDPOINT(dev)][OUT];
		if (sce == NULL)
			return (EINVAL);

		klist = &sce->rsel.sel_klist;
		switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
		case UE_ISOCHRONOUS:
			/* XXX poll doesn't support this */
			return (EINVAL);

		case UE_BULK:
			kn->kn_fop = &ugenwrite_bulk_filtops;
			break;
		default:
			return (EINVAL);
		}
		break;

	default:
		return (EINVAL);
	}

	kn->kn_hook = sce;

	mutex_enter(&sc->sc_lock);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mutex_exit(&sc->sc_lock);

	return (0);
}
