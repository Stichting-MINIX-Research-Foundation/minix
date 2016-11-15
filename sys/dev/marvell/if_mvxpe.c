/*	$NetBSD: if_mvxpe.c,v 1.2 2015/06/03 03:55:47 hsuenaga Exp $	*/
/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_mvxpe.c,v 1.2 2015/06/03 03:55:47 hsuenaga Exp $");

#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/evcnt.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>
#include <dev/marvell/mvxpbmvar.h>
#include <dev/marvell/if_mvxpereg.h>
#include <dev/marvell/if_mvxpevar.h>

#include "locators.h"

#if BYTE_ORDER == BIG_ENDIAN
#error "BIG ENDIAN not supported"
#endif

#ifdef MVXPE_DEBUG
#define STATIC /* nothing */
#else
#define STATIC static
#endif

/* autoconf(9) */
STATIC int mvxpe_match(device_t, struct cfdata *, void *);
STATIC void mvxpe_attach(device_t, device_t, void *);
STATIC int mvxpe_evcnt_attach(struct mvxpe_softc *);
CFATTACH_DECL_NEW(mvxpe_mbus, sizeof(struct mvxpe_softc),
    mvxpe_match, mvxpe_attach, NULL, NULL);
STATIC void mvxpe_sc_lock(struct mvxpe_softc *);
STATIC void mvxpe_sc_unlock(struct mvxpe_softc *);

/* MII */
STATIC int mvxpe_miibus_readreg(device_t, int, int);
STATIC void mvxpe_miibus_writereg(device_t, int, int, int);
STATIC void mvxpe_miibus_statchg(struct ifnet *);

/* Addres Decoding Window */
STATIC void mvxpe_wininit(struct mvxpe_softc *, enum marvell_tags *);

/* Device Register Initialization */
STATIC int mvxpe_initreg(struct ifnet *);

/* Descriptor Ring Control for each of queues */
STATIC void *mvxpe_dma_memalloc(struct mvxpe_softc *, bus_dmamap_t *, size_t);
STATIC int mvxpe_ring_alloc_queue(struct mvxpe_softc *, int);
STATIC void mvxpe_ring_dealloc_queue(struct mvxpe_softc *, int);
STATIC void mvxpe_ring_init_queue(struct mvxpe_softc *, int);
STATIC void mvxpe_ring_flush_queue(struct mvxpe_softc *, int);
STATIC void mvxpe_ring_sync_rx(struct mvxpe_softc *, int, int, int, int);
STATIC void mvxpe_ring_sync_tx(struct mvxpe_softc *, int, int, int, int);

/* Rx/Tx Queue Control */
STATIC int mvxpe_rx_queue_init(struct ifnet *, int);
STATIC int mvxpe_tx_queue_init(struct ifnet *, int);
STATIC int mvxpe_rx_queue_enable(struct ifnet *, int);
STATIC int mvxpe_tx_queue_enable(struct ifnet *, int);
STATIC void mvxpe_rx_lockq(struct mvxpe_softc *, int);
STATIC void mvxpe_rx_unlockq(struct mvxpe_softc *, int);
STATIC void mvxpe_tx_lockq(struct mvxpe_softc *, int);
STATIC void mvxpe_tx_unlockq(struct mvxpe_softc *, int);

/* Interrupt Handlers */
STATIC void mvxpe_disable_intr(struct mvxpe_softc *);
STATIC void mvxpe_enable_intr(struct mvxpe_softc *);
STATIC int mvxpe_rxtxth_intr(void *);
STATIC int mvxpe_misc_intr(void *);
STATIC int mvxpe_rxtx_intr(void *);
STATIC void mvxpe_tick(void *);

/* struct ifnet and mii callbacks*/
STATIC void mvxpe_start(struct ifnet *);
STATIC int mvxpe_ioctl(struct ifnet *, u_long, void *);
STATIC int mvxpe_init(struct ifnet *);
STATIC void mvxpe_stop(struct ifnet *, int);
STATIC void mvxpe_watchdog(struct ifnet *);
STATIC int mvxpe_ifflags_cb(struct ethercom *);
STATIC int mvxpe_mediachange(struct ifnet *);
STATIC void mvxpe_mediastatus(struct ifnet *, struct ifmediareq *);

/* Link State Notify */
STATIC void mvxpe_linkupdate(struct mvxpe_softc *sc);
STATIC void mvxpe_linkup(struct mvxpe_softc *);
STATIC void mvxpe_linkdown(struct mvxpe_softc *);
STATIC void mvxpe_linkreset(struct mvxpe_softc *);

/* Tx Subroutines */
STATIC int mvxpe_tx_queue_select(struct mvxpe_softc *, struct mbuf *);
STATIC int mvxpe_tx_queue(struct mvxpe_softc *, struct mbuf *, int);
STATIC void mvxpe_tx_set_csumflag(struct ifnet *,
    struct mvxpe_tx_desc *, struct mbuf *);
STATIC void mvxpe_tx_complete(struct mvxpe_softc *, uint32_t);
STATIC void mvxpe_tx_queue_complete(struct mvxpe_softc *, int);

/* Rx Subroutines */
STATIC void mvxpe_rx(struct mvxpe_softc *, uint32_t);
STATIC void mvxpe_rx_queue(struct mvxpe_softc *, int, int);
STATIC int mvxpe_rx_queue_select(struct mvxpe_softc *, uint32_t, int *);
STATIC void mvxpe_rx_refill(struct mvxpe_softc *, uint32_t);
STATIC void mvxpe_rx_queue_refill(struct mvxpe_softc *, int);
STATIC int mvxpe_rx_queue_add(struct mvxpe_softc *, int);
STATIC void mvxpe_rx_set_csumflag(struct ifnet *,
    struct mvxpe_rx_desc *, struct mbuf *);

/* MAC address filter */
STATIC uint8_t mvxpe_crc8(const uint8_t *, size_t);
STATIC void mvxpe_filter_setup(struct mvxpe_softc *);

/* sysctl(9) */
STATIC int sysctl_read_mib(SYSCTLFN_PROTO);
STATIC int sysctl_clear_mib(SYSCTLFN_PROTO);
STATIC int sysctl_set_queue_length(SYSCTLFN_PROTO);
STATIC int sysctl_set_queue_rxthtime(SYSCTLFN_PROTO);
STATIC void sysctl_mvxpe_init(struct mvxpe_softc *);

/* MIB */
STATIC void mvxpe_clear_mib(struct mvxpe_softc *);
STATIC void mvxpe_update_mib(struct mvxpe_softc *);

/* for Debug */
STATIC void mvxpe_dump_txdesc(struct mvxpe_tx_desc *, int) __attribute__((__unused__));
STATIC void mvxpe_dump_rxdesc(struct mvxpe_rx_desc *, int) __attribute__((__unused__));

STATIC int mvxpe_root_num;
STATIC kmutex_t mii_mutex;
STATIC int mii_init = 0;
#ifdef MVXPE_DEBUG
STATIC int mvxpe_debug = MVXPE_DEBUG;
#endif

/*
 * List of MIB register and names
 */
STATIC struct mvxpe_mib_def {
	uint32_t regnum;
	int reg64;
	const char *sysctl_name;
	const char *desc;
} mvxpe_mib_list[] = {
	{MVXPE_MIB_RX_GOOD_OCT, 1,	"rx_good_oct",
	    "Good Octets Rx"},
	{MVXPE_MIB_RX_BAD_OCT, 0,	"rx_bad_oct",
	    "Bad  Octets Rx"},
	{MVXPE_MIB_RX_MAC_TRNS_ERR, 0,	"rx_mac_err",
	    "MAC Transmit Error"},
	{MVXPE_MIB_RX_GOOD_FRAME, 0,	"rx_good_frame",
	    "Good Frames Rx"},
	{MVXPE_MIB_RX_BAD_FRAME, 0,	"rx_bad_frame",
	    "Bad Frames Rx"},
	{MVXPE_MIB_RX_BCAST_FRAME, 0,	"rx_bcast_frame",
	    "Broadcast Frames Rx"},
	{MVXPE_MIB_RX_MCAST_FRAME, 0,	"rx_mcast_frame",
	    "Multicast Frames Rx"},
	{MVXPE_MIB_RX_FRAME64_OCT, 0,	"rx_frame_1_64",
	    "Frame Size    1 -   64"},
	{MVXPE_MIB_RX_FRAME127_OCT, 0,	"rx_frame_65_127",
	    "Frame Size   65 -  127"},
	{MVXPE_MIB_RX_FRAME255_OCT, 0,	"rx_frame_128_255",
	    "Frame Size  128 -  255"},
	{MVXPE_MIB_RX_FRAME511_OCT, 0,	"rx_frame_256_511",
	    "Frame Size  256 -  511"},
	{MVXPE_MIB_RX_FRAME1023_OCT, 0,	"rx_frame_512_1023",
	    "Frame Size  512 - 1023"},
	{MVXPE_MIB_RX_FRAMEMAX_OCT, 0,	"rx_fame_1024_max",
	    "Frame Size 1024 -  Max"},
	{MVXPE_MIB_TX_GOOD_OCT, 1,	"tx_good_oct",
	    "Good Octets Tx"},
	{MVXPE_MIB_TX_GOOD_FRAME, 0,	"tx_good_frame",
	    "Good Frames Tx"},
	{MVXPE_MIB_TX_EXCES_COL, 0,	"tx_exces_collision",
	    "Excessive Collision"},
	{MVXPE_MIB_TX_MCAST_FRAME, 0,	"tx_mcast_frame",
	    "Multicast Frames Tx"},
	{MVXPE_MIB_TX_BCAST_FRAME, 0,	"tx_bcast_frame",
	    "Broadcast Frames Tx"},
	{MVXPE_MIB_TX_MAC_CTL_ERR, 0,	"tx_mac_err",
	    "Unknown MAC Control"},
	{MVXPE_MIB_FC_SENT, 0,		"fc_tx",
	    "Flow Control Tx"},
	{MVXPE_MIB_FC_GOOD, 0,		"fc_rx_good",
	    "Good Flow Control Rx"},
	{MVXPE_MIB_FC_BAD, 0,		"fc_rx_bad",
	    "Bad Flow Control Rx"},
	{MVXPE_MIB_PKT_UNDERSIZE, 0,	"pkt_undersize",
	    "Undersized Packets Rx"},
	{MVXPE_MIB_PKT_FRAGMENT, 0,	"pkt_fragment",
	    "Fragmented Packets Rx"},
	{MVXPE_MIB_PKT_OVERSIZE, 0,	"pkt_oversize",
	    "Oversized Packets Rx"},
	{MVXPE_MIB_PKT_JABBER, 0,	"pkt_jabber",
	    "Jabber Packets Rx"},
	{MVXPE_MIB_MAC_RX_ERR, 0,	"mac_rx_err",
	    "MAC Rx Errors"},
	{MVXPE_MIB_MAC_CRC_ERR, 0,	"mac_crc_err",
	    "MAC CRC Errors"},
	{MVXPE_MIB_MAC_COL, 0,		"mac_collision",
	    "MAC Collision"},
	{MVXPE_MIB_MAC_LATE_COL, 0,	"mac_late_collision",
	    "MAC Late Collision"},
};

/*
 * autoconf(9)
 */
/* ARGSUSED */
STATIC int
mvxpe_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;
	bus_size_t pv_off;
	uint32_t pv;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT)
		return 0;

	/* check port version */
	pv_off = mva->mva_offset + MVXPE_PV;
	pv = bus_space_read_4(mva->mva_iot, mva->mva_ioh, pv_off);
	if (MVXPE_PV_GET_VERSION(pv) < 0x10)
		return 0; /* old version is not supported */

	return 1;
}

