/* $NetBSD: irmce.c,v 1.1 2011/07/19 12:23:04 jmcneill Exp $ */

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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
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
 * IR receiver/transceiver for Windows Media Center
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irmce.c,v 1.1 2011/07/19 12:23:04 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/select.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/ir/ir.h>
#include <dev/ir/cirio.h>
#include <dev/ir/cirvar.h>

enum irmce_state {
	IRMCE_STATE_HEADER,
	IRMCE_STATE_IRDATA,
	IRMCE_STATE_CMDHEADER,
	IRMCE_STATE_CMDDATA,
};

struct irmce_softc {
	device_t		sc_dev;
	device_t		sc_cirdev;

	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	int			sc_bulkin_ep;
	uint16_t		sc_bulkin_maxpktsize;
	usbd_pipe_handle	sc_bulkin_pipe;
	usbd_xfer_handle	sc_bulkin_xfer;
	uint8_t *		sc_bulkin_buffer;

	int			sc_bulkout_ep;
	uint16_t		sc_bulkout_maxpktsize;
	usbd_pipe_handle	sc_bulkout_pipe;
	usbd_xfer_handle	sc_bulkout_xfer;
	uint8_t *		sc_bulkout_buffer;

	bool			sc_raw;

	uint8_t			sc_ir_buf[16];
	size_t			sc_ir_bufused;
	size_t			sc_ir_resid;
	enum irmce_state	sc_ir_state;
	uint8_t			sc_ir_header;

	bool			sc_rc6_hb[256];
	size_t			sc_rc6_nhb;
};

static int	irmce_match(device_t, cfdata_t, void *);
static void	irmce_attach(device_t, device_t, void *);
static int	irmce_detach(device_t, int);
static void	irmce_childdet(device_t, device_t);
static int	irmce_activate(device_t, enum devact);
static int	irmce_rescan(device_t, const char *, const int *);

static int	irmce_print(void *, const char *);

static int	irmce_reset(struct irmce_softc *);

static int	irmce_open(void *, int, int, struct proc *);
static int	irmce_close(void *, int, int, struct proc *);
static int	irmce_read(void *, struct uio *, int);
static int	irmce_write(void *, struct uio *, int);
static int	irmce_setparams(void *, struct cir_params *);

static const struct cir_methods irmce_cir_methods = {
	.im_open = irmce_open,
	.im_close = irmce_close,
	.im_read = irmce_read,
	.im_write = irmce_write,
	.im_setparams = irmce_setparams,
};

static const struct {
	uint16_t		vendor;
	uint16_t		product;
} irmce_devices[] = {
	{ USB_VENDOR_SMK, USB_PRODUCT_SMK_MCE_IR },
};

CFATTACH_DECL2_NEW(irmce, sizeof(struct irmce_softc),
    irmce_match, irmce_attach, irmce_detach, irmce_activate,
    irmce_rescan, irmce_childdet);

static int
irmce_match(device_t parent, cfdata_t match, void *opaque)
{
	struct usbif_attach_arg *uiaa = opaque;
	unsigned int i;

	for (i = 0; i < __arraycount(irmce_devices); i++) {
		if (irmce_devices[i].vendor == uiaa->vendor &&
		    irmce_devices[i].product == uiaa->product)
			return UMATCH_VENDOR_PRODUCT;
	}

	return UMATCH_NONE;
}

