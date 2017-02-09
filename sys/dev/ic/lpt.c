/*	$NetBSD: lpt.c,v 1.80 2014/07/25 08:10:37 dholland Exp $	*/

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
 * Device Driver for AT style parallel printer port
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt.c,v 1.80 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/intr.h>

#include <sys/bus.h>

#include <dev/ic/lptreg.h>
#include <dev/ic/lptvar.h>

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#define LPTDEBUG

#ifndef LPTDEBUG
#define LPRINTF(a)
#else
#define LPRINTF(a)	if (lptdebug) printf a
int lptdebug = 0;
#endif

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
	.d_flag = D_OTHER
};

#define	LPTUNIT(s)	(minor(s) & 0x1f)
#define	LPTFLAGS(s)	(minor(s) & 0xe0)

static void	lptsoftintr(void *);

void
lpt_attach_subr(struct lpt_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	sc->sc_state = 0;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);

	callout_init(&sc->sc_wakeup_ch, 0);
	sc->sc_sih = softint_establish(SOFTINT_SERIAL, lptsoftintr, sc);

	sc->sc_dev_ok = 1;
}

int
lpt_detach_subr(device_t self, int flags)
{
	struct lpt_softc *sc = device_private(self);

	sc->sc_dev_ok = 0;
	softint_disestablish(sc->sc_sih);
	callout_destroy(&sc->sc_wakeup_ch);
	return 0;
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	u_char flags = LPTFLAGS(dev);
	struct lpt_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char control;
	int error;
	int spin;

	sc = device_lookup_private(&lpt_cd, LPTUNIT(dev));
	if (!sc || !sc->sc_dev_ok)
		return ENXIO;

#if 0	/* XXX what to do? */
	if (sc->sc_irq == IRQUNK && (flags & LPT_NOINTR) == 0)
		return ENXIO;
#endif

#ifdef DIAGNOSTIC
	if (sc->sc_state)
		aprint_verbose_dev(sc->sc_dev, "stat=0x%x not zero\n",
		    sc->sc_state);
#endif

	if (sc->sc_state)
		return EBUSY;

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;
	LPRINTF(("%s: open: flags=0x%x\n", device_xname(sc->sc_dev),
	    (unsigned)flags));
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	if ((flags & LPT_NOPRIME) == 0) {
		/* assert INIT for 100 usec to start up printer */
		bus_space_write_1(iot, ioh, lpt_control, LPC_SELECT);
		delay(100);
	}

	control = LPC_SELECT | LPC_NINIT;
	bus_space_write_1(iot, ioh, lpt_control, control);

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; NOT_READY_ERR(); spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return EBUSY;
		}

		/* wait 1/4 second, give up if we get a signal */
		error = tsleep((void *)sc, LPTPRI | PCATCH, "lptopen", STEP);
		if (error != EWOULDBLOCK) {
			sc->sc_state = 0;
			return error;
		}
	}

	if ((flags & LPT_NOINTR) == 0)
		control |= LPC_IENABLE;
	if (flags & LPT_AUTOLF)
		control |= LPC_AUTOLF;
	sc->sc_control = control;
	bus_space_write_1(iot, ioh, lpt_control, control);

	sc->sc_inbuf = malloc(LPT_BSIZE, M_DEVBUF, M_WAITOK);
	sc->sc_count = 0;
	sc->sc_state = LPT_OPEN;

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		lptwakeup(sc);

	LPRINTF(("%s: opened\n", device_xname(sc->sc_dev)));
	return 0;
}

int
lptnotready(u_char status, struct lpt_softc *sc)
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (sc->sc_state & LPT_OPEN) {
		if (new & LPS_SELECT)
			log(LOG_NOTICE,
			    "%s: offline\n", device_xname(sc->sc_dev));
		else if (new & LPS_NOPAPER)
			log(LOG_NOTICE,
			    "%s: out of paper\n", device_xname(sc->sc_dev));
		else if (new & LPS_NERR)
			log(LOG_NOTICE,
			    "%s: output error\n", device_xname(sc->sc_dev));
	}

	return status;
}

void
lptwakeup(void *arg)
{
	struct lpt_softc *sc = arg;
	int s;

	s = splvm();
	lptintr(sc);
	splx(s);

	callout_reset(&sc->sc_wakeup_ch, STEP, lptwakeup, sc);
}

