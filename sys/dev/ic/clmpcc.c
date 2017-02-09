/*	$NetBSD: clmpcc.c,v 1.51 2014/11/15 19:18:18 christos Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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
 * Cirrus Logic CD2400/CD2401 Four Channel Multi-Protocol Comms. Controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clmpcc.c,v 1.51 2014/11/15 19:18:18 christos Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <sys/intr.h>

#include <sys/bus.h>
#include <machine/param.h>

#include <dev/ic/clmpccreg.h>
#include <dev/ic/clmpccvar.h>
#include <dev/cons.h>


#if defined(CLMPCC_ONLY_BYTESWAP_LOW) && defined(CLMPCC_ONLY_BYTESWAP_HIGH)
#error	"CLMPCC_ONLY_BYTESWAP_LOW and CLMPCC_ONLY_BYTESWAP_HIGH are mutually exclusive."
#endif


static int	clmpcc_init(struct clmpcc_softc *sc);
static void	clmpcc_shutdown(struct clmpcc_chan *);
static int	clmpcc_speed(struct clmpcc_softc *, speed_t, int *, int *);
static int	clmpcc_param(struct tty *, struct termios *);
static void	clmpcc_set_params(struct clmpcc_chan *);
static void	clmpcc_start(struct tty *);
static int 	clmpcc_modem_control(struct clmpcc_chan *, int, int);

#define	CLMPCCUNIT(x)		(TTUNIT(x) & ~0x3)	// XXX >> 2? 
#define	CLMPCCCHAN(x)		(TTUNIT(x) & 0x3)
#define	CLMPCCDIALOUT(x)	TTDIALOUT(x)

/*
 * These should be in a header file somewhere...
 */
#define	ISCLR(v, f)	(((v) & (f)) == 0)

extern struct cfdriver clmpcc_cd;

dev_type_open(clmpccopen);
dev_type_close(clmpccclose);
dev_type_read(clmpccread);
dev_type_write(clmpccwrite);
dev_type_ioctl(clmpccioctl);
dev_type_stop(clmpccstop);
dev_type_tty(clmpcctty);
dev_type_poll(clmpccpoll);

const struct cdevsw clmpcc_cdevsw = {
	.d_open = clmpccopen,
	.d_close = clmpccclose,
	.d_read = clmpccread,
	.d_write = clmpccwrite,
	.d_ioctl = clmpccioctl,
	.d_stop = clmpccstop,
	.d_tty = clmpcctty,
	.d_poll = clmpccpoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

/*
 * Make this an option variable one can patch.
 */
u_int clmpcc_ibuf_size = CLMPCC_RING_SIZE;


/*
 * Things needed when the device is used as a console
 */
static struct clmpcc_softc *cons_sc = NULL;
static int cons_chan;
static int cons_rate;

static int	clmpcc_common_getc(struct clmpcc_softc *, int);
static void	clmpcc_common_putc(struct clmpcc_softc *, int, int);
int		clmpcccngetc(dev_t);
void		clmpcccnputc(dev_t, int);


/*
 * Convenience functions, inlined for speed
 */
#define	integrate   static inline
integrate u_int8_t  clmpcc_rdreg(struct clmpcc_softc *, u_int);
integrate void      clmpcc_wrreg(struct clmpcc_softc *, u_int, u_int);
integrate u_int8_t  clmpcc_rdreg_odd(struct clmpcc_softc *, u_int);
integrate void      clmpcc_wrreg_odd(struct clmpcc_softc *, u_int, u_int);
integrate void      clmpcc_wrtx_multi(struct clmpcc_softc *, u_int8_t *,
					u_int);
integrate u_int8_t  clmpcc_select_channel(struct clmpcc_softc *, u_int);
integrate void      clmpcc_channel_cmd(struct clmpcc_softc *,int,int);
integrate void      clmpcc_enable_transmitter(struct clmpcc_chan *);

#define clmpcc_rd_msvr(s)	clmpcc_rdreg_odd(s,CLMPCC_REG_MSVR)
#define clmpcc_wr_msvr(s,r,v)	clmpcc_wrreg_odd(s,r,v)
#define clmpcc_wr_pilr(s,r,v)	clmpcc_wrreg_odd(s,r,v)
#define clmpcc_rd_rxdata(s)	clmpcc_rdreg_odd(s,CLMPCC_REG_RDR)
#define clmpcc_wr_txdata(s,v)	clmpcc_wrreg_odd(s,CLMPCC_REG_TDR,v)


integrate u_int8_t
clmpcc_rdreg(struct clmpcc_softc *sc, u_int offset)
{
#if !defined(CLMPCC_ONLY_BYTESWAP_LOW) && !defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= sc->sc_byteswap;
#elif defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= CLMPCC_BYTESWAP_HIGH;
#endif
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, offset);
}

integrate void
clmpcc_wrreg(struct clmpcc_softc *sc, u_int offset, u_int val)
{
#if !defined(CLMPCC_ONLY_BYTESWAP_LOW) && !defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= sc->sc_byteswap;
#elif defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= CLMPCC_BYTESWAP_HIGH;
#endif
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, offset, val);
}

integrate u_int8_t
clmpcc_rdreg_odd(struct clmpcc_softc *sc, u_int offset)
{
#if !defined(CLMPCC_ONLY_BYTESWAP_LOW) && !defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (sc->sc_byteswap & 2);
#elif defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (CLMPCC_BYTESWAP_HIGH & 2);
#endif
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, offset);
}

integrate void
clmpcc_wrreg_odd(struct clmpcc_softc *sc, u_int offset, u_int val)
{
#if !defined(CLMPCC_ONLY_BYTESWAP_LOW) && !defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (sc->sc_byteswap & 2);
#elif defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (CLMPCC_BYTESWAP_HIGH & 2);
#endif
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, offset, val);
}

integrate void
clmpcc_wrtx_multi(struct clmpcc_softc *sc, u_int8_t *buff, u_int count)
{
	u_int offset = CLMPCC_REG_TDR;

#if !defined(CLMPCC_ONLY_BYTESWAP_LOW) && !defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (sc->sc_byteswap & 2);
#elif defined(CLMPCC_ONLY_BYTESWAP_HIGH)
	offset ^= (CLMPCC_BYTESWAP_HIGH & 2);
#endif
	bus_space_write_multi_1(sc->sc_iot, sc->sc_ioh, offset, buff, count);
}

integrate u_int8_t
clmpcc_select_channel(struct clmpcc_softc *sc, u_int new_chan)
{
	u_int old_chan = clmpcc_rdreg_odd(sc, CLMPCC_REG_CAR);

	clmpcc_wrreg_odd(sc, CLMPCC_REG_CAR, new_chan);

	return old_chan;
}

