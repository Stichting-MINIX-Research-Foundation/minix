/*	$NetBSD: if_jme.c,v 1.28 2015/09/12 19:19:11 christos Exp $	*/

/*
 * Copyright (c) 2008 Manuel Bouyer.  All rights reserved.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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


/*
 * Driver for JMicron Technologies JMC250 (Giganbit) and JMC260 (Fast)
 * Ethernet Controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_jme.c,v 1.28 2015/09/12 19:19:11 christos Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#include <sys/device.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#if defined(SIOCSIFMEDIA)
#include <net/if_media.h>
#endif
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/rndsource.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifdef INET
#include <netinet/in_var.h>
#endif

#include <netinet/tcp.h>

#include <net/if_ether.h>
#if defined(INET)
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/if_jmereg.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

struct jme_product_desc {
	u_int32_t jme_product;
	const char *jme_desc;
};

/* number of entries in transmit and receive rings */
#define JME_NBUFS (PAGE_SIZE / sizeof(struct jme_desc))

#define JME_DESC_INC(x, y)	((x) = ((x) + 1) % (y))

/* Water mark to kick reclaiming Tx buffers. */
#define JME_TX_DESC_HIWAT	(JME_NBUFS - (((JME_NBUFS) * 3) / 10))


struct jme_softc {
	device_t jme_dev;		/* base device */
	bus_space_tag_t jme_bt_mac;
	bus_space_handle_t jme_bh_mac;  /* Mac registers */
	bus_space_tag_t jme_bt_phy;
	bus_space_handle_t jme_bh_phy;  /* PHY registers */
	bus_space_tag_t jme_bt_misc;
	bus_space_handle_t jme_bh_misc; /* Misc registers */
	bus_dma_tag_t jme_dmatag;
	bus_dma_segment_t jme_txseg;	/* transmit ring seg */
	bus_dmamap_t jme_txmap;		/* transmit ring DMA map */
	struct jme_desc* jme_txring;	/* transmit ring */
	bus_dmamap_t jme_txmbufm[JME_NBUFS]; /* transmit mbufs DMA map */
	struct mbuf *jme_txmbuf[JME_NBUFS]; /* mbufs being transmitted */
	int jme_tx_cons;		/* transmit ring consumer */
	int jme_tx_prod;		/* transmit ring producer */
	int jme_tx_cnt;			/* transmit ring active count */
	bus_dma_segment_t jme_rxseg;	/* receive ring seg */
	bus_dmamap_t jme_rxmap;		/* receive ring DMA map */
	struct jme_desc* jme_rxring;	/* receive ring */
	bus_dmamap_t jme_rxmbufm[JME_NBUFS]; /* receive mbufs DMA map */
	struct mbuf *jme_rxmbuf[JME_NBUFS]; /* mbufs being received */
	int jme_rx_cons;		/* receive ring consumer */
	int jme_rx_prod;		/* receive ring producer */
	void* jme_ih;			/* our interrupt */
	struct ethercom jme_ec;
	struct callout jme_tick_ch;	/* tick callout */
	u_int8_t jme_enaddr[ETHER_ADDR_LEN];/* hardware address */
	u_int8_t jme_phyaddr;		/* address of integrated phy */
	u_int8_t jme_chip_rev;		/* chip revision */
	u_int8_t jme_rev;		/* PCI revision */
	mii_data_t jme_mii;		/* mii bus */
	u_int32_t jme_flags;		/* device features, see below */
	uint32_t jme_txcsr;		/* TX config register */
	uint32_t jme_rxcsr;		/* RX config register */
	krndsource_t rnd_source;
	/* interrupt coalition parameters */
	struct sysctllog *jme_clog;
	int jme_intrxto;		/* interrupt RX timeout */
	int jme_intrxct;		/* interrupt RX packets counter */
	int jme_inttxto;		/* interrupt TX timeout */
	int jme_inttxct;		/* interrupt TX packets counter */
};

#define JME_FLAG_FPGA	0x0001 /* FPGA version */
#define JME_FLAG_GIGA	0x0002 /* giga Ethernet capable */


#define jme_if	jme_ec.ec_if
#define jme_bpf	jme_if.if_bpf

typedef struct jme_softc jme_softc_t;
typedef u_long ioctl_cmd_t;

static int jme_pci_match(device_t, cfdata_t, void *);
static void jme_pci_attach(device_t, device_t, void *);
static void jme_intr_rx(jme_softc_t *);
static int jme_intr(void *);

static int jme_ifioctl(struct ifnet *, ioctl_cmd_t, void *);
static int jme_mediachange(struct ifnet *);
static void jme_ifwatchdog(struct ifnet *);
static bool jme_shutdown(device_t, int);

static void jme_txeof(struct jme_softc *);
static void jme_ifstart(struct ifnet *);
static void jme_reset(jme_softc_t *);
static int  jme_ifinit(struct ifnet *);
static int  jme_init(struct ifnet *, int);
static void jme_stop(struct ifnet *, int);
// static void jme_restart(void *);
static void jme_ticks(void *);
static void jme_mac_config(jme_softc_t *);
static void jme_set_filter(jme_softc_t *);

int jme_mii_read(device_t, int, int);
void jme_mii_write(device_t, int, int, int);
void jme_statchg(struct ifnet *);

static int jme_eeprom_read_byte(struct jme_softc *, uint8_t, uint8_t *);
static int jme_eeprom_macaddr(struct jme_softc *);
static int jme_reg_macaddr(struct jme_softc *);

#define JME_TIMEOUT		1000
#define JME_PHY_TIMEOUT		1000
#define JME_EEPROM_TIMEOUT	1000

static int jme_sysctl_intrxto(SYSCTLFN_PROTO);
static int jme_sysctl_intrxct(SYSCTLFN_PROTO);
static int jme_sysctl_inttxto(SYSCTLFN_PROTO);
static int jme_sysctl_inttxct(SYSCTLFN_PROTO);
static int jme_root_num;


CFATTACH_DECL_NEW(jme, sizeof(jme_softc_t),
    jme_pci_match, jme_pci_attach, NULL, NULL);

static const struct jme_product_desc jme_products[] = {
	{ PCI_PRODUCT_JMICRON_JMC250,
	  "JMicron JMC250 Gigabit Ethernet Controller" },
	{ PCI_PRODUCT_JMICRON_JMC260,
	  "JMicron JMC260 Gigabit Ethernet Controller" },
	{ 0, NULL },
};

static const struct jme_product_desc *jme_lookup_product(uint32_t);

static const struct jme_product_desc *
jme_lookup_product(uint32_t id)
{
	const struct jme_product_desc *jp;

	for (jp = jme_products ; jp->jme_desc != NULL; jp++)
		if (PCI_PRODUCT(id) == jp->jme_product)
			return jp;

	return NULL;
}

static int
jme_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_JMICRON)
		return 0;

	if (jme_lookup_product(pa->pa_id) != NULL)
		return 1;

	return 0;
}

