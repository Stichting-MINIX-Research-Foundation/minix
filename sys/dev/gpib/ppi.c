/*	$NetBSD: ppi.c,v 1.22 2014/07/25 08:10:36 dholland Exp $	*/

/*-
 * Copyright (c) 1996-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ppi.c	8.1 (Berkeley) 6/16/93
 */

/*
 * Printer/Plotter GPIB interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppi.c,v 1.22 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <dev/gpib/gpibvar.h>

#include <dev/gpib/ppiio.h>

struct	ppi_softc {
	device_t sc_dev;
	gpib_chipset_tag_t sc_ic;
	gpib_handle_t sc_hdl;

	int sc_address;			/* GPIB address */
	int	sc_flags;
	int	sc_sec;
	struct	ppiparam sc_param;
#define sc_burst sc_param.burst
#define sc_timo  sc_param.timo
#define sc_delay sc_param.delay
	struct	callout sc_timo_ch;
	struct	callout sc_start_ch;
};

/* sc_flags values */
#define	PPIF_ALIVE	0x01
#define	PPIF_OPEN	0x02
#define PPIF_UIO	0x04
#define PPIF_TIMO	0x08
#define PPIF_DELAY	0x10

int	ppimatch(device_t, cfdata_t, void *);
void	ppiattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ppi, sizeof(struct ppi_softc),
	ppimatch, ppiattach, NULL, NULL);

extern struct cfdriver ppi_cd;

void	ppicallback(void *, int);
void	ppistart(void *);

void	ppitimo(void *);
int	ppirw(dev_t, struct uio *);
int	ppihztoms(int);
int	ppimstohz(int);

dev_type_open(ppiopen);
dev_type_close(ppiclose);
dev_type_read(ppiread);
dev_type_write(ppiwrite);
dev_type_ioctl(ppiioctl);

