/* $NetBSD: if_bce.c,v 1.40 2015/04/13 16:33:25 riastradh Exp $	 */

/*
 * Copyright (c) 2003 Clifford Wright. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Broadcom BCM440x 10/100 ethernet (broadcom.com)
 * SiliconBackplane is technology from Sonics, Inc.(sonicsinc.com)
 *
 * Cliff Wright cliff@snipe444.org
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_bce.c,v 1.40 2015/04/13 16:33:25 riastradh Exp $");

#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>
#include <sys/rndsource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/if_bcereg.h>

/* transmit buffer max frags allowed */
#define BCE_NTXFRAGS	16

/* ring descriptor */
struct bce_dma_slot {
	uint32_t ctrl;
	uint32_t addr;
};
#define CTRL_BC_MASK	0x1fff	/* buffer byte count */
#define CTRL_EOT	0x10000000	/* end of descriptor table */
#define CTRL_IOC	0x20000000	/* interrupt on completion */
#define CTRL_EOF	0x40000000	/* end of frame */
#define CTRL_SOF	0x80000000	/* start of frame */

/* Packet status is returned in a pre-packet header */
struct rx_pph {
	uint16_t len;
	uint16_t flags;
	uint16_t pad[12];
};

/* packet status flags bits */
#define RXF_NO				0x8	/* odd number of nibbles */
#define RXF_RXER			0x4	/* receive symbol error */
#define RXF_CRC				0x2	/* crc error */
#define RXF_OV				0x1	/* fifo overflow */

/* number of descriptors used in a ring */
#define BCE_NRXDESC		128
#define BCE_NTXDESC		128

/*
 * Mbuf pointers. We need these to keep track of the virtual addresses
 * of our mbuf chains since we can only convert from physical to virtual,
 * not the other way around.
 */
struct bce_chain_data {
	struct mbuf    *bce_tx_chain[BCE_NTXDESC];
	struct mbuf    *bce_rx_chain[BCE_NRXDESC];
	bus_dmamap_t	bce_tx_map[BCE_NTXDESC];
	bus_dmamap_t	bce_rx_map[BCE_NRXDESC];
};

#define BCE_TIMEOUT		100	/* # 10us for mii read/write */

struct bce_softc {
	device_t		bce_dev;
	bus_space_tag_t		bce_btag;
	bus_space_handle_t	bce_bhandle;
	bus_dma_tag_t		bce_dmatag;
	struct ethercom		ethercom;	/* interface info */
	void			*bce_intrhand;
	struct pci_attach_args	bce_pa;
	struct mii_data		bce_mii;
	uint32_t		bce_phy;	/* eeprom indicated phy */
	struct ifmedia		bce_ifmedia;	/* media info *//* Check */
	uint8_t			enaddr[ETHER_ADDR_LEN];
	struct bce_dma_slot	*bce_rx_ring;	/* receive ring */
	struct bce_dma_slot	*bce_tx_ring;	/* transmit ring */
	struct bce_chain_data	bce_cdata;	/* mbufs */
	bus_dmamap_t		bce_ring_map;
	uint32_t		bce_intmask;	/* current intr mask */
	uint32_t		bce_rxin;	/* last rx descriptor seen */
	uint32_t		bce_txin;	/* last tx descriptor seen */
	int			bce_txsfree;	/* no. tx slots available */
	int			bce_txsnext;	/* next available tx slot */
	callout_t		bce_timeout;
	krndsource_t	rnd_source;
};

/* for ring descriptors */
#define BCE_RXBUF_LEN	(MCLBYTES - 4)
#define BCE_INIT_RXDESC(sc, x)						\
do {									\
	struct bce_dma_slot *__bced = &sc->bce_rx_ring[x];		\
									\
	*mtod(sc->bce_cdata.bce_rx_chain[x], uint32_t *) = 0;		\
	__bced->addr =							\
	    htole32(sc->bce_cdata.bce_rx_map[x]->dm_segs[0].ds_addr	\
	    + 0x40000000);						\
	if (x != (BCE_NRXDESC - 1))					\
		__bced->ctrl = htole32(BCE_RXBUF_LEN);			\
	else								\
		__bced->ctrl = htole32(BCE_RXBUF_LEN | CTRL_EOT);	\
	bus_dmamap_sync(sc->bce_dmatag, sc->bce_ring_map,		\
	    sizeof(struct bce_dma_slot) * x,				\
	    sizeof(struct bce_dma_slot),				\
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);			\
} while (/* CONSTCOND */ 0)

static	int	bce_probe(device_t, cfdata_t, void *);
static	void	bce_attach(device_t, device_t, void *);
static	int	bce_ioctl(struct ifnet *, u_long, void *);
static	void	bce_start(struct ifnet *);
static	void	bce_watchdog(struct ifnet *);
static	int	bce_intr(void *);
static	void	bce_rxintr(struct bce_softc *);
static	void	bce_txintr(struct bce_softc *);
static	int	bce_init(struct ifnet *);
static	void	bce_add_mac(struct bce_softc *, uint8_t *, unsigned long);
static	int	bce_add_rxbuf(struct bce_softc *, int);
static	void	bce_rxdrain(struct bce_softc *);
static	void	bce_stop(struct ifnet *, int);
static	void	bce_reset(struct bce_softc *);
static	bool	bce_resume(device_t, const pmf_qual_t *);
static	void	bce_set_filter(struct ifnet *);
static	int	bce_mii_read(device_t, int, int);
static	void	bce_mii_write(device_t, int, int, int);
static	void	bce_statchg(struct ifnet *);
static	void	bce_tick(void *);

