/*	$NetBSD: if_ste.c,v 1.45 2014/03/29 19:28:25 christos Exp $	*/

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
 * Device driver for the Sundance Tech. ST-201 10/100
 * Ethernet controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ste.c,v 1.45 2014/03/29 19:28:25 christos Exp $");


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

#include <dev/pci/if_stereg.h>

/*
 * Transmit descriptor list size.
 */
#define	STE_NTXDESC		256
#define	STE_NTXDESC_MASK	(STE_NTXDESC - 1)
#define	STE_NEXTTX(x)		(((x) + 1) & STE_NTXDESC_MASK)

/*
 * Receive descriptor list size.
 */
#define	STE_NRXDESC		128
#define	STE_NRXDESC_MASK	(STE_NRXDESC - 1)
#define	STE_NEXTRX(x)		(((x) + 1) & STE_NRXDESC_MASK)

/*
 * Control structures are DMA'd to the ST-201 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct ste_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct ste_tfd scd_txdescs[STE_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct ste_rfd scd_rxdescs[STE_NRXDESC];
};

#define	STE_CDOFF(x)	offsetof(struct ste_control_data, x)
#define	STE_CDTXOFF(x)	STE_CDOFF(scd_txdescs[(x)])
#define	STE_CDRXOFF(x)	STE_CDOFF(scd_rxdescs[(x)])

/*
 * Software state for transmit and receive jobs.
 */
struct ste_descsoft {
	struct mbuf *ds_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t ds_dmamap;		/* our DMA map */
};

/*
 * Software state per device.
 */
struct ste_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */

	void *sc_ih;			/* interrupt cookie */

	struct mii_data sc_mii;		/* MII/media information */

	callout_t sc_tick_ch;		/* tick callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct ste_descsoft sc_txsoft[STE_NTXDESC];
	struct ste_descsoft sc_rxsoft[STE_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct ste_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->scd_txdescs
#define	sc_rxdescs	sc_control_data->scd_rxdescs

	int	sc_txpending;		/* number of Tx requests pending */
	int	sc_txdirty;		/* first dirty Tx descriptor */
	int	sc_txlast;		/* last used Tx descriptor */

	int	sc_rxptr;		/* next ready Rx descriptor/descsoft */

	int	sc_txthresh;		/* Tx threshold */
	uint32_t sc_DMACtrl;		/* prototype DMACtrl register */
	uint16_t sc_IntEnable;		/* prototype IntEnable register */
	uint16_t sc_MacCtrl0;		/* prototype MacCtrl0 register */
	uint8_t	sc_ReceiveMode;		/* prototype ReceiveMode register */
};

#define	STE_CDTXADDR(sc, x)	((sc)->sc_cddma + STE_CDTXOFF((x)))
#define	STE_CDRXADDR(sc, x)	((sc)->sc_cddma + STE_CDRXOFF((x)))

#define	STE_CDTXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STE_CDTXOFF((x)), sizeof(struct ste_tfd), (ops))

#define	STE_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STE_CDRXOFF((x)), sizeof(struct ste_rfd), (ops))

