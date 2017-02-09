/*	$NetBSD: irframe_tty.c,v 1.61 2015/08/20 14:40:18 christos Exp $	*/

/*
 * TODO
 *  Test dongle code.
 */

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and Tommy Bohlin
 * (tommy@gatespace.com).
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
 * Loosely based on ppp_tty.c.
 * Framing and dongle handling written by Tommy Bohlin.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irframe_tty.c,v 1.61 2015/08/20 14:40:18 christos Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/kauth.h>

#include <dev/ir/ir.h>
#include <dev/ir/sir.h>
#include <dev/ir/irdaio.h>
#include <dev/ir/irframevar.h>

#include "ioconf.h"

#ifdef IRFRAMET_DEBUG
#define DPRINTF(x)	if (irframetdebug) printf x
int irframetdebug = 0;
#else
#define DPRINTF(x)
#endif

/*****/

/* Max size with framing. */
#define MAX_IRDA_FRAME (2*IRDA_MAX_FRAME_SIZE + IRDA_MAX_EBOFS + 4)

struct irt_frame {
	u_char *buf;
	u_int len;
};
#define MAXFRAMES 8

struct irframet_softc {
	struct irframe_softc sc_irp;
	struct tty *sc_tp;

	int sc_dongle;
	int sc_dongle_private;

	int sc_state;
#define	IRT_RSLP		0x01	/* waiting for data (read) */
#if 0
#define	IRT_WSLP		0x02	/* waiting for data (write) */
#define IRT_CLOSING		0x04	/* waiting for output to drain */
#endif
	kmutex_t sc_wr_lk;

	struct irda_params sc_params;

	u_char* sc_inbuf;
	int sc_framestate;
#define FRAME_OUTSIDE    0
#define FRAME_INSIDE     1
#define FRAME_ESCAPE     2
	int sc_inchars;
	int sc_inFCS;
	struct callout sc_timeout;

	u_int sc_nframes;
	u_int sc_framei;
	u_int sc_frameo;
	struct irt_frame sc_frames[MAXFRAMES];
	u_int8_t sc_buffer[MAX_IRDA_FRAME];
	struct selinfo sc_rsel;
	/* XXXJRT Nothing selnotify's sc_wsel */
	struct selinfo sc_wsel;
};

/* line discipline methods */
int	irframetopen(dev_t, struct tty *);
int	irframetclose(struct tty *, int);
int	irframetioctl(struct tty *, u_long, void *, int, struct lwp *);
int	irframetinput(int, struct tty *);
int	irframetstart(struct tty *);


/* irframe methods */
static int	irframet_open(void *, int, int, struct lwp *);
static int	irframet_close(void *, int, int, struct lwp *);
static int	irframet_read(void *, struct uio *, int);
static int	irframet_write(void *, struct uio *, int);
static int	irframet_poll(void *, int, struct lwp *);
static int	irframet_kqfilter(void *, struct knote *);

static int	irframet_set_params(void *, struct irda_params *);
static int	irframet_get_speeds(void *, int *);
static int	irframet_get_turnarounds(void *, int *);

/* internal */
static int	irt_write_frame(struct tty *, u_int8_t *, size_t);
static int	irt_putc(struct tty *, int);
static void	irt_frame(struct irframet_softc *, u_char *, u_int);
static void	irt_timeout(void *);
static void	irt_ioctl(struct tty *, u_long, void *);
static void	irt_setspeed(struct tty *, u_int);
static void	irt_setline(struct tty *, u_int);
static void	irt_delay(struct tty *, u_int);
static void	irt_buffer(struct irframet_softc *, u_int);

static const struct irframe_methods irframet_methods = {
	irframet_open, irframet_close, irframet_read, irframet_write,
	irframet_poll, irframet_kqfilter, irframet_set_params,
	irframet_get_speeds, irframet_get_turnarounds
};

static void irts_none(struct tty *, u_int);
static void irts_tekram(struct tty *, u_int);
static void irts_jeteye(struct tty *, u_int);
static void irts_actisys(struct tty *, u_int);
static void irts_litelink(struct tty *, u_int);
static void irts_girbil(struct tty *, u_int);

