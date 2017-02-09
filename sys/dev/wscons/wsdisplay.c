/* $NetBSD: wsdisplay.c,v 1.139 2015/08/24 22:50:33 pooka Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
__KERNEL_RCSID(0, "$NetBSD: wsdisplay.c,v 1.139 2015/08/24 22:50:33 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_wsdisplay_compat.h"
#include "opt_wsmsgattrs.h"
#endif

#include "wskbd.h"
#include "wsmux.h"
#include "wsdisplay.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/kauth.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wsmuxvar.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsemulvar.h>
#include <dev/wscons/wscons_callbacks.h>
#include <dev/cons.h>

#include "locators.h"

struct wsscreen_internal {
	const struct wsdisplay_emulops *emulops;
	void	*emulcookie;

	const struct wsscreen_descr *scrdata;

	const struct wsemul_ops *wsemul;
	void	*wsemulcookie;
};

struct wsscreen {
	struct wsscreen_internal *scr_dconf;

	struct tty *scr_tty;
	int	scr_hold_screen;		/* hold tty output */

	int scr_flags;
#define SCR_OPEN 1		/* is it open? */
#define SCR_WAITACTIVE 2	/* someone waiting on activation */
#define SCR_GRAPHICS 4		/* graphics mode, no text (emulation) output */
#define	SCR_DUMBFB 8		/* in use as a dumb fb (iff SCR_GRAPHICS) */
	const struct wscons_syncops *scr_syncops;
	void *scr_synccookie;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int scr_rawkbd;
#endif

	struct wsdisplay_softc *sc;

#ifdef DIAGNOSTIC
	/* XXX this is to support a hack in emulinput, see comment below */
	int scr_in_ttyoutput;
#endif
};

struct wsscreen *wsscreen_attach(struct wsdisplay_softc *, int,
				 const char *,
				 const struct wsscreen_descr *, void *,
				 int, int, long);
void wsscreen_detach(struct wsscreen *);
int wsdisplay_addscreen(struct wsdisplay_softc *, int, const char *, const char *);
static void wsdisplay_addscreen_print(struct wsdisplay_softc *, int, int);
static void wsdisplay_closescreen(struct wsdisplay_softc *, struct wsscreen *);
int wsdisplay_delscreen(struct wsdisplay_softc *, int, int);

#define WSDISPLAY_MAXSCREEN 8

struct wsdisplay_softc {
	device_t sc_dev;

	const struct wsdisplay_accessops *sc_accessops;
	void	*sc_accesscookie;

	const struct wsscreen_list *sc_scrdata;
#ifdef WSDISPLAY_SCROLLSUPPORT
	struct wsdisplay_scroll_data sc_scroll_values;
#endif

	struct wsscreen *sc_scr[WSDISPLAY_MAXSCREEN];
	int sc_focusidx;	/* available only if sc_focus isn't null */
	struct wsscreen *sc_focus;

	struct wseventvar evar;

	int	sc_isconsole;

	int sc_flags;
#define SC_SWITCHPENDING 1
#define SC_SWITCHERROR 2
#define SC_XATTACHED 4 /* X server active */
	kmutex_t sc_flagsmtx; /* for flags, might also be used for focus */
	kcondvar_t sc_flagscv;

	int sc_screenwanted, sc_oldscreen; /* valid with SC_SWITCHPENDING */

#if NWSKBD > 0
	struct wsevsrc *sc_input;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;
#endif
#endif /* NWSKBD > 0 */
};

#ifdef WSDISPLAY_SCROLLSUPPORT

struct wsdisplay_scroll_data wsdisplay_default_scroll_values = {
	WSDISPLAY_SCROLL_DOALL,
	25,
	2,
};
#endif

extern struct cfdriver wsdisplay_cd;

