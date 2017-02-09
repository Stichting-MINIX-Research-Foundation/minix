/*	$NetBSD: if_age.c,v 1.45 2015/04/13 16:33:25 riastradh Exp $ */
/*	$OpenBSD: if_age.c,v 1.1 2009/01/16 05:00:34 kevlo Exp $	*/

/*-
 * Copyright (c) 2008, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

/* Driver for Attansic Technology Corp. L1 Gigabit Ethernet. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_age.c,v 1.45 2015/04/13 16:33:25 riastradh Exp $");

#include "vlan.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/if_types.h>
#include <net/if_vlanvar.h>

#include <net/bpf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_agereg.h>

static int	age_match(device_t, cfdata_t, void *);
static void	age_attach(device_t, device_t, void *);
static int	age_detach(device_t, int);

static bool	age_resume(device_t, const pmf_qual_t *);

static int	age_miibus_readreg(device_t, int, int);
static void	age_miibus_writereg(device_t, int, int, int);
static void	age_miibus_statchg(struct ifnet *);

static int	age_init(struct ifnet *);
static int	age_ioctl(struct ifnet *, u_long, void *);
static void	age_start(struct ifnet *);
static void	age_watchdog(struct ifnet *);
static bool	age_shutdown(device_t, int);
static void	age_mediastatus(struct ifnet *, struct ifmediareq *);
static int	age_mediachange(struct ifnet *);

static int	age_intr(void *);
static int	age_dma_alloc(struct age_softc *);
static void	age_dma_free(struct age_softc *);
static void	age_get_macaddr(struct age_softc *, uint8_t[]);
static void	age_phy_reset(struct age_softc *);

static int	age_encap(struct age_softc *, struct mbuf **);
static void	age_init_tx_ring(struct age_softc *);
static int	age_init_rx_ring(struct age_softc *);
static void	age_init_rr_ring(struct age_softc *);
static void	age_init_cmb_block(struct age_softc *);
static void	age_init_smb_block(struct age_softc *);
static int	age_newbuf(struct age_softc *, struct age_rxdesc *, int);
static void	age_mac_config(struct age_softc *);
static void	age_txintr(struct age_softc *, int);
static void	age_rxeof(struct age_softc *sc, struct rx_rdesc *);
static void	age_rxintr(struct age_softc *, int);
static void	age_tick(void *);
static void	age_reset(struct age_softc *);
static void	age_stop(struct ifnet *, int);
static void	age_stats_update(struct age_softc *);
static void	age_stop_txmac(struct age_softc *);
static void	age_stop_rxmac(struct age_softc *);
static void	age_rxvlan(struct age_softc *sc);
static void	age_rxfilter(struct age_softc *);

CFATTACH_DECL_NEW(age, sizeof(struct age_softc),
    age_match, age_attach, age_detach, NULL);

int agedebug = 0;
#define	DPRINTF(x)	do { if (agedebug) printf x; } while (0)

#define ETHER_ALIGN 2
#define AGE_CSUM_FEATURES	(M_CSUM_TCPv4 | M_CSUM_UDPv4)

static int
age_match(device_t dev, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	return (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ATTANSIC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATTANSIC_ETHERNET_GIGA);
}

static void
age_attach(device_t parent, device_t self, void *aux)
{
	struct age_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	pcireg_t memtype;
	int error = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive("\n");
	aprint_normal(": Attansic/Atheros L1 Gigabit Ethernet\n");

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	/*
	 * Allocate IO memory
	 */
	memtype = pci_mapreg_type(sc->sc_pct, sc->sc_pcitag, AGE_PCIR_BAR);
	switch (memtype) {
        case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
        case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT_1M:
        case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
        default:
		aprint_error_dev(self, "invalid base address register\n");
		break;
	}

	if (pci_mapreg_map(pa, AGE_PCIR_BAR, memtype, 0, &sc->sc_mem_bt,
	    &sc->sc_mem_bh, NULL, &sc->sc_mem_size) != 0) {
		aprint_error_dev(self, "could not map mem space\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "could not map interrupt\n");
		goto fail;
	}

	/*
	 * Allocate IRQ
	 */
	intrstr = pci_intr_string(sc->sc_pct, ih, intrbuf, sizeof(intrbuf));
	sc->sc_irq_handle = pci_intr_establish(sc->sc_pct, ih, IPL_NET,
	    age_intr, sc);
	if (sc->sc_irq_handle == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "%s\n", intrstr);

	/* Set PHY address. */
	sc->age_phyaddr = AGE_PHY_ADDR;

	/* Reset PHY. */
	age_phy_reset(sc);

	/* Reset the ethernet controller. */
	age_reset(sc);

	/* Get PCI and chip id/revision. */
	sc->age_rev = PCI_REVISION(pa->pa_class);
	sc->age_chip_rev = CSR_READ_4(sc, AGE_MASTER_CFG) >>
	    MASTER_CHIP_REV_SHIFT;

	aprint_debug_dev(self, "PCI device revision : 0x%04x\n", sc->age_rev);
	aprint_debug_dev(self, "Chip id/revision : 0x%04x\n", sc->age_chip_rev);

	if (agedebug) {
		aprint_debug_dev(self, "%d Tx FIFO, %d Rx FIFO\n",
		    CSR_READ_4(sc, AGE_SRAM_TX_FIFO_LEN),
		    CSR_READ_4(sc, AGE_SRAM_RX_FIFO_LEN));
	}

	/* Set max allowable DMA size. */
	sc->age_dma_rd_burst = DMA_CFG_RD_BURST_128;
	sc->age_dma_wr_burst = DMA_CFG_WR_BURST_128;

	/* Allocate DMA stuffs */
	error = age_dma_alloc(sc);
	if (error)
		goto fail;

	callout_init(&sc->sc_tick_ch, 0);
	callout_setfunc(&sc->sc_tick_ch, age_tick, sc);

	/* Load station address. */
	age_get_macaddr(sc, sc->sc_enaddr);

	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = age_init;
	ifp->if_ioctl = age_ioctl;
	ifp->if_start = age_start;
	ifp->if_stop = age_stop;
	ifp->if_watchdog = age_watchdog;
	ifp->if_baudrate = IF_Gbps(1);
	IFQ_SET_MAXLEN(&ifp->if_snd, AGE_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	sc->sc_ec.ec_capabilities = ETHERCAP_VLAN_MTU;

	ifp->if_capabilities |= IFCAP_CSUM_IPv4_Rx |
				IFCAP_CSUM_TCPv4_Rx |
				IFCAP_CSUM_UDPv4_Rx;
#ifdef AGE_CHECKSUM
	ifp->if_capabilities |= IFCAP_CSUM_IPv4_Tx |
				IFCAP_CSUM_TCPv4_Tx |
				IFCAP_CSUM_UDPv4_Tx;
#endif

#if NVLAN > 0
	sc->sc_ec.ec_capabilities |= ETHERCAP_VLAN_HWTAGGING;
#endif

	/* Set up MII bus. */
	sc->sc_miibus.mii_ifp = ifp;
	sc->sc_miibus.mii_readreg = age_miibus_readreg;
	sc->sc_miibus.mii_writereg = age_miibus_writereg;
	sc->sc_miibus.mii_statchg = age_miibus_statchg;

	sc->sc_ec.ec_mii = &sc->sc_miibus;
	ifmedia_init(&sc->sc_miibus.mii_media, 0, age_mediachange,
	    age_mediastatus);
	mii_attach(self, &sc->sc_miibus, 0xffffffff, MII_PHY_ANY,
	   MII_OFFSET_ANY, MIIF_DOPAUSE);

	if (LIST_FIRST(&sc->sc_miibus.mii_phys) == NULL) {
		aprint_error_dev(self, "no PHY found!\n");
		ifmedia_add(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	if (pmf_device_register1(self, NULL, age_resume, age_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

fail:
	age_dma_free(sc);
	if (sc->sc_irq_handle != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_irq_handle);
		sc->sc_irq_handle = NULL;
	}
	if (sc->sc_mem_size) {
		bus_space_unmap(sc->sc_mem_bt, sc->sc_mem_bh, sc->sc_mem_size);
		sc->sc_mem_size = 0;
	}
}

static int
age_detach(device_t self, int flags)
{
	struct age_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int s;

	pmf_device_deregister(self);
	s = splnet();
	age_stop(ifp, 0);
	splx(s);

	mii_detach(&sc->sc_miibus, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_miibus.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);
	age_dma_free(sc);

	if (sc->sc_irq_handle != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_irq_handle);
		sc->sc_irq_handle = NULL;
	}
	if (sc->sc_mem_size) {
		bus_space_unmap(sc->sc_mem_bt, sc->sc_mem_bh, sc->sc_mem_size);
		sc->sc_mem_size = 0;
	}
	return 0;
}

/*
 *	Read a PHY register on the MII of the L1.
 */
static int
age_miibus_readreg(device_t dev, int phy, int reg)
{
	struct age_softc *sc = device_private(dev);
	uint32_t v;
	int i;

	if (phy != sc->age_phyaddr)
		return 0;

	CSR_WRITE_4(sc, AGE_MDIO, MDIO_OP_EXECUTE | MDIO_OP_READ |
	    MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));
	for (i = AGE_PHY_TIMEOUT; i > 0; i--) {
		DELAY(1);
		v = CSR_READ_4(sc, AGE_MDIO);
		if ((v & (MDIO_OP_EXECUTE | MDIO_OP_BUSY)) == 0)
			break;
	}

	if (i == 0) {
		printf("%s: phy read timeout: phy %d, reg %d\n",
			device_xname(sc->sc_dev), phy, reg);
		return 0;
	}

	return ((v & MDIO_DATA_MASK) >> MDIO_DATA_SHIFT);
}

/*
 * 	Write a PHY register on the MII of the L1.
 */
static void
age_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct age_softc *sc = device_private(dev);
	uint32_t v;
	int i;

	if (phy != sc->age_phyaddr)
		return;

	CSR_WRITE_4(sc, AGE_MDIO, MDIO_OP_EXECUTE | MDIO_OP_WRITE |
	    (val & MDIO_DATA_MASK) << MDIO_DATA_SHIFT |
	    MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));

	for (i = AGE_PHY_TIMEOUT; i > 0; i--) {
		DELAY(1);
		v = CSR_READ_4(sc, AGE_MDIO);
		if ((v & (MDIO_OP_EXECUTE | MDIO_OP_BUSY)) == 0)
			break;
	}

	if (i == 0) {
		printf("%s: phy write timeout: phy %d, reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
	}
}

/*
 *	Callback from MII layer when media changes.
 */
static void
age_miibus_statchg(struct ifnet *ifp)
{
	struct age_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	sc->age_flags &= ~AGE_FLAG_LINK;
	if ((mii->mii_media_status & IFM_AVALID) != 0) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
		case IFM_1000_T:
			sc->age_flags |= AGE_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	/* Stop Rx/Tx MACs. */
	age_stop_rxmac(sc);
	age_stop_txmac(sc);

	/* Program MACs with resolved speed/duplex/flow-control. */
	if ((sc->age_flags & AGE_FLAG_LINK) != 0) {
		uint32_t reg;

		age_mac_config(sc);
		reg = CSR_READ_4(sc, AGE_MAC_CFG);
		/* Restart DMA engine and Tx/Rx MAC. */
		CSR_WRITE_4(sc, AGE_DMA_CFG, CSR_READ_4(sc, AGE_DMA_CFG) |
		    DMA_CFG_RD_ENB | DMA_CFG_WR_ENB);
		reg |= MAC_CFG_TX_ENB | MAC_CFG_RX_ENB;
		CSR_WRITE_4(sc, AGE_MAC_CFG, reg);
	}
}

/*
 *	Get the current interface media status.
 */
static void
age_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct age_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;

	mii_pollstat(mii);
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = mii->mii_media_active;
}