CFATTACH_DECL_NEW(bce, sizeof(struct bce_softc),
		  bce_probe, bce_attach, NULL, NULL);

static const struct bce_product {
	pci_vendor_id_t bp_vendor;
	pci_product_id_t bp_product;
	const	char *bp_name;
} bce_products[] = {
	{
		PCI_VENDOR_BROADCOM,
		PCI_PRODUCT_BROADCOM_BCM4401,
		"Broadcom BCM4401 10/100 Ethernet"
	},
	{
		PCI_VENDOR_BROADCOM,
		PCI_PRODUCT_BROADCOM_BCM4401_B0,
		"Broadcom BCM4401-B0 10/100 Ethernet"
	},
	{

		0,
		0,
		NULL
	},
};

static const struct bce_product *
bce_lookup(const struct pci_attach_args * pa)
{
	const struct bce_product *bp;

	for (bp = bce_products; bp->bp_name != NULL; bp++) {
		if (PCI_VENDOR(pa->pa_id) == bp->bp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == bp->bp_product)
			return (bp);
	}

	return (NULL);
}

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against drivers product list, and return its name if a match is found.
 */
static int
bce_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (bce_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
bce_attach(device_t parent, device_t self, void *aux)
{
	struct bce_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	const struct bce_product *bp;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char     *intrstr = NULL;
	uint32_t	command;
	pcireg_t	memtype, pmode;
	bus_addr_t	memaddr;
	bus_size_t	memsize;
	void		*kva;
	bus_dma_segment_t seg;
	int             error, i, pmreg, rseg;
	struct ifnet   *ifp;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->bce_dev = self;

	bp = bce_lookup(pa);
	KASSERT(bp != NULL);

	sc->bce_pa = *pa;

	/* BCM440x can only address 30 bits (1GB) */
	if (bus_dmatag_subregion(pa->pa_dmat, 0, (1 << 30),
	    &(sc->bce_dmatag), BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(self,
		    "WARNING: failed to restrict dma range,"
		    " falling back to parent bus dma range\n");
		sc->bce_dmatag = pa->pa_dmat;
	}

	 aprint_naive(": Ethernet controller\n");
	 aprint_normal(": %s\n", bp->bp_name);

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		aprint_error_dev(self, "failed to enable memory mapping!\n");
		return;
	}
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BCE_PCI_BAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		if (pci_mapreg_map(pa, BCE_PCI_BAR0, memtype, 0, &sc->bce_btag,
		    &sc->bce_bhandle, &memaddr, &memsize) == 0)
			break;
	default:
		aprint_error_dev(self, "unable to find mem space\n");
		return;
	}

	/* Get it out of power save mode if needed. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, NULL)) {
		pmode = pci_conf_read(pc, pa->pa_tag, pmreg + 4) & 0x3;
		if (pmode == 3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			aprint_error_dev(self,
			    "unable to wake up from power state D3\n");
			return;
		}
		if (pmode != 0) {
			aprint_normal_dev(self,
			    "waking up from power state D%d\n", pmode);
			pci_conf_write(pc, pa->pa_tag, pmreg + 4, 0);
		}
	}
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	sc->bce_intrhand = pci_intr_establish(pc, ih, IPL_NET, bce_intr, sc);

	if (sc->bce_intrhand == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* reset the chip */
	bce_reset(sc);

	/*
	 * Allocate DMA-safe memory for ring descriptors.
	 * The receive, and transmit rings can not share the same
	 * 4k space, however both are allocated at once here.
	 */
	/*
	 * XXX PAGE_SIZE is wasteful; we only need 1KB + 1KB, but
	 * due to the limition above. ??
	 */
	if ((error = bus_dmamem_alloc(sc->bce_dmatag,
	    2 * PAGE_SIZE, PAGE_SIZE, 2 * PAGE_SIZE,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT))) {
		aprint_error_dev(self,
		    "unable to alloc space for ring descriptors, error = %d\n",
		    error);
		return;
	}
	/* map ring space to kernel */
	if ((error = bus_dmamem_map(sc->bce_dmatag, &seg, rseg,
	    2 * PAGE_SIZE, &kva, BUS_DMA_NOWAIT))) {
		aprint_error_dev(self,
		    "unable to map DMA buffers, error = %d\n", error);
		bus_dmamem_free(sc->bce_dmatag, &seg, rseg);
		return;
	}
	/* create a dma map for the ring */
	if ((error = bus_dmamap_create(sc->bce_dmatag,
	    2 * PAGE_SIZE, 1, 2 * PAGE_SIZE, 0, BUS_DMA_NOWAIT,
	    &sc->bce_ring_map))) {
		aprint_error_dev(self,
		    "unable to create ring DMA map, error = %d\n", error);
		bus_dmamem_unmap(sc->bce_dmatag, kva, 2 * PAGE_SIZE);
		bus_dmamem_free(sc->bce_dmatag, &seg, rseg);
		return;
	}
	/* connect the ring space to the dma map */
	if (bus_dmamap_load(sc->bce_dmatag, sc->bce_ring_map, kva,
	    2 * PAGE_SIZE, NULL, BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(sc->bce_dmatag, sc->bce_ring_map);
		bus_dmamem_unmap(sc->bce_dmatag, kva, 2 * PAGE_SIZE);
		bus_dmamem_free(sc->bce_dmatag, &seg, rseg);
		return;
	}
	/* save the ring space in softc */
	sc->bce_rx_ring = (struct bce_dma_slot *) kva;
	sc->bce_tx_ring = (struct bce_dma_slot *) ((char *)kva + PAGE_SIZE);

	/* Create the transmit buffer DMA maps. */
	for (i = 0; i < BCE_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->bce_dmatag, MCLBYTES,
		    BCE_NTXFRAGS, MCLBYTES, 0, 0, &sc->bce_cdata.bce_tx_map[i])) != 0) {
			aprint_error_dev(self,
			    "unable to create tx DMA map, error = %d\n", error);
		}
		sc->bce_cdata.bce_tx_chain[i] = NULL;
	}

	/* Create the receive buffer DMA maps. */
	for (i = 0; i < BCE_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->bce_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->bce_cdata.bce_rx_map[i])) != 0) {
			aprint_error_dev(self,
			    "unable to create rx DMA map, error = %d\n", error);
		}
		sc->bce_cdata.bce_rx_chain[i] = NULL;
	}

	/* Set up ifnet structure */
	ifp = &sc->ethercom.ec_if;
	strcpy(ifp->if_xname, device_xname(self));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bce_ioctl;
	ifp->if_start = bce_start;
	ifp->if_watchdog = bce_watchdog;
	ifp->if_init = bce_init;
	ifp->if_stop = bce_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize our media structures and probe the MII. */

	sc->bce_mii.mii_ifp = ifp;
	sc->bce_mii.mii_readreg = bce_mii_read;
	sc->bce_mii.mii_writereg = bce_mii_write;
	sc->bce_mii.mii_statchg = bce_statchg;

	sc->ethercom.ec_mii = &sc->bce_mii;
	ifmedia_init(&sc->bce_mii.mii_media, 0, ether_mediachange,
	    ether_mediastatus);
	mii_attach(sc->bce_dev, &sc->bce_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_FORCEANEG|MIIF_DOPAUSE);
	if (LIST_FIRST(&sc->bce_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->bce_mii.mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&sc->bce_mii.mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&sc->bce_mii.mii_media, IFM_ETHER | IFM_AUTO);
	/* get the phy */
	sc->bce_phy = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_PHY) & 0x1f;
	/*
	 * Enable activity led.
	 * XXX This should be in a phy driver, but not currently.
	 */
	bce_mii_write(sc->bce_dev, 1, 26,	 /* MAGIC */
	    bce_mii_read(sc->bce_dev, 1, 26) & 0x7fff);	 /* MAGIC */
	/* enable traffic meter led mode */
	bce_mii_write(sc->bce_dev, 1, 27,	 /* MAGIC */
	    bce_mii_read(sc->bce_dev, 1, 27) | (1 << 6));	 /* MAGIC */

	/* Attach the interface */
	if_attach(ifp);
	sc->enaddr[0] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET0);
	sc->enaddr[1] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET1);
	sc->enaddr[2] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET2);
	sc->enaddr[3] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET3);
	sc->enaddr[4] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET4);
	sc->enaddr[5] = bus_space_read_1(sc->bce_btag, sc->bce_bhandle,
	    BCE_MAGIC_ENET5);
	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->enaddr));
	ether_ifattach(ifp, sc->enaddr);
	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);
	callout_init(&sc->bce_timeout, 0);

	if (pmf_device_register(self, NULL, bce_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

/* handle media, and ethernet requests */
static int
bce_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int		s, error;

	s = splnet();
	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		/* change multicast list */
		error = 0;
	}

	/* Try to get more packets going. */
	bce_start(ifp);

	splx(s);
	return error;
}

