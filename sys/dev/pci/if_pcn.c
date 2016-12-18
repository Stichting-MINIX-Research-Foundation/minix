/*	$NetBSD: if_pcn.c,v 1.60 2015/04/27 17:40:13 christos Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for the AMD PCnet-PCI series of Ethernet
 * chips:
 *
 *	* Am79c970 PCnet-PCI Single-Chip Ethernet Controller for PCI
 *	  Local Bus
 *
 *	* Am79c970A PCnet-PCI II Single-Chip Full-Duplex Ethernet Controller
 *	  for PCI Local Bus
 *
 *	* Am79c971 PCnet-FAST Single-Chip Full-Duplex 10/100Mbps
 *	  Ethernet Controller for PCI Local Bus
 *
 *	* Am79c972 PCnet-FAST+ Enhanced 10/100Mbps PCI Ethernet Controller
 *	  with OnNow Support
 *
 *	* Am79c973/Am79c975 PCnet-FAST III Single-Chip 10/100Mbps PCI
 *	  Ethernet Controller with Integrated PHY
 *
 * This also supports the virtual PCnet-PCI Ethernet interface found
 * in VMware.
 *
 * TODO:
 *
 *	* Split this into bus-specific and bus-independent portions.
 *	  The core could also be used for the ILACC (Am79900) 32-bit
 *	  Ethernet chip (XXX only if we use an ILACC-compatible SWSTYLE).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_pcn.c,v 1.60 2015/04/27 17:40:13 christos Exp $");

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

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/am79900reg.h>
#include <dev/ic/lancereg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_pcnreg.h>

/*
 * Transmit descriptor list size.  This is arbitrary, but allocate
 * enough descriptors for 128 pending transmissions, and 4 segments
 * per packet.  This MUST work out to a power of 2.
 *
 * NOTE: We can't have any more than 512 Tx descriptors, SO BE CAREFUL!
 *
 * So we play a little trick here.  We give each packet up to 16
 * DMA segments, but only allocate the max of 512 descriptors.  The
 * transmit logic can deal with this, we just are hoping to sneak by.
 */
#define	PCN_NTXSEGS		16
#define	PCN_NTXSEGS_VMWARE	8	/* bug in VMware's emulation */

#define	PCN_TXQUEUELEN		128
#define	PCN_TXQUEUELEN_MASK	(PCN_TXQUEUELEN - 1)
#define	PCN_NTXDESC		512
#define	PCN_NTXDESC_MASK	(PCN_NTXDESC - 1)
#define	PCN_NEXTTX(x)		(((x) + 1) & PCN_NTXDESC_MASK)
#define	PCN_NEXTTXS(x)		(((x) + 1) & PCN_TXQUEUELEN_MASK)

/* Tx interrupt every N + 1 packets. */
#define	PCN_TXINTR_MASK		7

/*
 * Receive descriptor list size.  We have one Rx buffer per incoming
 * packet, so this logic is a little simpler.
 */
#define	PCN_NRXDESC		128
#define	PCN_NRXDESC_MASK	(PCN_NRXDESC - 1)
#define	PCN_NEXTRX(x)		(((x) + 1) & PCN_NRXDESC_MASK)

/*
 * Control structures are DMA'd to the PCnet chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct pcn_control_data {
	/* The transmit descriptors. */
	struct letmd pcd_txdescs[PCN_NTXDESC];

	/* The receive descriptors. */
	struct lermd pcd_rxdescs[PCN_NRXDESC];

	/* The init block. */
	struct leinit pcd_initblock;
};

#define	PCN_CDOFF(x)	offsetof(struct pcn_control_data, x)
#define	PCN_CDTXOFF(x)	PCN_CDOFF(pcd_txdescs[(x)])
#define	PCN_CDRXOFF(x)	PCN_CDOFF(pcd_rxdescs[(x)])
#define	PCN_CDINITOFF	PCN_CDOFF(pcd_initblock)

/*
 * Software state for transmit jobs.
 */
struct pcn_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
};

/*
 * Software state for receive jobs.
 */
struct pcn_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

/*
 * Description of Rx FIFO watermarks for various revisions.
 */
static const char * const pcn_79c970_rcvfw[] = {
	"16 bytes",
	"64 bytes",
	"128 bytes",
	NULL,
};

static const char * const pcn_79c971_rcvfw[] = {
	"16 bytes",
	"64 bytes",
	"112 bytes",
	NULL,
};

/*
 * Description of Tx start points for various revisions.
 */
static const char * const pcn_79c970_xmtsp[] = {
	"8 bytes",
	"64 bytes",
	"128 bytes",
	"248 bytes",
};

static const char * const pcn_79c971_xmtsp[] = {
	"20 bytes",
	"64 bytes",
	"128 bytes",
	"248 bytes",
};

static const char * const pcn_79c971_xmtsp_sram[] = {
	"44 bytes",
	"64 bytes",
	"128 bytes",
	"store-and-forward",
};

/*
 * Description of Tx FIFO watermarks for various revisions.
 */
static const char * const pcn_79c970_xmtfw[] = {
	"16 bytes",
	"64 bytes",
	"128 bytes",
	NULL,
};

static const char * const pcn_79c971_xmtfw[] = {
	"16 bytes",
	"64 bytes",
	"108 bytes",
	NULL,
};

/*
 * Software state per device.
 */
struct pcn_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* Ethernet common data */

	/* Points to our media routines, etc. */
	const struct pcn_variant *sc_variant;

	void *sc_ih;			/* interrupt cookie */

	struct mii_data sc_mii;		/* MII/media information */

	callout_t sc_tick_ch;		/* tick callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/* Software state for transmit and receive descriptors. */
	struct pcn_txsoft sc_txsoft[PCN_TXQUEUELEN];
	struct pcn_rxsoft sc_rxsoft[PCN_NRXDESC];

	/* Control data structures */
	struct pcn_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->pcd_txdescs
#define	sc_rxdescs	sc_control_data->pcd_rxdescs
#define	sc_initblock	sc_control_data->pcd_initblock

#ifdef PCN_EVENT_COUNTERS
	/* Event counters. */
	struct evcnt sc_ev_txsstall;	/* Tx stalled due to no txs */
	struct evcnt sc_ev_txdstall;	/* Tx stalled due to no txd */
	struct evcnt sc_ev_txintr;	/* Tx interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_babl;	/* BABL in pcn_intr() */
	struct evcnt sc_ev_miss;	/* MISS in pcn_intr() */
	struct evcnt sc_ev_merr;	/* MERR in pcn_intr() */

	struct evcnt sc_ev_txseg1;	/* Tx packets w/ 1 segment */
	struct evcnt sc_ev_txseg2;	/* Tx packets w/ 2 segments */
	struct evcnt sc_ev_txseg3;	/* Tx packets w/ 3 segments */
	struct evcnt sc_ev_txseg4;	/* Tx packets w/ 4 segments */
	struct evcnt sc_ev_txseg5;	/* Tx packets w/ 5 segments */
	struct evcnt sc_ev_txsegmore;	/* Tx packets w/ more than 5 segments */
	struct evcnt sc_ev_txcopy;	/* Tx copies required */