#define	STE_INIT_RXDESC(sc, x)						\
do {									\
	struct ste_descsoft *__ds = &(sc)->sc_rxsoft[(x)];		\
	struct ste_rfd *__rfd = &(sc)->sc_rxdescs[(x)];			\
	struct mbuf *__m = __ds->ds_mbuf;				\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 */								\
	__m->m_data = __m->m_ext.ext_buf + 2;				\
	__rfd->rfd_frag.frag_addr =					\
	    htole32(__ds->ds_dmamap->dm_segs[0].ds_addr + 2);		\
	__rfd->rfd_frag.frag_len = htole32((MCLBYTES - 2) | FRAG_LAST);	\
	__rfd->rfd_next = htole32(STE_CDRXADDR((sc), STE_NEXTRX((x))));	\
	__rfd->rfd_status = 0;						\
	STE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (/*CONSTCOND*/0)

#define STE_TIMEOUT 1000

static void	ste_start(struct ifnet *);
static void	ste_watchdog(struct ifnet *);
static int	ste_ioctl(struct ifnet *, u_long, void *);
static int	ste_init(struct ifnet *);
static void	ste_stop(struct ifnet *, int);

static bool	ste_shutdown(device_t, int);

static void	ste_reset(struct ste_softc *, u_int32_t);
static void	ste_setthresh(struct ste_softc *);
static void	ste_txrestart(struct ste_softc *, u_int8_t);
static void	ste_rxdrain(struct ste_softc *);
static int	ste_add_rxbuf(struct ste_softc *, int);
static void	ste_read_eeprom(struct ste_softc *, int, uint16_t *);
static void	ste_tick(void *);

static void	ste_stats_update(struct ste_softc *);

static void	ste_set_filter(struct ste_softc *);

static int	ste_intr(void *);
static void	ste_txintr(struct ste_softc *);
static void	ste_rxintr(struct ste_softc *);

static int	ste_mii_readreg(device_t, int, int);
static void	ste_mii_writereg(device_t, int, int, int);
static void	ste_mii_statchg(struct ifnet *);

static int	ste_match(device_t, cfdata_t, void *);
static void	ste_attach(device_t, device_t, void *);

int	ste_copy_small = 0;

CFATTACH_DECL_NEW(ste, sizeof(struct ste_softc),
    ste_match, ste_attach, NULL, NULL);

static uint32_t ste_mii_bitbang_read(device_t);
static void	ste_mii_bitbang_write(device_t, uint32_t);

static const struct mii_bitbang_ops ste_mii_bitbang_ops = {
	ste_mii_bitbang_read,
	ste_mii_bitbang_write,
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
static const struct ste_product {
	pci_vendor_id_t		ste_vendor;
	pci_product_id_t	ste_product;
	const char		*ste_name;
} ste_products[] = {
	{ PCI_VENDOR_SUNDANCETI, 	PCI_PRODUCT_SUNDANCETI_IP100A,
	  "IC Plus Corp. IP00A 10/100 Fast Ethernet Adapter" },

	{ PCI_VENDOR_SUNDANCETI,	PCI_PRODUCT_SUNDANCETI_ST201,
	  "Sundance ST-201 10/100 Ethernet" },

	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DL1002,
	  "D-Link DL-1002 10/100 Ethernet" },

	{ 0,				0,
	  NULL },
};

static const struct ste_product *
ste_lookup(const struct pci_attach_args *pa)
{
	const struct ste_product *sp;

	for (sp = ste_products; sp->ste_name != NULL; sp++) {
		if (PCI_VENDOR(pa->pa_id) == sp->ste_vendor &&
		    PCI_PRODUCT(pa->pa_id) == sp->ste_product)
			return (sp);
	}
	return (NULL);
}

static int
ste_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (ste_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
ste_attach(device_t parent, device_t self, void *aux)
{
	struct ste_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_dma_segment_t seg;
	int ioh_valid, memh_valid;
	int i, rseg, error;
	const struct ste_product *sp;
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint16_t myea[ETHER_ADDR_LEN / 2];
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	callout_init(&sc->sc_tick_ch, 0);

	sp = ste_lookup(pa);
	if (sp == NULL) {
		printf("\n");
		panic("ste_attach: impossible");
	}

	printf(": %s\n", sp->ste_name);

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, STE_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
	memh_valid = (pci_mapreg_map(pa, STE_PCI_MMBA,
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
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n",
		    error);
		return;
	}

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ste_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct ste_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct ste_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct ste_control_data), 1,
	    sizeof(struct ste_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct ste_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < STE_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    STE_NTXFRAGS, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create tx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < STE_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create rx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].ds_mbuf = NULL;
	}

	/*
	 * Reset the chip to a known state.
	 */
	ste_reset(sc, AC_GlobalReset | AC_RxReset | AC_TxReset | AC_DMA |
	    AC_FIFO | AC_Network | AC_Host | AC_AutoInit | AC_RstOut);

	/*
	 * Read the Ethernet address from the EEPROM.
	 */
	for (i = 0; i < 3; i++) {
		ste_read_eeprom(sc, STE_EEPROM_StationAddress0 + i, &myea[i]);
		myea[i] = le16toh(myea[i]);
	}
	memcpy(enaddr, myea, sizeof(enaddr));

	printf("%s: Ethernet address %s\n", device_xname(sc->sc_dev),
	    ether_sprintf(enaddr));

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = ste_mii_readreg;
	sc->sc_mii.mii_writereg = ste_mii_writereg;
	sc->sc_mii.mii_statchg = ste_mii_statchg;
	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, ether_mediachange,
	    ether_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	ifp = &sc->sc_ethercom.ec_if;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ste_ioctl;
	ifp->if_start = ste_start;
	ifp->if_watchdog = ste_watchdog;
	ifp->if_init = ste_init;
	ifp->if_stop = ste_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Default the transmit threshold to 128 bytes.
	 */
	sc->sc_txthresh = 128;

	/*
	 * Disable MWI if the PCI layer tells us to.
	 */
	sc->sc_DMACtrl = 0;
	if ((pa->pa_flags & PCI_FLAGS_MWI_OKAY) == 0)
		sc->sc_DMACtrl |= DC_MWIDisable;

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	if (pmf_device_register1(self, NULL, NULL, ste_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < STE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].ds_dmamap);
	}
 fail_4:
	for (i = 0; i < STE_NTXDESC; i++) {
		if (sc->sc_txsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].ds_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct ste_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * ste_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static bool
ste_shutdown(device_t self, int howto)
{
	struct ste_softc *sc;

	sc = device_private(self);
	ste_stop(&sc->sc_ethercom.ec_if, 1);

	return true;
}

static void
ste_dmahalt_wait(struct ste_softc *sc)
{
	int i;

	for (i = 0; i < STE_TIMEOUT; i++) {
		delay(2);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STE_DMACtrl) &
		     DC_DMAHaltBusy) == 0)
			break;
	}

	if (i == STE_TIMEOUT)
		printf("%s: DMA halt timed out\n", device_xname(sc->sc_dev));
}

