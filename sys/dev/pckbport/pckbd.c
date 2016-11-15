/* $NetBSD: pckbd.c,v 1.32 2015/07/16 15:01:04 prlw1 Exp $ */

/*-
 * Copyright (c) 1998, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
 */

/*
 * code to work keyboard for PC-style console
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pckbd.c,v 1.32 2015/07/16 15:01:04 prlw1 Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>

#include <sys/bus.h>

#include <dev/pckbport/pckbportvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/pckbport/pckbdreg.h>
#include <dev/pckbport/pckbdvar.h>
#include <dev/pckbport/wskbdmap_mfii.h>

#include "locators.h"

#include "opt_pckbd_layout.h"
#include "opt_pckbd_cnattach_may_fail.h"
#include "opt_wsdisplay_compat.h"

struct pckbd_internal {
	int t_isconsole;
	pckbport_tag_t t_kbctag;
	pckbport_slot_t t_kbcslot;

	int t_translating;

	int t_lastchar;
	int t_extended0;
	int t_extended1;
	int t_releasing;

	struct pckbd_softc *t_sc; /* back pointer */
};

struct pckbd_softc {
        device_t sc_dev;

	struct pckbd_internal *id;
	int sc_enabled;

	int sc_ledstate;

	device_t sc_wskbddev;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int rawkbd;
#endif
};

static int pckbd_is_console(pckbport_tag_t, pckbport_slot_t);

int pckbdprobe(device_t, cfdata_t, void *);
void pckbdattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pckbd, sizeof(struct pckbd_softc),
    pckbdprobe, pckbdattach, NULL, NULL);

int	pckbd_enable(void *, int);
void	pckbd_set_leds(void *, int);
int	pckbd_ioctl(void *, u_long, void *, int, struct lwp *);

const struct wskbd_accessops pckbd_accessops = {
	pckbd_enable,
	pckbd_set_leds,
	pckbd_ioctl,
};

void	pckbd_cngetc(void *, u_int *, int *);
void	pckbd_cnpollc(void *, int);
void	pckbd_cnbell(void *, u_int, u_int, u_int);

const struct wskbd_consops pckbd_consops = {
	pckbd_cngetc,
	pckbd_cnpollc,
	pckbd_cnbell,
};

const struct wskbd_mapdata pckbd_keymapdata = {
	pckbd_keydesctab,
#ifdef PCKBD_LAYOUT
	PCKBD_LAYOUT,
#else
	KB_US,
#endif
};

/*
 * Hackish support for a bell on the PC Keyboard; when a suitable feeper
 * is found, it attaches itself into the pckbd driver here.
 */
void	(*pckbd_bell_fn)(void *, u_int, u_int, u_int, int);
void	*pckbd_bell_fn_arg;

void	pckbd_bell(u_int, u_int, u_int, int);

int	pckbd_scancode_translate(struct pckbd_internal *, int);
int	pckbd_set_xtscancode(pckbport_tag_t, pckbport_slot_t,
	    struct pckbd_internal *);
int	pckbd_init(struct pckbd_internal *, pckbport_tag_t, pckbport_slot_t,
			int);
void	pckbd_input(void *, int);

static int	pckbd_decode(struct pckbd_internal *, int, u_int *, int *);
static int	pckbd_led_encode(int);
static int	pckbd_led_decode(int);

struct pckbd_internal pckbd_consdata;

int
pckbd_set_xtscancode(pckbport_tag_t kbctag, pckbport_slot_t kbcslot,
    struct pckbd_internal *id)
{
	int xt, res = 0;
	u_char cmd[2];

