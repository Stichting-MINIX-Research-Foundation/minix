/* $NetBSD: wsmouse.c,v 1.66 2014/07/25 08:10:39 dholland Exp $ */

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal.
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

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Mouse driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsmouse.c,v 1.66 2014/07/25 08:10:39 dholland Exp $");

#include "wsmouse.h"
#include "wsdisplay.h"
#include "wsmux.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/callout.h>
#include <sys/malloc.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wsmuxvar.h>

#if defined(WSMUX_DEBUG) && NWSMUX > 0
#define DPRINTF(x)	if (wsmuxdebug) printf x
#define DPRINTFN(n,x)	if (wsmuxdebug > (n)) printf x
extern int wsmuxdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define INVALID_X	INT_MAX
#define INVALID_Y	INT_MAX
#define INVALID_Z	INT_MAX
#define INVALID_W	INT_MAX

struct wsmouse_softc {
	struct wsevsrc	sc_base;

	const struct wsmouse_accessops *sc_accessops;
	void		*sc_accesscookie;

	u_int		sc_mb;		/* mouse button state */
	u_int		sc_ub;		/* user button state */
	int		sc_dx;		/* delta-x */
	int		sc_dy;		/* delta-y */
	int		sc_dz;		/* delta-z */
	int		sc_dw;		/* delta-w */
	int		sc_x;		/* absolute-x */
	int		sc_y;		/* absolute-y */
	int		sc_z;		/* absolute-z */
	int		sc_w;		/* absolute-w */

	int		sc_refcnt;
	u_char		sc_dying;	/* device is being detached */

	struct wsmouse_repeat	sc_repeat;
	int			sc_repeat_button;
	callout_t		sc_repeat_callout;
	unsigned int		sc_repeat_delay;
};

static int  wsmouse_match(device_t, cfdata_t, void *);
static void wsmouse_attach(device_t, device_t, void *);
static int  wsmouse_detach(device_t, int);
static int  wsmouse_activate(device_t, enum devact);

static int  wsmouse_do_ioctl(struct wsmouse_softc *, u_long, void *,
			     int, struct lwp *);

#if NWSMUX > 0
static int  wsmouse_mux_open(struct wsevsrc *, struct wseventvar *);
static int  wsmouse_mux_close(struct wsevsrc *);
#endif

static int  wsmousedoioctl(device_t, u_long, void *, int, struct lwp *);

static int  wsmousedoopen(struct wsmouse_softc *, struct wseventvar *);

CFATTACH_DECL_NEW(wsmouse, sizeof (struct wsmouse_softc),
    wsmouse_match, wsmouse_attach, wsmouse_detach, wsmouse_activate);

static void wsmouse_repeat(void *v);

extern struct cfdriver wsmouse_cd;

dev_type_open(wsmouseopen);
dev_type_close(wsmouseclose);
dev_type_read(wsmouseread);
dev_type_ioctl(wsmouseioctl);
dev_type_poll(wsmousepoll);
dev_type_kqfilter(wsmousekqfilter);