#define NORMAL_SPEEDS (IRDA_SPEEDS_SIR & ~IRDA_SPEED_2400)
#define TURNT_POS (IRDA_TURNT_10000 | IRDA_TURNT_5000 | IRDA_TURNT_1000 | \
	IRDA_TURNT_500 | IRDA_TURNT_100 | IRDA_TURNT_50 | IRDA_TURNT_10)
static const struct dongle {
	void (*setspeed)(struct tty *, u_int);
	u_int speedmask;
	u_int turnmask;
} irt_dongles[DONGLE_MAX] = {
	/* Indexed by dongle number from irdaio.h */
	{ irts_none, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 },
	{ irts_tekram, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 },
	{ irts_jeteye, IRDA_SPEED_9600|IRDA_SPEED_19200|IRDA_SPEED_115200,
	  				IRDA_TURNT_10000 },
	{ irts_actisys, NORMAL_SPEEDS & ~IRDA_SPEED_38400, TURNT_POS },
	{ irts_actisys, NORMAL_SPEEDS, TURNT_POS },
	{ irts_litelink, NORMAL_SPEEDS, TURNT_POS },
	{ irts_girbil, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 | IRDA_TURNT_5000 },
};

static struct linesw irframet_disc = {
	.l_name = "irframe",
	.l_open = irframetopen,
	.l_close = irframetclose,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = irframetioctl,
	.l_rint = irframetinput,
	.l_start = irframetstart,
	.l_modem = ttymodem,
	.l_poll = ttyerrpoll
};

/* glue to attach irframe device */
static void irframet_attach(device_t, device_t, void *);
static int irframet_detach(device_t, int);

CFATTACH_DECL_NEW(irframet, sizeof(struct irframet_softc),
	NULL, irframet_attach, irframet_detach, NULL);

void
irframettyattach(int n)
{
	extern struct cfdriver irframe_cd;

	(void) ttyldisc_attach(&irframet_disc);

	/* XXX might fail if "real" attachments have pulled this in */
	/* XXX should not be done here */
	config_cfdriver_attach(&irframe_cd);

	config_cfattach_attach("irframe", &irframet_ca);
}

static void
irframet_attach(device_t parent, device_t self, void *aux)
{
	struct irframet_softc *sc = device_private(self);

	/* pseudo-device attachment does not print name */
	aprint_normal("%s", device_xname(self));

	callout_init(&sc->sc_timeout, 0);
	mutex_init(&sc->sc_wr_lk, MUTEX_DEFAULT, IPL_NONE);
	selinit(&sc->sc_rsel);
	selinit(&sc->sc_wsel);
	
#if 0 /* XXX can't do it yet because pseudo-devices don't get aux */
	struct ir_attach_args ia;

	ia.ia_methods = &irframet_methods;
	ia.ia_handle = aux->xxx;

	irframe_attach(parent, self, &ia);
#endif
}

static int
irframet_detach(device_t dev, int flags)
{
	struct irframet_softc *sc = device_private(dev);
	int rc;

	callout_halt(&sc->sc_timeout, NULL);

	rc = irframe_detach(dev, flags);

	callout_destroy(&sc->sc_timeout);
	mutex_destroy(&sc->sc_wr_lk);
	seldestroy(&sc->sc_wsel);
	seldestroy(&sc->sc_rsel);

	return rc;
}

/*
 * Line specific open routine for async tty devices.
 * Attach the given tty to the first available irframe unit.
 * Called from device open routine or ttioctl.
 */
