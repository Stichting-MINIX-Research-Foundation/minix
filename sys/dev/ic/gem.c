/*	$NetBSD: gem.c,v 1.103 2015/08/30 04:17:48 dholland Exp $ */

/*
 *
 * Copyright (C) 2001 Eduardo Horvath.
 * Copyright (c) 2001-2003 Thomas Moestl
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Driver for Apple GMAC, Sun ERI and Sun GEM Ethernet controllers
 * See `GEM Gigabit Ethernet ASIC Specification'
 *   http://www.sun.com/processors/manuals/ge.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gem.c,v 1.103 2015/08/30 04:17:48 dholland Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
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

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#endif

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/gemreg.h>
#include <dev/ic/gemvar.h>

#define TRIES	10000

static void	gem_inten(struct gem_softc *);
static void	gem_start(struct ifnet *);
static void	gem_stop(struct ifnet *, int);
int		gem_ioctl(struct ifnet *, u_long, void *);
void		gem_tick(void *);
void		gem_watchdog(struct ifnet *);
void		gem_rx_watchdog(void *);
void		gem_pcs_start(struct gem_softc *sc);
void		gem_pcs_stop(struct gem_softc *sc, int);
int		gem_init(struct ifnet *);
void		gem_init_regs(struct gem_softc *sc);
static int	gem_ringsize(int sz);
static int	gem_meminit(struct gem_softc *);
void		gem_mifinit(struct gem_softc *);
static int	gem_bitwait(struct gem_softc *sc, bus_space_handle_t, int,
		    u_int32_t, u_int32_t);
void		gem_reset(struct gem_softc *);
int		gem_reset_rx(struct gem_softc *sc);
static void	gem_reset_rxdma(struct gem_softc *sc);
static void	gem_rx_common(struct gem_softc *sc);
int		gem_reset_tx(struct gem_softc *sc);
int		gem_disable_rx(struct gem_softc *sc);
int		gem_disable_tx(struct gem_softc *sc);
static void	gem_rxdrain(struct gem_softc *sc);
int		gem_add_rxbuf(struct gem_softc *sc, int idx);
void		gem_setladrf(struct gem_softc *);

/* MII methods & callbacks */
static int	gem_mii_readreg(device_t, int, int);
static void	gem_mii_writereg(device_t, int, int, int);
static void	gem_mii_statchg(struct ifnet *);

static int	gem_ifflags_cb(struct ethercom *);

void		gem_statuschange(struct gem_softc *);

int		gem_ser_mediachange(struct ifnet *);
void		gem_ser_mediastatus(struct ifnet *, struct ifmediareq *);

static void	gem_partial_detach(struct gem_softc *, enum gem_attach_stage);

struct mbuf	*gem_get(struct gem_softc *, int, int);
int		gem_put(struct gem_softc *, int, struct mbuf *);
void		gem_read(struct gem_softc *, int, int);
int		gem_pint(struct gem_softc *);
int		gem_eint(struct gem_softc *, u_int);
int		gem_rint(struct gem_softc *);
int		gem_tint(struct gem_softc *);
void		gem_power(int, void *);

#ifdef GEM_DEBUG
static void gem_txsoft_print(const struct gem_softc *, int, int);
#define	DPRINTF(sc, x)	if ((sc)->sc_ethercom.ec_if.if_flags & IFF_DEBUG) \
				printf x
#else
#define	DPRINTF(sc, x)	/* nothing */
#endif

#define ETHER_MIN_TX (ETHERMIN + sizeof(struct ether_header))

int
gem_detach(struct gem_softc *sc, int flags)
{
	int i;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;

	/*
	 * Free any resources we've allocated during the attach.
	 * Do this in reverse order and fall through.
	 */
	switch (sc->sc_att_stage) {
	case GEM_ATT_BACKEND_2:
	case GEM_ATT_BACKEND_1:
	case GEM_ATT_FINISHED:
		bus_space_write_4(t, h, GEM_INTMASK, ~(uint32_t)0);
		gem_stop(&sc->sc_ethercom.ec_if, 1);

#ifdef GEM_COUNTERS
		for (i = __arraycount(sc->sc_ev_rxhist); --i >= 0; )
			evcnt_detach(&sc->sc_ev_rxhist[i]);
		evcnt_detach(&sc->sc_ev_rxnobuf);
		evcnt_detach(&sc->sc_ev_rxfull);
		evcnt_detach(&sc->sc_ev_rxint);
		evcnt_detach(&sc->sc_ev_txint);
#endif
		evcnt_detach(&sc->sc_ev_intr);

		rnd_detach_source(&sc->rnd_source);
		ether_ifdetach(ifp);
		if_detach(ifp);
		ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

		callout_destroy(&sc->sc_tick_ch);
		callout_destroy(&sc->sc_rx_watchdog);

		/*FALLTHROUGH*/
	case GEM_ATT_MII:
		sc->sc_att_stage = GEM_ATT_MII;
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
		/*FALLTHROUGH*/
	case GEM_ATT_7:
		for (i = 0; i < GEM_NRXDESC; i++) {
			if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
				bus_dmamap_destroy(sc->sc_dmatag,
				    sc->sc_rxsoft[i].rxs_dmamap);
		}
		/*FALLTHROUGH*/
	case GEM_ATT_6:
		for (i = 0; i < GEM_TXQUEUELEN; i++) {
			if (sc->sc_txsoft[i].txs_dmamap != NULL)
				bus_dmamap_destroy(sc->sc_dmatag,
				    sc->sc_txsoft[i].txs_dmamap);
		}
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case GEM_ATT_5:
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_nulldmamap);
		/*FALLTHROUGH*/
	case GEM_ATT_4:
		bus_dmamap_destroy(sc->sc_dmatag, sc->sc_nulldmamap);
		/*FALLTHROUGH*/
	case GEM_ATT_3:
		bus_dmamap_destroy(sc->sc_dmatag, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case GEM_ATT_2:
		bus_dmamem_unmap(sc->sc_dmatag, sc->sc_control_data,
		    sizeof(struct gem_control_data));
		/*FALLTHROUGH*/
	case GEM_ATT_1:
		bus_dmamem_free(sc->sc_dmatag, &sc->sc_cdseg, sc->sc_cdnseg);
		/*FALLTHROUGH*/
	case GEM_ATT_0:
		sc->sc_att_stage = GEM_ATT_0;
		/*FALLTHROUGH*/
	case GEM_ATT_BACKEND_0:
		break;
	}
	return 0;
}

static void
gem_partial_detach(struct gem_softc *sc, enum gem_attach_stage stage)
{
	cfattach_t ca = device_cfattach(sc->sc_dev);

	sc->sc_att_stage = stage;
	(*ca->ca_detach)(sc->sc_dev, 0);
}

/*
 * gem_attach:
 *
 *	Attach a Gem interface to the system.
 */