/* Autoconfiguration definitions. */
static int wsdisplay_emul_match(device_t , cfdata_t, void *);
static void wsdisplay_emul_attach(device_t, device_t, void *);
static int wsdisplay_emul_detach(device_t, int);
static int wsdisplay_noemul_match(device_t, cfdata_t, void *);
static void wsdisplay_noemul_attach(device_t, device_t, void *);
static bool wsdisplay_suspend(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(wsdisplay_emul, sizeof (struct wsdisplay_softc),
    wsdisplay_emul_match, wsdisplay_emul_attach, wsdisplay_emul_detach, NULL);
  
CFATTACH_DECL_NEW(wsdisplay_noemul, sizeof (struct wsdisplay_softc),
    wsdisplay_noemul_match, wsdisplay_noemul_attach, NULL, NULL);

dev_type_open(wsdisplayopen);
dev_type_close(wsdisplayclose);
dev_type_read(wsdisplayread);
dev_type_write(wsdisplaywrite);
dev_type_ioctl(wsdisplayioctl);
dev_type_stop(wsdisplaystop);
dev_type_tty(wsdisplaytty);
dev_type_poll(wsdisplaypoll);
dev_type_mmap(wsdisplaymmap);
dev_type_kqfilter(wsdisplaykqfilter);

const struct cdevsw wsdisplay_cdevsw = {
	.d_open = wsdisplayopen,
	.d_close = wsdisplayclose,
	.d_read = wsdisplayread,
	.d_write = wsdisplaywrite,
	.d_ioctl = wsdisplayioctl,
	.d_stop = wsdisplaystop,
	.d_tty = wsdisplaytty,
	.d_poll = wsdisplaypoll,
	.d_mmap = wsdisplaymmap,
	.d_kqfilter = wsdisplaykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

static void wsdisplaystart(struct tty *);
static int wsdisplayparam(struct tty *, struct termios *);


#define	WSDISPLAYUNIT(dev)	(minor(dev) >> 8)
#define	WSDISPLAYSCREEN(dev)	(minor(dev) & 0xff)
#define ISWSDISPLAYSTAT(dev)	(WSDISPLAYSCREEN(dev) == 254)
#define ISWSDISPLAYCTL(dev)	(WSDISPLAYSCREEN(dev) == 255)
#define WSDISPLAYMINOR(unit, screen)	(((unit) << 8) | (screen))

#define	WSSCREEN_HAS_EMULATOR(scr)	((scr)->scr_dconf->wsemul != NULL)
#define	WSSCREEN_HAS_TTY(scr)	((scr)->scr_tty != NULL)

static void wsdisplay_common_attach(struct wsdisplay_softc *sc,
	    int console, int kbdmux, const struct wsscreen_list *,
	    const struct wsdisplay_accessops *accessops,
	    void *accesscookie);

#ifdef WSDISPLAY_COMPAT_RAWKBD
int wsdisplay_update_rawkbd(struct wsdisplay_softc *,
				 struct wsscreen *);
#endif

static int wsdisplay_console_initted;
static int wsdisplay_console_attached;
static struct wsdisplay_softc *wsdisplay_console_device;
static struct wsscreen_internal wsdisplay_console_conf;

static int wsdisplay_getc_dummy(dev_t);
static void wsdisplay_pollc(dev_t, int);

static int wsdisplay_cons_pollmode;
static void (*wsdisplay_cons_kbd_pollc)(dev_t, int);

static struct consdev wsdisplay_cons = {
	NULL, NULL, wsdisplay_getc_dummy, wsdisplay_cnputc,
	wsdisplay_pollc, NULL, NULL, NULL, NODEV, CN_NORMAL
};

#ifndef WSDISPLAY_DEFAULTSCREENS
# define WSDISPLAY_DEFAULTSCREENS	0
#endif
int wsdisplay_defaultscreens = WSDISPLAY_DEFAULTSCREENS;

static int wsdisplay_switch1(device_t, int, int);
static void wsdisplay_switch1_cb(void *, int, int);
static int wsdisplay_switch2(device_t, int, int);
static void wsdisplay_switch2_cb(void *, int, int);
static int wsdisplay_switch3(device_t, int, int);
static void wsdisplay_switch3_cb(void *, int, int);

int wsdisplay_clearonclose;

struct wsscreen *
wsscreen_attach(struct wsdisplay_softc *sc, int console, const char *emul,
	const struct wsscreen_descr *type, void *cookie, int ccol,
	int crow, long defattr)
{
	struct wsscreen_internal *dconf;
	struct wsscreen *scr;

	scr = malloc(sizeof(struct wsscreen), M_DEVBUF, M_WAITOK);
	if (!scr)
		return (NULL);

	if (console) {
		dconf = &wsdisplay_console_conf;
		/*
		 * If there's an emulation, tell it about the callback argument.
		 * The other stuff is already there.
		 */
		if (dconf->wsemul != NULL)
			(*dconf->wsemul->attach)(1, 0, 0, 0, 0, scr, 0);
	} else { /* not console */
		dconf = malloc(sizeof(struct wsscreen_internal),
			       M_DEVBUF, M_NOWAIT);
		dconf->emulops = type->textops;
		dconf->emulcookie = cookie;
		if (dconf->emulops) {
			dconf->wsemul = wsemul_pick(emul);
			if (dconf->wsemul == NULL) {
				free(dconf, M_DEVBUF);
				free(scr, M_DEVBUF);
				return (NULL);
			}
			dconf->wsemulcookie =
			  (*dconf->wsemul->attach)(0, type, cookie,
						   ccol, crow, scr, defattr);
		} else
			dconf->wsemul = NULL;
		dconf->scrdata = type;
	}

	scr->scr_dconf = dconf;

	scr->scr_tty = tty_alloc();
	tty_attach(scr->scr_tty);
	scr->scr_hold_screen = 0;
	if (WSSCREEN_HAS_EMULATOR(scr))
		scr->scr_flags = 0;
	else
		scr->scr_flags = SCR_GRAPHICS;

	scr->scr_syncops = 0;
	scr->sc = sc;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	scr->scr_rawkbd = 0;
#endif
	return (scr);
}

void
wsscreen_detach(struct wsscreen *scr)
{
	u_int ccol, crow; /* XXX */

	if (WSSCREEN_HAS_TTY(scr)) {
		tty_detach(scr->scr_tty);
		tty_free(scr->scr_tty);
	}
	if (WSSCREEN_HAS_EMULATOR(scr)) {
		(*scr->scr_dconf->wsemul->detach)(scr->scr_dconf->wsemulcookie,
						  &ccol, &crow);
		wsemul_drop(scr->scr_dconf->wsemul);
	}
	free(scr->scr_dconf, M_DEVBUF);
	free(scr, M_DEVBUF);
}

const struct wsscreen_descr *
wsdisplay_screentype_pick(const struct wsscreen_list *scrdata, const char *name)
{
	int i;
	const struct wsscreen_descr *scr;

	KASSERT(scrdata->nscreens > 0);

	if (name == NULL)
		return (scrdata->screens[0]);

	for (i = 0; i < scrdata->nscreens; i++) {
		scr = scrdata->screens[i];
		if (!strcmp(name, scr->name))
			return (scr);
	}

	return (0);
}

/*
 * print info about attached screen
 */
static void
wsdisplay_addscreen_print(struct wsdisplay_softc *sc, int idx, int count)
{
	aprint_verbose_dev(sc->sc_dev, "screen %d", idx);
	if (count > 1)
		aprint_verbose("-%d", idx + (count-1));
	aprint_verbose(" added (%s", sc->sc_scr[idx]->scr_dconf->scrdata->name);
	if (WSSCREEN_HAS_EMULATOR(sc->sc_scr[idx])) {
		aprint_verbose(", %s emulation",
			sc->sc_scr[idx]->scr_dconf->wsemul->name);
	}
	aprint_verbose(")\n");
}

int
wsdisplay_addscreen(struct wsdisplay_softc *sc, int idx,
	const char *screentype, const char *emul)
{
	const struct wsscreen_descr *scrdesc;
	int error;
	void *cookie;
	int ccol, crow;
	long defattr;
	struct wsscreen *scr;
	int s;

	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if (sc->sc_scr[idx] != NULL)
		return (EBUSY);

	scrdesc = wsdisplay_screentype_pick(sc->sc_scrdata, screentype);
	if (!scrdesc)
		return (ENXIO);
	error = (*sc->sc_accessops->alloc_screen)(sc->sc_accesscookie,
			scrdesc, &cookie, &ccol, &crow, &defattr);
	if (error)
		return (error);

	scr = wsscreen_attach(sc, 0, emul, scrdesc,
			      cookie, ccol, crow, defattr);
	if (scr == NULL) {
		(*sc->sc_accessops->free_screen)(sc->sc_accesscookie,
						 cookie);
		return (ENXIO);
	}

	sc->sc_scr[idx] = scr;

	/* if no screen has focus yet, activate the first we get */
	s = spltty();
	if (!sc->sc_focus) {
		(*sc->sc_accessops->show_screen)(sc->sc_accesscookie,
						 scr->scr_dconf->emulcookie,
						 0, 0, 0);
		sc->sc_focusidx = idx;
		sc->sc_focus = scr;
	}
	splx(s);
	return (0);
}

static void
wsdisplay_closescreen(struct wsdisplay_softc *sc, struct wsscreen *scr)
{
	int maj, mn, idx;

	/* hangup */
	if (WSSCREEN_HAS_TTY(scr)) {
		struct tty *tp = scr->scr_tty;
		(*tp->t_linesw->l_modem)(tp, 0);
	}

	/* locate the major number */
	maj = cdevsw_lookup_major(&wsdisplay_cdevsw);
	/* locate the screen index */
	for (idx = 0; idx < WSDISPLAY_MAXSCREEN; idx++)
		if (scr == sc->sc_scr[idx])
			break;
#ifdef DIAGNOSTIC
	if (idx == WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_forceclose: bad screen");
#endif

	/* nuke the vnodes */
	mn = WSDISPLAYMINOR(device_unit(sc->sc_dev), idx);
	vdevgone(maj, mn, mn, VCHR);
}

#ifdef WSDISPLAY_SCROLLSUPPORT
void
wsdisplay_scroll(void *arg, int op)
{
	device_t dv = arg;
	struct wsdisplay_softc *sc = device_private(dv);
	struct wsscreen *scr;
	int lines;

	scr = sc->sc_focus;

	if (!scr)
		return;

	if (op == WSDISPLAY_SCROLL_RESET)
		lines = 0;
	else {
		lines = (op & WSDISPLAY_SCROLL_LOW) ?
			sc->sc_scroll_values.slowlines :
			sc->sc_scroll_values.fastlines;
		if (op & WSDISPLAY_SCROLL_BACKWARD)
			lines = -(lines);
	}

	if (sc->sc_accessops->scroll) {
		(*sc->sc_accessops->scroll)(sc->sc_accesscookie,
		    sc->sc_focus->scr_dconf->emulcookie, lines);
	}
}
#endif

int
wsdisplay_delscreen(struct wsdisplay_softc *sc, int idx, int flags)
{
	struct wsscreen *scr;
	int s;
	void *cookie;

	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if ((scr = sc->sc_scr[idx]) == NULL)
		return (ENXIO);

	if (scr->scr_dconf == &wsdisplay_console_conf ||
	    scr->scr_syncops ||
	    ((scr->scr_flags & SCR_OPEN) && !(flags & WSDISPLAY_DELSCR_FORCE)))
		return(EBUSY);

	wsdisplay_closescreen(sc, scr);

	/*
	 * delete pointers, so neither device entries
	 * nor keyboard input can reference it anymore
	 */
	s = spltty();
	if (sc->sc_focus == scr) {
		sc->sc_focus = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
		wsdisplay_update_rawkbd(sc, 0);
#endif
	}
	sc->sc_scr[idx] = 0;
	splx(s);

	/*
	 * Wake up processes waiting for the screen to
	 * be activated. Sleepers must check whether
	 * the screen still exists.
	 */
	if (scr->scr_flags & SCR_WAITACTIVE)
		wakeup(scr);

	/* save a reference to the graphics screen */
	cookie = scr->scr_dconf->emulcookie;

	wsscreen_detach(scr);

	(*sc->sc_accessops->free_screen)(sc->sc_accesscookie,
					 cookie);

	aprint_verbose_dev(sc->sc_dev, "screen %d deleted\n", idx);
	return (0);
}

/*
 * Autoconfiguration functions.
 */
int
wsdisplay_emul_match(device_t parent, cfdata_t match, void *aux)
{
	struct wsemuldisplaydev_attach_args *ap = aux;

	if (match->cf_loc[WSEMULDISPLAYDEVCF_CONSOLE] !=
	    WSEMULDISPLAYDEVCF_CONSOLE_DEFAULT) {
		/*
		 * If console-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (match->cf_loc[WSEMULDISPLAYDEVCF_CONSOLE] != 0 &&
		    ap->console != 0)
			return (10);
		else
			return (0);
	}

	/* If console-ness unspecified, it wins. */
	return (1);
}

void
wsdisplay_emul_attach(device_t parent, device_t self, void *aux)
{
	struct wsdisplay_softc *sc = device_private(self);
	struct wsemuldisplaydev_attach_args *ap = aux;

	sc->sc_dev = self;

	/* Don't allow more than one console to attach */
	if (wsdisplay_console_attached && ap->console)
		ap->console = 0;

	wsdisplay_common_attach(sc, ap->console,
	     device_cfdata(self)->cf_loc[WSEMULDISPLAYDEVCF_KBDMUX],
	     ap->scrdata, ap->accessops, ap->accesscookie);

	if (ap->console) {
		int maj;

		/* locate the major number */
		maj = cdevsw_lookup_major(&wsdisplay_cdevsw);

		cn_tab->cn_dev = makedev(maj, WSDISPLAYMINOR(device_unit(self),
					 0));
	}
}

/* Print function (for parent devices). */
int
wsemuldisplaydevprint(void *aux, const char *pnp)
{
#if 0 /* -Wunused */
	struct wsemuldisplaydev_attach_args *ap = aux;
#endif

	if (pnp)
		aprint_normal("wsdisplay at %s", pnp);
#if 0 /* don't bother; it's ugly */
	aprint_normal(" console %d", ap->console);
#endif

	return (UNCONF);
}

int
wsdisplay_emul_detach(device_t dev, int how)
{
	struct wsdisplay_softc *sc = device_private(dev);
	int flag, i, res;

	flag = (how & DETACH_FORCE ? WSDISPLAY_DELSCR_FORCE : 0);
	for (i = 0; i < WSDISPLAY_MAXSCREEN; i++)
		if (sc->sc_scr[i]) {
			res = wsdisplay_delscreen(sc, i, flag);
			if (res)
				return res;
		}

	cv_destroy(&sc->sc_flagscv);
	mutex_destroy(&sc->sc_flagsmtx);
	return 0;
}

int
wsdisplay_noemul_match(device_t parent, cfdata_t match, void *aux)
{
#if 0 /* -Wunused */
	struct wsdisplaydev_attach_args *ap = aux;
#endif

	/* Always match. */
	return (1);
}

void
wsdisplay_noemul_attach(device_t parent, device_t self, void *aux)
{
	struct wsdisplay_softc *sc = device_private(self);
	struct wsdisplaydev_attach_args *ap = aux;

	sc->sc_dev = self;

	wsdisplay_common_attach(sc, 0,
	    device_cfdata(self)->cf_loc[WSDISPLAYDEVCF_KBDMUX], NULL,
	    ap->accessops, ap->accesscookie);
}

static void
wsdisplay_swdone_cb(void *arg, int error, int waitok)
{
	struct wsdisplay_softc *sc = arg;

	mutex_enter(&sc->sc_flagsmtx);
	KASSERT(sc->sc_flags & SC_SWITCHPENDING);
	if (error)
		sc->sc_flags |= SC_SWITCHERROR;
	sc->sc_flags &= ~SC_SWITCHPENDING;
	cv_signal(&sc->sc_flagscv);
	mutex_exit(&sc->sc_flagsmtx);
}

static int
wsdisplay_dosync(struct wsdisplay_softc *sc, int attach)
{
	struct wsscreen *scr;
	int (*op)(void *, int, void (*)(void *, int, int), void *);
	int res;

	scr = sc->sc_focus;
	if (!scr || !scr->scr_syncops)
		return 0; /* XXX check SCR_GRAPHICS? */

	sc->sc_flags |= SC_SWITCHPENDING;
	sc->sc_flags &= ~SC_SWITCHERROR;
	if (attach)
		op = scr->scr_syncops->attach;
	else
		op = scr->scr_syncops->detach;
	res = (*op)(scr->scr_synccookie, 1, wsdisplay_swdone_cb, sc);
	if (res == EAGAIN) {
		/* wait for callback */
		mutex_enter(&sc->sc_flagsmtx);
		while (sc->sc_flags & SC_SWITCHPENDING)
			cv_wait_sig(&sc->sc_flagscv, &sc->sc_flagsmtx);
		mutex_exit(&sc->sc_flagsmtx);
		if (sc->sc_flags & SC_SWITCHERROR)
			return (EIO); /* XXX pass real error */
	} else {
		sc->sc_flags &= ~SC_SWITCHPENDING;
		if (res)
			return (res);
	}
	if (attach)
		sc->sc_flags |= SC_XATTACHED;
	else
		sc->sc_flags &= ~SC_XATTACHED;
	return 0;
}

int
wsdisplay_handlex(int resume)
{
	int i, res;
	device_t dv;

	for (i = 0; i < wsdisplay_cd.cd_ndevs; i++) {
		dv = device_lookup(&wsdisplay_cd, i);
		if (!dv)
			continue;
		res = wsdisplay_dosync(device_private(dv), resume);
		if (res)
			return (res);
	}
	return (0);
}

static bool
wsdisplay_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct wsdisplay_softc *sc = device_private(dv);
#ifdef DIAGNOSTIC
	struct wsscreen *scr = sc->sc_focus;
	if (sc->sc_flags & SC_XATTACHED) {
		KASSERT(scr && scr->scr_syncops);
	}
#endif
#if 1
	/*
	 * XXX X servers should have been detached earlier.
	 * pmf currently ignores our return value and suspends the system
	 * after device suspend failures. We try to avoid bigger damage
	 * and try to detach the X server here. This is not safe because
	 * other parts of the system which the X server deals with
	 * might already be suspended.
	 */
	if (sc->sc_flags & SC_XATTACHED) {
		printf("%s: emergency X server detach\n", device_xname(dv));
		wsdisplay_dosync(sc, 0);
	}
#endif
	return (!(sc->sc_flags & SC_XATTACHED));
}

/* Print function (for parent devices). */
int
wsdisplaydevprint(void *aux, const char *pnp)
{
#if 0 /* -Wunused */
	struct wsdisplaydev_attach_args *ap = aux;
#endif

	if (pnp)
		aprint_normal("wsdisplay at %s", pnp);

	return (UNCONF);
}

static void
wsdisplay_common_attach(struct wsdisplay_softc *sc, int console, int kbdmux,
	const struct wsscreen_list *scrdata,
	const struct wsdisplay_accessops *accessops,
	void *accesscookie)
{
	int i, start=0;
#if NWSKBD > 0
	struct wsevsrc *kme;
#if NWSMUX > 0
	struct wsmux_softc *mux;

	if (kbdmux >= 0)
		mux = wsmux_getmux(kbdmux);
	else
		mux = wsmux_create("dmux", device_unit(sc->sc_dev));
	/* XXX panic()ing isn't nice, but attach cannot fail */
	if (mux == NULL)
		panic("wsdisplay_common_attach: no memory");
	sc->sc_input = &mux->sc_base;
	mux->sc_base.me_dispdv = sc->sc_dev;
	aprint_normal(" kbdmux %d", kbdmux);
#else
	if (kbdmux >= 0)
		aprint_normal(" (kbdmux ignored)");
#endif
#endif

	sc->sc_isconsole = console;

	if (console) {
		KASSERT(wsdisplay_console_initted);
		KASSERT(wsdisplay_console_device == NULL);

		sc->sc_scr[0] = wsscreen_attach(sc, 1, 0, 0, 0, 0, 0, 0);
		wsdisplay_console_device = sc;

		aprint_normal(": console (%s, %s emulation)",
		       wsdisplay_console_conf.scrdata->name,
		       wsdisplay_console_conf.wsemul->name);

#if NWSKBD > 0
		kme = wskbd_set_console_display(sc->sc_dev, sc->sc_input);
		if (kme != NULL)
			aprint_normal(", using %s", device_xname(kme->me_dv));
#if NWSMUX == 0
		sc->sc_input = kme;
#endif
#endif

		sc->sc_focusidx = 0;
		sc->sc_focus = sc->sc_scr[0];
		start = 1;

		wsdisplay_console_attached = 1;
	}
	aprint_normal("\n");
	aprint_naive("\n");

#if NWSKBD > 0 && NWSMUX > 0
	wsmux_set_display(mux, sc->sc_dev);
#endif

	mutex_init(&sc->sc_flagsmtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_flagscv, "wssw");

	sc->sc_accessops = accessops;
	sc->sc_accesscookie = accesscookie;
	sc->sc_scrdata = scrdata;

#ifdef WSDISPLAY_SCROLLSUPPORT
	sc->sc_scroll_values = wsdisplay_default_scroll_values;
#endif

	/*
	 * Set up a number of virtual screens if wanted. The
	 * WSDISPLAYIO_ADDSCREEN ioctl is more flexible, so this code
	 * is for special cases like installation kernels.
	 */
	for (i = start; i < wsdisplay_defaultscreens; i++) {
		if (wsdisplay_addscreen(sc, i, 0, 0))
			break;
	}

	if (i > start)
		wsdisplay_addscreen_print(sc, start, i-start);

	if (!pmf_device_register(sc->sc_dev, wsdisplay_suspend, NULL))
		aprint_error_dev(sc->sc_dev, "couldn't establish power handler\n");
}

void
wsdisplay_cnattach(const struct wsscreen_descr *type, void *cookie,
	int ccol, int crow, long defattr)
{
	const struct wsemul_ops *wsemul;

	KASSERT(wsdisplay_console_initted < 2);
	KASSERT(type->nrows > 0);
	KASSERT(type->ncols > 0);
	KASSERT(crow < type->nrows);
	KASSERT(ccol < type->ncols);

	wsdisplay_console_conf.emulops = type->textops;
	wsdisplay_console_conf.emulcookie = cookie;
	wsdisplay_console_conf.scrdata = type;

	wsemul = wsemul_pick(0); /* default */
	wsdisplay_console_conf.wsemul = wsemul;
	wsdisplay_console_conf.wsemulcookie = (*wsemul->cnattach)(type, cookie,
								  ccol, crow,
								  defattr);

	cn_tab = &wsdisplay_cons;
	wsdisplay_console_initted = 2;
}

void
wsdisplay_preattach(const struct wsscreen_descr *type, void *cookie,
	int ccol, int crow, long defattr)
{
	const struct wsemul_ops *wsemul;

	KASSERT(!wsdisplay_console_initted);
	KASSERT(type->nrows > 0);
	KASSERT(type->ncols > 0);
	KASSERT(crow < type->nrows);
	KASSERT(ccol < type->ncols);

	wsdisplay_console_conf.emulops = type->textops;
	wsdisplay_console_conf.emulcookie = cookie;
	wsdisplay_console_conf.scrdata = type;

	wsemul = wsemul_pick(0); /* default */
	wsdisplay_console_conf.wsemul = wsemul;
	wsdisplay_console_conf.wsemulcookie = (*wsemul->cnattach)(type, cookie,
								  ccol, crow,
								  defattr);

	cn_tab = &wsdisplay_cons;
	wsdisplay_console_initted = 1;
}

void
wsdisplay_cndetach(void)
{
	KASSERT(wsdisplay_console_initted == 2);

	cn_tab = NULL;
	wsdisplay_console_initted = 0;
}

/*
 * Tty and cdevsw functions.
 */
int
wsdisplayopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int newopen, error;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));
	if (sc == NULL)			/* make sure it was attached */
		return (ENXIO);

	if (ISWSDISPLAYSTAT(dev)) {
		wsevent_init(&sc->evar, l->l_proc);
		return (0);
	}

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if (WSDISPLAYSCREEN(dev) >= WSDISPLAY_MAXSCREEN)
		return (ENXIO);
	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (WSSCREEN_HAS_TTY(scr)) {
		tp = scr->scr_tty;
		tp->t_oproc = wsdisplaystart;
		tp->t_param = wsdisplayparam;
		tp->t_dev = dev;
		newopen = (tp->t_state & TS_ISOPEN) == 0;

		if (kauth_authorize_device_tty(l->l_cred,
			KAUTH_DEVICE_TTY_OPEN, tp))
			return (EBUSY);

		if (newopen) {
			ttychars(tp);
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
			wsdisplayparam(tp, &tp->t_termios);
			ttsetwater(tp);
		}
		tp->t_state |= TS_CARR_ON;

		error = ((*tp->t_linesw->l_open)(dev, tp));
		if (error)
			return (error);

		if (newopen && WSSCREEN_HAS_EMULATOR(scr)) {
			/* set window sizes as appropriate, and reset
			 the emulation */
			tp->t_winsize.ws_row = scr->scr_dconf->scrdata->nrows;
			tp->t_winsize.ws_col = scr->scr_dconf->scrdata->ncols;

			/* wsdisplay_set_emulation() */
		}
	}

	scr->scr_flags |= SCR_OPEN;
	return (0);
}

