/* $NetBSD: emdtv_dtv.c,v 1.11 2015/04/02 06:23:04 skrll Exp $ */

/*-
 * Copyright (c) 2008, 2011 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: emdtv_dtv.c,v 1.11 2015/04/02 06:23:04 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/i2c/i2cvar.h>

#include <dev/usb/emdtvvar.h>
#include <dev/usb/emdtvreg.h>

static void		emdtv_dtv_get_devinfo(void *,
			    struct dvb_frontend_info *);
static int		emdtv_dtv_open(void *, int);
static void		emdtv_dtv_close(void *);
static int		emdtv_dtv_set_tuner(void *,
			    const struct dvb_frontend_parameters *);
static fe_status_t	emdtv_dtv_get_status(void *);
static uint16_t		emdtv_dtv_get_signal_strength(void *);
static uint16_t		emdtv_dtv_get_snr(void *);
static int		emdtv_dtv_start_transfer(void *,
			    void (*)(void *, const struct dtv_payload *),
			    void *);
static int		emdtv_dtv_stop_transfer(void *);

static int		emdtv_dtv_tuner_reset(void *);

static void		emdtv_dtv_isoc_startall(struct emdtv_softc *);
static int		emdtv_dtv_isoc_start(struct emdtv_softc *,
			    struct emdtv_isoc_xfer *);
static void		emdtv_dtv_isoc(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);

static const struct dtv_hw_if emdtv_dtv_if = {
	.get_devinfo = emdtv_dtv_get_devinfo,
	.open = emdtv_dtv_open,
	.close = emdtv_dtv_close,
	.set_tuner = emdtv_dtv_set_tuner,
	.get_status = emdtv_dtv_get_status,
	.get_signal_strength = emdtv_dtv_get_signal_strength,
	.get_snr = emdtv_dtv_get_snr,
	.start_transfer = emdtv_dtv_start_transfer,
	.stop_transfer = emdtv_dtv_stop_transfer,
};

void
emdtv_dtv_attach(struct emdtv_softc *sc)
{
	usb_endpoint_descriptor_t *ed;
	usbd_status status;
	int i;

	for (i = 0; i < EMDTV_NXFERS; i++) {
		sc->sc_ix[i].ix_altix = (i & 1) ?
		    &sc->sc_ix[i - 1] : &sc->sc_ix[i + 1];
		sc->sc_ix[i].ix_sc = sc;
	}

	ed = usbd_interface2endpoint_descriptor(sc->sc_iface, 3);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't find endpoint 3\n");
		return;
	}
	sc->sc_isoc_maxpacketsize = UGETW(ed->wMaxPacketSize);
	sc->sc_isoc_buflen = sc->sc_isoc_maxpacketsize * EMDTV_NFRAMES;

	aprint_debug_dev(sc->sc_dev, "calling usbd_open_pipe, ep 0x%02x\n",
	    ed->bEndpointAddress);
	status = usbd_open_pipe(sc->sc_iface, 
	    ed->bEndpointAddress, USBD_EXCLUSIVE_USE|USBD_MPSAFE,
	    &sc->sc_isoc_pipe);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't open isoc pipe\n");
		usbd_set_interface(sc->sc_iface, 0);
		return;
	}

	emdtv_write_1(sc, UR_GET_STATUS, 0x48, 0x00);
	emdtv_write_1(sc, UR_GET_STATUS, 0x12, 0x77);
	usbd_delay_ms(sc->sc_udev, 6);

	emdtv_gpio_ctl(sc, EMDTV_GPIO_ANALOG_ON, false);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_TS1_ON, true);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_TUNER1_ON, true);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_DEMOD1_RESET, true);
	usbd_delay_ms(sc->sc_udev, 100);

	emdtv_dtv_rescan(sc, NULL, NULL);
}

void
emdtv_dtv_detach(struct emdtv_softc *sc, int flags)
{
	sc->sc_streaming = false;

	if (sc->sc_dtvdev != NULL) {
		config_detach(sc->sc_dtvdev, flags);
		sc->sc_dtvdev = NULL;
	}

	if (sc->sc_xc3028)
		xc3028_close(sc->sc_xc3028);
	if (sc->sc_lg3303)
		lg3303_close(sc->sc_lg3303);

	if (sc->sc_isoc_pipe) {
		usbd_abort_pipe(sc->sc_isoc_pipe);
		usbd_close_pipe(sc->sc_isoc_pipe);
		sc->sc_isoc_pipe = NULL;
	}
}

void
emdtv_dtv_rescan(struct emdtv_softc *sc, const char *ifattr, const int *locs)
{
	struct dtv_attach_args daa;

	daa.hw = &emdtv_dtv_if;
	daa.priv = sc;

	if (ifattr_match(ifattr, "dtvbus") && sc->sc_dtvdev == NULL)
		sc->sc_dtvdev = config_found_ia(sc->sc_dev, "dtvbus",
		    &daa, dtv_print);
}

static void
emdtv_dtv_get_devinfo(void *priv, struct dvb_frontend_info *info)
{
	struct emdtv_softc *sc = priv;

	memset(info, 0, sizeof(*info));
	strlcpy(info->name, sc->sc_board->eb_name, sizeof(info->name));
	info->type = FE_ATSC;
	info->frequency_min = 54000000;
	info->frequency_max = 858000000;
	info->frequency_stepsize = 62500;
	info->caps = FE_CAN_8VSB;
}

static int
emdtv_dtv_open(void *priv, int flags)
{
	struct emdtv_softc *sc = priv;

	if (sc->sc_dying)
		return ENXIO;

	switch (sc->sc_board->eb_tuner) {
	case EMDTV_TUNER_XC3028:
		if (sc->sc_xc3028 == NULL) {
			sc->sc_xc3028 = xc3028_open(sc->sc_dev,
			    &sc->sc_i2c, 0x61 << 1, emdtv_dtv_tuner_reset, sc,
			    XC3028);
		}
		if (sc->sc_xc3028 == NULL) {
			aprint_error_dev(sc->sc_dev, "couldn't open xc3028\n");
			return ENXIO;
		}
		break;
	case EMDTV_TUNER_XC3028L:
		if (sc->sc_xc3028 == NULL) {
			sc->sc_xc3028 = xc3028_open(sc->sc_dev,
			    &sc->sc_i2c, 0x61 << 1, emdtv_dtv_tuner_reset, sc,
			    XC3028L);
		}
		if (sc->sc_xc3028 == NULL) {
			aprint_error_dev(sc->sc_dev, "couldn't open xc3028l\n");
			return ENXIO;
		}
		break;
	default:
		aprint_error_dev(sc->sc_dev, "unsupported tuner (%d)\n",
		    sc->sc_board->eb_tuner);
		return EIO;
	}

	switch (sc->sc_board->eb_demod) {
	case EMDTV_DEMOD_LG3303:
		if (sc->sc_lg3303 == NULL) {
			sc->sc_lg3303 = lg3303_open(sc->sc_dev,
			    &sc->sc_i2c, 0x1c, 0);
		}
		if (sc->sc_lg3303 == NULL) {
			aprint_error_dev(sc->sc_dev, "couldn't open lg3303\n");
			return ENXIO;
		}
		break;
	default:
		aprint_error_dev(sc->sc_dev, "unsupported demod (%d)\n",
		    sc->sc_board->eb_demod);
		return EIO;
	}

	return 0;
}

static void
emdtv_dtv_close(void *priv)
{
	return;
}

static int
emdtv_dtv_set_tuner(void *priv, const struct dvb_frontend_parameters *params)
{
	struct emdtv_softc *sc = priv;
	int error;

	/* Setup demod */
	error = ENXIO;
	if (sc->sc_lg3303)
		error = lg3303_set_modulation(sc->sc_lg3303,
		    params->u.vsb.modulation);
	if (error)
		return error;

	/* Setup tuner */
	error = ENXIO;
	if (sc->sc_xc3028)
		error = xc3028_tune_dtv(sc->sc_xc3028, params);

	return error;
}