	/*
	 * Some keyboard/8042 combinations do not seem to work if the keyboard
	 * is set to table 1; in fact, it would appear that some keyboards just
	 * ignore the command altogether.  So by default, we use the AT scan
	 * codes and have the 8042 translate them.  Unfortunately, this is
	 * known to not work on some PS/2 machines.  We try desperately to deal
	 * with this by checking the (lack of a) translate bit in the 8042 and
	 * attempting to set the keyboard to XT mode.  If this all fails, well,
	 * tough luck.  If the PCKBC_CANT_TRANSLATE pckbc flag was set, we
	 * enable software translation.
	 *
	 * XXX It would perhaps be a better choice to just use AT scan codes
	 * and not bother with this.
	 */
	xt = pckbport_xt_translation(kbctag, kbcslot, 1);
	if (xt == 1) {
		/* The 8042 is translating for us; use AT codes. */
		cmd[0] = KBC_SETTABLE;
		cmd[1] = 2;
		res = pckbport_poll_cmd(kbctag, kbcslot, cmd, 2, 0, 0, 0);
		if (res) {
			u_char cmdb[1];
			aprint_debug("pckbd: error setting scanset 2\n");
			/*
			 * XXX at least one keyboard is reported to lock up
			 * if a "set table" is attempted, thus the "reset".
			 * XXX ignore errors, scanset 2 should be
			 * default anyway.
			 */
			cmdb[0] = KBC_RESET;
			(void)pckbport_poll_cmd(kbctag, kbcslot, cmdb, 1, 1, 0, 1);
			pckbport_flush(kbctag, kbcslot);
			res = 0;
		}
		if (id != NULL)
			id->t_translating = 1;
	} else if (xt == -1) {
		/* Software translation required */
		if (id != NULL)
			id->t_translating = 0;
	} else {
		/* Stupid 8042; set keyboard to XT codes. */
		cmd[0] = KBC_SETTABLE;
		cmd[1] = 1;
		res = pckbport_poll_cmd(kbctag, kbcslot, cmd, 2, 0, 0, 0);
		if (res)
			aprint_debug("pckbd: error setting scanset 1\n");
		if (id != NULL)
			id->t_translating = 1;
	}
	return res;
}

static int
pckbd_is_console(pckbport_tag_t tag, pckbport_slot_t slot)
{

	return pckbd_consdata.t_isconsole &&
	    tag == pckbd_consdata.t_kbctag && slot == pckbd_consdata.t_kbcslot;
}

static bool
pckbd_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct pckbd_softc *sc = device_private(dv);
	u_char cmd[1];
	int res;

	/* XXX duped from pckbd_enable, but we want to disable
	 *     it even if it's the console kbd
	 */
	cmd[0] = KBC_DISABLE;
	res = pckbport_enqueue_cmd(sc->id->t_kbctag,
	    sc->id->t_kbcslot, cmd, 1, 0, 1, 0);
	if (res)
		return false;

	pckbport_slot_enable(sc->id->t_kbctag,
	    sc->id->t_kbcslot, 0);

	sc->sc_enabled = 0;
	return true;
}

static bool
pckbd_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pckbd_softc *sc = device_private(dv);
	u_char cmd[1], resp[1];
	int res;

	/* XXX jmcneill reset the keyboard */
	pckbport_flush(sc->id->t_kbctag, sc->id->t_kbcslot);

	cmd[0] = KBC_RESET;
	res = pckbport_poll_cmd(sc->id->t_kbctag,
	    sc->id->t_kbcslot, cmd, 1, 1, resp, 1);
	if (res)
		aprint_debug("%s: reset error %d\n", __func__, res);
	if (resp[0] != KBR_RSTDONE)
		printf("%s: reset response 0x%x\n", __func__, resp[0]);

	pckbport_flush(sc->id->t_kbctag, sc->id->t_kbcslot);

	pckbd_enable(sc, 1);

	return true;
}

/*
 * these are both bad jokes
 */
int
pckbdprobe(device_t parent, cfdata_t cf, void *aux)
{
	struct pckbport_attach_args *pa = aux;
	int res;
	u_char cmd[1], resp[1];

	/*
	 * XXX There are rumours that a keyboard can be connected
	 * to the aux port as well. For me, this didn't work.
	 * For further experiments, allow it if explicitly
	 * wired in the config file.
	 */
	if ((pa->pa_slot != PCKBPORT_KBD_SLOT) &&
	    (cf->cf_loc[PCKBPORTCF_SLOT] == PCKBPORTCF_SLOT_DEFAULT))
		return 0;

	/* Flush any garbage. */
	pckbport_flush(pa->pa_tag, pa->pa_slot);

	/* Reset the keyboard. */
	cmd[0] = KBC_RESET;
	res = pckbport_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 1, resp, 1);
	if (res) {
		aprint_debug("pckbdprobe: reset error %d\n", res);
		/*
		 * There is probably no keyboard connected.
		 * Let the probe succeed if the keyboard is used
		 * as console input - it can be connected later.
		 */
		return pckbd_is_console(pa->pa_tag, pa->pa_slot) ? 1 : 0;
	}
	if (resp[0] != KBR_RSTDONE) {
		printf("pckbdprobe: reset response 0x%x\n", resp[0]);
		return 0;
	}

	/*
	 * Some keyboards seem to leave a second ack byte after the reset.
	 * This is kind of stupid, but we account for them anyway by just
	 * flushing the buffer.
	 */
	pckbport_flush(pa->pa_tag, pa->pa_slot);

	if (pckbd_set_xtscancode(pa->pa_tag, pa->pa_slot, NULL))
		return 0;

	return 2;
}

