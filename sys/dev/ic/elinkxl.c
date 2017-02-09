/*	$NetBSD: elinkxl.c,v 1.117 2015/04/13 16:33:24 riastradh Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: elinkxl.c,v 1.117 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/miivar.h>
#include <dev/mii/mii.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/elink3reg.h>
/* #include <dev/ic/elink3var.h> */
#include <dev/ic/elinkxlreg.h>
#include <dev/ic/elinkxlvar.h>

#ifdef DEBUG
int exdebug = 0;
#endif

/* ifmedia callbacks */
int ex_media_chg(struct ifnet *ifp);
void ex_media_stat(struct ifnet *ifp, struct ifmediareq *req);

static int ex_ifflags_cb(struct ethercom *);

void ex_probe_media(struct ex_softc *);
void ex_set_filter(struct ex_softc *);
void ex_set_media(struct ex_softc *);
void ex_set_xcvr(struct ex_softc *, uint16_t);
struct mbuf *ex_get(struct ex_softc *, int);
uint16_t ex_read_eeprom(struct ex_softc *, int);
int ex_init(struct ifnet *);
void ex_read(struct ex_softc *);
void ex_reset(struct ex_softc *);
void ex_set_mc(struct ex_softc *);
void ex_getstats(struct ex_softc *);
void ex_printstats(struct ex_softc *);
void ex_tick(void *);

static int ex_eeprom_busy(struct ex_softc *);
static int ex_add_rxbuf(struct ex_softc *, struct ex_rxdesc *);
static void ex_init_txdescs(struct ex_softc *);

static void ex_setup_tx(struct ex_softc *);
static bool ex_shutdown(device_t, int);
static void ex_start(struct ifnet *);
static void ex_txstat(struct ex_softc *);

int ex_mii_readreg(device_t, int, int);
void ex_mii_writereg(device_t, int, int, int);
void ex_mii_statchg(struct ifnet *);

void ex_probemedia(struct ex_softc *);

/*
 * Structure to map media-present bits in boards to ifmedia codes and
 * printable media names.  Used for table-driven ifmedia initialization.
 */
struct ex_media {
	int	exm_mpbit;		/* media present bit */
	const char *exm_name;		/* name of medium */
	int	exm_ifmedia;		/* ifmedia word for medium */
	int	exm_epmedia;		/* ELINKMEDIA_* constant */
};

/*
 * Media table for 3c90x chips.  Note that chips with MII have no
 * `native' media.
 */
struct ex_media ex_native_media[] = {
	{ ELINK_PCI_10BASE_T,	"10baseT",	IFM_ETHER|IFM_10_T,
	  ELINKMEDIA_10BASE_T },
	{ ELINK_PCI_10BASE_T,	"10baseT-FDX",	IFM_ETHER|IFM_10_T|IFM_FDX,
	  ELINKMEDIA_10BASE_T },
	{ ELINK_PCI_AUI,	"10base5",	IFM_ETHER|IFM_10_5,
	  ELINKMEDIA_AUI },
	{ ELINK_PCI_BNC,	"10base2",	IFM_ETHER|IFM_10_2,
	  ELINKMEDIA_10BASE_2 },
	{ ELINK_PCI_100BASE_TX,	"100baseTX",	IFM_ETHER|IFM_100_TX,
	  ELINKMEDIA_100BASE_TX },
	{ ELINK_PCI_100BASE_TX,	"100baseTX-FDX",IFM_ETHER|IFM_100_TX|IFM_FDX,
	  ELINKMEDIA_100BASE_TX },
	{ ELINK_PCI_100BASE_FX,	"100baseFX",	IFM_ETHER|IFM_100_FX,
	  ELINKMEDIA_100BASE_FX },
	{ ELINK_PCI_100BASE_MII,"manual",	IFM_ETHER|IFM_MANUAL,
	  ELINKMEDIA_MII },
	{ ELINK_PCI_100BASE_T4,	"100baseT4",	IFM_ETHER|IFM_100_T4,
	  ELINKMEDIA_100BASE_T4 },
	{ 0,			NULL,		0,
	  0 },
};

/*
 * MII bit-bang glue.
 */
uint32_t ex_mii_bitbang_read(device_t);
void ex_mii_bitbang_write(device_t, uint32_t);