/* Start packet transmission on the interface. */
static void
bce_start(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;
	struct mbuf    *m0;
	bus_dmamap_t	dmamap;
	int		txstart;
	int		txsfree;
	int		newpkts = 0;
	int		error;

	/*
	 * do not start another if currently transmitting, and more
	 * descriptors(tx slots) are needed for next packet.
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/* determine number of descriptors available */
	if (sc->bce_txsnext >= sc->bce_txin)
		txsfree = BCE_NTXDESC - 1 + sc->bce_txin - sc->bce_txsnext;
	else
		txsfree = sc->bce_txin - sc->bce_txsnext - 1;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (txsfree > 0) {
		int		seg;

		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/* get the transmit slot dma map */
		dmamap = sc->bce_cdata.bce_tx_map[sc->bce_txsnext];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources. If the packet will not fit,
		 * it will be dropped. If short on resources, it will
		 * be tried again later.
		 */
		error = bus_dmamap_load_mbuf(sc->bce_dmatag, dmamap, m0,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			aprint_error_dev(sc->bce_dev,
			    "Tx packet consumes too many DMA segments, "
			    "dropping...\n");
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			m_freem(m0);
			ifp->if_oerrors++;
			continue;
		} else if (error) {
			/* short on resources, come back later */
			aprint_error_dev(sc->bce_dev,
			    "unable to load Tx buffer, error = %d\n",
			    error);
			break;
		}
		/* If not enough descriptors available, try again later */
		if (dmamap->dm_nsegs > txsfree) {
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->bce_dmatag, dmamap);
			break;
		}
		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		/* So take it off the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/* save the pointer so it can be freed later */
		sc->bce_cdata.bce_tx_chain[sc->bce_txsnext] = m0;

		/* Sync the data DMA map. */
		bus_dmamap_sync(sc->bce_dmatag, dmamap, 0, dmamap->dm_mapsize,
				BUS_DMASYNC_PREWRITE);

		/* Initialize the transmit descriptor(s). */
		txstart = sc->bce_txsnext;
		for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
			uint32_t ctrl;

			ctrl = dmamap->dm_segs[seg].ds_len & CTRL_BC_MASK;
			if (seg == 0)
				ctrl |= CTRL_SOF;
			if (seg == dmamap->dm_nsegs - 1)
				ctrl |= CTRL_EOF;
			if (sc->bce_txsnext == BCE_NTXDESC - 1)
				ctrl |= CTRL_EOT;
			ctrl |= CTRL_IOC;
			sc->bce_tx_ring[sc->bce_txsnext].ctrl = htole32(ctrl);
			sc->bce_tx_ring[sc->bce_txsnext].addr =
			    htole32(dmamap->dm_segs[seg].ds_addr + 0x40000000);	/* MAGIC */
			if (sc->bce_txsnext + 1 > BCE_NTXDESC - 1)
				sc->bce_txsnext = 0;
			else
				sc->bce_txsnext++;
			txsfree--;
		}
		/* sync descriptors being used */
		if ( sc->bce_txsnext > txstart ) {
			bus_dmamap_sync(sc->bce_dmatag, sc->bce_ring_map,
			    PAGE_SIZE + sizeof(struct bce_dma_slot) * txstart,
			    sizeof(struct bce_dma_slot) * dmamap->dm_nsegs,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		} else {
			bus_dmamap_sync(sc->bce_dmatag, sc->bce_ring_map,
			    PAGE_SIZE + sizeof(struct bce_dma_slot) * txstart,
			    sizeof(struct bce_dma_slot) *
			    (BCE_NTXDESC - txstart),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			if ( sc->bce_txsnext != 0 ) {
				bus_dmamap_sync(sc->bce_dmatag,
				    sc->bce_ring_map, PAGE_SIZE,
				    sc->bce_txsnext *
				    sizeof(struct bce_dma_slot),
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			}
		}

		/* Give the packet to the chip. */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_DPTR,
		    sc->bce_txsnext * sizeof(struct bce_dma_slot));

		newpkts++;

		/* Pass the packet to any BPF listeners. */
		bpf_mtap(ifp, m0);
	}
	if (txsfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}
	if (newpkts) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/* Watchdog timer handler. */
static void
bce_watchdog(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;

	aprint_error_dev(sc->bce_dev, "device timeout\n");
	ifp->if_oerrors++;

	(void) bce_init(ifp);

	/* Try to get more packets going. */
	bce_start(ifp);
}

int
bce_intr(void *xsc)
{
	struct bce_softc *sc;
	struct ifnet   *ifp;
	uint32_t	intstatus;
	int		wantinit;
	int		handled = 0;

	sc = xsc;
	ifp = &sc->ethercom.ec_if;

	for (wantinit = 0; wantinit == 0;) {
		intstatus = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_INT_STS);

		/* ignore if not ours, or unsolicited interrupts */
		intstatus &= sc->bce_intmask;
		if (intstatus == 0)
			break;

		handled = 1;

		/* Ack interrupt */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_INT_STS,
		    intstatus);

		/* Receive interrupts. */
		if (intstatus & I_RI)
			bce_rxintr(sc);
		/* Transmit interrupts. */
		if (intstatus & I_XI)
			bce_txintr(sc);
		/* Error interrupts */
		if (intstatus & ~(I_RI | I_XI)) {
			const char *msg = NULL;
			if (intstatus & I_XU)
				msg = "transmit fifo underflow";
			if (intstatus & I_RO) {
				msg = "receive fifo overflow";
				ifp->if_ierrors++;
			}
			if (intstatus & I_RU)
				msg = "receive descriptor underflow";
			if (intstatus & I_DE)
				msg = "descriptor protocol error";
			if (intstatus & I_PD)
				msg = "data error";
			if (intstatus & I_PC)
				msg = "descriptor error";
			if (intstatus & I_TO)
				msg = "general purpose timeout";
			if (msg != NULL)
				aprint_error_dev(sc->bce_dev, "%s\n", msg);
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			bce_init(ifp);
		rnd_add_uint32(&sc->rnd_source, intstatus);
		/* Try to get more packets going. */
		bce_start(ifp);
	}
	return (handled);
}

