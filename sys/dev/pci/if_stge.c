/*	$NetBSD: if_stge.c,v 1.57 2014/03/29 19:28:25 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Device driver for the Sundance Tech. TC9021 10/100/1000
 * Ethernet controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_stge.c,v 1.57 2014/03/29 19:28:25 christos Exp $");


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
#include <sys/queue.h>

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_stgereg.h>

#include <prop/proplib.h>

/* #define	STGE_CU_BUG			1 */
#define	STGE_VLAN_UNTAG			1
/* #define	STGE_VLAN_CFI		1 */

/*
 * Transmit descriptor list size.
 */
#define	STGE_NTXDESC		256
#define	STGE_NTXDESC_MASK	(STGE_NTXDESC - 1)
#define	STGE_NEXTTX(x)		(((x) + 1) & STGE_NTXDESC_MASK)

/*
 * Receive descriptor list size.
 */
#define	STGE_NRXDESC		256
#define	STGE_NRXDESC_MASK	(STGE_NRXDESC - 1)
#define	STGE_NEXTRX(x)		(((x) + 1) & STGE_NRXDESC_MASK)

/*
 * Only interrupt every N frames.  Must be a power-of-two.
 */
#define	STGE_TXINTR_SPACING	16
#define	STGE_TXINTR_SPACING_MASK (STGE_TXINTR_SPACING - 1)

/*
 * Control structures are DMA'd to the TC9021 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct stge_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct stge_tfd scd_txdescs[STGE_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct stge_rfd scd_rxdescs[STGE_NRXDESC];
};

#define	STGE_CDOFF(x)	offsetof(struct stge_control_data, x)
#define	STGE_CDTXOFF(x)	STGE_CDOFF(scd_txdescs[(x)])
#define	STGE_CDRXOFF(x)	STGE_CDOFF(scd_rxdescs[(x)])

/*
 * Software state for transmit and receive jobs.
 */
struct stge_descsoft {
	struct mbuf *ds_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t ds_dmamap;		/* our DMA map */
};

/*
 * Software state per device.
 */
struct stge_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */
	int sc_rev;			/* silicon revision */

	void *sc_ih;			/* interrupt cookie */

	struct mii_data sc_mii;		/* MII/media information */

	callout_t sc_tick_ch;		/* tick callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct stge_descsoft sc_txsoft[STGE_NTXDESC];
	struct stge_descsoft sc_rxsoft[STGE_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct stge_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->scd_txdescs
#define	sc_rxdescs	sc_control_data->scd_rxdescs

#ifdef STGE_EVENT_COUNTERS
	/*
	 * Event counters.
	 */
	struct evcnt sc_ev_txstall;	/* Tx stalled */
	struct evcnt sc_ev_txdmaintr;	/* Tx DMA interrupts */
	struct evcnt sc_ev_txindintr;	/* Tx Indicate interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */

	struct evcnt sc_ev_txseg1;	/* Tx packets w/ 1 segment */
	struct evcnt sc_ev_txseg2;	/* Tx packets w/ 2 segments */
	struct evcnt sc_ev_txseg3;	/* Tx packets w/ 3 segments */
	struct evcnt sc_ev_txseg4;	/* Tx packets w/ 4 segments */
	struct evcnt sc_ev_txseg5;	/* Tx packets w/ 5 segments */
	struct evcnt sc_ev_txsegmore;	/* Tx packets w/ more than 5 segments */
	struct evcnt sc_ev_txcopy;	/* Tx packets that we had to copy */

	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtcpsum;	/* TCP checksums checked in-bound */
	struct evcnt sc_ev_rxudpsum;	/* UDP checksums checked in-bound */

	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtcpsum;	/* TCP checksums comp. out-bound */
	struct evcnt sc_ev_txudpsum;	/* UDP checksums comp. out-bound */
#endif /* STGE_EVENT_COUNTERS */

	int	sc_txpending;		/* number of Tx requests pending */
	int	sc_txdirty;		/* first dirty Tx descriptor */
	int	sc_txlast;		/* last used Tx descriptor */

	int	sc_rxptr;		/* next ready Rx descriptor/descsoft */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	int	sc_txthresh;		/* Tx threshold */
	uint32_t sc_usefiber:1;		/* if we're fiber */
	uint32_t sc_stge1023:1;		/* are we a 1023 */
	uint32_t sc_DMACtrl;		/* prototype DMACtrl register */
	uint32_t sc_MACCtrl;		/* prototype MacCtrl register */
	uint16_t sc_IntEnable;		/* prototype IntEnable register */
	uint16_t sc_ReceiveMode;	/* prototype ReceiveMode register */
	uint8_t sc_PhyCtrl;		/* prototype PhyCtrl register */
};

#define	STGE_RXCHAIN_RESET(sc)						\
do {									\
	(sc)->sc_rxtailp = &(sc)->sc_rxhead;				\
	*(sc)->sc_rxtailp = NULL;					\
	(sc)->sc_rxlen = 0;						\
} while (/*CONSTCOND*/0)

#define	STGE_RXCHAIN_LINK(sc, m)					\
do {									\
	*(sc)->sc_rxtailp = (sc)->sc_rxtail = (m);			\
	(sc)->sc_rxtailp = &(m)->m_next;				\
} while (/*CONSTCOND*/0)

#ifdef STGE_EVENT_COUNTERS
#define	STGE_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	STGE_EVCNT_INCR(ev)	/* nothing */
#endif

#define	STGE_CDTXADDR(sc, x)	((sc)->sc_cddma + STGE_CDTXOFF((x)))
#define	STGE_CDRXADDR(sc, x)	((sc)->sc_cddma + STGE_CDRXOFF((x)))