integrate void
clmpcc_channel_cmd(struct clmpcc_softc *sc, int chan, int cmd)
{
	int i;

	for (i = 5000; i; i--) {
		if ( clmpcc_rdreg(sc, CLMPCC_REG_CCR) == 0 )
			break;
		delay(1);
	}

	if ( i == 0 )
		printf("%s: channel %d command timeout (idle)\n",
			device_xname(sc->sc_dev), chan);

	clmpcc_wrreg(sc, CLMPCC_REG_CCR, cmd);
}

integrate void
clmpcc_enable_transmitter(struct clmpcc_chan *ch)
{
	u_int old;
	int s;

	old = clmpcc_select_channel(ch->ch_sc, ch->ch_car);

	s = splserial();
	clmpcc_wrreg(ch->ch_sc, CLMPCC_REG_IER,
		clmpcc_rdreg(ch->ch_sc, CLMPCC_REG_IER) | CLMPCC_IER_TX_EMPTY);
	SET(ch->ch_tty->t_state, TS_BUSY);
	splx(s);

	clmpcc_select_channel(ch->ch_sc, old);
}

static int
clmpcc_speed(struct clmpcc_softc *sc, speed_t speed, int *cor, int *bpr)
{
	int c, co, br;

	for (co = 0, c = 8; c <= 2048; co++, c *= 4) {
		br = ((sc->sc_clk / c) / speed) - 1;
		if ( br < 0x100 ) {
			*cor = co;
			*bpr = br;
			return 0;
		}
	}

	return -1;
}

void
clmpcc_attach(struct clmpcc_softc *sc)
{
	struct clmpcc_chan *ch;
	struct tty *tp;
	int chan;

	if ( cons_sc != NULL &&
	     sc->sc_iot == cons_sc->sc_iot && sc->sc_ioh == cons_sc->sc_ioh )
		cons_sc = sc;

	/* Initialise the chip */
	clmpcc_init(sc);

	printf(": Cirrus Logic CD240%c Serial Controller\n",
		(clmpcc_rd_msvr(sc) & CLMPCC_MSVR_PORT_ID) ? '0' : '1');

	sc->sc_softintr_cookie =
	    softint_establish(SOFTINT_SERIAL, clmpcc_softintr, sc);
	if (sc->sc_softintr_cookie == NULL)
		panic("clmpcc_attach: softintr_establish");
	memset(&(sc->sc_chans[0]), 0, sizeof(sc->sc_chans));

	for (chan = 0; chan < CLMPCC_NUM_CHANS; chan++) {
		ch = &sc->sc_chans[chan];

		ch->ch_sc = sc;
		ch->ch_car = chan;

		tp = tty_alloc();
		tp->t_oproc = clmpcc_start;
		tp->t_param = clmpcc_param;

		ch->ch_tty = tp;

		ch->ch_ibuf = malloc(clmpcc_ibuf_size * 2, M_DEVBUF, M_NOWAIT);
		if ( ch->ch_ibuf == NULL ) {
			aprint_error_dev(sc->sc_dev, "(%d): unable to allocate ring buffer\n",
		    		chan);
			return;
		}

		ch->ch_ibuf_end = &(ch->ch_ibuf[clmpcc_ibuf_size * 2]);
		ch->ch_ibuf_rd = ch->ch_ibuf_wr = ch->ch_ibuf;

		tty_attach(tp);
	}

	aprint_error_dev(sc->sc_dev, "%d channels available",
					    CLMPCC_NUM_CHANS);
	if ( cons_sc == sc ) {
		printf(", console on channel %d.\n", cons_chan);
		SET(sc->sc_chans[cons_chan].ch_flags, CLMPCC_FLG_IS_CONSOLE);
		SET(sc->sc_chans[cons_chan].ch_openflags, TIOCFLAG_SOFTCAR);
	} else
		printf(".\n");
}

static int
clmpcc_init(struct clmpcc_softc *sc)
{
	u_int tcor = 0, tbpr = 0;
	u_int rcor = 0, rbpr = 0;
	u_int msvr_rts, msvr_dtr;
	u_int ccr;
	int is_console;
	int i;

	/*
	 * All we're really concerned about here is putting the chip
	 * into a quiescent state so that it won't do anything until
	 * clmpccopen() is called. (Except the console channel.)
	 */

	/*
	 * If the chip is acting as console, set all channels to the supplied
	 * console baud rate. Otherwise, plump for 9600.
	 */
	if ( cons_sc &&
	     sc->sc_ioh == cons_sc->sc_ioh && sc->sc_iot == cons_sc->sc_iot ) {
		clmpcc_speed(sc, cons_rate, &tcor, &tbpr);
		clmpcc_speed(sc, cons_rate, &rcor, &rbpr);
		is_console = 1;
	} else {
		clmpcc_speed(sc, 9600, &tcor, &tbpr);
		clmpcc_speed(sc, 9600, &rcor, &rbpr);
		is_console = 0;
	}

	/* Allow any pending output to be sent */
	delay(10000);

	/* Send the Reset All command  to channel 0 (resets all channels!) */
	clmpcc_channel_cmd(sc, 0, CLMPCC_CCR_T0_RESET_ALL);

	delay(1000);

	/*
	 * The chip will set its firmware revision register to a non-zero
	 * value to indicate completion of reset.
	 */
	for (i = 10000; clmpcc_rdreg(sc, CLMPCC_REG_GFRCR) == 0 && i; i--)
		delay(1);

	if ( i == 0 ) {
		/*
		 * Watch out... If this chip is console, the message
		 * probably won't be sent since we just reset it!
		 */
		aprint_error_dev(sc->sc_dev, "Failed to reset chip\n");
		return -1;
	}

	for (i = 0; i < CLMPCC_NUM_CHANS; i++) {
		clmpcc_select_channel(sc, i);

		/* All interrupts are disabled to begin with */
		clmpcc_wrreg(sc, CLMPCC_REG_IER, 0);

		/* Make sure the channel interrupts on the correct vectors */
		clmpcc_wrreg(sc, CLMPCC_REG_LIVR, sc->sc_vector_base);
		clmpcc_wr_pilr(sc, CLMPCC_REG_RPILR, sc->sc_rpilr);
		clmpcc_wr_pilr(sc, CLMPCC_REG_TPILR, sc->sc_tpilr);
		clmpcc_wr_pilr(sc, CLMPCC_REG_MPILR, sc->sc_mpilr);

		/* Receive timer prescaler set to 1ms */
		clmpcc_wrreg(sc, CLMPCC_REG_TPR,
				 CLMPCC_MSEC_TO_TPR(sc->sc_clk, 1));

		/* We support Async mode only */
		clmpcc_wrreg(sc, CLMPCC_REG_CMR, CLMPCC_CMR_ASYNC);

		/* Set the required baud rate */
		clmpcc_wrreg(sc, CLMPCC_REG_TCOR, CLMPCC_TCOR_CLK(tcor));
		clmpcc_wrreg(sc, CLMPCC_REG_TBPR, tbpr);
		clmpcc_wrreg(sc, CLMPCC_REG_RCOR, CLMPCC_RCOR_CLK(rcor));
		clmpcc_wrreg(sc, CLMPCC_REG_RBPR, rbpr);

		/* Always default to 8N1 (XXX what about console?) */
		clmpcc_wrreg(sc, CLMPCC_REG_COR1, CLMPCC_COR1_CHAR_8BITS |
						  CLMPCC_COR1_NO_PARITY |
						  CLMPCC_COR1_IGNORE_PAR);

		clmpcc_wrreg(sc, CLMPCC_REG_COR2, 0);

		clmpcc_wrreg(sc, CLMPCC_REG_COR3, CLMPCC_COR3_STOP_1);

		clmpcc_wrreg(sc, CLMPCC_REG_COR4, CLMPCC_COR4_DSRzd |
						  CLMPCC_COR4_CDzd |
						  CLMPCC_COR4_CTSzd);

		clmpcc_wrreg(sc, CLMPCC_REG_COR5, CLMPCC_COR5_DSRod |
						  CLMPCC_COR5_CDod |
						  CLMPCC_COR5_CTSod |
						  CLMPCC_COR5_FLOW_NORM);

		clmpcc_wrreg(sc, CLMPCC_REG_COR6, 0);
		clmpcc_wrreg(sc, CLMPCC_REG_COR7, 0);

		/* Set the receive FIFO timeout */
		clmpcc_wrreg(sc, CLMPCC_REG_RTPRl, CLMPCC_RTPR_DEFAULT);
		clmpcc_wrreg(sc, CLMPCC_REG_RTPRh, 0);

		/* At this point, we set up the console differently */
		if ( is_console && i == cons_chan ) {
			msvr_rts = CLMPCC_MSVR_RTS;
			msvr_dtr = CLMPCC_MSVR_DTR;
			ccr = CLMPCC_CCR_T0_RX_EN | CLMPCC_CCR_T0_TX_EN;
		} else {
			msvr_rts = 0;
			msvr_dtr = 0;
			ccr = CLMPCC_CCR_T0_RX_DIS | CLMPCC_CCR_T0_TX_DIS;
		}

		clmpcc_wrreg(sc, CLMPCC_REG_MSVR_RTS, msvr_rts);
		clmpcc_wrreg(sc, CLMPCC_REG_MSVR_DTR, msvr_dtr);
		clmpcc_channel_cmd(sc, i, CLMPCC_CCR_T0_INIT | ccr);
		delay(100);
	}

	return 0;
}