#endif /* PCN_EVENT_COUNTERS */

	const char * const *sc_rcvfw_desc;	/* Rx FIFO watermark info */
	int sc_rcvfw;

	const char * const *sc_xmtsp_desc;	/* Tx start point info */
	int sc_xmtsp;

	const char * const *sc_xmtfw_desc;	/* Tx FIFO watermark info */
	int sc_xmtfw;

	int sc_flags;			/* misc. flags; see below */
	int sc_swstyle;			/* the software style in use */

	int sc_txfree;			/* number of free Tx descriptors */
	int sc_txnext;			/* next ready Tx descriptor */

	int sc_txsfree;			/* number of free Tx jobs */
	int sc_txsnext;			/* next free Tx job */
	int sc_txsdirty;		/* dirty Tx jobs */

	int sc_rxptr;			/* next ready Rx descriptor/job */

	uint32_t sc_csr5;		/* prototype CSR5 register */
	uint32_t sc_mode;		/* prototype MODE register */

	krndsource_t rnd_source;	/* random source */
};

/* sc_flags */
#define	PCN_F_HAS_MII		0x0001	/* has MII */

#ifdef PCN_EVENT_COUNTERS
#define	PCN_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	PCN_EVCNT_INCR(ev)	/* nothing */
#endif

#define	PCN_CDTXADDR(sc, x)	((sc)->sc_cddma + PCN_CDTXOFF((x)))
#define	PCN_CDRXADDR(sc, x)	((sc)->sc_cddma + PCN_CDRXOFF((x)))
#define	PCN_CDINITADDR(sc)	((sc)->sc_cddma + PCN_CDINITOFF)

#define	PCN_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > PCN_NTXDESC) {				\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    PCN_CDTXOFF(__x), sizeof(struct letmd) *		\
		    (PCN_NTXDESC - __x), (ops));			\
		__n -= (PCN_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    PCN_CDTXOFF(__x), sizeof(struct letmd) * __n, (ops));	\
} while (/*CONSTCOND*/0)

#define	PCN_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    PCN_CDRXOFF((x)), sizeof(struct lermd), (ops))

#define	PCN_CDINITSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    PCN_CDINITOFF, sizeof(struct leinit), (ops))

#define	PCN_INIT_RXDESC(sc, x)						\
do {									\
	struct pcn_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	struct lermd *__rmd = &(sc)->sc_rxdescs[(x)];			\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 */								\
	__m->m_data = __m->m_ext.ext_buf + 2;				\
									\
	if ((sc)->sc_swstyle == LE_B20_SSTYLE_PCNETPCI3) {		\
		__rmd->rmd2 =						\
		    htole32(__rxs->rxs_dmamap->dm_segs[0].ds_addr + 2);	\
		__rmd->rmd0 = 0;					\
	} else {							\
		__rmd->rmd2 = 0;					\
		__rmd->rmd0 =						\
		    htole32(__rxs->rxs_dmamap->dm_segs[0].ds_addr + 2);	\
	}								\
	__rmd->rmd1 = htole32(LE_R1_OWN|LE_R1_ONES| 			\
	    (LE_BCNT(MCLBYTES - 2) & LE_R1_BCNT_MASK));			\
	PCN_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);\
} while(/*CONSTCOND*/0)

static void	pcn_start(struct ifnet *);
static void	pcn_watchdog(struct ifnet *);
static int	pcn_ioctl(struct ifnet *, u_long, void *);
static int	pcn_init(struct ifnet *);
static void	pcn_stop(struct ifnet *, int);

static bool	pcn_shutdown(device_t, int);

static void	pcn_reset(struct pcn_softc *);
static void	pcn_rxdrain(struct pcn_softc *);
static int	pcn_add_rxbuf(struct pcn_softc *, int);
static void	pcn_tick(void *);

static void	pcn_spnd(struct pcn_softc *);

static void	pcn_set_filter(struct pcn_softc *);

static int	pcn_intr(void *);
static void	pcn_txintr(struct pcn_softc *);
static int	pcn_rxintr(struct pcn_softc *);

static int	pcn_mii_readreg(device_t, int, int);
static void	pcn_mii_writereg(device_t, int, int, int);
static void	pcn_mii_statchg(struct ifnet *);

static void	pcn_79c970_mediainit(struct pcn_softc *);
static int	pcn_79c970_mediachange(struct ifnet *);
static void	pcn_79c970_mediastatus(struct ifnet *, struct ifmediareq *);

static void	pcn_79c971_mediainit(struct pcn_softc *);

/*
 * Description of a PCnet-PCI variant.  Used to select media access
 * method, mostly, and to print a nice description of the chip.
 */
static const struct pcn_variant {
	const char *pcv_desc;
	void (*pcv_mediainit)(struct pcn_softc *);
	uint16_t pcv_chipid;
} pcn_variants[] = {
	{ "Am79c970 PCnet-PCI",
	  pcn_79c970_mediainit,
	  PARTID_Am79c970 },

	{ "Am79c970A PCnet-PCI II",
	  pcn_79c970_mediainit,
	  PARTID_Am79c970A },

	{ "Am79c971 PCnet-FAST",
	  pcn_79c971_mediainit,
	  PARTID_Am79c971 },

	{ "Am79c972 PCnet-FAST+",
	  pcn_79c971_mediainit,
	  PARTID_Am79c972 },

	{ "Am79c973 PCnet-FAST III",
	  pcn_79c971_mediainit,
	  PARTID_Am79c973 },

	{ "Am79c975 PCnet-FAST III",
	  pcn_79c971_mediainit,
	  PARTID_Am79c975 },

	{ "Unknown PCnet-PCI variant",
	  pcn_79c971_mediainit,
	  0 },
};

int	pcn_copy_small = 0;

static int	pcn_match(device_t, cfdata_t, void *);
static void	pcn_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pcn, sizeof(struct pcn_softc),
    pcn_match, pcn_attach, NULL, NULL);

/*
 * Routines to read and write the PCnet-PCI CSR/BCR space.
 */

static inline uint32_t
pcn_csr_read(struct pcn_softc *sc, int reg)
{

	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RAP, reg);
	return (bus_space_read_4(sc->sc_st, sc->sc_sh, PCN32_RDP));
}

static inline void
pcn_csr_write(struct pcn_softc *sc, int reg, uint32_t val)
{

	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RAP, reg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RDP, val);
}