#define	STGE_CDTXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STGE_CDTXOFF((x)), sizeof(struct stge_tfd), (ops))

#define	STGE_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STGE_CDRXOFF((x)), sizeof(struct stge_rfd), (ops))

#define	STGE_INIT_RXDESC(sc, x)						\
do {									\
	struct stge_descsoft *__ds = &(sc)->sc_rxsoft[(x)];		\
	struct stge_rfd *__rfd = &(sc)->sc_rxdescs[(x)];		\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 */								\
	__rfd->rfd_frag.frag_word0 =					\
	    htole64(FRAG_ADDR(__ds->ds_dmamap->dm_segs[0].ds_addr + 2) |\
	    FRAG_LEN(MCLBYTES - 2));					\
	__rfd->rfd_next =						\
	    htole64((uint64_t)STGE_CDRXADDR((sc), STGE_NEXTRX((x))));	\
	__rfd->rfd_status = 0;						\
	STGE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (/*CONSTCOND*/0)

#define STGE_TIMEOUT 1000

static void	stge_start(struct ifnet *);
static void	stge_watchdog(struct ifnet *);
static int	stge_ioctl(struct ifnet *, u_long, void *);
static int	stge_init(struct ifnet *);
static void	stge_stop(struct ifnet *, int);

static bool	stge_shutdown(device_t, int);

static void	stge_reset(struct stge_softc *);
static void	stge_rxdrain(struct stge_softc *);
static int	stge_add_rxbuf(struct stge_softc *, int);
static void	stge_read_eeprom(struct stge_softc *, int, uint16_t *);
static void	stge_tick(void *);

static void	stge_stats_update(struct stge_softc *);

static void	stge_set_filter(struct stge_softc *);

static int	stge_intr(void *);
static void	stge_txintr(struct stge_softc *);
static void	stge_rxintr(struct stge_softc *);

static int	stge_mii_readreg(device_t, int, int);
static void	stge_mii_writereg(device_t, int, int, int);
static void	stge_mii_statchg(struct ifnet *);

static int	stge_match(device_t, cfdata_t, void *);
static void	stge_attach(device_t, device_t, void *);

int	stge_copy_small = 0;

CFATTACH_DECL_NEW(stge, sizeof(struct stge_softc),
    stge_match, stge_attach, NULL, NULL);

static uint32_t stge_mii_bitbang_read(device_t);
static void	stge_mii_bitbang_write(device_t, uint32_t);

static const struct mii_bitbang_ops stge_mii_bitbang_ops = {
	stge_mii_bitbang_read,
	stge_mii_bitbang_write,
	{
		PC_MgmtData,		/* MII_BIT_MDO */
		PC_MgmtData,		/* MII_BIT_MDI */
		PC_MgmtClk,		/* MII_BIT_MDC */
		PC_MgmtDir,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

/*
 * Devices supported by this driver.
 */
static const struct stge_product {
	pci_vendor_id_t		stge_vendor;
	pci_product_id_t	stge_product;
	const char		*stge_name;
} stge_products[] = {
	{ PCI_VENDOR_SUNDANCETI,	PCI_PRODUCT_SUNDANCETI_ST1023,
	  "Sundance ST-1023 Gigabit Ethernet" },

	{ PCI_VENDOR_SUNDANCETI,	PCI_PRODUCT_SUNDANCETI_ST2021,
	  "Sundance ST-2021 Gigabit Ethernet" },

	{ PCI_VENDOR_TAMARACK,		PCI_PRODUCT_TAMARACK_TC9021,
	  "Tamarack TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_TAMARACK,		PCI_PRODUCT_TAMARACK_TC9021_ALT,
	  "Tamarack TC9021 Gigabit Ethernet" },

	/*
	 * The Sundance sample boards use the Sundance vendor ID,
	 * but the Tamarack product ID.
	 */
	{ PCI_VENDOR_SUNDANCETI,	PCI_PRODUCT_TAMARACK_TC9021,
	  "Sundance TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_SUNDANCETI,	PCI_PRODUCT_TAMARACK_TC9021_ALT,
	  "Sundance TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DL4000,
	  "D-Link DL-4000 Gigabit Ethernet" },

	{ PCI_VENDOR_ANTARES,		PCI_PRODUCT_ANTARES_TC9021,
	  "Antares Gigabit Ethernet" },

	{ 0,				0,
	  NULL },
};

static const struct stge_product *
stge_lookup(const struct pci_attach_args *pa)
{
	const struct stge_product *sp;

	for (sp = stge_products; sp->stge_name != NULL; sp++) {
		if (PCI_VENDOR(pa->pa_id) == sp->stge_vendor &&
		    PCI_PRODUCT(pa->pa_id) == sp->stge_product)
			return (sp);
	}
	return (NULL);
}

static int
stge_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (stge_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
stge_attach(device_t parent, device_t self, void *aux)
{
	struct stge_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_dma_segment_t seg;
	prop_data_t data;
	int ioh_valid, memh_valid;
	int i, rseg, error;
	const struct stge_product *sp;
	uint8_t enaddr[ETHER_ADDR_LEN];
	char intrbuf[PCI_INTRSTR_LEN];

	callout_init(&sc->sc_tick_ch, 0);

	sp = stge_lookup(pa);
	if (sp == NULL) {
		printf("\n");
		panic("ste_attach: impossible");
	}

	sc->sc_rev = PCI_REVISION(pa->pa_class);

	pci_aprint_devinfo_fancy(pa, NULL, sp->stge_name, 1);

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, STGE_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
	memh_valid = (pci_mapreg_map(pa, STGE_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL) == 0);

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/* Enable bus mastering. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self, NULL)) &&
	    error != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate %d\n",
		    error);
		return;
	}
	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, stge_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct stge_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		aprint_error_dev(self,
		    "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct stge_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self,
		    "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct stge_control_data), 1,
	    sizeof(struct stge_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(self,
		    "unable to create control data DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct stge_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(self,
		    "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.  Note that rev B.3
	 * and earlier seem to have a bug regarding multi-fragment
	 * packets.  We need to limit the number of Tx segments on
	 * such chips to 1.
	 */
	for (i = 0; i < STGE_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat,
		    ETHER_MAX_LEN_JUMBO, STGE_NTXFRAGS, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(self,
			    "unable to create tx DMA map %d, error = %d\n",
			    i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < STGE_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(self,
			    "unable to create rx DMA map %d, error = %d\n",
			    i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].ds_mbuf = NULL;
	}

	/*
	 * Determine if we're copper or fiber.  It affects how we
	 * reset the card.
	 */
	if (bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl) &
	    AC_PhyMedia)
		sc->sc_usefiber = 1;
	else
		sc->sc_usefiber = 0;

	/*
	 * Reset the chip to a known state.
	 */
	stge_reset(sc);

	/*
	 * Reading the station address from the EEPROM doesn't seem
	 * to work, at least on my sample boards.  Instead, since
	 * the reset sequence does AutoInit, read it from the station
	 * address registers. For Sundance 1023 you can only read it
	 * from EEPROM.
	 */
	if (sp->stge_product != PCI_PRODUCT_SUNDANCETI_ST1023) {
		enaddr[0] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress0) & 0xff;
		enaddr[1] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress0) >> 8;
		enaddr[2] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress1) & 0xff;
		enaddr[3] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress1) >> 8;
		enaddr[4] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress2) & 0xff;
		enaddr[5] = bus_space_read_2(sc->sc_st, sc->sc_sh,
		    STGE_StationAddress2) >> 8;
		sc->sc_stge1023 = 0;
	} else {
		data = prop_dictionary_get(device_properties(self),
		    "mac-address");
		if (data != NULL) {
			/*
			 * Try to get the station address from device
			 * properties first, in case the EEPROM is missing.
			 */
			KASSERT(prop_object_type(data) == PROP_TYPE_DATA);
			KASSERT(prop_data_size(data) == ETHER_ADDR_LEN);
			(void)memcpy(enaddr, prop_data_data_nocopy(data),
			    ETHER_ADDR_LEN);
		} else {
			uint16_t myaddr[ETHER_ADDR_LEN / 2];
			for (i = 0; i <ETHER_ADDR_LEN / 2; i++) {
				stge_read_eeprom(sc,
				    STGE_EEPROM_StationAddress0 + i,
				    &myaddr[i]);
				myaddr[i] = le16toh(myaddr[i]);
			}
			(void)memcpy(enaddr, myaddr, sizeof(enaddr));
		}
		sc->sc_stge1023 = 1;
	}

	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(enaddr));

	/*
	 * Read some important bits from the PhyCtrl register.
	 */
	sc->sc_PhyCtrl = bus_space_read_1(sc->sc_st, sc->sc_sh,
	    STGE_PhyCtrl) & (PC_PhyDuplexPolarity | PC_PhyLnkPolarity);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = stge_mii_readreg;
	sc->sc_mii.mii_writereg = stge_mii_writereg;
	sc->sc_mii.mii_statchg = stge_mii_statchg;
	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, ether_mediachange,
	    ether_mediastatus);
	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_DOPAUSE);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	ifp = &sc->sc_ethercom.ec_if;
	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = stge_ioctl;
	ifp->if_start = stge_start;
	ifp->if_watchdog = stge_watchdog;
	ifp->if_init = stge_init;
	ifp->if_stop = stge_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * The manual recommends disabling early transmit, so we
	 * do.  It's disabled anyway, if using IP checksumming,
	 * since the entire packet must be in the FIFO in order
	 * for the chip to perform the checksum.
	 */
	sc->sc_txthresh = 0x0fff;

	/*
	 * Disable MWI if the PCI layer tells us to.
	 */
	sc->sc_DMACtrl = 0;
	if ((pa->pa_flags & PCI_FLAGS_MWI_OKAY) == 0)
		sc->sc_DMACtrl |= DMAC_MWIDisable;

	/*
	 * We can support 802.1Q VLAN-sized frames and jumbo
	 * Ethernet frames.
	 *
	 * XXX Figure out how to do hw-assisted VLAN tagging in
	 * XXX a reasonable way on this chip.
	 */
	sc->sc_ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | /* XXX ETHERCAP_JUMBO_MTU | */
	    ETHERCAP_VLAN_HWTAGGING;

	/*
	 * We can do IPv4/TCPv4/UDPv4 checksums in hardware.
	 */
	sc->sc_ethercom.ec_if.if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

