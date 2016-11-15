/*	$NetBSD: if_nfe.c,v 1.59 2014/03/29 19:28:25 christos Exp $	*/
/*	$OpenBSD: if_nfe.c,v 1.77 2008/02/05 16:52:50 brad Exp $	*/

/*-
 * Copyright (c) 2006, 2007 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2005, 2006 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Driver for NVIDIA nForce MCP Fast Ethernet and Gigabit Ethernet */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_nfe.c,v 1.59 2014/03/29 19:28:25 christos Exp $");

#include "opt_inet.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/socket.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <net/if_arp.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#endif

#include <net/bpf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_nfereg.h>
#include <dev/pci/if_nfevar.h>

static int nfe_ifflags_cb(struct ethercom *);

int	nfe_match(device_t, cfdata_t, void *);
void	nfe_attach(device_t, device_t, void *);
int	nfe_detach(device_t, int);
void	nfe_power(int, void *);
void	nfe_miibus_statchg(struct ifnet *);
int	nfe_miibus_readreg(device_t, int, int);
void	nfe_miibus_writereg(device_t, int, int, int);
int	nfe_intr(void *);
int	nfe_ioctl(struct ifnet *, u_long, void *);
void	nfe_txdesc32_sync(struct nfe_softc *, struct nfe_desc32 *, int);
void	nfe_txdesc64_sync(struct nfe_softc *, struct nfe_desc64 *, int);
void	nfe_txdesc32_rsync(struct nfe_softc *, int, int, int);
void	nfe_txdesc64_rsync(struct nfe_softc *, int, int, int);
void	nfe_rxdesc32_sync(struct nfe_softc *, struct nfe_desc32 *, int);
void	nfe_rxdesc64_sync(struct nfe_softc *, struct nfe_desc64 *, int);
void	nfe_rxeof(struct nfe_softc *);
void	nfe_txeof(struct nfe_softc *);
int	nfe_encap(struct nfe_softc *, struct mbuf *);
void	nfe_start(struct ifnet *);
void	nfe_watchdog(struct ifnet *);
int	nfe_init(struct ifnet *);
void	nfe_stop(struct ifnet *, int);
struct	nfe_jbuf *nfe_jalloc(struct nfe_softc *, int);
void	nfe_jfree(struct mbuf *, void *, size_t, void *);
int	nfe_jpool_alloc(struct nfe_softc *);
void	nfe_jpool_free(struct nfe_softc *);
int	nfe_alloc_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
void	nfe_reset_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
void	nfe_free_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
int	nfe_alloc_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
void	nfe_reset_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
void	nfe_free_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
void	nfe_setmulti(struct nfe_softc *);
void	nfe_get_macaddr(struct nfe_softc *, uint8_t *);
void	nfe_set_macaddr(struct nfe_softc *, const uint8_t *);
void	nfe_tick(void *);
void	nfe_poweron(device_t);
bool	nfe_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(nfe, sizeof(struct nfe_softc),
    nfe_match, nfe_attach, nfe_detach, NULL);

/* #define NFE_NO_JUMBO */

#ifdef NFE_DEBUG
int nfedebug = 0;
#define DPRINTF(x)	do { if (nfedebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (nfedebug >= (n)) printf x; } while (0)
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/* deal with naming differences */

#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN2 \
	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN1
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN3 \
	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN2
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN5 \
	PCI_PRODUCT_NVIDIA_NFORCE3_250_LAN

#define	PCI_PRODUCT_NVIDIA_CK804_LAN1 \
	PCI_PRODUCT_NVIDIA_NFORCE4_LAN1
#define	PCI_PRODUCT_NVIDIA_CK804_LAN2 \
	PCI_PRODUCT_NVIDIA_NFORCE4_LAN2

#define	PCI_PRODUCT_NVIDIA_MCP51_LAN1 \
	PCI_PRODUCT_NVIDIA_NFORCE430_LAN1
#define	PCI_PRODUCT_NVIDIA_MCP51_LAN2 \
	PCI_PRODUCT_NVIDIA_NFORCE430_LAN2

#ifdef	_LP64
#define	__LP64__ 1
#endif

const struct nfe_product {
	pci_vendor_id_t		vendor;
	pci_product_id_t	product;
} nfe_devices[] = {
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE_LAN },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE2_LAN },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN5 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_CK804_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_CK804_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP51_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP51_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_LAN4 }
};

int
nfe_match(device_t dev, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct nfe_product *np;
	int i;

	for (i = 0; i < __arraycount(nfe_devices); i++) {
		np = &nfe_devices[i];
		if (PCI_VENDOR(pa->pa_id) == np->vendor &&
		    PCI_PRODUCT(pa->pa_id) == np->product)
			return 1;
	}
	return 0;
}

void
nfe_attach(device_t parent, device_t self, void *aux)
{
	struct nfe_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	struct ifnet *ifp;
	pcireg_t memtype, csr;
	int mii_flags = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pc = pa->pa_pc;
	pci_aprint_devinfo(pa, NULL);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, NFE_PCI_BA);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		if (pci_mapreg_map(pa, NFE_PCI_BA, memtype, 0, &sc->sc_memt,
		    &sc->sc_memh, NULL, &sc->sc_mems) == 0)
			break;
		/* FALLTHROUGH */
	default:
		aprint_error_dev(self, "could not map mem space\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "could not map interrupt\n");
		goto fail;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, nfe_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

	sc->sc_flags = 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN2:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN3:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN4:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN5:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_HW_CSUM;
		break;
	case PCI_PRODUCT_NVIDIA_MCP51_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP51_LAN2:
		sc->sc_flags |= NFE_40BIT_ADDR | NFE_PWR_MGMT;
		break;
	case PCI_PRODUCT_NVIDIA_MCP61_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN4:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN4:
	case PCI_PRODUCT_NVIDIA_MCP73_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP73_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP73_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP73_LAN4:
		sc->sc_flags |= NFE_40BIT_ADDR | NFE_CORRECT_MACADDR |
		    NFE_PWR_MGMT;
		break;
	case PCI_PRODUCT_NVIDIA_MCP77_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP77_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP77_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP77_LAN4:
		sc->sc_flags |= NFE_40BIT_ADDR | NFE_HW_CSUM |
		    NFE_CORRECT_MACADDR | NFE_PWR_MGMT;
		break;
	case PCI_PRODUCT_NVIDIA_MCP79_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP79_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP79_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP79_LAN4:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM |
		    NFE_CORRECT_MACADDR | NFE_PWR_MGMT;
		break;
	case PCI_PRODUCT_NVIDIA_CK804_LAN1:
	case PCI_PRODUCT_NVIDIA_CK804_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN2:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM;
		break;
	case PCI_PRODUCT_NVIDIA_MCP65_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN4:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR |
		    NFE_CORRECT_MACADDR | NFE_PWR_MGMT;
		mii_flags = MIIF_DOPAUSE;
		break;
	case PCI_PRODUCT_NVIDIA_MCP55_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP55_LAN2:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM |
		    NFE_HW_VLAN | NFE_PWR_MGMT;
		break;
	}

	if (pci_dma64_available(pa) && (sc->sc_flags & NFE_40BIT_ADDR) != 0)
		sc->sc_dmat = pa->pa_dmat64;
	else
		sc->sc_dmat = pa->pa_dmat;

	nfe_poweron(self);

