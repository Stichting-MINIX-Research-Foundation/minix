/*	$NetBSD: oj6sh.c,v 1.1 2014/03/29 12:00:27 hkenken Exp $	*/

/*
 * Copyright (c) 2014  Genetec Corporation.  All rights reserved.
 * Written by Hashimoto Kenichi for Genetec Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Sharp NetWalker's Optical Joystick
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: oj6sh.c,v 1.1 2014/03/29 12:00:27 hkenken Exp $");

#include "opt_oj6sh.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/lock.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/mutex.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/hpc/hpcfbio.h>
#include <dev/hpc/hpctpanelvar.h>

#include <dev/spi/spivar.h>

#ifdef OJ6SH_DEBUG
int oj6sh_debug = OJ6SH_DEBUG;
#define DPRINTF(n,x)	if (oj6sh_debug>(n)) printf x;
#else
#define DPRINTF(n,x)
#endif

#define POLLRATE (hz/30)

/* register address */
#define OJ6SH_PRODUCT		0x00
#define OJ6SH_REVISION		0x01
#define OJ6SH_MOTION		0x02
#define OJ6SH_DELTA_X		0x03
#define OJ6SH_DELTA_Y		0x04
#define OJ6SH_SQUAL		0x05
#define OJ6SH_SHUTTER		0x06
#define OJ6SH_CONFIG		0x11
#define OJ6SH_RESET		0x3a
#define  POWERON_RESET		0x5a
#define OJ6SH_N_REVISION	0x3e
#define OJ6SH_N_PRODUCT		0x3f

struct oj6sh_softc {
	device_t sc_dev;

	struct spi_handle *sc_sh;
	struct callout sc_c;

	kmutex_t sc_lock;
	int sc_enabled;

	device_t sc_wsmousedev;
};

struct oj6sh_delta {
	int x;
	int y;
};

static uint8_t oj6sh_read(struct spi_handle *, uint8_t);
static void oj6sh_write(struct spi_handle *, uint8_t, uint8_t);

static int oj6sh_match(device_t , cfdata_t , void *);
static void oj6sh_attach(device_t , device_t , void *);

CFATTACH_DECL_NEW(oj6sh, sizeof(struct oj6sh_softc),
    oj6sh_match, oj6sh_attach, NULL, NULL);

static bool oj6sh_motion(struct spi_handle *);
static bool oj6sh_squal(struct spi_handle *);
static bool oj6sh_shuttrer(struct spi_handle *);
static int oj6sh_readdelta(struct spi_handle *, struct oj6sh_delta *);

static void oj6sh_poll(void *);
static int oj6sh_enable(void *v);
static void oj6sh_disable(void *v);
static int oj6sh_ioctl(void *, u_long, void *, int, struct lwp *);

static bool oj6sh_resume(device_t, const pmf_qual_t *);
static bool oj6sh_suspend(device_t, const pmf_qual_t *);

static const struct wsmouse_accessops oj6sh_accessops = {
	.enable = oj6sh_enable,
	.ioctl = oj6sh_ioctl,
	.disable = oj6sh_disable
};

static int
oj6sh_match(device_t parent, cfdata_t match, void *aux)
{
	struct spi_attach_args *sa = aux;

	if (strcmp(match->cf_name, "oj6sh"))
		return 0;
	if (spi_configure(sa->sa_handle, SPI_MODE_0, 2500000))
		return 0;

	return 2;
}

static void
oj6sh_doattach(device_t self)
{
	struct oj6sh_softc *sc = device_private(self);
	uint8_t product;
	uint8_t rev;
	uint8_t product_inv;
	uint8_t rev_inv;

	/* reset */
	oj6sh_write(sc->sc_sh, OJ6SH_RESET, POWERON_RESET);
	delay(10000);

	/* resolution */
	oj6sh_write(sc->sc_sh, OJ6SH_CONFIG, 0x80);

	product = oj6sh_read(sc->sc_sh, OJ6SH_PRODUCT);
	rev = oj6sh_read(sc->sc_sh, OJ6SH_REVISION);
	product_inv = oj6sh_read(sc->sc_sh, OJ6SH_N_PRODUCT);
	rev_inv = oj6sh_read(sc->sc_sh, OJ6SH_N_REVISION);

	if (((product | product_inv) != 0xff) || ((rev | rev_inv) != 0xff)) {
		aprint_error_dev(self,
		    "mismatch product (%02x:%02x), rev (%02x:%02x)\n",
		    product, product_inv, rev, rev_inv);
		return;
	}

	aprint_normal("%s: id 0x%02x, revision 0x%02x\n",
	    device_xname(sc->sc_dev), product, rev);

	return;
}

static void
oj6sh_attach(device_t parent, device_t self, void *aux)
{
	struct oj6sh_softc *sc = device_private(self);
	struct spi_attach_args *sa = aux;
	struct wsmousedev_attach_args a;

	aprint_normal(": OJ6SH-T25 Optical Joystick\n");

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_dev = self;
	sc->sc_enabled = 0;

	callout_init(&sc->sc_c, 0);

	sc->sc_sh = sa->sa_handle;

	a.accessops = &oj6sh_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

	config_interrupts(self, oj6sh_doattach);
}

