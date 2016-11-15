/*	$NetBSD: uyurex.c,v 1.10 2015/03/07 20:20:55 mrg Exp $ */
/*	$OpenBSD: uyurex.c,v 1.3 2010/03/04 03:47:22 deraadt Exp $ */

/*
 * Copyright (c) 2010 Yojiro UO <yuo@nui.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Driver for Maywa-Denki & KAYAC YUREX BBU sensor
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uyurex.c,v 1.10 2015/03/07 20:20:55 mrg Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/envsys.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#define	CMD_NONE	0xf0
#define CMD_EOF		0x0d
#define CMD_ACK		0x21
#define	CMD_MODE	0x41 /* XXX */
#define	CMD_VALUE	0x43
#define CMD_READ	0x52
#define CMD_WRITE	0x53
#define CMD_PADDING	0xff

#define UPDATE_TICK	5 /* sec */

#ifdef UYUREX_DEBUG
int	uyurexdebug = 0;
#define DPRINTFN(n, x)	do { if (uyurexdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)

struct uyurex_softc {
	struct uhidev		 sc_hdev;
	usbd_device_handle	 sc_udev;
	u_char			 sc_dying;
	uint16_t		 sc_flag;

	/* uhidev parameters */
	size_t			 sc_flen;	/* feature report length */
	size_t			 sc_ilen;	/* input report length */
	size_t			 sc_olen;	/* output report length */

	uint8_t			*sc_ibuf;

	/* sensor framework */
	struct sysmon_envsys	*sc_sme;
	envsys_data_t		 sc_sensor_val;
	envsys_data_t		 sc_sensor_delta;

	/* device private */
	int			 sc_initialized;
	uint8_t			 issueing_cmd;
	uint8_t			 accepted_cmd;

	uint32_t		 sc_curval;
	uint32_t		 sc_oldval;
	callout_t		 sc_deltach;
};

const struct usb_devno uyurex_devs[] = {
	{ USB_VENDOR_MICRODIA, USB_PRODUCT_MICRODIA_YUREX },
};
#define uyurex_lookup(v, p) usb_lookup(uyurex_devs, v, p)

int uyurex_match(device_t, cfdata_t, void *);
void uyurex_attach(device_t, device_t, void *);
int uyurex_detach(device_t, int);
int uyurex_activate(device_t, enum devact);

void uyurex_set_mode(struct uyurex_softc *, uint8_t);
void uyurex_read_value_request(struct uyurex_softc *);
void uyurex_write_value_request(struct uyurex_softc *, uint32_t);

void uyurex_intr(struct uhidev *, void *, u_int);
static void uyurex_refresh(struct sysmon_envsys *, envsys_data_t *);
static void uyurex_delta(void *);

extern struct cfdriver uyurex_cd;
CFATTACH_DECL_NEW(uyurex, sizeof(struct uyurex_softc),
    uyurex_match, uyurex_attach, uyurex_detach, uyurex_activate);

int 
uyurex_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;

	if (uyurex_lookup(uha->uaa->vendor, uha->uaa->product) == NULL)
		return UMATCH_NONE;

	return (UMATCH_VENDOR_PRODUCT);
}

void 
uyurex_attach(device_t parent, device_t self, void *aux)
{
	struct uyurex_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usbd_device_handle dev = uha->parent->sc_udev;
	int size, repid, err;
	void *desc;

	sc->sc_udev = dev;
	sc->sc_hdev.sc_dev = self;
	sc->sc_hdev.sc_intr = uyurex_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_ilen = hid_report_size(desc, size, hid_input, repid);
	sc->sc_olen = hid_report_size(desc, size, hid_output, repid);
	sc->sc_flen = hid_report_size(desc, size, hid_feature, repid);

	aprint_normal("\n");
	aprint_naive("\n");

	/*
	 * XXX uhidev_open enables the interrupt, so we should do it as
	 * one of the final things here.
	 */
	err = uhidev_open(&sc->sc_hdev);
	if (err) {
		aprint_error_dev(self, "uyurex_open: uhidev_open %d\n", err);
		return;
	}
	sc->sc_ibuf = malloc(sc->sc_ilen, M_USBDEV, M_WAITOK);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    sc->sc_hdev.sc_dev);

	/* attach sensor */
	sc->sc_sme = sysmon_envsys_create();
	/* error handling? XXX */
	sc->sc_sme->sme_name = device_xname(self);

	/* add BBU sensor */
	sc->sc_sensor_val.units = ENVSYS_INTEGER;
	sc->sc_sensor_val.state = ENVSYS_SINVALID;
	sc->sc_sensor_val.flags = ENVSYS_FMONCRITICAL;	/* abuse XXX */
	strlcpy(sc->sc_sensor_val.desc, "BBU",
		sizeof(sc->sc_sensor_val.desc));
	sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_val);

	/* add BBU delta sensor */
	sc->sc_sensor_delta.units = ENVSYS_INTEGER;
	sc->sc_sensor_delta.state = ENVSYS_SINVALID;
	strlcpy(sc->sc_sensor_delta.desc, "mBBU/sec",
		sizeof(sc->sc_sensor_delta.desc));
	sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_delta);

	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = uyurex_refresh;
	sc->sc_sme->sme_events_timeout = UPDATE_TICK;
	sc->sc_sme->sme_flags = SME_INIT_REFRESH;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(self, "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
	}

	callout_init(&sc->sc_deltach, 0);
	callout_reset(&sc->sc_deltach, UPDATE_TICK * hz, uyurex_delta, sc);

	DPRINTF(("uyurex_attach: complete\n"));

	/* init device */ /* XXX */
	uyurex_set_mode(sc, 0);
}