static void
jme_pci_attach(device_t parent, device_t self, void *aux)
{
	jme_softc_t *sc = device_private(self);
	struct pci_attach_args * const pa = (struct pci_attach_args *)aux;
	const struct jme_product_desc *jp;
	struct ifnet * const ifp = &sc->jme_if;
	bus_space_tag_t iot1, iot2, memt;
	bus_space_handle_t ioh1, ioh2, memh;
	bus_size_t size, size2;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	pcireg_t csr;
	int nsegs, i;
	const struct sysctlnode *node;
	int jme_nodenum;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->jme_dev = self;
	aprint_normal("\n");
	callout_init(&sc->jme_tick_ch, 0);

	jp = jme_lookup_product(pa->pa_id);
	if (jp == NULL)
		panic("jme_pci_attach: impossible");

	if (jp->jme_product == PCI_PRODUCT_JMICRON_JMC250)
		sc->jme_flags = JME_FLAG_GIGA;

	/*
	 * Map the card space. Try Mem first.
	 */
	if (pci_mapreg_map(pa, JME_PCI_BAR0,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
	    0, &memt, &memh, NULL, &size) == 0) {
		sc->jme_bt_mac = memt;
		sc->jme_bh_mac = memh;
		sc->jme_bt_phy = memt;
		if (bus_space_subregion(memt, memh, JME_PHY_EEPROM_BASE_MEMOFF,
		    JME_PHY_EEPROM_SIZE, &sc->jme_bh_phy) != 0) {
			aprint_error_dev(self, "can't subregion PHY space\n");
			bus_space_unmap(memt, memh, size);
			return;
		}
		sc->jme_bt_misc = memt;
		if (bus_space_subregion(memt, memh, JME_MISC_BASE_MEMOFF,
		    JME_MISC_SIZE, &sc->jme_bh_misc) != 0) {
			aprint_error_dev(self, "can't subregion misc space\n");
			bus_space_unmap(memt, memh, size);
			return;
		}
	} else {
		if (pci_mapreg_map(pa, JME_PCI_BAR1, PCI_MAPREG_TYPE_IO,
		    0, &iot1, &ioh1, NULL, &size) != 0) {
			aprint_error_dev(self, "can't map I/O space 1\n");
			return;
		}
		sc->jme_bt_mac = iot1;
		sc->jme_bh_mac = ioh1;
		if (pci_mapreg_map(pa, JME_PCI_BAR2, PCI_MAPREG_TYPE_IO,
		    0, &iot2, &ioh2, NULL, &size2) != 0) {
			aprint_error_dev(self, "can't map I/O space 2\n");
			bus_space_unmap(iot1, ioh1, size);
			return;
		}
		sc->jme_bt_phy = iot2;
		sc->jme_bh_phy = ioh2;
		sc->jme_bt_misc = iot2;
		if (bus_space_subregion(iot2, ioh2, JME_MISC_BASE_IOOFF,
		    JME_MISC_SIZE, &sc->jme_bh_misc) != 0) {
			aprint_error_dev(self, "can't subregion misc space\n");
			bus_space_unmap(iot1, ioh1, size);
			bus_space_unmap(iot2, ioh2, size2);
			return;
		}
	}

	if (pci_dma64_available(pa))
		sc->jme_dmatag = pa->pa_dmat64;
	else
		sc->jme_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	aprint_normal_dev(self, "%s\n", jp->jme_desc);

	sc->jme_rev = PCI_REVISION(pa->pa_class);

	csr = bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_CHIPMODE);
	if (((csr & CHIPMODE_FPGA_REV_MASK) >> CHIPMODE_FPGA_REV_SHIFT) !=
	    CHIPMODE_NOT_FPGA)
		sc->jme_flags |= JME_FLAG_FPGA;
	sc->jme_chip_rev = (csr & CHIPMODE_REV_MASK) >> CHIPMODE_REV_SHIFT;
	aprint_verbose_dev(self, "PCI device revision : 0x%x, Chip revision: "
	    "0x%x", sc->jme_rev, sc->jme_chip_rev);
	if (sc->jme_flags & JME_FLAG_FPGA)
		aprint_verbose(" FPGA revision: 0x%x",
		    (csr & CHIPMODE_FPGA_REV_MASK) >> CHIPMODE_FPGA_REV_SHIFT);
	aprint_verbose("\n");

	/*
	 * Save PHY address.
	 * Integrated JR0211 has fixed PHY address whereas FPGA version
	 * requires PHY probing to get correct PHY address.
	 */
	if ((sc->jme_flags & JME_FLAG_FPGA) == 0) {
		sc->jme_phyaddr =
		    bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc,
				     JME_GPREG0) & GPREG0_PHY_ADDR_MASK;
	} else
		sc->jme_phyaddr = 0;


	jme_reset(sc);

	/* read mac addr */
	if (jme_eeprom_macaddr(sc) && jme_reg_macaddr(sc)) {
		aprint_error_dev(self, "error reading Ethernet address\n");
		/* return; */
	}
	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->jme_enaddr));

	/* Map and establish interrupts */
	if (pci_intr_map(pa, &intrhandle)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	sc->jme_if.if_softc = sc;
	sc->jme_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
	    jme_intr, sc);
	if (sc->jme_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* allocate and map DMA-safe memory for transmit ring */
	if (bus_dmamem_alloc(sc->jme_dmatag, PAGE_SIZE, 0, PAGE_SIZE,
	    &sc->jme_txseg, 1, &nsegs, BUS_DMA_NOWAIT) != 0 ||
	    bus_dmamem_map(sc->jme_dmatag, &sc->jme_txseg,
	    nsegs, PAGE_SIZE, (void **)&sc->jme_txring,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0 ||
	    bus_dmamap_create(sc->jme_dmatag, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->jme_txmap) != 0 ||
	    bus_dmamap_load(sc->jme_dmatag, sc->jme_txmap, sc->jme_txring,
	    PAGE_SIZE, NULL, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(self, "can't allocate DMA memory TX ring\n");
		return;
	}
	/* allocate and map DMA-safe memory for receive ring */
	if (bus_dmamem_alloc(sc->jme_dmatag, PAGE_SIZE, 0, PAGE_SIZE,
	      &sc->jme_rxseg, 1, &nsegs, BUS_DMA_NOWAIT) != 0 ||
	    bus_dmamem_map(sc->jme_dmatag, &sc->jme_rxseg,
	      nsegs, PAGE_SIZE, (void **)&sc->jme_rxring,
	      BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0 ||
	    bus_dmamap_create(sc->jme_dmatag, PAGE_SIZE, 1, PAGE_SIZE, 0,
	      BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->jme_rxmap) != 0 ||
	    bus_dmamap_load(sc->jme_dmatag, sc->jme_rxmap, sc->jme_rxring,
	      PAGE_SIZE, NULL, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(self, "can't allocate DMA memory RX ring\n");
		return;
	}
	for (i = 0; i < JME_NBUFS; i++) {
		sc->jme_txmbuf[i] = sc->jme_rxmbuf[i] = NULL;
		if (bus_dmamap_create(sc->jme_dmatag, JME_MAX_TX_LEN,
		    JME_NBUFS, JME_MAX_TX_LEN, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->jme_txmbufm[i]) != 0) {
			aprint_error_dev(self, "can't allocate DMA TX map\n");
			return;
		}
		if (bus_dmamap_create(sc->jme_dmatag, JME_MAX_RX_LEN,
		    1, JME_MAX_RX_LEN, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->jme_rxmbufm[i]) != 0) {
			aprint_error_dev(self, "can't allocate DMA RX map\n");
			return;
		}
	}
	/*
	 * Initialize our media structures and probe the MII.
	 *
	 * Note that we don't care about the media instance.  We
	 * are expecting to have multiple PHYs on the 10/100 cards,
	 * and on those cards we exclude the internal PHY from providing
	 * 10baseT.  By ignoring the instance, it allows us to not have
	 * to specify it on the command line when switching media.
	 */
	sc->jme_mii.mii_ifp = ifp;
	sc->jme_mii.mii_readreg = jme_mii_read;
	sc->jme_mii.mii_writereg = jme_mii_write;
	sc->jme_mii.mii_statchg = jme_statchg;
	sc->jme_ec.ec_mii = &sc->jme_mii;
	ifmedia_init(&sc->jme_mii.mii_media, IFM_IMASK, jme_mediachange,
	    ether_mediastatus);
	mii_attach(self, &sc->jme_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->jme_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->jme_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->jme_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->jme_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->jme_ec.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;

	if (sc->jme_flags & JME_FLAG_GIGA)
		sc->jme_ec.ec_capabilities |= ETHERCAP_JUMBO_MTU;


	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
	ifp->if_ioctl = jme_ifioctl;
	ifp->if_start = jme_ifstart;
	ifp->if_watchdog = jme_ifwatchdog;
	ifp->if_init = jme_ifinit;
	ifp->if_stop = jme_stop;
	ifp->if_timer = 0;
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx |
	    IFCAP_CSUM_TCPv6_Tx | /* IFCAP_CSUM_TCPv6_Rx | hardware bug */
	    IFCAP_CSUM_UDPv6_Tx | /* IFCAP_CSUM_UDPv6_Rx | hardware bug */
	    IFCAP_TSOv4 | IFCAP_TSOv6;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	ether_ifattach(&(sc)->jme_if, (sc)->jme_enaddr);

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot.
	 */
	if (pmf_device_register1(self, NULL, NULL, jme_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	sc->jme_intrxto = PCCRX_COAL_TO_DEFAULT;
	sc->jme_intrxct = PCCRX_COAL_PKT_DEFAULT;
	sc->jme_inttxto = PCCTX_COAL_TO_DEFAULT;
	sc->jme_inttxct = PCCTX_COAL_PKT_DEFAULT;
	if (sysctl_createv(&sc->jme_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, device_xname(sc->jme_dev),
	    SYSCTL_DESCR("jme per-controller controls"),
	    NULL, 0, NULL, 0, CTL_HW, jme_root_num, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->jme_dev, "couldn't create sysctl node\n");
		return;
	}
	jme_nodenum = node->sysctl_num;

	/* interrupt moderation sysctls */
	if (sysctl_createv(&sc->jme_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_rxto",
	    SYSCTL_DESCR("jme RX interrupt moderation timer"),
	    jme_sysctl_intrxto, 0, (void *)sc,
	    0, CTL_HW, jme_root_num, jme_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->jme_dev,
		    "couldn't create int_rxto sysctl node\n");
	}
	if (sysctl_createv(&sc->jme_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_rxct",
	    SYSCTL_DESCR("jme RX interrupt moderation packet counter"),
	    jme_sysctl_intrxct, 0, (void *)sc,
	    0, CTL_HW, jme_root_num, jme_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->jme_dev,
		    "couldn't create int_rxct sysctl node\n");
	}
	if (sysctl_createv(&sc->jme_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_txto",
	    SYSCTL_DESCR("jme TX interrupt moderation timer"),
	    jme_sysctl_inttxto, 0, (void *)sc,
	    0, CTL_HW, jme_root_num, jme_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->jme_dev,
		    "couldn't create int_txto sysctl node\n");
	}
	if (sysctl_createv(&sc->jme_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_txct",
	    SYSCTL_DESCR("jme TX interrupt moderation packet counter"),
	    jme_sysctl_inttxct, 0, (void *)sc,
	    0, CTL_HW, jme_root_num, jme_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->jme_dev,
		    "couldn't create int_txct sysctl node\n");
	}
}

static void
jme_stop_rx(jme_softc_t *sc)
{
	uint32_t reg;
	int i;

	reg = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXCSR);
	if ((reg & RXCSR_RX_ENB) == 0)
		return;
	reg &= ~RXCSR_RX_ENB;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXCSR, reg);
	for (i = JME_TIMEOUT / 10; i > 0; i--) {
		DELAY(10);
		if ((bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac,
		    JME_RXCSR) & RXCSR_RX_ENB) == 0)
			break;
	}
	if (i == 0)
		aprint_error_dev(sc->jme_dev, "stopping recevier timeout!\n");

}

static void
jme_stop_tx(jme_softc_t *sc)
{
	uint32_t reg;
	int i;

	reg = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR);
	if ((reg & TXCSR_TX_ENB) == 0)
		return;
	reg &= ~TXCSR_TX_ENB;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR, reg);
	for (i = JME_TIMEOUT / 10; i > 0; i--) {
		DELAY(10);
		if ((bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac,
		    JME_TXCSR) & TXCSR_TX_ENB) == 0)
			break;
	}
	if (i == 0)
		aprint_error_dev(sc->jme_dev,
		    "stopping transmitter timeout!\n");
}

static void
jme_reset(jme_softc_t *sc)
{
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_GHC, GHC_RESET);
	DELAY(10);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_GHC, 0);
}

