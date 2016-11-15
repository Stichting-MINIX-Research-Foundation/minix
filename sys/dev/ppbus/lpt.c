/* $NetBSD: lpt.c,v 1.30 2014/07/25 08:10:38 dholland Exp $ */

/*
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
 *
 *	from: unknown origin, 386BSD 0.1
 *	From Id: lpt.c,v 1.55.2.1 1996/11/12 09:08:38 phk Exp
 *	From Id: nlpt.c,v 1.14 1999/02/08 13:55:43 des Exp
 * FreeBSD: src/sys/dev/ppbus/lpt.c,v 1.15.2.3 2000/07/07 00:30:40 obrien Exp
 */

/*
 * Device Driver for AT parallel printer port
 * Written by William Jolitz 12/18/90
 */

/*
 * Updated for ppbus by Nicolas Souchu
 * [Mon Jul 28 1997]
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt.c,v 1.30 2014/07/25 08:10:38 dholland Exp $");

#include "opt_ppbus_lpt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/syslog.h>

#include <sys/bus.h>

#include <dev/ppbus/ppbus_1284.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_msq.h>
#include <dev/ppbus/ppbus_var.h>

#include <dev/ppbus/lptvar.h>
#include <dev/ppbus/lptreg.h>
#include <dev/ppbus/lptio.h>

/* Autoconf functions */
static int lpt_probe(device_t, cfdata_t, void *);
static void lpt_attach(device_t, device_t, void *);
static int lpt_detach(device_t, int);

/* Autoconf structure */
CFATTACH_DECL_NEW(lpt_ppbus, sizeof(struct lpt_softc), lpt_probe, lpt_attach,
	lpt_detach, NULL);

extern struct cfdriver lpt_cd;

dev_type_open(lptopen);
dev_type_close(lptclose);
dev_type_read(lptread);
dev_type_write(lptwrite);
dev_type_ioctl(lptioctl);