/* ARGSUSED */
STATIC void
mvxpe_attach(device_t parent, device_t self, void *aux)
{
	struct mvxpe_softc *sc = device_private(self);
	struct mii_softc *mii;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct marvell_attach_args *mva = aux;
	prop_dictionary_t dict;
	prop_data_t enaddrp = NULL;
	uint32_t phyaddr, maddrh, maddrl;
	uint8_t enaddr[ETHER_ADDR_LEN];
	int q;

	aprint_naive("\n");
	aprint_normal(": Marvell ARMADA GbE Controller\n");
	memset(sc, 0, sizeof(*sc));
	sc->sc_dev = self;
	sc->sc_port = mva->mva_unit;
	sc->sc_iot = mva->mva_iot;
	sc->sc_dmat = mva->mva_dmat;
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NET);
	callout_init(&sc->sc_tick_ch, 0);
	callout_setfunc(&sc->sc_tick_ch, mvxpe_tick, sc);

	/*
	 * BUS space
	 */
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
	    mva->mva_offset, mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "Cannot map registers\n");
		goto fail;
	}
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
	    mva->mva_offset + MVXPE_PORTMIB_BASE, MVXPE_PORTMIB_SIZE,
	    &sc->sc_mibh)) {
		aprint_error_dev(self,
		    "Cannot map destination address filter registers\n");
		goto fail;
	}
	sc->sc_version = MVXPE_READ(sc, MVXPE_PV);
	aprint_normal_dev(self, "Port Version %#x\n", sc->sc_version);

	/*
	 * Buffer Manager(BM) subsystem.
	 */
	sc->sc_bm = mvxpbm_device(mva);
	if (sc->sc_bm == NULL) {
		aprint_error_dev(self, "no Buffer Manager.\n");
		goto fail;
	}
	aprint_normal_dev(self,
	    "Using Buffer Manager: %s\n", mvxpbm_xname(sc->sc_bm));	
	aprint_normal_dev(sc->sc_dev,
	    "%zu kbytes managed buffer, %zu bytes * %u entries allocated.\n",
	    mvxpbm_buf_size(sc->sc_bm) / 1024,
	    mvxpbm_chunk_size(sc->sc_bm), mvxpbm_chunk_count(sc->sc_bm));

	/*
	 * make sure DMA engines are in reset state
	 */
	MVXPE_WRITE(sc, MVXPE_PRXINIT, 0x00000001);
	MVXPE_WRITE(sc, MVXPE_PTXINIT, 0x00000001);

	/*
	 * Address decoding window
	 */
	mvxpe_wininit(sc, mva->mva_tags);

	/*
	 * MAC address
	 */
	dict = device_properties(self);
	if (dict)
		enaddrp = prop_dictionary_get(dict, "mac-address");
	if (enaddrp) {
		memcpy(enaddr, prop_data_data_nocopy(enaddrp), ETHER_ADDR_LEN);
		maddrh  = enaddr[0] << 24;
		maddrh |= enaddr[1] << 16;
		maddrh |= enaddr[2] << 8;
		maddrh |= enaddr[3];
		maddrl  = enaddr[4] << 8;
		maddrl |= enaddr[5];
		MVXPE_WRITE(sc, MVXPE_MACAH, maddrh);
		MVXPE_WRITE(sc, MVXPE_MACAL, maddrl);
	}
	else {
		/*
		 * even if enaddr is not found in dictionary,
		 * the port may be initialized by IPL program such as U-BOOT.
		 */
		maddrh = MVXPE_READ(sc, MVXPE_MACAH);
		maddrl = MVXPE_READ(sc, MVXPE_MACAL);
		if ((maddrh | maddrl) == 0) {
			aprint_error_dev(self, "No Ethernet address\n");
			return;
		}
	}
	sc->sc_enaddr[0] = maddrh >> 24;
	sc->sc_enaddr[1] = maddrh >> 16;
	sc->sc_enaddr[2] = maddrh >> 8;
	sc->sc_enaddr[3] = maddrh >> 0;
	sc->sc_enaddr[4] = maddrl >> 8;
	sc->sc_enaddr[5] = maddrl >> 0;
	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	/*
	 * Register interrupt handlers
	 * XXX: handle Ethernet unit intr. and Error intr.
	 */
	mvxpe_disable_intr(sc);
	marvell_intr_establish(mva->mva_irq, IPL_NET, mvxpe_rxtxth_intr, sc);

	/*
	 * MIB buffer allocation
	 */
	sc->sc_sysctl_mib_size =
	    __arraycount(mvxpe_mib_list) * sizeof(struct mvxpe_sysctl_mib);
	sc->sc_sysctl_mib = kmem_alloc(sc->sc_sysctl_mib_size, KM_NOSLEEP);
	if (sc->sc_sysctl_mib == NULL)
		goto fail;
	memset(sc->sc_sysctl_mib, 0, sc->sc_sysctl_mib_size);

	/*
	 * Device DMA Buffer allocation
	 */
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		if (mvxpe_ring_alloc_queue(sc, q) != 0)
			goto fail;
		mvxpe_ring_init_queue(sc, q);
	}

	/*
	 * We can support 802.1Q VLAN-sized frames and jumbo
	 * Ethernet frames.
	 */
	sc->sc_ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | ETHERCAP_JUMBO_MTU;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = mvxpe_start;
	ifp->if_ioctl = mvxpe_ioctl;
	ifp->if_init = mvxpe_init;
	ifp->if_stop = mvxpe_stop;
	ifp->if_watchdog = mvxpe_watchdog;

	/*
	 * We can do IPv4/TCPv4/UDPv4/TCPv6/UDPv6 checksums in hardware.
	 */
	ifp->if_capabilities |= IFCAP_CSUM_IPv4_Tx;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4_Rx;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4_Tx;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4_Rx;
	ifp->if_capabilities |= IFCAP_CSUM_UDPv4_Tx;
	ifp->if_capabilities |= IFCAP_CSUM_UDPv4_Rx;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv6_Tx;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv6_Rx;
	ifp->if_capabilities |= IFCAP_CSUM_UDPv6_Tx;
	ifp->if_capabilities |= IFCAP_CSUM_UDPv6_Rx;

	/*
	 * Initialize struct ifnet
	 */
	IFQ_SET_MAXLEN(&ifp->if_snd, max(MVXPE_TX_RING_CNT - 1, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), sizeof(ifp->if_xname));

	/*
	 * Enable DMA engines and Initiazlie Device Regisers.
	 */
	MVXPE_WRITE(sc, MVXPE_PRXINIT, 0x00000000);
	MVXPE_WRITE(sc, MVXPE_PTXINIT, 0x00000000);
	MVXPE_WRITE(sc, MVXPE_PACC, MVXPE_PACC_ACCELERATIONMODE_EDM);
	mvxpe_sc_lock(sc); /* XXX */
	mvxpe_filter_setup(sc);
	mvxpe_sc_unlock(sc);
	mvxpe_initreg(ifp);

	/*
	 * Now MAC is working, setup MII.
	 */
	if (mii_init == 0) {
		/*
		 * MII bus is shared by all MACs and all PHYs in SoC.
		 * serializing the bus access should be safe.
		 */
		mutex_init(&mii_mutex, MUTEX_DEFAULT, IPL_NET);
		mii_init = 1;
	}
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = mvxpe_miibus_readreg;
	sc->sc_mii.mii_writereg = mvxpe_miibus_writereg;
	sc->sc_mii.mii_statchg = mvxpe_miibus_statchg;

	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    mvxpe_mediachange, mvxpe_mediastatus);
	/*
	 * XXX: phy addressing highly depends on Board Design.
	 * we assume phyaddress == MAC unit number here, 
	 * but some boards may not.
	 */
	mii_attach(self, &sc->sc_mii, 0xffffffff,
	    MII_PHY_ANY, sc->sc_dev->dv_unit, 0);
	mii = LIST_FIRST(&sc->sc_mii.mii_phys);
	if (mii == NULL) {
		aprint_error_dev(self, "no PHY found!\n");
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
		phyaddr = MVXPE_PHYADDR_PHYAD(mii->mii_phy);
		MVXPE_WRITE(sc, MVXPE_PHYADDR, phyaddr);
		DPRINTSC(sc, 1, "PHYADDR: %#x\n", MVXPE_READ(sc, MVXPE_PHYADDR));
	}

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);

	ether_ifattach(ifp, sc->sc_enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, mvxpe_ifflags_cb);

	sysctl_mvxpe_init(sc);
	mvxpe_evcnt_attach(sc);
	rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	return;

fail:
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++)
		mvxpe_ring_dealloc_queue(sc, q);
	if (sc->sc_sysctl_mib)
		kmem_free(sc->sc_sysctl_mib, sc->sc_sysctl_mib_size);

	return;
}

STATIC int
mvxpe_evcnt_attach(struct mvxpe_softc *sc)
{
#ifdef MVXPE_EVENT_COUNTERS
	int q;

	/* Master Interrupt Handler */
	evcnt_attach_dynamic(&sc->sc_ev.ev_i_rxtxth, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTxTH Intr.");
	evcnt_attach_dynamic(&sc->sc_ev.ev_i_rxtx, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx Intr.");
	evcnt_attach_dynamic(&sc->sc_ev.ev_i_misc, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Intr.");

	/* RXTXTH Interrupt */
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtxth_txerr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTxTH Tx error summary");

	/* MISC Interrupt */
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_phystatuschng, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC phy status changed");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_linkchange, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC link status changed");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_iae, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC internal address error");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_rxoverrun, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Rx FIFO overrun");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_rxcrc, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Rx CRC error");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_rxlargepacket, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Rx too large frame");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_txunderrun, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Tx FIFO underrun");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_prbserr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC SERDES loopback test err");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_srse, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC SERDES sync error");
	evcnt_attach_dynamic(&sc->sc_ev.ev_misc_txreq, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "MISC Tx resource erorr");

	/* RxTx Interrupt */
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_rreq, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx Rx resource erorr");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_rpq, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx Rx pakcet");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_tbrq, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx Tx complete");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_rxtxth, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx RxTxTH summary");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_txerr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx Tx error summary");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxtx_misc, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "RxTx MISC summary");

	/* Link */
	evcnt_attach_dynamic(&sc->sc_ev.ev_link_up, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "link up");
	evcnt_attach_dynamic(&sc->sc_ev.ev_link_down, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "link down");

	/* Rx Descriptor */
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxd_ce, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx CRC error counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxd_or, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx FIFO overrun counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxd_mf, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx too large frame counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxd_re, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx resource error counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_rxd_scat, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx unexpected scatter bufs");

	/* Tx Descriptor */
	evcnt_attach_dynamic(&sc->sc_ev.ev_txd_lc, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx late collision counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_txd_rl, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx excess. collision counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_txd_ur, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx FIFO underrun counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_txd_oth, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx unkonwn erorr counter");

	/* Status Registers */
	evcnt_attach_dynamic(&sc->sc_ev.ev_reg_pdfc, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx discard counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_reg_pofc, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Rx overrun counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_reg_txbadfcs, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx bad FCS counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_reg_txdropped, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx dorpped counter");
	evcnt_attach_dynamic(&sc->sc_ev.ev_reg_lpic, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "LP_IDLE counter");

	/* Device Driver Errors */
	evcnt_attach_dynamic(&sc->sc_ev.ev_drv_wdogsoft, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "watchdog timer expired");
	evcnt_attach_dynamic(&sc->sc_ev.ev_drv_txerr, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Tx descriptor alloc failed");
#define MVXPE_QUEUE_DESC(q) "Rx success in queue " # q
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		static const char *rxq_desc[] = {
			MVXPE_QUEUE_DESC(0), MVXPE_QUEUE_DESC(1),
			MVXPE_QUEUE_DESC(2), MVXPE_QUEUE_DESC(3),
			MVXPE_QUEUE_DESC(4), MVXPE_QUEUE_DESC(5),
			MVXPE_QUEUE_DESC(6), MVXPE_QUEUE_DESC(7),
		};
		evcnt_attach_dynamic(&sc->sc_ev.ev_drv_rxq[q], EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), rxq_desc[q]);
	}
#undef MVXPE_QUEUE_DESC
#define MVXPE_QUEUE_DESC(q) "Tx success in queue " # q
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		static const char *txq_desc[] = {
			MVXPE_QUEUE_DESC(0), MVXPE_QUEUE_DESC(1),
			MVXPE_QUEUE_DESC(2), MVXPE_QUEUE_DESC(3),
			MVXPE_QUEUE_DESC(4), MVXPE_QUEUE_DESC(5),
			MVXPE_QUEUE_DESC(6), MVXPE_QUEUE_DESC(7),
		};
		evcnt_attach_dynamic(&sc->sc_ev.ev_drv_txq[q], EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), txq_desc[q]);
	}
#undef MVXPE_QUEUE_DESC
#define MVXPE_QUEUE_DESC(q) "Rx error in queue " # q
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		static const char *rxqe_desc[] = {
			MVXPE_QUEUE_DESC(0), MVXPE_QUEUE_DESC(1),
			MVXPE_QUEUE_DESC(2), MVXPE_QUEUE_DESC(3),
			MVXPE_QUEUE_DESC(4), MVXPE_QUEUE_DESC(5),
			MVXPE_QUEUE_DESC(6), MVXPE_QUEUE_DESC(7),
		};
		evcnt_attach_dynamic(&sc->sc_ev.ev_drv_rxqe[q], EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), rxqe_desc[q]);
	}
#undef MVXPE_QUEUE_DESC
#define MVXPE_QUEUE_DESC(q) "Tx error in queue " # q
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		static const char *txqe_desc[] = {
			MVXPE_QUEUE_DESC(0), MVXPE_QUEUE_DESC(1),
			MVXPE_QUEUE_DESC(2), MVXPE_QUEUE_DESC(3),
			MVXPE_QUEUE_DESC(4), MVXPE_QUEUE_DESC(5),
			MVXPE_QUEUE_DESC(6), MVXPE_QUEUE_DESC(7),
		};
		evcnt_attach_dynamic(&sc->sc_ev.ev_drv_txqe[q], EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), txqe_desc[q]);
	}
#undef MVXPE_QUEUE_DESC

#endif /* MVXPE_EVENT_COUNTERS */
	return 0;
}

STATIC void
mvxpe_sc_lock(struct mvxpe_softc *sc)
{
	mutex_enter(&sc->sc_mtx);
}

STATIC void
mvxpe_sc_unlock(struct mvxpe_softc *sc)
{
	mutex_exit(&sc->sc_mtx);
}

/*
 * MII
 */
STATIC int
mvxpe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct mvxpe_softc *sc = device_private(dev);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t smi, val;
	int i;

	mutex_enter(&mii_mutex);

	for (i = 0; i < MVXPE_PHY_TIMEOUT; i++) {
		DELAY(1);
		if (!(MVXPE_READ(sc, MVXPE_SMI) & MVXPE_SMI_BUSY))
			break;
	}
	if (i == MVXPE_PHY_TIMEOUT) {
		aprint_error_ifnet(ifp, "SMI busy timeout\n");
		mutex_exit(&mii_mutex);
		return -1;
	}

	smi =
	    MVXPE_SMI_PHYAD(phy) | MVXPE_SMI_REGAD(reg) | MVXPE_SMI_OPCODE_READ;
	MVXPE_WRITE(sc, MVXPE_SMI, smi);

	for (i = 0; i < MVXPE_PHY_TIMEOUT; i++) {
		DELAY(1);
		smi = MVXPE_READ(sc, MVXPE_SMI);
		if (smi & MVXPE_SMI_READVALID)
			break;
	}

	mutex_exit(&mii_mutex);

	DPRINTDEV(dev, 9, "i=%d, timeout=%d\n", i, MVXPE_PHY_TIMEOUT);

	val = smi & MVXPE_SMI_DATA_MASK;

	DPRINTDEV(dev, 9, "phy=%d, reg=%#x, val=%#x\n", phy, reg, val);

	return val;
}

STATIC void
mvxpe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct mvxpe_softc *sc = device_private(dev);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t smi;
	int i;

	DPRINTDEV(dev, 9, "phy=%d reg=%#x val=%#x\n", phy, reg, val);

	mutex_enter(&mii_mutex);

	for (i = 0; i < MVXPE_PHY_TIMEOUT; i++) {
		DELAY(1);
		if (!(MVXPE_READ(sc, MVXPE_SMI) & MVXPE_SMI_BUSY))
			break;
	}
	if (i == MVXPE_PHY_TIMEOUT) {
		aprint_error_ifnet(ifp, "SMI busy timeout\n");
		mutex_exit(&mii_mutex);
		return;
	}

	smi = MVXPE_SMI_PHYAD(phy) | MVXPE_SMI_REGAD(reg) |
	    MVXPE_SMI_OPCODE_WRITE | (val & MVXPE_SMI_DATA_MASK);
	MVXPE_WRITE(sc, MVXPE_SMI, smi);

	for (i = 0; i < MVXPE_PHY_TIMEOUT; i++) {
		DELAY(1);
		if (!(MVXPE_READ(sc, MVXPE_SMI) & MVXPE_SMI_BUSY))
			break;
	}

	mutex_exit(&mii_mutex);

	if (i == MVXPE_PHY_TIMEOUT)
		aprint_error_ifnet(ifp, "phy write timed out\n");
}

STATIC void
mvxpe_miibus_statchg(struct ifnet *ifp)
{

	/* nothing to do */
}

/*
 * Address Decoding Window
 */
STATIC void
mvxpe_wininit(struct mvxpe_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t en, ac, size;
	int window, target, attr, rv, i;

	/* First disable all address decode windows */
	en = MVXPE_BARE_EN_MASK;
	MVXPE_WRITE(sc, MVXPE_BARE, en);

	ac = 0;
	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MVXPE_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (base > 0xffffffffULL) {
			if (window >= MVXPE_NREMAP) {
				aprint_error_dev(sc->sc_dev,
				    "can't remap window %d\n", window);
				continue;
			}
			MVXPE_WRITE(sc, MVXPE_HA(window),
			    (base >> 32) & 0xffffffff);
		}

		MVXPE_WRITE(sc, MVXPE_BASEADDR(window),
		    MVXPE_BASEADDR_TARGET(target)	|
		    MVXPE_BASEADDR_ATTR(attr)		|
		    MVXPE_BASEADDR_BASE(base));
		MVXPE_WRITE(sc, MVXPE_S(window), MVXPE_S_SIZE(size));

		DPRINTSC(sc, 1, "Window %d Base 0x%016llx: Size 0x%08x\n",
				window, base, size);

		en &= ~(1 << window);
		/* set full access (r/w) */
		ac |= MVXPE_EPAP_EPAR(window, MVXPE_EPAP_AC_FA);
		window++;
	}
	/* allow to access decode window */
	MVXPE_WRITE(sc, MVXPE_EPAP, ac);

	MVXPE_WRITE(sc, MVXPE_BARE, en);
}

