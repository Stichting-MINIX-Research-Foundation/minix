/*	$NetBSD: cy.c,v 1.60 2014/11/15 19:18:18 christos Exp $	*/

/*
 * cy.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 *
 * Supports both ISA and PCI Cyclom cards
 *
 * Lots of debug output can be enabled by defining CY_DEBUG
 * Some debugging counters (number of receive/transmit interrupts etc.)
 * can be enabled by defining CY_DEBUG1
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cy.c,v 1.60 2014/11/15 19:18:18 christos Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kauth.h>

#include <sys/bus.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>
#include <dev/ic/cyvar.h>

static int	cyparam(struct tty *, struct termios *);
static void	cystart(struct tty *);
static void	cy_poll(void *);
static int	cy_modem_control(struct cy_softc *, struct cy_port *, int, int);
static void	cy_enable_transmitter(struct cy_softc *, struct cy_port *);
static void	cd1400_channel_cmd(struct cy_softc *, struct cy_port *, int);
static int	cy_speed(speed_t, int *, int *, int);

extern struct cfdriver cy_cd;

static dev_type_open(cyopen);
static dev_type_close(cyclose);
static dev_type_read(cyread);
static dev_type_write(cywrite);
static dev_type_ioctl(cyioctl);
static dev_type_stop(cystop);
static dev_type_tty(cytty);
static dev_type_poll(cypoll);

const struct cdevsw cy_cdevsw = {
	.d_open = cyopen,
	.d_close = cyclose,
	.d_read = cyread,
	.d_write = cywrite,
	.d_ioctl = cyioctl,
	.d_stop = cystop,
	.d_tty = cytty,
	.d_poll = cypoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

static int      cy_open = 0;
static int      cy_events = 0;

int	cy_attached_ttys;
bool	cy_callout_init;
callout_t cy_poll_callout;

/*
 * Common probe routine
 */
int
cy_find(struct cy_softc *sc)
{
	int cy_chip, chip;
	u_char firmware_ver;
	bus_space_tag_t tag = sc->sc_memt;
	bus_space_handle_t bsh = sc->sc_bsh;
	int bustype = sc->sc_bustype;

	/* Cyclom card hardware reset */
	bus_space_write_1(tag, bsh, CY16_RESET << bustype, 0);
	DELAY(500);		/* wait for reset to complete */
	bus_space_write_1(tag, bsh, CY_CLEAR_INTR << bustype, 0);

#ifdef CY_DEBUG
	printf("cy: card reset done\n");
#endif
	sc->sc_nchips = 0;

	for (cy_chip = 0, chip = 0; cy_chip < CY_MAX_CD1400s;
	    cy_chip++, chip += (CY_CD1400_MEMSPACING << bustype)) {
		int i;

		/*
		 * the last 4 nchips are 'interleaved' with the first 4 on
		 * 32-port boards
		 */
		if (cy_chip == 4)
			chip -= (CY32_ADDR_FIX << bustype);

#ifdef CY_DEBUG
		printf("sy: probe chip %d offset 0x%x ... ", cy_chip, chip);
#endif

		/* wait until the chip is ready for command */
		DELAY(1000);
		if (bus_space_read_1(tag, bsh, chip +
		    ((CD1400_CCR << 1) << bustype)) != 0) {
#ifdef CY_DEBUG
			printf("not ready for command\n");
#endif
			break;
		}
		/* clear the firmware version reg. */
		bus_space_write_1(tag, bsh, chip +
		    ((CD1400_GFRCR << 1) << bustype), 0);

		/*
	         * On Cyclom-16 references to non-existent chip 4
	         * actually access chip 0 (address line 9 not decoded).
	         * Here we check if the clearing of chip 4 GFRCR actually
	         * cleared chip 0 GFRCR. In that case we have a 16 port card.
	         */
		if (cy_chip == 4 &&
		    bus_space_read_1(tag, bsh, /* off for chip 0 (0) + */
		       ((CD1400_GFRCR << 1) << bustype)) == 0)
			break;

		/* reset the chip */
		bus_space_write_1(tag, bsh, chip +
		    ((CD1400_CCR << 1) << bustype),
		    CD1400_CCR_CMDRESET | CD1400_CCR_FULLRESET);

		/* wait for the chip to initialize itself */
		for (i = 0; i < 200; i++) {
			DELAY(50);
			firmware_ver = bus_space_read_1(tag, bsh, chip +
			    ((CD1400_GFRCR << 1) << bustype));
			if ((firmware_ver & 0xf0) == 0x40) /* found a CD1400 */
				break;
		}
#ifdef CY_DEBUG
		printf("firmware version 0x%x\n", firmware_ver);
#endif

		if ((firmware_ver & 0xf0) != 0x40)
			break;

		/* firmware version OK, CD1400 found */
		sc->sc_nchips++;
	}

	if (sc->sc_nchips == 0) {
#ifdef CY_DEBUG
		printf("no CD1400s found\n");
#endif
		return 0;
	}
#ifdef CY_DEBUG
	printf("found %d CD1400s\n", sc->sc_nchips);
#endif

	return 1;
}