static void
irmce_attach(device_t parent, device_t self, void *opaque)
{
	struct irmce_softc *sc = device_private(self);
	struct usbif_attach_arg *uiaa = opaque;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	unsigned int i;
	uint8_t nep;

	pmf_device_register(self, NULL, NULL);

	aprint_naive("\n");

	devinfop = usbd_devinfo_alloc(uiaa->device, 0);
	aprint_normal(": %s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;
	sc->sc_udev = uiaa->device;
	sc->sc_iface = uiaa->iface;

	nep = 0;
	usbd_endpoint_count(sc->sc_iface, &nep);
	sc->sc_bulkin_ep = sc->sc_bulkout_ep = -1;
	for (i = 0; i < nep; i++) {
		int dir, type;

		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "couldn't read endpoint descriptor %d\n", i);
			continue;
		}

		dir = UE_GET_DIR(ed->bEndpointAddress);
		type = UE_GET_XFERTYPE(ed->bmAttributes);

		if (type != UE_BULK)
			continue;

		if (dir == UE_DIR_IN && sc->sc_bulkin_ep == -1) {
			sc->sc_bulkin_ep = ed->bEndpointAddress;
			sc->sc_bulkin_maxpktsize =
			    UE_GET_SIZE(UGETW(ed->wMaxPacketSize)) *
			    (UE_GET_TRANS(UGETW(ed->wMaxPacketSize)) + 1);
		}
		if (dir == UE_DIR_OUT && sc->sc_bulkout_ep == -1) {
			sc->sc_bulkout_ep = ed->bEndpointAddress;
			sc->sc_bulkout_maxpktsize =
			    UE_GET_SIZE(UGETW(ed->wMaxPacketSize)) *
			    (UE_GET_TRANS(UGETW(ed->wMaxPacketSize)) + 1);
		}
	}

	aprint_debug_dev(self, "in 0x%02x/%d out 0x%02x/%d\n",
	    sc->sc_bulkin_ep, sc->sc_bulkin_maxpktsize,
	    sc->sc_bulkout_ep, sc->sc_bulkout_maxpktsize);

	if (sc->sc_bulkin_maxpktsize < 16 || sc->sc_bulkout_maxpktsize < 16) {
		aprint_error_dev(self, "bad maxpktsize\n");
		return;
	}

	sc->sc_bulkin_xfer = usbd_alloc_xfer(sc->sc_udev);
	sc->sc_bulkout_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkin_xfer == NULL || sc->sc_bulkout_xfer == NULL) {
		aprint_error_dev(self, "couldn't alloc xfer\n");
		return;
	}
	sc->sc_bulkin_buffer = usbd_alloc_buffer(sc->sc_bulkin_xfer,
	    sc->sc_bulkin_maxpktsize);
	sc->sc_bulkout_buffer = usbd_alloc_buffer(sc->sc_bulkout_xfer,
	    sc->sc_bulkout_maxpktsize);
	if (sc->sc_bulkin_buffer == NULL || sc->sc_bulkout_buffer == NULL) {
		aprint_error_dev(self, "couldn't alloc xfer buffer\n");
		return;
	}

	irmce_rescan(self, NULL, NULL);
}

static int
irmce_detach(device_t self, int flags)
{
	struct irmce_softc *sc = device_private(self);
	int error;

	if (sc->sc_cirdev) {
		error = config_detach(sc->sc_cirdev, flags);
		if (error)
			return error;
	}

	if (sc->sc_bulkin_xfer) {
		usbd_free_xfer(sc->sc_bulkin_xfer);
		sc->sc_bulkin_buffer = NULL;
		sc->sc_bulkin_xfer = NULL;
	}
	if (sc->sc_bulkout_xfer) {
		usbd_free_xfer(sc->sc_bulkout_xfer);
		sc->sc_bulkout_buffer = NULL;
		sc->sc_bulkout_xfer = NULL;
	}

	pmf_device_deregister(self);

	return 0;
}

static int
irmce_activate(device_t self, enum devact act)
{
	return 0;
}

static int
irmce_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct irmce_softc *sc = device_private(self);
	struct ir_attach_args iaa;

	if (sc->sc_cirdev == NULL) {
		iaa.ia_type = IR_TYPE_CIR;
		iaa.ia_methods = &irmce_cir_methods;
		iaa.ia_handle = sc;
		sc->sc_cirdev = config_found_ia(self, "irbus",
		    &iaa, irmce_print);
	}

	return 0;
}

static int
irmce_print(void *priv, const char *pnp)
{
	if (pnp)
		aprint_normal("cir at %s", pnp);

	return UNCONF;
}

static void
irmce_childdet(device_t self, device_t child)
{
	struct irmce_softc *sc = device_private(self);

	if (sc->sc_cirdev == child)
		sc->sc_cirdev = NULL;
}

static int
irmce_reset(struct irmce_softc *sc)
{
	static const uint8_t reset_cmd[] = { 0x00, 0xff, 0xaa };
	uint8_t *p = sc->sc_bulkout_buffer;
	usbd_status err;
	uint32_t wlen;
	unsigned int n;

	for (n = 0; n < __arraycount(reset_cmd); n++)
		*p++ = reset_cmd[n];

	wlen = sizeof(reset_cmd);
	err = usbd_bulk_transfer(sc->sc_bulkin_xfer,
	    sc->sc_bulkout_pipe, USBD_NO_COPY|USBD_FORCE_SHORT_XFER,
	    USBD_DEFAULT_TIMEOUT, sc->sc_bulkout_buffer, &wlen,
	    "irmcereset");
	if (err != USBD_NORMAL_COMPLETION) {
		if (err == USBD_INTERRUPTED)
			return EINTR;
		else if (err == USBD_TIMEOUT)
			return ETIMEDOUT;
		else
			return EIO;
	}

	return 0;
}