/* Receive interrupt handler */
void
bce_rxintr(struct bce_softc *sc)
{
	struct ifnet   *ifp = &sc->ethercom.ec_if;
	struct rx_pph  *pph;
	struct mbuf    *m;
	int		curr;
	int		len;
	int		i;

	/* get pointer to active receive slot */
	curr = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXSTATUS)
	    & RS_CD_MASK;
	curr = curr / sizeof(struct bce_dma_slot);
	if (curr >= BCE_NRXDESC)
		curr = BCE_NRXDESC - 1;

	/* process packets up to but not current packet being worked on */
	for (i = sc->bce_rxin; i != curr;
	    i + 1 > BCE_NRXDESC - 1 ? i = 0 : i++) {
		/* complete any post dma memory ops on packet */
		bus_dmamap_sync(sc->bce_dmatag, sc->bce_cdata.bce_rx_map[i], 0,
		    sc->bce_cdata.bce_rx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		/*
		 * If the packet had an error, simply recycle the buffer,
		 * resetting the len, and flags.
		 */
		pph = mtod(sc->bce_cdata.bce_rx_chain[i], struct rx_pph *);
		if (pph->flags & (RXF_NO | RXF_RXER | RXF_CRC | RXF_OV)) {
			ifp->if_ierrors++;
			pph->len = 0;
			pph->flags = 0;
			continue;
		}
		/* receive the packet */
		len = pph->len;
		if (len == 0)
			continue;	/* no packet if empty */
		pph->len = 0;
		pph->flags = 0;
		/* bump past pre header to packet */
		sc->bce_cdata.bce_rx_chain[i]->m_data += 30;	/* MAGIC */

		/*
		 * The chip includes the CRC with every packet.  Trim
		 * it off here.
		 */
		len -= ETHER_CRC_LEN;

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when receiving lots
		 * of small packets.
		 *
		 * Otherwise, add a new buffer to the receive
		 * chain.  If this fails, drop the packet and
		 * recycle the old buffer.
		 */
		if (len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			memcpy(mtod(m, void *),
			 mtod(sc->bce_cdata.bce_rx_chain[i], void *), len);
			sc->bce_cdata.bce_rx_chain[i]->m_data -= 30;	/* MAGIC */
		} else {
			m = sc->bce_cdata.bce_rx_chain[i];
			if (bce_add_rxbuf(sc, i) != 0) {
		dropit:
				ifp->if_ierrors++;
				/* continue to use old buffer */
				sc->bce_cdata.bce_rx_chain[i]->m_data -= 30;
				bus_dmamap_sync(sc->bce_dmatag,
				    sc->bce_cdata.bce_rx_map[i], 0,
				    sc->bce_cdata.bce_rx_map[i]->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		ifp->if_ipackets++;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input) (ifp, m);

		/* re-check current in case it changed */
		curr = (bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_DMA_RXSTATUS) & RS_CD_MASK) /
		    sizeof(struct bce_dma_slot);
		if (curr >= BCE_NRXDESC)
			curr = BCE_NRXDESC - 1;
	}
	sc->bce_rxin = curr;
}

/* Transmit interrupt handler */
void
bce_txintr(struct bce_softc *sc)
{
	struct ifnet   *ifp = &sc->ethercom.ec_if;
	int		curr;
	int		i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	curr = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_TXSTATUS) &
		RS_CD_MASK;
	curr = curr / sizeof(struct bce_dma_slot);
	if (curr >= BCE_NTXDESC)
		curr = BCE_NTXDESC - 1;
	for (i = sc->bce_txin; i != curr;
	    i + 1 > BCE_NTXDESC - 1 ? i = 0 : i++) {
		/* do any post dma memory ops on transmit data */
		if (sc->bce_cdata.bce_tx_chain[i] == NULL)
			continue;
		bus_dmamap_sync(sc->bce_dmatag, sc->bce_cdata.bce_tx_map[i], 0,
		    sc->bce_cdata.bce_tx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->bce_dmatag, sc->bce_cdata.bce_tx_map[i]);
		m_freem(sc->bce_cdata.bce_tx_chain[i]);
		sc->bce_cdata.bce_tx_chain[i] = NULL;
		ifp->if_opackets++;
	}
	sc->bce_txin = curr;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer
	 */
	if (sc->bce_txsnext == sc->bce_txin)
		ifp->if_timer = 0;
}