static void
clmpcc_shutdown(struct clmpcc_chan *ch)
{
	int oldch;

	oldch = clmpcc_select_channel(ch->ch_sc, ch->ch_car);

	/* Turn off interrupts. */
	clmpcc_wrreg(ch->ch_sc, CLMPCC_REG_IER, 0);

	if ( ISCLR(ch->ch_flags, CLMPCC_FLG_IS_CONSOLE) ) {
		/* Disable the transmitter and receiver */
		clmpcc_channel_cmd(ch->ch_sc, ch->ch_car, CLMPCC_CCR_T0_RX_DIS |
							  CLMPCC_CCR_T0_TX_DIS);

		/* Drop RTS and DTR */
		clmpcc_modem_control(ch, TIOCM_RTS | TIOCM_DTR, DMBIS);
	}

	clmpcc_select_channel(ch->ch_sc, oldch);
}

int
clmpccopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct clmpcc_softc *sc;
	struct clmpcc_chan *ch;
	struct tty *tp;
	int oldch;
	int error;

	sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	ch = &sc->sc_chans[CLMPCCCHAN(dev)];

	tp = ch->ch_tty;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return EBUSY;

	/*
	 * Do the following iff this is a first open.
	 */
	if ( ISCLR(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0 ) {

		ttychars(tp);

		tp->t_dev = dev;
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ospeed = tp->t_ispeed = TTYDEF_SPEED;

		if ( ISSET(ch->ch_openflags, TIOCFLAG_CLOCAL) )
			SET(tp->t_cflag, CLOCAL);
		if ( ISSET(ch->ch_openflags, TIOCFLAG_CRTSCTS) )
			SET(tp->t_cflag, CRTSCTS);
		if ( ISSET(ch->ch_openflags, TIOCFLAG_MDMBUF) )
			SET(tp->t_cflag, MDMBUF);

		/*
		 * Override some settings if the channel is being
		 * used as the console.
		 */
		if ( ISSET(ch->ch_flags, CLMPCC_FLG_IS_CONSOLE) ) {
			tp->t_ospeed = tp->t_ispeed = cons_rate;
			SET(tp->t_cflag, CLOCAL);
			CLR(tp->t_cflag, CRTSCTS);
			CLR(tp->t_cflag, HUPCL);
		}

		ch->ch_control = 0;

		clmpcc_param(tp, &tp->t_termios);
		ttsetwater(tp);

		/* Clear the input ring */
		ch->ch_ibuf_rd = ch->ch_ibuf_wr = ch->ch_ibuf;

		/* Select the channel */
		oldch = clmpcc_select_channel(sc, ch->ch_car);

		/* Reset it */
		clmpcc_channel_cmd(sc, ch->ch_car, CLMPCC_CCR_T0_CLEAR |
						   CLMPCC_CCR_T0_RX_EN |
						   CLMPCC_CCR_T0_TX_EN);

		/* Enable receiver and modem change interrupts. */
		clmpcc_wrreg(sc, CLMPCC_REG_IER, CLMPCC_IER_MODEM |
						 CLMPCC_IER_RET |
						 CLMPCC_IER_RX_FIFO);

		/* Raise RTS and DTR */
		clmpcc_modem_control(ch, TIOCM_RTS | TIOCM_DTR, DMBIS);

		clmpcc_select_channel(sc, oldch);
	}

	error = ttyopen(tp, CLMPCCDIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (error)
		goto bad;

	error = (*tp->t_linesw->l_open)(dev, tp);
	if (error)
		goto bad;

	return 0;

bad:
	if ( ISCLR(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0 ) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		clmpcc_shutdown(ch);
	}

	return error;
}

int
clmpccclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct clmpcc_softc	*sc =
		device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	struct clmpcc_chan	*ch = &sc->sc_chans[CLMPCCCHAN(dev)];
	struct tty		*tp = ch->ch_tty;
	int s;

	if ( ISCLR(tp->t_state, TS_ISOPEN) )
		return 0;

	(*tp->t_linesw->l_close)(tp, flag);

	s = spltty();

	if ( ISCLR(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0 ) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		clmpcc_shutdown(ch);
	}

	ttyclose(tp);

	splx(s);

	return 0;
}