void
gem_attach(struct gem_softc *sc, const uint8_t *enaddr)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	struct ifmedia_entry *ifm;
	int i, error, phyaddr;
	u_int32_t v;
	char *nullbuf;

	/* Make sure the chip is stopped. */
	ifp->if_softc = sc;
	gem_reset(sc);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it. gem_control_data is 9216 bytes, we have space for
	 * the padding buffer in the bus_dmamem_alloc()'d memory.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmatag,
	    sizeof(struct gem_control_data) + ETHER_MIN_TX, PAGE_SIZE,
	    0, &sc->sc_cdseg, 1, &sc->sc_cdnseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		   "unable to allocate control data, error = %d\n",
		    error);
		gem_partial_detach(sc, GEM_ATT_0);
		return;
	}

	/* XXX should map this in with correct endianness */
	if ((error = bus_dmamem_map(sc->sc_dmatag, &sc->sc_cdseg, sc->sc_cdnseg,
	    sizeof(struct gem_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control data, error = %d\n", error);
		gem_partial_detach(sc, GEM_ATT_1);
		return;
	}

	nullbuf =
	    (char *)sc->sc_control_data + sizeof(struct gem_control_data);

	if ((error = bus_dmamap_create(sc->sc_dmatag,
	    sizeof(struct gem_control_data), 1,
	    sizeof(struct gem_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control data DMA map, error = %d\n",
		    error);
		gem_partial_detach(sc, GEM_ATT_2);
		return;
	}

	if ((error = bus_dmamap_load(sc->sc_dmatag, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct gem_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n",
		    error);
		gem_partial_detach(sc, GEM_ATT_3);
		return;
	}

	memset(nullbuf, 0, ETHER_MIN_TX);
	if ((error = bus_dmamap_create(sc->sc_dmatag,
	    ETHER_MIN_TX, 1, ETHER_MIN_TX, 0, 0, &sc->sc_nulldmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create padding DMA map, error = %d\n", error);
		gem_partial_detach(sc, GEM_ATT_4);
		return;
	}

	if ((error = bus_dmamap_load(sc->sc_dmatag, sc->sc_nulldmamap,
	    nullbuf, ETHER_MIN_TX, NULL, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load padding DMA map, error = %d\n", error);
		gem_partial_detach(sc, GEM_ATT_5);
		return;
	}

	bus_dmamap_sync(sc->sc_dmatag, sc->sc_nulldmamap, 0, ETHER_MIN_TX,
	    BUS_DMASYNC_PREWRITE);

	/*
	 * Initialize the transmit job descriptors.
	 */
	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		struct gem_txsoft *txs;

		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		if ((error = bus_dmamap_create(sc->sc_dmatag,
		    ETHER_MAX_LEN_JUMBO, GEM_NTXSEGS,
		    ETHER_MAX_LEN_JUMBO, 0, 0,
		    &txs->txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create tx DMA map %d, error = %d\n",
			    i, error);
			gem_partial_detach(sc, GEM_ATT_6);
			return;
		}
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create rx DMA map %d, error = %d\n",
			    i, error);
			gem_partial_detach(sc, GEM_ATT_7);
			return;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/* Initialize ifmedia structures and MII info */
	mii->mii_ifp = ifp;
	mii->mii_readreg = gem_mii_readreg;
	mii->mii_writereg = gem_mii_writereg;
	mii->mii_statchg = gem_mii_statchg;

	sc->sc_ethercom.ec_mii = mii;

	/*
	 * Initialization based  on `GEM Gigabit Ethernet ASIC Specification'
	 * Section 3.2.1 `Initialization Sequence'.
	 * However, we can't assume SERDES or Serialink if neither
	 * GEM_MIF_CONFIG_MDI0 nor GEM_MIF_CONFIG_MDI1 are set
	 * being set, as both are set on Sun X1141A (with SERDES).  So,
	 * we rely on our bus attachment setting GEM_SERDES or GEM_SERIAL.
	 * Also, for variants that report 2 PHY's, we prefer the external
	 * PHY over the internal PHY, so we look for that first.
	 */
	gem_mifinit(sc);

	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) == 0) {
		ifmedia_init(&mii->mii_media, IFM_IMASK, ether_mediachange,
		    ether_mediastatus);
		/* Look for external PHY */
		if (sc->sc_mif_config & GEM_MIF_CONFIG_MDI1) {
			sc->sc_mif_config |= GEM_MIF_CONFIG_PHY_SEL;
			bus_space_write_4(t, h, GEM_MIF_CONFIG,
			    sc->sc_mif_config);
			switch (sc->sc_variant) {
			case GEM_SUN_ERI:
				phyaddr = GEM_PHYAD_EXTERNAL;
				break;
			default:
				phyaddr = MII_PHY_ANY;
				break;
			}
			mii_attach(sc->sc_dev, mii, 0xffffffff, phyaddr,
			    MII_OFFSET_ANY, MIIF_FORCEANEG);
		}
#ifdef GEM_DEBUG
		  else
			aprint_debug_dev(sc->sc_dev, "using external PHY\n");
#endif
		/* Look for internal PHY if no external PHY was found */
		if (LIST_EMPTY(&mii->mii_phys) && 
		    sc->sc_mif_config & GEM_MIF_CONFIG_MDI0) {
			sc->sc_mif_config &= ~GEM_MIF_CONFIG_PHY_SEL;
			bus_space_write_4(t, h, GEM_MIF_CONFIG,
			    sc->sc_mif_config);
			switch (sc->sc_variant) {
			case GEM_SUN_ERI:
			case GEM_APPLE_K2_GMAC:
				phyaddr = GEM_PHYAD_INTERNAL;
				break;
			case GEM_APPLE_GMAC:
				phyaddr = GEM_PHYAD_EXTERNAL;
				break;
			default:
				phyaddr = MII_PHY_ANY;
				break;
			}
			mii_attach(sc->sc_dev, mii, 0xffffffff, phyaddr,
			    MII_OFFSET_ANY, MIIF_FORCEANEG);
#ifdef GEM_DEBUG
			if (!LIST_EMPTY(&mii->mii_phys))
				aprint_debug_dev(sc->sc_dev,
				    "using internal PHY\n");
#endif
		}
		if (LIST_EMPTY(&mii->mii_phys)) {
				/* No PHY attached */
				aprint_error_dev(sc->sc_dev,
				    "PHY probe failed\n");
				gem_partial_detach(sc, GEM_ATT_MII);
				return;
		} else {
			struct mii_softc *child;

			/*
			 * Walk along the list of attached MII devices and
			 * establish an `MII instance' to `PHY number'
			 * mapping.
			 */
			LIST_FOREACH(child, &mii->mii_phys, mii_list) {
				/*
				 * Note: we support just one PHY: the internal
				 * or external MII is already selected for us
				 * by the GEM_MIF_CONFIG  register.
				 */
				if (child->mii_phy > 1 || child->mii_inst > 0) {
					aprint_error_dev(sc->sc_dev,
					    "cannot accommodate MII device"
					    " %s at PHY %d, instance %d\n",
					       device_xname(child->mii_dev),
					       child->mii_phy, child->mii_inst);
					continue;
				}
				sc->sc_phys[child->mii_inst] = child->mii_phy;
			}

			if (sc->sc_variant != GEM_SUN_ERI)
				bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
				    GEM_MII_DATAPATH_MII);

			/*
			 * XXX - we can really do the following ONLY if the
			 * PHY indeed has the auto negotiation capability!!
			 */
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
		}
	} else {
		ifmedia_init(&mii->mii_media, IFM_IMASK, gem_ser_mediachange,
		    gem_ser_mediastatus);
		/* SERDES or Serialink */
		if (sc->sc_flags & GEM_SERDES) {
			bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
			    GEM_MII_DATAPATH_SERDES);
		} else {
			sc->sc_flags |= GEM_SERIAL;
			bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
			    GEM_MII_DATAPATH_SERIAL);
		}

		aprint_normal_dev(sc->sc_dev, "using external PCS %s: ",
		    sc->sc_flags & GEM_SERDES ? "SERDES" : "Serialink");

		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO, 0, NULL);
		/* Check for FDX and HDX capabilities */
		sc->sc_mii_anar = bus_space_read_4(t, h, GEM_MII_ANAR);
		if (sc->sc_mii_anar & GEM_MII_ANEG_FUL_DUPLX) {
			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_ETHER|IFM_1000_SX|IFM_MANUAL|IFM_FDX, 0, NULL);
			aprint_normal("1000baseSX-FDX, ");
		}
		if (sc->sc_mii_anar & GEM_MII_ANEG_HLF_DUPLX) {
			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_ETHER|IFM_1000_SX|IFM_MANUAL|IFM_HDX, 0, NULL);
			aprint_normal("1000baseSX-HDX, ");
		}
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
		sc->sc_mii_media = IFM_AUTO;
		aprint_normal("auto\n");

		gem_pcs_stop(sc, 1);
	}

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */

	/* Announce ourselves. */
	aprint_normal_dev(sc->sc_dev, "Ethernet address %s",
	    ether_sprintf(enaddr));

	/* Get RX FIFO size */
	sc->sc_rxfifosize = 64 *
	    bus_space_read_4(t, h, GEM_RX_FIFO_SIZE);
	aprint_normal(", %uKB RX fifo", sc->sc_rxfifosize / 1024);

	/* Get TX FIFO size */
	v = bus_space_read_4(t, h, GEM_TX_FIFO_SIZE);
	aprint_normal(", %uKB TX fifo\n", v / 16);

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	sc->sc_if_flags = ifp->if_flags;
#if 0
	/*
	 * The GEM hardware supports basic TCP checksum offloading only.
	 * Several (all?) revisions (Sun rev. 01 and Apple rev. 00 and 80)
	 * have bugs in the receive checksum, so don't enable it for now.
	 */
	if ((GEM_IS_SUN(sc) && sc->sc_chiprev != 1) ||
	    (GEM_IS_APPLE(sc) &&
	    (sc->sc_chiprev != 0 && sc->sc_chiprev != 0x80)))
		ifp->if_capabilities |= IFCAP_CSUM_TCPv4_Rx;
#endif
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4_Tx;
	ifp->if_start = gem_start;
	ifp->if_ioctl = gem_ioctl;
	ifp->if_watchdog = gem_watchdog;
	ifp->if_stop = gem_stop;
	ifp->if_init = gem_init;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * If we support GigE media, we support jumbo frames too.
	 * Unless we are Apple.
	 */
	TAILQ_FOREACH(ifm, &sc->sc_mii.mii_media.ifm_list, ifm_list) {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_T ||
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_SX ||
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_LX ||
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_CX) {
			if (!GEM_IS_APPLE(sc))
				sc->sc_ethercom.ec_capabilities
				    |= ETHERCAP_JUMBO_MTU;
			sc->sc_flags |= GEM_GIGABIT;
			break;
		}
	}

	/* claim 802.1q capability */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, gem_ifflags_cb);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	evcnt_attach_dynamic(&sc->sc_ev_intr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "interrupts");
#ifdef GEM_COUNTERS
	evcnt_attach_dynamic(&sc->sc_ev_txint, EVCNT_TYPE_INTR,
	    &sc->sc_ev_intr, device_xname(sc->sc_dev), "tx interrupts");
	evcnt_attach_dynamic(&sc->sc_ev_rxint, EVCNT_TYPE_INTR,
	    &sc->sc_ev_intr, device_xname(sc->sc_dev), "rx interrupts");
	evcnt_attach_dynamic(&sc->sc_ev_rxfull, EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx ring full");
	evcnt_attach_dynamic(&sc->sc_ev_rxnobuf, EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx malloc failure");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[0], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx 0desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[1], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx 1desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[2], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx 2desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[3], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx 3desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[4], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx >3desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[5], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx >7desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[6], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx >15desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[7], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx >31desc");
	evcnt_attach_dynamic(&sc->sc_ev_rxhist[8], EVCNT_TYPE_INTR,
	    &sc->sc_ev_rxint, device_xname(sc->sc_dev), "rx >63desc");
#endif

	callout_init(&sc->sc_tick_ch, 0);
	callout_init(&sc->sc_rx_watchdog, 0);
	callout_setfunc(&sc->sc_rx_watchdog, gem_rx_watchdog, sc);

	sc->sc_att_stage = GEM_ATT_FINISHED;

	return;
}

void
gem_tick(void *arg)
{
	struct gem_softc *sc = arg;
	int s;

	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) != 0) {
		/*
		 * We have to reset everything if we failed to get a
		 * PCS interrupt.  Restarting the callout is handled
		 * in gem_pcs_start().
		 */
		gem_init(&sc->sc_ethercom.ec_if);
	} else {
		s = splnet();
		mii_tick(&sc->sc_mii);
		splx(s);
		callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);
	}
}

static int
gem_bitwait(struct gem_softc *sc, bus_space_handle_t h, int r, u_int32_t clr, u_int32_t set)
{
	int i;
	u_int32_t reg;

	for (i = TRIES; i--; DELAY(100)) {
		reg = bus_space_read_4(sc->sc_bustag, h, r);
		if ((reg & clr) == 0 && (reg & set) == set)
			return (1);
	}
	return (0);
}

void
gem_reset(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h2;
	int s;

	s = splnet();
	DPRINTF(sc, ("%s: gem_reset\n", device_xname(sc->sc_dev)));
	gem_reset_rx(sc);
	gem_reset_tx(sc);

	/* Do a full reset */
	bus_space_write_4(t, h, GEM_RESET, GEM_RESET_RX|GEM_RESET_TX);
	if (!gem_bitwait(sc, h, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX, 0))
		aprint_error_dev(sc->sc_dev, "cannot reset device\n");
	splx(s);
}


/*
 * gem_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
gem_rxdrain(struct gem_softc *sc)
{
	struct gem_rxsoft *rxs;
	int i;

	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmatag, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * Reset the whole thing.
 */
