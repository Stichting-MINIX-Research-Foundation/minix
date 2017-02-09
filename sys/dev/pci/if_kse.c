/*	$NetBSD: if_kse.c,v 1.28 2014/06/16 16:48:16 msaitoh Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
__KERNEL_RCSID(0, "$NetBSD: if_kse.c,v 1.28 2014/06/16 16:48:16 msaitoh Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <machine/endian.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define CSR_READ_4(sc, off) \
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off)
#define CSR_WRITE_4(sc, off, val) \
	    bus_space_write_4(sc->sc_st, sc->sc_sh, off, val)
#define CSR_READ_2(sc, off) \
	    bus_space_read_2(sc->sc_st, sc->sc_sh, off)
#define CSR_WRITE_2(sc, off, val) \
	    bus_space_write_2(sc->sc_st, sc->sc_sh, off, val)

#define MDTXC	0x000	/* DMA transmit control */
#define MDRXC	0x004	/* DMA receive control */
#define MDTSC	0x008	/* DMA transmit start */
#define MDRSC	0x00c	/* DMA receive start */
#define TDLB	0x010	/* transmit descriptor list base */
#define RDLB	0x014	/* receive descriptor list base */
#define MTR0	0x020	/* multicast table 31:0 */
#define MTR1	0x024	/* multicast table 63:32 */
#define INTEN	0x028	/* interrupt enable */
#define INTST	0x02c	/* interrupt status */
#define MARL	0x200	/* MAC address low */
#define MARM	0x202	/* MAC address middle */
#define MARH	0x204	/* MAC address high */
#define GRR	0x216	/* global reset */
#define CIDR	0x400	/* chip ID and enable */
#define CGCR	0x40a	/* chip global control */
#define IACR	0x4a0	/* indirect access control */
#define IADR1	0x4a2	/* indirect access data 66:63 */
#define IADR2	0x4a4	/* indirect access data 47:32 */
#define IADR3	0x4a6	/* indirect access data 63:48 */
#define IADR4	0x4a8	/* indirect access data 15:0 */
#define IADR5	0x4aa	/* indirect access data 31:16 */
#define P1CR4	0x512	/* port 1 control 4 */
#define P1SR	0x514	/* port 1 status */
#define P2CR4	0x532	/* port 2 control 4 */
#define P2SR	0x534	/* port 2 status */

#define TXC_BS_MSK	0x3f000000	/* burst size */
#define TXC_BS_SFT	(24)		/* 1,2,4,8,16,32 or 0 for unlimited */
#define TXC_UCG		(1U<<18)	/* generate UDP checksum */
#define TXC_TCG		(1U<<17)	/* generate TCP checksum */
#define TXC_ICG		(1U<<16)	/* generate IP checksum */
#define TXC_FCE		(1U<<9)		/* enable flowcontrol */
#define TXC_EP		(1U<<2)		/* enable automatic padding */
#define TXC_AC		(1U<<1)		/* add CRC to frame */
#define TXC_TEN		(1)		/* enable DMA to run */

#define RXC_BS_MSK	0x3f000000	/* burst size */
#define RXC_BS_SFT	(24)		/* 1,2,4,8,16,32 or 0 for unlimited */
#define RXC_IHAE	(1U<<19)	/* IP header alignment enable */
#define RXC_UCC		(1U<<18)	/* run UDP checksum */
#define RXC_TCC		(1U<<17)	/* run TDP checksum */
#define RXC_ICC		(1U<<16)	/* run IP checksum */
#define RXC_FCE		(1U<<9)		/* enable flowcontrol */
#define RXC_RB		(1U<<6)		/* receive broadcast frame */
#define RXC_RM		(1U<<5)		/* receive multicast frame */
#define RXC_RU		(1U<<4)		/* receive unicast frame */
#define RXC_RE		(1U<<3)		/* accept error frame */
#define RXC_RA		(1U<<2)		/* receive all frame */
#define RXC_MHTE	(1U<<1)		/* use multicast hash table */
#define RXC_REN		(1)		/* enable DMA to run */

#define INT_DMLCS	(1U<<31)	/* link status change */
#define INT_DMTS	(1U<<30)	/* sending desc. has posted Tx done */
#define INT_DMRS	(1U<<29)	/* frame was received */
#define INT_DMRBUS	(1U<<27)	/* Rx descriptor pool is full */

#define T0_OWN		(1U<<31)	/* desc is ready to Tx */

#define R0_OWN		(1U<<31)	/* desc is empty */
#define R0_FS		(1U<<30)	/* first segment of frame */
#define R0_LS		(1U<<29)	/* last segment of frame */
#define R0_IPE		(1U<<28)	/* IP checksum error */
#define R0_TCPE		(1U<<27)	/* TCP checksum error */
#define R0_UDPE		(1U<<26)	/* UDP checksum error */
#define R0_ES		(1U<<25)	/* error summary */
#define R0_MF		(1U<<24)	/* multicast frame */
#define R0_SPN		0x00300000	/* 21:20 switch port 1/2 */
#define R0_ALIGN	0x00300000	/* 21:20 (KSZ8692P) Rx align amount */
#define R0_RE		(1U<<19)	/* MII reported error */
#define R0_TL		(1U<<18)	/* frame too long, beyond 1518 */
#define R0_RF		(1U<<17)	/* damaged runt frame */
#define R0_CE		(1U<<16)	/* CRC error */
#define R0_FT		(1U<<15)	/* frame type */
#define R0_FL_MASK	0x7ff		/* frame length 10:0 */