static inline uint32_t
pcn_bcr_read(struct pcn_softc *sc, int reg)
{

	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RAP, reg);
	return (bus_space_read_4(sc->sc_st, sc->sc_sh, PCN32_BDP));
}

static inline void
pcn_bcr_write(struct pcn_softc *sc, int reg, uint32_t val)
{

	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RAP, reg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_BDP, val);
}

static bool
pcn_is_vmware(const char *enaddr)
{

	/*
	 * VMware uses the OUI 00:0c:29 for auto-generated MAC
	 * addresses.
	 */
	if (enaddr[0] == 0x00 && enaddr[1] == 0x0c && enaddr[2] == 0x29)
		return (TRUE);

	/*
	 * VMware uses the OUI 00:50:56 for manually-set MAC
	 * addresses (and some auto-generated ones).
	 */
	if (enaddr[0] == 0x00 && enaddr[1] == 0x50 && enaddr[2] == 0x56)
		return (TRUE);

	return (FALSE);
}

static const struct pcn_variant *
pcn_lookup_variant(uint16_t chipid)
{
	const struct pcn_variant *pcv;

	for (pcv = pcn_variants; pcv->pcv_chipid != 0; pcv++) {
		if (chipid == pcv->pcv_chipid)
			return (pcv);
	}

	/*
	 * This covers unknown chips, which we simply treat like
	 * a generic PCnet-FAST.
	 */
	return (pcv);
}

static int
pcn_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	/*
	 * IBM Makes a PCI variant of this card which shows up as a
	 * Trident Microsystems 4DWAVE DX (ethernet network, revision 0x25)
	 * this card is truly a pcn card, so we have a special case match for
	 * it
	 */

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TRIDENT &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_TRIDENT_4DWAVE_DX &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_NETWORK)
		return(1);

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_AMD)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCNET_PCI:
		/* Beat if_le_pci.c */
		return (10);
	}

	return (0);
}