/* ARGSUSED */
int
irframetopen(dev_t dev, struct tty *tp)
{
	struct lwp *l = curlwp;		/* XXX */
	struct irframet_softc *sc;
	int error, s;
	cfdata_t cfdata;
	struct ir_attach_args ia;
	device_t d;

	DPRINTF(("%s\n", __func__));

	if ((error = kauth_authorize_device_tty(l->l_cred, 
		KAUTH_DEVICE_TTY_OPEN, tp)))
		return (error);

	s = spltty();

	DPRINTF(("%s: linesw=%p disc=%s\n", __func__, tp->t_linesw,
		 tp->t_linesw->l_name));
	if (tp->t_linesw == &irframet_disc) {
		sc = (struct irframet_softc *)tp->t_sc;
		DPRINTF(("%s: sc=%p sc_tp=%p\n", __func__, sc, sc->sc_tp));
		if (sc != NULL) {
			splx(s);
			return (EBUSY);
		}
	}

	cfdata = malloc(sizeof(struct cfdata), M_DEVBUF, M_WAITOK);
	cfdata->cf_name = "irframe";
	cfdata->cf_atname = "irframet";
	cfdata->cf_fstate = FSTATE_STAR;
	cfdata->cf_unit = 0;
	d = config_attach_pseudo(cfdata);
	sc = device_private(d);
	sc->sc_irp.sc_dev = d;

	/* XXX should be done in irframet_attach() */
	ia.ia_methods = &irframet_methods;
	ia.ia_handle = tp;
	irframe_attach(0, d, &ia);

	tp->t_sc = sc;
	sc->sc_tp = tp;
	aprint_normal("%s attached at tty%02d\n", device_xname(d),
	    (int)minor(tp->t_dev));

	DPRINTF(("%s: set sc=%p\n", __func__, sc));

	mutex_spin_enter(&tty_lock);
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);

	sc->sc_dongle = DONGLE_NONE;
	sc->sc_dongle_private = 0;

	splx(s);

	return (0);
}

/*
 * Line specific close routine, called from device close routine
 * and from ttioctl.
 * Detach the tty from the irframe unit.
 * Mimics part of ttyclose().
 */
int
irframetclose(struct tty *tp, int flag)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int s;
	cfdata_t cfdata;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	s = spltty();
	mutex_spin_enter(&tty_lock);
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);	 /* XXX */
	ttyldisc_release(tp->t_linesw);
	tp->t_linesw = ttyldisc_default(); if (sc != NULL) {
		irt_buffer(sc, 0);
		tp->t_sc = NULL;
		aprint_normal("%s detached from tty%02d\n",
		    device_xname(sc->sc_irp.sc_dev), (int)minor(tp->t_dev));

		if (sc->sc_tp == tp) {
			cfdata = device_cfdata(sc->sc_irp.sc_dev);
			config_detach(sc->sc_irp.sc_dev, 0);
			free(cfdata, M_DEVBUF);
		}
	}
	splx(s);
	return (0);
}

/*
 * Line specific (tty) ioctl routine.
 * This discipline requires that tty device drivers call
 * the line specific l_ioctl routine from their ioctl routines.
 */
/* ARGSUSED */
int
irframetioctl(struct tty *tp, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error;
	int d;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	if (sc == NULL || tp != sc->sc_tp)
		return (EPASSTHROUGH);

	error = 0;
	switch (cmd) {
	case IRFRAMETTY_GET_DEVICE:
		*(int *)data = device_unit(sc->sc_irp.sc_dev);
		break;
	case IRFRAMETTY_GET_DONGLE:
		*(int *)data = sc->sc_dongle;
		break;
	case IRFRAMETTY_SET_DONGLE:
		d = *(int *)data;
		if (d < 0 || d >= DONGLE_MAX)
			return (EINVAL);
		sc->sc_dongle = d;
		break;
	default:
		error = EPASSTHROUGH;
		break;
	}

	return (error);
}

/*
 * Start output on async tty interface.
 */
int
irframetstart(struct tty *tp)
{
	/*struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;*/
	int s;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	s = spltty();
	if (tp->t_oproc != NULL)
		(*tp->t_oproc)(tp);
	splx(s);

	return (0);
}