void
pckbdattach(device_t parent, device_t self, void *aux)
{
	struct pckbd_softc *sc = device_private(self);
	struct pckbport_attach_args *pa = aux;
	struct wskbddev_attach_args a;
	int isconsole;
	u_char cmd[1];

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;
	isconsole = pckbd_is_console(pa->pa_tag, pa->pa_slot);

	if (isconsole) {
		sc->id = &pckbd_consdata;

		/*
		 * Some keyboards are not enabled after a reset,
		 * so make sure it is enabled now.
		 */
		cmd[0] = KBC_ENABLE;
		(void) pckbport_poll_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				      cmd, 1, 0, 0, 0);
		sc->sc_enabled = 1;
	} else {
		sc->id = malloc(sizeof(struct pckbd_internal),
				M_DEVBUF, M_WAITOK);
		(void) pckbd_init(sc->id, pa->pa_tag, pa->pa_slot, 0);

		/* no interrupts until enabled */
		cmd[0] = KBC_DISABLE;
		(void) pckbport_poll_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				      cmd, 1, 0, 0, 0);
		sc->sc_enabled = 0;
	}

	sc->id->t_sc = sc;

	pckbport_set_inputhandler(sc->id->t_kbctag, sc->id->t_kbcslot,
			       pckbd_input, sc, device_xname(sc->sc_dev));

	a.console = isconsole;

	a.keymap = &pckbd_keymapdata;

	a.accessops = &pckbd_accessops;
	a.accesscookie = sc;

	if (!pmf_device_register(self, pckbd_suspend, pckbd_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * Attach the wskbd, saving a handle to it.
	 * XXX XXX XXX
	 */
	sc->sc_wskbddev = config_found_ia(self, "wskbddev", &a, wskbddevprint);
}

int
pckbd_enable(void *v, int on)
{
	struct pckbd_softc *sc = v;
	int res;
	u_char cmd[1];

	if (on) {
		if (sc->sc_enabled) {
			aprint_debug("pckbd_enable: bad enable\n");
			return EBUSY;
		}

		pckbport_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 1);

		cmd[0] = KBC_ENABLE;
		res = pckbport_poll_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, NULL, 0);
		if (res) {
			printf("pckbd_enable: command error\n");
			return (res);
		}

		res = pckbd_set_xtscancode(sc->id->t_kbctag,
					   sc->id->t_kbcslot, sc->id);
		if (res)
			return res;

		sc->sc_enabled = 1;
	} else {
		if (sc->id->t_isconsole)
			return EBUSY;

		cmd[0] = KBC_DISABLE;
		res = pckbport_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, 1, 0);
		if (res) {
			printf("pckbd_disable: command error\n");
			return res;
		}

		pckbport_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 0);

		sc->sc_enabled = 0;
	}

	return 0;
}