static fe_status_t
emdtv_dtv_get_status(void *priv)
{
	struct emdtv_softc *sc = priv;

	if (sc->sc_lg3303)
		return lg3303_get_dtv_status(sc->sc_lg3303);

	return 0;
}

uint16_t
emdtv_dtv_get_signal_strength(void *priv)
{
	struct emdtv_softc *sc = priv;

	if (sc->sc_lg3303)
		return lg3303_get_signal_strength(sc->sc_lg3303);

	return 0;
}

uint16_t
emdtv_dtv_get_snr(void *priv)
{
	struct emdtv_softc *sc = priv;

	if (sc->sc_lg3303)
		return lg3303_get_snr(sc->sc_lg3303);

	return 0;
}

static int
emdtv_dtv_start_transfer(void *priv,
    void (*cb)(void *, const struct dtv_payload *), void *arg)
{
	struct emdtv_softc *sc = priv;
	int i, s;

	s = splusb();

	sc->sc_streaming = true;
	sc->sc_dtvsubmitcb = cb;
	sc->sc_dtvsubmitarg = arg;

	aprint_debug_dev(sc->sc_dev, "allocating isoc xfers (pktsz %d)\n",
	    sc->sc_isoc_maxpacketsize);

	KERNEL_LOCK(1, curlwp);
	for (i = 0; i < EMDTV_NXFERS; i++) {
		sc->sc_ix[i].ix_xfer = usbd_alloc_xfer(sc->sc_udev);
		sc->sc_ix[i].ix_buf = usbd_alloc_buffer(sc->sc_ix[i].ix_xfer,
		    sc->sc_isoc_buflen);
		aprint_debug_dev(sc->sc_dev, "  ix[%d] xfer %p buf %p\n",
		    i, sc->sc_ix[i].ix_xfer, sc->sc_ix[i].ix_buf);
	}
	KERNEL_UNLOCK_ONE(curlwp);

	aprint_debug_dev(sc->sc_dev, "starting isoc transactions\n");

	emdtv_dtv_isoc_startall(sc);
	splx(s);

	return 0;
}