/*
 * Close the device, and free the local line buffer.
 */
int
lptclose(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct lpt_softc *sc =
	    device_lookup_private(&lpt_cd, LPTUNIT(dev));
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (sc->sc_count)
		(void) lptpushbytes(sc);

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		callout_stop(&sc->sc_wakeup_ch);

	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);
	sc->sc_state = 0;
	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);
	free(sc->sc_inbuf, M_DEVBUF);

	LPRINTF(("%s: closed\n", device_xname(sc->sc_dev)));
	return 0;
}

int
lptpushbytes(struct lpt_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int error;

	if (sc->sc_flags & LPT_NOINTR) {
		int spin, tic;
		u_char control = sc->sc_control;

		while (sc->sc_count > 0) {
			spin = 0;
			while (NOT_READY()) {
				if (++spin < sc->sc_spinmax)
					continue;
				tic = 0;
				/* adapt busy-wait algorithm */
				sc->sc_spinmax++;
				while (NOT_READY_ERR()) {
					/* exponential backoff */
					tic = tic + tic + 1;
					if (tic > TIMEOUT)
						tic = TIMEOUT;
					error = tsleep((void *)sc,
					    LPTPRI | PCATCH, "lptpsh", tic);
					if (error != EWOULDBLOCK)
						return error;
				}
				break;
			}

			bus_space_write_1(iot, ioh, lpt_data, *sc->sc_cp++);
			DELAY(1);
			bus_space_write_1(iot, ioh, lpt_control,
			    control | LPC_STROBE);
			DELAY(1);
			sc->sc_count--;
			bus_space_write_1(iot, ioh, lpt_control, control);
			DELAY(1);

			/* adapt busy-wait algorithm */
			if (spin*2 + 16 < sc->sc_spinmax)
				sc->sc_spinmax--;
		}
	} else {
		int s;

		while (sc->sc_count > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & LPT_OBUSY) == 0) {
				LPRINTF(("%s: write %lu\n",
				    device_xname(sc->sc_dev),
				    (u_long)sc->sc_count));
				s = splvm();
				(void) lptintr(sc);
				splx(s);
			}
			error = tsleep((void *)sc, LPTPRI | PCATCH,
			    "lptwrite2", 0);
			if (error)
				return error;
		}
	}
	return 0;
}

/*
 * Copy a line from user space to a local buffer, then call putc to get the
 * chars moved to the output queue.
 */
int
lptwrite(dev_t dev, struct uio *uio, int flags)
{
	struct lpt_softc *sc =
	    device_lookup_private(&lpt_cd, LPTUNIT(dev));
	size_t n;
	int error = 0;

	while ((n = min(LPT_BSIZE, uio->uio_resid)) != 0) {
		uiomove(sc->sc_cp = sc->sc_inbuf, n, uio);
		sc->sc_count = n;
		error = lptpushbytes(sc);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += sc->sc_count;
			sc->sc_count = 0;
			return error;
		}
	}
	return 0;
}

/*
 * Handle printer interrupts which occur when the printer is ready to accept
 * another char.
 */
int
lptintr(void *arg)
{
	struct lpt_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

#if 0
	if ((sc->sc_state & LPT_OPEN) == 0)
		return 0;
#endif

	/* is printer online and ready for output */
	if (NOT_READY() && NOT_READY_ERR())
		return 0;

	if (sc->sc_count) {
		u_char control = sc->sc_control;
		/* send char */
		bus_space_write_1(iot, ioh, lpt_data, *sc->sc_cp++);
		DELAY(1);
		bus_space_write_1(iot, ioh, lpt_control, control | LPC_STROBE);
		DELAY(1);
		sc->sc_count--;
		bus_space_write_1(iot, ioh, lpt_control, control);
		DELAY(1);
		sc->sc_state |= LPT_OBUSY;
	} else
		sc->sc_state &= ~LPT_OBUSY;

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		softint_schedule(sc->sc_sih);
	}

	return 1;
}

static void
lptsoftintr(void *cookie)
{

	wakeup(cookie);
}

int
lptioctl(dev_t dev, u_long cmd, void *data,
    int flag, struct lwp *l)
{
	return ENODEV;
}