/* initialize the interface */
static int
bce_init(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;
	uint32_t	reg_win;
	int		error;
	int		i;

	/* Cancel any pending I/O. */
	bce_stop(ifp, 0);

	/* enable pci inerrupts, bursts, and prefetch */

	/* remap the pci registers to the Sonics config registers */

	/* save the current map, so it can be restored */
	reg_win = pci_conf_read(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag,
	    BCE_REG_WIN);

	/* set register window to Sonics registers */
	pci_conf_write(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag, BCE_REG_WIN,
	    BCE_SONICS_WIN);

	/* enable SB to PCI interrupt */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBINTVEC,
	    bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBINTVEC) |
	    SBIV_ENET0);

	/* enable prefetch and bursts for sonics-to-pci translation 2 */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SPCI_TR2,
	    bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SPCI_TR2) |
	    SBTOPCI_PREF | SBTOPCI_BURST);

	/* restore to ethernet register space */
	pci_conf_write(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag, BCE_REG_WIN,
	    reg_win);

	/* Reset the chip to a known state. */
	bce_reset(sc);

	/* Initialize transmit descriptors */
	memset(sc->bce_tx_ring, 0, BCE_NTXDESC * sizeof(struct bce_dma_slot));
	sc->bce_txsnext = 0;
	sc->bce_txin = 0;

	/* enable crc32 generation */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MACCTL,
	    bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_MACCTL) |
	    BCE_EMC_CG);

	/* setup DMA interrupt control */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMAI_CTL, 1 << 24);	/* MAGIC */

	/* setup packet filter */
	bce_set_filter(ifp);

	/* set max frame length, account for possible vlan tag */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_MAX,
	    ETHER_MAX_LEN + 32);
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_TX_MAX,
	    ETHER_MAX_LEN + 32);

	/* set tx watermark */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_TX_WATER, 56);

	/* enable transmit */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_TXCTL, XC_XE);
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_TXADDR,
	    sc->bce_ring_map->dm_segs[0].ds_addr + PAGE_SIZE + 0x40000000);	/* MAGIC */

	/*
	 * Give the receive ring to the chip, and
	 * start the receive DMA engine.
	 */
	sc->bce_rxin = 0;

	/* clear the rx descriptor ring */
	memset(sc->bce_rx_ring, 0, BCE_NRXDESC * sizeof(struct bce_dma_slot));
	/* enable receive */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXCTL,
	    30 << 1 | 1);	/* MAGIC */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXADDR,
	    sc->bce_ring_map->dm_segs[0].ds_addr + 0x40000000);		/* MAGIC */

	/* Initalize receive descriptors */
	for (i = 0; i < BCE_NRXDESC; i++) {
		if (sc->bce_cdata.bce_rx_chain[i] == NULL) {
			if ((error = bce_add_rxbuf(sc, i)) != 0) {
				aprint_error_dev(sc->bce_dev,
				    "unable to allocate or map rx(%d) "
				    "mbuf, error = %d\n", i, error);
				bce_rxdrain(sc);
				return (error);
			}
		} else
			BCE_INIT_RXDESC(sc, i);
	}

	/* Enable interrupts */
	sc->bce_intmask =
	    I_XI | I_RI | I_XU | I_RO | I_RU | I_DE | I_PD | I_PC | I_TO;
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_INT_MASK,
	    sc->bce_intmask);

	/* start the receive dma */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXDPTR,
	    BCE_NRXDESC * sizeof(struct bce_dma_slot));

	/* set media */
	if ((error = ether_mediachange(ifp)) != 0)
		return error;

	/* turn on the ethernet mac */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_ENET_CTL,
	    bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
	    BCE_ENET_CTL) | EC_EE);

	/* start timer */
	callout_reset(&sc->bce_timeout, hz, bce_tick, sc);

	/* mark as running, and no outputs active */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}

