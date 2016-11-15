/*	$NetBSD: lpt_mvme.c,v 1.18 2014/07/25 08:10:37 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * Copyright (c) 1993, 1994 Charles M. Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software is a component of "386BSD" developed by
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device Driver for an MVME68K/MVME88K board's parallel printer port
 * This driver attaches above the board-specific back-end.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt_mvme.c,v 1.18 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/syslog.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/mvme/lptvar.h>


#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#if !defined(DEBUG) || !defined(notdef)
#define LPRINTF(a)
#else
#define LPRINTF		if (lptdebug) aprint_verbose_dev a
int lptdebug = 1;
#endif

#define	LPTUNIT(s)	(minor(s) & 0x0f)
#define	LPTFLAGS(s)	(minor(s) & 0xf0)

static void lpt_wakeup(void *arg);
static int pushbytes(struct lpt_softc *);

extern struct cfdriver lpt_cd;

dev_type_open(lptopen);
dev_type_close(lptclose);
dev_type_write(lptwrite);
dev_type_ioctl(lptioctl);

const struct cdevsw lpt_cdevsw = {
	.d_open = lptopen,
	.d_close = lptclose,
	.d_read = noread,
	.d_write = lptwrite,
	.d_ioctl = lptioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = 0
};

void
lpt_attach_subr(struct lpt_softc *sc)
{

	sc->sc_state = 0;
	callout_init(&sc->sc_wakeup_ch, 0);
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	u_char flags;
	struct lpt_softc *sc;
	int error;
	int spin;

	flags = LPTFLAGS(dev);

	sc = device_lookup_private(&lpt_cd, LPTUNIT(dev));
	if (!sc)
		return (ENXIO);

#ifdef DIAGNOSTIC
	if (sc->sc_state)
		aprint_verbose_dev(sc->sc_dev, "stat=0x%x not zero\n",
		    sc->sc_state);
#endif

	if (sc->sc_state)
		return (EBUSY);

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;
	LPRINTF((sc->sc_dev, "open: flags=0x%x\n", flags));

	if ((flags & LPT_NOPRIME) == 0) {
		/* assert Input Prime for 100 usec to start up printer */
		(sc->sc_funcs->lf_iprime) (sc);
	}

	/* select fast or slow strobe depending on minor device number */
	if (flags & LPT_FAST_STROBE)
		(sc->sc_funcs->lf_speed) (sc, LPT_STROBE_FAST);
	else
		(sc->sc_funcs->lf_speed) (sc, LPT_STROBE_SLOW);

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; (sc->sc_funcs->lf_notrdy) (sc, 1); spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return (EBUSY);
		}
		/* wait 1/4 second, give up if we get a signal */
		error = tsleep((void *) sc, LPTPRI | PCATCH, "lptopen", STEP);
		if (error != EWOULDBLOCK) {
			sc->sc_state = 0;
			return (error);
		}
	}

	sc->sc_inbuf = geteblk(LPT_BSIZE);
	sc->sc_count = 0;
	sc->sc_state = LPT_OPEN;

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		lpt_wakeup(sc);

	(sc->sc_funcs->lf_open) (sc, sc->sc_flags & LPT_NOINTR);

	LPRINTF((sc->sc_dev, "opened\n"));
	return (0);
}

void
lpt_wakeup(void *arg)
{
	struct lpt_softc *sc;
	int s;

	sc = arg;

	s = spltty();
	lpt_intr(sc);
	splx(s);

	callout_reset(&sc->sc_wakeup_ch, STEP, lpt_wakeup, sc);
}

/*
 * Close the device, and free the local line buffer.
 */
int
lptclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct lpt_softc *sc;

	sc = device_lookup_private(&lpt_cd, LPTUNIT(dev));

	if (sc->sc_count)
		(void) pushbytes(sc);

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		callout_stop(&sc->sc_wakeup_ch);

	(sc->sc_funcs->lf_close) (sc);

	sc->sc_state = 0;
	brelse(sc->sc_inbuf, 0);

	LPRINTF((sc->sc_dev, "%s: closed\n"));
	return (0);
}

int
pushbytes(struct lpt_softc *sc)
{
	int s, error, spin, tic;

	if (sc->sc_flags & LPT_NOINTR) {
		while (sc->sc_count > 0) {
			spin = 0;
			while ((sc->sc_funcs->lf_notrdy) (sc, 0)) {
				if (++spin < sc->sc_spinmax)
					continue;
				tic = 0;
				/* adapt busy-wait algorithm */
				sc->sc_spinmax++;
				while ((sc->sc_funcs->lf_notrdy) (sc, 1)) {
					/* exponential backoff */
					tic = tic + tic + 1;
					if (tic > TIMEOUT)
						tic = TIMEOUT;
					error = tsleep((void *) sc,
					    LPTPRI | PCATCH, "lptpsh", tic);
					if (error != EWOULDBLOCK)
						return (error);
				}
				break;
			}

			(sc->sc_funcs->lf_wrdata) (sc, *sc->sc_cp++);
			sc->sc_count--;

			/* adapt busy-wait algorithm */
			if (spin * 2 + 16 < sc->sc_spinmax)
				sc->sc_spinmax--;
		}
	} else {
		while (sc->sc_count > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & LPT_OBUSY) == 0) {
				LPRINTF((sc->sc_dev, "write %d\n",
					sc->sc_count));
				s = spltty();
				(void) lpt_intr(sc);
				splx(s);
			}
			error = tsleep((void *) sc, LPTPRI | PCATCH,
			    "lptwrite2", 0);
			if (error)
				return (error);
		}
	}
	return (0);
}

/*
 * Copy a line from user space to a local buffer, then call putc to get the
 * chars moved to the output queue.
 */
int
lptwrite(dev_t dev, struct uio *uio, int flags)
{
	struct lpt_softc *sc;
	size_t n;
	int error;

	sc = device_lookup_private(&lpt_cd, LPTUNIT(dev));
	error = 0;

	while ((n = min(LPT_BSIZE, uio->uio_resid)) != 0) {
		uiomove(sc->sc_cp = sc->sc_inbuf->b_data, n, uio);
		sc->sc_count = n;
		error = pushbytes(sc);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += sc->sc_count;
			sc->sc_count = 0;
			return (error);
		}
	}
	return (0);
}

/*
 * Handle printer interrupts which occur when the printer is ready to accept
 * another char.
 */
int
lpt_intr(struct lpt_softc *sc)
{

	if (sc->sc_count) {
		/* send char */
		(sc->sc_funcs->lf_wrdata) (sc, *sc->sc_cp++);
		sc->sc_count--;
		sc->sc_state |= LPT_OBUSY;
	} else
		sc->sc_state &= ~LPT_OBUSY;

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		wakeup((void *) sc);
	}

	return (1);
}

/* ARGSUSED */
int
lptioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{

	return (ENODEV);
}