static void
gem_stop(struct ifnet *ifp, int disable)
{
	struct gem_softc *sc = ifp->if_softc;
	struct gem_txsoft *txs;

	DPRINTF(sc, ("%s: gem_stop\n", device_xname(sc->sc_dev)));

	callout_halt(&sc->sc_tick_ch, NULL);
	callout_halt(&sc->sc_rx_watchdog, NULL);
	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) != 0)
		gem_pcs_stop(sc, disable);
	else
		mii_down(&sc->sc_mii);

	/* XXX - Should we reset these instead? */
	gem_disable_tx(sc);
	gem_disable_rx(sc);

	/*
	 * Release any queued transmit buffers.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmatag, txs->txs_dmamap, 0,
			    txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmatag, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_timer = 0;

	if (disable)
		gem_rxdrain(sc);
}


/*
 * Reset the receiver
 */
int
gem_reset_rx(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1, h2 = sc->sc_h2;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_rx(sc);
	bus_space_write_4(t, h, GEM_RX_CONFIG, 0);
	bus_space_barrier(t, h, GEM_RX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, h, GEM_RX_CONFIG, 1, 0))
		aprint_error_dev(sc->sc_dev, "cannot disable read dma\n");
	/* Wait 5ms extra. */
	delay(5000);

	/* Finally, reset the ERX */
	bus_space_write_4(t, h2, GEM_RESET, GEM_RESET_RX);
	bus_space_barrier(t, h, GEM_RESET, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, h2, GEM_RESET, GEM_RESET_RX, 0)) {
		aprint_error_dev(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}


/*
 * Reset the receiver DMA engine.
 *
 * Intended to be used in case of GEM_INTR_RX_TAG_ERR, GEM_MAC_RX_OVERFLOW
 * etc in order to reset the receiver DMA engine only and not do a full
 * reset which amongst others also downs the link and clears the FIFOs.
 */
static void
gem_reset_rxdma(struct gem_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	int i;

	if (gem_reset_rx(sc) != 0) {
		gem_init(ifp);
		return;
	}
	for (i = 0; i < GEM_NRXDESC; i++)
		if (sc->sc_rxsoft[i].rxs_mbuf != NULL)
			GEM_UPDATE_RXDESC(sc, i);
	sc->sc_rxptr = 0;
	GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
	GEM_CDSYNC(sc, BUS_DMASYNC_PREREAD);

	/* Reprogram Descriptor Ring Base Addresses */
	/* NOTE: we use only 32-bit DMA addresses here. */
	bus_space_write_4(t, h, GEM_RX_RING_PTR_HI, 0);
	bus_space_write_4(t, h, GEM_RX_RING_PTR_LO, GEM_CDRXADDR(sc, 0));

	/* Redo ERX Configuration */
	gem_rx_common(sc);

	/* Give the reciever a swift kick */
	bus_space_write_4(t, h, GEM_RX_KICK, GEM_NRXDESC - 4);
}

/*
 * Common RX configuration for gem_init() and gem_reset_rxdma().
 */
static void
gem_rx_common(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t v;

	/* Encode Receive Descriptor ring size: four possible values */
	v = gem_ringsize(GEM_NRXDESC /*XXX*/);

	/* Set receive h/w checksum offset */
#ifdef INET
	v |= (ETHER_HDR_LEN + sizeof(struct ip) +
	    ((sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) ?
	    ETHER_VLAN_ENCAP_LEN : 0)) << GEM_RX_CONFIG_CXM_START_SHFT;
#endif

	/* Enable RX DMA */
	bus_space_write_4(t, h, GEM_RX_CONFIG,
	    v | (GEM_THRSH_1024 << GEM_RX_CONFIG_FIFO_THRS_SHIFT) |
	    (2 << GEM_RX_CONFIG_FBOFF_SHFT) | GEM_RX_CONFIG_RXDMA_EN);

	/*
	 * The following value is for an OFF Threshold of about 3/4 full
	 * and an ON Threshold of 1/4 full.
	 */
	bus_space_write_4(t, h, GEM_RX_PAUSE_THRESH,
	    (3 * sc->sc_rxfifosize / 256) |
	    ((sc->sc_rxfifosize / 256) << 12));
	bus_space_write_4(t, h, GEM_RX_BLANKING,
	    (6 << GEM_RX_BLANKING_TIME_SHIFT) | 8);
}

/*
 * Reset the transmitter
 */
int
gem_reset_tx(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1, h2 = sc->sc_h2;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_tx(sc);
	bus_space_write_4(t, h, GEM_TX_CONFIG, 0);
	bus_space_barrier(t, h, GEM_TX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, h, GEM_TX_CONFIG, 1, 0))
		aprint_error_dev(sc->sc_dev, "cannot disable read dma\n");
	/* Wait 5ms extra. */
	delay(5000);

	/* Finally, reset the ETX */
	bus_space_write_4(t, h2, GEM_RESET, GEM_RESET_TX);
	bus_space_barrier(t, h, GEM_RESET, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, h2, GEM_RESET, GEM_RESET_TX, 0)) {
		aprint_error_dev(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}

/*
 * disable receiver.
 */
int
gem_disable_rx(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);
	cfg &= ~GEM_MAC_RX_ENABLE;
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, cfg);
	bus_space_barrier(t, h, GEM_MAC_RX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait for it to finish */
	return (gem_bitwait(sc, h, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0));
}

/*
 * disable transmitter.
 */
int
gem_disable_tx(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, GEM_MAC_TX_CONFIG);
	cfg &= ~GEM_MAC_TX_ENABLE;
	bus_space_write_4(t, h, GEM_MAC_TX_CONFIG, cfg);
	bus_space_barrier(t, h, GEM_MAC_TX_CONFIG, 4, BUS_SPACE_BARRIER_WRITE);
	/* Wait for it to finish */
	return (gem_bitwait(sc, h, GEM_MAC_TX_CONFIG, GEM_MAC_TX_ENABLE, 0));
}

/*
 * Initialize interface.
 */
int
gem_meminit(struct gem_softc *sc)
{
	struct gem_rxsoft *rxs;
	int i, error;

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < GEM_NTXDESC; i++) {
		sc->sc_txdescs[i].gd_flags = 0;
		sc->sc_txdescs[i].gd_addr = 0;
	}
	GEM_CDTXSYNC(sc, 0, GEM_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = GEM_NTXDESC-1;
	sc->sc_txnext = 0;
	sc->sc_txwin = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = gem_add_rxbuf(sc, i)) != 0) {
				aprint_error_dev(sc->sc_dev,
				    "unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				gem_rxdrain(sc);
				return (1);
			}
		} else
			GEM_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	sc->sc_meminited = 1;
	GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
	GEM_CDSYNC(sc, BUS_DMASYNC_PREREAD);

	return (0);
}

static int
gem_ringsize(int sz)
{
	switch (sz) {
	case 32:
		return GEM_RING_SZ_32;
	case 64:
		return GEM_RING_SZ_64;
	case 128:
		return GEM_RING_SZ_128;
	case 256:
		return GEM_RING_SZ_256;
	case 512:
		return GEM_RING_SZ_512;
	case 1024:
		return GEM_RING_SZ_1024;
	case 2048:
		return GEM_RING_SZ_2048;
	case 4096:
		return GEM_RING_SZ_4096;
	case 8192:
		return GEM_RING_SZ_8192;
	default:
		printf("gem: invalid Receive Descriptor ring size %d\n", sz);
		return GEM_RING_SZ_32;
	}
}


/*
 * Start PCS
 */
void
gem_pcs_start(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	uint32_t v;

#ifdef GEM_DEBUG
	aprint_debug_dev(sc->sc_dev, "gem_pcs_start()\n");
#endif

	/*
	 * Set up.  We must disable the MII before modifying the
	 * GEM_MII_ANAR register
	 */
	if (sc->sc_flags & GEM_SERDES) {
		bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
		    GEM_MII_DATAPATH_SERDES);
		bus_space_write_4(t, h, GEM_MII_SLINK_CONTROL,
		    GEM_MII_SLINK_LOOPBACK);
	} else {
		bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
		    GEM_MII_DATAPATH_SERIAL);
		bus_space_write_4(t, h, GEM_MII_SLINK_CONTROL, 0);
	}
	bus_space_write_4(t, h, GEM_MII_CONFIG, 0);
	v = bus_space_read_4(t, h, GEM_MII_ANAR);
	v |= (GEM_MII_ANEG_SYM_PAUSE | GEM_MII_ANEG_ASYM_PAUSE);
	if (sc->sc_mii_media == IFM_AUTO)
		v |= (GEM_MII_ANEG_FUL_DUPLX | GEM_MII_ANEG_HLF_DUPLX);
	else if (sc->sc_mii_media == IFM_FDX) {
		v |= GEM_MII_ANEG_FUL_DUPLX;
		v &= ~GEM_MII_ANEG_HLF_DUPLX;
	} else if (sc->sc_mii_media == IFM_HDX) {
		v &= ~GEM_MII_ANEG_FUL_DUPLX;
		v |= GEM_MII_ANEG_HLF_DUPLX;
	}

	/* Configure link. */
	bus_space_write_4(t, h, GEM_MII_ANAR, v);
	bus_space_write_4(t, h, GEM_MII_CONTROL,
	    GEM_MII_CONTROL_AUTONEG | GEM_MII_CONTROL_RAN);
	bus_space_write_4(t, h, GEM_MII_CONFIG, GEM_MII_CONFIG_ENABLE);
	gem_bitwait(sc, h, GEM_MII_STATUS, 0, GEM_MII_STATUS_ANEG_CPT);

	/* Start the 10 second timer */
	callout_reset(&sc->sc_tick_ch, hz * 10, gem_tick, sc);
}

/*
 * Stop PCS
 */
void
gem_pcs_stop(struct gem_softc *sc, int disable)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;

#ifdef GEM_DEBUG
	aprint_debug_dev(sc->sc_dev, "gem_pcs_stop()\n");