/*
 *	Set hardware to newly-selected media.
 */
static int
age_mediachange(struct ifnet *ifp)
{
	struct age_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;
	int error;

	if (mii->mii_instance != 0) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);

	return error;
}

static int
age_intr(void *arg)
{
        struct age_softc *sc = arg;
        struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct cmb *cmb;
        uint32_t status;

	status = CSR_READ_4(sc, AGE_INTR_STATUS);
	if (status == 0 || (status & AGE_INTRS) == 0)
		return 0;

	cmb = sc->age_rdata.age_cmb_block;
	if (cmb == NULL) {
		/* Happens when bringing up the interface
		 * w/o having a carrier. Ack the interrupt.
		 */
		CSR_WRITE_4(sc, AGE_INTR_STATUS, status);
		return 0;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_cmb_block_map, 0,
	    sc->age_cdata.age_cmb_block_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	status = le32toh(cmb->intr_status);
	/* ACK/reenable interrupts */
	CSR_WRITE_4(sc, AGE_INTR_STATUS, status);
	while ((status & AGE_INTRS) != 0) {
		sc->age_tpd_cons = (le32toh(cmb->tpd_cons) & TPD_CONS_MASK) >>
		    TPD_CONS_SHIFT;
		sc->age_rr_prod = (le32toh(cmb->rprod_cons) & RRD_PROD_MASK) >>
		    RRD_PROD_SHIFT;

		/* Let hardware know CMB was served. */
		cmb->intr_status = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_cmb_block_map, 0,
		    sc->age_cdata.age_cmb_block_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		if (ifp->if_flags & IFF_RUNNING) {
			if (status & INTR_CMB_RX)
				age_rxintr(sc, sc->age_rr_prod);

			if (status & INTR_CMB_TX)
				age_txintr(sc, sc->age_tpd_cons);

			if (status & (INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST)) {
				if (status & INTR_DMA_RD_TO_RST)
					printf("%s: DMA read error! -- "
					    "resetting\n",
					    device_xname(sc->sc_dev));
				if (status & INTR_DMA_WR_TO_RST)
					printf("%s: DMA write error! -- "
					    "resetting\n",
					    device_xname(sc->sc_dev));
				age_init(ifp);
			}

			age_start(ifp);

			if (status & INTR_SMB)
				age_stats_update(sc);
		}
		/* check if more interrupts did came in */
		bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_cmb_block_map, 0,
		    sc->age_cdata.age_cmb_block_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		status = le32toh(cmb->intr_status);
	}

	return 1;
}

static void
age_get_macaddr(struct age_softc *sc, uint8_t eaddr[])
{
	uint32_t ea[2], reg;
	int i, vpdc;

	reg = CSR_READ_4(sc, AGE_SPI_CTRL);
	if ((reg & SPI_VPD_ENB) != 0) {
		/* Get VPD stored in TWSI EEPROM. */
		reg &= ~SPI_VPD_ENB;
		CSR_WRITE_4(sc, AGE_SPI_CTRL, reg);
	}

	if (pci_get_capability(sc->sc_pct, sc->sc_pcitag,
	    PCI_CAP_VPD, &vpdc, NULL)) {
		/*
		 * PCI VPD capability found, let TWSI reload EEPROM.
		 * This will set Ethernet address of controller.
		 */
		CSR_WRITE_4(sc, AGE_TWSI_CTRL, CSR_READ_4(sc, AGE_TWSI_CTRL) |
		    TWSI_CTRL_SW_LD_START);
		for (i = 100; i > 0; i++) {
			DELAY(1000);
			reg = CSR_READ_4(sc, AGE_TWSI_CTRL);
			if ((reg & TWSI_CTRL_SW_LD_START) == 0)
				break;
		}
		if (i == 0)
			printf("%s: reloading EEPROM timeout!\n",
			    device_xname(sc->sc_dev));
	} else {
		if (agedebug)
			printf("%s: PCI VPD capability not found!\n",
			    device_xname(sc->sc_dev));
	}

	ea[0] = CSR_READ_4(sc, AGE_PAR0);
	ea[1] = CSR_READ_4(sc, AGE_PAR1);

	eaddr[0] = (ea[1] >> 8) & 0xFF;
	eaddr[1] = (ea[1] >> 0) & 0xFF;
	eaddr[2] = (ea[0] >> 24) & 0xFF;
	eaddr[3] = (ea[0] >> 16) & 0xFF;
	eaddr[4] = (ea[0] >> 8) & 0xFF;
	eaddr[5] = (ea[0] >> 0) & 0xFF;
}

static void
age_phy_reset(struct age_softc *sc)
{
	uint16_t reg, pn;
	int i, linkup;

	/* Reset PHY. */
	CSR_WRITE_4(sc, AGE_GPHY_CTRL, GPHY_CTRL_RST);
	DELAY(2000);
	CSR_WRITE_4(sc, AGE_GPHY_CTRL, GPHY_CTRL_CLR);
	DELAY(2000);

#define ATPHY_DBG_ADDR		0x1D
#define ATPHY_DBG_DATA		0x1E
#define ATPHY_CDTC		0x16
#define PHY_CDTC_ENB		0x0001
#define PHY_CDTC_POFF		8
#define ATPHY_CDTS		0x1C
#define PHY_CDTS_STAT_OK	0x0000
#define PHY_CDTS_STAT_SHORT	0x0100
#define PHY_CDTS_STAT_OPEN	0x0200
#define PHY_CDTS_STAT_INVAL	0x0300
#define PHY_CDTS_STAT_MASK	0x0300

	/* Check power saving mode. Magic from Linux. */
	age_miibus_writereg(sc->sc_dev, sc->age_phyaddr, MII_BMCR, BMCR_RESET);
	for (linkup = 0, pn = 0; pn < 4; pn++) {
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr, ATPHY_CDTC,
		    (pn << PHY_CDTC_POFF) | PHY_CDTC_ENB);
		for (i = 200; i > 0; i--) {
			DELAY(1000);
			reg = age_miibus_readreg(sc->sc_dev, sc->age_phyaddr,
			    ATPHY_CDTC);
			if ((reg & PHY_CDTC_ENB) == 0)
				break;
		}
		DELAY(1000);
		reg = age_miibus_readreg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_CDTS);
		if ((reg & PHY_CDTS_STAT_MASK) != PHY_CDTS_STAT_OPEN) {
			linkup++;
			break;
		}
	}
	age_miibus_writereg(sc->sc_dev, sc->age_phyaddr, MII_BMCR,
	    BMCR_RESET | BMCR_AUTOEN | BMCR_STARTNEG);
	if (linkup == 0) {
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_ADDR, 0);
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_DATA, 0x124E);
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_ADDR, 1);
		reg = age_miibus_readreg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_DATA);
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_DATA, reg | 0x03);
		/* XXX */
		DELAY(1500 * 1000);
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_ADDR, 0);
		age_miibus_writereg(sc->sc_dev, sc->age_phyaddr,
		    ATPHY_DBG_DATA, 0x024E);
	}

#undef ATPHY_DBG_ADDR
#undef ATPHY_DBG_DATA
#undef ATPHY_CDTC
#undef PHY_CDTC_ENB
#undef PHY_CDTC_POFF
#undef ATPHY_CDTS
#undef PHY_CDTS_STAT_OK
#undef PHY_CDTS_STAT_SHORT
#undef PHY_CDTS_STAT_OPEN
#undef PHY_CDTS_STAT_INVAL
#undef PHY_CDTS_STAT_MASK
}