/*
 * ste_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
ste_start(struct ifnet *ifp)
{
	struct ste_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct ste_descsoft *ds;
	struct ste_tfd *tfd;
	bus_dmamap_t dmamap;
	int error, olasttx, nexttx, opending, seg, totlen;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of pending transmissions
	 * and the current last descriptor in the list.
	 */
	opending = sc->sc_txpending;
	olasttx = sc->sc_txlast;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (sc->sc_txpending < STE_NTXDESC) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		/*
		 * Get the last and next available transmit descriptor.
		 */
		nexttx = STE_NEXTTX(sc->sc_txlast);
		tfd = &sc->sc_txdescs[nexttx];
		ds = &sc->sc_txsoft[nexttx];

		dmamap = ds->ds_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.  In this case, we'll copy
		 * and try again.
		 */
		if (bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    device_xname(sc->sc_dev));
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n", device_xname(sc->sc_dev));
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
			    m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n", device_xname(sc->sc_dev), error);
				break;
			}
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

		/* Initialize the fragment list. */
		for (totlen = 0, seg = 0; seg < dmamap->dm_nsegs; seg++) {
			tfd->tfd_frags[seg].frag_addr =
			    htole32(dmamap->dm_segs[seg].ds_addr);
			tfd->tfd_frags[seg].frag_len =
			    htole32(dmamap->dm_segs[seg].ds_len);
			totlen += dmamap->dm_segs[seg].ds_len;
		}
		tfd->tfd_frags[seg - 1].frag_len |= htole32(FRAG_LAST);

		/* Initialize the descriptor. */
		tfd->tfd_next = htole32(STE_CDTXADDR(sc, nexttx));
		tfd->tfd_control = htole32(TFD_FrameId(nexttx) | (totlen & 3));

		/* Sync the descriptor. */
		STE_CDTXSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
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

	if (sc->sc_txpending == STE_NTXDESC) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/*
		 * We enqueued packets.  If the transmitter was idle,
		 * reset the txdirty pointer.
		 */
		if (opending == 0)
			sc->sc_txdirty = STE_NEXTTX(olasttx);

		/*
		 * Cause a descriptor interrupt to happen on the
		 * last packet we enqueued, and also cause the
		 * DMA engine to wait after is has finished processing
		 * it.
		 */
		sc->sc_txdescs[sc->sc_txlast].tfd_next = 0;
		sc->sc_txdescs[sc->sc_txlast].tfd_control |=
		    htole32(TFD_TxDMAIndicate);
		STE_CDTXSYNC(sc, sc->sc_txlast,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Link up the new chain of descriptors to the
		 * last.
		 */
		sc->sc_txdescs[olasttx].tfd_next =
		    htole32(STE_CDTXADDR(sc, STE_NEXTTX(olasttx)));
		STE_CDTXSYNC(sc, olasttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Kick the transmit DMA logic.  Note that since we're
		 * using auto-polling, reading the Tx desc pointer will
		 * give it the nudge it needs to get going.
		 */
		if (bus_space_read_4(sc->sc_st, sc->sc_sh,
		    STE_TxDMAListPtr) == 0) {
			bus_space_write_4(sc->sc_st, sc->sc_sh,
			    STE_DMACtrl, DC_TxDMAHalt);
			ste_dmahalt_wait(sc);
			bus_space_write_4(sc->sc_st, sc->sc_sh,
			    STE_TxDMAListPtr,
			    STE_CDTXADDR(sc, STE_NEXTTX(olasttx)));
			bus_space_write_4(sc->sc_st, sc->sc_sh,
			    STE_DMACtrl, DC_TxDMAResume);
		}

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * ste_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
ste_watchdog(struct ifnet *ifp)
{
	struct ste_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;

	ste_txintr(sc);
	ste_rxintr(sc);
	(void) ste_init(ifp);

	/* Try to get more packets going. */
	ste_start(ifp);
}

/*
 * ste_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
ste_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ste_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			ste_set_filter(sc);
		error = 0;
	}

	/* Try to get more packets going. */
	ste_start(ifp);

	splx(s);
	return (error);
}

/*
 * ste_intr:
 *
 *	Interrupt service routine.
 */
static int
ste_intr(void *arg)
{
	struct ste_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint16_t isr;
	uint8_t txstat;
	int wantinit;

	if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STE_IntStatus) &
	     IS_InterruptStatus) == 0)
		return (0);

	for (wantinit = 0; wantinit == 0;) {
		isr = bus_space_read_2(sc->sc_st, sc->sc_sh, STE_IntStatusAck);
		if ((isr & sc->sc_IntEnable) == 0)
			break;

		/* Receive interrupts. */
		if (isr & IE_RxDMAComplete)
			ste_rxintr(sc);

		/* Transmit interrupts. */
		if (isr & (IE_TxDMAComplete|IE_TxComplete))
			ste_txintr(sc);

		/* Statistics overflow. */
		if (isr & IE_UpdateStats)
			ste_stats_update(sc);

		/* Transmission errors. */
		if (isr & IE_TxComplete) {
			for (;;) {
				txstat = bus_space_read_1(sc->sc_st, sc->sc_sh,
				    STE_TxStatus);
				if ((txstat & TS_TxComplete) == 0)
					break;
				if (txstat & TS_TxUnderrun) {
					sc->sc_txthresh += 32;
					if (sc->sc_txthresh > 0x1ffc)
						sc->sc_txthresh = 0x1ffc;
					printf("%s: transmit underrun, new "
					    "threshold: %d bytes\n",
					    device_xname(sc->sc_dev),
					    sc->sc_txthresh);
					ste_reset(sc, AC_TxReset | AC_DMA |
					    AC_FIFO | AC_Network);
					ste_setthresh(sc);
					bus_space_write_1(sc->sc_st, sc->sc_sh,
					    STE_TxDMAPollPeriod, 127);
					ste_txrestart(sc,
					    bus_space_read_1(sc->sc_st,
						sc->sc_sh, STE_TxFrameId));
				}
				if (txstat & TS_TxReleaseError) {
					printf("%s: Tx FIFO release error\n",
					    device_xname(sc->sc_dev));
					wantinit = 1;
				}
				if (txstat & TS_MaxCollisions) {
					printf("%s: excessive collisions\n",
					    device_xname(sc->sc_dev));
					wantinit = 1;
				}
				if (txstat & TS_TxStatusOverflow) {
					printf("%s: status overflow\n",
					    device_xname(sc->sc_dev));
					wantinit = 1;
				}
				bus_space_write_2(sc->sc_st, sc->sc_sh,
				    STE_TxStatus, 0);
			}
		}

		/* Host interface errors. */
		if (isr & IE_HostError) {
			printf("%s: Host interface error\n",
			    device_xname(sc->sc_dev));
			wantinit = 1;
		}
	}

	if (wantinit)
		ste_init(ifp);

	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_IntEnable,
	    sc->sc_IntEnable);

	/* Try to get more packets going. */
	ste_start(ifp);

	return (1);
}