const struct mii_bitbang_ops ex_mii_bitbang_ops = {
	ex_mii_bitbang_read,
	ex_mii_bitbang_write,
	{
		ELINK_PHY_DATA,		/* MII_BIT_MDO */
		ELINK_PHY_DATA,		/* MII_BIT_MDI */
		ELINK_PHY_CLK,		/* MII_BIT_MDC */
		ELINK_PHY_DIR,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

/*
 * Back-end attach and configure.
 */
void
ex_config(struct ex_softc *sc)
{
	struct ifnet *ifp;
	uint16_t val;
	uint8_t macaddr[ETHER_ADDR_LEN] = {0};
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, error, attach_stage;

	pmf_self_suspensor_init(sc->sc_dev, &sc->sc_suspensor, &sc->sc_qual);

	callout_init(&sc->ex_mii_callout, 0);

	ex_reset(sc);

	val = ex_read_eeprom(sc, EEPROM_OEM_ADDR0);
	macaddr[0] = val >> 8;
	macaddr[1] = val & 0xff;
	val = ex_read_eeprom(sc, EEPROM_OEM_ADDR1);
	macaddr[2] = val >> 8;
	macaddr[3] = val & 0xff;
	val = ex_read_eeprom(sc, EEPROM_OEM_ADDR2);
	macaddr[4] = val >> 8;
	macaddr[5] = val & 0xff;

	aprint_normal_dev(sc->sc_dev, "MAC address %s\n", ether_sprintf(macaddr));

	if (sc->ex_conf & (EX_CONF_INV_LED_POLARITY|EX_CONF_PHY_POWER)) {
		GO_WINDOW(2);
		val = bus_space_read_2(iot, ioh, ELINK_W2_RESET_OPTIONS);
		if (sc->ex_conf & EX_CONF_INV_LED_POLARITY)
			val |= ELINK_RESET_OPT_LEDPOLAR;
		if (sc->ex_conf & EX_CONF_PHY_POWER)
			val |= ELINK_RESET_OPT_PHYPOWER;
		bus_space_write_2(iot, ioh, ELINK_W2_RESET_OPTIONS, val);
	}
	if (sc->ex_conf & EX_CONF_NO_XCVR_PWR) {
		GO_WINDOW(0);
		bus_space_write_2(iot, ioh, ELINK_W0_MFG_ID,
		    EX_XCVR_PWR_MAGICBITS);
	}

	attach_stage = 0;

	/*
	 * Allocate the upload descriptors, and create and load the DMA
	 * map for them.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    EX_NUPD * sizeof (struct ex_upd), PAGE_SIZE, 0, &sc->sc_useg, 1,
            &sc->sc_urseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't allocate upload descriptors, error = %d\n", error);
		goto fail;
	}

	attach_stage = 1;

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_useg, sc->sc_urseg,
	    EX_NUPD * sizeof (struct ex_upd), (void **)&sc->sc_upd,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't map upload descriptors, error = %d\n", error);
		goto fail;
	}

	attach_stage = 2;

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    EX_NUPD * sizeof (struct ex_upd), 1,
	    EX_NUPD * sizeof (struct ex_upd), 0, BUS_DMA_NOWAIT,
	    &sc->sc_upd_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't create upload desc. DMA map, error = %d\n", error);
		goto fail;
	}

	attach_stage = 3;

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_upd_dmamap,
	    sc->sc_upd, EX_NUPD * sizeof (struct ex_upd), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't load upload desc. DMA map, error = %d\n", error);
		goto fail;
	}

	attach_stage = 4;

	/*
	 * Allocate the download descriptors, and create and load the DMA
	 * map for them.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN, PAGE_SIZE, 0, &sc->sc_dseg, 1,
	    &sc->sc_drseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't allocate download descriptors, error = %d\n", error);
		goto fail;
	}

	attach_stage = 5;

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dseg, sc->sc_drseg,
	    DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN, (void **)&sc->sc_dpd,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't map download descriptors, error = %d\n", error);
		goto fail;
	}
	memset(sc->sc_dpd, 0, DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN);

	attach_stage = 6;

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN, 1,
	    DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN, 0, BUS_DMA_NOWAIT,
	    &sc->sc_dpd_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't create download desc. DMA map, error = %d\n", error);
		goto fail;
	}

	attach_stage = 7;

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dpd_dmamap,
	    sc->sc_dpd, DPDMEM_SIZE + EX_IP4CSUMTX_PADLEN, NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't load download desc. DMA map, error = %d\n", error);
		goto fail;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
	    DPDMEMPAD_OFF, EX_IP4CSUMTX_PADLEN, BUS_DMASYNC_PREWRITE);

	attach_stage = 8;


	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < EX_NDPD; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    EX_NTFRAGS, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->sc_tx_dmamaps[i])) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't create tx DMA map %d, error = %d\n",
			    i, error);
			goto fail;
		}
	}

	attach_stage = 9;

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < EX_NUPD; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    EX_NRFRAGS, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->sc_rx_dmamaps[i])) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't create rx DMA map %d, error = %d\n",
			    i, error);
			goto fail;
		}
	}

	attach_stage = 10;

	/*
	 * Create ring of upload descriptors, only once. The DMA engine
	 * will loop over this when receiving packets, stalling if it
	 * hits an UPD with a finished receive.
	 */
	for (i = 0; i < EX_NUPD; i++) {
		sc->sc_rxdescs[i].rx_dmamap = sc->sc_rx_dmamaps[i];
		sc->sc_rxdescs[i].rx_upd = &sc->sc_upd[i];
		sc->sc_upd[i].upd_frags[0].fr_len =
		    htole32((MCLBYTES - 2) | EX_FR_LAST);
		if (ex_add_rxbuf(sc, &sc->sc_rxdescs[i]) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't allocate or map rx buffers\n");
			goto fail;
		}
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_upd_dmamap, 0,
	    EX_NUPD * sizeof (struct ex_upd),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	ex_init_txdescs(sc);

	attach_stage = 11;


	GO_WINDOW(3);
	val = bus_space_read_2(iot, ioh, ELINK_W3_RESET_OPTIONS);
	if (val & ELINK_MEDIACAP_MII)
		sc->ex_conf |= EX_CONF_MII;

	ifp = &sc->sc_ethercom.ec_if;

	/*
	 * Initialize our media structures and MII info.  We'll
	 * probe the MII if we discover that we have one.
	 */
	sc->ex_mii.mii_ifp = ifp;
	sc->ex_mii.mii_readreg = ex_mii_readreg;
	sc->ex_mii.mii_writereg = ex_mii_writereg;
	sc->ex_mii.mii_statchg = ex_mii_statchg;
	ifmedia_init(&sc->ex_mii.mii_media, IFM_IMASK, ex_media_chg,
	    ex_media_stat);

	if (sc->ex_conf & EX_CONF_MII) {
		/*
		 * Find PHY, extract media information from it.
		 * First, select the right transceiver.
		 */
		ex_set_xcvr(sc, val);

		mii_attach(sc->sc_dev, &sc->ex_mii, 0xffffffff,
		    MII_PHY_ANY, MII_OFFSET_ANY, 0);
		if (LIST_FIRST(&sc->ex_mii.mii_phys) == NULL) {
			ifmedia_add(&sc->ex_mii.mii_media, IFM_ETHER|IFM_NONE,
			    0, NULL);
			ifmedia_set(&sc->ex_mii.mii_media, IFM_ETHER|IFM_NONE);
		} else {
			ifmedia_set(&sc->ex_mii.mii_media, IFM_ETHER|IFM_AUTO);
		}
	} else
		ex_probemedia(sc);

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = ex_start;
	ifp->if_ioctl = ex_ioctl;
	ifp->if_watchdog = ex_watchdog;
	ifp->if_init = ex_init;
	ifp->if_stop = ex_stop;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	sc->sc_if_flags = ifp->if_flags;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * The 3c90xB has hardware IPv4/TCPv4/UDPv4 checksum support.
	 */
	if (sc->ex_conf & EX_CONF_90XB)
		sc->sc_ethercom.ec_if.if_capabilities |=
		    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;

	if_attach(ifp);
	ether_ifattach(ifp, macaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, ex_ifflags_cb);

	GO_WINDOW(1);

	sc->tx_start_thresh = 20;
	sc->tx_succ_ok = 0;

	/* TODO: set queues to 0 */

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (pmf_device_register1(sc->sc_dev, NULL, NULL, ex_shutdown))
		pmf_class_network_register(sc->sc_dev, &sc->sc_ethercom.ec_if);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	/* The attach is successful. */
	sc->ex_flags |= EX_FLAGS_ATTACHED;
	return;

 fail:
	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall though.
	 */
	switch (attach_stage) {
	case 11:
	    {
		struct ex_rxdesc *rxd;

		for (i = 0; i < EX_NUPD; i++) {
			rxd = &sc->sc_rxdescs[i];
			if (rxd->rx_mbhead != NULL) {
				bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
				m_freem(rxd->rx_mbhead);
			}
		}
	    }
		/* FALLTHROUGH */

	case 10:
		for (i = 0; i < EX_NUPD; i++)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_dmamaps[i]);
		/* FALLTHROUGH */

	case 9:
		for (i = 0; i < EX_NDPD; i++)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_dmamaps[i]);
		/* FALLTHROUGH */
	case 8:
		bus_dmamap_unload(sc->sc_dmat, sc->sc_dpd_dmamap);
		/* FALLTHROUGH */

	case 7:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dpd_dmamap);
		/* FALLTHROUGH */

	case 6:
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_dpd,
		    EX_NDPD * sizeof (struct ex_dpd));
		/* FALLTHROUGH */

	case 5:
		bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_drseg);
		break;

	case 4:
		bus_dmamap_unload(sc->sc_dmat, sc->sc_upd_dmamap);
		/* FALLTHROUGH */

	case 3:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_upd_dmamap);
		/* FALLTHROUGH */

	case 2:
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_upd,
		    EX_NUPD * sizeof (struct ex_upd));
		/* FALLTHROUGH */

	case 1:
		bus_dmamem_free(sc->sc_dmat, &sc->sc_useg, sc->sc_urseg);
		break;
	}

}