int
clmpccread(dev_t dev, struct uio *uio, int flag)
{
	struct clmpcc_softc *sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	struct tty *tp = sc->sc_chans[CLMPCCCHAN(dev)].ch_tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
clmpccwrite(dev_t dev, struct uio *uio, int flag)
{
	struct clmpcc_softc *sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	struct tty *tp = sc->sc_chans[CLMPCCCHAN(dev)].ch_tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
clmpccpoll(dev_t dev, int events, struct lwp *l)
{
	struct clmpcc_softc *sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	struct tty *tp = sc->sc_chans[CLMPCCCHAN(dev)].ch_tty;

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

struct tty *
clmpcctty(dev_t dev)
{
	struct clmpcc_softc *sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));

	return (sc->sc_chans[CLMPCCCHAN(dev)].ch_tty);
}

int
clmpccioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct clmpcc_softc *sc = device_lookup_private(&clmpcc_cd, CLMPCCUNIT(dev));
	struct clmpcc_chan *ch = &sc->sc_chans[CLMPCCCHAN(dev)];
	struct tty *tp = ch->ch_tty;
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = 0;

	switch (cmd) {
	case TIOCSBRK:
		SET(ch->ch_flags, CLMPCC_FLG_START_BREAK);
		clmpcc_enable_transmitter(ch);
		break;

	case TIOCCBRK:
		SET(ch->ch_flags, CLMPCC_FLG_END_BREAK);
		clmpcc_enable_transmitter(ch);
		break;

	case TIOCSDTR:
		clmpcc_modem_control(ch, TIOCM_DTR, DMBIS);
		break;

	case TIOCCDTR:
		clmpcc_modem_control(ch, TIOCM_DTR, DMBIC);
		break;

	case TIOCMSET:
		clmpcc_modem_control(ch, *((int *)data), DMSET);
		break;

	case TIOCMBIS:
		clmpcc_modem_control(ch, *((int *)data), DMBIS);
		break;

	case TIOCMBIC:
		clmpcc_modem_control(ch, *((int *)data), DMBIC);
		break;

	case TIOCMGET:
		*((int *)data) = clmpcc_modem_control(ch, 0, DMGET);
		break;

	case TIOCGFLAGS:
		*((int *)data) = ch->ch_openflags;
		break;

	case TIOCSFLAGS:
		error = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);
		if ( error )
			break;
		ch->ch_openflags = *((int *)data) &
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL |
			 TIOCFLAG_CRTSCTS | TIOCFLAG_MDMBUF);
		if ( ISSET(ch->ch_flags, CLMPCC_FLG_IS_CONSOLE) )
			SET(ch->ch_openflags, TIOCFLAG_SOFTCAR);
		break;

	default:
		error = EPASSTHROUGH;
		break;
	}

	return error;
}

int
clmpcc_modem_control(struct clmpcc_chan *ch, int bits, int howto)
{
	struct clmpcc_softc *sc = ch->ch_sc;
	struct tty *tp = ch->ch_tty;
	int oldch;
	int msvr;
	int rbits = 0;

	oldch = clmpcc_select_channel(sc, ch->ch_car);

	switch ( howto ) {
	case DMGET:
		msvr = clmpcc_rd_msvr(sc);

		if ( sc->sc_swaprtsdtr ) {
			rbits |= (msvr & CLMPCC_MSVR_RTS) ? TIOCM_DTR : 0;
			rbits |= (msvr & CLMPCC_MSVR_DTR) ? TIOCM_RTS : 0;
		} else {
			rbits |= (msvr & CLMPCC_MSVR_RTS) ? TIOCM_RTS : 0;
			rbits |= (msvr & CLMPCC_MSVR_DTR) ? TIOCM_DTR : 0;
		}

		rbits |= (msvr & CLMPCC_MSVR_CTS) ? TIOCM_CTS : 0;
		rbits |= (msvr & CLMPCC_MSVR_CD)  ? TIOCM_CD  : 0;
		rbits |= (msvr & CLMPCC_MSVR_DSR) ? TIOCM_DSR : 0;
		break;

	case DMSET:
		if ( sc->sc_swaprtsdtr ) {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_DTR,
					bits & TIOCM_RTS ? CLMPCC_MSVR_DTR : 0);
		    clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_RTS,
				bits & TIOCM_DTR ? CLMPCC_MSVR_RTS : 0);
		} else {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_RTS,
					bits & TIOCM_RTS ? CLMPCC_MSVR_RTS : 0);
		    clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_DTR,
				bits & TIOCM_DTR ? CLMPCC_MSVR_DTR : 0);
		}
		break;

	case DMBIS:
		if ( sc->sc_swaprtsdtr ) {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) && ISSET(bits, TIOCM_RTS) )
			clmpcc_wr_msvr(sc,CLMPCC_REG_MSVR_DTR, CLMPCC_MSVR_DTR);
		    if ( ISSET(bits, TIOCM_DTR) )
			clmpcc_wr_msvr(sc,CLMPCC_REG_MSVR_RTS, CLMPCC_MSVR_RTS);
		} else {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) && ISSET(bits, TIOCM_RTS) )
			clmpcc_wr_msvr(sc,CLMPCC_REG_MSVR_RTS, CLMPCC_MSVR_RTS);
		    if ( ISSET(bits, TIOCM_DTR) )
			clmpcc_wr_msvr(sc,CLMPCC_REG_MSVR_DTR, CLMPCC_MSVR_DTR);
		}
		break;

	case DMBIC:
		if ( sc->sc_swaprtsdtr ) {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) && ISCLR(bits, TIOCM_RTS) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_DTR, 0);
		    if ( ISCLR(bits, TIOCM_DTR) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_RTS, 0);
		} else {
		    if ( ISCLR(tp->t_cflag, CRTSCTS) && ISCLR(bits, TIOCM_RTS) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_RTS, 0);
		    if ( ISCLR(bits, TIOCM_DTR) )
			clmpcc_wr_msvr(sc, CLMPCC_REG_MSVR_DTR, 0);
		}
		break;
	}

	clmpcc_select_channel(sc, oldch);

	return rbits;
}