int 
uyurex_detach(device_t self, int flags)
{
	struct uyurex_softc *sc = device_private(self);
	int rv = 0;

	sc->sc_dying = 1;

	callout_halt(&sc->sc_deltach, NULL);
	callout_destroy(&sc->sc_deltach);
	sysmon_envsys_unregister(sc->sc_sme);

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_ibuf = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_hdev.sc_dev);

	return (rv);
}

int
uyurex_activate(device_t self, enum devact act)
{
	struct uyurex_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

void
uyurex_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct uyurex_softc *sc = (struct uyurex_softc *)addr;
	uint8_t buf[8];
	uint32_t val;

	if (sc->sc_ibuf == NULL)
		return;

	/* process requests */
	memcpy(buf, ibuf, 8);
	DPRINTF(("intr: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
		buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7]));


	switch (buf[0]) {
	case CMD_ACK:
		if (buf[1] == sc->issueing_cmd) {
			DPRINTF(("ack received for cmd 0x%.2x\n", buf[1]));
			sc->accepted_cmd = buf[1];
		} else {
			DPRINTF(("cmd-ack mismatch: received 0x%.2x, "
			    "expect 0x%.2x\n", buf[1], sc->issueing_cmd));
			/* discard previous command */
			sc->accepted_cmd = CMD_NONE;
			sc->issueing_cmd = CMD_NONE;
		}
		break;
	case CMD_READ:
	case CMD_VALUE:
		val = (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8)  + buf[5];
		if (!sc->sc_initialized) {
			sc->sc_oldval = val;
			sc->sc_initialized = 1;
		}
		sc->sc_sensor_val.value_cur = val;
		sc->sc_sensor_val.state = ENVSYS_SVALID;
		sc->sc_curval = val;
		DPRINTF(("recv value update message: %d\n", val));
		break;
	default:
		DPRINTF(("unknown message: 0x%.2x\n", buf[0]));
	}

	return;
}

static void
uyurex_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct uyurex_softc *sc = sme->sme_cookie;
	DPRINTF(("refresh: edata = %p\n", edata));

	if (edata != &sc->sc_sensor_val)
		return;

	if (!sc->sc_initialized) {
		DPRINTF(("refresh: notinit\n"));
		uyurex_read_value_request(sc);
	}
}

static void
uyurex_delta(void *arg)
{
	struct uyurex_softc *sc = arg;
	envsys_data_t *edata = &sc->sc_sensor_delta;

	/* calculate delta value */
	edata->value_cur =
	    (1000 * (sc->sc_curval - sc->sc_oldval)) / UPDATE_TICK;
	edata->state = ENVSYS_SVALID;
#if 0
	DPRINTF(("delta: update: %d -> %d\n", sc->sc_oldval, sc->sc_curval));
#endif
	sc->sc_oldval = sc->sc_curval;
	callout_reset(&sc->sc_deltach, UPDATE_TICK * hz, uyurex_delta, sc);
}

void
uyurex_set_mode(struct uyurex_softc *sc, uint8_t val)
{
	uint8_t req[8];
	usbd_status err;

	memset(req, CMD_PADDING, sizeof(req));
	req[0] = CMD_MODE;
	req[1] = val;
	req[2] = CMD_EOF;
	sc->issueing_cmd = CMD_MODE;		/* necessary? */
	sc->accepted_cmd = CMD_NONE;		/* necessary? */
	err = uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen);
	if (err) {
		printf("uhidev_set_report error:EIO\n");
		return;
	}

	/* wait ack */
	tsleep(&sc->sc_sme, 0, "uyurex", (1000*hz+999)/1000 + 1);
}

void
uyurex_read_value_request(struct uyurex_softc *sc)
{
	uint8_t req[8];

	memset(req, CMD_PADDING, sizeof(req));
	req[0] = CMD_READ;
	req[1] = CMD_EOF;
	sc->issueing_cmd = CMD_READ;
	sc->accepted_cmd = CMD_NONE;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen))
		return;

	/* wait till sensor data are updated, 500ms will be enough */
	tsleep(&sc->sc_sme, 0, "uyurex", (500*hz+999)/1000 + 1);
}

void
uyurex_write_value_request(struct uyurex_softc *sc, uint32_t val)
{
	uint32_t v;
	uint8_t req[8];

	req[0] = CMD_WRITE;
	req[1] = 0;
	req[6] = CMD_EOF;
	req[7] = CMD_PADDING;
	v = htobe32(val);
	memcpy(req + 2, &v, sizeof(uint32_t));

	sc->issueing_cmd = CMD_WRITE;
	sc->accepted_cmd = CMD_NONE;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT, req,
		sc->sc_olen))
		return;

	/* wait till sensor data are updated, 250ms will be enough */
	tsleep(&sc->sc_sme, 0, "uyurex", (250*hz+999)/1000 + 1);
}