void
cy_attach(struct cy_softc *sc)
{
	int port, cy_chip, num_chips, cdu, chip;
	int cy_clock;

	if (!cy_callout_init) {
		cy_callout_init = true;
		callout_init(&cy_poll_callout, 0);
	}

	num_chips = sc->sc_nchips;
	if (num_chips == 0)
		return;

	memset(sc->sc_ports, 0, sizeof(sc->sc_ports));

	port = 0;
	for (cy_chip = 0, chip = 0; cy_chip < num_chips; cy_chip++,
	    chip += (CY_CD1400_MEMSPACING << sc->sc_bustype)) {

		if (cy_chip == 4)
			chip -= (CY32_ADDR_FIX << sc->sc_bustype);

#ifdef CY_DEBUG
		aprint_debug("attach CD1400 #%d offset 0x%x\n", cy_chip, chip);
#endif
		sc->sc_cd1400_offs[cy_chip] = chip;

		/*
		 * configure port 0 as serial port (should already be after
		 * reset)
		 */
		cd_write_reg(sc, cy_chip, CD1400_GCR, 0);

		if (cd_read_reg(sc, cy_chip, CD1400_GFRCR) <= 0x46)
			cy_clock = CY_CLOCK;
		else
			cy_clock = CY_CLOCK_60;

		/* set up a receive timeout period (1ms) */
		cd_write_reg(sc, cy_chip, CD1400_PPR,
		    (cy_clock / CD1400_PPR_PRESCALER / 1000) + 1);

		for (cdu = 0; cdu < CD1400_NO_OF_CHANNELS; cdu++) {
			sc->sc_ports[port].cy_softc = sc;
			sc->sc_ports[port].cy_port_num = port;
			sc->sc_ports[port].cy_chip = cy_chip;
			sc->sc_ports[port].cy_clock = cy_clock;

			/* should we initialize anything else here? */
			port++;
		} /* for(each port on one CD1400...) */

	} /* for(each CD1400 on a card... ) */

	sc->sc_nchannels = port;

	aprint_normal_dev(sc->sc_dev, "%d channels (ttyCY%03d..ttyCY%03d)\n",
	    sc->sc_nchannels, cy_attached_ttys,
	    cy_attached_ttys + (sc->sc_nchannels - 1));

	cy_attached_ttys += sc->sc_nchannels;

	/* ensure an edge for the next interrupt */
	bus_space_write_1(sc->sc_memt, sc->sc_bsh,
	    CY_CLEAR_INTR << sc->sc_bustype, 0);
}

#define	CY_UNIT(dev)		TTUNIT(dev)
#define	CY_DIALOUT(dev)		TTDIALOUT(dev)

#define	CY_PORT(dev)		cy_getport((dev))
#define	CY_BOARD(cy)		((cy)->cy_softc)

static struct cy_port *
cy_getport(dev_t dev)
{
	int i, j, k, u = CY_UNIT(dev);
	struct cy_softc *sc;

	for (i = 0, j = 0; i < cy_cd.cd_ndevs; i++) {
		k = j;
		sc = device_lookup_private(&cy_cd, i);
		if (sc == NULL)
			continue;
		if (sc->sc_nchannels == 0)
			continue;
		j += sc->sc_nchannels;
		if (j > u)
			return (&sc->sc_ports[u - k]);
	}

	return (NULL);
}

/*
 * open routine. returns zero if successful, else error code
 */
int
cyopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct cy_softc *sc;
	struct cy_port *cy;
	struct tty *tp;
	int s, error;

	cy = CY_PORT(dev);
	if (cy == NULL)
		return (ENXIO);
	sc = CY_BOARD(cy);

	s = spltty();
	if (cy->cy_tty == NULL) {
		if ((cy->cy_tty = tty_alloc()) == NULL) {
			splx(s);
			aprint_error_dev(sc->sc_dev,
			    "port %d: can't allocate tty\n",
			    cy->cy_port_num);
			return (ENOMEM);
		}
		tty_attach(cy->cy_tty);
	}
	splx(s);

	tp = cy->cy_tty;
	tp->t_oproc = cystart;
	tp->t_param = cyparam;
	tp->t_dev = dev;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(cy->cy_openflags, TIOCFLAG_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(cy->cy_openflags, TIOCFLAG_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(cy->cy_openflags, TIOCFLAG_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;

		s = spltty();

		/*
		 * Allocate input ring buffer if we don't already have one
		 */
		if (cy->cy_ibuf == NULL) {
			cy->cy_ibuf = malloc(CY_IBUF_SIZE, M_DEVBUF, M_NOWAIT);
			if (cy->cy_ibuf == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "port %d: can't allocate input buffer\n",
				    cy->cy_port_num);
				splx(s);
				return ENOMEM;
			}
			cy->cy_ibuf_end = cy->cy_ibuf + CY_IBUF_SIZE;
		}
		/* mark the ring buffer as empty */
		cy->cy_ibuf_rd_ptr = cy->cy_ibuf_wr_ptr = cy->cy_ibuf;

		/* select CD1400 channel */
		cd_write_reg(sc, cy->cy_chip, CD1400_CAR,
		    cy->cy_port_num & CD1400_CAR_CHAN);
		/* reset the channel */
		cd1400_channel_cmd(sc, cy, CD1400_CCR_CMDRESET);
		/* encode unit (port) number in LIVR */
		/* there is just enough space for 5 bits (32 ports) */
		cd_write_reg(sc, cy->cy_chip, CD1400_LIVR,
		    cy->cy_port_num << 3);

		cy->cy_channel_control = 0;

		/* hmm... need spltty() here? */
		if (cy_open == 0) {
			cy_open = 1;
			callout_reset(&cy_poll_callout, 1, cy_poll, NULL);
		}
		/* this sets parameters and raises DTR */
		cyparam(tp, &tp->t_termios);

		ttsetwater(tp);

		/* raise RTS too */
		cy_modem_control(sc, cy, TIOCM_RTS, DMBIS);

		cy->cy_carrier_stat =
			cd_read_reg(sc, cy->cy_chip, CD1400_MSVR2);

		/* enable receiver and modem change interrupts */
		cd_write_reg(sc, cy->cy_chip, CD1400_SRER,
		    CD1400_SRER_MDMCH | CD1400_SRER_RXDATA);

		if (CY_DIALOUT(dev) ||
		    ISSET(cy->cy_openflags, TIOCFLAG_SOFTCAR) ||
		    ISSET(tp->t_cflag, MDMBUF) ||
		    ISSET(cy->cy_carrier_stat, CD1400_MSVR2_CD))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
		splx(s);
	}

	/* wait for carrier if necessary */
	if (!ISSET(flag, O_NONBLOCK)) {
		mutex_spin_enter(&tty_lock);
		while (!ISSET(tp->t_cflag, CLOCAL) &&
		    !ISSET(tp->t_state, TS_CARR_ON)) {
			tp->t_wopen++;
			error = ttysleep(tp, &tp->t_rawcv, true, 0);
			tp->t_wopen--;
			if (error != 0) {
				mutex_spin_exit(&tty_lock);
				return error;
			}
		}
		mutex_spin_exit(&tty_lock);
	}

	return (*tp->t_linesw->l_open) (dev, tp);
}