static void
irt_buffer(struct irframet_softc *sc, u_int maxsize)
{
	int i;

	DPRINTF(("%s: sc=%p, maxsize=%u\n", __func__, sc, maxsize));

	if (sc->sc_params.maxsize != maxsize) {
		sc->sc_params.maxsize = maxsize;
		if (sc->sc_inbuf != NULL)
			free(sc->sc_inbuf, M_DEVBUF);
		for (i = 0; i < MAXFRAMES; i++)
			if (sc->sc_frames[i].buf != NULL)
				free(sc->sc_frames[i].buf, M_DEVBUF);
		if (sc->sc_params.maxsize != 0) {
			sc->sc_inbuf = malloc(sc->sc_params.maxsize+2,
					      M_DEVBUF, M_WAITOK);
			for (i = 0; i < MAXFRAMES; i++)
				sc->sc_frames[i].buf =
					malloc(sc->sc_params.maxsize,
					       M_DEVBUF, M_WAITOK);
		} else {
			sc->sc_inbuf = NULL;
			for (i = 0; i < MAXFRAMES; i++)
				sc->sc_frames[i].buf = NULL;
		}
	}
}

void
irt_frame(struct irframet_softc *sc, u_char *tbuf, u_int len)
{
	DPRINTF(("%s: nframe=%d framei=%d frameo=%d\n",
		 __func__, sc->sc_nframes, sc->sc_framei, sc->sc_frameo));

	if (sc->sc_inbuf == NULL) /* XXX happens if device is closed? */
		return;
	if (sc->sc_nframes >= MAXFRAMES) {
#ifdef IRFRAMET_DEBUG
		printf("%s: dropped frame\n", __func__);
#endif
		return;
	}
	if (sc->sc_frames[sc->sc_framei].buf == NULL)
		return;
	memcpy(sc->sc_frames[sc->sc_framei].buf, tbuf, len);
	sc->sc_frames[sc->sc_framei].len = len;
	sc->sc_framei = (sc->sc_framei+1) % MAXFRAMES;
	sc->sc_nframes++;
	if (sc->sc_state & IRT_RSLP) {
		sc->sc_state &= ~IRT_RSLP;
		DPRINTF(("%s: waking up reader\n", __func__));
		wakeup(sc->sc_frames);
	}
	selnotify(&sc->sc_rsel, 0, 0);
}

void
irt_timeout(void *v)
{
	struct irframet_softc *sc = v;

#ifdef IRFRAMET_DEBUG
	if (sc->sc_framestate != FRAME_OUTSIDE)
		printf("%s: input frame timeout\n", __func__);
#endif
	sc->sc_framestate = FRAME_OUTSIDE;
}

int
irframetinput(int c, struct tty *tp)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	c &= 0xff;

#if IRFRAMET_DEBUG
	if (irframetdebug > 1)
		DPRINTF(("%s: tp=%p c=0x%02x\n", __func__, tp, c));
#endif

	if (sc == NULL || tp != (struct tty *)sc->sc_tp)
		return (0);

	if (sc->sc_inbuf == NULL)
		return (0);

	switch (c) {
	case SIR_BOF:
		DPRINTF(("%s: BOF\n", __func__));
		sc->sc_framestate = FRAME_INSIDE;
		sc->sc_inchars = 0;
		sc->sc_inFCS = INITFCS;
		break;
	case SIR_EOF:
		DPRINTF(("%s: EOF state=%d inchars=%d fcs=0x%04x\n",
			 __func__,
			 sc->sc_framestate, sc->sc_inchars, sc->sc_inFCS));
		if (sc->sc_framestate == FRAME_INSIDE &&
		    sc->sc_inchars >= 4 && sc->sc_inFCS == GOODFCS) {
			irt_frame(sc, sc->sc_inbuf, sc->sc_inchars - 2);
		} else if (sc->sc_framestate != FRAME_OUTSIDE) {
#ifdef IRFRAMET_DEBUG
			printf("%s: malformed input frame\n", __func__);
#endif
		}
		sc->sc_framestate = FRAME_OUTSIDE;
		break;
	case SIR_CE:
		DPRINTF(("%s: CE\n", __func__));
		if (sc->sc_framestate == FRAME_INSIDE)
			sc->sc_framestate = FRAME_ESCAPE;
		break;
	default:
#if IRFRAMET_DEBUG
	if (irframetdebug > 1)
		DPRINTF(("%s: c=0x%02x, inchar=%d state=%d\n", __func__, c,
			 sc->sc_inchars, sc->sc_state));
#endif
		if (sc->sc_framestate != FRAME_OUTSIDE) {
			if (sc->sc_framestate == FRAME_ESCAPE) {
				sc->sc_framestate = FRAME_INSIDE;
				c ^= SIR_ESC_BIT;
			}
			if (sc->sc_inchars < sc->sc_params.maxsize + 2) {
				sc->sc_inbuf[sc->sc_inchars++] = c;
				sc->sc_inFCS = updateFCS(sc->sc_inFCS, c);
			} else {
				sc->sc_framestate = FRAME_OUTSIDE;
#ifdef IRFRAMET_DEBUG
				printf("%s: input frame overrun\n",
				       __func__);
#endif
			}
		}
		break;
	}