/*
 * Device Register Initialization
 *  reset device registers to device driver default value.
 *  the device is not enabled here.
 */
STATIC int
mvxpe_initreg(struct ifnet *ifp)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	int serdes = 0;
	uint32_t reg;
	int q, i;

	DPRINTIFNET(ifp, 1, "initializing device register\n");

	/* Init TX/RX Queue Registers */
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		mvxpe_rx_lockq(sc, q);
		if (mvxpe_rx_queue_init(ifp, q) != 0) {
			aprint_error_ifnet(ifp,
			    "initialization failed: cannot initialize queue\n");
			mvxpe_rx_unlockq(sc, q);
			mvxpe_tx_unlockq(sc, q);
			return ENOBUFS;
		}
		mvxpe_rx_unlockq(sc, q);

		mvxpe_tx_lockq(sc, q);
		if (mvxpe_tx_queue_init(ifp, q) != 0) {
			aprint_error_ifnet(ifp,
			    "initialization failed: cannot initialize queue\n");
			mvxpe_rx_unlockq(sc, q);
			mvxpe_tx_unlockq(sc, q);
			return ENOBUFS;
		}
		mvxpe_tx_unlockq(sc, q);
	}

	/* Tx MTU Limit */
	MVXPE_WRITE(sc, MVXPE_TXMTU, MVXPE_MTU);

	/* Check SGMII or SERDES(asume IPL/U-BOOT initialize this) */
	reg = MVXPE_READ(sc, MVXPE_PMACC0);
	if ((reg & MVXPE_PMACC0_PORTTYPE) != 0)
		serdes = 1;

	/* Ethernet Unit Control */
	reg = MVXPE_READ(sc, MVXPE_EUC);
	reg |= MVXPE_EUC_POLLING;
	MVXPE_WRITE(sc, MVXPE_EUC, reg);

	/* Auto Negotiation */
	reg  = MVXPE_PANC_MUSTSET;	/* must write 0x1 */
	reg |= MVXPE_PANC_FORCELINKFAIL;/* force link state down */
	reg |= MVXPE_PANC_ANSPEEDEN;	/* interface speed negotiation */
	reg |= MVXPE_PANC_ANDUPLEXEN;	/* negotiate duplex mode */
	if (serdes) {
		reg |= MVXPE_PANC_INBANDANEN; /* In Band negotiation */
		reg |= MVXPE_PANC_INBANDANBYPASSEN; /* bypass negotiation */
		reg |= MVXPE_PANC_SETFULLDX; /* set full-duplex on failure */
	}
	MVXPE_WRITE(sc, MVXPE_PANC, reg);

	/* EEE: Low Power Idle */
	reg  = MVXPE_LPIC0_LILIMIT(MVXPE_LPI_LI);
	reg |= MVXPE_LPIC0_TSLIMIT(MVXPE_LPI_TS);
	MVXPE_WRITE(sc, MVXPE_LPIC0, reg);

	reg  = MVXPE_LPIC1_TWLIMIT(MVXPE_LPI_TS);
	MVXPE_WRITE(sc, MVXPE_LPIC1, reg);

	reg = MVXPE_LPIC2_MUSTSET;
	MVXPE_WRITE(sc, MVXPE_LPIC2, reg);

	/* Port MAC Control set 0 */
	reg  = MVXPE_PMACC0_MUSTSET;	/* must write 0x1 */
	reg &= ~MVXPE_PMACC0_PORTEN;	/* port is still disabled */
	reg |= MVXPE_PMACC0_FRAMESIZELIMIT(MVXPE_MRU);
	if (serdes)
		reg |= MVXPE_PMACC0_PORTTYPE;
	MVXPE_WRITE(sc, MVXPE_PMACC0, reg);

	/* Port MAC Control set 1 is only used for loop-back test */

	/* Port MAC Control set 2 */ 
	reg = MVXPE_READ(sc, MVXPE_PMACC2);
	reg &= (MVXPE_PMACC2_PCSEN | MVXPE_PMACC2_RGMIIEN);
	reg |= MVXPE_PMACC2_MUSTSET;
	MVXPE_WRITE(sc, MVXPE_PMACC2, reg);

	/* Port MAC Control set 3 is used for IPG tune */

	/* Port MAC Control set 4 is not used */

	/* Port Configuration Extended: enable Tx CRC generation */
	reg = MVXPE_READ(sc, MVXPE_PXCX);
	reg &= ~MVXPE_PXCX_TXCRCDIS;
	MVXPE_WRITE(sc, MVXPE_PXCX, reg);

	/* clear MIB counter registers(clear by read) */
	for (i = 0; i < __arraycount(mvxpe_mib_list); i++)
		MVXPE_READ_MIB(sc, (mvxpe_mib_list[i].regnum));

	/* Set SDC register except IPGINT bits */
	reg  = MVXPE_SDC_RXBSZ_16_64BITWORDS;
	reg |= MVXPE_SDC_TXBSZ_16_64BITWORDS;
	reg |= MVXPE_SDC_BLMR;
	reg |= MVXPE_SDC_BLMT;
	MVXPE_WRITE(sc, MVXPE_SDC, reg);

	return 0;
}

/*
 * Descriptor Ring Controls for each of queues
 */
STATIC void *
mvxpe_dma_memalloc(struct mvxpe_softc *sc, bus_dmamap_t *map, size_t size)
{
	bus_dma_segment_t segs;
	void *kva = NULL;
	int nsegs;

	/*
	 * Allocate the descriptor queues.
	 * struct mvxpe_ring_data contians array of descriptor per queue.
	 */
	if (bus_dmamem_alloc(sc->sc_dmat,
	    size, PAGE_SIZE, 0, &segs, 1, &nsegs, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "can't alloc device memory (%zu bytes)\n", size);
		return NULL;
	}
	if (bus_dmamem_map(sc->sc_dmat,
	    &segs, nsegs, size, &kva, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "can't map dma buffers (%zu bytes)\n", size);
		goto fail1;
	}

	if (bus_dmamap_create(sc->sc_dmat,
	    size, 1, size, 0, BUS_DMA_NOWAIT, map)) {
		aprint_error_dev(sc->sc_dev, "can't create dma map\n");
		goto fail2;
	}
	if (bus_dmamap_load(sc->sc_dmat,
	    *map, kva, size, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't load dma map\n");
		goto fail3;
	}
	memset(kva, 0, size);
	return kva;

fail3:
	bus_dmamap_destroy(sc->sc_dmat, *map);
	memset(map, 0, sizeof(*map));
fail2:
	bus_dmamem_unmap(sc->sc_dmat, kva, size);
fail1:
	bus_dmamem_free(sc->sc_dmat, &segs, nsegs);
	return NULL;
}

STATIC int
mvxpe_ring_alloc_queue(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);

	/*
	 * MVXPE_RX_RING_CNT and MVXPE_TX_RING_CNT is a hard limit of
	 * queue length. real queue length is limited by
	 * sc->sc_rx_ring[q].rx_queue_len and sc->sc_tx_ring[q].tx_queue_len.
	 *
	 * because descriptor ring reallocation needs reprogramming of
	 * DMA registers, we allocate enough descriptor for hard limit
	 * of queue length.
	 */
	rx->rx_descriptors =
	    mvxpe_dma_memalloc(sc, &rx->rx_descriptors_map,
		(sizeof(struct mvxpe_rx_desc) * MVXPE_RX_RING_CNT));
	if (rx->rx_descriptors == NULL)
		goto fail;

	tx->tx_descriptors =
	    mvxpe_dma_memalloc(sc, &tx->tx_descriptors_map,
		(sizeof(struct mvxpe_tx_desc) * MVXPE_TX_RING_CNT));
	if (tx->tx_descriptors == NULL)
		goto fail;

	return 0;
fail:
	mvxpe_ring_dealloc_queue(sc, q);
	aprint_error_dev(sc->sc_dev, "DMA Ring buffer allocation failure.\n");
	return ENOMEM;
}

STATIC void
mvxpe_ring_dealloc_queue(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	bus_dma_segment_t *segs;
	bus_size_t size;
	void *kva;
	int nsegs;

	/* Rx */
	kva = (void *)MVXPE_RX_RING_MEM_VA(sc, q);
	if (kva) {
		segs = MVXPE_RX_RING_MEM_MAP(sc, q)->dm_segs;
		nsegs = MVXPE_RX_RING_MEM_MAP(sc, q)->dm_nsegs;
		size = MVXPE_RX_RING_MEM_MAP(sc, q)->dm_mapsize;

		bus_dmamap_unload(sc->sc_dmat, MVXPE_RX_RING_MEM_MAP(sc, q));
		bus_dmamap_destroy(sc->sc_dmat, MVXPE_RX_RING_MEM_MAP(sc, q));
		bus_dmamem_unmap(sc->sc_dmat, kva, size);
		bus_dmamem_free(sc->sc_dmat, segs, nsegs);
	}

	/* Tx */
	kva = (void *)MVXPE_TX_RING_MEM_VA(sc, q);
	if (kva) {
		segs = MVXPE_TX_RING_MEM_MAP(sc, q)->dm_segs;
		nsegs = MVXPE_TX_RING_MEM_MAP(sc, q)->dm_nsegs;
		size = MVXPE_TX_RING_MEM_MAP(sc, q)->dm_mapsize;

		bus_dmamap_unload(sc->sc_dmat, MVXPE_TX_RING_MEM_MAP(sc, q));
		bus_dmamap_destroy(sc->sc_dmat, MVXPE_TX_RING_MEM_MAP(sc, q));
		bus_dmamem_unmap(sc->sc_dmat, kva, size);
		bus_dmamem_free(sc->sc_dmat, segs, nsegs);
	}

	/* Clear doungling pointers all */
	memset(rx, 0, sizeof(*rx));
	memset(tx, 0, sizeof(*tx));
}

STATIC void
mvxpe_ring_init_queue(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_desc *rxd = MVXPE_RX_RING_MEM_VA(sc, q);
	struct mvxpe_tx_desc *txd = MVXPE_TX_RING_MEM_VA(sc, q);
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	static const int rx_default_queue_len[] = {
		MVXPE_RX_QUEUE_LIMIT_0, MVXPE_RX_QUEUE_LIMIT_1,
		MVXPE_RX_QUEUE_LIMIT_2, MVXPE_RX_QUEUE_LIMIT_3,
		MVXPE_RX_QUEUE_LIMIT_4, MVXPE_RX_QUEUE_LIMIT_5,
		MVXPE_RX_QUEUE_LIMIT_6, MVXPE_RX_QUEUE_LIMIT_7,
	};
	static const int tx_default_queue_len[] = {
		MVXPE_TX_QUEUE_LIMIT_0, MVXPE_TX_QUEUE_LIMIT_1,
		MVXPE_TX_QUEUE_LIMIT_2, MVXPE_TX_QUEUE_LIMIT_3,
		MVXPE_TX_QUEUE_LIMIT_4, MVXPE_TX_QUEUE_LIMIT_5,
		MVXPE_TX_QUEUE_LIMIT_6, MVXPE_TX_QUEUE_LIMIT_7,
	};
	extern uint32_t mvTclk;
	int i;

	/* Rx handle */
	for (i = 0; i < MVXPE_RX_RING_CNT; i++) {
		MVXPE_RX_DESC(sc, q, i) = &rxd[i];
		MVXPE_RX_DESC_OFF(sc, q, i) = sizeof(struct mvxpe_rx_desc) * i;
		MVXPE_RX_PKTBUF(sc, q, i) = NULL;
	}
	mutex_init(&rx->rx_ring_mtx, MUTEX_DEFAULT, IPL_NET);
	rx->rx_dma = rx->rx_cpu = 0;
	rx->rx_queue_len = rx_default_queue_len[q];
	if (rx->rx_queue_len > MVXPE_RX_RING_CNT)
		rx->rx_queue_len = MVXPE_RX_RING_CNT;
	rx->rx_queue_th_received = rx->rx_queue_len / MVXPE_RXTH_RATIO;
	rx->rx_queue_th_free = rx->rx_queue_len / MVXPE_RXTH_REFILL_RATIO;
	rx->rx_queue_th_time = (mvTclk / 1000) / 2; /* 0.5 [ms] */

	/* Tx handle */
	for (i = 0; i < MVXPE_TX_RING_CNT; i++) {
		MVXPE_TX_DESC(sc, q, i) = &txd[i];
		MVXPE_TX_DESC_OFF(sc, q, i) = sizeof(struct mvxpe_tx_desc) * i;
		MVXPE_TX_MBUF(sc, q, i) = NULL;
		/* Tx handle needs DMA map for busdma_load_mbuf() */
		if (bus_dmamap_create(sc->sc_dmat,
		    mvxpbm_chunk_size(sc->sc_bm),
		    MVXPE_TX_SEGLIMIT, mvxpbm_chunk_size(sc->sc_bm), 0,
		    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &MVXPE_TX_MAP(sc, q, i))) {
			aprint_error_dev(sc->sc_dev,
			    "can't create dma map (tx ring %d)\n", i);
		}
	}
	mutex_init(&tx->tx_ring_mtx, MUTEX_DEFAULT, IPL_NET);
	tx->tx_dma = tx->tx_cpu = 0;
	tx->tx_queue_len = tx_default_queue_len[q];
	if (tx->tx_queue_len > MVXPE_TX_RING_CNT)
		tx->tx_queue_len = MVXPE_TX_RING_CNT;
       	tx->tx_used = 0;
	tx->tx_queue_th_free = tx->tx_queue_len / MVXPE_TXTH_RATIO;
}

STATIC void
mvxpe_ring_flush_queue(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	int i;

	KASSERT_RX_MTX(sc, q);
	KASSERT_TX_MTX(sc, q);

	/* Rx handle */
	for (i = 0; i < MVXPE_RX_RING_CNT; i++) {
		if (MVXPE_RX_PKTBUF(sc, q, i) == NULL)
			continue;
		mvxpbm_free_chunk(MVXPE_RX_PKTBUF(sc, q, i));
		MVXPE_RX_PKTBUF(sc, q, i) = NULL;
	}
	rx->rx_dma = rx->rx_cpu = 0;

	/* Tx handle */
	for (i = 0; i < MVXPE_TX_RING_CNT; i++) {
		if (MVXPE_TX_MBUF(sc, q, i) == NULL)
			continue;
		bus_dmamap_unload(sc->sc_dmat, MVXPE_TX_MAP(sc, q, i));
		m_freem(MVXPE_TX_MBUF(sc, q, i));
		MVXPE_TX_MBUF(sc, q, i) = NULL;
	}
	tx->tx_dma = tx->tx_cpu = 0;
       	tx->tx_used = 0;
}

