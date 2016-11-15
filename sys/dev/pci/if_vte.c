/*	$NetBSD: if_vte.c,v 1.12 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 2011 Manuel Bouyer.  All rights reserved.
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
 * Copyright (c) 2010, Pyun YongHyeon <yongari@FreeBSD.org>
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
/* FreeBSD: src/sys/dev/vte/if_vte.c,v 1.2 2010/12/31 01:23:04 yongari Exp */

/* Driver for DM&P Electronics, Inc, Vortex86 RDC R6040 FastEthernet. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_vte.c,v 1.12 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/rndsource.h>

#include "opt_inet.h"
#include <net/if_ether.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/if_vtereg.h>
#include <dev/pci/if_vtevar.h>

static int	vte_match(device_t, cfdata_t, void *);
static void	vte_attach(device_t, device_t, void *);
static int	vte_detach(device_t, int);
static int	vte_dma_alloc(struct vte_softc *);
static void	vte_dma_free(struct vte_softc *);
static struct vte_txdesc *
		vte_encap(struct vte_softc *, struct mbuf **);
static void	vte_get_macaddr(struct vte_softc *);
static int	vte_init(struct ifnet *);
static int	vte_init_rx_ring(struct vte_softc *);
static int	vte_init_tx_ring(struct vte_softc *);
static int	vte_intr(void *);
static int	vte_ifioctl(struct ifnet *, u_long, void *);
static void	vte_mac_config(struct vte_softc *);
static int	vte_miibus_readreg(device_t, int, int);
static void	vte_miibus_statchg(struct ifnet *);
static void	vte_miibus_writereg(device_t, int, int, int);
static int	vte_mediachange(struct ifnet *);
static int	vte_newbuf(struct vte_softc *, struct vte_rxdesc *);
static void	vte_reset(struct vte_softc *);
static void	vte_rxeof(struct vte_softc *);
static void	vte_rxfilter(struct vte_softc *);
static bool	vte_shutdown(device_t, int);
static bool	vte_suspend(device_t, const pmf_qual_t *);
static bool	vte_resume(device_t, const pmf_qual_t *);
static void	vte_ifstart(struct ifnet *);
static void	vte_start_mac(struct vte_softc *);
static void	vte_stats_clear(struct vte_softc *);
static void	vte_stats_update(struct vte_softc *);
static void	vte_stop(struct ifnet *, int);
static void	vte_stop_mac(struct vte_softc *);
static void	vte_tick(void *);
static void	vte_txeof(struct vte_softc *);
static void	vte_ifwatchdog(struct ifnet *);

static int vte_sysctl_intrxct(SYSCTLFN_PROTO);
static int vte_sysctl_inttxct(SYSCTLFN_PROTO);
static int vte_root_num;

#define DPRINTF(a)

CFATTACH_DECL3_NEW(vte, sizeof(struct vte_softc),
    vte_match, vte_attach, vte_detach, NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);


static int
vte_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_RDC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RDC_R6040)
		return 1;

	return 0;
}

static void
vte_attach(device_t parent, device_t self, void *aux)
{
	struct vte_softc *sc = device_private(self);
	struct pci_attach_args * const pa = (struct pci_attach_args *)aux;
	struct ifnet * const ifp = &sc->vte_if;
	int h_valid;
	pcireg_t reg, csr;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	int error;
	const struct sysctlnode *node;
	int vte_nodenum;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->vte_dev = self;

	callout_init(&sc->vte_tick_ch, 0);

	/* Map the device. */
	h_valid = 0;
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, VTE_PCI_BMEM);
	if (PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_MEM) {
		h_valid = (pci_mapreg_map(pa, VTE_PCI_BMEM,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
		    0, &sc->vte_bustag, &sc->vte_bushandle, NULL, NULL) == 0);
	}
	if (h_valid == 0) {
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, VTE_PCI_BIO);
		if (PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_IO) {
			h_valid = (pci_mapreg_map(pa, VTE_PCI_BIO,
			    PCI_MAPREG_TYPE_IO, 0, &sc->vte_bustag,
			    &sc->vte_bushandle, NULL, NULL) == 0);
		}
	}
	if (h_valid == 0) {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}
	sc->vte_dmatag = pa->pa_dmat;
	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	pci_aprint_devinfo(pa, NULL);

	/* Reset the ethernet controller. */
	vte_reset(sc);

	if ((error = vte_dma_alloc(sc)) != 0)
		return;

	/* Load station address. */
	vte_get_macaddr(sc);

	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->vte_eaddr));

	/* Map and establish interrupts */
	if (pci_intr_map(pa, &intrhandle)) {
	    aprint_error_dev(self, "couldn't map interrupt\n");
	    return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	sc->vte_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
	    vte_intr, sc);
	if (sc->vte_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->vte_if.if_softc = sc;
	sc->vte_mii.mii_ifp = ifp;
	sc->vte_mii.mii_readreg = vte_miibus_readreg;
	sc->vte_mii.mii_writereg = vte_miibus_writereg;
	sc->vte_mii.mii_statchg = vte_miibus_statchg;
	sc->vte_ec.ec_mii = &sc->vte_mii;
	ifmedia_init(&sc->vte_mii.mii_media, IFM_IMASK, vte_mediachange,
	    ether_mediastatus);
	mii_attach(self, &sc->vte_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->vte_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->vte_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->vte_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->vte_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->vte_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;

        strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
        ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
        ifp->if_ioctl = vte_ifioctl;  
        ifp->if_start = vte_ifstart;  
        ifp->if_watchdog = vte_ifwatchdog;
        ifp->if_init = vte_init;      
        ifp->if_stop = vte_stop;      
        ifp->if_timer = 0;
        IFQ_SET_READY(&ifp->if_snd); 
        if_attach(ifp);
        ether_ifattach(&(sc)->vte_if, (sc)->vte_eaddr);

	if (pmf_device_register1(self, vte_suspend, vte_resume, vte_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

        rnd_attach_source(&sc->rnd_source, device_xname(self),
            RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (sysctl_createv(&sc->vte_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, device_xname(sc->vte_dev),
	    SYSCTL_DESCR("vte per-controller controls"),
	    NULL, 0, NULL, 0, CTL_HW, vte_root_num, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->vte_dev, "couldn't create sysctl node\n");
		return;
	}
	vte_nodenum = node->sysctl_num;
	if (sysctl_createv(&sc->vte_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_rxct",
	    SYSCTL_DESCR("vte RX interrupt moderation packet counter"),
	    vte_sysctl_intrxct, 0, (void *)sc,
	    0, CTL_HW, vte_root_num, vte_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->vte_dev,
		    "couldn't create int_rxct sysctl node\n");
	}
	if (sysctl_createv(&sc->vte_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_txct",
	    SYSCTL_DESCR("vte TX interrupt moderation packet counter"),
	    vte_sysctl_inttxct, 0, (void *)sc,
	    0, CTL_HW, vte_root_num, vte_nodenum, CTL_CREATE,
	    CTL_EOL) != 0) {
		aprint_normal_dev(sc->vte_dev,
		    "couldn't create int_txct sysctl node\n");
	}
}

static int
vte_detach(device_t dev, int flags __unused)
{
	struct vte_softc *sc = device_private(dev);
	struct ifnet *ifp = &sc->vte_if;
	int s;

	s = splnet();
	/* Stop the interface. Callouts are stopped in it. */
	vte_stop(ifp, 1);
	splx(s);

	pmf_device_deregister(dev);

	mii_detach(&sc->vte_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->vte_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	vte_dma_free(sc);

	return (0);
}

static int
vte_miibus_readreg(device_t dev, int phy, int reg)
{
	struct vte_softc *sc = device_private(dev);
	int i;

	CSR_WRITE_2(sc, VTE_MMDIO, MMDIO_READ |
	    (phy << MMDIO_PHY_ADDR_SHIFT) | (reg << MMDIO_REG_ADDR_SHIFT));
	for (i = VTE_PHY_TIMEOUT; i > 0; i--) {
		DELAY(5);
		if ((CSR_READ_2(sc, VTE_MMDIO) & MMDIO_READ) == 0)
			break;
	}

	if (i == 0) {
		aprint_error_dev(sc->vte_dev, "phy read timeout : %d\n", reg);
		return (0);
	}

	return (CSR_READ_2(sc, VTE_MMRD));
}

static void
vte_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct vte_softc *sc = device_private(dev);
	int i;

	CSR_WRITE_2(sc, VTE_MMWD, val);
	CSR_WRITE_2(sc, VTE_MMDIO, MMDIO_WRITE |
	    (phy << MMDIO_PHY_ADDR_SHIFT) | (reg << MMDIO_REG_ADDR_SHIFT));
	for (i = VTE_PHY_TIMEOUT; i > 0; i--) {
		DELAY(5);
		if ((CSR_READ_2(sc, VTE_MMDIO) & MMDIO_WRITE) == 0)
			break;
	}

	if (i == 0)
		aprint_error_dev(sc->vte_dev, "phy write timeout : %d\n", reg);

}

static void
vte_miibus_statchg(struct ifnet *ifp)
{
	struct vte_softc *sc = ifp->if_softc;
	uint16_t val;

	DPRINTF(("vte_miibus_statchg 0x%x 0x%x\n",
	    sc->vte_mii.mii_media_status, sc->vte_mii.mii_media_active));

	sc->vte_flags &= ~VTE_FLAG_LINK;
	if ((sc->vte_mii.mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(sc->vte_mii.mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			sc->vte_flags |= VTE_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	/* Stop RX/TX MACs. */
	vte_stop_mac(sc);
	/* Program MACs with resolved duplex and flow control. */
	if ((sc->vte_flags & VTE_FLAG_LINK) != 0) {
		/*
		 * Timer waiting time : (63 + TIMER * 64) MII clock.
		 * MII clock : 25MHz(100Mbps) or 2.5MHz(10Mbps).
		 */
		if (IFM_SUBTYPE(sc->vte_mii.mii_media_active) == IFM_100_TX)
			val = 18 << VTE_IM_TIMER_SHIFT;
		else
			val = 1 << VTE_IM_TIMER_SHIFT;
		val |= sc->vte_int_rx_mod << VTE_IM_BUNDLE_SHIFT;
		/* 48.6us for 100Mbps, 50.8us for 10Mbps */
		CSR_WRITE_2(sc, VTE_MRICR, val);

		if (IFM_SUBTYPE(sc->vte_mii.mii_media_active) == IFM_100_TX)
			val = 18 << VTE_IM_TIMER_SHIFT;
		else
			val = 1 << VTE_IM_TIMER_SHIFT;
		val |= sc->vte_int_tx_mod << VTE_IM_BUNDLE_SHIFT;
		/* 48.6us for 100Mbps, 50.8us for 10Mbps */
		CSR_WRITE_2(sc, VTE_MTICR, val);

		vte_mac_config(sc);
		vte_start_mac(sc);
		DPRINTF(("vte_miibus_statchg: link\n"));
	}
}

static void
vte_get_macaddr(struct vte_softc *sc)
{
	uint16_t mid;

	/*
	 * It seems there is no way to reload station address and
	 * it is supposed to be set by BIOS.
	 */
	mid = CSR_READ_2(sc, VTE_MID0L);
	sc->vte_eaddr[0] = (mid >> 0) & 0xFF;
	sc->vte_eaddr[1] = (mid >> 8) & 0xFF;
	mid = CSR_READ_2(sc, VTE_MID0M);
	sc->vte_eaddr[2] = (mid >> 0) & 0xFF;
	sc->vte_eaddr[3] = (mid >> 8) & 0xFF;
	mid = CSR_READ_2(sc, VTE_MID0H);
	sc->vte_eaddr[4] = (mid >> 0) & 0xFF;
	sc->vte_eaddr[5] = (mid >> 8) & 0xFF;
}


static int
vte_dma_alloc(struct vte_softc *sc)
{
	struct vte_txdesc *txd;
	struct vte_rxdesc *rxd;
	int error, i, rseg;

	/* create DMA map for TX ring */
	error = bus_dmamap_create(sc->vte_dmatag, VTE_TX_RING_SZ, 1,
	    VTE_TX_RING_SZ, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &sc->vte_cdata.vte_tx_ring_map);
	if (error) {
		aprint_error_dev(sc->vte_dev,
		    "could not create dma map for TX ring (%d)\n",
		    error);
		goto fail;
	}
	/* Allocate and map DMA'able memory and load the DMA map for TX ring. */
	error = bus_dmamem_alloc(sc->vte_dmatag, VTE_TX_RING_SZ,
	    VTE_TX_RING_ALIGN, 0,
	    sc->vte_cdata.vte_tx_ring_seg, 1, &rseg,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not allocate DMA'able memory for TX ring (%d).\n",
		    error);
		goto fail;
	}
	KASSERT(rseg == 1);
	error = bus_dmamem_map(sc->vte_dmatag,
	    sc->vte_cdata.vte_tx_ring_seg, 1,
	    VTE_TX_RING_SZ, (void **)(&sc->vte_cdata.vte_tx_ring),
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not map DMA'able memory for TX ring (%d).\n",
		    error);
		goto fail;
	}
	memset(sc->vte_cdata.vte_tx_ring, 0, VTE_TX_RING_SZ);
	error = bus_dmamap_load(sc->vte_dmatag,
	    sc->vte_cdata.vte_tx_ring_map, sc->vte_cdata.vte_tx_ring,
	    VTE_TX_RING_SZ, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not load DMA'able memory for TX ring.\n");
		goto fail;
	}

	/* create DMA map for RX ring */
	error = bus_dmamap_create(sc->vte_dmatag, VTE_RX_RING_SZ, 1,
	    VTE_RX_RING_SZ, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &sc->vte_cdata.vte_rx_ring_map);
	if (error) {
		aprint_error_dev(sc->vte_dev,
		    "could not create dma map for RX ring (%d)\n",
		    error);
		goto fail;
	}
	/* Allocate and map DMA'able memory and load the DMA map for RX ring. */
	error = bus_dmamem_alloc(sc->vte_dmatag, VTE_RX_RING_SZ,
	    VTE_RX_RING_ALIGN, 0,
	    sc->vte_cdata.vte_rx_ring_seg, 1, &rseg,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not allocate DMA'able memory for RX ring (%d).\n",
		    error);
		goto fail;
	}
	KASSERT(rseg == 1);
	error = bus_dmamem_map(sc->vte_dmatag,
	    sc->vte_cdata.vte_rx_ring_seg, 1,
	    VTE_RX_RING_SZ, (void **)(&sc->vte_cdata.vte_rx_ring),
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not map DMA'able memory for RX ring (%d).\n",
		    error);
		goto fail;
	}
	memset(sc->vte_cdata.vte_rx_ring, 0, VTE_RX_RING_SZ);
	error = bus_dmamap_load(sc->vte_dmatag,
	    sc->vte_cdata.vte_rx_ring_map, sc->vte_cdata.vte_rx_ring,
	    VTE_RX_RING_SZ, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
	if (error != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not load DMA'able memory for RX ring (%d).\n",
		    error);
		goto fail;
	}

	/* Create DMA maps for TX buffers. */
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		txd = &sc->vte_cdata.vte_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_dmamap = NULL;
		error = bus_dmamap_create(sc->vte_dmatag, MCLBYTES,
		    1, MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &txd->tx_dmamap);
		if (error != 0) {
			aprint_error_dev(sc->vte_dev,
			    "could not create TX DMA map %d (%d).\n", i, error);
			goto fail;
		}
	}
	/* Create DMA maps for RX buffers. */
	if ((error = bus_dmamap_create(sc->vte_dmatag, MCLBYTES,
	    1, MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &sc->vte_cdata.vte_rx_sparemap)) != 0) {
		aprint_error_dev(sc->vte_dev,
		    "could not create spare RX dmamap (%d).\n", error);
		goto fail;
	}
	for (i = 0; i < VTE_RX_RING_CNT; i++) {
		rxd = &sc->vte_cdata.vte_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_dmamap = NULL;
		error = bus_dmamap_create(sc->vte_dmatag, MCLBYTES,
		    1, MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &rxd->rx_dmamap);
		if (error != 0) {
			aprint_error_dev(sc->vte_dev,
			    "could not create RX dmamap %d (%d).\n", i, error);
			goto fail;
		}
	}
	return 0;

fail:
	vte_dma_free(sc);
	return (error);
}

static void
vte_dma_free(struct vte_softc *sc)
{
	struct vte_txdesc *txd;
	struct vte_rxdesc *rxd;
	int i;

	/* TX buffers. */
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		txd = &sc->vte_cdata.vte_txdesc[i];
		if (txd->tx_dmamap != NULL) {
			bus_dmamap_destroy(sc->vte_dmatag, txd->tx_dmamap);
			txd->tx_dmamap = NULL;
		}
	}
	/* RX buffers */
	for (i = 0; i < VTE_RX_RING_CNT; i++) {
		rxd = &sc->vte_cdata.vte_rxdesc[i];
		if (rxd->rx_dmamap != NULL) {
			bus_dmamap_destroy(sc->vte_dmatag, rxd->rx_dmamap);
			rxd->rx_dmamap = NULL;
		}
	}
	if (sc->vte_cdata.vte_rx_sparemap != NULL) {
		bus_dmamap_destroy(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_sparemap);
		sc->vte_cdata.vte_rx_sparemap = NULL;
	}
	/* TX descriptor ring. */
	if (sc->vte_cdata.vte_tx_ring_map != NULL) {
		bus_dmamap_unload(sc->vte_dmatag,
		    sc->vte_cdata.vte_tx_ring_map);
		bus_dmamap_destroy(sc->vte_dmatag,
		    sc->vte_cdata.vte_tx_ring_map);
	}
	if (sc->vte_cdata.vte_tx_ring != NULL) {
		bus_dmamem_unmap(sc->vte_dmatag,
		    sc->vte_cdata.vte_tx_ring, VTE_TX_RING_SZ);
		bus_dmamem_free(sc->vte_dmatag,
		    sc->vte_cdata.vte_tx_ring_seg, 1);
	}
	sc->vte_cdata.vte_tx_ring = NULL;
	sc->vte_cdata.vte_tx_ring_map = NULL;
	/* RX ring. */
	if (sc->vte_cdata.vte_rx_ring_map != NULL) {
		bus_dmamap_unload(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_ring_map);
		bus_dmamap_destroy(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_ring_map);
	}
	if (sc->vte_cdata.vte_rx_ring != NULL) {
		bus_dmamem_unmap(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_ring, VTE_RX_RING_SZ);
		bus_dmamem_free(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_ring_seg, 1);
	}
	sc->vte_cdata.vte_rx_ring = NULL;
	sc->vte_cdata.vte_rx_ring_map = NULL;
}

static bool
vte_shutdown(device_t dev, int howto)
{

	return (vte_suspend(dev, NULL));
}

static bool
vte_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct vte_softc *sc = device_private(dev);
	struct ifnet *ifp = &sc->vte_if;

	DPRINTF(("vte_suspend if_flags 0x%x\n", ifp->if_flags));
	if ((ifp->if_flags & IFF_RUNNING) != 0)
		vte_stop(ifp, 1);
	return (0);
}