static void
pcn_attach(device_t parent, device_t self, void *aux)
{
	struct pcn_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_dma_segment_t seg;
	int ioh_valid, memh_valid;
	int ntxsegs, i, rseg, error;
	uint32_t chipid, reg;
	uint8_t enaddr[ETHER_ADDR_LEN];
	prop_object_t obj;
	bool is_vmware;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	callout_init(&sc->sc_tick_ch, 0);

	aprint_normal(": AMD PCnet-PCI Ethernet\n");

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, PCN_PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
	memh_valid = (pci_mapreg_map(pa, PCN_PCI_CBMEM,
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

	/* Make sure bus mastering is enabled. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate %d\n", error);
		return;
	}

	/*
	 * Reset the chip to a known state.  This also puts the
	 * chip into 32-bit mode.
	 */
	pcn_reset(sc);

	/*
	 * On some systems with the chip is an on-board device, the
	 * EEPROM is not used.  Handle this by reading the MAC address
	 * from the CSRs (assuming that boot firmware has written
	 * it there).
	 */
	obj = prop_dictionary_get(device_properties(sc->sc_dev),
				  "am79c970-no-eeprom");
	if (prop_bool_true(obj)) {
	        for (i = 0; i < 3; i++) {
			uint32_t val;
			val = pcn_csr_read(sc, LE_CSR12 + i);
			enaddr[2 * i] = val & 0xff;
			enaddr[2 * i + 1] = (val >> 8) & 0xff;
		}
	} else {
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			enaddr[i] = bus_space_read_1(sc->sc_st, sc->sc_sh,
			    PCN32_APROM + i);
		}
	}

	/* Check to see if this is a VMware emulated network interface. */
	is_vmware = pcn_is_vmware(enaddr);

	/*
	 * Now that the device is mapped, attempt to figure out what
	 * kind of chip we have.  Note that IDL has all 32 bits of
	 * the chip ID when we're in 32-bit mode.
	 */
	chipid = pcn_csr_read(sc, LE_CSR88);
	sc->sc_variant = pcn_lookup_variant(CHIPID_PARTID(chipid));

	aprint_normal_dev(self, "%s rev %d, Ethernet address %s\n",
	    sc->sc_variant->pcv_desc, CHIPID_VER(chipid),
	    ether_sprintf(enaddr));

	/*
	 * VMware has a bug in its network interface emulation; we must
	 * limit the number of Tx segments.
	 */
	if (is_vmware) {
		ntxsegs = PCN_NTXSEGS_VMWARE;
		prop_dictionary_set_bool(device_properties(sc->sc_dev),
					 "am79c970-vmware-tx-bug", TRUE);
		aprint_verbose_dev(self,
		    "VMware Tx segment count bug detected\n");
	} else {
		ntxsegs = PCN_NTXSEGS;
	}

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, pcn_intr, sc);
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
	     sizeof(struct pcn_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	     0)) != 0) {
		aprint_error_dev(self, "unable to allocate control data, "
		    "error = %d\n", error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	     sizeof(struct pcn_control_data), (void **)&sc->sc_control_data,
	     BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self, "unable to map control data, "
		    "error = %d\n", error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	     sizeof(struct pcn_control_data), 1,
	     sizeof(struct pcn_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(self, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	     sc->sc_control_data, sizeof(struct pcn_control_data), NULL,
	     0)) != 0) {
		aprint_error_dev(self,
		    "unable to load control data DMA map, error = %d\n", error);
		goto fail_3;
	}

	/* Create the transmit buffer DMA maps. */
	for (i = 0; i < PCN_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		     ntxsegs, MCLBYTES, 0, 0,
		     &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(self,
			    "unable to create tx DMA map %d, error = %d\n",
			    i, error);
			goto fail_4;
		}
	}

	/* Create the receive buffer DMA maps. */
	for (i = 0; i < PCN_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		     MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(self,
			    "unable to create rx DMA map %d, error = %d\n",
			    i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/* Initialize our media structures. */
	(*sc->sc_variant->pcv_mediainit)(sc);

	/*
	 * Initialize FIFO watermark info.
	 */
	switch (sc->sc_variant->pcv_chipid) {
	case PARTID_Am79c970:
	case PARTID_Am79c970A:
		sc->sc_rcvfw_desc = pcn_79c970_rcvfw;
		sc->sc_xmtsp_desc = pcn_79c970_xmtsp;
		sc->sc_xmtfw_desc = pcn_79c970_xmtfw;
		break;

	default:
		sc->sc_rcvfw_desc = pcn_79c971_rcvfw;
		/*
		 * Read BCR25 to determine how much SRAM is
		 * on the board.  If > 0, then we the chip
		 * uses different Start Point thresholds.
		 *
		 * Note BCR25 and BCR26 are loaded from the
		 * EEPROM on RST, and unaffected by S_RESET,
		 * so we don't really have to worry about
		 * them except for this.
		 */
		reg = pcn_bcr_read(sc, LE_BCR25) & 0x00ff;
		if (reg != 0)
			sc->sc_xmtsp_desc = pcn_79c971_xmtsp_sram;
		else
			sc->sc_xmtsp_desc = pcn_79c971_xmtsp;
		sc->sc_xmtfw_desc = pcn_79c971_xmtfw;
		break;
	}

	/*
	 * Set up defaults -- see the tables above for what these
	 * values mean.
	 *
	 * XXX How should we tune RCVFW and XMTFW?
	 */
	sc->sc_rcvfw = 1;	/* minimum for full-duplex */
	sc->sc_xmtsp = 1;
	sc->sc_xmtfw = 0;

	ifp = &sc->sc_ethercom.ec_if;
	strcpy(ifp->if_xname, device_xname(self));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = pcn_ioctl;
	ifp->if_start = pcn_start;
	ifp->if_watchdog = pcn_watchdog;
	ifp->if_init = pcn_init;
	ifp->if_stop = pcn_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

#ifdef PCN_EVENT_COUNTERS
	/* Attach event counters. */
	evcnt_attach_dynamic(&sc->sc_ev_txsstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txsstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "txdstall");
	evcnt_attach_dynamic(&sc->sc_ev_txintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(self), "txintr");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(self), "rxintr");
	evcnt_attach_dynamic(&sc->sc_ev_babl, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "babl");
	evcnt_attach_dynamic(&sc->sc_ev_miss, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "miss");
	evcnt_attach_dynamic(&sc->sc_ev_merr, EVCNT_TYPE_MISC,
	    NULL, device_xname(self), "merr");

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
#endif /* PCN_EVENT_COUNTERS */

	/*
	 * Establish power handler with shutdown hook, to make sure
	 * the interface is shutdown during reboot.
	 */
	if (pmf_device_register1(self, NULL, NULL, pcn_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < PCN_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < PCN_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct pcn_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * pcn_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static bool
pcn_shutdown(device_t self, int howto)
{
	struct pcn_softc *sc = device_private(self);

	pcn_stop(&sc->sc_ethercom.ec_if, 1);
	/* explicitly reset the chip for some onboard one with lazy firmware */
	pcn_reset(sc);

	return true;
}

/*
 * pcn_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
pcn_start(struct ifnet *ifp)
{
	struct pcn_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct pcn_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx = -1, ofree, seg;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		/* Get a work queue entry. */
		if (sc->sc_txsfree == 0) {
			PCN_EVCNT_INCR(&sc->sc_ev_txsstall);
			break;
		}

		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.  In this case, we'll copy
		 * and try again.
		 */
		if (bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0) {
			PCN_EVCNT_INCR(&sc->sc_ev_txcopy);
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
					    "cluster\n",
					    device_xname(sc->sc_dev));
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
				    "error = %d\n", device_xname(sc->sc_dev),
				    error);
				m_freem(m);
				break;
			}
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring as a termination point, to
		 * prevent wrap-around.
		 */
		if (dmamap->dm_nsegs > (sc->sc_txfree - 1)) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.  Notify the upper
			 * layer that there are not more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX is it worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			if (m != NULL)
				m_freem(m);
			PCN_EVCNT_INCR(&sc->sc_ev_txdstall);
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

#ifdef PCN_EVENT_COUNTERS
		switch (dmamap->dm_nsegs) {
		case 1:
			PCN_EVCNT_INCR(&sc->sc_ev_txseg1);
			break;
		case 2:
			PCN_EVCNT_INCR(&sc->sc_ev_txseg2);
			break;
		case 3:
			PCN_EVCNT_INCR(&sc->sc_ev_txseg3);
			break;
		case 4:
			PCN_EVCNT_INCR(&sc->sc_ev_txseg4);
			break;
		case 5:
			PCN_EVCNT_INCR(&sc->sc_ev_txseg5);
			break;
		default:
			PCN_EVCNT_INCR(&sc->sc_ev_txsegmore);
			break;
		}
#endif /* PCN_EVENT_COUNTERS */

		/*
		 * Initialize the transmit descriptors.
		 */
		if (sc->sc_swstyle == LE_B20_SSTYLE_PCNETPCI3) {
			for (nexttx = sc->sc_txnext, seg = 0;
			     seg < dmamap->dm_nsegs;
			     seg++, nexttx = PCN_NEXTTX(nexttx)) {
				/*
				 * If this is the first descriptor we're
				 * enqueueing, don't set the OWN bit just
				 * yet.  That could cause a race condition.
				 * We'll do it below.
				 */
				sc->sc_txdescs[nexttx].tmd0 = 0;
				sc->sc_txdescs[nexttx].tmd2 =
				    htole32(dmamap->dm_segs[seg].ds_addr);
				sc->sc_txdescs[nexttx].tmd1 =
				    htole32(LE_T1_ONES |
				    (nexttx == sc->sc_txnext ? 0 : LE_T1_OWN) |
				    (LE_BCNT(dmamap->dm_segs[seg].ds_len) &
				     LE_T1_BCNT_MASK));
				lasttx = nexttx;
			}
		} else {
			for (nexttx = sc->sc_txnext, seg = 0;
			     seg < dmamap->dm_nsegs;
			     seg++, nexttx = PCN_NEXTTX(nexttx)) {
				/*
				 * If this is the first descriptor we're
				 * enqueueing, don't set the OWN bit just
				 * yet.  That could cause a race condition.
				 * We'll do it below.
				 */
				sc->sc_txdescs[nexttx].tmd0 =
				    htole32(dmamap->dm_segs[seg].ds_addr);
				sc->sc_txdescs[nexttx].tmd2 = 0;
				sc->sc_txdescs[nexttx].tmd1 =
				    htole32(LE_T1_ONES |
				    (nexttx == sc->sc_txnext ? 0 : LE_T1_OWN) |
				    (LE_BCNT(dmamap->dm_segs[seg].ds_len) &
				     LE_T1_BCNT_MASK));
				lasttx = nexttx;
			}
		}

		KASSERT(lasttx != -1);
		/* Interrupt on the packet, if appropriate. */
		if ((sc->sc_txsnext & PCN_TXINTR_MASK) == 0)
			sc->sc_txdescs[lasttx].tmd1 |= htole32(LE_T1_LTINT);

		/* Set `start of packet' and `end of packet' appropriately. */
		sc->sc_txdescs[lasttx].tmd1 |= htole32(LE_T1_ENP);
		sc->sc_txdescs[sc->sc_txnext].tmd1 |=
		    htole32(LE_T1_OWN|LE_T1_STP);

		/* Sync the descriptors we're using. */
		PCN_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Kick the transmitter. */
		pcn_csr_write(sc, LE_CSR0, LE_C0_INEA|LE_C0_TDMD);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;

		/* Advance the tx pointer. */
		sc->sc_txfree -= dmamap->dm_nsegs;
		sc->sc_txnext = nexttx;

		sc->sc_txsfree--;
		sc->sc_txsnext = PCN_NEXTTXS(sc->sc_txsnext);

		/* Pass the packet to any BPF listeners. */
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

/*
 * pcn_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
pcn_watchdog(struct ifnet *ifp)
{
	struct pcn_softc *sc = ifp->if_softc;

	/*
	 * Since we're not interrupting every packet, sweep
	 * up before we report an error.
	 */
	pcn_txintr(sc);

	if (sc->sc_txfree != PCN_NTXDESC) {
		printf("%s: device timeout (txfree %d txsfree %d)\n",
		    device_xname(sc->sc_dev), sc->sc_txfree, sc->sc_txsfree);
		ifp->if_oerrors++;

		/* Reset the interface. */
		(void) pcn_init(ifp);
	}

	/* Try to get more packets going. */
	pcn_start(ifp);
}