#ifndef NFE_NO_JUMBO
	/* enable jumbo frames for adapters that support it */
	if (sc->sc_flags & NFE_JUMBO_SUP)
		sc->sc_flags |= NFE_USE_JUMBO;
#endif

	/* Check for reversed ethernet address */
	if ((NFE_READ(sc, NFE_TX_UNK) & NFE_MAC_ADDR_INORDER) != 0)
		sc->sc_flags |= NFE_CORRECT_MACADDR;

	nfe_get_macaddr(sc, sc->sc_enaddr);
	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	/*
	 * Allocate Tx and Rx rings.
	 */
	if (nfe_alloc_tx_ring(sc, &sc->txq) != 0) {
		aprint_error_dev(self, "could not allocate Tx ring\n");
		goto fail;
	}

	mutex_init(&sc->rxq.mtx, MUTEX_DEFAULT, IPL_NET);

	if (nfe_alloc_rx_ring(sc, &sc->rxq) != 0) {
		aprint_error_dev(self, "could not allocate Rx ring\n");
		nfe_free_tx_ring(sc, &sc->txq);
		goto fail;
	}

	ifp = &sc->sc_ethercom.ec_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nfe_ioctl;
	ifp->if_start = nfe_start;
	ifp->if_stop = nfe_stop;
	ifp->if_watchdog = nfe_watchdog;
	ifp->if_init = nfe_init;
	ifp->if_baudrate = IF_Gbps(1);
	IFQ_SET_MAXLEN(&ifp->if_snd, NFE_IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	if (sc->sc_flags & NFE_USE_JUMBO)
		sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;

#if NVLAN > 0
	if (sc->sc_flags & NFE_HW_VLAN)
		sc->sc_ethercom.ec_capabilities |=
			ETHERCAP_VLAN_HWTAGGING | ETHERCAP_VLAN_MTU;
#endif
	if (sc->sc_flags & NFE_HW_CSUM) {
		ifp->if_capabilities |=
		    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;
	}

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = nfe_miibus_readreg;
	sc->sc_mii.mii_writereg = nfe_miibus_writereg;
	sc->sc_mii.mii_statchg = nfe_miibus_statchg;

	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, 0, ether_mediachange,
	    ether_mediastatus);

	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY, 0, mii_flags);

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		aprint_error_dev(self, "no PHY found!\n");
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER | IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, nfe_ifflags_cb);

	callout_init(&sc->sc_tick_ch, 0);
	callout_setfunc(&sc->sc_tick_ch, nfe_tick, sc);

	if (pmf_device_register(self, NULL, nfe_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

fail:
	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc_mems != 0) {
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
		sc->sc_mems = 0;
	}
}

int
nfe_detach(device_t self, int flags)
{
	struct nfe_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int s;

	s = splnet();

	nfe_stop(ifp, 1);

	pmf_device_deregister(self);
	callout_destroy(&sc->sc_tick_ch);
	ether_ifdetach(ifp);
	if_detach(ifp);
	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	nfe_free_rx_ring(sc, &sc->rxq);
	mutex_destroy(&sc->rxq.mtx);
	nfe_free_tx_ring(sc, &sc->txq);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if ((sc->sc_flags & NFE_CORRECT_MACADDR) != 0) {
		nfe_set_macaddr(sc, sc->sc_enaddr);
	} else {
		NFE_WRITE(sc, NFE_MACADDR_LO,
		    sc->sc_enaddr[0] <<  8 | sc->sc_enaddr[1]);
		NFE_WRITE(sc, NFE_MACADDR_HI,
		    sc->sc_enaddr[2] << 24 | sc->sc_enaddr[3] << 16 |
		    sc->sc_enaddr[4] <<  8 | sc->sc_enaddr[5]);
	}

	if (sc->sc_mems != 0) {
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
		sc->sc_mems = 0;
	}

	splx(s);

	return 0;
}

void
nfe_miibus_statchg(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	uint32_t phy, seed, misc = NFE_MISC1_MAGIC, link = NFE_MEDIA_SET;

	phy = NFE_READ(sc, NFE_PHY_IFACE);
	phy &= ~(NFE_PHY_HDX | NFE_PHY_100TX | NFE_PHY_1000T);

	seed = NFE_READ(sc, NFE_RNDSEED);
	seed &= ~NFE_SEED_MASK;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_HDX) {
		phy  |= NFE_PHY_HDX;	/* half-duplex */
		misc |= NFE_MISC1_HDX;
	}

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:	/* full-duplex only */
		link |= NFE_MEDIA_1000T;
		seed |= NFE_SEED_1000T;
		phy  |= NFE_PHY_1000T;
		break;
	case IFM_100_TX:
		link |= NFE_MEDIA_100TX;
		seed |= NFE_SEED_100TX;
		phy  |= NFE_PHY_100TX;
		break;
	case IFM_10_T:
		link |= NFE_MEDIA_10T;
		seed |= NFE_SEED_10T;
		break;
	}

	NFE_WRITE(sc, NFE_RNDSEED, seed);	/* XXX: gigabit NICs only? */

	NFE_WRITE(sc, NFE_PHY_IFACE, phy);
	NFE_WRITE(sc, NFE_MISC1, misc);
	NFE_WRITE(sc, NFE_LINKSPEED, link);
}