#if 1
	if (sc->sc_framestate != FRAME_OUTSIDE) {
		callout_reset(&sc->sc_timeout, hz/20, irt_timeout, sc);
	}
#endif

	return (0);
}


/*** irframe methods ***/

int
irframet_open(void *h, int flag, int mode,
    struct lwp *l)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	sc->sc_params.speed = 0;
	sc->sc_params.ebofs = IRDA_DEFAULT_EBOFS;
	sc->sc_params.maxsize = 0;
	sc->sc_framestate = FRAME_OUTSIDE;
	sc->sc_nframes = 0;
	sc->sc_framei = 0;
	sc->sc_frameo = 0;

	return (0);
}

int
irframet_close(void *h, int flag, int mode,
    struct lwp *l)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int s;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	/* line discipline was closed */
	if (sc == NULL)
		return (0);

	callout_stop(&sc->sc_timeout);
	s = splir();
	irt_buffer(sc, 0);
	splx(s);

	return (0);
}

int
irframet_read(void *h, struct uio *uio, int flag)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error = 0;
	int s;

	DPRINTF(("%s: resid=%zd, iovcnt=%d, offset=%ld\n",
		 __func__, uio->uio_resid, uio->uio_iovcnt,
		 (long)uio->uio_offset));
	DPRINTF(("%s: nframe=%d framei=%d frameo=%d\n",
		 __func__, sc->sc_nframes, sc->sc_framei, sc->sc_frameo));


	s = splir();
	while (sc->sc_nframes == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_state |= IRT_RSLP;
		DPRINTF(("%s: sleep\n", __func__));
		error = tsleep(sc->sc_frames, PZERO | PCATCH, "irtrd", 0);
		DPRINTF(("%s: woke, error=%d\n", __func__, error));
		if (error) {
			sc->sc_state &= ~IRT_RSLP;
			break;
		}
	}

	/* Do just one frame transfer per read */
	if (!error) {
		if (uio->uio_resid < sc->sc_frames[sc->sc_frameo].len) {
			DPRINTF(("%s: uio buffer smaller than frame size "
				 "(%zd < %d)\n", __func__, uio->uio_resid,
				 sc->sc_frames[sc->sc_frameo].len));
			error = EINVAL;
		} else {
			DPRINTF(("%s: moving %d bytes\n", __func__,
				 sc->sc_frames[sc->sc_frameo].len));
			error = uiomove(sc->sc_frames[sc->sc_frameo].buf,
					sc->sc_frames[sc->sc_frameo].len, uio);
			DPRINTF(("%s: error=%d\n", __func__, error));
		}
		sc->sc_frameo = (sc->sc_frameo+1) % MAXFRAMES;
		sc->sc_nframes--;
	}
	splx(s);

	return (error);
}

int
irt_putc(struct tty *tp, int c)
{
	int error;

#if IRFRAMET_DEBUG
	if (irframetdebug > 3)
		DPRINTF(("%s: tp=%p c=0x%02x cc=%d\n", __func__, tp, c,
			 tp->t_outq.c_cc));
#endif
	if (tp->t_outq.c_cc > tp->t_hiwat) {
		irframetstart(tp);
		mutex_spin_enter(&tty_lock);
		/*
		 * This can only occur if FLUSHO is set in t_lflag,
		 * or if ttstart/oproc is synchronous (or very fast).
		 */
		if (tp->t_outq.c_cc <= tp->t_hiwat) {
			mutex_spin_exit(&tty_lock);
			goto go;
		}
		error = ttysleep(tp, &tp->t_outcv, true, 0);
		mutex_spin_exit(&tty_lock);
		if (error)
			return (error);
	}
 go:
	if (putc(c, &tp->t_outq) < 0) {
		printf("irframe: putc failed\n");
		return (EIO);
	}
	return (0);
}

