/*	$NetBSD: spic.c,v 1.19 2013/10/17 21:24:24 christos Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * The SPIC is used on some Sony Vaios to handle the jog dial and other
 * peripherals.
 * The protocol used by the SPIC seems to vary wildly among the different
 * models, and I've found no documentation.
 * This file handles the jog dial on the SRX77 model, and perhaps nothing
 * else.
 *
 * The general way of talking to the SPIC was gleaned from the Linux and
 * FreeBSD drivers.  The hex numbers were taken from these drivers (they
 * come from reverese engineering.)
 *
 * TODO:
 *   Make it handle more models.
 *   Figure out why the interrupt mode doesn't work.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spic.c,v 1.19 2013/10/17 21:24:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/callout.h>

#include <sys/bus.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/ic/spicvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#define	SPIC_EVENT_BRIGHTNESS_DOWN	0x15
#define	SPIC_EVENT_BRIGHTNESS_UP	0x16

#define POLLRATE (hz/30)

/* Some hardware constants */
#define SPIC_PORT1 0
#define SPIC_PORT2 4

#ifdef SPIC_DEBUG
int spicdebug = 0;
#endif

static int spicerror = 0;

static int	spic_enable(void *);
static void	spic_disable(void *);
static int	spic_ioctl(void *, u_long, void *, int, struct lwp *);

static const struct wsmouse_accessops spic_accessops = {
	spic_enable,
	spic_ioctl,
	spic_disable,
};

#define SPIC_COMMAND(quiet, command) do { \
	unsigned int n = 10000; \
	while (--n && (command)) \
		delay(1); \
	if (n == 0 && !(quiet)) { \
		printf("spic0: command failed at line %d\n", __LINE__); \
		spicerror++; \
	} \
} while (0)

#if 0
#define INB(sc, p) (delay(100), printf("inb(%x)=%x\n", (uint)sc->sc_ioh+p, bus_space_read_1(sc->sc_iot, sc->sc_ioh, p)), delay(100), bus_space_read_1(sc->sc_iot, sc->sc_ioh, (p)))
#define OUTB(sc, v, p) do { delay(100); bus_space_write_1(sc->sc_iot, sc->sc_ioh, (p), (v)); printf("outb(%x, %x)\n", (uint)sc->sc_ioh+p, v); } while(0)
#else
#define INB(sc, p) (delay(100), bus_space_read_1(sc->sc_iot, sc->sc_ioh, (p)))
#define OUTB(sc, v, p) do { delay(100); bus_space_write_1(sc->sc_iot, sc->sc_ioh, (p), (v)); } while(0)
#endif

static u_int8_t
spic_call1(struct spic_softc *sc, u_int8_t dev)
{
	u_int8_t v2;

	SPIC_COMMAND(0, INB(sc, SPIC_PORT2) & 2);
	OUTB(sc, dev, SPIC_PORT2);
	(void)INB(sc, SPIC_PORT2);
	v2 = INB(sc, SPIC_PORT1);
	return v2;
}

static u_int8_t
spic_call2(struct spic_softc *sc, u_int8_t dev, u_int8_t fn)
{
	u_int8_t v1;

	SPIC_COMMAND(0, INB(sc, SPIC_PORT2) & 2);
	OUTB(sc, dev, SPIC_PORT2);
	SPIC_COMMAND(0, INB(sc, SPIC_PORT2) & 2);
	OUTB(sc, fn, SPIC_PORT1);
	v1 = INB(sc, SPIC_PORT1);
	return v1;
}