const struct cdevsw wsmouse_cdevsw = {
	.d_open = wsmouseopen,
	.d_close = wsmouseclose,
	.d_read = wsmouseread,
	.d_write = nowrite,
	.d_ioctl = wsmouseioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = wsmousepoll,
	.d_mmap = nommap,
	.d_kqfilter = wsmousekqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

#if NWSMUX > 0
struct wssrcops wsmouse_srcops = {
	WSMUX_MOUSE,
	wsmouse_mux_open, wsmouse_mux_close, wsmousedoioctl, NULL, NULL
};
#endif

/*
 * Print function (for parent devices).
 */
int
wsmousedevprint(void *aux, const char *pnp)
{

	if (pnp)
		aprint_normal("wsmouse at %s", pnp);
	return (UNCONF);
}

int
wsmouse_match(device_t parent, cfdata_t match, void *aux)
{
	return (1);
}

void
wsmouse_attach(device_t parent, device_t self, void *aux)
{
        struct wsmouse_softc *sc = device_private(self);
	struct wsmousedev_attach_args *ap = aux;
#if NWSMUX > 0
	int mux, error;
#endif

	sc->sc_base.me_dv = self;
	sc->sc_accessops = ap->accessops;
	sc->sc_accesscookie = ap->accesscookie;

	/* Initialize button repeating. */
	memset(&sc->sc_repeat, 0, sizeof(sc->sc_repeat));
	sc->sc_repeat_button = -1;
	sc->sc_repeat_delay = 0;
	callout_init(&sc->sc_repeat_callout, 0);
	callout_setfunc(&sc->sc_repeat_callout, wsmouse_repeat, sc);

#if NWSMUX > 0
	sc->sc_base.me_ops = &wsmouse_srcops;
	mux = device_cfdata(self)->wsmousedevcf_mux;
	if (mux >= 0) {
		error = wsmux_attach_sc(wsmux_getmux(mux), &sc->sc_base);
		if (error)
			aprint_error(" attach error=%d", error);
		else
			aprint_normal(" mux %d", mux);
	}
#else
	if (device_cfdata(self)->wsmousedevcf_mux >= 0)
		aprint_normal(" (mux ignored)");
#endif

	aprint_naive("\n");
	aprint_normal("\n");

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
wsmouse_activate(device_t self, enum devact act)
{
	struct wsmouse_softc *sc = device_private(self);

	if (act == DVACT_DEACTIVATE)
		sc->sc_dying = 1;
	return (0);
}

/*
 * Detach a mouse.  To keep track of users of the softc we keep
 * a reference count that's incremented while inside, e.g., read.
 * If the mouse is active and the reference count is > 0 (0 is the
 * normal state) we post an event and then wait for the process
 * that had the reference to wake us up again.  Then we blow away the
 * vnode and return (which will deallocate the softc).
 */
int
wsmouse_detach(device_t self, int flags)
{
	struct wsmouse_softc *sc = device_private(self);
	struct wseventvar *evar;
	int maj, mn;
	int s;

#if NWSMUX > 0
	/* Tell parent mux we're leaving. */
	if (sc->sc_base.me_parent != NULL) {
		DPRINTF(("wsmouse_detach:\n"));
		wsmux_detach_sc(&sc->sc_base);
	}
#endif

	/* If we're open ... */
	evar = sc->sc_base.me_evp;
	if (evar != NULL && evar->io != NULL) {
		s = spltty();
		if (--sc->sc_refcnt >= 0) {
			struct wscons_event event;

			/* Wake everyone by generating a dummy event. */
			event.type = 0;
			event.value = 0;
			if (wsevent_inject(evar, &event, 1) != 0)
				wsevent_wakeup(evar);

			/* Wait for processes to go away. */
			if (tsleep(sc, PZERO, "wsmdet", hz * 60))
				printf("wsmouse_detach: %s didn't detach\n",
				       device_xname(self));
		}
		splx(s);
	}

	/* locate the major number */
	maj = cdevsw_lookup_major(&wsmouse_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

void
wsmouse_input(device_t wsmousedev, u_int btns /* 0 is up */,
	int x, int y, int z, int w, u_int flags)
{
	struct wsmouse_softc *sc = device_private(wsmousedev);
	struct wseventvar *evar;
	int mb, ub, d, nevents;
	/* one for each dimension (4) + a bit for each button */
	struct wscons_event events[4 + sizeof(d) * 8];

        /*
         * Discard input if not open.
         */
	evar = sc->sc_base.me_evp;
	if (evar == NULL)
		return;

#ifdef DIAGNOSTIC
	if (evar->q == NULL) {
		printf("wsmouse_input: evar->q=NULL\n");
		return;
	}
#endif

#if NWSMUX > 0
	DPRINTFN(5,("wsmouse_input: %s mux=%p, evar=%p\n",
		    device_xname(sc->sc_base.me_dv),
		    sc->sc_base.me_parent, evar));
#endif

	sc->sc_mb = btns;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_X))
		sc->sc_dx += x;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Y))
		sc->sc_dy += y;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Z))
		sc->sc_dz += z;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_W))
		sc->sc_dw += w;

	/*
	 * We have at least one event (mouse button, delta-X, or
	 * delta-Y; possibly all three, and possibly three separate
	 * button events).  Deliver these events until we are out
	 * of changes or out of room.  As events get delivered,
	 * mark them `unchanged'.
	 */
	ub = sc->sc_ub;
	nevents = 0;

	if (flags & WSMOUSE_INPUT_ABSOLUTE_X) {
		if (sc->sc_x != x) {
			events[nevents].type = WSCONS_EVENT_MOUSE_ABSOLUTE_X;
			events[nevents].value = x;
			nevents++;
		}
	} else {
		if (sc->sc_dx) {
			events[nevents].type = WSCONS_EVENT_MOUSE_DELTA_X;
			events[nevents].value = sc->sc_dx;
			nevents++;
		}
	}
	if (flags & WSMOUSE_INPUT_ABSOLUTE_Y) {
		if (sc->sc_y != y) {
			events[nevents].type = WSCONS_EVENT_MOUSE_ABSOLUTE_Y;
			events[nevents].value = y;
			nevents++;
		}
	} else {
		if (sc->sc_dy) {
			events[nevents].type = WSCONS_EVENT_MOUSE_DELTA_Y;
			events[nevents].value = sc->sc_dy;
			nevents++;
		}
	}
	if (flags & WSMOUSE_INPUT_ABSOLUTE_Z) {
		if (sc->sc_z != z) {
			events[nevents].type = WSCONS_EVENT_MOUSE_ABSOLUTE_Z;
			events[nevents].value = z;
			nevents++;
		}
	} else {
		if (sc->sc_dz) {
			events[nevents].type = WSCONS_EVENT_MOUSE_DELTA_Z;
			events[nevents].value = sc->sc_dz;
			nevents++;
		}
	}
	if (flags & WSMOUSE_INPUT_ABSOLUTE_W) {
		if (sc->sc_w != w) {
			events[nevents].type = WSCONS_EVENT_MOUSE_ABSOLUTE_W;
			events[nevents].value = w;
			nevents++;
		}
	} else {
		if (sc->sc_dw) {
			events[nevents].type = WSCONS_EVENT_MOUSE_DELTA_W;
			events[nevents].value = sc->sc_dw;
			nevents++;
		}
	}

	mb = sc->sc_mb;
	while ((d = mb ^ ub) != 0) {
		int btnno;

		/*
		 * Cancel button repeating if button status changed.
		 */
		if (sc->sc_repeat_button != -1) {
			KASSERT(sc->sc_repeat_button >= 0);
			KASSERT(sc->sc_repeat.wr_buttons &
                            (1 << sc->sc_repeat_button));
			ub &= ~(1 << sc->sc_repeat_button);
			sc->sc_repeat_button = -1;
			callout_stop(&sc->sc_repeat_callout);
		}

		/*
		 * Mouse button change.  Find the first change and drop
		 * it into the event queue.
		 */
		btnno = ffs(d) - 1;
		KASSERT(btnno >= 0);

		if (nevents >= sizeof(events) / sizeof(events[0])) {
			aprint_error_dev(sc->sc_base.me_dv,
			    "Event queue full (button status mb=0x%x"
			    " ub=0x%x)\n", mb, ub);
			break;
		}

		events[nevents].type =
		    (mb & d) ? WSCONS_EVENT_MOUSE_DOWN : WSCONS_EVENT_MOUSE_UP;
		events[nevents].value = btnno;
		nevents++;

		ub ^= (1 << btnno);

		/*
		 * Program button repeating if configured for this button.
		 */
		if ((mb & d) && (sc->sc_repeat.wr_buttons & (1 << btnno)) &&
		    sc->sc_repeat.wr_delay_first > 0) {
			sc->sc_repeat_button = btnno;
			sc->sc_repeat_delay = sc->sc_repeat.wr_delay_first;
			callout_schedule(&sc->sc_repeat_callout,
			    mstohz(sc->sc_repeat_delay));
		}
	}

	if (nevents == 0 || wsevent_inject(evar, events, nevents) == 0) {
		/* All events were correctly injected into the queue.
		 * Synchronize the mouse's status with what the user
		 * has received. */
		sc->sc_x = x; sc->sc_dx = 0;
		sc->sc_y = y; sc->sc_dy = 0;
		sc->sc_z = z; sc->sc_dz = 0;
		sc->sc_w = w; sc->sc_dw = 0;
		sc->sc_ub = ub;
#if NWSMUX > 0
		DPRINTFN(5,("wsmouse_input: %s wakeup evar=%p\n",
			    device_xname(sc->sc_base.me_dv), evar));
#endif
	}
}

