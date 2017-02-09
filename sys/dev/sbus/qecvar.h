/*	$NetBSD: qecvar.h,v 1.14 2009/09/19 04:48:18 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

struct qec_softc {
	device_t sc_dev;		/* us as a device */
	bus_space_tag_t	sc_bustag;	/* bus & DMA tags */
	bus_dma_tag_t	sc_dmatag;
	struct	openprom_intr *sc_intr;	/* interrupt info */

	bus_space_handle_t sc_regs;	/* QEC registers */
	int	sc_nchannels;		/* # of channels on board */
	int	sc_burst;		/* DVMA burst size in effect */
	void *	sc_buffer;		/* VA of the buffer we provide */
	int	sc_bufsiz;		/* Size of buffer */

	u_int	sc_msize;		/* QEC buffer offset per channel */
	u_int	sc_rsize;		/* QEC buffer size for receive */
};

struct qec_ring {
	/* Ring Descriptors */
	void *		rb_membase;	/* Packet buffer: CPU address */
	bus_addr_t	rb_dmabase;	/* Packet buffer: DMA address */
	struct	qec_xd	*rb_txd;	/* Transmit descriptors */
	bus_addr_t	rb_txddma;	/* DMA address of same */
	struct	qec_xd	*rb_rxd;	/* Receive descriptors */
	bus_addr_t	rb_rxddma;	/* DMA address of same */
	uint8_t		*rb_txbuf;	/* Transmit buffers */
	uint8_t		*rb_rxbuf;	/* Receive buffers */
	int		rb_ntbuf;	/* # of transmit buffers */
	int		rb_nrbuf;	/* # of receive buffers */

	/* Ring Descriptor state */
	int	rb_tdhead, rb_tdtail;
	int	rb_rdtail;
	int	rb_td_nbusy;
};

void	qec_meminit(struct qec_ring *, unsigned int);