static bool
jme_shutdown(device_t self, int howto)
{
	jme_softc_t *sc;
	struct ifnet *ifp;

	sc = device_private(self);
	ifp = &sc->jme_if;
	jme_stop(ifp, 1);

	return true;
}

static void
jme_stop(struct ifnet *ifp, int disable)
{
	jme_softc_t *sc = ifp->if_softc;
	int i;
	/* Stop receiver, transmitter. */
	jme_stop_rx(sc);
	jme_stop_tx(sc);
	/* free receive mbufs */
	for (i = 0; i < JME_NBUFS; i++) {
		if (sc->jme_rxmbuf[i]) {
			bus_dmamap_unload(sc->jme_dmatag, sc->jme_rxmbufm[i]);
			m_freem(sc->jme_rxmbuf[i]);
		}
		sc->jme_rxmbuf[i] = NULL;
	}
	/* process completed transmits */
	jme_txeof(sc);
	/* free abort pending transmits */
	for (i = 0; i < JME_NBUFS; i++) {
		if (sc->jme_txmbuf[i]) {
			bus_dmamap_unload(sc->jme_dmatag, sc->jme_txmbufm[i]);
			m_freem(sc->jme_txmbuf[i]);
			sc->jme_txmbuf[i] = NULL;
		}
	}
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

#if 0
static void
jme_restart(void *v)
{

	jme_init(v);
}
#endif

static int
jme_add_rxbuf(jme_softc_t *sc, struct mbuf *m)
{
	int error;
	bus_dmamap_t map;
	int i = sc->jme_rx_prod;

	if (sc->jme_rxmbuf[i] != NULL) {
		aprint_error_dev(sc->jme_dev,
		    "mbuf already here: rxprod %d rxcons %d\n",
		    sc->jme_rx_prod, sc->jme_rx_cons);
		if (m)
			m_freem(m);
		return EINVAL;
	}

	if (m == NULL) {
		sc->jme_rxmbuf[i] = NULL;
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return (ENOBUFS);
		}
	}
	map = sc->jme_rxmbufm[i];
	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	KASSERT(m->m_len == MCLBYTES);

	error = bus_dmamap_load_mbuf(sc->jme_dmatag, map, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		sc->jme_rxmbuf[i] = NULL;
		aprint_error_dev(sc->jme_dev,
		    "unable to load rx DMA map %d, error = %d\n",
		    i, error);
		m_freem(m);
		return (error);
	}
	bus_dmamap_sync(sc->jme_dmatag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	sc->jme_rxmbuf[i] = m;

	sc->jme_rxring[i].buflen = htole32(map->dm_segs[0].ds_len);
	sc->jme_rxring[i].addr_lo =
	    htole32(JME_ADDR_LO(map->dm_segs[0].ds_addr));
	sc->jme_rxring[i].addr_hi =
	    htole32(JME_ADDR_HI(map->dm_segs[0].ds_addr));
	sc->jme_rxring[i].flags =
	    htole32(JME_RD_OWN | JME_RD_INTR | JME_RD_64BIT);
	bus_dmamap_sync(sc->jme_dmatag, sc->jme_rxmap,
	    i * sizeof(struct jme_desc), sizeof(struct jme_desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	JME_DESC_INC(sc->jme_rx_prod, JME_NBUFS);
	return (0);
}

static int
jme_ifinit(struct ifnet *ifp)
{
	return jme_init(ifp, 1);
}

static int
jme_init(struct ifnet *ifp, int do_ifinit)
{
	jme_softc_t *sc = ifp->if_softc;
	int i, s;
	uint8_t eaddr[ETHER_ADDR_LEN];
	uint32_t reg;

	s = splnet();
	/* cancel any pending IO */
	jme_stop(ifp, 1);
	jme_reset(sc);
	if ((sc->jme_if.if_flags & IFF_UP) == 0) {
		splx(s);
		return 0;
	}
	/* allocate receive ring */
	sc->jme_rx_prod = 0;
	for (i = 0; i < JME_NBUFS; i++) {
		if (jme_add_rxbuf(sc, NULL) < 0) {
			aprint_error_dev(sc->jme_dev,
			    "can't allocate rx mbuf\n");
			for (i--; i >= 0; i--) {
				bus_dmamap_unload(sc->jme_dmatag,
				    sc->jme_rxmbufm[i]);
				m_freem(sc->jme_rxmbuf[i]);
				sc->jme_rxmbuf[i] = NULL;
			}
			splx(s);
			return ENOMEM;
		}
	}
	/* init TX ring */
	memset(sc->jme_txring, 0, JME_NBUFS * sizeof(struct jme_desc));
	bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmap,
	    0, JME_NBUFS * sizeof(struct jme_desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	for (i = 0; i < JME_NBUFS; i++)
		sc->jme_txmbuf[i] = NULL;
	sc->jme_tx_cons = sc->jme_tx_prod = sc->jme_tx_cnt = 0;

	/* Reprogram the station address. */
	memcpy(eaddr, CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PAR0,
	    eaddr[3] << 24 | eaddr[2] << 16 | eaddr[1] << 8 | eaddr[0]);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
	    JME_PAR1, eaddr[5] << 8 | eaddr[4]);

	/*
	 * Configure Tx queue.
	 *  Tx priority queue weight value : 0
	 *  Tx FIFO threshold for processing next packet : 16QW
	 *  Maximum Tx DMA length : 512
	 *  Allow Tx DMA burst.
	 */
	sc->jme_txcsr = TXCSR_TXQ_N_SEL(TXCSR_TXQ0);
	sc->jme_txcsr |= TXCSR_TXQ_WEIGHT(TXCSR_TXQ_WEIGHT_MIN);
	sc->jme_txcsr |= TXCSR_FIFO_THRESH_16QW;
	sc->jme_txcsr |= TXCSR_DMA_SIZE_512;
	sc->jme_txcsr |= TXCSR_DMA_BURST;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
	     JME_TXCSR, sc->jme_txcsr);

	/* Set Tx descriptor counter. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
	     JME_TXQDC, JME_NBUFS);

	/* Set Tx ring address to the hardware. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_HI,
	    JME_ADDR_HI(sc->jme_txmap->dm_segs[0].ds_addr));
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_LO,
	    JME_ADDR_LO(sc->jme_txmap->dm_segs[0].ds_addr));

	/* Configure TxMAC parameters. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXMAC,
	    TXMAC_IFG1_DEFAULT | TXMAC_IFG2_DEFAULT | TXMAC_IFG_ENB |
	    TXMAC_THRESH_1_PKT | TXMAC_CRC_ENB | TXMAC_PAD_ENB);

	/*
	 * Configure Rx queue.
	 *  FIFO full threshold for transmitting Tx pause packet : 128T
	 *  FIFO threshold for processing next packet : 128QW
	 *  Rx queue 0 select
	 *  Max Rx DMA length : 128
	 *  Rx descriptor retry : 32
	 *  Rx descriptor retry time gap : 256ns
	 *  Don't receive runt/bad frame.
	 */
	sc->jme_rxcsr = RXCSR_FIFO_FTHRESH_128T;
	/*
	 * Since Rx FIFO size is 4K bytes, receiving frames larger
	 * than 4K bytes will suffer from Rx FIFO overruns. So
	 * decrease FIFO threshold to reduce the FIFO overruns for
	 * frames larger than 4000 bytes.
	 * For best performance of standard MTU sized frames use
	 * maximum allowable FIFO threshold, 128QW.
	 */
	if ((ifp->if_mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN +
	    ETHER_CRC_LEN) > JME_RX_FIFO_SIZE)
		sc->jme_rxcsr |= RXCSR_FIFO_THRESH_16QW;
	else
		sc->jme_rxcsr |= RXCSR_FIFO_THRESH_128QW;
	sc->jme_rxcsr |= RXCSR_DMA_SIZE_128 | RXCSR_RXQ_N_SEL(RXCSR_RXQ0);
	sc->jme_rxcsr |= RXCSR_DESC_RT_CNT(RXCSR_DESC_RT_CNT_DEFAULT);
	sc->jme_rxcsr |= RXCSR_DESC_RT_GAP_256 & RXCSR_DESC_RT_GAP_MASK;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
	     JME_RXCSR, sc->jme_rxcsr);

	/* Set Rx descriptor counter. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
	     JME_RXQDC, JME_NBUFS);

	/* Set Rx ring address to the hardware. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXDBA_HI,
	    JME_ADDR_HI(sc->jme_rxmap->dm_segs[0].ds_addr));
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXDBA_LO,
	    JME_ADDR_LO(sc->jme_rxmap->dm_segs[0].ds_addr));

	/* Clear receive filter. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC, 0);
	/* Set up the receive filter. */
	jme_set_filter(sc);

	/*
	 * Disable all WOL bits as WOL can interfere normal Rx
	 * operation. Also clear WOL detection status bits.
	 */
	reg = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PMCS);
	reg &= ~PMCS_WOL_ENB_MASK;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PMCS, reg);

	reg = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC);
	/*
	 * Pad 10bytes right before received frame. This will greatly
	 * help Rx performance on strict-alignment architectures as
	 * it does not need to copy the frame to align the payload.
	 */
	reg |= RXMAC_PAD_10BYTES;
	if ((ifp->if_capenable &
	    (IFCAP_CSUM_IPv4_Rx|IFCAP_CSUM_TCPv4_Rx|IFCAP_CSUM_UDPv4_Rx|
	     IFCAP_CSUM_TCPv6_Rx|IFCAP_CSUM_UDPv6_Rx)) != 0)
		reg |= RXMAC_CSUM_ENB;
	reg |= RXMAC_VLAN_ENB; /* enable hardware vlan */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC, reg);

	/* Configure general purpose reg0 */
	reg = bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_GPREG0);
	reg &= ~GPREG0_PCC_UNIT_MASK;
	/* Set PCC timer resolution to micro-seconds unit. */
	reg |= GPREG0_PCC_UNIT_US;
	/*
	 * Disable all shadow register posting as we have to read
	 * JME_INTR_STATUS register in jme_int_task. Also it seems
	 * that it's hard to synchronize interrupt status between
	 * hardware and software with shadow posting due to
	 * requirements of bus_dmamap_sync(9).
	 */
	reg |= GPREG0_SH_POST_DW7_DIS | GPREG0_SH_POST_DW6_DIS |
	    GPREG0_SH_POST_DW5_DIS | GPREG0_SH_POST_DW4_DIS |
	    GPREG0_SH_POST_DW3_DIS | GPREG0_SH_POST_DW2_DIS |
	    GPREG0_SH_POST_DW1_DIS | GPREG0_SH_POST_DW0_DIS;
	/* Disable posting of DW0. */
	reg &= ~GPREG0_POST_DW0_ENB;
	/* Clear PME message. */
	reg &= ~GPREG0_PME_ENB;
	/* Set PHY address. */
	reg &= ~GPREG0_PHY_ADDR_MASK;
	reg |= sc->jme_phyaddr;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_GPREG0, reg);

	/* Configure Tx queue 0 packet completion coalescing. */
	reg = (sc->jme_inttxto << PCCTX_COAL_TO_SHIFT) & PCCTX_COAL_TO_MASK;
	reg |= (sc->jme_inttxct << PCCTX_COAL_PKT_SHIFT) & PCCTX_COAL_PKT_MASK;
	reg |= PCCTX_COAL_TXQ0;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCTX, reg);

	/* Configure Rx queue 0 packet completion coalescing. */
	reg = (sc->jme_intrxto << PCCRX_COAL_TO_SHIFT) & PCCRX_COAL_TO_MASK;
	reg |= (sc->jme_intrxct << PCCRX_COAL_PKT_SHIFT) & PCCRX_COAL_PKT_MASK;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCRX0, reg);

	/* Disable Timers */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_TMCSR, 0);
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_TIMER1, 0);
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_TIMER2, 0);

	/* Configure retry transmit period, retry limit value. */
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD,
	    ((TXTRHD_RT_PERIOD_DEFAULT << TXTRHD_RT_PERIOD_SHIFT) &
	    TXTRHD_RT_PERIOD_MASK) |
	    ((TXTRHD_RT_LIMIT_DEFAULT << TXTRHD_RT_LIMIT_SHIFT) &
	    TXTRHD_RT_LIMIT_SHIFT));

	/* Disable RSS. */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	    JME_RSSC, RSSC_DIS_RSS);

	/* Initialize the interrupt mask. */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_MASK_SET, JME_INTRS_ENABLE);
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_STATUS, 0xFFFFFFFF);

	/* set media, if not already handling a media change */
	if (do_ifinit) {
		int error;
		if ((error = mii_mediachg(&sc->jme_mii)) == ENXIO)
			error = 0;
		else if (error != 0) {
			aprint_error_dev(sc->jme_dev, "could not set media\n");
			splx(s);
			return error;
		}
	}

	/* Program MAC with resolved speed/duplex/flow-control. */
	jme_mac_config(sc);

	/* Start receiver/transmitter. */
	sc->jme_rx_cons = 0;
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXCSR,
	    sc->jme_rxcsr | RXCSR_RX_ENB | RXCSR_RXQ_START);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR,
	    sc->jme_txcsr | TXCSR_TX_ENB);

	/* start ticks calls */
	callout_reset(&sc->jme_tick_ch, hz, jme_ticks, sc);
	sc->jme_if.if_flags |= IFF_RUNNING;
	sc->jme_if.if_flags &= ~IFF_OACTIVE;
	splx(s);
	return 0;
}