/*
 * close routine. returns zero if successful, else error code
 */
int
cyclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct cy_softc *sc;
	struct cy_port *cy;
	struct tty *tp;
	int s;

	cy = CY_PORT(dev);
	sc = CY_BOARD(cy);
	tp = cy->cy_tty;

	(*tp->t_linesw->l_close) (tp, flag);
	s = spltty();

	if (ISSET(tp->t_cflag, HUPCL) &&
	    !ISSET(cy->cy_openflags, TIOCFLAG_SOFTCAR)) {
		/*
		 * drop DTR and RTS (should we wait for output buffer to
		 * become empty first?)
		 */
		cy_modem_control(sc, cy, 0, DMSET);
	}
	/*
	 * XXX should we disable modem change and
	 * receive interrupts here or somewhere ?
	 */
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);

	splx(s);
	ttyclose(tp);

	return 0;
}

/*
 * Read routine
 */
int
cyread(dev_t dev, struct uio *uio, int flag)
{
	struct cy_port *cy;
	struct tty *tp;

	cy = CY_PORT(dev);
	tp = cy->cy_tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

/*
 * Write routine
 */
int
cywrite(dev_t dev, struct uio *uio, int flag)
{
	struct cy_port *cy;
	struct tty *tp;

	cy = CY_PORT(dev);
	tp = cy->cy_tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

/*
 * Poll routine
 */
int
cypoll(dev_t dev, int events, struct lwp *l)
{
	struct cy_port *cy;
	struct tty *tp;

	cy = CY_PORT(dev);
	tp = cy->cy_tty;

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

/*
 * return tty pointer
 */
struct tty *
cytty(dev_t dev)
{
	struct cy_port *cy;

	cy = CY_PORT(dev);

	return (cy->cy_tty);
}

/*
 * ioctl routine
 */
int
cyioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct cy_softc *sc;
	struct cy_port *cy;
	struct tty *tp;
	int error;

	cy = CY_PORT(dev);
	sc = CY_BOARD(cy);
	tp = cy->cy_tty;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	/* XXX should not allow dropping DTR when dialin? */

	switch (cmd) {
	case TIOCSBRK:		/* start break */
		SET(cy->cy_flags, CY_F_START_BREAK);
		cy_enable_transmitter(sc, cy);
		break;

	case TIOCCBRK:		/* stop break */
		SET(cy->cy_flags, CY_F_END_BREAK);
		cy_enable_transmitter(sc, cy);
		break;

	case TIOCSDTR:		/* DTR on */
		cy_modem_control(sc, cy, TIOCM_DTR, DMBIS);
		break;

	case TIOCCDTR:		/* DTR off */
		cy_modem_control(sc, cy, TIOCM_DTR, DMBIC);
		break;

	case TIOCMSET:		/* set new modem control line values */
		cy_modem_control(sc, cy, *((int *) data), DMSET);
		break;

	case TIOCMBIS:		/* turn modem control bits on */
		cy_modem_control(sc, cy, *((int *) data), DMBIS);
		break;

	case TIOCMBIC:		/* turn modem control bits off */
		cy_modem_control(sc, cy, *((int *) data), DMBIC);
		break;

	case TIOCMGET:		/* get modem control/status line state */
		*((int *) data) = cy_modem_control(sc, cy, 0, DMGET);
		break;

	case TIOCGFLAGS:
		*((int *) data) = cy->cy_openflags |
			(CY_DIALOUT(dev) ? TIOCFLAG_SOFTCAR : 0);
		break;

	case TIOCSFLAGS:
		error = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);
		if (error != 0)
			return EPERM;

		cy->cy_openflags = *((int *) data) &
		    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL |
		     TIOCFLAG_CRTSCTS | TIOCFLAG_MDMBUF);
		break;

	default:
		return EPASSTHROUGH;
	}

	return 0;
}

/*
 * start output
 */
void
cystart(struct tty *tp)
{
	struct cy_softc *sc;
	struct cy_port *cy;
	int s;

	cy = CY_PORT(tp->t_dev);
	sc = cy->cy_softc;

	s = spltty();

#ifdef CY_DEBUG1
	cy->cy_start_count++;
#endif

	if (!ISSET(tp->t_state, TS_TTSTOP | TS_TIMEOUT | TS_BUSY)) {
		if (!ttypull(tp))
			goto out;
		SET(tp->t_state, TS_BUSY);
		cy_enable_transmitter(sc, cy);
	}
out:

	splx(s);
}