int
nfe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct nfe_softc *sc = device_private(dev);
	uint32_t val;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_CTL, (phy << NFE_PHYADD_SHIFT) | reg);

	for (ntries = 0; ntries < 1000; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
	if (ntries == 1000) {
		DPRINTFN(2, ("%s: timeout waiting for PHY\n",
		    device_xname(sc->sc_dev)));
		return 0;
	}

	if (NFE_READ(sc, NFE_PHY_STATUS) & NFE_PHY_ERROR) {
		DPRINTFN(2, ("%s: could not read PHY\n",
		    device_xname(sc->sc_dev)));
		return 0;
	}

	val = NFE_READ(sc, NFE_PHY_DATA);
	if (val != 0xffffffff && val != 0)
		sc->mii_phyaddr = phy;

	DPRINTFN(2, ("%s: mii read phy %d reg 0x%x ret 0x%x\n",
	    device_xname(sc->sc_dev), phy, reg, val));

	return val;
}

void
nfe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct nfe_softc *sc = device_private(dev);
	uint32_t ctl;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_DATA, val);
	ctl = NFE_PHY_WRITE | (phy << NFE_PHYADD_SHIFT) | reg;
	NFE_WRITE(sc, NFE_PHY_CTL, ctl);

	for (ntries = 0; ntries < 1000; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
#ifdef NFE_DEBUG
	if (nfedebug >= 2 && ntries == 1000)
		printf("could not write to PHY\n");
#endif
}

int
nfe_intr(void *arg)
{
	struct nfe_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t r;
	int handled;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	handled = 0;

	for (;;) {
		r = NFE_READ(sc, NFE_IRQ_STATUS);
		if ((r & NFE_IRQ_WANTED) == 0)
			break;

		NFE_WRITE(sc, NFE_IRQ_STATUS, r);
		handled = 1;
		DPRINTFN(5, ("nfe_intr: interrupt register %x\n", r));

		if ((r & (NFE_IRQ_RXERR|NFE_IRQ_RX_NOBUF|NFE_IRQ_RX)) != 0) {
			/* check Rx ring */
			nfe_rxeof(sc);
		}
		if ((r & (NFE_IRQ_TXERR|NFE_IRQ_TXERR2|NFE_IRQ_TX_DONE)) != 0) {
			/* check Tx ring */
			nfe_txeof(sc);
		}
		if ((r & NFE_IRQ_LINK) != 0) {
			NFE_READ(sc, NFE_PHY_STATUS);
			NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);
			DPRINTF(("%s: link state changed\n",
			    device_xname(sc->sc_dev)));
		}
	}

	if (handled && !IF_IS_EMPTY(&ifp->if_snd))
		nfe_start(ifp);

	return handled;
}

static int
nfe_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct nfe_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	/*
	 * If only the PROMISC flag changes, then
	 * don't do a full re-init of the chip, just update
	 * the Rx filter.
	 */
	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	else if ((change & IFF_PROMISC) != 0)
		nfe_setmulti(sc);

	return 0;
}

int
nfe_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		nfe_init(ifp);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif
		default:
			break;
		}
		break;
	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING)
			nfe_setmulti(sc);
		break;
	}
	sc->sc_if_flags = ifp->if_flags;

	splx(s);

	return error;
}

void
nfe_txdesc32_sync(struct nfe_softc *sc, struct nfe_desc32 *desc32, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
	    (char *)desc32 - (char *)sc->txq.desc32,
	    sizeof (struct nfe_desc32), ops);
}

void
nfe_txdesc64_sync(struct nfe_softc *sc, struct nfe_desc64 *desc64, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
	    (char *)desc64 - (char *)sc->txq.desc64,
	    sizeof (struct nfe_desc64), ops);
}

void
nfe_txdesc32_rsync(struct nfe_softc *sc, int start, int end, int ops)
{
	if (end > start) {
		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    (char *)&sc->txq.desc32[start] - (char *)sc->txq.desc32,
		    (char *)&sc->txq.desc32[end] -
		    (char *)&sc->txq.desc32[start], ops);
		return;
	}
	/* sync from 'start' to end of ring */
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
	    (char *)&sc->txq.desc32[start] - (char *)sc->txq.desc32,
	    (char *)&sc->txq.desc32[NFE_TX_RING_COUNT] -
	    (char *)&sc->txq.desc32[start], ops);

	/* sync from start of ring to 'end' */
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map, 0,
	    (char *)&sc->txq.desc32[end] - (char *)sc->txq.desc32, ops);
}

void
nfe_txdesc64_rsync(struct nfe_softc *sc, int start, int end, int ops)
{
	if (end > start) {
		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    (char *)&sc->txq.desc64[start] - (char *)sc->txq.desc64,
		    (char *)&sc->txq.desc64[end] -
		    (char *)&sc->txq.desc64[start], ops);
		return;
	}
	/* sync from 'start' to end of ring */
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
	    (char *)&sc->txq.desc64[start] - (char *)sc->txq.desc64,
	    (char *)&sc->txq.desc64[NFE_TX_RING_COUNT] -
	    (char *)&sc->txq.desc64[start], ops);

	/* sync from start of ring to 'end' */
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map, 0,
	    (char *)&sc->txq.desc64[end] - (char *)sc->txq.desc64, ops);
}

void
nfe_rxdesc32_sync(struct nfe_softc *sc, struct nfe_desc32 *desc32, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
	    (char *)desc32 - (char *)sc->rxq.desc32,
	    sizeof (struct nfe_desc32), ops);
}

void
nfe_rxdesc64_sync(struct nfe_softc *sc, struct nfe_desc64 *desc64, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
	    (char *)desc64 - (char *)sc->rxq.desc64,
	    sizeof (struct nfe_desc64), ops);
}