int
jme_mii_read(device_t self, int phy, int reg)
{
	struct jme_softc *sc = device_private(self);
	int val, i;

	/* For FPGA version, PHY address 0 should be ignored. */
	if ((sc->jme_flags & JME_FLAG_FPGA) != 0) {
		if (phy == 0)
			return (0);
	} else {
		if (sc->jme_phyaddr != phy)
			return (0);
	}

	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_SMI,
	    SMI_OP_READ | SMI_OP_EXECUTE |
	    SMI_PHY_ADDR(phy) | SMI_REG_ADDR(reg));
	for (i = JME_PHY_TIMEOUT / 10; i > 0; i--) {
		delay(10);
		if (((val = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac,
		    JME_SMI)) & SMI_OP_EXECUTE) == 0)
			break;
	}

	if (i == 0) {
		aprint_error_dev(sc->jme_dev, "phy read timeout : %d\n", reg);
		return (0);
	}

	return ((val & SMI_DATA_MASK) >> SMI_DATA_SHIFT);
}

void
jme_mii_write(device_t self, int phy, int reg, int val)
{
	struct jme_softc *sc = device_private(self);
	int i;

	/* For FPGA version, PHY address 0 should be ignored. */
	if ((sc->jme_flags & JME_FLAG_FPGA) != 0) {
		if (phy == 0)
			return;
	} else {
		if (sc->jme_phyaddr != phy)
			return;
	}

	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_SMI,
	    SMI_OP_WRITE | SMI_OP_EXECUTE |
	    ((val << SMI_DATA_SHIFT) & SMI_DATA_MASK) |
	    SMI_PHY_ADDR(phy) | SMI_REG_ADDR(reg));
	for (i = JME_PHY_TIMEOUT / 10; i > 0; i--) {
		delay(10);
		if (((val = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac,
		    JME_SMI)) & SMI_OP_EXECUTE) == 0)
			break;
	}

	if (i == 0)
		aprint_error_dev(sc->jme_dev, "phy write timeout : %d\n", reg);

	return;
}

void
jme_statchg(struct ifnet *ifp)
{
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING))
		jme_init(ifp, 0);
}

