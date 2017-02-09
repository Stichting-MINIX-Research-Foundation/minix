/*	$NetBSD: tulip.c,v 1.185 2015/02/26 16:07:10 nakayama Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; and by Charles M. Hannum.
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
 * Device driver for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family, and a variety of clone chips.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tulip.c,v 1.185 2015/02/26 16:07:10 nakayama Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/tulipreg.h>
#include <dev/ic/tulipvar.h>

static const char * const tlp_chip_names[] = TULIP_CHIP_NAMES;

static const struct tulip_txthresh_tab tlp_10_txthresh_tab[] =
    TLP_TXTHRESH_TAB_10;

static const struct tulip_txthresh_tab tlp_10_100_txthresh_tab[] =
    TLP_TXTHRESH_TAB_10_100;

static const struct tulip_txthresh_tab tlp_dm9102_txthresh_tab[] =
    TLP_TXTHRESH_TAB_DM9102;

static void	tlp_start(struct ifnet *);
static void	tlp_watchdog(struct ifnet *);
static int	tlp_ioctl(struct ifnet *, u_long, void *);
static int	tlp_init(struct ifnet *);
static void	tlp_stop(struct ifnet *, int);
static int	tlp_ifflags_cb(struct ethercom *);

static void	tlp_rxdrain(struct tulip_softc *);
static int	tlp_add_rxbuf(struct tulip_softc *, int);
static void	tlp_srom_idle(struct tulip_softc *);
static int	tlp_srom_size(struct tulip_softc *);

static int	tlp_enable(struct tulip_softc *);
static void	tlp_disable(struct tulip_softc *);

static void	tlp_filter_setup(struct tulip_softc *);
static void	tlp_winb_filter_setup(struct tulip_softc *);
static void	tlp_al981_filter_setup(struct tulip_softc *);
static void	tlp_asix_filter_setup(struct tulip_softc *);

static void	tlp_rxintr(struct tulip_softc *);
static void	tlp_txintr(struct tulip_softc *);

static void	tlp_mii_tick(void *);
static void	tlp_mii_statchg(struct ifnet *);
static void	tlp_winb_mii_statchg(struct ifnet *);
static void	tlp_dm9102_mii_statchg(struct ifnet *);

static void	tlp_mii_getmedia(struct tulip_softc *, struct ifmediareq *);
static int	tlp_mii_setmedia(struct tulip_softc *);

static int	tlp_bitbang_mii_readreg(device_t, int, int);
static void	tlp_bitbang_mii_writereg(device_t, int, int, int);

static int	tlp_pnic_mii_readreg(device_t, int, int);
static void	tlp_pnic_mii_writereg(device_t, int, int, int);

static int	tlp_al981_mii_readreg(device_t, int, int);
static void	tlp_al981_mii_writereg(device_t, int, int, int);

static void	tlp_2114x_preinit(struct tulip_softc *);
static void	tlp_2114x_mii_preinit(struct tulip_softc *);
static void	tlp_pnic_preinit(struct tulip_softc *);
static void	tlp_dm9102_preinit(struct tulip_softc *);
static void	tlp_asix_preinit(struct tulip_softc *);

static void	tlp_21140_reset(struct tulip_softc *);
static void	tlp_21142_reset(struct tulip_softc *);
static void	tlp_pmac_reset(struct tulip_softc *);
#if 0
static void	tlp_dm9102_reset(struct tulip_softc *);
#endif

static void	tlp_2114x_nway_tick(void *);

#define	tlp_mchash(addr, sz)						\
	(ether_crc32_le((addr), ETHER_ADDR_LEN) & ((sz) - 1))

/*
 * MII bit-bang glue.
 */
static uint32_t tlp_sio_mii_bitbang_read(device_t);
static void	tlp_sio_mii_bitbang_write(device_t, uint32_t);

static const struct mii_bitbang_ops tlp_sio_mii_bitbang_ops = {
	tlp_sio_mii_bitbang_read,
	tlp_sio_mii_bitbang_write,
	{
		MIIROM_MDO,		/* MII_BIT_MDO */
		MIIROM_MDI,		/* MII_BIT_MDI */
		MIIROM_MDC,		/* MII_BIT_MDC */
		0,			/* MII_BIT_DIR_HOST_PHY */
		MIIROM_MIIDIR,		/* MII_BIT_DIR_PHY_HOST */
	}
};

#ifdef TLP_DEBUG
#define	DPRINTF(sc, x)	if ((sc)->sc_ethercom.ec_if.if_flags & IFF_DEBUG) \
				printf x
#else
#define	DPRINTF(sc, x)	/* nothing */
#endif

#ifdef TLP_STATS
static void	tlp_print_stats(struct tulip_softc *);
#endif

/*
 * Can be used to debug the SROM-related things, including contents.
 * Initialized so that it's patchable.
 */
int	tlp_srom_debug = 0;

/*
 * tlp_attach:
 *
 *	Attach a Tulip interface to the system.
 */
int
tlp_attach(struct tulip_softc *sc, const uint8_t *enaddr)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	device_t self = sc->sc_dev;
	int i, error;

	callout_init(&sc->sc_nway_callout, 0);
	callout_init(&sc->sc_tick_callout, 0);

	/*
	 * NOTE: WE EXPECT THE FRONT-END TO INITIALIZE sc_regshift!
	 */

	/*
	 * Setup the transmit threshold table.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_DE425:
	case TULIP_CHIP_21040:
	case TULIP_CHIP_21041:
		sc->sc_txth = tlp_10_txthresh_tab;
		break;

	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		sc->sc_txth = tlp_dm9102_txthresh_tab;
		break;

	default:
		sc->sc_txth = tlp_10_100_txthresh_tab;
		break;
	}

	/*
	 * Setup the filter setup function.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_WB89C840F:
		sc->sc_filter_setup = tlp_winb_filter_setup;
		break;

	case TULIP_CHIP_AL981:
	case TULIP_CHIP_AN983:
	case TULIP_CHIP_AN985:
		sc->sc_filter_setup = tlp_al981_filter_setup;
		break;

	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		sc->sc_filter_setup = tlp_asix_filter_setup;
		break;

	default:
		sc->sc_filter_setup = tlp_filter_setup;
		break;
	}

	/*
	 * Set up the media status change function.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_WB89C840F:
		sc->sc_statchg = tlp_winb_mii_statchg;
		break;

	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		sc->sc_statchg = tlp_dm9102_mii_statchg;
		break;

	default:
		/*
		 * We may override this if we have special media
		 * handling requirements (e.g. flipping GPIO pins).
		 *
		 * The pure-MII statchg function covers the basics.
		 */
		sc->sc_statchg = tlp_mii_statchg;
		break;
	}

	/*
	 * Default to no FS|LS in setup packet descriptors.  They're
	 * supposed to be zero according to the 21040 and 21143
	 * manuals, and some chips fall over badly if they're
	 * included.  Yet, other chips seem to require them.  Sigh.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_X3201_3:
		sc->sc_setup_fsls = TDCTL_Tx_FS|TDCTL_Tx_LS;
		break;

	default:
		sc->sc_setup_fsls = 0;
	}

	/*
	 * Set up various chip-specific quirks.
	 *
	 * Note that wherever we can, we use the "ring" option for
	 * transmit and receive descriptors.  This is because some
	 * clone chips apparently have problems when using chaining,
	 * although some *only* support chaining.
	 *
	 * What we do is always program the "next" pointer, and then
	 * conditionally set the TDCTL_CH and TDCTL_ER bits in the
	 * appropriate places.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21140:
	case TULIP_CHIP_21140A:
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
	case TULIP_CHIP_82C115:		/* 21143-like */
	case TULIP_CHIP_MX98713:	/* 21140-like */
	case TULIP_CHIP_MX98713A:	/* 21143-like */
	case TULIP_CHIP_MX98715:	/* 21143-like */
	case TULIP_CHIP_MX98715A:	/* 21143-like */
	case TULIP_CHIP_MX98715AEC_X:	/* 21143-like */
	case TULIP_CHIP_MX98725:	/* 21143-like */
	case TULIP_CHIP_RS7112:		/* 21143-like */
		/*
		 * Run these chips in ring mode.
		 */
		sc->sc_tdctl_ch = 0;
		sc->sc_tdctl_er = TDCTL_ER;
		sc->sc_preinit = tlp_2114x_preinit;
		break;

	case TULIP_CHIP_82C168:
	case TULIP_CHIP_82C169:
		/*
		 * Run these chips in ring mode.
		 */
		sc->sc_tdctl_ch = 0;
		sc->sc_tdctl_er = TDCTL_ER;
		sc->sc_preinit = tlp_pnic_preinit;

		/*
		 * These chips seem to have busted DMA engines; just put them
		 * in Store-and-Forward mode from the get-go.
		 */
		sc->sc_txthresh = TXTH_SF;
		break;

	case TULIP_CHIP_WB89C840F:
		/*
		 * Run this chip in chained mode.
		 */
		sc->sc_tdctl_ch = TDCTL_CH;
		sc->sc_tdctl_er = 0;
		sc->sc_flags |= TULIPF_IC_FS;
		break;

	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		/*
		 * Run these chips in chained mode.
		 */
		sc->sc_tdctl_ch = TDCTL_CH;
		sc->sc_tdctl_er = 0;
		sc->sc_preinit = tlp_dm9102_preinit;

		/*
		 * These chips have a broken bus interface, so we
		 * can't use any optimized bus commands.  For this
		 * reason, we tend to underrun pretty quickly, so
		 * just to Store-and-Forward mode from the get-go.
		 */
		sc->sc_txthresh = TXTH_DM9102_SF;
		break;

	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		/*
		 * Run these chips in ring mode.
		 */
		sc->sc_tdctl_ch = 0;
		sc->sc_tdctl_er = TDCTL_ER;
		sc->sc_preinit = tlp_asix_preinit;
		break;

	default:
		/*
		 * Default to running in ring mode.
		 */
		sc->sc_tdctl_ch = 0;
		sc->sc_tdctl_er = TDCTL_ER;
	}

	/*
	 * Set up the MII bit-bang operations.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_WB89C840F:	/* XXX direction bit different? */
		sc->sc_bitbang_ops = &tlp_sio_mii_bitbang_ops;
		break;

	default:
		sc->sc_bitbang_ops = &tlp_sio_mii_bitbang_ops;
	}

	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct tulip_control_data), PAGE_SIZE, 0, &sc->sc_cdseg,
	    1, &sc->sc_cdnseg, 0)) != 0) {
		aprint_error_dev(self, "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg,
	    sizeof(struct tulip_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self, "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct tulip_control_data), 1,
	    sizeof(struct tulip_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		sc->sc_cddmamap = NULL;
		aprint_error_dev(self, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct tulip_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(self, "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 *
	 * Note that on the Xircom clone, transmit buffers must be
	 * 4-byte aligned.  We're almost guaranteed to have to copy
	 * the packet in that case, so we just limit ourselves to
	 * one segment.
	 *
	 * On the DM9102, the transmit logic can only handle one
	 * DMA segment.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_X3201_3:
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		sc->sc_ntxsegs = 1;
		break;

	default:
		sc->sc_ntxsegs = TULIP_NTXSEGS;
	}
	for (i = 0; i < TULIP_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    sc->sc_ntxsegs, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			sc->sc_txsoft[i].txs_dmamap = NULL;
			aprint_error_dev(self, "unable to create tx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < TULIP_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
		        sc->sc_rxsoft[i].rxs_dmamap = NULL;
			aprint_error_dev(self, "unable to create rx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */
	sc->sc_flags |= TULIPF_ATTACHED;

	/*
	 * Reset the chip to a known state.
	 */
	tlp_reset(sc);

	/* Announce ourselves. */
	aprint_normal_dev(self, "%s%sEthernet address %s\n",
	    sc->sc_name[0] != '\0' ? sc->sc_name : "",
	    sc->sc_name[0] != '\0' ? ", " : "",
	    ether_sprintf(enaddr));

	/*
	 * Check to see if we're the simulated Ethernet on Connectix
	 * Virtual PC.
	 */
	if (enaddr[0] == 0x00 && enaddr[1] == 0x03 && enaddr[2] == 0xff)
		sc->sc_flags |= TULIPF_VPC;

	/*
	 * Initialize our media structures.  This may probe the MII, if
	 * present.
	 */
	(*sc->sc_mediasw->tmsw_init)(sc);

	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_ioctl = tlp_ioctl;
	ifp->if_start = tlp_start;
	ifp->if_watchdog = tlp_watchdog;
	ifp->if_init = tlp_init;
	ifp->if_stop = tlp_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, tlp_ifflags_cb);

	rnd_attach_source(&sc->sc_rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return 0;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < TULIP_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < TULIP_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct tulip_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);
 fail_0:
	return error;
}

/*
 * tlp_activate:
 *
 *	Handle device activation/deactivation requests.
 */
int
tlp_activate(device_t self, enum devact act)
{
	struct tulip_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * tlp_detach:
 *
 *	Detach a Tulip interface.
 */
int
tlp_detach(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct tulip_rxsoft *rxs;
	struct tulip_txsoft *txs;
	device_t self = sc->sc_dev;
	int i, s;

	/*
	 * Succeed now if there isn't any work to do.
	 */
	if ((sc->sc_flags & TULIPF_ATTACHED) == 0)
		return (0);

	s = splnet();
	/* Stop the interface. Callouts are stopped in it. */
	tlp_stop(ifp, 1);
	splx(s);

	/* Destroy our callouts. */
	callout_destroy(&sc->sc_nway_callout);
	callout_destroy(&sc->sc_tick_callout);

	if (sc->sc_flags & TULIPF_HAS_MII) {
		/* Detach all PHYs */
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	}

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	rnd_detach_source(&sc->sc_rnd_source);

	ether_ifdetach(ifp);
	if_detach(ifp);

	for (i = 0; i < TULIP_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rxs->rxs_dmamap);
	}
	for (i = 0; i < TULIP_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, txs->txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct tulip_control_data));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);

	pmf_device_deregister(self);

	if (sc->sc_srom)
		free(sc->sc_srom, M_DEVBUF);

	return (0);
}

/*
 * tlp_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
tlp_start(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct tulip_txsoft *txs, *last_txs = NULL;
	bus_dmamap_t dmamap;
	int error, firsttx, nexttx, lasttx = 1, ofree, seg;
	struct tulip_desc *txd;

	DPRINTF(sc, ("%s: tlp_start: sc_flags 0x%08x, if_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags, ifp->if_flags));

	/*
	 * If we want a filter setup, it means no more descriptors were
	 * available for the setup routine.  Let it get a chance to wedge
	 * itself into the ring.
	 */
	if (sc->sc_flags & TULIPF_WANT_SETUP)
		ifp->if_flags |= IFF_OACTIVE;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	if (sc->sc_tick == tlp_2114x_nway_tick &&
	    (sc->sc_flags & TULIPF_LINK_UP) == 0 && ifp->if_snd.ifq_len < 10)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;
	firsttx = sc->sc_txnext;

	DPRINTF(sc, ("%s: tlp_start: txfree %d, txnext %d\n",
	    device_xname(sc->sc_dev), ofree, firsttx));

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txfreeq)) != NULL &&
	       sc->sc_txfree != 0) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we were
		 * short on resources.  In this case, we'll copy and try
		 * again.
		 *
		 * Note that if we're only allowed 1 Tx segment, we
		 * have an alignment restriction.  Do this test before
		 * attempting to load the DMA map, because it's more
		 * likely we'll trip the alignment test than the
		 * more-than-one-segment test.
		 */
		if ((sc->sc_ntxsegs == 1 && (mtod(m0, uintptr_t) & 3) != 0) ||
		    bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		      BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_dev(sc->sc_dev, "unable to allocate Tx mbuf\n");
				break;
			}
			MCLAIM(m, &sc->sc_ethercom.ec_tx_mowner);
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					aprint_error_dev(sc->sc_dev,
					    "unable to allocate Tx cluster\n");
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
			    m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				aprint_error_dev(sc->sc_dev,
				    "unable to load Tx buffer, error = %d",
				    error);
				break;
			}
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > sc->sc_txfree) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed to anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX it is worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			if (m != NULL)
				m_freem(m);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = TULIP_NEXTTX(nexttx)) {
			/*
			 * If this is the first descriptor we're
			 * enqueueing, don't set the OWN bit just
			 * yet.  That could cause a race condition.
			 * We'll do it below.
			 */
			txd = &sc->sc_txdescs[nexttx];
			txd->td_status =
			    (nexttx == firsttx) ? 0 : htole32(TDSTAT_OWN);
			txd->td_bufaddr1 =
			    htole32(dmamap->dm_segs[seg].ds_addr);
			txd->td_ctl =
			    htole32((dmamap->dm_segs[seg].ds_len <<
			        TDCTL_SIZE1_SHIFT) | sc->sc_tdctl_ch |
				(nexttx == (TULIP_NTXDESC - 1) ?
				 sc->sc_tdctl_er : 0));
			lasttx = nexttx;
		}

		KASSERT(lasttx != -1);

		/* Set `first segment' and `last segment' appropriately. */
		sc->sc_txdescs[sc->sc_txnext].td_ctl |= htole32(TDCTL_Tx_FS);
		sc->sc_txdescs[lasttx].td_ctl |= htole32(TDCTL_Tx_LS);

#ifdef TLP_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			printf("     txsoft %p transmit chain:\n", txs);
			for (seg = sc->sc_txnext;; seg = TULIP_NEXTTX(seg)) {
				txd = &sc->sc_txdescs[seg];
				printf("     descriptor %d:\n", seg);
				printf("       td_status:   0x%08x\n",
				    le32toh(txd->td_status));
				printf("       td_ctl:      0x%08x\n",
				    le32toh(txd->td_ctl));
				printf("       td_bufaddr1: 0x%08x\n",
				    le32toh(txd->td_bufaddr1));
				printf("       td_bufaddr2: 0x%08x\n",
				    le32toh(txd->td_bufaddr2));
				if (seg == lasttx)
					break;
			}
		}