const struct cdevsw lpt_cdevsw = {
        .d_open = lptopen,
	.d_close = lptclose,
	.d_read = lptread,
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


/* Function prototypes */
static int lpt_detect(device_t);
static int lpt_request_ppbus(struct lpt_softc *, int);
static int lpt_release_ppbus(struct lpt_softc *, int);
static int lpt_logstatus(const device_t, const unsigned char);

/*
 * lpt_probe()
 */
static int
lpt_probe(device_t parent, cfdata_t match, void *aux)
{
	/* Test ppbus's capability */
	return lpt_detect(parent);
}

static void
lpt_attach(device_t parent, device_t self, void *aux)
{
	struct lpt_softc * sc = device_private(self);
	struct ppbus_device_softc * ppbdev = &(sc->ppbus_dev);
	struct ppbus_attach_args * args = aux;
	char buf[64];
	int error;

	ppbdev->sc_dev = self;

	error = lpt_request_ppbus(sc, 0);
	if(error) {
		printf("%s(%s): error (%d) requesting bus(%s). Device not "
			"properly attached.\n", __func__, device_xname(self),
			error, device_xname(parent));
		return;
	}

	/* Record capabilities */
	ppbdev->capabilities = args->capabilities;

	/* Allocate memory buffers */
	if(ppbdev->capabilities & PPBUS_HAS_DMA) {
		if(ppbus_dma_malloc(parent, &(sc->sc_inbuf),
			&(sc->sc_in_baddr), BUFSIZE)) {

			printf(" : cannot allocate input DMA buffer. Device "
				"not properly attached!\n");
			return;
		}
		if(ppbus_dma_malloc(parent, &(sc->sc_outbuf),
			&(sc->sc_out_baddr), BUFSIZE)) {

			ppbus_dma_free(parent, &(sc->sc_inbuf),
				&(sc->sc_in_baddr), BUFSIZE);
			printf(" : cannot allocate output DMA buffer. Device "
				"not properly attached!\n");
			return;
		}
	} else {
		sc->sc_inbuf = malloc(BUFSIZE, M_DEVBUF, M_WAITOK);
		sc->sc_outbuf = malloc(BUFSIZE, M_DEVBUF, M_WAITOK);
	}

	/* Print out mode */
        ppbdev->ctx.mode = ppbus_get_mode(parent);
	snprintb(buf, sizeof(buf),
	    "\20\1COMPATIBLE\2NIBBLE\3PS2\4EPP\5ECP\6FAST_CENTR",
	    ppbdev->ctx.mode);
	printf(": port mode = %s\n", buf);

	/* Initialize the device on open by default */
	sc->sc_flags = LPT_PRIME;

	lpt_release_ppbus(sc, 0);
}

static int
lpt_detach(device_t self, int flags)
{
	struct lpt_softc * lpt = device_private(self);
	struct ppbus_device_softc * ppbdev = (struct ppbus_device_softc *) lpt;
	int err;

	if(lpt->sc_state & HAVEBUS) {
		err = lpt_release_ppbus(lpt, 0);
		if(err) {
			printf("%s error (%d) while releasing bus",
				device_xname(self), err);
			if(flags & DETACH_FORCE) {
				printf(", continuing (DETACH_FORCE)!\n");
			}
			else {
				printf(", terminating!\n");
				return 0;
			}
		}
		lpt->sc_state &= ~HAVEBUS;
	}

	ppbdev->ctx.valid = 0;

	/* Free memory buffers */
	if(ppbdev->capabilities & PPBUS_HAS_DMA) {
		ppbus_dma_free(device_parent(self), &(lpt->sc_inbuf),
			&(lpt->sc_in_baddr), BUFSIZE);
		ppbus_dma_free(device_parent(self), &(lpt->sc_outbuf),
			&(lpt->sc_out_baddr), BUFSIZE);
	} else {
		free(lpt->sc_inbuf, M_DEVBUF);
		free(lpt->sc_outbuf, M_DEVBUF);
	}

	return 1;
}

/* Grab bus for lpt device */
static int
lpt_request_ppbus(struct lpt_softc * lpt, int how)
{
	device_t dev = lpt->ppbus_dev.sc_dev;
	int error;

	error = ppbus_request_bus(device_parent(dev), dev, how, (hz));
	if (!(error)) {
		lpt->sc_state |= HAVEBUS;
	}
	else {
		LPT_DPRINTF(("%s(%s): error %d requesting bus.\n", __func__,
			device_xname(dev), error));
	}

	return error;
}

/* Release ppbus to enable other devices to use it. */
static int
lpt_release_ppbus(struct lpt_softc * lpt, int how)
{
	device_t dev = lpt->ppbus_dev.sc_dev;
	int error;

	if(lpt->sc_state & HAVEBUS) {
		error = ppbus_release_bus(device_parent(dev), dev, how, (hz));
		if(!(error))
			lpt->sc_state &= ~HAVEBUS;
		else {
			LPT_DPRINTF(("%s(%s): error releasing bus.\n", __func__,
				device_xname(dev)));
		}
	}
	else {
		error = EINVAL;
		LPT_DPRINTF(("%s(%s): device does not own bus.\n", __func__,
			device_xname(dev)));
	}

	return error;
}


/*
 * Probe simplified by replacing multiple loops with a hardcoded
 * test pattern - 1999/02/08 des@freebsd.org
 *
 * New lpt port probe Geoff Rehmet - Rhodes University - 14/2/94
 * Based partially on Rod Grimes' printer probe
 *
 * Logic:
 *	1) If no port address was given, use the bios detected ports
 *	   and autodetect what ports the printers are on.
 *	2) Otherwise, probe the data port at the address given,
 *	   using the method in Rod Grimes' port probe.
 *	   (Much code ripped off directly from Rod's probe.)
 *
 * Comments from Rod's probe:
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	   We are finally left with a mask of 0x14, due to some printers
 *	   being adamant about holding other bits high ........
 *
 *	   Before probing the control port, we write a 0 to the data port -
 *	   If not, some printers chuck out garbage when the strobe line
 *	   gets toggled.
 *
 *	3) Set the data and control ports to a value of 0
 *
 *	This probe routine has been tested on Epson Lx-800, HP LJ3P,
 *	Epson FX-1170 and C.Itoh 8510RM
 *	printers.
 *	Quick exit on fail added.
 */
static int
lpt_detect(device_t dev)
{
	static const u_char testbyte[18] = {
		0x55,			/* alternating zeros */
		0xaa,			/* alternating ones */
		0xfe, 0xfd, 0xfb, 0xf7,
		0xef, 0xdf, 0xbf, 0x7f,	/* walking zero */
		0x01, 0x02, 0x04, 0x08,
		0x10, 0x20, 0x40, 0x80	/* walking one */
	};
	int i, status;
	u_char dtr, ctr, str, var;

	/* Save register contents */
	dtr = ppbus_rdtr(dev);
	ctr = ppbus_rctr(dev);
	str = ppbus_rstr(dev);

	status = 1;				/* assume success */

	/* Test data port */
	for(i = 0; i < 18; i++) {
		ppbus_wdtr(dev, testbyte[i]);
		if((var = ppbus_rdtr(dev)) != testbyte[i]) {
			status = 0;
			LPT_DPRINTF(("%s(%s): byte value %x cannot be written "
				"and read from data port (got %x instead).\n",
				__func__, device_xname(dev), testbyte[i], var));
			goto end;
		}
	}

	/* Test control port */
	ppbus_wdtr(dev, 0);
	for(i = 0; i < 18; i++) {
		ppbus_wctr(dev, (testbyte[i] & 0x14));
		if(((var = ppbus_rctr(dev)) & 0x14) != (testbyte[i] & 0x14)) {
			status = 0;
			LPT_DPRINTF(("%s(%s): byte value %x (unmasked value "
				"%x) cannot be written and read from control "
				"port (got %x instead).\n", __func__,
				device_xname(dev), (testbyte[i] & 0x14),
				testbyte[i], (var & 0x14)));
			break;
		}
	}

end:
	/* Restore contents of registers */
	ppbus_wdtr(dev, dtr);
	ppbus_wctr(dev, ctr);
	ppbus_wstr(dev, str);

	return status;
}

/* Log status of status register for printer port */
static int
lpt_logstatus(const device_t dev, const unsigned char status)
{
	int err;

	err = EIO;
	if(!(status & LPS_SEL)) {
		log(LOG_ERR, "%s: offline.", device_xname(dev));
	}
	else if(!(status & LPS_NBSY)) {
		log(LOG_ERR, "%s: busy.", device_xname(dev));
	}
	else if(status & LPS_OUT) {
		log(LOG_ERR, "%s: out of paper.", device_xname(dev));
		err = EAGAIN;
	}
	else if(!(status & LPS_NERR)) {
		log(LOG_ERR, "%s: output error.", device_xname(dev));
	}
	else {
		log(LOG_ERR, "%s: no error indication.", device_xname(dev));
		err = 0;
	}

	return err;
}

/*
 * lptopen -- reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev_t dev_id, int flags, int fmt, struct lwp *l)
{
	int trys, err;
	u_int8_t status;
	device_t dev;
	struct lpt_softc * lpt;
	struct ppbus_device_softc * ppbus_dev;
	device_t ppbus;

	dev = device_lookup(&lpt_cd, LPTUNIT(dev_id));
	if(!dev) {
		LPT_DPRINTF(("%s(): device not configured.\n", __func__));
		return ENXIO;
	}

	lpt = device_private(dev);

	ppbus = device_parent(dev);
	ppbus_dev = &(lpt->ppbus_dev);

	/* Request the ppbus */
	err = lpt_request_ppbus(lpt, PPBUS_WAIT|PPBUS_INTR);
	if(err) {
		LPT_DPRINTF(("%s(%s): error (%d) while requesting bus.\n",
			__func__, device_xname(dev), err));
		return (err);
	}

	/* Update bus mode */
	ppbus_dev->ctx.mode = ppbus_get_mode(ppbus);

	/* init printer */
	if ((lpt->sc_flags & LPT_PRIME) && !LPTCTL(dev_id)) {
		LPT_VPRINTF(("%s(%s): initializing printer.\n", __func__,
			device_xname(dev)));
		lpt->sc_state |= LPTINIT;
		ppbus_wctr(ppbus, LPC_SEL | LPC_NINIT);

		/* wait till ready (printer running diagnostics) */
		for(trys = 0, status = ppbus_rstr(ppbus); (status & RDY_MASK)
			!= LP_READY; trys += LPT_STEP, status =
			ppbus_rstr(ppbus)) {

			/* Time up waiting for the printer */
			if(trys >= LPT_TIMEOUT)
				break;
			/* wait LPT_STEP ticks, give up if we get a signal */
			else {
				err = tsleep((void *)lpt, LPPRI|PCATCH,
					"lptinit", LPT_STEP);
				if((err) && (err != EWOULDBLOCK)) {
					lpt->sc_state &= ~LPTINIT;
					LPT_DPRINTF(("%s(%s): interrupted "
					"during initialization.\n", __func__,
					device_xname(dev)));
					lpt_release_ppbus(lpt, PPBUS_WAIT);
					return (err);
				}
			}
		}

		lpt->sc_state &= ~LPTINIT;
		if(trys >= LPT_TIMEOUT) {
			LPT_DPRINTF(("%s(%s): timed out while initializing "
				"printer. [status %x]\n", __func__,
				device_xname(dev), status));
			err = lpt_logstatus(dev, status);
			lpt_release_ppbus(lpt, PPBUS_WAIT);
			return (err);
		}
		else {
			LPT_VPRINTF(("%s(%s): printer ready.\n", __func__,
				device_xname(dev)));
		}
	}

	/* Set autolinefeed if requested */
	if (lpt->sc_flags & LPT_AUTOLF)
		ppbus_wctr(ppbus, LPC_AUTOL);
	else
		ppbus_wctr(ppbus, 0);

	/* ready now */
	lpt->sc_state |= OPEN;

	return 0;
}

