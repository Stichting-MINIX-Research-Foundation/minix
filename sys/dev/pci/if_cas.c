/*	$NetBSD: if_cas.c,v 1.23 2015/04/13 16:33:25 riastradh Exp $	*/
/*	$OpenBSD: if_cas.c,v 1.29 2009/11/29 16:19:38 kettenis Exp $	*/

/*
 *
 * Copyright (C) 2007 Mark Kettenis.
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Driver for Sun Cassini ethernet controllers.
 *
 * There are basically two variants of this chip: Cassini and
 * Cassini+.  We can distinguish between the two by revision: 0x10 and
 * up are Cassini+.  The most important difference is that Cassini+
 * has a second RX descriptor ring.  Cassini+ will not work without
 * configuring that second ring.  However, since we don't use it we
 * don't actually fill the descriptors, and only hand off the first
 * four to the chip.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_cas.c,v 1.23 2015/04/13 16:33:25 riastradh Exp $");

#ifndef _MODULE
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/module.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#endif

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/rndsource.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <prop/proplib.h>

#include <dev/pci/if_casreg.h>
#include <dev/pci/if_casvar.h>

#define TRIES	10000

static bool	cas_estintr(struct cas_softc *sc, int);
bool		cas_shutdown(device_t, int);
static bool	cas_suspend(device_t, const pmf_qual_t *);
static bool	cas_resume(device_t, const pmf_qual_t *);
static int	cas_detach(device_t, int);
static void	cas_partial_detach(struct cas_softc *, enum cas_attach_stage);

int		cas_match(device_t, cfdata_t, void *);
void		cas_attach(device_t, device_t, void *);


CFATTACH_DECL3_NEW(cas, sizeof(struct cas_softc),
    cas_match, cas_attach, cas_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

int	cas_pci_enaddr(struct cas_softc *, struct pci_attach_args *, uint8_t *);

void		cas_config(struct cas_softc *, const uint8_t *);
void		cas_start(struct ifnet *);
void		cas_stop(struct ifnet *, int);
int		cas_ioctl(struct ifnet *, u_long, void *);
void		cas_tick(void *);
void		cas_watchdog(struct ifnet *);
int		cas_init(struct ifnet *);
void		cas_init_regs(struct cas_softc *);
int		cas_ringsize(int);
int		cas_cringsize(int);
int		cas_meminit(struct cas_softc *);
void		cas_mifinit(struct cas_softc *);
int		cas_bitwait(struct cas_softc *, bus_space_handle_t, int,
		    u_int32_t, u_int32_t);
void		cas_reset(struct cas_softc *);
int		cas_reset_rx(struct cas_softc *);
int		cas_reset_tx(struct cas_softc *);
int		cas_disable_rx(struct cas_softc *);
int		cas_disable_tx(struct cas_softc *);
void		cas_rxdrain(struct cas_softc *);
int		cas_add_rxbuf(struct cas_softc *, int idx);
void		cas_iff(struct cas_softc *);
int		cas_encap(struct cas_softc *, struct mbuf *, u_int32_t *);

/* MII methods & callbacks */
int		cas_mii_readreg(device_t, int, int);
void		cas_mii_writereg(device_t, int, int, int);
void		cas_mii_statchg(struct ifnet *);
int		cas_pcs_readreg(device_t, int, int);
void		cas_pcs_writereg(device_t, int, int, int);

int		cas_mediachange(struct ifnet *);
void		cas_mediastatus(struct ifnet *, struct ifmediareq *);

int		cas_eint(struct cas_softc *, u_int);
int		cas_rint(struct cas_softc *);
int		cas_tint(struct cas_softc *, u_int32_t);
int		cas_pint(struct cas_softc *);
int		cas_intr(void *);

#ifdef CAS_DEBUG
#define	DPRINTF(sc, x)	if ((sc)->sc_ethercom.ec_if.if_flags & IFF_DEBUG) \
				printf x
#else
#define	DPRINTF(sc, x)	/* nothing */
#endif

int
cas_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_CASSINI))
		return 1;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NS &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NS_SATURN))
		return 1;

	return 0;
}

#define	PROMHDR_PTR_DATA	0x18
#define	PROMDATA_PTR_VPD	0x08
#define	PROMDATA_DATA2		0x0a

static const u_int8_t cas_promhdr[] = { 0x55, 0xaa };
static const u_int8_t cas_promdat[] = {
	'P', 'C', 'I', 'R',
	PCI_VENDOR_SUN & 0xff, PCI_VENDOR_SUN >> 8,
	PCI_PRODUCT_SUN_CASSINI & 0xff, PCI_PRODUCT_SUN_CASSINI >> 8
};
static const u_int8_t cas_promdat_ns[] = {
	'P', 'C', 'I', 'R',
	PCI_VENDOR_NS & 0xff, PCI_VENDOR_NS >> 8,
	PCI_PRODUCT_NS_SATURN & 0xff, PCI_PRODUCT_NS_SATURN >> 8
};

static const u_int8_t cas_promdat2[] = {
	0x18, 0x00,			/* structure length */
	0x00,				/* structure revision */
	0x00,				/* interface revision */
	PCI_SUBCLASS_NETWORK_ETHERNET,	/* subclass code */
	PCI_CLASS_NETWORK		/* class code */
};

int
cas_pci_enaddr(struct cas_softc *sc, struct pci_attach_args *pa,
    uint8_t *enaddr)
{
	struct pci_vpd_largeres *res;
	struct pci_vpd *vpd;
	bus_space_handle_t romh;
	bus_space_tag_t romt;
	bus_size_t romsize = 0;
	u_int8_t buf[32], *desc;
	pcireg_t address;
	int dataoff, vpdoff, len;
	int rv = -1;

	if (pci_mapreg_map(pa, PCI_MAPREG_ROM, PCI_MAPREG_TYPE_MEM, 0,
	    &romt, &romh, NULL, &romsize))
		return (-1);

	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	address |= PCI_MAPREG_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START, address);

	bus_space_read_region_1(romt, romh, 0, buf, sizeof(buf));
	if (bcmp(buf, cas_promhdr, sizeof(cas_promhdr)))
		goto fail;

	dataoff = buf[PROMHDR_PTR_DATA] | (buf[PROMHDR_PTR_DATA + 1] << 8);
	if (dataoff < 0x1c)
		goto fail;

	bus_space_read_region_1(romt, romh, dataoff, buf, sizeof(buf));
	if ((bcmp(buf, cas_promdat, sizeof(cas_promdat)) &&
	     bcmp(buf, cas_promdat_ns, sizeof(cas_promdat_ns))) ||
	    bcmp(buf + PROMDATA_DATA2, cas_promdat2, sizeof(cas_promdat2)))
		goto fail;

	vpdoff = buf[PROMDATA_PTR_VPD] | (buf[PROMDATA_PTR_VPD + 1] << 8);
	if (vpdoff < 0x1c)
		goto fail;