#define T1_IC		(1U<<31)	/* post interrupt on complete */
#define T1_FS		(1U<<30)	/* first segment of frame */
#define T1_LS		(1U<<29)	/* last segment of frame */
#define T1_IPCKG	(1U<<28)	/* generate IP checksum */
#define T1_TCPCKG	(1U<<27)	/* generate TCP checksum */
#define T1_UDPCKG	(1U<<26)	/* generate UDP checksum */
#define T1_TER		(1U<<25)	/* end of ring */
#define T1_SPN		0x00300000	/* 21:20 switch port 1/2 */
#define T1_TBS_MASK	0x7ff		/* segment size 10:0 */

#define R1_RER		(1U<<25)	/* end of ring */
#define R1_RBS_MASK	0x7fc		/* segment size 10:0 */

#define KSE_NTXSEGS		16
#define KSE_TXQUEUELEN		64
#define KSE_TXQUEUELEN_MASK	(KSE_TXQUEUELEN - 1)
#define KSE_TXQUEUE_GC		(KSE_TXQUEUELEN / 4)
#define KSE_NTXDESC		256
#define KSE_NTXDESC_MASK	(KSE_NTXDESC - 1)
#define KSE_NEXTTX(x)		(((x) + 1) & KSE_NTXDESC_MASK)
#define KSE_NEXTTXS(x)		(((x) + 1) & KSE_TXQUEUELEN_MASK)

#define KSE_NRXDESC		64
#define KSE_NRXDESC_MASK	(KSE_NRXDESC - 1)
#define KSE_NEXTRX(x)		(((x) + 1) & KSE_NRXDESC_MASK)

struct tdes {
	uint32_t t0, t1, t2, t3;
};

struct rdes {
	uint32_t r0, r1, r2, r3;
};

struct kse_control_data {
	struct tdes kcd_txdescs[KSE_NTXDESC];
	struct rdes kcd_rxdescs[KSE_NRXDESC];
};
#define KSE_CDOFF(x)		offsetof(struct kse_control_data, x)
#define KSE_CDTXOFF(x)		KSE_CDOFF(kcd_txdescs[(x)])
#define KSE_CDRXOFF(x)		KSE_CDOFF(kcd_rxdescs[(x)])

struct kse_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndesc;			/* # of descriptors used */
};

struct kse_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

struct kse_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* Ethernet common data */
	void *sc_ih;			/* interrupt cookie */

	struct ifmedia sc_media;	/* ifmedia information */
	int sc_media_status;		/* PHY */
	int sc_media_active;		/* PHY */
	callout_t  sc_callout;		/* MII tick callout */
	callout_t  sc_stat_ch;		/* statistics counter callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	struct kse_control_data *sc_control_data;
#define sc_txdescs	sc_control_data->kcd_txdescs
#define sc_rxdescs	sc_control_data->kcd_rxdescs

	struct kse_txsoft sc_txsoft[KSE_TXQUEUELEN];
	struct kse_rxsoft sc_rxsoft[KSE_NRXDESC];
	int sc_txfree;			/* number of free Tx descriptors */
	int sc_txnext;			/* next ready Tx descriptor */
	int sc_txsfree;			/* number of free Tx jobs */
	int sc_txsnext;			/* next ready Tx job */
	int sc_txsdirty;		/* dirty Tx jobs */
	int sc_rxptr;			/* next ready Rx descriptor/descsoft */

	uint32_t sc_txc, sc_rxc;
	uint32_t sc_t1csum;
	int sc_mcsum;
	uint32_t sc_inten;

	uint32_t sc_chip;
	uint8_t sc_altmac[16][ETHER_ADDR_LEN];
	uint16_t sc_vlan[16];

#ifdef KSE_EVENT_COUNTERS
	struct ksext {
		char evcntname[3][8];
		struct evcnt pev[3][34];
	} sc_ext;			/* switch statistics */
#endif
};

#define KSE_CDTXADDR(sc, x)	((sc)->sc_cddma + KSE_CDTXOFF((x)))
#define KSE_CDRXADDR(sc, x)	((sc)->sc_cddma + KSE_CDRXOFF((x)))

#define KSE_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > KSE_NTXDESC) {				\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    KSE_CDTXOFF(__x), sizeof(struct tdes) *		\
		    (KSE_NTXDESC - __x), (ops));			\
		__n -= (KSE_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    KSE_CDTXOFF(__x), sizeof(struct tdes) * __n, (ops));	\
} while (/*CONSTCOND*/0)

#define KSE_CDRXSYNC(sc, x, ops)					\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    KSE_CDRXOFF((x)), sizeof(struct rdes), (ops));		\
} while (/*CONSTCOND*/0)