/*
 * ste_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
ste_txintr(struct ste_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ste_descsoft *ds;
	uint32_t control;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txdirty; sc->sc_txpending != 0;
	     i = STE_NEXTTX(i), sc->sc_txpending--) {
		ds = &sc->sc_txsoft[i];

		STE_CDTXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		control = le32toh(sc->sc_txdescs[i].tfd_control);
		if ((control & TFD_TxDMAComplete) == 0)
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
 * ste_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
ste_rxintr(struct ste_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ste_descsoft *ds;
	struct mbuf *m;
	uint32_t status;
	int i, len;

	for (i = sc->sc_rxptr;; i = STE_NEXTRX(i)) {
		ds = &sc->sc_rxsoft[i];

		STE_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = le32toh(sc->sc_rxdescs[i].rfd_status);

		if ((status & RFD_RxDMAComplete) == 0)
			break;

		/*
		 * If the packet had an error, simply recycle the
		 * buffer.  Note, we count the error later in the
		 * periodic stats update.
		 */
		if (status & RFD_RxFrameError) {
			STE_INIT_RXDESC(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.  Note, we have
		 * configured the chip to not include the CRC at
		 * the end of the packet.
		 */
		len = RFD_RxDMAFrameLen(status);

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 *
		 * Otherwise, we add a new buffer to the receive
		 * chain.  If this fails, we drop the packet and
		 * recycle the old buffer.
		 */
		if (ste_copy_small != 0 && len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			memcpy(mtod(m, void *),
			    mtod(ds->ds_mbuf, void *), len);
			STE_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
		} else {
			m = ds->ds_mbuf;
			if (ste_add_rxbuf(sc, i) != 0) {
 dropit:
				ifp->if_ierrors++;
				STE_INIT_RXDESC(sc, i);
				bus_dmamap_sync(sc->sc_dmat,
				    ds->ds_dmamap, 0,
				    ds->ds_dmamap->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * ste_tick:
 *
 *	One second timer, used to tick the MII.
 */
static void
ste_tick(void *arg)
{
	struct ste_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	ste_stats_update(sc);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, ste_tick, sc);
}

/*
 * ste_stats_update:
 *
 *	Read the ST-201 statistics counters.
 */
static void
ste_stats_update(struct ste_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	(void) bus_space_read_2(st, sh, STE_OctetsReceivedOk0);
	(void) bus_space_read_2(st, sh, STE_OctetsReceivedOk1);

	(void) bus_space_read_2(st, sh, STE_OctetsTransmittedOk0);
	(void) bus_space_read_2(st, sh, STE_OctetsTransmittedOk1);

	ifp->if_opackets +=
	    (u_int) bus_space_read_2(st, sh, STE_FramesTransmittedOK);
	ifp->if_ipackets +=
	    (u_int) bus_space_read_2(st, sh, STE_FramesReceivedOK);

	ifp->if_collisions +=
	    (u_int) bus_space_read_1(st, sh, STE_LateCollisions) +
	    (u_int) bus_space_read_1(st, sh, STE_MultipleColFrames) +
	    (u_int) bus_space_read_1(st, sh, STE_SingleColFrames);

	(void) bus_space_read_1(st, sh, STE_FramesWDeferredXmt);

	ifp->if_ierrors +=
	    (u_int) bus_space_read_1(st, sh, STE_FramesLostRxErrors);

	ifp->if_oerrors +=
	    (u_int) bus_space_read_1(st, sh, STE_FramesWExDeferral) +
	    (u_int) bus_space_read_1(st, sh, STE_FramesXbortXSColls) +
	    bus_space_read_1(st, sh, STE_CarrierSenseErrors);

	(void) bus_space_read_1(st, sh, STE_BcstFramesXmtdOk);
	(void) bus_space_read_1(st, sh, STE_BcstFramesRcvdOk);
	(void) bus_space_read_1(st, sh, STE_McstFramesXmtdOk);
	(void) bus_space_read_1(st, sh, STE_McstFramesRcvdOk);
}

/*
 * ste_reset:
 *
 *	Perform a soft reset on the ST-201.
 */
static void
ste_reset(struct ste_softc *sc, u_int32_t rstbits)
{
	uint32_t ac;
	int i;

	ac = bus_space_read_4(sc->sc_st, sc->sc_sh, STE_AsicCtrl);

	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_AsicCtrl, ac | rstbits);

	delay(50000);

	for (i = 0; i < STE_TIMEOUT; i++) {
		delay(1000);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STE_AsicCtrl) &
		     AC_ResetBusy) == 0)
			break;
	}

	if (i == STE_TIMEOUT)
		printf("%s: reset failed to complete\n", device_xname(sc->sc_dev));

	delay(1000);
}