void
nfe_rxeof(struct nfe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct nfe_jbuf *jbuf;
	struct mbuf *m, *mnew;
	bus_addr_t physaddr;
	uint16_t flags;
	int error, len, i;

	desc32 = NULL;
	desc64 = NULL;
	for (i = sc->rxq.cur;; i = NFE_RX_NEXTDESC(i)) {
		data = &sc->rxq.data[i];

		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->rxq.desc64[i];
			nfe_rxdesc64_sync(sc, desc64,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			flags = le16toh(desc64->flags);
			len = le16toh(desc64->length) & 0x3fff;
		} else {
			desc32 = &sc->rxq.desc32[i];
			nfe_rxdesc32_sync(sc, desc32,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			flags = le16toh(desc32->flags);
			len = le16toh(desc32->length) & 0x3fff;
		}

		if ((flags & NFE_RX_READY) != 0)
			break;

		if ((sc->sc_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if ((flags & NFE_RX_VALID_V1) == 0)
				goto skip;

			if ((flags & NFE_RX_FIXME_V1) == NFE_RX_FIXME_V1) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		} else {
			if ((flags & NFE_RX_VALID_V2) == 0)
				goto skip;

			if ((flags & NFE_RX_FIXME_V2) == NFE_RX_FIXME_V2) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		}

		if (flags & NFE_RX_ERROR) {
			ifp->if_ierrors++;
			goto skip;
		}

		/*
		 * Try to allocate a new mbuf for this ring element and load
		 * it before processing the current mbuf. If the ring element
		 * cannot be loaded, drop the received packet and reuse the
		 * old mbuf. In the unlikely case that the old mbuf can't be
		 * reloaded either, explicitly panic.
		 */
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}

		if (sc->sc_flags & NFE_USE_JUMBO) {
			physaddr =
			    sc->rxq.jbuf[sc->rxq.jbufmap[i]].physaddr;
			if ((jbuf = nfe_jalloc(sc, i)) == NULL) {
				if (len > MCLBYTES) {
					m_freem(mnew);
					ifp->if_ierrors++;
					goto skip1;
				}
				MCLGET(mnew, M_DONTWAIT);
				if ((mnew->m_flags & M_EXT) == 0) {
					m_freem(mnew);
					ifp->if_ierrors++;
					goto skip1;
				}

				(void)memcpy(mtod(mnew, void *),
				    mtod(data->m, const void *), len);
				m = mnew;
				goto mbufcopied;
			} else {
				MEXTADD(mnew, jbuf->buf, NFE_JBYTES, 0, nfe_jfree, sc);
				bus_dmamap_sync(sc->sc_dmat, sc->rxq.jmap,
				    mtod(data->m, char *) - (char *)sc->rxq.jpool,
				    NFE_JBYTES, BUS_DMASYNC_POSTREAD);

				physaddr = jbuf->physaddr;
			}
		} else {
			MCLGET(mnew, M_DONTWAIT);
			if ((mnew->m_flags & M_EXT) == 0) {
				m_freem(mnew);
				ifp->if_ierrors++;
				goto skip;
			}

			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, data->map);

			error = bus_dmamap_load(sc->sc_dmat, data->map,
			    mtod(mnew, void *), MCLBYTES, NULL,
			    BUS_DMA_READ | BUS_DMA_NOWAIT);
			if (error != 0) {
				m_freem(mnew);

				/* try to reload the old mbuf */
				error = bus_dmamap_load(sc->sc_dmat, data->map,
				    mtod(data->m, void *), MCLBYTES, NULL,
				    BUS_DMA_READ | BUS_DMA_NOWAIT);
				if (error != 0) {
					/* very unlikely that it will fail.. */
					panic("%s: could not load old rx mbuf",
					    device_xname(sc->sc_dev));
				}
				ifp->if_ierrors++;
				goto skip;
			}
			physaddr = data->map->dm_segs[0].ds_addr;
		}

		/*
		 * New mbuf successfully loaded, update Rx ring and continue
		 * processing.
		 */
		m = data->m;
		data->m = mnew;

mbufcopied:
		/* finalize mbuf */
		m->m_pkthdr.len = m->m_len = len;
		m->m_pkthdr.rcvif = ifp;

		if ((sc->sc_flags & NFE_HW_CSUM) != 0) {
			/*
			 * XXX
			 * no way to check M_CSUM_IPv4_BAD or non-IPv4 packets?
			 */
			if (flags & NFE_RX_IP_CSUMOK) {
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
				DPRINTFN(3, ("%s: ip4csum-rx ok\n",
				    device_xname(sc->sc_dev)));
			}
			/*
			 * XXX
			 * no way to check M_CSUM_TCP_UDP_BAD or
			 * other protocols?
			 */
			if (flags & NFE_RX_UDP_CSUMOK) {
				m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
				DPRINTFN(3, ("%s: udp4csum-rx ok\n",
				    device_xname(sc->sc_dev)));
			} else if (flags & NFE_RX_TCP_CSUMOK) {
				m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
				DPRINTFN(3, ("%s: tcp4csum-rx ok\n",
				    device_xname(sc->sc_dev)));
			}
		}
		bpf_mtap(ifp, m);
		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, m);

skip1:
		/* update mapping address in h/w descriptor */
		if (sc->sc_flags & NFE_40BIT_ADDR) {
#if defined(__LP64__)
			desc64->physaddr[0] = htole32(physaddr >> 32);
#endif
			desc64->physaddr[1] = htole32(physaddr & 0xffffffff);
		} else {
			desc32->physaddr = htole32(physaddr);
		}

skip:
		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc64->length = htole16(sc->rxq.bufsz);
			desc64->flags = htole16(NFE_RX_READY);

			nfe_rxdesc64_sync(sc, desc64,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		} else {
			desc32->length = htole16(sc->rxq.bufsz);
			desc32->flags = htole16(NFE_RX_READY);

			nfe_rxdesc32_sync(sc, desc32,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}
	}
	/* update current RX pointer */
	sc->rxq.cur = i;
}

