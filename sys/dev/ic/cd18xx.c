/*	$NetBSD: cd18xx.c,v 1.31 2014/07/25 08:10:37 dholland Exp $	*/

/*
 * Copyright (c) 1998, 2001 Matthew R. Green
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

/*
 * cirrus logic CL-CD180/CD1864/CD1865 driver, based in (large) parts on
 * the com and z8530 drivers.  thanks charles.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cd18xx.c,v 1.31 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/tty.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/intr.h>

#include <sys/bus.h>

#include <dev/ic/cd18xxvar.h>
#include <dev/ic/cd18xxreg.h>

#include "ioconf.h"

/*
 * some helpers
 */

static void	cdtty_attach(struct cd18xx_softc *, int);

static inline void cd18xx_rint(struct cd18xx_softc *, int *);
static inline void cd18xx_tint(struct cd18xx_softc *, int *);
static inline void cd18xx_mint(struct cd18xx_softc *, int *);

void cdtty_rxsoft(struct cd18xx_softc *, struct cdtty_port *, struct tty *);
void cdtty_txsoft(struct cd18xx_softc *, struct cdtty_port *, struct tty *);
void cdtty_stsoft(struct cd18xx_softc *, struct cdtty_port *, struct tty *);
void cd18xx_softintr(void *);

dev_type_open(cdttyopen);
dev_type_close(cdttyclose);
dev_type_read(cdttyread);
dev_type_write(cdttywrite);
dev_type_ioctl(cdttyioctl);
dev_type_stop(cdttystop);
dev_type_tty(cdttytty);
dev_type_poll(cdttypoll);