#define KSE_INIT_RXDESC(sc, x)						\
do {									\
	struct kse_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	struct rdes *__rxd = &(sc)->sc_rxdescs[(x)];			\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	__m->m_data = __m->m_ext.ext_buf;				\
	__rxd->r2 = __rxs->rxs_dmamap->dm_segs[0].ds_addr;		\
	__rxd->r1 = R1_RBS_MASK /* __m->m_ext.ext_size */;		\
	__rxd->r0 = R0_OWN;						\
	KSE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (/*CONSTCOND*/0)

u_int kse_burstsize = 8;	/* DMA burst length tuning knob */

#ifdef KSEDIAGNOSTIC
u_int kse_monitor_rxintr;	/* fragmented UDP csum HW bug hook */
#endif

static int kse_match(device_t, cfdata_t, void *);
static void kse_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(kse, sizeof(struct kse_softc),
    kse_match, kse_attach, NULL, NULL);

static int kse_ioctl(struct ifnet *, u_long, void *);
static void kse_start(struct ifnet *);
static void kse_watchdog(struct ifnet *);
static int kse_init(struct ifnet *);
static void kse_stop(struct ifnet *, int);
static void kse_reset(struct kse_softc *);
static void kse_set_filter(struct kse_softc *);
static int add_rxbuf(struct kse_softc *, int);
static void rxdrain(struct kse_softc *);
static int kse_intr(void *);
static void rxintr(struct kse_softc *);
static void txreap(struct kse_softc *);
static void lnkchg(struct kse_softc *);
static int ifmedia_upd(struct ifnet *);
static void ifmedia_sts(struct ifnet *, struct ifmediareq *);
static void phy_tick(void *);
static int ifmedia2_upd(struct ifnet *);
static void ifmedia2_sts(struct ifnet *, struct ifmediareq *);
#ifdef KSE_EVENT_COUNTERS
static void stat_tick(void *);
static void zerostats(struct kse_softc *);
#endif

static int
kse_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_MICREL &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_MICREL_KSZ8842 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_MICREL_KSZ8841) &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_NETWORK)
		return 1;

	return 0;
}

