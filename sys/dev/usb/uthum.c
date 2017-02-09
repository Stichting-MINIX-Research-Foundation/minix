/*	$NetBSD: uthum.c,v 1.10 2013/01/05 23:34:21 christos Exp $   */
/*	$OpenBSD: uthum.c,v 1.6 2010/01/03 18:43:02 deraadt Exp $   */

/*
 * Copyright (c) 2009 Yojiro UO <yuo@nui.org>
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
 * Driver for TEMPer and TEMPerHUM HID
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uthum.c,v 1.10 2013/01/05 23:34:21 christos Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#ifdef UTHUM_DEBUG
int	uthumdebug = 0;
#define DPRINTFN(n, x)	do { if (uthumdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)


/* TEMPerHUM */
#define CMD_DEVTYPE 0x52 /* XXX */
#define CMD_GETDATA 0x48 /* XXX */
#define CMD_GETTEMP 0x54 /* XXX */
static uint8_t cmd_start[8] =
	{ 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x00, 0x02, 0x00 };
static uint8_t cmd_end[8] =
	{ 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x00, 0x01, 0x00 };

/* sensors */
#define UTHUM_TEMP		0
#define UTHUM_HUMIDITY		1
#define UTHUM_MAX_SENSORS	2

#define UTHUM_TYPE_UNKNOWN	0
#define UTHUM_TYPE_SHT1x	1
#define UTHUM_TYPE_TEMPER	2

struct uthum_softc {
	struct uhidev		 sc_hdev;
	usbd_device_handle	 sc_udev;
	u_char			 sc_dying;
	uint16_t		 sc_flag;
	int			 sc_sensortype;

	/* uhidev parameters */
	size_t			 sc_flen;	/* feature report length */
	size_t			 sc_ilen;	/* input report length */
	size_t			 sc_olen;	/* output report length */

	/* sensor framework */
	struct sysmon_envsys		 *sc_sme;
	envsys_data_t			 sc_sensor[UTHUM_MAX_SENSORS];

	uint8_t			 sc_num_sensors;
};


const struct usb_devno uthum_devs[] = {
	/* XXX: various TEMPer variants using same VID/PID */
	{ USB_VENDOR_TENX, USB_PRODUCT_TENX_TEMPER},
};
#define uthum_lookup(v, p) usb_lookup(uthum_devs, v, p)

int uthum_match(device_t, cfdata_t, void *);
void uthum_attach(device_t, device_t, void *);
void uthum_childdet(device_t, device_t);
int uthum_detach(device_t, int);
int uthum_activate(device_t, enum devact);

int uthum_read_data(struct uthum_softc *, uint8_t, uint8_t *, size_t, int);
int uthum_check_sensortype(struct uthum_softc *);
int uthum_temper_temp(uint8_t, uint8_t);
int uthum_sht1x_temp(uint8_t, uint8_t);
int uthum_sht1x_rh(unsigned int, int);

void uthum_intr(struct uhidev *, void *, u_int);
static void uthum_refresh(struct sysmon_envsys *, envsys_data_t *);

extern struct cfdriver uthum_cd;
CFATTACH_DECL_NEW(uthum, sizeof(struct uthum_softc), uthum_match, uthum_attach,
    uthum_detach, uthum_activate);