/* Interrupt handler: some event is available */
int
spic_intr(void *v) {
	struct spic_softc *sc = v;
	u_int8_t v1, v2;
	int dz, buttons;

	v1 = INB(sc, SPIC_PORT1);
	v2 = INB(sc, SPIC_PORT2);

	/* Handle lid switch */
	if (v2 == 0x30) {
		switch (v1) {
		case 0x50:	/* opened */
			sysmon_pswitch_event(&sc->sc_smpsw[SPIC_PSWITCH_LID],
			    PSWITCH_EVENT_RELEASED);
			goto skip;
			break;
		case 0x51:	/* closed */
			sysmon_pswitch_event(&sc->sc_smpsw[SPIC_PSWITCH_LID],
			    PSWITCH_EVENT_PRESSED);
			goto skip;
			break;
		default:
			aprint_debug_dev(sc->sc_dev, "unknown lid event 0x%02x\n", v1);
			goto skip;
			break;
		}
	}

	/* Handle suspend/hibernate buttons */
	if (v2 == 0x20) {
		switch (v1) {
		case 0x10:	/* suspend */
			sysmon_pswitch_event(
			    &sc->sc_smpsw[SPIC_PSWITCH_SUSPEND],
			    PSWITCH_EVENT_PRESSED);
			goto skip;
			break;
		case 0x1c:	/* hibernate */
			sysmon_pswitch_event(
			    &sc->sc_smpsw[SPIC_PSWITCH_HIBERNATE],
			    PSWITCH_EVENT_PRESSED);
			goto skip;
			break;
		}
	}

	buttons = 0;
	if (v1 & 0x40)
		buttons |= 1 << 1;
	if (v1 & 0x20)
		buttons |= 1 << 5;
	dz = v1 & 0x1f;
	switch (dz) {
	case 0:
	case 1:
	case 2:
	case 3:
		break;
	case 0x1f:
	case 0x1e:
	case 0x1d:
		dz -= 0x20;
		break;
	case SPIC_EVENT_BRIGHTNESS_UP:
		pmf_event_inject(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_UP);
		break;
	case SPIC_EVENT_BRIGHTNESS_DOWN:
		pmf_event_inject(sc->sc_dev, PMFE_DISPLAY_BRIGHTNESS_DOWN);
		break;
	default:
		printf("spic0: v1=0x%02x v2=0x%02x\n", v1, v2);
		goto skip;
	}

	if (!sc->sc_enabled) {
		/*printf("spic: not enabled\n");*/
		goto skip;
	}

	if (dz != 0 || buttons != sc->sc_buttons) {
#ifdef SPIC_DEBUG
		if (spicdebug)
			printf("spic: but=0x%x dz=%d v1=0x%02x v2=0x%02x\n",
			       buttons, dz, v1, v2);
#endif
		sc->sc_buttons = buttons;
		if (sc->sc_wsmousedev != NULL) {
			wsmouse_input(sc->sc_wsmousedev, buttons, 0, 0, dz, 0,
				      WSMOUSE_INPUT_DELTA);
		}
	}

skip:
	spic_call2(sc, 0x81, 0xff); /* Clear event */
	return (1);
}

static void
spictimeout(void *v)
{
	struct spic_softc *sc = v;
	int s;

	if (spicerror >= 3)
		return;

	s = spltty();
	spic_intr(v);
	splx(s);
	callout_reset(&sc->sc_poll, POLLRATE, spictimeout, sc);
}

void
spic_attach(struct spic_softc *sc)
{
	struct wsmousedev_attach_args a;
	int i, rv;

#ifdef SPIC_DEBUG
	if (spicdebug)
		printf("spic_attach %x\n", (uint)sc->sc_ioh);
#endif

	callout_init(&sc->sc_poll, 0);

	spic_call1(sc, 0x82);
	spic_call2(sc, 0x81, 0xff);
	spic_call1(sc, 0x92);	/* or 0x82 */

	a.accessops = &spic_accessops;
	a.accesscookie = sc;
	sc->sc_wsmousedev = config_found(sc->sc_dev, &a, wsmousedevprint);

	sc->sc_smpsw[SPIC_PSWITCH_LID].smpsw_name = "spiclid0";
	sc->sc_smpsw[SPIC_PSWITCH_LID].smpsw_type = PSWITCH_TYPE_LID;
	sc->sc_smpsw[SPIC_PSWITCH_SUSPEND].smpsw_name = "spicsuspend0";
	sc->sc_smpsw[SPIC_PSWITCH_SUSPEND].smpsw_type = PSWITCH_TYPE_SLEEP;
	sc->sc_smpsw[SPIC_PSWITCH_HIBERNATE].smpsw_name = "spichibernate0";
	sc->sc_smpsw[SPIC_PSWITCH_HIBERNATE].smpsw_type = PSWITCH_TYPE_SLEEP;

	for (i = 0; i < SPIC_NPSWITCH; i++) {
		rv = sysmon_pswitch_register(&sc->sc_smpsw[i]);
		if (rv != 0)
			aprint_error_dev(sc->sc_dev, "unable to register %s with sysmon\n",
			    sc->sc_smpsw[i].smpsw_name);
	}

	callout_reset(&sc->sc_poll, POLLRATE, spictimeout, sc);

	return;
}

bool
spic_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct spic_softc *sc = device_private(dev);

	callout_stop(&sc->sc_poll);

	return true;
}

bool
spic_resume(device_t dev, const pmf_qual_t *qual)
{
	struct spic_softc *sc = device_private(dev);

	spic_call1(sc, 0x82);
	spic_call2(sc, 0x81, 0xff);
	spic_call1(sc, 0x92);	/* or 0x82 */

	callout_reset(&sc->sc_poll, POLLRATE, spictimeout, sc);
	return true;
}

static int
spic_enable(void *v)
{
	struct spic_softc *sc = v;

	if (sc->sc_enabled)
		return (EBUSY);

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

#ifdef SPIC_DEBUG
	if (spicdebug)
		printf("spic_enable\n");
#endif

	return (0);
}

static void
spic_disable(void *v)
{
	struct spic_softc *sc = v;

#ifdef DIAGNOSTIC
	if (!sc->sc_enabled) {
		printf("spic_disable: not enabled\n");
		return;
	}
#endif

	sc->sc_enabled = 0;

#ifdef SPIC_DEBUG
	if (spicdebug)
		printf("spic_disable\n");
#endif
}

static int
spic_ioctl(void *v, u_long cmd, void *data,
    int flag, struct lwp *l)
{
	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		/* XXX this is not really correct */
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		return (0);
	}

	return (-1);
}