const struct cdevsw ppi_cdevsw = {
        .d_open = ppiopen,
	.d_close = ppiclose,
	.d_read = ppiread,
	.d_write = ppiwrite,
	.d_ioctl = ppiioctl,
        .d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

#define UNIT(x)		minor(x)

#ifdef DEBUG
int	ppidebug = 0x80;
#define PDB_FOLLOW	0x01
#define PDB_IO		0x02
#define PDB_NOCHECK	0x80
#define DPRINTF(mask, str)	if (ppidebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

int
ppimatch(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

void
ppiattach(device_t parent, device_t self, void *aux)
{
	struct ppi_softc *sc = device_private(self);
	struct gpib_attach_args *ga = aux;

	printf("\n");

	sc->sc_ic = ga->ga_ic;
	sc->sc_address = ga->ga_address;

	callout_init(&sc->sc_timo_ch, 0);
	callout_init(&sc->sc_start_ch, 0);

	if (gpibregister(sc->sc_ic, sc->sc_address, ppicallback, sc,
	    &sc->sc_hdl)) {
		aprint_error_dev(sc->sc_dev, "can't register callback\n");
		return;
	}

	sc->sc_flags = PPIF_ALIVE;
}

int
ppiopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ppi_softc *sc;

	sc = device_lookup_private(&ppi_cd, UNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	if ((sc->sc_flags & PPIF_ALIVE) == 0)
		return (ENXIO);

	DPRINTF(PDB_FOLLOW, ("ppiopen(%" PRIx64 ", %x): flags %x\n",
	    dev, flags, sc->sc_flags));

	if (sc->sc_flags & PPIF_OPEN)
		return (EBUSY);
	sc->sc_flags |= PPIF_OPEN;
	sc->sc_burst = PPI_BURST;
	sc->sc_timo = ppimstohz(PPI_TIMO);
	sc->sc_delay = ppimstohz(PPI_DELAY);
	sc->sc_sec = -1;
	return (0);
}

int
ppiclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ppi_softc *sc;

	sc = device_lookup_private(&ppi_cd, UNIT(dev));

	DPRINTF(PDB_FOLLOW, ("ppiclose(%" PRIx64 ", %x): flags %x\n",
		       dev, flags, sc->sc_flags));

	sc->sc_flags &= ~PPIF_OPEN;
	return (0);
}

void
ppicallback(void *v, int action)
{
	struct ppi_softc *sc = v;

	DPRINTF(PDB_FOLLOW, ("ppicallback: v=%p, action=%d\n", v, action));

	switch (action) {
	case GPIBCBF_START:
		ppistart(sc);
	case GPIBCBF_INTR:
		/* no-op */
		break;
#ifdef DEBUG
	default:
		DPRINTF(PDB_FOLLOW, ("ppicallback: unknown action %d\n",
		    action));
		break;
#endif
	}
}

void
ppistart(void *v)
{
	struct ppi_softc *sc = v;

	DPRINTF(PDB_FOLLOW, ("ppistart(%x)\n", device_unit(sc->sc_dev)));

	sc->sc_flags &= ~PPIF_DELAY;
	wakeup(sc);
}

void
ppitimo(void *arg)
{
	struct ppi_softc *sc = arg;

	DPRINTF(PDB_FOLLOW, ("ppitimo(%x)\n", device_unit(sc->sc_dev)));

	sc->sc_flags &= ~(PPIF_UIO|PPIF_TIMO);
	wakeup(sc);
}

int
ppiread(dev_t dev, struct uio *uio, int flags)
{

	DPRINTF(PDB_FOLLOW, ("ppiread(%" PRIx64 ", %p)\n", dev, uio));

	return (ppirw(dev, uio));
}

int
ppiwrite(dev_t dev, struct uio *uio, int flags)
{

	DPRINTF(PDB_FOLLOW, ("ppiwrite(%" PRIx64 ", %p)\n", dev, uio));

	return (ppirw(dev, uio));
}

int
ppirw(dev_t dev, struct uio *uio)
{
	struct ppi_softc *sc = device_lookup_private(&ppi_cd, UNIT(dev));
	int s1, s2, len, cnt;
	char *cp;
	int error = 0, gotdata = 0;
	int buflen, address;
	char *buf;

	if (uio->uio_resid == 0)
		return (0);

	address = sc->sc_address;

	DPRINTF(PDB_FOLLOW|PDB_IO,
	    ("ppirw(%" PRIx64 ", %p, %c): burst %d, timo %d, resid %x\n",
	    dev, uio, uio->uio_rw == UIO_READ ? 'R' : 'W',
	    sc->sc_burst, sc->sc_timo, uio->uio_resid));

	buflen = min(sc->sc_burst, uio->uio_resid);
	buf = (char *)malloc(buflen, M_DEVBUF, M_WAITOK);
	sc->sc_flags |= PPIF_UIO;
	if (sc->sc_timo > 0) {
		sc->sc_flags |= PPIF_TIMO;
		callout_reset(&sc->sc_timo_ch, sc->sc_timo, ppitimo, sc);
	}
	len = cnt = 0;
	while (uio->uio_resid > 0) {
		len = min(buflen, uio->uio_resid);
		cp = buf;
		if (uio->uio_rw == UIO_WRITE) {
			error = uiomove(cp, len, uio);
			if (error)
				break;
		}
again:
		s1 = splsoftclock();
		s2 = splbio();
		if (sc->sc_flags & PPIF_UIO) {
			if (gpibrequest(sc->sc_ic, sc->sc_hdl) == 0)
				(void) tsleep(sc, PRIBIO + 1, "ppirw", 0);
		}
		/*
		 * Check if we timed out during sleep or uiomove
		 */
		splx(s2);
		if ((sc->sc_flags & PPIF_UIO) == 0) {
			DPRINTF(PDB_IO,
			    ("ppirw: uiomove/sleep timo, flags %x\n",
			    sc->sc_flags));
			if (sc->sc_flags & PPIF_TIMO) {
				callout_stop(&sc->sc_timo_ch);
				sc->sc_flags &= ~PPIF_TIMO;
			}
			splx(s1);
			break;
		}
		splx(s1);
		/*
		 * Perform the operation
		 */
		if (uio->uio_rw == UIO_WRITE)
			cnt = gpibsend(sc->sc_ic, address, sc->sc_sec,
			    cp, len);
		else
			cnt = gpibrecv(sc->sc_ic, address, sc->sc_sec,
			    cp, len);
		s1 = splbio();
		gpibrelease(sc->sc_ic, sc->sc_hdl);
		DPRINTF(PDB_IO, ("ppirw: %s(%d, %x, %p, %d) -> %d\n",
		    uio->uio_rw == UIO_READ ? "recv" : "send",
		    address, sc->sc_sec, cp, len, cnt));
		splx(s1);
		if (uio->uio_rw == UIO_READ) {
			if (cnt) {
				error = uiomove(cp, cnt, uio);
				if (error)
					break;
				gotdata++;
			}
			/*
			 * Didn't get anything this time, but did in the past.
			 * Consider us done.
			 */
			else if (gotdata)
				break;
		}
		s1 = splsoftclock();
		/*
		 * Operation timeout (or non-blocking), quit now.
		 */
		if ((sc->sc_flags & PPIF_UIO) == 0) {
			DPRINTF(PDB_IO, ("ppirw: timeout/done\n"));
			splx(s1);
			break;
		}
		/*
		 * Implement inter-read delay
		 */
		if (sc->sc_delay > 0) {
			sc->sc_flags |= PPIF_DELAY;
			callout_reset(&sc->sc_start_ch, sc->sc_delay,
			    ppistart, sc);
			error = tsleep(sc, (PCATCH|PZERO) + 1, "gpib", 0);
			if (error) {
				splx(s1);
				break;
			}
		}
		splx(s1);
		/*
		 * Must not call uiomove again til we've used all data
		 * that we already grabbed.
		 */
		if (uio->uio_rw == UIO_WRITE && cnt != len) {
			cp += cnt;
			len -= cnt;
			cnt = 0;
			goto again;
		}
	}
	s1 = splsoftclock();
	if (sc->sc_flags & PPIF_TIMO) {
		callout_stop(&sc->sc_timo_ch);
		sc->sc_flags &= ~PPIF_TIMO;
	}
	if (sc->sc_flags & PPIF_DELAY) {
		callout_stop(&sc->sc_start_ch);
		sc->sc_flags &= ~PPIF_DELAY;
	}
	splx(s1);
	/*
	 * Adjust for those chars that we uiomove'ed but never wrote
	 */
	if (uio->uio_rw == UIO_WRITE && cnt != len) {
		uio->uio_resid += (len - cnt);
		DPRINTF(PDB_IO, ("ppirw: short write, adjust by %d\n",
		    len - cnt));
	}
	free(buf, M_DEVBUF);
	DPRINTF(PDB_FOLLOW|PDB_IO, ("ppirw: return %d, resid %d\n",
	    error, uio->uio_resid));
	return (error);
}

int
ppiioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct ppi_softc *sc = device_lookup_private(&ppi_cd, UNIT(dev));
	struct ppiparam *pp, *upp;
	int error = 0;

	switch (cmd) {
	case PPIIOCGPARAM:
		pp = &sc->sc_param;
		upp = (struct ppiparam *)data;
		upp->burst = pp->burst;
		upp->timo = ppihztoms(pp->timo);
		upp->delay = ppihztoms(pp->delay);
		break;
	case PPIIOCSPARAM:
		pp = &sc->sc_param;
		upp = (struct ppiparam *)data;
		if (upp->burst < PPI_BURST_MIN || upp->burst > PPI_BURST_MAX ||
		    upp->delay < PPI_DELAY_MIN || upp->delay > PPI_DELAY_MAX)
			return (EINVAL);
		pp->burst = upp->burst;
		pp->timo = ppimstohz(upp->timo);
		pp->delay = ppimstohz(upp->delay);
		break;
	case PPIIOCSSEC:
		sc->sc_sec = *(int *)data;
		break;
	default:
		return (EINVAL);
	}
	return (error);
}

int
ppihztoms(int h)
{
	extern int hz;
	int m = h;

	if (m > 0)
		m = m * 1000 / hz;
	return (m);
}

int
ppimstohz(int m)
{
	extern int hz;
	int h = m;

	if (h > 0) {
		h = h * hz / 1000;
		if (h == 0)
			h = 1000 / hz;
	}
	return (h);
}