/* add a mac address to packet filter */
void
bce_add_mac(struct bce_softc *sc, uint8_t *mac, u_long idx)
{
	int		i;
	uint32_t	rval;

	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_FILT_LOW,
	    mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5]);
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_FILT_HI,
	    mac[0] << 8 | mac[1] | 0x10000);	/* MAGIC */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_FILT_CTL,
	    idx << 16 | 8);	/* MAGIC */
	/* wait for write to complete */
	for (i = 0; i < 100; i++) {
		rval = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_FILT_CTL);
		if (!(rval & 0x80000000))	/* MAGIC */
			break;
		delay(10);
	}
	if (i == 100) {
		aprint_error_dev(sc->bce_dev,
		    "timed out writing pkt filter ctl\n");
	}
}

/* Add a receive buffer to the indiciated descriptor. */
static int
bce_add_rxbuf(struct bce_softc *sc, int idx)
{
	struct mbuf    *m;
	int		error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	if (sc->bce_cdata.bce_rx_chain[idx] != NULL)
		bus_dmamap_unload(sc->bce_dmatag,
		    sc->bce_cdata.bce_rx_map[idx]);

	sc->bce_cdata.bce_rx_chain[idx] = m;

	error = bus_dmamap_load(sc->bce_dmatag, sc->bce_cdata.bce_rx_map[idx],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error)
		return (error);

	bus_dmamap_sync(sc->bce_dmatag, sc->bce_cdata.bce_rx_map[idx], 0,
	    sc->bce_cdata.bce_rx_map[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	BCE_INIT_RXDESC(sc, idx);

	return (0);

}

/* Drain the receive queue. */
static void
bce_rxdrain(struct bce_softc *sc)
{
	int		i;

	for (i = 0; i < BCE_NRXDESC; i++) {
		if (sc->bce_cdata.bce_rx_chain[i] != NULL) {
			bus_dmamap_unload(sc->bce_dmatag,
			    sc->bce_cdata.bce_rx_map[i]);
			m_freem(sc->bce_cdata.bce_rx_chain[i]);
			sc->bce_cdata.bce_rx_chain[i] = NULL;
		}
	}
}

/* Stop transmission on the interface */
static void
bce_stop(struct ifnet *ifp, int disable)
{
	struct bce_softc *sc = ifp->if_softc;
	int		i;
	uint32_t	val;

	/* Stop the 1 second timer */
	callout_stop(&sc->bce_timeout);

	/* Down the MII. */
	mii_down(&sc->bce_mii);

	/* Disable interrupts. */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_INT_MASK, 0);
	sc->bce_intmask = 0;
	delay(10);

	/* Disable emac */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_ENET_CTL, EC_ED);
	for (i = 0; i < 200; i++) {
		val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_ENET_CTL);
		if (!(val & EC_ED))
			break;
		delay(10);
	}

	/* Stop the DMA */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXCTL, 0);
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_TXCTL, 0);
	delay(10);

	/* Release any queued transmit buffers. */
	for (i = 0; i < BCE_NTXDESC; i++) {
		if (sc->bce_cdata.bce_tx_chain[i] != NULL) {
			bus_dmamap_unload(sc->bce_dmatag,
			    sc->bce_cdata.bce_tx_map[i]);
			m_freem(sc->bce_cdata.bce_tx_chain[i]);
			sc->bce_cdata.bce_tx_chain[i] = NULL;
		}
	}

	/* Mark the interface down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* drain receive queue */
	if (disable)
		bce_rxdrain(sc);
}