int
wsdisplayclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	device_t dv;
	struct wsdisplay_softc *sc;
	struct tty *tp;
	struct wsscreen *scr;

	dv = device_lookup(&wsdisplay_cd, WSDISPLAYUNIT(dev));
	sc = device_private(dv);

	if (ISWSDISPLAYSTAT(dev)) {
		wsevent_fini(&sc->evar);
		return (0);
	}

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (0);

	if (WSSCREEN_HAS_TTY(scr)) {
		if (scr->scr_hold_screen) {
			int s;

			/* XXX RESET KEYBOARD LEDS, etc. */
			s = spltty();	/* avoid conflict with keyboard */
			wsdisplay_kbdholdscreen(dv, 0);
			splx(s);
		}
		tp = scr->scr_tty;
		(*tp->t_linesw->l_close)(tp, flag);
		ttyclose(tp);
	}

	if (scr->scr_syncops)
		(*scr->scr_syncops->destroy)(scr->scr_synccookie);

	if (WSSCREEN_HAS_EMULATOR(scr)) {
		scr->scr_flags &= ~SCR_GRAPHICS;
		(*scr->scr_dconf->wsemul->reset)(scr->scr_dconf->wsemulcookie,
						 WSEMUL_RESET);
		if (wsdisplay_clearonclose)
			(*scr->scr_dconf->wsemul->reset)
				(scr->scr_dconf->wsemulcookie,
				 WSEMUL_CLEARSCREEN);
	}

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (scr->scr_rawkbd) {
		int kbmode = WSKBD_TRANSLATED;
		(void)wsdisplay_internal_ioctl(sc, scr, WSKBDIO_SETMODE,
					       (void *)&kbmode, 0, l);
	}
