/*	$NetBSD: msvar.h,v 1.9 2009/05/12 14:46:39 cegger Exp $	*/

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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#define	MS_RX_RING_SIZE	256
#define MS_RX_RING_MASK (MS_RX_RING_SIZE-1)
/*
 * Output buffer.  Only need a few chars.
 */
#define	MS_TX_RING_SIZE	16
#define MS_TX_RING_MASK (MS_TX_RING_SIZE-1)
/*
 * mouse serial line speed defaults to 1200 bps.
 */
#define MS_DEFAULT_BPS 	1200

/*
 * Mouse state.  A Mouse Systems mouse is a fairly simple device,
 * producing five-byte blobs of the form:
 *
 *	b dx dy dx dy
 *
 * where b is the button state, encoded as 0x80|(~buttons)---there are
 * three buttons (4=left, 2=middle, 1=right)---and dx,dy are X and Y
 * delta values, none of which have are in [0x80..0x87].  (This lets
 * us sync up with the mouse after an error.)
 */
struct ms_softc {
	device_t ms_dev;		/* required first: base device */
	union {
		void *msu_priv;
		struct zs_chanstate *msu_cs;
	} ms_u;
#define ms_priv	ms_u.msu_priv
#define ms_cs	ms_u.msu_cs

	/*
	 * The deviopen and deviclose routines are provided
	 * by the lower level driver and used as a back door
	 * when opening and closing the internal device.
	 */
	int	(*ms_deviopen)	(device_t, int);
	int	(*ms_deviclose)	(device_t, int);

	/* Flags to communicate with ms_softintr() */
	volatile int ms_intr_flags;
#define	INTR_RX_OVERRUN 1
#define INTR_TX_EMPTY   2
#define INTR_ST_CHECK   4

	/*
	 * The receive ring buffer.
	 */
	u_int	ms_rbget;	/* ring buffer `get' index */
	volatile u_int	ms_rbput;	/* ring buffer `put' index */
	u_short	ms_rbuf[MS_RX_RING_SIZE]; /* rr1, data pairs */

	/*
	 * State of input translator
	 */
	short	ms_byteno;		/* input byte number, for decode */
	char	ms_mb;			/* mouse button state */
	char	ms_ub;			/* user button state */
	int	ms_dx;			/* delta-x */
	int	ms_dy;			/* delta-y */

	/*
	 * State of upper interface.
	 */
	volatile int ms_ready;		/* event queue is ready */
	struct	evvar ms_events;	/* event queue state */

	device_t ms_wsmousedev;
};

/* front-end call back for mouse input */
void ms_input(struct ms_softc *, int c);