/* reset the chip */
static void
bce_reset(struct bce_softc *sc)
{
	uint32_t	val;
	uint32_t	sbval;
	int		i;

	/* if SB core is up */
	sbval = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
	    BCE_SBTMSTATELOW);
	if ((sbval & (SBTML_RESET | SBTML_REJ | SBTML_CLK)) == SBTML_CLK) {
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMAI_CTL,
		    0);

		/* disable emac */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_ENET_CTL,
		    EC_ED);
		for (i = 0; i < 200; i++) {
			val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_ENET_CTL);
			if (!(val & EC_ED))
				break;
			delay(10);
		}
		if (i == 200) {
			aprint_error_dev(sc->bce_dev,
			    "timed out disabling ethernet mac\n");
		}

		/* reset the dma engines */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_TXCTL, 0);
		val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_DMA_RXSTATUS);
		/* if error on receive, wait to go idle */
		if (val & RS_ERROR) {
			for (i = 0; i < 100; i++) {
				val = bus_space_read_4(sc->bce_btag,
				    sc->bce_bhandle, BCE_DMA_RXSTATUS);
				if (val & RS_DMA_IDLE)
					break;
				delay(10);
			}
			if (i == 100) {
				aprint_error_dev(sc->bce_dev,
				    "receive dma did not go idle after"
				    " error\n");
			}
		}
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
		   BCE_DMA_RXSTATUS, 0);

		/* reset ethernet mac */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_ENET_CTL,
		    EC_ES);
		for (i = 0; i < 200; i++) {
			val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_ENET_CTL);
			if (!(val & EC_ES))
				break;
			delay(10);
		}
		if (i == 200) {
			aprint_error_dev(sc->bce_dev,
			    "timed out resetting ethernet mac\n");
		}
	} else {
		uint32_t reg_win;

		/* remap the pci registers to the Sonics config registers */

		/* save the current map, so it can be restored */
		reg_win = pci_conf_read(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag,
		    BCE_REG_WIN);
		/* set register window to Sonics registers */
		pci_conf_write(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag,
		    BCE_REG_WIN, BCE_SONICS_WIN);

		/* enable SB to PCI interrupt */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBINTVEC,
		    bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			BCE_SBINTVEC) |
		    SBIV_ENET0);

		/* enable prefetch and bursts for sonics-to-pci translation 2 */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SPCI_TR2,
		    bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			BCE_SPCI_TR2) |
		    SBTOPCI_PREF | SBTOPCI_BURST);

		/* restore to ethernet register space */
		pci_conf_write(sc->bce_pa.pa_pc, sc->bce_pa.pa_tag, BCE_REG_WIN,
		    reg_win);
	}

	/* disable SB core if not in reset */
	if (!(sbval & SBTML_RESET)) {

		/* set the reject bit */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_SBTMSTATELOW, SBTML_REJ | SBTML_CLK);
		for (i = 0; i < 200; i++) {
			val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_SBTMSTATELOW);
			if (val & SBTML_REJ)
				break;
			delay(1);
		}
		if (i == 200) {
			aprint_error_dev(sc->bce_dev,
			    "while resetting core, reject did not set\n");
		}
		/* wait until busy is clear */
		for (i = 0; i < 200; i++) {
			val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_SBTMSTATEHI);
			if (!(val & 0x4))
				break;
			delay(1);
		}
		if (i == 200) {
			aprint_error_dev(sc->bce_dev,
			    "while resetting core, busy did not clear\n");
		}
		/* set reset and reject while enabling the clocks */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_SBTMSTATELOW,
		    SBTML_FGC | SBTML_CLK | SBTML_REJ | SBTML_RESET);
		val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_SBTMSTATELOW);
		delay(10);
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_SBTMSTATELOW, SBTML_REJ | SBTML_RESET);
		delay(1);
	}
	/* enable clock */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_RESET);
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW);
	delay(1);

	/* clear any error bits that may be on */
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATEHI);
	if (val & 1)
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATEHI,
		    0);
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBIMSTATE);
	if (val & SBIM_MAGIC_ERRORBITS)
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBIMSTATE,
		    val & ~SBIM_MAGIC_ERRORBITS);

	/* clear reset and allow it to propagate throughout the core */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK);
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW);
	delay(1);

	/* leave clock enabled */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW,
	    SBTML_CLK);
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_SBTMSTATELOW);
	delay(1);

	/* initialize MDC preamble, frequency */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_CTL, 0x8d);	/* MAGIC */

	/* enable phy, differs for internal, and external */
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_DEVCTL);
	if (!(val & BCE_DC_IP)) {
		/* select external phy */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_ENET_CTL, EC_EP);
	} else if (val & BCE_DC_ER) {	/* internal, clear reset bit if on */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_DEVCTL,
		    val & ~BCE_DC_ER);
		delay(100);
	}
}