/*
 * Find the media present on non-MII chips.
 */
void
ex_probemedia(struct ex_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifmedia *ifm = &sc->ex_mii.mii_media;
	struct ex_media *exm;
	uint16_t config1, reset_options, default_media;
	int defmedia = 0;
	const char *sep = "", *defmedianame = NULL;

	GO_WINDOW(3);
	config1 = bus_space_read_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG + 2);
	reset_options = bus_space_read_1(iot, ioh, ELINK_W3_RESET_OPTIONS);
	GO_WINDOW(0);

	default_media = (config1 & CONFIG_MEDIAMASK) >> CONFIG_MEDIAMASK_SHIFT;

	/* Sanity check that there are any media! */
	if ((reset_options & ELINK_PCI_MEDIAMASK) == 0) {
		aprint_error_dev(sc->sc_dev, "no media present!\n");
		ifmedia_add(ifm, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(ifm, IFM_ETHER|IFM_NONE);
		return;
	}

	aprint_normal_dev(sc->sc_dev, "");

#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	for (exm = ex_native_media; exm->exm_name != NULL; exm++) {
		if (reset_options & exm->exm_mpbit) {
			/*
			 * Default media is a little complicated.  We
			 * support full-duplex which uses the same
			 * reset options bit.
			 *
			 * XXX Check EEPROM for default to FDX?
			 */
			if (exm->exm_epmedia == default_media) {
				if ((exm->exm_ifmedia & IFM_FDX) == 0) {
					defmedia = exm->exm_ifmedia;
					defmedianame = exm->exm_name;
				}
			} else if (defmedia == 0) {
				defmedia = exm->exm_ifmedia;
				defmedianame = exm->exm_name;
			}
			ifmedia_add(ifm, exm->exm_ifmedia, exm->exm_epmedia,
			    NULL);
			PRINT(exm->exm_name);
		}
	}

#undef PRINT

#ifdef DIAGNOSTIC
	if (defmedia == 0)
		panic("ex_probemedia: impossible");
#endif

	aprint_normal(", default %s\n", defmedianame);
	ifmedia_set(ifm, defmedia);
}

/*
 * Setup transmitter parameters.
 */
static void
ex_setup_tx(struct ex_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/*
	 * Disable reclaim threshold for 90xB, set free threshold to
	 * 6 * 256 = 1536 for 90x.
	 */
	if (sc->ex_conf & EX_CONF_90XB)
		bus_space_write_2(iot, ioh, ELINK_COMMAND,
		    ELINK_TXRECLTHRESH | 255);
	else
		bus_space_write_1(iot, ioh, ELINK_TXFREETHRESH, 6);

	/* Setup early transmission start threshold. */
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    ELINK_TXSTARTTHRESH | sc->tx_start_thresh);
}

/*
 * Bring device up.
 */
int
ex_init(struct ifnet *ifp)
{
	struct ex_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;
	uint16_t val;
	int error = 0;

	if ((error = ex_enable(sc)) != 0)
		goto out;

	ex_waitcmd(sc);
	ex_stop(ifp, 0);

	GO_WINDOW(2);

	/* Turn on PHY power. */
	if (sc->ex_conf & (EX_CONF_PHY_POWER | EX_CONF_INV_LED_POLARITY)) {
		val = bus_space_read_2(iot, ioh, ELINK_W2_RESET_OPTIONS);
		if (sc->ex_conf & EX_CONF_PHY_POWER)
			val |= ELINK_RESET_OPT_PHYPOWER; /* turn on PHY power */
		if (sc->ex_conf & EX_CONF_INV_LED_POLARITY)
			val |= ELINK_RESET_OPT_LEDPOLAR; /* invert LED polarity */
		bus_space_write_2(iot, ioh, ELINK_W2_RESET_OPTIONS, val);
	}

	/*
	 * Set the station address and clear the station mask. The latter
	 * is needed for 90x cards, 0 is the default for 90xB cards.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		bus_space_write_1(iot, ioh, ELINK_W2_ADDR_0 + i,
		    CLLADDR(ifp->if_sadl)[i]);
		bus_space_write_1(iot, ioh, ELINK_W2_RECVMASK_0 + i, 0);
	}

	GO_WINDOW(3);

	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_RESET);
	ex_waitcmd(sc);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_RESET);
	ex_waitcmd(sc);

	/* Load Tx parameters. */
	ex_setup_tx(sc);

	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_RX_EARLY_THRESH | ELINK_THRESH_DISABLE);

	bus_space_write_4(iot, ioh, ELINK_DMACTRL,
	    bus_space_read_4(iot, ioh, ELINK_DMACTRL) | ELINK_DMAC_UPRXEAREN);

	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_RD_0_MASK | XL_WATCHED_INTERRUPTS);
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_INTR_MASK | XL_WATCHED_INTERRUPTS);

	bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR | 0xff);
	if (sc->intr_ack)
	    (* sc->intr_ack)(sc);
	ex_set_media(sc);
	ex_set_mc(sc);


	bus_space_write_2(iot, ioh, ELINK_COMMAND, STATS_ENABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_ENABLE);
	bus_space_write_4(iot, ioh, ELINK_UPLISTPTR, sc->sc_upddma);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_ENABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, ELINK_UPUNSTALL);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ex_start(ifp);
	sc->sc_if_flags = ifp->if_flags;

	GO_WINDOW(1);

	callout_reset(&sc->ex_mii_callout, hz, ex_tick, sc);

 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		aprint_error_dev(sc->sc_dev, "interface not running\n");
	}
	return (error);
}

#define	MCHASHSIZE		256
#define	ex_mchash(addr)		(ether_crc32_be((addr), ETHER_ADDR_LEN) & \
				    (MCHASHSIZE - 1))

/*
 * Set multicast receive filter. Also take care of promiscuous mode
 * here (XXX).
 */
void
ex_set_mc(struct ex_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ethercom *ec = &sc->sc_ethercom;
	struct ether_multi *enm;
	struct ether_multistep estep;
	int i;
	uint16_t mask = FIL_INDIVIDUAL | FIL_BRDCST;

	if (ifp->if_flags & IFF_PROMISC) {
		mask |= FIL_PROMISC;
		goto allmulti;
	}

	ETHER_FIRST_MULTI(estep, ec, enm);
	if (enm == NULL)
		goto nomulti;

	if ((sc->ex_conf & EX_CONF_90XB) == 0)
		/* No multicast hash filtering. */
		goto allmulti;

	for (i = 0; i < MCHASHSIZE; i++)
		bus_space_write_2(sc->sc_iot, sc->sc_ioh,
		    ELINK_COMMAND, ELINK_CLEARHASHFILBIT | i);

	do {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		i = ex_mchash(enm->enm_addrlo);
		bus_space_write_2(sc->sc_iot, sc->sc_ioh,
		    ELINK_COMMAND, ELINK_SETHASHFILBIT | i);
		ETHER_NEXT_MULTI(estep, enm);
	} while (enm != NULL);
	mask |= FIL_MULTIHASH;

nomulti:
	ifp->if_flags &= ~IFF_ALLMULTI;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ELINK_COMMAND,
	    SET_RX_FILTER | mask);
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	mask |= FIL_MULTICAST;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ELINK_COMMAND,
	    SET_RX_FILTER | mask);
}


