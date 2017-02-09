/* $NetBSD: emdtv_ir.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: emdtv_ir.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $");

#include <sys/select.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/emdtvvar.h>
#include <dev/usb/emdtvreg.h>

#include <dev/ir/ir.h>
#include <dev/ir/cirio.h>
#include <dev/ir/cirvar.h>

static void		emdtv_ir_intr(usbd_xfer_handle, usbd_private_handle,
				      usbd_status);
static void		emdtv_ir_worker(struct work *, void *);

static int		emdtv_ir_open(void *, int, int, struct proc *);
static int		emdtv_ir_close(void *, int, int, struct proc *);
static int		emdtv_ir_read(void *, struct uio *, int);
static int		emdtv_ir_write(void *, struct uio *, int);
static int		emdtv_ir_setparams(void *, struct cir_params *);

static const struct cir_methods emdtv_ir_methods = {
	.im_open = emdtv_ir_open,
	.im_close = emdtv_ir_close,
	.im_read = emdtv_ir_read,
	.im_write = emdtv_ir_write,
	.im_setparams = emdtv_ir_setparams,
};

void
emdtv_ir_attach(struct emdtv_softc *sc)
{
	struct ir_attach_args ia;
	usb_endpoint_descriptor_t *ed;
	usbd_status status;
	int err;

	ed = usbd_interface2endpoint_descriptor(sc->sc_iface, 0);
	if (ed == NULL)
		return;

	status = usbd_open_pipe_intr(sc->sc_iface, ed->bEndpointAddress,
	    USBD_EXCLUSIVE_USE, &sc->sc_intr_pipe, sc, &sc->sc_intr_buf, 1,
	    emdtv_ir_intr, USBD_DEFAULT_INTERVAL);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't open intr pipe: %s\n",
		    usbd_errstr(status));
		return;
	}

	mutex_init(&sc->sc_ir_mutex, MUTEX_DEFAULT, IPL_VM);

	err = workqueue_create(&sc->sc_ir_wq, "emdtvir",
	    emdtv_ir_worker, sc, PRI_NONE, IPL_VM, 0);
	if (err)
		aprint_error_dev(sc->sc_dev, "couldn't create workqueue: %d\n",
		    err);

	ia.ia_type = IR_TYPE_CIR;
	ia.ia_methods = &emdtv_ir_methods;
	ia.ia_handle = sc;

	sc->sc_cirdev =
	    config_found_ia(sc->sc_dev, "irbus", &ia, ir_print);
}

void
emdtv_ir_detach(struct emdtv_softc *sc, int flags)
{
	if (sc->sc_ir_wq != NULL)
		workqueue_destroy(sc->sc_ir_wq);

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}

	mutex_enter(&sc->sc_ir_mutex);
	mutex_exit(&sc->sc_ir_mutex);
	mutex_destroy(&sc->sc_ir_mutex);

	if (sc->sc_cirdev != NULL)
		config_detach(sc->sc_cirdev, flags);
}

static void
emdtv_ir_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct emdtv_softc *sc = priv;
	uint32_t len;

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);
	if (status == USBD_CANCELLED)
		return;

	if (sc->sc_ir_wq)
		workqueue_enqueue(sc->sc_ir_wq, &sc->sc_ir_work, NULL);
}

static void
emdtv_ir_worker(struct work *wk, void *opaque)
{
	struct emdtv_softc *sc = opaque;
	struct cir_softc *csc;
	uint8_t evt[3];
	int pos;

	if (sc->sc_cirdev == NULL || sc->sc_dying == true ||
	    sc->sc_ir_open == false)
		return;

	emdtv_read_multi_1(sc, UR_GET_STATUS, EM28XX_REG_IR, evt, sizeof(evt));

	csc = device_private(sc->sc_cirdev);

	mutex_enter(&sc->sc_ir_mutex);
	pos = (sc->sc_ir_ptr + sc->sc_ir_cnt) % EMDTV_CIR_BUFLEN;
	memcpy(&sc->sc_ir_queue[pos], evt, sizeof(evt));
	if (sc->sc_ir_cnt < EMDTV_CIR_BUFLEN - 1) {
		++sc->sc_ir_cnt;
		++csc->sc_rdframes;
	}
	selnotify(&csc->sc_rdsel, 0, 1);
	mutex_exit(&sc->sc_ir_mutex);
}

/*
 * cir(4)
 */
static int
emdtv_ir_open(void *opaque, int flag, int mode, struct proc *p)
{
	struct emdtv_softc *sc = opaque;

	if (sc->sc_ir_open == true)
		return EBUSY;

	sc->sc_ir_cnt = 0;
	sc->sc_ir_ptr = EMDTV_CIR_BUFLEN - 1;
	sc->sc_ir_open = true;

	return 0;
}

static int
emdtv_ir_close(void *opaque, int flag, int mode, struct proc *p)
{
	struct emdtv_softc *sc = opaque;

	sc->sc_ir_open = false;

	return 0;
}

static int
emdtv_ir_read(void *opaque, struct uio *uio, int flag)
{
	struct emdtv_softc *sc = opaque;
	struct cir_softc *csc;
	int error = 0;

	if (uio->uio_resid != 3)	/* 3 byte protocol */
		return EINVAL;

	if (sc->sc_dying)
		return EIO;

	csc = device_private(sc->sc_cirdev);

	mutex_enter(&sc->sc_ir_mutex);
	if (sc->sc_ir_cnt == 0)
		goto out;

	error = uiomove(&sc->sc_ir_queue[sc->sc_ir_ptr], 3, uio);
	sc->sc_ir_ptr++;
	if (sc->sc_ir_ptr == EMDTV_CIR_BUFLEN)
		sc->sc_ir_ptr = 0;
	--sc->sc_ir_cnt;
	--csc->sc_rdframes;
	KASSERT(sc->sc_ir_cnt >= 0);

out:
	mutex_exit(&sc->sc_ir_mutex);
	return error;
}

static int
emdtv_ir_write(void *opaque, struct uio *uio, int flag)
{
	return EINVAL;
}

static int
emdtv_ir_setparams(void *opaque, struct cir_params *cp)
{
	return 0;
}
