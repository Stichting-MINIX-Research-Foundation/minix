/*	$NetBSD: smc90cx6var.h,v 1.11 2012/10/27 17:18:22 chs Exp $	*/

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * BAH (SMC 8bit ARCnet chipset) k/dpi
 *
 * The SMC 8bit ARCnet chip family uses a register and a memory window, which
 * we get passed via bus_space_tags and bus_space_handles.
 *
 * As the reset functionality differs between the Amiga boards (using the
 * 90c26 chip) and middle-aged ISA boards (using the 90c56 chip), we have
 * a sc_reset callback function in the softc, which does a stop function
 * (reset and leave dead) or a reset function depending on whether the 2nd
 * parameter is 0 or 1.
 */

#ifndef _SMC90CX6VAR_H_
#define _SMC90CX6VAR_H_

#include <sys/callout.h>

struct bah_softc {
	device_t		sc_dev;
	struct	arccom		sc_arccom;	/* Common arcnet structures */
	bus_space_tag_t		sc_bst_r, sc_bst_m;
	bus_space_handle_t 	sc_regs, sc_mem;
	void 	(*sc_reset)(struct bah_softc *, int);
	void 	*sc_rxcookie;		/* softcallback cookies */
	void	*sc_txcookie;
	struct callout sc_recon_ch;
	u_long	sc_recontime;		/* seconds only, I'm lazy */
	u_long	sc_reconcount;		/* for the above */
	u_long	sc_reconcount_excessive; /* for the above */
#define ARC_EXCESSIVE_RECONS 20
#define ARC_EXCESSIVE_RECONS_REWARN 400
	u_char	sc_intmask;
	u_char	sc_rx_act;		/* 2..3 */
	u_char	sc_tx_act;		/* 0..1 */
	u_char	sc_rx_fillcount;
	u_char	sc_tx_fillcount;
	u_char	sc_broadcast[2];	/* is it a broadcast packet? */
	u_char	sc_retransmits[2];	/* unused at the moment */
};

void	bah_attach_subr(struct bah_softc *);
int	bahintr(void *);

#endif