void
nfe_txeof(struct nfe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_tx_data *data = NULL;
	int i;
	uint16_t flags;
	char buf[128];

	for (i = sc->txq.next;
	    sc->txq.queued > 0;
	    i = NFE_TX_NEXTDESC(i), sc->txq.queued--) {
		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[i];
			nfe_txdesc64_sync(sc, desc64,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			flags = le16toh(desc64->flags);
		} else {
			desc32 = &sc->txq.desc32[i];
			nfe_txdesc32_sync(sc, desc32,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			flags = le16toh(desc32->flags);
		}

		if ((flags & NFE_TX_VALID) != 0)
			break;

		data = &sc->txq.data[i];

		if ((sc->sc_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if ((flags & NFE_TX_LASTFRAG_V1) == 0 &&
			    data->m == NULL)
				continue;

			if ((flags & NFE_TX_ERROR_V1) != 0) {
				snprintb(buf, sizeof(buf), NFE_V1_TXERR, flags);
				aprint_error_dev(sc->sc_dev, "tx v1 error %s\n",
				    buf);
				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		} else {
			if ((flags & NFE_TX_LASTFRAG_V2) == 0 &&
			    data->m == NULL)
				continue;

			if ((flags & NFE_TX_ERROR_V2) != 0) {
				snprintb(buf, sizeof(buf), NFE_V2_TXERR, flags);
				aprint_error_dev(sc->sc_dev, "tx v2 error %s\n",
				    buf);
				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		}

		if (data->m == NULL) {	/* should not get there */
			aprint_error_dev(sc->sc_dev,
			    "last fragment bit w/o associated mbuf!\n");
			continue;
		}

		/* last fragment of the mbuf chain transmitted */
		bus_dmamap_sync(sc->sc_dmat, data->active, 0,
		    data->active->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->active);
		m_freem(data->m);
		data->m = NULL;
	}

	sc->txq.next = i;

	if (sc->txq.queued < NFE_TX_RING_COUNT) {
		/* at least one slot freed */
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	if (sc->txq.queued == 0) {
		/* all queued packets are sent */
		ifp->if_timer = 0;
	}
}

int
nfe_encap(struct nfe_softc *sc, struct mbuf *m0)
{
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_tx_data *data;
	bus_dmamap_t map;
	uint16_t flags, csumflags;
#if NVLAN > 0
	struct m_tag *mtag;
	uint32_t vtag = 0;
#endif
	int error, i, first;

	desc32 = NULL;
	desc64 = NULL;
	data = NULL;

	flags = 0;
	csumflags = 0;
	first = sc->txq.cur;

	map = sc->txq.data[first].map;

	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m0, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		return error;
	}

	if (sc->txq.queued + map->dm_nsegs >= NFE_TX_RING_COUNT - 1) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return ENOBUFS;
	}

#if NVLAN > 0
	/* setup h/w VLAN tagging */
	if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0)) != NULL)
		vtag = NFE_TX_VTAG | VLAN_TAG_VALUE(mtag);
#endif
	if ((sc->sc_flags & NFE_HW_CSUM) != 0) {
		if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4)
			csumflags |= NFE_TX_IP_CSUM;
		if (m0->m_pkthdr.csum_flags & (M_CSUM_TCPv4 | M_CSUM_UDPv4))
			csumflags |= NFE_TX_TCP_UDP_CSUM;
	}

	for (i = 0; i < map->dm_nsegs; i++) {
		data = &sc->txq.data[sc->txq.cur];

		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[sc->txq.cur];
#if defined(__LP64__)
			desc64->physaddr[0] =
			    htole32(map->dm_segs[i].ds_addr >> 32);
#endif
			desc64->physaddr[1] =
			    htole32(map->dm_segs[i].ds_addr & 0xffffffff);
			desc64->length = htole16(map->dm_segs[i].ds_len - 1);
			desc64->flags = htole16(flags);
			desc64->vtag = 0;
		} else {
			desc32 = &sc->txq.desc32[sc->txq.cur];

			desc32->physaddr = htole32(map->dm_segs[i].ds_addr);
			desc32->length = htole16(map->dm_segs[i].ds_len - 1);
			desc32->flags = htole16(flags);
		}

		/*
		 * Setting of the valid bit in the first descriptor is
		 * deferred until the whole chain is fully setup.
		 */
		flags |= NFE_TX_VALID;

		sc->txq.queued++;
		sc->txq.cur = NFE_TX_NEXTDESC(sc->txq.cur);
	}

	/* the whole mbuf chain has been setup */
	if (sc->sc_flags & NFE_40BIT_ADDR) {
		/* fix last descriptor */
		flags |= NFE_TX_LASTFRAG_V2;
		desc64->flags = htole16(flags);

		/* Checksum flags and vtag belong to the first fragment only. */
#if NVLAN > 0
		sc->txq.desc64[first].vtag = htole32(vtag);
#endif
		sc->txq.desc64[first].flags |= htole16(csumflags);

		/* finally, set the valid bit in the first descriptor */
		sc->txq.desc64[first].flags |= htole16(NFE_TX_VALID);
	} else {
		/* fix last descriptor */
		if (sc->sc_flags & NFE_JUMBO_SUP)
			flags |= NFE_TX_LASTFRAG_V2;
		else
			flags |= NFE_TX_LASTFRAG_V1;
		desc32->flags = htole16(flags);

		/* Checksum flags belong to the first fragment only. */
		sc->txq.desc32[first].flags |= htole16(csumflags);

		/* finally, set the valid bit in the first descriptor */
		sc->txq.desc32[first].flags |= htole16(NFE_TX_VALID);
	}

	data->m = m0;
	data->active = map;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return 0;
}