/*
 * stop output
 */
void
cystop(struct tty *tp, int flag)
{
	struct cy_port *cy;
	int s;

	cy = CY_PORT(tp->t_dev);

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);

		/*
		 * the transmit interrupt routine will disable transmit when it
		 * notices that CY_F_STOP has been set.
		 */
		SET(cy->cy_flags, CY_F_STOP);
	}
	splx(s);
}

/*
 * parameter setting routine.
 * returns 0 if successful, else returns error code
 */
int
cyparam(struct tty *tp, struct termios *t)
{
	struct cy_softc *sc;
	struct cy_port *cy;
	int ibpr = 0, obpr = 0, i_clk_opt = 0, o_clk_opt = 0;	/* XXX: GCC */
	int s, opt;

	cy = CY_PORT(tp->t_dev);
	sc = CY_BOARD(cy);

	if (t->c_ospeed != 0 && cy_speed(t->c_ospeed, &o_clk_opt, &obpr, cy->cy_clock) < 0)
		return EINVAL;

	if (t->c_ispeed != 0 && cy_speed(t->c_ispeed, &i_clk_opt, &ibpr, cy->cy_clock) < 0)
		return EINVAL;

	s = spltty();

	/* hang up the line is ospeed is zero, else turn DTR on */
	cy_modem_control(sc, cy, TIOCM_DTR, (t->c_ospeed == 0 ? DMBIC : DMBIS));

	/* channel was selected by the above call to cy_modem_control() */
#if 0
	cd_write_reg(sc, cy->cy_chip, CD1400_CAR, port & CD1400_CAR_CHAN);
#endif

	/* set transmit speed */
	if (t->c_ospeed != 0) {
		cd_write_reg(sc, cy->cy_chip, CD1400_TCOR, o_clk_opt);
		cd_write_reg(sc, cy->cy_chip, CD1400_TBPR, obpr);
	}
	/* set receive speed */
	if (t->c_ispeed != 0) {
		cd_write_reg(sc, cy->cy_chip, CD1400_RCOR, i_clk_opt);
		cd_write_reg(sc, cy->cy_chip, CD1400_RBPR, ibpr);
	}
	opt = CD1400_CCR_CMDCHANCTL | CD1400_CCR_XMTEN
		| (ISSET(t->c_cflag, CREAD) ? CD1400_CCR_RCVEN : CD1400_CCR_RCVDIS);

	if (opt != cy->cy_channel_control) {
		cy->cy_channel_control = opt;
		cd1400_channel_cmd(sc, cy, opt);
	}
	/* compute COR1 contents */
	opt = 0;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			opt |= CD1400_COR1_PARODD;
		opt |= CD1400_COR1_PARNORMAL;
	}
	if (!ISSET(t->c_iflag, INPCK))
		opt |= CD1400_COR1_NOINPCK;	/* no parity checking */

	if (ISSET(t->c_cflag, CSTOPB))
		opt |= CD1400_COR1_STOP2;

	switch (t->c_cflag & CSIZE) {
	case CS5:
		opt |= CD1400_COR1_CS5;
		break;

	case CS6:
		opt |= CD1400_COR1_CS6;
		break;

	case CS7:
		opt |= CD1400_COR1_CS7;
		break;

	default:
		opt |= CD1400_COR1_CS8;
		break;
	}

	cd_write_reg(sc, cy->cy_chip, CD1400_COR1, opt);

#ifdef CY_DEBUG
	printf("cor1 = 0x%x...", opt);
#endif

	/*
	 * use the CD1400 automatic CTS flow control if CRTSCTS is set
	 *
	 * CD1400_COR2_ETC is used because breaks are generated with
	 * embedded transmit commands
	 */
	cd_write_reg(sc, cy->cy_chip, CD1400_COR2,
		     CD1400_COR2_ETC |
		 (ISSET(t->c_cflag, CRTSCTS) ? CD1400_COR2_CCTS_OFLOW : 0));

	cd_write_reg(sc, cy->cy_chip, CD1400_COR3, CY_RX_FIFO_THRESHOLD);

	cd1400_channel_cmd(sc, cy, CD1400_CCR_CMDCORCHG |
	    CD1400_CCR_COR1 | CD1400_CCR_COR2 | CD1400_CCR_COR3);

	cd_write_reg(sc, cy->cy_chip, CD1400_COR4, CD1400_COR4_PFO_EXCEPTION);
	cd_write_reg(sc, cy->cy_chip, CD1400_COR5, 0);

	/*
         * set modem change option registers to generate interrupts
         * on carrier detect changes.
         *
         * if hardware RTS handshaking is used
         * also set the handshaking threshold.
         */
	if (cy->cy_clock == CY_CLOCK_60) {
	   cd_write_reg(sc, cy->cy_chip, CD1400_MCOR1, CD1400_MCOR1_CDzd |
    	      (ISSET(t->c_cflag, CRTSCTS) ? CY_RX_DTR_THRESHOLD : 0));
	} else {
	   cd_write_reg(sc, cy->cy_chip, CD1400_MCOR1, CD1400_MCOR1_CDzd);
	}

	cd_write_reg(sc, cy->cy_chip, CD1400_MCOR2, CD1400_MCOR2_CDod);

	/*
         * set receive timeout to approx. 2ms
         * could use more complex logic here...
         * (but is it actually needed or even useful?)
         */
	cd_write_reg(sc, cy->cy_chip, CD1400_RTPR, 2);

	/*
         * should do anything else here?
         * XXX check MDMBUF handshaking like in com.c?
         */

	splx(s);
	return 0;
}