#endif

		/* Sync the descriptors we're using. */
		TULIP_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;
		txs->txs_ndescs = dmamap->dm_nsegs;

		/* Advance the tx pointer. */
		sc->sc_txfree -= dmamap->dm_nsegs;
		sc->sc_txnext = nexttx;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
		SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

		last_txs = txs;

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (txs == NULL || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		DPRINTF(sc, ("%s: packets enqueued, IC on %d, OWN on %d\n",
		    device_xname(sc->sc_dev), lasttx, firsttx));
		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued.
		 */
		sc->sc_txdescs[lasttx].td_ctl |= htole32(TDCTL_Tx_IC);
		TULIP_CDTXSYNC(sc, lasttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Some clone chips want IC on the *first* segment in
		 * the packet.  Appease them.
		 */
		KASSERT(last_txs != NULL);
		if ((sc->sc_flags & TULIPF_IC_FS) != 0 &&
		    last_txs->txs_firstdesc != lasttx) {
			sc->sc_txdescs[last_txs->txs_firstdesc].td_ctl |=
			    htole32(TDCTL_Tx_IC);
			TULIP_CDTXSYNC(sc, last_txs->txs_firstdesc, 1,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}

		/*
		 * The entire packet chain is set up.  Give the
		 * first descriptor to the chip now.
		 */
		sc->sc_txdescs[firsttx].td_status |= htole32(TDSTAT_OWN);
		TULIP_CDTXSYNC(sc, firsttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Wake up the transmitter. */
		/* XXX USE AUTOPOLLING? */
		TULIP_WRITE(sc, CSR_TXPOLL, TXPOLL_TPD);

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * tlp_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
tlp_watchdog(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;
	int doing_setup, doing_transmit;

	doing_setup = (sc->sc_flags & TULIPF_DOING_SETUP);
	doing_transmit = (! SIMPLEQ_EMPTY(&sc->sc_txdirtyq));

	if (doing_setup && doing_transmit) {
		printf("%s: filter setup and transmit timeout\n", device_xname(sc->sc_dev));
		ifp->if_oerrors++;
	} else if (doing_transmit) {
		printf("%s: transmit timeout\n", device_xname(sc->sc_dev));
		ifp->if_oerrors++;
	} else if (doing_setup)
		printf("%s: filter setup timeout\n", device_xname(sc->sc_dev));
	else
		printf("%s: spurious watchdog timeout\n", device_xname(sc->sc_dev));

	(void) tlp_init(ifp);

	/* Try to get more packets going. */
	tlp_start(ifp);
}

/* If the interface is up and running, only modify the receive
 * filter when setting promiscuous or debug mode.  Otherwise fall
 * through to ether_ioctl, which will reset the chip.
 */
static int
tlp_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct tulip_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	if ((change & IFF_PROMISC) != 0)
		(*sc->sc_filter_setup)(sc);
	return 0;
}

/*
 * tlp_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
tlp_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct tulip_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				/*
				 * Multicast list has changed.  Set the
				 * hardware filter accordingly.
				 */
				(*sc->sc_filter_setup)(sc);
			}
			error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	if (TULIP_IS_ENABLED(sc))
		tlp_start(ifp);

	sc->sc_if_flags = ifp->if_flags;
	splx(s);
	return (error);
}

/*
 * tlp_intr:
 *
 *	Interrupt service routine.
 */
int
tlp_intr(void *arg)
{
	struct tulip_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t status, rxstatus, txstatus;
	int handled = 0, txthresh;

	DPRINTF(sc, ("%s: tlp_intr\n", device_xname(sc->sc_dev)));

#ifdef DEBUG
	if (TULIP_IS_ENABLED(sc) == 0)
		panic("%s: tlp_intr: not enabled", device_xname(sc->sc_dev));
#endif

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_is_active(sc->sc_dev))
		return (0);

	/* Disable interrupts on the DM9102 (interrupt edge bug). */
	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		TULIP_WRITE(sc, CSR_INTEN, 0);
		break;

	default:
		/* Nothing. */
		break;
	}

	for (;;) {
		status = TULIP_READ(sc, CSR_STATUS);
		if (status)
			TULIP_WRITE(sc, CSR_STATUS, status);

		if ((status & sc->sc_inten) == 0)
			break;

		handled = 1;

		rxstatus = status & sc->sc_rxint_mask;
		txstatus = status & sc->sc_txint_mask;

		if (rxstatus) {
			/* Grab new any new packets. */
			tlp_rxintr(sc);

			if (rxstatus & STATUS_RWT)
				printf("%s: receive watchdog timeout\n",
				    device_xname(sc->sc_dev));

			if (rxstatus & STATUS_RU) {
				printf("%s: receive ring overrun\n",
				    device_xname(sc->sc_dev));
				/* Get the receive process going again. */
				if (sc->sc_tdctl_er != TDCTL_ER) {
					tlp_idle(sc, OPMODE_SR);
					TULIP_WRITE(sc, CSR_RXLIST,
					    TULIP_CDRXADDR(sc, sc->sc_rxptr));
					TULIP_WRITE(sc, CSR_OPMODE,
					    sc->sc_opmode);
				}
				TULIP_WRITE(sc, CSR_RXPOLL, RXPOLL_RPD);
				break;
			}
		}

		if (txstatus) {
			/* Sweep up transmit descriptors. */
			tlp_txintr(sc);

			if (txstatus & STATUS_TJT)
				printf("%s: transmit jabber timeout\n",
				    device_xname(sc->sc_dev));

			if (txstatus & STATUS_UNF) {
				/*
				 * Increase our transmit threshold if
				 * another is available.
				 */
				txthresh = sc->sc_txthresh + 1;
				if (sc->sc_txth[txthresh].txth_name != NULL) {
					/* Idle the transmit process. */
					tlp_idle(sc, OPMODE_ST);

					sc->sc_txthresh = txthresh;
					sc->sc_opmode &= ~(OPMODE_TR|OPMODE_SF);
					sc->sc_opmode |=
					    sc->sc_txth[txthresh].txth_opmode;
					printf("%s: transmit underrun; new "
					    "threshold: %s\n",
					    device_xname(sc->sc_dev),
					    sc->sc_txth[txthresh].txth_name);

					/*
					 * Set the new threshold and restart
					 * the transmit process.
					 */
					TULIP_WRITE(sc, CSR_OPMODE,
					    sc->sc_opmode);
				}
					/*
					 * XXX Log every Nth underrun from
					 * XXX now on?
					 */
			}
		}

		if (status & (STATUS_TPS|STATUS_RPS)) {
			if (status & STATUS_TPS)
				printf("%s: transmit process stopped\n",
				    device_xname(sc->sc_dev));
			if (status & STATUS_RPS)
				printf("%s: receive process stopped\n",
				    device_xname(sc->sc_dev));
			(void) tlp_init(ifp);
			break;
		}

		if (status & STATUS_SE) {
			const char *str;
			switch (status & STATUS_EB) {
			case STATUS_EB_PARITY:
				str = "parity error";
				break;

			case STATUS_EB_MABT:
				str = "master abort";
				break;

			case STATUS_EB_TABT:
				str = "target abort";
				break;

			default:
				str = "unknown error";
				break;
			}
			aprint_error_dev(sc->sc_dev, "fatal system error: %s\n",
			    str);
			(void) tlp_init(ifp);
			break;
		}

		/*
		 * Not handled:
		 *
		 *	Transmit buffer unavailable -- normal
		 *	condition, nothing to do, really.
		 *
		 *	General purpose timer experied -- we don't
		 *	use the general purpose timer.
		 *
		 *	Early receive interrupt -- not available on
		 *	all chips, we just use RI.  We also only
		 *	use single-segment receive DMA, so this
		 *	is mostly useless.
		 */
	}

	/* Bring interrupts back up on the DM9102. */
	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		TULIP_WRITE(sc, CSR_INTEN, sc->sc_inten);
		break;

	default:
		/* Nothing. */
		break;
	}

	/* Try to get more packets going. */
	tlp_start(ifp);

	if (handled)
		rnd_add_uint32(&sc->sc_rnd_source, status);

	return (handled);
}

/*
 * tlp_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
tlp_rxintr(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_header *eh;
	struct tulip_rxsoft *rxs;
	struct mbuf *m;
	uint32_t rxstat, errors;
	int i, len;

	for (i = sc->sc_rxptr;; i = TULIP_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		TULIP_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = le32toh(sc->sc_rxdescs[i].td_status);

		if (rxstat & TDSTAT_OWN) {
			/*
			 * We have processed all of the receive buffers.
			 */
			break;
		}

		/*
		 * Make sure the packet fit in one buffer.  This should
		 * always be the case.  But the Lite-On PNIC, rev 33
		 * has an awful receive engine bug, which may require
		 * a very icky work-around.
		 */
		if ((rxstat & (TDSTAT_Rx_FS|TDSTAT_Rx_LS)) !=
		    (TDSTAT_Rx_FS|TDSTAT_Rx_LS)) {
			printf("%s: incoming packet spilled, resetting\n",
			    device_xname(sc->sc_dev));
			(void) tlp_init(ifp);
			return;
		}

		/*
		 * If any collisions were seen on the wire, count one.
		 */
		if (rxstat & TDSTAT_Rx_CS)
			ifp->if_collisions++;

		/*
		 * If an error occurred, update stats, clear the status
		 * word, and leave the packet buffer in place.  It will
		 * simply be reused the next time the ring comes around.
		 */
		errors = TDSTAT_Rx_DE | TDSTAT_Rx_RF | TDSTAT_Rx_TL |
		    TDSTAT_Rx_CS | TDSTAT_Rx_RE | TDSTAT_Rx_DB | TDSTAT_Rx_CE;
		/*
	 	 * If 802.1Q VLAN MTU is enabled, ignore the Frame Too Long
		 * error.
		 */
		if ((sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) != 0)
			errors &= ~TDSTAT_Rx_TL;
		/*
		 * If chip doesn't have MII, ignore the MII error bit.
		 */
		if ((sc->sc_flags & TULIPF_HAS_MII) == 0)
			errors &= ~TDSTAT_Rx_RE;

		if ((rxstat & TDSTAT_ES) != 0 &&
		    (rxstat & errors) != 0) {
			rxstat &= errors;
#define	PRINTERR(bit, str)						\
			if (rxstat & (bit))				\
				aprint_error_dev(sc->sc_dev, "receive error: %s\n",	\
				    str)
			ifp->if_ierrors++;
			PRINTERR(TDSTAT_Rx_DE, "descriptor error");
			PRINTERR(TDSTAT_Rx_RF, "runt frame");
			PRINTERR(TDSTAT_Rx_TL, "frame too long");
			PRINTERR(TDSTAT_Rx_RE, "MII error");
			PRINTERR(TDSTAT_Rx_DB, "dribbling bit");
			PRINTERR(TDSTAT_Rx_CE, "CRC error");
#undef PRINTERR
			TULIP_INIT_RXDESC(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.  Note the Tulip
		 * includes the CRC with every packet.
		 */
		len = TDSTAT_Rx_LENGTH(rxstat) - ETHER_CRC_LEN;

#ifdef __NO_STRICT_ALIGNMENT
		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		m = rxs->rxs_mbuf;
		if (tlp_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			TULIP_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}
#else
		/*
		 * The Tulip's receive buffers must be 4-byte aligned.
		 * But this means that the data after the Ethernet header
		 * is misaligned.  We must allocate a new buffer and
		 * copy the data, shifted forward 2 bytes.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
 dropit:
			ifp->if_ierrors++;
			TULIP_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}
		MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
		if (len > (MHLEN - 2)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				goto dropit;
			}
		}
		m->m_data += 2;

		/*
		 * Note that we use clusters for incoming frames, so the
		 * buffer is virtually contiguous.
		 */
		memcpy(mtod(m, void *), mtod(rxs->rxs_mbuf, void *), len);

		/* Allow the receive descriptor to continue using its mbuf. */
		TULIP_INIT_RXDESC(sc, i);
		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
#endif /* __NO_STRICT_ALIGNMENT */

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/*
		 * XXX Work-around for a weird problem with the emulated
		 * 21041 on Connectix Virtual PC:
		 *
		 * When we receive a full-size TCP segment, we seem to get
		 * a packet there the Rx status says 1522 bytes, yet we do
		 * not get a frame-too-long error from the chip.  The extra
		 * bytes seem to always be zeros.  Perhaps Virtual PC is
		 * inserting 4 bytes of zeros after every packet.  In any
		 * case, let's try and detect this condition and truncate
		 * the length so that it will pass up the stack.
		 */
		if (__predict_false((sc->sc_flags & TULIPF_VPC) != 0)) {
			uint16_t etype = ntohs(eh->ether_type);

			if (len > ETHER_MAX_FRAME(ifp, etype, 0))
				m->m_pkthdr.len = m->m_len = len =
				    ETHER_MAX_FRAME(ifp, etype, 0);
		}

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/*
		 * We sometimes have to run the 21140 in Hash-Only
		 * mode.  If we're in that mode, and not in promiscuous
		 * mode, and we have a unicast packet that isn't for
		 * us, then drop it.
		 */
		if (sc->sc_filtmode == TDCTL_Tx_FT_HASHONLY &&
		    (ifp->if_flags & IFF_PROMISC) == 0 &&
		    ETHER_IS_MULTICAST(eh->ether_dhost) == 0 &&
		    memcmp(CLLADDR(ifp->if_sadl), eh->ether_dhost,
			   ETHER_ADDR_LEN) != 0) {
			m_freem(m);
			continue;
		}

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * tlp_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
tlp_txintr(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct tulip_txsoft *txs;
	uint32_t txstat;

	DPRINTF(sc, ("%s: tlp_txintr: sc_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags));

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		TULIP_CDTXSYNC(sc, txs->txs_lastdesc,
		    txs->txs_ndescs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

#ifdef TLP_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			int i;
			struct tulip_desc *txd;
			printf("    txsoft %p transmit chain:\n", txs);
			for (i = txs->txs_firstdesc;; i = TULIP_NEXTTX(i)) {
				txd = &sc->sc_txdescs[i];
				printf("     descriptor %d:\n", i);
				printf("       td_status:   0x%08x\n",
				    le32toh(txd->td_status));
				printf("       td_ctl:      0x%08x\n",
				    le32toh(txd->td_ctl));
				printf("       td_bufaddr1: 0x%08x\n",
				    le32toh(txd->td_bufaddr1));
				printf("       td_bufaddr2: 0x%08x\n",
				    le32toh(sc->sc_txdescs[i].td_bufaddr2));
				if (i == txs->txs_lastdesc)
					break;
			}
		}
#endif

		txstat = le32toh(sc->sc_txdescs[txs->txs_lastdesc].td_status);
		if (txstat & TDSTAT_OWN)
			break;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_ndescs;

		if (txs->txs_mbuf == NULL) {
			/*
			 * If we didn't have an mbuf, it was the setup
			 * packet.
			 */
#ifdef DIAGNOSTIC
			if ((sc->sc_flags & TULIPF_DOING_SETUP) == 0)
				panic("tlp_txintr: null mbuf, not doing setup");
#endif
			TULIP_CDSPSYNC(sc, BUS_DMASYNC_POSTWRITE);
			sc->sc_flags &= ~TULIPF_DOING_SETUP;
			SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;

		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		/*
		 * Check for errors and collisions.
		 */
#ifdef TLP_STATS
		if (txstat & TDSTAT_Tx_UF)
			sc->sc_stats.ts_tx_uf++;
		if (txstat & TDSTAT_Tx_TO)
			sc->sc_stats.ts_tx_to++;
		if (txstat & TDSTAT_Tx_EC)
			sc->sc_stats.ts_tx_ec++;
		if (txstat & TDSTAT_Tx_LC)
			sc->sc_stats.ts_tx_lc++;
#endif

		if (txstat & (TDSTAT_Tx_UF|TDSTAT_Tx_TO))
			ifp->if_oerrors++;

		if (txstat & TDSTAT_Tx_EC)
			ifp->if_collisions += 16;
		else
			ifp->if_collisions += TDSTAT_Tx_COLLISIONS(txstat);
		if (txstat & TDSTAT_Tx_LC)
			ifp->if_collisions++;

		ifp->if_opackets++;
	}

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (txs == NULL && (sc->sc_flags & TULIPF_DOING_SETUP) == 0)
		ifp->if_timer = 0;

	/*
	 * If we have a receive filter setup pending, do it now.
	 */
	if (sc->sc_flags & TULIPF_WANT_SETUP)
		(*sc->sc_filter_setup)(sc);
}

#ifdef TLP_STATS
void
tlp_print_stats(struct tulip_softc *sc)
{

	printf("%s: tx_uf %lu, tx_to %lu, tx_ec %lu, tx_lc %lu\n",
	    device_xname(sc->sc_dev),
	    sc->sc_stats.ts_tx_uf, sc->sc_stats.ts_tx_to,
	    sc->sc_stats.ts_tx_ec, sc->sc_stats.ts_tx_lc);
}
#endif

/*
 * tlp_reset:
 *
 *	Perform a soft reset on the Tulip.
 */
void
tlp_reset(struct tulip_softc *sc)
{
	int i;

	TULIP_WRITE(sc, CSR_BUSMODE, BUSMODE_SWR);

	/*
	 * Xircom, ASIX and Conexant clones don't bring themselves
	 * out of reset automatically.
	 * Instead, we have to wait at least 50 PCI cycles, and then
	 * clear SWR.
	 */
	switch (sc->sc_chip) {
		case TULIP_CHIP_X3201_3:
		case TULIP_CHIP_AX88140:
		case TULIP_CHIP_AX88141:
		case TULIP_CHIP_RS7112:
			delay(10);
			TULIP_WRITE(sc, CSR_BUSMODE, 0);
			break;
		default:
			break;
	}

	for (i = 0; i < 1000; i++) {
		/*
		 * Wait at least 50 PCI cycles for the reset to
		 * complete before peeking at the Tulip again.
		 * 10 uSec is a bit longer than 50 PCI cycles
		 * (at 33MHz), but it doesn't hurt have the extra
		 * wait.
		 */
		delay(10);
		if (TULIP_ISSET(sc, CSR_BUSMODE, BUSMODE_SWR) == 0)
			break;
	}

	if (TULIP_ISSET(sc, CSR_BUSMODE, BUSMODE_SWR))
		aprint_error_dev(sc->sc_dev, "reset failed to complete\n");

	delay(1000);

	/*
	 * If the board has any GPIO reset sequences to issue, do them now.
	 */
	if (sc->sc_reset != NULL)
		(*sc->sc_reset)(sc);
}

/*
 * tlp_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
tlp_init(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;
	struct tulip_txsoft *txs;
	struct tulip_rxsoft *rxs;
	int i, error = 0;

	if ((error = tlp_enable(sc)) != 0)
		goto out;

	/*
	 * Cancel any pending I/O.
	 */
	tlp_stop(ifp, 0);

	/*
	 * Initialize `opmode' to 0, and call the pre-init routine, if
	 * any.  This is required because the 2114x and some of the
	 * clones require that the media-related bits in `opmode' be
	 * set before performing a soft-reset in order to get internal
	 * chip pathways are correct.  Yay!
	 */
	sc->sc_opmode = 0;
	if (sc->sc_preinit != NULL)
		(*sc->sc_preinit)(sc);

	/*
	 * Reset the Tulip to a known state.
	 */
	tlp_reset(sc);

	/*
	 * Initialize the BUSMODE register.
	 */
	sc->sc_busmode = BUSMODE_BAR;
	switch (sc->sc_chip) {
	case TULIP_CHIP_21140:
	case TULIP_CHIP_21140A:
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
	case TULIP_CHIP_82C115:
	case TULIP_CHIP_MX98725:
		/*
		 * If we're allowed to do so, use Memory Read Line
		 * and Memory Read Multiple.
		 *
		 * XXX Should we use Memory Write and Invalidate?
		 */
		if (sc->sc_flags & TULIPF_MRL)
			sc->sc_busmode |= BUSMODE_RLE;
		if (sc->sc_flags & TULIPF_MRM)
			sc->sc_busmode |= BUSMODE_RME;
#if 0
		if (sc->sc_flags & TULIPF_MWI)
			sc->sc_busmode |= BUSMODE_WLE;
#endif
		break;

	case TULIP_CHIP_82C168:
	case TULIP_CHIP_82C169:
		sc->sc_busmode |= BUSMODE_PNIC_MBO;
		if (sc->sc_maxburst == 0)
			sc->sc_maxburst = 16;
		break;

	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		if (sc->sc_maxburst == 0)
			sc->sc_maxburst = 16;
		break;

	default:
		/* Nothing. */
		break;
	}
	switch (sc->sc_cacheline) {
	default:
		/*
		 * Note: We must *always* set these bits; a cache
		 * alignment of 0 is RESERVED.
		 */
	case 8:
		sc->sc_busmode |= BUSMODE_CAL_8LW;
		break;
	case 16:
		sc->sc_busmode |= BUSMODE_CAL_16LW;
		break;
	case 32:
		sc->sc_busmode |= BUSMODE_CAL_32LW;
		break;
	}
	switch (sc->sc_maxburst) {
	case 1:
		sc->sc_busmode |= BUSMODE_PBL_1LW;
		break;
	case 2:
		sc->sc_busmode |= BUSMODE_PBL_2LW;
		break;
	case 4:
		sc->sc_busmode |= BUSMODE_PBL_4LW;
		break;
	case 8:
		sc->sc_busmode |= BUSMODE_PBL_8LW;
		break;
	case 16:
		sc->sc_busmode |= BUSMODE_PBL_16LW;
		break;
	case 32:
		sc->sc_busmode |= BUSMODE_PBL_32LW;
		break;
	default:
		sc->sc_busmode |= BUSMODE_PBL_DEFAULT;
		break;
	}
#if BYTE_ORDER == BIG_ENDIAN
	/*
	 * Can't use BUSMODE_BLE or BUSMODE_DBO; not all chips
	 * support them, and even on ones that do, it doesn't
	 * always work.  So we always access descriptors with
	 * little endian via htole32/le32toh.
	 */
#endif
	/*
	 * Big-endian bus requires BUSMODE_BLE anyway.
	 * Also, BUSMODE_DBO is needed because we assume
	 * descriptors are little endian.
	 */
	if (sc->sc_flags & TULIPF_BLE)
		sc->sc_busmode |= BUSMODE_BLE;
	if (sc->sc_flags & TULIPF_DBO)
		sc->sc_busmode |= BUSMODE_DBO;

	/*
	 * Some chips have a broken bus interface.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		sc->sc_busmode = 0;
		break;

	default:
		/* Nothing. */
		break;
	}

	TULIP_WRITE(sc, CSR_BUSMODE, sc->sc_busmode);

	/*
	 * Initialize the OPMODE register.  We don't write it until
	 * we're ready to begin the transmit and receive processes.
	 *
	 * Media-related OPMODE bits are set in the media callbacks
	 * for each specific chip/board.
	 */
	sc->sc_opmode |= OPMODE_SR | OPMODE_ST |
	    sc->sc_txth[sc->sc_txthresh].txth_opmode;

	/*
	 * Magical mystery initialization on the Macronix chips.
	 * The MX98713 uses its own magic value, the rest share
	 * a common one.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_MX98713:
		TULIP_WRITE(sc, CSR_PMAC_TOR, PMAC_TOR_98713);
		break;

	case TULIP_CHIP_MX98713A:
	case TULIP_CHIP_MX98715:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98715AEC_X:
	case TULIP_CHIP_MX98725:
		TULIP_WRITE(sc, CSR_PMAC_TOR, PMAC_TOR_98715);
		break;

	default:
		/* Nothing. */
		break;
	}

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < TULIP_NTXDESC; i++) {
		struct tulip_desc *txd = &sc->sc_txdescs[i];
		txd->td_ctl = htole32(sc->sc_tdctl_ch);
		txd->td_bufaddr2 = htole32(TULIP_CDTXADDR(sc, TULIP_NEXTTX(i)));
	}
	sc->sc_txdescs[TULIP_NTXDESC - 1].td_ctl |= htole32(sc->sc_tdctl_er);
	TULIP_CDTXSYNC(sc, 0, TULIP_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = TULIP_NTXDESC;
	sc->sc_txnext = 0;

	/*
	 * Initialize the transmit job descriptors.
	 */
	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);
	for (i = 0; i < TULIP_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < TULIP_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = tlp_add_rxbuf(sc, i)) != 0) {
				aprint_error_dev(sc->sc_dev, "unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				tlp_rxdrain(sc);
				goto out;
			}
		} else
			TULIP_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/*
	 * Initialize the interrupt mask and enable interrupts.
	 */
	/* normal interrupts */
	sc->sc_inten =  STATUS_TI | STATUS_TU | STATUS_RI | STATUS_NIS;

	/* abnormal interrupts */
	sc->sc_inten |= STATUS_TPS | STATUS_TJT | STATUS_UNF |
	    STATUS_RU | STATUS_RPS | STATUS_RWT | STATUS_SE | STATUS_AIS;

	sc->sc_rxint_mask = STATUS_RI|STATUS_RU|STATUS_RWT;
	sc->sc_txint_mask = STATUS_TI|STATUS_UNF|STATUS_TJT;

	switch (sc->sc_chip) {
	case TULIP_CHIP_WB89C840F:
		/*
		 * Clear bits that we don't want that happen to
		 * overlap or don't exist.
		 */
		sc->sc_inten &= ~(STATUS_WINB_REI|STATUS_RWT);
		break;

	default:
		/* Nothing. */
		break;
	}

	sc->sc_rxint_mask &= sc->sc_inten;
	sc->sc_txint_mask &= sc->sc_inten;

	TULIP_WRITE(sc, CSR_INTEN, sc->sc_inten);
	TULIP_WRITE(sc, CSR_STATUS, 0xffffffff);

	/*
	 * Give the transmit and receive rings to the Tulip.
	 */
	TULIP_WRITE(sc, CSR_TXLIST, TULIP_CDTXADDR(sc, sc->sc_txnext));
	TULIP_WRITE(sc, CSR_RXLIST, TULIP_CDRXADDR(sc, sc->sc_rxptr));

	/*
	 * On chips that do this differently, set the station address.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_WB89C840F:
	    {
		/* XXX Do this with stream writes? */
		bus_addr_t cpa = TULIP_CSR_OFFSET(sc, CSR_WINB_CPA0);

		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			bus_space_write_1(sc->sc_st, sc->sc_sh,
			    cpa + i, CLLADDR(ifp->if_sadl)[i]);
		}
		break;
	    }

	case TULIP_CHIP_AL981:
	case TULIP_CHIP_AN983:
	case TULIP_CHIP_AN985:
	    {
		uint32_t reg;
		const uint8_t *enaddr = CLLADDR(ifp->if_sadl);

		reg = enaddr[0] |
		      (enaddr[1] << 8) |
		      (enaddr[2] << 16) |
		      (enaddr[3] << 24);
		bus_space_write_4(sc->sc_st, sc->sc_sh, CSR_ADM_PAR0, reg);

		reg = enaddr[4] |
		      (enaddr[5] << 8);
		bus_space_write_4(sc->sc_st, sc->sc_sh, CSR_ADM_PAR1, reg);
		break;
	    }

	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
	    {
		uint32_t reg;
		const uint8_t *enaddr = CLLADDR(ifp->if_sadl);

		reg = enaddr[0] |
		      (enaddr[1] << 8) |
		      (enaddr[2] << 16) |
		      (enaddr[3] << 24);
		TULIP_WRITE(sc, CSR_AX_FILTIDX, AX_FILTIDX_PAR0);
		TULIP_WRITE(sc, CSR_AX_FILTDATA, reg);

		reg = enaddr[4] | (enaddr[5] << 8);
		TULIP_WRITE(sc, CSR_AX_FILTIDX, AX_FILTIDX_PAR1);
		TULIP_WRITE(sc, CSR_AX_FILTDATA, reg);
		break;
	    }

	default:
		/* Nothing. */
		break;
	}

	/*
	 * Set the receive filter.  This will start the transmit and
	 * receive processes.
	 */
	(*sc->sc_filter_setup)(sc);

	/*
	 * Set the current media.
	 */
	(void) (*sc->sc_mediasw->tmsw_set)(sc);

	/*
	 * Start the receive process.
	 */
	TULIP_WRITE(sc, CSR_RXPOLL, RXPOLL_RPD);

	if (sc->sc_tick != NULL) {
		/* Start the one second clock. */
		callout_reset(&sc->sc_tick_callout, hz >> 3, sc->sc_tick, sc);
	}

	/*
	 * Note that the interface is now running.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_if_flags = ifp->if_flags;

 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	}
	return (error);
}

/*
 * tlp_enable:
 *
 *	Enable the Tulip chip.
 */