#endif

	scr->scr_flags &= ~SCR_OPEN;

	return (0);
}

int
wsdisplayread(dev_t dev, struct uio *uio, int flag)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	struct wsscreen *scr;
	int error;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYSTAT(dev)) {
		error = wsevent_read(&sc->evar, uio, flag);
		return (error);
	}

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (!WSSCREEN_HAS_TTY(scr))
		return (ENODEV);

	tp = scr->scr_tty;
	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
wsdisplaywrite(dev_t dev, struct uio *uio, int flag)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYSTAT(dev)) {
		return (0);
	}

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (!WSSCREEN_HAS_TTY(scr))
		return (ENODEV);

	tp = scr->scr_tty;
	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
wsdisplaypoll(dev_t dev, int events, struct lwp *l)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYSTAT(dev))
		return (wsevent_poll(&sc->evar, events, l));

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (POLLHUP);

	if (!WSSCREEN_HAS_TTY(scr))
		return (POLLERR);

	tp = scr->scr_tty;
	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

int
wsdisplaykqfilter(dev_t dev, struct knote *kn)
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYCTL(dev))
		return (1);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (1);


	if (WSSCREEN_HAS_TTY(scr))
		return (ttykqfilter(dev, kn));
	else
		return (1);
}

struct tty *
wsdisplaytty(dev_t dev)
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYSTAT(dev))
		panic("wsdisplaytty() on status device");

	if (ISWSDISPLAYCTL(dev))
		panic("wsdisplaytty() on ctl device");

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return NULL;

	return (scr->scr_tty);
}