static void
wsmouse_repeat(void *v)
{
	int oldspl;
	unsigned int newdelay;
	struct wsmouse_softc *sc;
	struct wscons_event events[2];

	oldspl = spltty();
	sc = (struct wsmouse_softc *)v;

	if (sc->sc_repeat_button == -1) {
		/* Race condition: a "button up" event came in when
		 * this function was already called but did not do
		 * spltty() yet. */
		splx(oldspl);
		return;
	}
	KASSERT(sc->sc_repeat_button >= 0);

	KASSERT(sc->sc_repeat.wr_buttons & (1 << sc->sc_repeat_button));

	newdelay = sc->sc_repeat_delay;

	events[0].type = WSCONS_EVENT_MOUSE_UP;
	events[0].value = sc->sc_repeat_button;
	events[1].type = WSCONS_EVENT_MOUSE_DOWN;
	events[1].value = sc->sc_repeat_button;

	if (wsevent_inject(sc->sc_base.me_evp, events, 2) == 0) {
		sc->sc_ub = 1 << sc->sc_repeat_button;

		if (newdelay - sc->sc_repeat.wr_delay_decrement <
		    sc->sc_repeat.wr_delay_minimum)
			newdelay = sc->sc_repeat.wr_delay_minimum;
		else if (newdelay > sc->sc_repeat.wr_delay_minimum)
			newdelay -= sc->sc_repeat.wr_delay_decrement;
		KASSERT(newdelay >= sc->sc_repeat.wr_delay_minimum &&
		    newdelay <= sc->sc_repeat.wr_delay_first);
	}

	/*
	 * Reprogram the repeating event.
	 */
	sc->sc_repeat_delay = newdelay;
	callout_schedule(&sc->sc_repeat_callout, mstohz(newdelay));

	splx(oldspl);
}

