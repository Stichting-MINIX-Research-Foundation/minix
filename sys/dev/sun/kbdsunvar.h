/*	$NetBSD: kbdsunvar.h,v 1.7 2009/05/12 14:46:39 cegger Exp $ */

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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Keyboard driver - middle layer for sun keyboard off a serial line.
 * This code is used by kbd_zs and sunkbd (line discipline) drivers.
 */


/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#define	KBD_RX_RING_SIZE	256
#define KBD_RX_RING_MASK	(KBD_RX_RING_SIZE - 1)

/*
 * Output buffer.  Only need a few chars.
 */
#define	KBD_TX_RING_SIZE	16
#define KBD_TX_RING_MASK	(KBD_TX_RING_SIZE - 1)

/*
 * Keyboard serial line speed defaults to 1200 bps.
 */
#define KBD_DEFAULT_BPS		1200

#define KBD_RESET_TIMO		1000 /* mS. */


struct kbd_sun_softc {
	/* upper layer (also inherits device_t) */
	struct kbd_softc k_kbd;

	union {
		void *ku_priv;
		struct zs_chanstate *ku_cs;
	} k_u;
#define k_priv	k_u.ku_priv
#define	k_cs	k_u.ku_cs

	/*
	 * The deviopen and deviclose routines are provided by the
	 * underlying lower level driver and used as a back door when
	 * opening and closing the internal device.
	 */
	int (*k_deviopen)(device_t, int);
	int (*k_deviclose)(device_t, int);

	/*
	 * Callback provided by the lower layer (actual device driver).
	 * Middle layer uses it to send commands to sun keyboard.
	 */
	void (*k_write_data)(struct kbd_sun_softc *, int);

	/* Was initialized once. */
	int k_isopen;

	/*
	 * Magic sequence stuff (Stop-A, aka L1-A).
	 * XXX: convert to cnmagic(9).
	 */
	char k_magic1_down;
	u_char k_magic1;	/* L1 */
	u_char k_magic2;	/* A */

	/* Expecting ID or layout byte from keyboard */
	int k_expect;
#define	KBD_EXPECT_IDCODE	1
#define	KBD_EXPECT_LAYOUT	2

	/* Flags to communicate with kbd_softint() */
	volatile int k_intr_flags;
#define	INTR_RX_OVERRUN 1
#define INTR_TX_EMPTY   2
#define INTR_ST_CHECK   4

	/* Transmit state */
	volatile int k_txflags;
#define	K_TXBUSY 1
#define K_TXWANT 2

	/*
	 * The transmit ring buffer.
	 */
	volatile u_int k_tbget;	/* transmit buffer `get' index */
	volatile u_int k_tbput;	/* transmit buffer `put' index */
	u_char k_tbuf[KBD_TX_RING_SIZE]; /* data */

	/*
	 * The receive ring buffer.
	 */
	u_int k_rbget;		/* ring buffer `get' index */
	volatile u_int k_rbput; /* ring buffer `put' index */
	u_short	k_rbuf[KBD_RX_RING_SIZE]; /* rr1, data pairs */
};

/* Middle layer methods exported to the upper layer. */
extern const struct kbd_ops kbd_ops_sun;

/* Methods for the lower layer to call. */
extern int	kbd_sun_input(struct kbd_sun_softc *k, int);
extern void	kbd_sun_output(struct kbd_sun_softc *k, int c);
extern void	kbd_sun_start_tx(struct kbd_sun_softc *k);