STATIC void
mvxpe_ring_sync_rx(struct mvxpe_softc *sc, int q, int idx, int count, int ops)
{
	int wrap;

	KASSERT_RX_MTX(sc, q);
	KASSERT(count > 0 && count <= MVXPE_RX_RING_CNT);
	KASSERT(idx >= 0 && idx < MVXPE_RX_RING_CNT);

	wrap = (idx + count) - MVXPE_RX_RING_CNT;
	if (wrap > 0) {
		count -= wrap;
		KASSERT(count > 0);
		bus_dmamap_sync(sc->sc_dmat, MVXPE_RX_RING_MEM_MAP(sc, q),
		    0, sizeof(struct mvxpe_rx_desc) * wrap, ops);
	}
	bus_dmamap_sync(sc->sc_dmat, MVXPE_RX_RING_MEM_MAP(sc, q),
	    MVXPE_RX_DESC_OFF(sc, q, idx),
	    sizeof(struct mvxpe_rx_desc) * count, ops);
}

STATIC void
mvxpe_ring_sync_tx(struct mvxpe_softc *sc, int q, int idx, int count, int ops)
{
	int wrap = 0;

	KASSERT_TX_MTX(sc, q);
	KASSERT(count > 0 && count <= MVXPE_TX_RING_CNT);
	KASSERT(idx >= 0 && idx < MVXPE_TX_RING_CNT);

	wrap = (idx + count) - MVXPE_TX_RING_CNT;
	if (wrap > 0)  {
		count -= wrap;
		bus_dmamap_sync(sc->sc_dmat, MVXPE_TX_RING_MEM_MAP(sc, q),
		    0, sizeof(struct mvxpe_tx_desc) * wrap, ops);
	}
	bus_dmamap_sync(sc->sc_dmat, MVXPE_TX_RING_MEM_MAP(sc, q),
	    MVXPE_TX_DESC_OFF(sc, q, idx),
	    sizeof(struct mvxpe_tx_desc) * count, ops);
}

/*
 * Rx/Tx Queue Control
 */
STATIC int
mvxpe_rx_queue_init(struct ifnet *ifp, int q)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	uint32_t reg;

	KASSERT_RX_MTX(sc, q);
	KASSERT(MVXPE_RX_RING_MEM_PA(sc, q) != 0);

	/* descriptor address */
	MVXPE_WRITE(sc, MVXPE_PRXDQA(q), MVXPE_RX_RING_MEM_PA(sc, q));

	/* Rx buffer size and descriptor ring size */
	reg  = MVXPE_PRXDQS_BUFFERSIZE(mvxpbm_chunk_size(sc->sc_bm) >> 3);
	reg |= MVXPE_PRXDQS_DESCRIPTORSQUEUESIZE(MVXPE_RX_RING_CNT);
	MVXPE_WRITE(sc, MVXPE_PRXDQS(q), reg);
	DPRINTIFNET(ifp, 1, "PRXDQS(%d): %#x\n",
	    q, MVXPE_READ(sc, MVXPE_PRXDQS(q)));

	/* Rx packet offset address */
	reg = MVXPE_PRXC_PACKETOFFSET(mvxpbm_packet_offset(sc->sc_bm) >> 3);
	MVXPE_WRITE(sc, MVXPE_PRXC(q), reg);
	DPRINTIFNET(ifp, 1, "PRXC(%d): %#x\n",
	    q, MVXPE_READ(sc, MVXPE_PRXC(q)));

	/* Rx DMA SNOOP */
	reg  = MVXPE_PRXSNP_SNOOPNOOFBYTES(MVXPE_MRU);
	reg |= MVXPE_PRXSNP_L2DEPOSITNOOFBYTES(MVXPE_MRU);
	MVXPE_WRITE(sc, MVXPE_PRXSNP(q), reg);

	/* if DMA is not working, register is not updated */
	KASSERT(MVXPE_READ(sc, MVXPE_PRXDQA(q)) == MVXPE_RX_RING_MEM_PA(sc, q));
	return 0;
}

STATIC int
mvxpe_tx_queue_init(struct ifnet *ifp, int q)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	uint32_t reg;

	KASSERT_TX_MTX(sc, q);
	KASSERT(MVXPE_TX_RING_MEM_PA(sc, q) != 0);

	/* descriptor address */
	MVXPE_WRITE(sc, MVXPE_PTXDQA(q), MVXPE_TX_RING_MEM_PA(sc, q));

	/* Tx threshold, and descriptor ring size */
	reg  = MVXPE_PTXDQS_TBT(tx->tx_queue_th_free);
	reg |= MVXPE_PTXDQS_DQS(MVXPE_TX_RING_CNT);
	MVXPE_WRITE(sc, MVXPE_PTXDQS(q), reg);
	DPRINTIFNET(ifp, 1, "PTXDQS(%d): %#x\n",
	    q, MVXPE_READ(sc, MVXPE_PTXDQS(q)));

	/* if DMA is not working, register is not updated */
	KASSERT(MVXPE_READ(sc, MVXPE_PTXDQA(q)) == MVXPE_TX_RING_MEM_PA(sc, q));
	return 0;
}

STATIC int
mvxpe_rx_queue_enable(struct ifnet *ifp, int q)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	uint32_t reg;

	KASSERT_RX_MTX(sc, q);

	/* Set Rx interrupt threshold */
	reg  = MVXPE_PRXDQTH_ODT(rx->rx_queue_th_received);
	reg |= MVXPE_PRXDQTH_NODT(rx->rx_queue_th_free);
	MVXPE_WRITE(sc, MVXPE_PRXDQTH(q), reg);

	reg  = MVXPE_PRXITTH_RITT(rx->rx_queue_th_time);
	MVXPE_WRITE(sc, MVXPE_PRXITTH(q), reg);

	/* Unmask RXTX Intr. */
	reg = MVXPE_READ(sc, MVXPE_PRXTXIM);
	reg |= MVXPE_PRXTXI_RREQ(q); /* Rx resource error */
	MVXPE_WRITE(sc, MVXPE_PRXTXIM, reg);

	/* Unmask RXTX_TH Intr. */
	reg = MVXPE_READ(sc, MVXPE_PRXTXTIM);
	reg |= MVXPE_PRXTXTI_RBICTAPQ(q); /* Rx Buffer Interrupt Coalese */
	reg |= MVXPE_PRXTXTI_RDTAQ(q); /* Rx Descriptor Alart */
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, reg);

	/* Enable Rx queue */
	reg = MVXPE_READ(sc, MVXPE_RQC) & MVXPE_RQC_EN_MASK;
	reg |= MVXPE_RQC_ENQ(q);
	MVXPE_WRITE(sc, MVXPE_RQC, reg);

	return 0;
}

STATIC int
mvxpe_tx_queue_enable(struct ifnet *ifp, int q)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	uint32_t reg;

	KASSERT_TX_MTX(sc, q);

	/* Set Tx interrupt threshold */
	reg  = MVXPE_READ(sc, MVXPE_PTXDQS(q));
	reg &= ~MVXPE_PTXDQS_TBT_MASK; /* keep queue size */
	reg |= MVXPE_PTXDQS_TBT(tx->tx_queue_th_free);
	MVXPE_WRITE(sc, MVXPE_PTXDQS(q), reg);

	/* Unmask RXTX_TH Intr. */
	reg = MVXPE_READ(sc, MVXPE_PRXTXTIM);
	reg |= MVXPE_PRXTXTI_TBTCQ(q); /* Tx Threshold cross */
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, reg);

	/* Don't update MVXPE_TQC here, there is no packet yet. */
	return 0;
}

STATIC void
mvxpe_rx_lockq(struct mvxpe_softc *sc, int q)
{
	KASSERT(q >= 0);
	KASSERT(q < MVXPE_QUEUE_SIZE);
	mutex_enter(&sc->sc_rx_ring[q].rx_ring_mtx);
}

STATIC void
mvxpe_rx_unlockq(struct mvxpe_softc *sc, int q)
{
	KASSERT(q >= 0);
	KASSERT(q < MVXPE_QUEUE_SIZE);
	mutex_exit(&sc->sc_rx_ring[q].rx_ring_mtx);
}

STATIC void
mvxpe_tx_lockq(struct mvxpe_softc *sc, int q)
{
	KASSERT(q >= 0);
	KASSERT(q < MVXPE_QUEUE_SIZE);
	mutex_enter(&sc->sc_tx_ring[q].tx_ring_mtx);
}

STATIC void
mvxpe_tx_unlockq(struct mvxpe_softc *sc, int q)
{
	KASSERT(q >= 0);
	KASSERT(q < MVXPE_QUEUE_SIZE);
	mutex_exit(&sc->sc_tx_ring[q].tx_ring_mtx);
}

/*
 * Interrupt Handlers
 */
STATIC void
mvxpe_disable_intr(struct mvxpe_softc *sc)
{
	MVXPE_WRITE(sc, MVXPE_EUIM, 0);
	MVXPE_WRITE(sc, MVXPE_EUIC, 0);
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, 0);
	MVXPE_WRITE(sc, MVXPE_PRXTXTIC, 0);
	MVXPE_WRITE(sc, MVXPE_PRXTXIM, 0);
	MVXPE_WRITE(sc, MVXPE_PRXTXIC, 0);
	MVXPE_WRITE(sc, MVXPE_PMIM, 0);
	MVXPE_WRITE(sc, MVXPE_PMIC, 0);
	MVXPE_WRITE(sc, MVXPE_PIE, 0);
}

STATIC void
mvxpe_enable_intr(struct mvxpe_softc *sc)
{
	uint32_t reg;

	/* Enable Port MISC Intr. (via RXTX_TH_Summary bit) */
	reg  = MVXPE_READ(sc, MVXPE_PMIM);
	reg |= MVXPE_PMI_PHYSTATUSCHNG;
	reg |= MVXPE_PMI_LINKCHANGE;
	reg |= MVXPE_PMI_IAE;
	reg |= MVXPE_PMI_RXOVERRUN;
	reg |= MVXPE_PMI_RXCRCERROR;
	reg |= MVXPE_PMI_RXLARGEPACKET;
	reg |= MVXPE_PMI_TXUNDRN;
	reg |= MVXPE_PMI_PRBSERROR;
	reg |= MVXPE_PMI_SRSE;
	reg |= MVXPE_PMI_TREQ_MASK;
	MVXPE_WRITE(sc, MVXPE_PMIM, reg);

	/* Enable RXTX Intr. (via RXTX_TH Summary bit) */
	reg  = MVXPE_READ(sc, MVXPE_PRXTXIM);
	reg |= MVXPE_PRXTXI_RREQ_MASK; /* Rx resource error */
	MVXPE_WRITE(sc, MVXPE_PRXTXIM, reg);

	/* Enable Summary Bit to check all interrupt cause. */
	reg  = MVXPE_READ(sc, MVXPE_PRXTXTIM);
	reg |= MVXPE_PRXTXTI_PMISCICSUMMARY;
	reg |= MVXPE_PRXTXTI_PTXERRORSUMMARY;
	reg |= MVXPE_PRXTXTI_PRXTXICSUMMARY;
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, reg);

	/* Enable All Queue Interrupt */
	reg  = MVXPE_READ(sc, MVXPE_PIE);
	reg |= MVXPE_PIE_RXPKTINTRPTENB_MASK;
	reg |= MVXPE_PIE_TXPKTINTRPTENB_MASK;
	MVXPE_WRITE(sc, MVXPE_PIE, reg);
}

STATIC int
mvxpe_rxtxth_intr(void *arg)
{
	struct mvxpe_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t ic, queues, datum = 0;

	DPRINTSC(sc, 2, "got RXTX_TH_Intr\n");
	MVXPE_EVCNT_INCR(&sc->sc_ev.ev_i_rxtxth);

	mvxpe_sc_lock(sc);
	ic = MVXPE_READ(sc, MVXPE_PRXTXTIC);
	if (ic == 0)
		return 0;
	MVXPE_WRITE(sc, MVXPE_PRXTXTIC, ~ic);
	datum = datum ^ ic;

	DPRINTIFNET(ifp, 2, "PRXTXTIC: %#x\n", ic);

	/* ack maintance interrupt first */
	if (ic & MVXPE_PRXTXTI_PTXERRORSUMMARY) {
		DPRINTIFNET(ifp, 1, "PRXTXTIC: +PTXERRORSUMMARY\n");
		MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtxth_txerr);
	}
	if ((ic & MVXPE_PRXTXTI_PMISCICSUMMARY)) {
		DPRINTIFNET(ifp, 2, "PTXTXTIC: +PMISCICSUMMARY\n");
		mvxpe_misc_intr(sc);
	}
	if (ic & MVXPE_PRXTXTI_PRXTXICSUMMARY) {
		DPRINTIFNET(ifp, 2, "PTXTXTIC: +PRXTXICSUMMARY\n");
		mvxpe_rxtx_intr(sc);
	}
	if (!(ifp->if_flags & IFF_RUNNING))
		return 1;

	/* RxTxTH interrupt */
	queues = MVXPE_PRXTXTI_GET_RBICTAPQ(ic);
	if (queues) {
		DPRINTIFNET(ifp, 2, "PRXTXTIC: +RXEOF\n");
		mvxpe_rx(sc, queues);
	}
	queues = MVXPE_PRXTXTI_GET_TBTCQ(ic);
	if (queues) {
		DPRINTIFNET(ifp, 2, "PRXTXTIC: +TBTCQ\n");
		mvxpe_tx_complete(sc, queues);
	}
	queues = MVXPE_PRXTXTI_GET_RDTAQ(ic);
	if (queues) {
		DPRINTIFNET(ifp, 2, "PRXTXTIC: +RDTAQ\n");
		mvxpe_rx_refill(sc, queues);
	}
	mvxpe_sc_unlock(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		mvxpe_start(ifp);

	rnd_add_uint32(&sc->sc_rnd_source, datum);

	return 1;
}