/*
 * set/get modem line status
 *
 * bits can be: TIOCM_DTR, TIOCM_RTS, TIOCM_CTS, TIOCM_CD, TIOCM_RI, TIOCM_DSR
 */
int
cy_modem_control(struct cy_softc *sc, struct cy_port *cy, int bits, int howto)
{
	struct tty *tp = cy->cy_tty;
	int s, msvr;

	s = spltty();

	/* select channel */
	cd_write_reg(sc, cy->cy_chip, CD1400_CAR,
	    cy->cy_port_num & CD1400_CAR_CHAN);

	/* Does not manipulate RTS if it is used for flow control. */
	switch (howto) {
	case DMGET:
		bits = 0;
		if (cy->cy_channel_control & CD1400_CCR_RCVEN)
			bits |= TIOCM_LE;
		msvr = cd_read_reg(sc, cy->cy_chip, CD1400_MSVR2);
		if (cy->cy_clock == CY_CLOCK_60) {
			if (cd_read_reg(sc, cy->cy_chip, CD1400_MSVR1) &
		    		CD1400_MSVR1_RTS)
				bits |= TIOCM_DTR;
			if (msvr & CD1400_MSVR2_DTR)
				bits |= TIOCM_RTS;
		} else {
			if (cd_read_reg(sc, cy->cy_chip, CD1400_MSVR1) &
			    CD1400_MSVR1_RTS)
				bits |= TIOCM_RTS;
			if (msvr & CD1400_MSVR2_DTR)
				bits |= TIOCM_DTR;
		}
		if (msvr & CD1400_MSVR2_CTS)
			bits |= TIOCM_CTS;
		if (msvr & CD1400_MSVR2_CD)
			bits |= TIOCM_CD;
		/* Not connected on some Cyclom-Y boards? */
		if (msvr & CD1400_MSVR2_DSR)
			bits |= TIOCM_DSR;
		/* Not connected on some Cyclom-8Y boards? */
		if (msvr & CD1400_MSVR2_RI)
			bits |= TIOCM_RI;
		break;

	case DMSET:		/* replace old values with new ones */
		if (cy->cy_clock == CY_CLOCK_60) {
			if (!ISSET(tp->t_cflag, CRTSCTS))
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2,
				   ((bits & TIOCM_RTS) ? CD1400_MSVR2_DTR : 0));
			cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1,
			    ((bits & TIOCM_DTR) ? CD1400_MSVR1_RTS : 0));
		} else {
			if (!ISSET(tp->t_cflag, CRTSCTS))
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1,
				   ((bits & TIOCM_RTS) ? CD1400_MSVR1_RTS : 0));
			cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2,
			    ((bits & TIOCM_DTR) ? CD1400_MSVR2_DTR : 0));
		}
		break;

	case DMBIS:		/* set bits */
		if (cy->cy_clock == CY_CLOCK_60) {
			if (!ISSET(tp->t_cflag, CRTSCTS) &&
			    (bits & TIOCM_RTS) != 0)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2,
				    CD1400_MSVR2_DTR);
			if (bits & TIOCM_DTR)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1,
				    CD1400_MSVR1_RTS);
		} else {
			if (!ISSET(tp->t_cflag, CRTSCTS) &&
			    (bits & TIOCM_RTS) != 0)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1,
				    CD1400_MSVR1_RTS);
			if (bits & TIOCM_DTR)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2,
				    CD1400_MSVR2_DTR);
		}
		break;

	case DMBIC:		/* clear bits */
		if (cy->cy_clock == CY_CLOCK_60) {
			if (!ISSET(tp->t_cflag, CRTSCTS) && (bits & TIOCM_RTS))
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2, 0);
			if (bits & TIOCM_DTR)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1, 0);
		} else {
			if (!ISSET(tp->t_cflag, CRTSCTS) && (bits & TIOCM_RTS))
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR1, 0);
			if (bits & TIOCM_DTR)
				cd_write_reg(sc, cy->cy_chip, CD1400_MSVR2, 0);
		}
		break;
	}
	splx(s);
	return ((howto == DMGET) ? bits : 0);
}

/*
 * Upper-level handler loop (called from timer interrupt?)
 * This routine is common for multiple cards
 */