static void
jme_intr_rx(jme_softc_t *sc) {
	struct mbuf *m, *mhead;
	bus_dmamap_t mmap;
	struct ifnet *ifp = &sc->jme_if;
	uint32_t flags,  buflen;
	int i, ipackets, nsegs, seg, error;
	struct jme_desc *desc;

	bus_dmamap_sync(sc->jme_dmatag, sc->jme_rxmap, 0,
	    sizeof(struct jme_desc) * JME_NBUFS,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#ifdef JMEDEBUG_RX
	printf("rxintr sc->jme_rx_cons %d flags 0x%x\n",
	    sc->jme_rx_cons, le32toh(sc->jme_rxring[sc->jme_rx_cons].flags));
#endif
	ipackets = 0;
	while((le32toh(sc->jme_rxring[sc->jme_rx_cons].flags) & JME_RD_OWN)
	    == 0) {
		i = sc->jme_rx_cons;
		desc = &sc->jme_rxring[i];
#ifdef JMEDEBUG_RX
		printf("rxintr i %d flags 0x%x buflen 0x%x\n",
		    i,  le32toh(desc->flags), le32toh(desc->buflen));
#endif
		if (sc->jme_rxmbuf[i] == NULL) {
			if ((error = jme_add_rxbuf(sc, NULL)) != 0) {
				aprint_error_dev(sc->jme_dev,
				    "can't add new mbuf to empty slot: %d\n",
				    error);
				break;
			}
			JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
			i = sc->jme_rx_cons;
			continue;
		}
		if ((le32toh(desc->buflen) & JME_RD_VALID) == 0)
			break;

		buflen = le32toh(desc->buflen);
		nsegs = JME_RX_NSEGS(buflen);
		flags = le32toh(desc->flags);
		if ((buflen & JME_RX_ERR_STAT) != 0 ||
		    JME_RX_BYTES(buflen) < sizeof(struct ether_header) ||
		    JME_RX_BYTES(buflen) >
		    (ifp->if_mtu + ETHER_HDR_LEN + JME_RX_PAD_BYTES)) {
#ifdef JMEDEBUG_RX
			printf("rx error flags 0x%x buflen 0x%x\n",
			    flags, buflen);
#endif
			ifp->if_ierrors++;
			/* reuse the mbufs */
			for (seg = 0; seg < nsegs; seg++) {
				m = sc->jme_rxmbuf[i];
				sc->jme_rxmbuf[i] = NULL;
				mmap = sc->jme_rxmbufm[i];
				bus_dmamap_sync(sc->jme_dmatag, mmap, 0,
				    mmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->jme_dmatag, mmap);
				if ((error = jme_add_rxbuf(sc, m)) != 0)
					aprint_error_dev(sc->jme_dev,
					    "can't reuse mbuf: %d\n", error);
				JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
				i = sc->jme_rx_cons;
			}
			continue;
		}
		/* receive this packet */
		mhead = m = sc->jme_rxmbuf[i];
		sc->jme_rxmbuf[i] = NULL;
		mmap = sc->jme_rxmbufm[i];
		bus_dmamap_sync(sc->jme_dmatag, mmap, 0,
		    mmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->jme_dmatag, mmap);
		/* add a new buffer to chain */
		if (jme_add_rxbuf(sc, NULL) != 0) {
			if ((error = jme_add_rxbuf(sc, m)) != 0)
				aprint_error_dev(sc->jme_dev,
				    "can't reuse mbuf: %d\n", error);
			JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
			i = sc->jme_rx_cons;
			for (seg = 1; seg < nsegs; seg++) {
				m = sc->jme_rxmbuf[i];
				sc->jme_rxmbuf[i] = NULL;
				mmap = sc->jme_rxmbufm[i];
				bus_dmamap_sync(sc->jme_dmatag, mmap, 0,
				    mmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->jme_dmatag, mmap);
				if ((error = jme_add_rxbuf(sc, m)) != 0)
					aprint_error_dev(sc->jme_dev,
					    "can't reuse mbuf: %d\n", error);
				JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
				i = sc->jme_rx_cons;
			}
			ifp->if_ierrors++;
			continue;
		}

		/* build mbuf chain: head, then remaining segments */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = JME_RX_BYTES(buflen) - JME_RX_PAD_BYTES;
		m->m_len = (nsegs > 1) ? (MCLBYTES - JME_RX_PAD_BYTES) :
		    m->m_pkthdr.len;
		m->m_data = m->m_ext.ext_buf + JME_RX_PAD_BYTES;
		JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
		for (seg = 1; seg < nsegs; seg++) {
			i = sc->jme_rx_cons;
			m = sc->jme_rxmbuf[i];
			sc->jme_rxmbuf[i] = NULL;
			mmap = sc->jme_rxmbufm[i];
			bus_dmamap_sync(sc->jme_dmatag, mmap, 0,
			    mmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->jme_dmatag, mmap);
			if ((error = jme_add_rxbuf(sc, NULL)) != 0)
				aprint_error_dev(sc->jme_dev,
				    "can't add new mbuf: %d\n", error);
			m->m_flags &= ~M_PKTHDR;
			m_cat(mhead, m);
			JME_DESC_INC(sc->jme_rx_cons, JME_NBUFS);
		}
		/* and adjust last mbuf's size */
		if (nsegs > 1) {
			m->m_len =
			    JME_RX_BYTES(buflen) - (MCLBYTES * (nsegs - 1));
		}
		ifp->if_ipackets++;
		ipackets++;
		bpf_mtap(ifp, mhead);

		if ((ifp->if_capenable & IFCAP_CSUM_IPv4_Rx) &&
		    (flags & JME_RD_IPV4)) {
			mhead->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (!(flags & JME_RD_IPCSUM))
				mhead->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		}
		if ((ifp->if_capenable & IFCAP_CSUM_TCPv4_Rx) &&
		    (flags & JME_RD_TCPV4) == JME_RD_TCPV4) {
			mhead->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
			if (!(flags & JME_RD_TCPCSUM))
				mhead->m_pkthdr.csum_flags |=
				    M_CSUM_TCP_UDP_BAD;
		}
		if ((ifp->if_capenable & IFCAP_CSUM_UDPv4_Rx) &&
		    (flags & JME_RD_UDPV4) == JME_RD_UDPV4) {
			mhead->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
			if (!(flags & JME_RD_UDPCSUM))
				mhead->m_pkthdr.csum_flags |=
				    M_CSUM_TCP_UDP_BAD;
		}
		if ((ifp->if_capenable & IFCAP_CSUM_TCPv6_Rx) &&
		    (flags & JME_RD_TCPV6) == JME_RD_TCPV6) {
			mhead->m_pkthdr.csum_flags |= M_CSUM_TCPv6;
			if (!(flags & JME_RD_TCPCSUM))
				mhead->m_pkthdr.csum_flags |=
				    M_CSUM_TCP_UDP_BAD;
		}
		if ((ifp->if_capenable & IFCAP_CSUM_UDPv6_Rx) &&
		    (flags & JME_RD_UDPV6) == JME_RD_UDPV6) {
			m->m_pkthdr.csum_flags |= M_CSUM_UDPv6;
			if (!(flags & JME_RD_UDPCSUM))
				mhead->m_pkthdr.csum_flags |=
				    M_CSUM_TCP_UDP_BAD;
		}
		if (flags & JME_RD_VLAN_TAG) {
			/* pass to vlan_input() */
			VLAN_INPUT_TAG(ifp, mhead,
			    (flags & JME_RD_VLAN_MASK), continue);
		}
		(*ifp->if_input)(ifp, mhead);
	}
	if (ipackets)
		rnd_add_uint32(&sc->rnd_source, ipackets);
}

static int
jme_intr(void *v)
{
	jme_softc_t *sc = v;
	uint32_t istatus;

	istatus = bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_STATUS);
	if (istatus == 0 || istatus == 0xFFFFFFFF)
		return 0;
	/* Disable interrupts. */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	    JME_INTR_MASK_CLR, 0xFFFFFFFF);
again:
	/* and update istatus */
	istatus = bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_STATUS);
	if ((istatus & JME_INTRS_CHECK) == 0)
		goto done;
	/* Reset PCC counter/timer and Ack interrupts. */
	if ((istatus & (INTR_TXQ_COMP | INTR_TXQ_COAL | INTR_TXQ_COAL_TO)) != 0)
		istatus |= INTR_TXQ_COAL | INTR_TXQ_COAL_TO | INTR_TXQ_COMP;
	if ((istatus & (INTR_RXQ_COMP | INTR_RXQ_COAL | INTR_RXQ_COAL_TO)) != 0)
		istatus |= INTR_RXQ_COAL | INTR_RXQ_COAL_TO | INTR_RXQ_COMP;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_STATUS, istatus);

	if ((sc->jme_if.if_flags & IFF_RUNNING) == 0)
		goto done;
#ifdef JMEDEBUG_RX
	printf("jme_intr 0x%x RXCS 0x%x RXDBA 0x%x  0x%x RXQDC 0x%x RXNDA 0x%x RXMCS 0x%x\n", istatus,
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXCSR),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXDBA_LO),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXDBA_HI),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXQDC),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXNDA),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC));
	printf("jme_intr RXUMA 0x%x 0x%x RXMCHT 0x%x 0x%x GHC 0x%x\n",
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PAR0),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PAR1),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_MAR0),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_MAR1),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_GHC));
#endif
	if ((istatus & (INTR_RXQ_COMP | INTR_RXQ_COAL | INTR_RXQ_COAL_TO)) != 0)
		jme_intr_rx(sc);
	if ((istatus & INTR_RXQ_DESC_EMPTY) != 0) {
		/*
		 * Notify hardware availability of new Rx
		 * buffers.
		 * Reading RXCSR takes very long time under
		 * heavy load so cache RXCSR value and writes
		 * the ORed value with the kick command to
		 * the RXCSR. This saves one register access
		 * cycle.
		 */
		sc->jme_rx_cons = 0;
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
		    JME_RXCSR,
		    sc->jme_rxcsr | RXCSR_RX_ENB | RXCSR_RXQ_START);
	}
	if ((istatus & (INTR_TXQ_COMP | INTR_TXQ_COAL | INTR_TXQ_COAL_TO)) != 0)
		jme_ifstart(&sc->jme_if);

	goto again;

done:
	/* enable interrupts. */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	    JME_INTR_MASK_SET, JME_INTRS_ENABLE);
	return 1;
}


static int
jme_ifioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct jme_softc *sc = ifp->if_softc;
	int s, error;
	struct ifreq *ifr;
	struct ifcapreq *ifcr;

	s = splnet();
	/*
	 * we can't support at the same time jumbo frames and
	 * TX checksums offload/TSO
	 */
	switch(cmd) {
	case SIOCSIFMTU:
		ifr = data;
		if (ifr->ifr_mtu > JME_TX_FIFO_SIZE &&
		    (ifp->if_capenable & (
		    IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_UDPv4_Tx|
		    IFCAP_CSUM_TCPv6_Tx|IFCAP_CSUM_UDPv6_Tx|
		    IFCAP_TSOv4|IFCAP_TSOv6)) != 0) {
			splx(s);
			return EINVAL;
		}
		break;
	case SIOCSIFCAP:
		ifcr = data;
		if (ifp->if_mtu > JME_TX_FIFO_SIZE &&
		    (ifcr->ifcr_capenable & (
		    IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_UDPv4_Tx|
		    IFCAP_CSUM_TCPv6_Tx|IFCAP_CSUM_UDPv6_Tx|
		    IFCAP_TSOv4|IFCAP_TSOv6)) != 0) {
			splx(s);
			return EINVAL;
		}
		break;
	}

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET && (ifp->if_flags & IFF_RUNNING)) {
		if (cmd == SIOCADDMULTI || cmd == SIOCDELMULTI) {
			jme_set_filter(sc);
			error = 0;
		} else {
			error = jme_init(ifp, 0);
		}
	}
	splx(s);
	return error;
}