#endif

	/* Tell link partner that we're going away */
	bus_space_write_4(t, h, GEM_MII_ANAR, GEM_MII_ANEG_RF);

	/*
	 * Disable PCS MII.  The documentation suggests that setting
	 * GEM_MII_CONFIG_ENABLE to zero and then restarting auto-
	 * negotiation will shut down the link.  However, it appears
	 * that we also need to unset the datapath mode.
	 */
	bus_space_write_4(t, h, GEM_MII_CONFIG, 0);
	bus_space_write_4(t, h, GEM_MII_CONTROL,
	    GEM_MII_CONTROL_AUTONEG | GEM_MII_CONTROL_RAN);
	bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE, GEM_MII_DATAPATH_MII);
	bus_space_write_4(t, h, GEM_MII_CONFIG, 0);

	if (disable) {
		if (sc->sc_flags & GEM_SERDES)
			bus_space_write_4(t, h, GEM_MII_SLINK_CONTROL,
				GEM_MII_SLINK_POWER_OFF);
		else
			bus_space_write_4(t, h, GEM_MII_SLINK_CONTROL,
			    GEM_MII_SLINK_LOOPBACK | GEM_MII_SLINK_POWER_OFF);
	}

	sc->sc_flags &= ~GEM_LINK;
	sc->sc_mii.mii_media_active = IFM_ETHER | IFM_NONE;
	sc->sc_mii.mii_media_status = IFM_AVALID;
}


/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
int
gem_init(struct ifnet *ifp)
{
	struct gem_softc *sc = ifp->if_softc;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	int rc = 0, s;
	u_int max_frame_size;
	u_int32_t v;

	s = splnet();

	DPRINTF(sc, ("%s: gem_init: calling stop\n", device_xname(sc->sc_dev)));
	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	gem_stop(ifp, 0);
	gem_reset(sc);
	DPRINTF(sc, ("%s: gem_init: restarting\n", device_xname(sc->sc_dev)));

	/* Re-initialize the MIF */
	gem_mifinit(sc);

	/* Set up correct datapath for non-SERDES/Serialink */
	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) == 0 &&
	    sc->sc_variant != GEM_SUN_ERI)
		bus_space_write_4(t, h, GEM_MII_DATAPATH_MODE,
		    GEM_MII_DATAPATH_MII);

	/* Call MI reset function if any */
	if (sc->sc_hwreset)
		(*sc->sc_hwreset)(sc);

	/* step 3. Setup data structures in host memory */
	if (gem_meminit(sc) != 0) {
		splx(s);
		return 1;
	}

	/* step 4. TX MAC registers & counters */
	gem_init_regs(sc);
	max_frame_size = max(sc->sc_ethercom.ec_if.if_mtu, ETHERMTU);
	max_frame_size += ETHER_HDR_LEN + ETHER_CRC_LEN;
	if (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU)
		max_frame_size += ETHER_VLAN_ENCAP_LEN;
	bus_space_write_4(t, h, GEM_MAC_MAC_MAX_FRAME,
	    max_frame_size|/* burst size */(0x2000<<16));

	/* step 5. RX MAC registers & counters */
	gem_setladrf(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	/* NOTE: we use only 32-bit DMA addresses here. */
	bus_space_write_4(t, h, GEM_TX_RING_PTR_HI, 0);
	bus_space_write_4(t, h, GEM_TX_RING_PTR_LO, GEM_CDTXADDR(sc, 0));

	bus_space_write_4(t, h, GEM_RX_RING_PTR_HI, 0);
	bus_space_write_4(t, h, GEM_RX_RING_PTR_LO, GEM_CDRXADDR(sc, 0));

	/* step 8. Global Configuration & Interrupt Mask */
	gem_inten(sc);
	bus_space_write_4(t, h, GEM_MAC_RX_MASK,
			GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT);
	bus_space_write_4(t, h, GEM_MAC_TX_MASK, 0xffff); /* XXX */
	bus_space_write_4(t, h, GEM_MAC_CONTROL_MASK,
	    GEM_MAC_PAUSED | GEM_MAC_PAUSE | GEM_MAC_RESUME);

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable TX DMA */
	v = gem_ringsize(GEM_NTXDESC /*XXX*/);
	bus_space_write_4(t, h, GEM_TX_CONFIG,
	    v | GEM_TX_CONFIG_TXDMA_EN |
	    (((sc->sc_flags & GEM_GIGABIT ? 0x4FF : 0x100) << 10) &
	    GEM_TX_CONFIG_TXFIFO_TH));
	bus_space_write_4(t, h, GEM_TX_KICK, sc->sc_txnext);

	/* step 10. ERX Configuration */
	gem_rx_common(sc);

	/* step 11. Configure Media */
	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) == 0 &&
	    (rc = mii_ifmedia_change(&sc->sc_mii)) != 0)
		goto out;

	/* step 12. RX_MAC Configuration Register */
	v = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);
	v |= GEM_MAC_RX_ENABLE | GEM_MAC_RX_STRIP_CRC;
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, v);

	/* step 14. Issue Transmit Pending command */

	/* Call MI initialization function if any */
	if (sc->sc_hwinit)
		(*sc->sc_hwinit)(sc);


	/* step 15.  Give the reciever a swift kick */
	bus_space_write_4(t, h, GEM_RX_KICK, GEM_NRXDESC-4);

	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) != 0)
		/* Configure PCS */
		gem_pcs_start(sc);
	else
		/* Start the one second timer. */
		callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);

	sc->sc_flags &= ~GEM_LINK;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	sc->sc_if_flags = ifp->if_flags;
out:
	splx(s);

	return (0);
}

void
gem_init_regs(struct gem_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	const u_char *laddr = CLLADDR(ifp->if_sadl);
	u_int32_t v;

	/* These regs are not cleared on reset */
	if (!sc->sc_inited) {

		/* Load recommended values */
		bus_space_write_4(t, h, GEM_MAC_IPG0, 0x00);
		bus_space_write_4(t, h, GEM_MAC_IPG1, 0x08);
		bus_space_write_4(t, h, GEM_MAC_IPG2, 0x04);

		bus_space_write_4(t, h, GEM_MAC_MAC_MIN_FRAME, ETHER_MIN_LEN);
		/* Max frame and max burst size */
		bus_space_write_4(t, h, GEM_MAC_MAC_MAX_FRAME,
		    ETHER_MAX_LEN | (0x2000<<16));

		bus_space_write_4(t, h, GEM_MAC_PREAMBLE_LEN, 0x07);
		bus_space_write_4(t, h, GEM_MAC_JAM_SIZE, 0x04);
		bus_space_write_4(t, h, GEM_MAC_ATTEMPT_LIMIT, 0x10);
		bus_space_write_4(t, h, GEM_MAC_CONTROL_TYPE, 0x8088);
		bus_space_write_4(t, h, GEM_MAC_RANDOM_SEED,
		    ((laddr[5]<<8)|laddr[4])&0x3ff);

		/* Secondary MAC addr set to 0:0:0:0:0:0 */
		bus_space_write_4(t, h, GEM_MAC_ADDR3, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR4, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR5, 0);

		/* MAC control addr set to 01:80:c2:00:00:01 */
		bus_space_write_4(t, h, GEM_MAC_ADDR6, 0x0001);
		bus_space_write_4(t, h, GEM_MAC_ADDR7, 0xc200);
		bus_space_write_4(t, h, GEM_MAC_ADDR8, 0x0180);

		/* MAC filter addr set to 0:0:0:0:0:0 */
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER0, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER1, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER2, 0);

		bus_space_write_4(t, h, GEM_MAC_ADR_FLT_MASK1_2, 0);
		bus_space_write_4(t, h, GEM_MAC_ADR_FLT_MASK0, 0);

		sc->sc_inited = 1;
	}

	/* Counters need to be zeroed */
	bus_space_write_4(t, h, GEM_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_LATE_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_DEFER_TMR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_PEAK_ATTEMPTS, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_FRAME_COUNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_LEN_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_ALIGN_ERR, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CRC_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CODE_VIOL, 0);

	/* Set XOFF PAUSE time. */
	bus_space_write_4(t, h, GEM_MAC_SEND_PAUSE_CMD, 0x1BF0);

	/*
	 * Set the internal arbitration to "infinite" bursts of the
	 * maximum length of 31 * 64 bytes so DMA transfers aren't
	 * split up in cache line size chunks. This greatly improves
	 * especially RX performance.
	 * Enable silicon bug workarounds for the Apple variants.
	 */
	bus_space_write_4(t, h, GEM_CONFIG,
	    GEM_CONFIG_TXDMA_LIMIT | GEM_CONFIG_RXDMA_LIMIT |
	    ((sc->sc_flags & GEM_PCI) ?
	    GEM_CONFIG_BURST_INF : GEM_CONFIG_BURST_64) | (GEM_IS_APPLE(sc) ?
	    GEM_CONFIG_RONPAULBIT | GEM_CONFIG_BUG2FIX : 0));

	/*
	 * Set the station address.
	 */
	bus_space_write_4(t, h, GEM_MAC_ADDR0, (laddr[4]<<8)|laddr[5]);
	bus_space_write_4(t, h, GEM_MAC_ADDR1, (laddr[2]<<8)|laddr[3]);
	bus_space_write_4(t, h, GEM_MAC_ADDR2, (laddr[0]<<8)|laddr[1]);

	/*
	 * Enable MII outputs.  Enable GMII if there is a gigabit PHY.
	 */
	sc->sc_mif_config = bus_space_read_4(t, h, GEM_MIF_CONFIG);
	v = GEM_MAC_XIF_TX_MII_ENA;
	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) == 0)  {
		if (sc->sc_mif_config & GEM_MIF_CONFIG_MDI1) {
			v |= GEM_MAC_XIF_FDPLX_LED;
				if (sc->sc_flags & GEM_GIGABIT)
					v |= GEM_MAC_XIF_GMII_MODE;
		}
	} else {
		v |= GEM_MAC_XIF_GMII_MODE;
	}
	bus_space_write_4(t, h, GEM_MAC_XIF_CONFIG, v);
}

#ifdef GEM_DEBUG
static void
gem_txsoft_print(const struct gem_softc *sc, int firstdesc, int lastdesc)
{
	int i;

	for (i = firstdesc;; i = GEM_NEXTTX(i)) {
		printf("descriptor %d:\t", i);
		printf("gd_flags:   0x%016" PRIx64 "\t",
			GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_flags));
		printf("gd_addr: 0x%016" PRIx64 "\n",
			GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_addr));
		if (i == lastdesc)
			break;
	}
}
#endif

