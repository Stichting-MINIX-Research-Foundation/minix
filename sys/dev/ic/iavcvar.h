/*	$NetBSD: iavcvar.h,v 1.5 2012/10/27 17:18:20 chs Exp $	*/

/*
 * Copyright (c) 2001-2003 Cubical Solutions Ltd. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * capi/iavc/iavc.h	The AVM ISDN controllers' common declarations.
 *
 * $FreeBSD: src/sys/i4b/capi/iavc/iavc.h,v 1.1.2.1 2001/08/10 14:08:34 obrien Exp $
 */

#include <netisdn/i4b_capi.h>

/*
//  iavc_softc_t
//      The software context of one AVM T1 controller.
*/

#define IAVC_IO_BASES 1
#define IAVC_DMA_SIZE (128 + 2048)

typedef struct iavc_softc {
    device_t	sc_dev;
    capi_softc_t	sc_capi;

    bus_space_handle_t	sc_mem_bh;
    bus_space_tag_t	sc_mem_bt;

    bus_space_handle_t	sc_io_bh;
    bus_space_tag_t	sc_io_bt;

    bus_dma_tag_t	dmat;

    bus_dmamap_t	tx_map;
    bus_dmamap_t	rx_map;

    bus_dma_segment_t	txseg;
    bus_dma_segment_t	rxseg;
    int			ntxsegs, nrxsegs;

    uint32_t		sc_unit;
    uint32_t		sc_intr;

    int32_t		sc_state;
#define IAVC_DOWN       0
#define IAVC_POLL       1
#define IAVC_INIT       2
#define IAVC_UP         3
    uint32_t		sc_blocked;
    uint32_t		sc_dma;
    uint32_t		sc_t1;

    u_int32_t		sc_csr;

    void *		sc_sendbuf;
    void *		sc_recvbuf;

    u_int32_t		sc_recv1;

    struct ifqueue	sc_txq;
} iavc_softc_t;

/*
//  {b1,b1dma,t1}_{detect,reset}
//      Routines to detect and manage the specific type of card.
*/

int      iavc_b1_detect(iavc_softc_t *sc);
void     iavc_b1_disable_irq(iavc_softc_t *sc);
void     iavc_b1_reset(iavc_softc_t *sc);

int      iavc_b1dma_detect(iavc_softc_t *sc);
void     iavc_b1dma_reset(iavc_softc_t *sc);

int      iavc_t1_detect(iavc_softc_t *sc);
void     iavc_t1_disable_irq(iavc_softc_t *sc);
void     iavc_t1_reset(iavc_softc_t *sc);


/*
//  iavc_handle_intr
//      Interrupt handler, called by the bus specific interrupt routine
//      in iavc_<bustype>.c module.
//
//  iavc_load
//      CAPI callback. Resets device and loads firmware.
//
//  iavc_register
//      CAPI callback. Registers an application id.
//
//  iavc_release
//      CAPI callback. Releases an application id.
//
//  iavc_send
//      CAPI callback. Sends a CAPI message. A B3_DATA_REQ message has
//      m_next point to a data mbuf.
*/

int iavc_handle_intr(iavc_softc_t *);
int iavc_load(capi_softc_t *, int, u_int8_t *);
int iavc_register(capi_softc_t *, int, int);
int iavc_release(capi_softc_t *, int);
int iavc_send(capi_softc_t *, struct mbuf *);

#ifdef notyet
extern void b1isa_setup_irq(struct iavc_softc *sc);
#endif
