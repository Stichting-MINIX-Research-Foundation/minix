/*	$NetBSD: hme.c,v 1.91 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * HME Ethernet module driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hme.c,v 1.91 2015/04/13 16:33:24 riastradh Exp $");

/* #define HMEDEBUG */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <net/if_vlanvar.h>
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <sys/bus.h>

#include <dev/ic/hmereg.h>
#include <dev/ic/hmevar.h>

static void	hme_start(struct ifnet *);
static void	hme_stop(struct ifnet *, int);
static int	hme_ioctl(struct ifnet *, u_long, void *);
static void	hme_tick(void *);
static void	hme_watchdog(struct ifnet *);
static bool	hme_shutdown(device_t, int);
static int	hme_init(struct ifnet *);
static void	hme_meminit(struct hme_softc *);
static void	hme_mifinit(struct hme_softc *);
static void	hme_reset(struct hme_softc *);  
static void	hme_chipreset(struct hme_softc *);
static void	hme_setladrf(struct hme_softc *);

/* MII methods & callbacks */
static int	hme_mii_readreg(device_t, int, int);
static void	hme_mii_writereg(device_t, int, int, int);
static void	hme_mii_statchg(struct ifnet *);

static int	hme_mediachange(struct ifnet *);

static struct mbuf *hme_get(struct hme_softc *, int, uint32_t);
static int	hme_put(struct hme_softc *, int, struct mbuf *);
static void	hme_read(struct hme_softc *, int, uint32_t);
static int	hme_eint(struct hme_softc *, u_int);
static int	hme_rint(struct hme_softc *);
static int	hme_tint(struct hme_softc *);

#if 0
/* Default buffer copy routines */
static void	hme_copytobuf_contig(struct hme_softc *, void *, int, int);
static void	hme_copyfrombuf_contig(struct hme_softc *, void *, int, int);
#endif