void
nfe_start(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	int old = sc->txq.queued;
	struct mbuf *m0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (nfe_encap(sc, m0) != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* packet put in h/w queue, remove from s/w queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		bpf_mtap(ifp, m0);
	}

	if (sc->txq.queued != old) {
		/* packets are queued */
		if (sc->sc_flags & NFE_40BIT_ADDR)
			nfe_txdesc64_rsync(sc, old, sc->txq.cur,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		else
			nfe_txdesc32_rsync(sc, old, sc->txq.cur,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		/* kick Tx */
		NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_KICKTX | sc->rxtxctl);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}
}

void
nfe_watchdog(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;

	aprint_error_dev(sc->sc_dev, "watchdog timeout\n");

	ifp->if_flags &= ~IFF_RUNNING;
	nfe_init(ifp);

	ifp->if_oerrors++;
}

int
nfe_init(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	uint32_t tmp;
	int rc = 0, s;

	if (ifp->if_flags & IFF_RUNNING)
		return 0;

	nfe_stop(ifp, 0);

	NFE_WRITE(sc, NFE_TX_UNK, 0);
	NFE_WRITE(sc, NFE_STATUS, 0);

	sc->rxtxctl = NFE_RXTX_BIT2;
	if (sc->sc_flags & NFE_40BIT_ADDR)
		sc->rxtxctl |= NFE_RXTX_V3MAGIC;
	else if (sc->sc_flags & NFE_JUMBO_SUP)
		sc->rxtxctl |= NFE_RXTX_V2MAGIC;
	if (sc->sc_flags & NFE_HW_CSUM)
		sc->rxtxctl |= NFE_RXTX_RXCSUM;
#if NVLAN > 0
	/*
	 * Although the adapter is capable of stripping VLAN tags from received
	 * frames (NFE_RXTX_VTAG_STRIP), we do not enable this functionality on
	 * purpose.  This will be done in software by our network stack.
	 */
	if (sc->sc_flags & NFE_HW_VLAN)
		sc->rxtxctl |= NFE_RXTX_VTAG_INSERT;
#endif
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | sc->rxtxctl);
	DELAY(10);
	NFE_WRITE(sc, NFE_RXTX_CTL, sc->rxtxctl);

#if NVLAN
	if (sc->sc_flags & NFE_HW_VLAN)
		NFE_WRITE(sc, NFE_VTAG_CTL, NFE_VTAG_ENABLE);
#endif

	NFE_WRITE(sc, NFE_SETUP_R6, 0);

	/* set MAC address */
	nfe_set_macaddr(sc, sc->sc_enaddr);

	/* tell MAC where rings are in memory */
#ifdef __LP64__
	NFE_WRITE(sc, NFE_RX_RING_ADDR_HI, sc->rxq.physaddr >> 32);
#endif
	NFE_WRITE(sc, NFE_RX_RING_ADDR_LO, sc->rxq.physaddr & 0xffffffff);
#ifdef __LP64__
	NFE_WRITE(sc, NFE_TX_RING_ADDR_HI, sc->txq.physaddr >> 32);
#endif
	NFE_WRITE(sc, NFE_TX_RING_ADDR_LO, sc->txq.physaddr & 0xffffffff);

	NFE_WRITE(sc, NFE_RING_SIZE,
	    (NFE_RX_RING_COUNT - 1) << 16 |
	    (NFE_TX_RING_COUNT - 1));

	NFE_WRITE(sc, NFE_RXBUFSZ, sc->rxq.bufsz);

	/* force MAC to wakeup */
	tmp = NFE_READ(sc, NFE_PWR_STATE);
	NFE_WRITE(sc, NFE_PWR_STATE, tmp | NFE_PWR_WAKEUP);
	DELAY(10);
	tmp = NFE_READ(sc, NFE_PWR_STATE);
	NFE_WRITE(sc, NFE_PWR_STATE, tmp | NFE_PWR_VALID);

	s = splnet();
	NFE_WRITE(sc, NFE_IRQ_MASK, 0);
	nfe_intr(sc); /* XXX clear IRQ status registers */
	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED);
	splx(s);

#if 1
	/* configure interrupts coalescing/mitigation */
	NFE_WRITE(sc, NFE_IMTIMER, NFE_IM_DEFAULT);
#else
	/* no interrupt mitigation: one interrupt per packet */
	NFE_WRITE(sc, NFE_IMTIMER, 970);
#endif

	NFE_WRITE(sc, NFE_SETUP_R1, NFE_R1_MAGIC);
	NFE_WRITE(sc, NFE_SETUP_R2, NFE_R2_MAGIC);
	NFE_WRITE(sc, NFE_SETUP_R6, NFE_R6_MAGIC);

	/* update MAC knowledge of PHY; generates a NFE_IRQ_LINK interrupt */
	NFE_WRITE(sc, NFE_STATUS, sc->mii_phyaddr << 24 | NFE_STATUS_MAGIC);

	NFE_WRITE(sc, NFE_SETUP_R4, NFE_R4_MAGIC);
	NFE_WRITE(sc, NFE_WOL_CTL, NFE_WOL_ENABLE);

	sc->rxtxctl &= ~NFE_RXTX_BIT2;
	NFE_WRITE(sc, NFE_RXTX_CTL, sc->rxtxctl);
	DELAY(10);
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_BIT1 | sc->rxtxctl);

	/* set Rx filter */
	nfe_setmulti(sc);

	if ((rc = ether_mediachange(ifp)) != 0)
		goto out;

	nfe_tick(sc);

	/* enable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, NFE_RX_START);

	/* enable Tx */
	NFE_WRITE(sc, NFE_TX_CTL, NFE_TX_START);

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	/* enable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED);

	callout_schedule(&sc->sc_tick_ch, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

out:
	return rc;
}

void
nfe_stop(struct ifnet *ifp, int disable)
{
	struct nfe_softc *sc = ifp->if_softc;

	callout_stop(&sc->sc_tick_ch);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	mii_down(&sc->sc_mii);

	/* abort Tx */
	NFE_WRITE(sc, NFE_TX_CTL, 0);

	/* disable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, 0);

	/* disable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, 0);

	/* reset Tx and Rx rings */
	nfe_reset_tx_ring(sc, &sc->txq);
	nfe_reset_rx_ring(sc, &sc->rxq);
}