/*
 * pcn_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
pcn_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct pcn_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
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
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				error = pcn_init(ifp);
			else
				error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	pcn_start(ifp);

	splx(s);
	return (error);
}

/*
 * pcn_intr:
 *
 *	Interrupt service routine.
 */
static int
pcn_intr(void *arg)
{
	struct pcn_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t csr0;
	int wantinit, handled = 0;

	for (wantinit = 0; wantinit == 0;) {
		csr0 = pcn_csr_read(sc, LE_CSR0);
		if ((csr0 & LE_C0_INTR) == 0)
			break;

		rnd_add_uint32(&sc->rnd_source, csr0);

		/* ACK the bits and re-enable interrupts. */
		pcn_csr_write(sc, LE_CSR0, csr0 &
		    (LE_C0_INEA|LE_C0_BABL|LE_C0_MISS|LE_C0_MERR|LE_C0_RINT|
		     LE_C0_TINT|LE_C0_IDON));

		handled = 1;

		if (csr0 & LE_C0_RINT) {
			PCN_EVCNT_INCR(&sc->sc_ev_rxintr);
			wantinit = pcn_rxintr(sc);
		}

		if (csr0 & LE_C0_TINT) {
			PCN_EVCNT_INCR(&sc->sc_ev_txintr);
			pcn_txintr(sc);
		}

		if (csr0 & LE_C0_ERR) {
			if (csr0 & LE_C0_BABL) {
				PCN_EVCNT_INCR(&sc->sc_ev_babl);
				ifp->if_oerrors++;
			}
			if (csr0 & LE_C0_MISS) {
				PCN_EVCNT_INCR(&sc->sc_ev_miss);
				ifp->if_ierrors++;
			}
			if (csr0 & LE_C0_MERR) {
				PCN_EVCNT_INCR(&sc->sc_ev_merr);
				printf("%s: memory error\n",
				    device_xname(sc->sc_dev));
				wantinit = 1;
				break;
			}
		}

		if ((csr0 & LE_C0_RXON) == 0) {
			printf("%s: receiver disabled\n",
			    device_xname(sc->sc_dev));
			ifp->if_ierrors++;
			wantinit = 1;
		}

		if ((csr0 & LE_C0_TXON) == 0) {
			printf("%s: transmitter disabled\n",
			    device_xname(sc->sc_dev));
			ifp->if_oerrors++;
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			pcn_init(ifp);

		/* Try to get more packets going. */
		pcn_start(ifp);
	}

	return (handled);
}

/*
 * pcn_spnd:
 *
 *	Suspend the chip.
 */
static void
pcn_spnd(struct pcn_softc *sc)
{
	int i;

	pcn_csr_write(sc, LE_CSR5, sc->sc_csr5 | LE_C5_SPND);

	for (i = 0; i < 10000; i++) {
		if (pcn_csr_read(sc, LE_CSR5) & LE_C5_SPND)
			return;
		delay(5);
	}

	printf("%s: WARNING: chip failed to enter suspended state\n",
	    device_xname(sc->sc_dev));
}

/*
 * pcn_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
pcn_txintr(struct pcn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct pcn_txsoft *txs;
	uint32_t tmd1, tmd2, tmd;
	int i, j;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txsdirty; sc->sc_txsfree != PCN_TXQUEUELEN;
	     i = PCN_NEXTTXS(i), sc->sc_txsfree++) {
		txs = &sc->sc_txsoft[i];

		PCN_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_dmamap->dm_nsegs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		tmd1 = le32toh(sc->sc_txdescs[txs->txs_lastdesc].tmd1);
		if (tmd1 & LE_T1_OWN)
			break;

		/*
		 * Slightly annoying -- we have to loop through the
		 * descriptors we've used looking for ERR, since it
		 * can appear on any descriptor in the chain.
		 */
		for (j = txs->txs_firstdesc;; j = PCN_NEXTTX(j)) {
			tmd = le32toh(sc->sc_txdescs[j].tmd1);
			if (tmd & LE_T1_ERR) {
				ifp->if_oerrors++;
				if (sc->sc_swstyle == LE_B20_SSTYLE_PCNETPCI3)
					tmd2 = le32toh(sc->sc_txdescs[j].tmd0);
				else
					tmd2 = le32toh(sc->sc_txdescs[j].tmd2);
				if (tmd2 & LE_T2_UFLO) {
					if (sc->sc_xmtsp < LE_C80_XMTSP_MAX) {
						sc->sc_xmtsp++;
						printf("%s: transmit "
						    "underrun; new threshold: "
						    "%s\n",
						    device_xname(sc->sc_dev),
						    sc->sc_xmtsp_desc[
						    sc->sc_xmtsp]);
						pcn_spnd(sc);
						pcn_csr_write(sc, LE_CSR80,
						    LE_C80_RCVFW(sc->sc_rcvfw) |
						    LE_C80_XMTSP(sc->sc_xmtsp) |
						    LE_C80_XMTFW(sc->sc_xmtfw));
						pcn_csr_write(sc, LE_CSR5,
						    sc->sc_csr5);
					} else {
						printf("%s: transmit "
						    "underrun\n",
						    device_xname(sc->sc_dev));
					}
				} else if (tmd2 & LE_T2_BUFF) {
					printf("%s: transmit buffer error\n",
					    device_xname(sc->sc_dev));
				}
				if (tmd2 & LE_T2_LCOL)
					ifp->if_collisions++;
				if (tmd2 & LE_T2_RTRY)
					ifp->if_collisions += 16;
				goto next_packet;
			}
			if (j == txs->txs_lastdesc)
				break;
		}
		if (tmd1 & LE_T1_ONE)
			ifp->if_collisions++;
		else if (tmd & LE_T1_MORE) {
			/* Real number is unknown. */
			ifp->if_collisions += 2;
		}
		ifp->if_opackets++;
 next_packet:
		sc->sc_txfree += txs->txs_dmamap->dm_nsegs;
		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txsdirty = i;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txsfree == PCN_TXQUEUELEN)
		ifp->if_timer = 0;
}