/*
 * The Tx Complete interrupts occur only on errors,
 * and this is the error handler.
 */
static void
ex_txstat(struct ex_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, err = 0;

	/*
	 * We need to read+write TX_STATUS until we get a 0 status
	 * in order to turn off the interrupt flag.
	 * ELINK_TXSTATUS is in the upper byte of 2 with ELINK_TIMER.
	 */
	for (;;) {
		i = bus_space_read_2(iot, ioh, ELINK_TIMER);
		if ((i & TXS_COMPLETE) == 0)
			break;
		bus_space_write_2(iot, ioh, ELINK_TIMER, 0x0);
		err |= i;
	}
	err &= ~TXS_TIMER;

	if ((err & (TXS_UNDERRUN | TXS_JABBER | TXS_RECLAIM))
	    || err == 0 /* should not happen, just in case */) {
		/*
		 * Make sure the transmission is stopped.
		 */
		bus_space_write_2(iot, ioh, ELINK_COMMAND, ELINK_DNSTALL);
		for (i = 1000; i > 0; i--)
			if ((bus_space_read_4(iot, ioh, ELINK_DMACTRL) &
			    ELINK_DMAC_DNINPROG) == 0)
				break;

		/*
		 * Reset the transmitter.
		 */
		bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_RESET);

		/* Resetting takes a while and we will do more than wait. */

		ifp->if_flags &= ~IFF_OACTIVE;
		++sc->sc_ethercom.ec_if.if_oerrors;
		aprint_error_dev(sc->sc_dev, "%s%s%s",
		    (err & TXS_UNDERRUN) ? " transmit underrun" : "",
		    (err & TXS_JABBER) ? " jabber" : "",
		    (err & TXS_RECLAIM) ? " reclaim" : "");
		if (err == 0)
			aprint_error(" unknown Tx error");
		printf(" (%x)", err);
		if (err & TXS_UNDERRUN) {
			aprint_error(" @%d", sc->tx_start_thresh);
			if (sc->tx_succ_ok < 256 &&
			    (i = min(ETHER_MAX_LEN, sc->tx_start_thresh + 20))
			    > sc->tx_start_thresh) {
				aprint_error(", new threshold is %d", i);
				sc->tx_start_thresh = i;
			}
			sc->tx_succ_ok = 0;
		}
		aprint_error("\n");
		if (err & TXS_MAX_COLLISION)
			++sc->sc_ethercom.ec_if.if_collisions;

		/* Wait for TX_RESET to finish. */
		ex_waitcmd(sc);

		/* Reload Tx parameters. */
		ex_setup_tx(sc);
	} else {
		if (err & TXS_MAX_COLLISION)
			++sc->sc_ethercom.ec_if.if_collisions;
		sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
	}

	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_ENABLE);

	/* Retransmit current packet if any. */
	if (sc->tx_head) {
		ifp->if_flags |= IFF_OACTIVE;
		bus_space_write_2(iot, ioh, ELINK_COMMAND,
		    ELINK_DNUNSTALL);
		bus_space_write_4(iot, ioh, ELINK_DNLISTPTR,
		    DPD_DMADDR(sc, sc->tx_head));

		/* Retrigger watchdog if stopped. */
		if (ifp->if_timer == 0)
			ifp->if_timer = 1;
	}
}

int
ex_media_chg(struct ifnet *ifp)
{

	if (ifp->if_flags & IFF_UP)
		ex_init(ifp);
	return 0;
}

void
ex_set_xcvr(struct ex_softc *sc, const uint16_t media)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint32_t icfg;

	/*
	 * We're already in Window 3
	 */
	icfg = bus_space_read_4(iot, ioh, ELINK_W3_INTERNAL_CONFIG);
	icfg &= ~(CONFIG_XCVR_SEL << 16);
	if (media & (ELINK_MEDIACAP_MII | ELINK_MEDIACAP_100BASET4))
		icfg |= ELINKMEDIA_MII << (CONFIG_XCVR_SEL_SHIFT + 16);
	if (media & ELINK_MEDIACAP_100BASETX)
		icfg |= ELINKMEDIA_AUTO << (CONFIG_XCVR_SEL_SHIFT + 16);
	if (media & ELINK_MEDIACAP_100BASEFX)
		icfg |= ELINKMEDIA_100BASE_FX
			<< (CONFIG_XCVR_SEL_SHIFT + 16);
	bus_space_write_4(iot, ioh, ELINK_W3_INTERNAL_CONFIG, icfg);
}

void
ex_set_media(struct ex_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint32_t configreg;

	if (((sc->ex_conf & EX_CONF_MII) &&
	    (sc->ex_mii.mii_media_active & IFM_FDX))
	    || (!(sc->ex_conf & EX_CONF_MII) &&
	    (sc->ex_mii.mii_media.ifm_media & IFM_FDX))) {
		bus_space_write_2(iot, ioh, ELINK_W3_MAC_CONTROL,
		    MAC_CONTROL_FDX);
	} else {
		bus_space_write_2(iot, ioh, ELINK_W3_MAC_CONTROL, 0);
	}

	/*
	 * If the device has MII, select it, and then tell the
	 * PHY which media to use.
	 */
	if (sc->ex_conf & EX_CONF_MII) {
		uint16_t val;

		GO_WINDOW(3);
		val = bus_space_read_2(iot, ioh, ELINK_W3_RESET_OPTIONS);
		ex_set_xcvr(sc, val);
		mii_mediachg(&sc->ex_mii);
		return;
	}

	GO_WINDOW(4);
	bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE, 0);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, STOP_TRANSCEIVER);
	delay(800);

	/*
	 * Now turn on the selected media/transceiver.
	 */
	switch (IFM_SUBTYPE(sc->ex_mii.mii_media.ifm_cur->ifm_media)) {
	case IFM_10_T:
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    JABBER_GUARD_ENABLE|LINKBEAT_ENABLE);
		break;

	case IFM_10_2:
		bus_space_write_2(iot, ioh, ELINK_COMMAND, START_TRANSCEIVER);
		DELAY(800);
		break;

	case IFM_100_TX:
	case IFM_100_FX:
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    LINKBEAT_ENABLE);
		DELAY(800);
		break;

	case IFM_10_5:
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    SQE_ENABLE);
		DELAY(800);
		break;

	case IFM_MANUAL:
		break;

	case IFM_NONE:
		return;

	default:
		panic("ex_set_media: impossible");
	}

	GO_WINDOW(3);
	configreg = bus_space_read_4(iot, ioh, ELINK_W3_INTERNAL_CONFIG);

	configreg &= ~(CONFIG_MEDIAMASK << 16);
	configreg |= (sc->ex_mii.mii_media.ifm_cur->ifm_data <<
	    (CONFIG_MEDIAMASK_SHIFT + 16));

	bus_space_write_4(iot, ioh, ELINK_W3_INTERNAL_CONFIG, configreg);
}