int
wsdisplayioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	device_t dv;
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int error;
	struct wsscreen *scr;

	dv = device_lookup(&wsdisplay_cd, WSDISPLAYUNIT(dev));
	sc = device_private(dv);

#ifdef WSDISPLAY_COMPAT_USL
	error = wsdisplay_usl_ioctl1(dv, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);
#endif

	if (ISWSDISPLAYSTAT(dev))
		return (wsdisplay_stat_ioctl(sc, cmd, data, flag, l));

	if (ISWSDISPLAYCTL(dev))
		return (wsdisplay_cfg_ioctl(sc, cmd, data, flag, l));

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (WSSCREEN_HAS_TTY(scr)) {
		tp = scr->scr_tty;

/* printf("disc\n"); */
		/* do the line discipline ioctls first */
		error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
		if (error != EPASSTHROUGH)
			return (error);

/* printf("tty\n"); */
		/* then the tty ioctls */
		error = ttioctl(tp, cmd, data, flag, l);
		if (error != EPASSTHROUGH)
			return (error);
	}

#ifdef WSDISPLAY_COMPAT_USL
	error = wsdisplay_usl_ioctl2(sc, scr, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);
#endif

	return (wsdisplay_internal_ioctl(sc, scr, cmd, data, flag, l));
}

int
wsdisplay_param(device_t dv, u_long cmd, struct wsdisplay_param *dp)
{
	struct wsdisplay_softc *sc = device_private(dv);
	return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie, 
					   sc->sc_focus->scr_dconf->emulcookie,
					   cmd, (void *)dp, 0, NULL));
}

int
wsdisplay_internal_ioctl(struct wsdisplay_softc *sc, struct wsscreen *scr,
	u_long cmd, void *data, int flag, struct lwp *l)
{
	int error;
	char namebuf[16];
	struct wsdisplay_font fd;
#ifdef WSDISPLAY_SCROLLSUPPORT
	struct wsdisplay_scroll_data *ksdp, *usdp;
#endif

#if NWSKBD > 0
	struct wsevsrc *inp;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	switch (cmd) {
	case WSKBDIO_SETMODE:
		scr->scr_rawkbd = (*(int *)data == WSKBD_RAW);
		return (wsdisplay_update_rawkbd(sc, scr));
	case WSKBDIO_GETMODE:
		*(int *)data = (scr->scr_rawkbd ?
				WSKBD_RAW : WSKBD_TRANSLATED);
		return (0);
	}
#endif
	inp = sc->sc_input;
	if (inp == NULL)
		return (ENXIO);
	error = wsevsrc_display_ioctl(inp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);
#endif /* NWSKBD > 0 */

	switch (cmd) {
	case WSDISPLAYIO_GMODE:
		if (scr->scr_flags & SCR_GRAPHICS) {
			if (scr->scr_flags & SCR_DUMBFB)
				*(u_int *)data = WSDISPLAYIO_MODE_DUMBFB;
			else
				*(u_int *)data = WSDISPLAYIO_MODE_MAPPED;
		} else
			*(u_int *)data = WSDISPLAYIO_MODE_EMUL;
		return (0);

	case WSDISPLAYIO_SMODE:
#define d (*(int *)data)
		if (d != WSDISPLAYIO_MODE_EMUL &&
		    d != WSDISPLAYIO_MODE_MAPPED &&
		    d != WSDISPLAYIO_MODE_DUMBFB)
			return (EINVAL);

	    if (WSSCREEN_HAS_EMULATOR(scr)) {
		    scr->scr_flags &= ~SCR_GRAPHICS;
		    if (d == WSDISPLAYIO_MODE_MAPPED ||
			d == WSDISPLAYIO_MODE_DUMBFB)
			    scr->scr_flags |= SCR_GRAPHICS |
				    ((d == WSDISPLAYIO_MODE_DUMBFB) ? SCR_DUMBFB : 0);
	    } else if (d == WSDISPLAYIO_MODE_EMUL)
		    return (EINVAL);

	    (void)(*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
	        scr->scr_dconf->emulcookie, cmd, data, flag, l);

	    return (0);
#undef d

#ifdef WSDISPLAY_SCROLLSUPPORT
#define	SETSCROLLLINES(dstp, srcp, dfltp)				\
    do {								\
	(dstp)->fastlines = ((srcp)->which &				\
			     WSDISPLAY_SCROLL_DOFASTLINES) ?		\
			     (srcp)->fastlines : (dfltp)->fastlines;	\
	(dstp)->slowlines = ((srcp)->which &				\
			     WSDISPLAY_SCROLL_DOSLOWLINES) ?		\
			     (srcp)->slowlines : (dfltp)->slowlines;	\
	(dstp)->which = WSDISPLAY_SCROLL_DOALL;				\
    } while (0)


	case WSDISPLAYIO_DSSCROLL:
		usdp = (struct wsdisplay_scroll_data *)data;
		ksdp = &sc->sc_scroll_values;
		SETSCROLLLINES(ksdp, usdp, ksdp);
		return (0);

	case WSDISPLAYIO_DGSCROLL:
		usdp = (struct wsdisplay_scroll_data *)data;
		ksdp = &sc->sc_scroll_values;
		SETSCROLLLINES(usdp, ksdp, ksdp);
		return (0);
#else
	case WSDISPLAYIO_DSSCROLL:
	case WSDISPLAYIO_DGSCROLL:
		return ENODEV;
#endif

	case WSDISPLAYIO_SFONT:
#define d ((struct wsdisplay_usefontdata *)data)
		if (!sc->sc_accessops->load_font)
			return (EINVAL);
		if (d->name) {
			error = copyinstr(d->name, namebuf, sizeof(namebuf), 0);
			if (error)
				return (error);
			fd.name = namebuf;
		} else
			fd.name = 0;
		fd.data = 0;
		error = (*sc->sc_accessops->load_font)(sc->sc_accesscookie,
					scr->scr_dconf->emulcookie, &fd);
		if (!error && WSSCREEN_HAS_EMULATOR(scr))
			(*scr->scr_dconf->wsemul->reset)
				(scr->scr_dconf->wsemulcookie, WSEMUL_SYNCFONT);
		return (error);
#undef d

#ifdef WSDISPLAY_CUSTOM_OUTPUT
	case WSDISPLAYIO_GMSGATTRS:
#define d ((struct wsdisplay_msgattrs *)data)
		(*scr->scr_dconf->wsemul->getmsgattrs)
		    (scr->scr_dconf->wsemulcookie, d);
		return (0);
#undef d

	case WSDISPLAYIO_SMSGATTRS: {
#define d ((struct wsdisplay_msgattrs *)data)
		int i;
		for (i = 0; i < WSDISPLAY_MAXSCREEN; i++)
			if (sc->sc_scr[i] != NULL)
				(*sc->sc_scr[i]->scr_dconf->wsemul->setmsgattrs)
				    (sc->sc_scr[i]->scr_dconf->wsemulcookie,
				     sc->sc_scr[i]->scr_dconf->scrdata,
				     d);
		}
		return (0);
#undef d
#else
	case WSDISPLAYIO_GMSGATTRS:
	case WSDISPLAYIO_SMSGATTRS:
		return (ENODEV);
#endif
	case WSDISPLAYIO_SETVERSION:
		return wsevent_setversion(&sc->evar, *(int *)data);
	}

	/* check ioctls for display */
	return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
	    scr->scr_dconf->emulcookie, cmd, data, flag, l));
}