STATIC int
mvxpe_misc_intr(void *arg)
{
	struct mvxpe_softc *sc = arg;
#ifdef MVXPE_DEBUG
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#endif
	uint32_t ic;
	uint32_t datum = 0;
	int claimed = 0;

	DPRINTSC(sc, 2, "got MISC_INTR\n");
	MVXPE_EVCNT_INCR(&sc->sc_ev.ev_i_misc);

	KASSERT_SC_MTX(sc);

	for (;;) {
		ic = MVXPE_READ(sc, MVXPE_PMIC);
		ic &= MVXPE_READ(sc, MVXPE_PMIM);
		if (ic == 0)
			break;
		MVXPE_WRITE(sc, MVXPE_PMIC, ~ic);
		datum = datum ^ ic;
		claimed = 1;

		DPRINTIFNET(ifp, 2, "PMIC=%#x\n", ic);
		if (ic & MVXPE_PMI_PHYSTATUSCHNG) {
			DPRINTIFNET(ifp, 2, "+PHYSTATUSCHNG\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_phystatuschng);
		}
		if (ic & MVXPE_PMI_LINKCHANGE) {
			DPRINTIFNET(ifp, 2, "+LINKCHANGE\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_linkchange);
			mvxpe_linkupdate(sc);
		}
		if (ic & MVXPE_PMI_IAE) {
			DPRINTIFNET(ifp, 2, "+IAE\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_iae);
		}
		if (ic & MVXPE_PMI_RXOVERRUN) {
			DPRINTIFNET(ifp, 2, "+RXOVERRUN\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_rxoverrun);
		}
		if (ic & MVXPE_PMI_RXCRCERROR) {
			DPRINTIFNET(ifp, 2, "+RXCRCERROR\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_rxcrc);
		}
		if (ic & MVXPE_PMI_RXLARGEPACKET) {
			DPRINTIFNET(ifp, 2, "+RXLARGEPACKET\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_rxlargepacket);
		}
		if (ic & MVXPE_PMI_TXUNDRN) {
			DPRINTIFNET(ifp, 2, "+TXUNDRN\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_txunderrun);
		}
		if (ic & MVXPE_PMI_PRBSERROR) {
			DPRINTIFNET(ifp, 2, "+PRBSERROR\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_prbserr);
		}
		if (ic & MVXPE_PMI_TREQ_MASK) {
			DPRINTIFNET(ifp, 2, "+TREQ\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_misc_txreq);
		}
	}
	if (datum)
		rnd_add_uint32(&sc->sc_rnd_source, datum);

	return claimed;
}

STATIC int
mvxpe_rxtx_intr(void *arg)
{
	struct mvxpe_softc *sc = arg;
#ifdef MVXPE_DEBUG
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#endif
	uint32_t datum = 0;
	uint32_t prxtxic;
	int claimed = 0;

	DPRINTSC(sc, 2, "got RXTX_Intr\n");
	MVXPE_EVCNT_INCR(&sc->sc_ev.ev_i_rxtx);

	KASSERT_SC_MTX(sc);

	for (;;) {
		prxtxic = MVXPE_READ(sc, MVXPE_PRXTXIC);
		prxtxic &= MVXPE_READ(sc, MVXPE_PRXTXIM);
		if (prxtxic == 0)
			break;
		MVXPE_WRITE(sc, MVXPE_PRXTXIC, ~prxtxic);
		datum = datum ^ prxtxic;
		claimed = 1;

		DPRINTSC(sc, 2, "PRXTXIC: %#x\n", prxtxic);

		if (prxtxic & MVXPE_PRXTXI_RREQ_MASK) {
			DPRINTIFNET(ifp, 1, "Rx Resource Error.\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_rreq);
		}
		if (prxtxic & MVXPE_PRXTXI_RPQ_MASK) {
			DPRINTIFNET(ifp, 1, "Rx Packet in Queue.\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_rpq);
		}
		if (prxtxic & MVXPE_PRXTXI_TBRQ_MASK) {
			DPRINTIFNET(ifp, 1, "Tx Buffer Return.\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_tbrq);
		}
		if (prxtxic & MVXPE_PRXTXI_PRXTXTHICSUMMARY) {
			DPRINTIFNET(ifp, 1, "PRXTXTHIC Sumary\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_rxtxth);
		}
		if (prxtxic & MVXPE_PRXTXI_PTXERRORSUMMARY) {
			DPRINTIFNET(ifp, 1, "PTXERROR Sumary\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_txerr);
		}
		if (prxtxic & MVXPE_PRXTXI_PMISCICSUMMARY) {
			DPRINTIFNET(ifp, 1, "PMISCIC Sumary\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxtx_misc);
		}
	}
	if (datum)
		rnd_add_uint32(&sc->sc_rnd_source, datum);

	return claimed;
}

STATIC void
mvxpe_tick(void *arg)
{
	struct mvxpe_softc *sc = arg;
	struct mii_data *mii = &sc->sc_mii;

	mvxpe_sc_lock(sc);

	mii_tick(mii);
	mii_pollstat(&sc->sc_mii);

	/* read mib regisers(clear by read) */
	mvxpe_update_mib(sc);

	/* read counter registers(clear by read) */
	MVXPE_EVCNT_ADD(&sc->sc_ev.ev_reg_pdfc,
	    MVXPE_READ(sc, MVXPE_PDFC));
	MVXPE_EVCNT_ADD(&sc->sc_ev.ev_reg_pofc,
	    MVXPE_READ(sc, MVXPE_POFC));
	MVXPE_EVCNT_ADD(&sc->sc_ev.ev_reg_txbadfcs,
	    MVXPE_READ(sc, MVXPE_TXBADFCS));
	MVXPE_EVCNT_ADD(&sc->sc_ev.ev_reg_txdropped,
	    MVXPE_READ(sc, MVXPE_TXDROPPED));
	MVXPE_EVCNT_ADD(&sc->sc_ev.ev_reg_lpic,
	    MVXPE_READ(sc, MVXPE_LPIC));

	mvxpe_sc_unlock(sc);

	callout_schedule(&sc->sc_tick_ch, hz);
}


/*
 * struct ifnet and mii callbacks
 */
STATIC void
mvxpe_start(struct ifnet *ifp)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int q;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING) {
		DPRINTIFNET(ifp, 1, "not running\n");
		return;
	}

	mvxpe_sc_lock(sc);
	if (!MVXPE_IS_LINKUP(sc)) {
		/* If Link is DOWN, can't start TX */
		DPRINTIFNET(ifp, 1, "link fail\n");
		for (;;) {
			/*
			 * discard stale packets all.
			 * these may confuse DAD, ARP or timer based protocols.
			 */
			IFQ_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL)
				break;
			m_freem(m);
		}
		mvxpe_sc_unlock(sc);
		return;
	}
	for (;;) {
		/*
		 * don't use IFQ_POLL().
		 * there is lock problem between IFQ_POLL and IFQ_DEQUEUE
		 * on SMP enabled networking stack.
		 */ 
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		q = mvxpe_tx_queue_select(sc, m);
		if (q < 0)
			break;
		/* mutex is held in mvxpe_tx_queue_select() */

		if (mvxpe_tx_queue(sc, m, q) != 0) {
			DPRINTIFNET(ifp, 1, "cannot add packet to tx ring\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_txerr);
			mvxpe_tx_unlockq(sc, q);
			break;
		}
		mvxpe_tx_unlockq(sc, q);
		KASSERT(sc->sc_tx_ring[q].tx_used >= 0);
		KASSERT(sc->sc_tx_ring[q].tx_used <=
		    sc->sc_tx_ring[q].tx_queue_len);
		DPRINTIFNET(ifp, 1, "a packet is added to tx ring\n");
		sc->sc_tx_pending++;
		ifp->if_timer = 1;
		sc->sc_wdogsoft = 1;
		bpf_mtap(ifp, m);
	}
	mvxpe_sc_unlock(sc);

	return;
}

STATIC int
mvxpe_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = data;
	int error = 0;
	int s;

	switch (cmd) {
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		DPRINTIFNET(ifp, 2, "mvxpe_ioctl MEDIA\n");
		s = splnet(); /* XXX: is there suitable mutex? */
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		splx(s);
		break;
	default:
		DPRINTIFNET(ifp, 2, "mvxpe_ioctl ETHER\n");
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				mvxpe_sc_lock(sc);
				mvxpe_filter_setup(sc);
				mvxpe_sc_unlock(sc);
			}
			error = 0;
		}
		break;
	}

	return error;
}

STATIC int
mvxpe_init(struct ifnet *ifp)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	uint32_t reg;
	int q;

	mvxpe_sc_lock(sc);

	/* Start DMA Engine */
	MVXPE_WRITE(sc, MVXPE_PRXINIT, 0x00000000);
	MVXPE_WRITE(sc, MVXPE_PTXINIT, 0x00000000);
	MVXPE_WRITE(sc, MVXPE_PACC, MVXPE_PACC_ACCELERATIONMODE_EDM);

	/* Enable port */
	reg  = MVXPE_READ(sc, MVXPE_PMACC0);
	reg |= MVXPE_PMACC0_PORTEN;
	MVXPE_WRITE(sc, MVXPE_PMACC0, reg);

	/* Link up */
	mvxpe_linkup(sc);

	/* Enable All Queue and interrupt of each Queue */
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		mvxpe_rx_lockq(sc, q);
		mvxpe_rx_queue_enable(ifp, q);
		mvxpe_rx_queue_refill(sc, q);
		mvxpe_rx_unlockq(sc, q);

		mvxpe_tx_lockq(sc, q);
		mvxpe_tx_queue_enable(ifp, q);
		mvxpe_tx_unlockq(sc, q);
	}

	/* Enable interrupt */
	mvxpe_enable_intr(sc);

	/* Set Counter */
	callout_schedule(&sc->sc_tick_ch, hz);

	/* Media check */
	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	mvxpe_sc_unlock(sc);
	return 0;
}

/* ARGSUSED */
STATIC void
mvxpe_stop(struct ifnet *ifp, int disable)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	uint32_t reg;
	int q, cnt;

	DPRINTIFNET(ifp, 1, "stop device dma and interrupts.\n");

	mvxpe_sc_lock(sc);

	callout_stop(&sc->sc_tick_ch);

	/* Link down */
	mvxpe_linkdown(sc);

	/* Disable Rx interrupt */
	reg  = MVXPE_READ(sc, MVXPE_PIE);
	reg &= ~MVXPE_PIE_RXPKTINTRPTENB_MASK;
	MVXPE_WRITE(sc, MVXPE_PIE, reg);

	reg  = MVXPE_READ(sc, MVXPE_PRXTXTIM);
	reg &= ~MVXPE_PRXTXTI_RBICTAPQ_MASK;
	reg &= ~MVXPE_PRXTXTI_RDTAQ_MASK;
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, reg);

	/* Wait for all Rx activity to terminate. */
	reg = MVXPE_READ(sc, MVXPE_RQC) & MVXPE_RQC_EN_MASK;
	reg = MVXPE_RQC_DIS(reg);
	MVXPE_WRITE(sc, MVXPE_RQC, reg);
	cnt = 0;
	do {
		if (cnt >= RX_DISABLE_TIMEOUT) {
			aprint_error_ifnet(ifp,
			    "timeout for RX stopped. rqc 0x%x\n", reg);
			break;
		}
		cnt++;
		reg = MVXPE_READ(sc, MVXPE_RQC);
	} while (reg & MVXPE_RQC_EN_MASK);

	/* Wait for all Tx activety to terminate. */
	reg  = MVXPE_READ(sc, MVXPE_PIE);
	reg &= ~MVXPE_PIE_TXPKTINTRPTENB_MASK;
	MVXPE_WRITE(sc, MVXPE_PIE, reg);

	reg  = MVXPE_READ(sc, MVXPE_PRXTXTIM);
	reg &= ~MVXPE_PRXTXTI_TBTCQ_MASK;
	MVXPE_WRITE(sc, MVXPE_PRXTXTIM, reg);

	reg = MVXPE_READ(sc, MVXPE_TQC) & MVXPE_TQC_EN_MASK;
	reg = MVXPE_TQC_DIS(reg);
	MVXPE_WRITE(sc, MVXPE_TQC, reg);
	cnt = 0;
	do {
		if (cnt >= TX_DISABLE_TIMEOUT) {
			aprint_error_ifnet(ifp,
			    "timeout for TX stopped. tqc 0x%x\n", reg);
			break;
		}
		cnt++;
		reg = MVXPE_READ(sc, MVXPE_TQC);
	} while (reg & MVXPE_TQC_EN_MASK);

	/* Wait for all Tx FIFO is empty */
	cnt = 0;
	do {
		if (cnt >= TX_FIFO_EMPTY_TIMEOUT) {
			aprint_error_ifnet(ifp,
			    "timeout for TX FIFO drained. ps0 0x%x\n", reg);
			break;
		}
		cnt++;
		reg = MVXPE_READ(sc, MVXPE_PS0);
	} while (!(reg & MVXPE_PS0_TXFIFOEMP) && (reg & MVXPE_PS0_TXINPROG));

	/* Reset the MAC Port Enable bit */
	reg = MVXPE_READ(sc, MVXPE_PMACC0);
	reg &= ~MVXPE_PMACC0_PORTEN;
	MVXPE_WRITE(sc, MVXPE_PMACC0, reg);

	/* Disable each of queue */
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);

		mvxpe_rx_lockq(sc, q);
		mvxpe_tx_lockq(sc, q);

		/* Disable Rx packet buffer refill request */
		reg  = MVXPE_PRXDQTH_ODT(rx->rx_queue_th_received);
		reg |= MVXPE_PRXDQTH_NODT(0);
		MVXPE_WRITE(sc, MVXPE_PRXITTH(q), reg);

		if (disable) {
			/*
			 * Hold Reset state of DMA Engine 
			 * (must write 0x0 to restart it)
			 */
			MVXPE_WRITE(sc, MVXPE_PRXINIT, 0x00000001);
			MVXPE_WRITE(sc, MVXPE_PTXINIT, 0x00000001);
			mvxpe_ring_flush_queue(sc, q);
		}

		mvxpe_tx_unlockq(sc, q);
		mvxpe_rx_unlockq(sc, q);
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	mvxpe_sc_unlock(sc);
}

STATIC void
mvxpe_watchdog(struct ifnet *ifp)
{
	struct mvxpe_softc *sc = ifp->if_softc;
	int q;

	mvxpe_sc_lock(sc);

	/*
	 * Reclaim first as there is a possibility of losing Tx completion
	 * interrupts.
	 */
	mvxpe_tx_complete(sc, 0xff);
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);

		if (tx->tx_dma != tx->tx_cpu) {
			if (sc->sc_wdogsoft) {
				/*
				 * There is race condition between CPU and DMA
				 * engine. When DMA engine encounters queue end,
				 * it clears MVXPE_TQC_ENQ bit.
				 * XXX: how about enhanced mode?
				 */
				MVXPE_WRITE(sc, MVXPE_TQC, MVXPE_TQC_ENQ(q));
				ifp->if_timer = 5;
				sc->sc_wdogsoft = 0;
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_wdogsoft);
			} else {
				aprint_error_ifnet(ifp, "watchdog timeout\n");
				ifp->if_oerrors++;
				mvxpe_linkreset(sc);
				mvxpe_sc_unlock(sc);

				/* trigger reinitialize sequence */
				mvxpe_stop(ifp, 1);
				mvxpe_init(ifp);

				mvxpe_sc_lock(sc);
			}
		}
	}
	mvxpe_sc_unlock(sc);
}

STATIC int
mvxpe_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct mvxpe_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	mvxpe_sc_lock(sc);

	if (change != 0)
		sc->sc_if_flags = ifp->if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0) {
		mvxpe_sc_unlock(sc);
		return ENETRESET;
	}

	if ((change & IFF_PROMISC) != 0)
		mvxpe_filter_setup(sc);

	if ((change & IFF_UP) != 0)
		mvxpe_linkreset(sc);

	mvxpe_sc_unlock(sc);
	return 0;
}

STATIC int
mvxpe_mediachange(struct ifnet *ifp)
{
	return ether_mediachange(ifp);
}

STATIC void
mvxpe_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	ether_mediastatus(ifp, ifmr);
}

/*
 * Link State Notify
 */