static bool
vte_resume(device_t dev, const pmf_qual_t *qual)
{
	struct vte_softc *sc = device_private(dev);
	struct ifnet *ifp;

	ifp = &sc->vte_if;
	if ((ifp->if_flags & IFF_UP) != 0) {
		ifp->if_flags &= ~IFF_RUNNING;
		vte_init(ifp);
	}

	return (0);
}

static struct vte_txdesc *
vte_encap(struct vte_softc *sc, struct mbuf **m_head)
{
	struct vte_txdesc *txd;
	struct mbuf *m, *n;
	int copy, error, padlen;

	txd = &sc->vte_cdata.vte_txdesc[sc->vte_cdata.vte_tx_prod];
	m = *m_head;
	/*
	 * Controller doesn't auto-pad, so we have to make sure pad
	 * short frames out to the minimum frame length.
	 */
	if (m->m_pkthdr.len < VTE_MIN_FRAMELEN)
		padlen = VTE_MIN_FRAMELEN - m->m_pkthdr.len;
	else
		padlen = 0;

	/*
	 * Controller does not support multi-fragmented TX buffers.
	 * Controller spends most of its TX processing time in
	 * de-fragmenting TX buffers.  Either faster CPU or more
	 * advanced controller DMA engine is required to speed up
	 * TX path processing.
	 * To mitigate the de-fragmenting issue, perform deep copy
	 * from fragmented mbuf chains to a pre-allocated mbuf
	 * cluster with extra cost of kernel memory.  For frames
	 * that is composed of single TX buffer, the deep copy is
	 * bypassed.
	 */
	copy = 0;
	if (m->m_next != NULL)
		copy++;
	if (padlen > 0 && (M_READONLY(m) ||
	    padlen > M_TRAILINGSPACE(m)))
		copy++;
	if (copy != 0) {
		n = sc->vte_cdata.vte_txmbufs[sc->vte_cdata.vte_tx_prod];
		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, char *));
		n->m_pkthdr.len = m->m_pkthdr.len;
		n->m_len = m->m_pkthdr.len;
		m = n;
		txd->tx_flags |= VTE_TXMBUF;
	}

	if (padlen > 0) {
		/* Zero out the bytes in the pad area. */
		bzero(mtod(m, char *) + m->m_pkthdr.len, padlen);
		m->m_pkthdr.len += padlen;
		m->m_len = m->m_pkthdr.len;
	}

	error = bus_dmamap_load_mbuf(sc->vte_dmatag, txd->tx_dmamap, m, 0);
	if (error != 0) {
		txd->tx_flags &= ~VTE_TXMBUF;
		return (NULL);
	}
	KASSERT(txd->tx_dmamap->dm_nsegs == 1);
	bus_dmamap_sync(sc->vte_dmatag, txd->tx_dmamap, 0,
	    txd->tx_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);

	txd->tx_desc->dtlen =
	    htole16(VTE_TX_LEN(txd->tx_dmamap->dm_segs[0].ds_len));
	txd->tx_desc->dtbp = htole32(txd->tx_dmamap->dm_segs[0].ds_addr);
	sc->vte_cdata.vte_tx_cnt++;
	/* Update producer index. */
	VTE_DESC_INC(sc->vte_cdata.vte_tx_prod, VTE_TX_RING_CNT);

	/* Finally hand over ownership to controller. */
	txd->tx_desc->dtst = htole16(VTE_DTST_TX_OWN);
	txd->tx_m = m;

	return (txd);
}