static void
gem_start(struct ifnet *ifp)
{
	struct gem_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct gem_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, firsttx, nexttx = -1, lasttx = -1, ofree, seg;
	uint64_t flags = 0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;
	firsttx = sc->sc_txnext;

	DPRINTF(sc, ("%s: gem_start: txfree %d, txnext %d\n",
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
		 */
		if (bus_dmamap_load_mbuf(sc->sc_dmatag, dmamap, m0,
		      BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0 ||
		      (m0->m_pkthdr.len < ETHER_MIN_TX &&
		       dmamap->dm_nsegs == GEM_NTXSEGS)) {
			if (m0->m_pkthdr.len > MCLBYTES) {
				aprint_error_dev(sc->sc_dev,
				    "unable to allocate jumbo Tx cluster\n");
				IFQ_DEQUEUE(&ifp->if_snd, m0);
				m_freem(m0);
				continue;
			}
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "unable to allocate Tx mbuf\n");
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
			error = bus_dmamap_load_mbuf(sc->sc_dmatag, dmamap,
			    m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				aprint_error_dev(sc->sc_dev,
				    "unable to load Tx buffer, error = %d\n",
				    error);
				break;
			}
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > ((m0->m_pkthdr.len < ETHER_MIN_TX) ?
		     (sc->sc_txfree - 1) : sc->sc_txfree)) {
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
			sc->sc_if_flags = ifp->if_flags;
			bus_dmamap_unload(sc->sc_dmatag, dmamap);
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
		bus_dmamap_sync(sc->sc_dmatag, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = GEM_NEXTTX(nexttx)) {

			/*
			 * If this is the first descriptor we're
			 * enqueueing, set the start of packet flag,
			 * and the checksum stuff if we want the hardware
			 * to do it.
			 */
			sc->sc_txdescs[nexttx].gd_addr =
			    GEM_DMA_WRITE(sc, dmamap->dm_segs[seg].ds_addr);
			flags = dmamap->dm_segs[seg].ds_len & GEM_TD_BUFSIZE;
			if (nexttx == firsttx) {
				flags |= GEM_TD_START_OF_PACKET;
				if (++sc->sc_txwin > GEM_NTXSEGS * 2 / 3) {
					sc->sc_txwin = 0;
					flags |= GEM_TD_INTERRUPT_ME;
				}

#ifdef INET
				/* h/w checksum */
				if (ifp->if_csum_flags_tx & M_CSUM_TCPv4 &&
				    m0->m_pkthdr.csum_flags & M_CSUM_TCPv4) {
					struct ether_header *eh;
					uint16_t offset, start;

					eh = mtod(m0, struct ether_header *);
					switch (ntohs(eh->ether_type)) {
					case ETHERTYPE_IP:
						start = ETHER_HDR_LEN;
						break;
					case ETHERTYPE_VLAN:
						start = ETHER_HDR_LEN +
							ETHER_VLAN_ENCAP_LEN;
						break;
					default:
						/* unsupported, drop it */
						m_free(m0);
						continue;
					}
					start += M_CSUM_DATA_IPv4_IPHL(m0->m_pkthdr.csum_data);
					offset = M_CSUM_DATA_IPv4_OFFSET(m0->m_pkthdr.csum_data) + start;
					flags |= (start <<
						  GEM_TD_CXSUM_STARTSHFT) |
						 (offset <<
						  GEM_TD_CXSUM_STUFFSHFT) |
						 GEM_TD_CXSUM_ENABLE;
				}
#endif
			}
			if (seg == dmamap->dm_nsegs - 1) {
				flags |= GEM_TD_END_OF_PACKET;
			} else {
				/* last flag set outside of loop */
				sc->sc_txdescs[nexttx].gd_flags =
					GEM_DMA_WRITE(sc, flags);
			}
			lasttx = nexttx;
		}
		if (m0->m_pkthdr.len < ETHER_MIN_TX) {
			/* add padding buffer at end of chain */
			flags &= ~GEM_TD_END_OF_PACKET;
			sc->sc_txdescs[lasttx].gd_flags =
			    GEM_DMA_WRITE(sc, flags);

			sc->sc_txdescs[nexttx].gd_addr =
			    GEM_DMA_WRITE(sc,
			    sc->sc_nulldmamap->dm_segs[0].ds_addr);
			flags = ((ETHER_MIN_TX - m0->m_pkthdr.len) &
			    GEM_TD_BUFSIZE) | GEM_TD_END_OF_PACKET;
			lasttx = nexttx;
			nexttx = GEM_NEXTTX(nexttx);
			seg++;
		}
		sc->sc_txdescs[lasttx].gd_flags = GEM_DMA_WRITE(sc, flags);

		KASSERT(lasttx != -1);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;
		txs->txs_ndescs = seg;

#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			printf("     gem_start %p transmit chain:\n", txs);
			gem_txsoft_print(sc, txs->txs_firstdesc,
			    txs->txs_lastdesc);
		}
#endif

		/* Sync the descriptors we're using. */
		GEM_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_ndescs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Advance the tx pointer. */
		sc->sc_txfree -= txs->txs_ndescs;
		sc->sc_txnext = nexttx;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
		SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (txs == NULL || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
		sc->sc_if_flags = ifp->if_flags;
	}

	if (sc->sc_txfree != ofree) {
		DPRINTF(sc, ("%s: packets enqueued, IC on %d, OWN on %d\n",
		    device_xname(sc->sc_dev), lasttx, firsttx));
		/*
		 * The entire packet chain is set up.
		 * Kick the transmitter.
		 */
		DPRINTF(sc, ("%s: gem_start: kicking tx %d\n",
			device_xname(sc->sc_dev), nexttx));
		bus_space_write_4(sc->sc_bustag, sc->sc_h1, GEM_TX_KICK,
			sc->sc_txnext);

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
		DPRINTF(sc, ("%s: gem_start: watchdog %d\n",
			device_xname(sc->sc_dev), ifp->if_timer));
	}
}

/*
 * Transmit interrupt.
 */
int
gem_tint(struct gem_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_h1;
	struct gem_txsoft *txs;
	int txlast;
	int progress = 0;
	u_int32_t v;

	DPRINTF(sc, ("%s: gem_tint\n", device_xname(sc->sc_dev)));

	/* Unload collision counters ... */
	v = bus_space_read_4(t, mac, GEM_MAC_EXCESS_COLL_CNT) +
	    bus_space_read_4(t, mac, GEM_MAC_LATE_COLL_CNT);
	ifp->if_collisions += v +
	    bus_space_read_4(t, mac, GEM_MAC_NORM_COLL_CNT) +
	    bus_space_read_4(t, mac, GEM_MAC_FIRST_COLL_CNT);
	ifp->if_oerrors += v;

	/* ... then clear the hardware counters. */
	bus_space_write_4(t, mac, GEM_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_LATE_COLL_CNT, 0);

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		/*
		 * In theory, we could harvest some descriptors before
		 * the ring is empty, but that's a bit complicated.
		 *
		 * GEM_TX_COMPLETION points to the last descriptor
		 * processed +1.
		 *
		 * Let's assume that the NIC writes back to the Tx
		 * descriptors before it updates the completion
		 * register.  If the NIC has posted writes to the
		 * Tx descriptors, PCI ordering requires that the
		 * posted writes flush to RAM before the register-read
		 * finishes.  So let's read the completion register,
		 * before syncing the descriptors, so that we
		 * examine Tx descriptors that are at least as
		 * current as the completion register.
		 */
		txlast = bus_space_read_4(t, mac, GEM_TX_COMPLETION);
		DPRINTF(sc,
			("gem_tint: txs->txs_lastdesc = %d, txlast = %d\n",
				txs->txs_lastdesc, txlast));
		if (txs->txs_firstdesc <= txs->txs_lastdesc) {
			if (txlast >= txs->txs_firstdesc &&
			    txlast <= txs->txs_lastdesc)
				break;
		} else if (txlast >= txs->txs_firstdesc ||
			   txlast <= txs->txs_lastdesc)
			break;

		GEM_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_ndescs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

#ifdef GEM_DEBUG	/* XXX DMA synchronization? */
		if (ifp->if_flags & IFF_DEBUG) {
			printf("    txsoft %p transmit chain:\n", txs);
			gem_txsoft_print(sc, txs->txs_firstdesc,
			    txs->txs_lastdesc);
		}
#endif


		DPRINTF(sc, ("gem_tint: releasing a desc\n"));
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_ndescs;

		bus_dmamap_sync(sc->sc_dmatag, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmatag, txs->txs_dmamap);
		if (txs->txs_mbuf != NULL) {
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		ifp->if_opackets++;
		progress = 1;
	}

#if 0
	DPRINTF(sc, ("gem_tint: GEM_TX_STATE_MACHINE %x "
		"GEM_TX_DATA_PTR %" PRIx64 "GEM_TX_COMPLETION %" PRIx32 "\n",
		bus_space_read_4(sc->sc_bustag, sc->sc_h1, GEM_TX_STATE_MACHINE),
		((uint64_t)bus_space_read_4(sc->sc_bustag, sc->sc_h1,
			GEM_TX_DATA_PTR_HI) << 32) |
			     bus_space_read_4(sc->sc_bustag, sc->sc_h1,
			GEM_TX_DATA_PTR_LO),
		bus_space_read_4(sc->sc_bustag, sc->sc_h1, GEM_TX_COMPLETION)));
#endif

	if (progress) {
		if (sc->sc_txfree == GEM_NTXDESC - 1)
			sc->sc_txwin = 0;

		/* Freed some descriptors, so reset IFF_OACTIVE and restart. */
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->sc_if_flags = ifp->if_flags;
		ifp->if_timer = SIMPLEQ_EMPTY(&sc->sc_txdirtyq) ? 0 : 5;
		gem_start(ifp);
	}
	DPRINTF(sc, ("%s: gem_tint: watchdog %d\n",
		device_xname(sc->sc_dev), ifp->if_timer));

	return (1);
}

/*
 * Receive interrupt.
 */
int
gem_rint(struct gem_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	struct gem_rxsoft *rxs;
	struct mbuf *m;
	u_int64_t rxstat;
	u_int32_t rxcomp;
	int i, len, progress = 0;

	DPRINTF(sc, ("%s: gem_rint\n", device_xname(sc->sc_dev)));

	/*
	 * Ignore spurious interrupt that sometimes occurs before
	 * we are set up when we network boot.
	 */
	if (!sc->sc_meminited)
		return 1;

	/*
	 * Read the completion register once.  This limits
	 * how long the following loop can execute.
	 */
	rxcomp = bus_space_read_4(t, h, GEM_RX_COMPLETION);

	/*
	 * XXX Read the lastrx only once at the top for speed.
	 */
	DPRINTF(sc, ("gem_rint: sc->rxptr %d, complete %d\n",
		sc->sc_rxptr, rxcomp));

	/*
	 * Go into the loop at least once.
	 */
	for (i = sc->sc_rxptr; i == sc->sc_rxptr || i != rxcomp;
	     i = GEM_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		GEM_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_flags);

		if (rxstat & GEM_RD_OWN) {
			GEM_CDRXSYNC(sc, i, BUS_DMASYNC_PREREAD);
			/*
			 * We have processed all of the receive buffers.
			 */
			break;
		}

		progress++;
		ifp->if_ipackets++;

		if (rxstat & GEM_RD_BAD_CRC) {
			ifp->if_ierrors++;
			aprint_error_dev(sc->sc_dev,
			    "receive error: CRC error\n");
			GEM_INIT_RXDESC(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			printf("    rxsoft %p descriptor %d: ", rxs, i);
			printf("gd_flags: 0x%016llx\t", (long long)
				GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_flags));
			printf("gd_addr: 0x%016llx\n", (long long)
				GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_addr));
		}
#endif

		/* No errors; receive the packet. */
		len = GEM_RD_BUFLEN(rxstat);

		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		m = rxs->rxs_mbuf;
		if (gem_add_rxbuf(sc, i) != 0) {
			GEM_COUNTER_INCR(sc, sc_ev_rxnobuf);
			ifp->if_ierrors++;
			aprint_error_dev(sc->sc_dev,
			    "receive error: RX no buffer space\n");
			GEM_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}
		m->m_data += 2; /* We're already off by two */

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

#ifdef INET
		/* hardware checksum */
		if (ifp->if_csum_flags_rx & M_CSUM_TCPv4) {
			struct ether_header *eh;
			struct ip *ip;
			int32_t hlen, pktlen;

			if (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) {
				pktlen = m->m_pkthdr.len - ETHER_HDR_LEN -
					 ETHER_VLAN_ENCAP_LEN;
				eh = (struct ether_header *) (mtod(m, char *) +
					ETHER_VLAN_ENCAP_LEN);
			} else {
				pktlen = m->m_pkthdr.len - ETHER_HDR_LEN;
				eh = mtod(m, struct ether_header *);
			}
			if (ntohs(eh->ether_type) != ETHERTYPE_IP)
				goto swcsum;
			ip = (struct ip *) ((char *)eh + ETHER_HDR_LEN);

			/* IPv4 only */
			if (ip->ip_v != IPVERSION)
				goto swcsum;

			hlen = ip->ip_hl << 2;
			if (hlen < sizeof(struct ip))
				goto swcsum;

			/*
			 * bail if too short, has random trailing garbage,
			 * truncated, fragment, or has ethernet pad.
			 */
			if ((ntohs(ip->ip_len) < hlen) ||
			    (ntohs(ip->ip_len) != pktlen) ||
			    (ntohs(ip->ip_off) & (IP_MF | IP_OFFMASK)))
				goto swcsum;

			switch (ip->ip_p) {
			case IPPROTO_TCP:
				if (! (ifp->if_csum_flags_rx & M_CSUM_TCPv4))
					goto swcsum;
				if (pktlen < (hlen + sizeof(struct tcphdr)))
					goto swcsum;
				m->m_pkthdr.csum_flags = M_CSUM_TCPv4;
				break;
			case IPPROTO_UDP:
				/* FALLTHROUGH */
			default:
				goto swcsum;
			}

			/* the uncomplemented sum is expected */
			m->m_pkthdr.csum_data = (~rxstat) & GEM_RD_CHECKSUM;

			/* if the pkt had ip options, we have to deduct them */
			if (hlen > sizeof(struct ip)) {
				uint16_t *opts;
				uint32_t optsum, temp;

				optsum = 0;
				temp = hlen - sizeof(struct ip);
				opts = (uint16_t *) ((char *) ip +
					sizeof(struct ip));

				while (temp > 1) {
					optsum += ntohs(*opts++);
					temp -= 2;
				}
				while (optsum >> 16)
					optsum = (optsum >> 16) +
						 (optsum & 0xffff);

				/* Deduct ip opts sum from hwsum. */
				m->m_pkthdr.csum_data += (uint16_t)~optsum;

				while (m->m_pkthdr.csum_data >> 16)
					m->m_pkthdr.csum_data =
						(m->m_pkthdr.csum_data >> 16) +
						(m->m_pkthdr.csum_data &
						 0xffff);
			}

			m->m_pkthdr.csum_flags |= M_CSUM_DATA |
						  M_CSUM_NO_PSEUDOHDR;
		} else
swcsum:
			m->m_pkthdr.csum_flags = 0;
#endif
		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	if (progress) {
		/* Update the receive pointer. */
		if (i == sc->sc_rxptr) {
			GEM_COUNTER_INCR(sc, sc_ev_rxfull);
#ifdef GEM_DEBUG
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: rint: ring wrap\n",
				    device_xname(sc->sc_dev));
#endif
		}
		sc->sc_rxptr = i;
		bus_space_write_4(t, h, GEM_RX_KICK, GEM_PREVRX(i));
	}
#ifdef GEM_COUNTERS
	if (progress <= 4) {
		GEM_COUNTER_INCR(sc, sc_ev_rxhist[progress]);
	} else if (progress < 32) {
		if (progress < 16)
			GEM_COUNTER_INCR(sc, sc_ev_rxhist[5]);
		else
			GEM_COUNTER_INCR(sc, sc_ev_rxhist[6]);

	} else {
		if (progress < 64)
			GEM_COUNTER_INCR(sc, sc_ev_rxhist[7]);
		else
			GEM_COUNTER_INCR(sc, sc_ev_rxhist[8]);
	}
#endif

	DPRINTF(sc, ("gem_rint: done sc->rxptr %d, complete %d\n",
		sc->sc_rxptr, bus_space_read_4(t, h, GEM_RX_COMPLETION)));

	/* Read error counters ... */
	ifp->if_ierrors +=
	    bus_space_read_4(t, h, GEM_MAC_RX_LEN_ERR_CNT) +
	    bus_space_read_4(t, h, GEM_MAC_RX_ALIGN_ERR) +
	    bus_space_read_4(t, h, GEM_MAC_RX_CRC_ERR_CNT) +
	    bus_space_read_4(t, h, GEM_MAC_RX_CODE_VIOL);

	/* ... then clear the hardware counters. */
	bus_space_write_4(t, h, GEM_MAC_RX_LEN_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_ALIGN_ERR, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CRC_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CODE_VIOL, 0);

	return (1);
}