void
hme_config(struct hme_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;
	struct mii_softc *child;
	bus_dma_tag_t dmatag = sc->sc_dmatag;
	bus_dma_segment_t seg;
	bus_size_t size;
	int rseg, error;

	/*
	 * HME common initialization.
	 *
	 * hme_softc fields that must be initialized by the front-end:
	 *
	 * the bus tag:
	 *	sc_bustag
	 *
	 * the DMA bus tag:
	 *	sc_dmatag
	 *
	 * the bus handles:
	 *	sc_seb		(Shared Ethernet Block registers)
	 *	sc_erx		(Receiver Unit registers)
	 *	sc_etx		(Transmitter Unit registers)
	 *	sc_mac		(MAC registers)
	 *	sc_mif		(Management Interface registers)
	 *
	 * the maximum bus burst size:
	 *	sc_burst
	 *
	 * (notyet:DMA capable memory for the ring descriptors & packet buffers:
	 *	rb_membase, rb_dmabase)
	 *
	 * the local Ethernet address:
	 *	sc_enaddr
	 *
	 */

	/* Make sure the chip is stopped. */
	hme_chipreset(sc);

	/*
	 * Allocate descriptors and buffers
	 * XXX - do all this differently.. and more configurably,
	 * eg. use things as `dma_load_mbuf()' on transmit,
	 *     and a pool of `EXTMEM' mbufs (with buffers DMA-mapped
	 *     all the time) on the receiver side.
	 *
	 * Note: receive buffers must be 64-byte aligned.
	 * Also, apparently, the buffers must extend to a DMA burst
	 * boundary beyond the maximum packet size.
	 */
#define _HME_NDESC	128
#define _HME_BUFSZ	1600

	/* Note: the # of descriptors must be a multiple of 16 */
	sc->sc_rb.rb_ntbuf = _HME_NDESC;
	sc->sc_rb.rb_nrbuf = _HME_NDESC;

	/*
	 * Allocate DMA capable memory
	 * Buffer descriptors must be aligned on a 2048 byte boundary;
	 * take this into account when calculating the size. Note that
	 * the maximum number of descriptors (256) occupies 2048 bytes,
	 * so we allocate that much regardless of _HME_NDESC.
	 */
	size =	2048 +					/* TX descriptors */
		2048 +					/* RX descriptors */
		sc->sc_rb.rb_ntbuf * _HME_BUFSZ +	/* TX buffers */
		sc->sc_rb.rb_nrbuf * _HME_BUFSZ;	/* RX buffers */

	/* Allocate DMA buffer */
	if ((error = bus_dmamem_alloc(dmatag, size,
				      2048, 0,
				      &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "DMA buffer alloc error %d\n",
			error);
		return;
	}

	/* Map DMA memory in CPU addressable space */
	if ((error = bus_dmamem_map(dmatag, &seg, rseg, size,
				    &sc->sc_rb.rb_membase,
				    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "DMA buffer map error %d\n",
			error);
		bus_dmamap_unload(dmatag, sc->sc_dmamap);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}

	if ((error = bus_dmamap_create(dmatag, size, 1, size, 0,
				    BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "DMA map create error %d\n",
			error);
		return;
	}

	/* Load the buffer */
	if ((error = bus_dmamap_load(dmatag, sc->sc_dmamap,
	    sc->sc_rb.rb_membase, size, NULL,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "DMA buffer map load error %d\n",
			error);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}
	sc->sc_rb.rb_dmabase = sc->sc_dmamap->dm_segs[0].ds_addr;

	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = hme_start;
	ifp->if_stop = hme_stop;
	ifp->if_ioctl = hme_ioctl;
	ifp->if_init = hme_init;
	ifp->if_watchdog = hme_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_capabilities |=
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize ifmedia structures and MII info */
	mii->mii_ifp = ifp;
	mii->mii_readreg = hme_mii_readreg;
	mii->mii_writereg = hme_mii_writereg;
	mii->mii_statchg = hme_mii_statchg;

	sc->sc_ethercom.ec_mii = mii;
	ifmedia_init(&mii->mii_media, 0, hme_mediachange, ether_mediastatus);

	hme_mifinit(sc);

	mii_attach(sc->sc_dev, mii, 0xffffffff,
			MII_PHY_ANY, MII_OFFSET_ANY, MIIF_FORCEANEG);

	child = LIST_FIRST(&mii->mii_phys);
	if (child == NULL) {
		/* No PHY attached */
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
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
		 * Set the default media to auto negotiation if the phy has
		 * the auto negotiation capability.
		 * XXX; What to do otherwise?
		 */
		if (ifmedia_match(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO, 0))
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
/*
		else
			ifmedia_set(&sc->sc_mii.mii_media, sc->sc_defaultmedia);
*/
	}

	/* claim 802.1q capability */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	if (pmf_device_register1(sc->sc_dev, NULL, NULL, hme_shutdown))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	callout_init(&sc->sc_tick_ch, 0);
}

void
hme_tick(void *arg)
{
	struct hme_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, hme_tick, sc);
}

void
hme_reset(struct hme_softc *sc)
{
	int s;

	s = splnet();
	(void)hme_init(&sc->sc_ethercom.ec_if);
	splx(s);
}

void
hme_chipreset(struct hme_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	int n;

	/* Mask all interrupts */
	bus_space_write_4(t, seb, HME_SEBI_IMASK, 0xffffffff);

	/* Reset transmitter and receiver */
	bus_space_write_4(t, seb, HME_SEBI_RESET,
			  (HME_SEB_RESET_ETX | HME_SEB_RESET_ERX));

	for (n = 0; n < 20; n++) {
		uint32_t v = bus_space_read_4(t, seb, HME_SEBI_RESET);
		if ((v & (HME_SEB_RESET_ETX | HME_SEB_RESET_ERX)) == 0)
			return;
		DELAY(20);
	}

	printf("%s: %s: reset failed\n", device_xname(sc->sc_dev), __func__);
}

void
hme_stop(struct ifnet *ifp, int disable)
{
	struct hme_softc *sc;

	sc = ifp->if_softc;

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->sc_tick_ch);
	mii_down(&sc->sc_mii);

	hme_chipreset(sc);
}

void
hme_meminit(struct hme_softc *sc)
{
	bus_addr_t txbufdma, rxbufdma;
	bus_addr_t dma;
	char *p;
	unsigned int ntbuf, nrbuf, i;
	struct hme_ring *hr = &sc->sc_rb;

	p = hr->rb_membase;
	dma = hr->rb_dmabase;

	ntbuf = hr->rb_ntbuf;
	nrbuf = hr->rb_nrbuf;

	/*
	 * Allocate transmit descriptors
	 */
	hr->rb_txd = p;
	hr->rb_txddma = dma;
	p += ntbuf * HME_XD_SIZE;
	dma += ntbuf * HME_XD_SIZE;
	/* We have reserved descriptor space until the next 2048 byte boundary.*/
	dma = (bus_addr_t)roundup((u_long)dma, 2048);
	p = (void *)roundup((u_long)p, 2048);

	/*
	 * Allocate receive descriptors
	 */
	hr->rb_rxd = p;
	hr->rb_rxddma = dma;
	p += nrbuf * HME_XD_SIZE;
	dma += nrbuf * HME_XD_SIZE;
	/* Again move forward to the next 2048 byte boundary.*/
	dma = (bus_addr_t)roundup((u_long)dma, 2048);
	p = (void *)roundup((u_long)p, 2048);


	/*
	 * Allocate transmit buffers
	 */
	hr->rb_txbuf = p;
	txbufdma = dma;
	p += ntbuf * _HME_BUFSZ;
	dma += ntbuf * _HME_BUFSZ;

	/*
	 * Allocate receive buffers
	 */
	hr->rb_rxbuf = p;
	rxbufdma = dma;
	p += nrbuf * _HME_BUFSZ;
	dma += nrbuf * _HME_BUFSZ;

	/*
	 * Initialize transmit buffer descriptors
	 */
	for (i = 0; i < ntbuf; i++) {
		HME_XD_SETADDR(sc->sc_pci, hr->rb_txd, i, txbufdma + i * _HME_BUFSZ);
		HME_XD_SETFLAGS(sc->sc_pci, hr->rb_txd, i, 0);
	}

	/*
	 * Initialize receive buffer descriptors
	 */
	for (i = 0; i < nrbuf; i++) {
		HME_XD_SETADDR(sc->sc_pci, hr->rb_rxd, i, rxbufdma + i * _HME_BUFSZ);
		HME_XD_SETFLAGS(sc->sc_pci, hr->rb_rxd, i,
				HME_XD_OWN | HME_XD_ENCODE_RSIZE(_HME_BUFSZ));
	}

	hr->rb_tdhead = hr->rb_tdtail = 0;
	hr->rb_td_nbusy = 0;
	hr->rb_rdtail = 0;
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
int
hme_init(struct ifnet *ifp)
{
	struct hme_softc *sc = ifp->if_softc;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	bus_space_handle_t etx = sc->sc_etx;
	bus_space_handle_t erx = sc->sc_erx;
	bus_space_handle_t mac = sc->sc_mac;
	uint8_t *ea;
	uint32_t v;
	int rc;

	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	hme_stop(ifp, 0);

	/* Re-initialize the MIF */
	hme_mifinit(sc);

	/* Call MI reset function if any */
	if (sc->sc_hwreset)
		(*sc->sc_hwreset)(sc);

#if 0
	/* Mask all MIF interrupts, just in case */
	bus_space_write_4(t, mif, HME_MIFI_IMASK, 0xffff);
#endif

	/* step 3. Setup data structures in host memory */
	hme_meminit(sc);

	/* step 4. TX MAC registers & counters */
	bus_space_write_4(t, mac, HME_MACI_NCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_FCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_EXCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_LTCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_TXSIZE,
	    (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) ?
	    ETHER_VLAN_ENCAP_LEN + ETHER_MAX_LEN : ETHER_MAX_LEN);
	sc->sc_ec_capenable = sc->sc_ethercom.ec_capenable;

	/* Load station MAC address */
	ea = sc->sc_enaddr;
	bus_space_write_4(t, mac, HME_MACI_MACADDR0, (ea[0] << 8) | ea[1]);
	bus_space_write_4(t, mac, HME_MACI_MACADDR1, (ea[2] << 8) | ea[3]);
	bus_space_write_4(t, mac, HME_MACI_MACADDR2, (ea[4] << 8) | ea[5]);

	/*
	 * Init seed for backoff
	 * (source suggested by manual: low 10 bits of MAC address)
	 */
	v = ((ea[4] << 8) | ea[5]) & 0x3fff;
	bus_space_write_4(t, mac, HME_MACI_RANDSEED, v);


	/* Note: Accepting power-on default for other MAC registers here.. */


	/* step 5. RX MAC registers & counters */
	hme_setladrf(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	bus_space_write_4(t, etx, HME_ETXI_RING, sc->sc_rb.rb_txddma);
	bus_space_write_4(t, etx, HME_ETXI_RSIZE, sc->sc_rb.rb_ntbuf);

	bus_space_write_4(t, erx, HME_ERXI_RING, sc->sc_rb.rb_rxddma);
	bus_space_write_4(t, mac, HME_MACI_RXSIZE,
	    (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) ?
	    ETHER_VLAN_ENCAP_LEN + ETHER_MAX_LEN : ETHER_MAX_LEN);

	/* step 8. Global Configuration & Interrupt Mask */
	bus_space_write_4(t, seb, HME_SEBI_IMASK,
			~(
			  /*HME_SEB_STAT_GOTFRAME | HME_SEB_STAT_SENTFRAME |*/
			  HME_SEB_STAT_HOSTTOTX |
			  HME_SEB_STAT_RXTOHOST |
			  HME_SEB_STAT_TXALL |
			  HME_SEB_STAT_TXPERR |
			  HME_SEB_STAT_RCNTEXP |
			  HME_SEB_STAT_MIFIRQ |
			  HME_SEB_STAT_ALL_ERRORS ));

	switch (sc->sc_burst) {
	default:
		v = 0;
		break;
	case 16:
		v = HME_SEB_CFG_BURST16;
		break;
	case 32:
		v = HME_SEB_CFG_BURST32;
		break;
	case 64:
		v = HME_SEB_CFG_BURST64;
		break;
	}
	bus_space_write_4(t, seb, HME_SEBI_CFG, v);

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable DMA */
	v = bus_space_read_4(t, etx, HME_ETXI_CFG);
	v |= HME_ETX_CFG_DMAENABLE;
	bus_space_write_4(t, etx, HME_ETXI_CFG, v);

	/* Transmit Descriptor ring size: in increments of 16 */
	bus_space_write_4(t, etx, HME_ETXI_RSIZE, _HME_NDESC / 16 - 1);


	/* step 10. ERX Configuration */
	v = bus_space_read_4(t, erx, HME_ERXI_CFG);

	/* Encode Receive Descriptor ring size: four possible values */
	switch (_HME_NDESC /*XXX*/) {
	case 32:
		v |= HME_ERX_CFG_RINGSIZE32;
		break;
	case 64:
		v |= HME_ERX_CFG_RINGSIZE64;
		break;
	case 128:
		v |= HME_ERX_CFG_RINGSIZE128;
		break;
	case 256:
		v |= HME_ERX_CFG_RINGSIZE256;
		break;
	default:
		printf("hme: invalid Receive Descriptor ring size\n");
		break;
	}

	/* Enable DMA */
	v |= HME_ERX_CFG_DMAENABLE;

	/* set h/w rx checksum start offset (# of half-words) */
#ifdef INET
	v |= (((ETHER_HDR_LEN + sizeof(struct ip)) / sizeof(uint16_t))
		<< HME_ERX_CFG_CSUMSHIFT) &
		HME_ERX_CFG_CSUMSTART;
#endif
	bus_space_write_4(t, erx, HME_ERXI_CFG, v);

	/* step 11. XIF Configuration */
	v = bus_space_read_4(t, mac, HME_MACI_XIF);
	v |= HME_MAC_XIF_OE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);

	/* step 12. RX_MAC Configuration Register */
	v = bus_space_read_4(t, mac, HME_MACI_RXCFG);
	v |= HME_MAC_RXCFG_ENABLE | HME_MAC_RXCFG_PSTRIP;
	bus_space_write_4(t, mac, HME_MACI_RXCFG, v);

	/* step 13. TX_MAC Configuration Register */
	v = bus_space_read_4(t, mac, HME_MACI_TXCFG);
	v |= (HME_MAC_TXCFG_ENABLE | HME_MAC_TXCFG_DGIVEUP);
	bus_space_write_4(t, mac, HME_MACI_TXCFG, v);

	/* step 14. Issue Transmit Pending command */

	/* Call MI initialization function if any */
	if (sc->sc_hwinit)
		(*sc->sc_hwinit)(sc);

	/* Set the current media. */
	if ((rc = hme_mediachange(ifp)) != 0)
		return rc;

	/* Start the one second timer. */
	callout_reset(&sc->sc_tick_ch, hz, hme_tick, sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_timer = 0;
	hme_start(ifp);
	return 0;
}

/*
 * Routine to copy from mbuf chain to transmit buffer in
 * network buffer memory.
 * Returns the amount of data copied.
 */
int
hme_put(struct hme_softc *sc, int ri, struct mbuf *m)
	/* ri:			 Ring index */
{
	struct mbuf *n;
	int len, tlen = 0;
	char *bp;

	bp = (char *)sc->sc_rb.rb_txbuf + (ri % sc->sc_rb.rb_ntbuf) * _HME_BUFSZ;
	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		memcpy(bp, mtod(m, void *), len);
		bp += len;
		tlen += len;
		MFREE(m, n);
	}
	return (tlen);
}

/*
 * Pull data off an interface.
 * Len is length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
hme_get(struct hme_softc *sc, int ri, uint32_t flags)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m, *m0, *newm;
	char *bp;
	int len, totlen;
#ifdef INET
	int csum_flags;
#endif

	totlen = HME_XD_DECODE_RSIZE(flags);
	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == 0)
		return (0);
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	len = MHLEN;
	m = m0;

	bp = (char *)sc->sc_rb.rb_rxbuf + (ri % sc->sc_rb.rb_nrbuf) * _HME_BUFSZ;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0)
				goto bad;
			len = MCLBYTES;
		}

		if (m == m0) {
			char *newdata = (char *)
			    ALIGN(m->m_data + sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}

		m->m_len = len = min(totlen, len);
		memcpy(mtod(m, void *), bp, len);
		bp += len;

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == 0)
				goto bad;
			len = MLEN;
			m = m->m_next = newm;
		}
	}

#ifdef INET
	/* hardware checksum */
	csum_flags = 0;
	if (ifp->if_csum_flags_rx & (M_CSUM_TCPv4 | M_CSUM_UDPv4)) {
		struct ether_header *eh;
		struct ether_vlan_header *evh;
		struct ip *ip;
		struct udphdr *uh;
		uint16_t *opts;
		int32_t hlen, pktlen;
		uint32_t csum_data;

		eh = mtod(m0, struct ether_header *);
		if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
			ip = (struct ip *)((char *)eh + ETHER_HDR_LEN);
			pktlen = m0->m_pkthdr.len - ETHER_HDR_LEN;
		} else if (ntohs(eh->ether_type) == ETHERTYPE_VLAN) {
			evh = (struct ether_vlan_header *)eh;
			if (ntohs(evh->evl_proto != ETHERTYPE_IP))
				goto swcsum;
			ip = (struct ip *)((char *)eh + ETHER_HDR_LEN +
			    ETHER_VLAN_ENCAP_LEN);
			pktlen = m0->m_pkthdr.len -
			    ETHER_HDR_LEN - ETHER_VLAN_ENCAP_LEN;
		} else
			goto swcsum;

		/* IPv4 only */
		if (ip->ip_v != IPVERSION)
			goto swcsum;

		hlen = ip->ip_hl << 2;
		if (hlen < sizeof(struct ip))
			goto swcsum;

		/*
		 * bail if too short, has random trailing garbage, truncated,
		 * fragment, or has ethernet pad.
		 */
		if (ntohs(ip->ip_len) < hlen ||
		    ntohs(ip->ip_len) != pktlen ||
		    (ntohs(ip->ip_off) & (IP_MF | IP_OFFMASK)) != 0)
			goto swcsum;

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			if ((ifp->if_csum_flags_rx & M_CSUM_TCPv4) == 0)
				goto swcsum;
			if (pktlen < (hlen + sizeof(struct tcphdr)))
				goto swcsum;
			csum_flags =
			    M_CSUM_TCPv4 | M_CSUM_DATA | M_CSUM_NO_PSEUDOHDR;
			break;
		case IPPROTO_UDP:
			if ((ifp->if_csum_flags_rx & M_CSUM_UDPv4) == 0)
				goto swcsum;
			if (pktlen < (hlen + sizeof(struct udphdr)))
				goto swcsum;
			uh = (struct udphdr *)((char *)ip + hlen);
			/* no checksum */
			if (uh->uh_sum == 0)
				goto swcsum;
			csum_flags =
			    M_CSUM_UDPv4 | M_CSUM_DATA | M_CSUM_NO_PSEUDOHDR;
			break;
		default:
			goto swcsum;
		}

		/* w/ M_CSUM_NO_PSEUDOHDR, the uncomplemented sum is expected */
		csum_data = ~flags & HME_XD_RXCKSUM;

		/*
		 * If data offset is different from RX cksum start offset,
		 * we have to deduct them.
		 */
		hlen = ((char *)ip + hlen) -
		    ((char *)eh + ETHER_HDR_LEN + sizeof(struct ip));
		if (hlen > 1) {
			uint32_t optsum;

			optsum = 0;
			opts = (uint16_t *)((char *)eh +
			    ETHER_HDR_LEN + sizeof(struct ip));

			while (hlen > 1) {
				optsum += ntohs(*opts++);
				hlen -= 2;
			}
			while (optsum >> 16)
				optsum = (optsum >> 16) + (optsum & 0xffff);

			/* Deduct the ip opts sum from the hwsum. */
			csum_data += (uint16_t)~optsum;

			while (csum_data >> 16)
				csum_data =
				    (csum_data >> 16) + (csum_data & 0xffff);
		}
		m0->m_pkthdr.csum_data = csum_data;
	}
swcsum:
	m0->m_pkthdr.csum_flags = csum_flags;
#endif

	return (m0);

bad:
	m_freem(m0);
	return (0);
}