const u_int8_t pckbd_xtbl[] = {
/* 0x00 */
	0,
	0x43,		/* F9 */
	0x89,		/* SunStop */
	0x3f,		/* F5 */
	0x3d,		/* F3 */
	0x3b,		/* F1 */
	0x3c,		/* F2 */
	0x58,		/* F12 */
	0,
	0x44,		/* F10 */
	0x42,		/* F8 */
	0x40,		/* F6 */
	0x3e,		/* F4 */
	0x0f,		/* Tab */
	0x29,		/* ` ~ */
	0,
/* 0x10 */
	0,
	0x38,		/* Left Alt */
	0x2a,		/* Left Shift */
	0,
	0x1d,		/* Left Ctrl */
	0x10,		/* q */
	0x02,		/* 1 ! */
	0,
	0,
	0,
	0x2c,		/* z */
	0x1f,		/* s */
	0x1e,		/* a */
	0x11,		/* w */
	0x03,		/* 2 @ */
	0,
/* 0x20 */	
	0,
	0x2e,		/* c */
	0x2d,		/* x */
	0x20,		/* d */
	0x12,		/* e */
	0x05,		/* 4 $ */
	0x04,		/* 3 # */
	0,
	0,
	0x39,		/* Space */
	0x2f,		/* v */
	0x21,		/* f */
	0x14,		/* t */
	0x13,		/* r */
	0x06,		/* 5 % */
	0,
/* 0x30 */
	0,
	0x31,		/* n */
	0x30,		/* b */
	0x23,		/* h */
	0x22,		/* g */
	0x15,		/* y */
	0x07,		/* 6 ^ */
	0,
	0,
	0,
	0x32,		/* m */
	0x24,		/* j */
	0x16,		/* u */
	0x08,		/* 7 & */
	0x09,		/* 8 * */
	0,
/* 0x40 */
	0,
	0x33,		/* , < */
	0x25,		/* k */
	0x17,		/* i */
	0x18,		/* o */
	0x0b,		/* 0 ) */
	0x0a,		/* 9 ( */
	0,
	0,
	0x34,		/* . > */
	0x35,		/* / ? */
	0x26,		/* l */
	0x27,		/* ; : */
	0x19,		/* p */
	0x0c,		/* - _ */
	0,
/* 0x50 */
	0,
	0,
	0x28,		/* ' " */
	0,
	0x1a,		/* [ { */
	0x0d,		/* = + */
	0,
	0,
	0x3a,		/* Caps Lock */
	0x36,		/* Right Shift */
	0x1c,		/* Return */
	0x1b,		/* ] } */
	0,
	0x2b,		/* \ | */
	0,
	0,
/* 0x60 */
	0,
	0,
	0,
	0,
	0,
	0,
	0x0e,		/* Back Space */
	0,
	0,
	0x4f,		/* KP 1 */
	0,
	0x4b,		/* KP 4 */
	0x47,		/* KP 7 */
	0,
	0,
	0,
/* 0x70 */
	0x52,		/* KP 0 */
	0x53,		/* KP . */
	0x50,		/* KP 2 */
	0x4c,		/* KP 5 */
	0x4d,		/* KP 6 */
	0x48,		/* KP 8 */
	0x01,		/* Escape */
	0x45,		/* Num Lock */
	0x57,		/* F11 */
	0x4e,		/* KP + */
	0x51,		/* KP 3 */
	0x4a,		/* KP - */
	0x37,		/* KP * */
	0x49,		/* KP 9 */
	0x46,		/* Scroll Lock */
	0,
/* 0x80 */
	0,
	0,
	0,
	0x41,		/* F7 (produced as an actual 8 bit code) */
	0,		/* Alt-Print Screen */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/* 0x90 */
	0xdb,		/* Left Meta */
	0x88,		/* SunHelp */
	0x8a,		/* SunAgain */
	0x8c,		/* SunUndo */
	0x8e,		/* SunCopy */
	0x90,		/* SunPaste */
	0x92,		/* SunCut */
	0x8b,		/* SunProps */
	0x8d,		/* SunFront */
	0x8f,		/* SunOpen */
	0x91		/* SunFind */
};

const u_int8_t pckbd_xtbl_ext[] = {
/* 0x00 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/* 0x10 */
	0,
	0x38,		/* Right Alt */
	0,		/* E0 12, to be ignored */
	0,
	0x1d,		/* Right Ctrl */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/* 0x20 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0xdd,		/* Compose */
/* 0x30 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/* 0x40 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0xb5,		/* KP / */
	0,
	0,
	0,
	0,
	0,
/* 0x50 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0x1c,		/* KP Return */
	0,
	0,
	0,
	0,
	0,
/* 0x60 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0x4f,		/* End */
	0,
	0x4b,		/* Left */
	0x47,		/* Home */
	0,
	0,
	0,
/* 0x70 */
	0x52,		/* Insert */
	0x53,		/* Delete */
	0x50,		/* Down */
	0,
	0x4d,		/* Right */
	0x48,		/* Up */
	0,
	0,
	0,
	0,
	0x51,		/* Page Down */
	0,
	0x37,		/* Print Screen */
	0x49,		/* Page Up */
	0x46,		/* Ctrl-Break */
	0
};

/*
 * Translate scan codes from set 2 to set 1
 */