/*
 * Get currently-selected media from card.
 * (if_media callback, may be called before interface is brought up).
 */
void
ex_media_stat(struct ifnet *ifp, struct ifmediareq *req)
{
	struct ex_softc *sc = ifp->if_softc;
	uint16_t help;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING)) {
		if (sc->ex_conf & EX_CONF_MII) {
			mii_pollstat(&sc->ex_mii);
			req->ifm_status = sc->ex_mii.mii_media_status;
			req->ifm_active = sc->ex_mii.mii_media_active;
		} else {
			GO_WINDOW(4);
			req->ifm_status = IFM_AVALID;
			req->ifm_active =
			    sc->ex_mii.mii_media.ifm_cur->ifm_media;
			help = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
						ELINK_W4_MEDIA_TYPE);
			if (help & LINKBEAT_DETECT)
				req->ifm_status |= IFM_ACTIVE;
			GO_WINDOW(1);
		}
	}
}



/*
 * Start outputting on the interface.
 */
static void
ex_start(struct ifnet *ifp)
{
	struct ex_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	volatile struct ex_fraghdr *fr = NULL;
	volatile struct ex_dpd *dpd = NULL, *prevdpd = NULL;
	struct ex_txdesc *txp;
	struct mbuf *mb_head;
	bus_dmamap_t dmamap;
	int m_csumflags, offset, seglen, totlen, segment, error;
	uint32_t csum_flags;

	if (sc->tx_head || sc->tx_free == NULL)
		return;

	txp = NULL;

	/*
	 * We're finished if there is nothing more to add to the list or if
	 * we're all filled up with buffers to transmit.
	 */
	while (sc->tx_free != NULL) {
		/*
		 * Grab a packet to transmit.
		 */
		IFQ_DEQUEUE(&ifp->if_snd, mb_head);
		if (mb_head == NULL)
			break;

		/*
		 * mb_head might be updated later,
		 * so preserve csum_flags here.
		 */
		m_csumflags = mb_head->m_pkthdr.csum_flags;

		/*
		 * Get pointer to next available tx desc.
		 */
		txp = sc->tx_free;
		dmamap = txp->tx_dmamap;

		/*
		 * Go through each of the mbufs in the chain and initialize
		 * the transmit buffer descriptors with the physical address
		 * and size of the mbuf.
		 */
 reload:
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
		    mb_head, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		switch (error) {
		case 0:
			/* Success. */
			break;

		case EFBIG:
		    {
			struct mbuf *mn;

			/*
			 * We ran out of segments.  We have to recopy this
			 * mbuf chain first.  Bail out if we can't get the
			 * new buffers.
			 */
			aprint_error_dev(sc->sc_dev, "too many segments, ");

			MGETHDR(mn, M_DONTWAIT, MT_DATA);
			if (mn == NULL) {
				m_freem(mb_head);
				aprint_error("aborting\n");
				goto out;
			}
			if (mb_head->m_pkthdr.len > MHLEN) {
				MCLGET(mn, M_DONTWAIT);
				if ((mn->m_flags & M_EXT) == 0) {
					m_freem(mn);
					m_freem(mb_head);
					aprint_error("aborting\n");
					goto out;
				}
			}
			m_copydata(mb_head, 0, mb_head->m_pkthdr.len,
			    mtod(mn, void *));
			mn->m_pkthdr.len = mn->m_len = mb_head->m_pkthdr.len;
			m_freem(mb_head);
			mb_head = mn;
			aprint_error("retrying\n");
			goto reload;
		    }

		default:
			/*
			 * Some other problem; report it.
			 */
			aprint_error_dev(sc->sc_dev,
			    "can't load mbuf chain, error = %d\n", error);
			m_freem(mb_head);
			goto out;
		}

		/*
		 * remove our tx desc from freelist.
		 */
		sc->tx_free = txp->tx_next;
		txp->tx_next = NULL;

		fr = &txp->tx_dpd->dpd_frags[0];
		totlen = 0;
		for (segment = 0; segment < dmamap->dm_nsegs; segment++, fr++) {
			fr->fr_addr = htole32(dmamap->dm_segs[segment].ds_addr);
			seglen = dmamap->dm_segs[segment].ds_len;
			fr->fr_len = htole32(seglen);
			totlen += seglen;
		}
		if (__predict_false(totlen <= EX_IP4CSUMTX_PADLEN &&
		    (m_csumflags & M_CSUM_IPv4) != 0)) {
			/*
			 * Pad short packets to avoid ip4csum-tx bug.
			 *
			 * XXX Should we still consider if such short
			 *     (36 bytes or less) packets might already
			 *     occupy EX_NTFRAG (== 32) fragments here?
			 */
			KASSERT(segment < EX_NTFRAGS);
			fr->fr_addr = htole32(DPDMEMPAD_DMADDR(sc));
			seglen = EX_IP4CSUMTX_PADLEN + 1 - totlen;
			fr->fr_len = htole32(EX_FR_LAST | seglen);
			totlen += seglen;
		} else {
			fr--;
			fr->fr_len |= htole32(EX_FR_LAST);
		}
		txp->tx_mbhead = mb_head;

		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		dpd = txp->tx_dpd;
		dpd->dpd_nextptr = 0;
		dpd->dpd_fsh = htole32(totlen);

		/* Byte-swap constants so compiler can optimize. */

		if (sc->ex_conf & EX_CONF_90XB) {
			csum_flags = 0;

			if (m_csumflags & M_CSUM_IPv4)
				csum_flags |= htole32(EX_DPD_IPCKSUM);

			if (m_csumflags & M_CSUM_TCPv4)
				csum_flags |= htole32(EX_DPD_TCPCKSUM);
			else if (m_csumflags & M_CSUM_UDPv4)
				csum_flags |= htole32(EX_DPD_UDPCKSUM);

			dpd->dpd_fsh |= csum_flags;
		} else {
			KDASSERT((mb_head->m_pkthdr.csum_flags &
			    (M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4)) == 0);
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
		    ((const char *)(intptr_t)dpd - (const char *)sc->sc_dpd),
		    sizeof (struct ex_dpd),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * No need to stall the download engine, we know it's
		 * not busy right now.
		 *
		 * Fix up pointers in both the "soft" tx and the physical
		 * tx list.
		 */
		if (sc->tx_head != NULL) {
			prevdpd = sc->tx_tail->tx_dpd;
			offset = ((const char *)(intptr_t)prevdpd - (const char *)sc->sc_dpd);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
			    offset, sizeof (struct ex_dpd),
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			prevdpd->dpd_nextptr = htole32(DPD_DMADDR(sc, txp));
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
			    offset, sizeof (struct ex_dpd),
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			sc->tx_tail->tx_next = txp;
			sc->tx_tail = txp;
		} else {
			sc->tx_tail = sc->tx_head = txp;
		}

		/*
		 * Pass packet to bpf if there is a listener.
		 */
		bpf_mtap(ifp, mb_head);
	}
 out:
	if (sc->tx_head) {
		sc->tx_tail->tx_dpd->dpd_fsh |= htole32(EX_DPD_DNIND);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
		    ((char *)sc->tx_tail->tx_dpd - (char *)sc->sc_dpd),
		    sizeof (struct ex_dpd),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		ifp->if_flags |= IFF_OACTIVE;
		bus_space_write_2(iot, ioh, ELINK_COMMAND, ELINK_DNUNSTALL);
		bus_space_write_4(iot, ioh, ELINK_DNLISTPTR,
		    DPD_DMADDR(sc, sc->tx_head));

		/* trigger watchdog */
		ifp->if_timer = 5;
	}
}


int
ex_intr(void *arg)
{
	struct ex_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint16_t stat;
	int ret = 0;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_is_active(sc->sc_dev))
		return (0);

	for (;;) {
		stat = bus_space_read_2(iot, ioh, ELINK_STATUS);

		if ((stat & XL_WATCHED_INTERRUPTS) == 0) {
			if ((stat & INTR_LATCH) == 0) {
#if 0
				aprint_error_dev(sc->sc_dev,
				       "intr latch cleared\n");
#endif
				break;
			}
		}

		ret = 1;

		/*
		 * Acknowledge interrupts.
		 */
		bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR |
		    (stat & (XL_WATCHED_INTERRUPTS | INTR_LATCH)));
		if (sc->intr_ack)
			(*sc->intr_ack)(sc);

		if (stat & HOST_ERROR) {
			aprint_error_dev(sc->sc_dev,
			    "adapter failure (%x)\n", stat);
			ex_reset(sc);
			ex_init(ifp);
			return 1;
		}
		if (stat & UPD_STATS) {
			ex_getstats(sc);
		}
		if (stat & TX_COMPLETE) {
			ex_txstat(sc);
#if 0
			if (stat & DN_COMPLETE)
				aprint_error_dev(sc->sc_dev,
				    "Ignoring Dn interrupt (%x)\n", stat);
#endif
			/*
			 * In some rare cases, both Tx Complete and
			 * Dn Complete bits are set.  However, the packet
			 * has been reloaded in ex_txstat() and should not
			 * handle the Dn Complete event here.
			 * Hence the "else" below.
			 */
		} else if (stat & DN_COMPLETE) {
			struct ex_txdesc *txp, *ptxp = NULL;
			bus_dmamap_t txmap;

			/* reset watchdog timer, was set in ex_start() */
			ifp->if_timer = 0;

			for (txp = sc->tx_head; txp != NULL;
			    txp = txp->tx_next) {
				bus_dmamap_sync(sc->sc_dmat,
				    sc->sc_dpd_dmamap,
				    (char *)txp->tx_dpd - (char *)sc->sc_dpd,
				    sizeof (struct ex_dpd),
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
				if (txp->tx_mbhead != NULL) {
					txmap = txp->tx_dmamap;
					bus_dmamap_sync(sc->sc_dmat, txmap,
					    0, txmap->dm_mapsize,
					    BUS_DMASYNC_POSTWRITE);
					bus_dmamap_unload(sc->sc_dmat, txmap);
					m_freem(txp->tx_mbhead);
					txp->tx_mbhead = NULL;
				}
				ptxp = txp;
			}

			/*
			 * Move finished tx buffers back to the tx free list.
			 */
			if (sc->tx_free) {
				sc->tx_ftail->tx_next = sc->tx_head;
				sc->tx_ftail = ptxp;
			} else
				sc->tx_ftail = sc->tx_free = sc->tx_head;

			sc->tx_head = sc->tx_tail = NULL;
			ifp->if_flags &= ~IFF_OACTIVE;

			if (sc->tx_succ_ok < 256)
				sc->tx_succ_ok++;
		}

		if (stat & UP_COMPLETE) {
			struct ex_rxdesc *rxd;
			struct mbuf *m;
			struct ex_upd *upd;
			bus_dmamap_t rxmap;
			uint32_t pktstat;

 rcvloop:
			rxd = sc->rx_head;
			rxmap = rxd->rx_dmamap;
			m = rxd->rx_mbhead;
			upd = rxd->rx_upd;

			bus_dmamap_sync(sc->sc_dmat, rxmap, 0,
			    rxmap->dm_mapsize,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_upd_dmamap,
			    ((char *)upd - (char *)sc->sc_upd),
			    sizeof (struct ex_upd),
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			pktstat = le32toh(upd->upd_pktstatus);

			if (pktstat & EX_UPD_COMPLETE) {
				/*
				 * Remove first packet from the chain.
				 */
				sc->rx_head = rxd->rx_next;
				rxd->rx_next = NULL;

				/*
				 * Add a new buffer to the receive chain.
				 * If this fails, the old buffer is recycled
				 * instead.
				 */
				if (ex_add_rxbuf(sc, rxd) == 0) {
					uint16_t total_len;

					if (pktstat &
					    ((sc->sc_ethercom.ec_capenable &
					    ETHERCAP_VLAN_MTU) ?
					    EX_UPD_ERR_VLAN : EX_UPD_ERR)) {
						ifp->if_ierrors++;
						m_freem(m);
						goto rcvloop;
					}

					total_len = pktstat & EX_UPD_PKTLENMASK;
					if (total_len <
					    sizeof(struct ether_header)) {
						m_freem(m);
						goto rcvloop;
					}
					m->m_pkthdr.rcvif = ifp;
					m->m_pkthdr.len = m->m_len = total_len;
					bpf_mtap(ifp, m);
		/*
		 * Set the incoming checksum information for the packet.
		 */
		if ((sc->ex_conf & EX_CONF_90XB) != 0 &&
		    (pktstat & EX_UPD_IPCHECKED) != 0) {
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (pktstat & EX_UPD_IPCKSUMERR)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
			if (pktstat & EX_UPD_TCPCHECKED) {
				m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
				if (pktstat & EX_UPD_TCPCKSUMERR)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			} else if (pktstat & EX_UPD_UDPCHECKED) {
				m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
				if (pktstat & EX_UPD_UDPCKSUMERR)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			}
		}
					(*ifp->if_input)(ifp, m);
				}
				goto rcvloop;
			}
			/*
			 * Just in case we filled up all UPDs and the DMA engine
			 * stalled. We could be more subtle about this.
			 */
			if (bus_space_read_4(iot, ioh, ELINK_UPLISTPTR) == 0) {
				aprint_error_dev(sc->sc_dev,
				       "uplistptr was 0\n");
				ex_init(ifp);
			} else if (bus_space_read_4(iot, ioh, ELINK_UPPKTSTATUS)
				   & 0x2000) {
				aprint_error_dev(sc->sc_dev,
				       "receive stalled\n");
				bus_space_write_2(iot, ioh, ELINK_COMMAND,
						  ELINK_UPUNSTALL);
			}
		}

		if (stat)
			rnd_add_uint32(&sc->rnd_source, stat);
	}

	/* no more interrupts */
	if (ret && IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		ex_start(ifp);
	return ret;
}

static int
ex_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct ex_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;
	 
	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	else if ((change & IFF_PROMISC) != 0)
		ex_set_mc(sc);
	return 0;
}

int
ex_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ex_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ex_mii.mii_media, cmd);
		break;
	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			ex_set_mc(sc);
		}
		break;
	}

	sc->sc_if_flags = ifp->if_flags;
	splx(s);
	return (error);
}