static int
age_dma_alloc(struct age_softc *sc)
{
	struct age_txdesc *txd;
	struct age_rxdesc *rxd;
	int nsegs, error, i;

	/*
	 * Create DMA stuffs for TX ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, AGE_TX_RING_SZ, 1,
	    AGE_TX_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->age_cdata.age_tx_ring_map);
	if (error) {
		sc->age_cdata.age_tx_ring_map = NULL;
		return ENOBUFS;
	}

	/* Allocate DMA'able memory for TX ring */
	error = bus_dmamem_alloc(sc->sc_dmat, AGE_TX_RING_SZ,
	    ETHER_ALIGN, 0, &sc->age_rdata.age_tx_ring_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Tx ring, "
		    "error = %i\n", device_xname(sc->sc_dev), error);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->age_rdata.age_tx_ring_seg,
	    nsegs, AGE_TX_RING_SZ, (void **)&sc->age_rdata.age_tx_ring,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error)
		return ENOBUFS;

	memset(sc->age_rdata.age_tx_ring, 0, AGE_TX_RING_SZ);

	/*  Load the DMA map for Tx ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->age_cdata.age_tx_ring_map,
	    sc->age_rdata.age_tx_ring, AGE_TX_RING_SZ, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not load DMA'able memory for Tx ring, "
		    "error = %i\n", device_xname(sc->sc_dev), error);
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_tx_ring_seg, 1);
		return error;
	}

	sc->age_rdata.age_tx_ring_paddr =
	    sc->age_cdata.age_tx_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for RX ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, AGE_RX_RING_SZ, 1,
	    AGE_RX_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->age_cdata.age_rx_ring_map);
	if (error) {
		sc->age_cdata.age_rx_ring_map = NULL;
		return ENOBUFS;
	}

	/* Allocate DMA'able memory for RX ring */
	error = bus_dmamem_alloc(sc->sc_dmat, AGE_RX_RING_SZ,
	    ETHER_ALIGN, 0, &sc->age_rdata.age_rx_ring_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Rx ring, "
		    "error = %i.\n", device_xname(sc->sc_dev), error);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->age_rdata.age_rx_ring_seg,
	    nsegs, AGE_RX_RING_SZ, (void **)&sc->age_rdata.age_rx_ring,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error)
		return ENOBUFS;

	memset(sc->age_rdata.age_rx_ring, 0, AGE_RX_RING_SZ);

	/* Load the DMA map for Rx ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->age_cdata.age_rx_ring_map,
	    sc->age_rdata.age_rx_ring, AGE_RX_RING_SZ, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not load DMA'able memory for Rx ring, "
		    "error = %i.\n", device_xname(sc->sc_dev), error);
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_rx_ring_seg, 1);
		return error;
	}

	sc->age_rdata.age_rx_ring_paddr =
	    sc->age_cdata.age_rx_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for RX return ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, AGE_RR_RING_SZ, 1,
	    AGE_RR_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->age_cdata.age_rr_ring_map);
	if (error) {
		sc->age_cdata.age_rr_ring_map = NULL;
		return ENOBUFS;
	}

	/* Allocate DMA'able memory for RX return ring */
	error = bus_dmamem_alloc(sc->sc_dmat, AGE_RR_RING_SZ,
	    ETHER_ALIGN, 0, &sc->age_rdata.age_rr_ring_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Rx "
		    "return ring, error = %i.\n",
		    device_xname(sc->sc_dev), error);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->age_rdata.age_rr_ring_seg,
	    nsegs, AGE_RR_RING_SZ, (void **)&sc->age_rdata.age_rr_ring,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error)
		return ENOBUFS;

	memset(sc->age_rdata.age_rr_ring, 0, AGE_RR_RING_SZ);

	/*  Load the DMA map for Rx return ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->age_cdata.age_rr_ring_map,
	    sc->age_rdata.age_rr_ring, AGE_RR_RING_SZ, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not load DMA'able memory for Rx return ring, "
		    "error = %i\n", device_xname(sc->sc_dev), error);
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_rr_ring_seg, 1);
		return error;
	}

	sc->age_rdata.age_rr_ring_paddr =
	    sc->age_cdata.age_rr_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for CMB block
	 */
	error = bus_dmamap_create(sc->sc_dmat, AGE_CMB_BLOCK_SZ, 1,
	    AGE_CMB_BLOCK_SZ, 0, BUS_DMA_NOWAIT,
	    &sc->age_cdata.age_cmb_block_map);
	if (error) {
		sc->age_cdata.age_cmb_block_map = NULL;
		return ENOBUFS;
	}

	/* Allocate DMA'able memory for CMB block */
	error = bus_dmamem_alloc(sc->sc_dmat, AGE_CMB_BLOCK_SZ,
	    ETHER_ALIGN, 0, &sc->age_rdata.age_cmb_block_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not allocate DMA'able memory for "
		    "CMB block, error = %i\n", device_xname(sc->sc_dev), error);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->age_rdata.age_cmb_block_seg,
	    nsegs, AGE_CMB_BLOCK_SZ, (void **)&sc->age_rdata.age_cmb_block,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error)
		return ENOBUFS;

	memset(sc->age_rdata.age_cmb_block, 0, AGE_CMB_BLOCK_SZ);

	/*  Load the DMA map for CMB block. */
	error = bus_dmamap_load(sc->sc_dmat, sc->age_cdata.age_cmb_block_map,
	    sc->age_rdata.age_cmb_block, AGE_CMB_BLOCK_SZ, NULL,
	    BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not load DMA'able memory for CMB block, "
		    "error = %i\n", device_xname(sc->sc_dev), error);
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_cmb_block_seg, 1);
		return error;
	}

	sc->age_rdata.age_cmb_block_paddr =
	    sc->age_cdata.age_cmb_block_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for SMB block
	 */
	error = bus_dmamap_create(sc->sc_dmat, AGE_SMB_BLOCK_SZ, 1,
	    AGE_SMB_BLOCK_SZ, 0, BUS_DMA_NOWAIT,
	    &sc->age_cdata.age_smb_block_map);
	if (error) {
		sc->age_cdata.age_smb_block_map = NULL;
		return ENOBUFS;
	}

	/* Allocate DMA'able memory for SMB block */
	error = bus_dmamem_alloc(sc->sc_dmat, AGE_SMB_BLOCK_SZ,
	    ETHER_ALIGN, 0, &sc->age_rdata.age_smb_block_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not allocate DMA'able memory for "
		    "SMB block, error = %i\n", device_xname(sc->sc_dev), error);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->age_rdata.age_smb_block_seg,
	    nsegs, AGE_SMB_BLOCK_SZ, (void **)&sc->age_rdata.age_smb_block,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error)
		return ENOBUFS;

	memset(sc->age_rdata.age_smb_block, 0, AGE_SMB_BLOCK_SZ);

	/*  Load the DMA map for SMB block */
	error = bus_dmamap_load(sc->sc_dmat, sc->age_cdata.age_smb_block_map,
	    sc->age_rdata.age_smb_block, AGE_SMB_BLOCK_SZ, NULL,
	    BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: could not load DMA'able memory for SMB block, "
		    "error = %i\n", device_xname(sc->sc_dev), error);
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_smb_block_seg, 1);
		return error;
	}

	sc->age_rdata.age_smb_block_paddr =
	    sc->age_cdata.age_smb_block_map->dm_segs[0].ds_addr;

	/* Create DMA maps for Tx buffers. */
	for (i = 0; i < AGE_TX_RING_CNT; i++) {
		txd = &sc->age_cdata.age_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_dmamap = NULL;
		error = bus_dmamap_create(sc->sc_dmat, AGE_TSO_MAXSIZE,
		    AGE_MAXTXSEGS, AGE_TSO_MAXSEGSIZE, 0, BUS_DMA_NOWAIT,
		    &txd->tx_dmamap);
		if (error) {
			txd->tx_dmamap = NULL;
			printf("%s: could not create Tx dmamap, error = %i.\n",
			    device_xname(sc->sc_dev), error);
			return error;
		}
	}

	/* Create DMA maps for Rx buffers. */
	error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
	    BUS_DMA_NOWAIT, &sc->age_cdata.age_rx_sparemap);
	if (error) {
		sc->age_cdata.age_rx_sparemap = NULL;
		printf("%s: could not create spare Rx dmamap, error = %i.\n",
		    device_xname(sc->sc_dev), error);
		return error;
	}
	for (i = 0; i < AGE_RX_RING_CNT; i++) {
		rxd = &sc->age_cdata.age_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_dmamap = NULL;
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &rxd->rx_dmamap);
		if (error) {
			rxd->rx_dmamap = NULL;
			printf("%s: could not create Rx dmamap, error = %i.\n",
			    device_xname(sc->sc_dev), error);
			return error;
		}
	}

	return 0;
}

static void
age_dma_free(struct age_softc *sc)
{
	struct age_txdesc *txd;
	struct age_rxdesc *rxd;
	int i;

	/* Tx buffers */
	for (i = 0; i < AGE_TX_RING_CNT; i++) {
		txd = &sc->age_cdata.age_txdesc[i];
		if (txd->tx_dmamap != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, txd->tx_dmamap);
			txd->tx_dmamap = NULL;
		}
	}
	/* Rx buffers */
	for (i = 0; i < AGE_RX_RING_CNT; i++) {
		rxd = &sc->age_cdata.age_rxdesc[i];
		if (rxd->rx_dmamap != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, rxd->rx_dmamap);
			rxd->rx_dmamap = NULL;
		}
	}
	if (sc->age_cdata.age_rx_sparemap != NULL) {
		bus_dmamap_destroy(sc->sc_dmat, sc->age_cdata.age_rx_sparemap);
		sc->age_cdata.age_rx_sparemap = NULL;
	}

	/* Tx ring. */
	if (sc->age_cdata.age_tx_ring_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->age_cdata.age_tx_ring_map);
	if (sc->age_cdata.age_tx_ring_map != NULL &&
	    sc->age_rdata.age_tx_ring != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_tx_ring_seg, 1);
	sc->age_rdata.age_tx_ring = NULL;
	sc->age_cdata.age_tx_ring_map = NULL;

	/* Rx ring. */
	if (sc->age_cdata.age_rx_ring_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->age_cdata.age_rx_ring_map);
	if (sc->age_cdata.age_rx_ring_map != NULL &&
	    sc->age_rdata.age_rx_ring != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_rx_ring_seg, 1);
	sc->age_rdata.age_rx_ring = NULL;
	sc->age_cdata.age_rx_ring_map = NULL;

	/* Rx return ring. */
	if (sc->age_cdata.age_rr_ring_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->age_cdata.age_rr_ring_map);
	if (sc->age_cdata.age_rr_ring_map != NULL &&
	    sc->age_rdata.age_rr_ring != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_rr_ring_seg, 1);
	sc->age_rdata.age_rr_ring = NULL;
	sc->age_cdata.age_rr_ring_map = NULL;

	/* CMB block */
	if (sc->age_cdata.age_cmb_block_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->age_cdata.age_cmb_block_map);
	if (sc->age_cdata.age_cmb_block_map != NULL &&
	    sc->age_rdata.age_cmb_block != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_cmb_block_seg, 1);
	sc->age_rdata.age_cmb_block = NULL;
	sc->age_cdata.age_cmb_block_map = NULL;

	/* SMB block */
	if (sc->age_cdata.age_smb_block_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->age_cdata.age_smb_block_map);
	if (sc->age_cdata.age_smb_block_map != NULL &&
	    sc->age_rdata.age_smb_block != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    &sc->age_rdata.age_smb_block_seg, 1);
	sc->age_rdata.age_smb_block = NULL;
	sc->age_cdata.age_smb_block_map = NULL;
}