static int
irmce_open(void *priv, int flag, int mode, struct proc *p)
{
	struct irmce_softc *sc = priv;
	usbd_status err;

	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin_ep,
	    USBD_EXCLUSIVE_USE, &sc->sc_bulkin_pipe);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't open bulk-in pipe: %s\n", usbd_errstr(err));
		return ENXIO;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout_ep,
	    USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't open bulk-out pipe: %s\n", usbd_errstr(err));
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
		return ENXIO;
	}

	err = irmce_reset(sc);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't reset device: %s\n", usbd_errstr(err));
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
		usbd_close_pipe(sc->sc_bulkout_pipe);
		sc->sc_bulkout_pipe = NULL;
	}
	sc->sc_ir_state = IRMCE_STATE_HEADER;
	sc->sc_rc6_nhb = 0;

	return 0;
}

static int
irmce_close(void *priv, int flag, int mode, struct proc *p)
{
	struct irmce_softc *sc = priv;

	if (sc->sc_bulkin_pipe) {
		usbd_abort_pipe(sc->sc_bulkin_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
	}
	if (sc->sc_bulkout_pipe) {
		usbd_abort_pipe(sc->sc_bulkout_pipe);
		usbd_close_pipe(sc->sc_bulkout_pipe);
		sc->sc_bulkout_pipe = NULL;
	}

	return 0;
}

static int
irmce_rc6_decode(struct irmce_softc *sc, uint8_t *buf, size_t buflen,
    struct uio *uio)
{
	bool *hb = &sc->sc_rc6_hb[0];
	unsigned int n;
	int state, pulse;
	uint32_t data;
	uint8_t mode;
	bool idle = false;

	for (n = 0; n < buflen; n++) {
		state = (buf[n] & 0x80) ? 1 : 0;
		pulse = (buf[n] & 0x7f) * 50;

		if (pulse >= 300 && pulse <= 600) {
			hb[sc->sc_rc6_nhb++] = state;
		} else if (pulse >= 680 && pulse <= 1080) {
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
		} else if (pulse >= 1150 && pulse <= 1450) {
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
		} else if (pulse >= 2400 && pulse <= 2800) {
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
			hb[sc->sc_rc6_nhb++] = state;
		} else if (pulse > 3000) {
			if (sc->sc_rc6_nhb & 1)
				hb[sc->sc_rc6_nhb++] = state;
			idle = true;
			break;
		} else {
			aprint_debug_dev(sc->sc_dev,
			    "error parsing RC6 stream (pulse=%d)\n", pulse);
			return EIO;
		}
	}

	if (!idle)
		return 0;

	if (sc->sc_rc6_nhb < 20) {
		aprint_debug_dev(sc->sc_dev, "not enough RC6 data\n");
		return EIO;
	}

	/* RC6 leader 11111100 */
	if (!hb[0] || !hb[1] || !hb[2] || !hb[3] || !hb[4] || !hb[5] ||
	    hb[6] || hb[7]) {
		aprint_debug_dev(sc->sc_dev, "bad RC6 leader\n");
		return EIO;
	}

	/* start bit 10 */
	if (!hb[8] || hb[9]) {
		aprint_debug_dev(sc->sc_dev, "missing RC6 start bit\n");
		return EIO;
	}

	/* mode info */
	mode = 0x00;
	for (n = 10; n < 15; n += 2) {
		if (hb[n] && !hb[n + 1])
			mode = (mode << 1) | 1;
		else if (!hb[n] && hb[n + 1])
			mode = (mode << 1) | 0;
		else {
			aprint_debug_dev(sc->sc_dev, "bad RC6 mode bits\n");
			return EIO;
		}
	}

	data = 0;
	for (n = 20; n < sc->sc_rc6_nhb; n += 2) {
		if (hb[n] && !hb[n + 1])
			data = (data << 1) | 1;
		else if (!hb[n] && hb[n + 1])
			data = (data << 1) | 0;
		else {
			aprint_debug_dev(sc->sc_dev, "bad RC6 data bits\n");
			return EIO;
		}
	}

	sc->sc_rc6_nhb = 0;

	return uiomove(&data, sizeof(data), uio);
}

static int
irmce_process(struct irmce_softc *sc, uint8_t *buf, size_t buflen,
    struct uio *uio)
{
	uint8_t *p = buf;
	uint8_t data, cmd;
	int error;

	while (p - buf < (ssize_t)buflen) {
		switch (sc->sc_ir_state) {
		case IRMCE_STATE_HEADER:
			sc->sc_ir_header = data = *p++;
			if ((data & 0xe0) == 0x80 && (data & 0x1f) != 0x1f) {
				sc->sc_ir_bufused = 0;
				sc->sc_ir_resid = data & 0x1f;
				sc->sc_ir_state = IRMCE_STATE_IRDATA;
				if (sc->sc_ir_resid > sizeof(sc->sc_ir_buf))
					return EIO;
				if (sc->sc_ir_resid == 0)
					sc->sc_ir_state = IRMCE_STATE_HEADER;
			} else {
				sc->sc_ir_state = IRMCE_STATE_CMDHEADER;
			}
			break;
		case IRMCE_STATE_CMDHEADER:
			cmd = *p++;
			data = sc->sc_ir_header;
			if (data == 0x00 && cmd == 0x9f)
				sc->sc_ir_resid = 1;
			else if (data == 0xff && cmd == 0x0b)
				sc->sc_ir_resid = 2;
			else if (data == 0x9f) {
				if (cmd == 0x04 || cmd == 0x06 ||
				    cmd == 0x0c || cmd == 0x15) {
					sc->sc_ir_resid = 2;
				} else if (cmd == 0x01 || cmd == 0x08 ||
				    cmd == 0x14) {
					sc->sc_ir_resid = 1;
				}
			}
			if (sc->sc_ir_resid > 0)
				sc->sc_ir_state = IRMCE_STATE_CMDDATA;
			else
				sc->sc_ir_state = IRMCE_STATE_HEADER;
			break;
		case IRMCE_STATE_IRDATA:
			sc->sc_ir_resid--;
			sc->sc_ir_buf[sc->sc_ir_bufused++] = *p;
			p++;
			if (sc->sc_ir_resid == 0) {
				sc->sc_ir_state = IRMCE_STATE_HEADER;
				error = irmce_rc6_decode(sc,
				    sc->sc_ir_buf, sc->sc_ir_bufused, uio);
				if (error)
					sc->sc_rc6_nhb = 0;
			}
			break;
		case IRMCE_STATE_CMDDATA:
			p++;
			sc->sc_ir_resid--;
			if (sc->sc_ir_resid == 0)
				sc->sc_ir_state = IRMCE_STATE_HEADER;
			break;
		}

	}

	return 0;
}

static int
irmce_read(void *priv, struct uio *uio, int flag)
{
	struct irmce_softc *sc = priv;
	usbd_status err;
	uint32_t rlen;
	int error = 0;

	while (uio->uio_resid > 0) {
		rlen = sc->sc_bulkin_maxpktsize;
		err = usbd_bulk_transfer(sc->sc_bulkin_xfer,
		    sc->sc_bulkin_pipe, USBD_NO_COPY|USBD_SHORT_XFER_OK,
		    USBD_DEFAULT_TIMEOUT, sc->sc_bulkin_buffer, &rlen,
		    "irmcerd");
		if (err != USBD_NORMAL_COMPLETION) {
			if (err == USBD_INTERRUPTED)
				return EINTR;
			else if (err == USBD_TIMEOUT)
				continue;
			else
				return EIO;
		}

		if (sc->sc_raw) {
			error = uiomove(sc->sc_bulkin_buffer, rlen, uio);
			break;
		} else {
			error = irmce_process(sc, sc->sc_bulkin_buffer,
			    rlen, uio);
			if (error)
				break;
		}
	}

	return error;
}

static int
irmce_write(void *priv, struct uio *uio, int flag)
{
	return EIO;
}

static int
irmce_setparams(void *priv, struct cir_params *params)
{
	struct irmce_softc *sc = priv;

	if (params->raw > 1)
		return EINVAL;
	sc->sc_raw = params->raw;

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, irmce, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
irmce_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_irmce,
		    cfattach_ioconf_irmce, cfdata_ioconf_irmce);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_irmce,
		    cfattach_ioconf_irmce, cfdata_ioconf_irmce);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