/*
 * lptclose -- close the device, free the local line buffer.
 *
 * Check for interrupted write call added.
 */
int
lptclose(dev_t dev_id, int flags, int fmt, struct lwp *l)
{
	device_t dev = device_lookup(&lpt_cd, LPTUNIT(dev_id));
	struct lpt_softc *sc = device_private(dev);
	int err;

	err = lpt_release_ppbus(sc, PPBUS_WAIT|PPBUS_INTR);
	if(err) {
		LPT_DPRINTF(("%s(%s): error (%d) while releasing ppbus.\n",
			__func__, device_xname(dev), err));
	}

	sc->sc_state = 0;

	return err;
}

/*
 * lptread --retrieve printer status in IEEE1284 NIBBLE mode
 */
int
lptread(dev_t dev_id, struct uio *uio, int ioflag)
{
	size_t len = 0;
	int error = 0;
	device_t dev = device_lookup(&lpt_cd, LPTUNIT(dev_id));
	struct lpt_softc *sc = device_private(dev);

	if(!(sc->sc_state & HAVEBUS)) {
		LPT_DPRINTF(("%s(%s): attempt to read using device which does "
			"not own the bus(%s).\n", __func__, device_xname(dev),
			device_xname(device_parent(dev))));
		return (ENODEV);
	}

	sc->sc_state &= ~INTERRUPTED;
	while (uio->uio_resid) {
		error = ppbus_read(device_parent(dev), sc->sc_outbuf,
			min(BUFSIZE, uio->uio_resid), 0, &len);

		/* If error or no more data, stop */
		if (error) {
			if (error != EWOULDBLOCK)
				sc->sc_state |= INTERRUPTED;
			break;
		}
		if (len == 0)
			break;

		if ((error = uiomove(sc->sc_outbuf, len, uio)))
			break;
	}

	return error;
}