STATIC void mvxpe_linkupdate(struct mvxpe_softc *sc)
{
	int linkup; /* bool */

	KASSERT_SC_MTX(sc);

	/* tell miibus */
	mii_pollstat(&sc->sc_mii);

	/* syslog */
	linkup = MVXPE_IS_LINKUP(sc);
	if (sc->sc_linkstate == linkup)
		return;

#ifdef DEBUG
	log(LOG_DEBUG,
	    "%s: link %s\n", device_xname(sc->sc_dev), linkup ? "up" : "down");
#endif
	if (linkup)
		MVXPE_EVCNT_INCR(&sc->sc_ev.ev_link_up);
	else
		MVXPE_EVCNT_INCR(&sc->sc_ev.ev_link_down);

	sc->sc_linkstate = linkup;
}

STATIC void
mvxpe_linkup(struct mvxpe_softc *sc)
{
	uint32_t reg;

	KASSERT_SC_MTX(sc);

	/* set EEE parameters */
	reg = MVXPE_READ(sc, MVXPE_LPIC1);
	if (sc->sc_cf.cf_lpi)
		reg |= MVXPE_LPIC1_LPIRE;
	else
		reg &= ~MVXPE_LPIC1_LPIRE;
	MVXPE_WRITE(sc, MVXPE_LPIC1, reg);

	/* set auto-negotiation parameters */
	reg  = MVXPE_READ(sc, MVXPE_PANC);
	if (sc->sc_cf.cf_fc) {
		/* flow control negotiation */
		reg |= MVXPE_PANC_PAUSEADV;
		reg |= MVXPE_PANC_ANFCEN;
	}
	else {
		reg &= ~MVXPE_PANC_PAUSEADV;
		reg &= ~MVXPE_PANC_ANFCEN;
	}
	reg &= ~MVXPE_PANC_FORCELINKFAIL;
	reg &= ~MVXPE_PANC_FORCELINKPASS;
	MVXPE_WRITE(sc, MVXPE_PANC, reg);

	mii_mediachg(&sc->sc_mii);
}

STATIC void
mvxpe_linkdown(struct mvxpe_softc *sc)
{
	struct mii_softc *mii;
	uint32_t reg;

	KASSERT_SC_MTX(sc);
	return;

	reg  = MVXPE_READ(sc, MVXPE_PANC);
	reg |= MVXPE_PANC_FORCELINKFAIL;
	reg &= MVXPE_PANC_FORCELINKPASS;
	MVXPE_WRITE(sc, MVXPE_PANC, reg);

	mii = LIST_FIRST(&sc->sc_mii.mii_phys);
	if (mii)
		mii_phy_down(mii);
}

STATIC void
mvxpe_linkreset(struct mvxpe_softc *sc)
{
	struct mii_softc *mii;

	KASSERT_SC_MTX(sc);

	/* force reset PHY first */
	mii = LIST_FIRST(&sc->sc_mii.mii_phys);
	if (mii)
		mii_phy_reset(mii);

	/* reinit MAC and PHY */
	mvxpe_linkdown(sc);
	if ((sc->sc_if_flags & IFF_UP) != 0)
		mvxpe_linkup(sc);
}

/*
 * Tx Subroutines
 */
STATIC int
mvxpe_tx_queue_select(struct mvxpe_softc *sc, struct mbuf *m)
{
	int q = 0;

	/* XXX: get attribute from ALTQ framework? */
	mvxpe_tx_lockq(sc, q);
	return 0;
}

STATIC int
mvxpe_tx_queue(struct mvxpe_softc *sc, struct mbuf *m, int q)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_dma_segment_t *txsegs;
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	struct mvxpe_tx_desc *t = NULL;
	uint32_t ptxsu;
	int txnsegs;
	int start, used;
	int i;

	KASSERT_TX_MTX(sc, q);
	KASSERT(tx->tx_used >= 0);
	KASSERT(tx->tx_used <= tx->tx_queue_len);

	/* load mbuf using dmamap of 1st descriptor */
	if (bus_dmamap_load_mbuf(sc->sc_dmat,
	    MVXPE_TX_MAP(sc, q, tx->tx_cpu), m, BUS_DMA_NOWAIT) != 0) {
		m_freem(m);
		return ENOBUFS;
	}
	txsegs = MVXPE_TX_MAP(sc, q, tx->tx_cpu)->dm_segs;
	txnsegs = MVXPE_TX_MAP(sc, q, tx->tx_cpu)->dm_nsegs;
	if (txnsegs <= 0 || (txnsegs + tx->tx_used) > tx->tx_queue_len) {
		/* we have no enough descriptors or mbuf is broken */
		bus_dmamap_unload(sc->sc_dmat, MVXPE_TX_MAP(sc, q, tx->tx_cpu));
		m_freem(m);
		return ENOBUFS;
	}
	DPRINTSC(sc, 2, "send packet %p descriptor %d\n", m, tx->tx_cpu);
	KASSERT(MVXPE_TX_MBUF(sc, q, tx->tx_cpu) == NULL);

	/* remember mbuf using 1st descriptor */
	MVXPE_TX_MBUF(sc, q, tx->tx_cpu) = m;
	bus_dmamap_sync(sc->sc_dmat,
	    MVXPE_TX_MAP(sc, q, tx->tx_cpu), 0, m->m_pkthdr.len,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREWRITE);

	/* load to tx descriptors */
	start = tx->tx_cpu;
	used = 0;
	for (i = 0; i < txnsegs; i++) {
		if (__predict_false(txsegs[i].ds_len == 0))
			continue;
		t = MVXPE_TX_DESC(sc, q, tx->tx_cpu);
		t->command = 0;
		t->l4ichk = 0;
		t->flags = 0;
		if (i == 0) {
			/* 1st descriptor */
			t->command |= MVXPE_TX_CMD_W_PACKET_OFFSET(0);
			t->command |= MVXPE_TX_CMD_PADDING;
			t->command |= MVXPE_TX_CMD_F;
			mvxpe_tx_set_csumflag(ifp, t, m);
		}
		t->bufptr = txsegs[i].ds_addr;
		t->bytecnt = txsegs[i].ds_len;
		tx->tx_cpu = tx_counter_adv(tx->tx_cpu, 1);
		tx->tx_used++;
		used++;
	}
	/* t is last descriptor here */
	KASSERT(t != NULL);
	t->command |= MVXPE_TX_CMD_L;

	DPRINTSC(sc, 2, "queue %d, %d descriptors used\n", q, used);
#ifdef MVXPE_DEBUG
	if (mvxpe_debug > 2)
		for (i = start; i <= tx->tx_cpu; i++) {
			t = MVXPE_TX_DESC(sc, q, i);
			mvxpe_dump_txdesc(t, i);
		}
#endif
	mvxpe_ring_sync_tx(sc, q, start, used,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	while (used > 255) {
		ptxsu = MVXPE_PTXSU_NOWD(255);
		MVXPE_WRITE(sc, MVXPE_PTXSU(q), ptxsu);
		used -= 255;
	}
	if (used > 0) {
		ptxsu = MVXPE_PTXSU_NOWD(used);
		MVXPE_WRITE(sc, MVXPE_PTXSU(q), ptxsu);
	}
	MVXPE_WRITE(sc, MVXPE_TQC, MVXPE_TQC_ENQ(q));

	DPRINTSC(sc, 2,
	    "PTXDQA: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PTXDQA(q)));
	DPRINTSC(sc, 2,
	    "PTXDQS: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PTXDQS(q)));
	DPRINTSC(sc, 2,
	    "PTXS: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PTXS(q)));
	DPRINTSC(sc, 2,
	    "PTXDI: queue %d, %d\n", q, MVXPE_READ(sc, MVXPE_PTXDI(q)));
	DPRINTSC(sc, 2, "TQC: %#x\n", MVXPE_READ(sc, MVXPE_TQC));
	DPRINTIFNET(ifp, 2,
	    "Tx: tx_cpu = %d, tx_dma = %d, tx_used = %d\n",
	    tx->tx_cpu, tx->tx_dma, tx->tx_used);
	return 0;
}

STATIC void
mvxpe_tx_set_csumflag(struct ifnet *ifp,
    struct mvxpe_tx_desc *t, struct mbuf *m)
{
	struct ether_header *eh;
	int csum_flags;
	uint32_t iphl = 0, ipoff = 0;

	
       	csum_flags = ifp->if_csum_flags_tx & m->m_pkthdr.csum_flags;

	eh = mtod(m, struct ether_header *);
	switch (htons(eh->ether_type)) {
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		ipoff = ETHER_HDR_LEN;
		break;
	case ETHERTYPE_VLAN:
		ipoff = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		break;
	}

	if (csum_flags & (M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		iphl = M_CSUM_DATA_IPv4_IPHL(m->m_pkthdr.csum_data);
		t->command |= MVXPE_TX_CMD_L3_IP4;
	}
	else if (csum_flags & (M_CSUM_TCPv6|M_CSUM_UDPv6)) {
		iphl = M_CSUM_DATA_IPv6_HL(m->m_pkthdr.csum_data);
		t->command |= MVXPE_TX_CMD_L3_IP6;
	}
	else {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NONE;
		return;
	}


	/* L3 */
	if (csum_flags & M_CSUM_IPv4) {
		t->command |= MVXPE_TX_CMD_IP4_CHECKSUM;
	}

	/* L4 */
	if ((csum_flags & 
	    (M_CSUM_TCPv4|M_CSUM_UDPv4|M_CSUM_TCPv6|M_CSUM_UDPv6)) == 0) {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NONE;
	}
	else if (csum_flags & M_CSUM_TCPv4) {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NOFRAG;
		t->command |= MVXPE_TX_CMD_L4_TCP;
	}
	else if (csum_flags & M_CSUM_UDPv4) {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NOFRAG;
		t->command |= MVXPE_TX_CMD_L4_UDP;
	}
	else if (csum_flags & M_CSUM_TCPv6) {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NOFRAG;
		t->command |= MVXPE_TX_CMD_L4_TCP;
	}
	else if (csum_flags & M_CSUM_UDPv6) {
		t->command |= MVXPE_TX_CMD_L4_CHECKSUM_NOFRAG;
		t->command |= MVXPE_TX_CMD_L4_UDP;
	}

	t->l4ichk = 0;
	t->command |= MVXPE_TX_CMD_IP_HEADER_LEN(iphl >> 2);
	t->command |= MVXPE_TX_CMD_L3_OFFSET(ipoff);
}

STATIC void
mvxpe_tx_complete(struct mvxpe_softc *sc, uint32_t queues)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int q;

	DPRINTSC(sc, 2, "tx completed.\n");

	KASSERT_SC_MTX(sc);

	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		if (!MVXPE_IS_QUEUE_BUSY(queues, q))
			continue;
		mvxpe_tx_lockq(sc, q);
		mvxpe_tx_queue_complete(sc, q);
		mvxpe_tx_unlockq(sc, q);
	}
	KASSERT(sc->sc_tx_pending >= 0);
	if (sc->sc_tx_pending == 0)
		ifp->if_timer = 0;
}

STATIC void
mvxpe_tx_queue_complete(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_tx_ring *tx = MVXPE_TX_RING(sc, q);
	struct mvxpe_tx_desc *t;
	uint32_t ptxs, ptxsu, ndesc;
	int i;

	KASSERT_TX_MTX(sc, q);

	ptxs = MVXPE_READ(sc, MVXPE_PTXS(q));
	ndesc = MVXPE_PTXS_GET_TBC(ptxs);
	if (ndesc == 0)
		return;

	DPRINTSC(sc, 2,
	    "tx complete queue %d, %d descriptors.\n", q, ndesc);

	mvxpe_ring_sync_tx(sc, q, tx->tx_dma, ndesc,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	for (i = 0; i < ndesc; i++) {
		int error = 0;

		t = MVXPE_TX_DESC(sc, q, tx->tx_dma);
		if (t->flags & MVXPE_TX_F_ES) {
			DPRINTSC(sc, 1,
			    "tx error queue %d desc %d\n",
			    q, tx->tx_dma);
			switch (t->flags & MVXPE_TX_F_EC_MASK) {
			case MVXPE_TX_F_EC_LC:
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_txd_lc);
			case MVXPE_TX_F_EC_UR:
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_txd_ur);
			case MVXPE_TX_F_EC_RL:
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_txd_rl);
			default:
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_txd_oth);
			}
			error = 1;
		}
		if (MVXPE_TX_MBUF(sc, q, tx->tx_dma) != NULL) {
			KASSERT((t->command & MVXPE_TX_CMD_F) != 0);
			bus_dmamap_unload(sc->sc_dmat,
			    MVXPE_TX_MAP(sc, q, tx->tx_dma));
			m_freem(MVXPE_TX_MBUF(sc, q, tx->tx_dma));
			MVXPE_TX_MBUF(sc, q, tx->tx_dma) = NULL;
			sc->sc_tx_pending--;
		}
		else
			KASSERT((t->flags & MVXPE_TX_CMD_F) == 0);
		tx->tx_dma = tx_counter_adv(tx->tx_dma, 1);
		tx->tx_used--;
		if (error)
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_txqe[q]);
		else
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_txq[q]);
	}
	KASSERT(tx->tx_used >= 0);
	KASSERT(tx->tx_used <= tx->tx_queue_len);
	while (ndesc > 255) {
		ptxsu = MVXPE_PTXSU_NORB(255);
		MVXPE_WRITE(sc, MVXPE_PTXSU(q), ptxsu);
		ndesc -= 255;
	}
	if (ndesc > 0) {
		ptxsu = MVXPE_PTXSU_NORB(ndesc);
		MVXPE_WRITE(sc, MVXPE_PTXSU(q), ptxsu);
	}
	DPRINTSC(sc, 2,
	    "Tx complete q %d, tx_cpu = %d, tx_dma = %d, tx_used = %d\n",
	    q, tx->tx_cpu, tx->tx_dma, tx->tx_used);
}

/*
 * Rx Subroutines
 */
STATIC void
mvxpe_rx(struct mvxpe_softc *sc, uint32_t queues)
{
	int q, npkt;

	KASSERT_SC_MTX(sc);

	while ( (npkt = mvxpe_rx_queue_select(sc, queues, &q))) {
		/* mutex is held by rx_queue_select */
		mvxpe_rx_queue(sc, q, npkt);
		mvxpe_rx_unlockq(sc, q);
	}
}