static int
clmpcc_param(struct tty *tp, struct termios *t)
{
	struct clmpcc_softc *sc =
	    device_lookup_private(&clmpcc_cd, CLMPCCUNIT(tp->t_dev));
	struct clmpcc_chan *ch = &sc->sc_chans[CLMPCCCHAN(tp->t_dev)];
	u_char cor;
	u_char oldch;
	int oclk = 0, obpr = 0;
	int iclk = 0, ibpr = 0;
	int s;

	/* Check requested parameters. */
	if ( t->c_ospeed && clmpcc_speed(sc, t->c_ospeed, &oclk, &obpr) < 0 )
		return EINVAL;

	if ( t->c_ispeed && clmpcc_speed(sc, t->c_ispeed, &iclk, &ibpr) < 0 )
		return EINVAL;

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if ( ISSET(ch->ch_openflags, TIOCFLAG_SOFTCAR) ||
	     ISSET(ch->ch_flags, CLMPCC_FLG_IS_CONSOLE) ) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	CLR(ch->ch_flags, CLMPCC_FLG_UPDATE_PARMS);

	/* If ospeed it zero, hangup the line */
	clmpcc_modem_control(ch, TIOCM_DTR, t->c_ospeed == 0 ? DMBIC : DMBIS);

	if ( t->c_ospeed ) {
		ch->ch_tcor = CLMPCC_TCOR_CLK(oclk);
		ch->ch_tbpr = obpr;
	} else {
		ch->ch_tcor = 0;
		ch->ch_tbpr = 0;
	}

	if ( t->c_ispeed ) {
		ch->ch_rcor = CLMPCC_RCOR_CLK(iclk);
		ch->ch_rbpr = ibpr;
	} else {
		ch->ch_rcor = 0;
		ch->ch_rbpr = 0;
	}

	/* Work out value to use for COR1 */
	cor = 0;
	if ( ISSET(t->c_cflag, PARENB) ) {
		cor |= CLMPCC_COR1_NORM_PARITY;
		if ( ISSET(t->c_cflag, PARODD) )
			cor |= CLMPCC_COR1_ODD_PARITY;
	}

	if ( ISCLR(t->c_cflag, INPCK) )
		cor |= CLMPCC_COR1_IGNORE_PAR;

	switch ( t->c_cflag & CSIZE ) {
	  case CS5:
		cor |= CLMPCC_COR1_CHAR_5BITS;
		break;

	  case CS6:
		cor |= CLMPCC_COR1_CHAR_6BITS;
		break;

	  case CS7:
		cor |= CLMPCC_COR1_CHAR_7BITS;
		break;

	  case CS8:
		cor |= CLMPCC_COR1_CHAR_8BITS;
		break;
	}

	ch->ch_cor1 = cor;

	/*
	 * The only interesting bit in COR2 is 'CTS Automatic Enable'
	 * when hardware flow control is in effect.
	 */
	ch->ch_cor2 = ISSET(t->c_cflag, CRTSCTS) ? CLMPCC_COR2_CtsAE : 0;

	/* COR3 needs to be set to the number of stop bits... */
	ch->ch_cor3 = ISSET(t->c_cflag, CSTOPB) ? CLMPCC_COR3_STOP_2 :
						  CLMPCC_COR3_STOP_1;

	/*
	 * COR4 contains the FIFO threshold setting.
	 * We adjust the threshold depending on the input speed...
	 */
	if ( t->c_ispeed <= 1200 )
		ch->ch_cor4 = CLMPCC_COR4_FIFO_LOW;
	else if ( t->c_ispeed <= 19200 )
		ch->ch_cor4 = CLMPCC_COR4_FIFO_MED;
	else
		ch->ch_cor4 = CLMPCC_COR4_FIFO_HIGH;

	/*
	 * If chip is used with CTS and DTR swapped, we can enable
	 * automatic hardware flow control.
	 */
	if ( sc->sc_swaprtsdtr && ISSET(t->c_cflag, CRTSCTS) )
		ch->ch_cor5 = CLMPCC_COR5_FLOW_NORM;
	else
		ch->ch_cor5 = 0;

	s = splserial();
	oldch = clmpcc_select_channel(sc, ch->ch_car);

	/*
	 * COR2 needs to be set immediately otherwise we might never get
	 * a Tx EMPTY interrupt to change the other parameters.
	 */
	if ( clmpcc_rdreg(sc, CLMPCC_REG_COR2) != ch->ch_cor2 )
		clmpcc_wrreg(sc, CLMPCC_REG_COR2, ch->ch_cor2);

	if ( ISCLR(ch->ch_tty->t_state, TS_BUSY) )
		clmpcc_set_params(ch);
	else
		SET(ch->ch_flags, CLMPCC_FLG_UPDATE_PARMS);

	clmpcc_select_channel(sc, oldch);

	splx(s);

	return 0;
}

static void
clmpcc_set_params(struct clmpcc_chan *ch)
{
	struct clmpcc_softc *sc = ch->ch_sc;
	u_char r1;
	u_char r2;

	if ( ch->ch_tcor || ch->ch_tbpr ) {
		r1 = clmpcc_rdreg(sc, CLMPCC_REG_TCOR);
		r2 = clmpcc_rdreg(sc, CLMPCC_REG_TBPR);
		/* Only write Tx rate if it really has changed */
		if ( ch->ch_tcor != r1 || ch->ch_tbpr != r2 ) {
			clmpcc_wrreg(sc, CLMPCC_REG_TCOR, ch->ch_tcor);
			clmpcc_wrreg(sc, CLMPCC_REG_TBPR, ch->ch_tbpr);
		}
	}

	if ( ch->ch_rcor || ch->ch_rbpr ) {
		r1 = clmpcc_rdreg(sc, CLMPCC_REG_RCOR);
		r2 = clmpcc_rdreg(sc, CLMPCC_REG_RBPR);
		/* Only write Rx rate if it really has changed */
		if ( ch->ch_rcor != r1 || ch->ch_rbpr != r2 ) {
			clmpcc_wrreg(sc, CLMPCC_REG_RCOR, ch->ch_rcor);
			clmpcc_wrreg(sc, CLMPCC_REG_RBPR, ch->ch_rbpr);
		}
	}

	if ( clmpcc_rdreg(sc, CLMPCC_REG_COR1) != ch->ch_cor1 ) {
		clmpcc_wrreg(sc, CLMPCC_REG_COR1, ch->ch_cor1);
		/* Any change to COR1 requires an INIT command */
		SET(ch->ch_flags, CLMPCC_FLG_NEED_INIT);
	}

	if ( clmpcc_rdreg(sc, CLMPCC_REG_COR3) != ch->ch_cor3 )
		clmpcc_wrreg(sc, CLMPCC_REG_COR3, ch->ch_cor3);

	r1 = clmpcc_rdreg(sc, CLMPCC_REG_COR4);
	if ( ch->ch_cor4 != (r1 & CLMPCC_COR4_FIFO_MASK) ) {
		/*
		 * Note: If the FIFO has changed, we always set it to
		 * zero here and disable the Receive Timeout interrupt.
		 * It's up to the Rx Interrupt handler to pick the
		 * appropriate moment to write the new FIFO length.
		 */
		clmpcc_wrreg(sc, CLMPCC_REG_COR4, r1 & ~CLMPCC_COR4_FIFO_MASK);
		r1 = clmpcc_rdreg(sc, CLMPCC_REG_IER);
		clmpcc_wrreg(sc, CLMPCC_REG_IER, r1 & ~CLMPCC_IER_RET);
		SET(ch->ch_flags, CLMPCC_FLG_FIFO_CLEAR);
	}

	r1 = clmpcc_rdreg(sc, CLMPCC_REG_COR5);
	if ( ch->ch_cor5 != (r1 & CLMPCC_COR5_FLOW_MASK) ) {
		r1 &= ~CLMPCC_COR5_FLOW_MASK;
		clmpcc_wrreg(sc, CLMPCC_REG_COR5, r1 | ch->ch_cor5);
	}
}