/*
 * pcn_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static int
pcn_rxintr(struct pcn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct pcn_rxsoft *rxs;
	struct mbuf *m;
	uint32_t rmd1;
	int i, len;

	for (i = sc->sc_rxptr;; i = PCN_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		PCN_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rmd1 = le32toh(sc->sc_rxdescs[i].rmd1);

		if (rmd1 & LE_R1_OWN)
			break;

		/*
		 * Check for errors and make sure the packet fit into
		 * a single buffer.  We have structured this block of
		 * code the way it is in order to compress it into
		 * one test in the common case (no error).
		 */
		if (__predict_false((rmd1 & (LE_R1_STP|LE_R1_ENP|LE_R1_ERR)) !=
		    (LE_R1_STP|LE_R1_ENP))) {
			/* Make sure the packet is in a single buffer. */
			if ((rmd1 & (LE_R1_STP|LE_R1_ENP)) !=
			    (LE_R1_STP|LE_R1_ENP)) {
				printf("%s: packet spilled into next buffer\n",
				    device_xname(sc->sc_dev));
				return (1);	/* pcn_intr() will re-init */
			}

			/*
			 * If the packet had an error, simple recycle the
			 * buffer.
			 */
			if (rmd1 & LE_R1_ERR) {
				ifp->if_ierrors++;
				/*
				 * If we got an overflow error, chances
				 * are there will be a CRC error.  In
				 * this case, just print the overflow
				 * error, and skip the others.
				 */
				if (rmd1 & LE_R1_OFLO)
					printf("%s: overflow error\n",
					    device_xname(sc->sc_dev));
				else {
#define	PRINTIT(x, str)							\
					if (rmd1 & (x))			\
						printf("%s: %s\n",	\
						    device_xname(sc->sc_dev), \
						    str);
					PRINTIT(LE_R1_FRAM, "framing error");
					PRINTIT(LE_R1_CRC, "CRC error");
					PRINTIT(LE_R1_BUFF, "buffer error");
				}
#undef PRINTIT
				PCN_INIT_RXDESC(sc, i);
				continue;
			}
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.
		 */
		if (sc->sc_swstyle == LE_B20_SSTYLE_PCNETPCI3)
			len = le32toh(sc->sc_rxdescs[i].rmd0) & LE_R1_BCNT_MASK;
		else
			len = le32toh(sc->sc_rxdescs[i].rmd2) & LE_R1_BCNT_MASK;

		/*
		 * The LANCE family includes the CRC with every packet;
		 * trim it off here.
		 */
		len -= ETHER_CRC_LEN;

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
		if (pcn_copy_small != 0 && len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			memcpy(mtod(m, void *),
			    mtod(rxs->rxs_mbuf, void *), len);
			PCN_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
		} else {
			m = rxs->rxs_mbuf;
			if (pcn_add_rxbuf(sc, i) != 0) {
 dropit:
				ifp->if_ierrors++;
				PCN_INIT_RXDESC(sc, i);
				bus_dmamap_sync(sc->sc_dmat,
				    rxs->rxs_dmamap, 0,
				    rxs->rxs_dmamap->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/* Pass this up to any BPF listeners. */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
		ifp->if_ipackets++;
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
	return (0);
}

/*
 * pcn_tick:
 *
 *	One second timer, used to tick the MII.
 */
static void
pcn_tick(void *arg)
{
	struct pcn_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, pcn_tick, sc);
}

/*
 * pcn_reset:
 *
 *	Perform a soft reset on the PCnet-PCI.
 */
static void
pcn_reset(struct pcn_softc *sc)
{

	/*
	 * The PCnet-PCI chip is reset by reading from the
	 * RESET register.  Note that while the NE2100 LANCE
	 * boards require a write after the read, the PCnet-PCI
	 * chips do not require this.
	 *
	 * Since we don't know if we're in 16-bit or 32-bit
	 * mode right now, issue both (it's safe) in the
	 * hopes that one will succeed.
	 */
	(void) bus_space_read_2(sc->sc_st, sc->sc_sh, PCN16_RESET);
	(void) bus_space_read_4(sc->sc_st, sc->sc_sh, PCN32_RESET);

	/* Wait 1ms for it to finish. */
	delay(1000);

	/*
	 * Select 32-bit I/O mode by issuing a 32-bit write to the
	 * RDP.  Since the RAP is 0 after a reset, writing a 0
	 * to RDP is safe (since it simply clears CSR0).
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, PCN32_RDP, 0);
}

/*
 * pcn_init:		[ifnet interface function]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
pcn_init(struct ifnet *ifp)
{
	struct pcn_softc *sc = ifp->if_softc;
	struct pcn_rxsoft *rxs;
	const uint8_t *enaddr = CLLADDR(ifp->if_sadl);
	int i, error = 0;
	uint32_t reg;

	/* Cancel any pending I/O. */
	pcn_stop(ifp, 0);

	/* Reset the chip to a known state. */
	pcn_reset(sc);

	/*
	 * On the Am79c970, select SSTYLE 2, and SSTYLE 3 on everything
	 * else.
	 *
	 * XXX It'd be really nice to use SSTYLE 2 on all the chips,
	 * because the structure layout is compatible with ILACC,
	 * but the burst mode is only available in SSTYLE 3, and
	 * burst mode should provide some performance enhancement.
	 */
	if (sc->sc_variant->pcv_chipid == PARTID_Am79c970)
		sc->sc_swstyle = LE_B20_SSTYLE_PCNETPCI2;
	else
		sc->sc_swstyle = LE_B20_SSTYLE_PCNETPCI3;
	pcn_bcr_write(sc, LE_BCR20, sc->sc_swstyle);

	/* Initialize the transmit descriptor ring. */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	PCN_CDTXSYNC(sc, 0, PCN_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = PCN_NTXDESC;
	sc->sc_txnext = 0;

	/* Initialize the transmit job descriptors. */
	for (i = 0; i < PCN_TXQUEUELEN; i++)
		sc->sc_txsoft[i].txs_mbuf = NULL;
	sc->sc_txsfree = PCN_TXQUEUELEN;
	sc->sc_txsnext = 0;
	sc->sc_txsdirty = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < PCN_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = pcn_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				pcn_rxdrain(sc);
				goto out;
			}
		} else
			PCN_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/* Initialize MODE for the initialization block. */
	sc->sc_mode = 0;
	if (ifp->if_flags & IFF_PROMISC)
		sc->sc_mode |= LE_C15_PROM;
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		sc->sc_mode |= LE_C15_DRCVBC;

	/*
	 * If we have MII, simply select MII in the MODE register,
	 * and clear ASEL.  Otherwise, let ASEL stand (for now),
	 * and leave PORTSEL alone (it is ignored with ASEL is set).
	 */
	if (sc->sc_flags & PCN_F_HAS_MII) {
		pcn_bcr_write(sc, LE_BCR2,
		    pcn_bcr_read(sc, LE_BCR2) & ~LE_B2_ASEL);
		sc->sc_mode |= LE_C15_PORTSEL(PORTSEL_MII);

		/*
		 * Disable MII auto-negotiation.  We handle that in
		 * our own MII layer.
		 */
		pcn_bcr_write(sc, LE_BCR32,
		    pcn_bcr_read(sc, LE_BCR32) | LE_B32_DANAS);
	}

	/*
	 * Set the Tx and Rx descriptor ring addresses in the init
	 * block, the TLEN and RLEN other fields of the init block
	 * MODE register.
	 */
	sc->sc_initblock.init_rdra = htole32(PCN_CDRXADDR(sc, 0));
	sc->sc_initblock.init_tdra = htole32(PCN_CDTXADDR(sc, 0));
	sc->sc_initblock.init_mode = htole32(sc->sc_mode |
	    ((ffs(PCN_NTXDESC) - 1) << 28) |
	    ((ffs(PCN_NRXDESC) - 1) << 20));

	/* Set the station address in the init block. */
	sc->sc_initblock.init_padr[0] = htole32(enaddr[0] |
	    (enaddr[1] << 8) | (enaddr[2] << 16) | (enaddr[3] << 24));
	sc->sc_initblock.init_padr[1] = htole32(enaddr[4] |
	    (enaddr[5] << 8));

	/* Set the multicast filter in the init block. */
	pcn_set_filter(sc);

	/* Initialize CSR3. */
	pcn_csr_write(sc, LE_CSR3, LE_C3_MISSM|LE_C3_IDONM|LE_C3_DXSUFLO);

	/* Initialize CSR4. */
	pcn_csr_write(sc, LE_CSR4, LE_C4_DMAPLUS|LE_C4_APAD_XMT|
	    LE_C4_MFCOM|LE_C4_RCVCCOM|LE_C4_TXSTRTM);

	/* Initialize CSR5. */
	sc->sc_csr5 = LE_C5_LTINTEN|LE_C5_SINTE;
	pcn_csr_write(sc, LE_CSR5, sc->sc_csr5);

	/*
	 * If we have an Am79c971 or greater, initialize CSR7.
	 *
	 * XXX Might be nice to use the MII auto-poll interrupt someday.
	 */
	switch (sc->sc_variant->pcv_chipid) {
	case PARTID_Am79c970:
	case PARTID_Am79c970A:
		/* Not available on these chips. */
		break;

	default:
		pcn_csr_write(sc, LE_CSR7, LE_C7_FASTSPNDE);
		break;
	}

	/*
	 * On the Am79c970A and greater, initialize BCR18 to
	 * enable burst mode.
	 *
	 * Also enable the "no underflow" option on the Am79c971 and
	 * higher, which prevents the chip from generating transmit
	 * underflows, yet sill provides decent performance.  Note if
	 * chip is not connected to external SRAM, then we still have
	 * to handle underflow errors (the NOUFLO bit is ignored in
	 * that case).
	 */
	reg = pcn_bcr_read(sc, LE_BCR18);
	switch (sc->sc_variant->pcv_chipid) {
	case PARTID_Am79c970:
		break;

	case PARTID_Am79c970A:
		reg |= LE_B18_BREADE|LE_B18_BWRITE;
		break;

	default:
		reg |= LE_B18_BREADE|LE_B18_BWRITE|LE_B18_NOUFLO;
		break;
	}
	pcn_bcr_write(sc, LE_BCR18, reg);

	/*
	 * Initialize CSR80 (FIFO thresholds for Tx and Rx).
	 */
	pcn_csr_write(sc, LE_CSR80, LE_C80_RCVFW(sc->sc_rcvfw) |
	    LE_C80_XMTSP(sc->sc_xmtsp) | LE_C80_XMTFW(sc->sc_xmtfw));

	/*
	 * Send the init block to the chip, and wait for it
	 * to be processed.
	 */
	PCN_CDINITSYNC(sc, BUS_DMASYNC_PREWRITE);
	pcn_csr_write(sc, LE_CSR1, PCN_CDINITADDR(sc) & 0xffff);
	pcn_csr_write(sc, LE_CSR2, (PCN_CDINITADDR(sc) >> 16) & 0xffff);
	pcn_csr_write(sc, LE_CSR0, LE_C0_INIT);
	delay(100);
	for (i = 0; i < 10000; i++) {
		if (pcn_csr_read(sc, LE_CSR0) & LE_C0_IDON)
			break;
		delay(10);
	}
	PCN_CDINITSYNC(sc, BUS_DMASYNC_POSTWRITE);
	if (i == 10000) {
		printf("%s: timeout processing init block\n",
		    device_xname(sc->sc_dev));
		error = EIO;
		goto out;
	}

	/* Set the media. */
	if ((error = mii_ifmedia_change(&sc->sc_mii)) != 0)
		goto out;

	/* Enable interrupts and external activity (and ACK IDON). */
	pcn_csr_write(sc, LE_CSR0, LE_C0_INEA|LE_C0_STRT|LE_C0_IDON);

	if (sc->sc_flags & PCN_F_HAS_MII) {
		/* Start the one second MII clock. */
		callout_reset(&sc->sc_tick_ch, hz, pcn_tick, sc);
	}

	/* ...all done! */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	return (error);
}

/*
 * pcn_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
pcn_rxdrain(struct pcn_softc *sc)
{
	struct pcn_rxsoft *rxs;
	int i;

	for (i = 0; i < PCN_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * pcn_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
static void
pcn_stop(struct ifnet *ifp, int disable)
{
	struct pcn_softc *sc = ifp->if_softc;
	struct pcn_txsoft *txs;
	int i;

	if (sc->sc_flags & PCN_F_HAS_MII) {
		/* Stop the one second clock. */
		callout_stop(&sc->sc_tick_ch);

		/* Down the MII. */
		mii_down(&sc->sc_mii);
	}

	/* Stop the chip. */
	pcn_csr_write(sc, LE_CSR0, LE_C0_STOP);

	/* Release any queued transmit buffers. */
	for (i = 0; i < PCN_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}

	/* Mark the interface as down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		pcn_rxdrain(sc);
}

/*
 * pcn_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
pcn_add_rxbuf(struct pcn_softc *sc, int idx)
{
	struct pcn_rxsoft *rxs = &sc->sc_rxsoft[idx];
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

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("pcn_add_rxbuf");
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	PCN_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * pcn_set_filter:
 *
 *	Set up the receive filter.
 */