next:
	bus_space_read_region_1(romt, romh, vpdoff, buf, sizeof(buf));
	if (!PCI_VPDRES_ISLARGE(buf[0]))
		goto fail;

	res = (struct pci_vpd_largeres *)buf;
	vpdoff += sizeof(*res);

	len = ((res->vpdres_len_msb << 8) + res->vpdres_len_lsb);
	switch(PCI_VPDRES_LARGE_NAME(res->vpdres_byte0)) {
	case PCI_VPDRES_TYPE_IDENTIFIER_STRING:
		/* Skip identifier string. */
		vpdoff += len;
		goto next;

	case PCI_VPDRES_TYPE_VPD:
		while (len > 0) {
			bus_space_read_region_1(romt, romh, vpdoff,
			     buf, sizeof(buf));

			vpd = (struct pci_vpd *)buf;
			vpdoff += sizeof(*vpd) + vpd->vpd_len;
			len -= sizeof(*vpd) + vpd->vpd_len;

			/*
			 * We're looking for an "Enhanced" VPD...
			 */
			if (vpd->vpd_key0 != 'Z')
				continue;

			desc = buf + sizeof(*vpd);

			/*
			 * ...which is an instance property...
			 */
			if (desc[0] != 'I')
				continue;
			desc += 3;

			/*
			 * ...that's a byte array with the proper
			 * length for a MAC address...
			 */
			if (desc[0] != 'B' || desc[1] != ETHER_ADDR_LEN)
				continue;
			desc += 2;

			/*
			 * ...named "local-mac-address".
			 */
			if (strcmp(desc, "local-mac-address") != 0)
				continue;
			desc += strlen("local-mac-address") + 1;
				
			memcpy(enaddr, desc, ETHER_ADDR_LEN);
			rv = 0;
		}
		break;

	default:
		goto fail;
	}

 fail:
	if (romsize != 0)
		bus_space_unmap(romt, romh, romsize);

	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM);
	address &= ~PCI_MAPREG_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM, address);

	return (rv);
}

void
cas_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct cas_softc *sc = device_private(self);
	prop_data_t data;
	uint8_t enaddr[ETHER_ADDR_LEN];

	sc->sc_dev = self;
	pci_aprint_devinfo(pa, NULL);
	sc->sc_rev = PCI_REVISION(pa->pa_class);
	sc->sc_dmatag = pa->pa_dmat;

#define PCI_CAS_BASEADDR	0x10
	if (pci_mapreg_map(pa, PCI_CAS_BASEADDR, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_size) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map device registers\n");
		return;
	}

	if ((data = prop_dictionary_get(device_properties(sc->sc_dev),
	    "mac-address")) != NULL)
		memcpy(enaddr, prop_data_data_nocopy(data), ETHER_ADDR_LEN);
	else if (cas_pci_enaddr(sc, pa, enaddr) != 0) {
		aprint_error_dev(sc->sc_dev, "no Ethernet address found\n");
		memset(enaddr, 0, sizeof(enaddr));
	}

	sc->sc_burst = 16;	/* XXX */

	sc->sc_att_stage = CAS_ATT_BACKEND_0;

	if (pci_intr_map(pa, &sc->sc_handle) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_size);
		return;
	}
	sc->sc_pc = pa->pa_pc;
	if (!cas_estintr(sc, CAS_INTR_PCI)) {
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_size);
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt\n");
		return;
	}

	sc->sc_att_stage = CAS_ATT_BACKEND_1;

	/*
	 * call the main configure
	 */
	cas_config(sc, enaddr);

	if (pmf_device_register1(sc->sc_dev,
	    cas_suspend, cas_resume, cas_shutdown))
		pmf_class_network_register(sc->sc_dev, &sc->sc_ethercom.ec_if);
	else
		aprint_error_dev(sc->sc_dev,
		    "could not establish power handlers\n");

	sc->sc_att_stage = CAS_ATT_FINISHED;
		/*FALLTHROUGH*/
}

/*
 * cas_config:
 *
 *	Attach a Cassini interface to the system.
 */
void
cas_config(struct cas_softc *sc, const uint8_t *enaddr)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;
	struct mii_softc *child;
	int i, error;

	/* Make sure the chip is stopped. */
	ifp->if_softc = sc;
	cas_reset(sc);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmatag,
	    sizeof(struct cas_control_data), CAS_PAGE_SIZE, 0, &sc->sc_cdseg,
	    1, &sc->sc_cdnseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control data, error = %d\n",
		    error);
		cas_partial_detach(sc, CAS_ATT_0);
	}

	/* XXX should map this in with correct endianness */
	if ((error = bus_dmamem_map(sc->sc_dmatag, &sc->sc_cdseg, sc->sc_cdnseg,
	    sizeof(struct cas_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control data, error = %d\n", error);
		cas_partial_detach(sc, CAS_ATT_1);
	}

	if ((error = bus_dmamap_create(sc->sc_dmatag,
	    sizeof(struct cas_control_data), 1,
	    sizeof(struct cas_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control data DMA map, error = %d\n", error);
		cas_partial_detach(sc, CAS_ATT_2);
	}

	if ((error = bus_dmamap_load(sc->sc_dmatag, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct cas_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n",
		    error);
		cas_partial_detach(sc, CAS_ATT_3);
	}

	memset(sc->sc_control_data, 0, sizeof(struct cas_control_data));

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < CAS_NRXDESC; i++) {
		bus_dma_segment_t seg;
		char *kva;
		int rseg;

		if ((error = bus_dmamem_alloc(sc->sc_dmatag, CAS_PAGE_SIZE,
		    CAS_PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to alloc rx DMA mem %d, error = %d\n",
			    i, error);
			cas_partial_detach(sc, CAS_ATT_5);
		}
		sc->sc_rxsoft[i].rxs_dmaseg = seg;

		if ((error = bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
		    CAS_PAGE_SIZE, (void **)&kva, BUS_DMA_NOWAIT)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to alloc rx DMA mem %d, error = %d\n",
			    i, error);
			cas_partial_detach(sc, CAS_ATT_5);
		}
		sc->sc_rxsoft[i].rxs_kva = kva;

		if ((error = bus_dmamap_create(sc->sc_dmatag, CAS_PAGE_SIZE, 1,
		    CAS_PAGE_SIZE, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create rx DMA map %d, error = %d\n",
			    i, error);
			cas_partial_detach(sc, CAS_ATT_5);
		}

		if ((error = bus_dmamap_load(sc->sc_dmatag,
		   sc->sc_rxsoft[i].rxs_dmamap, kva, CAS_PAGE_SIZE, NULL,
		   BUS_DMA_NOWAIT)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to load rx DMA map %d, error = %d\n",
			    i, error);
			cas_partial_detach(sc, CAS_ATT_5);
		}
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < CAS_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmatag, MCLBYTES,
		    CAS_NTXSEGS, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->sc_txd[i].sd_map)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create tx DMA map %d, error = %d\n",
			    i, error);
			cas_partial_detach(sc, CAS_ATT_6);
		}
		sc->sc_txd[i].sd_mbuf = NULL;
	}

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */

	/* Announce ourselves. */
	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(enaddr));
	aprint_naive(": Ethernet controller\n");

	/* Get RX FIFO size */
	sc->sc_rxfifosize = 16 * 1024;

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	ifp->if_start = cas_start;
	ifp->if_ioctl = cas_ioctl;
	ifp->if_watchdog = cas_watchdog;
	ifp->if_stop = cas_stop;
	ifp->if_init = cas_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, CAS_NTXDESC - 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize ifmedia structures and MII info */
	mii->mii_ifp = ifp;
	mii->mii_readreg = cas_mii_readreg;
	mii->mii_writereg = cas_mii_writereg;
	mii->mii_statchg = cas_mii_statchg;

	ifmedia_init(&mii->mii_media, 0, cas_mediachange, cas_mediastatus);
	sc->sc_ethercom.ec_mii = mii;

	bus_space_write_4(sc->sc_memt, sc->sc_memh, CAS_MII_DATAPATH_MODE, 0);

	cas_mifinit(sc);

	if (sc->sc_mif_config & CAS_MIF_CONFIG_MDI1) {
		sc->sc_mif_config |= CAS_MIF_CONFIG_PHY_SEL;
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
	            CAS_MIF_CONFIG, sc->sc_mif_config);
	}

	mii_attach(sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	child = LIST_FIRST(&mii->mii_phys);
	if (child == NULL &&
	    sc->sc_mif_config & (CAS_MIF_CONFIG_MDI0|CAS_MIF_CONFIG_MDI1)) {
		/*
		 * Try the external PCS SERDES if we didn't find any
		 * MII devices.
		 */
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    CAS_MII_DATAPATH_MODE, CAS_MII_DATAPATH_SERDES);

		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		     CAS_MII_CONFIG, CAS_MII_CONFIG_ENABLE);

		mii->mii_readreg = cas_pcs_readreg;
		mii->mii_writereg = cas_pcs_writereg;

		mii_attach(sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY,
		    MII_OFFSET_ANY, MIIF_NOISOLATE);
	}

	child = LIST_FIRST(&mii->mii_phys);
	if (child == NULL) {
		/* No PHY attached */
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	} else {
		/*
		 * Walk along the list of attached MII devices and
		 * establish an `MII instance' to `phy number'
		 * mapping. We'll use this mapping in media change
		 * requests to determine which phy to use to program
		 * the MIF configuration register.
		 */
		for (; child != NULL; child = LIST_NEXT(child, mii_list)) {
			/*
			 * Note: we support just two PHYs: the built-in
			 * internal device and an external on the MII
			 * connector.
			 */
			if (child->mii_phy > 1 || child->mii_inst > 1) {
				aprint_error_dev(sc->sc_dev,
				    "cannot accommodate MII device %s"
				    " at phy %d, instance %d\n",
				    device_xname(child->mii_dev),
				    child->mii_phy, child->mii_inst);
				continue;
			}

			sc->sc_phys[child->mii_inst] = child->mii_phy;
		}

		/*
		 * XXX - we can really do the following ONLY if the
		 * phy indeed has the auto negotiation capability!!
		 */
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);
	}

	/* claim 802.1q capability */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	evcnt_attach_dynamic(&sc->sc_ev_intr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "interrupts");

	callout_init(&sc->sc_tick_ch, 0);

	return;
}