static void
vte_ifstart(struct ifnet *ifp)
{
	struct vte_softc *sc = ifp->if_softc;
	struct vte_txdesc *txd;
	struct mbuf *m_head, *m;
	int enq;

	ifp = &sc->vte_if;

	DPRINTF(("vte_ifstart 0x%x 0x%x\n", ifp->if_flags, sc->vte_flags));

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) !=
	    IFF_RUNNING || (sc->vte_flags & VTE_FLAG_LINK) == 0)
		return;

	for (enq = 0; !IFQ_IS_EMPTY(&ifp->if_snd); ) {
		/* Reserve one free TX descriptor. */
		if (sc->vte_cdata.vte_tx_cnt >= VTE_TX_RING_CNT - 1) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		DPRINTF(("vte_encap:"));
		if ((txd = vte_encap(sc, &m_head)) == NULL) {
			DPRINTF((" failed\n"));
			break;
		}
		DPRINTF((" ok\n"));
		IFQ_DEQUEUE(&ifp->if_snd, m);
		KASSERT(m == m_head);

		enq++;
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m_head);
		/* Free consumed TX frame. */
		if ((txd->tx_flags & VTE_TXMBUF) != 0)
			m_freem(m_head);
	}

	if (enq > 0) {
		bus_dmamap_sync(sc->vte_dmatag,
		    sc->vte_cdata.vte_tx_ring_map, 0,
		    sc->vte_cdata.vte_tx_ring_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		CSR_WRITE_2(sc, VTE_TX_POLL, TX_POLL_START);
		sc->vte_watchdog_timer = VTE_TX_TIMEOUT;
	}
}