static void
pcn_set_filter(struct pcn_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc;

	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the high
	 * order 6 bits as an index into the 64-bit logical address
	 * filter.  The high order bits select the word, while the rest
	 * of the bits select the bit within the word.
	 */

	if (ifp->if_flags & IFF_PROMISC)
		goto allmulti;

	sc->sc_initblock.init_ladrf[0] =
	    sc->sc_initblock.init_ladrf[1] =
	    sc->sc_initblock.init_ladrf[2] =
	    sc->sc_initblock.init_ladrf[3] = 0;

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

		crc = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		sc->sc_initblock.init_ladrf[crc >> 4] |=
		    htole16(1 << (crc & 0xf));

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_initblock.init_ladrf[0] =
	    sc->sc_initblock.init_ladrf[1] =
	    sc->sc_initblock.init_ladrf[2] =
	    sc->sc_initblock.init_ladrf[3] = 0xffff;
}

/*
 * pcn_79c970_mediainit:
 *
 *	Initialize media for the Am79c970.
 */
static void
pcn_79c970_mediainit(struct pcn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const char *sep = "";

	sc->sc_mii.mii_ifp = ifp;

	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, pcn_79c970_mediachange,
	    pcn_79c970_mediastatus);

#define	ADD(str, m, d)							\
do {									\
	aprint_normal("%s%s", sep, str);					\
	ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|(m), (d), NULL);	\
	sep = ", ";							\
} while (/*CONSTCOND*/0)

	aprint_normal("%s: ", device_xname(sc->sc_dev));
	ADD("10base5", IFM_10_5, PORTSEL_AUI);
	if (sc->sc_variant->pcv_chipid == PARTID_Am79c970A)
		ADD("10base5-FDX", IFM_10_5|IFM_FDX, PORTSEL_AUI);
	ADD("10baseT", IFM_10_T, PORTSEL_10T);
	if (sc->sc_variant->pcv_chipid == PARTID_Am79c970A)
		ADD("10baseT-FDX", IFM_10_T|IFM_FDX, PORTSEL_10T);
	ADD("auto", IFM_AUTO, 0);
	if (sc->sc_variant->pcv_chipid == PARTID_Am79c970A)
		ADD("auto-FDX", IFM_AUTO|IFM_FDX, 0);
	aprint_normal("\n");

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
}