int
wsdisplay_stat_ioctl(struct wsdisplay_softc *sc, u_long cmd, void *data,
	int flag, struct lwp *l)
{
	switch (cmd) {
	case WSDISPLAYIO_GETACTIVESCREEN:
		*(int*)data = wsdisplay_getactivescreen(sc);
		return (0);
	}

	return (EPASSTHROUGH);
}

int
wsdisplay_cfg_ioctl(struct wsdisplay_softc *sc, u_long cmd, void *data,
	int flag, struct lwp *l)
{
	int error;
	char *type, typebuf[16], *emul, emulbuf[16];
	void *tbuf;
	u_int fontsz;
#if defined(COMPAT_14) && NWSKBD > 0
	struct wsmux_device wsmuxdata;
#endif
#if NWSKBD > 0
	struct wsevsrc *inp;
#endif

	switch (cmd) {
	case WSDISPLAYIO_ADDSCREEN:
#define d ((struct wsdisplay_addscreendata *)data)
		if (d->screentype) {
			error = copyinstr(d->screentype, typebuf,
					  sizeof(typebuf), 0);
			if (error)
				return (error);
			type = typebuf;
		} else
			type = 0;
		if (d->emul) {
			error = copyinstr(d->emul, emulbuf, sizeof(emulbuf),0);
			if (error)
				return (error);
			emul = emulbuf;
		} else
			emul = 0;

		if ((error = wsdisplay_addscreen(sc, d->idx, type, emul)) == 0)
			wsdisplay_addscreen_print(sc, d->idx, 0);
		return (error);
#undef d
	case WSDISPLAYIO_DELSCREEN:
#define d ((struct wsdisplay_delscreendata *)data)
		return (wsdisplay_delscreen(sc, d->idx, d->flags));
#undef d
	case WSDISPLAYIO_LDFONT:
#define d ((struct wsdisplay_font *)data)
		if (!sc->sc_accessops->load_font)
			return (EINVAL);
		if (d->name) {
			error = copyinstr(d->name, typebuf, sizeof(typebuf), 0);
			if (error)
				return (error);
			d->name = typebuf;
		} else
			d->name = "loaded"; /* ??? */
		fontsz = d->fontheight * d->stride * d->numchars;
		if (fontsz > WSDISPLAY_MAXFONTSZ)
			return (EINVAL);

		tbuf = malloc(fontsz, M_DEVBUF, M_WAITOK);
		error = copyin(d->data, tbuf, fontsz);
		if (error) {
			free(tbuf, M_DEVBUF);
			return (error);
		}
		d->data = tbuf;
		error =
		  (*sc->sc_accessops->load_font)(sc->sc_accesscookie, 0, d);
		free(tbuf, M_DEVBUF);
#undef d
		return (error);

#if NWSKBD > 0
#ifdef COMPAT_14
	case _O_WSDISPLAYIO_SETKEYBOARD:
#define d ((struct wsdisplay_kbddata *)data)
		inp = sc->sc_input;
		if (inp == NULL)
			return (ENXIO);
		switch (d->op) {
		case _O_WSDISPLAY_KBD_ADD:
			if (d->idx == -1) {
				d->idx = wskbd_pickfree();
				if (d->idx == -1)
					return (ENXIO);
			}
			wsmuxdata.type = WSMUX_KBD;
			wsmuxdata.idx = d->idx;
			return (wsevsrc_ioctl(inp, WSMUX_ADD_DEVICE,
					      &wsmuxdata, flag, l));
		case _O_WSDISPLAY_KBD_DEL:
			wsmuxdata.type = WSMUX_KBD;
			wsmuxdata.idx = d->idx;
			return (wsevsrc_ioctl(inp, WSMUX_REMOVE_DEVICE,
					      &wsmuxdata, flag, l));
		default:
			return (EINVAL);
		}
#undef d
#endif

	case WSMUXIO_ADD_DEVICE:
#define d ((struct wsmux_device *)data)
		if (d->idx == -1 && d->type == WSMUX_KBD)
			d->idx = wskbd_pickfree();
#undef d
		/* fall into */
	case WSMUXIO_INJECTEVENT:
	case WSMUXIO_REMOVE_DEVICE:
	case WSMUXIO_LIST_DEVICES:
		inp = sc->sc_input;
		if (inp == NULL)
			return (ENXIO);
		return (wsevsrc_ioctl(inp, cmd, data, flag, l));
#endif /* NWSKBD > 0 */

	}
	return (EPASSTHROUGH);
}

int
wsdisplay_stat_inject(device_t dv, u_int type, int value)
{
	struct wsdisplay_softc *sc = device_private(dv);
	struct wseventvar *evar;
	struct wscons_event event;

	evar = &sc->evar;

	if (evar == NULL)
		return (0);

	if (evar->q == NULL)
		return (1);

	event.type = type;
	event.value = value;
	if (wsevent_inject(evar, &event, 1) != 0) {
		log(LOG_WARNING, "wsdisplay: event queue overflow\n");
		return (1);
	}

	return (0);
}

paddr_t
wsdisplaymmap(dev_t dev, off_t offset, int prot)
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;

	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(dev));

	if (ISWSDISPLAYSTAT(dev))
		return (-1);

	if (ISWSDISPLAYCTL(dev))
		return (-1);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (-1);

	if (!(scr->scr_flags & SCR_GRAPHICS))
		return (-1);

	/* pass mmap to display */
	return ((*sc->sc_accessops->mmap)(sc->sc_accesscookie,
	    scr->scr_dconf->emulcookie, offset, prot));
}

void
wsdisplaystart(struct tty *tp)
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;
	int s, n;
	u_char *tbuf;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	sc = device_lookup_private(&wsdisplay_cd, WSDISPLAYUNIT(tp->t_dev));
	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(tp->t_dev)]) == NULL) {
		splx(s);
		return;
	}

	if (scr->scr_hold_screen) {
		tp->t_state |= TS_TIMEOUT;
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);

#ifdef DIAGNOSTIC
	scr->scr_in_ttyoutput = 1;
#endif

	/*
	 * Drain output from ring buffer.
	 * The output will normally be in one contiguous chunk, but when the
	 * ring wraps, it will be in two pieces.. one at the end of the ring,
	 * the other at the start.  For performance, rather than loop here,
	 * we output one chunk, see if there's another one, and if so, output
	 * it too.
	 */

	n = ndqb(&tp->t_outq, 0);
	tbuf = tp->t_outq.c_cf;

	if (!(scr->scr_flags & SCR_GRAPHICS)) {
		KASSERT(WSSCREEN_HAS_EMULATOR(scr));
		(*scr->scr_dconf->wsemul->output)(scr->scr_dconf->wsemulcookie,
						  tbuf, n, 0);
	}
	ndflush(&tp->t_outq, n);

	if ((n = ndqb(&tp->t_outq, 0)) > 0) {
		tbuf = tp->t_outq.c_cf;

		if (!(scr->scr_flags & SCR_GRAPHICS)) {
			KASSERT(WSSCREEN_HAS_EMULATOR(scr));
			(*scr->scr_dconf->wsemul->output)
			    (scr->scr_dconf->wsemulcookie, tbuf, n, 0);
		}
		ndflush(&tp->t_outq, n);
	}

#ifdef DIAGNOSTIC
	scr->scr_in_ttyoutput = 0;
#endif

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (ttypull(tp)) {
		tp->t_state |= TS_TIMEOUT;
		callout_schedule(&tp->t_rstrt_ch, (hz > 128) ? (hz / 128) : 1);
	}
	splx(s);
}