static int
tlp_enable(struct tulip_softc *sc)
{

	if (TULIP_IS_ENABLED(sc) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
		sc->sc_flags |= TULIPF_ENABLED;
	}
	return (0);
}

/*
 * tlp_disable:
 *
 *	Disable the Tulip chip.
 */
static void
tlp_disable(struct tulip_softc *sc)
{

	if (TULIP_IS_ENABLED(sc) && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_flags &= ~TULIPF_ENABLED;
	}
}

/*
 * tlp_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
tlp_rxdrain(struct tulip_softc *sc)
{
	struct tulip_rxsoft *rxs;
	int i;

	for (i = 0; i < TULIP_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * tlp_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
static void
tlp_stop(struct ifnet *ifp, int disable)
{
	struct tulip_softc *sc = ifp->if_softc;
	struct tulip_txsoft *txs;

	if (sc->sc_tick != NULL) {
		/* Stop the one second clock. */
		callout_stop(&sc->sc_tick_callout);
	}

	if (sc->sc_flags & TULIPF_HAS_MII) {
		/* Down the MII. */
		mii_down(&sc->sc_mii);
	}

	/* Disable interrupts. */
	TULIP_WRITE(sc, CSR_INTEN, 0);

	/* Stop the transmit and receive processes. */
	sc->sc_opmode = 0;
	TULIP_WRITE(sc, CSR_OPMODE, 0);
	TULIP_WRITE(sc, CSR_RXLIST, 0);
	TULIP_WRITE(sc, CSR_TXLIST, 0);

	/*
	 * Release any queued transmit buffers.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	sc->sc_flags &= ~(TULIPF_WANT_SETUP|TULIPF_DOING_SETUP);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_timer = 0;

	/*
	 * Reset the chip (needed on some flavors to actually disable it).
	 */
	tlp_reset(sc);

	if (disable) {
		tlp_rxdrain(sc);
		tlp_disable(sc);
	}
}

#define	SROM_EMIT(sc, x)						\
do {									\
	TULIP_WRITE((sc), CSR_MIIROM, (x));				\
	delay(2);							\
} while (0)

/*
 * tlp_srom_idle:
 *
 *	Put the SROM in idle state.
 */
static void
tlp_srom_idle(struct tulip_softc *sc)
{
	uint32_t miirom;
	int i;

	miirom = MIIROM_SR;
	SROM_EMIT(sc, miirom);

	miirom |= MIIROM_RD;
	SROM_EMIT(sc, miirom);

	miirom |= MIIROM_SROMCS;
	SROM_EMIT(sc, miirom);

	SROM_EMIT(sc, miirom|MIIROM_SROMSK);

	/* Strobe the clock 32 times. */
	for (i = 0; i < 32; i++) {
		SROM_EMIT(sc, miirom);
		SROM_EMIT(sc, miirom|MIIROM_SROMSK);
	}

	SROM_EMIT(sc, miirom);

	miirom &= ~MIIROM_SROMCS;
	SROM_EMIT(sc, miirom);

	SROM_EMIT(sc, 0);
}

/*
 * tlp_srom_size:
 *
 *	Determine the number of address bits in the SROM.
 */
static int
tlp_srom_size(struct tulip_softc *sc)
{
	uint32_t miirom;
	int x;

	/* Select the SROM. */
	miirom = MIIROM_SR;
	SROM_EMIT(sc, miirom);

	miirom |= MIIROM_RD;
	SROM_EMIT(sc, miirom);

	/* Send CHIP SELECT for one clock tick. */
	miirom |= MIIROM_SROMCS;
	SROM_EMIT(sc, miirom);

	/* Shift in the READ opcode. */
	for (x = 3; x > 0; x--) {
		if (TULIP_SROM_OPC_READ & (1 << (x - 1)))
			miirom |= MIIROM_SROMDI;
		else
			miirom &= ~MIIROM_SROMDI;
		SROM_EMIT(sc, miirom);
		SROM_EMIT(sc, miirom|MIIROM_SROMSK);
		SROM_EMIT(sc, miirom);
	}

	/* Shift in address and look for dummy 0 bit. */
	for (x = 1; x <= 12; x++) {
		miirom &= ~MIIROM_SROMDI;
		SROM_EMIT(sc, miirom);
		SROM_EMIT(sc, miirom|MIIROM_SROMSK);
		if (!TULIP_ISSET(sc, CSR_MIIROM, MIIROM_SROMDO))
			break;
		SROM_EMIT(sc, miirom);
	}

	/* Clear CHIP SELECT. */
	miirom &= ~MIIROM_SROMCS;
	SROM_EMIT(sc, miirom);

	/* Deselect the SROM. */
	SROM_EMIT(sc, 0);

	if (x < 4 || x > 12) {
		aprint_debug_dev(sc->sc_dev, "broken MicroWire interface detected; "
		    "setting SROM size to 1Kb\n");
		return (6);
	} else {
		if (tlp_srom_debug)
			printf("%s: SROM size is 2^%d*16 bits (%d bytes)\n",
			    device_xname(sc->sc_dev), x, (1 << (x + 4)) >> 3);
		return (x);
	}
}

/*
 * tlp_read_srom:
 *
 *	Read the Tulip SROM.
 */
int
tlp_read_srom(struct tulip_softc *sc)
{
	int size;
	uint32_t miirom;
	uint16_t datain;
	int i, x;

	tlp_srom_idle(sc);

	sc->sc_srom_addrbits = tlp_srom_size(sc);
	if (sc->sc_srom_addrbits == 0)
		return (0);
	size = TULIP_ROM_SIZE(sc->sc_srom_addrbits);
	sc->sc_srom = malloc(size, M_DEVBUF, M_NOWAIT);

	/* Select the SROM. */
	miirom = MIIROM_SR;
	SROM_EMIT(sc, miirom);

	miirom |= MIIROM_RD;
	SROM_EMIT(sc, miirom);

	for (i = 0; i < size; i += 2) {
		/* Send CHIP SELECT for one clock tick. */
		miirom |= MIIROM_SROMCS;
		SROM_EMIT(sc, miirom);

		/* Shift in the READ opcode. */
		for (x = 3; x > 0; x--) {
			if (TULIP_SROM_OPC_READ & (1 << (x - 1)))
				miirom |= MIIROM_SROMDI;
			else
				miirom &= ~MIIROM_SROMDI;
			SROM_EMIT(sc, miirom);
			SROM_EMIT(sc, miirom|MIIROM_SROMSK);
			SROM_EMIT(sc, miirom);
		}

		/* Shift in address. */
		for (x = sc->sc_srom_addrbits; x > 0; x--) {
			if (i & (1 << x))
				miirom |= MIIROM_SROMDI;
			else
				miirom &= ~MIIROM_SROMDI;
			SROM_EMIT(sc, miirom);
			SROM_EMIT(sc, miirom|MIIROM_SROMSK);
			SROM_EMIT(sc, miirom);
		}

		/* Shift out data. */
		miirom &= ~MIIROM_SROMDI;
		datain = 0;
		for (x = 16; x > 0; x--) {
			SROM_EMIT(sc, miirom|MIIROM_SROMSK);
			if (TULIP_ISSET(sc, CSR_MIIROM, MIIROM_SROMDO))
				datain |= (1 << (x - 1));
			SROM_EMIT(sc, miirom);
		}
		sc->sc_srom[i] = datain & 0xff;
		sc->sc_srom[i + 1] = datain >> 8;

		/* Clear CHIP SELECT. */
		miirom &= ~MIIROM_SROMCS;
		SROM_EMIT(sc, miirom);
	}

	/* Deselect the SROM. */
	SROM_EMIT(sc, 0);

	/* ...and idle it. */
	tlp_srom_idle(sc);

	if (tlp_srom_debug) {
		printf("SROM CONTENTS:");
		for (i = 0; i < size; i++) {
			if ((i % 8) == 0)
				printf("\n\t");
			printf("0x%02x ", sc->sc_srom[i]);
		}
		printf("\n");
	}

	return (1);
}

#undef SROM_EMIT

/*
 * tlp_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
tlp_add_rxbuf(struct tulip_softc *sc, int idx)
{
	struct tulip_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't load rx DMA map %d, error = %d\n",
		    idx, error);
		panic("tlp_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	TULIP_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * tlp_srom_crcok:
 *
 *	Check the CRC of the Tulip SROM.
 */
int
tlp_srom_crcok(const uint8_t *romdata)
{
	uint32_t crc;

	crc = ether_crc32_le(romdata, TULIP_ROM_CRC32_CHECKSUM);
	crc = (crc & 0xffff) ^ 0xffff;
	if (crc == TULIP_ROM_GETW(romdata, TULIP_ROM_CRC32_CHECKSUM))
		return (1);

	/*
	 * Try an alternate checksum.
	 */
	crc = ether_crc32_le(romdata, TULIP_ROM_CRC32_CHECKSUM1);
	crc = (crc & 0xffff) ^ 0xffff;
	if (crc == TULIP_ROM_GETW(romdata, TULIP_ROM_CRC32_CHECKSUM1))
		return (1);

	return (0);
}

/*
 * tlp_isv_srom:
 *
 *	Check to see if the SROM is in the new standardized format.
 */
int
tlp_isv_srom(const uint8_t *romdata)
{
	int i;
	uint16_t cksum;

	if (tlp_srom_crcok(romdata)) {
		/*
		 * SROM CRC checks out; must be in the new format.
		 */
		return (1);
	}

	cksum = TULIP_ROM_GETW(romdata, TULIP_ROM_CRC32_CHECKSUM);
	if (cksum == 0xffff || cksum == 0) {
		/*
		 * No checksum present.  Check the SROM ID; 18 bytes of 0
		 * followed by 1 (version) followed by the number of
		 * adapters which use this SROM (should be non-zero).
		 */
		for (i = 0; i < TULIP_ROM_SROM_FORMAT_VERION; i++) {
			if (romdata[i] != 0)
				return (0);
		}
		if (romdata[TULIP_ROM_SROM_FORMAT_VERION] != 1)
			return (0);
		if (romdata[TULIP_ROM_CHIP_COUNT] == 0)
			return (0);
		return (1);
	}

	return (0);
}

/*
 * tlp_isv_srom_enaddr:
 *
 *	Get the Ethernet address from an ISV SROM.
 */
int
tlp_isv_srom_enaddr(struct tulip_softc *sc, uint8_t *enaddr)
{
	int i, devcnt;

	if (tlp_isv_srom(sc->sc_srom) == 0)
		return (0);

	devcnt = sc->sc_srom[TULIP_ROM_CHIP_COUNT];
	for (i = 0; i < devcnt; i++) {
		if (sc->sc_srom[TULIP_ROM_CHIP_COUNT] == 1)
			break;
		if (sc->sc_srom[TULIP_ROM_CHIPn_DEVICE_NUMBER(i)] ==
		    sc->sc_devno)
			break;
	}

	if (i == devcnt)
		return (0);

	memcpy(enaddr, &sc->sc_srom[TULIP_ROM_IEEE_NETWORK_ADDRESS],
	    ETHER_ADDR_LEN);
	enaddr[5] += i;

	return (1);
}

/*
 * tlp_parse_old_srom:
 *
 *	Parse old-format SROMs.
 *
 *	This routine is largely lifted from Matt Thomas's `de' driver.
 */
int
tlp_parse_old_srom(struct tulip_softc *sc, uint8_t *enaddr)
{
	static const uint8_t testpat[] =
	    { 0xff, 0, 0x55, 0xaa, 0xff, 0, 0x55, 0xaa };
	int i;
	uint32_t cksum;

	if (memcmp(&sc->sc_srom[0], &sc->sc_srom[16], 8) != 0) {
		/*
		 * Phobos G100 interfaces have the address at
		 * offsets 0 and 20, but each pair of bytes is
		 * swapped.
		 */
		if (sc->sc_srom_addrbits == 6 &&
		    sc->sc_srom[1] == 0x00 &&
		    sc->sc_srom[0] == 0x60 &&
		    sc->sc_srom[3] == 0xf5 &&
		    memcmp(&sc->sc_srom[0], &sc->sc_srom[20], 6) == 0) {
			for (i = 0; i < 6; i += 2) {
				enaddr[i] = sc->sc_srom[i + 1];
				enaddr[i + 1] = sc->sc_srom[i];
			}
			return (1);
		}

		/*
		 * Phobos G130/G160 interfaces have the address at
		 * offsets 20 and 84, but each pair of bytes is
		 * swapped.
		 */
		if (sc->sc_srom_addrbits == 6 &&
		    sc->sc_srom[21] == 0x00 &&
		    sc->sc_srom[20] == 0x60 &&
		    sc->sc_srom[23] == 0xf5 &&
		    memcmp(&sc->sc_srom[20], &sc->sc_srom[84], 6) == 0) {
			for (i = 0; i < 6; i += 2) {
				enaddr[i] = sc->sc_srom[20 + i + 1];
				enaddr[i + 1] = sc->sc_srom[20 + i];
			}
			return (1);
		}

		/*
		 * Cobalt Networks interfaces simply have the address
		 * in the first six bytes. The rest is zeroed out
		 * on some models, but others contain unknown data.
		 */
		if (sc->sc_srom[0] == 0x00 &&
		    sc->sc_srom[1] == 0x10 &&
		    sc->sc_srom[2] == 0xe0) {
			memcpy(enaddr, sc->sc_srom, ETHER_ADDR_LEN);
			return (1);
		}

		/*
		 * Some vendors (e.g. ZNYX) don't use the standard
		 * DEC Address ROM format, but rather just have an
		 * Ethernet address in the first 6 bytes, maybe a
		 * 2 byte checksum, and then all 0xff's.
		 */
		for (i = 8; i < 32; i++) {
			if (sc->sc_srom[i] != 0xff &&
			    sc->sc_srom[i] != 0)
				return (0);
		}

		/*
		 * Sanity check the Ethernet address:
		 *
		 *	- Make sure it's not multicast or locally
		 *	  assigned
		 *	- Make sure it has a non-0 OUI
		 */
		if (sc->sc_srom[0] & 3)
			return (0);
		if (sc->sc_srom[0] == 0 && sc->sc_srom[1] == 0 &&
		    sc->sc_srom[2] == 0)
			return (0);

		memcpy(enaddr, sc->sc_srom, ETHER_ADDR_LEN);
		return (1);
	}

	/*
	 * Standard DEC Address ROM test.
	 */

	if (memcmp(&sc->sc_srom[24], testpat, 8) != 0)
		return (0);

	for (i = 0; i < 8; i++) {
		if (sc->sc_srom[i] != sc->sc_srom[15 - i])
			return (0);
	}

	memcpy(enaddr, sc->sc_srom, ETHER_ADDR_LEN);

	cksum = *(uint16_t *) &enaddr[0];

	cksum <<= 1;
	if (cksum > 0xffff)
		cksum -= 0xffff;

	cksum += *(uint16_t *) &enaddr[2];
	if (cksum > 0xffff)
		cksum -= 0xffff;

	cksum <<= 1;
	if (cksum > 0xffff)
		cksum -= 0xffff;

	cksum += *(uint16_t *) &enaddr[4];
	if (cksum >= 0xffff)
		cksum -= 0xffff;

	if (cksum != *(uint16_t *) &sc->sc_srom[6])
		return (0);

	return (1);
}