int
wsmouseopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct wsmouse_softc *sc;
	struct wseventvar *evar;
	int error;

	sc = device_lookup_private(&wsmouse_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;

#if NWSMUX > 0
	DPRINTF(("wsmouseopen: %s mux=%p p=%p\n", device_xname(sc->sc_base.me_dv),
		 sc->sc_base.me_parent, l));
#endif

	if (sc->sc_dying)
		return (EIO);

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		return (0);			/* always allow open for write
						   so ioctl() is possible. */

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	evar = &sc->sc_base.me_evar;
	wsevent_init(evar, l->l_proc);
	sc->sc_base.me_evp = evar;

	error = wsmousedoopen(sc, evar);
	if (error) {
		DPRINTF(("wsmouseopen: %s open failed\n",
			 device_xname(sc->sc_base.me_dv)));
		sc->sc_base.me_evp = NULL;
		wsevent_fini(evar);
	}
	return (error);
}

int
wsmouseclose(dev_t dev, int flags, int mode,
    struct lwp *l)
{
	struct wsmouse_softc *sc =
	    device_lookup_private(&wsmouse_cd, minor(dev));
	struct wseventvar *evar = sc->sc_base.me_evp;

	if (evar == NULL)
		/* not open for read */
		return (0);
	sc->sc_base.me_evp = NULL;
	(*sc->sc_accessops->disable)(sc->sc_accesscookie);
	wsevent_fini(evar);

	return (0);
}

int
wsmousedoopen(struct wsmouse_softc *sc, struct wseventvar *evp)
{
	sc->sc_base.me_evp = evp;
	sc->sc_x = INVALID_X;
	sc->sc_y = INVALID_Y;
	sc->sc_z = INVALID_Z;
	sc->sc_w = INVALID_W;

	/* Stop button repeating when messing with the device. */
	if (sc->sc_repeat_button != -1) {
		KASSERT(sc->sc_repeat_button >= 0);
		sc->sc_repeat_button = -1;
		callout_stop(&sc->sc_repeat_callout);
	}

	/* enable the device, and punt if that's not possible */
	return (*sc->sc_accessops->enable)(sc->sc_accesscookie);
}

int
wsmouseread(dev_t dev, struct uio *uio, int flags)
{
	struct wsmouse_softc *sc =
	    device_lookup_private(&wsmouse_cd, minor(dev));
	int error;

	if (sc->sc_dying)
		return (EIO);

#ifdef DIAGNOSTIC
	if (sc->sc_base.me_evp == NULL) {
		printf("wsmouseread: evp == NULL\n");
		return (EINVAL);
	}
#endif

	sc->sc_refcnt++;
	error = wsevent_read(sc->sc_base.me_evp, uio, flags);
	if (--sc->sc_refcnt < 0) {
		wakeup(sc);
		error = EIO;
	}
	return (error);
}