/*
 * pcn_79c970_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status (Am79c970 version).
 */
static void
pcn_79c970_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct pcn_softc *sc = ifp->if_softc;

	/*
	 * The currently selected media is always the active media.
	 * Note: We have no way to determine what media the AUTO
	 * process picked.
	 */
	ifmr->ifm_active = sc->sc_mii.mii_media.ifm_media;
}

/*
 * pcn_79c970_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media (Am79c970 version).
 */
static int
pcn_79c970_mediachange(struct ifnet *ifp)
{
	struct pcn_softc *sc = ifp->if_softc;
	uint32_t reg;

	if (IFM_SUBTYPE(sc->sc_mii.mii_media.ifm_media) == IFM_AUTO) {
		/*
		 * CSR15:PORTSEL doesn't matter.  Just set BCR2:ASEL.
		 */
		reg = pcn_bcr_read(sc, LE_BCR2);
		reg |= LE_B2_ASEL;
		pcn_bcr_write(sc, LE_BCR2, reg);
	} else {
		/*
		 * Clear BCR2:ASEL and set the new CSR15:PORTSEL value.
		 */
		reg = pcn_bcr_read(sc, LE_BCR2);
		reg &= ~LE_B2_ASEL;
		pcn_bcr_write(sc, LE_BCR2, reg);

		reg = pcn_csr_read(sc, LE_CSR15);
		reg = (reg & ~LE_C15_PORTSEL(PORTSEL_MASK)) |
		    LE_C15_PORTSEL(sc->sc_mii.mii_media.ifm_cur->ifm_data);
		pcn_csr_write(sc, LE_CSR15, reg);
	}

	if ((sc->sc_mii.mii_media.ifm_media & IFM_FDX) != 0) {
		reg = LE_B9_FDEN;
		if (IFM_SUBTYPE(sc->sc_mii.mii_media.ifm_media) == IFM_10_5)
			reg |= LE_B9_AUIFD;
		pcn_bcr_write(sc, LE_BCR9, reg);
	} else
		pcn_bcr_write(sc, LE_BCR9, 0);

	return (0);
}

/*
 * pcn_79c971_mediainit:
 *
 *	Initialize media for the Am79c971.
 */
static void
pcn_79c971_mediainit(struct pcn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* We have MII. */
	sc->sc_flags |= PCN_F_HAS_MII;

	/*
	 * The built-in 10BASE-T interface is mapped to the MII
	 * on the PCNet-FAST.  Unfortunately, there's no EEPROM
	 * word that tells us which PHY to use.
	 * This driver used to ignore all but the first PHY to
	 * answer, but this code was removed to support multiple
	 * external PHYs. As the default instance will be the first
	 * one to answer, no harm is done by letting the possibly
	 * non-connected internal PHY show up.
	 */

	/* Initialize our media structures and probe the MII. */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = pcn_mii_readreg;
	sc->sc_mii.mii_writereg = pcn_mii_writereg;
	sc->sc_mii.mii_statchg = pcn_mii_statchg;

	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, 0, ether_mediachange,
	    ether_mediastatus);

	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
}

/*
 * pcn_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII.
 */
static int
pcn_mii_readreg(device_t self, int phy, int reg)
{
	struct pcn_softc *sc = device_private(self);
	uint32_t rv;

	pcn_bcr_write(sc, LE_BCR33, reg | (phy << PHYAD_SHIFT));
	rv = pcn_bcr_read(sc, LE_BCR34) & LE_B34_MIIMD;
	if (rv == 0xffff)
		return (0);

	return (rv);
}

/*
 * pcn_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII.
 */
static void
pcn_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct pcn_softc *sc = device_private(self);

	pcn_bcr_write(sc, LE_BCR33, reg | (phy << PHYAD_SHIFT));
	pcn_bcr_write(sc, LE_BCR34, val);
}

/*
 * pcn_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
pcn_mii_statchg(struct ifnet *ifp)
{
	struct pcn_softc *sc = ifp->if_softc;

	if ((sc->sc_mii.mii_media_active & IFM_FDX) != 0)
		pcn_bcr_write(sc, LE_BCR9, LE_B9_FDEN);
	else
		pcn_bcr_write(sc, LE_BCR9, 0);
}