/*
 * tlp_filter_setup:
 *
 *	Set the Tulip's receive filter.
 */
static void
tlp_filter_setup(struct tulip_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	volatile uint32_t *sp;
	struct tulip_txsoft *txs;
	struct tulip_desc *txd;
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint32_t hash, hashsize;
	int cnt, nexttx;

	DPRINTF(sc, ("%s: tlp_filter_setup: sc_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags));

	memcpy(enaddr, CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN);

	/*
	 * If there are transmissions pending, wait until they have
	 * completed.
	 */
	if (! SIMPLEQ_EMPTY(&sc->sc_txdirtyq) ||
	    (sc->sc_flags & TULIPF_DOING_SETUP) != 0) {
		sc->sc_flags |= TULIPF_WANT_SETUP;
		DPRINTF(sc, ("%s: tlp_filter_setup: deferring\n",
		    device_xname(sc->sc_dev)));
		return;
	}
	sc->sc_flags &= ~TULIPF_WANT_SETUP;

	switch (sc->sc_chip) {
	case TULIP_CHIP_82C115:
		hashsize = TULIP_PNICII_HASHSIZE;
		break;

	default:
		hashsize = TULIP_MCHASHSIZE;
	}

	/*
	 * If we're running, idle the transmit and receive engines.  If
	 * we're NOT running, we're being called from tlp_init(), and our
	 * writing OPMODE will start the transmit and receive processes
	 * in motion.
	 */
	if (ifp->if_flags & IFF_RUNNING)
		tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	sc->sc_opmode &= ~(OPMODE_PR|OPMODE_PM);

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_opmode |= OPMODE_PR;
		goto allmulti;
	}

	/*
	 * Try Perfect filtering first.
	 */

	sc->sc_filtmode = TDCTL_Tx_FT_PERFECT;
	sp = TULIP_CDSP(sc);
	memset(TULIP_CDSP(sc), 0, TULIP_SETUP_PACKET_LEN);
	cnt = 0;
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}
		if (cnt == (TULIP_MAXADDRS - 2)) {
			/*
			 * We already have our multicast limit (still need
			 * our station address and broadcast).  Go to
			 * Hash-Perfect mode.
			 */
			goto hashperfect;
		}
		cnt++;
		*sp++ = htole32(TULIP_SP_FIELD(enm->enm_addrlo, 0));
		*sp++ = htole32(TULIP_SP_FIELD(enm->enm_addrlo, 1));
		*sp++ = htole32(TULIP_SP_FIELD(enm->enm_addrlo, 2));
		ETHER_NEXT_MULTI(step, enm);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		/* ...and the broadcast address. */
		cnt++;
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
	}

	/* Pad the rest with our station address. */
	for (; cnt < TULIP_MAXADDRS; cnt++) {
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 0));
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 1));
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 2));
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 hashperfect:
	/*
	 * Try Hash-Perfect mode.
	 */

	/*
	 * Some 21140 chips have broken Hash-Perfect modes.  On these
	 * chips, we simply use Hash-Only mode, and put our station
	 * address into the filter.
	 */
	if (sc->sc_chip == TULIP_CHIP_21140)
		sc->sc_filtmode = TDCTL_Tx_FT_HASHONLY;
	else
		sc->sc_filtmode = TDCTL_Tx_FT_HASH;
	sp = TULIP_CDSP(sc);
	memset(TULIP_CDSP(sc), 0, TULIP_SETUP_PACKET_LEN);
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}
		hash = tlp_mchash(enm->enm_addrlo, hashsize);
		sp[hash >> 4] |= htole32(1 << (hash & 0xf));
		ETHER_NEXT_MULTI(step, enm);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		/* ...and the broadcast address. */
		hash = tlp_mchash(etherbroadcastaddr, hashsize);
		sp[hash >> 4] |= htole32(1 << (hash & 0xf));
	}

	if (sc->sc_filtmode == TDCTL_Tx_FT_HASHONLY) {
		/* ...and our station address. */
		hash = tlp_mchash(enaddr, hashsize);
		sp[hash >> 4] |= htole32(1 << (hash & 0xf));
	} else {
		/*
		 * Hash-Perfect mode; put our station address after
		 * the hash table.
		 */
		sp[39] = htole32(TULIP_SP_FIELD(enaddr, 0));
		sp[40] = htole32(TULIP_SP_FIELD(enaddr, 1));
		sp[41] = htole32(TULIP_SP_FIELD(enaddr, 2));
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	/*
	 * Use Perfect filter mode.  First address is the broadcast address,
	 * and pad the rest with our station address.  We'll set Pass-all-
	 * multicast in OPMODE below.
	 */
	sc->sc_filtmode = TDCTL_Tx_FT_PERFECT;
	sp = TULIP_CDSP(sc);
	memset(TULIP_CDSP(sc), 0, TULIP_SETUP_PACKET_LEN);
	cnt = 0;
	if (ifp->if_flags & IFF_BROADCAST) {
		cnt++;
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
		*sp++ = htole32(TULIP_SP_FIELD_C(0xff, 0xff));
	}
	for (; cnt < TULIP_MAXADDRS; cnt++) {
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 0));
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 1));
		*sp++ = htole32(TULIP_SP_FIELD(enaddr, 2));
	}
	ifp->if_flags |= IFF_ALLMULTI;

 setit:
	if (ifp->if_flags & IFF_ALLMULTI)
		sc->sc_opmode |= OPMODE_PM;

	/* Sync the setup packet buffer. */
	TULIP_CDSPSYNC(sc, BUS_DMASYNC_PREWRITE);

	/*
	 * Fill in the setup packet descriptor.
	 */
	txs = SIMPLEQ_FIRST(&sc->sc_txfreeq);

	txs->txs_firstdesc = sc->sc_txnext;
	txs->txs_lastdesc = sc->sc_txnext;
	txs->txs_ndescs = 1;
	txs->txs_mbuf = NULL;

	nexttx = sc->sc_txnext;
	txd = &sc->sc_txdescs[nexttx];
	txd->td_status = 0;
	txd->td_bufaddr1 = htole32(TULIP_CDSPADDR(sc));
	txd->td_ctl = htole32((TULIP_SETUP_PACKET_LEN << TDCTL_SIZE1_SHIFT) |
	    sc->sc_filtmode | TDCTL_Tx_SET | sc->sc_setup_fsls |
	    TDCTL_Tx_IC | sc->sc_tdctl_ch |
	    (nexttx == (TULIP_NTXDESC - 1) ? sc->sc_tdctl_er : 0));
	TULIP_CDTXSYNC(sc, nexttx, 1,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

#ifdef TLP_DEBUG
	if (ifp->if_flags & IFF_DEBUG) {
		printf("     filter_setup %p transmit chain:\n", txs);
		printf("     descriptor %d:\n", nexttx);
		printf("       td_status:   0x%08x\n", le32toh(txd->td_status));
		printf("       td_ctl:      0x%08x\n", le32toh(txd->td_ctl));
		printf("       td_bufaddr1: 0x%08x\n",
		    le32toh(txd->td_bufaddr1));
		printf("       td_bufaddr2: 0x%08x\n",
		    le32toh(txd->td_bufaddr2));
	}
#endif

	txd->td_status = htole32(TDSTAT_OWN);
	TULIP_CDTXSYNC(sc, nexttx, 1,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/* Advance the tx pointer. */
	sc->sc_txfree -= 1;
	sc->sc_txnext = TULIP_NEXTTX(nexttx);

	SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
	SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

	/*
	 * Set the OPMODE register.  This will also resume the
	 * transmit process we idled above.
	 */
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	sc->sc_flags |= TULIPF_DOING_SETUP;

	/*
	 * Kick the transmitter; this will cause the Tulip to
	 * read the setup descriptor.
	 */
	/* XXX USE AUTOPOLLING? */
	TULIP_WRITE(sc, CSR_TXPOLL, TXPOLL_TPD);

	/* Set up a watchdog timer in case the chip flakes out. */
	ifp->if_timer = 5;

	DPRINTF(sc, ("%s: tlp_filter_setup: returning\n", device_xname(sc->sc_dev)));
}

/*
 * tlp_winb_filter_setup:
 *
 *	Set the Winbond 89C840F's receive filter.
 */
static void
tlp_winb_filter_setup(struct tulip_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t hash, mchash[2];

	DPRINTF(sc, ("%s: tlp_winb_filter_setup: sc_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags));

	sc->sc_opmode &= ~(OPMODE_WINB_APP|OPMODE_WINB_AMP|OPMODE_WINB_ABP);

	if (ifp->if_flags & IFF_MULTICAST)
		sc->sc_opmode |= OPMODE_WINB_AMP;

	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_opmode |= OPMODE_WINB_ABP;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_opmode |= OPMODE_WINB_APP;
		goto allmulti;
	}

	mchash[0] = mchash[1] = 0;

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		/*
		 * According to the FreeBSD `wb' driver, yes, you
		 * really do invert the hash.
		 */
		hash =
		    (~(ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26))
		    & 0x3f;
		mchash[hash >> 5] |= 1 << (hash & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	mchash[0] = mchash[1] = 0xffffffff;

 setit:
	TULIP_WRITE(sc, CSR_WINB_CMA0, mchash[0]);
	TULIP_WRITE(sc, CSR_WINB_CMA1, mchash[1]);
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
	DPRINTF(sc, ("%s: tlp_winb_filter_setup: returning\n",
	    device_xname(sc->sc_dev)));
}

/*
 * tlp_al981_filter_setup:
 *
 *	Set the ADMtek AL981's receive filter.
 */
static void
tlp_al981_filter_setup(struct tulip_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t hash, mchash[2];

	/*
	 * If the chip is running, we need to reset the interface,
	 * and will revisit here (with IFF_RUNNING) clear.  The
	 * chip seems to really not like to have its multicast
	 * filter programmed without a reset.
	 */
	if (ifp->if_flags & IFF_RUNNING) {
		(void) tlp_init(ifp);
		return;
	}

	DPRINTF(sc, ("%s: tlp_al981_filter_setup: sc_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags));

	sc->sc_opmode &= ~(OPMODE_PR|OPMODE_PM);

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_opmode |= OPMODE_PR;
		goto allmulti;
	}

	mchash[0] = mchash[1] = 0;

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		hash = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN) & 0x3f;
		mchash[hash >> 5] |= 1 << (hash & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	mchash[0] = mchash[1] = 0xffffffff;

 setit:
	bus_space_write_4(sc->sc_st, sc->sc_sh, CSR_ADM_MAR0, mchash[0]);
	bus_space_write_4(sc->sc_st, sc->sc_sh, CSR_ADM_MAR1, mchash[1]);
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
	DPRINTF(sc, ("%s: tlp_al981_filter_setup: returning\n",
	    device_xname(sc->sc_dev)));
}

/*
 * tlp_asix_filter_setup:
 *
 * 	Set the ASIX AX8814x recieve filter.
 */
static void
tlp_asix_filter_setup(struct tulip_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t hash, mchash[2];

	DPRINTF(sc, ("%s: tlp_asix_filter_setup: sc_flags 0x%08x\n",
		device_xname(sc->sc_dev), sc->sc_flags));

	sc->sc_opmode &= ~(OPMODE_PM|OPMODE_AX_RB|OPMODE_PR);

	if (ifp->if_flags & IFF_MULTICAST)
		sc->sc_opmode |= OPMODE_PM;

	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_opmode |= OPMODE_AX_RB;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_opmode |= OPMODE_PR;
		goto allmulti;
	}

	mchash[0] = mchash[1] = 0;

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}
		hash = (ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26)
		       & 0x3f;
		if (hash < 32)
			mchash[0] |= (1 << hash);
		else
			mchash[1] |= (1 << (hash - 32));
		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	mchash[0] = mchash[1] = 0xffffffff;

setit:
	TULIP_WRITE(sc, CSR_AX_FILTIDX, AX_FILTIDX_MAR0);
	TULIP_WRITE(sc, CSR_AX_FILTDATA, mchash[0]);
	TULIP_WRITE(sc, CSR_AX_FILTIDX, AX_FILTIDX_MAR1);
	TULIP_WRITE(sc, CSR_AX_FILTDATA, mchash[1]);
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
	DPRINTF(sc, ("%s: tlp_asix_filter_setup: returning\n",
		device_xname(sc->sc_dev)));
}


/*
 * tlp_idle:
 *
 *	Cause the transmit and/or receive processes to go idle.
 */
void
tlp_idle(struct tulip_softc *sc, uint32_t bits)
{
	static const char * const tlp_tx_state_names[] = {
		"STOPPED",
		"RUNNING - FETCH",
		"RUNNING - WAIT",
		"RUNNING - READING",
		"-- RESERVED --",
		"RUNNING - SETUP",
		"SUSPENDED",
		"RUNNING - CLOSE",
	};
	static const char * const tlp_rx_state_names[] = {
		"STOPPED",
		"RUNNING - FETCH",
		"RUNNING - CHECK",
		"RUNNING - WAIT",
		"SUSPENDED",
		"RUNNING - CLOSE",
		"RUNNING - FLUSH",
		"RUNNING - QUEUE",
	};
	static const char * const dm9102_tx_state_names[] = {
		"STOPPED",
		"RUNNING - FETCH",
		"RUNNING - SETUP",
		"RUNNING - READING",
		"RUNNING - CLOSE - CLEAR OWNER",
		"RUNNING - WAIT",
		"RUNNING - CLOSE - WRITE STATUS",
		"SUSPENDED",
	};
	static const char * const dm9102_rx_state_names[] = {
		"STOPPED",
		"RUNNING - FETCH",
		"RUNNING - WAIT",
		"RUNNING - QUEUE",
		"RUNNING - CLOSE - CLEAR OWNER",
		"RUNNING - CLOSE - WRITE STATUS",
		"SUSPENDED",
		"RUNNING - FLUSH",
	};

	const char * const *tx_state_names, * const *rx_state_names;
	uint32_t csr, ackmask = 0;
	int i;

	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		tx_state_names = dm9102_tx_state_names;
		rx_state_names = dm9102_rx_state_names;
		break;

	default:
		tx_state_names = tlp_tx_state_names;
		rx_state_names = tlp_rx_state_names;
		break;
	}

	if (bits & OPMODE_ST)
		ackmask |= STATUS_TPS;

	if (bits & OPMODE_SR)
		ackmask |= STATUS_RPS;

	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode & ~bits);

	for (i = 0; i < 1000; i++) {
		if (TULIP_ISSET(sc, CSR_STATUS, ackmask) == ackmask)
			break;
		delay(10);
	}

	csr = TULIP_READ(sc, CSR_STATUS);
	if ((csr & ackmask) != ackmask) {
		if ((bits & OPMODE_ST) != 0 && (csr & STATUS_TPS) == 0 &&
		    (csr & STATUS_TS) != STATUS_TS_STOPPED) {
			switch (sc->sc_chip) {
			case TULIP_CHIP_AX88140:
			case TULIP_CHIP_AX88141:
				/*
				 * Filter the message out on noisy chips.
				 */
				break;
			default:
				printf("%s: transmit process failed to idle: "
				    "state %s\n", device_xname(sc->sc_dev),
				    tx_state_names[(csr & STATUS_TS) >> 20]);
			}
		}
		if ((bits & OPMODE_SR) != 0 && (csr & STATUS_RPS) == 0 &&
		    (csr & STATUS_RS) != STATUS_RS_STOPPED) {
			switch (sc->sc_chip) {
			case TULIP_CHIP_AN983:
			case TULIP_CHIP_AN985:
			case TULIP_CHIP_DM9102A:
			case TULIP_CHIP_RS7112:
				/*
				 * Filter the message out on noisy chips.
				 */
				break;
			default:
				printf("%s: receive process failed to idle: "
				    "state %s\n", device_xname(sc->sc_dev),
				    rx_state_names[(csr & STATUS_RS) >> 17]);
			}
		}
	}
	TULIP_WRITE(sc, CSR_STATUS, ackmask);
}

/*****************************************************************************
 * Generic media support functions.
 *****************************************************************************/

/*
 * tlp_mediastatus:	[ifmedia interface function]
 *
 *	Query the current media.
 */
void
tlp_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct tulip_softc *sc = ifp->if_softc;

	if (TULIP_IS_ENABLED(sc) == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	(*sc->sc_mediasw->tmsw_get)(sc, ifmr);
}

/*
 * tlp_mediachange:	[ifmedia interface function]
 *
 *	Update the current media.
 */
int
tlp_mediachange(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return (0);
	return ((*sc->sc_mediasw->tmsw_set)(sc));
}

/*****************************************************************************
 * Support functions for MII-attached media.
 *****************************************************************************/

/*
 * tlp_mii_tick:
 *
 *	One second timer, used to tick the MII.
 */
static void
tlp_mii_tick(void *arg)
{
	struct tulip_softc *sc = arg;
	int s;

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_callout, hz, sc->sc_tick, sc);
}

/*
 * tlp_mii_statchg:	[mii interface function]
 *
 *	Callback from PHY when media changes.
 */