/*
 * gem_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
gem_add_rxbuf(struct gem_softc *sc, int idx)
{
	struct gem_rxsoft *rxs = &sc->sc_rxsoft[idx];
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

#ifdef GEM_DEBUG
/* bzero the packet to check DMA */
	memset(m->m_ext.ext_buf, 0, m->m_ext.ext_size);
#endif

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmatag, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmatag, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "can't load rx DMA map %d, error = %d\n", idx, error);
		panic("gem_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	GEM_INIT_RXDESC(sc, idx);

	return (0);
}


int
gem_eint(struct gem_softc *sc, u_int status)
{
	char bits[128];
	u_int32_t r, v;

	if ((status & GEM_INTR_MIF) != 0) {
		printf("%s: XXXlink status changed\n", device_xname(sc->sc_dev));
		return (1);
	}

	if ((status & GEM_INTR_RX_TAG_ERR) != 0) {
		gem_reset_rxdma(sc);
		return (1);
	}

	if (status & GEM_INTR_BERR) {
		if (sc->sc_flags & GEM_PCI)
			r = GEM_ERROR_STATUS;
		else
			r = GEM_SBUS_ERROR_STATUS;
		bus_space_read_4(sc->sc_bustag, sc->sc_h2, r);
		v = bus_space_read_4(sc->sc_bustag, sc->sc_h2, r);
		aprint_error_dev(sc->sc_dev, "bus error interrupt: 0x%02x\n",
		    v);
		return (1);
	}
	snprintb(bits, sizeof(bits), GEM_INTR_BITS, status);
	printf("%s: status=%s\n", device_xname(sc->sc_dev), bits);
		
	return (1);
}


/*
 * PCS interrupts.
 * We should receive these when the link status changes, but sometimes
 * we don't receive them for link up.  We compensate for this in the
 * gem_tick() callout.
 */
int
gem_pint(struct gem_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t v, v2;

	/*
	 * Clear the PCS interrupt from GEM_STATUS.  The PCS register is
	 * latched, so we have to read it twice.  There is only one bit in
	 * use, so the value is meaningless.
	 */
	bus_space_read_4(t, h, GEM_MII_INTERRUP_STATUS);
	bus_space_read_4(t, h, GEM_MII_INTERRUP_STATUS);

	if ((ifp->if_flags & IFF_UP) == 0)
		return 1;

	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) == 0)
		return 1;

	v = bus_space_read_4(t, h, GEM_MII_STATUS);
	/* If we see remote fault, our link partner is probably going away */
	if ((v & GEM_MII_STATUS_REM_FLT) != 0) {
		gem_bitwait(sc, h, GEM_MII_STATUS, GEM_MII_STATUS_REM_FLT, 0);
		v = bus_space_read_4(t, h, GEM_MII_STATUS);
	/* Otherwise, we may need to wait after auto-negotiation completes */
	} else if ((v & (GEM_MII_STATUS_LINK_STS | GEM_MII_STATUS_ANEG_CPT)) ==
	    GEM_MII_STATUS_ANEG_CPT) {
		gem_bitwait(sc, h, GEM_MII_STATUS, 0, GEM_MII_STATUS_LINK_STS);
		v = bus_space_read_4(t, h, GEM_MII_STATUS);
	}
	if ((v & GEM_MII_STATUS_LINK_STS) != 0) {
		if (sc->sc_flags & GEM_LINK) {
			return 1;
		}
		callout_stop(&sc->sc_tick_ch);
		v = bus_space_read_4(t, h, GEM_MII_ANAR);
		v2 = bus_space_read_4(t, h, GEM_MII_ANLPAR);
		sc->sc_mii.mii_media_active = IFM_ETHER | IFM_1000_SX;
		sc->sc_mii.mii_media_status = IFM_AVALID | IFM_ACTIVE;
		v &= v2;
		if (v & GEM_MII_ANEG_FUL_DUPLX) {
			sc->sc_mii.mii_media_active |= IFM_FDX;
#ifdef GEM_DEBUG
			aprint_debug_dev(sc->sc_dev, "link up: full duplex\n");
#endif
		} else if (v & GEM_MII_ANEG_HLF_DUPLX) {
			sc->sc_mii.mii_media_active |= IFM_HDX;
#ifdef GEM_DEBUG
			aprint_debug_dev(sc->sc_dev, "link up: half duplex\n");
#endif
		} else {
#ifdef GEM_DEBUG
			aprint_debug_dev(sc->sc_dev, "duplex mismatch\n");
#endif
		}
		gem_statuschange(sc);
	} else {
		if ((sc->sc_flags & GEM_LINK) == 0) {
			return 1;
		}
		sc->sc_mii.mii_media_active = IFM_ETHER | IFM_NONE;
		sc->sc_mii.mii_media_status = IFM_AVALID;
#ifdef GEM_DEBUG
			aprint_debug_dev(sc->sc_dev, "link down\n");
#endif
		gem_statuschange(sc);

		/* Start the 10 second timer */
		callout_reset(&sc->sc_tick_ch, hz * 10, gem_tick, sc);
	}
	return 1;
}