static void
age_start(struct ifnet *ifp)
{
        struct age_softc *sc = ifp->if_softc;
        struct mbuf *m_head;
	int enq;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;
	if ((sc->age_flags & AGE_FLAG_LINK) == 0)
		return;
	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	enq = 0;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (age_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		enq = 1;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m_head);
	}

	if (enq) {
		/* Update mbox. */
		AGE_COMMIT_MBOX(sc);
		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = AGE_TX_TIMEOUT;
	}
}

static void
age_watchdog(struct ifnet *ifp)
{
	struct age_softc *sc = ifp->if_softc;

	if ((sc->age_flags & AGE_FLAG_LINK) == 0) {
		printf("%s: watchdog timeout (missed link)\n",
		    device_xname(sc->sc_dev));
		ifp->if_oerrors++;
		age_init(ifp);
		return;
	}

	if (sc->age_cdata.age_tx_cnt == 0) {
		printf("%s: watchdog timeout (missed Tx interrupts) "
		    "-- recovering\n", device_xname(sc->sc_dev));
		age_start(ifp);
		return;
	}

	printf("%s: watchdog timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;
	age_init(ifp);
	age_start(ifp);
}

static bool
age_shutdown(device_t self, int howto)
{
	struct age_softc *sc;
	struct ifnet *ifp;

	sc = device_private(self);
	ifp = &sc->sc_ec.ec_if;
	age_stop(ifp, 1);

	return true;
}      


static int
age_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct age_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			age_rxfilter(sc);
		error = 0;
	}

	splx(s);
	return error;
}

static void
age_mac_config(struct age_softc *sc)
{
	struct mii_data *mii;
	uint32_t reg;

	mii = &sc->sc_miibus;

	reg = CSR_READ_4(sc, AGE_MAC_CFG);
	reg &= ~MAC_CFG_FULL_DUPLEX;
	reg &= ~(MAC_CFG_TX_FC | MAC_CFG_RX_FC);
	reg &= ~MAC_CFG_SPEED_MASK;

	/* Reprogram MAC with resolved speed/duplex. */
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
	case IFM_100_TX:
		reg |= MAC_CFG_SPEED_10_100;
		break;
	case IFM_1000_T:
		reg |= MAC_CFG_SPEED_1000;
		break;
	}
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		reg |= MAC_CFG_FULL_DUPLEX;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			reg |= MAC_CFG_TX_FC;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			reg |= MAC_CFG_RX_FC;
	}

	CSR_WRITE_4(sc, AGE_MAC_CFG, reg);
}

static bool
age_resume(device_t dv, const pmf_qual_t *qual)
{
	struct age_softc *sc = device_private(dv);
	uint16_t cmd;

	/*
	 * Clear INTx emulation disable for hardware that
	 * is set in resume event. From Linux.
	 */
	cmd = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	if ((cmd & PCI_COMMAND_INTERRUPT_DISABLE) != 0) {
		cmd &= ~PCI_COMMAND_INTERRUPT_DISABLE;
		pci_conf_write(sc->sc_pct, sc->sc_pcitag,
		    PCI_COMMAND_STATUS_REG, cmd);
	}

	return true;
}

static int
age_encap(struct age_softc *sc, struct mbuf **m_head)
{
	struct age_txdesc *txd, *txd_last;
	struct tx_desc *desc;
	struct mbuf *m;
	bus_dmamap_t map;
	uint32_t cflags, poff, vtag;
	int error, i, nsegs, prod;
#if NVLAN > 0
	struct m_tag *mtag;
#endif

	m = *m_head;
	cflags = vtag = 0;
	poff = 0;

	prod = sc->age_cdata.age_tx_prod;
	txd = &sc->age_cdata.age_txdesc[prod];
	txd_last = txd;
	map = txd->tx_dmamap;

	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, *m_head, BUS_DMA_NOWAIT);

	if (error == EFBIG) {
		error = 0;

		*m_head = m_pullup(*m_head, MHLEN);
		if (*m_head == NULL) {
			printf("%s: can't defrag TX mbuf\n",
			    device_xname(sc->sc_dev));
			return ENOBUFS;
		}

		error = bus_dmamap_load_mbuf(sc->sc_dmat, map, *m_head,
		  	    BUS_DMA_NOWAIT);

		if (error != 0) {
			printf("%s: could not load defragged TX mbuf\n",
			    device_xname(sc->sc_dev));
			m_freem(*m_head);
			*m_head = NULL;
			return error;
		}
	} else if (error) {
		printf("%s: could not load TX mbuf\n", device_xname(sc->sc_dev));
		return error;
	}

	nsegs = map->dm_nsegs;

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return EIO;
	}

	/* Check descriptor overrun. */
	if (sc->age_cdata.age_tx_cnt + nsegs >= AGE_TX_RING_CNT - 2) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return ENOBUFS;
	}
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	m = *m_head;
	/* Configure Tx IP/TCP/UDP checksum offload. */
	if ((m->m_pkthdr.csum_flags & AGE_CSUM_FEATURES) != 0) {
		cflags |= AGE_TD_CSUM;
		if ((m->m_pkthdr.csum_flags & M_CSUM_TCPv4) != 0)
			cflags |= AGE_TD_TCPCSUM;
		if ((m->m_pkthdr.csum_flags & M_CSUM_UDPv4) != 0)
			cflags |= AGE_TD_UDPCSUM;
		/* Set checksum start offset. */
		cflags |= (poff << AGE_TD_CSUM_PLOADOFFSET_SHIFT);
	}

#if NVLAN > 0
	/* Configure VLAN hardware tag insertion. */
	if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ec, m))) {
		vtag = AGE_TX_VLAN_TAG(htons(VLAN_TAG_VALUE(mtag)));
		vtag = ((vtag << AGE_TD_VLAN_SHIFT) & AGE_TD_VLAN_MASK);
		cflags |= AGE_TD_INSERT_VLAN_TAG;
	}
#endif

	desc = NULL;
	KASSERT(nsegs > 0);
	for (i = 0; ; i++) {
		desc = &sc->age_rdata.age_tx_ring[prod];
		desc->addr = htole64(map->dm_segs[i].ds_addr);
		desc->len =
		    htole32(AGE_TX_BYTES(map->dm_segs[i].ds_len) | vtag);
		desc->flags = htole32(cflags);
		sc->age_cdata.age_tx_cnt++;
		if (i == (nsegs - 1))
			break;
	
		/* sync this descriptor and go to the next one */
		bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_tx_ring_map,
		    prod * sizeof(struct tx_desc), sizeof(struct tx_desc),
		    BUS_DMASYNC_PREWRITE);
		AGE_DESC_INC(prod, AGE_TX_RING_CNT);
	}

	/* Set EOP on the last descriptor and sync it. */
	desc->flags |= htole32(AGE_TD_EOP);
	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_tx_ring_map,
	    prod * sizeof(struct tx_desc), sizeof(struct tx_desc),
	    BUS_DMASYNC_PREWRITE);

	if (nsegs > 1) {
		/* Swap dmamap of the first and the last. */
		txd = &sc->age_cdata.age_txdesc[prod];
		map = txd_last->tx_dmamap;
		txd_last->tx_dmamap = txd->tx_dmamap;
		txd->tx_dmamap = map;
		txd->tx_m = m;
		KASSERT(txd_last->tx_m == NULL);
	} else {
		KASSERT(txd_last == &sc->age_cdata.age_txdesc[prod]);
		txd_last->tx_m = m;
	}

	/* Update producer index. */
	AGE_DESC_INC(prod, AGE_TX_RING_CNT);
	sc->age_cdata.age_tx_prod = prod;

	return 0;
}

static void
age_txintr(struct age_softc *sc, int tpd_cons)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct age_txdesc *txd;
	int cons, prog;


	if (sc->age_cdata.age_tx_cnt <= 0) {
		if (ifp->if_timer != 0)
			printf("timer running without packets\n");
		if (sc->age_cdata.age_tx_cnt)
			printf("age_tx_cnt corrupted\n");
	}

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	cons = sc->age_cdata.age_tx_cons;
	for (prog = 0; cons != tpd_cons; AGE_DESC_INC(cons, AGE_TX_RING_CNT)) {
		if (sc->age_cdata.age_tx_cnt <= 0)
			break;
		prog++;
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->age_cdata.age_tx_cnt--;
		txd = &sc->age_cdata.age_txdesc[cons];
		/*
		 * Clear Tx descriptors, it's not required but would
		 * help debugging in case of Tx issues.
		 */
		bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_tx_ring_map,
		    cons * sizeof(struct tx_desc), sizeof(struct tx_desc),
		    BUS_DMASYNC_POSTWRITE);
		txd->tx_desc->addr = 0;
		txd->tx_desc->len = 0;
		txd->tx_desc->flags = 0;

		if (txd->tx_m == NULL)
			continue;
		/* Reclaim transmitted mbufs. */
		bus_dmamap_unload(sc->sc_dmat, txd->tx_dmamap);
		m_freem(txd->tx_m);
		txd->tx_m = NULL;
	}

	if (prog > 0) {
		sc->age_cdata.age_tx_cons = cons;

		/*
		 * Unarm watchdog timer only when there are no pending
		 * Tx descriptors in queue.
		 */
		if (sc->age_cdata.age_tx_cnt == 0)
			ifp->if_timer = 0;
	}
}