#ifdef STGE_EVENT_COUNTERS
	/*
	 * Attach event counters.
	 */
	evcnt_attach_dynamic(&sc->sc_ev_txstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdmaintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(self), "txdmaintr");
	evcnt_attach_dynamic(&sc->sc_ev_txindintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(self), "txindintr");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(self), "rxintr");

	evcnt_attach_dynamic(&sc->sc_ev_txseg1, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txseg1");
	evcnt_attach_dynamic(&sc->sc_ev_txseg2, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txseg2");
	evcnt_attach_dynamic(&sc->sc_ev_txseg3, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txseg3");
	evcnt_attach_dynamic(&sc->sc_ev_txseg4, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txseg4");
	evcnt_attach_dynamic(&sc->sc_ev_txseg5, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txseg5");
	evcnt_attach_dynamic(&sc->sc_ev_txsegmore, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txsegmore");
	evcnt_attach_dynamic(&sc->sc_ev_txcopy, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txcopy");

	evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "rxipsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxtcpsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "rxtcpsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxudpsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "rxudpsum");
	evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txipsum");
	evcnt_attach_dynamic(&sc->sc_ev_txtcpsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txtcpsum");
	evcnt_attach_dynamic(&sc->sc_ev_txudpsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txudpsum");
#endif /* STGE_EVENT_COUNTERS */

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	if (pmf_device_register1(self, NULL, NULL, stge_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < STGE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].ds_dmamap);
	}
 fail_4:
	for (i = 0; i < STGE_NTXDESC; i++) {
		if (sc->sc_txsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].ds_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct stge_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * stge_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static bool
stge_shutdown(device_t self, int howto)
{
	struct stge_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	stge_stop(ifp, 1);
	stge_reset(sc);
	return true;
}

static void
stge_dma_wait(struct stge_softc *sc)
{
	int i;

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(2);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_DMACtrl) &
		     DMAC_TxDMAInProg) == 0)
			break;
	}

	if (i == STGE_TIMEOUT)
		printf("%s: DMA wait timed out\n", device_xname(sc->sc_dev));
}

/*
 * stge_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
stge_start(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct stge_descsoft *ds;
	struct stge_tfd *tfd;
	bus_dmamap_t dmamap;
	int error, firsttx, nexttx, opending, seg, totlen;
	uint64_t csum_flags;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of pending transmissions
	 * and the first descriptor we will use.
	 */
	opending = sc->sc_txpending;
	firsttx = STGE_NEXTTX(sc->sc_txlast);

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		struct m_tag *mtag;
		uint64_t tfc;

		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/*
		 * Leave one unused descriptor at the end of the
		 * list to prevent wrapping completely around.
		 */
		if (sc->sc_txpending == (STGE_NTXDESC - 1)) {
			STGE_EVCNT_INCR(&sc->sc_ev_txstall);
			break;
		}

		/*
		 * See if we have any VLAN stuff.
		 */
		mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0);

		/*
		 * Get the last and next available transmit descriptor.
		 */
		nexttx = STGE_NEXTTX(sc->sc_txlast);
		tfd = &sc->sc_txdescs[nexttx];
		ds = &sc->sc_txsoft[nexttx];

		dmamap = ds->ds_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.  For the too-many-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				printf("%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    device_xname(sc->sc_dev));
				IFQ_DEQUEUE(&ifp->if_snd, m0);
				m_freem(m0);
				continue;
			}
			/*
			 * Short on resources, just stop for now.
			 */
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the fragment list. */
		for (totlen = 0, seg = 0; seg < dmamap->dm_nsegs; seg++) {
			tfd->tfd_frags[seg].frag_word0 =
			    htole64(FRAG_ADDR(dmamap->dm_segs[seg].ds_addr) |
			    FRAG_LEN(dmamap->dm_segs[seg].ds_len));
			totlen += dmamap->dm_segs[seg].ds_len;
		}

#ifdef STGE_EVENT_COUNTERS
		switch (dmamap->dm_nsegs) {
		case 1:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg1);
			break;
		case 2:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg2);
			break;
		case 3:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg3);
			break;
		case 4:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg4);
			break;
		case 5:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg5);
			break;
		default:
			STGE_EVCNT_INCR(&sc->sc_ev_txsegmore);
			break;
		}