int
pckbd_scancode_translate(struct pckbd_internal *id, int datain)
{
	if (id->t_translating != 0)
		return datain;

	if (datain == KBR_BREAK) {
		id->t_releasing = 0x80;	/* next keycode is a release */
		return 0;	/* consume scancode */
	}

	/*
	 * Handle extended sequences
	 */
	if (datain == KBR_EXTENDED0 || datain == KBR_EXTENDED1)
		return datain;

	/*
	 * Convert BREAK sequence (14 77 -> 1D 45)
	 */
	if (id->t_extended1 == 2 && datain == 0x14)
		return 0x1d | id->t_releasing;
	else if (id->t_extended1 == 1 && datain == 0x77)
		return 0x45 | id->t_releasing;

	if (id->t_extended0 != 0) {
		if (datain >= sizeof pckbd_xtbl_ext)
			datain = 0;
		else
			datain = pckbd_xtbl_ext[datain];
	} else {
		if (datain >= sizeof pckbd_xtbl)
			datain = 0;
		else
			datain = pckbd_xtbl[datain];
	}

	/* 
	 * If we are mapping in the range 128-254, then make this
	 * an extended keycode, as table 1 codes are limited to
	 * the range 0-127 (the top bit is used for key up/break).
	 */
	if (datain > 0x7f) {
		datain &= 0x7f;
		id->t_extended0 = 0x80;
	}
		
	if (datain == 0) {
		/*
		 * We don't know how to translate this scan code, but
		 * we can't silently eat it either (because there might
		 * have been an extended byte transmitted already).
		 * Hopefully this value will be harmless to the upper
		 * layers.
		 */
		return 0xff;
	}
	return datain | id->t_releasing;
}

static int
pckbd_decode(struct pckbd_internal *id, int datain, u_int *type, int *dataout)
{
	int key;
	int releasing;

	if (datain == KBR_EXTENDED0) {
		id->t_extended0 = 0x80;
		return 0;
	} else if (datain == KBR_EXTENDED1) {
		id->t_extended1 = 2;
		return 0;
	}

	releasing = datain & 0x80;
	datain &= 0x7f;

	if (id->t_extended0 == 0x80) {
		switch (datain) {
		case 0x2a:
		case 0x36:
			id->t_extended0 = 0;
			return 0;
		default:
			break;
		}
	}

	/* map extended keys to (unused) codes 128-254 */
	key = datain | id->t_extended0;
	id->t_extended0 = 0;

	/*
	 * process PAUSE (also break) key (EXT1 1D 45  EXT1 9D C5):
	 * map to (unused) code 7F
	 */
	if (id->t_extended1 == 2 && (datain == 0x1d || datain == 0x9d)) {
		id->t_extended1 = 1;
		return 0;
	} else if (id->t_extended1 == 1 &&
		   (datain == 0x45 || datain == 0xc5)) {
		id->t_extended1 = 0;
		key = 0x7f;
	} else if (id->t_extended1 > 0) {
		id->t_extended1 = 0;
	}

	if (id->t_translating != 0) {
		id->t_releasing = releasing;
	} else {
		/* id->t_releasing computed in pckbd_scancode_translate() */
	}

	if (id->t_releasing) {
		id->t_releasing = 0;
		id->t_lastchar = 0;
		*type = WSCONS_EVENT_KEY_UP;
	} else {
		/* Always ignore typematic keys */
		if (key == id->t_lastchar)
			return 0;
		id->t_lastchar = key;
		*type = WSCONS_EVENT_KEY_DOWN;
	}

	*dataout = key;
	return 1;
}

int
pckbd_init(struct pckbd_internal *t, pckbport_tag_t kbctag,
    pckbport_slot_t kbcslot, int console)
{

	memset(t, 0, sizeof(struct pckbd_internal));

	t->t_isconsole = console;
	t->t_kbctag = kbctag;
	t->t_kbcslot = kbcslot;

	return pckbd_set_xtscancode(kbctag, kbcslot, t);
}

static int
pckbd_led_encode(int led)
{
	int res;

	res = 0;

	if (led & WSKBD_LED_SCROLL)
		res |= 0x01;
	if (led & WSKBD_LED_NUM)
		res |= 0x02;
	if (led & WSKBD_LED_CAPS)
		res |= 0x04;
	return res;
}