int
cas_detach(device_t self, int flags)
{
	int i;
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
	switch (sc->sc_att_stage) {
	case CAS_ATT_FINISHED:
		bus_space_write_4(t, h, CAS_INTMASK, ~(uint32_t)0);
		pmf_device_deregister(self);
		cas_stop(&sc->sc_ethercom.ec_if, 1);
		evcnt_detach(&sc->sc_ev_intr);

		rnd_detach_source(&sc->rnd_source);

		ether_ifdetach(ifp);
		if_detach(ifp);
		ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

		callout_destroy(&sc->sc_tick_ch);

		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

		/*FALLTHROUGH*/
	case CAS_ATT_MII:
	case CAS_ATT_7:
	case CAS_ATT_6:
		for (i = 0; i < CAS_NTXDESC; i++) {
			if (sc->sc_txd[i].sd_map != NULL)
				bus_dmamap_destroy(sc->sc_dmatag,
				    sc->sc_txd[i].sd_map);
		}
		/*FALLTHROUGH*/
	case CAS_ATT_5:
		for (i = 0; i < CAS_NRXDESC; i++) {
			if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
				bus_dmamap_unload(sc->sc_dmatag,
				    sc->sc_rxsoft[i].rxs_dmamap);
			if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
				bus_dmamap_destroy(sc->sc_dmatag,
				    sc->sc_rxsoft[i].rxs_dmamap);
			if (sc->sc_rxsoft[i].rxs_kva != NULL)
				bus_dmamem_unmap(sc->sc_dmatag,
				    sc->sc_rxsoft[i].rxs_kva, CAS_PAGE_SIZE);
			/* XXX   need to check that bus_dmamem_alloc suceeded
			if (sc->sc_rxsoft[i].rxs_dmaseg != NULL)
			*/
				bus_dmamem_free(sc->sc_dmatag,
				    &(sc->sc_rxsoft[i].rxs_dmaseg), 1);
		}
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case CAS_ATT_4:
	case CAS_ATT_3:
		bus_dmamap_destroy(sc->sc_dmatag, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case CAS_ATT_2:
		bus_dmamem_unmap(sc->sc_dmatag, sc->sc_control_data,
		    sizeof(struct cas_control_data));
		/*FALLTHROUGH*/
	case CAS_ATT_1:
		bus_dmamem_free(sc->sc_dmatag, &sc->sc_cdseg, sc->sc_cdnseg);
		/*FALLTHROUGH*/
	case CAS_ATT_0:
		sc->sc_att_stage = CAS_ATT_0;
		/*FALLTHROUGH*/
	case CAS_ATT_BACKEND_2:
	case CAS_ATT_BACKEND_1:
		if (sc->sc_ih != NULL) {
			pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
			sc->sc_ih = NULL;
		}
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_size);
		/*FALLTHROUGH*/
	case CAS_ATT_BACKEND_0:
		break;
	}
	return 0;
}

static void
cas_partial_detach(struct cas_softc *sc, enum cas_attach_stage stage)
{
	cfattach_t ca = device_cfattach(sc->sc_dev);

	sc->sc_att_stage = stage;
	(*ca->ca_detach)(sc->sc_dev, 0);
}

void
cas_tick(void *arg)
{
	struct cas_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t mac = sc->sc_memh;
	int s;
	u_int32_t v;

	/* unload collisions counters */
	v = bus_space_read_4(t, mac, CAS_MAC_EXCESS_COLL_CNT) +
	    bus_space_read_4(t, mac, CAS_MAC_LATE_COLL_CNT);
	ifp->if_collisions += v +
	    bus_space_read_4(t, mac, CAS_MAC_NORM_COLL_CNT) +
	    bus_space_read_4(t, mac, CAS_MAC_FIRST_COLL_CNT);
	ifp->if_oerrors += v;

	/* read error counters */
	ifp->if_ierrors +=
	    bus_space_read_4(t, mac, CAS_MAC_RX_LEN_ERR_CNT) +
	    bus_space_read_4(t, mac, CAS_MAC_RX_ALIGN_ERR) +
	    bus_space_read_4(t, mac, CAS_MAC_RX_CRC_ERR_CNT) +
	    bus_space_read_4(t, mac, CAS_MAC_RX_CODE_VIOL);

	/* clear the hardware counters */
	bus_space_write_4(t, mac, CAS_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_LATE_COLL_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_RX_LEN_ERR_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_RX_ALIGN_ERR, 0);
	bus_space_write_4(t, mac, CAS_MAC_RX_CRC_ERR_CNT, 0);
	bus_space_write_4(t, mac, CAS_MAC_RX_CODE_VIOL, 0);

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, cas_tick, sc);
}

int
cas_bitwait(struct cas_softc *sc, bus_space_handle_t h, int r,
    u_int32_t clr, u_int32_t set)
{
	int i;
	u_int32_t reg;

	for (i = TRIES; i--; DELAY(100)) {
		reg = bus_space_read_4(sc->sc_memt, h, r);
		if ((reg & clr) == 0 && (reg & set) == set)
			return (1);
	}

	return (0);
}

void
cas_reset(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	int s;

	s = splnet();
	DPRINTF(sc, ("%s: cas_reset\n", device_xname(sc->sc_dev)));
	cas_reset_rx(sc);
	cas_reset_tx(sc);

	/* Disable interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CAS_INTMASK, ~(uint32_t)0);

	/* Do a full reset */
	bus_space_write_4(t, h, CAS_RESET,
	    CAS_RESET_RX | CAS_RESET_TX | CAS_RESET_BLOCK_PCS);
	if (!cas_bitwait(sc, h, CAS_RESET, CAS_RESET_RX | CAS_RESET_TX, 0))
		aprint_error_dev(sc->sc_dev, "cannot reset device\n");
	splx(s);
}


/*
 * cas_rxdrain:
 *
 *	Drain the receive queue.
 */
void
cas_rxdrain(struct cas_softc *sc)
{
	/* Nothing to do yet. */
}

/*
 * Reset the whole thing.
 */