#endif /* STGE_EVENT_COUNTERS */

		/*
		 * Initialize checksumming flags in the descriptor.
		 * Byte-swap constants so the compiler can optimize.
		 */
		csum_flags = 0;
		if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4) {
			STGE_EVCNT_INCR(&sc->sc_ev_txipsum);
			csum_flags |= TFD_IPChecksumEnable;
		}

		if (m0->m_pkthdr.csum_flags & M_CSUM_TCPv4) {
			STGE_EVCNT_INCR(&sc->sc_ev_txtcpsum);
			csum_flags |= TFD_TCPChecksumEnable;
		} else if (m0->m_pkthdr.csum_flags & M_CSUM_UDPv4) {
			STGE_EVCNT_INCR(&sc->sc_ev_txudpsum);
			csum_flags |= TFD_UDPChecksumEnable;
		}

		/*
		 * Initialize the descriptor and give it to the chip.
		 * Check to see if we have a VLAN tag to insert.
		 */

		tfc = TFD_FrameId(nexttx) | TFD_WordAlign(/*totlen & */3) |
		    TFD_FragCount(seg) | csum_flags |
		    (((nexttx & STGE_TXINTR_SPACING_MASK) == 0) ?
			TFD_TxDMAIndicate : 0);
		if (mtag) {
#if	0
			struct ether_header *eh =
			    mtod(m0, struct ether_header *);
			u_int16_t etype = ntohs(eh->ether_type);
			printf("%s: xmit (tag %d) etype %x\n",
			   ifp->if_xname, *mtod(n, int *), etype);
#endif
			tfc |= TFD_VLANTagInsert |
#ifdef	STGE_VLAN_CFI
			    TFD_CFI |
#endif
			    TFD_VID(VLAN_TAG_VALUE(mtag));
		}
		tfd->tfd_control = htole64(tfc);

		/* Sync the descriptor. */
		STGE_CDTXSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Kick the transmit DMA logic.
		 */
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_DMACtrl,
		    sc->sc_DMACtrl | DMAC_TxDMAPollNow);

		/*
		 * Store a pointer to the packet so we can free it later.
		 */
		ds->ds_mbuf = m0;

		/* Advance the tx pointer. */
		sc->sc_txpending++;
		sc->sc_txlast = nexttx;

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (sc->sc_txpending == (STGE_NTXDESC - 1)) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/*
		 * We enqueued packets.  If the transmitter was idle,
		 * reset the txdirty pointer.
		 */
		if (opending == 0)
			sc->sc_txdirty = firsttx;

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * stge_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
stge_watchdog(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;

	/*
	 * Sweep up first, since we don't interrupt every frame.
	 */
	stge_txintr(sc);
	if (sc->sc_txpending != 0) {
		printf("%s: device timeout\n", device_xname(sc->sc_dev));
		ifp->if_oerrors++;

		(void) stge_init(ifp);

		/* Try to get more packets going. */
		stge_start(ifp);
	}
}