int
uthum_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;

	return (uthum_lookup(uha->uaa->vendor, uha->uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void
uthum_attach(device_t parent, device_t self, void *aux)
{
	struct uthum_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usbd_device_handle dev = uha->parent->sc_udev;
	int size, repid;
	void *desc;

	sc->sc_udev = dev;
	sc->sc_hdev.sc_dev = self;
	sc->sc_hdev.sc_intr = uthum_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;
	sc->sc_num_sensors = 0;

	aprint_normal("\n");
	aprint_naive("\n");

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_ilen = hid_report_size(desc, size, hid_input, repid);
	sc->sc_olen = hid_report_size(desc, size, hid_output, repid);
	sc->sc_flen = hid_report_size(desc, size, hid_feature, repid);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    sc->sc_hdev.sc_dev);

	if (sc->sc_flen < 32) {
		/* not sensor interface, just attach */
		return;
	}

	sc->sc_sensortype = uthum_check_sensortype(sc);

	/* attach sensor */
	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(self);

	switch (sc->sc_sensortype) {
	case UTHUM_TYPE_SHT1x:
		(void)strlcpy(sc->sc_sensor[UTHUM_TEMP].desc, "temp",
		    sizeof(sc->sc_sensor[UTHUM_TEMP].desc));
		sc->sc_sensor[UTHUM_TEMP].units = ENVSYS_STEMP;
		sc->sc_sensor[UTHUM_TEMP].state = ENVSYS_SINVALID;

		(void)strlcpy(sc->sc_sensor[UTHUM_HUMIDITY].desc,
		    "relative humidity",
		    sizeof(sc->sc_sensor[UTHUM_HUMIDITY].desc));
		sc->sc_sensor[UTHUM_HUMIDITY].units = ENVSYS_INTEGER;
		sc->sc_sensor[UTHUM_HUMIDITY].value_cur = 0;
		sc->sc_sensor[UTHUM_HUMIDITY].state = ENVSYS_SINVALID;

		sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor[UTHUM_TEMP]);
		sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor[UTHUM_HUMIDITY]);
		sc->sc_num_sensors = 2;
		DPRINTF(("sensor type: SHT1x\n"));
		break;
	case UTHUM_TYPE_TEMPER:
		(void)strlcpy(sc->sc_sensor[UTHUM_TEMP].desc, "temp",
		    sizeof(sc->sc_sensor[UTHUM_TEMP].desc));
		sc->sc_sensor[UTHUM_TEMP].units = ENVSYS_STEMP;
		sc->sc_sensor[UTHUM_TEMP].state = ENVSYS_SINVALID;

		sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor[UTHUM_TEMP]);
		sc->sc_num_sensors = 1;
		DPRINTF(("sensor type: TEMPer\n"));
		break;
	case UTHUM_TYPE_UNKNOWN:
		DPRINTF(("sensor type: unknown, give up to attach sensors\n"));
	default:
		break;
	}

	if (sc->sc_num_sensors > 0) {
		sc->sc_sme->sme_cookie = sc;
		sc->sc_sme->sme_refresh = uthum_refresh;

		if (sysmon_envsys_register(sc->sc_sme)) {
			aprint_error_dev(self, "unable to register with sysmon\n");
			sysmon_envsys_destroy(sc->sc_sme);
		}
	} else {
		sysmon_envsys_destroy(sc->sc_sme);
	}

	DPRINTF(("uthum_attach: complete\n"));
}

int
uthum_detach(device_t self, int flags)
{
	struct uthum_softc *sc = device_private(self);
	int rv = 0;

	sc->sc_dying = 1;

	if (sc->sc_num_sensors > 0) {
		sysmon_envsys_unregister(sc->sc_sme);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_hdev.sc_dev);

	return (rv);
}

int
uthum_activate(device_t self, enum devact act)
{
	struct uthum_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

void
uthum_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	/* do nothing */
}

int
uthum_read_data(struct uthum_softc *sc, uint8_t target_cmd, uint8_t *buf,
	size_t len, int need_delay)
{
	int i;
	uint8_t cmdbuf[32], report[256];

	/* if return buffer is null, do nothing */
	if ((buf == NULL) || len == 0)
		return 0;

	/* issue query */
	bzero(cmdbuf, sizeof(cmdbuf));
	memcpy(cmdbuf, cmd_start, sizeof(cmd_start));
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	bzero(cmdbuf, sizeof(cmdbuf));
	cmdbuf[0] = target_cmd;
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	bzero(cmdbuf, sizeof(cmdbuf));
	for (i = 0; i < 7; i++) {
		if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
		    cmdbuf, sc->sc_olen))
			return EIO;
	}
	memcpy(cmdbuf, cmd_end, sizeof(cmd_end));
	if (uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
	    cmdbuf, sc->sc_olen))
		return EIO;

	/* wait if required */
	if (need_delay > 1)
		tsleep(&sc->sc_sme, 0, "uthum", (need_delay*hz+999)/1000 + 1);

	/* get answer */
	if (uhidev_get_report(&sc->sc_hdev, UHID_FEATURE_REPORT,
	    report, sc->sc_flen))
		return EIO;
	memcpy(buf, report, len);
	return 0;
}