/*
 * Pass a packet to the higher levels.
 */
void
hme_read(struct hme_softc *sc, int ix, uint32_t flags)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;
	int len;

	len = HME_XD_DECODE_RSIZE(flags);
	if (len <= sizeof(struct ether_header) ||
	    len > ((sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU) ?
	    ETHER_VLAN_ENCAP_LEN + ETHERMTU + sizeof(struct ether_header) :
	    ETHERMTU + sizeof(struct ether_header))) {
#ifdef HMEDEBUG
		printf("%s: invalid packet size %d; dropping\n",
		    device_xname(sc->sc_dev), len);
#endif
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = hme_get(sc, ix, flags);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	bpf_mtap(ifp, m);

	/* Pass the packet up. */
	(*ifp->if_input)(ifp, m);
}

void
hme_start(struct ifnet *ifp)
{
	struct hme_softc *sc = ifp->if_softc;
	void *txd = sc->sc_rb.rb_txd;
	struct mbuf *m;
	unsigned int txflags;
	unsigned int ri, len, obusy;
	unsigned int ntbuf = sc->sc_rb.rb_ntbuf;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	ri = sc->sc_rb.rb_tdhead;
	obusy = sc->sc_rb.rb_td_nbusy;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		bpf_mtap(ifp, m);

#ifdef INET
		/* collect bits for h/w csum, before hme_put frees the mbuf */
		if (ifp->if_csum_flags_tx & (M_CSUM_TCPv4 | M_CSUM_UDPv4) &&
		    m->m_pkthdr.csum_flags & (M_CSUM_TCPv4 | M_CSUM_UDPv4)) {
			struct ether_header *eh;
			uint16_t offset, start;

			eh = mtod(m, struct ether_header *);
			switch (ntohs(eh->ether_type)) {
			case ETHERTYPE_IP:
				start = ETHER_HDR_LEN;
				break;
			case ETHERTYPE_VLAN:
				start = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
				break;
			default:
				/* unsupported, drop it */
				m_free(m);
				continue;
			}
			start += M_CSUM_DATA_IPv4_IPHL(m->m_pkthdr.csum_data);
			offset = M_CSUM_DATA_IPv4_OFFSET(m->m_pkthdr.csum_data)
			    + start;
			txflags = HME_XD_TXCKSUM |
				  (offset << HME_XD_TXCSSTUFFSHIFT) |
		  		  (start << HME_XD_TXCSSTARTSHIFT);
		} else
#endif
			txflags = 0;

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = hme_put(sc, ri, m);

		/*
		 * Initialize transmit registers and start transmission
		 */
		HME_XD_SETFLAGS(sc->sc_pci, txd, ri,
			HME_XD_OWN | HME_XD_SOP | HME_XD_EOP |
			HME_XD_ENCODE_TSIZE(len) | txflags);

		/*if (sc->sc_rb.rb_td_nbusy <= 0)*/
		bus_space_write_4(sc->sc_bustag, sc->sc_etx, HME_ETXI_PENDING,
				  HME_ETX_TP_DMAWAKEUP);

		if (++ri == ntbuf)
			ri = 0;

		if (++sc->sc_rb.rb_td_nbusy == ntbuf) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	if (obusy != sc->sc_rb.rb_td_nbusy) {
		sc->sc_rb.rb_tdhead = ri;
		ifp->if_timer = 5;
	}
}