static void
tlp_mii_statchg(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;

	/* Idle the transmit and receive processes. */
	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	sc->sc_opmode &= ~(OPMODE_TTM|OPMODE_FD|OPMODE_HBD);

	if (IFM_SUBTYPE(sc->sc_mii.mii_media_active) == IFM_10_T)
		sc->sc_opmode |= OPMODE_TTM;
	else
		sc->sc_opmode |= OPMODE_HBD;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_opmode |= OPMODE_FD|OPMODE_HBD;

	/*
	 * Write new OPMODE bits.  This also restarts the transmit
	 * and receive processes.
	 */
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_winb_mii_statchg: [mii interface function]
 *
 *	Callback from PHY when media changes.  This version is
 *	for the Winbond 89C840F, which has different OPMODE bits.
 */
static void
tlp_winb_mii_statchg(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;

	/* Idle the transmit and receive processes. */
	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	sc->sc_opmode &= ~(OPMODE_WINB_FES|OPMODE_FD);

	if (IFM_SUBTYPE(sc->sc_mii.mii_media_active) == IFM_100_TX)
		sc->sc_opmode |= OPMODE_WINB_FES;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_opmode |= OPMODE_FD;

	/*
	 * Write new OPMODE bits.  This also restarts the transmit
	 * and receive processes.
	 */
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_dm9102_mii_statchg: [mii interface function]
 *
 *	Callback from PHY when media changes.  This version is
 *	for the DM9102.
 */
static void
tlp_dm9102_mii_statchg(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;

	/*
	 * Don't idle the transmit and receive processes, here.  It
	 * seems to fail, and just causes excess noise.
	 */
	sc->sc_opmode &= ~(OPMODE_TTM|OPMODE_FD);

	if (IFM_SUBTYPE(sc->sc_mii.mii_media_active) != IFM_100_TX)
		sc->sc_opmode |= OPMODE_TTM;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_opmode |= OPMODE_FD;

	/*
	 * Write new OPMODE bits.
	 */
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_mii_getmedia:
 *
 *	Callback from ifmedia to request current media status.
 */
static void
tlp_mii_getmedia(struct tulip_softc *sc, struct ifmediareq *ifmr)
{

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

/*
 * tlp_mii_setmedia:
 *
 *	Callback from ifmedia to request new media setting.
 */
static int
tlp_mii_setmedia(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int rc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;
	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
		/* Disable the internal Nway engine. */
		TULIP_WRITE(sc, CSR_SIATXRX, 0);
		break;

	default:
		/* Nothing. */
		break;
	}
	if ((rc = mii_mediachg(&sc->sc_mii)) == ENXIO)
		return 0;
	return rc;
}

/*
 * tlp_bitbang_mii_readreg:
 *
 *	Read a PHY register via bit-bang'ing the MII.
 */
static int
tlp_bitbang_mii_readreg(device_t self, int phy, int reg)
{
	struct tulip_softc *sc = device_private(self);

	return (mii_bitbang_readreg(self, sc->sc_bitbang_ops, phy, reg));
}

/*
 * tlp_bitbang_mii_writereg:
 *
 *	Write a PHY register via bit-bang'ing the MII.
 */
static void
tlp_bitbang_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct tulip_softc *sc = device_private(self);

	mii_bitbang_writereg(self, sc->sc_bitbang_ops, phy, reg, val);
}

/*
 * tlp_sio_mii_bitbang_read:
 *
 *	Read the MII serial port for the MII bit-bang module.
 */
static uint32_t
tlp_sio_mii_bitbang_read(device_t self)
{
	struct tulip_softc *sc = device_private(self);

	return (TULIP_READ(sc, CSR_MIIROM));
}

/*
 * tlp_sio_mii_bitbang_write:
 *
 *	Write the MII serial port for the MII bit-bang module.
 */
static void
tlp_sio_mii_bitbang_write(device_t self, uint32_t val)
{
	struct tulip_softc *sc = device_private(self);

	TULIP_WRITE(sc, CSR_MIIROM, val);
}

/*
 * tlp_pnic_mii_readreg:
 *
 *	Read a PHY register on the Lite-On PNIC.
 */
static int
tlp_pnic_mii_readreg(device_t self, int phy, int reg)
{
	struct tulip_softc *sc = device_private(self);
	uint32_t val;
	int i;

	TULIP_WRITE(sc, CSR_PNIC_MII,
	    PNIC_MII_MBO | PNIC_MII_RESERVED |
	    PNIC_MII_READ | (phy << PNIC_MII_PHYSHIFT) |
	    (reg << PNIC_MII_REGSHIFT));

	for (i = 0; i < 1000; i++) {
		delay(10);
		val = TULIP_READ(sc, CSR_PNIC_MII);
		if ((val & PNIC_MII_BUSY) == 0) {
			if ((val & PNIC_MII_DATA) == PNIC_MII_DATA)
				return (0);
			else
				return (val & PNIC_MII_DATA);
		}
	}
	printf("%s: MII read timed out\n", device_xname(sc->sc_dev));
	return (0);
}

/*
 * tlp_pnic_mii_writereg:
 *
 *	Write a PHY register on the Lite-On PNIC.
 */
static void
tlp_pnic_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct tulip_softc *sc = device_private(self);
	int i;

	TULIP_WRITE(sc, CSR_PNIC_MII,
	    PNIC_MII_MBO | PNIC_MII_RESERVED |
	    PNIC_MII_WRITE | (phy << PNIC_MII_PHYSHIFT) |
	    (reg << PNIC_MII_REGSHIFT) | val);

	for (i = 0; i < 1000; i++) {
		delay(10);
		if (TULIP_ISSET(sc, CSR_PNIC_MII, PNIC_MII_BUSY) == 0)
			return;
	}
	printf("%s: MII write timed out\n", device_xname(sc->sc_dev));
}

static const bus_addr_t tlp_al981_phy_regmap[] = {
	CSR_ADM_BMCR,
	CSR_ADM_BMSR,
	CSR_ADM_PHYIDR1,
	CSR_ADM_PHYIDR2,
	CSR_ADM_ANAR,
	CSR_ADM_ANLPAR,
	CSR_ADM_ANER,

	CSR_ADM_XMC,
	CSR_ADM_XCIIS,
	CSR_ADM_XIE,
	CSR_ADM_100CTR,
};
static const int tlp_al981_phy_regmap_size = sizeof(tlp_al981_phy_regmap) /
    sizeof(tlp_al981_phy_regmap[0]);

/*
 * tlp_al981_mii_readreg:
 *
 *	Read a PHY register on the ADMtek AL981.
 */
static int
tlp_al981_mii_readreg(device_t self, int phy, int reg)
{
	struct tulip_softc *sc = device_private(self);

	/* AL981 only has an internal PHY. */
	if (phy != 0)
		return (0);

	if (reg >= tlp_al981_phy_regmap_size)
		return (0);

	return (bus_space_read_4(sc->sc_st, sc->sc_sh,
	    tlp_al981_phy_regmap[reg]) & 0xffff);
}

/*
 * tlp_al981_mii_writereg:
 *
 *	Write a PHY register on the ADMtek AL981.
 */
static void
tlp_al981_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct tulip_softc *sc = device_private(self);

	/* AL981 only has an internal PHY. */
	if (phy != 0)
		return;

	if (reg >= tlp_al981_phy_regmap_size)
		return;

	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    tlp_al981_phy_regmap[reg], val);
}

/*****************************************************************************
 * Chip-specific pre-init and reset functions.
 *****************************************************************************/

/*
 * tlp_2114x_preinit:
 *
 *	Pre-init function shared by DECchip 21140, 21140A, 21142, and 21143.
 */
static void
tlp_2114x_preinit(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	struct tulip_21x4x_media *tm = ife->ifm_aux;

	/*
	 * Whether or not we're in MII or SIA/SYM mode, the media info
	 * contains the appropriate OPMODE bits.
	 *
	 * Also, we always set the Must-Be-One bit.
	 */
	sc->sc_opmode |= OPMODE_MBO | tm->tm_opmode;

	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_2114x_mii_preinit:
 *
 *	Pre-init function shared by DECchip 21140, 21140A, 21142, and 21143.
 *	This version is used by boards which only have MII and don't have
 *	an ISV SROM.
 */
static void
tlp_2114x_mii_preinit(struct tulip_softc *sc)
{

	/*
	 * Always set the Must-Be-One bit, and Port Select (to select MII).
	 * We'll never be called during a media change.
	 */
	sc->sc_opmode |= OPMODE_MBO|OPMODE_PS;
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_pnic_preinit:
 *
 *	Pre-init function for the Lite-On 82c168 and 82c169.
 */
static void
tlp_pnic_preinit(struct tulip_softc *sc)
{

	if (sc->sc_flags & TULIPF_HAS_MII) {
		/*
		 * MII case: just set the port-select bit; we will never
		 * be called during a media change.
		 */
		sc->sc_opmode |= OPMODE_PS;
	} else {
		/*
		 * ENDEC/PCS/Nway mode; enable the Tx backoff counter.
		 */
		sc->sc_opmode |= OPMODE_PNIC_TBEN;
	}
}

/*
 * tlp_asix_preinit:
 *
 * 	Pre-init function for the ASIX chipsets.
 */
static void
tlp_asix_preinit(struct tulip_softc *sc)
{

	switch (sc->sc_chip) {
		case TULIP_CHIP_AX88140:
		case TULIP_CHIP_AX88141:
			/* XXX Handle PHY. */
			sc->sc_opmode |= OPMODE_HBD|OPMODE_PS;
			break;
		default:
			/* Nothing */
			break;
	}

	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_dm9102_preinit:
 *
 *	Pre-init function for the Davicom DM9102.
 */
static void
tlp_dm9102_preinit(struct tulip_softc *sc)
{

	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
		sc->sc_opmode |= OPMODE_MBO|OPMODE_HBD|OPMODE_PS;
		break;

	case TULIP_CHIP_DM9102A:
		/*
		 * XXX Figure out how to actually deal with the HomePNA
		 * XXX portion of the DM9102A.
		 */
		sc->sc_opmode |= OPMODE_MBO|OPMODE_HBD;
		break;

	default:
		/* Nothing. */
		break;
	}

	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

/*
 * tlp_21140_reset:
 *
 *	Issue a reset sequence on the 21140 via the GPIO facility.
 */
static void
tlp_21140_reset(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	struct tulip_21x4x_media *tm = ife->ifm_aux;
	int i;

	/* First, set the direction on the GPIO pins. */
	TULIP_WRITE(sc, CSR_GPP, GPP_GPC|sc->sc_gp_dir);

	/* Now, issue the reset sequence. */
	for (i = 0; i < tm->tm_reset_length; i++) {
		delay(10);
		TULIP_WRITE(sc, CSR_GPP, sc->sc_srom[tm->tm_reset_offset + i]);
	}

	/* Now, issue the selection sequence. */
	for (i = 0; i < tm->tm_gp_length; i++) {
		delay(10);
		TULIP_WRITE(sc, CSR_GPP, sc->sc_srom[tm->tm_gp_offset + i]);
	}

	/* If there were no sequences, just lower the pins. */
	if (tm->tm_reset_length == 0 && tm->tm_gp_length == 0) {
		delay(10);
		TULIP_WRITE(sc, CSR_GPP, 0);
	}
}

/*
 * tlp_21142_reset:
 *
 *	Issue a reset sequence on the 21142 via the GPIO facility.
 */
static void
tlp_21142_reset(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	struct tulip_21x4x_media *tm = ife->ifm_aux;
	const uint8_t *cp;
	int i;

	cp = &sc->sc_srom[tm->tm_reset_offset];
	for (i = 0; i < tm->tm_reset_length; i++, cp += 2) {
		delay(10);
		TULIP_WRITE(sc, CSR_SIAGEN, TULIP_ROM_GETW(cp, 0) << 16);
	}

	cp = &sc->sc_srom[tm->tm_gp_offset];
	for (i = 0; i < tm->tm_gp_length; i++, cp += 2) {
		delay(10);
		TULIP_WRITE(sc, CSR_SIAGEN, TULIP_ROM_GETW(cp, 0) << 16);
	}

	/* If there were no sequences, just lower the pins. */
	if (tm->tm_reset_length == 0 && tm->tm_gp_length == 0) {
		delay(10);
		TULIP_WRITE(sc, CSR_SIAGEN, 0);
	}
}

/*
 * tlp_pmac_reset:
 *
 *	Reset routine for Macronix chips.
 */
static void
tlp_pmac_reset(struct tulip_softc *sc)
{

	switch (sc->sc_chip) {
	case TULIP_CHIP_82C115:
	case TULIP_CHIP_MX98715:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98725:
		/*
		 * Set the LED operating mode.  This information is located
		 * in the EEPROM at byte offset 0x77, per the MX98715A and
		 * MX98725 application notes.
		 */
		TULIP_WRITE(sc, CSR_MIIROM, sc->sc_srom[0x77] << 24);
		break;
	case TULIP_CHIP_MX98715AEC_X:
		/*
		 * Set the LED operating mode.  This information is located
		 * in the EEPROM at byte offset 0x76, per the MX98715AEC
		 * application note.
		 */
		TULIP_WRITE(sc, CSR_MIIROM, ((0xf & sc->sc_srom[0x76]) << 28)
		    | ((0xf0 & sc->sc_srom[0x76]) << 20));
		break;

	default:
		/* Nothing. */
		break;
	}
}

#if 0
/*
 * tlp_dm9102_reset:
 *
 *	Reset routine for the Davicom DM9102.
 */
static void
tlp_dm9102_reset(struct tulip_softc *sc)
{

	TULIP_WRITE(sc, CSR_DM_PHYSTAT, DM_PHYSTAT_GEPC|DM_PHYSTAT_GPED);
	delay(100);
	TULIP_WRITE(sc, CSR_DM_PHYSTAT, 0);
}
#endif

/*****************************************************************************
 * Chip/board-specific media switches.  The ones here are ones that
 * are potentially common to multiple front-ends.
 *****************************************************************************/

/*
 * This table is a common place for all sorts of media information,
 * keyed off of the SROM media code for that media.
 *
 * Note that we explicitly configure the 21142/21143 to always advertise
 * NWay capabilities when using the UTP port.
 * XXX Actually, we don't yet.
 */
static const struct tulip_srom_to_ifmedia tulip_srom_to_ifmedia_table[] = {
	{ TULIP_ROM_MB_MEDIA_TP,	IFM_10_T,	0,
	  "10baseT",
	  OPMODE_TTM,
	  BMSR_10THDX,
	  { SIACONN_21040_10BASET,
	    SIATXRX_21040_10BASET,
	    SIAGEN_21040_10BASET },

	  { SIACONN_21041_10BASET,
	    SIATXRX_21041_10BASET,
	    SIAGEN_21041_10BASET },

	  { SIACONN_21142_10BASET,
	    SIATXRX_21142_10BASET,
	    SIAGEN_21142_10BASET } },

	{ TULIP_ROM_MB_MEDIA_BNC,	IFM_10_2,	0,
	  "10base2",
	  0,
	  0,
	  { 0,
	    0,
	    0 },

	  { SIACONN_21041_BNC,
	    SIATXRX_21041_BNC,
	    SIAGEN_21041_BNC },

	  { SIACONN_21142_BNC,
	    SIATXRX_21142_BNC,
	    SIAGEN_21142_BNC } },

	{ TULIP_ROM_MB_MEDIA_AUI,	IFM_10_5,	0,
	  "10base5",
	  0,
	  0,
	  { SIACONN_21040_AUI,
	    SIATXRX_21040_AUI,
	    SIAGEN_21040_AUI },

	  { SIACONN_21041_AUI,
	    SIATXRX_21041_AUI,
	    SIAGEN_21041_AUI },

	  { SIACONN_21142_AUI,
	    SIATXRX_21142_AUI,
	    SIAGEN_21142_AUI } },

	{ TULIP_ROM_MB_MEDIA_100TX,	IFM_100_TX,	0,
	  "100baseTX",
	  OPMODE_PS|OPMODE_PCS|OPMODE_SCR|OPMODE_HBD,
	  BMSR_100TXHDX,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    SIAGEN_ABM } },

	{ TULIP_ROM_MB_MEDIA_TP_FDX,	IFM_10_T,	IFM_FDX,
	  "10baseT-FDX",
	  OPMODE_TTM|OPMODE_FD|OPMODE_HBD,
	  BMSR_10TFDX,
	  { SIACONN_21040_10BASET_FDX,
	    SIATXRX_21040_10BASET_FDX,
	    SIAGEN_21040_10BASET_FDX },

	  { SIACONN_21041_10BASET_FDX,
	    SIATXRX_21041_10BASET_FDX,
	    SIAGEN_21041_10BASET_FDX },

	  { SIACONN_21142_10BASET_FDX,
	    SIATXRX_21142_10BASET_FDX,
	    SIAGEN_21142_10BASET_FDX } },

	{ TULIP_ROM_MB_MEDIA_100TX_FDX,	IFM_100_TX,	IFM_FDX,
	  "100baseTX-FDX",
	  OPMODE_PS|OPMODE_PCS|OPMODE_SCR|OPMODE_FD|OPMODE_HBD,
	  BMSR_100TXFDX,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    SIAGEN_ABM } },

	{ TULIP_ROM_MB_MEDIA_100T4,	IFM_100_T4,	0,
	  "100baseT4",
	  OPMODE_PS|OPMODE_PCS|OPMODE_SCR|OPMODE_HBD,
	  BMSR_100T4,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    SIAGEN_ABM } },

	{ TULIP_ROM_MB_MEDIA_100FX,	IFM_100_FX,	0,
	  "100baseFX",
	  OPMODE_PS|OPMODE_PCS|OPMODE_HBD,
	  0,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    SIAGEN_ABM } },

	{ TULIP_ROM_MB_MEDIA_100FX_FDX,	IFM_100_FX,	IFM_FDX,
	  "100baseFX-FDX",
	  OPMODE_PS|OPMODE_PCS|OPMODE_FD|OPMODE_HBD,
	  0,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    SIAGEN_ABM } },

	{ 0,				0,		0,
	  NULL,
	  0,
	  0,
	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 },

	  { 0,
	    0,
	    0 } },
};

static const struct tulip_srom_to_ifmedia *tlp_srom_to_ifmedia(uint8_t);
static void	tlp_srom_media_info(struct tulip_softc *,
		    const struct tulip_srom_to_ifmedia *,
		    struct tulip_21x4x_media *);
static void	tlp_add_srom_media(struct tulip_softc *, int,
		    void (*)(struct tulip_softc *, struct ifmediareq *),
		    int (*)(struct tulip_softc *), const uint8_t *, int);
static void	tlp_print_media(struct tulip_softc *);
static void	tlp_nway_activate(struct tulip_softc *, int);
static void	tlp_get_minst(struct tulip_softc *);

static const struct tulip_srom_to_ifmedia *
tlp_srom_to_ifmedia(uint8_t sm)
{
	const struct tulip_srom_to_ifmedia *tsti;

	for (tsti = tulip_srom_to_ifmedia_table;
	     tsti->tsti_name != NULL; tsti++) {
		if (tsti->tsti_srom == sm)
			return (tsti);
	}

	return (NULL);
}

static void
tlp_srom_media_info(struct tulip_softc *sc,
    const struct tulip_srom_to_ifmedia *tsti, struct tulip_21x4x_media *tm)
{

	tm->tm_name = tsti->tsti_name;
	tm->tm_opmode = tsti->tsti_opmode;

	sc->sc_sia_cap |= tsti->tsti_sia_cap;

	switch (sc->sc_chip) {
	case TULIP_CHIP_DE425:
	case TULIP_CHIP_21040:
		tm->tm_sia = tsti->tsti_21040;	/* struct assignment */
		break;

	case TULIP_CHIP_21041:
		tm->tm_sia = tsti->tsti_21041;	/* struct assignment */
		break;

	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
	case TULIP_CHIP_82C115:
	case TULIP_CHIP_MX98715:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98715AEC_X:
	case TULIP_CHIP_MX98725:
		tm->tm_sia = tsti->tsti_21142;	/* struct assignment */
		break;

	default:
		/* Nothing. */
		break;
	}
}

static void
tlp_add_srom_media(struct tulip_softc *sc, int type,
    void (*get)(struct tulip_softc *, struct ifmediareq *),
    int (*set)(struct tulip_softc *), const uint8_t *list,
    int cnt)
{
	struct tulip_21x4x_media *tm;
	const struct tulip_srom_to_ifmedia *tsti;
	int i;

	for (i = 0; i < cnt; i++) {
		tsti = tlp_srom_to_ifmedia(list[i]);
		tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);
		tlp_srom_media_info(sc, tsti, tm);
		tm->tm_type = type;
		tm->tm_get = get;
		tm->tm_set = set;

		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_MAKEWORD(IFM_ETHER, tsti->tsti_subtype,
		    tsti->tsti_options, sc->sc_tlp_minst), 0, tm);
	}
}

static void
tlp_print_media(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife;
	struct tulip_21x4x_media *tm;
	const char *sep = "";

#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	aprint_normal_dev(sc->sc_dev, "");
	TAILQ_FOREACH(ife, &sc->sc_mii.mii_media.ifm_list, ifm_list) {
		tm = ife->ifm_aux;
		if (tm == NULL) {
#ifdef DIAGNOSTIC
			if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
				panic("tlp_print_media");
#endif
			PRINT("auto");
		} else if (tm->tm_type != TULIP_ROM_MB_21140_MII &&
			   tm->tm_type != TULIP_ROM_MB_21142_MII) {
			PRINT(tm->tm_name);
		}
	}
	aprint_normal("\n");

#undef PRINT
}

static void
tlp_nway_activate(struct tulip_softc *sc, int media)
{
	struct ifmedia_entry *ife;

	ife = ifmedia_match(&sc->sc_mii.mii_media, media, 0);
#ifdef DIAGNOSTIC
	if (ife == NULL)
		panic("tlp_nway_activate");
#endif
	sc->sc_nway_active = ife;
}

static void
tlp_get_minst(struct tulip_softc *sc)
{

	if ((sc->sc_media_seen &
	    ~((1 << TULIP_ROM_MB_21140_MII) |
	      (1 << TULIP_ROM_MB_21142_MII))) == 0) {
		/*
		 * We have not yet seen any SIA/SYM media (but are
		 * about to; that's why we're called!), so assign
		 * the current media instance to be the `internal media'
		 * instance, and advance it so any MII media gets a
		 * fresh one (used to selecting/isolating a PHY).
		 */
		sc->sc_tlp_minst = sc->sc_mii.mii_instance++;
	}
}