int
gem_intr(void *v)
{
	struct gem_softc *sc = v;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t status;
	int r = 0;
#ifdef GEM_DEBUG
	char bits[128];
#endif

	/* XXX We should probably mask out interrupts until we're done */

	sc->sc_ev_intr.ev_count++;

	status = bus_space_read_4(t, h, GEM_STATUS);
#ifdef GEM_DEBUG
	snprintb(bits, sizeof(bits), GEM_INTR_BITS, status);
#endif
	DPRINTF(sc, ("%s: gem_intr: cplt 0x%x status %s\n",
		device_xname(sc->sc_dev), (status >> 19), bits));
		

	if ((status & (GEM_INTR_RX_TAG_ERR | GEM_INTR_BERR)) != 0)
		r |= gem_eint(sc, status);

	/* We don't bother with GEM_INTR_TX_DONE */
	if ((status & (GEM_INTR_TX_EMPTY | GEM_INTR_TX_INTME)) != 0) {
		GEM_COUNTER_INCR(sc, sc_ev_txint);
		r |= gem_tint(sc);
	}

	if ((status & (GEM_INTR_RX_DONE | GEM_INTR_RX_NOBUF)) != 0) {
		GEM_COUNTER_INCR(sc, sc_ev_rxint);
		r |= gem_rint(sc);
	}

	/* We should eventually do more than just print out error stats. */
	if (status & GEM_INTR_TX_MAC) {
		int txstat = bus_space_read_4(t, h, GEM_MAC_TX_STATUS);
		if (txstat & ~GEM_MAC_TX_XMIT_DONE)
			printf("%s: MAC tx fault, status %x\n",
			    device_xname(sc->sc_dev), txstat);
		if (txstat & (GEM_MAC_TX_UNDERRUN | GEM_MAC_TX_PKT_TOO_LONG))
			gem_init(ifp);
	}
	if (status & GEM_INTR_RX_MAC) {
		int rxstat = bus_space_read_4(t, h, GEM_MAC_RX_STATUS);
		/*
		 * At least with GEM_SUN_GEM and some GEM_SUN_ERI
		 * revisions GEM_MAC_RX_OVERFLOW happen often due to a
		 * silicon bug so handle them silently.  So if we detect
		 * an RX FIFO overflow, we fire off a timer, and check
		 * whether we're still making progress by looking at the
		 * RX FIFO write and read pointers.
		 */
		if (rxstat & GEM_MAC_RX_OVERFLOW) {
			ifp->if_ierrors++;
			aprint_error_dev(sc->sc_dev,
			    "receive error: RX overflow sc->rxptr %d, complete %d\n", sc->sc_rxptr, bus_space_read_4(t, h, GEM_RX_COMPLETION));
			sc->sc_rx_fifo_wr_ptr =
				bus_space_read_4(t, h, GEM_RX_FIFO_WR_PTR);
			sc->sc_rx_fifo_rd_ptr =
				bus_space_read_4(t, h, GEM_RX_FIFO_RD_PTR);
			callout_schedule(&sc->sc_rx_watchdog, 400);
		} else if (rxstat & ~(GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT))
			printf("%s: MAC rx fault, status 0x%02x\n",
			    device_xname(sc->sc_dev), rxstat);
	}
	if (status & GEM_INTR_PCS) {
		r |= gem_pint(sc);
	}

/* Do we need to do anything with these?
	if ((status & GEM_MAC_CONTROL_STATUS) != 0) {
		status2 = bus_read_4(sc->sc_res[0], GEM_MAC_CONTROL_STATUS);
		if ((status2 & GEM_MAC_PAUSED) != 0)
			aprintf_debug_dev(sc->sc_dev, "PAUSE received (%d slots)\n",
			    GEM_MAC_PAUSE_TIME(status2));
		if ((status2 & GEM_MAC_PAUSE) != 0)
			aprintf_debug_dev(sc->sc_dev, "transited to PAUSE state\n");
		if ((status2 & GEM_MAC_RESUME) != 0)
			aprintf_debug_dev(sc->sc_dev, "transited to non-PAUSE state\n");
	}
	if ((status & GEM_INTR_MIF) != 0)
		aprintf_debug_dev(sc->sc_dev, "MIF interrupt\n");
*/
	rnd_add_uint32(&sc->rnd_source, status);
	return (r);
}

void
gem_rx_watchdog(void *arg)
{
	struct gem_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t rx_fifo_wr_ptr;
	u_int32_t rx_fifo_rd_ptr;
	u_int32_t state;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		aprint_error_dev(sc->sc_dev, "receiver not running\n");
		return;
	}

	rx_fifo_wr_ptr = bus_space_read_4(t, h, GEM_RX_FIFO_WR_PTR);
	rx_fifo_rd_ptr = bus_space_read_4(t, h, GEM_RX_FIFO_RD_PTR);
	state = bus_space_read_4(t, h, GEM_MAC_MAC_STATE);
	if ((state & GEM_MAC_STATE_OVERFLOW) == GEM_MAC_STATE_OVERFLOW &&
	    ((rx_fifo_wr_ptr == rx_fifo_rd_ptr) ||
	     ((sc->sc_rx_fifo_wr_ptr == rx_fifo_wr_ptr) &&
	      (sc->sc_rx_fifo_rd_ptr == rx_fifo_rd_ptr))))
	{
		/*
		 * The RX state machine is still in overflow state and
		 * the RX FIFO write and read pointers seem to be
		 * stuck.  Whack the chip over the head to get things
		 * going again.
		 */
		aprint_error_dev(sc->sc_dev,
		    "receiver stuck in overflow, resetting\n");
		gem_init(ifp);
	} else {
		if ((state & GEM_MAC_STATE_OVERFLOW) != GEM_MAC_STATE_OVERFLOW) {
			aprint_error_dev(sc->sc_dev,
				"rx_watchdog: not in overflow state: 0x%x\n",
				state);
		}
		if (rx_fifo_wr_ptr != rx_fifo_rd_ptr) {
			aprint_error_dev(sc->sc_dev,
				"rx_watchdog: wr & rd ptr different\n");
		}
		if (sc->sc_rx_fifo_wr_ptr != rx_fifo_wr_ptr) {
			aprint_error_dev(sc->sc_dev,
				"rx_watchdog: wr pointer != saved\n");
		}
		if (sc->sc_rx_fifo_rd_ptr != rx_fifo_rd_ptr) {
			aprint_error_dev(sc->sc_dev,
				"rx_watchdog: rd pointer != saved\n");
		}
		aprint_error_dev(sc->sc_dev, "resetting anyway\n");
		gem_init(ifp);
	}
}

void
gem_watchdog(struct ifnet *ifp)
{
	struct gem_softc *sc = ifp->if_softc;

	DPRINTF(sc, ("gem_watchdog: GEM_RX_CONFIG %x GEM_MAC_RX_STATUS %x "
		"GEM_MAC_RX_CONFIG %x\n",
		bus_space_read_4(sc->sc_bustag, sc->sc_h1, GEM_RX_CONFIG),
		bus_space_read_4(sc->sc_bustag, sc->sc_h1, GEM_MAC_RX_STATUS),
		bus_space_read_4(sc->sc_bustag, sc->sc_h1, GEM_MAC_RX_CONFIG)));

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;

	/* Try to get more packets going. */
	gem_init(ifp);
	gem_start(ifp);
}

/*
 * Initialize the MII Management Interface
 */
void
gem_mifinit(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h1;

	/* Configure the MIF in frame mode */
	sc->sc_mif_config = bus_space_read_4(t, mif, GEM_MIF_CONFIG);
	sc->sc_mif_config &= ~GEM_MIF_CONFIG_BB_ENA;
	bus_space_write_4(t, mif, GEM_MIF_CONFIG, sc->sc_mif_config);
}

/*
 * MII interface
 *
 * The GEM MII interface supports at least three different operating modes:
 *
 * Bitbang mode is implemented using data, clock and output enable registers.
 *
 * Frame mode is implemented by loading a complete frame into the frame
 * register and polling the valid bit for completion.
 *
 * Polling mode uses the frame register but completion is indicated by
 * an interrupt.
 *
 */
static int
gem_mii_readreg(device_t self, int phy, int reg)
{
	struct gem_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h1;
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG1
	if (sc->sc_debug)
		printf("gem_mii_readreg: PHY %d reg %d\n", phy, reg);
#endif

	/* Construct the frame command */
	v = (reg << GEM_MIF_REG_SHIFT)	| (phy << GEM_MIF_PHY_SHIFT) |
		GEM_MIF_FRAME_READ;

	bus_space_write_4(t, mif, GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return (v & GEM_MIF_FRAME_DATA);
	}

	printf("%s: mii_read timeout\n", device_xname(sc->sc_dev));
	return (0);
}

static void
gem_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct gem_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h1;
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG1
	if (sc->sc_debug)
		printf("gem_mii_writereg: PHY %d reg %d val %x\n",
			phy, reg, val);
#endif

	/* Construct the frame command */
	v = GEM_MIF_FRAME_WRITE			|
	    (phy << GEM_MIF_PHY_SHIFT)		|
	    (reg << GEM_MIF_REG_SHIFT)		|
	    (val & GEM_MIF_FRAME_DATA);

	bus_space_write_4(t, mif, GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return;
	}

	printf("%s: mii_write timeout\n", device_xname(sc->sc_dev));
}