void
cas_stop(struct ifnet *ifp, int disable)
{
	struct cas_softc *sc = (struct cas_softc *)ifp->if_softc;
	struct cas_sxd *sd;
	u_int32_t i;

	DPRINTF(sc, ("%s: cas_stop\n", device_xname(sc->sc_dev)));

	callout_stop(&sc->sc_tick_ch);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	mii_down(&sc->sc_mii);

	cas_reset_rx(sc);
	cas_reset_tx(sc);

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < CAS_NTXDESC; i++) {
		sd = &sc->sc_txd[i];
		if (sd->sd_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmatag, sd->sd_map, 0,
			    sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmatag, sd->sd_map);
			m_freem(sd->sd_mbuf);
			sd->sd_mbuf = NULL;
		}
	}
	sc->sc_tx_cnt = sc->sc_tx_prod = sc->sc_tx_cons = 0;

	if (disable)
		cas_rxdrain(sc);
}


/*
 * Reset the receiver
 */
int
cas_reset_rx(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	cas_disable_rx(sc);
	bus_space_write_4(t, h, CAS_RX_CONFIG, 0);
	/* Wait till it finishes */
	if (!cas_bitwait(sc, h, CAS_RX_CONFIG, 1, 0))
		aprint_error_dev(sc->sc_dev, "cannot disable rx dma\n");
	/* Wait 5ms extra. */
	delay(5000);

	/* Finally, reset the ERX */
	bus_space_write_4(t, h, CAS_RESET, CAS_RESET_RX);
	/* Wait till it finishes */
	if (!cas_bitwait(sc, h, CAS_RESET, CAS_RESET_RX, 0)) {
		aprint_error_dev(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}


/*
 * Reset the transmitter
 */
int
cas_reset_tx(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	cas_disable_tx(sc);
	bus_space_write_4(t, h, CAS_TX_CONFIG, 0);
	/* Wait till it finishes */
	if (!cas_bitwait(sc, h, CAS_TX_CONFIG, 1, 0))
		aprint_error_dev(sc->sc_dev, "cannot disable tx dma\n");
	/* Wait 5ms extra. */
	delay(5000);

	/* Finally, reset the ETX */
	bus_space_write_4(t, h, CAS_RESET, CAS_RESET_TX);
	/* Wait till it finishes */
	if (!cas_bitwait(sc, h, CAS_RESET, CAS_RESET_TX, 0)) {
		aprint_error_dev(sc->sc_dev, "cannot reset transmitter\n");
		return (1);
	}
	return (0);
}

/*
 * Disable receiver.
 */
int
cas_disable_rx(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, CAS_MAC_RX_CONFIG);
	cfg &= ~CAS_MAC_RX_ENABLE;
	bus_space_write_4(t, h, CAS_MAC_RX_CONFIG, cfg);

	/* Wait for it to finish */
	return (cas_bitwait(sc, h, CAS_MAC_RX_CONFIG, CAS_MAC_RX_ENABLE, 0));
}

/*
 * Disable transmitter.
 */
int
cas_disable_tx(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, CAS_MAC_TX_CONFIG);
	cfg &= ~CAS_MAC_TX_ENABLE;
	bus_space_write_4(t, h, CAS_MAC_TX_CONFIG, cfg);

	/* Wait for it to finish */
	return (cas_bitwait(sc, h, CAS_MAC_TX_CONFIG, CAS_MAC_TX_ENABLE, 0));
}

/*
 * Initialize interface.
 */
