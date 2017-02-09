/*	$NetBSD: cd18xxvar.h,v 1.5 2014/11/15 19:18:18 christos Exp $	*/

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

/*
 * cd18xxvar.h:  header file for cirrus-logic CL-CD180/CD1864/CD1865 8
 * port serial chip.
 */

#include <sys/tty.h>

/* allocated per-serial port */
struct cdtty_port {
	struct	tty		*p_tty;

	int			p_swflags;	/* TIOCFLAG_SOFTCAR, etc. */
	int			p_defspeed;	/* default speed */
	int			p_defcflag;	/* default termios cflag */

	u_int			p_r_hiwat;	/* high water mark */
	u_int			p_r_lowat;	/* low water mark */
	u_char *volatile	p_rbget;	/* ring buffer get ptr */
	u_char *volatile	p_rbput;	/* ring buffer put ptr */
	volatile u_int		p_rbavail;	/* size available */
	u_char			*p_rbuf;	/* ring buffer */
	u_char			*p_ebuf;	/* end of ring buffer */

	u_char *		p_tba;		/* transmit buffer address */
	u_int			p_tbc,		/* transmit byte count */
				p_heldtbc;	/* held tbc; waiting for tx */
#define CDTTY_RING_SIZE	2048

	u_int			p_parityerr,	/* number of parity errors */
				p_frameerr,	/* number of framing errors */
				p_overflows,	/* number of overruns */
				p_floods;	/* number of rbuf floods */

	volatile u_char		p_break,	/* current break status */
				p_needbreak,	/* need to generate a break */
				p_rx_flags,	/* software state */
#define	RX_TTY_BLOCKED		0x01
#define	RX_TTY_OVERFLOWED	0x02
#define	RX_IBUF_BLOCKED		0x04
#define	RX_IBUF_OVERFLOWED	0x08
#define	RX_ANY_BLOCK		0x0f
				p_rx_ready,	/* soft rx interrupt ready */
				p_rx_busy,
				p_tx_done,	/* soft tx interrupt ready */
				p_tx_busy,
				p_tx_stopped,
				p_st_check,	/* soft modem interrupt ready */
				p_heldchange;	/* waiting to update regs */

	/*
	 * cd18xx channel registers we keep a copy of, for writing in
	 * loadchannelregs().
	 */
	u_char			p_srer,		/* service request enable */
				p_msvr,		/* modem signal value */
				p_msvr_cts, p_msvr_rts, p_msvr_dcd,
				p_msvr_mask, p_msvr_active, p_msvr_delta,
				p_cor1,		/* channel option reg 1 */
				p_cor2,		/* channel option reg 2 */
				p_cor3,		/* channel option reg 3 */
				p_mcor1,	/* modem option reg 1 */
				p_mcor1_dtr,
				p_rbprh,	/* recv bps high */
				p_rbprl,	/* recv bps low */
				p_tbprh,	/* xmit bps high */
				p_tbprl,	/* xmit bps low */
				p_chanctl;	/* chanctl command */
};

/* softc allocated per-cd18xx */
struct cd18xx_softc {
	device_t		sc_dev;

	/* tag and handle for our registers (128 bytes) */
	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_handle;

	/*
	 * cd18xx has weird interrupt acknowledgement and configuration,
	 * so we have to defer this to our parent.  this function must
	 * do whatever is required to genereate *iack signals that are
	 * required for the cd180.  this probably also depends on the
	 * values of the sc_rsmr, sc_tsmr and sc_msmr variables.  the
	 * function is called with the provided argument, and with any
	 * of the 4 #defines below, depending on the ack needing to be
	 * generated.
	 */
	u_char			(*sc_ackfunc)(void *, int);
#define CD18xx_INTRACK_MxINT	0x01		/* modem interrupt */
#define CD18xx_INTRACK_TxINT	0x02		/* tx interrupt */
#define CD18xx_INTRACK_RxINT	0x04		/* rx (good data) interrupt */
#define CD18xx_INTRACK_REINT	0x08		/* rx (exception) interrupt */
	void			*sc_ackfunc_arg;

	u_char			sc_rsmr;
	u_char			sc_tsmr;
	u_char			sc_msmr;

	u_int			sc_osc;

	/*
	 * everything above here needs to be setup by our caller, and
	 * everything below here is setup by the generic cd18xx code.
	 */
	u_int			sc_chip_id;	/* unique per-cd18xx value */
	void			*sc_si;		/* softintr(9) cookie */

	struct cdtty_port	sc_ports[8];

	u_char			sc_pprh;
	u_char			sc_pprl;
};

/* hard interrupt, to be configured by our caller */
int cd18xx_hardintr(void *);

/* main attach routine called by the high level driver */
void cd18xx_attach(struct cd18xx_softc *);

/*
 * device minor layout has bit 19 for dialout and bits 0..18 for the unit.
 * the first 3 bits of the unit are the channel number inside a single
 * cd18xx instance, and the remaining bits indicate the instance number.
 */
#define CD18XX_TTY(x)		TTUNIT(x)
#define CD18XX_CHANNEL(x)	(TTUNIT(x) & 7)
#define CD18XX_INSTANCE(x)	(TTUNIT(x) >> 3)
#define CD18XX_DIALOUT(x)	TTDIALOUT(x)

/* short helpers for read/write */
#define cd18xx_read(sc, o)		\
	bus_space_read_1((sc)->sc_tag, (sc)->sc_handle, o)
#define cd18xx_read_multi(sc, o, b, c)	\
	bus_space_read_multi_1((sc)->sc_tag, (sc)->sc_handle, o, b, c)

#define cd18xx_write(sc, o, v)		\
	bus_space_write_1((sc)->sc_tag, (sc)->sc_handle, o, v)
#define cd18xx_write_multi(sc, o, b, c)	\
	bus_space_write_multi_1((sc)->sc_tag, (sc)->sc_handle, o, b, c)

/* set the current channel */
#define cd18xx_set_car(sc, c)		\
do { \
	bus_space_write_1((sc)->sc_tag, (sc)->sc_handle, CD18xx_CAR, c); \
	delay(1); \
} while (0)

/* get the current channel */
#define cd18xx_get_gscr1_channel(sc)	\
	((bus_space_read_1((sc)->sc_tag, (sc)->sc_handle, CD18xx_GSCR1) >> 2)&7)