static int
emdtv_dtv_stop_transfer(void *priv)
{
	struct emdtv_softc *sc = priv;
	int i;

	aprint_debug_dev(sc->sc_dev, "stopping stream\n");

	sc->sc_streaming = false;

	KERNEL_LOCK(1, curlwp);
	if (sc->sc_isoc_pipe != NULL)
		usbd_abort_pipe(sc->sc_isoc_pipe);

	for (i = 0; i < EMDTV_NXFERS; i++)
		if (sc->sc_ix[i].ix_xfer) {
			usbd_free_xfer(sc->sc_ix[i].ix_xfer);
			sc->sc_ix[i].ix_xfer = NULL;
			sc->sc_ix[i].ix_buf = NULL;
		}
	KERNEL_UNLOCK_ONE(curlwp);

	sc->sc_dtvsubmitcb = NULL;
	sc->sc_dtvsubmitarg = NULL;

	return 0;
}

static void
emdtv_dtv_isoc_startall(struct emdtv_softc *sc)
{
	int i;

	if (sc->sc_streaming == false || sc->sc_dying == true)
		return;

	for (i = 0; i < EMDTV_NXFERS; i += 2)
		emdtv_dtv_isoc_start(sc, &sc->sc_ix[i]);
}

static int
emdtv_dtv_isoc_start(struct emdtv_softc *sc, struct emdtv_isoc_xfer *ix)
{
	int i;

	if (sc->sc_isoc_pipe == NULL)
		return EIO;

	for (i = 0; i < EMDTV_NFRAMES; i++)
		ix->ix_frlengths[i] = sc->sc_isoc_maxpacketsize;

	usbd_setup_isoc_xfer(ix->ix_xfer,
			     sc->sc_isoc_pipe,
			     ix,
			     ix->ix_frlengths,
			     EMDTV_NFRAMES,
			     USBD_NO_COPY | USBD_SHORT_XFER_OK,
			     emdtv_dtv_isoc);

	KERNEL_LOCK(1, curlwp);
	usbd_transfer(ix->ix_xfer);
	KERNEL_UNLOCK_ONE(curlwp);

	return 0;
}

static void
emdtv_dtv_isoc(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status err)
{
	struct emdtv_isoc_xfer *ix = priv;
	struct emdtv_softc *sc = ix->ix_sc;
	struct dtv_payload payload;
	usbd_pipe_handle isoc = sc->sc_isoc_pipe;
	uint32_t len;
	uint8_t *buf;
	int i;

	KASSERT(xfer == ix->ix_xfer);

	if (sc->sc_dying || sc->sc_dtvsubmitcb == NULL)
		return;

	if (err) {
		if (err == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(isoc);
			goto resched;
		}
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len == 0)
		goto resched;

	buf = usbd_get_buffer(xfer);
	if (buf == NULL)
		goto resched;

	for (i = 0; i < EMDTV_NFRAMES; i++, buf += sc->sc_isoc_maxpacketsize) {
		if (ix->ix_frlengths[i] == 0)
			continue;
		payload.data = buf;
		payload.size = ix->ix_frlengths[i];
		sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);
	}

resched:
	emdtv_dtv_isoc_start(sc, ix->ix_altix);
}

static int
emdtv_dtv_tuner_reset(void *opaque)
{
	struct emdtv_softc *sc = opaque;
	emdtv_gpio_ctl(sc, EMDTV_GPIO_TUNER1_RESET, true);
	return 0;
}