/* Receive a frame. */
static void
age_rxeof(struct age_softc *sc, struct rx_rdesc *rxrd)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct age_rxdesc *rxd;
	struct rx_desc *desc;
	struct mbuf *mp, *m;
	uint32_t status, index;
	int count, nsegs, pktlen;
	int rx_cons;

	status = le32toh(rxrd->flags);
	index = le32toh(rxrd->index);
	rx_cons = AGE_RX_CONS(index);
	nsegs = AGE_RX_NSEGS(index);

	sc->age_cdata.age_rxlen = AGE_RX_BYTES(le32toh(rxrd->len));
	if ((status & AGE_RRD_ERROR) != 0 &&
	    (status & (AGE_RRD_CRC | AGE_RRD_CODE | AGE_RRD_DRIBBLE |
	    AGE_RRD_RUNT | AGE_RRD_OFLOW | AGE_RRD_TRUNC)) != 0) {
		/*
		 * We want to pass the following frames to upper
		 * layer regardless of error status of Rx return
		 * ring.
		 *
		 *  o IP/TCP/UDP checksum is bad.
		 *  o frame length and protocol specific length
		 *     does not match.
		 */
		sc->age_cdata.age_rx_cons += nsegs;
		sc->age_cdata.age_rx_cons %= AGE_RX_RING_CNT;
		return;
	}

	pktlen = 0;
	for (count = 0; count < nsegs; count++,
	    AGE_DESC_INC(rx_cons, AGE_RX_RING_CNT)) {
		rxd = &sc->age_cdata.age_rxdesc[rx_cons];
		mp = rxd->rx_m;
		desc = rxd->rx_desc;
		/* Add a new receive buffer to the ring. */
		if (age_newbuf(sc, rxd, 0) != 0) {
			ifp->if_iqdrops++;
			/* Reuse Rx buffers. */
			if (sc->age_cdata.age_rxhead != NULL) {
				m_freem(sc->age_cdata.age_rxhead);
				AGE_RXCHAIN_RESET(sc);
			}
			break;
		}

		/* The length of the first mbuf is computed last. */
		if (count != 0) {
			mp->m_len = AGE_RX_BYTES(le32toh(desc->len));
			pktlen += mp->m_len;
		}

		/* Chain received mbufs. */
		if (sc->age_cdata.age_rxhead == NULL) {
			sc->age_cdata.age_rxhead = mp;
			sc->age_cdata.age_rxtail = mp;
		} else {
			mp->m_flags &= ~M_PKTHDR;
			sc->age_cdata.age_rxprev_tail =
			    sc->age_cdata.age_rxtail;
			sc->age_cdata.age_rxtail->m_next = mp;
			sc->age_cdata.age_rxtail = mp;
		}

		if (count == nsegs - 1) {
			/*
			 * It seems that L1 controller has no way
			 * to tell hardware to strip CRC bytes.
			 */
			sc->age_cdata.age_rxlen -= ETHER_CRC_LEN;
			if (nsegs > 1) {
				/* Remove the CRC bytes in chained mbufs. */
				pktlen -= ETHER_CRC_LEN;
				if (mp->m_len <= ETHER_CRC_LEN) {
					sc->age_cdata.age_rxtail =
					    sc->age_cdata.age_rxprev_tail;
					sc->age_cdata.age_rxtail->m_len -=
					    (ETHER_CRC_LEN - mp->m_len);
					sc->age_cdata.age_rxtail->m_next = NULL;
					m_freem(mp);
				} else {
					mp->m_len -= ETHER_CRC_LEN;
				}
			}

			m = sc->age_cdata.age_rxhead;
			m->m_flags |= M_PKTHDR;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = sc->age_cdata.age_rxlen;
			/* Set the first mbuf length. */
			m->m_len = sc->age_cdata.age_rxlen - pktlen;

			/*
			 * Set checksum information.
			 * It seems that L1 controller can compute partial
			 * checksum. The partial checksum value can be used
			 * to accelerate checksum computation for fragmented
			 * TCP/UDP packets. Upper network stack already
			 * takes advantage of the partial checksum value in
			 * IP reassembly stage. But I'm not sure the
			 * correctness of the partial hardware checksum
			 * assistance due to lack of data sheet. If it is
			 * proven to work on L1 I'll enable it.
			 */
			if (status & AGE_RRD_IPV4) {
				if (status & AGE_RRD_IPCSUM_NOK)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4_BAD;
				if ((status & (AGE_RRD_TCP | AGE_RRD_UDP)) &&
				    (status & AGE_RRD_TCP_UDPCSUM_NOK)) {
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
				}
				/*
				 * Don't mark bad checksum for TCP/UDP frames
				 * as fragmented frames may always have set
				 * bad checksummed bit of descriptor status.
				 */
			}
#if NVLAN > 0
			/* Check for VLAN tagged frames. */
			if (status & AGE_RRD_VLAN) {
				uint32_t vtag = AGE_RX_VLAN(le32toh(rxrd->vtags));
				VLAN_INPUT_TAG(ifp, m, AGE_RX_VLAN_TAG(vtag),
					continue);
			}
#endif

			bpf_mtap(ifp, m);
			/* Pass it on. */
			(*ifp->if_input)(ifp, m);

			/* Reset mbuf chains. */
			AGE_RXCHAIN_RESET(sc);
		}
	}

	if (count != nsegs) {
		sc->age_cdata.age_rx_cons += nsegs;
		sc->age_cdata.age_rx_cons %= AGE_RX_RING_CNT;
	} else
		sc->age_cdata.age_rx_cons = rx_cons;
}

static void
age_rxintr(struct age_softc *sc, int rr_prod)
{
	struct rx_rdesc *rxrd;
	int rr_cons, nsegs, pktlen, prog;

	rr_cons = sc->age_cdata.age_rr_cons;
	if (rr_cons == rr_prod)
		return;

	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_rr_ring_map, 0,
	    sc->age_cdata.age_rr_ring_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	for (prog = 0; rr_cons != rr_prod; prog++) {
		rxrd = &sc->age_rdata.age_rr_ring[rr_cons];
		nsegs = AGE_RX_NSEGS(le32toh(rxrd->index));
		if (nsegs == 0)
			break;
		/*
		 * Check number of segments against received bytes
		 * Non-matching value would indicate that hardware
		 * is still trying to update Rx return descriptors.
		 * I'm not sure whether this check is really needed.
		 */
		pktlen = AGE_RX_BYTES(le32toh(rxrd->len));
		if (nsegs != ((pktlen + (MCLBYTES - ETHER_ALIGN - 1)) /
		    (MCLBYTES - ETHER_ALIGN)))
			break;

		/* Received a frame. */
		age_rxeof(sc, rxrd);

		/* Clear return ring. */
		rxrd->index = 0;
		AGE_DESC_INC(rr_cons, AGE_RR_RING_CNT);
	}

	if (prog > 0) {
		/* Update the consumer index. */
		sc->age_cdata.age_rr_cons = rr_cons;

		/* Sync descriptors. */
		bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_rr_ring_map, 0,
		    sc->age_cdata.age_rr_ring_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Notify hardware availability of new Rx buffers. */
		AGE_COMMIT_MBOX(sc);
	}
}

static void
age_tick(void *xsc)
{
	struct age_softc *sc = xsc;
	struct mii_data *mii = &sc->sc_miibus;
	int s;

	s = splnet();
	mii_tick(mii);
	splx(s);

	callout_schedule(&sc->sc_tick_ch, hz);
}

static void
age_reset(struct age_softc *sc)
{
	uint32_t reg;
	int i;

	CSR_WRITE_4(sc, AGE_MASTER_CFG, MASTER_RESET);
	CSR_READ_4(sc, AGE_MASTER_CFG);
	DELAY(1000);
	for (i = AGE_RESET_TIMEOUT; i > 0; i--) {
		if ((reg = CSR_READ_4(sc, AGE_IDLE_STATUS)) == 0)
			break;
		DELAY(10);
	}

	if (i == 0)
		printf("%s: reset timeout(0x%08x)!\n", device_xname(sc->sc_dev),
		    reg);

	/* Initialize PCIe module. From Linux. */
	CSR_WRITE_4(sc, 0x12FC, 0x6500);
	CSR_WRITE_4(sc, 0x1008, CSR_READ_4(sc, 0x1008) | 0x8000);
}