static int
pckbd_led_decode(int led)
{
	int res;

	res = 0;
	if (led & 0x01)
		res |= WSKBD_LED_SCROLL;
	if (led & 0x02)
		res |= WSKBD_LED_NUM;
	if (led & 0x04)
		res |= WSKBD_LED_CAPS;
	return res;
}

void
pckbd_set_leds(void *v, int leds)
{
	struct pckbd_softc *sc = v;
	u_char cmd[2];

	cmd[0] = KBC_MODEIND;
	cmd[1] = pckbd_led_encode(leds);
	sc->sc_ledstate = cmd[1];

	(void)pckbport_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				 cmd, 2, 0, 0, 0);
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 */
void
pckbd_input(void *vsc, int data)
{
	struct pckbd_softc *sc = vsc;
	int key;
	u_int type;

	data = pckbd_scancode_translate(sc->id, data);
	if (data == 0)
		return;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->rawkbd) {
		u_char d = data;
		wskbd_rawinput(sc->sc_wskbddev, &d, 1);
		return;
	}
#endif
	if (pckbd_decode(sc->id, data, &type, &key))
		wskbd_input(sc->sc_wskbddev, type, key);
}

int
pckbd_ioctl(void *v, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct pckbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_PC_XT;
		return 0;
	case WSKBDIO_SETLEDS:
	{
		int res;
		u_char cmdb[2];

		cmdb[0] = KBC_MODEIND;
		cmdb[1] = pckbd_led_encode(*(int *)data);
		sc->sc_ledstate = cmdb[1];
		res = pckbport_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmdb, 2, 0, 1, 0);
		return res;
	}
	case WSKBDIO_GETLEDS:
		*(int *)data = pckbd_led_decode(sc->sc_ledstate);
		return 0;
	case WSKBDIO_COMPLEXBELL:
#define d ((struct wskbd_bell_data *)data)
		/*
		 * Keyboard can't beep directly; we have an
		 * externally-provided global hook to do this.
		 */
		pckbd_bell(d->pitch, d->period, d->volume, 0);
#undef d
		return 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->rawkbd = (*(int *)data == WSKBD_RAW);
		return 0;
#endif
	}
	return EPASSTHROUGH;
}

void
pckbd_bell(u_int pitch, u_int period, u_int volume, int poll)
{

	if (pckbd_bell_fn != NULL)
		(*pckbd_bell_fn)(pckbd_bell_fn_arg, pitch, period,
		    volume, poll);
}

void
pckbd_unhook_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *arg)
{
	if (pckbd_bell_fn != fn && pckbd_bell_fn_arg != arg)
		return;
	pckbd_bell_fn = NULL;
	pckbd_bell_fn_arg = NULL;
}

void
pckbd_hookup_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *arg)
{

	if (pckbd_bell_fn == NULL) {
		pckbd_bell_fn = fn;
		pckbd_bell_fn_arg = arg;
	}
}

int
pckbd_cnattach(pckbport_tag_t kbctag, int kbcslot)
{
	int res;
	u_char cmd[1];

	res = pckbd_init(&pckbd_consdata, kbctag, kbcslot, 1);
	/* We may allow the console to be attached if no keyboard is present */
#if defined(PCKBD_CNATTACH_MAY_FAIL)
	if (res)
		return res;
#endif

	/* Just to be sure. */
	cmd[0] = KBC_ENABLE;
	res = pckbport_poll_cmd(kbctag, kbcslot, cmd, 1, 0, 0, 0);

#if defined(PCKBD_CNATTACH_MAY_FAIL)
	if (res)
		return res;
#endif

	wskbd_cnattach(&pckbd_consops, &pckbd_consdata, &pckbd_keymapdata);

	return res;
}

/* ARGSUSED */
void
pckbd_cngetc(void *v, u_int *type, int *data)
{
        struct pckbd_internal *t = v;
	int val;

	for (;;) {
		val = pckbport_poll_data(t->t_kbctag, t->t_kbcslot);
		if (val == -1)
			continue;

		val = pckbd_scancode_translate(t, val);
		if (val == 0)
			continue;

		if (pckbd_decode(t, val, type, data))
			return;
	}
}

void
pckbd_cnpollc(void *v, int on)
{
	struct pckbd_internal *t = v;

	pckbport_set_poll(t->t_kbctag, t->t_kbcslot, on);
}

void
pckbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{

	pckbd_bell(pitch, period, volume, 1);
}