const struct cdevsw cdtty_cdevsw = {
	.d_open = cdttyopen,
	.d_close = cdttyclose,
	.d_read = cdttyread,
	.d_write = cdttywrite,
	.d_ioctl = cdttyioctl,
	.d_stop = cdttystop,
	.d_tty = cdttytty,
	.d_poll = cdttypoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

static void	cdtty_shutdown(struct cd18xx_softc *, struct cdtty_port *);
static void	cdttystart(struct tty *);
static int	cdttyparam(struct tty *, struct termios *);
static void	cdtty_break(struct cd18xx_softc *, struct cdtty_port *, int);
static void	cdtty_modem(struct cd18xx_softc *, struct cdtty_port *, int);
static int	cdttyhwiflow(struct tty *, int);
static void	cdtty_hwiflow(struct cd18xx_softc *, struct cdtty_port *);

static void	cdtty_loadchannelregs(struct cd18xx_softc *,
					   struct cdtty_port *);

/* default read buffer size */
u_int cdtty_rbuf_size = CDTTY_RING_SIZE;

/* Stop input when 3/4 of the ring is full; restart when only 1/4 is full. */
u_int cdtty_rbuf_hiwat = (CDTTY_RING_SIZE * 1) / 4;
u_int cdtty_rbuf_lowat = (CDTTY_RING_SIZE * 3) / 4;

#define CD18XXDEBUG
#ifdef CD18XXDEBUG
#define CDD_INFO	0x0001
#define CDD_INTR	0x0002
int cd18xx_debug = CDD_INTR|CDD_INFO;
# define DPRINTF(l, x)	if (cd18xx_debug & l) printf x
#else
# define DPRINTF(l, x)	/* nothing */
#endif

/* Known supported revisions. */
struct cd18xx_revs {
	u_char		revision;
	u_char		onehundred_pin;
	const char	*name;
} cd18xx_revs[] = {
	{ CD180_GFRCR_REV_B,		0, "CL-CD180 rev. B" },
	{ CD180_GFRCR_REV_C,		0, "CL-CD180 rev. C" },
	{ CD1864_GFRCR_REVISION_A,	1, "CL-CD1864 rev. A" },
	{ CD1865_GFRCR_REVISION_A,	1, "CL-CD1865 rev. A" },
	{ CD1865_GFRCR_REVISION_B,	1, "CL-CD1865 rev. B" },
	{ CD1865_GFRCR_REVISION_C,	1, "CL-CD1865 rev. C" },
	{ 0, 0, 0 }
};

/* wait for the CCR to go to zero */
static inline int cd18xx_wait_ccr(struct cd18xx_softc *);
static inline int
cd18xx_wait_ccr(struct cd18xx_softc *sc)
{
	int i = 100000;

	while (--i &&
	    bus_space_read_1(sc->sc_tag, sc->sc_handle, CD18xx_CCR) != 0)
		;
	return (i == 0);
}

/*
 * device attach routine, high-end portion
 */
void
cd18xx_attach(struct cd18xx_softc *sc)
{
	static int chip_id_next = 1;
	int onehundred_pin, revision, i, port;

	/* read and print the revision */
	revision = cd18xx_read(sc, CD18xx_GFRCR);
	onehundred_pin = ISSET(cd18xx_read(sc, CD18xx_SRCR),CD18xx_SRCR_PKGTYP);
	for (i = 0; cd18xx_revs[i].name; i++)
		if (revision == cd18xx_revs[i].revision ||
		    onehundred_pin == cd18xx_revs[i].onehundred_pin) {
			printf(": %s", cd18xx_revs[i].name);
			break;
		}

	if (cd18xx_revs[i].name == NULL) {
		aprint_error_dev(sc->sc_dev, "unknown revision, bailing.\n");
		return;
	}

	/* prepare for reset */
	cd18xx_set_car(sc, 0);
	cd18xx_write(sc, CD18xx_GSVR, CD18xx_GSVR_CLEAR);

	/* wait for CCR to go to zero */
	if (cd18xx_wait_ccr(sc)) {
		printf("cd18xx_attach: reset change command timed out\n");
		return;
	}

	/* full reset of all channels */
	cd18xx_write(sc, CD18xx_CCR,
	    CD18xx_CCR_RESET|CD18xx_CCR_RESET_HARD);

	/* loop until the GSVR is ready */
	i = 100000;
	while (--i && cd18xx_read(sc, CD18xx_GSVR) == CD18xx_GSVR_READY)
		;
	if (i == 0) {
		aprint_normal("\n");
		aprint_error_dev(sc->sc_dev, "did not reset!\n");
		return;
	}

	/* write the chip_id */
	sc->sc_chip_id = chip_id_next++;
#ifdef DIAGNOSTIC
	if (sc->sc_chip_id > 31)
		panic("more than 31 cd18xx's?  help.");
#endif
	cd18xx_write(sc, CD18xx_GSVR, CD18xx_GSVR_SETID(sc));

	/* rx/tx/modem service match vectors, initalised by higher level */
	cd18xx_write(sc, CD18xx_MSMR, sc->sc_msmr | 0x80);
	cd18xx_write(sc, CD18xx_TSMR, sc->sc_tsmr | 0x80);
	cd18xx_write(sc, CD18xx_RSMR, sc->sc_rsmr | 0x80);

	printf(", gsvr %x msmr %x tsmr %x rsmr %x",
	    cd18xx_read(sc, CD18xx_GSVR),
	    cd18xx_read(sc, CD18xx_MSMR),
	    cd18xx_read(sc, CD18xx_TSMR),
	    cd18xx_read(sc, CD18xx_RSMR));

	/* prescale registers */
	sc->sc_pprh = 0xf0;
	sc->sc_pprl = 0;
	cd18xx_write(sc, CD18xx_PPRH, sc->sc_pprh);
	cd18xx_write(sc, CD18xx_PPRL, sc->sc_pprl);

	/* establish our soft interrupt. */
	sc->sc_si = softint_establish(SOFTINT_SERIAL, cd18xx_softintr, sc);

	printf(", 8 ports ready (chip id %d)\n", sc->sc_chip_id);

	/*
	 * finally, we loop over all 8 channels initialising them
	 */
	for (port = 0; port < 8; port++)
		cdtty_attach(sc, port);
}

/* tty portion of the code */

/*
 * tty portion attach routine
 */
void
cdtty_attach(struct cd18xx_softc *sc, int port)
{
	struct cdtty_port *p = &sc->sc_ports[port];

	/* load CAR with channel number */
	cd18xx_set_car(sc, port);

	/* wait for CCR to go to zero */
	if (cd18xx_wait_ccr(sc)) {
		printf("cd18xx_attach: change command timed out setting "
		       "CAR for port %d\n", port);
		return;
	}

	/* set the RPTR to (arbitrary) 8 */
	cd18xx_write(sc, CD18xx_RTPR, 8);

	/* reset the modem signal value register */
	sc->sc_ports[port].p_msvr = CD18xx_MSVR_RESET;

	/* zero the service request enable register */
	cd18xx_write(sc, CD18xx_SRER, 0);

	/* enable the transmitter & receiver */
	SET(p->p_chanctl, CD18xx_CCR_CHANCTL |
		          CD18xx_CCR_CHANCTL_TxEN |
		          CD18xx_CCR_CHANCTL_RxEN);

	/* XXX no console or kgdb support yet! */

	/* get a tty structure */
	p->p_tty = tty_alloc();
	p->p_tty->t_oproc = cdttystart;
	p->p_tty->t_param = cdttyparam;
	p->p_tty->t_hwiflow = cdttyhwiflow;

	p->p_rbuf = malloc(cdtty_rbuf_size << 1, M_DEVBUF, M_WAITOK);
	p->p_rbput = p->p_rbget = p->p_rbuf;
	p->p_rbavail = cdtty_rbuf_size;
	if (p->p_rbuf == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to allocate ring buffer for tty %d\n", port);
		return;
	}
	p->p_ebuf = p->p_rbuf + (cdtty_rbuf_size << 1);

	tty_attach(p->p_tty);
}

/*
 * cdtty_shutdown: called when the device is last closed.
 */
void
cdtty_shutdown(struct cd18xx_softc *sc, struct cdtty_port *p)
{
	struct tty *tp = p->p_tty;
	int s;

	s = splserial();

	/* If we were asserting flow control, then deassert it. */
	SET(p->p_rx_flags, RX_IBUF_BLOCKED);
	cdtty_hwiflow(sc, p);

	/* Clear any break condition set with TIOCSBRK. */
	cdtty_break(sc, p, 0);

	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 * Avoid tsleeping above splhigh().
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		cdtty_modem(sc, p, 0);
		splx(s);
		/* XXX tsleep will only timeout */
		(void) tsleep(sc, TTIPRI, ttclos, hz);
		s = splserial();
	}

	/* Turn off interrupts. */
	p->p_srer = 0;
	cd18xx_write(sc, CD18xx_SRER, p->p_srer);

	splx(s);
}

/*
 * cdttyopen:  open syscall for cdtty terminals..
 */