/*
 * ste_setthresh:
 *
 * 	set the various transmit threshold registers
 */
static void
ste_setthresh(struct ste_softc *sc)
{
	/* set the TX threhold */
	bus_space_write_2(sc->sc_st, sc->sc_sh,
	    STE_TxStartThresh, sc->sc_txthresh);
	/* Urgent threshold: set to sc_txthresh / 2 */
	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_TxDMAUrgentThresh,
	    sc->sc_txthresh >> 6);
	/* Burst threshold: use default value (256 bytes) */
}

/*
 * restart TX at the given frame ID in the transmitter ring
 */
static void
ste_txrestart(struct ste_softc *sc, u_int8_t id)
{
	u_int32_t control;

	STE_CDTXSYNC(sc, id, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	control = le32toh(sc->sc_txdescs[id].tfd_control);
	control &= ~TFD_TxDMAComplete;
	sc->sc_txdescs[id].tfd_control = htole32(control);
	STE_CDTXSYNC(sc, id, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_TxDMAListPtr, 0);
	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_MacCtrl1, MC1_TxEnable);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_DMACtrl, DC_TxDMAHalt);
	ste_dmahalt_wait(sc);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_TxDMAListPtr,
	    STE_CDTXADDR(sc, id));
	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_DMACtrl, DC_TxDMAResume);
}

