/*	$NetBSD: btnmgr.c,v 1.28 2014/07/25 08:10:37 dholland Exp $	*/

/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btnmgr.c,v 1.28 2014/07/25 08:10:37 dholland Exp $");

#ifdef _KERNEL_OPT
#include "opt_btnmgr.h"
#include "opt_wsdisplay_compat.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#ifdef WSDISPLAY_COMPAT_RAWKBD
#include <dev/hpc/pckbd_encode.h>
#endif

#include <sys/bus.h>
#include <machine/autoconf.h>
#include <machine/config_hook.h>

#ifdef BTNMGRDEBUG
#ifndef BTNMGRDEBUG_CONF
#define BTNMGRDEBUG_CONF 0
#endif
int	btnmgr_debug = BTNMGRDEBUG_CONF;
#define	DPRINTF(arg) if (btnmgr_debug) printf arg;
#define	DPRINTFN(n, arg) if (btnmgr_debug > (n)) printf arg;
#else
#define	DPRINTF(arg)
#define DPRINTFN(n, arg)
#endif

struct btnmgr_softc {
	config_hook_tag	sc_hook_tag;
	int sc_enabled;
	device_t sc_wskbddev;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;
#endif
};

int btnmgrmatch(device_t, cfdata_t, void *);
void btnmgrattach(device_t, device_t, void *);
const char *btnmgr_name(long);
static int btnmgr_hook(void *, int, long, void *);

/*
 * global/static data
 */
CFATTACH_DECL_NEW(btnmgr, sizeof(struct btnmgr_softc),
    btnmgrmatch, btnmgrattach, NULL, NULL);

#ifdef notyet
dev_type_open(btnmgropen);
dev_type_close(btnmgrclose);
dev_type_read(btnmgrread);
dev_type_write(btnmgrwrite);
dev_type_ioctl(btnmgrioctl);

const struct cdevsw btnmgr_cdevsw = {
	.d_open = btnmgropen,
	.d_close = btnmgrclose,
	.d_read = btnmgrread,
	.d_write = btnmgrwrite,
	.d_ioctl = btnmgrioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};
#endif /* notyet */

/* wskbd accessopts */
int	btnmgr_wskbd_enable(void *, int);
void	btnmgr_wskbd_set_leds(void *, int);
int	btnmgr_wskbd_ioctl(void *, u_long, void *, int, struct lwp *);

const struct wskbd_accessops btnmgr_wskbd_accessops = {
	btnmgr_wskbd_enable,
	btnmgr_wskbd_set_leds,
	btnmgr_wskbd_ioctl,
};

/* button config: index by button event id */
static const struct {
	int  kevent;
	int  keycode;
	const char *name;
} button_config[] = {
	/* id					kevent keycode name	*/
	[CONFIG_HOOK_BUTTONEVENT_POWER] =	{ 0,   0, "Power"	},
	[CONFIG_HOOK_BUTTONEVENT_OK] =		{ 1,  28, "OK"		},
	[CONFIG_HOOK_BUTTONEVENT_CANCEL] =	{ 1,   1, "Cancel"	},
	[CONFIG_HOOK_BUTTONEVENT_UP] =		{ 1,  72, "Up"		},
	[CONFIG_HOOK_BUTTONEVENT_DOWN] =	{ 1,  80, "Down"	},
	[CONFIG_HOOK_BUTTONEVENT_REC] =		{ 0,   0, "Rec"		},
	[CONFIG_HOOK_BUTTONEVENT_COVER] =	{ 0,   0, "Cover"	},
	[CONFIG_HOOK_BUTTONEVENT_LIGHT] =	{ 1,  57, "Light"	},
	[CONFIG_HOOK_BUTTONEVENT_CONTRAST] =	{ 0,   0, "Contrast"	},
	[CONFIG_HOOK_BUTTONEVENT_APP0] =	{ 1,  67, "Application 0" },
	[CONFIG_HOOK_BUTTONEVENT_APP1] =	{ 1,  68, "Application 1" },
	[CONFIG_HOOK_BUTTONEVENT_APP2] =	{ 1,  87, "Application 2" },
	[CONFIG_HOOK_BUTTONEVENT_APP3] =	{ 1,  88, "Application 3" },
};
static const int n_button_config =
	sizeof(button_config) / sizeof(*button_config);

#define KC(n) KS_KEYCODE(n)
static const keysym_t btnmgr_keydesc_default[] = {
/*  pos				normal			shifted		*/
	KC(1), 			KS_Escape,
	KC(28), 			KS_Return,
	KC(57),	KS_Cmd,		KS_Cmd_BacklightToggle,
	KC(67), 			KS_f9,
	KC(68), 			KS_f10,
	KC(72), 			KS_KP_Up,
	KC(80), 			KS_KP_Down,
	KC(87), 			KS_f11,
	KC(88), 			KS_f12,
};
#undef KC
#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }
const struct wscons_keydesc btnmgr_keydesctab[] = {
	KBD_MAP(KB_US,		0,	btnmgr_keydesc_default),
	{0, 0, 0, 0}
};
#undef KBD_MAP