/*
 * SIA Utility functions.
 */
static void	tlp_sia_update_link(struct tulip_softc *);
static void	tlp_sia_get(struct tulip_softc *, struct ifmediareq *);
static int	tlp_sia_set(struct tulip_softc *);
static int	tlp_sia_media(struct tulip_softc *, struct ifmedia_entry *);
static void	tlp_sia_fixup(struct tulip_softc *);

static void
tlp_sia_update_link(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife;
	struct tulip_21x4x_media *tm;
	uint32_t siastat;

	ife = TULIP_CURRENT_MEDIA(sc);
	tm = ife->ifm_aux;

	sc->sc_flags &= ~(TULIPF_LINK_UP|TULIPF_LINK_VALID);

	siastat = TULIP_READ(sc, CSR_SIASTAT);

	/*
	 * Note that when we do SIA link tests, we are assuming that
	 * the chip is really in the mode that the current media setting
	 * reflects.  If we're not, then the link tests will not be
	 * accurate!
	 */
	switch (IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_10_T:
		sc->sc_flags |= TULIPF_LINK_VALID;
		if ((siastat & SIASTAT_LS10) == 0)
			sc->sc_flags |= TULIPF_LINK_UP;
		break;

	case IFM_100_TX:
	case IFM_100_T4:
		sc->sc_flags |= TULIPF_LINK_VALID;
		if ((siastat & SIASTAT_LS100) == 0)
			sc->sc_flags |= TULIPF_LINK_UP;
		break;
	}

	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
		/*
		 * On these chips, we can tell more information about
		 * AUI/BNC.  Note that the AUI/BNC selection is made
		 * in a different register; for our purpose, it's all
		 * AUI.
		 */
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_10_2:
		case IFM_10_5:
			sc->sc_flags |= TULIPF_LINK_VALID;
			if (siastat & SIASTAT_ARA) {
				TULIP_WRITE(sc, CSR_SIASTAT, SIASTAT_ARA);
				sc->sc_flags |= TULIPF_LINK_UP;
			}
			break;

		default:
			/*
			 * If we're SYM media and can detect the link
			 * via the GPIO facility, prefer that status
			 * over LS100.
			 */
			if (tm->tm_type == TULIP_ROM_MB_21143_SYM &&
			    tm->tm_actmask != 0) {
				sc->sc_flags = (sc->sc_flags &
				    ~TULIPF_LINK_UP) | TULIPF_LINK_VALID;
				if (TULIP_ISSET(sc, CSR_SIAGEN,
				    tm->tm_actmask) == tm->tm_actdata)
					sc->sc_flags |= TULIPF_LINK_UP;
			}
		}
		break;

	default:
		/* Nothing. */
		break;
	}
}

static void
tlp_sia_get(struct tulip_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia_entry *ife;

	ifmr->ifm_status = 0;

	tlp_sia_update_link(sc);

	ife = TULIP_CURRENT_MEDIA(sc);

	if (sc->sc_flags & TULIPF_LINK_VALID)
		ifmr->ifm_status |= IFM_AVALID;
	if (sc->sc_flags & TULIPF_LINK_UP)
		ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active = ife->ifm_media;
}

static void
tlp_sia_fixup(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife;
	struct tulip_21x4x_media *tm;
	uint32_t siaconn, siatxrx, siagen;

	switch (sc->sc_chip) {
	case TULIP_CHIP_82C115:
	case TULIP_CHIP_MX98713A:
	case TULIP_CHIP_MX98715:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98715AEC_X:
	case TULIP_CHIP_MX98725:
		siaconn = PMAC_SIACONN_MASK;
		siatxrx = PMAC_SIATXRX_MASK;
		siagen  = PMAC_SIAGEN_MASK;
		break;

	default:
		/* No fixups required on any other chips. */
		return;
	}

	TAILQ_FOREACH(ife, &sc->sc_mii.mii_media.ifm_list, ifm_list) {
		tm = ife->ifm_aux;
		if (tm == NULL)
			continue;

		tm->tm_siaconn &= siaconn;
		tm->tm_siatxrx &= siatxrx;
		tm->tm_siagen  &= siagen;
	}
}

static int
tlp_sia_set(struct tulip_softc *sc)
{

	return (tlp_sia_media(sc, TULIP_CURRENT_MEDIA(sc)));
}

static int
tlp_sia_media(struct tulip_softc *sc, struct ifmedia_entry *ife)
{
	struct tulip_21x4x_media *tm;

	tm = ife->ifm_aux;

	/*
	 * XXX This appears to be necessary on a bunch of the clone chips.
	 */
	delay(20000);

	/*
	 * Idle the chip.
	 */
	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	/*
	 * Program the SIA.  It's important to write in this order,
	 * resetting the SIA first.
	 */
	TULIP_WRITE(sc, CSR_SIACONN, 0);		/* SRL bit clear */
	delay(1000);

	TULIP_WRITE(sc, CSR_SIATXRX, tm->tm_siatxrx);

	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
		TULIP_WRITE(sc, CSR_SIAGEN, tm->tm_siagen | tm->tm_gpctl);
		TULIP_WRITE(sc, CSR_SIAGEN, tm->tm_siagen | tm->tm_gpdata);
		break;
	default:
		TULIP_WRITE(sc, CSR_SIAGEN, tm->tm_siagen);
	}

	TULIP_WRITE(sc, CSR_SIACONN, tm->tm_siaconn);

	/*
	 * Set the OPMODE bits for this media and write OPMODE.
	 * This will resume the transmit and receive processes.
	 */
	sc->sc_opmode = (sc->sc_opmode & ~OPMODE_MEDIA_BITS) | tm->tm_opmode;
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	return (0);
}

/*
 * 21140 GPIO utility functions.
 */
static void	tlp_21140_gpio_update_link(struct tulip_softc *);

static void
tlp_21140_gpio_update_link(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife;
	struct tulip_21x4x_media *tm;

	ife = TULIP_CURRENT_MEDIA(sc);
	tm = ife->ifm_aux;

	sc->sc_flags &= ~(TULIPF_LINK_UP|TULIPF_LINK_VALID);

	if (tm->tm_actmask != 0) {
		sc->sc_flags |= TULIPF_LINK_VALID;
		if (TULIP_ISSET(sc, CSR_GPP, tm->tm_actmask) ==
		    tm->tm_actdata)
			sc->sc_flags |= TULIPF_LINK_UP;
	}
}

void
tlp_21140_gpio_get(struct tulip_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia_entry *ife;

	ifmr->ifm_status = 0;

	tlp_21140_gpio_update_link(sc);

	ife = TULIP_CURRENT_MEDIA(sc);

	if (sc->sc_flags & TULIPF_LINK_VALID)
		ifmr->ifm_status |= IFM_AVALID;
	if (sc->sc_flags & TULIPF_LINK_UP)
		ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active = ife->ifm_media;
}

int
tlp_21140_gpio_set(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife;
	struct tulip_21x4x_media *tm;

	ife = TULIP_CURRENT_MEDIA(sc);
	tm = ife->ifm_aux;

	/*
	 * Idle the chip.
	 */
	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	/*
	 * Set the GPIO pins for this media, to flip any
	 * relays, etc.
	 */
	TULIP_WRITE(sc, CSR_GPP, GPP_GPC|sc->sc_gp_dir);
	delay(10);
	TULIP_WRITE(sc, CSR_GPP, tm->tm_gpdata);

	/*
	 * Set the OPMODE bits for this media and write OPMODE.
	 * This will resume the transmit and receive processes.
	 */
	sc->sc_opmode = (sc->sc_opmode & ~OPMODE_MEDIA_BITS) | tm->tm_opmode;
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	return (0);
}

/*
 * 21040 and 21041 media switches.
 */
static void	tlp_21040_tmsw_init(struct tulip_softc *);
static void	tlp_21040_tp_tmsw_init(struct tulip_softc *);
static void	tlp_21040_auibnc_tmsw_init(struct tulip_softc *);
static void	tlp_21041_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_21040_mediasw = {
	tlp_21040_tmsw_init, tlp_sia_get, tlp_sia_set
};

const struct tulip_mediasw tlp_21040_tp_mediasw = {
	tlp_21040_tp_tmsw_init, tlp_sia_get, tlp_sia_set
};

const struct tulip_mediasw tlp_21040_auibnc_mediasw = {
	tlp_21040_auibnc_tmsw_init, tlp_sia_get, tlp_sia_set
};

const struct tulip_mediasw tlp_21041_mediasw = {
	tlp_21041_tmsw_init, tlp_sia_get, tlp_sia_set
};

static void
tlp_21040_tmsw_init(struct tulip_softc *sc)
{
	static const uint8_t media[] = {
		TULIP_ROM_MB_MEDIA_TP,
		TULIP_ROM_MB_MEDIA_TP_FDX,
		TULIP_ROM_MB_MEDIA_AUI,
	};
	struct tulip_21x4x_media *tm;

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	tlp_add_srom_media(sc, 0, NULL, NULL, media, 3);

	/*
	 * No SROM type for External SIA.
	 */
	tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);
	tm->tm_name = "manual";
	tm->tm_opmode = 0;
	tm->tm_siaconn = SIACONN_21040_EXTSIA;
	tm->tm_siatxrx = SIATXRX_21040_EXTSIA;
	tm->tm_siagen  = SIAGEN_21040_EXTSIA;
	ifmedia_add(&sc->sc_mii.mii_media,
	    IFM_MAKEWORD(IFM_ETHER, IFM_MANUAL, 0, sc->sc_tlp_minst), 0, tm);

	/*
	 * XXX Autosense not yet supported.
	 */

	/* XXX This should be auto-sense. */
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);

	tlp_print_media(sc);
}

static void
tlp_21040_tp_tmsw_init(struct tulip_softc *sc)
{
	static const uint8_t media[] = {
		TULIP_ROM_MB_MEDIA_TP,
		TULIP_ROM_MB_MEDIA_TP_FDX,
	};

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	tlp_add_srom_media(sc, 0, NULL, NULL, media, 2);

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);

	tlp_print_media(sc);
}

static void
tlp_21040_auibnc_tmsw_init(struct tulip_softc *sc)
{
	static const uint8_t media[] = {
		TULIP_ROM_MB_MEDIA_AUI,
	};

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	tlp_add_srom_media(sc, 0, NULL, NULL, media, 1);

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_5);

	tlp_print_media(sc);
}

static void
tlp_21041_tmsw_init(struct tulip_softc *sc)
{
	static const uint8_t media[] = {
		TULIP_ROM_MB_MEDIA_TP,
		TULIP_ROM_MB_MEDIA_TP_FDX,
		TULIP_ROM_MB_MEDIA_BNC,
		TULIP_ROM_MB_MEDIA_AUI,
	};
	int i, defmedia, devcnt, leaf_offset, mb_offset, m_cnt;
	const struct tulip_srom_to_ifmedia *tsti;
	struct tulip_21x4x_media *tm;
	uint16_t romdef;
	uint8_t mb;

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	if (tlp_isv_srom(sc->sc_srom) == 0) {
 not_isv_srom:
		/*
		 * If we have a board without the standard 21041 SROM format,
		 * we just assume all media are present and try and pick a
		 * reasonable default.
		 */
		tlp_add_srom_media(sc, 0, NULL, NULL, media, 4);

		/*
		 * XXX Autosense not yet supported.
		 */

		/* XXX This should be auto-sense. */
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);

		tlp_print_media(sc);
		return;
	}

	devcnt = sc->sc_srom[TULIP_ROM_CHIP_COUNT];
	for (i = 0; i < devcnt; i++) {
		if (sc->sc_srom[TULIP_ROM_CHIP_COUNT] == 1)
			break;
		if (sc->sc_srom[TULIP_ROM_CHIPn_DEVICE_NUMBER(i)] ==
		    sc->sc_devno)
			break;
	}

	if (i == devcnt)
		goto not_isv_srom;

	leaf_offset = TULIP_ROM_GETW(sc->sc_srom,
	    TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(i));
	mb_offset = leaf_offset + TULIP_ROM_IL_MEDIAn_BLOCK_BASE;
	m_cnt = sc->sc_srom[leaf_offset + TULIP_ROM_IL_MEDIA_COUNT];

	for (; m_cnt != 0;
	     m_cnt--, mb_offset += TULIP_ROM_MB_SIZE(mb)) {
		mb = sc->sc_srom[mb_offset];
		tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);
		switch (mb & TULIP_ROM_MB_MEDIA_CODE) {
		case TULIP_ROM_MB_MEDIA_TP_FDX:
		case TULIP_ROM_MB_MEDIA_TP:
		case TULIP_ROM_MB_MEDIA_BNC:
		case TULIP_ROM_MB_MEDIA_AUI:
			tsti = tlp_srom_to_ifmedia(mb &
			    TULIP_ROM_MB_MEDIA_CODE);

			tlp_srom_media_info(sc, tsti, tm);

			/*
			 * Override our default SIA settings if the
			 * SROM contains its own.
			 */
			if (mb & TULIP_ROM_MB_EXT) {
				tm->tm_siaconn = TULIP_ROM_GETW(sc->sc_srom,
				    mb_offset + TULIP_ROM_MB_CSR13);
				tm->tm_siatxrx = TULIP_ROM_GETW(sc->sc_srom,
				    mb_offset + TULIP_ROM_MB_CSR14);
				tm->tm_siagen = TULIP_ROM_GETW(sc->sc_srom,
				    mb_offset + TULIP_ROM_MB_CSR15);
			}

			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_MAKEWORD(IFM_ETHER, tsti->tsti_subtype,
			    tsti->tsti_options, sc->sc_tlp_minst), 0, tm);
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown media code 0x%02x\n",
			    mb & TULIP_ROM_MB_MEDIA_CODE);
			free(tm, M_DEVBUF);
		}
	}

	/*
	 * XXX Autosense not yet supported.
	 */

	romdef = TULIP_ROM_GETW(sc->sc_srom, leaf_offset +
	    TULIP_ROM_IL_SELECT_CONN_TYPE);
	switch (romdef) {
	case SELECT_CONN_TYPE_TP:
	case SELECT_CONN_TYPE_TP_AUTONEG:
	case SELECT_CONN_TYPE_TP_NOLINKPASS:
		defmedia = IFM_ETHER|IFM_10_T;
		break;

	case SELECT_CONN_TYPE_TP_FDX:
		defmedia = IFM_ETHER|IFM_10_T|IFM_FDX;
		break;

	case SELECT_CONN_TYPE_BNC:
		defmedia = IFM_ETHER|IFM_10_2;
		break;

	case SELECT_CONN_TYPE_AUI:
		defmedia = IFM_ETHER|IFM_10_5;
		break;
#if 0 /* XXX */
	case SELECT_CONN_TYPE_ASENSE:
	case SELECT_CONN_TYPE_ASENSE_AUTONEG:
		defmedia = IFM_ETHER|IFM_AUTO;
		break;
#endif
	default:
		defmedia = 0;
	}

	if (defmedia == 0) {
		/*
		 * XXX We should default to auto-sense.
		 */
		defmedia = IFM_ETHER|IFM_10_T;
	}

	ifmedia_set(&sc->sc_mii.mii_media, defmedia);

	tlp_print_media(sc);
}

/*
 * DECchip 2114x ISV media switch.
 */
static void	tlp_2114x_isv_tmsw_init(struct tulip_softc *);
static void	tlp_2114x_isv_tmsw_get(struct tulip_softc *,
		    struct ifmediareq *);
static int	tlp_2114x_isv_tmsw_set(struct tulip_softc *);

const struct tulip_mediasw tlp_2114x_isv_mediasw = {
	tlp_2114x_isv_tmsw_init, tlp_2114x_isv_tmsw_get, tlp_2114x_isv_tmsw_set
};

static void	tlp_2114x_nway_get(struct tulip_softc *, struct ifmediareq *);
static int	tlp_2114x_nway_set(struct tulip_softc *);

static void	tlp_2114x_nway_statchg(struct ifnet *);
static int	tlp_2114x_nway_service(struct tulip_softc *, int);
static void	tlp_2114x_nway_auto(struct tulip_softc *);
static void	tlp_2114x_nway_status(struct tulip_softc *);

