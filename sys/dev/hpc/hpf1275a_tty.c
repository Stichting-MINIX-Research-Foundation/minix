/*	$NetBSD: hpf1275a_tty.c,v 1.27 2013/06/28 14:44:15 christos Exp $ */

/*
 * Copyright (c) 2004 Valeriy E. Ushakov
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpf1275a_tty.c,v 1.27 2013/06/28 14:44:15 christos Exp $");

#include "opt_wsdisplay_compat.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kauth.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/pckbport/wskbdmap_mfii.h>
#ifdef WSDISPLAY_COMPAT_RAWKBD
#include <dev/hpc/pckbd_encode.h>
#endif


extern struct cfdriver hpf1275a_cd;

struct hpf1275a_softc {
	device_t sc_dev;

	struct tty *sc_tp;		/* back reference to the tty */
	device_t sc_wskbd;	/* wskbd child */
	int sc_enabled;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;
#endif
};


/* pseudo-device initialization */
extern void	hpf1275aattach(int);

/* line discipline methods */
static int	hpf1275a_open(dev_t, struct tty *);
static int	hpf1275a_close(struct tty *, int);
static int	hpf1275a_input(int, struct tty *);

/* autoconf(9) methods */
static int	hpf1275a_match(device_t, cfdata_t, void *);
static void	hpf1275a_attach(device_t, device_t, void *);
static int	hpf1275a_detach(device_t, int);

/* wskbd(4) accessops */
static int	hpf1275a_wskbd_enable(void *, int);
static void	hpf1275a_wskbd_set_leds(void *, int);
static int	hpf1275a_wskbd_ioctl(void *, u_long, void *, int,
				     struct lwp *);


/* 
 * It doesn't need to be exported, as only hpf1275aattach() uses it,
 * but there's no "official" way to make it static.
 */
CFATTACH_DECL_NEW(hpf1275a, sizeof(struct hpf1275a_softc),
    hpf1275a_match, hpf1275a_attach, hpf1275a_detach, NULL);


static struct linesw hpf1275a_disc = {
	.l_name = "hpf1275a",
	.l_open = hpf1275a_open,
	.l_close = hpf1275a_close,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = ttynullioctl,
	.l_rint = hpf1275a_input,
	.l_start = ttstart,
	.l_modem = nullmodem,
	.l_poll = ttpoll
};


static const struct wskbd_accessops hpf1275a_wskbd_accessops = {
	hpf1275a_wskbd_enable,
	hpf1275a_wskbd_set_leds,
	hpf1275a_wskbd_ioctl
};


static const struct wskbd_mapdata hpf1275a_wskbd_keymapdata = {
	pckbd_keydesctab, KB_US
};


