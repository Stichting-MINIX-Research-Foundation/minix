/*	$NetBSD: ms.c,v 1.40 2014/07/25 08:10:39 dholland Exp $	*/

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
 * Mouse driver (/dev/mouse)
 */

/*
 * Zilog Z8530 Dual UART driver (mouse interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for a Sun mouse.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ms.c,v 1.40 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/poll.h>

#include <machine/vuid_event.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>
#include <dev/sun/event_var.h>
#include <dev/sun/msvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include "ioconf.h"
#include "locators.h"
#include "wsmouse.h"

dev_type_open(msopen);
dev_type_close(msclose);
dev_type_read(msread);
dev_type_ioctl(msioctl);
dev_type_poll(mspoll);
dev_type_kqfilter(mskqfilter);

const struct cdevsw ms_cdevsw = {
	.d_open = msopen,
	.d_close = msclose,
	.d_read = msread,
	.d_write = nowrite,
	.d_ioctl = msioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = mspoll,
	.d_mmap = nommap,
	.d_kqfilter = mskqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/****************************************************************
 *  Entry points for /dev/mouse
 *  (open,close,read,write,...)
 ****************************************************************/

int
msopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));
	if (ms == NULL)
		return ENXIO;

	/* This is an exclusive open device. */
	if (ms->ms_events.ev_io)
		return EBUSY;

	if (ms->ms_deviopen) {
		int err;
		err = (*ms->ms_deviopen)(ms->ms_dev, flags);
		if (err)
			return err;
	}
	ms->ms_events.ev_io = l->l_proc;
	ev_init(&ms->ms_events);	/* may cause sleep */

	ms->ms_ready = 1;		/* start accepting events */
	return 0;
}

int
msclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));
	ms->ms_ready = 0;		/* stop accepting events */
	ev_fini(&ms->ms_events);

	ms->ms_events.ev_io = NULL;
	if (ms->ms_deviclose) {
		int err;
		err = (*ms->ms_deviclose)(ms->ms_dev, flags);
		if (err)
			return err;
	}
	return 0;
}

int
msread(dev_t dev, struct uio *uio, int flags)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));
	return ev_read(&ms->ms_events, uio, flags);
}

int
msioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));

	switch (cmd) {

	case FIONBIO:		/* we will remove this someday (soon???) */
		return 0;

	case FIOASYNC:
		ms->ms_events.ev_async = *(int *)data != 0;
		return 0;

	case FIOSETOWN:
		if (-*(int *)data != ms->ms_events.ev_io->p_pgid
		    && *(int *)data != ms->ms_events.ev_io->p_pid)
			return EPERM;
		return 0;

	case TIOCSPGRP:
		if (*(int *)data != ms->ms_events.ev_io->p_pgid)
			return EPERM;
		return 0;

	case VUIDGFORMAT:
		/* we only do firm_events */
		*(int *)data = VUID_FIRM_EVENT;
		return 0;

	case VUIDSFORMAT:
		if (*(int *)data != VUID_FIRM_EVENT)
			return EINVAL;
		return 0;
	}
	return ENOTTY;
}

int
mspoll(dev_t dev, int events, struct lwp *l)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));
	return ev_poll(&ms->ms_events, events, l);
}

int
mskqfilter(dev_t dev, struct knote *kn)
{
	struct ms_softc *ms;

	ms = device_lookup_private(&ms_cd, minor(dev));
	return ev_kqfilter(&ms->ms_events, kn);
}

/****************************************************************
 * Middle layer (translator)
 ****************************************************************/

/*
 * Called by our ms_softint() routine on input.
 */