void
ex_getstats(struct ex_softc *sc)
{
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_space_tag_t iot = sc->sc_iot;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint8_t upperok;

	GO_WINDOW(6);
	upperok = bus_space_read_1(iot, ioh, UPPER_FRAMES_OK);
	ifp->if_ipackets += bus_space_read_1(iot, ioh, RX_FRAMES_OK);
	ifp->if_ipackets += (upperok & 0x03) << 8;
	ifp->if_opackets += bus_space_read_1(iot, ioh, TX_FRAMES_OK);
	ifp->if_opackets += (upperok & 0x30) << 4;
	ifp->if_ierrors += bus_space_read_1(iot, ioh, RX_OVERRUNS);
	ifp->if_collisions += bus_space_read_1(iot, ioh, TX_COLLISIONS);
	/*
	 * There seems to be no way to get the exact number of collisions,
	 * this is the number that occurred at the very least.
	 */
	ifp->if_collisions += 2 * bus_space_read_1(iot, ioh,
	    TX_AFTER_X_COLLISIONS);
	/*
	 * Interface byte counts are counted by ether_input() and
	 * ether_output(), so don't accumulate them here.  Just
	 * read the NIC counters so they don't generate overflow interrupts.
	 * Upper byte counters are latched from reading the totals, so
	 * they don't need to be read if we don't need their values.
	 */
	(void)bus_space_read_2(iot, ioh, RX_TOTAL_OK);
	(void)bus_space_read_2(iot, ioh, TX_TOTAL_OK);

	/*
	 * Clear the following to avoid stats overflow interrupts
	 */
	(void)bus_space_read_1(iot, ioh, TX_DEFERRALS);
	(void)bus_space_read_1(iot, ioh, TX_AFTER_1_COLLISION);
	(void)bus_space_read_1(iot, ioh, TX_NO_SQE);
	(void)bus_space_read_1(iot, ioh, TX_CD_LOST);
	GO_WINDOW(4);
	(void)bus_space_read_1(iot, ioh, ELINK_W4_BADSSD);
	GO_WINDOW(1);
}

