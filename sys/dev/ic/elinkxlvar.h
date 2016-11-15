/*	$NetBSD: elinkxlvar.h,v 1.25 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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

#include <sys/rndsource.h>

/*
 * Ethernet software status per interface.
 */
struct ex_softc {
	device_t sc_dev;
	device_suspensor_t sc_suspensor;
	pmf_qual_t sc_qual;
	void *sc_ih;

	struct ethercom sc_ethercom;	/* Ethernet common part		*/
	bus_space_tag_t sc_iot;		/* bus cookie			*/
	bus_space_handle_t sc_ioh;	/* bus i/o handle		*/
	bus_dma_tag_t sc_dmat;		/* bus dma tag */
	bus_dmamap_t sc_dpd_dmamap;
	bus_dmamap_t sc_upd_dmamap;
#define sc_upddma	sc_upd_dmamap->dm_segs[0].ds_addr
#define sc_dpddma	sc_dpd_dmamap->dm_segs[0].ds_addr
	struct ex_upd *sc_upd;
	struct ex_dpd *sc_dpd;
	bus_dmamap_t sc_tx_dmamaps[EX_NDPD];	/* DMA maps for DPDs	*/
	bus_dmamap_t sc_rx_dmamaps[EX_NUPD];	/* DMA maps for UPDs	*/
	struct ex_rxdesc sc_rxdescs[EX_NUPD];
	struct ex_txdesc sc_txdescs[EX_NDPD];

	struct ex_rxdesc *rx_head;
	struct ex_rxdesc *rx_tail;

	struct ex_txdesc *tx_head;
	struct ex_txdesc *tx_tail;
	struct ex_txdesc *tx_free;
	struct ex_txdesc *tx_ftail;

	int tx_start_thresh;		/* Current TX_start_thresh.     */
	int tx_succ_ok;			/* # packets sent in sequence   */

	u_int ex_connectors;		/* Connectors on this card.	*/
	mii_data_t ex_mii;		/* mii bus data 		*/
	struct callout ex_mii_callout;	/* mii callout			*/
	u_int ex_conf;			/* config flags */

#define EX_CONF_MII		0x0001	/* has MII bus */
#define EX_CONF_INTPHY		0x0002	/* has internal PHY */
#define EX_CONF_90XB		0x0004	/* is 90xB */
#define EX_CONF_INV_LED_POLARITY 0x0010	/* CardBus & MiniPCI: LED polarity */
#define EX_CONF_PHY_POWER	0x0020	/* CardBus & MiniPCI: PHY power */
#define EX_CONF_EEPROM_OFF	0x0040	/* EEPROM is offset by 0x30 */
#define EX_CONF_EEPROM_8BIT	0x0080	/* 8 bit EEPROM */
#define EX_CONF_PCI_FUNCREG	0x0100	/* Has PCI function registers */
#define EX_CONF_RESETHACK	0x0200	/* Hack to make reset work on 556B */
#define EX_CONF_NO_XCVR_PWR	0x0400	/* Hack to enable later 556B xcvr */

#define EX_XCVR_PWR_MAGICBITS	0x0900	/* NO_XCVR_PWR magic value */


	/*
	 * XXX code duplication from elink3var.h
	 */
	u_int	ex_flags;		/* capabilities flag (from EEPROM) */
#define EX_FLAGS_PNP			0x0001
#define EX_FLAGS_FULLDUPLEX		0x0002
#define EX_FLAGS_LARGEPKT		0x0004	/* 4k packet support */
#define EX_FLAGS_SLAVEDMA		0x0008
#define EX_FLAGS_SECONDDMA		0x0010
#define EX_FLAGS_FULLDMA		0x0020
#define EX_FLAGS_FRAGMENTDMA		0x0040
#define EX_FLAGS_CRC_PASSTHRU		0x0080
#define EX_FLAGS_TXDONE			0x0100
#define EX_FLAGS_NO_TXLENGTH		0x0200
#define EX_FLAGS_RXREPEAT		0x0400
#define EX_FLAGS_SNOOPING		0x0800
#define EX_FLAGS_100MBIT		0x1000
#define EX_FLAGS_POWERMGMT		0x2000
#define EX_FLAGS_ATTACHED		0x4000	/* attach has succeeded */

	krndsource_t rnd_source;

	/* power management hooks */
	int (*enable)(struct ex_softc *);
	void (*disable)(struct ex_softc *);
	int enabled;

	/* interrupt acknowledge hook */
	void (*intr_ack)(struct ex_softc *);

	bus_dma_segment_t sc_useg, sc_dseg;
	int sc_urseg, sc_drseg;

	short sc_if_flags;
};

#define ex_waitcmd(sc) \
	do { \
		int stat; \
		do { \
			stat = bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, \
			    ELINK_STATUS); \
		} while ((stat & COMMAND_IN_PROGRESS) && (stat != 0xffff)); \
	} while (0)\

u_int16_t exreadeeprom(bus_space_tag_t, bus_space_handle_t, int);
void	ex_config(struct ex_softc *);

int	ex_intr(void *);
void	ex_stop(struct ifnet *, int);
void	ex_watchdog(struct ifnet *);
int	ex_ioctl(struct ifnet *ifp, u_long, void *);
int	ex_activate(device_t, enum devact);
int	ex_detach(struct ex_softc *);

int	ex_enable(struct ex_softc *);
void	ex_disable(struct ex_softc *);