/*
 * ste_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
ste_init(struct ifnet *ifp)
{
	struct ste_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ste_descsoft *ds;
	int i, error = 0;

	/*
	 * Cancel any pending I/O.
	 */
	ste_stop(ifp, 0);

	/*
	 * Reset the chip to a known state.
	 */
	ste_reset(sc, AC_GlobalReset | AC_RxReset | AC_TxReset | AC_DMA |
	    AC_FIFO | AC_Network | AC_Host | AC_AutoInit | AC_RstOut);

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = STE_NTXDESC - 1;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < STE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf == NULL) {
			if ((error = ste_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				ste_rxdrain(sc);
				goto out;
			}
		} else
			STE_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/* Set the station address. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		bus_space_write_1(st, sh, STE_StationAddress0 + 1,
		    CLLADDR(ifp->if_sadl)[i]);

	/* Set up the receive filter. */
	ste_set_filter(sc);

	/*
	 * Give the receive ring to the chip.
	 */
	bus_space_write_4(st, sh, STE_RxDMAListPtr,
	    STE_CDRXADDR(sc, sc->sc_rxptr));

	/*
	 * We defer giving the transmit ring to the chip until we
	 * transmit the first packet.
	 */

	/*
	 * Initialize the Tx auto-poll period.  It's OK to make this number
	 * large (127 is the max) -- we explicitly kick the transmit engine
	 * when there's actually a packet.  We are using auto-polling only
	 * to make the interface to the transmit engine not suck.
	 */
	bus_space_write_1(sc->sc_st, sc->sc_sh, STE_TxDMAPollPeriod, 127);

	/* ..and the Rx auto-poll period. */
	bus_space_write_1(st, sh, STE_RxDMAPollPeriod, 64);

	/* Initialize the Tx start threshold. */
	ste_setthresh(sc);

	/* Set the FIFO release threshold to 512 bytes. */
	bus_space_write_1(st, sh, STE_TxReleaseThresh, 512 >> 4);

	/* Set maximum packet size for VLAN. */
	if (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU)
		bus_space_write_2(st, sh, STE_MaxFrameSize, ETHER_MAX_LEN + 4);
	else
		bus_space_write_2(st, sh, STE_MaxFrameSize, ETHER_MAX_LEN);

	/*
	 * Initialize the interrupt mask.
	 */
	sc->sc_IntEnable = IE_HostError | IE_TxComplete | IE_UpdateStats |
	    IE_TxDMAComplete | IE_RxDMAComplete;

	bus_space_write_2(st, sh, STE_IntStatus, 0xffff);
	bus_space_write_2(st, sh, STE_IntEnable, sc->sc_IntEnable);

	/*
	 * Start the receive DMA engine.
	 */
	bus_space_write_4(st, sh, STE_DMACtrl, sc->sc_DMACtrl | DC_RxDMAResume);

	/*
	 * Initialize MacCtrl0 -- do it before setting the media,
	 * as setting the media will actually program the register.
	 */
	sc->sc_MacCtrl0 = MC0_IFSSelect(0);
	if (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU)
		sc->sc_MacCtrl0 |= MC0_RcvLargeFrames;

	/*
	 * Set the current media.
	 */
	if ((error = ether_mediachange(ifp)) != 0)
		goto out;

	/*
	 * Start the MAC.
	 */
	bus_space_write_2(st, sh, STE_MacCtrl1,
	    MC1_StatisticsEnable | MC1_TxEnable | MC1_RxEnable);

	/*
	 * Start the one second MII clock.
	 */
	callout_reset(&sc->sc_tick_ch, hz, ste_tick, sc);

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
 * ste_drain:
 *
 *	Drain the receive queue.
 */
static void
ste_rxdrain(struct ste_softc *sc)
{
	struct ste_descsoft *ds;
	int i;

	for (i = 0; i < STE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}
}

/*
 * ste_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
static void
ste_stop(struct ifnet *ifp, int disable)
{
	struct ste_softc *sc = ifp->if_softc;
	struct ste_descsoft *ds;
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
	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_IntEnable, 0);

	/*
	 * Stop receiver, transmitter, and stats update.
	 */
	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_MacCtrl1,
	    MC1_StatisticsDisable | MC1_TxDisable | MC1_RxDisable);

	/*
	 * Stop the transmit and receive DMA.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, STE_DMACtrl,
	    DC_RxDMAHalt | DC_TxDMAHalt);
	ste_dmahalt_wait(sc);

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < STE_NTXDESC; i++) {
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
		ste_rxdrain(sc);
}

static int
ste_eeprom_wait(struct ste_softc *sc)
{
	int i;

	for (i = 0; i < STE_TIMEOUT; i++) {
		delay(1000);
		if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STE_EepromCtrl) &
		     EC_EepromBusy) == 0)
			return (0);
	}
	return (1);
}

/*
 * ste_read_eeprom:
 *
 *	Read data from the serial EEPROM.
 */