int
nfe_alloc_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct nfe_jbuf *jbuf;
	void **desc;
	bus_addr_t physaddr;
	int i, nsegs, error, descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = (void **)&ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->cur = ring->next = 0;
	ring->bufsz = MCLBYTES;

	error = bus_dmamap_create(sc->sc_dmat, NFE_RX_RING_COUNT * descsize, 1,
	    NFE_RX_RING_COUNT * descsize, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create desc DMA map\n");
		ring->map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, NFE_RX_RING_COUNT * descsize,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    NFE_RX_RING_COUNT * descsize, (void **)desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, *desc,
	    NFE_RX_RING_COUNT * descsize, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	memset(*desc, 0, NFE_RX_RING_COUNT * descsize);
	ring->physaddr = ring->map->dm_segs[0].ds_addr;

	if (sc->sc_flags & NFE_USE_JUMBO) {
		ring->bufsz = NFE_JBYTES;
		if ((error = nfe_jpool_alloc(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate jumbo frames\n");
			goto fail;
		}
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		data = &sc->rxq.data[i];

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		if (sc->sc_flags & NFE_USE_JUMBO) {
			if ((jbuf = nfe_jalloc(sc, i)) == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "could not allocate jumbo buffer\n");
				goto fail;
			}
			MEXTADD(data->m, jbuf->buf, NFE_JBYTES, 0, nfe_jfree,
			    sc);

			physaddr = jbuf->physaddr;
		} else {
			error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
			    MCLBYTES, 0, BUS_DMA_NOWAIT, &data->map);
			if (error != 0) {
				aprint_error_dev(sc->sc_dev,
				    "could not create DMA map\n");
				data->map = NULL;
				goto fail;
			}
			MCLGET(data->m, M_DONTWAIT);
			if (!(data->m->m_flags & M_EXT)) {
				aprint_error_dev(sc->sc_dev,
				    "could not allocate mbuf cluster\n");
				error = ENOMEM;
				goto fail;
			}

			error = bus_dmamap_load(sc->sc_dmat, data->map,
			    mtod(data->m, void *), MCLBYTES, NULL,
			    BUS_DMA_READ | BUS_DMA_NOWAIT);
			if (error != 0) {
				aprint_error_dev(sc->sc_dev,
				    "could not load rx buf DMA map");
				goto fail;
			}
			physaddr = data->map->dm_segs[0].ds_addr;
		}

		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->rxq.desc64[i];
#if defined(__LP64__)
			desc64->physaddr[0] = htole32(physaddr >> 32);
#endif
			desc64->physaddr[1] = htole32(physaddr & 0xffffffff);
			desc64->length = htole16(sc->rxq.bufsz);
			desc64->flags = htole16(NFE_RX_READY);
		} else {
			desc32 = &sc->rxq.desc32[i];
			desc32->physaddr = htole32(physaddr);
			desc32->length = htole16(sc->rxq.bufsz);
			desc32->flags = htole16(NFE_RX_READY);
		}
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	nfe_free_rx_ring(sc, ring);
	return error;
}

void
nfe_reset_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	int i;

	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		if (sc->sc_flags & NFE_40BIT_ADDR) {
			ring->desc64[i].length = htole16(ring->bufsz);
			ring->desc64[i].flags = htole16(NFE_RX_READY);
		} else {
			ring->desc32[i].length = htole16(ring->bufsz);
			ring->desc32[i].flags = htole16(NFE_RX_READY);
		}
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}

void
nfe_free_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_rx_data *data;
	void *desc;
	int i, descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	if (desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (void *)desc,
		    NFE_RX_RING_COUNT * descsize);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->map != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		if (data->m != NULL)
			m_freem(data->m);
	}

	nfe_jpool_free(sc);
}

struct nfe_jbuf *
nfe_jalloc(struct nfe_softc *sc, int i)
{
	struct nfe_jbuf *jbuf;

	mutex_enter(&sc->rxq.mtx);
	jbuf = SLIST_FIRST(&sc->rxq.jfreelist);
	if (jbuf != NULL)
		SLIST_REMOVE_HEAD(&sc->rxq.jfreelist, jnext);
	mutex_exit(&sc->rxq.mtx);
	if (jbuf == NULL)
		return NULL;
	sc->rxq.jbufmap[i] =
	    ((char *)jbuf->buf - (char *)sc->rxq.jpool) / NFE_JBYTES;
	return jbuf;
}

/*
 * This is called automatically by the network stack when the mbuf is freed.
 * Caution must be taken that the NIC might be reset by the time the mbuf is
 * freed.
 */
void
nfe_jfree(struct mbuf *m, void *buf, size_t size, void *arg)
{
	struct nfe_softc *sc = arg;
	struct nfe_jbuf *jbuf;
	int i;

	/* find the jbuf from the base pointer */
	i = ((char *)buf - (char *)sc->rxq.jpool) / NFE_JBYTES;
	if (i < 0 || i >= NFE_JPOOL_COUNT) {
		aprint_error_dev(sc->sc_dev,
		    "request to free a buffer (%p) not managed by us\n", buf);
		return;
	}
	jbuf = &sc->rxq.jbuf[i];

	/* ..and put it back in the free list */
	mutex_enter(&sc->rxq.mtx);
	SLIST_INSERT_HEAD(&sc->rxq.jfreelist, jbuf, jnext);
	mutex_exit(&sc->rxq.mtx);

	if (m != NULL)
		pool_cache_put(mb_cache, m);
}

int
nfe_jpool_alloc(struct nfe_softc *sc)
{
	struct nfe_rx_ring *ring = &sc->rxq;
	struct nfe_jbuf *jbuf;
	bus_addr_t physaddr;
	char *buf;
	int i, nsegs, error;

	/*
	 * Allocate a big chunk of DMA'able memory.
	 */
	error = bus_dmamap_create(sc->sc_dmat, NFE_JPOOL_SIZE, 1,
	    NFE_JPOOL_SIZE, 0, BUS_DMA_NOWAIT, &ring->jmap);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create jumbo DMA map\n");
		ring->jmap = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, NFE_JPOOL_SIZE, PAGE_SIZE, 0,
	    &ring->jseg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate jumbo DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->jseg, nsegs, NFE_JPOOL_SIZE,
	    &ring->jpool, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map jumbo DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->jmap, ring->jpool,
	    NFE_JPOOL_SIZE, NULL, BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load jumbo DMA map\n");
		goto fail;
	}

	/* ..and split it into 9KB chunks */
	SLIST_INIT(&ring->jfreelist);

	buf = ring->jpool;
	physaddr = ring->jmap->dm_segs[0].ds_addr;
	for (i = 0; i < NFE_JPOOL_COUNT; i++) {
		jbuf = &ring->jbuf[i];

		jbuf->buf = buf;
		jbuf->physaddr = physaddr;

		SLIST_INSERT_HEAD(&ring->jfreelist, jbuf, jnext);

		buf += NFE_JBYTES;
		physaddr += NFE_JBYTES;
	}

	return 0;

fail:	nfe_jpool_free(sc);
	return error;
}