int
cdttyopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct tty *tp;
	struct cd18xx_softc *sc;
	struct cdtty_port *port;
	int channel, instance, s, error;

	channel = CD18XX_CHANNEL(dev);
	instance = CD18XX_INSTANCE(dev);

	/* ensure instance is valid */
	if (instance >= clcd_cd.cd_ndevs)
		return (ENXIO);

	/* get softc and port */
	sc = device_lookup_private(&clcd_cd, instance);
	if (sc == NULL)
		return (ENXIO);
	port = &sc->sc_ports[channel];
	if (port == NULL || port->p_rbuf == NULL)
		return (ENXIO);

	/* kgdb support?  maybe later... */

	tp = port->p_tty;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);

	s = spltty();

	/*
	 * Do the following iff this is a first open.
	 */
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		struct termios t;

		/* set up things in tp as necessary */
		tp->t_dev = dev;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;

		if (ISSET(port->p_swflags, TIOCFLAG_CLOCAL))
			SET(t.c_cflag, CLOCAL);
		if (ISSET(port->p_swflags, TIOCFLAG_CRTSCTS))
			SET(t.c_cflag, CRTSCTS);
		if (ISSET(port->p_swflags, TIOCFLAG_CDTRCTS))
			SET(t.c_cflag, CDTRCTS);
		if (ISSET(port->p_swflags, TIOCFLAG_MDMBUF))
			SET(t.c_cflag, MDMBUF);

		/* Make sure param will see changes. */
		tp->t_ospeed = 0;	/* XXX set above ignored? */
		(void)cdttyparam(tp, &t);

		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		(void)splserial();

		/* turn on rx and modem interrupts */
		cd18xx_set_car(sc, CD18XX_CHANNEL(dev));
		SET(port->p_srer, CD18xx_SRER_Rx |
				  CD18xx_SRER_RxSC |
				  CD18xx_SRER_CD);
		cd18xx_write(sc, CD18xx_SRER, port->p_srer);

		/* always turn on DTR when open */
		cdtty_modem(sc, port, 1);

		/* initialise ring buffer */
		port->p_rbget = port->p_rbput = port->p_rbuf;
		port->p_rbavail = cdtty_rbuf_size;
		CLR(port->p_rx_flags, RX_ANY_BLOCK);
		cdtty_hwiflow(sc, port);
	}

	/* drop spl back before going into the line open */
	splx(s);

	error = ttyopen(tp, CD18XX_DIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (error == 0)
		error = (*tp->t_linesw->l_open)(dev, tp);

	return (error);
}

/*
 * cdttyclose:  close syscall for cdtty terminals..
 */
int
cdttyclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct cd18xx_softc *sc;
	struct cdtty_port *port;
	struct tty *tp;
	int channel, instance;

	channel = CD18XX_CHANNEL(dev);
	instance = CD18XX_INSTANCE(dev);

	/* ensure instance is valid */
	if (instance >= clcd_cd.cd_ndevs)
		return (ENXIO);

	/* get softc and port */
	sc = device_lookup_private(&clcd_cd, instance);
	if (sc == NULL)
		return (ENXIO);
	port = &sc->sc_ports[channel];

	tp = port->p_tty;

	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		cdtty_shutdown(sc, port);
	}

	return (0);
}

/*
 * cdttyread:  read syscall for cdtty terminals..
 */
int
cdttyread(dev_t dev, struct uio *uio, int flag)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(dev));
	struct cdtty_port *port = &sc->sc_ports[CD18XX_CHANNEL(dev)];
	struct tty *tp = port->p_tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

/*
 * cdttywrite:  write syscall for cdtty terminals..
 */
int
cdttywrite(dev_t dev, struct uio *uio, int flag)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(dev));
	struct cdtty_port *port = &sc->sc_ports[CD18XX_CHANNEL(dev)];
	struct tty *tp = port->p_tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
cdttypoll(dev_t dev, int events, struct lwp *l)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(dev));
	struct cdtty_port *port = &sc->sc_ports[CD18XX_CHANNEL(dev)];
	struct tty *tp = port->p_tty;

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

/*
 * cdttytty:  return a pointer to our (cdtty) tp.
 */
struct tty *
cdttytty(dev_t dev)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(dev));
	struct cdtty_port *port = &sc->sc_ports[CD18XX_CHANNEL(dev)];

	return (port->p_tty);
}

/*
 * cdttyioctl:  ioctl syscall for cdtty terminals..
 */
int
cdttyioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(dev));
	struct cdtty_port *port = &sc->sc_ports[CD18XX_CHANNEL(dev)];
	struct tty *tp = port->p_tty;
	int error, s;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	s = splserial();

	switch (cmd) {
	case TIOCSBRK:
		cdtty_break(sc, port, 1);
		break;

	case TIOCCBRK:
		cdtty_break(sc, port, 0);
		break;

	case TIOCSDTR:
		cdtty_modem(sc, port, 1);
		break;

	case TIOCCDTR:
		cdtty_modem(sc, port, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = port->p_swflags;
		break;

	case TIOCSFLAGS:
		error = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);
		if (error)
			return (error);
		port->p_swflags = *(int *)data;
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMGET:
	default:
		return (EPASSTHROUGH);
	}

	splx(s);
	return (0);
}

/*
 * Start or restart transmission.
 */
