/*	$NetBSD: gtmpscvar.h,v 1.8 2010/04/28 13:51:56 kiyohara Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * gtmpscvar.h - includes for gtmpsctty GT-64260 UART-mode serial driver
 *
 * creation	Mon Apr  9 19:54:33 PDT 2001	cliff
 */
#ifndef _DEV_MARVELL_GTMPSCVAR_H
#define	_DEV_MARVELL_GTMPSCVAR_H

#include "opt_marvell.h"

#ifndef GT_MPSC_DEFAULT_BAUD_RATE
#define	GT_MPSC_DEFAULT_BAUD_RATE	115200
#endif
#define	GTMPSC_CLOCK_DIVIDER            8
#define	GTMPSC_MMCR_HI_TCDV_DEFAULT     GTMPSC_MMCR_HI_TCDV_8X
#define	GTMPSC_MMCR_HI_RCDV_DEFAULT     GTMPSC_MMCR_HI_RCDV_8X
#define	BRG_BCR_CDV_MAX                 0xffff

/*
 * gtmpsc_poll_dmapage_t - used for MPSC getchar/putchar polled console
 *
 *	sdma descriptors must be 16 byte aligned
 *	sdma RX buffer pointers must be 8 byte aligned
 */
#define	GTMPSC_NTXDESC 64
#define	GTMPSC_NRXDESC 64
#define	GTMPSC_TXBUFSZ 16
#define	GTMPSC_RXBUFSZ 16

typedef struct gtmpsc_polltx {
	sdma_desc_t txdesc;
	unsigned char txbuf[GTMPSC_TXBUFSZ];
} gtmpsc_polltx_t;

typedef struct gtmpsc_pollrx {
	sdma_desc_t rxdesc;
	unsigned char rxbuf[GTMPSC_RXBUFSZ];
} gtmpsc_pollrx_t;

typedef struct {
	gtmpsc_polltx_t tx[GTMPSC_NTXDESC];
	gtmpsc_pollrx_t rx[GTMPSC_NRXDESC];
} gtmpsc_poll_sdma_t;


#include <sys/timepps.h>
#include <sys/tty.h>

typedef struct gtmpsc_softc {
	device_t sc_dev;
	int sc_unit;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_mpsch;
	bus_space_handle_t sc_sdmah;
	bus_dma_tag_t sc_dmat;
	gtmpsc_poll_sdma_t *sc_poll_sdmapage;
	bus_dmamap_t sc_rxdma_map;
	bus_dmamap_t sc_txdma_map;
	int sc_brg;				/* using Baud Rate Generator */
	int sc_baudrate;
	tcflag_t sc_cflag;
	void *sc_si;				/* softintr cookie */
	struct tty *sc_tty;			/* our tty */
	uint32_t sc_flags;
#define	GTMPSC_CONSOLE		(1 << 0)
#define	GTMPSC_KGDB		(1 << 1)

	volatile int sc_rcvcnt;			/* byte count of RX buffer */
	volatile int sc_roffset;		/* offset of RX buffer */
	volatile int sc_rcvrx;			/* receive rx xfer index */
	volatile int sc_rcvdrx;			/* received rx xfer index */
	volatile int sc_readrx;			/* read rx xfer index */
	volatile int sc_nexttx;			/* "next" tx xfer index */
	volatile int sc_lasttx;			/* "last" tx xfer index */
	volatile u_char sc_rx_ready;
	volatile u_char sc_tx_busy;
	volatile u_char sc_tx_done;
	volatile u_char sc_tx_stopped;
	volatile u_char sc_heldchange;		/* new params wait for output */
	u_char *sc_tba;				/* Tx buf ptr */
	u_int sc_tbc;				/* Tx buf cnt */
	u_int sc_heldtbc;

	kmutex_t sc_lock;
	struct pps_state sc_pps_state;
} gtmpsc_softc_t;

/* Make receiver interrupt 8 times a second */
/* There are 10 bits in a frame */
#define	GTMPSC_MAXIDLE(baudrate) ((baudrate) / (10 * 8))


static __inline int
compute_cdv(unsigned int baud)
{
	unsigned int cdv;

	if (baud == 0)
		return 0;
	cdv = (GT_MPSC_FREQUENCY / (baud * GTMPSC_CLOCK_DIVIDER) + 1) / 2 - 1;
	if (cdv > BRG_BCR_CDV_MAX)
		return -1;
	return cdv;
}


int  gtmpsc_intr(void *);

#ifdef MPSC_CONSOLE
extern gtmpsc_softc_t gtmpsc_cn_softc;

int gtmpsccnattach(bus_space_tag_t, bus_dma_tag_t, bus_addr_t, int, int, int,
		   tcflag_t);
#endif

#endif /* _DEV_MARVELL_GTPSCVAR_H */