static void
gem_mii_statchg(struct ifnet *ifp)
{
	struct gem_softc *sc = ifp->if_softc;
#ifdef GEM_DEBUG
	int instance = IFM_INST(sc->sc_mii.mii_media.ifm_cur->ifm_media);
#endif

#ifdef GEM_DEBUG
	if (sc->sc_debug)
		printf("gem_mii_statchg: status change: phy = %d\n",
			sc->sc_phys[instance]);
#endif
	gem_statuschange(sc);
}

/*
 * Common status change for gem_mii_statchg() and gem_pint()
 */
void
gem_statuschange(struct gem_softc* sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_h1;
	int gigabit;
	u_int32_t rxcfg, txcfg, v;

	if ((sc->sc_mii.mii_media_status & IFM_ACTIVE) != 0 &&
	    IFM_SUBTYPE(sc->sc_mii.mii_media_active) != IFM_NONE)
		sc->sc_flags |= GEM_LINK;
	else
		sc->sc_flags &= ~GEM_LINK;

	if (sc->sc_ethercom.ec_if.if_baudrate == IF_Mbps(1000))
		gigabit = 1;
	else
		gigabit = 0;

	/*
	 * The configuration done here corresponds to the steps F) and
	 * G) and as far as enabling of RX and TX MAC goes also step H)
	 * of the initialization sequence outlined in section 3.2.1 of
	 * the GEM Gigabit Ethernet ASIC Specification.
	 */

	rxcfg = bus_space_read_4(t, mac, GEM_MAC_RX_CONFIG);
	rxcfg &= ~(GEM_MAC_RX_CARR_EXTEND | GEM_MAC_RX_ENABLE);
	txcfg = GEM_MAC_TX_ENA_IPG0 | GEM_MAC_TX_NGU | GEM_MAC_TX_NGU_LIMIT;
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0)
		txcfg |= GEM_MAC_TX_IGN_CARRIER | GEM_MAC_TX_IGN_COLLIS;
	else if (gigabit) {
		rxcfg |= GEM_MAC_RX_CARR_EXTEND;
		txcfg |= GEM_MAC_RX_CARR_EXTEND;
	}
	bus_space_write_4(t, mac, GEM_MAC_TX_CONFIG, 0);
	bus_space_barrier(t, mac, GEM_MAC_TX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, mac, GEM_MAC_TX_CONFIG, GEM_MAC_TX_ENABLE, 0))
		aprint_normal_dev(sc->sc_dev, "cannot disable TX MAC\n");
	bus_space_write_4(t, mac, GEM_MAC_TX_CONFIG, txcfg);
	bus_space_write_4(t, mac, GEM_MAC_RX_CONFIG, 0);
	bus_space_barrier(t, mac, GEM_MAC_RX_CONFIG, 4,
	    BUS_SPACE_BARRIER_WRITE);
	if (!gem_bitwait(sc, mac, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0))
		aprint_normal_dev(sc->sc_dev, "cannot disable RX MAC\n");
	bus_space_write_4(t, mac, GEM_MAC_RX_CONFIG, rxcfg);

	v = bus_space_read_4(t, mac, GEM_MAC_CONTROL_CONFIG) &
	    ~(GEM_MAC_CC_RX_PAUSE | GEM_MAC_CC_TX_PAUSE);
	bus_space_write_4(t, mac, GEM_MAC_CONTROL_CONFIG, v);

	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) == 0 &&
	    gigabit != 0)
		bus_space_write_4(t, mac, GEM_MAC_SLOT_TIME,
		    GEM_MAC_SLOT_TIME_CARR_EXTEND);
	else
		bus_space_write_4(t, mac, GEM_MAC_SLOT_TIME,
		    GEM_MAC_SLOT_TIME_NORMAL);

	/* XIF Configuration */
	if (sc->sc_flags & GEM_LINK)
		v = GEM_MAC_XIF_LINK_LED;
	else
		v = 0;
	v |= GEM_MAC_XIF_TX_MII_ENA;

	/* If an external transceiver is connected, enable its MII drivers */
	sc->sc_mif_config = bus_space_read_4(t, mac, GEM_MIF_CONFIG);
	if ((sc->sc_flags &(GEM_SERDES | GEM_SERIAL)) == 0) {
		if ((sc->sc_mif_config & GEM_MIF_CONFIG_MDI1) != 0) {
			if (gigabit)
				v |= GEM_MAC_XIF_GMII_MODE;
			else
				v &= ~GEM_MAC_XIF_GMII_MODE;
		} else
			/* Internal MII needs buf enable */
			v |= GEM_MAC_XIF_MII_BUF_ENA;
		/* MII needs echo disable if half duplex. */
		if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0)
			/* turn on full duplex LED */
			v |= GEM_MAC_XIF_FDPLX_LED;
		else
			/* half duplex -- disable echo */
			v |= GEM_MAC_XIF_ECHO_DISABL;
	} else {
		if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0)
			v |= GEM_MAC_XIF_FDPLX_LED;
		v |= GEM_MAC_XIF_GMII_MODE;
	}
	bus_space_write_4(t, mac, GEM_MAC_XIF_CONFIG, v);

	if ((ifp->if_flags & IFF_RUNNING) != 0 &&
	    (sc->sc_flags & GEM_LINK) != 0) {
		bus_space_write_4(t, mac, GEM_MAC_TX_CONFIG,
		    txcfg | GEM_MAC_TX_ENABLE);
		bus_space_write_4(t, mac, GEM_MAC_RX_CONFIG,
		    rxcfg | GEM_MAC_RX_ENABLE);
	}
}

int
gem_ser_mediachange(struct ifnet *ifp)
{
	struct gem_softc *sc = ifp->if_softc;
	u_int s, t;

	if (IFM_TYPE(sc->sc_mii.mii_media.ifm_media) != IFM_ETHER)
		return EINVAL;

	s = IFM_SUBTYPE(sc->sc_mii.mii_media.ifm_media);
	if (s == IFM_AUTO) {
		if (sc->sc_mii_media != s) {
#ifdef GEM_DEBUG
			aprint_debug_dev(sc->sc_dev, "setting media to auto\n");
#endif
			sc->sc_mii_media = s;
			if (ifp->if_flags & IFF_UP) {
				gem_pcs_stop(sc, 0);
				gem_pcs_start(sc);
			}
		}
		return 0;
	}
	if (s == IFM_1000_SX) {
		t = IFM_OPTIONS(sc->sc_mii.mii_media.ifm_media);
		if (t == IFM_FDX || t == IFM_HDX) {
			if (sc->sc_mii_media != t) {
				sc->sc_mii_media = t;
#ifdef GEM_DEBUG
				aprint_debug_dev(sc->sc_dev,
				    "setting media to 1000baseSX-%s\n",
				    t == IFM_FDX ? "FDX" : "HDX");
#endif
				if (ifp->if_flags & IFF_UP) {
					gem_pcs_stop(sc, 0);
					gem_pcs_start(sc);
				}
			}
			return 0;
		}
	}
	return EINVAL;
}

void
gem_ser_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct gem_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

static int
gem_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct gem_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	else if ((change & IFF_PROMISC) != 0)
		gem_setladrf(sc);
	return 0;
}

/*
 * Process an ioctl request.
 */
int
gem_ioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct gem_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		error = 0;
		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			gem_setladrf(sc);
		}
	}

	/* Try to get things going again */
	if (ifp->if_flags & IFF_UP)
		gem_start(ifp);
	splx(s);
	return (error);
}

static void
gem_inten(struct gem_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	uint32_t v;

	if ((sc->sc_flags & (GEM_SERDES | GEM_SERIAL)) != 0)
		v = GEM_INTR_PCS;
	else
		v = GEM_INTR_MIF;
	bus_space_write_4(t, h, GEM_INTMASK,
		      ~(GEM_INTR_TX_INTME |
			GEM_INTR_TX_EMPTY |
			GEM_INTR_TX_MAC |
			GEM_INTR_RX_DONE | GEM_INTR_RX_NOBUF|
			GEM_INTR_RX_TAG_ERR | GEM_INTR_MAC_CONTROL|
			GEM_INTR_BERR | v));
}

bool
gem_resume(device_t self, const pmf_qual_t *qual)
{
	struct gem_softc *sc = device_private(self);

	gem_inten(sc);

	return true;
}

bool
gem_suspend(device_t self, const pmf_qual_t *qual)
{
	struct gem_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;

	bus_space_write_4(t, h, GEM_INTMASK, ~(uint32_t)0);

	return true;
}

bool
gem_shutdown(device_t self, int howto)
{
	struct gem_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	gem_stop(ifp, 1);

	return true;
}

/*
 * Set up the logical address filter.
 */
void
gem_setladrf(struct gem_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &ec->ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h1;
	u_int32_t crc;
	u_int32_t hash[16];
	u_int32_t v;
	int i;

	/* Get current RX configuration */
	v = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);

	/*
	 * Turn off promiscuous mode, promiscuous group mode (all multicast),
	 * and hash filter.  Depending on the case, the right bit will be
	 * enabled.
	 */
	v &= ~(GEM_MAC_RX_PROMISCUOUS|GEM_MAC_RX_HASH_FILTER|
	    GEM_MAC_RX_PROMISC_GRP);

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		/* Turn on promiscuous mode */
		v |= GEM_MAC_RX_PROMISCUOUS;
		ifp->if_flags |= IFF_ALLMULTI;
		goto chipit;
	}

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 8 bits as an
	 * index into the 256 bit logical address filter.  The high order 4
	 * bits selects the word, while the other 4 bits select the bit within
	 * the word (where bit 0 is the MSB).
	 */

	/* Clear hash table */
	memset(hash, 0, sizeof(hash));

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
			 * XXX should use the address filters for this
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			v |= GEM_MAC_RX_PROMISC_GRP;
			goto chipit;
		}

		/* Get the LE CRC32 of the address */
		crc = ether_crc32_le(enm->enm_addrlo, sizeof(enm->enm_addrlo));

		/* Just want the 8 most significant bits. */
		crc >>= 24;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (15 - (crc & 15));

		ETHER_NEXT_MULTI(step, enm);
	}

	v |= GEM_MAC_RX_HASH_FILTER;
	ifp->if_flags &= ~IFF_ALLMULTI;

	/* Now load the hash table into the chip (if we are using it) */
	for (i = 0; i < 16; i++) {
		bus_space_write_4(t, h,
		    GEM_MAC_HASH0 + i * (GEM_MAC_HASH1-GEM_MAC_HASH0),
		    hash[i]);
	}

chipit:
	sc->sc_if_flags = ifp->if_flags;
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, v);
}