/* F1275A scancodes -> XT scancodes so that we can use pckbd_keydesctab. */
static const uint8_t hpf1275a_to_xtscan[128] = {
	[0x04] = 30,		/* a */
	[0x05] = 48,		/* b */
	[0x06] = 46,		/* c */
	[0x07] = 32,		/* d */
	[0x08] = 18,		/* e */
	[0x09] = 33,		/* f */
	[0x0a] = 34,		/* g */
	[0x0b] = 35,		/* h */
	[0x0c] = 23,		/* i */
	[0x0d] = 36,		/* j */
	[0x0e] = 37,		/* k */
	[0x0f] = 38,		/* l */
	[0x10] = 50,		/* m */
	[0x11] = 49,		/* n */
	[0x12] = 24,		/* o */
	[0x13] = 25,		/* p */
	[0x14] = 16,		/* q */
	[0x15] = 19,		/* r */
	[0x16] = 31,		/* s */
	[0x17] = 20,		/* t */
	[0x18] = 22,		/* u */
	[0x19] = 47,		/* v */
	[0x1a] = 17,		/* w */
	[0x1b] = 45,		/* x */
	[0x1c] = 21,		/* y */
	[0x1d] = 44,		/* z */

	[0x1e] = 2,		/* 1 */
	[0x1f] = 3,		/* 2 */
	[0x20] = 4,		/* 3 */
	[0x21] = 5,		/* 4 */
	[0x22] = 6,		/* 5 */
	[0x23] = 7,		/* 6 */
	[0x24] = 8,		/* 7 */
	[0x25] = 9,		/* 8 */
	[0x26] = 10,		/* 9 */
	[0x27] = 11,		/* 0 */

	[0x28] = 28,		/* Enter */

	[0x29] = 1,		/* ESC */
	[0x2a] = 14,		/* Backspace */
	[0x2b] = 15,		/* Tab */
	[0x2c] = 57,		/* Space */

	[0x2d] = 12,		/* - */
	[0x2e] = 13,		/* = */
	[0x2f] = 26,		/* [ */
	[0x30] = 27,		/* ] */
	[0x31] = 43,		/* \ */

	[0x33] = 39,		/* ; */
	[0x34] = 40,		/* ' */
	[0x35] = 41,		/* ` */
	[0x36] = 51,		/* , */
	[0x37] = 52,		/* . */
	[0x38] = 53,		/* / */

	[0x3a] = 59,		/* F1 */
	[0x3b] = 60,		/* F2 */
	[0x3c] = 61,		/* F3 */
	[0x3d] = 62,		/* F4 */
	[0x3e] = 63,		/* F5 */
	[0x3f] = 64,		/* F6 */
	[0x40] = 65,		/* F7 */
	[0x41] = 66,		/* F8 */

	[0x42] = 68,		/* "OK" -> F10 */
	[0x43] = 87,		/* "Cancel" -> F11 */

	[0x4c] = 211,		/* Del */

	[0x4f] = 205,		/* Right */
	[0x50] = 203,		/* Left  */
	[0x51] = 208,		/* Down  */
	[0x52] = 200,		/* Up    */

	[0x53] = 67,		/* "task switch" -> F9 */

	[0x65] = 221,		/* windows */
	[0x66] = 88,		/* "keyboard" -> F12 */

	[0x74] = 42,		/* Shift (left) */
	[0x75] = 54,		/* Shift (right) */
	[0x76] = 56,		/* Alt (left) */
	[0x77] = 184,		/* Fn -> AltGr == Mode Switch */
	[0x78] = 29,		/* Control (left) */
};


/*
 * Pseudo-device initialization routine called from main().
 */
void
hpf1275aattach(int n)
{
	int error;

	error = ttyldisc_attach(&hpf1275a_disc);
	if (error) {
		printf("%s: unable to register line discipline, error = %d\n",
		       hpf1275a_cd.cd_name, error);
		return;
	}

	error = config_cfattach_attach(hpf1275a_cd.cd_name, &hpf1275a_ca);
	if (error) {
		printf("%s: unable to register cfattach, error = %d\n",
		       hpf1275a_cd.cd_name, error);
		config_cfdriver_detach(&hpf1275a_cd);
		(void) ttyldisc_detach(&hpf1275a_disc);
	}
}


/*
 * Autoconf match routine.
 *
 * XXX: unused: config_attach_pseudo(9) does not call ca_match.
 */
static int
hpf1275a_match(device_t self, cfdata_t cfdata, void *arg)
{

	/* pseudo-device; always present */
	return (1);
}


/*
 * Autoconf attach routine.  Called by config_attach_pseudo(9) when we
 * open the line discipline.
 */
static void
hpf1275a_attach(device_t parent, device_t self, void *aux)
{
	struct hpf1275a_softc *sc = device_private(self);
	struct wskbddev_attach_args wska;

	wska.console = 0;
	wska.keymap = &hpf1275a_wskbd_keymapdata;
	wska.accessops = &hpf1275a_wskbd_accessops;
	wska.accesscookie = sc;

	sc->sc_dev = self;
	sc->sc_enabled = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	sc->sc_rawkbd = 0;
#endif
	sc->sc_wskbd = config_found(self, &wska, wskbddevprint);
}


