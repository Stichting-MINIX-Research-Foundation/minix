/*	$NetBSD: clmpccvar.h,v 1.13 2012/10/27 17:18:20 chs Exp $ */

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

#ifndef __clmpccvar_h
#define __clmpccvar_h


/* Buffer size for character buffer */
#define	CLMPCC_RING_SIZE	512

/* How many channels per chip */
#define CLMPCC_NUM_CHANS	4

/* Reasons for calling the MD code's iack hook function */
#define CLMPCC_IACK_MODEM	0
#define CLMPCC_IACK_RX		1
#define CLMPCC_IACK_TX		2


struct clmpcc_softc;

/*
 * Each channel is represented by one of the following structures
 */
struct clmpcc_chan {
	struct tty	*ch_tty;	/* This channel's tty structure */
	struct clmpcc_softc *ch_sc;	/* Pointer to chip's softc structure */
	u_char		ch_car;		/* Channel number (CD2400_REG_CAR) */
	u_char		ch_openflags;	/* Persistent TIOC flags */
	volatile u_short ch_flags;	/* Various channel-specific flags */
#define	CLMPCC_FLG_IS_CONSOLE	0x0001	/* Channel is system console */
#define CLMPCC_FLG_START_BREAK 	0x0002
#define CLMPCC_FLG_END_BREAK 	0x0004
#define CLMPCC_FLG_FIFO_CLEAR	0x0008
#define CLMPCC_FLG_UPDATE_PARMS	0x0010
#define CLMPCC_FLG_NEED_INIT	0x0020

	u_char		ch_tx_done;

	u_char		ch_control;

	/* New port parameters wait here until written by the Tx ISR */
	u_char		ch_tcor;
	u_char		ch_tbpr;
	u_char		ch_rcor;
	u_char		ch_rbpr;
	u_char		ch_cor1;
	u_char		ch_cor2;
	u_char		ch_cor3;
	u_char		ch_cor4;	/* Current Rx Fifo threshold */
	u_char		ch_cor5;

	u_int8_t	*ch_ibuf;	/* Start of input ring buffer */
	u_int8_t	*ch_ibuf_end;	/* End of input ring buffer */
	u_int8_t	*ch_ibuf_rd;	/* Input buffer tail (reader) */
	u_int8_t	*ch_ibuf_wr;	/* Input buffer head (writer) */

	u_int8_t	*ch_obuf_addr;	/* Output buffer address */
	u_int		ch_obuf_size;	/* Output buffer size (in bytes) */
};


struct clmpcc_softc {
	device_t	sc_dev;

	/*
	 * The bus/MD-specific attachment code must initialise the
	 * following fields before calling 'clmpcc_attach_subr()'.
	 */
	bus_space_tag_t	sc_iot;		/* Tag for parent bus */
	bus_space_handle_t sc_ioh;	/* Handle for chip's regs */
	void		*sc_data;	/* MD-specific data */
	int		sc_clk;		/* Clock-rate, in Hz */
	struct evcnt	*sc_evcnt;	/* Parent Event Counter (or NULL) */
	u_char		sc_vector_base;	/* Vector base reg, or 0 for auto */
	u_char		sc_rpilr;	/* Receive Priority Interrupt Level */
	u_char		sc_tpilr;	/* Transmit Priority Interrupt Level */
	u_char		sc_mpilr;	/* Modem Priority Interrupt Level */
	int		sc_swaprtsdtr;	/* Non-zero if RTS and DTR swapped */
	u_int		sc_byteswap;	/* One of the following ... */
#define CLMPCC_BYTESWAP_LOW	0x00	/* *byteswap pin is low */
#define CLMPCC_BYTESWAP_HIGH	0x03	/* *byteswap pin is high */

	void		*sc_softintr_cookie;

	/* Called when an interrupt has to be acknowledged in polled mode. */
	void		(*sc_iackhook)(struct clmpcc_softc *, int);

	/*
	 * No user-serviceable parts below
	 */
	struct clmpcc_chan sc_chans[CLMPCC_NUM_CHANS];
};

extern void	clmpcc_attach(struct clmpcc_softc *);
extern int	clmpcc_cnattach(struct clmpcc_softc *, int, int);
extern int	clmpcc_rxintr(void *);
extern int	clmpcc_txintr(void *);
extern int	clmpcc_mdintr(void *);
extern void 	clmpcc_softintr(void *);

#endif	/* __clmpccvar_h */