/*
 * stge_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
stge_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct stge_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		error = 0;

		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			stge_set_filter(sc);
		}
	}

	/* Try to get more packets going. */
	stge_start(ifp);

	splx(s);
	return (error);
}

/*
 * stge_intr:
 *
 *	Interrupt service routine.
 */
static int
stge_intr(void *arg)
{
	struct stge_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t txstat;
	int wantinit;
	uint16_t isr;

	if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_IntStatus) &
	     IS_InterruptStatus) == 0)
		return (0);

	for (wantinit = 0; wantinit == 0;) {
		isr = bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_IntStatusAck);
		if ((isr & sc->sc_IntEnable) == 0)
			break;

		/* Host interface errors. */
		if (isr & IS_HostError) {
			printf("%s: Host interface error\n",
			    device_xname(sc->sc_dev));
			wantinit = 1;
			continue;
		}

		/* Receive interrupts. */
		if (isr & (IS_RxDMAComplete|IS_RFDListEnd)) {
			STGE_EVCNT_INCR(&sc->sc_ev_rxintr);
			stge_rxintr(sc);
			if (isr & IS_RFDListEnd) {
				printf("%s: receive ring overflow\n",
				    device_xname(sc->sc_dev));
				/*
				 * XXX Should try to recover from this
				 * XXX more gracefully.
				 */
				wantinit = 1;
			}
		}

		/* Transmit interrupts. */
		if (isr & (IS_TxDMAComplete|IS_TxComplete)) {
#ifdef STGE_EVENT_COUNTERS
			if (isr & IS_TxDMAComplete)
				STGE_EVCNT_INCR(&sc->sc_ev_txdmaintr);
#endif
			stge_txintr(sc);
		}

		/* Statistics overflow. */
		if (isr & IS_UpdateStats)
			stge_stats_update(sc);

		/* Transmission errors. */
		if (isr & IS_TxComplete) {
			STGE_EVCNT_INCR(&sc->sc_ev_txindintr);
			for (;;) {
				txstat = bus_space_read_4(sc->sc_st, sc->sc_sh,
				    STGE_TxStatus);
				if ((txstat & TS_TxComplete) == 0)
					break;
				if (txstat & TS_TxUnderrun) {
					sc->sc_txthresh++;
					if (sc->sc_txthresh > 0x0fff)
						sc->sc_txthresh = 0x0fff;
					printf("%s: transmit underrun, new "
					    "threshold: %d bytes\n",
					    device_xname(sc->sc_dev),
					    sc->sc_txthresh << 5);
				}
				if (txstat & TS_MaxCollisions)
					printf("%s: excessive collisions\n",
					    device_xname(sc->sc_dev));
			}
			wantinit = 1;
		}

	}

	if (wantinit)
		stge_init(ifp);

	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_IntEnable,
	    sc->sc_IntEnable);

	/* Try to get more packets going. */
	stge_start(ifp);

	return (1);
}

/*
 * stge_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
stge_txintr(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct stge_descsoft *ds;
	uint64_t control;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txdirty; sc->sc_txpending != 0;
	     i = STGE_NEXTTX(i), sc->sc_txpending--) {
		ds = &sc->sc_txsoft[i];

		STGE_CDTXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		control = le64toh(sc->sc_txdescs[i].tfd_control);
		if ((control & TFD_TFDDone) == 0)
			break;

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap,
		    0, ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
		m_freem(ds->ds_mbuf);
		ds->ds_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txdirty = i;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txpending == 0)
		ifp->if_timer = 0;
}

/*
 * stge_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
stge_rxintr(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct stge_descsoft *ds;
	struct mbuf *m, *tailm;
	uint64_t status;
	int i, len;

	for (i = sc->sc_rxptr;; i = STGE_NEXTRX(i)) {
		ds = &sc->sc_rxsoft[i];

		STGE_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = le64toh(sc->sc_rxdescs[i].rfd_status);

		if ((status & RFD_RFDDone) == 0)
			break;

		if (__predict_false(sc->sc_rxdiscard)) {
			STGE_INIT_RXDESC(sc, i);
			if (status & RFD_FrameEnd) {
				/* Reset our state. */
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = ds->ds_mbuf;

		/*
		 * Add a new receive buffer to the ring.
		 */
		if (stge_add_rxbuf(sc, i) != 0) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			STGE_INIT_RXDESC(sc, i);
			if ((status & RFD_FrameEnd) == 0)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			STGE_RXCHAIN_RESET(sc);
			continue;
		}

#ifdef DIAGNOSTIC
		if (status & RFD_FrameStart) {
			KASSERT(sc->sc_rxhead == NULL);
			KASSERT(sc->sc_rxtailp == &sc->sc_rxhead);
		}