void
ex_printstats(struct ex_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	ex_getstats(sc);
	printf("in %llu out %llu ierror %llu oerror %llu ibytes %llu obytes "
	    "%llu\n", (unsigned long long)ifp->if_ipackets,
	    (unsigned long long)ifp->if_opackets,
	    (unsigned long long)ifp->if_ierrors,
	    (unsigned long long)ifp->if_oerrors,
	    (unsigned long long)ifp->if_ibytes,
	    (unsigned long long)ifp->if_obytes);
}

void
ex_tick(void *arg)
{
	struct ex_softc *sc = arg;
	int s;

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();

	if (sc->ex_conf & EX_CONF_MII)
		mii_tick(&sc->ex_mii);

	if (!(bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, ELINK_STATUS)
	    & COMMAND_IN_PROGRESS))
		ex_getstats(sc);

	splx(s);

	callout_reset(&sc->ex_mii_callout, hz, ex_tick, sc);
}

void
ex_reset(struct ex_softc *sc)
{
	uint16_t val = GLOBAL_RESET;

	if (sc->ex_conf & EX_CONF_RESETHACK)
		val |= 0x10;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ELINK_COMMAND, val);
	/*
	 * XXX apparently the command in progress bit can't be trusted
	 * during a reset, so we just always wait this long. Fortunately
	 * we normally only reset the chip during autoconfig.
	 */
	delay(100000);
	ex_waitcmd(sc);
}

void
ex_watchdog(struct ifnet *ifp)
{
	struct ex_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++sc->sc_ethercom.ec_if.if_oerrors;

	ex_reset(sc);
	ex_init(ifp);
}

void
ex_stop(struct ifnet *ifp, int disable)
{
	struct ex_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ex_txdesc *tx;
	struct ex_rxdesc *rx;
	int i;

	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_DISABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_DISABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, STOP_TRANSCEIVER);

	for (tx = sc->tx_head ; tx != NULL; tx = tx->tx_next) {
		if (tx->tx_mbhead == NULL)
			continue;
		m_freem(tx->tx_mbhead);
		tx->tx_mbhead = NULL;
		bus_dmamap_unload(sc->sc_dmat, tx->tx_dmamap);
		tx->tx_dpd->dpd_fsh = tx->tx_dpd->dpd_nextptr = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dpd_dmamap,
		    ((char *)tx->tx_dpd - (char *)sc->sc_dpd),
		    sizeof (struct ex_dpd),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	sc->tx_tail = sc->tx_head = NULL;
	ex_init_txdescs(sc);

	sc->rx_tail = sc->rx_head = 0;
	for (i = 0; i < EX_NUPD; i++) {
		rx = &sc->sc_rxdescs[i];
		if (rx->rx_mbhead != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rx->rx_dmamap);
			m_freem(rx->rx_mbhead);
			rx->rx_mbhead = NULL;
		}
		ex_add_rxbuf(sc, rx);
	}

	bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR | INTR_LATCH);

	callout_stop(&sc->ex_mii_callout);
	if (sc->ex_conf & EX_CONF_MII)
		mii_down(&sc->ex_mii);

	if (disable)
		ex_disable(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_timer = 0;
}

static void
ex_init_txdescs(struct ex_softc *sc)
{
	int i;

	for (i = 0; i < EX_NDPD; i++) {
		sc->sc_txdescs[i].tx_dmamap = sc->sc_tx_dmamaps[i];
		sc->sc_txdescs[i].tx_dpd = &sc->sc_dpd[i];
		if (i < EX_NDPD - 1)
			sc->sc_txdescs[i].tx_next = &sc->sc_txdescs[i + 1];
		else
			sc->sc_txdescs[i].tx_next = NULL;
	}
	sc->tx_free = &sc->sc_txdescs[0];
	sc->tx_ftail = &sc->sc_txdescs[EX_NDPD-1];
}


int
ex_activate(device_t self, enum devact act)
{
	struct ex_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
ex_detach(struct ex_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ex_rxdesc *rxd;
	int i, s;

	/* Succeed now if there's no work to do. */
	if ((sc->ex_flags & EX_FLAGS_ATTACHED) == 0)
		return (0);

	s = splnet();
	/* Stop the interface. Callouts are stopped in it. */
	ex_stop(ifp, 1);
	splx(s);

	/* Destroy our callout. */
	callout_destroy(&sc->ex_mii_callout);

	if (sc->ex_conf & EX_CONF_MII) {
		/* Detach all PHYs */
		mii_detach(&sc->ex_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	}

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->ex_mii.mii_media, IFM_INST_ANY);

	rnd_detach_source(&sc->rnd_source);
	ether_ifdetach(ifp);
	if_detach(ifp);

	for (i = 0; i < EX_NUPD; i++) {
		rxd = &sc->sc_rxdescs[i];
		if (rxd->rx_mbhead != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
			m_freem(rxd->rx_mbhead);
			rxd->rx_mbhead = NULL;
		}
	}
	for (i = 0; i < EX_NUPD; i++)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_dmamaps[i]);
	for (i = 0; i < EX_NDPD; i++)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_dmamaps[i]);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dpd_dmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dpd_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_dpd,
	    EX_NDPD * sizeof (struct ex_dpd));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_drseg);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_upd_dmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_upd_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_upd,
	    EX_NUPD * sizeof (struct ex_upd));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_useg, sc->sc_urseg);

	pmf_device_deregister(sc->sc_dev);

	return (0);
}