/*
 * Transmit interrupt.
 */
int
hme_tint(struct hme_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	unsigned int ri, txflags;

	/*
	 * Unload collision counters
	 */
	ifp->if_collisions +=
		bus_space_read_4(t, mac, HME_MACI_NCCNT) +
		bus_space_read_4(t, mac, HME_MACI_FCCNT);
	ifp->if_oerrors +=
		bus_space_read_4(t, mac, HME_MACI_EXCNT) +
		bus_space_read_4(t, mac, HME_MACI_LTCNT);

	/*
	 * then clear the hardware counters.
	 */
	bus_space_write_4(t, mac, HME_MACI_NCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_FCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_EXCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_LTCNT, 0);

	/* Fetch current position in the transmit ring */
	ri = sc->sc_rb.rb_tdtail;

	for (;;) {
		if (sc->sc_rb.rb_td_nbusy <= 0)
			break;

		txflags = HME_XD_GETFLAGS(sc->sc_pci, sc->sc_rb.rb_txd, ri);

		if (txflags & HME_XD_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;

		if (++ri == sc->sc_rb.rb_ntbuf)
			ri = 0;

		--sc->sc_rb.rb_td_nbusy;
	}

	/* Update ring */
	sc->sc_rb.rb_tdtail = ri;

	hme_start(ifp);

	if (sc->sc_rb.rb_td_nbusy == 0)
		ifp->if_timer = 0;

	return (1);
}

/*
 * Receive interrupt.
 */
int
hme_rint(struct hme_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	void *xdr = sc->sc_rb.rb_rxd;
	unsigned int nrbuf = sc->sc_rb.rb_nrbuf;
	unsigned int ri;
	uint32_t flags;

	ri = sc->sc_rb.rb_rdtail;

	/*
	 * Process all buffers with valid data.
	 */
	for (;;) {
		flags = HME_XD_GETFLAGS(sc->sc_pci, xdr, ri);
		if (flags & HME_XD_OWN)
			break;

		if (flags & HME_XD_OFL) {
			printf("%s: buffer overflow, ri=%d; flags=0x%x\n",
					device_xname(sc->sc_dev), ri, flags);
		} else
			hme_read(sc, ri, flags);

		/* This buffer can be used by the hardware again */
		HME_XD_SETFLAGS(sc->sc_pci, xdr, ri,
				HME_XD_OWN | HME_XD_ENCODE_RSIZE(_HME_BUFSZ));

		if (++ri == nrbuf)
			ri = 0;
	}

	sc->sc_rb.rb_rdtail = ri;

	/* Read error counters ... */
	ifp->if_ierrors +=
	    bus_space_read_4(t, mac, HME_MACI_STAT_LCNT) +
	    bus_space_read_4(t, mac, HME_MACI_STAT_ACNT) +
	    bus_space_read_4(t, mac, HME_MACI_STAT_CCNT) +
	    bus_space_read_4(t, mac, HME_MACI_STAT_CVCNT);

	/* ... then clear the hardware counters. */
	bus_space_write_4(t, mac, HME_MACI_STAT_LCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_STAT_ACNT, 0);
	bus_space_write_4(t, mac, HME_MACI_STAT_CCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_STAT_CVCNT, 0);
	return (1);
}

int
hme_eint(struct hme_softc *sc, u_int status)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	char bits[128];

	if ((status & HME_SEB_STAT_MIFIRQ) != 0) {
		bus_space_tag_t t = sc->sc_bustag;
		bus_space_handle_t mif = sc->sc_mif;
		uint32_t cf, st, sm;
		cf = bus_space_read_4(t, mif, HME_MIFI_CFG);
		st = bus_space_read_4(t, mif, HME_MIFI_STAT);
		sm = bus_space_read_4(t, mif, HME_MIFI_SM);
		printf("%s: XXXlink status changed: cfg=%x, stat %x, sm %x\n",
			device_xname(sc->sc_dev), cf, st, sm);
		return (1);
	}

	/* Receive error counters rolled over */
	if (status & HME_SEB_STAT_ACNTEXP)
		ifp->if_ierrors += 0xff;
	if (status & HME_SEB_STAT_CCNTEXP)
		ifp->if_ierrors += 0xff;
	if (status & HME_SEB_STAT_LCNTEXP)
		ifp->if_ierrors += 0xff;
	if (status & HME_SEB_STAT_CVCNTEXP)
		ifp->if_ierrors += 0xff;

	/* RXTERR locks up the interface, so do a reset */
	if (status & HME_SEB_STAT_RXTERR)
		hme_reset(sc);

	snprintb(bits, sizeof(bits), HME_SEB_STAT_BITS, status);
	printf("%s: status=%s\n", device_xname(sc->sc_dev), bits);
		
	return (1);
}