static void
clmpcc_start(struct tty *tp)
{
	struct clmpcc_softc *sc =
	    device_lookup_private(&clmpcc_cd, CLMPCCUNIT(tp->t_dev));
	struct clmpcc_chan *ch = &sc->sc_chans[CLMPCCCHAN(tp->t_dev)];
	u_int oldch;
	int s;

	s = spltty();

	if ( ISCLR(tp->t_state, TS_TTSTOP | TS_TIMEOUT | TS_BUSY) ) {
		ttypull(tp);
		if ( ISSET(ch->ch_flags, CLMPCC_FLG_START_BREAK |
					 CLMPCC_FLG_END_BREAK) ||
		     tp->t_outq.c_cc > 0 ) {

			if ( ISCLR(ch->ch_flags, CLMPCC_FLG_START_BREAK |
						 CLMPCC_FLG_END_BREAK) ) {
				ch->ch_obuf_addr = tp->t_outq.c_cf;
				ch->ch_obuf_size = ndqb(&tp->t_outq, 0);
			}

			/* Enable TX empty interrupts */
			oldch = clmpcc_select_channel(ch->ch_sc, ch->ch_car);
			clmpcc_wrreg(ch->ch_sc, CLMPCC_REG_IER,
				clmpcc_rdreg(ch->ch_sc, CLMPCC_REG_IER) |
					     CLMPCC_IER_TX_EMPTY);
			clmpcc_select_channel(ch->ch_sc, oldch);
			SET(tp->t_state, TS_BUSY);
		}
	}

	splx(s);
}

/*
 * Stop output on a line.
 */
void
clmpccstop(struct tty *tp, int flag)
{
	struct clmpcc_softc *sc =
	    device_lookup_private(&clmpcc_cd, CLMPCCUNIT(tp->t_dev));
	struct clmpcc_chan *ch = &sc->sc_chans[CLMPCCCHAN(tp->t_dev)];
	int s;

	s = splserial();

	if ( ISSET(tp->t_state, TS_BUSY) ) {
		if ( ISCLR(tp->t_state, TS_TTSTOP) )
			SET(tp->t_state, TS_FLUSH);
		ch->ch_obuf_size = 0;
	}
	splx(s);
}

/*
 * RX interrupt routine
 */
int
clmpcc_rxintr(void *arg)
{
	struct clmpcc_softc *sc = (struct clmpcc_softc *)arg;
	struct clmpcc_chan *ch;
	u_int8_t *put, *end, rxd;
	u_char errstat;
	u_char fc, tc;
	u_char risr;
	u_char rir;
#ifdef DDB
	int saw_break = 0;
#endif

	/* Receive interrupt active? */
	rir = clmpcc_rdreg(sc, CLMPCC_REG_RIR);

	/*
	 * If we're using auto-vectored interrupts, we have to
	 * verify if the chip is generating the interrupt.
	 */
	if ( sc->sc_vector_base == 0 && (rir & CLMPCC_RIR_RACT) == 0 )
		return 0;

	/* Get pointer to interrupting channel's data structure */
	ch = &sc->sc_chans[rir & CLMPCC_RIR_RCN_MASK];

	/* Get the interrupt status register */
	risr = clmpcc_rdreg(sc, CLMPCC_REG_RISRl);
	if ( risr & CLMPCC_RISR_TIMEOUT ) {
		u_char reg;
		/*
		 * Set the FIFO threshold to zero, and disable
		 * further receive timeout interrupts.
		 */
		reg = clmpcc_rdreg(sc, CLMPCC_REG_COR4);
		clmpcc_wrreg(sc, CLMPCC_REG_COR4, reg & ~CLMPCC_COR4_FIFO_MASK);
		reg = clmpcc_rdreg(sc, CLMPCC_REG_IER);
		clmpcc_wrreg(sc, CLMPCC_REG_IER, reg & ~CLMPCC_IER_RET);
		clmpcc_wrreg(sc, CLMPCC_REG_REOIR, CLMPCC_REOIR_NO_TRANS);
		SET(ch->ch_flags, CLMPCC_FLG_FIFO_CLEAR);
		return 1;
	}

	/* How many bytes are waiting in the FIFO?  */
	fc = tc = clmpcc_rdreg(sc, CLMPCC_REG_RFOC) & CLMPCC_RFOC_MASK;

#ifdef DDB
	/*
	 * Allow BREAK on the console to drop to the debugger.
	 */
	if ( ISSET(ch->ch_flags, CLMPCC_FLG_IS_CONSOLE) &&
	     risr & CLMPCC_RISR_BREAK ) {
		saw_break = 1;
	}
#endif

	if ( ISCLR(ch->ch_tty->t_state, TS_ISOPEN) && fc ) {
		/* Just get rid of the data */
		while ( fc-- )
			(void) clmpcc_rd_rxdata(sc);
		goto rx_done;
	}

	put = ch->ch_ibuf_wr;
	end = ch->ch_ibuf_end;

	/*
	 * Note: The chip is completely hosed WRT these error
	 *       conditions; there seems to be no way to associate
	 *       the error with the correct character in the FIFO.
	 *       We compromise by tagging the first character we read
	 *       with the error. Not perfect, but there's no other way.
	 */
	errstat = 0;
	if ( risr & CLMPCC_RISR_PARITY )
		errstat |= TTY_PE;
	if ( risr & (CLMPCC_RISR_FRAMING | CLMPCC_RISR_BREAK) )
		errstat |= TTY_FE;

	/*
	 * As long as there are characters in the FIFO, and we
	 * have space for them...
	 */
	while ( fc > 0 ) {

		*put++ = rxd = clmpcc_rd_rxdata(sc);
		*put++ = errstat;

		if ( put >= end )
			put = ch->ch_ibuf;

		if ( put == ch->ch_ibuf_rd ) {
			put -= 2;
			if ( put < ch->ch_ibuf )
				put = end - 2;
		}

		errstat = 0;
		fc--;
	}

	ch->ch_ibuf_wr = put;

#if 0
	if ( sc->sc_swaprtsdtr == 0 &&
	     ISSET(cy->cy_tty->t_cflag, CRTSCTS) && cc < ch->ch_r_hiwat) {
		/*
		 * If RTS/DTR are not physically swapped, we have to
		 * do hardware flow control manually
		 */
		clmpcc_wr_msvr(sc, CLMPCC_MSVR_RTS, 0);
	}
#endif

rx_done:
	if ( fc != tc ) {
		if ( ISSET(ch->ch_flags, CLMPCC_FLG_FIFO_CLEAR) ) {
			u_char reg;
			/*
			 * Set the FIFO threshold to the preset value,
			 * and enable receive timeout interrupts.
			 */
			reg = clmpcc_rdreg(sc, CLMPCC_REG_COR4);
			reg = (reg & ~CLMPCC_COR4_FIFO_MASK) | ch->ch_cor4;
			clmpcc_wrreg(sc, CLMPCC_REG_COR4, reg);
			reg = clmpcc_rdreg(sc, CLMPCC_REG_IER);
			clmpcc_wrreg(sc, CLMPCC_REG_IER, reg | CLMPCC_IER_RET);
			CLR(ch->ch_flags, CLMPCC_FLG_FIFO_CLEAR);
		}

		clmpcc_wrreg(sc, CLMPCC_REG_REOIR, 0);
		softint_schedule(sc->sc_softintr_cookie);
	} else
		clmpcc_wrreg(sc, CLMPCC_REG_REOIR, CLMPCC_REOIR_NO_TRANS);

#ifdef DDB
	/*
	 * Only =after= we write REOIR is it safe to drop to the debugger.
	 */
	if ( saw_break )
		Debugger();
#endif

	return 1;
}