/*
 * lptwrite --copy a line from user space to a local buffer, then call
 * putc to get the chars moved to the output queue.
 *
 * Flagging of interrupted write added.
 */
int
lptwrite(dev_t dev_id, struct uio * uio, int ioflag)
{
	int error=0;
	size_t n, cnt;
	device_t dev = device_lookup(&lpt_cd, LPTUNIT(dev_id));
	struct lpt_softc * sc = device_private(dev);

	/* Check state and flags */
	if(!(sc->sc_state & HAVEBUS)) {
		LPT_DPRINTF(("%s(%s): attempt to write using device which does "
			"not own the bus(%s).\n", __func__, device_xname(dev),
			device_xname(device_parent(dev))));
		return EINVAL;
	}

	LPT_VPRINTF(("%s(%s): writing %zu bytes\n", __func__,
	    device_xname(dev), uio->uio_resid));

	/* Write the data */
	sc->sc_state &= ~INTERRUPTED;
	while (uio->uio_resid) {
		n = MIN(BUFSIZE, uio->uio_resid);
		error = uiomove(sc->sc_inbuf, n, uio);
		if (error)
			break;

		error = ppbus_write(device_parent(dev), sc->sc_inbuf, n, ioflag,
			&cnt);
		if (error) {
			if (error != EWOULDBLOCK)
				sc->sc_state |= INTERRUPTED;
			break;
		}
	}

	LPT_VPRINTF(("%s(%s): transfer finished, error %d.\n", __func__,
	    device_xname(dev), error));

	return error;
}