#endif

		STGE_RXCHAIN_LINK(sc, m);

		/*
		 * If this is not the end of the packet, keep
		 * looking.
		 */
		if ((status & RFD_FrameEnd) == 0) {
			sc->sc_rxlen += m->m_len;
			continue;
		}

		/*
		 * Okay, we have the entire packet now...
		 */
		*sc->sc_rxtailp = NULL;
		m = sc->sc_rxhead;
		tailm = sc->sc_rxtail;

		STGE_RXCHAIN_RESET(sc);

		/*
		 * If the packet had an error, drop it.  Note we
		 * count the error later in the periodic stats update.
		 */
		if (status & (RFD_RxFIFOOverrun | RFD_RxRuntFrame |
			      RFD_RxAlignmentError | RFD_RxFCSError |
			      RFD_RxLengthError)) {
			m_freem(m);
			continue;
		}

		/*
		 * No errors.
		 *
		 * Note we have configured the chip to not include
		 * the CRC at the end of the packet.
		 */
		len = RFD_RxDMAFrameLen(status);
		tailm->m_len = len - sc->sc_rxlen;

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 */
		if (stge_copy_small != 0 && len <= (MHLEN - 2)) {
			struct mbuf *nm;
			MGETHDR(nm, M_DONTWAIT, MT_DATA);
			if (nm == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}
			nm->m_data += 2;
			nm->m_pkthdr.len = nm->m_len = len;
			m_copydata(m, 0, len, mtod(nm, void *));
			m_freem(m);
			m = nm;
		}

		/*
		 * Set the incoming checksum information for the packet.
		 */
		if (status & RFD_IPDetected) {
			STGE_EVCNT_INCR(&sc->sc_ev_rxipsum);
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (status & RFD_IPError)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
			if (status & RFD_TCPDetected) {
				STGE_EVCNT_INCR(&sc->sc_ev_rxtcpsum);
				m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
				if (status & RFD_TCPError)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			} else if (status & RFD_UDPDetected) {
				STGE_EVCNT_INCR(&sc->sc_ev_rxudpsum);
				m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
				if (status & RFD_UDPError)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);
#ifdef	STGE_VLAN_UNTAG
		/*
		 * Check for VLAN tagged packets
		 */
		if (status & RFD_VLANDetected)
			VLAN_INPUT_TAG(ifp, m, RFD_TCI(status), continue);

#endif
#if	0
		if (status & RFD_VLANDetected) {
			struct ether_header *eh;
			u_int16_t etype;

			eh = mtod(m, struct ether_header *);
			etype = ntohs(eh->ether_type);
			printf("%s: VLANtag detected (TCI %d) etype %x\n",
			    ifp->if_xname, (u_int16_t) RFD_TCI(status),
			    etype);
		}
#endif
		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * stge_tick:
 *
 *	One second timer, used to tick the MII.
 */
static void
stge_tick(void *arg)
{
	struct stge_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	stge_stats_update(sc);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, stge_tick, sc);
}

/*
 * stge_stats_update:
 *
 *	Read the TC9021 statistics counters.
 */
static void
stge_stats_update(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	(void) bus_space_read_4(st, sh, STGE_OctetRcvOk);

	ifp->if_ipackets +=
	    bus_space_read_4(st, sh, STGE_FramesRcvdOk);

	ifp->if_ierrors +=
	    (u_int) bus_space_read_2(st, sh, STGE_FramesLostRxErrors);

	(void) bus_space_read_4(st, sh, STGE_OctetXmtdOk);

	ifp->if_opackets +=
	    bus_space_read_4(st, sh, STGE_FramesXmtdOk);

	ifp->if_collisions +=
	    bus_space_read_4(st, sh, STGE_LateCollisions) +
	    bus_space_read_4(st, sh, STGE_MultiColFrames) +
	    bus_space_read_4(st, sh, STGE_SingleColFrames);

	ifp->if_oerrors +=
	    (u_int) bus_space_read_2(st, sh, STGE_FramesAbortXSColls) +
	    (u_int) bus_space_read_2(st, sh, STGE_FramesWEXDeferal);
}

/*
 * stge_reset:
 *
 *	Perform a soft reset on the TC9021.
 */