int
irframet_write(void *h, struct uio *uio, int flag)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int n;

	DPRINTF(("%s: resid=%zd, iovcnt=%d, offset=%ld\n",
		 __func__, uio->uio_resid, uio->uio_iovcnt,
		 (long)uio->uio_offset));

	n = irda_sir_frame(sc->sc_buffer, sizeof(sc->sc_buffer), uio,
	    sc->sc_params.ebofs);
	if (n < 0) {
#ifdef IRFRAMET_DEBUG
		printf("%s: irda_sir_frame() error=%d\n", __func__, -n);
#endif
		return (-n);
	}
	return (irt_write_frame(tp, sc->sc_buffer, n));
}

int
irt_write_frame(struct tty *tp, u_int8_t *tbuf, size_t len)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error, i;

	DPRINTF(("%s: tp=%p len=%zd\n", __func__, tp, len));

	mutex_enter(&sc->sc_wr_lk);
	error = 0;
	for (i = 0; !error && i < len; i++)
		error = irt_putc(tp, tbuf[i]);
	mutex_exit(&sc->sc_wr_lk);

	irframetstart(tp);

	DPRINTF(("%s: done, error=%d\n", __func__, error));

	return (error);
}

int
irframet_poll(void *h, int events, struct lwp *l)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int revents = 0;
	int s;

	DPRINTF(("%s: sc=%p\n", __func__, sc));

	s = splir();
	/* XXX is this a good check? */
	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_outq.c_cc <= tp->t_lowat)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_nframes > 0) {
			DPRINTF(("%s: have data\n", __func__));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			DPRINTF(("%s: recording select\n", __func__));
			selrecord(l, &sc->sc_rsel);
		}
	}
	splx(s);

	return (revents);
}

static void
filt_irframetrdetach(struct knote *kn)
{
	struct tty *tp = kn->kn_hook;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int s;

	s = splir();
	SLIST_REMOVE(&sc->sc_rsel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_irframetread(struct knote *kn, long hint)
{
	struct tty *tp = kn->kn_hook;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	kn->kn_data = sc->sc_nframes;
	return (kn->kn_data > 0);
}

static void
filt_irframetwdetach(struct knote *kn)
{
	struct tty *tp = kn->kn_hook;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int s;

	s = splir();
	SLIST_REMOVE(&sc->sc_wsel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_irframetwrite(struct knote *kn, long hint)
{
	struct tty *tp = kn->kn_hook;

	/* XXX double-check this */

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		kn->kn_data = tp->t_lowat - tp->t_outq.c_cc;
		return (1);
	}

	kn->kn_data = 0;
	return (0);
}

static const struct filterops irframetread_filtops =
	{ 1, NULL, filt_irframetrdetach, filt_irframetread };
static const struct filterops irframetwrite_filtops =
	{ 1, NULL, filt_irframetwdetach, filt_irframetwrite };

int
irframet_kqfilter(void *h, struct knote *kn)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rsel.sel_klist;
		kn->kn_fop = &irframetread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->sc_wsel.sel_klist;
		kn->kn_fop = &irframetwrite_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = tp;

	s = splir();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

int
irframet_set_params(void *h, struct irda_params *p)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	DPRINTF(("%s: tp=%p speed=%d ebofs=%d maxsize=%d\n",
		 __func__, tp, p->speed, p->ebofs, p->maxsize));

	if (p->speed != sc->sc_params.speed) {
		/* Checked in irframe.c */
		mutex_enter(&sc->sc_wr_lk);
		irt_dongles[sc->sc_dongle].setspeed(tp, p->speed);
		mutex_exit(&sc->sc_wr_lk);
		sc->sc_params.speed = p->speed;
	}

	/* Max size checked in irframe.c */
	sc->sc_params.ebofs = p->ebofs;
	irt_buffer(sc, p->maxsize);
	sc->sc_framestate = FRAME_OUTSIDE;

	return (0);
}

int
irframet_get_speeds(void *h, int *speeds)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	if (sc == NULL)		/* during attach */
		*speeds = IRDA_SPEEDS_SIR;
	else
		*speeds = irt_dongles[sc->sc_dongle].speedmask;
	return (0);
}

int
irframet_get_turnarounds(void *h, int *turnarounds)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	DPRINTF(("%s: tp=%p\n", __func__, tp));

	*turnarounds = irt_dongles[sc->sc_dongle].turnmask;
	return (0);
}