int
wsmouseioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	return (wsmousedoioctl(device_lookup(&wsmouse_cd, minor(dev)),
			       cmd, data, flag, l));
}

/* A wrapper around the ioctl() workhorse to make reference counting easy. */
int
wsmousedoioctl(device_t dv, u_long cmd, void *data, int flag,
	       struct lwp *l)
{
	struct wsmouse_softc *sc = device_private(dv);
	int error;

	sc->sc_refcnt++;
	error = wsmouse_do_ioctl(sc, cmd, data, flag, l);
	if (--sc->sc_refcnt < 0)
		wakeup(sc);
	return (error);
}

int
wsmouse_do_ioctl(struct wsmouse_softc *sc, u_long cmd, void *data,
		 int flag, struct lwp *l)
{
	int error;
	struct wsmouse_repeat *wr;

	if (sc->sc_dying)
		return (EIO);

	/*
	 * Try the generic ioctls that the wsmouse interface supports.
	 */
	switch (cmd) {
	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		sc->sc_base.me_evp->async = *(int *)data != 0;
		return (0);

	case FIOSETOWN:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		if (-*(int *)data != sc->sc_base.me_evp->io->p_pgid
		    && *(int *)data != sc->sc_base.me_evp->io->p_pid)
			return (EPERM);
		return (0);

	case TIOCSPGRP:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		if (*(int *)data != sc->sc_base.me_evp->io->p_pgid)
			return (EPERM);
		return (0);
	}

	/*
	 * Try the wsmouse specific ioctls.
	 */
	switch (cmd) {
	case WSMOUSEIO_GETREPEAT:
		wr = (struct wsmouse_repeat *)data;
		memcpy(wr, &sc->sc_repeat, sizeof(sc->sc_repeat));
		return 0;

	case WSMOUSEIO_SETREPEAT:
		if ((flag & FWRITE) == 0)
			return EACCES;

		/* Validate input data. */
		wr = (struct wsmouse_repeat *)data;
		if (wr->wr_delay_first != 0 &&
		    (wr->wr_delay_first < wr->wr_delay_decrement ||
		     wr->wr_delay_first < wr->wr_delay_minimum ||
		     wr->wr_delay_first < wr->wr_delay_minimum +
		     wr->wr_delay_decrement))
			return EINVAL;

		/* Stop current repeating and set new data. */
		sc->sc_repeat_button = -1;
		callout_stop(&sc->sc_repeat_callout);
		memcpy(&sc->sc_repeat, wr, sizeof(sc->sc_repeat));

		return 0;

	case WSMOUSEIO_SETVERSION:
		return wsevent_setversion(sc->sc_base.me_evp, *(int *)data);
	}

	/*
	 * Try the mouse driver for WSMOUSEIO ioctls.  It returns -1
	 * if it didn't recognize the request.
	 */
	error = (*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd,
	    data, flag, l);
	return (error); /* may be EPASSTHROUGH */
}

int
wsmousepoll(dev_t dev, int events, struct lwp *l)
{
	struct wsmouse_softc *sc =
	    device_lookup_private(&wsmouse_cd, minor(dev));

	if (sc->sc_base.me_evp == NULL)
		return (POLLERR);
	return (wsevent_poll(sc->sc_base.me_evp, events, l));
}

int
wsmousekqfilter(dev_t dev, struct knote *kn)
{
	struct wsmouse_softc *sc =
	    device_lookup_private(&wsmouse_cd, minor(dev));

	if (sc->sc_base.me_evp == NULL)
		return (1);
	return (wsevent_kqfilter(sc->sc_base.me_evp, kn));
}

#if NWSMUX > 0
int
wsmouse_mux_open(struct wsevsrc *me, struct wseventvar *evp)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)me;

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return wsmousedoopen(sc, evp);
}

int
wsmouse_mux_close(struct wsevsrc *me)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)me;

	sc->sc_base.me_evp = NULL;
	(*sc->sc_accessops->disable)(sc->sc_accesscookie);

	return (0);
}

int
wsmouse_add_mux(int unit, struct wsmux_softc *muxsc)
{
	struct wsmouse_softc *sc;

	sc = device_lookup_private(&wsmouse_cd, unit);
	if (sc == NULL)
		return ENXIO;

	if (sc->sc_base.me_parent != NULL || sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return (wsmux_attach_sc(muxsc, &sc->sc_base));
}
#endif /* NWSMUX > 0 */