/* Set up the receive filter. */
void
bce_set_filter(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_CTL,
		    bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_CTL)
		    | ERC_PE);
	} else {
		ifp->if_flags &= ~IFF_ALLMULTI;

		/* turn off promiscuous */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_CTL,
		    bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_RX_CTL) & ~ERC_PE);

		/* enable/disable broadcast */
		if (ifp->if_flags & IFF_BROADCAST)
			bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_RX_CTL, bus_space_read_4(sc->bce_btag,
			    sc->bce_bhandle, BCE_RX_CTL) & ~ERC_DB);
		else
			bus_space_write_4(sc->bce_btag, sc->bce_bhandle,
			    BCE_RX_CTL, bus_space_read_4(sc->bce_btag,
			    sc->bce_bhandle, BCE_RX_CTL) | ERC_DB);

		/* disable the filter */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_FILT_CTL,
		    0);

		/* add our own address */
		bce_add_mac(sc, sc->enaddr, 0);

		/* for now accept all multicast */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_CTL,
		bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_RX_CTL) |
		    ERC_AM);
		ifp->if_flags |= IFF_ALLMULTI;

		/* enable the filter */
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_FILT_CTL,
		    bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_FILT_CTL) | 1);
	}
}

static bool
bce_resume(device_t self, const pmf_qual_t *qual)
{
	struct bce_softc *sc = device_private(self);

	bce_reset(sc);

	return true;
}

/* Read a PHY register on the MII. */
int
bce_mii_read(device_t self, int phy, int reg)
{
	struct bce_softc *sc = device_private(self);
	int		i;
	uint32_t	val;

	/* clear mii_int */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_STS, BCE_MIINTR);

	/* Read the PHY register */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_COMM,
	    (MII_COMMAND_READ << 28) | (MII_COMMAND_START << 30) |	/* MAGIC */
	    (MII_COMMAND_ACK << 16) | BCE_MIPHY(phy) | BCE_MIREG(reg));	/* MAGIC */

	for (i = 0; i < BCE_TIMEOUT; i++) {
		val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_STS);
		if (val & BCE_MIINTR)
			break;
		delay(10);
	}
	val = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_COMM);
	if (i == BCE_TIMEOUT) {
		aprint_error_dev(sc->bce_dev,
		    "PHY read timed out reading phy %d, reg %d, val = "
		    "0x%08x\n", phy, reg, val);
		return (0);
	}
	return (val & BCE_MICOMM_DATA);
}

/* Write a PHY register on the MII */
void
bce_mii_write(device_t self, int phy, int reg, int val)
{
	struct bce_softc *sc = device_private(self);
	int		i;
	uint32_t	rval;

	/* clear mii_int */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_STS,
	    BCE_MIINTR);

	/* Write the PHY register */
	bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_COMM,
	    (MII_COMMAND_WRITE << 28) | (MII_COMMAND_START << 30) |	/* MAGIC */
	    (MII_COMMAND_ACK << 16) | (val & BCE_MICOMM_DATA) |	/* MAGIC */
	    BCE_MIPHY(phy) | BCE_MIREG(reg));

	/* wait for write to complete */
	for (i = 0; i < BCE_TIMEOUT; i++) {
		rval = bus_space_read_4(sc->bce_btag, sc->bce_bhandle,
		    BCE_MI_STS);
		if (rval & BCE_MIINTR)
			break;
		delay(10);
	}
	rval = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_MI_COMM);
	if (i == BCE_TIMEOUT) {
		aprint_error_dev(sc->bce_dev,
		    "PHY timed out writing phy %d, reg %d, val = 0x%08x\n", phy,
		    reg, val);
	}
}

/* sync hardware duplex mode to software state */
void
bce_statchg(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;
	uint32_t	reg;

	/* if needed, change register to match duplex mode */
	reg = bus_space_read_4(sc->bce_btag, sc->bce_bhandle, BCE_TX_CTL);
	if (sc->bce_mii.mii_media_active & IFM_FDX && !(reg & EXC_FD))
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_TX_CTL,
		    reg | EXC_FD);
	else if (!(sc->bce_mii.mii_media_active & IFM_FDX) && reg & EXC_FD)
		bus_space_write_4(sc->bce_btag, sc->bce_bhandle, BCE_TX_CTL,
		    reg & ~EXC_FD);

	/*
	 * Enable activity led.
	 * XXX This should be in a phy driver, but not currently.
	 */
	bce_mii_write(sc->bce_dev, 1, 26,	/* MAGIC */
	    bce_mii_read(sc->bce_dev, 1, 26) & 0x7fff);	/* MAGIC */
	/* enable traffic meter led mode */
	bce_mii_write(sc->bce_dev, 1, 26,	/* MAGIC */
	    bce_mii_read(sc->bce_dev, 1, 27) | (1 << 6));	/* MAGIC */
}

/* One second timer, checks link status */
static void
bce_tick(void *v)
{
	struct bce_softc *sc = v;

	/* Tick the MII. */
	mii_tick(&sc->bce_mii);

	callout_reset(&sc->bce_timeout, hz, bce_tick, sc);
}