static void
ste_read_eeprom(struct ste_softc *sc, int offset, uint16_t *data)
{

	if (ste_eeprom_wait(sc))
		printf("%s: EEPROM failed to come ready\n",
		    device_xname(sc->sc_dev));

	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_EepromCtrl,
	    EC_EepromAddress(offset) | EC_EepromOpcode(EC_OP_R));
	if (ste_eeprom_wait(sc))
		printf("%s: EEPROM read timed out\n",
		    device_xname(sc->sc_dev));
	*data = bus_space_read_2(sc->sc_st, sc->sc_sh, STE_EepromData);
}

/*
 * ste_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
ste_add_rxbuf(struct ste_softc *sc, int idx)
{
	struct ste_descsoft *ds = &sc->sc_rxsoft[idx];
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

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, ds->ds_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("ste_add_rxbuf");		/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
	    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	STE_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * ste_set_filter:
 *
 *	Set up the receive filter.
 */
static void
ste_set_filter(struct ste_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc;
	uint16_t mchash[4];

	sc->sc_ReceiveMode = RM_ReceiveUnicast;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_ReceiveMode |= RM_ReceiveBroadcast;

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
		mchash[crc >> 4] |= 1 << (crc & 0xf);

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
		bus_space_write_2(sc->sc_st, sc->sc_sh, STE_HashTable0,
		    mchash[0]);
		bus_space_write_2(sc->sc_st, sc->sc_sh, STE_HashTable1,
		    mchash[1]);
		bus_space_write_2(sc->sc_st, sc->sc_sh, STE_HashTable2,
		    mchash[2]);
		bus_space_write_2(sc->sc_st, sc->sc_sh, STE_HashTable3,
		    mchash[3]);
	}

	bus_space_write_1(sc->sc_st, sc->sc_sh, STE_ReceiveMode,
	    sc->sc_ReceiveMode);
}