static void
vte_ifwatchdog(struct ifnet *ifp)
{
	struct vte_softc *sc = ifp->if_softc;

	if (sc->vte_watchdog_timer == 0 || --sc->vte_watchdog_timer)
		return;

	aprint_error_dev(sc->vte_dev, "watchdog timeout -- resetting\n");
	ifp->if_oerrors++;
	vte_init(ifp);
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		vte_ifstart(ifp);
}

static int
vte_mediachange(struct ifnet *ifp)
{
	int error;
	struct vte_softc *sc = ifp->if_softc;

	if ((error = mii_mediachg(&sc->vte_mii)) == ENXIO)
		error = 0;
	else if (error != 0) {
		aprint_error_dev(sc->vte_dev, "could not set media\n");
		return error;
	}
											return 0;

}

static int
vte_ifioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct vte_softc *sc = ifp->if_softc;
	int error, s;

	s = splnet();
	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		DPRINTF(("vte_ifioctl if_flags 0x%x\n", ifp->if_flags));
		if (ifp->if_flags & IFF_RUNNING)
			vte_rxfilter(sc);
		error = 0;
	}
	splx(s);
	return error;
}

static void
vte_mac_config(struct vte_softc *sc)
{
	uint16_t mcr;

	mcr = CSR_READ_2(sc, VTE_MCR0);
	mcr &= ~(MCR0_FC_ENB | MCR0_FULL_DUPLEX);
	if ((IFM_OPTIONS(sc->vte_mii.mii_media_active) & IFM_FDX) != 0) {
		mcr |= MCR0_FULL_DUPLEX;
#ifdef notyet
		if ((IFM_OPTIONS(sc->vte_mii.mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			mcr |= MCR0_FC_ENB;
		/*
		 * The data sheet is not clear whether the controller
		 * honors received pause frames or not.  The is no
		 * separate control bit for RX pause frame so just
		 * enable MCR0_FC_ENB bit.
		 */
		if ((IFM_OPTIONS(sc->vte_mii.mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			mcr |= MCR0_FC_ENB;
#endif
	}
	CSR_WRITE_2(sc, VTE_MCR0, mcr);
}

static void
vte_stats_clear(struct vte_softc *sc)
{

	/* Reading counter registers clears its contents. */
	CSR_READ_2(sc, VTE_CNT_RX_DONE);
	CSR_READ_2(sc, VTE_CNT_MECNT0);
	CSR_READ_2(sc, VTE_CNT_MECNT1);
	CSR_READ_2(sc, VTE_CNT_MECNT2);
	CSR_READ_2(sc, VTE_CNT_MECNT3);
	CSR_READ_2(sc, VTE_CNT_TX_DONE);
	CSR_READ_2(sc, VTE_CNT_MECNT4);
	CSR_READ_2(sc, VTE_CNT_PAUSE);
}

static void
vte_stats_update(struct vte_softc *sc)
{
	struct vte_hw_stats *stat;
	struct ifnet *ifp = &sc->vte_if;
	uint16_t value;

	stat = &sc->vte_stats;

	CSR_READ_2(sc, VTE_MECISR);
	/* RX stats. */
	stat->rx_frames += CSR_READ_2(sc, VTE_CNT_RX_DONE);
	value = CSR_READ_2(sc, VTE_CNT_MECNT0);
	stat->rx_bcast_frames += (value >> 8);
	stat->rx_mcast_frames += (value & 0xFF);
	value = CSR_READ_2(sc, VTE_CNT_MECNT1);
	stat->rx_runts += (value >> 8);
	stat->rx_crcerrs += (value & 0xFF);
	value = CSR_READ_2(sc, VTE_CNT_MECNT2);
	stat->rx_long_frames += (value & 0xFF);
	value = CSR_READ_2(sc, VTE_CNT_MECNT3);
	stat->rx_fifo_full += (value >> 8);
	stat->rx_desc_unavail += (value & 0xFF);

	/* TX stats. */
	stat->tx_frames += CSR_READ_2(sc, VTE_CNT_TX_DONE);
	value = CSR_READ_2(sc, VTE_CNT_MECNT4);
	stat->tx_underruns += (value >> 8);
	stat->tx_late_colls += (value & 0xFF);

	value = CSR_READ_2(sc, VTE_CNT_PAUSE);
	stat->tx_pause_frames += (value >> 8);
	stat->rx_pause_frames += (value & 0xFF);

	/* Update ifp counters. */
	ifp->if_opackets = stat->tx_frames;
	ifp->if_oerrors = stat->tx_late_colls + stat->tx_underruns;
	ifp->if_ipackets = stat->rx_frames;
	ifp->if_ierrors = stat->rx_crcerrs + stat->rx_runts +
	    stat->rx_long_frames + stat->rx_fifo_full;
}

static int
vte_intr(void *arg)
{
	struct vte_softc *sc = (struct vte_softc *)arg;
	struct ifnet *ifp = &sc->vte_if;
	uint16_t status;
	int n;

	/* Reading VTE_MISR acknowledges interrupts. */
	status = CSR_READ_2(sc, VTE_MISR);
	DPRINTF(("vte_intr status 0x%x\n", status));
	if ((status & VTE_INTRS) == 0) {
		/* Not ours. */
		return 0;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, VTE_MIER, 0);
	for (n = 8; (status & VTE_INTRS) != 0;) {
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;
		if ((status & (MISR_RX_DONE | MISR_RX_DESC_UNAVAIL |
		    MISR_RX_FIFO_FULL)) != 0)
			vte_rxeof(sc);
		if ((status & MISR_TX_DONE) != 0)
			vte_txeof(sc);
		if ((status & MISR_EVENT_CNT_OFLOW) != 0)
			vte_stats_update(sc);
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			vte_ifstart(ifp);
		if (--n > 0)
			status = CSR_READ_2(sc, VTE_MISR);
		else
			break;
	}

	if ((ifp->if_flags & IFF_RUNNING) != 0) {
		/* Re-enable interrupts. */
		CSR_WRITE_2(sc, VTE_MIER, VTE_INTRS);
	}
	return 1;
}

static void
vte_txeof(struct vte_softc *sc)
{
	struct ifnet *ifp;
	struct vte_txdesc *txd;
	uint16_t status;
	int cons, prog;

	ifp = &sc->vte_if;

	if (sc->vte_cdata.vte_tx_cnt == 0)
		return;
	bus_dmamap_sync(sc->vte_dmatag,
	    sc->vte_cdata.vte_tx_ring_map, 0,
	    sc->vte_cdata.vte_tx_ring_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	cons = sc->vte_cdata.vte_tx_cons;
	/*
	 * Go through our TX list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (prog = 0; sc->vte_cdata.vte_tx_cnt > 0; prog++) {
		txd = &sc->vte_cdata.vte_txdesc[cons];
		status = le16toh(txd->tx_desc->dtst);
		if ((status & VTE_DTST_TX_OWN) != 0)
			break;
		if ((status & VTE_DTST_TX_OK) != 0)
			ifp->if_collisions += (status & 0xf);
		sc->vte_cdata.vte_tx_cnt--;
		/* Reclaim transmitted mbufs. */
		bus_dmamap_sync(sc->vte_dmatag, txd->tx_dmamap, 0,
		    txd->tx_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->vte_dmatag, txd->tx_dmamap);
		if ((txd->tx_flags & VTE_TXMBUF) == 0)
			m_freem(txd->tx_m);
		txd->tx_flags &= ~VTE_TXMBUF;
		txd->tx_m = NULL;
		prog++;
		VTE_DESC_INC(cons, VTE_TX_RING_CNT);
	}

	if (prog > 0) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->vte_cdata.vte_tx_cons = cons;
		/*
		 * Unarm watchdog timer only when there is no pending
		 * frames in TX queue.
		 */
		if (sc->vte_cdata.vte_tx_cnt == 0)
			sc->vte_watchdog_timer = 0;
	}
}

static int
vte_newbuf(struct vte_softc *sc, struct vte_rxdesc *rxd)
{
	struct mbuf *m;
	bus_dmamap_t map;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, sizeof(uint32_t));

	if (bus_dmamap_load_mbuf(sc->vte_dmatag,
	    sc->vte_cdata.vte_rx_sparemap, m, 0) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	KASSERT(sc->vte_cdata.vte_rx_sparemap->dm_nsegs == 1);

	if (rxd->rx_m != NULL) {
		bus_dmamap_sync(sc->vte_dmatag, rxd->rx_dmamap,
		    0, rxd->rx_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->vte_dmatag, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = sc->vte_cdata.vte_rx_sparemap;
	sc->vte_cdata.vte_rx_sparemap = map;
	bus_dmamap_sync(sc->vte_dmatag, rxd->rx_dmamap,
	    0, rxd->rx_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);
	rxd->rx_m = m;
	rxd->rx_desc->drbp =
	    htole32(rxd->rx_dmamap->dm_segs[0].ds_addr);
	rxd->rx_desc->drlen = htole16(
	    VTE_RX_LEN(rxd->rx_dmamap->dm_segs[0].ds_len));
	DPRINTF(("rx data %p mbuf %p buf 0x%x/0x%x\n", rxd, m, (u_int)rxd->rx_dmamap->dm_segs[0].ds_addr, rxd->rx_dmamap->dm_segs[0].ds_len));
	rxd->rx_desc->drst = htole16(VTE_DRST_RX_OWN);

	return (0);
}

static void
vte_rxeof(struct vte_softc *sc)
{
	struct ifnet *ifp;
	struct vte_rxdesc *rxd;
	struct mbuf *m;
	uint16_t status, total_len;
	int cons, prog;

	bus_dmamap_sync(sc->vte_dmatag,
	    sc->vte_cdata.vte_rx_ring_map, 0,
	    sc->vte_cdata.vte_rx_ring_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	cons = sc->vte_cdata.vte_rx_cons;
	ifp = &sc->vte_if;
	DPRINTF(("vte_rxeof if_flags 0x%x\n", ifp->if_flags));
	for (prog = 0; (ifp->if_flags & IFF_RUNNING) != 0; prog++,
	    VTE_DESC_INC(cons, VTE_RX_RING_CNT)) {
		rxd = &sc->vte_cdata.vte_rxdesc[cons];
		status = le16toh(rxd->rx_desc->drst);
		DPRINTF(("vte_rxoef rxd %d/%p mbuf %p status 0x%x len %d\n", cons, rxd, rxd->rx_m, status, VTE_RX_LEN(le16toh(rxd->rx_desc->drlen))));
		if ((status & VTE_DRST_RX_OWN) != 0)
			break;
		total_len = VTE_RX_LEN(le16toh(rxd->rx_desc->drlen));
		m = rxd->rx_m;
		if ((status & VTE_DRST_RX_OK) == 0) {
			/* Discard errored frame. */
			rxd->rx_desc->drlen =
			    htole16(MCLBYTES - sizeof(uint32_t));
			rxd->rx_desc->drst = htole16(VTE_DRST_RX_OWN);
			continue;
		}
		if (vte_newbuf(sc, rxd) != 0) {
			DPRINTF(("vte_rxeof newbuf failed\n"));
			ifp->if_ierrors++;
			rxd->rx_desc->drlen =
			    htole16(MCLBYTES - sizeof(uint32_t));
			rxd->rx_desc->drst = htole16(VTE_DRST_RX_OWN);
			continue;
		}

		/*
		 * It seems there is no way to strip FCS bytes.
		 */
		m->m_pkthdr.len = m->m_len = total_len - ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;
		ifp->if_ipackets++;
		bpf_mtap(ifp, m);
		(*ifp->if_input)(ifp, m);
	}

	if (prog > 0) {
		/* Update the consumer index. */
		sc->vte_cdata.vte_rx_cons = cons;
		/*
		 * Sync updated RX descriptors such that controller see
		 * modified RX buffer addresses.
		 */
		bus_dmamap_sync(sc->vte_dmatag,
		    sc->vte_cdata.vte_rx_ring_map, 0,
		    sc->vte_cdata.vte_rx_ring_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
#ifdef notyet
		/*
		 * Update residue counter.  Controller does not
		 * keep track of number of available RX descriptors
		 * such that driver should have to update VTE_MRDCR
		 * to make controller know how many free RX
		 * descriptors were added to controller.  This is
		 * a similar mechanism used in VIA velocity
		 * controllers and it indicates controller just
		 * polls OWN bit of current RX descriptor pointer.
		 * A couple of severe issues were seen on sample
		 * board where the controller continuously emits TX
		 * pause frames once RX pause threshold crossed.
		 * Once triggered it never recovered form that
		 * state, I couldn't find a way to make it back to
		 * work at least.  This issue effectively
		 * disconnected the system from network.  Also, the
		 * controller used 00:00:00:00:00:00 as source
		 * station address of TX pause frame. Probably this
		 * is one of reason why vendor recommends not to
		 * enable flow control on R6040 controller.
		 */
		CSR_WRITE_2(sc, VTE_MRDCR, prog |
		    (((VTE_RX_RING_CNT * 2) / 10) <<
		    VTE_MRDCR_RX_PAUSE_THRESH_SHIFT));
#endif
	rnd_add_uint32(&sc->rnd_source, prog);
	}
}

static void
vte_tick(void *arg)
{
	struct vte_softc *sc;
	int s = splnet();

	sc = (struct vte_softc *)arg;

	mii_tick(&sc->vte_mii);
	vte_stats_update(sc);
	vte_txeof(sc);
	vte_ifwatchdog(&sc->vte_if);
	callout_reset(&sc->vte_tick_ch, hz, vte_tick, sc);
	splx(s);
}

static void
vte_reset(struct vte_softc *sc)
{
	uint16_t mcr;
	int i;

	mcr = CSR_READ_2(sc, VTE_MCR1);
	CSR_WRITE_2(sc, VTE_MCR1, mcr | MCR1_MAC_RESET);
	for (i = VTE_RESET_TIMEOUT; i > 0; i--) {
		DELAY(10);
		if ((CSR_READ_2(sc, VTE_MCR1) & MCR1_MAC_RESET) == 0)
			break;
	}
	if (i == 0)
		aprint_error_dev(sc->vte_dev, "reset timeout(0x%04x)!\n", mcr);
	/*
	 * Follow the guide of vendor recommended way to reset MAC.
	 * Vendor confirms relying on MCR1_MAC_RESET of VTE_MCR1 is
	 * not reliable so manually reset internal state machine.
	 */
	CSR_WRITE_2(sc, VTE_MACSM, 0x0002);
	CSR_WRITE_2(sc, VTE_MACSM, 0);
	DELAY(5000);
}


static int
vte_init(struct ifnet *ifp)
{
	struct vte_softc *sc = ifp->if_softc;
	bus_addr_t paddr;
	uint8_t eaddr[ETHER_ADDR_LEN];
	int s, error;

	s = splnet();
	/*
	 * Cancel any pending I/O.
	 */
	vte_stop(ifp, 1);
	/*
	 * Reset the chip to a known state.
	 */
	vte_reset(sc);

	if ((sc->vte_if.if_flags & IFF_UP) == 0) {
		splx(s);
		return 0;
	}

	/* Initialize RX descriptors. */
	if (vte_init_rx_ring(sc) != 0) {
		aprint_error_dev(sc->vte_dev, "no memory for RX buffers.\n");
		vte_stop(ifp, 1);
		splx(s);
		return ENOMEM;
	}
	if (vte_init_tx_ring(sc) != 0) {
		aprint_error_dev(sc->vte_dev, "no memory for TX buffers.\n");
		vte_stop(ifp, 1);
		splx(s);
		return ENOMEM;
	}

	/*
	 * Reprogram the station address.  Controller supports up
	 * to 4 different station addresses so driver programs the
	 * first station address as its own ethernet address and
	 * configure the remaining three addresses as perfect
	 * multicast addresses.
	 */
	memcpy(eaddr, CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN);
	CSR_WRITE_2(sc, VTE_MID0L, eaddr[1] << 8 | eaddr[0]);
	CSR_WRITE_2(sc, VTE_MID0M, eaddr[3] << 8 | eaddr[2]);
	CSR_WRITE_2(sc, VTE_MID0H, eaddr[5] << 8 | eaddr[4]);

	/* Set TX descriptor base addresses. */
	paddr = sc->vte_cdata.vte_tx_ring_map->dm_segs[0].ds_addr;
	DPRINTF(("tx paddr 0x%x\n", (u_int)paddr));
	CSR_WRITE_2(sc, VTE_MTDSA1, paddr >> 16);
	CSR_WRITE_2(sc, VTE_MTDSA0, paddr & 0xFFFF);

	/* Set RX descriptor base addresses. */
	paddr = sc->vte_cdata.vte_rx_ring_map->dm_segs[0].ds_addr;
	DPRINTF(("rx paddr 0x%x\n", (u_int)paddr));
	CSR_WRITE_2(sc, VTE_MRDSA1, paddr >> 16);
	CSR_WRITE_2(sc, VTE_MRDSA0, paddr & 0xFFFF);
	/*
	 * Initialize RX descriptor residue counter and set RX
	 * pause threshold to 20% of available RX descriptors.
	 * See comments on vte_rxeof() for details on flow control
	 * issues.
	 */
	CSR_WRITE_2(sc, VTE_MRDCR, (VTE_RX_RING_CNT & VTE_MRDCR_RESIDUE_MASK) |
	    (((VTE_RX_RING_CNT * 2) / 10) << VTE_MRDCR_RX_PAUSE_THRESH_SHIFT));

	/*
	 * Always use maximum frame size that controller can
	 * support.  Otherwise received frames that has longer
	 * frame length than vte(4) MTU would be silently dropped
	 * in controller.  This would break path-MTU discovery as
	 * sender wouldn't get any responses from receiver. The
	 * RX buffer size should be multiple of 4.
	 * Note, jumbo frames are silently ignored by controller
	 * and even MAC counters do not detect them.
	 */
	CSR_WRITE_2(sc, VTE_MRBSR, VTE_RX_BUF_SIZE_MAX);

	/* Configure FIFO. */
	CSR_WRITE_2(sc, VTE_MBCR, MBCR_FIFO_XFER_LENGTH_16 |
	    MBCR_TX_FIFO_THRESH_64 | MBCR_RX_FIFO_THRESH_16 |
	    MBCR_SDRAM_BUS_REQ_TIMER_DEFAULT);

	/*
	 * Configure TX/RX MACs.  Actual resolved duplex and flow
	 * control configuration is done after detecting a valid
	 * link.  Note, we don't generate early interrupt here
	 * as well since FreeBSD does not have interrupt latency
	 * problems like Windows.
	 */
	CSR_WRITE_2(sc, VTE_MCR0, MCR0_ACCPT_LONG_PKT);
	/*
	 * We manually keep track of PHY status changes to
	 * configure resolved duplex and flow control since only
	 * duplex configuration can be automatically reflected to
	 * MCR0.
	 */
	CSR_WRITE_2(sc, VTE_MCR1, MCR1_PKT_LENGTH_1537 |
	    MCR1_EXCESS_COL_RETRY_16);

	/* Initialize RX filter. */
	vte_rxfilter(sc);

	/* Disable TX/RX interrupt moderation control. */
	CSR_WRITE_2(sc, VTE_MRICR, 0);
	CSR_WRITE_2(sc, VTE_MTICR, 0);

	/* Enable MAC event counter interrupts. */
	CSR_WRITE_2(sc, VTE_MECIER, VTE_MECIER_INTRS);
	/* Clear MAC statistics. */
	vte_stats_clear(sc);

	/* Acknowledge all pending interrupts and clear it. */
	CSR_WRITE_2(sc, VTE_MIER, VTE_INTRS);
	CSR_WRITE_2(sc, VTE_MISR, 0);
	DPRINTF(("before ipend 0x%x 0x%x\n", CSR_READ_2(sc, VTE_MIER), CSR_READ_2(sc, VTE_MISR)));

	sc->vte_flags &= ~VTE_FLAG_LINK;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* calling mii_mediachg will call back vte_start_mac() */
	if ((error = mii_mediachg(&sc->vte_mii)) == ENXIO)
		error = 0;
	else if (error != 0) {
		aprint_error_dev(sc->vte_dev, "could not set media\n");
		splx(s);
		return error;
	}

	callout_reset(&sc->vte_tick_ch, hz, vte_tick, sc);

	DPRINTF(("ipend 0x%x 0x%x\n", CSR_READ_2(sc, VTE_MIER), CSR_READ_2(sc, VTE_MISR)));
	splx(s);
	return 0;
}

static void
vte_stop(struct ifnet *ifp, int disable)
{
	struct vte_softc *sc = ifp->if_softc;
	struct vte_txdesc *txd;
	struct vte_rxdesc *rxd;
	int i;

	DPRINTF(("vte_stop if_flags 0x%x\n", ifp->if_flags));
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	sc->vte_flags &= ~VTE_FLAG_LINK;
	callout_stop(&sc->vte_tick_ch);
	sc->vte_watchdog_timer = 0;
	vte_stats_update(sc);
	/* Disable interrupts. */
	CSR_WRITE_2(sc, VTE_MIER, 0);
	CSR_WRITE_2(sc, VTE_MECIER, 0);
	/* Stop RX/TX MACs. */
	vte_stop_mac(sc);
	/* Clear interrupts. */
	CSR_READ_2(sc, VTE_MISR);
	/*
	 * Free TX/RX mbufs still in the queues.
	 */
	for (i = 0; i < VTE_RX_RING_CNT; i++) {
		rxd = &sc->vte_cdata.vte_rxdesc[i];
		if (rxd->rx_m != NULL) {
			bus_dmamap_sync(sc->vte_dmatag,
			    rxd->rx_dmamap, 0, rxd->rx_dmamap->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->vte_dmatag,
			    rxd->rx_dmamap);
			m_freem(rxd->rx_m);
			rxd->rx_m = NULL;
		}
	}
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		txd = &sc->vte_cdata.vte_txdesc[i];
		if (txd->tx_m != NULL) {
			bus_dmamap_sync(sc->vte_dmatag,
			    txd->tx_dmamap, 0, txd->tx_dmamap->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->vte_dmatag,
			    txd->tx_dmamap);
			if ((txd->tx_flags & VTE_TXMBUF) == 0)
				m_freem(txd->tx_m);
			txd->tx_m = NULL;
			txd->tx_flags &= ~VTE_TXMBUF;
		}
	}
	/* Free TX mbuf pools used for deep copy. */
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		if (sc->vte_cdata.vte_txmbufs[i] != NULL) {
			m_freem(sc->vte_cdata.vte_txmbufs[i]);
			sc->vte_cdata.vte_txmbufs[i] = NULL;
		}
	}
}

static void
vte_start_mac(struct vte_softc *sc)
{
	struct ifnet *ifp = &sc->vte_if;
	uint16_t mcr;
	int i;

	/* Enable RX/TX MACs. */
	mcr = CSR_READ_2(sc, VTE_MCR0);
	if ((mcr & (MCR0_RX_ENB | MCR0_TX_ENB)) !=
	    (MCR0_RX_ENB | MCR0_TX_ENB) &&
	    (ifp->if_flags & IFF_RUNNING) != 0) {
		mcr |= MCR0_RX_ENB | MCR0_TX_ENB;
		CSR_WRITE_2(sc, VTE_MCR0, mcr);
		for (i = VTE_TIMEOUT; i > 0; i--) {
			mcr = CSR_READ_2(sc, VTE_MCR0);
			if ((mcr & (MCR0_RX_ENB | MCR0_TX_ENB)) ==
			    (MCR0_RX_ENB | MCR0_TX_ENB))
				break;
			DELAY(10);
		}
		if (i == 0)
			aprint_error_dev(sc->vte_dev,
			    "could not enable RX/TX MAC(0x%04x)!\n", mcr);
	}
	vte_rxfilter(sc);
}

static void
vte_stop_mac(struct vte_softc *sc)
{
	uint16_t mcr;
	int i;

	/* Disable RX/TX MACs. */
	mcr = CSR_READ_2(sc, VTE_MCR0);
	if ((mcr & (MCR0_RX_ENB | MCR0_TX_ENB)) != 0) {
		mcr &= ~(MCR0_RX_ENB | MCR0_TX_ENB);
		CSR_WRITE_2(sc, VTE_MCR0, mcr);
		for (i = VTE_TIMEOUT; i > 0; i--) {
			mcr = CSR_READ_2(sc, VTE_MCR0);
			if ((mcr & (MCR0_RX_ENB | MCR0_TX_ENB)) == 0)
				break;
			DELAY(10);
		}
		if (i == 0)
			aprint_error_dev(sc->vte_dev,
			    "could not disable RX/TX MAC(0x%04x)!\n", mcr);
	}
}

static int
vte_init_tx_ring(struct vte_softc *sc)
{
	struct vte_tx_desc *desc;
	struct vte_txdesc *txd;
	bus_addr_t addr;
	int i;

	sc->vte_cdata.vte_tx_prod = 0;
	sc->vte_cdata.vte_tx_cons = 0;
	sc->vte_cdata.vte_tx_cnt = 0;

	/* Pre-allocate TX mbufs for deep copy. */
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		sc->vte_cdata.vte_txmbufs[i] = m_getcl(M_DONTWAIT,
		    MT_DATA, M_PKTHDR);
		if (sc->vte_cdata.vte_txmbufs[i] == NULL)
			return (ENOBUFS);
		sc->vte_cdata.vte_txmbufs[i]->m_pkthdr.len = MCLBYTES;
		sc->vte_cdata.vte_txmbufs[i]->m_len = MCLBYTES;
	}
	desc = sc->vte_cdata.vte_tx_ring;
	bzero(desc, VTE_TX_RING_SZ);
	for (i = 0; i < VTE_TX_RING_CNT; i++) {
		txd = &sc->vte_cdata.vte_txdesc[i];
		txd->tx_m = NULL;
		if (i != VTE_TX_RING_CNT - 1)
			addr = sc->vte_cdata.vte_tx_ring_map->dm_segs[0].ds_addr +
			    sizeof(struct vte_tx_desc) * (i + 1);
		else
			addr = sc->vte_cdata.vte_tx_ring_map->dm_segs[0].ds_addr +
			    sizeof(struct vte_tx_desc) * 0;
		desc = &sc->vte_cdata.vte_tx_ring[i];
		desc->dtnp = htole32(addr);
		DPRINTF(("tx ring desc %d addr 0x%x\n", i, (u_int)addr));
		txd->tx_desc = desc;
	}

	bus_dmamap_sync(sc->vte_dmatag,
	    sc->vte_cdata.vte_tx_ring_map, 0,
	    sc->vte_cdata.vte_tx_ring_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return (0);
}

static int
vte_init_rx_ring(struct vte_softc *sc)
{
	struct vte_rx_desc *desc;
	struct vte_rxdesc *rxd;
	bus_addr_t addr;
	int i;

	sc->vte_cdata.vte_rx_cons = 0;
	desc = sc->vte_cdata.vte_rx_ring;
	bzero(desc, VTE_RX_RING_SZ);
	for (i = 0; i < VTE_RX_RING_CNT; i++) {
		rxd = &sc->vte_cdata.vte_rxdesc[i];
		rxd->rx_m = NULL;
		if (i != VTE_RX_RING_CNT - 1)
			addr = sc->vte_cdata.vte_rx_ring_map->dm_segs[0].ds_addr
			    + sizeof(struct vte_rx_desc) * (i + 1);
		else
			addr = sc->vte_cdata.vte_rx_ring_map->dm_segs[0].ds_addr
			    + sizeof(struct vte_rx_desc) * 0;
		desc = &sc->vte_cdata.vte_rx_ring[i];
		desc->drnp = htole32(addr);
		DPRINTF(("rx ring desc %d addr 0x%x\n", i, (u_int)addr));
		rxd->rx_desc = desc;
		if (vte_newbuf(sc, rxd) != 0)
			return (ENOBUFS);
	}

	bus_dmamap_sync(sc->vte_dmatag,
	    sc->vte_cdata.vte_rx_ring_map, 0,
	    sc->vte_cdata.vte_rx_ring_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

static void
vte_rxfilter(struct vte_softc *sc)
{
	struct ether_multistep step;
	struct ether_multi *enm;
	struct ifnet *ifp;
	uint8_t *eaddr;
	uint32_t crc;
	uint16_t rxfilt_perf[VTE_RXFILT_PERFECT_CNT][3];
	uint16_t mchash[4], mcr;
	int i, nperf;

	ifp = &sc->vte_if;

	DPRINTF(("vte_rxfilter\n"));
	memset(mchash, 0, sizeof(mchash));
	for (i = 0; i < VTE_RXFILT_PERFECT_CNT; i++) {
		rxfilt_perf[i][0] = 0xFFFF;
		rxfilt_perf[i][1] = 0xFFFF;
		rxfilt_perf[i][2] = 0xFFFF;
	}

	mcr = CSR_READ_2(sc, VTE_MCR0);
	DPRINTF(("vte_rxfilter mcr 0x%x\n", mcr));
	mcr &= ~(MCR0_PROMISC | MCR0_BROADCAST_DIS | MCR0_MULTICAST);
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		mcr |= MCR0_BROADCAST_DIS;
	if ((ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI)) != 0) {
		if ((ifp->if_flags & IFF_PROMISC) != 0)
			mcr |= MCR0_PROMISC;
		if ((ifp->if_flags & IFF_ALLMULTI) != 0)
			mcr |= MCR0_MULTICAST;
		mchash[0] = 0xFFFF;
		mchash[1] = 0xFFFF;
		mchash[2] = 0xFFFF;
		mchash[3] = 0xFFFF;
		goto chipit;
	}

	ETHER_FIRST_MULTI(step, &sc->vte_ec, enm);
	nperf = 0;
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN) != 0) {
			sc->vte_if.if_flags |= IFF_ALLMULTI;
			mcr |= MCR0_MULTICAST;
			mchash[0] = 0xFFFF;
			mchash[1] = 0xFFFF;
			mchash[2] = 0xFFFF;
			mchash[3] = 0xFFFF;
			goto chipit;
		}
		/*
		 * Program the first 3 multicast groups into
		 * the perfect filter.  For all others, use the
		 * hash table.
		 */
		if (nperf < VTE_RXFILT_PERFECT_CNT) {
			eaddr = enm->enm_addrlo;
			rxfilt_perf[nperf][0] = eaddr[1] << 8 | eaddr[0];
			rxfilt_perf[nperf][1] = eaddr[3] << 8 | eaddr[2];
			rxfilt_perf[nperf][2] = eaddr[5] << 8 | eaddr[4];
			nperf++;
		} else {
			crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);
			mchash[crc >> 30] |= 1 << ((crc >> 26) & 0x0F);
		}
		ETHER_NEXT_MULTI(step, enm);
	}
	if (mchash[0] != 0 || mchash[1] != 0 || mchash[2] != 0 ||
	    mchash[3] != 0)
		mcr |= MCR0_MULTICAST;

chipit:
	/* Program multicast hash table. */
	DPRINTF(("chipit write multicast\n"));
	CSR_WRITE_2(sc, VTE_MAR0, mchash[0]);
	CSR_WRITE_2(sc, VTE_MAR1, mchash[1]);
	CSR_WRITE_2(sc, VTE_MAR2, mchash[2]);
	CSR_WRITE_2(sc, VTE_MAR3, mchash[3]);
	/* Program perfect filter table. */
	DPRINTF(("chipit write perfect filter\n"));
	for (i = 0; i < VTE_RXFILT_PERFECT_CNT; i++) {
		CSR_WRITE_2(sc, VTE_RXFILTER_PEEFECT_BASE + 8 * i + 0,
		    rxfilt_perf[i][0]);
		CSR_WRITE_2(sc, VTE_RXFILTER_PEEFECT_BASE + 8 * i + 2,
		    rxfilt_perf[i][1]);
		CSR_WRITE_2(sc, VTE_RXFILTER_PEEFECT_BASE + 8 * i + 4,
		    rxfilt_perf[i][2]);
	}
	DPRINTF(("chipit mcr0 0x%x\n", mcr));
	CSR_WRITE_2(sc, VTE_MCR0, mcr);
	DPRINTF(("chipit read mcro\n"));
	CSR_READ_2(sc, VTE_MCR0);
	DPRINTF(("chipit done\n"));
}

/*
 * Set up sysctl(3) MIB, hw.vte.* - Individual controllers will be
 * set up in vte_pci_attach()
 */
SYSCTL_SETUP(sysctl_vte, "sysctl vte subtree setup")
{
	int rc;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "vte",
	    SYSCTL_DESCR("vte interface controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	vte_root_num = node->sysctl_num;
	return;

err:
	aprint_error("%s: syctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
vte_sysctl_intrxct(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct vte_softc *sc;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->vte_int_rx_mod;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	if (t < VTE_IM_BUNDLE_MIN || t > VTE_IM_BUNDLE_MAX)
		return EINVAL;

	sc->vte_int_rx_mod = t;
	vte_miibus_statchg(&sc->vte_if);
	return 0;
}

static int
vte_sysctl_inttxct(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct vte_softc *sc;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->vte_int_tx_mod;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < VTE_IM_BUNDLE_MIN || t > VTE_IM_BUNDLE_MAX)
		return EINVAL;
	sc->vte_int_tx_mod = t;
	vte_miibus_statchg(&sc->vte_if);
	return 0;
}