/*
 * Autoconf detach routine.  Called when we close the line discipline.
 */
static int
hpf1275a_detach(device_t self, int flags)
{
	struct hpf1275a_softc *sc = device_private(self);
	int error;

	if (sc->sc_wskbd == NULL)
		return (0);

	error = config_detach(sc->sc_wskbd, 0);

	return (error);
}


/*
 * Line discipline open routine.
 */
static int
hpf1275a_open(dev_t dev, struct tty *tp)
{
	static struct cfdata hpf1275a_cfdata = {
		.cf_name = "hpf1275a",
		.cf_atname = "hpf1275a",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
	};
	struct lwp *l = curlwp;		/* XXX */
	struct hpf1275a_softc *sc = device_private(self);
	device_t self;
	int error, s;

	if ((error = kauth_authorize_device_tty(l->l_cred,
			KAUTH_DEVICE_TTY_OPEN, tp)))
		return (error);

	s = spltty();

	if (tp->t_linesw == &hpf1275a_disc) {
		splx(s);
		return 0;
	}

	self = config_attach_pseudo(&hpf1275a_cfdata);
	if (self == NULL) {
		splx(s);
		return (EIO);
	}

	tp->t_sc = sc;
	sc->sc_tp = tp;

	splx(s);
	return (0);
}


/*
 * Line discipline close routine.
 */
static int
hpf1275a_close(struct tty *tp, int flag)
{
	struct hpf1275a_softc *sc = tp->t_sc;
	int s;

	s = spltty();
	mutex_spin_enter(&tty_lock);
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);	 /* XXX */
	ttyldisc_release(tp->t_linesw);
	tp->t_linesw = ttyldisc_default();
	if (sc != NULL) {
		tp->t_sc = NULL;
		if (sc->sc_tp == tp)
			config_detach(sc->sc_dev, 0);
	}
	splx(s);
	return (0);
}


/*
 * Feed input from the keyboard to wskbd(4).
 */
static int
hpf1275a_input(int c, struct tty *tp)
{
	struct hpf1275a_softc *sc = tp->t_sc;
	int code;
	u_int type;
	int xtscan;

	if (!sc->sc_enabled)
		return (0);

	if (c & TTY_ERRORMASK)
		return (0);	/* TODO? */

	code = c & TTY_CHARMASK;
	if (code & 0x80) {
		type = WSCONS_EVENT_KEY_UP;
		code &= ~0x80;
	} else
		type = WSCONS_EVENT_KEY_DOWN;

	xtscan = hpf1275a_to_xtscan[code];
	if (xtscan == 0) {
		aprint_error_dev(sc->sc_dev, "unknown code 0x%x\n", code);
		return (0);
	}

	KASSERT(sc->sc_wskbd != NULL);

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		u_char data[16];
		int n;

		n = pckbd_encode(type, xtscan, data);
		wskbd_rawinput(sc->sc_wskbd, data, n);
	} else
#endif
		wskbd_input(sc->sc_wskbd, type, xtscan);

	return (0);
}


static int
hpf1275a_wskbd_enable(void *self, int on)
{
	struct hpf1275a_softc *sc = self;

	sc->sc_enabled = on;
	return (0);
}


/* ARGSUSED */
static void
hpf1275a_wskbd_set_leds(void *self, int leds)
{

	/* this keyboard has no leds; nothing to do */
	return;
}


static int
hpf1275a_wskbd_ioctl(void *self, u_long cmd, void *data, int flag,
		     struct lwp *l)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct hpf1275a_softc *sc = self;
#endif

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_HPC_KBD; /* may be use new type? */
		return (0);

	case WSKBDIO_GETLEDS:
		*(int *)data = 0; /* this keyboard has no leds */
		return (0);

#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = (*(int *)data == WSKBD_RAW);
		return (0);
#endif

	default:
		return (EPASSTHROUGH);
	}
}