void
wsdisplaystop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	splx(s);
}

/* Set line parameters. */
int
wsdisplayparam(struct tty *tp, struct termios *t)
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

/*
 * Callbacks for the emulation code.
 */
void
wsdisplay_emulbell(void *v)
{
	struct wsscreen *scr = v;

	if (scr == NULL)		/* console, before real attach */
		return;

	if (scr->scr_flags & SCR_GRAPHICS) /* can this happen? */
		return;

	(void) wsdisplay_internal_ioctl(scr->sc, scr, WSKBDIO_BELL, NULL,
					FWRITE, NULL);
}

void
wsdisplay_emulinput(void *v, const u_char *data, u_int count)
{
	struct wsscreen *scr = v;
	struct tty *tp;
	int (*ifcn)(int, struct tty *);

	if (v == NULL)			/* console, before real attach */
		return;

	if (scr->scr_flags & SCR_GRAPHICS) /* XXX can't happen */
		return;
	if (!WSSCREEN_HAS_TTY(scr))
		return;

	tp = scr->scr_tty;

	/*
	 * XXX bad hack to work around locking problems in tty.c:
	 * ttyinput() will try to lock again, causing deadlock.
	 * We assume that wsdisplay_emulinput() can only be called
	 * from within wsdisplaystart(), and thus the tty lock
	 * is already held. Use an entry point which doesn't lock.
	 */
	KASSERT(scr->scr_in_ttyoutput);
	ifcn = tp->t_linesw->l_rint;
	if (ifcn == ttyinput)
		ifcn = ttyinput_wlock;

	while (count-- > 0)
		(*ifcn)(*data++, tp);
}

/*
 * Calls from the keyboard interface.
 */
void
wsdisplay_kbdinput(device_t dv, keysym_t ks)
{
	struct wsdisplay_softc *sc = device_private(dv);
	struct wsscreen *scr;
	const char *dp;
	int count;
	struct tty *tp;

	KASSERT(sc != NULL);

	scr = sc->sc_focus;

	if (!scr || !WSSCREEN_HAS_TTY(scr))
		return;

	tp = scr->scr_tty;

	if (KS_GROUP(ks) == KS_GROUP_Plain && KS_VALUE(ks) <= 0x7f)
		(*tp->t_linesw->l_rint)(KS_VALUE(ks), tp);
	else if (WSSCREEN_HAS_EMULATOR(scr)) {
		count = (*scr->scr_dconf->wsemul->translate)
		    (scr->scr_dconf->wsemulcookie, ks, &dp);
		while (count-- > 0)
			(*tp->t_linesw->l_rint)((unsigned char)(*dp++), tp);
	}
}

#if defined(WSDISPLAY_COMPAT_RAWKBD)
int
wsdisplay_update_rawkbd(struct wsdisplay_softc *sc, struct wsscreen *scr)
{
#if NWSKBD > 0
	int s, raw, data, error;
	struct wsevsrc *inp;

	s = spltty();

	raw = (scr ? scr->scr_rawkbd : 0);

	if (scr != sc->sc_focus ||
	    sc->sc_rawkbd == raw) {
		splx(s);
		return (0);
	}

	data = raw ? WSKBD_RAW : WSKBD_TRANSLATED;
	inp = sc->sc_input;
	if (inp == NULL) {
		splx(s);
		return (ENXIO);
	}
	error = wsevsrc_display_ioctl(inp, WSKBDIO_SETMODE, &data, 0, 0);
	if (!error)
		sc->sc_rawkbd = raw;
	splx(s);
	return (error);
#else
	return (0);
#endif
}
#endif

static void
wsdisplay_switch3_cb(void *arg, int error, int waitok)
{
	device_t dv = arg;

	wsdisplay_switch3(dv, error, waitok);
}

static int
wsdisplay_switch3(device_t dv, int error, int waitok)
{
	struct wsdisplay_softc *sc = device_private(dv);
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		aprint_error_dev(dv, "wsdisplay_switch3: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch3: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		aprint_error_dev(dv,
		    "wsdisplay_switch3: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		/* try to recover, avoid recursion */

		if (sc->sc_oldscreen == WSDISPLAY_NULLSCREEN) {
			aprint_error_dev(dv, "wsdisplay_switch3: giving up\n");
			sc->sc_focus = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
			wsdisplay_update_rawkbd(sc, 0);
#endif
			sc->sc_flags &= ~SC_SWITCHPENDING;
			return (error);
		}

		sc->sc_screenwanted = sc->sc_oldscreen;
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(dv, 0, waitok));
	}

	if (scr->scr_syncops && !error)
		sc->sc_flags |= SC_XATTACHED;

	sc->sc_flags &= ~SC_SWITCHPENDING;

	if (!error && (scr->scr_flags & SCR_WAITACTIVE))
		wakeup(scr);
	return (error);
}

static void
wsdisplay_switch2_cb(void *arg, int error, int waitok)
{
	device_t dv = arg;

	wsdisplay_switch2(dv, error, waitok);
}

static int
wsdisplay_switch2(device_t dv, int error, int waitok)
{
	struct wsdisplay_softc *sc = device_private(dv);
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		aprint_error_dev(dv, "wsdisplay_switch2: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch2: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		aprint_error_dev(dv,
		    "wsdisplay_switch2: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		/* try to recover, avoid recursion */

		if (sc->sc_oldscreen == WSDISPLAY_NULLSCREEN) {
			aprint_error_dev(dv, "wsdisplay_switch2: giving up\n");
			sc->sc_focus = 0;
			sc->sc_flags &= ~SC_SWITCHPENDING;
			return (error);
		}

		sc->sc_screenwanted = sc->sc_oldscreen;
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(dv, 0, waitok));
	}

	sc->sc_focusidx = no;
	sc->sc_focus = scr;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	(void) wsdisplay_update_rawkbd(sc, scr);
#endif
	/* keyboard map??? */

	if (scr->scr_syncops &&
	    !(sc->sc_isconsole && wsdisplay_cons_pollmode)) {
		error = (*scr->scr_syncops->attach)(scr->scr_synccookie, waitok,
						    wsdisplay_switch3_cb, dv);
		if (error == EAGAIN) {
			/* switch will be done asynchronously */
			return (0);
		}
	}

	return (wsdisplay_switch3(dv, error, waitok));
}

static void
wsdisplay_switch1_cb(void *arg, int error, int waitok)
{
	device_t dv = arg;

	wsdisplay_switch1(dv, error, waitok);
}

static int
wsdisplay_switch1(device_t dv, int error, int waitok)
{
	struct wsdisplay_softc *sc = device_private(dv);
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		aprint_error_dev(dv, "wsdisplay_switch1: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no == WSDISPLAY_NULLSCREEN) {
		sc->sc_flags &= ~SC_SWITCHPENDING;
		if (!error) {
			sc->sc_flags &= ~SC_XATTACHED;
			sc->sc_focus = 0;
		}
		wakeup(sc);
		return (error);
	}
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch1: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		aprint_error_dev(dv, "wsdisplay_switch1: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		sc->sc_flags &= ~SC_SWITCHPENDING;
		return (error);
	}

	sc->sc_flags &= ~SC_XATTACHED;

	error = (*sc->sc_accessops->show_screen)(sc->sc_accesscookie,
						 scr->scr_dconf->emulcookie,
						 waitok,
	  sc->sc_isconsole && wsdisplay_cons_pollmode ? 0 : wsdisplay_switch2_cb, dv);
	if (error == EAGAIN) {
		/* switch will be done asynchronously */
		return (0);
	}

	return (wsdisplay_switch2(dv, error, waitok));
}