int
cas_meminit(struct cas_softc *sc)
{
	int i;

	/*
	 * Initialize the transmit descriptor ring.
	 */
	for (i = 0; i < CAS_NTXDESC; i++) {
		sc->sc_txdescs[i].cd_flags = 0;
		sc->sc_txdescs[i].cd_addr = 0;
	}
	CAS_CDTXSYNC(sc, 0, CAS_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < CAS_NRXDESC; i++)
		CAS_INIT_RXDESC(sc, i, i);
	sc->sc_rxdptr = 0;
	sc->sc_rxptr = 0;

	/*
	 * Initialize the receive completion ring.
	 */
	for (i = 0; i < CAS_NRXCOMP; i++) {
		sc->sc_rxcomps[i].cc_word[0] = 0;
		sc->sc_rxcomps[i].cc_word[1] = 0;
		sc->sc_rxcomps[i].cc_word[2] = 0;
		sc->sc_rxcomps[i].cc_word[3] = CAS_DMA_WRITE(CAS_RC3_OWN);
		CAS_CDRXCSYNC(sc, i,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}

	return (0);
}

int
cas_ringsize(int sz)
{
	switch (sz) {
	case 32:
		return CAS_RING_SZ_32;
	case 64:
		return CAS_RING_SZ_64;
	case 128:
		return CAS_RING_SZ_128;
	case 256:
		return CAS_RING_SZ_256;
	case 512:
		return CAS_RING_SZ_512;
	case 1024:
		return CAS_RING_SZ_1024;
	case 2048:
		return CAS_RING_SZ_2048;
	case 4096:
		return CAS_RING_SZ_4096;
	case 8192:
		return CAS_RING_SZ_8192;
	default:
		aprint_error("cas: invalid Receive Descriptor ring size %d\n",
		    sz);
		return CAS_RING_SZ_32;
	}
}

int
cas_cringsize(int sz)
{
	int i;

	for (i = 0; i < 9; i++)
		if (sz == (128 << i))
			return i;

	aprint_error("cas: invalid completion ring size %d\n", sz);
	return 128;
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
int
cas_init(struct ifnet *ifp)
{
	struct cas_softc *sc = (struct cas_softc *)ifp->if_softc;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	int s;
	u_int max_frame_size;
	u_int32_t v;

	s = splnet();

	DPRINTF(sc, ("%s: cas_init: calling stop\n", device_xname(sc->sc_dev)));
	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	cas_stop(ifp, 0);
	cas_reset(sc);
	DPRINTF(sc, ("%s: cas_init: restarting\n", device_xname(sc->sc_dev)));

	/* Re-initialize the MIF */
	cas_mifinit(sc);

	/* step 3. Setup data structures in host memory */
	cas_meminit(sc);

	/* step 4. TX MAC registers & counters */
	cas_init_regs(sc);
	max_frame_size = ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN;
	v = (max_frame_size) | (0x2000 << 16) /* Burst size */;
	bus_space_write_4(t, h, CAS_MAC_MAC_MAX_FRAME, v);

	/* step 5. RX MAC registers & counters */
	cas_iff(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	KASSERT((CAS_CDTXADDR(sc, 0) & 0x1fff) == 0);
	bus_space_write_4(t, h, CAS_TX_RING_PTR_HI,
	    (((uint64_t)CAS_CDTXADDR(sc,0)) >> 32));
	bus_space_write_4(t, h, CAS_TX_RING_PTR_LO, CAS_CDTXADDR(sc, 0));

	KASSERT((CAS_CDRXADDR(sc, 0) & 0x1fff) == 0);
	bus_space_write_4(t, h, CAS_RX_DRING_PTR_HI,
	    (((uint64_t)CAS_CDRXADDR(sc,0)) >> 32));
	bus_space_write_4(t, h, CAS_RX_DRING_PTR_LO, CAS_CDRXADDR(sc, 0));

	KASSERT((CAS_CDRXCADDR(sc, 0) & 0x1fff) == 0);
	bus_space_write_4(t, h, CAS_RX_CRING_PTR_HI,
	    (((uint64_t)CAS_CDRXCADDR(sc,0)) >> 32));
	bus_space_write_4(t, h, CAS_RX_CRING_PTR_LO, CAS_CDRXCADDR(sc, 0));

	if (CAS_PLUS(sc)) {
		KASSERT((CAS_CDRXADDR2(sc, 0) & 0x1fff) == 0);
		bus_space_write_4(t, h, CAS_RX_DRING_PTR_HI2,
		    (((uint64_t)CAS_CDRXADDR2(sc,0)) >> 32));
		bus_space_write_4(t, h, CAS_RX_DRING_PTR_LO2,
		    CAS_CDRXADDR2(sc, 0));
	}

	/* step 8. Global Configuration & Interrupt Mask */
	cas_estintr(sc, CAS_INTR_REG);

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable DMA */
	v = cas_ringsize(CAS_NTXDESC /*XXX*/) << 10;
	bus_space_write_4(t, h, CAS_TX_CONFIG,
	    v|CAS_TX_CONFIG_TXDMA_EN|(1<<24)|(1<<29));
	bus_space_write_4(t, h, CAS_TX_KICK, 0);

	/* step 10. ERX Configuration */

	/* Encode Receive Descriptor ring size */
	v = cas_ringsize(CAS_NRXDESC) << CAS_RX_CONFIG_RXDRNG_SZ_SHIFT;
	if (CAS_PLUS(sc))
		v |= cas_ringsize(32) << CAS_RX_CONFIG_RXDRNG2_SZ_SHIFT;

	/* Encode Receive Completion ring size */
	v |= cas_cringsize(CAS_NRXCOMP) << CAS_RX_CONFIG_RXCRNG_SZ_SHIFT;

	/* Enable DMA */
	bus_space_write_4(t, h, CAS_RX_CONFIG,
	    v|(2<<CAS_RX_CONFIG_FBOFF_SHFT)|CAS_RX_CONFIG_RXDMA_EN);

	/*
	 * The following value is for an OFF Threshold of about 3/4 full
	 * and an ON Threshold of 1/4 full.
	 */
	bus_space_write_4(t, h, CAS_RX_PAUSE_THRESH,
	    (3 * sc->sc_rxfifosize / 256) |
	    ((sc->sc_rxfifosize / 256) << 12));
	bus_space_write_4(t, h, CAS_RX_BLANKING, (6 << 12) | 6);

	/* step 11. Configure Media */
	mii_ifmedia_change(&sc->sc_mii);

	/* step 12. RX_MAC Configuration Register */
	v = bus_space_read_4(t, h, CAS_MAC_RX_CONFIG);
	v |= CAS_MAC_RX_ENABLE | CAS_MAC_RX_STRIP_CRC;
	bus_space_write_4(t, h, CAS_MAC_RX_CONFIG, v);

	/* step 14. Issue Transmit Pending command */

	/* step 15.  Give the receiver a swift kick */
	bus_space_write_4(t, h, CAS_RX_KICK, CAS_NRXDESC-4);
	if (CAS_PLUS(sc))
		bus_space_write_4(t, h, CAS_RX_KICK2, 4);

	/* Start the one second timer. */
	callout_reset(&sc->sc_tick_ch, hz, cas_tick, sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	splx(s);

	return (0);
}

void
cas_init_regs(struct cas_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	const u_char *laddr = CLLADDR(ifp->if_sadl);
	u_int32_t v, r;

	/* These regs are not cleared on reset */
	sc->sc_inited = 0;
	if (!sc->sc_inited) {
		/* Load recommended values  */
		bus_space_write_4(t, h, CAS_MAC_IPG0, 0x00);
		bus_space_write_4(t, h, CAS_MAC_IPG1, 0x08);
		bus_space_write_4(t, h, CAS_MAC_IPG2, 0x04);

		bus_space_write_4(t, h, CAS_MAC_MAC_MIN_FRAME, ETHER_MIN_LEN);
		/* Max frame and max burst size */
		v = ETHER_MAX_LEN | (0x2000 << 16) /* Burst size */;
		bus_space_write_4(t, h, CAS_MAC_MAC_MAX_FRAME, v);

		bus_space_write_4(t, h, CAS_MAC_PREAMBLE_LEN, 0x07);
		bus_space_write_4(t, h, CAS_MAC_JAM_SIZE, 0x04);
		bus_space_write_4(t, h, CAS_MAC_ATTEMPT_LIMIT, 0x10);
		bus_space_write_4(t, h, CAS_MAC_CONTROL_TYPE, 0x8088);
		bus_space_write_4(t, h, CAS_MAC_RANDOM_SEED,
		    ((laddr[5]<<8)|laddr[4])&0x3ff);

		/* Secondary MAC addresses set to 0:0:0:0:0:0 */
		for (r = CAS_MAC_ADDR3; r < CAS_MAC_ADDR42; r += 4)
			bus_space_write_4(t, h, r, 0);

		/* MAC control addr set to 0:1:c2:0:1:80 */
		bus_space_write_4(t, h, CAS_MAC_ADDR42, 0x0001);
		bus_space_write_4(t, h, CAS_MAC_ADDR43, 0xc200);
		bus_space_write_4(t, h, CAS_MAC_ADDR44, 0x0180);

		/* MAC filter addr set to 0:0:0:0:0:0 */
		bus_space_write_4(t, h, CAS_MAC_ADDR_FILTER0, 0);
		bus_space_write_4(t, h, CAS_MAC_ADDR_FILTER1, 0);
		bus_space_write_4(t, h, CAS_MAC_ADDR_FILTER2, 0);

		bus_space_write_4(t, h, CAS_MAC_ADR_FLT_MASK1_2, 0);
		bus_space_write_4(t, h, CAS_MAC_ADR_FLT_MASK0, 0);

		/* Hash table initialized to 0 */
		for (r = CAS_MAC_HASH0; r <= CAS_MAC_HASH15; r += 4)
			bus_space_write_4(t, h, r, 0);

		sc->sc_inited = 1;
	}

	/* Counters need to be zeroed */
	bus_space_write_4(t, h, CAS_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_LATE_COLL_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_DEFER_TMR_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_PEAK_ATTEMPTS, 0);
	bus_space_write_4(t, h, CAS_MAC_RX_FRAME_COUNT, 0);
	bus_space_write_4(t, h, CAS_MAC_RX_LEN_ERR_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_RX_ALIGN_ERR, 0);
	bus_space_write_4(t, h, CAS_MAC_RX_CRC_ERR_CNT, 0);
	bus_space_write_4(t, h, CAS_MAC_RX_CODE_VIOL, 0);

	/* Un-pause stuff */
	bus_space_write_4(t, h, CAS_MAC_SEND_PAUSE_CMD, 0);

	/*
	 * Set the station address.
	 */
	bus_space_write_4(t, h, CAS_MAC_ADDR0, (laddr[4]<<8) | laddr[5]);
	bus_space_write_4(t, h, CAS_MAC_ADDR1, (laddr[2]<<8) | laddr[3]);
	bus_space_write_4(t, h, CAS_MAC_ADDR2, (laddr[0]<<8) | laddr[1]);
}

/*
 * Receive interrupt.
 */
int
cas_rint(struct cas_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	struct cas_rxsoft *rxs;
	struct mbuf *m;
	u_int64_t word[4];
	int len, off, idx;
	int i, skip;
	void *cp;

	for (i = sc->sc_rxptr;; i = CAS_NEXTRX(i + skip)) {
		CAS_CDRXCSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		word[0] = CAS_DMA_READ(sc->sc_rxcomps[i].cc_word[0]);
		word[1] = CAS_DMA_READ(sc->sc_rxcomps[i].cc_word[1]);
		word[2] = CAS_DMA_READ(sc->sc_rxcomps[i].cc_word[2]);
		word[3] = CAS_DMA_READ(sc->sc_rxcomps[i].cc_word[3]);

		/* Stop if the hardware still owns the descriptor. */
		if ((word[0] & CAS_RC0_TYPE) == 0 || word[3] & CAS_RC3_OWN)
			break;

		len = CAS_RC1_HDR_LEN(word[1]);
		if (len > 0) {
			off = CAS_RC1_HDR_OFF(word[1]);
			idx = CAS_RC1_HDR_IDX(word[1]);
			rxs = &sc->sc_rxsoft[idx];

			DPRINTF(sc, ("hdr at idx %d, off %d, len %d\n",
			    idx, off, len));

			bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

			cp = rxs->rxs_kva + off * 256 + ETHER_ALIGN;
			m = m_devget(cp, len, 0, ifp, NULL);
		
			if (word[0] & CAS_RC0_RELEASE_HDR)
				cas_add_rxbuf(sc, idx);

			if (m != NULL) {

				/*
				 * Pass this up to any BPF listeners, but only
				 * pass it up the stack if its for us.
				 */
				bpf_mtap(ifp, m);

				ifp->if_ipackets++;
				m->m_pkthdr.csum_flags = 0;
				(*ifp->if_input)(ifp, m);
			} else
				ifp->if_ierrors++;
		}

		len = CAS_RC0_DATA_LEN(word[0]);
		if (len > 0) {
			off = CAS_RC0_DATA_OFF(word[0]);
			idx = CAS_RC0_DATA_IDX(word[0]);
			rxs = &sc->sc_rxsoft[idx];

			DPRINTF(sc, ("data at idx %d, off %d, len %d\n",
			    idx, off, len));

			bus_dmamap_sync(sc->sc_dmatag, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

			/* XXX We should not be copying the packet here. */
			cp = rxs->rxs_kva + off + ETHER_ALIGN;
			m = m_devget(cp, len, 0, ifp, NULL);

			if (word[0] & CAS_RC0_RELEASE_DATA)
				cas_add_rxbuf(sc, idx);

			if (m != NULL) {
				/*
				 * Pass this up to any BPF listeners, but only
				 * pass it up the stack if its for us.
				 */
				bpf_mtap(ifp, m);

				ifp->if_ipackets++;
				m->m_pkthdr.csum_flags = 0;
				(*ifp->if_input)(ifp, m);
			} else
				ifp->if_ierrors++;
		}

		if (word[0] & CAS_RC0_SPLIT)
			aprint_error_dev(sc->sc_dev, "split packet\n");

		skip = CAS_RC0_SKIP(word[0]);
	}

	while (sc->sc_rxptr != i) {
		sc->sc_rxcomps[sc->sc_rxptr].cc_word[0] = 0;
		sc->sc_rxcomps[sc->sc_rxptr].cc_word[1] = 0;
		sc->sc_rxcomps[sc->sc_rxptr].cc_word[2] = 0;
		sc->sc_rxcomps[sc->sc_rxptr].cc_word[3] =
		    CAS_DMA_WRITE(CAS_RC3_OWN);
		CAS_CDRXCSYNC(sc, sc->sc_rxptr,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		sc->sc_rxptr = CAS_NEXTRX(sc->sc_rxptr);
	}

	bus_space_write_4(t, h, CAS_RX_COMP_TAIL, sc->sc_rxptr);

	DPRINTF(sc, ("cas_rint: done sc->rxptr %d, complete %d\n",
		sc->sc_rxptr, bus_space_read_4(t, h, CAS_RX_COMPLETION)));

	return (1);
}

/*
 * cas_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
cas_add_rxbuf(struct cas_softc *sc, int idx)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	CAS_INIT_RXDESC(sc, sc->sc_rxdptr, idx);

	if ((sc->sc_rxdptr % 4) == 0)
		bus_space_write_4(t, h, CAS_RX_KICK, sc->sc_rxdptr);

	if (++sc->sc_rxdptr == CAS_NRXDESC)
		sc->sc_rxdptr = 0;

	return (0);
}

int
cas_eint(struct cas_softc *sc, u_int status)
{
	char bits[128];
	if ((status & CAS_INTR_MIF) != 0) {
		DPRINTF(sc, ("%s: link status changed\n",
		    device_xname(sc->sc_dev)));
		return (1);
	}

	snprintb(bits, sizeof(bits), CAS_INTR_BITS, status);
	printf("%s: status=%s\n", device_xname(sc->sc_dev), bits);
	return (1);
}

int
cas_pint(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t seb = sc->sc_memh;
	u_int32_t status;

	status = bus_space_read_4(t, seb, CAS_MII_INTERRUP_STATUS);
	status |= bus_space_read_4(t, seb, CAS_MII_INTERRUP_STATUS);
#ifdef CAS_DEBUG
	if (status)
		printf("%s: link status changed\n", device_xname(sc->sc_dev));
#endif
	return (1);
}

int
cas_intr(void *v)
{
	struct cas_softc *sc = (struct cas_softc *)v;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t seb = sc->sc_memh;
	u_int32_t status;
	int r = 0;
#ifdef CAS_DEBUG
	char bits[128];
#endif

	sc->sc_ev_intr.ev_count++;

	status = bus_space_read_4(t, seb, CAS_STATUS);
#ifdef CAS_DEBUG
	snprintb(bits, sizeof(bits), CAS_INTR_BITS, status);
#endif
	DPRINTF(sc, ("%s: cas_intr: cplt %x status %s\n",
		device_xname(sc->sc_dev), (status>>19), bits));

	if ((status & CAS_INTR_PCS) != 0)
		r |= cas_pint(sc);

	if ((status & (CAS_INTR_TX_TAG_ERR | CAS_INTR_RX_TAG_ERR |
	    CAS_INTR_RX_COMP_FULL | CAS_INTR_BERR)) != 0)
		r |= cas_eint(sc, status);

	if ((status & (CAS_INTR_TX_EMPTY | CAS_INTR_TX_INTME)) != 0)
		r |= cas_tint(sc, status);

	if ((status & (CAS_INTR_RX_DONE | CAS_INTR_RX_NOBUF)) != 0)
		r |= cas_rint(sc);

	/* We should eventually do more than just print out error stats. */
	if (status & CAS_INTR_TX_MAC) {
		int txstat = bus_space_read_4(t, seb, CAS_MAC_TX_STATUS);
#ifdef CAS_DEBUG
		if (txstat & ~CAS_MAC_TX_XMIT_DONE)
			printf("%s: MAC tx fault, status %x\n",
			    device_xname(sc->sc_dev), txstat);
#endif
		if (txstat & (CAS_MAC_TX_UNDERRUN | CAS_MAC_TX_PKT_TOO_LONG))
			cas_init(ifp);
	}
	if (status & CAS_INTR_RX_MAC) {
		int rxstat = bus_space_read_4(t, seb, CAS_MAC_RX_STATUS);
#ifdef CAS_DEBUG
		if (rxstat & ~CAS_MAC_RX_DONE)
			printf("%s: MAC rx fault, status %x\n",
			    device_xname(sc->sc_dev), rxstat);
#endif
		/*
		 * On some chip revisions CAS_MAC_RX_OVERFLOW happen often
		 * due to a silicon bug so handle them silently.
		 */
		if (rxstat & CAS_MAC_RX_OVERFLOW) {
			ifp->if_ierrors++;
			cas_init(ifp);
		}
#ifdef CAS_DEBUG
		else if (rxstat & ~(CAS_MAC_RX_DONE | CAS_MAC_RX_FRAME_CNT))
			printf("%s: MAC rx fault, status %x\n",
			    device_xname(sc->sc_dev), rxstat);
#endif
	}
	rnd_add_uint32(&sc->rnd_source, status);
	return (r);
}


void
cas_watchdog(struct ifnet *ifp)
{
	struct cas_softc *sc = ifp->if_softc;

	DPRINTF(sc, ("cas_watchdog: CAS_RX_CONFIG %x CAS_MAC_RX_STATUS %x "
		"CAS_MAC_RX_CONFIG %x\n",
		bus_space_read_4(sc->sc_memt, sc->sc_memh, CAS_RX_CONFIG),
		bus_space_read_4(sc->sc_memt, sc->sc_memh, CAS_MAC_RX_STATUS),
		bus_space_read_4(sc->sc_memt, sc->sc_memh, CAS_MAC_RX_CONFIG)));

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;

	/* Try to get more packets going. */
	cas_init(ifp);
}

/*
 * Initialize the MII Management Interface
 */
void
cas_mifinit(struct cas_softc *sc)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t mif = sc->sc_memh;

	/* Configure the MIF in frame mode */
	sc->sc_mif_config = bus_space_read_4(t, mif, CAS_MIF_CONFIG);
	sc->sc_mif_config &= ~CAS_MIF_CONFIG_BB_ENA;
	bus_space_write_4(t, mif, CAS_MIF_CONFIG, sc->sc_mif_config);
}

/*
 * MII interface
 *
 * The Cassini MII interface supports at least three different operating modes:
 *
 * Bitbang mode is implemented using data, clock and output enable registers.
 *
 * Frame mode is implemented by loading a complete frame into the frame
 * register and polling the valid bit for completion.
 *
 * Polling mode uses the frame register but completion is indicated by
 * an interrupt.
 *
 */
int
cas_mii_readreg(device_t self, int phy, int reg)
{
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t mif = sc->sc_memh;
	int n;
	u_int32_t v;

#ifdef CAS_DEBUG
	if (sc->sc_debug)
		printf("cas_mii_readreg: phy %d reg %d\n", phy, reg);
#endif

	/* Construct the frame command */
	v = (reg << CAS_MIF_REG_SHIFT)	| (phy << CAS_MIF_PHY_SHIFT) |
		CAS_MIF_FRAME_READ;

	bus_space_write_4(t, mif, CAS_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, CAS_MIF_FRAME);
		if (v & CAS_MIF_FRAME_TA0)
			return (v & CAS_MIF_FRAME_DATA);
	}

	printf("%s: mii_read timeout\n", device_xname(sc->sc_dev));
	return (0);
}

void
cas_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t mif = sc->sc_memh;
	int n;
	u_int32_t v;

#ifdef CAS_DEBUG
	if (sc->sc_debug)
		printf("cas_mii_writereg: phy %d reg %d val %x\n",
			phy, reg, val);
#endif

	/* Construct the frame command */
	v = CAS_MIF_FRAME_WRITE			|
	    (phy << CAS_MIF_PHY_SHIFT)		|
	    (reg << CAS_MIF_REG_SHIFT)		|
	    (val & CAS_MIF_FRAME_DATA);

	bus_space_write_4(t, mif, CAS_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, CAS_MIF_FRAME);
		if (v & CAS_MIF_FRAME_TA0)
			return;
	}

	printf("%s: mii_write timeout\n", device_xname(sc->sc_dev));
}

void
cas_mii_statchg(struct ifnet *ifp)
{
	struct cas_softc *sc = ifp->if_softc;
#ifdef CAS_DEBUG
	int instance = IFM_INST(sc->sc_media.ifm_cur->ifm_media);
#endif
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t mac = sc->sc_memh;
	u_int32_t v;

#ifdef CAS_DEBUG
	if (sc->sc_debug)
		printf("cas_mii_statchg: status change: phy = %d\n",
		    sc->sc_phys[instance]);
#endif

	/* Set tx full duplex options */
	bus_space_write_4(t, mac, CAS_MAC_TX_CONFIG, 0);
	delay(10000); /* reg must be cleared and delay before changing. */
	v = CAS_MAC_TX_ENA_IPG0|CAS_MAC_TX_NGU|CAS_MAC_TX_NGU_LIMIT|
		CAS_MAC_TX_ENABLE;
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0) {
		v |= CAS_MAC_TX_IGN_CARRIER|CAS_MAC_TX_IGN_COLLIS;
	}
	bus_space_write_4(t, mac, CAS_MAC_TX_CONFIG, v);

	/* XIF Configuration */
	v = CAS_MAC_XIF_TX_MII_ENA;
	v |= CAS_MAC_XIF_LINK_LED;

	/* MII needs echo disable if half duplex. */
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0)
		/* turn on full duplex LED */
		v |= CAS_MAC_XIF_FDPLX_LED;
	else
		/* half duplex -- disable echo */
		v |= CAS_MAC_XIF_ECHO_DISABL;

	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_1000_T:  /* Gigabit using GMII interface */
	case IFM_1000_SX:
		v |= CAS_MAC_XIF_GMII_MODE;
		break;
	default:
		v &= ~CAS_MAC_XIF_GMII_MODE;
	}
	bus_space_write_4(t, mac, CAS_MAC_XIF_CONFIG, v);
}