int
hme_intr(void *v)
{
	struct hme_softc *sc = v;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	uint32_t status;
	int r = 0;

	status = bus_space_read_4(t, seb, HME_SEBI_STAT);

	if ((status & HME_SEB_STAT_ALL_ERRORS) != 0)
		r |= hme_eint(sc, status);

	if ((status & (HME_SEB_STAT_TXALL | HME_SEB_STAT_HOSTTOTX)) != 0)
		r |= hme_tint(sc);

	if ((status & HME_SEB_STAT_RXTOHOST) != 0)
		r |= hme_rint(sc);

	rnd_add_uint32(&sc->rnd_source, status);

	return (r);
}


void
hme_watchdog(struct ifnet *ifp)
{
	struct hme_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;

	hme_reset(sc);
}

/*
 * Initialize the MII Management Interface
 */
void
hme_mifinit(struct hme_softc *sc)
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	bus_space_handle_t mac = sc->sc_mac;
	int instance, phy;
	uint32_t v;

	if (sc->sc_mii.mii_media.ifm_cur != NULL) {
		instance = IFM_INST(sc->sc_mii.mii_media.ifm_cur->ifm_media);
		phy = sc->sc_phys[instance];
	} else
		/* No media set yet, pick phy arbitrarily.. */
		phy = HME_PHYAD_EXTERNAL;

	/* Configure the MIF in frame mode, no poll, current phy select */
	v = 0;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* If an external transceiver is selected, enable its MII drivers */
	v = bus_space_read_4(t, mac, HME_MACI_XIF);
	v &= ~HME_MAC_XIF_MIIENABLE;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);
}