STATIC void
mvxpe_rx_queue(struct mvxpe_softc *sc, int q, int npkt)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_rx_desc *r;
	struct mvxpbm_chunk *chunk;
	struct mbuf *m;
	uint32_t prxsu;
	int error = 0;
	int i;

	KASSERT_RX_MTX(sc, q);

	mvxpe_ring_sync_rx(sc, q, rx->rx_dma, npkt,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	for (i = 0; i < npkt; i++) {
		/* get descriptor and packet */
		chunk = MVXPE_RX_PKTBUF(sc, q, rx->rx_dma);
		MVXPE_RX_PKTBUF(sc, q, rx->rx_dma) = NULL;
		r = MVXPE_RX_DESC(sc, q, rx->rx_dma);
		mvxpbm_dmamap_sync(chunk, r->bytecnt, BUS_DMASYNC_POSTREAD);

		/* check errors */
		if (r->status & MVXPE_RX_ES) {
			switch (r->status & MVXPE_RX_EC_MASK) {
			case MVXPE_RX_EC_CE:
				DPRINTIFNET(ifp, 1, "CRC error\n");
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxd_ce);
				break;
			case MVXPE_RX_EC_OR:
				DPRINTIFNET(ifp, 1, "Rx FIFO overrun\n");
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxd_or);
				break;
			case MVXPE_RX_EC_MF:
				DPRINTIFNET(ifp, 1, "Rx too large frame\n");
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxd_mf);
				break;
			case MVXPE_RX_EC_RE:
				DPRINTIFNET(ifp, 1, "Rx resource error\n");
				MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxd_re);
				break;
			}
			error = 1;
			goto rx_done;
		}
		if (!(r->status & MVXPE_RX_F) || !(r->status & MVXPE_RX_L)) {
			DPRINTIFNET(ifp, 1, "not support scatter buf\n");
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_rxd_scat);
			error = 1;
			goto rx_done;
		}

		if (chunk == NULL) {
			device_printf(sc->sc_dev,
			    "got rx interrupt, but no chunk\n");
			error = 1;
			goto rx_done;
		}

		/* extract packet buffer */
		if (mvxpbm_init_mbuf_hdr(chunk) != 0) {
			error = 1;
			goto rx_done;
		}
		m = chunk->m;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = r->bytecnt - ETHER_CRC_LEN;
		m_adj(m, MVXPE_HWHEADER_SIZE); /* strip MH */
		mvxpe_rx_set_csumflag(ifp, r, m);
		ifp->if_ipackets++;
		bpf_mtap(ifp, m);
		(*ifp->if_input)(ifp, m);
		chunk = NULL; /* the BM chunk goes to networking stack now */
rx_done:
		if (chunk) {
			/* rx error. just return the chunk to BM. */
			mvxpbm_free_chunk(chunk);
		}
		if (error)
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_rxqe[q]);
		else
			MVXPE_EVCNT_INCR(&sc->sc_ev.ev_drv_rxq[q]);
		rx->rx_dma = rx_counter_adv(rx->rx_dma, 1);
	}
	/* DMA status update */
	DPRINTSC(sc, 2, "%d packets received from queue %d\n", npkt, q);
	while (npkt > 255) {
		prxsu = MVXPE_PRXSU_NOOFPROCESSEDDESCRIPTORS(255);
		MVXPE_WRITE(sc, MVXPE_PRXSU(q), prxsu);
		npkt -= 255;
	}
	if (npkt > 0) {
		prxsu = MVXPE_PRXSU_NOOFPROCESSEDDESCRIPTORS(npkt);
		MVXPE_WRITE(sc, MVXPE_PRXSU(q), prxsu);
	}

	DPRINTSC(sc, 2,
	    "PRXDQA: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PRXDQA(q)));
	DPRINTSC(sc, 2,
	    "PRXDQS: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PRXDQS(q)));
	DPRINTSC(sc, 2,
	    "PRXS: queue %d, %#x\n", q, MVXPE_READ(sc, MVXPE_PRXS(q)));
	DPRINTSC(sc, 2,
	    "PRXDI: queue %d, %d\n", q, MVXPE_READ(sc, MVXPE_PRXDI(q)));
	DPRINTSC(sc, 2, "RQC: %#x\n", MVXPE_READ(sc, MVXPE_RQC));
	DPRINTIFNET(ifp, 2, "Rx: rx_cpu = %d, rx_dma = %d\n",
	    rx->rx_cpu, rx->rx_dma);
}

STATIC int
mvxpe_rx_queue_select(struct mvxpe_softc *sc, uint32_t queues, int *queue)
{
	uint32_t prxs, npkt;
	int q;

	KASSERT_SC_MTX(sc);
	KASSERT(queue != NULL);
	DPRINTSC(sc, 2, "selecting rx queue\n");

	for (q = MVXPE_QUEUE_SIZE - 1; q >= 0; q--) {
		if (!MVXPE_IS_QUEUE_BUSY(queues, q))
			continue;

		prxs = MVXPE_READ(sc, MVXPE_PRXS(q));
		npkt = MVXPE_PRXS_GET_ODC(prxs);
		if (npkt == 0)
			continue;

		DPRINTSC(sc, 2, 
		    "queue %d selected: prxs=%#x, %u pakcet received.\n",
		    q, prxs, npkt);
		*queue = q;
		mvxpe_rx_lockq(sc, q);
		return npkt;
	}

	return 0;
}

STATIC void
mvxpe_rx_refill(struct mvxpe_softc *sc, uint32_t queues)
{
	int q;

	KASSERT_SC_MTX(sc);

	/* XXX: check rx bit array */
	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		if (!MVXPE_IS_QUEUE_BUSY(queues, q))
			continue;

		mvxpe_rx_lockq(sc, q);
		mvxpe_rx_queue_refill(sc, q);
		mvxpe_rx_unlockq(sc, q);
	}
}

STATIC void
mvxpe_rx_queue_refill(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	uint32_t prxs, prxsu, ndesc;
	int idx, refill = 0;
	int npkt;

	KASSERT_RX_MTX(sc, q);

	prxs = MVXPE_READ(sc, MVXPE_PRXS(q));
	ndesc = MVXPE_PRXS_GET_NODC(prxs) + MVXPE_PRXS_GET_ODC(prxs);
	refill = rx->rx_queue_len - ndesc;
	if (refill <= 0)
		return;
	DPRINTPRXS(2, q);
	DPRINTSC(sc, 2, "%d buffers to refill.\n", refill);

	idx = rx->rx_cpu;
	for (npkt = 0; npkt < refill; npkt++)
		if (mvxpe_rx_queue_add(sc, q) != 0)
			break;
	DPRINTSC(sc, 2, "queue %d, %d buffer refilled.\n", q, npkt);
	if (npkt == 0)
		return;

	mvxpe_ring_sync_rx(sc, q, idx, npkt,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	while (npkt > 255) {
		prxsu = MVXPE_PRXSU_NOOFNEWDESCRIPTORS(255);
		MVXPE_WRITE(sc, MVXPE_PRXSU(q), prxsu);
		npkt -= 255;
	}
	if (npkt > 0) {
		prxsu = MVXPE_PRXSU_NOOFNEWDESCRIPTORS(npkt);
		MVXPE_WRITE(sc, MVXPE_PRXSU(q), prxsu);
	}
	DPRINTPRXS(2, q);
	return;
}

STATIC int
mvxpe_rx_queue_add(struct mvxpe_softc *sc, int q)
{
	struct mvxpe_rx_ring *rx = MVXPE_RX_RING(sc, q);
	struct mvxpe_rx_desc *r;
	struct mvxpbm_chunk *chunk = NULL;

	KASSERT_RX_MTX(sc, q);

	/* Allocate the packet buffer */
	chunk = mvxpbm_alloc(sc->sc_bm);
	if (chunk == NULL) {
		DPRINTSC(sc, 1, "BM chunk allocation failed.\n");
		return ENOBUFS;
	}

	/* Add the packet to descritor */
	KASSERT(MVXPE_RX_PKTBUF(sc, q, rx->rx_cpu) == NULL);
	MVXPE_RX_PKTBUF(sc, q, rx->rx_cpu) = chunk;
	mvxpbm_dmamap_sync(chunk, BM_SYNC_ALL, BUS_DMASYNC_PREREAD);

	r = MVXPE_RX_DESC(sc, q, rx->rx_cpu);
	r->bufptr = chunk->buf_pa;
	DPRINTSC(sc, 9, "chunk added to index %d\n", rx->rx_cpu);
	rx->rx_cpu = rx_counter_adv(rx->rx_cpu, 1);
	return 0;
}

STATIC void
mvxpe_rx_set_csumflag(struct ifnet *ifp,
    struct mvxpe_rx_desc *r, struct mbuf *m0)
{
	uint32_t csum_flags = 0;

	if ((r->status & (MVXPE_RX_IP_HEADER_OK|MVXPE_RX_L3_IP)) == 0)
		return; /* not a IP packet */

	/* L3 */
	if (r->status & MVXPE_RX_L3_IP) {
		csum_flags |= M_CSUM_IPv4;
		if ((r->status & MVXPE_RX_IP_HEADER_OK) == 0) {
			csum_flags |= M_CSUM_IPv4_BAD;
			goto finish;
		}
		else if (r->status & MVXPE_RX_IPV4_FRAGMENT) {
			/*
			 * r->l4chk has partial checksum of each framgment.
			 * but there is no way to use it in NetBSD.
			 */
			return;
		}
	}

	/* L4 */
	switch (r->status & MVXPE_RX_L4_MASK) {
	case MVXPE_RX_L4_TCP:
		if (r->status & MVXPE_RX_L3_IP)
			csum_flags |= M_CSUM_TCPv4;
		else
			csum_flags |= M_CSUM_TCPv6;
		if ((r->status & MVXPE_RX_L4_CHECKSUM_OK) == 0)
			csum_flags |= M_CSUM_TCP_UDP_BAD;
		break;
	case MVXPE_RX_L4_UDP:
		if (r->status & MVXPE_RX_L3_IP)
			csum_flags |= M_CSUM_UDPv4;
		else
			csum_flags |= M_CSUM_UDPv6;
		if ((r->status & MVXPE_RX_L4_CHECKSUM_OK) == 0)
			csum_flags |= M_CSUM_TCP_UDP_BAD;
		break;
	case MVXPE_RX_L4_OTH:
	default:
		break;
	}
finish:
	m0->m_pkthdr.csum_flags |= (csum_flags & ifp->if_csum_flags_rx);
}

/*
 * MAC address filter
 */
STATIC uint8_t
mvxpe_crc8(const uint8_t *data, size_t size)
{
	int bit;
	uint8_t byte;
	uint8_t crc = 0;
	const uint8_t poly = 0x07;

	while(size--)
	  for (byte = *data++, bit = NBBY-1; bit >= 0; bit--)
	    crc = (crc << 1) ^ ((((crc >> 7) ^ (byte >> bit)) & 1) ? poly : 0);

	return crc;
}

CTASSERT(MVXPE_NDFSMT == MVXPE_NDFOMT);

STATIC void
mvxpe_filter_setup(struct mvxpe_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp= &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t dfut[MVXPE_NDFUT], dfsmt[MVXPE_NDFSMT], dfomt[MVXPE_NDFOMT];
	uint32_t pxc;
	int i;
	const uint8_t special[ETHER_ADDR_LEN] = {0x01,0x00,0x5e,0x00,0x00,0x00};

	KASSERT_SC_MTX(sc);

	memset(dfut, 0, sizeof(dfut));
	memset(dfsmt, 0, sizeof(dfsmt));
	memset(dfomt, 0, sizeof(dfomt));

	if (ifp->if_flags & (IFF_ALLMULTI|IFF_PROMISC)) {
		goto allmulti;
	}

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/* ranges are complex and somewhat rare */
			goto allmulti;
		}
		/* chip handles some IPv4 multicast specially */
		if (memcmp(enm->enm_addrlo, special, 5) == 0) {
			i = enm->enm_addrlo[5];
			dfsmt[i>>2] |=
			    MVXPE_DF(i&3, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS);
		} else {
			i = mvxpe_crc8(enm->enm_addrlo, ETHER_ADDR_LEN);
			dfomt[i>>2] |=
			    MVXPE_DF(i&3, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS);
		}

		ETHER_NEXT_MULTI(step, enm);
	}
	goto set;

allmulti:
	if (ifp->if_flags & (IFF_ALLMULTI|IFF_PROMISC)) {
		for (i = 0; i < MVXPE_NDFSMT; i++) {
			dfsmt[i] = dfomt[i] = 
			    MVXPE_DF(0, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS) |
			    MVXPE_DF(1, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS) |
			    MVXPE_DF(2, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS) |
			    MVXPE_DF(3, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS);
		}
	}

set:
	pxc = MVXPE_READ(sc, MVXPE_PXC);
	pxc &= ~MVXPE_PXC_UPM;
	pxc |= MVXPE_PXC_RB | MVXPE_PXC_RBIP | MVXPE_PXC_RBARP;
	if (ifp->if_flags & IFF_BROADCAST) {
		pxc &= ~(MVXPE_PXC_RB | MVXPE_PXC_RBIP | MVXPE_PXC_RBARP);
	}
	if (ifp->if_flags & IFF_PROMISC) {
		pxc |= MVXPE_PXC_UPM;
	}
	MVXPE_WRITE(sc, MVXPE_PXC, pxc);

	/* Set Destination Address Filter Unicast Table */
	i = sc->sc_enaddr[5] & 0xf;		/* last nibble */
	dfut[i>>2] = MVXPE_DF(i&3, MVXPE_DF_QUEUE_ALL | MVXPE_DF_PASS);
	MVXPE_WRITE_REGION(sc, MVXPE_DFUT(0), dfut, MVXPE_NDFUT);

	/* Set Destination Address Filter Multicast Tables */
	MVXPE_WRITE_REGION(sc, MVXPE_DFSMT(0), dfsmt, MVXPE_NDFSMT);
	MVXPE_WRITE_REGION(sc, MVXPE_DFOMT(0), dfomt, MVXPE_NDFOMT);
}

/*
 * sysctl(9)
 */
SYSCTL_SETUP(sysctl_mvxpe, "sysctl mvxpe subtree setup")
{
	int rc;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "mvxpe",
	    SYSCTL_DESCR("mvxpe interface controls"),
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	mvxpe_root_num = node->sysctl_num;
	return;

err:
	aprint_error("%s: syctl_createv failed (rc = %d)\n", __func__, rc);
}

STATIC int
sysctl_read_mib(SYSCTLFN_ARGS)
{
	struct mvxpe_sysctl_mib *arg;
	struct mvxpe_softc *sc;
	struct sysctlnode node;
	uint64_t val;
	int err;

	node = *rnode;
	arg = (struct mvxpe_sysctl_mib *)rnode->sysctl_data;
	if (arg == NULL)
		return EINVAL;

	sc = arg->sc;
	if (sc == NULL)
		return EINVAL;
	if (arg->index < 0 || arg->index > __arraycount(mvxpe_mib_list))
		return EINVAL;
	
	mvxpe_sc_lock(sc);
	val = arg->counter;
	mvxpe_sc_unlock(sc);

	node.sysctl_data = &val;
	err = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (err)
	       return err;
	if (newp)
		return EINVAL;

	return 0;
}


STATIC int
sysctl_clear_mib(SYSCTLFN_ARGS)
{
	struct mvxpe_softc *sc;
	struct sysctlnode node;
	int val;
	int err;

	node = *rnode;
	sc = (struct mvxpe_softc *)rnode->sysctl_data;
	if (sc == NULL)
		return EINVAL;

	val = 0;
	node.sysctl_data = &val;
	err = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (err || newp == NULL)
		return err;
	if (val < 0 || val > 1)
		return EINVAL;
	if (val == 1) {
		mvxpe_sc_lock(sc);
		mvxpe_clear_mib(sc);
		mvxpe_sc_unlock(sc);
	}

	return 0;
}