int
cas_pcs_readreg(device_t self, int phy, int reg)
{
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t pcs = sc->sc_memh;

#ifdef CAS_DEBUG
	if (sc->sc_debug)
		printf("cas_pcs_readreg: phy %d reg %d\n", phy, reg);
#endif

	if (phy != CAS_PHYAD_EXTERNAL)
		return (0);

	switch (reg) {
	case MII_BMCR:
		reg = CAS_MII_CONTROL;
		break;
	case MII_BMSR:
		reg = CAS_MII_STATUS;
		break;
	case MII_ANAR:
		reg = CAS_MII_ANAR;
		break;
	case MII_ANLPAR:
		reg = CAS_MII_ANLPAR;
		break;
	case MII_EXTSR:
		return (EXTSR_1000XFDX|EXTSR_1000XHDX);
	default:
		return (0);
	}

	return bus_space_read_4(t, pcs, reg);
}

void
cas_pcs_writereg(device_t self, int phy, int reg, int val)
{
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t pcs = sc->sc_memh;
	int reset = 0;

#ifdef CAS_DEBUG
	if (sc->sc_debug)
		printf("cas_pcs_writereg: phy %d reg %d val %x\n",
			phy, reg, val);
#endif

	if (phy != CAS_PHYAD_EXTERNAL)
		return;

	if (reg == MII_ANAR)
		bus_space_write_4(t, pcs, CAS_MII_CONFIG, 0);

	switch (reg) {
	case MII_BMCR:
		reset = (val & CAS_MII_CONTROL_RESET);
		reg = CAS_MII_CONTROL;
		break;
	case MII_BMSR:
		reg = CAS_MII_STATUS;
		break;
	case MII_ANAR:
		reg = CAS_MII_ANAR;
		break;
	case MII_ANLPAR:
		reg = CAS_MII_ANLPAR;
		break;
	default:
		return;
	}

	bus_space_write_4(t, pcs, reg, val);

	if (reset)
		cas_bitwait(sc, pcs, CAS_MII_CONTROL, CAS_MII_CONTROL_RESET, 0);

	if (reg == CAS_MII_ANAR || reset)
		bus_space_write_4(t, pcs, CAS_MII_CONFIG,
		    CAS_MII_CONFIG_ENABLE);
}