void
cy_poll(void *arg)
{
	int card, port;
	struct cy_softc *sc;
	struct cy_port *cy;
	struct tty *tp;
	static int counter = 0;
#ifdef CY_DEBUG1
	int did_something;
#endif
	int s = spltty();

	if (cy_events == 0 && ++counter < 200) {
		splx(s);
		goto out;
	}
	cy_events = 0;
	splx(s);

	for (card = 0; card < cy_cd.cd_ndevs; card++) {
		sc = device_lookup_private(&cy_cd, card);
		if (sc == NULL)
			continue;

#ifdef CY_DEBUG1
		sc->sc_poll_count1++;
		did_something = 0;
#endif

		for (port = 0; port < sc->sc_nchannels; port++) {
			cy = &sc->sc_ports[port];
			if ((tp = cy->cy_tty) == NULL || cy->cy_ibuf == NULL ||
			    (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0))
				continue;

			/*
		         * handle received data
		         */
			while (cy->cy_ibuf_rd_ptr != cy->cy_ibuf_wr_ptr) {
				u_char line_stat;
				int chr;

				line_stat = cy->cy_ibuf_rd_ptr[0];
				chr = cy->cy_ibuf_rd_ptr[1];

				if (line_stat &
				    (CD1400_RDSR_BREAK | CD1400_RDSR_FE))
					chr |= TTY_FE;
				if (line_stat & CD1400_RDSR_PE)
					chr |= TTY_PE;

				/*
				 * on an overrun error the data is treated as
				 * good just as it should be.
				 */

#ifdef CY_DEBUG
				aprint_debug_dev(sc->sc_dev,
				    "port %d ttyinput 0x%x\n",
				    port, chr);
#endif

				(*tp->t_linesw->l_rint) (chr, tp);

				s = spltty();	/* really necessary? */
				if ((cy->cy_ibuf_rd_ptr += 2) ==
				    cy->cy_ibuf_end)
					cy->cy_ibuf_rd_ptr = cy->cy_ibuf;
				splx(s);

#ifdef CY_DEBUG1
				did_something = 1;
#endif
			}

			/*
			 * If we don't have any received data in ibuf and
			 * CRTSCTS is on and RTS is turned off, it is time to
			 * turn RTS back on
			 */
			if (ISSET(tp->t_cflag, CRTSCTS)) {
				/*
				 * we can't use cy_modem_control() here as it
				 * doesn't change RTS if RTSCTS is on
				 */
				cd_write_reg(sc, cy->cy_chip, CD1400_CAR,
				    port & CD1400_CAR_CHAN);

				if (cy->cy_clock == CY_CLOCK_60) {
				  if ((cd_read_reg(sc, cy->cy_chip,
				    CD1400_MSVR2) & CD1400_MSVR2_DTR) == 0) {
					cd_write_reg(sc, cy->cy_chip,
					CD1400_MSVR2,CD1400_MSVR2_DTR);
#ifdef CY_DEBUG1
					did_something = 1;
#endif
				  }
				} else {
				  if ((cd_read_reg(sc, cy->cy_chip,
				    CD1400_MSVR1) & CD1400_MSVR1_RTS) == 0) {
					cd_write_reg(sc, cy->cy_chip,
					CD1400_MSVR1,CD1400_MSVR1_RTS);
#ifdef CY_DEBUG1
					did_something = 1;
#endif
				  }
				}
			}

			/*
		         * handle carrier changes
		         */
			s = spltty();
			if (ISSET(cy->cy_flags, CY_F_CARRIER_CHANGED)) {
				int             carrier;

				CLR(cy->cy_flags, CY_F_CARRIER_CHANGED);
				splx(s);

				carrier = ((cy->cy_carrier_stat &
				    CD1400_MSVR2_CD) != 0);

#ifdef CY_DEBUG
				printf("cy_poll: carrier change "
				    "(card %d, port %d, carrier %d)\n",
				    card, port, carrier);
#endif
				if (CY_DIALOUT(tp->t_dev) == 0 &&
				    !(*tp->t_linesw->l_modem)(tp, carrier))
					cy_modem_control(sc, cy,
					    TIOCM_DTR, DMBIC);

#ifdef CY_DEBUG1
				did_something = 1;
#endif
			} else
				splx(s);

			s = spltty();
			if (ISSET(cy->cy_flags, CY_F_START)) {
				CLR(cy->cy_flags, CY_F_START);
				splx(s);

				(*tp->t_linesw->l_start) (tp);

#ifdef CY_DEBUG1
				did_something = 1;
#endif
			} else
				splx(s);

			/* could move this to even upper level... */
			if (cy->cy_fifo_overruns) {
				cy->cy_fifo_overruns = 0;
				/*
				 * doesn't report overrun count, but
				 * shouldn't really matter
				 */
				log(LOG_WARNING, "%s: port %d fifo overrun\n",
				    device_xname(sc->sc_dev), port);
			}
			if (cy->cy_ibuf_overruns) {
				cy->cy_ibuf_overruns = 0;
				log(LOG_WARNING, "%s: port %d ibuf overrun\n",
				    device_xname(sc->sc_dev), port);
			}
		}		/* for(port...) */
#ifdef CY_DEBUG1
		if (did_something && counter >= 200)
			sc->sc_poll_count2++;
#endif
	} /* for(card...) */

	counter = 0;

out:
	callout_reset(&cy_poll_callout, 1, cy_poll, NULL);
}

/*
 * hardware interrupt routine
 */