void
nfe_jpool_free(struct nfe_softc *sc)
{
	struct nfe_rx_ring *ring = &sc->rxq;

	if (ring->jmap != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->jmap, 0,
		    ring->jmap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->jmap);
		bus_dmamap_destroy(sc->sc_dmat, ring->jmap);
		ring->jmap = NULL;
	}
	if (ring->jpool != NULL) {
		bus_dmamem_unmap(sc->sc_dmat, ring->jpool, NFE_JPOOL_SIZE);
		bus_dmamem_free(sc->sc_dmat, &ring->jseg, 1);
		ring->jpool = NULL;
	}
}

int
nfe_alloc_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	int i, nsegs, error;
	void **desc;
	int descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = (void **)&ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat, NFE_TX_RING_COUNT * descsize, 1,
	    NFE_TX_RING_COUNT * descsize, 0, BUS_DMA_NOWAIT, &ring->map);

	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create desc DMA map\n");
		ring->map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, NFE_TX_RING_COUNT * descsize,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    NFE_TX_RING_COUNT * descsize, (void **)desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, *desc,
	    NFE_TX_RING_COUNT * descsize, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	memset(*desc, 0, NFE_TX_RING_COUNT * descsize);
	ring->physaddr = ring->map->dm_segs[0].ds_addr;

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, NFE_JBYTES,
		    NFE_MAX_SCATTER, NFE_JBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create DMA map\n");
			ring->data[i].map = NULL;
			goto fail;
		}
	}

	return 0;

fail:	nfe_free_tx_ring(sc, ring);
	return error;
}

void
nfe_reset_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_tx_data *data;
	int i;

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		if (sc->sc_flags & NFE_40BIT_ADDR)
			ring->desc64[i].flags = 0;
		else
			ring->desc32[i].flags = 0;

		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->active, 0,
			    data->active->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->active);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

void
nfe_free_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_tx_data *data;
	void *desc;
	int i, descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	if (desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (void *)desc,
		    NFE_TX_RING_COUNT * descsize);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->active, 0,
			    data->active->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->active);
			m_freem(data->m);
		}
	}

	/* ..and now actually destroy the DMA mappings */
	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		data = &ring->data[i];
		if (data->map == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

void
nfe_setmulti(struct nfe_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &ec->ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint8_t addr[ETHER_ADDR_LEN], mask[ETHER_ADDR_LEN];
	uint32_t filter = NFE_RXFILTER_MAGIC;
	int i;

	if ((ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		memset(addr, 0, ETHER_ADDR_LEN);
		memset(mask, 0, ETHER_ADDR_LEN);
		goto done;
	}

	memcpy(addr, etherbroadcastaddr, ETHER_ADDR_LEN);
	memcpy(mask, etherbroadcastaddr, ETHER_ADDR_LEN);

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			memset(addr, 0, ETHER_ADDR_LEN);
			memset(mask, 0, ETHER_ADDR_LEN);
			goto done;
		}
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			addr[i] &=  enm->enm_addrlo[i];
			mask[i] &= ~enm->enm_addrlo[i];
		}
		ETHER_NEXT_MULTI(step, enm);
	}
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		mask[i] |= addr[i];

done:
	addr[0] |= 0x01;	/* make sure multicast bit is set */

	NFE_WRITE(sc, NFE_MULTIADDR_HI,
	    addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0]);
	NFE_WRITE(sc, NFE_MULTIADDR_LO,
	    addr[5] <<  8 | addr[4]);
	NFE_WRITE(sc, NFE_MULTIMASK_HI,
	    mask[3] << 24 | mask[2] << 16 | mask[1] << 8 | mask[0]);
	NFE_WRITE(sc, NFE_MULTIMASK_LO,
	    mask[5] <<  8 | mask[4]);

	filter |= (ifp->if_flags & IFF_PROMISC) ? NFE_PROMISC : NFE_U2M;
	NFE_WRITE(sc, NFE_RXFILTER, filter);
}

void
nfe_get_macaddr(struct nfe_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	if ((sc->sc_flags & NFE_CORRECT_MACADDR) != 0) {
		tmp = NFE_READ(sc, NFE_MACADDR_HI);
		addr[0] = (tmp & 0xff);
		addr[1] = (tmp >>  8) & 0xff;
		addr[2] = (tmp >> 16) & 0xff;
		addr[3] = (tmp >> 24) & 0xff;

		tmp = NFE_READ(sc, NFE_MACADDR_LO);
		addr[4] = (tmp & 0xff);
		addr[5] = (tmp >> 8) & 0xff;

	} else {
		tmp = NFE_READ(sc, NFE_MACADDR_LO);
		addr[0] = (tmp >> 8) & 0xff;
		addr[1] = (tmp & 0xff);

		tmp = NFE_READ(sc, NFE_MACADDR_HI);
		addr[2] = (tmp >> 24) & 0xff;
		addr[3] = (tmp >> 16) & 0xff;
		addr[4] = (tmp >>  8) & 0xff;
		addr[5] = (tmp & 0xff);
	}
}

void
nfe_set_macaddr(struct nfe_softc *sc, const uint8_t *addr)
{
	NFE_WRITE(sc, NFE_MACADDR_LO,
	    addr[5] <<  8 | addr[4]);
	NFE_WRITE(sc, NFE_MACADDR_HI,
	    addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0]);
}

void
nfe_tick(void *arg)
{
	struct nfe_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_schedule(&sc->sc_tick_ch, hz);
}

void
nfe_poweron(device_t self)
{
	struct nfe_softc *sc = device_private(self);

	if ((sc->sc_flags & NFE_PWR_MGMT) != 0) {
		NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | NFE_RXTX_BIT2);
		NFE_WRITE(sc, NFE_MAC_RESET, NFE_MAC_RESET_MAGIC);
		DELAY(100);
		NFE_WRITE(sc, NFE_MAC_RESET, 0);
		DELAY(100);
		NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_BIT2);
		NFE_WRITE(sc, NFE_PWR2_CTL,
		    NFE_READ(sc, NFE_PWR2_CTL) & ~NFE_PWR2_WAKEUP_MASK);
	}
}

bool
nfe_resume(device_t dv, const pmf_qual_t *qual)
{
	nfe_poweron(dv);

	return true;
}