static void
cdttystart(struct tty *tp)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(tp->t_dev));
	struct cdtty_port *p = &sc->sc_ports[CD18XX_CHANNEL(tp->t_dev)];
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (p->p_tx_stopped)
		goto out;

	if (!ttypull(tp))
		goto out;

	/* Grab the first contiguous region of buffer space. */
	{
		u_char *tba;
		int tbc;

		tba = tp->t_outq.c_cf;
		tbc = ndqb(&tp->t_outq, 0);

		(void)splserial();

		p->p_tba = tba;
		p->p_tbc = tbc;
	}

	SET(tp->t_state, TS_BUSY);
	p->p_tx_busy = 1;

	/* turn on tx interrupts */
	if ((p->p_srer & CD18xx_SRER_Tx) == 0) {
		cd18xx_set_car(sc, CD18XX_CHANNEL(tp->t_dev));
		SET(p->p_srer, CD18xx_SRER_Tx);
		cd18xx_write(sc, CD18xx_SRER, p->p_srer);
	}

	/*
	 * Now bail; we can't actually transmit bytes until we're in a
	 * transmit interrupt service routine.
	 */
out:
	splx(s);
	return;
}

/*
 * cdttystop:  handing ^S or other stop signals, for a cdtty
 */
void
cdttystop(struct tty *tp, int flag)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(tp->t_dev));
	struct cdtty_port *p = &sc->sc_ports[CD18XX_CHANNEL(tp->t_dev)];
	int s;

	s = splserial();
	if (ISSET(tp->t_state, TS_BUSY)) {
		/* Stop transmitting at the next chunk. */
		p->p_tbc = 0;
		p->p_heldtbc = 0;
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
}

/*
 * load a channel's registers.
 */
void
cdtty_loadchannelregs(struct cd18xx_softc *sc, struct cdtty_port *p)
{

	cd18xx_set_car(sc, CD18XX_CHANNEL(p->p_tty->t_dev));
	cd18xx_write(sc, CD18xx_SRER, p->p_srer);
	cd18xx_write(sc, CD18xx_MSVR, p->p_msvr_active = p->p_msvr);
	cd18xx_write(sc, CD18xx_COR1, p->p_cor1);
	cd18xx_write(sc, CD18xx_COR2, p->p_cor2);
	cd18xx_write(sc, CD18xx_COR3, p->p_cor3);
	/*
	 * COR2 and COR3 change commands are not required here for
	 * the CL-CD1865 but we do them anyway for simplicity.
	 */
	cd18xx_write(sc, CD18xx_CCR, CD18xx_CCR_CORCHG |
				     CD18xx_CCR_CORCHG_COR1 |
				     CD18xx_CCR_CORCHG_COR2 |
				     CD18xx_CCR_CORCHG_COR3);
	cd18xx_write(sc, CD18xx_RBPRH, p->p_rbprh);
	cd18xx_write(sc, CD18xx_RBPRL, p->p_rbprl);
	cd18xx_write(sc, CD18xx_TBPRH, p->p_tbprh);
	cd18xx_write(sc, CD18xx_TBPRL, p->p_tbprl);
	if (cd18xx_wait_ccr(sc)) {
		DPRINTF(CDD_INFO,
		    ("%s: cdtty_loadchannelregs ccr wait timed out\n",
		    device_xname(sc->sc_dev)));
	}
	cd18xx_write(sc, CD18xx_CCR, p->p_chanctl);
}

/*
 * Set tty parameters from termios.
 * XXX - Should just copy the whole termios after
 * making sure all the changes could be done.
 */