/*
 * MII interface
 */
static int
hme_mii_readreg(device_t self, int phy, int reg)
{
	struct hme_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	bus_space_handle_t mac = sc->sc_mac;
	uint32_t v, xif_cfg, mifi_cfg;
	int n;

	/* We can at most have two PHYs */
	if (phy != HME_PHYAD_EXTERNAL && phy != HME_PHYAD_INTERNAL)
		return (0);

	/* Select the desired PHY in the MIF configuration register */
	v = mifi_cfg = bus_space_read_4(t, mif, HME_MIFI_CFG);
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* Enable MII drivers on external transceiver */
	v = xif_cfg = bus_space_read_4(t, mac, HME_MACI_XIF);
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MAC_XIF_MIIENABLE;
	else
		v &= ~HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);

#if 0
/* This doesn't work reliably; the MDIO_1 bit is off most of the time */
	/*
	 * Check whether a transceiver is connected by testing
	 * the MIF configuration register's MDI_X bits. Note that
	 * MDI_0 (int) == 0x100 and MDI_1 (ext) == 0x200; see hmereg.h
	 */
	mif_mdi_bit = 1 << (8 + (1 - phy));
	delay(100);
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	if ((v & mif_mdi_bit) == 0)
		return (0);
#endif

	/* Construct the frame command */
	v = (MII_COMMAND_START << HME_MIF_FO_ST_SHIFT) |
	    HME_MIF_FO_TAMSB |
	    (MII_COMMAND_READ << HME_MIF_FO_OPC_SHIFT) |
	    (phy << HME_MIF_FO_PHYAD_SHIFT) |
	    (reg << HME_MIF_FO_REGAD_SHIFT);

	bus_space_write_4(t, mif, HME_MIFI_FO, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, HME_MIFI_FO);
		if (v & HME_MIF_FO_TALSB) {
			v &= HME_MIF_FO_DATA;
			goto out;
		}
	}

	v = 0;
	printf("%s: mii_read timeout\n", device_xname(sc->sc_dev));