static int
jme_encap(struct jme_softc *sc, struct mbuf **m_head)
{
	struct jme_desc *desc;
	struct mbuf *m;
	struct m_tag *mtag;
	int error, i, prod, headdsc, nsegs;
	uint32_t cflags, tso_segsz;

	if (((*m_head)->m_pkthdr.csum_flags & (M_CSUM_TSOv4|M_CSUM_TSOv6)) != 0){
		/*
		 * Due to the adherence to NDIS specification JMC250
		 * assumes upper stack computed TCP pseudo checksum
		 * without including payload length. This breaks
		 * checksum offload for TSO case so recompute TCP
		 * pseudo checksum for JMC250. Hopefully this wouldn't
		 * be much burden on modern CPUs.
		 */
		bool v4 = ((*m_head)->m_pkthdr.csum_flags & M_CSUM_TSOv4) != 0;
		int iphl = v4 ?
		    M_CSUM_DATA_IPv4_IPHL((*m_head)->m_pkthdr.csum_data) :
		    M_CSUM_DATA_IPv6_HL((*m_head)->m_pkthdr.csum_data);
		/*
		 * note: we support vlan offloading, so we should never have
		 * a ETHERTYPE_VLAN packet here - so ETHER_HDR_LEN is always
		 * right.
		 */
		int hlen = ETHER_HDR_LEN + iphl;

		if (__predict_false((*m_head)->m_len <
		    (hlen + sizeof(struct tcphdr)))) {
			   /*
			    * TCP/IP headers are not in the first mbuf; we need
			    * to do this the slow and painful way.  Let's just
			    * hope this doesn't happen very often.
			    */
			   struct tcphdr th;

			   m_copydata((*m_head), hlen, sizeof(th), &th);
			   if (v4) {
				    struct ip ip;

				    m_copydata((*m_head), ETHER_HDR_LEN,
				    sizeof(ip), &ip);
				    ip.ip_len = 0;
				    m_copyback((*m_head),
					 ETHER_HDR_LEN + offsetof(struct ip, ip_len),
					 sizeof(ip.ip_len), &ip.ip_len);
				    th.th_sum = in_cksum_phdr(ip.ip_src.s_addr,
					 ip.ip_dst.s_addr, htons(IPPROTO_TCP));
			   } else {
#if INET6
				    struct ip6_hdr ip6;

				    m_copydata((*m_head), ETHER_HDR_LEN,
				    sizeof(ip6), &ip6);
				    ip6.ip6_plen = 0;
				    m_copyback((*m_head), ETHER_HDR_LEN +
				    offsetof(struct ip6_hdr, ip6_plen),
					 sizeof(ip6.ip6_plen), &ip6.ip6_plen);
				    th.th_sum = in6_cksum_phdr(&ip6.ip6_src,
					 &ip6.ip6_dst, 0, htonl(IPPROTO_TCP));
#endif /* INET6 */
			   }
			   m_copyback((*m_head),
			    hlen + offsetof(struct tcphdr, th_sum),
				sizeof(th.th_sum), &th.th_sum);

			   hlen += th.th_off << 2;
		} else {
			   /*
			    * TCP/IP headers are in the first mbuf; we can do
			    * this the easy way.
			    */
			   struct tcphdr *th;

			   if (v4) {
				    struct ip *ip =
					 (void *)(mtod((*m_head), char *) +
					ETHER_HDR_LEN);
				    th = (void *)(mtod((*m_head), char *) + hlen);

				    ip->ip_len = 0;
				    th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
					 ip->ip_dst.s_addr, htons(IPPROTO_TCP));
			   } else {
#if INET6
				    struct ip6_hdr *ip6 =
				    (void *)(mtod((*m_head), char *) +
				    ETHER_HDR_LEN);
				    th = (void *)(mtod((*m_head), char *) + hlen);

				    ip6->ip6_plen = 0;
				    th->th_sum = in6_cksum_phdr(&ip6->ip6_src,
					 &ip6->ip6_dst, 0, htonl(IPPROTO_TCP));
#endif /* INET6 */
			   }
			hlen += th->th_off << 2;
		}

	}

	prod = sc->jme_tx_prod;

	error = bus_dmamap_load_mbuf(sc->jme_dmatag, sc->jme_txmbufm[prod],
	    *m_head, BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error) {
		if (error == EFBIG) {
			log(LOG_ERR, "%s: Tx packet consumes too many "
			    "DMA segments, dropping...\n",
			    device_xname(sc->jme_dev));
			m_freem(*m_head);
			m_head = NULL;
		}
		return (error);
	}
	/*
	 * Check descriptor overrun. Leave one free descriptor.
	 * Since we always use 64bit address mode for transmitting,
	 * each Tx request requires one more dummy descriptor.
	 */
	nsegs = sc->jme_txmbufm[prod]->dm_nsegs;
#ifdef JMEDEBUG_TX
	printf("jme_encap prod %d nsegs %d jme_tx_cnt %d\n", prod, nsegs, sc->jme_tx_cnt);
#endif
	if (sc->jme_tx_cnt + nsegs + 1 > JME_NBUFS - 1) {
		bus_dmamap_unload(sc->jme_dmatag, sc->jme_txmbufm[prod]);
		return (ENOBUFS);
	}
	bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmbufm[prod],
	    0, sc->jme_txmbufm[prod]->dm_mapsize, BUS_DMASYNC_PREWRITE);

	m = *m_head;
	cflags = 0;
	tso_segsz = 0;
	/* Configure checksum offload and TSO. */
	if ((m->m_pkthdr.csum_flags & (M_CSUM_TSOv4|M_CSUM_TSOv6)) != 0) {
		tso_segsz = (uint32_t)m->m_pkthdr.segsz << JME_TD_MSS_SHIFT;
		cflags |= JME_TD_TSO;
	} else {
		if ((m->m_pkthdr.csum_flags & M_CSUM_IPv4) != 0)
			cflags |= JME_TD_IPCSUM;
		if ((m->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_TCPv6)) != 0)
			cflags |= JME_TD_TCPCSUM;
		if ((m->m_pkthdr.csum_flags & (M_CSUM_UDPv4|M_CSUM_UDPv6)) != 0)
			cflags |= JME_TD_UDPCSUM;
	}
	/* Configure VLAN. */
	if ((mtag = VLAN_OUTPUT_TAG(&sc->jme_ec, m)) != NULL) {
		cflags |= (VLAN_TAG_VALUE(mtag) & JME_TD_VLAN_MASK);
		cflags |= JME_TD_VLAN_TAG;
	}

	desc = &sc->jme_txring[prod];
	desc->flags = htole32(cflags);
	desc->buflen = htole32(tso_segsz);
	desc->addr_hi = htole32(m->m_pkthdr.len);
	desc->addr_lo = 0;
	headdsc = prod;
	sc->jme_tx_cnt++;
	JME_DESC_INC(prod, JME_NBUFS);
	for (i = 0; i < nsegs; i++) {
		desc = &sc->jme_txring[prod];
		desc->flags = htole32(JME_TD_OWN | JME_TD_64BIT);
		desc->buflen =
		    htole32(sc->jme_txmbufm[headdsc]->dm_segs[i].ds_len);
		desc->addr_hi = htole32(
		    JME_ADDR_HI(sc->jme_txmbufm[headdsc]->dm_segs[i].ds_addr));
		desc->addr_lo = htole32(
		    JME_ADDR_LO(sc->jme_txmbufm[headdsc]->dm_segs[i].ds_addr));
		bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmap,
		    prod * sizeof(struct jme_desc), sizeof(struct jme_desc),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		sc->jme_txmbuf[prod] = NULL;
		sc->jme_tx_cnt++;
		JME_DESC_INC(prod, JME_NBUFS);
	}

	/* Update producer index. */
	sc->jme_tx_prod = prod;
#ifdef JMEDEBUG_TX
	printf("jme_encap prod now %d\n", sc->jme_tx_prod);
#endif
	/*
	 * Finally request interrupt and give the first descriptor
	 * owenership to hardware.
	 */
	desc = &sc->jme_txring[headdsc];
	desc->flags |= htole32(JME_TD_OWN | JME_TD_INTR);
	bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmap,
	    headdsc * sizeof(struct jme_desc), sizeof(struct jme_desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->jme_txmbuf[headdsc] = m;
	return (0);
}

static void
jme_txeof(struct jme_softc *sc)
{
	struct ifnet *ifp;
	struct jme_desc *desc;
	uint32_t status;
	int cons, cons0, nsegs, seg;

	ifp = &sc->jme_if;

#ifdef JMEDEBUG_TX
	printf("jme_txeof cons %d prod %d\n",
	    sc->jme_tx_cons, sc->jme_tx_prod);
	printf("jme_txeof JME_TXCSR 0x%x JME_TXDBA_LO 0x%x JME_TXDBA_HI 0x%x "
	    "JME_TXQDC 0x%x JME_TXNDA 0x%x JME_TXMAC 0x%x JME_TXPFC 0x%x "
	    "JME_TXTRHD 0x%x\n",
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_LO),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_HI),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXQDC),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXNDA),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXMAC),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXPFC),
	    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD));
	for (cons = sc->jme_tx_cons; cons != sc->jme_tx_prod; ) {
		desc = &sc->jme_txring[cons];
		printf("ring[%d] 0x%x 0x%x 0x%x 0x%x\n", cons,
		    desc->flags, desc->buflen, desc->addr_hi, desc->addr_lo);
		JME_DESC_INC(cons, JME_NBUFS);
	}
#endif

	cons = sc->jme_tx_cons;
	if (cons == sc->jme_tx_prod)
		return;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (; cons != sc->jme_tx_prod;) {
		bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmap,
		    cons * sizeof(struct jme_desc), sizeof(struct jme_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		desc = &sc->jme_txring[cons];
		status = le32toh(desc->flags);
#ifdef JMEDEBUG_TX
		printf("jme_txeof %i status 0x%x nsegs %d\n", cons, status,
		    sc->jme_txmbufm[cons]->dm_nsegs);
#endif
		if (status & JME_TD_OWN)
			break;

		if ((status & (JME_TD_TMOUT | JME_TD_RETRY_EXP)) != 0)
			ifp->if_oerrors++;
		else {
			ifp->if_opackets++;
			if ((status & JME_TD_COLLISION) != 0)
				ifp->if_collisions +=
				    le32toh(desc->buflen) &
				    JME_TD_BUF_LEN_MASK;
		}
		/*
		 * Only the first descriptor of multi-descriptor
		 * transmission is updated so driver have to skip entire
		 * chained buffers for the transmiited frame. In other
		 * words, JME_TD_OWN bit is valid only at the first
		 * descriptor of a multi-descriptor transmission.
		 */
		nsegs = sc->jme_txmbufm[cons]->dm_nsegs;
		cons0 = cons;
		JME_DESC_INC(cons, JME_NBUFS);
		for (seg = 1; seg < nsegs + 1; seg++) {
			bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmap,
			    cons * sizeof(struct jme_desc),
			    sizeof(struct jme_desc),
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			sc->jme_txring[cons].flags = 0;
			JME_DESC_INC(cons, JME_NBUFS);
		}
		/* Reclaim transferred mbufs. */
		bus_dmamap_sync(sc->jme_dmatag, sc->jme_txmbufm[cons0],
		    0, sc->jme_txmbufm[cons0]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->jme_dmatag, sc->jme_txmbufm[cons0]);

		KASSERT(sc->jme_txmbuf[cons0] != NULL);
		m_freem(sc->jme_txmbuf[cons0]);
		sc->jme_txmbuf[cons0] = NULL;
		sc->jme_tx_cnt -= nsegs + 1;
		KASSERT(sc->jme_tx_cnt >= 0);
		sc->jme_if.if_flags &= ~IFF_OACTIVE;
	}
	sc->jme_tx_cons = cons;
	/* Unarm watchog timer when there is no pending descriptors in queue. */
	if (sc->jme_tx_cnt == 0)
		ifp->if_timer = 0;
#ifdef JMEDEBUG_TX
	printf("jme_txeof jme_tx_cnt %d\n", sc->jme_tx_cnt);
#endif
}