STATIC int
sysctl_set_queue_length(SYSCTLFN_ARGS)
{
	struct mvxpe_sysctl_queue *arg;
	struct mvxpe_rx_ring *rx = NULL;
	struct mvxpe_tx_ring *tx = NULL;
	struct mvxpe_softc *sc;
	struct sysctlnode node;
	uint32_t reg;
	int val;
	int err;

	node = *rnode;

	arg = (struct mvxpe_sysctl_queue *)rnode->sysctl_data;
	if (arg == NULL)
		return EINVAL;
	if (arg->queue < 0 || arg->queue > MVXPE_RX_RING_CNT)
		return EINVAL;
	if (arg->rxtx != MVXPE_SYSCTL_RX && arg->rxtx != MVXPE_SYSCTL_TX)
		return EINVAL;

	sc = arg->sc;
	if (sc == NULL)
		return EINVAL;

	/* read queue length */
	mvxpe_sc_lock(sc);
	switch (arg->rxtx) {
	case  MVXPE_SYSCTL_RX:
		mvxpe_rx_lockq(sc, arg->queue);
		rx = MVXPE_RX_RING(sc, arg->queue);
		val = rx->rx_queue_len;
		mvxpe_rx_unlockq(sc, arg->queue);
		break;
	case  MVXPE_SYSCTL_TX:
		mvxpe_tx_lockq(sc, arg->queue);
		tx = MVXPE_TX_RING(sc, arg->queue);
		val = tx->tx_queue_len;
		mvxpe_tx_unlockq(sc, arg->queue);
		break;
	}

	node.sysctl_data = &val;
	err = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (err || newp == NULL) {
		mvxpe_sc_unlock(sc);
		return err;
	}

	/* update queue length */
	if (val < 8 || val > MVXPE_RX_RING_CNT) {
		mvxpe_sc_unlock(sc);
		return EINVAL;
	}
	switch (arg->rxtx) {
	case  MVXPE_SYSCTL_RX:
		mvxpe_rx_lockq(sc, arg->queue);
		rx->rx_queue_len = val;
		rx->rx_queue_th_received = 
		    rx->rx_queue_len / MVXPE_RXTH_RATIO;
		rx->rx_queue_th_free = 
		    rx->rx_queue_len / MVXPE_RXTH_REFILL_RATIO;

		reg  = MVXPE_PRXDQTH_ODT(rx->rx_queue_th_received);
		reg |= MVXPE_PRXDQTH_NODT(rx->rx_queue_th_free);
		MVXPE_WRITE(sc, MVXPE_PRXDQTH(arg->queue), reg);

		mvxpe_rx_unlockq(sc, arg->queue);
		break;
	case  MVXPE_SYSCTL_TX:
		mvxpe_tx_lockq(sc, arg->queue);
		tx->tx_queue_len = val;
		tx->tx_queue_th_free =
		    tx->tx_queue_len / MVXPE_TXTH_RATIO;

		reg  = MVXPE_PTXDQS_TBT(tx->tx_queue_th_free);
		reg |= MVXPE_PTXDQS_DQS(MVXPE_TX_RING_CNT);
		MVXPE_WRITE(sc, MVXPE_PTXDQS(arg->queue), reg);

		mvxpe_tx_unlockq(sc, arg->queue);
		break;
	}
	mvxpe_sc_unlock(sc);

	return 0;
}

STATIC int
sysctl_set_queue_rxthtime(SYSCTLFN_ARGS)
{
	struct mvxpe_sysctl_queue *arg;
	struct mvxpe_rx_ring *rx = NULL;
	struct mvxpe_softc *sc;
	struct sysctlnode node;
	extern uint32_t mvTclk;
	uint32_t reg, time_mvtclk;
	int time_us;
	int err;

	node = *rnode;

	arg = (struct mvxpe_sysctl_queue *)rnode->sysctl_data;
	if (arg == NULL)
		return EINVAL;
	if (arg->queue < 0 || arg->queue > MVXPE_RX_RING_CNT)
		return EINVAL;
	if (arg->rxtx != MVXPE_SYSCTL_RX)
		return EINVAL;

	sc = arg->sc;
	if (sc == NULL)
		return EINVAL;

	/* read queue length */
	mvxpe_sc_lock(sc);
	mvxpe_rx_lockq(sc, arg->queue);
	rx = MVXPE_RX_RING(sc, arg->queue);
	time_mvtclk = rx->rx_queue_th_time;
	time_us = ((uint64_t)time_mvtclk * 1000ULL * 1000ULL) / mvTclk;
	node.sysctl_data = &time_us;
	DPRINTSC(sc, 1, "RXITTH(%d) => %#x\n",
	    arg->queue, MVXPE_READ(sc, MVXPE_PRXITTH(arg->queue)));
	err = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (err || newp == NULL) {
		mvxpe_rx_unlockq(sc, arg->queue);
		mvxpe_sc_unlock(sc);
		return err;
	}

	/* update queue length (0[sec] - 1[sec]) */
	if (time_us < 0 || time_us > (1000 * 1000)) {
		mvxpe_rx_unlockq(sc, arg->queue);
		mvxpe_sc_unlock(sc);
		return EINVAL;
	}
	time_mvtclk =
	    (uint64_t)mvTclk * (uint64_t)time_us / (1000ULL * 1000ULL);
	rx->rx_queue_th_time = time_mvtclk;
	reg = MVXPE_PRXITTH_RITT(rx->rx_queue_th_time);
	MVXPE_WRITE(sc, MVXPE_PRXITTH(arg->queue), reg);
	DPRINTSC(sc, 1, "RXITTH(%d) => %#x\n", arg->queue, reg);
	mvxpe_rx_unlockq(sc, arg->queue);
	mvxpe_sc_unlock(sc);

	return 0;
}


STATIC void
sysctl_mvxpe_init(struct mvxpe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const struct sysctlnode *node;
	int mvxpe_nodenum;
	int mvxpe_mibnum;
	int mvxpe_rxqueuenum;
	int mvxpe_txqueuenum;
	int q, i;

	/* hw.mvxpe.mvxpe[unit] */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, ifp->if_xname,
	    SYSCTL_DESCR("mvxpe per-controller controls"),
	    NULL, 0, NULL, 0,
	    CTL_HW, mvxpe_root_num, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}
	mvxpe_nodenum = node->sysctl_num;

	/* hw.mvxpe.mvxpe[unit].mib */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "mib",
	    SYSCTL_DESCR("mvxpe per-controller MIB counters"),
	    NULL, 0, NULL, 0,
	    CTL_HW, mvxpe_root_num, mvxpe_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}
	mvxpe_mibnum = node->sysctl_num;

	/* hw.mvxpe.mvxpe[unit].rx */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "rx",
	    SYSCTL_DESCR("Rx Queues"),
	    NULL, 0, NULL, 0,
	    CTL_HW, mvxpe_root_num, mvxpe_nodenum, CTL_CREATE, CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}
	mvxpe_rxqueuenum = node->sysctl_num;

	/* hw.mvxpe.mvxpe[unit].tx */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "tx",
	    SYSCTL_DESCR("Tx Queues"),
	    NULL, 0, NULL, 0,
	    CTL_HW, mvxpe_root_num, mvxpe_nodenum, CTL_CREATE, CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}
	mvxpe_txqueuenum = node->sysctl_num;

#ifdef MVXPE_DEBUG
	/* hw.mvxpe.debug */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT, "debug",
	    SYSCTL_DESCR("mvgbe device driver debug control"),
	    NULL, 0, &mvxpe_debug, 0,
	    CTL_HW, mvxpe_root_num, CTL_CREATE, CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}
#endif
	/*
	 * MIB access
	 */
	/* hw.mvxpe.mvxpe[unit].mib.<mibs> */
	for (i = 0; i < __arraycount(mvxpe_mib_list); i++) {
		const char *name = mvxpe_mib_list[i].sysctl_name;
		const char *desc = mvxpe_mib_list[i].desc;
		struct mvxpe_sysctl_mib *mib_arg = &sc->sc_sysctl_mib[i];

		mib_arg->sc = sc;
		mib_arg->index = i; 
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    CTLFLAG_READONLY, CTLTYPE_QUAD, name, desc,
		    sysctl_read_mib, 0, (void *)mib_arg, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_mibnum,
		    CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}
	}

	for (q = 0; q < MVXPE_QUEUE_SIZE; q++) {
		struct mvxpe_sysctl_queue *rxarg = &sc->sc_sysctl_rx_queue[q];
		struct mvxpe_sysctl_queue *txarg = &sc->sc_sysctl_tx_queue[q];
#define MVXPE_SYSCTL_NAME(num) "queue" # num
		static const char *sysctl_queue_names[] = {
			MVXPE_SYSCTL_NAME(0), MVXPE_SYSCTL_NAME(1),
			MVXPE_SYSCTL_NAME(2), MVXPE_SYSCTL_NAME(3),
			MVXPE_SYSCTL_NAME(4), MVXPE_SYSCTL_NAME(5),
			MVXPE_SYSCTL_NAME(6), MVXPE_SYSCTL_NAME(7),
		};
#undef MVXPE_SYSCTL_NAME
#ifdef SYSCTL_INCLUDE_DESCR
#define MVXPE_SYSCTL_DESCR(num) "configuration parameters for queue " # num
		static const char *sysctl_queue_descrs[] = {
			MVXPE_SYSCTL_DESC(0), MVXPE_SYSCTL_DESC(1),
			MVXPE_SYSCTL_DESC(2), MVXPE_SYSCTL_DESC(3),
			MVXPE_SYSCTL_DESC(4), MVXPE_SYSCTL_DESC(5),
			MVXPE_SYSCTL_DESC(6), MVXPE_SYSCTL_DESC(7),
		};
#undef MVXPE_SYSCTL_DESCR
#endif /* SYSCTL_INCLUDE_DESCR */
		int mvxpe_curnum;

		rxarg->sc = txarg->sc = sc;
		rxarg->queue = txarg->queue = q;
		rxarg->rxtx = MVXPE_SYSCTL_RX;
		txarg->rxtx = MVXPE_SYSCTL_TX;

		/* hw.mvxpe.mvxpe[unit].rx.[queue] */
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    0, CTLTYPE_NODE,
		    sysctl_queue_names[q], SYSCTL_DESCR(sysctl_queue_descrs[q]),
		    NULL, 0, NULL, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_rxqueuenum,
		    CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}
		mvxpe_curnum = node->sysctl_num;

		/* hw.mvxpe.mvxpe[unit].rx.[queue].length */
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    CTLFLAG_READWRITE, CTLTYPE_INT, "length",
		    SYSCTL_DESCR("maximum length of the queue"),
		    sysctl_set_queue_length, 0, (void *)rxarg, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_rxqueuenum,
		    mvxpe_curnum, CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}

		/* hw.mvxpe.mvxpe[unit].rx.[queue].threshold_timer_us */
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    CTLFLAG_READWRITE, CTLTYPE_INT, "threshold_timer_us",
		    SYSCTL_DESCR("interrupt coalescing threshold timer [us]"),
		    sysctl_set_queue_rxthtime, 0, (void *)rxarg, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_rxqueuenum,
		    mvxpe_curnum, CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}

		/* hw.mvxpe.mvxpe[unit].tx.[queue] */
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    0, CTLTYPE_NODE,
		    sysctl_queue_names[q], SYSCTL_DESCR(sysctl_queue_descs[q]),
		    NULL, 0, NULL, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_txqueuenum,
		    CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}
		mvxpe_curnum = node->sysctl_num;

		/* hw.mvxpe.mvxpe[unit].tx.length[queue] */
		if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
		    CTLFLAG_READWRITE, CTLTYPE_INT, "length",
		    SYSCTL_DESCR("maximum length of the queue"),
		    sysctl_set_queue_length, 0, (void *)txarg, 0,
		    CTL_HW, mvxpe_root_num, mvxpe_nodenum, mvxpe_txqueuenum,
		    mvxpe_curnum, CTL_CREATE, CTL_EOL) != 0) {
			aprint_normal_dev(sc->sc_dev,
			    "couldn't create sysctl node\n");
			break;
		}
	}

	/* hw.mvxpe.mvxpe[unit].clear_mib */
	if (sysctl_createv(&sc->sc_mvxpe_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT, "clear_mib",
	    SYSCTL_DESCR("mvgbe device driver debug control"),
	    sysctl_clear_mib, 0, (void *)sc, 0,
	    CTL_HW, mvxpe_root_num, mvxpe_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->sc_dev, "couldn't create sysctl node\n");
		return;
	}

}

/*
 * MIB
 */
STATIC void
mvxpe_clear_mib(struct mvxpe_softc *sc)
{
	int i;

	KASSERT_SC_MTX(sc);

	for (i = 0; i < __arraycount(mvxpe_mib_list); i++) {
		if (mvxpe_mib_list[i].reg64)
			MVXPE_READ_MIB(sc, (mvxpe_mib_list[i].regnum + 4));
		MVXPE_READ_MIB(sc, mvxpe_mib_list[i].regnum);
		sc->sc_sysctl_mib[i].counter = 0;
	}
}

STATIC void
mvxpe_update_mib(struct mvxpe_softc *sc)
{
	int i;

	KASSERT_SC_MTX(sc);

	for (i = 0; i < __arraycount(mvxpe_mib_list); i++) {
		uint32_t val_hi;
		uint32_t val_lo;

		if (mvxpe_mib_list[i].reg64) {
			/* XXX: implement bus_space_read_8() */
			val_lo = MVXPE_READ_MIB(sc,
			    (mvxpe_mib_list[i].regnum + 4));
			val_hi = MVXPE_READ_MIB(sc, mvxpe_mib_list[i].regnum);
		}
		else {
			val_lo = MVXPE_READ_MIB(sc, mvxpe_mib_list[i].regnum);
			val_hi = 0;
		}

		if ((val_lo | val_hi) == 0)
			continue;

		sc->sc_sysctl_mib[i].counter +=
	       	    ((uint64_t)val_hi << 32) | (uint64_t)val_lo;
	}
}

/*
 * for Debug
 */
STATIC void
mvxpe_dump_txdesc(struct mvxpe_tx_desc *desc, int idx)
{
#define DESC_PRINT(X)					\
	if (X)						\
		printf("txdesc[%d]." #X "=%#x\n", idx, X);

       DESC_PRINT(desc->command);
       DESC_PRINT(desc->l4ichk);
       DESC_PRINT(desc->bytecnt);
       DESC_PRINT(desc->bufptr);
       DESC_PRINT(desc->flags);
#undef DESC_PRINT
}

STATIC void
mvxpe_dump_rxdesc(struct mvxpe_rx_desc *desc, int idx)
{
#define DESC_PRINT(X)					\
	if (X)						\
		printf("rxdesc[%d]." #X "=%#x\n", idx, X);

       DESC_PRINT(desc->status);
       DESC_PRINT(desc->bytecnt);
       DESC_PRINT(desc->bufptr);
       DESC_PRINT(desc->l4chk);
#undef DESC_PRINT
}