int
cy_intr(void *arg)
{
	struct cy_softc *sc = arg;
	struct cy_port *cy;
	int cy_chip, stat;
	int int_serviced = 0;

	/*
	 * Check interrupt status of each CD1400 chip on this card
	 * (multiple cards cannot share the same interrupt)
	 */
	for (cy_chip = 0; cy_chip < sc->sc_nchips; cy_chip++) {

		stat = cd_read_reg(sc, cy_chip, CD1400_SVRR);
		if (stat == 0)
			continue;

		if (ISSET(stat, CD1400_SVRR_RXRDY)) {
			u_char save_car, save_rir, serv_type;
			u_char line_stat, recv_data, n_chars;
			u_char *buf_p;

			save_rir = cd_read_reg(sc, cy_chip, CD1400_RIR);
			save_car = cd_read_reg(sc, cy_chip, CD1400_CAR);
			/* enter rx service */
			cd_write_reg(sc, cy_chip, CD1400_CAR, save_rir);

			serv_type = cd_read_reg(sc, cy_chip, CD1400_RIVR);
			cy = &sc->sc_ports[serv_type >> 3];

#ifdef CY_DEBUG1
			cy->cy_rx_int_count++;
#endif

			buf_p = cy->cy_ibuf_wr_ptr;

			if (ISSET(serv_type, CD1400_RIVR_EXCEPTION)) {
				line_stat = cd_read_reg(sc, cy->cy_chip,
				    CD1400_RDSR);
				recv_data = cd_read_reg(sc, cy->cy_chip,
				    CD1400_RDSR);

				if (cy->cy_tty == NULL ||
				    !ISSET(cy->cy_tty->t_state, TS_ISOPEN))
					goto end_rx_serv;

#ifdef CY_DEBUG
				aprint_debug_dev(
				    sc->sc_dev,
				    "port %d recv exception, "
				    "line_stat 0x%x, char 0x%x\n",
				    cy->cy_port_num, line_stat, recv_data);
#endif
				if (ISSET(line_stat, CD1400_RDSR_OE))
					cy->cy_fifo_overruns++;

				*buf_p++ = line_stat;
				*buf_p++ = recv_data;
				if (buf_p == cy->cy_ibuf_end)
					buf_p = cy->cy_ibuf;

				if (buf_p == cy->cy_ibuf_rd_ptr) {
					if (buf_p == cy->cy_ibuf)
						buf_p = cy->cy_ibuf_end;
					buf_p -= 2;
					cy->cy_ibuf_overruns++;
				}
				cy_events = 1;
			} else {/* no exception, received data OK */
				n_chars = cd_read_reg(sc, cy->cy_chip,
				    CD1400_RDCR);

				/* If no tty or not open, discard data */
				if (cy->cy_tty == NULL ||
				    !ISSET(cy->cy_tty->t_state, TS_ISOPEN)) {
					while (n_chars--)
						(void)cd_read_reg(sc,
						    cy->cy_chip, CD1400_RDSR);
					goto end_rx_serv;
				}

#ifdef CY_DEBUG
				aprint_debug_dev(sc->sc_dev,
				    "port %d receive ok %d chars\n",
				    cy->cy_port_num, n_chars);
#endif
				while (n_chars--) {
					*buf_p++ = 0;	/* status: OK */
					/* data byte */
					*buf_p++ = cd_read_reg(sc,
					    cy->cy_chip, CD1400_RDSR);
					if (buf_p == cy->cy_ibuf_end)
						buf_p = cy->cy_ibuf;
					if (buf_p == cy->cy_ibuf_rd_ptr) {
						if (buf_p == cy->cy_ibuf)
							buf_p = cy->cy_ibuf_end;
						buf_p -= 2;
						cy->cy_ibuf_overruns++;
						break;
					}
				}
				cy_events = 1;
			}

			cy->cy_ibuf_wr_ptr = buf_p;

			/* RTS handshaking for incoming data */
			if (ISSET(cy->cy_tty->t_cflag, CRTSCTS)) {
				int bf, msvr;

				bf = buf_p - cy->cy_ibuf_rd_ptr;
				if (bf < 0)
					bf += CY_IBUF_SIZE;

				if (bf > (CY_IBUF_SIZE / 2)) {
					/* turn RTS off */
					if (cy->cy_clock == CY_CLOCK_60)
						msvr = CD1400_MSVR2;
					else
						msvr = CD1400_MSVR1;
					cd_write_reg(sc, cy->cy_chip, msvr, 0);
				}
			}

	end_rx_serv:
			/* terminate service context */
			cd_write_reg(sc, cy->cy_chip, CD1400_RIR,
				     save_rir & 0x3f);
			cd_write_reg(sc, cy->cy_chip, CD1400_CAR, save_car);
			int_serviced = 1;
		} /* if (rx_service...) */
		if (ISSET(stat, CD1400_SVRR_MDMCH)) {
			u_char save_car, save_mir, serv_type, modem_stat;

			save_mir = cd_read_reg(sc, cy_chip, CD1400_MIR);
			save_car = cd_read_reg(sc, cy_chip, CD1400_CAR);
			/* enter modem service */
			cd_write_reg(sc, cy_chip, CD1400_CAR, save_mir);

			serv_type = cd_read_reg(sc, cy_chip, CD1400_MIVR);
			cy = &sc->sc_ports[serv_type >> 3];

#ifdef CY_DEBUG1
			cy->cy_modem_int_count++;
#endif

			modem_stat = cd_read_reg(sc, cy->cy_chip, CD1400_MSVR2);

#ifdef CY_DEBUG
			aprint_debug_dev(sc->sc_dev,
			    "port %d modem line change, new stat 0x%x\n",
			    cy->cy_port_num, modem_stat);
#endif
			if (ISSET((cy->cy_carrier_stat ^ modem_stat), CD1400_MSVR2_CD)) {
				SET(cy->cy_flags, CY_F_CARRIER_CHANGED);
				cy_events = 1;
			}
			cy->cy_carrier_stat = modem_stat;

			/* terminate service context */
			cd_write_reg(sc, cy->cy_chip, CD1400_MIR, save_mir & 0x3f);
			cd_write_reg(sc, cy->cy_chip, CD1400_CAR, save_car);
			int_serviced = 1;
		} /* if (modem_service...) */
		if (ISSET(stat, CD1400_SVRR_TXRDY)) {
			u_char          save_car, save_tir, serv_type,
			                count, ch;
			struct tty     *tp;

			save_tir = cd_read_reg(sc, cy_chip, CD1400_TIR);
			save_car = cd_read_reg(sc, cy_chip, CD1400_CAR);
			/* enter tx service */
			cd_write_reg(sc, cy_chip, CD1400_CAR, save_tir);

			serv_type = cd_read_reg(sc, cy_chip, CD1400_TIVR);
			cy = &sc->sc_ports[serv_type >> 3];

#ifdef CY_DEBUG1
			cy->cy_tx_int_count++;
#endif
#ifdef CY_DEBUG
			aprint_debug_dev(sc->sc_dev, "port %d tx service\n",
			    cy->cy_port_num);
#endif

			/* stop transmitting if no tty or CY_F_STOP set */
			tp = cy->cy_tty;
			if (tp == NULL || ISSET(cy->cy_flags, CY_F_STOP))
				goto txdone;

			count = 0;
			if (ISSET(cy->cy_flags, CY_F_SEND_NUL)) {
				cd_write_reg(sc, cy->cy_chip, CD1400_TDR, 0);
				cd_write_reg(sc, cy->cy_chip, CD1400_TDR, 0);
				count += 2;
				CLR(cy->cy_flags, CY_F_SEND_NUL);
			}
			if (tp->t_outq.c_cc > 0) {
				SET(tp->t_state, TS_BUSY);
				while (tp->t_outq.c_cc > 0 &&
				    count < CD1400_TX_FIFO_SIZE) {
					ch = getc(&tp->t_outq);
					/*
					 * remember to double NUL characters
					 * because embedded transmit commands
					 * are enabled
					 */
					if (ch == 0) {
						if (count >= CD1400_TX_FIFO_SIZE - 2) {
							SET(cy->cy_flags, CY_F_SEND_NUL);
							break;
						}
						cd_write_reg(sc, cy->cy_chip,
						    CD1400_TDR, ch);
						count++;
					}
					cd_write_reg(sc, cy->cy_chip,
					    CD1400_TDR, ch);
					count++;
				}
			} else {
				/*
				 * no data to send -- check if we should
				 * start/stop a break
				 */
				/*
				 * XXX does this cause too much delay before
				 * breaks?
				 */
				if (ISSET(cy->cy_flags, CY_F_START_BREAK)) {
					cd_write_reg(sc, cy->cy_chip,
					    CD1400_TDR, 0);
					cd_write_reg(sc, cy->cy_chip,
					    CD1400_TDR, 0x81);
					CLR(cy->cy_flags, CY_F_START_BREAK);
				}
				if (ISSET(cy->cy_flags, CY_F_END_BREAK)) {
					cd_write_reg(sc, cy->cy_chip,
					    CD1400_TDR, 0);
					cd_write_reg(sc, cy->cy_chip,
					    CD1400_TDR, 0x83);
					CLR(cy->cy_flags, CY_F_END_BREAK);
				}
			}

			if (tp->t_outq.c_cc == 0) {
		txdone:
				/*
				 * No data to send or requested to stop.
				 * Disable transmit interrupt
				 */
				cd_write_reg(sc, cy->cy_chip, CD1400_SRER,
				     cd_read_reg(sc, cy->cy_chip, CD1400_SRER)
				     & ~CD1400_SRER_TXRDY);
				CLR(cy->cy_flags, CY_F_STOP);
				CLR(tp->t_state, TS_BUSY);
			}
			if (tp->t_outq.c_cc <= tp->t_lowat) {
				SET(cy->cy_flags, CY_F_START);
				cy_events = 1;
			}
			/* terminate service context */
			cd_write_reg(sc, cy->cy_chip, CD1400_TIR, save_tir & 0x3f);
			cd_write_reg(sc, cy->cy_chip, CD1400_CAR, save_car);
			int_serviced = 1;
		}		/* if (tx_service...) */
	}			/* for(...all CD1400s on a card) */

	/* ensure an edge for next interrupt */
	bus_space_write_1(sc->sc_memt, sc->sc_bsh,
			CY_CLEAR_INTR << sc->sc_bustype, 0);
	return int_serviced;
}