/*
 * ste_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII of the ST-201.
 */
static int
ste_mii_readreg(device_t self, int phy, int reg)
{

	return (mii_bitbang_readreg(self, &ste_mii_bitbang_ops, phy, reg));
}

/*
 * ste_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII of the ST-201.
 */
static void
ste_mii_writereg(device_t self, int phy, int reg, int val)
{

	mii_bitbang_writereg(self, &ste_mii_bitbang_ops, phy, reg, val);
}

/*
 * ste_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
ste_mii_statchg(struct ifnet *ifp)
{
	struct ste_softc *sc = ifp->if_softc;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_MacCtrl0 |= MC0_FullDuplexEnable;
	else
		sc->sc_MacCtrl0 &= ~MC0_FullDuplexEnable;

	/* XXX 802.1x flow-control? */

	bus_space_write_2(sc->sc_st, sc->sc_sh, STE_MacCtrl0, sc->sc_MacCtrl0);
}

/*
 * ste_mii_bitbang_read: [mii bit-bang interface function]
 *
 *	Read the MII serial port for the MII bit-bang module.
 */
static uint32_t
ste_mii_bitbang_read(device_t self)
{
	struct ste_softc *sc = device_private(self);

	return (bus_space_read_1(sc->sc_st, sc->sc_sh, STE_PhyCtrl));
}

/*
 * ste_mii_bitbang_write: [mii big-bang interface function]
 *
 *	Write the MII serial port for the MII bit-bang module.
 */
static void
ste_mii_bitbang_write(device_t self, uint32_t val)
{
	struct ste_softc *sc = device_private(self);

	bus_space_write_1(sc->sc_st, sc->sc_sh, STE_PhyCtrl, val);
}