static void
tlp_2114x_isv_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ifmedia_entry *ife;
	struct mii_softc *phy;
	struct tulip_21x4x_media *tm;
	const struct tulip_srom_to_ifmedia *tsti;
	int i, devcnt, leaf_offset, m_cnt, type, length;
	int defmedia, miidef;
	uint16_t word;
	uint8_t *cp, *ncp;

	defmedia = miidef = 0;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;

	/*
	 * Ignore `instance'; we may get a mixture of SIA and MII
	 * media, and `instance' is used to isolate or select the
	 * PHY on the MII as appropriate.  Note that duplicate media
	 * are disallowed, so ignoring `instance' is safe.
	 */
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, tlp_mediachange,
	    tlp_mediastatus);

	devcnt = sc->sc_srom[TULIP_ROM_CHIP_COUNT];
	for (i = 0; i < devcnt; i++) {
		if (sc->sc_srom[TULIP_ROM_CHIP_COUNT] == 1)
			break;
		if (sc->sc_srom[TULIP_ROM_CHIPn_DEVICE_NUMBER(i)] ==
		    sc->sc_devno)
			break;
	}

	if (i == devcnt) {
		aprint_error_dev(sc->sc_dev, "unable to locate info leaf in SROM\n");
		return;
	}

	leaf_offset = TULIP_ROM_GETW(sc->sc_srom,
	    TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(i));

	/* XXX SELECT CONN TYPE */

	cp = &sc->sc_srom[leaf_offset + TULIP_ROM_IL_MEDIA_COUNT];

	/*
	 * On some chips, the first thing in the Info Leaf is the
	 * GPIO pin direction data.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21140:
	case TULIP_CHIP_21140A:
	case TULIP_CHIP_MX98713:
	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		sc->sc_gp_dir = *cp++;
		break;

	default:
		/* Nothing. */
		break;
	}

	/* Get the media count. */
	m_cnt = *cp++;

	if (m_cnt == 0) {
		sc->sc_mediasw = &tlp_sio_mii_mediasw;
		(*sc->sc_mediasw->tmsw_init)(sc);
		return;
	}

	for (; m_cnt != 0; cp = ncp, m_cnt--) {
		/*
		 * Determine the type and length of this media block.
		 * The 21143 is spec'd to always use extended format blocks,
		 * but some cards don't set the bit to indicate this.
		 * Hopefully there are no cards which really don't use
		 * extended format blocks.
		 */
		if ((*cp & 0x80) == 0 && sc->sc_chip != TULIP_CHIP_21143) {
			length = 4;
			type = TULIP_ROM_MB_21140_GPR;
		} else {
			length = (*cp++ & 0x7f) - 1;
			type = *cp++ & 0x3f;
		}

		/* Compute the start of the next block. */
		ncp = cp + length;

		/* Now, parse the block. */
		switch (type) {
		case TULIP_ROM_MB_21140_GPR:
			tlp_get_minst(sc);
			sc->sc_media_seen |= 1 << TULIP_ROM_MB_21140_GPR;

			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);

			tm->tm_type = TULIP_ROM_MB_21140_GPR;
			tm->tm_get = tlp_21140_gpio_get;
			tm->tm_set = tlp_21140_gpio_set;

			/* First is the media type code. */
			tsti = tlp_srom_to_ifmedia(cp[0] &
			    TULIP_ROM_MB_MEDIA_CODE);
			if (tsti == NULL) {
				/* Invalid media code. */
				free(tm, M_DEVBUF);
				break;
			}

			/* Get defaults. */
			tlp_srom_media_info(sc, tsti, tm);

			/* Next is any GPIO info for this media. */
			tm->tm_gpdata = cp[1];

			/*
			 * Next is a word containing OPMODE information
			 * and info on how to detect if this media is
			 * active.
			 */
			word = TULIP_ROM_GETW(cp, 2);
			tm->tm_opmode &= OPMODE_FD;
			tm->tm_opmode |= TULIP_ROM_MB_OPMODE(word);
			if ((word & TULIP_ROM_MB_NOINDICATOR) == 0) {
				tm->tm_actmask =
				    TULIP_ROM_MB_BITPOS(word);
				tm->tm_actdata =
				    (word & TULIP_ROM_MB_POLARITY) ?
				    0 : tm->tm_actmask;
			}

			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_MAKEWORD(IFM_ETHER, tsti->tsti_subtype,
			    tsti->tsti_options, sc->sc_tlp_minst), 0, tm);
			break;

		case TULIP_ROM_MB_21140_MII:
			sc->sc_media_seen |= 1 << TULIP_ROM_MB_21140_MII;

			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);

			tm->tm_type = TULIP_ROM_MB_21140_MII;
			tm->tm_get = tlp_mii_getmedia;
			tm->tm_set = tlp_mii_setmedia;
			tm->tm_opmode = OPMODE_PS;

			if (sc->sc_reset == NULL)
				sc->sc_reset = tlp_21140_reset;

			/* First is the PHY number. */
			tm->tm_phyno = *cp++;

			/* Next is the MII select sequence length and offset. */
			tm->tm_gp_length = *cp++;
			tm->tm_gp_offset = cp - &sc->sc_srom[0];
			cp += tm->tm_gp_length;

			/* Next is the MII reset sequence length and offset. */
			tm->tm_reset_length = *cp++;
			tm->tm_reset_offset = cp - &sc->sc_srom[0];
			cp += tm->tm_reset_length;

			/*
			 * The following items are left in the media block
			 * that we don't particularly care about:
			 *
			 *	capabilities		W
			 *	advertisement		W
			 *	full duplex		W
			 *	tx threshold		W
			 *
			 * These appear to be bits in the PHY registers,
			 * which our MII code handles on its own.
			 */

			/*
			 * Before we probe the MII bus, we need to reset
			 * it and issue the selection sequence.
			 */

			/* Set the direction of the pins... */
			TULIP_WRITE(sc, CSR_GPP, GPP_GPC|sc->sc_gp_dir);

			for (i = 0; i < tm->tm_reset_length; i++) {
				delay(10);
				TULIP_WRITE(sc, CSR_GPP,
				    sc->sc_srom[tm->tm_reset_offset + i]);
			}

			for (i = 0; i < tm->tm_gp_length; i++) {
				delay(10);
				TULIP_WRITE(sc, CSR_GPP,
				    sc->sc_srom[tm->tm_gp_offset + i]);
			}

			/* If there were no sequences, just lower the pins. */
			if (tm->tm_reset_length == 0 && tm->tm_gp_length == 0) {
				delay(10);
				TULIP_WRITE(sc, CSR_GPP, 0);
			}

			/*
			 * Now, probe the MII for the PHY.  Note, we know
			 * the location of the PHY on the bus, but we don't
			 * particularly care; the MII code just likes to
			 * search the whole thing anyhow.
			 */
			mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
			    MII_PHY_ANY, tm->tm_phyno, 0);

			/*
			 * Now, search for the PHY we hopefully just
			 * configured.  If it's not configured into the
			 * kernel, we lose.  The PHY's default media always
			 * takes priority.
			 */
			LIST_FOREACH(phy, &sc->sc_mii.mii_phys, mii_list) {
				if (phy->mii_offset == tm->tm_phyno)
					break;
			}
			if (phy == NULL) {
				aprint_error_dev(sc->sc_dev, "unable to configure MII\n");
				break;
			}

			sc->sc_flags |= TULIPF_HAS_MII;
			sc->sc_tick = tlp_mii_tick;
			miidef = IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0,
			    phy->mii_inst);

			/*
			 * Okay, now that we've found the PHY and the MII
			 * layer has added all of the media associated
			 * with that PHY, we need to traverse the media
			 * list, and add our `tm' to each entry's `aux'
			 * pointer.
			 *
			 * We do this by looking for media with our
			 * PHY's `instance'.
			 */
			TAILQ_FOREACH(ife, &sc->sc_mii.mii_media.ifm_list,
			      ifm_list) {
				if (IFM_INST(ife->ifm_media) != phy->mii_inst)
					continue;
				ife->ifm_aux = tm;
			}
			break;

		case TULIP_ROM_MB_21142_SIA:
			tlp_get_minst(sc);
			sc->sc_media_seen |= 1 << TULIP_ROM_MB_21142_SIA;

			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);

			tm->tm_type = TULIP_ROM_MB_21142_SIA;
			tm->tm_get = tlp_sia_get;
			tm->tm_set = tlp_sia_set;

			/* First is the media type code. */
			tsti = tlp_srom_to_ifmedia(cp[0] &
			    TULIP_ROM_MB_MEDIA_CODE);
			if (tsti == NULL) {
				/* Invalid media code. */
				free(tm, M_DEVBUF);
				break;
			}

			/* Get defaults. */
			tlp_srom_media_info(sc, tsti, tm);

			/*
			 * Override our default SIA settings if the
			 * SROM contains its own.
			 */
			if (cp[0] & 0x40) {
				tm->tm_siaconn = TULIP_ROM_GETW(cp, 1);
				tm->tm_siatxrx = TULIP_ROM_GETW(cp, 3);
				tm->tm_siagen  = TULIP_ROM_GETW(cp, 5);
				cp += 7;
			} else
				cp++;

			/* Next is GPIO control/data. */
			tm->tm_gpctl  = TULIP_ROM_GETW(cp, 0) << 16;
			tm->tm_gpdata = TULIP_ROM_GETW(cp, 2) << 16;

			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_MAKEWORD(IFM_ETHER, tsti->tsti_subtype,
			    tsti->tsti_options, sc->sc_tlp_minst), 0, tm);
			break;

		case TULIP_ROM_MB_21142_MII:
			sc->sc_media_seen |= 1 << TULIP_ROM_MB_21142_MII;

			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);

			tm->tm_type = TULIP_ROM_MB_21142_MII;
			tm->tm_get = tlp_mii_getmedia;
			tm->tm_set = tlp_mii_setmedia;
			tm->tm_opmode = OPMODE_PS;

			if (sc->sc_reset == NULL)
				sc->sc_reset = tlp_21142_reset;

			/* First is the PHY number. */
			tm->tm_phyno = *cp++;

			/* Next is the MII select sequence length and offset. */
			tm->tm_gp_length = *cp++;
			tm->tm_gp_offset = cp - &sc->sc_srom[0];
			cp += tm->tm_gp_length * 2;

			/* Next is the MII reset sequence length and offset. */
			tm->tm_reset_length = *cp++;
			tm->tm_reset_offset = cp - &sc->sc_srom[0];
			cp += tm->tm_reset_length * 2;

			/*
			 * The following items are left in the media block
			 * that we don't particularly care about:
			 *
			 *	capabilities		W
			 *	advertisement		W
			 *	full duplex		W
			 *	tx threshold		W
			 *	MII interrupt		W
			 *
			 * These appear to be bits in the PHY registers,
			 * which our MII code handles on its own.
			 */

			/*
			 * Before we probe the MII bus, we need to reset
			 * it and issue the selection sequence.
			 */

			cp = &sc->sc_srom[tm->tm_reset_offset];
			for (i = 0; i < tm->tm_reset_length; i++, cp += 2) {
				delay(10);
				TULIP_WRITE(sc, CSR_SIAGEN,
				    TULIP_ROM_GETW(cp, 0) << 16);
			}

			cp = &sc->sc_srom[tm->tm_gp_offset];
			for (i = 0; i < tm->tm_gp_length; i++, cp += 2) {
				delay(10);
				TULIP_WRITE(sc, CSR_SIAGEN,
				    TULIP_ROM_GETW(cp, 0) << 16);
			}

			/* If there were no sequences, just lower the pins. */
			if (tm->tm_reset_length == 0 && tm->tm_gp_length == 0) {
				delay(10);
				TULIP_WRITE(sc, CSR_SIAGEN, 0);
			}

			/*
			 * Now, probe the MII for the PHY.  Note, we know
			 * the location of the PHY on the bus, but we don't
			 * particularly care; the MII code just likes to
			 * search the whole thing anyhow.
			 */
			mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
			    MII_PHY_ANY, tm->tm_phyno, 0);

			/*
			 * Now, search for the PHY we hopefully just
			 * configured.  If it's not configured into the
			 * kernel, we lose.  The PHY's default media always
			 * takes priority.
			 */
			LIST_FOREACH(phy, &sc->sc_mii.mii_phys, mii_list) {
				if (phy->mii_offset == tm->tm_phyno)
					break;
			}
			if (phy == NULL) {
				aprint_error_dev(sc->sc_dev, "unable to configure MII\n");
				break;
			}

			sc->sc_flags |= TULIPF_HAS_MII;
			sc->sc_tick = tlp_mii_tick;
			miidef = IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0,
			    phy->mii_inst);

			/*
			 * Okay, now that we've found the PHY and the MII
			 * layer has added all of the media associated
			 * with that PHY, we need to traverse the media
			 * list, and add our `tm' to each entry's `aux'
			 * pointer.
			 *
			 * We do this by looking for media with our
			 * PHY's `instance'.
			 */
			TAILQ_FOREACH(ife, &sc->sc_mii.mii_media.ifm_list,
			      ifm_list) {
				if (IFM_INST(ife->ifm_media) != phy->mii_inst)
					continue;
				ife->ifm_aux = tm;
			}
			break;

		case TULIP_ROM_MB_21143_SYM:
			tlp_get_minst(sc);
			sc->sc_media_seen |= 1 << TULIP_ROM_MB_21143_SYM;

			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);

			tm->tm_type = TULIP_ROM_MB_21143_SYM;
			tm->tm_get = tlp_sia_get;
			tm->tm_set = tlp_sia_set;

			/* First is the media type code. */
			tsti = tlp_srom_to_ifmedia(cp[0] &
			    TULIP_ROM_MB_MEDIA_CODE);
			if (tsti == NULL) {
				/* Invalid media code. */
				free(tm, M_DEVBUF);
				break;
			}

			/* Get defaults. */
			tlp_srom_media_info(sc, tsti, tm);

			/* Next is GPIO control/data. */
			tm->tm_gpctl  = TULIP_ROM_GETW(cp, 1) << 16;
			tm->tm_gpdata = TULIP_ROM_GETW(cp, 3) << 16;

			/*
			 * Next is a word containing OPMODE information
			 * and info on how to detect if this media is
			 * active.
			 */
			word = TULIP_ROM_GETW(cp, 5);
			tm->tm_opmode &= OPMODE_FD;
			tm->tm_opmode |= TULIP_ROM_MB_OPMODE(word);
			if ((word & TULIP_ROM_MB_NOINDICATOR) == 0) {
				tm->tm_actmask =
				    TULIP_ROM_MB_BITPOS(word);
				tm->tm_actdata =
				    (word & TULIP_ROM_MB_POLARITY) ?
				    0 : tm->tm_actmask;
			}

			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_MAKEWORD(IFM_ETHER, tsti->tsti_subtype,
			    tsti->tsti_options, sc->sc_tlp_minst), 0, tm);
			break;

		case TULIP_ROM_MB_21143_RESET:
			aprint_normal_dev(sc->sc_dev, "21143 reset block\n");
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown ISV media block type 0x%02x\n", type);
		}
	}

	/*
	 * Deal with the case where no media is configured.
	 */
	if (TAILQ_FIRST(&sc->sc_mii.mii_media.ifm_list) == NULL) {
		aprint_error_dev(sc->sc_dev, "no media found!\n");
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
		return;
	}

	/*
	 * Pick the default media.
	 */
	if (miidef != 0)
		defmedia = miidef;
	else {
		switch (sc->sc_chip) {
		case TULIP_CHIP_21140:
		case TULIP_CHIP_21140A:
			/* XXX should come from SROM */
			defmedia = IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0);
			if (ifmedia_match(&sc->sc_mii.mii_media, defmedia,
				sc->sc_mii.mii_media.ifm_mask) == NULL) {
				/*
				 * There is not a 10baseT media.
				 * Fall back to the first found one.
				 */
				ife = TAILQ_FIRST(&sc->sc_mii.mii_media.ifm_list);
				defmedia = ife->ifm_media;
			}
			break;

		case TULIP_CHIP_21142:
		case TULIP_CHIP_21143:
		case TULIP_CHIP_MX98713A:
		case TULIP_CHIP_MX98715:
		case TULIP_CHIP_MX98715A:
		case TULIP_CHIP_MX98715AEC_X:
		case TULIP_CHIP_MX98725:
			tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);
			tm->tm_name = "auto";
			tm->tm_get = tlp_2114x_nway_get;
			tm->tm_set = tlp_2114x_nway_set;

			defmedia = IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0);
			ifmedia_add(&sc->sc_mii.mii_media, defmedia, 0, tm);

			sc->sc_statchg = tlp_2114x_nway_statchg;
			sc->sc_tick = tlp_2114x_nway_tick;
			break;

		default:
			defmedia = IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0);
			break;
		}
	}

	ifmedia_set(&sc->sc_mii.mii_media, defmedia);

	/*
	 * Display any non-MII media we've located.
	 */
	if (sc->sc_media_seen &
	    ~((1 << TULIP_ROM_MB_21140_MII) | (1 << TULIP_ROM_MB_21142_MII)))
		tlp_print_media(sc);

	tlp_sia_fixup(sc);
}

static void
tlp_2114x_nway_get(struct tulip_softc *sc, struct ifmediareq *ifmr)
{

	(void) tlp_2114x_nway_service(sc, MII_POLLSTAT);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

static int
tlp_2114x_nway_set(struct tulip_softc *sc)
{

	return (tlp_2114x_nway_service(sc, MII_MEDIACHG));
}

static void
tlp_2114x_nway_statchg(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife;

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE)
		return;

	if ((ife = ifmedia_match(&mii->mii_media, mii->mii_media_active,
	    mii->mii_media.ifm_mask)) == NULL) {
		printf("tlp_2114x_nway_statchg: no match for media 0x%x/0x%x\n",
		    mii->mii_media_active, ~mii->mii_media.ifm_mask);
		panic("tlp_2114x_nway_statchg");
	}

	tlp_sia_media(sc, ife);
}

static void
tlp_2114x_nway_tick(void *arg)
{
	struct tulip_softc *sc = arg;
	struct mii_data *mii = &sc->sc_mii;
	int s, ticks;

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();
	tlp_2114x_nway_service(sc, MII_TICK);
	if ((sc->sc_flags & TULIPF_LINK_UP) == 0 &&
	    (mii->mii_media_status & IFM_ACTIVE) != 0 &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sc_flags |= TULIPF_LINK_UP;
		tlp_start(&sc->sc_ethercom.ec_if);
	} else if ((sc->sc_flags & TULIPF_LINK_UP) != 0 &&
	    (mii->mii_media_status & IFM_ACTIVE) == 0) {
		sc->sc_flags &= ~TULIPF_LINK_UP;
	}
	splx(s);

	if ((sc->sc_flags & TULIPF_LINK_UP) == 0)
		ticks = hz >> 3;
	else
		ticks = hz;
	callout_reset(&sc->sc_tick_callout, ticks, tlp_2114x_nway_tick, sc);
}

/*
 * Support for the 2114X internal NWay block.  This is constructed
 * somewhat like a PHY driver for simplicity.
 */

static int
tlp_2114x_nway_service(struct tulip_softc *sc, int cmd)
{
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return (0);

	switch (cmd) {
	case MII_POLLSTAT:
		/* Nothing special to do here. */
		break;

	case MII_MEDIACHG:
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			goto restart;
		default:
			/* Manual setting doesn't go through here. */
			printf("tlp_2114x_nway_service: oops!\n");
			return (EINVAL);
		}
		break;

	case MII_TICK:
		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			break;

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.
		 */
#if 0
		if (mii->mii_media_status & IFM_ACTIVE)
#else
		if (sc->sc_flags & TULIPF_LINK_UP)
#endif
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->sc_nway_ticks != (5 << 3))
			break;

	restart:
		sc->sc_nway_ticks = 0;
		ife->ifm_data = IFM_NONE;
		tlp_2114x_nway_auto(sc);
		break;
	}

	/* Update the media status. */
	tlp_2114x_nway_status(sc);

	/*
	 * Callback if something changed.  Manually configuration goes through
	 * tlp_sia_set() anyway, so ignore that here.
	 */
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO &&
	    ife->ifm_data != mii->mii_media_active) {
		(*sc->sc_statchg)(mii->mii_ifp);
		ife->ifm_data = mii->mii_media_active;
	}
	return (0);
}

static void
tlp_2114x_nway_auto(struct tulip_softc *sc)
{
	uint32_t siastat, siatxrx;

	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	sc->sc_opmode &= ~(OPMODE_PS|OPMODE_PCS|OPMODE_SCR|OPMODE_FD);
	sc->sc_opmode |= OPMODE_TTM|OPMODE_HBD;
	siatxrx = 0xffbf;		/* XXX magic number */

	/* Compute the link code word to advertise. */
	if (sc->sc_sia_cap & BMSR_100T4)
		siatxrx |= SIATXRX_T4;
	if (sc->sc_sia_cap & BMSR_100TXFDX)
		siatxrx |= SIATXRX_TXF;
	if (sc->sc_sia_cap & BMSR_100TXHDX)
		siatxrx |= SIATXRX_THX;
	if (sc->sc_sia_cap & BMSR_10TFDX)
		sc->sc_opmode |= OPMODE_FD;
	if (sc->sc_sia_cap & BMSR_10THDX)
		siatxrx |= SIATXRX_TH;

	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	TULIP_WRITE(sc, CSR_SIACONN, 0);
	delay(1000);
	TULIP_WRITE(sc, CSR_SIATXRX, siatxrx);
	TULIP_WRITE(sc, CSR_SIACONN, SIACONN_SRL);

	siastat = TULIP_READ(sc, CSR_SIASTAT);
	siastat &= ~(SIASTAT_ANS|SIASTAT_LPC|SIASTAT_TRA|SIASTAT_ARA|
		     SIASTAT_LS100|SIASTAT_LS10|SIASTAT_MRA);
	siastat |= SIASTAT_ANS_TXDIS;
	TULIP_WRITE(sc, CSR_SIASTAT, siastat);
}

static void
tlp_2114x_nway_status(struct tulip_softc *sc)
{
	struct mii_data *mii = &sc->sc_mii;
	uint32_t siatxrx, siastat, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return;

	siastat = TULIP_READ(sc, CSR_SIASTAT);
	siatxrx = TULIP_READ(sc, CSR_SIATXRX);

	if (siatxrx & SIATXRX_ANE) {
		if ((siastat & SIASTAT_ANS) != SIASTAT_ANS_FLPGOOD) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if (~siastat & (SIASTAT_LS10 | SIASTAT_LS100))
			mii->mii_media_status |= IFM_ACTIVE;

		if (siastat & SIASTAT_LPN) {
			anlpar = SIASTAT_GETLPC(siastat);
			if (anlpar & ANLPAR_T4 &&
			    sc->sc_sia_cap & BMSR_100T4)
				mii->mii_media_active |= IFM_100_T4;
			else if (anlpar & ANLPAR_TX_FD &&
				 sc->sc_sia_cap & BMSR_100TXFDX)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_TX &&
				 sc->sc_sia_cap & BMSR_100TXHDX)
				mii->mii_media_active |= IFM_100_TX;
			else if (anlpar & ANLPAR_10_FD &&
				 sc->sc_sia_cap & BMSR_10TFDX)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10 &&
				 sc->sc_sia_cap & BMSR_10THDX)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE;
		} else {
			/*
			 * If the other side doesn't support NWAY, then the
			 * best we can do is determine if we have a 10Mbps or
			 * 100Mbps link. There's no way to know if the link
			 * is full or half duplex, so we default to half duplex
			 * and hope that the user is clever enough to manually
			 * change the media settings if we're wrong.
			 */
			if ((siastat & SIASTAT_LS100) == 0)
				mii->mii_media_active |= IFM_100_TX;
			else if ((siastat & SIASTAT_LS10) == 0)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE;
		}
	} else {
		if (~siastat & (SIASTAT_LS10 | SIASTAT_LS100))
			mii->mii_media_status |= IFM_ACTIVE;

		if (sc->sc_opmode & OPMODE_TTM)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_100_TX;
		if (sc->sc_opmode & OPMODE_FD)
			mii->mii_media_active |= IFM_FDX;
	}
}

static void
tlp_2114x_isv_tmsw_get(struct tulip_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	struct tulip_21x4x_media *tm = ife->ifm_aux;

	(*tm->tm_get)(sc, ifmr);
}