int
cas_mediachange(struct ifnet *ifp)
{
	struct cas_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	if (mii->mii_instance) {
		struct mii_softc *miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}

	return (mii_mediachg(&sc->sc_mii));
}

void
cas_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cas_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

/*
 * Process an ioctl request.
 */
int
cas_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct cas_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		error = 0;
		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			cas_iff(sc);
		}
	}

	splx(s);
	return (error);
}

static bool
cas_suspend(device_t self, const pmf_qual_t *qual)
{
	struct cas_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;

	bus_space_write_4(t, h, CAS_INTMASK, ~(uint32_t)0);
	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	return true;
}

static bool
cas_resume(device_t self, const pmf_qual_t *qual)
{
	struct cas_softc *sc = device_private(self);

	return cas_estintr(sc, CAS_INTR_PCI | CAS_INTR_REG);
}

static bool
cas_estintr(struct cas_softc *sc, int what)
{
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	const char *intrstr = NULL;
	char intrbuf[PCI_INTRSTR_LEN];

	/* PCI interrupts */
	if (what & CAS_INTR_PCI) {
		intrstr = pci_intr_string(sc->sc_pc, sc->sc_handle, intrbuf, sizeof(intrbuf));
		sc->sc_ih = pci_intr_establish(sc->sc_pc, sc->sc_handle,
		    IPL_NET, cas_intr, sc);
		if (sc->sc_ih == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "unable to establish interrupt");
			if (intrstr != NULL)
				aprint_error(" at %s", intrstr);
			aprint_error("\n");
			return false;
		}

		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
	}

	/* Interrupt register */
	if (what & CAS_INTR_REG) {
		bus_space_write_4(t, h, CAS_INTMASK,
		    ~(CAS_INTR_TX_INTME|CAS_INTR_TX_EMPTY|
		    CAS_INTR_TX_TAG_ERR|
		    CAS_INTR_RX_DONE|CAS_INTR_RX_NOBUF|
		    CAS_INTR_RX_TAG_ERR|
		    CAS_INTR_RX_COMP_FULL|CAS_INTR_PCS|
		    CAS_INTR_MAC_CONTROL|CAS_INTR_MIF|
		    CAS_INTR_BERR));
		bus_space_write_4(t, h, CAS_MAC_RX_MASK,
		    CAS_MAC_RX_DONE|CAS_MAC_RX_FRAME_CNT);
		bus_space_write_4(t, h, CAS_MAC_TX_MASK, CAS_MAC_TX_XMIT_DONE);
		bus_space_write_4(t, h, CAS_MAC_CONTROL_MASK, 0); /* XXXX */
	}
	return true;
}