static void
kse_attach(device_t parent, device_t self, void *aux)
{
	struct kse_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	struct ifnet *ifp;
	struct ifmedia *ifm;
	uint8_t enaddr[ETHER_ADDR_LEN];
	bus_dma_segment_t seg;
	int i, error, nseg;
	pcireg_t pmode;
	int pmreg;
	char intrbuf[PCI_INTRSTR_LEN];

	if (pci_mapreg_map(pa, 0x10,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
	    0, &sc->sc_st, &sc->sc_sh, NULL, NULL) != 0) {
		printf(": unable to map device registers\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;

	/* Make sure bus mastering is enabled. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* Get it out of power save mode, if needed. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		pmode = pci_conf_read(pc, pa->pa_tag, pmreg + PCI_PMCSR) &
		    PCI_PMCSR_STATE_MASK;
		if (pmode == PCI_PMCSR_STATE_D3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			printf("%s: unable to wake from power state D3\n",
			    device_xname(sc->sc_dev));
			return;
		}
		if (pmode != PCI_PMCSR_STATE_D0) {
			printf("%s: waking up from power date D%d\n",
			    device_xname(sc->sc_dev), pmode);
			pci_conf_write(pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    PCI_PMCSR_STATE_D0);
		}
	}

	sc->sc_chip = PCI_PRODUCT(pa->pa_id);
	printf(": Micrel KSZ%04x Ethernet (rev. 0x%02x)\n",
	    sc->sc_chip, PCI_REVISION(pa->pa_class));

	/*
	 * Read the Ethernet address from the EEPROM.
	 */
	i = CSR_READ_2(sc, MARL);
	enaddr[5] = i; enaddr[4] = i >> 8;
	i = CSR_READ_2(sc, MARM);
	enaddr[3] = i; enaddr[2] = i >> 8;
	i = CSR_READ_2(sc, MARH);
	enaddr[1] = i; enaddr[0] = i >> 8;
	printf("%s: Ethernet address: %s\n",
		device_xname(sc->sc_dev), ether_sprintf(enaddr));

	/*
	 * Enable chip function.
	 */
	CSR_WRITE_2(sc, CIDR, 1);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, kse_intr, sc);
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
	error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct kse_control_data), PAGE_SIZE, 0, &seg, 1, &nseg, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control data, error = %d\n", error);
		goto fail_0;
	}
	error = bus_dmamem_map(sc->sc_dmat, &seg, nseg,
	    sizeof(struct kse_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control data, error = %d\n", error);
		goto fail_1;
	}
	error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct kse_control_data), 1,
	    sizeof(struct kse_control_data), 0, 0, &sc->sc_cddmamap);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct kse_control_data), NULL, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}
	for (i = 0; i < KSE_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    KSE_NTXSEGS, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create tx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_4;
		}
	}
	for (i = 0; i < KSE_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    1, MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create rx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	callout_init(&sc->sc_callout, 0);
	callout_init(&sc->sc_stat_ch, 0);

	ifm = &sc->sc_media;
	if (sc->sc_chip == 0x8841) {
		ifmedia_init(ifm, 0, ifmedia_upd, ifmedia_sts);
		ifmedia_add(ifm, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(ifm, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(ifm, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(ifm, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(ifm, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(ifm, IFM_ETHER|IFM_AUTO);
	}
	else {
		ifmedia_init(ifm, 0, ifmedia2_upd, ifmedia2_sts);
		ifmedia_add(ifm, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(ifm, IFM_ETHER|IFM_AUTO);
	}

	printf("%s: 10baseT, 10baseT-FDX, 100baseTX, 100baseTX-FDX, auto\n",
	    device_xname(sc->sc_dev));

	ifp = &sc->sc_ethercom.ec_if;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kse_ioctl;
	ifp->if_start = kse_start;
	ifp->if_watchdog = kse_watchdog;
	ifp->if_init = kse_init;
	ifp->if_stop = kse_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * KSZ8842 can handle 802.1Q VLAN-sized frames,
	 * can do IPv4, TCPv4, and UDPv4 checksums in hardware.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;

	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

#ifdef KSE_EVENT_COUNTERS
	int p = (sc->sc_chip == 0x8842) ? 3 : 1;
	for (i = 0; i < p; i++) {
		struct ksext *ee = &sc->sc_ext;
		snprintf(ee->evcntname[i], sizeof(ee->evcntname[i]),
		    "%s.%d", device_xname(sc->sc_dev), i+1);
		evcnt_attach_dynamic(&ee->pev[i][0], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxLoPriotyByte");
		evcnt_attach_dynamic(&ee->pev[i][1], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxHiPriotyByte");
		evcnt_attach_dynamic(&ee->pev[i][2], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxUndersizePkt");
		evcnt_attach_dynamic(&ee->pev[i][3], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxFragments");
		evcnt_attach_dynamic(&ee->pev[i][4], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxOversize");
		evcnt_attach_dynamic(&ee->pev[i][5], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxJabbers");
		evcnt_attach_dynamic(&ee->pev[i][6], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxSymbolError");
		evcnt_attach_dynamic(&ee->pev[i][7], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxCRCError");
		evcnt_attach_dynamic(&ee->pev[i][8], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxAlignmentError");
		evcnt_attach_dynamic(&ee->pev[i][9], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxControl8808Pkts");
		evcnt_attach_dynamic(&ee->pev[i][10], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxPausePkts");
		evcnt_attach_dynamic(&ee->pev[i][11], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxBroadcast");
		evcnt_attach_dynamic(&ee->pev[i][12], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxMulticast");
		evcnt_attach_dynamic(&ee->pev[i][13], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxUnicast");
		evcnt_attach_dynamic(&ee->pev[i][14], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx64Octets");
		evcnt_attach_dynamic(&ee->pev[i][15], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx65To127Octets");
		evcnt_attach_dynamic(&ee->pev[i][16], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx128To255Octets");
		evcnt_attach_dynamic(&ee->pev[i][17], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx255To511Octets");
		evcnt_attach_dynamic(&ee->pev[i][18], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx512To1023Octets");
		evcnt_attach_dynamic(&ee->pev[i][19], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "Rx1024To1522Octets");
		evcnt_attach_dynamic(&ee->pev[i][20], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxLoPriotyByte");
		evcnt_attach_dynamic(&ee->pev[i][21], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxHiPriotyByte");
		evcnt_attach_dynamic(&ee->pev[i][22], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxLateCollision");
		evcnt_attach_dynamic(&ee->pev[i][23], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxPausePkts");
		evcnt_attach_dynamic(&ee->pev[i][24], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxBroadcastPkts");
		evcnt_attach_dynamic(&ee->pev[i][25], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxMulticastPkts");
		evcnt_attach_dynamic(&ee->pev[i][26], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxUnicastPkts");
		evcnt_attach_dynamic(&ee->pev[i][27], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxDeferred");
		evcnt_attach_dynamic(&ee->pev[i][28], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxTotalCollision");
		evcnt_attach_dynamic(&ee->pev[i][29], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxExcessiveCollision");
		evcnt_attach_dynamic(&ee->pev[i][30], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxSingleCollision");
		evcnt_attach_dynamic(&ee->pev[i][31], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxMultipleCollision");
		evcnt_attach_dynamic(&ee->pev[i][32], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "TxDropPkts");
		evcnt_attach_dynamic(&ee->pev[i][33], EVCNT_TYPE_MISC,
		    NULL, ee->evcntname[i], "RxDropPkts");
	}
#endif
	return;

 fail_5:
	for (i = 0; i < KSE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < KSE_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct kse_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, nseg);
 fail_0:
	return;
}

static int
kse_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct kse_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCSIFCAP)
			error = (*ifp->if_init)(ifp);
		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			kse_set_filter(sc);
		}
		break;
	}

	kse_start(ifp);

	splx(s);
	return error;
}

static int
kse_init(struct ifnet *ifp)
{
	struct kse_softc *sc = ifp->if_softc;
	uint32_t paddr;
	int i, error = 0;

	/* cancel pending I/O */
	kse_stop(ifp, 0);

	/* reset all registers but PCI configuration */
	kse_reset(sc);

	/* craft Tx descriptor ring */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0, paddr = KSE_CDTXADDR(sc, 1); i < KSE_NTXDESC - 1; i++) {
		sc->sc_txdescs[i].t3 = paddr;
		paddr += sizeof(struct tdes);
	}
	sc->sc_txdescs[KSE_NTXDESC - 1].t3 = KSE_CDTXADDR(sc, 0);
	KSE_CDTXSYNC(sc, 0, KSE_NTXDESC,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = KSE_NTXDESC;
	sc->sc_txnext = 0;

	for (i = 0; i < KSE_TXQUEUELEN; i++)
		sc->sc_txsoft[i].txs_mbuf = NULL;
	sc->sc_txsfree = KSE_TXQUEUELEN;
	sc->sc_txsnext = 0;
	sc->sc_txsdirty = 0;

	/* craft Rx descriptor ring */
	memset(sc->sc_rxdescs, 0, sizeof(sc->sc_rxdescs));
	for (i = 0, paddr = KSE_CDRXADDR(sc, 1); i < KSE_NRXDESC - 1; i++) {
		sc->sc_rxdescs[i].r3 = paddr;
		paddr += sizeof(struct rdes);
	}
	sc->sc_rxdescs[KSE_NRXDESC - 1].r3 = KSE_CDRXADDR(sc, 0);
	for (i = 0; i < KSE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_mbuf == NULL) {
			if ((error = add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				     device_xname(sc->sc_dev), i, error);
				rxdrain(sc);
				goto out;
			}
		}
		else
			KSE_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/* hand Tx/Rx rings to HW */
	CSR_WRITE_4(sc, TDLB, KSE_CDTXADDR(sc, 0));
	CSR_WRITE_4(sc, RDLB, KSE_CDRXADDR(sc, 0));

	sc->sc_txc = TXC_TEN | TXC_EP | TXC_AC | TXC_FCE;
	sc->sc_rxc = RXC_REN | RXC_RU | RXC_FCE;
	if (ifp->if_flags & IFF_PROMISC)
		sc->sc_rxc |= RXC_RA;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rxc |= RXC_RB;
	sc->sc_t1csum = sc->sc_mcsum = 0;
	if (ifp->if_capenable & IFCAP_CSUM_IPv4_Rx) {
		sc->sc_rxc |= RXC_ICC;
		sc->sc_mcsum |= M_CSUM_IPv4;
	}
	if (ifp->if_capenable & IFCAP_CSUM_IPv4_Tx) {
		sc->sc_txc |= TXC_ICG;
		sc->sc_t1csum |= T1_IPCKG;
	}
	if (ifp->if_capenable & IFCAP_CSUM_TCPv4_Rx) {
		sc->sc_rxc |= RXC_TCC;
		sc->sc_mcsum |= M_CSUM_TCPv4;
	}
	if (ifp->if_capenable & IFCAP_CSUM_TCPv4_Tx) {
		sc->sc_txc |= TXC_TCG;
		sc->sc_t1csum |= T1_TCPCKG;
	}
	if (ifp->if_capenable & IFCAP_CSUM_UDPv4_Rx) {
		sc->sc_rxc |= RXC_UCC;
		sc->sc_mcsum |= M_CSUM_UDPv4;
	}
	if (ifp->if_capenable & IFCAP_CSUM_UDPv4_Tx) {
		sc->sc_txc |= TXC_UCG;
		sc->sc_t1csum |= T1_UDPCKG;
	}
	sc->sc_txc |= (kse_burstsize << TXC_BS_SFT);
	sc->sc_rxc |= (kse_burstsize << RXC_BS_SFT);

	/* build multicast hash filter if necessary */
	kse_set_filter(sc);

	/* set current media */
	(void)ifmedia_upd(ifp);

	/* enable transmitter and receiver */
	CSR_WRITE_4(sc, MDTXC, sc->sc_txc);
	CSR_WRITE_4(sc, MDRXC, sc->sc_rxc);
	CSR_WRITE_4(sc, MDRSC, 1);

	/* enable interrupts */
	sc->sc_inten = INT_DMTS|INT_DMRS|INT_DMRBUS;
	if (sc->sc_chip == 0x8841)
		sc->sc_inten |= INT_DMLCS;
	CSR_WRITE_4(sc, INTST, ~0);
	CSR_WRITE_4(sc, INTEN, sc->sc_inten);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->sc_chip == 0x8841) {
		/* start one second timer */
		callout_reset(&sc->sc_callout, hz, phy_tick, sc);
	}
#ifdef KSE_EVENT_COUNTERS
	/* start statistics gather 1 minute timer */
	zerostats(sc);
	callout_reset(&sc->sc_stat_ch, hz * 60, stat_tick, sc);
#endif

 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	}
	return error;
}

static void
kse_stop(struct ifnet *ifp, int disable)
{
	struct kse_softc *sc = ifp->if_softc;
	struct kse_txsoft *txs;
	int i;

	if (sc->sc_chip == 0x8841)
		callout_stop(&sc->sc_callout);
	callout_stop(&sc->sc_stat_ch);

	sc->sc_txc &= ~TXC_TEN;
	sc->sc_rxc &= ~RXC_REN;
	CSR_WRITE_4(sc, MDTXC, sc->sc_txc);
	CSR_WRITE_4(sc, MDRXC, sc->sc_rxc);

	for (i = 0; i < KSE_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		rxdrain(sc);
}

static void
kse_reset(struct kse_softc *sc)
{

	CSR_WRITE_2(sc, GRR, 1);
	delay(1000); /* PDF does not mention the delay amount */
	CSR_WRITE_2(sc, GRR, 0);

	CSR_WRITE_2(sc, CIDR, 1);
}

static void
kse_watchdog(struct ifnet *ifp)
{
	struct kse_softc *sc = ifp->if_softc;

	/*
	 * Since we're not interrupting every packet, sweep
	 * up before we report an error.
	 */
	txreap(sc);

	if (sc->sc_txfree != KSE_NTXDESC) {
		printf("%s: device timeout (txfree %d txsfree %d txnext %d)\n",
		    device_xname(sc->sc_dev), sc->sc_txfree, sc->sc_txsfree,
		    sc->sc_txnext);
		ifp->if_oerrors++;

		/* Reset the interface. */
		kse_init(ifp);
	}
	else if (ifp->if_flags & IFF_DEBUG)
		printf("%s: recovered from device timeout\n",
		    device_xname(sc->sc_dev));

	/* Try to get more packets going. */
	kse_start(ifp);
}

static void
kse_start(struct ifnet *ifp)
{
	struct kse_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct kse_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx, ofree, seg;
	uint32_t tdes0;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors.
	 */
	ofree = sc->sc_txfree;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->sc_txsfree < KSE_TXQUEUE_GC) {
			txreap(sc);
			if (sc->sc_txsfree == 0)
				break;
		}
		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				printf("%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    device_xname(sc->sc_dev));
				    IFQ_DEQUEUE(&ifp->if_snd, m0);
				    m_freem(m0);
				    continue;
			}
			/* Short on resources, just stop for now. */
			break;
		}

		if (dmamap->dm_nsegs > sc->sc_txfree) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.	 Notify the upper
			 * layer that there are not more slots left.
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		lasttx = -1; tdes0 = 0;
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = KSE_NEXTTX(nexttx)) {
			struct tdes *tdes = &sc->sc_txdescs[nexttx];
			/*
			 * If this is the first descriptor we're
			 * enqueueing, don't set the OWN bit just
			 * yet.	 That could cause a race condition.
			 * We'll do it below.
			 */
			tdes->t2 = dmamap->dm_segs[seg].ds_addr;
			tdes->t1 = sc->sc_t1csum
			     | (dmamap->dm_segs[seg].ds_len & T1_TBS_MASK);
			tdes->t0 = tdes0;
			tdes0 |= T0_OWN;
			lasttx = nexttx;
		}

		/*
		 * Outgoing NFS mbuf must be unloaded when Tx completed.
		 * Without T1_IC NFS mbuf is left unack'ed for excessive
		 * time and NFS stops to proceed until kse_watchdog()
		 * calls txreap() to reclaim the unack'ed mbuf.
		 * It's painful to traverse every mbuf chain to determine
		 * whether someone is waiting for Tx completion.
		 */
		m = m0;
		do {
			if ((m->m_flags & M_EXT) && m->m_ext.ext_free) {
				sc->sc_txdescs[lasttx].t1 |= T1_IC;
				break;
			}
		} while ((m = m->m_next) != NULL);

		/* write last T0_OWN bit of the 1st segment */
		sc->sc_txdescs[lasttx].t1 |= T1_LS;
		sc->sc_txdescs[sc->sc_txnext].t1 |= T1_FS;
		sc->sc_txdescs[sc->sc_txnext].t0 = T0_OWN;
		KSE_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* tell DMA start transmit */
		CSR_WRITE_4(sc, MDTSC, 1);

		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;
		txs->txs_ndesc = dmamap->dm_nsegs;

		sc->sc_txfree -= txs->txs_ndesc;
		sc->sc_txnext = nexttx;
		sc->sc_txsfree--;
		sc->sc_txsnext = KSE_NEXTTXS(sc->sc_txsnext);
		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (sc->sc_txsfree == 0 || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}
	if (sc->sc_txfree != ofree) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

static void
kse_set_filter(struct kse_softc *sc)
{
	struct ether_multistep step;
	struct ether_multi *enm;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t h, hashes[2];

	sc->sc_rxc &= ~(RXC_MHTE | RXC_RM);
	ifp->if_flags &= ~IFF_ALLMULTI;
	if (ifp->if_flags & IFF_PROMISC)
		return;

	ETHER_FIRST_MULTI(step, &sc->sc_ethercom, enm);
	if (enm == NULL)
		return;
	hashes[0] = hashes[1] = 0;
	do {
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
		h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;
		hashes[h >> 5] |= 1 << (h & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
	} while (enm != NULL);
	sc->sc_rxc |= RXC_MHTE;
	CSR_WRITE_4(sc, MTR0, hashes[0]);
	CSR_WRITE_4(sc, MTR1, hashes[1]);
	return;
 allmulti:
	sc->sc_rxc |= RXC_RM;
	ifp->if_flags |= IFF_ALLMULTI;
}

static int
add_rxbuf(struct kse_softc *sc, int idx)
{
	struct kse_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("kse_add_rxbuf");
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	KSE_INIT_RXDESC(sc, idx);

	return 0;
}

static void
rxdrain(struct kse_softc *sc)
{
	struct kse_rxsoft *rxs;
	int i;

	for (i = 0; i < KSE_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

static int
kse_intr(void *arg)
{
	struct kse_softc *sc = arg;
	uint32_t isr;

	if ((isr = CSR_READ_4(sc, INTST)) == 0)
		return 0;

	if (isr & INT_DMRS)
		rxintr(sc);
	if (isr & INT_DMTS)
		txreap(sc);
	if (isr & INT_DMLCS)
		lnkchg(sc);
	if (isr & INT_DMRBUS)
		printf("%s: Rx descriptor full\n", device_xname(sc->sc_dev));

	CSR_WRITE_4(sc, INTST, isr);
	return 1;
}

static void
rxintr(struct kse_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct kse_rxsoft *rxs;
	struct mbuf *m;
	uint32_t rxstat;
	int i, len;

	for (i = sc->sc_rxptr; /*CONSTCOND*/ 1; i = KSE_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		KSE_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = sc->sc_rxdescs[i].r0;
	
		if (rxstat & R0_OWN) /* desc is left empty */
			break;

		/* R0_FS|R0_LS must have been marked for this desc */

		if (rxstat & R0_ES) {
			ifp->if_ierrors++;
#define PRINTERR(bit, str)						\
			if (rxstat & (bit))				\
				printf("%s: receive error: %s\n",	\
				    device_xname(sc->sc_dev), str)
			PRINTERR(R0_TL, "frame too long");
			PRINTERR(R0_RF, "runt frame");
			PRINTERR(R0_CE, "bad FCS");
#undef PRINTERR
			KSE_INIT_RXDESC(sc, i);
			continue;
		}

		/* HW errata; frame might be too small or too large */

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		len = rxstat & R0_FL_MASK;
		len -= ETHER_CRC_LEN;	/* trim CRC off */
		m = rxs->rxs_mbuf;

		if (add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			KSE_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat,
			    rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		if (sc->sc_mcsum) {
			m->m_pkthdr.csum_flags |= sc->sc_mcsum;
			if (rxstat & R0_IPE)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
			if (rxstat & (R0_TCPE | R0_UDPE))
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}
		bpf_mtap(ifp, m);
		(*ifp->if_input)(ifp, m);
#ifdef KSEDIAGNOSTIC
		if (kse_monitor_rxintr > 0) {
			printf("m stat %x data %p len %d\n",
			    rxstat, m->m_data, m->m_len);
		}
#endif
	}
	sc->sc_rxptr = i;
}

static void
txreap(struct kse_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct kse_txsoft *txs;
	uint32_t txstat;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	for (i = sc->sc_txsdirty; sc->sc_txsfree != KSE_TXQUEUELEN;
	     i = KSE_NEXTTXS(i), sc->sc_txsfree++) {
		txs = &sc->sc_txsoft[i];

		KSE_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_ndesc,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		txstat = sc->sc_txdescs[txs->txs_lastdesc].t0;

		if (txstat & T0_OWN) /* desc is still in use */
			break;

		/* there is no way to tell transmission status per frame */

		ifp->if_opackets++;

		sc->sc_txfree += txs->txs_ndesc;
		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
	}
	sc->sc_txsdirty = i;
	if (sc->sc_txsfree == KSE_TXQUEUELEN)
		ifp->if_timer = 0;
}

static void
lnkchg(struct kse_softc *sc)
{
	struct ifmediareq ifmr;

#if 0 /* rambling link status */
	printf("%s: link %s\n", device_xname(sc->sc_dev),
	    (CSR_READ_2(sc, P1SR) & (1U << 5)) ? "up" : "down");
#endif
	ifmedia_sts(&sc->sc_ethercom.ec_if, &ifmr);
}

static int
ifmedia_upd(struct ifnet *ifp)
{
	struct kse_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_media;
	uint16_t ctl;

	ctl = 0;
	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		ctl |= (1U << 13); /* restart AN */
		ctl |= (1U << 7);  /* enable AN */
		ctl |= (1U << 4);  /* advertise flow control pause */
		ctl |= (1U << 3) | (1U << 2) | (1U << 1) | (1U << 0);
	}
	else {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX)
			ctl |= (1U << 6);
		if (ifm->ifm_media & IFM_FDX)
			ctl |= (1U << 5);
	}
	CSR_WRITE_2(sc, P1CR4, ctl);

	sc->sc_media_active = IFM_NONE;
	sc->sc_media_status = IFM_AVALID;

	return 0;
}

static void
ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct kse_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_media;
	uint16_t ctl, sts, result;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	ctl = CSR_READ_2(sc, P1CR4);
	sts = CSR_READ_2(sc, P1SR);
	if ((sts & (1U << 5)) == 0) {
		ifmr->ifm_active |= IFM_NONE;
		goto out; /* link is down */
	}
	ifmr->ifm_status |= IFM_ACTIVE;
	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		if ((sts & (1U << 6)) == 0) {
			ifmr->ifm_active |= IFM_NONE;
			goto out; /* negotiation in progress */
		}
		result = ctl & sts & 017;
		if (result & (1U << 3))
			ifmr->ifm_active |= IFM_100_TX|IFM_FDX;
		else if (result & (1U << 2))
			ifmr->ifm_active |= IFM_100_TX|IFM_HDX;
		else if (result & (1U << 1))
			ifmr->ifm_active |= IFM_10_T|IFM_FDX;
		else if (result & (1U << 0))
			ifmr->ifm_active |= IFM_10_T|IFM_HDX;
		else
			ifmr->ifm_active |= IFM_NONE;
		if (ctl & (1U << 4))
			ifmr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE;
		if (sts & (1U << 4))
			ifmr->ifm_active |= IFM_FLOW | IFM_ETH_TXPAUSE;
	}
	else {
		ifmr->ifm_active |= (sts & (1U << 10)) ? IFM_100_TX : IFM_10_T;
		if (sts & (1U << 9))
			ifmr->ifm_active |= IFM_FDX;
		if (sts & (1U << 12))
			ifmr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE;
		if (sts & (1U << 11))
			ifmr->ifm_active |= IFM_FLOW | IFM_ETH_TXPAUSE;
	}

  out:
	sc->sc_media_status = ifmr->ifm_status;
	sc->sc_media_active = ifmr->ifm_active;
}

static void
phy_tick(void *arg)
{
	struct kse_softc *sc = arg;
	struct ifmediareq ifmr;
	int s;

	s = splnet();
	ifmedia_sts(&sc->sc_ethercom.ec_if, &ifmr);
	splx(s);

	callout_reset(&sc->sc_callout, hz, phy_tick, sc);
}

static int
ifmedia2_upd(struct ifnet *ifp)
{
	struct kse_softc *sc = ifp->if_softc;

	sc->sc_media_status = IFM_AVALID;
	sc->sc_media_active = IFM_NONE;
	return 0;
}

static void
ifmedia2_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct kse_softc *sc = ifp->if_softc;
	int p1sts, p2sts;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;
	p1sts = CSR_READ_2(sc, P1SR);
	p2sts = CSR_READ_2(sc, P2SR);
	if (((p1sts | p2sts) & (1U << 5)) == 0)
		ifmr->ifm_active |= IFM_NONE;
	else {
		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_100_TX|IFM_FDX;
		ifmr->ifm_active |= IFM_FLOW|IFM_ETH_RXPAUSE|IFM_ETH_TXPAUSE;
	}
	sc->sc_media_status = ifmr->ifm_status;
	sc->sc_media_active = ifmr->ifm_active;
}

#ifdef KSE_EVENT_COUNTERS
static void
stat_tick(void *arg)
{
	struct kse_softc *sc = arg;
	struct ksext *ee = &sc->sc_ext;
	int nport, p, i, val;

	nport = (sc->sc_chip == 0x8842) ? 3 : 1;
	for (p = 0; p < nport; p++) {
		for (i = 0; i < 32; i++) {
			val = 0x1c00 | (p * 0x20 + i);
			CSR_WRITE_2(sc, IACR, val);
			do {
				val = CSR_READ_2(sc, IADR5) << 16;
			} while ((val & (1U << 30)) == 0);
			if (val & (1U << 31)) {
				(void)CSR_READ_2(sc, IADR4);
				val = 0x3fffffff; /* has made overflow */
			}
			else {
				val &= 0x3fff0000;		/* 29:16 */
				val |= CSR_READ_2(sc, IADR4);	/* 15:0 */
			}
			ee->pev[p][i].ev_count += val; /* i (0-31) */
		}
		CSR_WRITE_2(sc, IACR, 0x1c00 + 0x100 + p);
		ee->pev[p][32].ev_count = CSR_READ_2(sc, IADR4); /* 32 */
		CSR_WRITE_2(sc, IACR, 0x1c00 + 0x100 + p * 3 + 1);
		ee->pev[p][33].ev_count = CSR_READ_2(sc, IADR4); /* 33 */
	}
	callout_reset(&sc->sc_stat_ch, hz * 60, stat_tick, arg);
}

static void
zerostats(struct kse_softc *sc)
{
	struct ksext *ee = &sc->sc_ext;
	int nport, p, i, val;

	/* make sure all the HW counters get zero */
	nport = (sc->sc_chip == 0x8842) ? 3 : 1;
	for (p = 0; p < nport; p++) {
		for (i = 0; i < 31; i++) {
			val = 0x1c00 | (p * 0x20 + i);
			CSR_WRITE_2(sc, IACR, val);
			do {
				val = CSR_READ_2(sc, IADR5) << 16;
			} while ((val & (1U << 30)) == 0);
			(void)CSR_READ_2(sc, IADR4);
			ee->pev[p][i].ev_count = 0;
		}
	}
}
#endif