out:
	/* Restore MIFI_CFG register */
	bus_space_write_4(t, mif, HME_MIFI_CFG, mifi_cfg);
	/* Restore XIF register */
	bus_space_write_4(t, mac, HME_MACI_XIF, xif_cfg);
	return (v);
}

static void
hme_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct hme_softc *sc = device_private(self);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	bus_space_handle_t mac = sc->sc_mac;
	uint32_t v, xif_cfg, mifi_cfg;
	int n;

	/* We can at most have two PHYs */
	if (phy != HME_PHYAD_EXTERNAL && phy != HME_PHYAD_INTERNAL)
		return;

	/* Select the desired PHY in the MIF configuration register */
	v = mifi_cfg = bus_space_read_4(t, mif, HME_MIFI_CFG);
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* Enable MII drivers on external transceiver */
	v = xif_cfg = bus_space_read_4(t, mac, HME_MACI_XIF);
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MAC_XIF_MIIENABLE;
	else
		v &= ~HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);

#if 0
/* This doesn't work reliably; the MDIO_1 bit is off most of the time */
	/*
	 * Check whether a transceiver is connected by testing
	 * the MIF configuration register's MDI_X bits. Note that
	 * MDI_0 (int) == 0x100 and MDI_1 (ext) == 0x200; see hmereg.h
	 */
	mif_mdi_bit = 1 << (8 + (1 - phy));
	delay(100);
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	if ((v & mif_mdi_bit) == 0)
		return;
#endif

	/* Construct the frame command */
	v = (MII_COMMAND_START << HME_MIF_FO_ST_SHIFT)	|
	    HME_MIF_FO_TAMSB				|
	    (MII_COMMAND_WRITE << HME_MIF_FO_OPC_SHIFT)	|
	    (phy << HME_MIF_FO_PHYAD_SHIFT)		|
	    (reg << HME_MIF_FO_REGAD_SHIFT)		|
	    (val & HME_MIF_FO_DATA);

	bus_space_write_4(t, mif, HME_MIFI_FO, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, HME_MIFI_FO);
		if (v & HME_MIF_FO_TALSB)
			goto out;
	}

	printf("%s: mii_write timeout\n", device_xname(sc->sc_dev));
out:
	/* Restore MIFI_CFG register */
	bus_space_write_4(t, mif, HME_MIFI_CFG, mifi_cfg);
	/* Restore XIF register */
	bus_space_write_4(t, mac, HME_MACI_XIF, xif_cfg);
}

static void
hme_mii_statchg(struct ifnet *ifp)
{
	struct hme_softc *sc = ifp->if_softc;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	uint32_t v;

#ifdef HMEDEBUG
	if (sc->sc_debug)
		printf("hme_mii_statchg: status change\n");
#endif

	/* Set the MAC Full Duplex bit appropriately */
	/* Apparently the hme chip is SIMPLEX if working in full duplex mode,
	   but not otherwise. */
	v = bus_space_read_4(t, mac, HME_MACI_TXCFG);
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0) {
		v |= HME_MAC_TXCFG_FULLDPLX;
		sc->sc_ethercom.ec_if.if_flags |= IFF_SIMPLEX;
	} else {
		v &= ~HME_MAC_TXCFG_FULLDPLX;
		sc->sc_ethercom.ec_if.if_flags &= ~IFF_SIMPLEX;
	}
	sc->sc_if_flags = sc->sc_ethercom.ec_if.if_flags;
	bus_space_write_4(t, mac, HME_MACI_TXCFG, v);
}