static int
tlp_2114x_isv_tmsw_set(struct tulip_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	struct tulip_21x4x_media *tm = ife->ifm_aux;

	/*
	 * Check to see if we need to reset the chip, and do it.  The
	 * reset path will get the OPMODE register right the next
	 * time through.
	 */
	if (TULIP_MEDIA_NEEDSRESET(sc, tm->tm_opmode))
		return (tlp_init(&sc->sc_ethercom.ec_if));

	return ((*tm->tm_set)(sc));
}

/*
 * MII-on-SIO media switch.  Handles only MII attached to the SIO.
 */
static void	tlp_sio_mii_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_sio_mii_mediasw = {
	tlp_sio_mii_tmsw_init, tlp_mii_getmedia, tlp_mii_setmedia
};

static void
tlp_sio_mii_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/*
	 * We don't attach any media info structures to the ifmedia
	 * entries, so if we're using a pre-init function that needs
	 * that info, override it to one that doesn't.
	 */
	if (sc->sc_preinit == tlp_2114x_preinit)
		sc->sc_preinit = tlp_2114x_mii_preinit;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}
}

/*
 * Lite-On PNIC media switch.  Must handle MII or internal NWAY.
 */
static void	tlp_pnic_tmsw_init(struct tulip_softc *);
static void	tlp_pnic_tmsw_get(struct tulip_softc *, struct ifmediareq *);
static int	tlp_pnic_tmsw_set(struct tulip_softc *);

const struct tulip_mediasw tlp_pnic_mediasw = {
	tlp_pnic_tmsw_init, tlp_pnic_tmsw_get, tlp_pnic_tmsw_set
};

static void	tlp_pnic_nway_statchg(struct ifnet *);
static void	tlp_pnic_nway_tick(void *);
static int	tlp_pnic_nway_service(struct tulip_softc *, int);
static void	tlp_pnic_nway_reset(struct tulip_softc *);
static int	tlp_pnic_nway_auto(struct tulip_softc *, int);
static void	tlp_pnic_nway_auto_timeout(void *);
static void	tlp_pnic_nway_status(struct tulip_softc *);
static void	tlp_pnic_nway_acomp(struct tulip_softc *);

static void
tlp_pnic_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const char *sep = "";

#define	ADD(m, c)	ifmedia_add(&sc->sc_mii.mii_media, (m), (c), NULL)
#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_pnic_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_pnic_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		/* XXX What about AUI/BNC support? */
		aprint_normal_dev(sc->sc_dev, "");

		tlp_pnic_nway_reset(sc);

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0),
		    PNIC_NWAY_TW|PNIC_NWAY_CAP10T);
		PRINT("10baseT");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, 0),
		    PNIC_NWAY_TW|PNIC_NWAY_FD|PNIC_NWAY_CAP10TFDX);
		PRINT("10baseT-FDX");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, 0),
		    PNIC_NWAY_TW|PNIC_NWAY_100|PNIC_NWAY_CAP100TX);
		PRINT("100baseTX");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, 0),
		    PNIC_NWAY_TW|PNIC_NWAY_100|PNIC_NWAY_FD|
		    PNIC_NWAY_CAP100TXFDX);
		PRINT("100baseTX-FDX");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0),
		    PNIC_NWAY_TW|PNIC_NWAY_RN|PNIC_NWAY_NW|
		    PNIC_NWAY_CAP10T|PNIC_NWAY_CAP10TFDX|
		    PNIC_NWAY_CAP100TXFDX|PNIC_NWAY_CAP100TX);
		PRINT("auto");

		aprint_normal("\n");

		sc->sc_statchg = tlp_pnic_nway_statchg;
		sc->sc_tick = tlp_pnic_nway_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}

#undef ADD
#undef PRINT
}

static void
tlp_pnic_tmsw_get(struct tulip_softc *sc, struct ifmediareq *ifmr)
{
	struct mii_data *mii = &sc->sc_mii;

	if (sc->sc_flags & TULIPF_HAS_MII)
		tlp_mii_getmedia(sc, ifmr);
	else {
		mii->mii_media_status = 0;
		mii->mii_media_active = IFM_NONE;
		tlp_pnic_nway_service(sc, MII_POLLSTAT);
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
	}
}

static int
tlp_pnic_tmsw_set(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;

	if (sc->sc_flags & TULIPF_HAS_MII) {
		/*
		 * Make sure the built-in Tx jabber timer is disabled.
		 */
		TULIP_WRITE(sc, CSR_PNIC_ENDEC, PNIC_ENDEC_JDIS);

		return (tlp_mii_setmedia(sc));
	}

	if (ifp->if_flags & IFF_UP) {
		mii->mii_media_status = 0;
		mii->mii_media_active = IFM_NONE;
		return (tlp_pnic_nway_service(sc, MII_MEDIACHG));
	}

	return (0);
}

static void
tlp_pnic_nway_statchg(struct ifnet *ifp)
{
	struct tulip_softc *sc = ifp->if_softc;

	/* Idle the transmit and receive processes. */
	tlp_idle(sc, OPMODE_ST|OPMODE_SR);

	sc->sc_opmode &= ~(OPMODE_TTM|OPMODE_FD|OPMODE_PS|OPMODE_PCS|
	    OPMODE_SCR|OPMODE_HBD);

	if (IFM_SUBTYPE(sc->sc_mii.mii_media_active) == IFM_10_T) {
		sc->sc_opmode |= OPMODE_TTM;
		TULIP_WRITE(sc, CSR_GPP,
		    GPP_PNIC_OUT(GPP_PNIC_PIN_SPEED_RLY, 0) |
		    GPP_PNIC_OUT(GPP_PNIC_PIN_100M_LPKB, 1));
	} else {
		sc->sc_opmode |= OPMODE_PS|OPMODE_PCS|OPMODE_SCR|OPMODE_HBD;
		TULIP_WRITE(sc, CSR_GPP,
		    GPP_PNIC_OUT(GPP_PNIC_PIN_SPEED_RLY, 1) |
		    GPP_PNIC_OUT(GPP_PNIC_PIN_100M_LPKB, 1));
	}

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_opmode |= OPMODE_FD|OPMODE_HBD;

	/*
	 * Write new OPMODE bits.  This also restarts the transmit
	 * and receive processes.
	 */
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);
}

static void
tlp_pnic_nway_tick(void *arg)
{
	struct tulip_softc *sc = arg;
	int s;

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();
	tlp_pnic_nway_service(sc, MII_TICK);
	splx(s);

	callout_reset(&sc->sc_tick_callout, hz, tlp_pnic_nway_tick, sc);
}

/*
 * Support for the Lite-On PNIC internal NWay block.  This is constructed
 * somewhat like a PHY driver for simplicity.
 */

static int
tlp_pnic_nway_service(struct tulip_softc *sc, int cmd)
{
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return (0);

	switch (cmd) {
	case MII_POLLSTAT:
		/* Nothing special to do here. */
		break;

	case MII_MEDIACHG:
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			(void) tlp_pnic_nway_auto(sc, 1);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		default:
			/*
			 * NWAY register data is stored in the ifmedia entry.
			 */
			TULIP_WRITE(sc, CSR_PNIC_NWAY, ife->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.
		 */
		if (sc->sc_flags & TULIPF_LINK_UP)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->sc_nway_ticks != 5)
			return (0);

		sc->sc_nway_ticks = 0;
		tlp_pnic_nway_reset(sc);
		if (tlp_pnic_nway_auto(sc, 0) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	tlp_pnic_nway_status(sc);

	/* Callback if something changed. */
	if ((sc->sc_nway_active == NULL ||
	     sc->sc_nway_active->ifm_media != mii->mii_media_active) ||
	    cmd == MII_MEDIACHG) {
		(*sc->sc_statchg)(mii->mii_ifp);
		tlp_nway_activate(sc, mii->mii_media_active);
	}
	return (0);
}

static void
tlp_pnic_nway_reset(struct tulip_softc *sc)
{

	TULIP_WRITE(sc, CSR_PNIC_NWAY, PNIC_NWAY_RS);
	delay(100);
	TULIP_WRITE(sc, CSR_PNIC_NWAY, 0);
}

static int
tlp_pnic_nway_auto(struct tulip_softc *sc, int waitfor)
{
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint32_t reg;
	int i;

	if ((sc->sc_flags & TULIPF_DOINGAUTO) == 0)
		TULIP_WRITE(sc, CSR_PNIC_NWAY, ife->ifm_data);

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			reg = TULIP_READ(sc, CSR_PNIC_NWAY);
			if (reg & PNIC_NWAY_LPAR_MASK) {
				tlp_pnic_nway_acomp(sc);
				return (0);
			}
			delay(1000);
		}
#if 0
		if ((reg & PNIC_NWAY_LPAR_MASK) == 0)
			aprint_error_dev(sc->sc_dev, "autonegotiation failed to complete\n");
#endif

		/*
		 * Don't need to worry about clearing DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((sc->sc_flags & TULIPF_DOINGAUTO) == 0) {
		sc->sc_flags |= TULIPF_DOINGAUTO;
		callout_reset(&sc->sc_nway_callout, hz >> 1,
		    tlp_pnic_nway_auto_timeout, sc);
	}
	return (EJUSTRETURN);
}

static void
tlp_pnic_nway_auto_timeout(void *arg)
{
	struct tulip_softc *sc = arg;
	/* uint32_t reg; */
	int s;

	s = splnet();
	sc->sc_flags &= ~TULIPF_DOINGAUTO;
	/* reg = */
	TULIP_READ(sc, CSR_PNIC_NWAY);
#if 0
	if ((reg & PNIC_NWAY_LPAR_MASK) == 0)
		aprint_error_dev(sc->sc_dev, "autonegotiation failed to complete\n");
#endif

	tlp_pnic_nway_acomp(sc);

	/* Update the media status. */
	(void) tlp_pnic_nway_service(sc, MII_POLLSTAT);
	splx(s);
}

static void
tlp_pnic_nway_status(struct tulip_softc *sc)
{
	struct mii_data *mii = &sc->sc_mii;
	uint32_t reg;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	reg = TULIP_READ(sc, CSR_PNIC_NWAY);

	if (sc->sc_flags & TULIPF_LINK_UP)
		mii->mii_media_status |= IFM_ACTIVE;

	if (reg & PNIC_NWAY_NW) {
		if ((reg & PNIC_NWAY_LPAR_MASK) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

#if 0
		if (reg & PNIC_NWAY_LPAR100T4)
			mii->mii_media_active |= IFM_100_T4;
		else
#endif
		if (reg & PNIC_NWAY_LPAR100TXFDX)
			mii->mii_media_active |= IFM_100_TX|IFM_FDX;
		else if (reg & PNIC_NWAY_LPAR100TX)
			mii->mii_media_active |= IFM_100_TX;
		else if (reg & PNIC_NWAY_LPAR10TFDX)
			mii->mii_media_active |= IFM_10_T|IFM_FDX;
		else if (reg & PNIC_NWAY_LPAR10T)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_NONE;
	} else {
		if (reg & PNIC_NWAY_100)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (reg & PNIC_NWAY_FD)
			mii->mii_media_active |= IFM_FDX;
	}
}

static void
tlp_pnic_nway_acomp(struct tulip_softc *sc)
{
	uint32_t reg;

	reg = TULIP_READ(sc, CSR_PNIC_NWAY);
	reg &= ~(PNIC_NWAY_FD|PNIC_NWAY_100|PNIC_NWAY_RN);

	if (reg & (PNIC_NWAY_LPAR100TXFDX|PNIC_NWAY_LPAR100TX))
		reg |= PNIC_NWAY_100;
	if (reg & (PNIC_NWAY_LPAR10TFDX|PNIC_NWAY_LPAR100TXFDX))
		reg |= PNIC_NWAY_FD;

	TULIP_WRITE(sc, CSR_PNIC_NWAY, reg);
}

/*
 * Macronix PMAC and Lite-On PNIC-II media switch:
 *
 *	MX98713 and MX98713A		21140-like MII or GPIO media.
 *
 *	MX98713A			21143-like MII or SIA/SYM media.
 *
 *	MX98715, MX98715A, MX98725,	21143-like SIA/SYM media.
 *	82C115, MX98715AEC-C, -E
 *
 * So, what we do here is fake MII-on-SIO or ISV media info, and
 * use the ISV media switch get/set functions to handle the rest.
 */

static void	tlp_pmac_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_pmac_mediasw = {
	tlp_pmac_tmsw_init, tlp_2114x_isv_tmsw_get, tlp_2114x_isv_tmsw_set
};

const struct tulip_mediasw tlp_pmac_mii_mediasw = {
	tlp_pmac_tmsw_init, tlp_mii_getmedia, tlp_mii_setmedia
};

static void
tlp_pmac_tmsw_init(struct tulip_softc *sc)
{
	static const uint8_t media[] = {
		TULIP_ROM_MB_MEDIA_TP,
		TULIP_ROM_MB_MEDIA_TP_FDX,
		TULIP_ROM_MB_MEDIA_100TX,
		TULIP_ROM_MB_MEDIA_100TX_FDX,
	};
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct tulip_21x4x_media *tm;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	if (sc->sc_chip == TULIP_CHIP_MX98713 ||
	    sc->sc_chip == TULIP_CHIP_MX98713A) {
		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
		    MII_PHY_ANY, MII_OFFSET_ANY, 0);
		if (LIST_FIRST(&sc->sc_mii.mii_phys) != NULL) {
			sc->sc_flags |= TULIPF_HAS_MII;
			sc->sc_tick = tlp_mii_tick;
			sc->sc_preinit = tlp_2114x_mii_preinit;
			sc->sc_mediasw = &tlp_pmac_mii_mediasw;
			ifmedia_set(&sc->sc_mii.mii_media,
			    IFM_ETHER|IFM_AUTO);
			return;
		}
	}

	switch (sc->sc_chip) {
	case TULIP_CHIP_MX98713:
		tlp_add_srom_media(sc, TULIP_ROM_MB_21140_GPR,
		    tlp_21140_gpio_get, tlp_21140_gpio_set, media, 4);

		/*
		 * XXX Should implement auto-sense for this someday,
		 * XXX when we do the same for the 21140.
		 */
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);
		break;

	default:
		tlp_add_srom_media(sc, TULIP_ROM_MB_21142_SIA,
		    tlp_sia_get, tlp_sia_set, media, 2);
		tlp_add_srom_media(sc, TULIP_ROM_MB_21143_SYM,
		    tlp_sia_get, tlp_sia_set, media + 2, 2);

		tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);
		tm->tm_name = "auto";
		tm->tm_get = tlp_2114x_nway_get;
		tm->tm_set = tlp_2114x_nway_set;
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0), 0, tm);

		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
		sc->sc_statchg = tlp_2114x_nway_statchg;
		sc->sc_tick = tlp_2114x_nway_tick;
		break;
	}

	tlp_print_media(sc);
	tlp_sia_fixup(sc);

	/* Set the LED modes. */
	tlp_pmac_reset(sc);

	sc->sc_reset = tlp_pmac_reset;
}

/*
 * ADMtek AL981 media switch.  Only has internal PHY.
 */
static void	tlp_al981_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_al981_mediasw = {
	tlp_al981_tmsw_init, tlp_mii_getmedia, tlp_mii_setmedia
};

static void
tlp_al981_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_al981_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_al981_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}
}

/*
 * ADMtek AN983/985 media switch.  Only has internal PHY, but
 * on an SIO-like interface.  Unfortunately, we can't use the
 * standard SIO media switch, because the AN985 "ghosts" the
 * singly PHY at every address.
 */
static void	tlp_an985_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_an985_mediasw = {
	tlp_an985_tmsw_init, tlp_mii_getmedia, tlp_mii_setmedia
};

static void
tlp_an985_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, 1,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}
}

/*
 * Davicom DM9102 media switch.  Internal PHY and possibly HomePNA.
 */
static void	tlp_dm9102_tmsw_init(struct tulip_softc *);
static void	tlp_dm9102_tmsw_getmedia(struct tulip_softc *,
		    struct ifmediareq *);
static int	tlp_dm9102_tmsw_setmedia(struct tulip_softc *);

const struct tulip_mediasw tlp_dm9102_mediasw = {
	tlp_dm9102_tmsw_init, tlp_dm9102_tmsw_getmedia,
	    tlp_dm9102_tmsw_setmedia
};

static void
tlp_dm9102_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t opmode;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	/* PHY block already reset via tlp_reset(). */

	/*
	 * Configure OPMODE properly for the internal MII interface.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_DM9102:
		opmode = OPMODE_MBO|OPMODE_HBD|OPMODE_PS;
		break;

	case TULIP_CHIP_DM9102A:
		opmode = OPMODE_MBO|OPMODE_HBD;
		break;

	default:
		opmode = 0;
		break;
	}

	TULIP_WRITE(sc, CSR_OPMODE, opmode);

	/* Now, probe the internal MII for the internal PHY. */
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	/*
	 * XXX Figure out what to do about the HomePNA portion
	 * XXX of the DM9102A.
	 */

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}
}

static void
tlp_dm9102_tmsw_getmedia(struct tulip_softc *sc, struct ifmediareq *ifmr)
{

	/* XXX HomePNA on DM9102A. */
	tlp_mii_getmedia(sc, ifmr);
}

static int
tlp_dm9102_tmsw_setmedia(struct tulip_softc *sc)
{

	/* XXX HomePNA on DM9102A. */
	return (tlp_mii_setmedia(sc));
}

/*
 * ASIX AX88140A/AX88141 media switch. Internal PHY or MII.
 */

static void	tlp_asix_tmsw_init(struct tulip_softc *);
static void	tlp_asix_tmsw_getmedia(struct tulip_softc *,
		    struct ifmediareq *);
static int	tlp_asix_tmsw_setmedia(struct tulip_softc *);

const struct tulip_mediasw tlp_asix_mediasw = {
	tlp_asix_tmsw_init, tlp_asix_tmsw_getmedia,
	tlp_asix_tmsw_setmedia
};

static void
tlp_asix_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t opmode;

	sc->sc_mii.mii_ifp = ifp;
        sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
        sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
            tlp_mediastatus);

	/*
	 * Configure OPMODE properly for the internal MII interface.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		opmode = OPMODE_HBD|OPMODE_PS;
		break;
        default:
                opmode = 0;
                break;
        }

	TULIP_WRITE(sc, CSR_OPMODE, opmode);

	/* Now, probe the internal MII for the internal PHY. */
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	/* XXX Figure how to handle the PHY. */

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}


}

static void
tlp_asix_tmsw_getmedia(struct tulip_softc *sc, struct ifmediareq *ifmr)
{

	/* XXX PHY handling. */
	tlp_mii_getmedia(sc, ifmr);
}

static int
tlp_asix_tmsw_setmedia(struct tulip_softc *sc)
{

	/* XXX PHY handling. */
	return (tlp_mii_setmedia(sc));
}

/*
 * RS7112 media switch.  Handles only MII attached to the SIO.
 * We only have a PHY at 1.
 */
void   tlp_rs7112_tmsw_init(struct tulip_softc *);

const struct tulip_mediasw tlp_rs7112_mediasw = {
	tlp_rs7112_tmsw_init, tlp_mii_getmedia, tlp_mii_setmedia
};

void
tlp_rs7112_tmsw_init(struct tulip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/*
	 * We don't attach any media info structures to the ifmedia
	 * entries, so if we're using a pre-init function that needs
	 * that info, override it to one that doesn't.
	 */
	if (sc->sc_preinit == tlp_2114x_preinit)
		sc->sc_preinit = tlp_2114x_mii_preinit;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = tlp_bitbang_mii_readreg;
	sc->sc_mii.mii_writereg = tlp_bitbang_mii_writereg;
	sc->sc_mii.mii_statchg = sc->sc_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);

	/*
	 * The RS7112 reports a PHY at 0 (possibly HomePNA?)
	 * and 1 (ethernet). We attach ethernet only.
	 */
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, 1,
	    MII_OFFSET_ANY, 0);

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		sc->sc_flags |= TULIPF_HAS_MII;
		sc->sc_tick = tlp_mii_tick;
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}
}

const char *
tlp_chip_name(tulip_chip_t t) {
	if ((int)t < 0 || (int)t >= __arraycount(tlp_chip_names)) {
		static char buf[256];
		(void)snprintf(buf, sizeof(buf), "[unknown 0x%x]", t);
		return buf;
	}
	return tlp_chip_names[t];
}