/*
 * subroutine to enable CD1400 transmitter
 */
void
cy_enable_transmitter(struct cy_softc *sc, struct cy_port *cy)
{
	int s = spltty();
	cd_write_reg(sc, cy->cy_chip, CD1400_CAR,
	    cy->cy_port_num & CD1400_CAR_CHAN);
	cd_write_reg(sc, cy->cy_chip, CD1400_SRER,
	    cd_read_reg(sc, cy->cy_chip, CD1400_SRER) | CD1400_SRER_TXRDY);
	splx(s);
}

/*
 * Execute a CD1400 channel command
 */
void
cd1400_channel_cmd(struct cy_softc *sc, struct cy_port *cy, int cmd)
{
	u_int waitcnt = 5 * 8 * 1024;	/* approx 5 ms */

#ifdef CY_DEBUG
	printf("c1400_channel_cmd cy %p command 0x%x\n", cy, cmd);
#endif

	/* wait until cd1400 is ready to process a new command */
	while (cd_read_reg(sc, cy->cy_chip, CD1400_CCR) != 0 && waitcnt-- > 0);

	if (waitcnt == 0)
		log(LOG_ERR, "%s: channel command timeout\n",
		    device_xname(sc->sc_dev));

	cd_write_reg(sc, cy->cy_chip, CD1400_CCR, cmd);
}

/*
 * Compute clock option register and baud rate register values
 * for a given speed. Return 0 on success, -1 on failure.
 *
 * The error between requested and actual speed seems
 * to be well within allowed limits (less than 3%)
 * with every speed value between 50 and 150000 bps.
 */
int
cy_speed(speed_t speed, int *cor, int *bpr, int cy_clock)
{
	int c, co, br;

	if (speed < 50 || speed > 150000)
		return -1;

	for (c = 0, co = 8; co <= 2048; co <<= 2, c++) {
		br = (cy_clock + (co * speed) / 2) / (co * speed);
		if (br < 0x100) {
			*bpr = br;
			*cor = c;
			return 0;
		}
	}

	return -1;
}