static int
cdttyparam(struct tty *tp, struct termios *t)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(tp->t_dev));
	struct cdtty_port *p = &sc->sc_ports[CD18XX_CHANNEL(tp->t_dev)];
	int s;

	/* Check requested parameters. */
	if (t->c_ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(p->p_swflags, TIOCFLAG_SOFTCAR)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 */
	s = splserial();

	/*
	 * Copy across the size, parity and stop bit info.
	 */
	switch (t->c_cflag & CSIZE) {
	case CS5:
		p->p_cor1 = CD18xx_COR1_CS5;
		break;
	case CS6:
		p->p_cor1 = CD18xx_COR1_CS6;
		break;
	case CS7:
		p->p_cor1 = CD18xx_COR1_CS7;
		break;
	default:
		p->p_cor1 = CD18xx_COR1_CS8;
		break;
	}
	if (ISSET(t->c_cflag, PARENB)) {
		SET(p->p_cor1, CD18xx_COR1_PARITY_NORMAL);
		if (ISSET(t->c_cflag, PARODD))
			SET(p->p_cor1, CD18xx_COR1_PARITY_ODD);
	}
	if (!ISSET(t->c_iflag, INPCK))
		SET(p->p_cor1, CD18xx_COR1_IGNORE);
	if (ISSET(t->c_cflag, CSTOPB))
		SET(p->p_cor1, CD18xx_COR1_STOPBIT_2);

	/*
	 * If we're not in a mode that assumes a connection is present, then
	 * ignore carrier changes.
	 */
	if (ISSET(t->c_cflag, CLOCAL | MDMBUF))
		p->p_msvr_dcd = 0;
	else
		p->p_msvr_dcd = CD18xx_MSVR_CD;

	/*
	 * Set the flow control pins depending on the current flow control
	 * mode.
	 */
	if (ISSET(t->c_cflag, CRTSCTS)) {
		p->p_mcor1_dtr = CD18xx_MCOR1_DTR;
		p->p_msvr_rts = CD18xx_MSVR_RTS;
		p->p_msvr_cts = CD18xx_MSVR_CTS;
		p->p_cor2 = CD18xx_COR2_RTSAOE|CD18xx_COR2_CTSAE;
	} else if (ISSET(t->c_cflag, MDMBUF)) {
		/*
		 * For DTR/DCD flow control, make sure we don't toggle DTR for
		 * carrier detection.
		 */
		p->p_mcor1_dtr = 0;
		p->p_msvr_rts = CD18xx_MSVR_DTR;
		p->p_msvr_cts = CD18xx_MSVR_CD;
		p->p_cor2 = 0;
	} else {
		/*
		 * If no flow control, then always set RTS.  This will make
		 * the other side happy if it mistakenly thinks we're doing
		 * RTS/CTS flow control.
		 */
		p->p_mcor1_dtr = CD18xx_MSVR_DTR;
		p->p_msvr_rts = 0;
		p->p_msvr_cts = 0;
		p->p_cor2 = 0;
	}
	p->p_msvr_mask = p->p_msvr_cts | p->p_msvr_dcd;

	/*
	 * Set the FIFO threshold based on the receive speed.
	 *
	 *  * If it's a low speed, it's probably a mouse or some other
	 *    interactive device, so set the threshold low.
	 *  * If it's a high speed, trim the trigger level down to prevent
	 *    overflows.
	 *  * Otherwise set it a bit higher.
	 */
	p->p_cor3 = (t->c_ospeed <= 1200 ? 1 : t->c_ospeed <= 38400 ? 8 : 4);

#define PORT_RATE(o, s)	\
	(((((o) + (s)/2) / (s)) + CD18xx_xBRPR_TPC/2) / CD18xx_xBRPR_TPC)
	/* Compute BPS for the requested speeds */
	if (t->c_ospeed) {
		u_int32_t tbpr = PORT_RATE(sc->sc_osc, t->c_ospeed);

		if (tbpr == 0 || tbpr > 0xffff)
			return (EINVAL);

		p->p_tbprh = tbpr >> 8;
		p->p_tbprl = tbpr & 0xff;
	}

	if (t->c_ispeed) {
		u_int32_t rbpr = PORT_RATE(sc->sc_osc, t->c_ispeed);

		if (rbpr == 0 || rbpr > 0xffff)
			return (EINVAL);

		p->p_rbprh = rbpr >> 8;
		p->p_rbprl = rbpr & 0xff;
	}

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	if (!p->p_heldchange) {
		if (p->p_tx_busy) {
			p->p_heldtbc = p->p_tbc;
			p->p_tbc = 0;
			p->p_heldchange = 1;
		} else
			cdtty_loadchannelregs(sc, p);
	}

	if (!ISSET(t->c_cflag, CHWFLOW)) {
		/* Disable the high water mark. */
		p->p_r_hiwat = 0;
		p->p_r_lowat = 0;
		if (ISSET(p->p_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(p->p_rx_flags, RX_TTY_OVERFLOWED);
			softint_schedule(sc->sc_si);
		}
		if (ISSET(p->p_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
			CLR(p->p_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
			cdtty_hwiflow(sc, p);
		}
	} else {
		p->p_r_hiwat = cdtty_rbuf_hiwat;
		p->p_r_lowat = cdtty_rbuf_lowat;
	}

	splx(s);

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	(void) (*tp->t_linesw->l_modem)(tp, ISSET(p->p_msvr, CD18xx_MSVR_CD));

	if (!ISSET(t->c_cflag, CHWFLOW)) {
		if (p->p_tx_stopped) {
			p->p_tx_stopped = 0;
			cdttystart(tp);
		}
	}

	return (0);
}

static void
cdtty_break(struct cd18xx_softc *sc, struct cdtty_port *p, int onoff)
{

	/* tell tx intr handler we need a break */
	p->p_needbreak = !!onoff;

	/* turn on tx interrupts if break has changed */
	if (p->p_needbreak != p->p_break)
		SET(p->p_srer, CD18xx_SRER_Tx);

	if (!p->p_heldchange) {
		if (p->p_tx_busy) {
			p->p_heldtbc = p->p_tbc;
			p->p_tbc = 0;
			p->p_heldchange = 1;
		} else
			cdtty_loadchannelregs(sc, p);
	}
}

/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
cdtty_modem(struct cd18xx_softc *sc, struct cdtty_port *p, int onoff)
{

	if (p->p_mcor1_dtr == 0)
		return;

	if (onoff)
		CLR(p->p_mcor1, p->p_mcor1_dtr);
	else
		SET(p->p_mcor1, p->p_mcor1_dtr);

	if (!p->p_heldchange) {
		if (p->p_tx_busy) {
			p->p_heldtbc = p->p_tbc;
			p->p_tbc = 0;
			p->p_heldchange = 1;
		} else
			cdtty_loadchannelregs(sc, p);
	}
}

/*
 * Try to block or unblock input using hardware flow-control.
 * This is called by kern/tty.c if MDMBUF|CRTSCTS is set, and
 * if this function returns non-zero, the TS_TBLOCK flag will
 * be set or cleared according to the "block" arg passed.
 */
int
cdttyhwiflow(struct tty *tp, int block)
{
	struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, CD18XX_INSTANCE(tp->t_dev));
	struct cdtty_port *p = &sc->sc_ports[CD18XX_CHANNEL(tp->t_dev)];
	int s;

	if (p->p_msvr_rts == 0)
		return (0);

	s = splserial();
	if (block) {
		if (!ISSET(p->p_rx_flags, RX_TTY_BLOCKED)) {
			SET(p->p_rx_flags, RX_TTY_BLOCKED);
			cdtty_hwiflow(sc, p);
		}
	} else {
		if (ISSET(p->p_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(p->p_rx_flags, RX_TTY_OVERFLOWED);
			softint_schedule(sc->sc_si);
		}
		if (ISSET(p->p_rx_flags, RX_TTY_BLOCKED)) {
			CLR(p->p_rx_flags, RX_TTY_BLOCKED);
			cdtty_hwiflow(sc, p);
		}
	}
	splx(s);
	return (1);
}

/*
 * Internal version of cdttyhwiflow, called at cdtty's priority.
 */
static void
cdtty_hwiflow(struct cd18xx_softc *sc, struct cdtty_port *p)
{

	if (p->p_msvr_rts == 0)
		return;

	if (ISSET(p->p_rx_flags, RX_ANY_BLOCK)) {
		CLR(p->p_msvr, p->p_msvr_rts);
		CLR(p->p_msvr_active, p->p_msvr_rts);
	} else {
		SET(p->p_msvr, p->p_msvr_rts);
		SET(p->p_msvr_active, p->p_msvr_rts);
	}
	cd18xx_set_car(sc, CD18XX_CHANNEL(p->p_tty->t_dev));
	cd18xx_write(sc, CD18xx_MSVR, p->p_msvr_active);
}

/*
 * indiviual interrupt routines.
 */

/*
 * this is the number of interrupts allowed, total.  set it to 0
 * to allow unlimited interrpts
 */
#define INTR_MAX_ALLOWED	0

#if INTR_MAX_ALLOWED == 0
#define GOTINTR(sc, p)	/* nothing */
#else
int intrcount;
#define GOTINTR(sc, p)	\
do { \
	if (intrcount++ == INTR_MAX_ALLOWED) { \
		CLR(p->p_srer, CD18xx_SRER_Tx); \
		cd18xx_write(sc, CD18xx_SRER, p->p_srer); \
	} \
	DPRINTF(CDD_INTR, (", intrcount %d srer %x", intrcount, p->p_srer)); \
} while (0)
#endif

/* receiver interrupt */
static inline void
cd18xx_rint(struct cd18xx_softc *sc, int *ns)
{
	struct cdtty_port *p;
	u_int channel, count;
	u_char *put, *end;
	u_int cc;

	/* work out the channel and softc */
	channel = cd18xx_get_gscr1_channel(sc);
	p = &sc->sc_ports[channel];
	DPRINTF(CDD_INTR, ("%s: rint: channel %d", device_xname(sc->sc_dev), channel));
	GOTINTR(sc, p);

	end = p->p_ebuf;
	put = p->p_rbput;
	cc = p->p_rbavail;

	/* read as many bytes as necessary */
	count = cd18xx_read(sc, CD18xx_RDCR);
	DPRINTF(CDD_INTR, (", %d bytes available: ", count));

	while (cc > 0 && count > 0) {
		u_char rcsr = cd18xx_read(sc, CD18xx_RCSR);

		put[0] = cd18xx_read(sc, CD18xx_RDR);
		put[1] = rcsr;

		if (rcsr)
			*ns = 1;

		put += 2;
		if (put >= end)
			put = p->p_rbuf;

		DPRINTF(CDD_INTR, ("."));
		cc--;
		count--;
	}

	DPRINTF(CDD_INTR, (" finished reading"));

	/*
	 * Current string of incoming characters ended because
	 * no more data was available or we ran out of space.
	 * If we're out of space, turn off receive interrupts.
	 */
	p->p_rbput = put;
	p->p_rbavail = cc;
	if (!ISSET(p->p_rx_flags, RX_TTY_OVERFLOWED)) {
		p->p_rx_ready = 1;
	}

	/*
	 * If we're out of space, disable receive interrupts
	 * until the queue has drained a bit.
	 */
	if (!cc) {
		SET(p->p_rx_flags, RX_IBUF_OVERFLOWED);
		CLR(p->p_srer, CD18xx_SRER_Rx |
			       CD18xx_SRER_RxSC |
			       CD18xx_SRER_CD);
		cd18xx_write(sc, CD18xx_SRER, p->p_srer);
	}

	/* finish the interrupt transaction with the IC */
	cd18xx_write(sc, CD18xx_EOSRR, 0);
	DPRINTF(CDD_INTR, (", done\n"));
}

/*
 * transmitter interrupt
 *
 * note this relys on the fact that we allow the transmitter FIFO to
 * drain completely
 */
static inline void
cd18xx_tint(struct cd18xx_softc *sc, int *ns)
{
	struct cdtty_port *p;
	u_int channel;

	/* work out the channel and softc */
	channel = cd18xx_get_gscr1_channel(sc);
	p = &sc->sc_ports[channel];
	DPRINTF(CDD_INTR, ("%s: tint: channel %d", device_xname(sc->sc_dev),
	    channel));
	GOTINTR(sc, p);

	/* if the current break condition is wrong, fix it */
	if (p->p_break != p->p_needbreak) {
		u_char buf[2];

		DPRINTF(CDD_INTR, (", changing break to %d", p->p_needbreak));

		/* turn on ETC processing */
		cd18xx_write(sc, CD18xx_COR2, p->p_cor2 | CD18xx_COR2_ETC);

		buf[0] = CD18xx_TDR_ETC_BYTE;
		buf[1] = p->p_needbreak ? CD18xx_TDR_BREAK_BYTE :
					    CD18xx_TDR_NOBREAK_BYTE;
		cd18xx_write_multi(sc, CD18xx_TDR, buf, 2);

		p->p_break = p->p_needbreak;

		/* turn off ETC processing */
		cd18xx_write(sc, CD18xx_COR2, p->p_cor2);
	}

	/*
	 * If we've delayed a parameter change, do it now, and restart
	 * output.
	 */
	if (p->p_heldchange) {
		cdtty_loadchannelregs(sc, p);
		p->p_heldchange = 0;
		p->p_tbc = p->p_heldtbc;
		p->p_heldtbc = 0;
	}

	/* Output the next chunk of the contiguous buffer, if any. */
	if (p->p_tbc > 0) {
		int n;

		n = p->p_tbc;
		if (n > 8) /* write up to 8 entries */
			n = 8;
		DPRINTF(CDD_INTR, (", writing %d bytes to TDR", n));
		cd18xx_write_multi(sc, CD18xx_TDR, p->p_tba, n);
		p->p_tbc -= n;
		p->p_tba += n;
	}

	/* Disable transmit completion interrupts if we ran out of bytes. */
	if (p->p_tbc == 0) {
		/* Note that Tx interrupts should already be enabled */
		if (ISSET(p->p_srer, CD18xx_SRER_Tx)) {
			DPRINTF(CDD_INTR, (", disabling tx interrupts"));
			CLR(p->p_srer, CD18xx_SRER_Tx);
			cd18xx_write(sc, CD18xx_SRER, p->p_srer);
		}
		if (p->p_tx_busy) {
			p->p_tx_busy = 0;
			p->p_tx_done = 1;
		}
	}
	*ns = 1;

	/* finish the interrupt transaction with the IC */
	cd18xx_write(sc, CD18xx_EOSRR, 0);
	DPRINTF(CDD_INTR, (", done\n"));
}

/* modem signal change interrupt */
static inline void
cd18xx_mint(struct cd18xx_softc *sc, int *ns)
{
	struct cdtty_port *p;
	u_int channel;
	u_char msvr, delta;

	/* work out the channel and softc */
	channel = cd18xx_get_gscr1_channel(sc);
	p = &sc->sc_ports[channel];
	DPRINTF(CDD_INTR, ("%s: mint: channel %d", device_xname(sc->sc_dev), channel));
	GOTINTR(sc, p);

	/*
	 * We ignore the MCR register, and handle detecting deltas
	 * via software, like many other serial drivers.
	 */
	msvr = cd18xx_read(sc, CD18xx_MSVR);
	delta = msvr ^ p->p_msvr;
	DPRINTF(CDD_INTR, (", msvr %d", msvr));

	/*
	 * Process normal status changes
	 */
	if (ISSET(delta, p->p_msvr_mask)) {
		SET(p->p_msvr_delta, delta);

		DPRINTF(CDD_INTR, (", status changed delta %d", delta));
		/*
		 * Stop output immediately if we lose the output
		 * flow control signal or carrier detect.
		 */
		if (ISSET(~msvr, p->p_msvr_mask)) {
			p->p_tbc = 0;
			p->p_heldtbc = 0;
			/* Stop modem interrupt processing */
		}
		p->p_st_check = 1;
		*ns = 1;
	}

	/* reset the modem signal register */
	cd18xx_write(sc, CD18xx_MCR, 0);

	/* finish the interrupt transaction with the IC */
	cd18xx_write(sc, CD18xx_EOSRR, 0);
	DPRINTF(CDD_INTR, (", done\n"));
}

/*
 * hardware interrupt routine.  call the relevant interrupt routines until
 * no interrupts are pending.
 *
 * note:  we do receive interrupts before all others (as we'd rather lose
 * a chance to transmit, than lose a character).  and we do transmit
 * interrupts before modem interrupts.
 *
 * we have to traverse all of the cd18xx's attached, unfortunately.
 */
int
cd18xx_hardintr(void *v)
{
	int i, rv = 0;
	u_char ack;

	DPRINTF(CDD_INTR, ("cd18xx_hardintr (ndevs %d):\n", clcd_cd.cd_ndevs));
	for (i = 0; i < clcd_cd.cd_ndevs; i++)
	{
		struct cd18xx_softc *sc = device_lookup_private(&clcd_cd, i);
		int status, ns = 0;
		int count = 1;	/* process only 1 interrupts at a time for now */

		if (sc == NULL)
			continue;

		DPRINTF(CDD_INTR, ("%s:", device_xname(sc->sc_dev)));
		while (count-- &&
		    (status = (cd18xx_read(sc, CD18xx_SRSR) &
		     CD18xx_SRSR_PENDING))) {
			rv = 1;

			DPRINTF(CDD_INTR, (" status %x:", status));
			if (ISSET(status, CD18xx_SRSR_RxPEND)) {
				ack = (*sc->sc_ackfunc)(sc->sc_ackfunc_arg,
				    CD18xx_INTRACK_RxINT);
				DPRINTF(CDD_INTR, (" rx: ack1 %x\n", ack));
				cd18xx_rint(sc, &ns);
			}
			if (ISSET(status, CD18xx_SRSR_TxPEND)) {
				ack = (*sc->sc_ackfunc)(sc->sc_ackfunc_arg,
				    CD18xx_INTRACK_TxINT);
				DPRINTF(CDD_INTR, (" tx: ack1 %x\n", ack));
				cd18xx_tint(sc, &ns);

			}
			if (ISSET(status, CD18xx_SRSR_MxPEND)) {
				ack = (*sc->sc_ackfunc)(sc->sc_ackfunc_arg,
				    CD18xx_INTRACK_MxINT);
				DPRINTF(CDD_INTR, (" mx: ack1 %x\n", ack));
				cd18xx_mint(sc, &ns);
			}
		}
		if (ns)
			softint_schedule(sc->sc_si);
	}

	return (rv);
}

/*
 * software interrupt
 */

void
cdtty_rxsoft(struct cd18xx_softc *sc, struct cdtty_port *p, struct tty *tp)
{
	u_char *get, *end;
	u_int cc, scc;
	u_char rcsr;
	int code;
	int s;

	end = p->p_ebuf;
	get = p->p_rbget;
	scc = cc = cdtty_rbuf_size - p->p_rbavail;

	if (cc == cdtty_rbuf_size) {
		p->p_floods++;
#if 0
		if (p->p_errors++ == 0)
			callout_reset(&p->p_diag_callout, 60 * hz,
			    cdttydiag, p);
#endif
	}

	while (cc) {
		code = get[0];
		rcsr = get[1];
		if (ISSET(rcsr, CD18xx_RCSR_OVERRUNERR | CD18xx_RCSR_BREAK |
				CD18xx_RCSR_FRAMERR | CD18xx_RCSR_PARITYERR)) {
			if (ISSET(rcsr, CD18xx_RCSR_OVERRUNERR)) {
				p->p_overflows++;
#if 0
				if (p->p_errors++ == 0)
					callout_reset(&p->p_diag_callout,
					    60 * hz, cdttydiag, p);
#endif
			}
			if (ISSET(rcsr, CD18xx_RCSR_BREAK|CD18xx_RCSR_FRAMERR))
				SET(code, TTY_FE);
			if (ISSET(rcsr, CD18xx_RCSR_PARITYERR))
				SET(code, TTY_PE);
		}
		if ((*tp->t_linesw->l_rint)(code, tp) == -1) {
			/*
			 * The line discipline's buffer is out of space.
			 */
			if (!ISSET(p->p_rx_flags, RX_TTY_BLOCKED)) {
				/*
				 * We're either not using flow control, or the
				 * line discipline didn't tell us to block for
				 * some reason.  Either way, we have no way to
				 * know when there's more space available, so
				 * just drop the rest of the data.
				 */
				get += cc << 1;
				if (get >= end)
					get -= cdtty_rbuf_size << 1;
				cc = 0;
			} else {
				/*
				 * Don't schedule any more receive processing
				 * until the line discipline tells us there's
				 * space available (through cdttyhwiflow()).
				 * Leave the rest of the data in the input
				 * buffer.
				 */
				SET(p->p_rx_flags, RX_TTY_OVERFLOWED);
			}
			break;
		}
		get += 2;
		if (get >= end)
			get = p->p_rbuf;
		cc--;
	}

	if (cc != scc) {
		p->p_rbget = get;
		s = splserial();

		cc = p->p_rbavail += scc - cc;
		/* Buffers should be ok again, release possible block. */
		if (cc >= p->p_r_lowat) {
			if (ISSET(p->p_rx_flags, RX_IBUF_OVERFLOWED)) {
				CLR(p->p_rx_flags, RX_IBUF_OVERFLOWED);
				cd18xx_set_car(sc, CD18XX_CHANNEL(tp->t_dev));
				SET(p->p_srer, CD18xx_SRER_Rx |
					       CD18xx_SRER_RxSC |
					       CD18xx_SRER_CD);
				cd18xx_write(sc, CD18xx_SRER, p->p_srer);
			}
			if (ISSET(p->p_rx_flags, RX_IBUF_BLOCKED)) {
				CLR(p->p_rx_flags, RX_IBUF_BLOCKED);
				cdtty_hwiflow(sc, p);
			}
		}
		splx(s);
	}
}

void
cdtty_txsoft(struct cd18xx_softc *sc, struct cdtty_port *p, struct tty *tp)
{

	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, (int)(p->p_tba - tp->t_outq.c_cf));
	(*tp->t_linesw->l_start)(tp);
}

void
cdtty_stsoft(struct cd18xx_softc *sc, struct cdtty_port *p, struct tty *tp)
{
	u_char msvr, delta;
	int s;

	s = splserial();
	msvr = p->p_msvr;
	delta = p->p_msvr_delta;
	p->p_msvr_delta = 0;
	splx(s);

	if (ISSET(delta, p->p_msvr_dcd)) {
		/*
		 * Inform the tty layer that carrier detect changed.
		 */
		(void) (*tp->t_linesw->l_modem)(tp, ISSET(msvr, CD18xx_MSVR_CD));
	}

	if (ISSET(delta, p->p_msvr_cts)) {
		/* Block or unblock output according to flow control. */
		if (ISSET(msvr, p->p_msvr_cts)) {
			p->p_tx_stopped = 0;
			(*tp->t_linesw->l_start)(tp);
		} else {
			p->p_tx_stopped = 1;
		}
	}
}

void
cd18xx_softintr(void *v)
{
	struct cd18xx_softc *sc = v;
	struct cdtty_port *p;
	struct tty *tp;
	int i;

	for (i = 0; i < 8; i++) {
		p = &sc->sc_ports[i];

		tp = p->p_tty;
		if (tp == NULL)
			continue;
		if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0)
			continue;

		if (p->p_rx_ready) {
			p->p_rx_ready = 0;
			cdtty_rxsoft(sc, p, tp);
		}

		if (p->p_st_check) {
			p->p_st_check = 0;
			cdtty_stsoft(sc, p, tp);
		}

		if (p->p_tx_done) {
			p->p_tx_done = 0;
			cdtty_txsoft(sc, p, tp);
		}
	}
}