struct wskbd_mapdata btnmgr_keymapdata = {
	btnmgr_keydesctab,
	KB_US, /* XXX, This is bad idea... */
};

/*
 *  function bodies
 */
int
btnmgrmatch(device_t parent, cfdata_t match, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, match->cf_name))
		return 0;

	return (1);
}

void
btnmgrattach(device_t parent,
	     device_t self, void *aux)
{
	int id;
	struct btnmgr_softc *sc = device_private(self);
	struct wskbddev_attach_args wa;

	printf("\n");

	/*
	 * install button event listener
	 */
	for (id = 0; id <= n_button_config; id++)
		if (button_config[id].name != NULL)
			sc->sc_hook_tag = config_hook(CONFIG_HOOK_BUTTONEVENT,
			    id, CONFIG_HOOK_SHARE,
			    btnmgr_hook, sc);

	/*
	 * attach wskbd
	 */
	wa.console = 0;
	wa.keymap = &btnmgr_keymapdata;
	wa.accessops = &btnmgr_wskbd_accessops;
	wa.accesscookie = sc;

	sc->sc_wskbddev = config_found(self, &wa, wskbddevprint);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "unable to establish power handler\n");
}

static int
btnmgr_hook(void *ctx, int type, long id, void *msg)
{
	struct btnmgr_softc *sc = ctx;

	DPRINTF(("%s button: %s\n", btnmgr_name(id), msg ? "ON" : "OFF"));

	if (button_config[id].kevent) {
		u_int evtype;
		evtype = msg ? WSCONS_EVENT_KEY_DOWN : WSCONS_EVENT_KEY_UP;
#ifdef WSDISPLAY_COMPAT_RAWKBD
		if (sc->sc_rawkbd) {
			int n;
			u_char data[16];
			n = pckbd_encode(evtype, button_config[id].keycode,
			    data);
			wskbd_rawinput(sc->sc_wskbddev, data, n);
		} else
#endif
			wskbd_input(sc->sc_wskbddev, evtype,
			    button_config[id].keycode);
	}

	if (id == CONFIG_HOOK_BUTTONEVENT_POWER && msg)
		config_hook_call(CONFIG_HOOK_PMEVENT,
		    CONFIG_HOOK_PMEVENT_SUSPENDREQ, NULL);
	else if (id == CONFIG_HOOK_BUTTONEVENT_COVER)
		config_hook_call(CONFIG_HOOK_POWERCONTROL,
		    CONFIG_HOOK_POWERCONTROL_LCDLIGHT, (void*)(msg ? 0: 1));

	return (0);
}

const char *
btnmgr_name(long id)
{
	if (id < n_button_config)
		return (button_config[id].name);
	return ("unknown");
}

int
btnmgr_wskbd_enable(void *scx, int on)
{
	struct btnmgr_softc *sc = scx;

	if (on) {
		if (sc->sc_enabled)
			return (EBUSY);
		sc->sc_enabled = 1;
	} else {
		sc->sc_enabled = 0;
	}

	return (0);
}

void
btnmgr_wskbd_set_leds(void *scx, int leds)
{
	/*
	 * We have nothing to do.
	 */
}

int
btnmgr_wskbd_ioctl(void *scx, u_long cmd, void *data, int flag,
		   struct lwp *l)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct btnmgr_softc *sc = scx;
#endif
	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_HPC_BTN;
		return (0);
	case WSKBDIO_SETLEDS:
		DPRINTF(("%s(%d): no LED\n", __FILE__, __LINE__));
		return (0);
	case WSKBDIO_GETLEDS:
		DPRINTF(("%s(%d): no LED\n", __FILE__, __LINE__));
		*(int *)data = 0;
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = (*(int *)data == WSKBD_RAW);
		DPRINTF(("%s(%d): rawkbd is %s\n", __FILE__, __LINE__,
		    sc->sc_rawkbd ? "on" : "off"));
		return (0);
#endif
	}
	return (EPASSTHROUGH);
}

#ifdef notyet
int
btnmgropen(dev_t dev, int flag, int mode, struct lwp *l)
{
	return (EINVAL);
}

int
btnmgrclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	return (EINVAL);
}

int
btnmgrread(dev_t dev, struct uio *uio, int flag)
{
	return (EINVAL);
}

int
btnmgrwrite(dev_t dev, struct uio *uio, int flag)
{
	return (EINVAL);
}

int
btnmgrioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	return (EINVAL);
}
#endif /* notyet */