static int
age_init(struct ifnet *ifp)
{
	struct age_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	uint8_t eaddr[ETHER_ADDR_LEN];
	bus_addr_t paddr;
	uint32_t reg, fsize;
	uint32_t rxf_hi, rxf_lo, rrd_hi, rrd_lo;
	int error;

	/*
	 * Cancel any pending I/O.
	 */
	age_stop(ifp, 0);

	/*
	 * Reset the chip to a known state.
	 */
	age_reset(sc);

	/* Initialize descriptors. */
	error = age_init_rx_ring(sc);
        if (error != 0) {
		printf("%s: no memory for Rx buffers.\n", device_xname(sc->sc_dev));
		age_stop(ifp, 0);
		return error;
        }
	age_init_rr_ring(sc);
	age_init_tx_ring(sc);
	age_init_cmb_block(sc);
	age_init_smb_block(sc);

	/* Reprogram the station address. */
	memcpy(eaddr, CLLADDR(ifp->if_sadl), sizeof(eaddr));
	CSR_WRITE_4(sc, AGE_PAR0,
	    eaddr[2] << 24 | eaddr[3] << 16 | eaddr[4] << 8 | eaddr[5]);
	CSR_WRITE_4(sc, AGE_PAR1, eaddr[0] << 8 | eaddr[1]);

	/* Set descriptor base addresses. */
	paddr = sc->age_rdata.age_tx_ring_paddr;
	CSR_WRITE_4(sc, AGE_DESC_ADDR_HI, AGE_ADDR_HI(paddr));
	paddr = sc->age_rdata.age_rx_ring_paddr;
	CSR_WRITE_4(sc, AGE_DESC_RD_ADDR_LO, AGE_ADDR_LO(paddr));
	paddr = sc->age_rdata.age_rr_ring_paddr;
	CSR_WRITE_4(sc, AGE_DESC_RRD_ADDR_LO, AGE_ADDR_LO(paddr));
	paddr = sc->age_rdata.age_tx_ring_paddr;
	CSR_WRITE_4(sc, AGE_DESC_TPD_ADDR_LO, AGE_ADDR_LO(paddr));
	paddr = sc->age_rdata.age_cmb_block_paddr;
	CSR_WRITE_4(sc, AGE_DESC_CMB_ADDR_LO, AGE_ADDR_LO(paddr));
	paddr = sc->age_rdata.age_smb_block_paddr;
	CSR_WRITE_4(sc, AGE_DESC_SMB_ADDR_LO, AGE_ADDR_LO(paddr));

	/* Set Rx/Rx return descriptor counter. */
	CSR_WRITE_4(sc, AGE_DESC_RRD_RD_CNT,
	    ((AGE_RR_RING_CNT << DESC_RRD_CNT_SHIFT) &
	    DESC_RRD_CNT_MASK) |
	    ((AGE_RX_RING_CNT << DESC_RD_CNT_SHIFT) & DESC_RD_CNT_MASK));

	/* Set Tx descriptor counter. */
	CSR_WRITE_4(sc, AGE_DESC_TPD_CNT,
	    (AGE_TX_RING_CNT << DESC_TPD_CNT_SHIFT) & DESC_TPD_CNT_MASK);

	/* Tell hardware that we're ready to load descriptors. */
	CSR_WRITE_4(sc, AGE_DMA_BLOCK, DMA_BLOCK_LOAD);

        /*
	 * Initialize mailbox register.
	 * Updated producer/consumer index information is exchanged
	 * through this mailbox register. However Tx producer and
	 * Rx return consumer/Rx producer are all shared such that
	 * it's hard to separate code path between Tx and Rx without
	 * locking. If L1 hardware have a separate mail box register
	 * for Tx and Rx consumer/producer management we could have
	 * indepent Tx/Rx handler which in turn Rx handler could have
	 * been run without any locking.
	*/
	AGE_COMMIT_MBOX(sc);

	/* Configure IPG/IFG parameters. */
	CSR_WRITE_4(sc, AGE_IPG_IFG_CFG,
	    ((IPG_IFG_IPG2_DEFAULT << IPG_IFG_IPG2_SHIFT) & IPG_IFG_IPG2_MASK) |
	    ((IPG_IFG_IPG1_DEFAULT << IPG_IFG_IPG1_SHIFT) & IPG_IFG_IPG1_MASK) |
	    ((IPG_IFG_MIFG_DEFAULT << IPG_IFG_MIFG_SHIFT) & IPG_IFG_MIFG_MASK) |
	    ((IPG_IFG_IPGT_DEFAULT << IPG_IFG_IPGT_SHIFT) & IPG_IFG_IPGT_MASK));

	/* Set parameters for half-duplex media. */
	CSR_WRITE_4(sc, AGE_HDPX_CFG,
	    ((HDPX_CFG_LCOL_DEFAULT << HDPX_CFG_LCOL_SHIFT) &
	    HDPX_CFG_LCOL_MASK) |
	    ((HDPX_CFG_RETRY_DEFAULT << HDPX_CFG_RETRY_SHIFT) &
	    HDPX_CFG_RETRY_MASK) | HDPX_CFG_EXC_DEF_EN |
	    ((HDPX_CFG_ABEBT_DEFAULT << HDPX_CFG_ABEBT_SHIFT) &
	    HDPX_CFG_ABEBT_MASK) |
	    ((HDPX_CFG_JAMIPG_DEFAULT << HDPX_CFG_JAMIPG_SHIFT) &
	     HDPX_CFG_JAMIPG_MASK));

	/* Configure interrupt moderation timer. */
	sc->age_int_mod = AGE_IM_TIMER_DEFAULT;
	CSR_WRITE_2(sc, AGE_IM_TIMER, AGE_USECS(sc->age_int_mod));
	reg = CSR_READ_4(sc, AGE_MASTER_CFG);
	reg &= ~MASTER_MTIMER_ENB;
	if (AGE_USECS(sc->age_int_mod) == 0)
		reg &= ~MASTER_ITIMER_ENB;
	else
		reg |= MASTER_ITIMER_ENB;
	CSR_WRITE_4(sc, AGE_MASTER_CFG, reg);
	if (agedebug)
		printf("%s: interrupt moderation is %d us.\n",
		    device_xname(sc->sc_dev), sc->age_int_mod);
	CSR_WRITE_2(sc, AGE_INTR_CLR_TIMER, AGE_USECS(1000));

	/* Set Maximum frame size but don't let MTU be lass than ETHER_MTU. */
	if (ifp->if_mtu < ETHERMTU)
		sc->age_max_frame_size = ETHERMTU;
	else
		sc->age_max_frame_size = ifp->if_mtu;
	sc->age_max_frame_size += ETHER_HDR_LEN +
	    sizeof(struct ether_vlan_header) + ETHER_CRC_LEN;
	CSR_WRITE_4(sc, AGE_FRAME_SIZE, sc->age_max_frame_size);

	/* Configure jumbo frame. */
	fsize = roundup(sc->age_max_frame_size, sizeof(uint64_t));
	CSR_WRITE_4(sc, AGE_RXQ_JUMBO_CFG,
	    (((fsize / sizeof(uint64_t)) <<
	    RXQ_JUMBO_CFG_SZ_THRESH_SHIFT) & RXQ_JUMBO_CFG_SZ_THRESH_MASK) |
	    ((RXQ_JUMBO_CFG_LKAH_DEFAULT <<
	    RXQ_JUMBO_CFG_LKAH_SHIFT) & RXQ_JUMBO_CFG_LKAH_MASK) |
	    ((AGE_USECS(8) << RXQ_JUMBO_CFG_RRD_TIMER_SHIFT) &
	    RXQ_JUMBO_CFG_RRD_TIMER_MASK));

	/* Configure flow-control parameters. From Linux. */
	if ((sc->age_flags & AGE_FLAG_PCIE) != 0) {
		/*
		 * Magic workaround for old-L1.
		 * Don't know which hw revision requires this magic.
		 */
		CSR_WRITE_4(sc, 0x12FC, 0x6500);
		/*
		 * Another magic workaround for flow-control mode
		 * change. From Linux.
		 */
		CSR_WRITE_4(sc, 0x1008, CSR_READ_4(sc, 0x1008) | 0x8000);
	}
	/*
	 * TODO
	 *  Should understand pause parameter relationships between FIFO
	 *  size and number of Rx descriptors and Rx return descriptors.
	 *
	 *  Magic parameters came from Linux.
	 */
	switch (sc->age_chip_rev) {
	case 0x8001:
	case 0x9001:
	case 0x9002:
	case 0x9003:
		rxf_hi = AGE_RX_RING_CNT / 16;
		rxf_lo = (AGE_RX_RING_CNT * 7) / 8;
		rrd_hi = (AGE_RR_RING_CNT * 7) / 8;
		rrd_lo = AGE_RR_RING_CNT / 16;
		break;
	default:
		reg = CSR_READ_4(sc, AGE_SRAM_RX_FIFO_LEN);
		rxf_lo = reg / 16;
		if (rxf_lo < 192)
			rxf_lo = 192;
		rxf_hi = (reg * 7) / 8;
		if (rxf_hi < rxf_lo)
			rxf_hi = rxf_lo + 16;
		reg = CSR_READ_4(sc, AGE_SRAM_RRD_LEN);
		rrd_lo = reg / 8;
		rrd_hi = (reg * 7) / 8;
		if (rrd_lo < 2)
			rrd_lo = 2;
		if (rrd_hi < rrd_lo)
			rrd_hi = rrd_lo + 3;
		break;
	}
	CSR_WRITE_4(sc, AGE_RXQ_FIFO_PAUSE_THRESH,
	    ((rxf_lo << RXQ_FIFO_PAUSE_THRESH_LO_SHIFT) &
	    RXQ_FIFO_PAUSE_THRESH_LO_MASK) |
	    ((rxf_hi << RXQ_FIFO_PAUSE_THRESH_HI_SHIFT) &
	    RXQ_FIFO_PAUSE_THRESH_HI_MASK));
	CSR_WRITE_4(sc, AGE_RXQ_RRD_PAUSE_THRESH,
	    ((rrd_lo << RXQ_RRD_PAUSE_THRESH_LO_SHIFT) &
	    RXQ_RRD_PAUSE_THRESH_LO_MASK) |
	    ((rrd_hi << RXQ_RRD_PAUSE_THRESH_HI_SHIFT) &
	    RXQ_RRD_PAUSE_THRESH_HI_MASK));

	/* Configure RxQ. */
	CSR_WRITE_4(sc, AGE_RXQ_CFG,
	    ((RXQ_CFG_RD_BURST_DEFAULT << RXQ_CFG_RD_BURST_SHIFT) &
	    RXQ_CFG_RD_BURST_MASK) |
	    ((RXQ_CFG_RRD_BURST_THRESH_DEFAULT <<
	    RXQ_CFG_RRD_BURST_THRESH_SHIFT) & RXQ_CFG_RRD_BURST_THRESH_MASK) |
	    ((RXQ_CFG_RD_PREF_MIN_IPG_DEFAULT <<
	    RXQ_CFG_RD_PREF_MIN_IPG_SHIFT) & RXQ_CFG_RD_PREF_MIN_IPG_MASK) |
	    RXQ_CFG_CUT_THROUGH_ENB | RXQ_CFG_ENB);

	/* Configure TxQ. */
	CSR_WRITE_4(sc, AGE_TXQ_CFG,
	    ((TXQ_CFG_TPD_BURST_DEFAULT << TXQ_CFG_TPD_BURST_SHIFT) &
	    TXQ_CFG_TPD_BURST_MASK) |
	    ((TXQ_CFG_TX_FIFO_BURST_DEFAULT << TXQ_CFG_TX_FIFO_BURST_SHIFT) &
	    TXQ_CFG_TX_FIFO_BURST_MASK) |
	    ((TXQ_CFG_TPD_FETCH_DEFAULT <<
	    TXQ_CFG_TPD_FETCH_THRESH_SHIFT) & TXQ_CFG_TPD_FETCH_THRESH_MASK) |
	    TXQ_CFG_ENB);

	/* Configure DMA parameters. */
	CSR_WRITE_4(sc, AGE_DMA_CFG,
	    DMA_CFG_ENH_ORDER | DMA_CFG_RCB_64 |
	    sc->age_dma_rd_burst | DMA_CFG_RD_ENB |
	    sc->age_dma_wr_burst | DMA_CFG_WR_ENB);

	/* Configure CMB DMA write threshold. */
	CSR_WRITE_4(sc, AGE_CMB_WR_THRESH,
	    ((CMB_WR_THRESH_RRD_DEFAULT << CMB_WR_THRESH_RRD_SHIFT) &
	    CMB_WR_THRESH_RRD_MASK) |
	    ((CMB_WR_THRESH_TPD_DEFAULT << CMB_WR_THRESH_TPD_SHIFT) &
	    CMB_WR_THRESH_TPD_MASK));

	/* Set CMB/SMB timer and enable them. */
	CSR_WRITE_4(sc, AGE_CMB_WR_TIMER,
	    ((AGE_USECS(2) << CMB_WR_TIMER_TX_SHIFT) & CMB_WR_TIMER_TX_MASK) |
	    ((AGE_USECS(2) << CMB_WR_TIMER_RX_SHIFT) & CMB_WR_TIMER_RX_MASK));

	/* Request SMB updates for every seconds. */
	CSR_WRITE_4(sc, AGE_SMB_TIMER, AGE_USECS(1000 * 1000));
	CSR_WRITE_4(sc, AGE_CSMB_CTRL, CSMB_CTRL_SMB_ENB | CSMB_CTRL_CMB_ENB);

	/*
	 * Disable all WOL bits as WOL can interfere normal Rx
	 * operation.
	 */
	CSR_WRITE_4(sc, AGE_WOL_CFG, 0);

        /*
	 * Configure Tx/Rx MACs.
	 *  - Auto-padding for short frames.
	 *  - Enable CRC generation.
	 *  Start with full-duplex/1000Mbps media. Actual reconfiguration
	 *  of MAC is followed after link establishment.
	 */
	CSR_WRITE_4(sc, AGE_MAC_CFG,
	    MAC_CFG_TX_CRC_ENB | MAC_CFG_TX_AUTO_PAD |
	    MAC_CFG_FULL_DUPLEX | MAC_CFG_SPEED_1000 |
	    ((MAC_CFG_PREAMBLE_DEFAULT << MAC_CFG_PREAMBLE_SHIFT) &
	    MAC_CFG_PREAMBLE_MASK));

	/* Set up the receive filter. */
	age_rxfilter(sc);
	age_rxvlan(sc);

	reg = CSR_READ_4(sc, AGE_MAC_CFG);
	reg |= MAC_CFG_RXCSUM_ENB;

	/* Ack all pending interrupts and clear it. */
	CSR_WRITE_4(sc, AGE_INTR_STATUS, 0);
	CSR_WRITE_4(sc, AGE_INTR_MASK, AGE_INTRS);

	/* Finally enable Tx/Rx MAC. */
	CSR_WRITE_4(sc, AGE_MAC_CFG, reg | MAC_CFG_TX_ENB | MAC_CFG_RX_ENB);

	sc->age_flags &= ~AGE_FLAG_LINK;

	/* Switch to the current media. */
	mii = &sc->sc_miibus;
	mii_mediachg(mii);

	callout_schedule(&sc->sc_tick_ch, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}

static void
age_stop(struct ifnet *ifp, int disable)
{
	struct age_softc *sc = ifp->if_softc;
	struct age_txdesc *txd;
	struct age_rxdesc *rxd;
	uint32_t reg;
	int i;

	callout_stop(&sc->sc_tick_ch);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	sc->age_flags &= ~AGE_FLAG_LINK;

	mii_down(&sc->sc_miibus);

	/*
	 * Disable interrupts.
	 */
	CSR_WRITE_4(sc, AGE_INTR_MASK, 0);
	CSR_WRITE_4(sc, AGE_INTR_STATUS, 0xFFFFFFFF);

	/* Stop CMB/SMB updates. */
	CSR_WRITE_4(sc, AGE_CSMB_CTRL, 0);

	/* Stop Rx/Tx MAC. */
	age_stop_rxmac(sc);
	age_stop_txmac(sc);

	/* Stop DMA. */
	CSR_WRITE_4(sc, AGE_DMA_CFG,
	    CSR_READ_4(sc, AGE_DMA_CFG) & ~(DMA_CFG_RD_ENB | DMA_CFG_WR_ENB));

	/* Stop TxQ/RxQ. */
	CSR_WRITE_4(sc, AGE_TXQ_CFG,
	    CSR_READ_4(sc, AGE_TXQ_CFG) & ~TXQ_CFG_ENB);
	CSR_WRITE_4(sc, AGE_RXQ_CFG,
	    CSR_READ_4(sc, AGE_RXQ_CFG) & ~RXQ_CFG_ENB);
	for (i = AGE_RESET_TIMEOUT; i > 0; i--) {
		if ((reg = CSR_READ_4(sc, AGE_IDLE_STATUS)) == 0)
			break;
		DELAY(10);
	}
	if (i == 0)
		printf("%s: stopping Rx/Tx MACs timed out(0x%08x)!\n",
		    device_xname(sc->sc_dev), reg);

	/* Reclaim Rx buffers that have been processed. */
	if (sc->age_cdata.age_rxhead != NULL)
		m_freem(sc->age_cdata.age_rxhead);
	AGE_RXCHAIN_RESET(sc);

	/*
	 * Free RX and TX mbufs still in the queues.
	 */
	for (i = 0; i < AGE_RX_RING_CNT; i++) {
		rxd = &sc->age_cdata.age_rxdesc[i];
		if (rxd->rx_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
			m_freem(rxd->rx_m);
			rxd->rx_m = NULL;
		}
	}
	for (i = 0; i < AGE_TX_RING_CNT; i++) {
		txd = &sc->age_cdata.age_txdesc[i];
		if (txd->tx_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txd->tx_dmamap);
			m_freem(txd->tx_m);
			txd->tx_m = NULL;
		}
	}
}