void
ms_input(struct ms_softc *ms, int c)
{
	struct firm_event *fe;
	int mb, ub, d, get, put, any;
	static const char to_one[] = { 1, 2, 2, 4, 4, 4, 4 };
	static const int to_id[] = { MS_RIGHT, MS_MIDDLE, 0, MS_LEFT };

	/*
	 * Discard input if not ready.  Drop sync on parity or framing
	 * error; gain sync on button byte.
	 */
	if (ms->ms_ready == 0)
		return;
	if (c == -1) {
		ms->ms_byteno = -1;
		return;
	}
	if ((c & 0xb0) == 0x80) {	/* if in 0x80..0x8f of 0xc0..0xcf */
		if (c & 8) {
			ms->ms_byteno = 1;	/* short form (3 bytes) */
		} else {
			ms->ms_byteno = 0;	/* long form (5 bytes) */
		}
	}

	/*
	 * Run the decode loop, adding to the current information.
	 * We add, rather than replace, deltas, so that if the event queue
	 * fills, we accumulate data for when it opens up again.
	 */
	switch (ms->ms_byteno) {

	case -1:
		return;

	case 0:
		/* buttons (long form) */
		ms->ms_byteno = 2;
		ms->ms_mb = (~c) & 0x7;
		return;

	case 1:
		/* buttons (short form) */
		ms->ms_byteno = 4;
		ms->ms_mb = (~c) & 0x7;
		return;

	case 2:
		/* first delta-x */
		ms->ms_byteno = 3;
		ms->ms_dx += (char)c;
		return;

	case 3:
		/* first delta-y */
		ms->ms_byteno = 4;
		ms->ms_dy += (char)c;
		return;

	case 4:
		/* second delta-x */
		ms->ms_byteno = 5;
		ms->ms_dx += (char)c;
		return;

	case 5:
		/* second delta-y */
		ms->ms_byteno = -1;	/* wait for button-byte again */
		ms->ms_dy += (char)c;
		break;

	default:
		panic("ms_rint");
		/* NOTREACHED */
	}

#if NWSMOUSE > 0
	if (ms->ms_wsmousedev != NULL && ms->ms_ready == 2) {
		mb = ((ms->ms_mb & 4) >> 2) |
			(ms->ms_mb & 2) |
			((ms->ms_mb & 1) << 2);
		wsmouse_input(ms->ms_wsmousedev,
				mb,
				ms->ms_dx, ms->ms_dy, 0, 0,
				WSMOUSE_INPUT_DELTA);
		ms->ms_dx = 0;
		ms->ms_dy = 0;
		return;
	}
#endif
	/*
	 * We have at least one event (mouse button, delta-X, or
	 * delta-Y; possibly all three, and possibly three separate
	 * button events).  Deliver these events until we are out
	 * of changes or out of room.  As events get delivered,
	 * mark them `unchanged'.
	 */
	any = 0;
	get = ms->ms_events.ev_get;
	put = ms->ms_events.ev_put;
	fe = &ms->ms_events.ev_q[put];

	/* NEXT prepares to put the next event, backing off if necessary */
#define	NEXT \
	if ((++put) % EV_QSIZE == get) { \
		put--; \
		goto out; \
	}
	/* ADVANCE completes the `put' of the event */
#define	ADVANCE \
	fe++; \
	if (put >= EV_QSIZE) { \
		put = 0; \
		fe = &ms->ms_events.ev_q[0]; \
	} \
	any = 1

	mb = ms->ms_mb;
	ub = ms->ms_ub;
	while ((d = mb ^ ub) != 0) {
		/*
		 * Mouse button change.  Convert up to three changes
		 * to the `first' change, and drop it into the event queue.
		 */
		NEXT;
		d = to_one[d - 1];		/* from 1..7 to {1,2,4} */
		fe->id = to_id[d - 1];		/* from {1,2,4} to ID */
		fe->value = mb & d ? VKEY_DOWN : VKEY_UP;
		firm_gettime(fe);
		ADVANCE;
		ub ^= d;
	}
	if (ms->ms_dx) {
		NEXT;
		fe->id = LOC_X_DELTA;
		fe->value = ms->ms_dx;
		firm_gettime(fe);
		ADVANCE;
		ms->ms_dx = 0;
	}
	if (ms->ms_dy) {
		NEXT;
		fe->id = LOC_Y_DELTA;
		fe->value = ms->ms_dy;
		firm_gettime(fe);
		ADVANCE;
		ms->ms_dy = 0;
	}
out:
	if (any) {
		ms->ms_ub = ub;
		ms->ms_events.ev_put = put;
		EV_WAKEUP(&ms->ms_events);
	}
}