/*
 * Tx interrupt routine
 */
int
clmpcc_txintr(void *arg)
{
	struct clmpcc_softc *sc = (struct clmpcc_softc *)arg;
	struct clmpcc_chan *ch;
	u_char ftc, oftc;
	u_char tir, teoir;
	int etcmode = 0;

	/* Tx interrupt active? */
	tir = clmpcc_rdreg(sc, CLMPCC_REG_TIR);

	/*
	 * If we're using auto-vectored interrupts, we have to
	 * verify if the chip is generating the interrupt.
	 */
	if ( sc->sc_vector_base == 0 && (tir & CLMPCC_TIR_TACT) == 0 )
		return 0;

	/* Get pointer to interrupting channel's data structure */
	ch = &sc->sc_chans[tir & CLMPCC_TIR_TCN_MASK];

	/* Dummy read of the interrupt status register */
	(void) clmpcc_rdreg(sc, CLMPCC_REG_TISR);

	/* Make sure embedded transmit commands are disabled */
	clmpcc_wrreg(sc, CLMPCC_REG_COR2, ch->ch_cor2);

	ftc = oftc = clmpcc_rdreg(sc, CLMPCC_REG_TFTC);

	/* Handle a delayed parameter change */
	if ( ISSET(ch->ch_flags, CLMPCC_FLG_UPDATE_PARMS) ) {
		CLR(ch->ch_flags, CLMPCC_FLG_UPDATE_PARMS);
		clmpcc_set_params(ch);
	}

	if ( ch->ch_obuf_size > 0 ) {
		u_int n = min(ch->ch_obuf_size, ftc);

		clmpcc_wrtx_multi(sc, ch->ch_obuf_addr, n);

		ftc -= n;
		ch->ch_obuf_size -= n;
		ch->ch_obuf_addr += n;

	} else {
		/*
		 * Check if we should start/stop a break
		 */
		if ( ISSET(ch->ch_flags, CLMPCC_FLG_START_BREAK) ) {
			CLR(ch->ch_flags, CLMPCC_FLG_START_BREAK);
			/* Enable embedded transmit commands */
			clmpcc_wrreg(sc, CLMPCC_REG_COR2,
					ch->ch_cor2 | CLMPCC_COR2_ETC);
			clmpcc_wr_txdata(sc, CLMPCC_ETC_MAGIC);
			clmpcc_wr_txdata(sc, CLMPCC_ETC_SEND_BREAK);
			ftc -= 2;
			etcmode = 1;
		}

		if ( ISSET(ch->ch_flags, CLMPCC_FLG_END_BREAK) ) {
			CLR(ch->ch_flags, CLMPCC_FLG_END_BREAK);
			/* Enable embedded transmit commands */
			clmpcc_wrreg(sc, CLMPCC_REG_COR2,
					ch->ch_cor2 | CLMPCC_COR2_ETC);
			clmpcc_wr_txdata(sc, CLMPCC_ETC_MAGIC);
			clmpcc_wr_txdata(sc, CLMPCC_ETC_STOP_BREAK);
			ftc -= 2;
			etcmode = 1;
		}
	}

	tir = clmpcc_rdreg(sc, CLMPCC_REG_IER);

	if ( ftc != oftc ) {
		/*
		 * Enable/disable the Tx FIFO threshold interrupt
		 * according to how much data is in the FIFO.
		 * However, always disable the FIFO threshold if
		 * we've left the channel in 'Embedded Transmit
		 * Command' mode.
		 */
		if ( etcmode || ftc >= ch->ch_cor4 )
			tir &= ~CLMPCC_IER_TX_FIFO;
		else
			tir |= CLMPCC_IER_TX_FIFO;
		teoir = 0;
	} else {
		/*
		 * No data was sent.
		 * Disable transmit interrupt.
		 */
		tir &= ~(CLMPCC_IER_TX_EMPTY|CLMPCC_IER_TX_FIFO);
		teoir = CLMPCC_TEOIR_NO_TRANS;

		/*
		 * Request Tx processing in the soft interrupt handler
		 */
		ch->ch_tx_done = 1;
		softint_schedule(sc->sc_softintr_cookie);
	}

	clmpcc_wrreg(sc, CLMPCC_REG_IER, tir);
	clmpcc_wrreg(sc, CLMPCC_REG_TEOIR, teoir);

	return 1;
}

/*
 * Modem change interrupt routine
 */
int
clmpcc_mdintr(void *arg)
{
	struct clmpcc_softc *sc = (struct clmpcc_softc *)arg;
	u_char mir;

	/* Modem status interrupt active? */
	mir = clmpcc_rdreg(sc, CLMPCC_REG_MIR);

	/*
	 * If we're using auto-vectored interrupts, we have to
	 * verify if the chip is generating the interrupt.
	 */
	if ( sc->sc_vector_base == 0 && (mir & CLMPCC_MIR_MACT) == 0 )
		return 0;

	/* Dummy read of the interrupt status register */
	(void) clmpcc_rdreg(sc, CLMPCC_REG_MISR);

	/* Retrieve current status of modem lines. */
	sc->sc_chans[mir & CLMPCC_MIR_MCN_MASK].ch_control |=
		clmpcc_rd_msvr(sc) & CLMPCC_MSVR_CD;

	clmpcc_wrreg(sc, CLMPCC_REG_MEOIR, 0);
	softint_schedule(sc->sc_softintr_cookie);

	return 1;
}