static void
jme_ifstart(struct ifnet *ifp)
{
	jme_softc_t *sc = ifp->if_softc;
	struct mbuf *mb_head;
	int enq;

	/*
	 * check if we can free some desc.
	 * Clear TX interrupt status to reset TX coalescing counters.
	 */
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
	     JME_INTR_STATUS, INTR_TXQ_COMP);
	jme_txeof(sc);

	if ((sc->jme_if.if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;
	for (enq = 0;; enq++) {
nexttx:
		/* Grab a paquet for output */
		IFQ_DEQUEUE(&ifp->if_snd, mb_head);
		if (mb_head == NULL) {
#ifdef JMEDEBUG_TX
			printf("%s: nothing to send\n", __func__);
#endif
			break;
		}
		/* try to add this mbuf to the TX ring */
		if (jme_encap(sc, &mb_head)) {
			if (mb_head == NULL) {
				ifp->if_oerrors++;
				/* packet dropped, try next one */
				goto nexttx;
			}
			/* resource shortage, try again later */
			IF_PREPEND(&ifp->if_snd, mb_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Pass packet to bpf if there is a listener */
		bpf_mtap(ifp, mb_head);
	}
#ifdef JMEDEBUG_TX
	printf("jme_ifstart enq %d\n", enq);
#endif
	if (enq) {
		/*
		 * Set a 5 second timer just in case we don't hear from
		 * the card again.
		 */
		ifp->if_timer = 5;
		/*
		 * Reading TXCSR takes very long time under heavy load
		 * so cache TXCSR value and writes the ORed value with
		 * the kick command to the TXCSR. This saves one register
		 * access cycle.
		 */
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR,
		  sc->jme_txcsr | TXCSR_TX_ENB | TXCSR_TXQ_N_START(TXCSR_TXQ0));
#ifdef JMEDEBUG_TX
		printf("jme_ifstart JME_TXCSR 0x%x JME_TXDBA_LO 0x%x JME_TXDBA_HI 0x%x "
		    "JME_TXQDC 0x%x JME_TXNDA 0x%x JME_TXMAC 0x%x JME_TXPFC 0x%x "
		    "JME_TXTRHD 0x%x\n",
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXCSR),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_LO),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXDBA_HI),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXQDC),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXNDA),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXMAC),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXPFC),
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD));
#endif
	}
}

static void
jme_ifwatchdog(struct ifnet *ifp)
{
	jme_softc_t *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	printf("%s: device timeout\n", device_xname(sc->jme_dev));
	ifp->if_oerrors++;
	jme_init(ifp, 0);
}

static int
jme_mediachange(struct ifnet *ifp)
{
	int error;
	jme_softc_t *sc = ifp->if_softc;

	if ((error = mii_mediachg(&sc->jme_mii)) == ENXIO)
		error = 0;
	else if (error != 0) {
		aprint_error_dev(sc->jme_dev, "could not set media\n");
		return error;
	}
	return 0;
}

static void
jme_ticks(void *v)
{
	jme_softc_t *sc = v;
	int s = splnet();

	/* Tick the MII. */
	mii_tick(&sc->jme_mii);

	/* every seconds */
	callout_reset(&sc->jme_tick_ch, hz, jme_ticks, sc);
	splx(s);
}

static void
jme_mac_config(jme_softc_t *sc)
{
	uint32_t ghc, gpreg, rxmac, txmac, txpause;
	struct mii_data *mii = &sc->jme_mii;

	ghc = 0;
	rxmac = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC);
	rxmac &= ~RXMAC_FC_ENB;
	txmac = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXMAC);
	txmac &= ~(TXMAC_CARRIER_EXT | TXMAC_FRAME_BURST);
	txpause = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXPFC);
	txpause &= ~TXPFC_PAUSE_ENB;

	if (mii->mii_media_active & IFM_FDX) {
		ghc |= GHC_FULL_DUPLEX;
		rxmac &= ~RXMAC_COLL_DET_ENB;
		txmac &= ~(TXMAC_COLL_ENB | TXMAC_CARRIER_SENSE |
		    TXMAC_BACKOFF | TXMAC_CARRIER_EXT |
		    TXMAC_FRAME_BURST);
		/* Disable retry transmit timer/retry limit. */
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD,
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD)
		    & ~(TXTRHD_RT_PERIOD_ENB | TXTRHD_RT_LIMIT_ENB));
	} else {
		rxmac |= RXMAC_COLL_DET_ENB;
		txmac |= TXMAC_COLL_ENB | TXMAC_CARRIER_SENSE | TXMAC_BACKOFF;
		/* Enable retry transmit timer/retry limit. */
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD,
		    bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXTRHD)		    | TXTRHD_RT_PERIOD_ENB | TXTRHD_RT_LIMIT_ENB);
	}
	/* Reprogram Tx/Rx MACs with resolved speed/duplex. */
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		ghc |= GHC_SPEED_10 | GHC_CLKSRC_10_100;
		break;
	case IFM_100_TX:
		ghc |= GHC_SPEED_100 | GHC_CLKSRC_10_100;
		break;
	case IFM_1000_T:
		ghc |= GHC_SPEED_1000 | GHC_CLKSRC_1000;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) == 0)
			txmac |= TXMAC_CARRIER_EXT | TXMAC_FRAME_BURST;
		break;
	default:
		break;
	}
	if ((sc->jme_flags & JME_FLAG_GIGA) &&
	    sc->jme_chip_rev == DEVICEREVID_JMC250_A2) {
		/*
		 * Workaround occasional packet loss issue of JMC250 A2
		 * when it runs on half-duplex media.
		 */
#ifdef JMEDEBUG
		printf("JME250 A2 workaround\n");
#endif
		gpreg = bus_space_read_4(sc->jme_bt_misc, sc->jme_bh_misc,
		    JME_GPREG1);
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
			gpreg &= ~GPREG1_HDPX_FIX;
		else
			gpreg |= GPREG1_HDPX_FIX;
		bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc,
		    JME_GPREG1, gpreg);
		/* Workaround CRC errors at 100Mbps on JMC250 A2. */
		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
			/* Extend interface FIFO depth. */
			jme_mii_write(sc->jme_dev, sc->jme_phyaddr,
			    0x1B, 0x0000);
		} else {
			/* Select default interface FIFO depth. */
			jme_mii_write(sc->jme_dev, sc->jme_phyaddr,
			    0x1B, 0x0004);
		}
	}
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_GHC, ghc);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC, rxmac);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXMAC, txmac);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_TXPFC, txpause);
}

static void
jme_set_filter(jme_softc_t *sc)
{
	struct ifnet *ifp = &sc->jme_if;
	struct ether_multistep step;
	struct ether_multi *enm;
	uint32_t hash[2] = {0, 0};
	int i;
	uint32_t rxcfg;

	rxcfg = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC);
	rxcfg &= ~ (RXMAC_BROADCAST | RXMAC_PROMISC | RXMAC_MULTICAST |
	    RXMAC_ALLMULTI);
	/* Always accept frames destined to our station address. */
	rxcfg |= RXMAC_UNICAST;
	if ((ifp->if_flags & IFF_BROADCAST) != 0)
		rxcfg |= RXMAC_BROADCAST;
	if ((ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI)) != 0) {
		if ((ifp->if_flags & IFF_PROMISC) != 0)
			rxcfg |= RXMAC_PROMISC;
		if ((ifp->if_flags & IFF_ALLMULTI) != 0)
			rxcfg |= RXMAC_ALLMULTI;
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
		     JME_MAR0, 0xFFFFFFFF);
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
		     JME_MAR1, 0xFFFFFFFF);
		bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac,
		     JME_RXMAC, rxcfg);
		return;
	}
	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the low-order
	 * 6 bits as an index into the 64 bit multicast hash table.  The
	 * high order bits select the register, while the rest of the bits
	 * select the bit within the register.
	 */
	rxcfg |= RXMAC_MULTICAST;
	memset(hash, 0, sizeof(hash));

	ETHER_FIRST_MULTI(step, &sc->jme_ec, enm);
	while (enm != NULL) {
#ifdef JEMDBUG
		printf("%s: addrs %s %s\n", __func__,
		   ether_sprintf(enm->enm_addrlo),
		   ether_sprintf(enm->enm_addrhi));
#endif
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) == 0) {
			i = ether_crc32_be(enm->enm_addrlo, 6);
			/* Just want the 6 least significant bits. */
			i &= 0x3f;
			hash[i / 32] |= 1 << (i%32);
		} else {
			hash[0] = hash[1] = 0xffffffff;
			sc->jme_if.if_flags |= IFF_ALLMULTI;
			break;
		}
		ETHER_NEXT_MULTI(step, enm);
	}
#ifdef JMEDEBUG
	printf("%s: hash1 %x has2 %x\n", __func__, hash[0], hash[1]);