int
uthum_check_sensortype(struct uthum_softc *sc)
{
	uint8_t buf[8];
	static uint8_t sht1x_sig0[] =
	    { 0x57, 0x5a, 0x13, 0x00, 0x14, 0x00, 0x53, 0x00 };
	static uint8_t sht1x_sig1[] =
	    { 0x57, 0x5a, 0x14, 0x00, 0x14, 0x00, 0x53, 0x00 };
	static uint8_t temper_sig[] =
	    { 0x57, 0x58, 0x14, 0x00, 0x14, 0x00, 0x53, 0x00 };

	if (uthum_read_data(sc, CMD_DEVTYPE, buf, sizeof(buf), 0) != 0) {
		DPRINTF(("uthum: read fail\n"));
		return UTHUM_TYPE_UNKNOWN;
	}

	/*
	 * currently we have not enough information about the return value,
	 * therefore, compare full bytes.
	 * TEMPerHUM HID (SHT1x version) will return:
	 *   { 0x57, 0x5a, 0x13, 0x00, 0x14, 0x00, 0x53, 0x00 }
	 *   { 0x57, 0x5a, 0x14, 0x00, 0x14, 0x00, 0x53, 0x00 }
	 * TEMPer HID (TEMPer version) will return:
	 *   { 0x57, 0x58, 0x14, 0x00, 0x14, 0x00, 0x53, 0x00 }
	 */
	DPRINTF(("uthum: device signature: "
	    "0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x\n",
	    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]));
	if (0 == memcmp(buf, sht1x_sig0, sizeof(sht1x_sig0)))
		return UTHUM_TYPE_SHT1x;
	if (0 == memcmp(buf, sht1x_sig1, sizeof(sht1x_sig1)))
		return UTHUM_TYPE_SHT1x;
	if (0 == memcmp(buf, temper_sig, sizeof(temper_sig)))
		return UTHUM_TYPE_TEMPER;

	return UTHUM_TYPE_UNKNOWN;
}


static void
uthum_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct uthum_softc *sc = sme->sme_cookie;
	uint8_t buf[8];
	unsigned int humidity_tick;
	int temp, rh;

	switch (sc->sc_sensortype) {
	case UTHUM_TYPE_SHT1x:
		if (uthum_read_data(sc, CMD_GETDATA, buf, sizeof(buf), 1000) != 0) {
			DPRINTF(("uthum: data read fail\n"));
			sc->sc_sensor[UTHUM_TEMP].state = ENVSYS_SINVALID;
			sc->sc_sensor[UTHUM_HUMIDITY].state = ENVSYS_SINVALID;
			return;
		}
		DPRINTF(("%s: read SHT1x data "
		    "0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x\n",
		    sc->sc_sme->sme_name, buf[0], buf[1], buf[2], buf[3],
		    buf[4], buf[5], buf[6], buf[7]));

		humidity_tick = (buf[2] * 256 + buf[3]) & 0x0fff;

		temp = uthum_sht1x_temp(buf[0], buf[1]);
		rh = uthum_sht1x_rh(humidity_tick, temp);

		sc->sc_sensor[UTHUM_HUMIDITY].value_cur = rh / 1000;
		sc->sc_sensor[UTHUM_HUMIDITY].state = ENVSYS_SVALID;
		break;
	case UTHUM_TYPE_TEMPER:
		if (uthum_read_data(sc, CMD_GETTEMP, buf, sizeof(buf), 0) != 0) {
			DPRINTF(("uthum: data read fail\n"));
			sc->sc_sensor[UTHUM_TEMP].state = ENVSYS_SINVALID;
			return;
		}
		DPRINTF(("%s: read TEMPER data "
		    "0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x 0x%0x\n",
		    sc->sc_sme->sme_name, buf[0], buf[1], buf[2], buf[3],
		    buf[4], buf[5], buf[6], buf[7]));
		temp = uthum_temper_temp(buf[0], buf[1]);
		break;
	default:
		/* do nothing */
		return;
	}

	sc->sc_sensor[UTHUM_TEMP].value_cur = (temp * 10000) + 273150000;
	sc->sc_sensor[UTHUM_TEMP].state = ENVSYS_SVALID;
}

/* return C-degree * 100 value */
int
uthum_temper_temp(uint8_t msb, uint8_t lsb)
{
	int val;

	val = (msb << 8) | lsb;
	if (val >= 32768) {
		val = val - 65536;
	}
	val = (val * 100) >> 8;
	return val;
}

/* return C-degree * 100 value */
int
uthum_sht1x_temp(uint8_t msb, uint8_t lsb)
{
	int val;

	val = ((msb << 8) + lsb) - 4096;
	return val;
}

/* return %RH * 1000 */
int
uthum_sht1x_rh(unsigned int ticks, int temp)
{
	int rh_l, rh;

	rh_l = (-40000 + 405 * ticks) - ((7 * ticks * ticks) / 250);
	rh = ((temp - 2500) * (1 + (ticks >> 7)) + rh_l) / 10;
	return rh;
}
