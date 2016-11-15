/* $NetBSD: auvitek_dtv.c,v 1.6 2013/01/22 12:40:42 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Auvitek AU0828 USB controller (Digital TV function)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek_dtv.c,v 1.6 2013/01/22 12:40:42 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/dtv/dtvif.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

static void		auvitek_dtv_get_devinfo(void *,
			    struct dvb_frontend_info *);
static int		auvitek_dtv_open(void *, int);
static void		auvitek_dtv_close(void *);
static int		auvitek_dtv_set_tuner(void *,
			    const struct dvb_frontend_parameters *);
static fe_status_t	auvitek_dtv_get_status(void *);
static uint16_t		auvitek_dtv_get_signal_strength(void *);
static uint16_t		auvitek_dtv_get_snr(void *);
static int		auvitek_dtv_start_transfer(void *,
			    void (*)(void *, const struct dtv_payload *),
			    void *);
static int		auvitek_dtv_stop_transfer(void *);

static int		auvitek_dtv_init_pipes(struct auvitek_softc *);
static int		auvitek_dtv_close_pipes(struct auvitek_softc *);

static int		auvitek_dtv_bulk_start(struct auvitek_softc *);
static int		auvitek_dtv_bulk_start1(struct auvitek_bulk_xfer *);
static void		auvitek_dtv_bulk_cb(usbd_xfer_handle,
					    usbd_private_handle,
					    usbd_status);

static const struct dtv_hw_if auvitek_dtv_if = {
	.get_devinfo = auvitek_dtv_get_devinfo,
	.open = auvitek_dtv_open,
	.close = auvitek_dtv_close,
	.set_tuner = auvitek_dtv_set_tuner,
	.get_status = auvitek_dtv_get_status,
	.get_signal_strength = auvitek_dtv_get_signal_strength,
	.get_snr = auvitek_dtv_get_snr,
	.start_transfer = auvitek_dtv_start_transfer,
	.stop_transfer = auvitek_dtv_stop_transfer,
};

int
auvitek_dtv_attach(struct auvitek_softc *sc)
{

	auvitek_dtv_rescan(sc, NULL, NULL);

	return (sc->sc_dtvdev != NULL);
}

int
auvitek_dtv_detach(struct auvitek_softc *sc, int flags)
{
	if (sc->sc_dtvdev != NULL) {
		config_detach(sc->sc_dtvdev, flags);
		sc->sc_dtvdev = NULL;
	}

	return 0;
}

void
auvitek_dtv_rescan(struct auvitek_softc *sc, const char *ifattr,
    const int *locs)
{
	struct dtv_attach_args daa;

	daa.hw = &auvitek_dtv_if;
	daa.priv = sc;

	if (ifattr_match(ifattr, "dtvbus") && sc->sc_dtvdev == NULL)
		sc->sc_dtvdev = config_found_ia(sc->sc_dev, "dtvbus",
		    &daa, dtv_print);
}

void
auvitek_dtv_childdet(struct auvitek_softc *sc, device_t child)
{
	if (sc->sc_dtvdev == child)
		sc->sc_dtvdev = NULL;
}

static void
auvitek_dtv_get_devinfo(void *priv, struct dvb_frontend_info *info)
{
	struct auvitek_softc *sc = priv;

	memset(info, 0, sizeof(*info));
	strlcpy(info->name, sc->sc_descr, sizeof(info->name));
	info->type = FE_ATSC;
	info->frequency_min = 54000000;
	info->frequency_max = 858000000;
	info->frequency_stepsize = 62500;
	info->caps = FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_8VSB;
}

static int
auvitek_dtv_open(void *priv, int flags)
{
	struct auvitek_softc *sc = priv;

	if (sc->sc_dying)
		return EIO;

	auvitek_attach_tuner(sc->sc_dev);
	if (sc->sc_xc5k == NULL)
		return ENXIO;

	return auvitek_dtv_init_pipes(sc);
}

static void
auvitek_dtv_close(void *priv)
{
	struct auvitek_softc *sc = priv;

	auvitek_dtv_stop_transfer(sc);
	auvitek_dtv_close_pipes(sc);

	sc->sc_dtvsubmitcb = NULL;
	sc->sc_dtvsubmitarg = NULL;
}

static int
auvitek_dtv_set_tuner(void *priv, const struct dvb_frontend_parameters *params)
{
	struct auvitek_softc *sc = priv;
	int error;

	error = au8522_set_modulation(sc->sc_au8522, params->u.vsb.modulation);
	if (error)
		return error;

	delay(100000);

	au8522_set_gate(sc->sc_au8522, true);
	error = xc5k_tune_dtv(sc->sc_xc5k, params);
	au8522_set_gate(sc->sc_au8522, false);

	return error;
}

fe_status_t
auvitek_dtv_get_status(void *priv)
{
	struct auvitek_softc *sc = priv;

	return au8522_get_dtv_status(sc->sc_au8522);
}

uint16_t
auvitek_dtv_get_signal_strength(void *priv)
{
	return auvitek_dtv_get_snr(priv);
}

uint16_t
auvitek_dtv_get_snr(void *priv)
{
	struct auvitek_softc *sc = priv;

	return au8522_get_snr(sc->sc_au8522);
}

static int
auvitek_dtv_start_transfer(void *priv,
    void (*cb)(void *, const struct dtv_payload *), void *arg)
{
	struct auvitek_softc *sc = priv;
	int s;

	if (sc->sc_ab.ab_running) {
		return 0;
	}

	sc->sc_dtvsubmitcb = cb;
	sc->sc_dtvsubmitarg = arg;

	auvitek_write_1(sc, 0x608, 0x90);
	auvitek_write_1(sc, 0x609, 0x72);
	auvitek_write_1(sc, 0x60a, 0x71);
	auvitek_write_1(sc, 0x60b, 0x01);

	sc->sc_ab.ab_running = true;

	s = splusb();
	auvitek_dtv_bulk_start(sc);
	splx(s);

	return 0;
}

static int
auvitek_dtv_stop_transfer(void *priv)
{
	struct auvitek_softc *sc = priv;

	sc->sc_ab.ab_running = false;

	auvitek_write_1(sc, 0x608, 0x00);
	auvitek_write_1(sc, 0x609, 0x00);
	auvitek_write_1(sc, 0x60a, 0x00);
	auvitek_write_1(sc, 0x60b, 0x00);

	return 0;
}

static int
auvitek_dtv_init_pipes(struct auvitek_softc *sc)
{
	usbd_status err;

	KERNEL_LOCK(1, curlwp);
	err = usbd_open_pipe(sc->sc_bulk_iface, sc->sc_ab.ab_endpt,
	    USBD_EXCLUSIVE_USE|USBD_MPSAFE, &sc->sc_ab.ab_pipe);
	KERNEL_UNLOCK_ONE(curlwp);

	if (err) {
		aprint_error_dev(sc->sc_dev, "couldn't open bulk-in pipe: %s\n",
		    usbd_errstr(err));
		return ENOMEM;
	}

	return 0;
}

static int
auvitek_dtv_close_pipes(struct auvitek_softc *sc)
{
	if (sc->sc_ab.ab_pipe != NULL) {
		KERNEL_LOCK(1, curlwp);
		usbd_abort_pipe(sc->sc_ab.ab_pipe);
		usbd_close_pipe(sc->sc_ab.ab_pipe);
		KERNEL_UNLOCK_ONE(curlwp);
		sc->sc_ab.ab_pipe = NULL;
	}

	return 0;
}

static void
auvitek_dtv_bulk_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct auvitek_bulk_xfer *bx = priv;
	struct auvitek_softc *sc = bx->bx_sc;
	struct auvitek_bulk *ab = &sc->sc_ab;
	struct dtv_payload payload;
	uint32_t xferlen;

	if (ab->ab_running == false || sc->sc_dtvsubmitcb == NULL)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &xferlen, NULL);

	//printf("%s: status=%d xferlen=%u\n", __func__, status, xferlen);

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: USB error (%s)\n", __func__, usbd_errstr(status));
		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(ab->ab_pipe);
			goto next;
		}
		if (status == USBD_SHORT_XFER) {
			goto next;
		}
		return;
	}

	if (xferlen == 0) {
		printf("%s: 0-length xfer\n", __func__);
		goto next;
	}

	payload.data = bx->bx_buffer;
	payload.size = xferlen;
	sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);

next:
	auvitek_dtv_bulk_start1(bx);
}

static int
auvitek_dtv_bulk_start(struct auvitek_softc *sc)
{
	int i, error;

	for (i = 0; i < AUVITEK_NBULK_XFERS; i++) {
		error = auvitek_dtv_bulk_start1(&sc->sc_ab.ab_bx[i]);
		if (error)
			return error;
	}

	return 0;
}

static int
auvitek_dtv_bulk_start1(struct auvitek_bulk_xfer *bx)
{
	struct auvitek_softc *sc = bx->bx_sc;
	struct auvitek_bulk *ab = &sc->sc_ab;
	int err;

	usbd_setup_xfer(bx->bx_xfer, ab->ab_pipe, bx,
	    bx->bx_buffer, AUVITEK_BULK_BUFLEN,
	    //USBD_SHORT_XFER_OK|USBD_NO_COPY, USBD_NO_TIMEOUT,
	    USBD_NO_COPY, 100,
	    auvitek_dtv_bulk_cb);

	KERNEL_LOCK(1, curlwp);
	err = usbd_transfer(bx->bx_xfer);
	KERNEL_UNLOCK_ONE(curlwp);

	if (err != USBD_IN_PROGRESS) {
		aprint_error_dev(sc->sc_dev, "USB error: %s\n",
		    usbd_errstr(err));
		return ENODEV;
	}

	return 0;
}