/* Printer ioctl */
int
lptioctl(dev_t dev_id, u_long cmd, void *data, int flags, struct lwp *l)
{
	device_t dev = device_lookup(&lpt_cd, LPTUNIT(dev_id));
	struct lpt_softc *sc = device_private(dev);
	int val, fl;
	int error=0;

	if(!(sc->sc_state & HAVEBUS)) {
		LPT_DPRINTF(("%s(%s): attempt to perform ioctl on device which "
			"does not own the bus(%s).\n", __func__, device_xname(dev),
			device_xname(device_parent(dev))));
		return EBUSY;
	}

	switch (cmd) {
	case LPTGMODE:
        	switch (ppbus_get_mode(device_parent(dev))) {
		case PPBUS_COMPATIBLE:
			val = mode_standard;
			break;
		case PPBUS_NIBBLE:
			val = mode_nibble;
			break;
		case PPBUS_PS2:
			val = mode_ps2;
			break;
		case PPBUS_FAST:
			val = mode_fast;
			break;
		case PPBUS_EPP:
			val = mode_epp;
			break;
		case PPBUS_ECP:
			val = mode_ecp;
			break;
		default:
			error = EINVAL;
			val = mode_unknown;
			break;
		}
		*(int *)data = val;
		break;

	case LPTSMODE:
        	switch (*(int *)data) {
		case mode_standard:
			val = PPBUS_COMPATIBLE;
			break;
		case mode_nibble:
			val = PPBUS_NIBBLE;
			break;
		case mode_ps2:
			val = PPBUS_PS2;
			break;
		case mode_fast:
			val = PPBUS_FAST;
			break;
		case mode_epp:
			val = PPBUS_EPP;
			break;
		case mode_ecp:
			val = PPBUS_ECP;
			break;
		default:
			error = EINVAL;
			val = mode_unknown;
			break;
		}

		if (!error)
			error = ppbus_set_mode(device_parent(dev), val, 0);

		break;

	case LPTGFLAGS:
		fl = 0;

		/* DMA */
		error = ppbus_read_ivar(device_parent(dev), PPBUS_IVAR_DMA, &val);
		if (error)
			break;
		if (val)
			fl |= LPT_DMA;

		/* IEEE mode negotiation */
		error = ppbus_read_ivar(device_parent(dev), PPBUS_IVAR_IEEE, &val);
		if (error)
			break;
		if (val)
			fl |= LPT_IEEE;

		/* interrupts */
		error = ppbus_read_ivar(device_parent(dev), PPBUS_IVAR_INTR, &val);
		if (error)
			break;
		if (val)
			fl |= LPT_INTR;

		/* lpt-only flags */
		fl |= sc->sc_flags;

		*(int *)data = fl;
		break;

	case LPTSFLAGS:
		fl = *(int *)data;

		/* DMA */
		val = (fl & LPT_DMA);
		error = ppbus_write_ivar(device_parent(dev), PPBUS_IVAR_DMA, &val);
		if (error)
			break;

		/* IEEE mode negotiation */
		val = (fl & LPT_IEEE);
		error = ppbus_write_ivar(device_parent(dev), PPBUS_IVAR_IEEE, &val);
		if (error)
			break;

		/* interrupts */
		val = (fl & LPT_INTR);
		error = ppbus_write_ivar(device_parent(dev), PPBUS_IVAR_INTR, &val);
		if (error)
			break;

		/* lpt-only flags */
		sc->sc_flags = fl & (LPT_PRIME|LPT_AUTOLF);

		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