bool
cas_shutdown(device_t self, int howto)
{
	struct cas_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	cas_stop(ifp, 1);

	return true;
}

void
cas_iff(struct cas_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ethercom *ec = &sc->sc_ethercom;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_space_tag_t t = sc->sc_memt;
	bus_space_handle_t h = sc->sc_memh;
	u_int32_t crc, hash[16], rxcfg;
	int i;

	rxcfg = bus_space_read_4(t, h, CAS_MAC_RX_CONFIG);
	rxcfg &= ~(CAS_MAC_RX_HASH_FILTER | CAS_MAC_RX_PROMISCUOUS |
	    CAS_MAC_RX_PROMISC_GRP);
	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC || ec->ec_multicnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			rxcfg |= CAS_MAC_RX_PROMISCUOUS;
		else
			rxcfg |= CAS_MAC_RX_PROMISC_GRP;
        } else {
		/*
		 * Set up multicast address filter by passing all multicast
		 * addresses through a crc generator, and then using the
		 * high order 8 bits as an index into the 256 bit logical
		 * address filter.  The high order 4 bits selects the word,
		 * while the other 4 bits select the bit within the word
		 * (where bit 0 is the MSB).
		 */

		rxcfg |= CAS_MAC_RX_HASH_FILTER;

		/* Clear hash table */
		for (i = 0; i < 16; i++)
			hash[i] = 0;

		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
                        crc = ether_crc32_le(enm->enm_addrlo,
                            ETHER_ADDR_LEN);

                        /* Just want the 8 most significant bits. */
                        crc >>= 24;

                        /* Set the corresponding bit in the filter. */
                        hash[crc >> 4] |= 1 << (15 - (crc & 15));

			ETHER_NEXT_MULTI(step, enm);
		}

		/* Now load the hash table into the chip (if we are using it) */
		for (i = 0; i < 16; i++) {
			bus_space_write_4(t, h,
			    CAS_MAC_HASH0 + i * (CAS_MAC_HASH1 - CAS_MAC_HASH0),
			    hash[i]);
		}
	}

	bus_space_write_4(t, h, CAS_MAC_RX_CONFIG, rxcfg);
}

int
cas_encap(struct cas_softc *sc, struct mbuf *mhead, u_int32_t *bixp)
{
	u_int64_t flags;
	u_int32_t cur, frag, i;
	bus_dmamap_t map;

	cur = frag = *bixp;
	map = sc->sc_txd[cur].sd_map;

	if (bus_dmamap_load_mbuf(sc->sc_dmatag, map, mhead,
	    BUS_DMA_NOWAIT) != 0) {
		return (ENOBUFS);
	}

	if ((sc->sc_tx_cnt + map->dm_nsegs) > (CAS_NTXDESC - 2)) {
		bus_dmamap_unload(sc->sc_dmatag, map);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->sc_dmatag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < map->dm_nsegs; i++) {
		sc->sc_txdescs[frag].cd_addr =
		    CAS_DMA_WRITE(map->dm_segs[i].ds_addr);
		flags = (map->dm_segs[i].ds_len & CAS_TD_BUFSIZE) |
		    (i == 0 ? CAS_TD_START_OF_PACKET : 0) |
		    ((i == (map->dm_nsegs - 1)) ? CAS_TD_END_OF_PACKET : 0);
		sc->sc_txdescs[frag].cd_flags = CAS_DMA_WRITE(flags);
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_cddmamap,
		    CAS_CDTXOFF(frag), sizeof(struct cas_desc),
		    BUS_DMASYNC_PREWRITE);
		cur = frag;
		if (++frag == CAS_NTXDESC)
			frag = 0;
	}

	sc->sc_tx_cnt += map->dm_nsegs;
	sc->sc_txd[*bixp].sd_map = sc->sc_txd[cur].sd_map;
	sc->sc_txd[cur].sd_map = map;
	sc->sc_txd[cur].sd_mbuf = mhead;

	bus_space_write_4(sc->sc_memt, sc->sc_memh, CAS_TX_KICK, frag);

	*bixp = frag;

	/* sync descriptors */

	return (0);
}

/*
 * Transmit interrupt.
 */
int
cas_tint(struct cas_softc *sc, u_int32_t status)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct cas_sxd *sd;
	u_int32_t cons, comp;

	comp = bus_space_read_4(sc->sc_memt, sc->sc_memh, CAS_TX_COMPLETION);
	cons = sc->sc_tx_cons;
	while (cons != comp) {
		sd = &sc->sc_txd[cons];
		if (sd->sd_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmatag, sd->sd_map, 0,
			    sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmatag, sd->sd_map);
			m_freem(sd->sd_mbuf);
			sd->sd_mbuf = NULL;
			ifp->if_opackets++;
		}
		sc->sc_tx_cnt--;
		if (++cons == CAS_NTXDESC)
			cons = 0;
	}
	sc->sc_tx_cons = cons;

	if (sc->sc_tx_cnt < CAS_NTXDESC - 2)
		ifp->if_flags &= ~IFF_OACTIVE;
	if (sc->sc_tx_cnt == 0)
		ifp->if_timer = 0;

	cas_start(ifp);

	return (1);
}

void
cas_start(struct ifnet *ifp)
{
	struct cas_softc *sc = ifp->if_softc;
	struct mbuf *m;
	u_int32_t bix;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_tx_prod;
	while (sc->sc_txd[bix].sd_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		bpf_mtap(ifp, m);

		/*
		 * Encapsulate this packet and start it going...
		 * or fail...
		 */
		if (cas_encap(sc, m, &bix)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);
		ifp->if_timer = 5;
	}

	sc->sc_tx_prod = bix;
}

MODULE(MODULE_CLASS_DRIVER, if_cas, "pci");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
if_cas_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_cas,
		    cfattach_ioconf_cas, cfdata_ioconf_cas);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_cas,
		    cfattach_ioconf_cas, cfdata_ioconf_cas);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