/*
 * Before reboots, reset card completely.
 */
static bool
ex_shutdown(device_t self, int flags)
{
	struct ex_softc *sc = device_private(self);

	ex_stop(&sc->sc_ethercom.ec_if, 1);
	/*
	 * Make sure the interface is powered up when we reboot,
	 * otherwise firmware on some systems gets really confused.
	 */
	(void) ex_enable(sc);
	return true;
}

/*
 * Read EEPROM data.
 * XXX what to do if EEPROM doesn't unbusy?
 */
uint16_t
ex_read_eeprom(struct ex_softc *sc, int offset)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint16_t data = 0, cmd = READ_EEPROM;
	int off;

	off = sc->ex_conf & EX_CONF_EEPROM_OFF ? 0x30 : 0;
	cmd = sc->ex_conf & EX_CONF_EEPROM_8BIT ? READ_EEPROM8 : READ_EEPROM;

	GO_WINDOW(0);
	if (ex_eeprom_busy(sc))
		goto out;
	bus_space_write_2(iot, ioh, ELINK_W0_EEPROM_COMMAND,
	    cmd | (off + (offset & 0x3f)));
	if (ex_eeprom_busy(sc))
		goto out;
	data = bus_space_read_2(iot, ioh, ELINK_W0_EEPROM_DATA);
out:
	return data;
}

static int
ex_eeprom_busy(struct ex_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i = 100;

	while (i--) {
		if (!(bus_space_read_2(iot, ioh, ELINK_W0_EEPROM_COMMAND) &
		    EEPROM_BUSY))
			return 0;
		delay(100);
	}
	aprint_error_dev(sc->sc_dev, "eeprom stays busy.\n");
	return (1);
}

/*
 * Create a new rx buffer and add it to the 'soft' rx list.
 */
static int
ex_add_rxbuf(struct ex_softc *sc, struct ex_rxdesc *rxd)
{
	struct mbuf *m, *oldm;
	bus_dmamap_t rxmap;
	int error, rval = 0;

	oldm = rxd->rx_mbhead;
	rxmap = rxd->rx_dmamap;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 1;
			m = oldm;
			MRESETDATA(m);
			rval = 1;
		}
	} else {
		if (oldm == NULL)
			return 1;
		m = oldm;
		MRESETDATA(m);
		rval = 1;
	}

	/*
	 * Setup the DMA map for this receive buffer.
	 */
	if (m != oldm) {
		if (oldm != NULL)
			bus_dmamap_unload(sc->sc_dmat, rxmap);
		error = bus_dmamap_load(sc->sc_dmat, rxmap,
		    m->m_ext.ext_buf, MCLBYTES, NULL,
		    BUS_DMA_READ|BUS_DMA_NOWAIT);
		if (error) {
			aprint_error_dev(sc->sc_dev, "can't load rx buffer, error = %d\n",
			    error);
			panic("ex_add_rxbuf");	/* XXX */
		}
	}

	/*
	 * Align for data after 14 byte header.
	 */
	m->m_data += 2;

	rxd->rx_mbhead = m;
	rxd->rx_upd->upd_pktstatus = htole32(MCLBYTES - 2);
	rxd->rx_upd->upd_frags[0].fr_addr =
	    htole32(rxmap->dm_segs[0].ds_addr + 2);
	rxd->rx_upd->upd_nextptr = 0;

	/*
	 * Attach it to the end of the list.
	 */
	if (sc->rx_head != NULL) {
		sc->rx_tail->rx_next = rxd;
		sc->rx_tail->rx_upd->upd_nextptr = htole32(sc->sc_upddma +
		    ((char *)rxd->rx_upd - (char *)sc->sc_upd));
		bus_dmamap_sync(sc->sc_dmat, sc->sc_upd_dmamap,
		    (char *)sc->rx_tail->rx_upd - (char *)sc->sc_upd,
		    sizeof (struct ex_upd),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	} else {
		sc->rx_head = rxd;
	}
	sc->rx_tail = rxd;

	bus_dmamap_sync(sc->sc_dmat, rxmap, 0, rxmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_upd_dmamap,
	    ((char *)rxd->rx_upd - (char *)sc->sc_upd),
	    sizeof (struct ex_upd), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	return (rval);
}

uint32_t
ex_mii_bitbang_read(device_t self)
{
	struct ex_softc *sc = device_private(self);

	/* We're already in Window 4. */
	return (bus_space_read_2(sc->sc_iot, sc->sc_ioh, ELINK_W4_PHYSMGMT));
}

void
ex_mii_bitbang_write(device_t self, uint32_t val)
{
	struct ex_softc *sc = device_private(self);

	/* We're already in Window 4. */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ELINK_W4_PHYSMGMT, val);
}

int
ex_mii_readreg(device_t v, int phy, int reg)
{
	struct ex_softc *sc = device_private(v);
	int val;

	if ((sc->ex_conf & EX_CONF_INTPHY) && phy != ELINK_INTPHY_ID)
		return 0;

	GO_WINDOW(4);

	val = mii_bitbang_readreg(v, &ex_mii_bitbang_ops, phy, reg);

	GO_WINDOW(1);

	return (val);
}

void
ex_mii_writereg(device_t v, int phy, int reg, int data)
{
	struct ex_softc *sc = device_private(v);

	GO_WINDOW(4);

	mii_bitbang_writereg(v, &ex_mii_bitbang_ops, phy, reg, data);

	GO_WINDOW(1);
}

void
ex_mii_statchg(struct ifnet *ifp)
{
	struct ex_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int mctl;

	GO_WINDOW(3);
	mctl = bus_space_read_2(iot, ioh, ELINK_W3_MAC_CONTROL);
	if (sc->ex_mii.mii_media_active & IFM_FDX)
		mctl |= MAC_CONTROL_FDX;
	else
		mctl &= ~MAC_CONTROL_FDX;
	bus_space_write_2(iot, ioh, ELINK_W3_MAC_CONTROL, mctl);
	GO_WINDOW(1);   /* back to operating window */
}

int
ex_enable(struct ex_softc *sc)
{
	if (sc->enabled == 0 && sc->enable != NULL) {
		if ((*sc->enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
		sc->enabled = 1;
	}
	return (0);
}

void
ex_disable(struct ex_softc *sc)
{
	if (sc->enabled == 1 && sc->disable != NULL) {
		(*sc->disable)(sc);
		sc->enabled = 0;
	}
}