static void
stge_reset(struct stge_softc *sc)
{
	uint32_t ac;
	int i;

	ac = bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl);

	/*
	 * Only assert RstOut if we're fiber.  We need GMII clocks
	 * to be present in order for the reset to complete on fiber
	 * cards.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl,
	    ac | AC_GlobalReset | AC_RxReset | AC_TxReset |
	    AC_DMA | AC_FIFO | AC_Network | AC_Host | AC_AutoInit |
	    (sc->sc_usefiber ? AC_RstOut : 0));

	delay(50000);

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(5000);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl) &
		     AC_ResetBusy) == 0)
			break;
	}

	if (i == STGE_TIMEOUT)
		printf("%s: reset failed to complete\n",
		    device_xname(sc->sc_dev));

	delay(1000);
}

/*
 * stge_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
stge_init(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct stge_descsoft *ds;
	int i, error = 0;

	/*
	 * Cancel any pending I/O.
	 */
	stge_stop(ifp, 0);

	/*
	 * Reset the chip to a known state.
	 */
	stge_reset(sc);

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < STGE_NTXDESC; i++) {
		sc->sc_txdescs[i].tfd_next = htole64(
		    STGE_CDTXADDR(sc, STGE_NEXTTX(i)));
		sc->sc_txdescs[i].tfd_control = htole64(TFD_TFDDone);
	}
	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = STGE_NTXDESC - 1;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < STGE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf == NULL) {
			if ((error = stge_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				stge_rxdrain(sc);
				goto out;
			}
		} else
			STGE_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	sc->sc_rxdiscard = 0;
	STGE_RXCHAIN_RESET(sc);

	/* Set the station address. */
	for (i = 0; i < 6; i++)
		bus_space_write_1(st, sh, STGE_StationAddress0 + i,
		    CLLADDR(ifp->if_sadl)[i]);

	/*
	 * Set the statistics masks.  Disable all the RMON stats,
	 * and disable selected stats in the non-RMON stats registers.
	 */
	bus_space_write_4(st, sh, STGE_RMONStatisticsMask, 0xffffffff);
	bus_space_write_4(st, sh, STGE_StatisticsMask,
	    (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5) |
	    (1U << 6) | (1U << 7) | (1U << 8) | (1U << 9) | (1U << 10) |
	    (1U << 13) | (1U << 14) | (1U << 15) | (1U << 19) | (1U << 20) |
	    (1U << 21));

	/* Set up the receive filter. */
	stge_set_filter(sc);

	/*
	 * Give the transmit and receive ring to the chip.
	 */
	bus_space_write_4(st, sh, STGE_TFDListPtrHi, 0); /* NOTE: 32-bit DMA */
	bus_space_write_4(st, sh, STGE_TFDListPtrLo,
	    STGE_CDTXADDR(sc, sc->sc_txdirty));

	bus_space_write_4(st, sh, STGE_RFDListPtrHi, 0); /* NOTE: 32-bit DMA */
	bus_space_write_4(st, sh, STGE_RFDListPtrLo,
	    STGE_CDRXADDR(sc, sc->sc_rxptr));

	/*
	 * Initialize the Tx auto-poll period.  It's OK to make this number
	 * large (255 is the max, but we use 127) -- we explicitly kick the
	 * transmit engine when there's actually a packet.
	 */
	bus_space_write_1(st, sh, STGE_TxDMAPollPeriod, 127);

	/* ..and the Rx auto-poll period. */
	bus_space_write_1(st, sh, STGE_RxDMAPollPeriod, 64);

	/* Initialize the Tx start threshold. */
	bus_space_write_2(st, sh, STGE_TxStartThresh, sc->sc_txthresh);

	/* RX DMA thresholds, from linux */
	bus_space_write_1(st, sh, STGE_RxDMABurstThresh, 0x30);
	bus_space_write_1(st, sh, STGE_RxDMAUrgentThresh, 0x30);

	/*
	 * Initialize the Rx DMA interrupt control register.  We
	 * request an interrupt after every incoming packet, but
	 * defer it for 32us (64 * 512 ns).  When the number of
	 * interrupts pending reaches 8, we stop deferring the
	 * interrupt, and signal it immediately.
	 */
	bus_space_write_4(st, sh, STGE_RxDMAIntCtrl,
	    RDIC_RxFrameCount(8) | RDIC_RxDMAWaitTime(512));

	/*
	 * Initialize the interrupt mask.
	 */
	sc->sc_IntEnable = IS_HostError | IS_TxComplete | IS_UpdateStats |
	    IS_TxDMAComplete | IS_RxDMAComplete | IS_RFDListEnd;
	bus_space_write_2(st, sh, STGE_IntStatus, 0xffff);
	bus_space_write_2(st, sh, STGE_IntEnable, sc->sc_IntEnable);

	/*
	 * Configure the DMA engine.
	 * XXX Should auto-tune TxBurstLimit.
	 */
	bus_space_write_4(st, sh, STGE_DMACtrl, sc->sc_DMACtrl |
	    DMAC_TxBurstLimit(3));

	/*
	 * Send a PAUSE frame when we reach 29,696 bytes in the Rx
	 * FIFO, and send an un-PAUSE frame when the FIFO is totally
	 * empty again.
	 */
	bus_space_write_2(st, sh, STGE_FlowOnTresh, 29696 / 16);
	bus_space_write_2(st, sh, STGE_FlowOffThresh, 0);

	/*
	 * Set the maximum frame size.
	 */
	bus_space_write_2(st, sh, STGE_MaxFrameSize,
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN +
	    ((sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) ?
	     ETHER_VLAN_ENCAP_LEN : 0));

	/*
	 * Initialize MacCtrl -- do it before setting the media,
	 * as setting the media will actually program the register.
	 *
	 * Note: We have to poke the IFS value before poking
	 * anything else.
	 */
	sc->sc_MACCtrl = MC_IFSSelect(0);
	bus_space_write_4(st, sh, STGE_MACCtrl, sc->sc_MACCtrl);
	sc->sc_MACCtrl |= MC_StatisticsEnable | MC_TxEnable | MC_RxEnable;
#ifdef	STGE_VLAN_UNTAG
	sc->sc_MACCtrl |= MC_AutoVLANuntagging;
#endif

	if (sc->sc_rev >= 6) {		/* >= B.2 */
		/* Multi-frag frame bug work-around. */
		bus_space_write_2(st, sh, STGE_DebugCtrl,
		    bus_space_read_2(st, sh, STGE_DebugCtrl) | 0x0200);

		/* Tx Poll Now bug work-around. */
		bus_space_write_2(st, sh, STGE_DebugCtrl,
		    bus_space_read_2(st, sh, STGE_DebugCtrl) | 0x0010);
		/* XXX ? from linux */
		bus_space_write_2(st, sh, STGE_DebugCtrl,
		    bus_space_read_2(st, sh, STGE_DebugCtrl) | 0x0020);
	}

	/*
	 * Set the current media.
	 */
	if ((error = ether_mediachange(ifp)) != 0)
		goto out;

	/*
	 * Start the one second MII clock.
	 */
	callout_reset(&sc->sc_tick_ch, hz, stge_tick, sc);

	/*
	 * ...all done!
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	return (error);
}

/*
 * stge_drain:
 *
 *	Drain the receive queue.
 */
static void
stge_rxdrain(struct stge_softc *sc)
{
	struct stge_descsoft *ds;
	int i;

	for (i = 0; i < STGE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			ds->ds_mbuf->m_next = NULL;
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}
}

/*
 * stge_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
static void
stge_stop(struct ifnet *ifp, int disable)
{
	struct stge_softc *sc = ifp->if_softc;
	struct stge_descsoft *ds;
	int i;

	/*
	 * Stop the one second clock.
	 */
	callout_stop(&sc->sc_tick_ch);

	/* Down the MII. */
	mii_down(&sc->sc_mii);

	/*
	 * Disable interrupts.
	 */
	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_IntEnable, 0);

	/*
	 * Stop receiver, transmitter, and stats update.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_MACCtrl,
	    MC_StatisticsDisable | MC_TxDisable | MC_RxDisable);

	/*
	 * Stop the transmit and receive DMA.
	 */
	stge_dma_wait(sc);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_TFDListPtrHi, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_TFDListPtrLo, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_RFDListPtrHi, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_RFDListPtrLo, 0);

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < STGE_NTXDESC; i++) {
		ds = &sc->sc_txsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		stge_rxdrain(sc);
}