void
irt_ioctl(struct tty *tp, u_long cmd, void *arg)
{
	const struct cdevsw *cdev;
	int error __diagused;
	dev_t dev;

	dev = tp->t_dev;
	cdev = cdevsw_lookup(dev);
	if (cdev != NULL)
		error = (*cdev->d_ioctl)(dev, cmd, arg, 0, curlwp);
	else
		error = ENXIO;
#ifdef DIAGNOSTIC
	if (error)
		printf("irt_ioctl: cmd=0x%08lx error=%d\n", cmd, error);
#endif
}

void
irt_setspeed(struct tty *tp, u_int speed)
{
	struct termios tt;

	irt_ioctl(tp, TIOCGETA,  &tt);
	tt.c_ispeed = tt.c_ospeed = speed;
	tt.c_cflag &= ~HUPCL;
	tt.c_cflag |= CLOCAL;
	irt_ioctl(tp, TIOCSETAF, &tt);
}

void
irt_setline(struct tty *tp, u_int line)
{
	int mline;

	irt_ioctl(tp, TIOCMGET, &mline);
	mline &= ~(TIOCM_DTR | TIOCM_RTS);
	mline |= line;
	irt_ioctl(tp, TIOCMSET, (void *)&mline);
}

void
irt_delay(struct tty *tp, u_int ms)
{
	if (cold)
		delay(ms * 1000);
	else
		tsleep(&irt_delay, PZERO, "irtdly", ms * hz / 1000 + 1);

}

/**********************************************************************
 * No dongle
 **********************************************************************/
void
irts_none(struct tty *tp, u_int speed)
{
	irt_setspeed(tp, speed);
}

/**********************************************************************
 * Tekram
 **********************************************************************/
#define TEKRAM_PW     0x10

#define TEKRAM_115200 (TEKRAM_PW|0x00)
#define TEKRAM_57600  (TEKRAM_PW|0x01)
#define TEKRAM_38400  (TEKRAM_PW|0x02)
#define TEKRAM_19200  (TEKRAM_PW|0x03)
#define TEKRAM_9600   (TEKRAM_PW|0x04)
#define TEKRAM_2400   (TEKRAM_PW|0x08)

#define TEKRAM_TV     (TEKRAM_PW|0x05)

void
irts_tekram(struct tty *tp, u_int speed)
{
	int s;

	irt_setspeed(tp, 9600);
	irt_setline(tp, 0);
	irt_delay(tp, 50);

	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 1);

	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	irt_delay(tp, 1);	/* 50 us */

	irt_setline(tp, TIOCM_DTR);
	irt_delay(tp, 1);	/* 7 us */

	switch(speed) {
	case 115200: s = TEKRAM_115200; break;
	case 57600:  s = TEKRAM_57600; break;
	case 38400:  s = TEKRAM_38400; break;
	case 19200:  s = TEKRAM_19200; break;
	case 2400:   s = TEKRAM_2400; break;
	default:     s = TEKRAM_9600; break;
	}
	irt_putc(tp, s);
	irframetstart(tp);

	irt_delay(tp, 100);

	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	if (speed != 9600)
		irt_setspeed(tp, speed);
	irt_delay(tp, 1);	/* 50 us */
}

/**********************************************************************
 * Jeteye
 **********************************************************************/