#endif
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_MAR0, hash[0]);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_MAR1, hash[1]);
	bus_space_write_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_RXMAC, rxcfg);
}

#if 0
static int
jme_multicast_hash(uint8_t *a)
{
	int hash;

#define DA(addr,bit) (addr[5 - (bit / 8)] & (1 << (bit % 8)))
#define xor8(a,b,c,d,e,f,g,h)						\
	(((a != 0) + (b != 0) + (c != 0) + (d != 0) + 			\
	  (e != 0) + (f != 0) + (g != 0) + (h != 0)) & 1)

	hash  = xor8(DA(a,0), DA(a, 6), DA(a,12), DA(a,18), DA(a,24), DA(a,30),
	    DA(a,36), DA(a,42));
	hash |= xor8(DA(a,1), DA(a, 7), DA(a,13), DA(a,19), DA(a,25), DA(a,31),
	    DA(a,37), DA(a,43)) << 1;
	hash |= xor8(DA(a,2), DA(a, 8), DA(a,14), DA(a,20), DA(a,26), DA(a,32),
	    DA(a,38), DA(a,44)) << 2;
	hash |= xor8(DA(a,3), DA(a, 9), DA(a,15), DA(a,21), DA(a,27), DA(a,33),
	    DA(a,39), DA(a,45)) << 3;
	hash |= xor8(DA(a,4), DA(a,10), DA(a,16), DA(a,22), DA(a,28), DA(a,34),
	    DA(a,40), DA(a,46)) << 4;
	hash |= xor8(DA(a,5), DA(a,11), DA(a,17), DA(a,23), DA(a,29), DA(a,35),
	    DA(a,41), DA(a,47)) << 5;

	return hash;
}
#endif

static int
jme_eeprom_read_byte(struct jme_softc *sc, uint8_t addr, uint8_t *val)
{
	 uint32_t reg;
	 int i;

	 *val = 0;
	 for (i = JME_EEPROM_TIMEOUT / 10; i > 0; i--) {
		  reg = bus_space_read_4(sc->jme_bt_phy, sc->jme_bh_phy,
		      JME_SMBCSR);
		  if ((reg & SMBCSR_HW_BUSY_MASK) == SMBCSR_HW_IDLE)
			   break;
		  delay(10);
	 }

	 if (i == 0) {
		  aprint_error_dev(sc->jme_dev, "EEPROM idle timeout!\n");
		  return (ETIMEDOUT);
	 }

	 reg = ((uint32_t)addr << SMBINTF_ADDR_SHIFT) & SMBINTF_ADDR_MASK;
	 bus_space_write_4(sc->jme_bt_phy, sc->jme_bh_phy,
	     JME_SMBINTF, reg | SMBINTF_RD | SMBINTF_CMD_TRIGGER);
	 for (i = JME_EEPROM_TIMEOUT / 10; i > 0; i--) {
		  delay(10);
		  reg = bus_space_read_4(sc->jme_bt_phy, sc->jme_bh_phy,
		      JME_SMBINTF);
		  if ((reg & SMBINTF_CMD_TRIGGER) == 0)
			   break;
	 }

	 if (i == 0) {
		  aprint_error_dev(sc->jme_dev, "EEPROM read timeout!\n");
		  return (ETIMEDOUT);
	 }

	 reg = bus_space_read_4(sc->jme_bt_phy, sc->jme_bh_phy, JME_SMBINTF);
	 *val = (reg & SMBINTF_RD_DATA_MASK) >> SMBINTF_RD_DATA_SHIFT;
	 return (0);
}


static int
jme_eeprom_macaddr(struct jme_softc *sc)
{
	uint8_t eaddr[ETHER_ADDR_LEN];
	uint8_t fup, reg, val;
	uint32_t offset;
	int match;

	offset = 0;
	if (jme_eeprom_read_byte(sc, offset++, &fup) != 0 ||
	    fup != JME_EEPROM_SIG0)
		return (ENOENT);
	if (jme_eeprom_read_byte(sc, offset++, &fup) != 0 ||
	    fup != JME_EEPROM_SIG1)
		return (ENOENT);
	match = 0;
	do {
		if (jme_eeprom_read_byte(sc, offset, &fup) != 0)
			break;
		if (JME_EEPROM_MKDESC(JME_EEPROM_FUNC0, JME_EEPROM_PAGE_BAR1)
		    == (fup & (JME_EEPROM_FUNC_MASK|JME_EEPROM_PAGE_MASK))) {
			if (jme_eeprom_read_byte(sc, offset + 1, &reg) != 0)
				break;
			if (reg >= JME_PAR0 &&
			    reg < JME_PAR0 + ETHER_ADDR_LEN) {
				if (jme_eeprom_read_byte(sc, offset + 2,
				    &val) != 0)
					break;
				eaddr[reg - JME_PAR0] = val;
				match++;
			}
		}
		if (fup & JME_EEPROM_DESC_END)
			break;
		
		/* Try next eeprom descriptor. */
		offset += JME_EEPROM_DESC_BYTES;
	} while (match != ETHER_ADDR_LEN && offset < JME_EEPROM_END);

	if (match == ETHER_ADDR_LEN) {
		memcpy(sc->jme_enaddr, eaddr, ETHER_ADDR_LEN);
		return (0);
	}

	return (ENOENT);
}

static int
jme_reg_macaddr(struct jme_softc *sc)
{
	uint32_t par0, par1;

	par0 = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PAR0);
	par1 = bus_space_read_4(sc->jme_bt_mac, sc->jme_bh_mac, JME_PAR1);
	par1 &= 0xffff;
	if ((par0 == 0 && par1 == 0) ||
	    (par0 == 0xffffffff && par1 == 0xffff)) {
		return (ENOENT);
	} else {
		sc->jme_enaddr[0] = (par0 >> 0) & 0xff;
		sc->jme_enaddr[1] = (par0 >> 8) & 0xff;
		sc->jme_enaddr[2] = (par0 >> 16) & 0xff;
		sc->jme_enaddr[3] = (par0 >> 24) & 0xff;
		sc->jme_enaddr[4] = (par1 >> 0) & 0xff;
		sc->jme_enaddr[5] = (par1 >> 8) & 0xff;
	}
	return (0);
}

/*
 * Set up sysctl(3) MIB, hw.jme.* - Individual controllers will be
 * set up in jme_pci_attach()
 */
SYSCTL_SETUP(sysctl_jme, "sysctl jme subtree setup")
{
	int rc;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "jme",
	    SYSCTL_DESCR("jme interface controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	jme_root_num = node->sysctl_num;
	return;

err:
	aprint_error("%s: syctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
jme_sysctl_intrxto(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct jme_softc *sc;
	uint32_t reg;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->jme_intrxto;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < PCCRX_COAL_TO_MIN || t > PCCRX_COAL_TO_MAX)
		return EINVAL;

	/*
	 * update the softc with sysctl-changed value, and mark
	 * for hardware update
	 */
	sc->jme_intrxto = t;
	/* Configure Rx queue 0 packet completion coalescing. */
	reg = (sc->jme_intrxto << PCCRX_COAL_TO_SHIFT) & PCCRX_COAL_TO_MASK;
	reg |= (sc->jme_intrxct << PCCRX_COAL_PKT_SHIFT) & PCCRX_COAL_PKT_MASK;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCRX0, reg);
	return 0;
}

static int
jme_sysctl_intrxct(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct jme_softc *sc;
	uint32_t reg;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->jme_intrxct;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < PCCRX_COAL_PKT_MIN || t > PCCRX_COAL_PKT_MAX)
		return EINVAL;

	/*
	 * update the softc with sysctl-changed value, and mark
	 * for hardware update
	 */
	sc->jme_intrxct = t;
	/* Configure Rx queue 0 packet completion coalescing. */
	reg = (sc->jme_intrxto << PCCRX_COAL_TO_SHIFT) & PCCRX_COAL_TO_MASK;
	reg |= (sc->jme_intrxct << PCCRX_COAL_PKT_SHIFT) & PCCRX_COAL_PKT_MASK;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCRX0, reg);
	return 0;
}

static int
jme_sysctl_inttxto(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct jme_softc *sc;
	uint32_t reg;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->jme_inttxto;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < PCCTX_COAL_TO_MIN || t > PCCTX_COAL_TO_MAX)
		return EINVAL;

	/*
	 * update the softc with sysctl-changed value, and mark
	 * for hardware update
	 */
	sc->jme_inttxto = t;
	/* Configure Tx queue 0 packet completion coalescing. */
	reg = (sc->jme_inttxto << PCCTX_COAL_TO_SHIFT) & PCCTX_COAL_TO_MASK;
	reg |= (sc->jme_inttxct << PCCTX_COAL_PKT_SHIFT) & PCCTX_COAL_PKT_MASK;
	reg |= PCCTX_COAL_TXQ0;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCTX, reg);
	return 0;
}

static int
jme_sysctl_inttxct(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct jme_softc *sc;
	uint32_t reg;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->jme_inttxct;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < PCCTX_COAL_PKT_MIN || t > PCCTX_COAL_PKT_MAX)
		return EINVAL;

	/*
	 * update the softc with sysctl-changed value, and mark
	 * for hardware update
	 */
	sc->jme_inttxct = t;
	/* Configure Tx queue 0 packet completion coalescing. */
	reg = (sc->jme_inttxto << PCCTX_COAL_TO_SHIFT) & PCCTX_COAL_TO_MASK;
	reg |= (sc->jme_inttxct << PCCTX_COAL_PKT_SHIFT) & PCCTX_COAL_PKT_MASK;
	reg |= PCCTX_COAL_TXQ0;
	bus_space_write_4(sc->jme_bt_misc, sc->jme_bh_misc, JME_PCCTX, reg);
	return 0;
}