int
hme_mediachange(struct ifnet *ifp)
{
	struct hme_softc *sc = ifp->if_softc;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	bus_space_handle_t mac = sc->sc_mac;
	int instance = IFM_INST(sc->sc_mii.mii_media.ifm_cur->ifm_media);
	int phy = sc->sc_phys[instance];
	int rc;
	uint32_t v;

#ifdef HMEDEBUG
	if (sc->sc_debug)
		printf("hme_mediachange: phy = %d\n", phy);
#endif

	/* Select the current PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* If an external transceiver is selected, enable its MII drivers */
	v = bus_space_read_4(t, mac, HME_MACI_XIF);
	v &= ~HME_MAC_XIF_MIIENABLE;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);

	if ((rc = mii_mediachg(&sc->sc_mii)) == ENXIO)
		return 0;
	return rc;
}

/*
 * Process an ioctl request.
 */
int
hme_ioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct hme_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			if (ifp->if_flags & IFF_UP)
				hme_setladrf(sc);
			else {
				ifp->if_flags |= IFF_UP;
				error = hme_init(ifp);
			}
			arp_ifinit(ifp, ifa);
			break;
#endif
		default:
			ifp->if_flags |= IFF_UP;
			error = hme_init(ifp);
			break;
		}
		break;

	case SIOCSIFFLAGS:
#ifdef HMEDEBUG
		{
			struct ifreq *ifr = data;
			sc->sc_debug =
			    (ifr->ifr_flags & IFF_DEBUG) != 0 ? 1 : 0;
		}
#endif
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			hme_stop(ifp, 0);
			ifp->if_flags &= ~IFF_RUNNING;
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			error = hme_init(ifp);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * If setting debug or promiscuous mode, do not reset
			 * the chip; for everything else, call hme_init()
			 * which will trigger a reset.
			 */
#define RESETIGN (IFF_CANTCHANGE | IFF_DEBUG)
			if (ifp->if_flags != sc->sc_if_flags) {
				if ((ifp->if_flags & (~RESETIGN))
				    == (sc->sc_if_flags & (~RESETIGN)))
					hme_setladrf(sc);
				else
					error = hme_init(ifp);
			}
#undef RESETIGN
			break;
		case 0:
			break;
		}

		if (sc->sc_ec_capenable != sc->sc_ethercom.ec_capenable)
			error = hme_init(ifp);

		break;

	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			hme_setladrf(sc);
		}
		break;
	}

	sc->sc_if_flags = ifp->if_flags;
	splx(s);
	return (error);
}

bool
hme_shutdown(device_t self, int howto)
{
	struct hme_softc *sc;
	struct ifnet *ifp;

	sc = device_private(self);
	ifp = &sc->sc_ethercom.ec_if;
	hme_stop(ifp, 1);

	return true;
}

/*
 * Set up the logical address filter.
 */
void
hme_setladrf(struct hme_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ethercom *ec = &sc->sc_ethercom;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	uint32_t v;
	uint32_t crc;
	uint32_t hash[4];

	/* Clear hash table */
	hash[3] = hash[2] = hash[1] = hash[0] = 0;

	/* Get current RX configuration */
	v = bus_space_read_4(t, mac, HME_MACI_RXCFG);

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		/* Turn on promiscuous mode; turn off the hash filter */
		v |= HME_MAC_RXCFG_PMISC;
		v &= ~HME_MAC_RXCFG_HENABLE;
		ifp->if_flags |= IFF_ALLMULTI;
		goto chipit;
	}

	/* Turn off promiscuous mode; turn on the hash filter */
	v &= ~HME_MAC_RXCFG_PMISC;
	v |= HME_MAC_RXCFG_HENABLE;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

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
			hash[3] = hash[2] = hash[1] = hash[0] = 0xffff;
			ifp->if_flags |= IFF_ALLMULTI;
			goto chipit;
		}

		crc = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

chipit:
	/* Now load the hash table into the chip */
	bus_space_write_4(t, mac, HME_MACI_HASHTAB0, hash[0]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB1, hash[1]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB2, hash[2]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB3, hash[3]);
	bus_space_write_4(t, mac, HME_MACI_RXCFG, v);
}

/*
 * Routines for accessing the transmit and receive buffers.
 * The various CPU and adapter configurations supported by this
 * driver require three different access methods for buffers
 * and descriptors:
 *	(1) contig (contiguous data; no padding),
 *	(2) gap2 (two bytes of data followed by two bytes of padding),
 *	(3) gap16 (16 bytes of data followed by 16 bytes of padding).
 */

#if 0
/*
 * contig: contiguous data with no padding.
 *
 * Buffers may have any alignment.
 */

void
hme_copytobuf_contig(struct hme_softc *sc, void *from, int ri, int len)
{
	volatile void *buf = sc->sc_rb.rb_txbuf + (ri * _HME_BUFSZ);

	/*
	 * Just call memcpy() to do the work.
	 */
	memcpy(buf, from, len);
}

void
hme_copyfrombuf_contig(struct hme_softc *sc, void *to, int boff, int len)
{
	volatile void *buf = sc->sc_rb.rb_rxbuf + (ri * _HME_BUFSZ);

	/*
	 * Just call memcpy() to do the work.
	 */
	memcpy(to, buf, len);
}
#endif
