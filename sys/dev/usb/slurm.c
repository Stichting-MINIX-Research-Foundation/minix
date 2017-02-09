/*	$NetBSD: slurm.c,v 1.1 2013/01/13 01:15:02 jakllsch Exp $ */

/*
 * Copyright (c) 2012 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: slurm.c,v 1.1 2013/01/13 01:15:02 jakllsch Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/ic/si470x_reg.h>

#include <sys/radioio.h>
#include <dev/radio_if.h>

#ifdef SLURM_DEBUG
int	slurmdebug = 0;
#define DPRINTFN(n, x)	do { if (slurmdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)

#define SI470X_VOLFACT (255 / __SHIFTOUT_MASK(SI470X_VOLUME))

struct slurm_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_uif;
	uint32_t		sc_band;
	uint32_t		sc_space;
};

static const struct usb_devno slurm_devs[] = {
	{ USB_VENDOR_ADS, USB_PRODUCT_ADS_RDX155 },
};

static int slurm_match(device_t, cfdata_t, void *);
static void slurm_attach(device_t, device_t, void *);
static int slurm_detach(device_t, int);

static int slurm_get_info(void *, struct radio_info *);
static int slurm_set_info(void *, struct radio_info *);
static int slurm_search(void *, int);

static usbd_status slurm_setreg(struct slurm_softc *, int, uint16_t);
static usbd_status slurm_getreg(struct slurm_softc *, int, uint16_t *);

static uint32_t slurm_si470x_get_freq(struct slurm_softc *, uint16_t);
static void slurm_si470x_get_bandspace(struct slurm_softc *, uint16_t);
static int slurm_si470x_get_info(uint16_t);
static int slurm_si470x_get_mute(uint16_t);
static int slurm_si470x_get_stereo(uint16_t);
static int slurm_si470x_get_volume(uint16_t);

static int slurm_si470x_search(struct slurm_softc *, int);

static void slurm_si470x_set_freq(struct slurm_softc *, uint32_t);
static void slurm_si470x_set_powercfg(struct slurm_softc *, int, int);
static void slurm_si470x_set_volume(struct slurm_softc *, int);

static const struct radio_hw_if slurm_radio = {
	.get_info = slurm_get_info,
	.set_info = slurm_set_info,
	.search = slurm_search,
};

CFATTACH_DECL_NEW(slurm, sizeof(struct slurm_softc),
    slurm_match, slurm_attach, slurm_detach, NULL);

static int 
slurm_match(device_t parent, cfdata_t match, void *aux)
{
	const struct usbif_attach_arg * const uaa = aux;

	if (uaa->ifaceno != 2)
		return UMATCH_NONE;

	if (usb_lookup(slurm_devs, uaa->vendor, uaa->product) != NULL) {
		return UMATCH_VENDOR_PRODUCT;
	}

	return UMATCH_NONE;
}

static void 
slurm_attach(device_t parent, device_t self, void *aux)
{
	struct slurm_softc * const sc = device_private(self);
	const struct usbif_attach_arg * const uaa = aux;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_uif = uaa->iface;

	aprint_normal("\n");
	aprint_naive("\n");

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

#ifdef SLURM_DEBUG
	{
		uint16_t val;
		for (int i = 0; i < 16; i++) {
			slurm_getreg(sc, i, &val);
			device_printf(self, "%02x -> %04x\n", i, val);
		}
	}
#endif

	radio_attach_mi(&slurm_radio, sc, self);
}

static int 
slurm_detach(device_t self, int flags)
{
	struct slurm_softc * const sc = device_private(self);
	int rv = 0;

	if ((rv = config_detach_children(self, flags)) != 0)
		return rv;

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_dev);

	return (rv);
}

static int
slurm_get_info(void *v, struct radio_info *ri)
{
	struct slurm_softc * const sc = v;
	uint16_t powercfg, sysconfig2, readchannel, statusrssi;

	slurm_getreg(sc, SI470X_POWERCFG, &powercfg);
	slurm_getreg(sc, SI470X_SYSCONFIG2, &sysconfig2);
	slurm_getreg(sc, SI470X_STATUSRSSI, &statusrssi);
	slurm_getreg(sc, SI470X_READCHANNEL, &readchannel);

	ri->mute = slurm_si470x_get_mute(powercfg);
	ri->volume = slurm_si470x_get_volume(sysconfig2);
	ri->stereo = slurm_si470x_get_stereo(powercfg);
	ri->rfreq = 0;
	ri->lock = 0;
	slurm_si470x_get_bandspace(sc, sysconfig2);
	ri->freq = slurm_si470x_get_freq(sc, readchannel);
	ri->caps = RADIO_CAPS_DETECT_STEREO | RADIO_CAPS_DETECT_SIGNAL |
		   RADIO_CAPS_SET_MONO | RADIO_CAPS_HW_SEARCH |
		   RADIO_CAPS_HW_AFC | RADIO_CAPS_LOCK_SENSITIVITY;
	ri->info = slurm_si470x_get_info(statusrssi);

	return 0;
}

static int
slurm_set_info(void *v, struct radio_info *ri)
{
	struct slurm_softc * const sc = v;

	slurm_si470x_set_freq(sc, ri->freq);
	slurm_si470x_set_powercfg(sc, ri->mute, ri->stereo);
	slurm_si470x_set_volume(sc, ri->volume);

	return 0;
}

static int
slurm_search(void *v, int f)
{
	struct slurm_softc * const sc = v;
	
	return slurm_si470x_search(sc, f);
}

static usbd_status
slurm_getreg(struct slurm_softc *sc, int reg, uint16_t *val)
{
	usbd_status status;
	uint8_t s[3];

	++reg;

	s[0] = reg;
	s[1] = s[2] = 0;

	status = usbd_get_report(sc->sc_uif, UHID_FEATURE_REPORT,
		reg, &s, sizeof(s));

	*val = (s[1] << 8) | s[2];

	return status;
}

static usbd_status
slurm_setreg(struct slurm_softc *sc, int reg, uint16_t val)
{
	usbd_status status;
	uint8_t s[3];

	++reg;

	s[0] = reg;
	s[1] = (val >> 8) & 0xff;
	s[2] = (val >> 0) & 0xff;

	status = usbd_set_report(sc->sc_uif, UHID_FEATURE_REPORT,
		reg, &s, sizeof(s));

	return status;
}

static int
slurm_si470x_await_stc(struct slurm_softc *sc)
{
	int i;
	uint16_t statusrssi;

	for (i = 50; i > 0; i--) {
		usbd_delay_ms(sc->sc_udev, 2);
		slurm_getreg(sc, SI470X_STATUSRSSI, &statusrssi);
		if ((statusrssi & (SI470X_STC|SI470X_SF_BL)) != 0)
			break;
	}

	if (i == 0)
		return -1;
	else
		return 0;
}

static void
slurm_si470x_get_bandspace(struct slurm_softc *sc, uint16_t sysconfig2)
{
	switch (__SHIFTOUT(sysconfig2, SI470X_SPACE)) {
	default:
	case 0:
		sc->sc_space = 200;
		break;
	case 1:
		sc->sc_space = 100;
		break;
	case 2:
		sc->sc_space = 50;
		break;
	}

	switch (__SHIFTOUT(sysconfig2, SI470X_BAND)) {
	default:
	case 0:
		sc->sc_band = 87500;
		break;
	case 1:
	case 2:
		sc->sc_band = 76000;
		break;
	}
}

static uint32_t
slurm_si470x_get_freq(struct slurm_softc *sc, uint16_t readchannel)
{
	readchannel = __SHIFTOUT(readchannel, SI470X_READCHAN);
	return sc->sc_band + readchannel * sc->sc_space;
}

static int
slurm_si470x_get_info(uint16_t statusrssi)
{
	return (__SHIFTOUT(statusrssi, SI470X_ST) ? RADIO_INFO_STEREO : 0)
	    | (__SHIFTOUT(statusrssi, SI470X_AFCRL) ? 0 : RADIO_INFO_SIGNAL);
}

static int
slurm_si470x_get_mute(uint16_t powercfg)
{
	return __SHIFTOUT(powercfg, SI470X_DMUTE) ? 0 : 1;
}

static int
slurm_si470x_get_stereo(uint16_t powercfg)
{
	return __SHIFTOUT(powercfg, SI470X_MONO) ? 0 : 1;
}

static int
slurm_si470x_get_volume(uint16_t sysconfig2)
{
	return __SHIFTOUT(sysconfig2, SI470X_VOLUME) * SI470X_VOLFACT;
}

static int
slurm_si470x_search(struct slurm_softc *sc, int up)
{
	uint16_t powercfg;

	slurm_getreg(sc, SI470X_POWERCFG, &powercfg);
	powercfg &= ~(SI470X_SKMODE|SI470X_SEEKUP|SI470X_SEEK);
	powercfg |= up ? SI470X_SEEKUP : 0;
	slurm_setreg(sc, SI470X_POWERCFG, SI470X_SEEK|powercfg);
	slurm_si470x_await_stc(sc);
	slurm_setreg(sc, SI470X_POWERCFG, powercfg);

	return 0;
}

static void
slurm_si470x_set_freq(struct slurm_softc *sc, uint32_t freq)
{
	uint16_t channel;

	channel = (freq - sc->sc_band) / sc->sc_space;

	slurm_setreg(sc, SI470X_CHANNEL, SI470X_TUNE|channel);
	slurm_si470x_await_stc(sc);
	slurm_setreg(sc, SI470X_CHANNEL, channel);

#ifdef SLURM_DEBUG
	device_printf(sc->sc_dev, "%s 0a -> %04x after %d\n", __func__, val, i);
#endif
}

static void
slurm_si470x_set_powercfg(struct slurm_softc *sc, int mute, int stereo)
{
	uint16_t powercfg;

	slurm_getreg(sc, SI470X_POWERCFG, &powercfg);
	powercfg &= ~(SI470X_DMUTE|SI470X_MONO);
	powercfg |= SI470X_DSMUTE;
	powercfg |= mute ? 0 : SI470X_DMUTE;
	powercfg |= stereo ? 0 : SI470X_MONO;
	slurm_setreg(sc, SI470X_POWERCFG, powercfg);
}

static void
slurm_si470x_set_volume(struct slurm_softc *sc, int volume)
{
	uint16_t sysconfig2;

	slurm_getreg(sc, SI470X_SYSCONFIG2, &sysconfig2);
	sysconfig2 &= ~SI470X_VOLUME;
	sysconfig2 |= __SHIFTIN(volume / SI470X_VOLFACT, SI470X_VOLUME);
	slurm_setreg(sc, SI470X_SYSCONFIG2, sysconfig2);
}