void
irts_jeteye(struct tty *tp, u_int speed)
{
	switch (speed) {
	case 19200:
		irt_setline(tp, TIOCM_DTR);
		break;
	case 115200:
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		break;
	default: /*9600*/
		irt_setline(tp, TIOCM_RTS);
		break;
	}
	irt_setspeed(tp, speed);
}

/**********************************************************************
 * Actisys
 **********************************************************************/
void
irts_actisys(struct tty *tp, u_int speed)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int pulses;

	irt_setspeed(tp, speed);

	switch(speed) {
	case 19200:  pulses=1; break;
	case 57600:  pulses=2; break;
	case 115200: pulses=3; break;
	case 38400:  pulses=4; break;
	default: /* 9600 */ pulses=0; break;
	}

	if (sc->sc_dongle_private == 0) {
		sc->sc_dongle_private = 1;
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		/*
		 * Must wait at least 50ms after initial
		 * power on to charge internal capacitor
		 */
		irt_delay(tp, 50);
	}
	irt_setline(tp, TIOCM_RTS);
	delay(2);
	for (;;) {
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		delay(2);
		if (--pulses <= 0)
			break;
		irt_setline(tp, TIOCM_DTR);
		delay(2);
	}
}

/**********************************************************************
 * Litelink
 **********************************************************************/
void
irts_litelink(struct tty *tp, u_int speed)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int pulses;

	irt_setspeed(tp, speed);

	switch(speed) {
	case 57600:  pulses=1; break;
	case 38400:  pulses=2; break;
	case 19200:  pulses=3; break;
	case 9600:   pulses=4; break;
	default: /* 115200 */ pulses=0; break;
	}

	if (sc->sc_dongle_private == 0) {
		sc->sc_dongle_private = 1;
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	}
	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 1); /* 15 us */;
	for (;;) {
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		irt_delay(tp, 1); /* 15 us */;
		if (--pulses <= 0)
			break;
		irt_setline(tp, TIOCM_DTR);
		irt_delay(tp, 1); /* 15 us */;
	}
}

/**********************************************************************
 * Girbil
 **********************************************************************/
/* Control register 1 */
#define GIRBIL_TXEN      0x01 /* Enable transmitter */
#define GIRBIL_RXEN      0x02 /* Enable receiver */
#define GIRBIL_ECAN      0x04 /* Cancel self emmited data */
#define GIRBIL_ECHO      0x08 /* Echo control characters */

/* LED Current Register */
#define GIRBIL_HIGH      0x20
#define GIRBIL_MEDIUM    0x21
#define GIRBIL_LOW       0x22

/* Baud register */
#define GIRBIL_2400      0x30
#define GIRBIL_4800      0x31
#define GIRBIL_9600      0x32
#define GIRBIL_19200     0x33
#define GIRBIL_38400     0x34
#define GIRBIL_57600     0x35
#define GIRBIL_115200    0x36

/* Mode register */
#define GIRBIL_IRDA      0x40
#define GIRBIL_ASK       0x41

/* Control register 2 */
#define GIRBIL_LOAD      0x51 /* Load the new baud rate value */

void
irts_girbil(struct tty *tp, u_int speed)
{
	int s;

	irt_setspeed(tp, 9600);
	irt_setline(tp, TIOCM_DTR);
	irt_delay(tp, 5);
	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 20);
	switch(speed) {
	case 115200: s = GIRBIL_115200; break;
	case 57600:  s = GIRBIL_57600; break;
	case 38400:  s = GIRBIL_38400; break;
	case 19200:  s = GIRBIL_19200; break;
	case 4800:   s = GIRBIL_4800; break;
	case 2400:   s = GIRBIL_2400; break;
	default:     s = GIRBIL_9600; break;
	}
	irt_putc(tp, GIRBIL_TXEN|GIRBIL_RXEN);
	irt_putc(tp, s);
	irt_putc(tp, GIRBIL_LOAD);
	irframetstart(tp);
	irt_delay(tp, 100);
	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	if (speed != 9600)
		irt_setspeed(tp, speed);
}