int
wsdisplay_switch(device_t dv, int no, int waitok)
{
	struct wsdisplay_softc *sc = device_private(dv);
	int s, res = 0;
	struct wsscreen *scr;

	if (no != WSDISPLAY_NULLSCREEN) {
		if ((no < 0 || no >= WSDISPLAY_MAXSCREEN))
			return (EINVAL);
		if (sc->sc_scr[no] == NULL)
			return (ENXIO);
	}

	wsdisplay_stat_inject(dv, WSCONS_EVENT_SCREEN_SWITCH, no);

	s = spltty();

	if ((sc->sc_focus && no == sc->sc_focusidx) ||
	    (sc->sc_focus == NULL && no == WSDISPLAY_NULLSCREEN)) {
		splx(s);
		return (0);
	}

	if (sc->sc_flags & SC_SWITCHPENDING) {
		splx(s);
		return (EBUSY);
	}

	sc->sc_flags |= SC_SWITCHPENDING;
	sc->sc_screenwanted = no;

	splx(s);

	scr = sc->sc_focus;
	if (!scr) {
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(dv, 0, waitok));
	} else
		sc->sc_oldscreen = sc->sc_focusidx;

	if (scr->scr_syncops) {
		if (!(sc->sc_flags & SC_XATTACHED) ||
		    (sc->sc_isconsole && wsdisplay_cons_pollmode)) {
			/* nothing to do here */
			return (wsdisplay_switch1(dv, 0, waitok));
		}
		res = (*scr->scr_syncops->detach)(scr->scr_synccookie, waitok,
						  wsdisplay_switch1_cb, dv);
		if (res == EAGAIN) {
			/* switch will be done asynchronously */
			return (0);
		}
	} else if (scr->scr_flags & SCR_GRAPHICS) {
		/* no way to save state */
		res = EBUSY;
	}

	return (wsdisplay_switch1(dv, res, waitok));
}

void
wsdisplay_reset(device_t dv, enum wsdisplay_resetops op)
{
	struct wsdisplay_softc *sc = device_private(dv);
	struct wsscreen *scr;

	KASSERT(sc != NULL);
	scr = sc->sc_focus;

	if (!scr)
		return;

	switch (op) {
	case WSDISPLAY_RESETEMUL:
		if (!WSSCREEN_HAS_EMULATOR(scr))
			break;
		(*scr->scr_dconf->wsemul->reset)(scr->scr_dconf->wsemulcookie,
						 WSEMUL_RESET);
		break;
	case WSDISPLAY_RESETCLOSE:
		wsdisplay_closescreen(sc, scr);
		break;
	}
}

/*
 * Interface for (external) VT switch / process synchronization code
 */
int
wsscreen_attach_sync(struct wsscreen *scr, const struct wscons_syncops *ops,
	void *cookie)
{
	if (scr->scr_syncops) {
		/*
		 * The screen is already claimed.
		 * Check if the owner is still alive.
		 */
		if ((*scr->scr_syncops->check)(scr->scr_synccookie))
			return (EBUSY);
	}
	scr->scr_syncops = ops;
	scr->scr_synccookie = cookie;
	if (scr == scr->sc->sc_focus)
		scr->sc->sc_flags |= SC_XATTACHED;
	return (0);
}

int
wsscreen_detach_sync(struct wsscreen *scr)
{
	if (!scr->scr_syncops)
		return (EINVAL);
	scr->scr_syncops = 0;
	if (scr == scr->sc->sc_focus)
		scr->sc->sc_flags &= ~SC_XATTACHED;
	return (0);
}

int
wsscreen_lookup_sync(struct wsscreen *scr,
	const struct wscons_syncops *ops, /* used as ID */
	void **cookiep)
{
	if (!scr->scr_syncops || ops != scr->scr_syncops)
		return (EINVAL);
	*cookiep = scr->scr_synccookie;
	return (0);
}

/*
 * Interface to virtual screen stuff
 */
int
wsdisplay_maxscreenidx(struct wsdisplay_softc *sc)
{
	return (WSDISPLAY_MAXSCREEN - 1);
}

int
wsdisplay_screenstate(struct wsdisplay_softc *sc, int idx)
{
	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if (!sc->sc_scr[idx])
		return (ENXIO);
	return ((sc->sc_scr[idx]->scr_flags & SCR_OPEN) ? EBUSY : 0);
}

int
wsdisplay_getactivescreen(struct wsdisplay_softc *sc)
{
	return (sc->sc_focus ? sc->sc_focusidx : WSDISPLAY_NULLSCREEN);
}

int
wsscreen_switchwait(struct wsdisplay_softc *sc, int no)
{
	struct wsscreen *scr;
	int s, res = 0;

	if (no == WSDISPLAY_NULLSCREEN) {
		s = spltty();
		while (sc->sc_focus && res == 0) {
			res = tsleep(sc, PCATCH, "wswait", 0);
		}
		splx(s);
		return (res);
	}

	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		return (ENXIO);
	scr = sc->sc_scr[no];
	if (!scr)
		return (ENXIO);

	s = spltty();
	if (scr != sc->sc_focus) {
		scr->scr_flags |= SCR_WAITACTIVE;
		res = tsleep(scr, PCATCH, "wswait", 0);
		if (scr != sc->sc_scr[no])
			res = ENXIO; /* disappeared in the meantime */
		else
			scr->scr_flags &= ~SCR_WAITACTIVE;
	}
	splx(s);
	return (res);
}

void
wsdisplay_kbdholdscreen(device_t dv, int hold)
{
	struct wsdisplay_softc *sc = device_private(dv);
	struct wsscreen *scr;

	scr = sc->sc_focus;

	if (!scr)
		return;

	if (hold)
		scr->scr_hold_screen = 1;
	else {
		scr->scr_hold_screen = 0;
		callout_schedule(&scr->scr_tty->t_rstrt_ch, 0);
	}
}

#if NWSKBD > 0
void
wsdisplay_set_console_kbd(struct wsevsrc *src)
{
	if (wsdisplay_console_device == NULL) {
		src->me_dispdv = NULL;
		return;
	}
#if NWSMUX > 0
	if (wsmux_attach_sc((struct wsmux_softc *)
			    wsdisplay_console_device->sc_input, src)) {
		src->me_dispdv = NULL;
		return;
	}
#else
	wsdisplay_console_device->sc_input = src;
#endif
	src->me_dispdv = wsdisplay_console_device->sc_dev;
}
#endif /* NWSKBD > 0 */

/*
 * Console interface.
 */
void
wsdisplay_cnputc(dev_t dev, int i)
{
	struct wsscreen_internal *dc;
	u_char c = i;

	if (!wsdisplay_console_initted)
		return;

	if ((wsdisplay_console_device != NULL) &&
	    (wsdisplay_console_device->sc_scr[0] != NULL) &&
	    (wsdisplay_console_device->sc_scr[0]->scr_flags & SCR_GRAPHICS))
		return;

	dc = &wsdisplay_console_conf;
	(*dc->wsemul->output)(dc->wsemulcookie, &c, 1, 1);
}

static int
wsdisplay_getc_dummy(dev_t dev)
{
	/* panic? */
	return (0);
}

static void
wsdisplay_pollc(dev_t dev, int on)
{

	wsdisplay_cons_pollmode = on;

	/* notify to fb drivers */
	if (wsdisplay_console_device != NULL &&
	    wsdisplay_console_device->sc_accessops->pollc != NULL)
		(*wsdisplay_console_device->sc_accessops->pollc)
			(wsdisplay_console_device->sc_accesscookie, on);

	/* notify to kbd drivers */
	if (wsdisplay_cons_kbd_pollc)
		(*wsdisplay_cons_kbd_pollc)(NODEV, on);
}

void
wsdisplay_set_cons_kbd(int (*get)(dev_t), void (*poll)(dev_t, int),
	void (*bell)(dev_t, u_int, u_int, u_int))
{
	wsdisplay_cons.cn_getc = get;
	wsdisplay_cons.cn_bell = bell;
	wsdisplay_cons_kbd_pollc = poll;
}

void
wsdisplay_unset_cons_kbd(void)
{
	wsdisplay_cons.cn_getc = wsdisplay_getc_dummy;
	wsdisplay_cons.cn_bell = NULL;
	wsdisplay_cons_kbd_pollc = 0;
}