void
clmpcc_softintr(void *arg)
{
	struct clmpcc_softc *sc = (struct clmpcc_softc *)arg;
	struct clmpcc_chan *ch;
	struct tty *tp;
	int (*rint)(int, struct tty *);
	u_char *get;
	u_char reg;
	u_int c;
	int chan;

	/* Handle Modem state changes too... */

	for (chan = 0; chan < CLMPCC_NUM_CHANS; chan++) {
		ch = &sc->sc_chans[chan];
		tp = ch->ch_tty;

		get = ch->ch_ibuf_rd;
		rint = tp->t_linesw->l_rint;

		/* Squirt buffered incoming data into the tty layer */
		while ( get != ch->ch_ibuf_wr ) {
			c = get[0];
			c |= ((u_int)get[1]) << 8;
			if ( (rint)(c, tp) == -1 ) {
				ch->ch_ibuf_rd = ch->ch_ibuf_wr;
				break;
			}

			get += 2;
			if ( get == ch->ch_ibuf_end )
				get = ch->ch_ibuf;

			ch->ch_ibuf_rd = get;
		}

		/*
		 * Is the transmitter idle and in need of attention?
		 */
		if ( ch->ch_tx_done ) {
			ch->ch_tx_done = 0;

			if ( ISSET(ch->ch_flags, CLMPCC_FLG_NEED_INIT) ) {
				clmpcc_channel_cmd(sc, ch->ch_car,
						       CLMPCC_CCR_T0_INIT  |
						       CLMPCC_CCR_T0_RX_EN |
					   	       CLMPCC_CCR_T0_TX_EN);
				CLR(ch->ch_flags, CLMPCC_FLG_NEED_INIT);

				/*
				 * Allow time for the channel to initialise.
				 * (Empirically derived duration; there must
				 * be another way to determine the command
				 * has completed without busy-waiting...)
				 */
				delay(800);

				/*
				 * Update the tty layer's idea of the carrier
				 * bit, in case we changed CLOCAL or MDMBUF.
				 * We don't hang up here; we only do that by
				 * explicit request.
				 */
				reg = clmpcc_rd_msvr(sc) & CLMPCC_MSVR_CD;
				(*tp->t_linesw->l_modem)(tp, reg != 0);
			}

			CLR(tp->t_state, TS_BUSY);
			if ( ISSET(tp->t_state, TS_FLUSH) )
				CLR(tp->t_state, TS_FLUSH);
			else
				ndflush(&tp->t_outq,
				     (int)(ch->ch_obuf_addr - tp->t_outq.c_cf));

			(*tp->t_linesw->l_start)(tp);
		}
	}
}


/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
/*
 * Following are all routines needed for a cd240x channel to act as console
 */
int
clmpcc_cnattach(struct clmpcc_softc *sc, int chan, int rate)
{
	cons_sc = sc;
	cons_chan = chan;
	cons_rate = rate;

	return (clmpcc_init(sc));
}

/*
 * The following functions are polled getc and putc routines, for console use.
 */
static int
clmpcc_common_getc(struct clmpcc_softc *sc, int chan)
{
	u_char old_chan;
	u_char old_ier;
	u_char ch, rir, risr;
	int s;

	s = splhigh();

	/* Save the currently active channel */
	old_chan = clmpcc_select_channel(sc, chan);

	/*
	 * We have to put the channel into RX interrupt mode before
	 * trying to read the Rx data register. So save the previous
	 * interrupt mode.
	 */
	old_ier = clmpcc_rdreg(sc, CLMPCC_REG_IER);
	clmpcc_wrreg(sc, CLMPCC_REG_IER, CLMPCC_IER_RX_FIFO);

	/* Loop until we get a character */
	for (;;) {
		/*
		 * The REN bit will be set in the Receive Interrupt Register
		 * when the CD240x has a character to process. Remember,
		 * the RACT bit won't be set until we generate an interrupt
		 * acknowledge cycle via the MD front-end.
		 */
		rir = clmpcc_rdreg(sc, CLMPCC_REG_RIR);
		if ( (rir & CLMPCC_RIR_REN) == 0 )
			continue;

		/* Acknowledge the request */
		if ( sc->sc_iackhook )
			(sc->sc_iackhook)(sc, CLMPCC_IACK_RX);

		/*
		 * Determine if the interrupt is for the required channel
		 * and if valid data is available.
		 */
		rir = clmpcc_rdreg(sc, CLMPCC_REG_RIR);
		risr = clmpcc_rdreg(sc, CLMPCC_REG_RISR);
		if ( (rir & CLMPCC_RIR_RCN_MASK) != chan ||
		     risr != 0 ) {
			/* Rx error, or BREAK */
			clmpcc_wrreg(sc, CLMPCC_REG_REOIR,
					 CLMPCC_REOIR_NO_TRANS);
		} else {
			/* Dummy read of the FIFO count register */
			(void) clmpcc_rdreg(sc, CLMPCC_REG_RFOC);

			/* Fetch the received character */
			ch = clmpcc_rd_rxdata(sc);

			clmpcc_wrreg(sc, CLMPCC_REG_REOIR, 0);
			break;
		}
	}

	/* Restore the original IER and CAR register contents */
	clmpcc_wrreg(sc, CLMPCC_REG_IER, old_ier);
	clmpcc_select_channel(sc, old_chan);

	splx(s);
	return ch;
}


static void
clmpcc_common_putc(struct clmpcc_softc *sc, int chan, int c)
{
	u_char old_chan;
	int s = splhigh();

	/* Save the currently active channel */
	old_chan = clmpcc_select_channel(sc, chan);

	/*
	 * Since we can only access the Tx Data register from within
	 * the interrupt handler, the easiest way to get console data
	 * onto the wire is using one of the Special Transmit Character
	 * registers.
	 */
	clmpcc_wrreg(sc, CLMPCC_REG_SCHR4, c);
	clmpcc_wrreg(sc, CLMPCC_REG_STCR, CLMPCC_STCR_SSPC(4) |
					  CLMPCC_STCR_SND_SPC);

	/* Wait until the "Send Special Character" command is accepted */
	while ( clmpcc_rdreg(sc, CLMPCC_REG_STCR) != 0 )
		;

	/* Restore the previous channel selected */
	clmpcc_select_channel(sc, old_chan);

	splx(s);
}

int
clmpcccngetc(dev_t dev)
{
	return clmpcc_common_getc(cons_sc, cons_chan);
}

/*
 * Console kernel output character routine.
 */
void
clmpcccnputc(dev_t dev, int c)
{
	if ( c == '\n' )
		clmpcc_common_putc(cons_sc, cons_chan, '\r');

	clmpcc_common_putc(cons_sc, cons_chan, c);
}
