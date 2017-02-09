/*	$NetBSD: mvsatavar.h,v 1.2 2010/07/13 12:53:42 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MVSATAVAR_H_
#define _MVSATAVAR_H_

struct mvsata_product {
	int vendor;
	int model;
	int hc;
	int port;
	int generation;
	int flags;
};

#define MVSATA_EDMAQ_LEN	32	/* keep compatibility to gen1 */
#define MVSATA_EDMAQ_INC(i)	((i) = ((i) + 1) % MVSATA_EDMAQ_LEN)
#define MVSATA_HC_MAX		2
#define MVSATA_PORT_MAX		4
#define MVSATA_CHANNEL_MAX	(MVSATA_HC_MAX * MVSATA_PORT_MAX)


struct mvsata_port;

union mvsata_crqb {
	struct crqb crqb;
	struct crqb_gen2e crqb_gen2e;
};

struct _fix_phy_param {
	uint32_t pre_amps;		/* Pre/SignalAmps */

	void (*_fix_phy)(struct mvsata_port *);
};

struct mvsata_port {
	struct ata_channel port_ata_channel;

	int port;
	struct mvsata_hc *port_hc;

	enum {
		nodma,
		dma,
		queued,
		ncq,
	} port_edmamode;

	int port_quetagidx;		/* Host Queue Tag valiable */

	int port_prev_erqqop;		/* previous Req Queue Out-Pointer */
	bus_dma_tag_t port_dmat;
	union mvsata_crqb *port_crqb;	/* EDMA Command Request Block */
	bus_dmamap_t port_crqb_dmamap;
	struct crpb *port_crpb;		/* EDMA Command Response Block */
	bus_dmamap_t port_crpb_dmamap;
	struct eprd *port_eprd;		/* EDMA Phy Region Description Table */
	bus_dmamap_t port_eprd_dmamap;
	struct {
		struct ata_xfer *xfer;		/* queued xfer */
		bus_dmamap_t data_dmamap;	/* DMA data buffer */
		bus_size_t eprd_offset;		/* offset of ePRD buffer */
		struct eprd *eprd;		/* ePRD buffer */
	} port_reqtbl[MVSATA_EDMAQ_LEN];

	bus_space_tag_t port_iot;
	bus_space_handle_t port_ioh;
	bus_space_handle_t port_sata_scontrol;	/* SATA Interface control reg */
	bus_space_handle_t port_sata_serror;	/* SATA Interface error reg */
	bus_space_handle_t port_sata_sstatus;	/* SATA Interface status reg */
	struct ata_queue port_ata_queue;

	struct _fix_phy_param _fix_phy_param;
};

struct mvsata_hc {
	int hc;
	struct mvsata_softc *hc_sc;

	bus_space_tag_t hc_iot;		/* Tag for SATAHC Arbiter */
	bus_space_handle_t hc_ioh;	/* Handle for SATAHC Arbiter */

	struct mvsata_port *hc_ports[MVSATA_CHANNEL_MAX];
};

struct mvsata_softc {
	struct wdc_softc sc_wdcdev;	/* common wdc definitions */

	int sc_model;
	int sc_rev;
	enum {
		gen_unknown = 0,
		gen1,
		gen2,
		gen2e
	} sc_gen;			/* Generation for LSI */
	int sc_hc;			/* number of host controller */
	int sc_port;			/* number of port/host */

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;

	struct wdc_regs *sc_wdc_regs;
	struct ata_channel *sc_ata_channels[MVSATA_CHANNEL_MAX];
	struct mvsata_hc sc_hcs[MVSATA_HC_MAX];

	int sc_flags;
#define MVSATA_FLAGS_PCIE	(1 << 0)

	void (*sc_edma_setup_crqb)(struct mvsata_port *, int, int,
				   struct ata_bio *);
	void (*sc_enable_intr)(struct mvsata_port *, int);
};

int mvsata_attach(struct mvsata_softc *, struct mvsata_product *,
		  int (*mvsata_sreset)(struct mvsata_softc *),
		  int (*mvsata_misc_reset)(struct mvsata_softc *), int);
int mvsata_intr(struct mvsata_hc *);
int mvsata_error(struct mvsata_port *);

#endif	/* _MVSATAVAR_H_ */