static int
stge_eeprom_wait(struct stge_softc *sc)
{
	int i;

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(1000);
		if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_EepromCtrl) &
		     EC_EepromBusy) == 0)
			return (0);
	}
	return (1);
}

/*
 * stge_read_eeprom:
 *
 *	Read data from the serial EEPROM.
 */
static void
stge_read_eeprom(struct stge_softc *sc, int offset, uint16_t *data)
{

	if (stge_eeprom_wait(sc))
		printf("%s: EEPROM failed to come ready\n",
		    device_xname(sc->sc_dev));

	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_EepromCtrl,
	    EC_EepromAddress(offset) | EC_EepromOpcode(EC_OP_RR));
	if (stge_eeprom_wait(sc))
		printf("%s: EEPROM read timed out\n",
		    device_xname(sc->sc_dev));
	*data = bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_EepromData);
}

/*
 * stge_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
stge_add_rxbuf(struct stge_softc *sc, int idx)
{
	struct stge_descsoft *ds = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	m->m_data = m->m_ext.ext_buf + 2;
	m->m_len = MCLBYTES - 2;

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, ds->ds_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("stge_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
	    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	STGE_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * stge_set_filter:
 *
 *	Set up the receive filter.
 */
static void
stge_set_filter(struct stge_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc;
	uint32_t mchash[2];

	sc->sc_ReceiveMode = RM_ReceiveUnicast;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_ReceiveMode |= RM_ReceiveBroadcast;

	/* XXX: ST1023 only works in promiscuous mode */
	if (sc->sc_stge1023)
		ifp->if_flags |= IFF_PROMISC;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_ReceiveMode |= RM_ReceiveAllFrames;
		goto allmulti;
	}

	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the low-order
	 * 6 bits as an index into the 64 bit multicast hash table.  The
	 * high order bits select the register, while the rest of the bits
	 * select the bit within the register.
	 */

	memset(mchash, 0, sizeof(mchash));

	ETHER_FIRST_MULTI(step, ec, enm);
	if (enm == NULL)
		goto done;

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

		crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 6 least significant bits. */
		crc &= 0x3f;

		/* Set the corresponding bit in the hash table. */
		mchash[crc >> 5] |= 1 << (crc & 0x1f);

		ETHER_NEXT_MULTI(step, enm);
	}

	sc->sc_ReceiveMode |= RM_ReceiveMulticastHash;

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto done;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_ReceiveMode |= RM_ReceiveMulticast;

 done:
	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		/*
		 * Program the multicast hash table.
		 */
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_HashTable0,
		    mchash[0]);
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_HashTable1,
		    mchash[1]);
	}

	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_ReceiveMode,
	    sc->sc_ReceiveMode);
}

/*
 * stge_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII of the TC9021.
 */
static int
stge_mii_readreg(device_t self, int phy, int reg)
{

	return (mii_bitbang_readreg(self, &stge_mii_bitbang_ops, phy, reg));
}

/*
 * stge_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII of the TC9021.
 */
static void
stge_mii_writereg(device_t self, int phy, int reg, int val)
{

	mii_bitbang_writereg(self, &stge_mii_bitbang_ops, phy, reg, val);
}

/*
 * stge_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
stge_mii_statchg(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_MACCtrl |= MC_DuplexSelect;
	else
		sc->sc_MACCtrl &= ~MC_DuplexSelect;

	/* XXX 802.1x flow-control? */

	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_MACCtrl, sc->sc_MACCtrl);
}

/*
 * sste_mii_bitbang_read: [mii bit-bang interface function]
 *
 *	Read the MII serial port for the MII bit-bang module.
 */
static uint32_t
stge_mii_bitbang_read(device_t self)
{
	struct stge_softc *sc = device_private(self);

	return (bus_space_read_1(sc->sc_st, sc->sc_sh, STGE_PhyCtrl));
}

/*
 * stge_mii_bitbang_write: [mii big-bang interface function]
 *
 *	Write the MII serial port for the MII bit-bang module.
 */
static void
stge_mii_bitbang_write(device_t self, uint32_t val)
{
	struct stge_softc *sc = device_private(self);

	bus_space_write_1(sc->sc_st, sc->sc_sh, STGE_PhyCtrl,
	    val | sc->sc_PhyCtrl);
}