static void
oj6sh_poll(void *arg)
{
	struct oj6sh_softc *sc = (struct oj6sh_softc *)arg;
	struct oj6sh_delta delta = {0, 0};
	uint32_t buttons = 0;
	int s;
	int x, y;

	mutex_enter(&sc->sc_lock);

	if (oj6sh_motion(sc->sc_sh) == false)
		goto out;
	else if ((oj6sh_squal(sc->sc_sh) == true) &&
	    (oj6sh_shuttrer(sc->sc_sh) == true))
		goto out;

	oj6sh_readdelta(sc->sc_sh, &delta);
	DPRINTF(3,("%s: x = %d, y = %d\n", device_xname(sc->sc_dev),
		delta.x, delta.y));

#if defined(J6SH_DOWN_Y_LEFT_X)
	y = -delta.y;
	x = -delta.x;
#elif defined(OJ6SH_UP_X_LEFT_Y)
	y = delta.x;
	x = -delta.y;
#elif defined(OJ6SH_DOWN_X_RIGHT_Y)
	y = -delta.x;
	x = delta.y;
#else /* OJ6SH_UP_Y_RIGHT_X */
	y = delta.y;
	x = delta.x;
#endif
	s = spltty();
	wsmouse_input(sc->sc_wsmousedev, buttons, x, y, 0, 0,
	    WSMOUSE_INPUT_DELTA);
	splx(s);
out:
	mutex_exit(&sc->sc_lock);

	if (sc->sc_enabled)
		callout_reset(&sc->sc_c, POLLRATE, oj6sh_poll, sc);

	return;
}

static uint8_t
oj6sh_read(struct spi_handle *spi, uint8_t reg)
{
	uint8_t ret = 0;

	spi_send_recv(spi, 1, &reg, 1, &ret);
	DPRINTF(4,("%s: 0x%02x = 0x%02x\n", __func__, reg, ret));
	return ret;
}

static void
oj6sh_write(struct spi_handle *spi, uint8_t reg, uint8_t val)
{
	uint8_t tmp[2] = {reg | 0x80, val};

	spi_send(spi, 2, tmp);
	DPRINTF(4,("%s: 0x%02x = 0x%02x\n", __func__, reg, val));
	return;
}

static bool
oj6sh_motion(struct spi_handle *spi)
{
	uint16_t motion;
	motion = oj6sh_read(spi, OJ6SH_MOTION);
	return (motion & __BIT(7) ? true : false);
}

static bool
oj6sh_squal(struct spi_handle *spi)
{
	uint16_t squal;
	squal = oj6sh_read(spi, OJ6SH_SQUAL);
	return (squal < 25 ? true : false);
}

static bool
oj6sh_shuttrer(struct spi_handle *spi)
{
	uint16_t shutter;
	shutter = oj6sh_read(spi, OJ6SH_SHUTTER) << 8;
	shutter |= oj6sh_read(spi, OJ6SH_SHUTTER + 1);
	return (shutter > 600 ? true : false);
}

static int
oj6sh_readdelta(struct spi_handle *spi, struct oj6sh_delta *delta)
{
	delta->x = (int8_t)oj6sh_read(spi, OJ6SH_DELTA_X);
	delta->y = (int8_t)oj6sh_read(spi, OJ6SH_DELTA_Y);
	return 0;
}

int
oj6sh_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct wsmouse_id *id;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		return 0;
	case WSMOUSEIO_GETID:
		id = (struct wsmouse_id *)data;
		if (id->type != WSMOUSE_ID_TYPE_UIDSTR)
			return EINVAL;
		strlcpy(id->data, "OJ6SH-T25", WSMOUSE_ID_MAXLEN);
		id->length = strlen(id->data);
		return 0;
	}

	return EPASSTHROUGH;
}

int
oj6sh_enable(void *v)
{
	struct oj6sh_softc *sc = (struct oj6sh_softc *)v;

	DPRINTF(3,("%s: oj6sh_enable()\n", device_xname(sc->sc_dev)));
	if (sc->sc_enabled) {
		DPRINTF(3,("%s: already enabled\n", device_xname(sc->sc_dev)));
		return EBUSY;
	}

	if (!pmf_device_register(sc->sc_dev, oj6sh_suspend, oj6sh_resume))
		aprint_error_dev(sc->sc_dev, "couldn't establish power handler\n");

	sc->sc_enabled = 1;
	callout_reset(&sc->sc_c, POLLRATE, oj6sh_poll, sc);

	return 0;
}

void
oj6sh_disable(void *v)
{
	struct oj6sh_softc *sc = (struct oj6sh_softc *)v;

	DPRINTF(3,("%s: oj6sh_disable()\n", device_xname(sc->sc_dev)));
	if (!sc->sc_enabled) {
		DPRINTF(3,("%s: already disabled()\n", device_xname(sc->sc_dev)));
		return;
	}

	pmf_device_deregister(sc->sc_dev);

	sc->sc_enabled = 0;

	return;
}

static bool
oj6sh_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct oj6sh_softc *sc = device_private(dv);

	DPRINTF(3,("%s: oj6sh_suspend()\n", device_xname(sc->sc_dev)));
	callout_stop(&sc->sc_c);
	sc->sc_enabled = 0;

	return true;
}

static bool
oj6sh_resume(device_t dv, const pmf_qual_t *qual)
{
	struct oj6sh_softc *sc = device_private(dv);

	DPRINTF(3,("%s: oj6sh_resume()\n", device_xname(sc->sc_dev)));
	sc->sc_enabled = 1;
	callout_reset(&sc->sc_c, POLLRATE, oj6sh_poll, sc);

	return true;
}