static void
age_stats_update(struct age_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct age_stats *stat;
	struct smb *smb;

	stat = &sc->age_stat;

	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_smb_block_map, 0,
	    sc->age_cdata.age_smb_block_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	smb = sc->age_rdata.age_smb_block;
	if (smb->updated == 0)
		return;

	/* Rx stats. */
	stat->rx_frames += smb->rx_frames;
	stat->rx_bcast_frames += smb->rx_bcast_frames;
	stat->rx_mcast_frames += smb->rx_mcast_frames;
	stat->rx_pause_frames += smb->rx_pause_frames;
	stat->rx_control_frames += smb->rx_control_frames;
	stat->rx_crcerrs += smb->rx_crcerrs;
	stat->rx_lenerrs += smb->rx_lenerrs;
	stat->rx_bytes += smb->rx_bytes;
	stat->rx_runts += smb->rx_runts;
	stat->rx_fragments += smb->rx_fragments;
	stat->rx_pkts_64 += smb->rx_pkts_64;
	stat->rx_pkts_65_127 += smb->rx_pkts_65_127;
	stat->rx_pkts_128_255 += smb->rx_pkts_128_255;
	stat->rx_pkts_256_511 += smb->rx_pkts_256_511;
	stat->rx_pkts_512_1023 += smb->rx_pkts_512_1023;
	stat->rx_pkts_1024_1518 += smb->rx_pkts_1024_1518;
	stat->rx_pkts_1519_max += smb->rx_pkts_1519_max;
	stat->rx_pkts_truncated += smb->rx_pkts_truncated;
	stat->rx_fifo_oflows += smb->rx_fifo_oflows;
	stat->rx_desc_oflows += smb->rx_desc_oflows;
	stat->rx_alignerrs += smb->rx_alignerrs;
	stat->rx_bcast_bytes += smb->rx_bcast_bytes;
	stat->rx_mcast_bytes += smb->rx_mcast_bytes;
	stat->rx_pkts_filtered += smb->rx_pkts_filtered;

	/* Tx stats. */
	stat->tx_frames += smb->tx_frames;
	stat->tx_bcast_frames += smb->tx_bcast_frames;
	stat->tx_mcast_frames += smb->tx_mcast_frames;
	stat->tx_pause_frames += smb->tx_pause_frames;
	stat->tx_excess_defer += smb->tx_excess_defer;
	stat->tx_control_frames += smb->tx_control_frames;
	stat->tx_deferred += smb->tx_deferred;
	stat->tx_bytes += smb->tx_bytes;
	stat->tx_pkts_64 += smb->tx_pkts_64;
	stat->tx_pkts_65_127 += smb->tx_pkts_65_127;
	stat->tx_pkts_128_255 += smb->tx_pkts_128_255;
	stat->tx_pkts_256_511 += smb->tx_pkts_256_511;
	stat->tx_pkts_512_1023 += smb->tx_pkts_512_1023;
	stat->tx_pkts_1024_1518 += smb->tx_pkts_1024_1518;
	stat->tx_pkts_1519_max += smb->tx_pkts_1519_max;
	stat->tx_single_colls += smb->tx_single_colls;
	stat->tx_multi_colls += smb->tx_multi_colls;
	stat->tx_late_colls += smb->tx_late_colls;
	stat->tx_excess_colls += smb->tx_excess_colls;
	stat->tx_underrun += smb->tx_underrun;
	stat->tx_desc_underrun += smb->tx_desc_underrun;
	stat->tx_lenerrs += smb->tx_lenerrs;
	stat->tx_pkts_truncated += smb->tx_pkts_truncated;
	stat->tx_bcast_bytes += smb->tx_bcast_bytes;
	stat->tx_mcast_bytes += smb->tx_mcast_bytes;

	/* Update counters in ifnet. */
	ifp->if_opackets += smb->tx_frames;

	ifp->if_collisions += smb->tx_single_colls +
	    smb->tx_multi_colls + smb->tx_late_colls +
	    smb->tx_excess_colls * HDPX_CFG_RETRY_DEFAULT;

	ifp->if_oerrors += smb->tx_excess_colls +
	    smb->tx_late_colls + smb->tx_underrun +
	    smb->tx_pkts_truncated;

	ifp->if_ipackets += smb->rx_frames;

	ifp->if_ierrors += smb->rx_crcerrs + smb->rx_lenerrs +
	    smb->rx_runts + smb->rx_pkts_truncated +
	    smb->rx_fifo_oflows + smb->rx_desc_oflows +
	    smb->rx_alignerrs;

	/* Update done, clear. */
	smb->updated = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_smb_block_map, 0,
	    sc->age_cdata.age_smb_block_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

static void
age_stop_txmac(struct age_softc *sc)
{
	uint32_t reg;
	int i;

	reg = CSR_READ_4(sc, AGE_MAC_CFG);
	if ((reg & MAC_CFG_TX_ENB) != 0) {
		reg &= ~MAC_CFG_TX_ENB;
		CSR_WRITE_4(sc, AGE_MAC_CFG, reg);
	}
	/* Stop Tx DMA engine. */
	reg = CSR_READ_4(sc, AGE_DMA_CFG);
	if ((reg & DMA_CFG_RD_ENB) != 0) {
		reg &= ~DMA_CFG_RD_ENB;
		CSR_WRITE_4(sc, AGE_DMA_CFG, reg);
	}
	for (i = AGE_RESET_TIMEOUT; i > 0; i--) {
		if ((CSR_READ_4(sc, AGE_IDLE_STATUS) &
		    (IDLE_STATUS_TXMAC | IDLE_STATUS_DMARD)) == 0)
			break;
		DELAY(10);
	}
	if (i == 0)
		printf("%s: stopping TxMAC timeout!\n", device_xname(sc->sc_dev));
}

static void
age_stop_rxmac(struct age_softc *sc)
{
	uint32_t reg;
	int i;

	reg = CSR_READ_4(sc, AGE_MAC_CFG);
	if ((reg & MAC_CFG_RX_ENB) != 0) {
		reg &= ~MAC_CFG_RX_ENB;
		CSR_WRITE_4(sc, AGE_MAC_CFG, reg);
	}
	/* Stop Rx DMA engine. */
	reg = CSR_READ_4(sc, AGE_DMA_CFG);
	if ((reg & DMA_CFG_WR_ENB) != 0) {
		reg &= ~DMA_CFG_WR_ENB;
		CSR_WRITE_4(sc, AGE_DMA_CFG, reg);
	}
	for (i = AGE_RESET_TIMEOUT; i > 0; i--) {
		if ((CSR_READ_4(sc, AGE_IDLE_STATUS) &
		    (IDLE_STATUS_RXMAC | IDLE_STATUS_DMAWR)) == 0)
			break;
		DELAY(10);
	}
	if (i == 0)
		printf("%s: stopping RxMAC timeout!\n", device_xname(sc->sc_dev));
}

static void
age_init_tx_ring(struct age_softc *sc)
{
	struct age_ring_data *rd;
	struct age_txdesc *txd;
	int i;

	sc->age_cdata.age_tx_prod = 0;
	sc->age_cdata.age_tx_cons = 0;
	sc->age_cdata.age_tx_cnt = 0;

	rd = &sc->age_rdata;
	memset(rd->age_tx_ring, 0, AGE_TX_RING_SZ);
	for (i = 0; i < AGE_TX_RING_CNT; i++) {
		txd = &sc->age_cdata.age_txdesc[i];
		txd->tx_desc = &rd->age_tx_ring[i];
		txd->tx_m = NULL;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_tx_ring_map, 0,
	    sc->age_cdata.age_tx_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

static int
age_init_rx_ring(struct age_softc *sc)
{
	struct age_ring_data *rd;
	struct age_rxdesc *rxd;
	int i;

	sc->age_cdata.age_rx_cons = AGE_RX_RING_CNT - 1;
	rd = &sc->age_rdata;
	memset(rd->age_rx_ring, 0, AGE_RX_RING_SZ);
	for (i = 0; i < AGE_RX_RING_CNT; i++) {
		rxd = &sc->age_cdata.age_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_desc = &rd->age_rx_ring[i];
		if (age_newbuf(sc, rxd, 1) != 0)
			return ENOBUFS;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_rx_ring_map, 0,
	    sc->age_cdata.age_rx_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	return 0;
}

static void
age_init_rr_ring(struct age_softc *sc)
{
	struct age_ring_data *rd;

	sc->age_cdata.age_rr_cons = 0;
	AGE_RXCHAIN_RESET(sc);

	rd = &sc->age_rdata;
	memset(rd->age_rr_ring, 0, AGE_RR_RING_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_rr_ring_map, 0,
	    sc->age_cdata.age_rr_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

static void
age_init_cmb_block(struct age_softc *sc)
{
	struct age_ring_data *rd;

	rd = &sc->age_rdata;
	memset(rd->age_cmb_block, 0, AGE_CMB_BLOCK_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_cmb_block_map, 0,
	    sc->age_cdata.age_cmb_block_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
}

static void
age_init_smb_block(struct age_softc *sc)
{
	struct age_ring_data *rd;

	rd = &sc->age_rdata;
	memset(rd->age_smb_block, 0, AGE_SMB_BLOCK_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->age_cdata.age_smb_block_map, 0,
	    sc->age_cdata.age_smb_block_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

static int
age_newbuf(struct age_softc *sc, struct age_rxdesc *rxd, int init)
{
	struct rx_desc *desc;
	struct mbuf *m;
	bus_dmamap_t map;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;
	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		 m_freem(m);
		 return ENOBUFS;
	}

	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(sc->sc_dmat,
	    sc->age_cdata.age_rx_sparemap, m, BUS_DMA_NOWAIT);

	if (error != 0) {
		if (!error) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->age_cdata.age_rx_sparemap);
			error = EFBIG;
			printf("%s: too many segments?!\n",
			    device_xname(sc->sc_dev));
		}
		m_freem(m);

		if (init)
			printf("%s: can't load RX mbuf\n", device_xname(sc->sc_dev));
		return error;
	}

	if (rxd->rx_m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, rxd->rx_dmamap, 0,
		    rxd->rx_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = sc->age_cdata.age_rx_sparemap;
	sc->age_cdata.age_rx_sparemap = map;
	rxd->rx_m = m;

	desc = rxd->rx_desc;
	desc->addr = htole64(rxd->rx_dmamap->dm_segs[0].ds_addr);
	desc->len =
	    htole32((rxd->rx_dmamap->dm_segs[0].ds_len & AGE_RD_LEN_MASK) <<
	    AGE_RD_LEN_SHIFT);

	return 0;
}

static void
age_rxvlan(struct age_softc *sc)
{
	uint32_t reg;

	reg = CSR_READ_4(sc, AGE_MAC_CFG);
	reg &= ~MAC_CFG_VLAN_TAG_STRIP;
	if (sc->sc_ec.ec_capenable & ETHERCAP_VLAN_HWTAGGING)
		reg |= MAC_CFG_VLAN_TAG_STRIP;
	CSR_WRITE_4(sc, AGE_MAC_CFG, reg);
}

static void
age_rxfilter(struct age_softc *sc)
{
	struct ethercom *ec = &sc->sc_ec;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc;
	uint32_t mchash[2];
	uint32_t rxcfg;

	rxcfg = CSR_READ_4(sc, AGE_MAC_CFG);
	rxcfg &= ~(MAC_CFG_ALLMULTI | MAC_CFG_BCAST | MAC_CFG_PROMISC);
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Always accept broadcast frames.
	 */
	rxcfg |= MAC_CFG_BCAST;

	if (ifp->if_flags & IFF_PROMISC || ec->ec_multicnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			rxcfg |= MAC_CFG_PROMISC;
		else
			rxcfg |= MAC_CFG_ALLMULTI;
		mchash[0] = mchash[1] = 0xFFFFFFFF;
	} else {
		/* Program new filter. */
		memset(mchash, 0, sizeof(mchash));

		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
			crc = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);
			mchash[crc >> 31] |= 1 << ((crc >> 26) & 0x1f);
			ETHER_NEXT_MULTI(step, enm);
		}
	}

	CSR_WRITE_4(sc, AGE_MAR0, mchash[0]);
	CSR_WRITE_4(sc, AGE_MAR1, mchash[1]);
	CSR_WRITE_4(sc, AGE_MAC_CFG, rxcfg);
}
