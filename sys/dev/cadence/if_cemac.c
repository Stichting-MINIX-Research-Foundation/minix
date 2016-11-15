/*	$NetBSD: if_cemac.c,v 1.7 2015/08/24 18:51:37 rjs Exp $	*/

/*
 * Copyright (c) 2015  Genetec Corporation.  All rights reserved.
 * Written by Hashimoto Kenichi for Genetec Corporation.
 *
 * Based on arch/arm/at91/at91emac.c
 *
 * Copyright (c) 2007 Embedtronics Oy
 * All rights reserved.
 *
 * Copyright (c) 2004 Jesse Off
 * All rights reserved.
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
 * Cadence EMAC/GEM ethernet controller IP driver
 * used by arm/at91, arm/zynq SoC
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_cemac.c,v 1.7 2015/08/24 18:51:37 rjs Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <sys/bus.h>
#include <machine/intr.h>

#include <arm/cpufunc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#ifdef IPKDB_AT91	// @@@
#include <ipkdb/ipkdb.h>
#endif

#include <dev/cadence/cemacreg.h>
#include <dev/cadence/if_cemacvar.h>

#define DEFAULT_MDCDIV	32

#define CEMAC_READ(x) \
	bus_space_read_4(sc->sc_iot, sc->sc_ioh, (x))
#define CEMAC_WRITE(x, y) \
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, (x), (y))
#define CEMAC_GEM_WRITE(x, y)						      \
	do {								      \
		if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))		      \
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, (GEM_##x), (y)); \
		else							      \
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, (ETH_##x), (y)); \
	} while(0)

#define RX_QLEN 64
#define	TX_QLEN	2		/* I'm very sorry but that's where we can get */

struct cemac_qmeta {
	struct mbuf 	*m;
	bus_dmamap_t	m_dmamap;
};

struct cemac_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	uint8_t			sc_enaddr[ETHER_ADDR_LEN];
	struct ethercom		sc_ethercom;
	mii_data_t		sc_mii;

	void			*rbqpage;
	unsigned		rbqlen;
	bus_addr_t		rbqpage_dsaddr;
	bus_dmamap_t		rbqpage_dmamap;
	void			*tbqpage;
	unsigned		tbqlen;
	bus_addr_t		tbqpage_dsaddr;
	bus_dmamap_t		tbqpage_dmamap;

	volatile struct eth_dsc *RDSC;
	int			rxqi;
	struct cemac_qmeta	rxq[RX_QLEN];
	volatile struct eth_dsc *TDSC;
	int			txqi, txqc;
	struct cemac_qmeta	txq[TX_QLEN];
	callout_t		cemac_tick_ch;

	int			cemac_flags;
};

static void	cemac_init(struct cemac_softc *);
static int	cemac_gctx(struct cemac_softc *);
static int	cemac_mediachange(struct ifnet *);
static void	cemac_mediastatus(struct ifnet *, struct ifmediareq *);
static int	cemac_mii_readreg(device_t, int, int);
static void	cemac_mii_writereg(device_t, int, int, int);
static void	cemac_statchg(struct ifnet *);
static void	cemac_tick(void *);
static int	cemac_ifioctl(struct ifnet *, u_long, void *);
static void	cemac_ifstart(struct ifnet *);
static void	cemac_ifwatchdog(struct ifnet *);
static int	cemac_ifinit(struct ifnet *);
static void	cemac_ifstop(struct ifnet *, int);
static void	cemac_setaddr(struct ifnet *);

#ifdef	CEMAC_DEBUG
int cemac_debug = CEMAC_DEBUG;
#define	DPRINTFN(n,fmt)	if (cemac_debug >= (n)) printf fmt
#else
#define	DPRINTFN(n,fmt)
#endif

CFATTACH_DECL_NEW(cemac, sizeof(struct cemac_softc),
    cemac_match, cemac_attach, NULL, NULL);

int
cemac_match_common(device_t parent, cfdata_t match, void *aux)
{
	if (strcmp(match->cf_name, "cemac") == 0)
		return 1;
	return 0;
}

void
cemac_attach_common(device_t self, bus_space_tag_t iot,
    bus_space_handle_t ioh, bus_dma_tag_t dmat, int flags)
{
	struct cemac_softc	*sc = device_private(self);
	prop_data_t		enaddr;
	uint32_t		u;


	sc->sc_dev = self;
	sc->sc_ioh = ioh;
	sc->sc_iot = iot;
	sc->sc_dmat = dmat;
	sc->cemac_flags = flags;

	aprint_naive("\n");
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		aprint_normal(": Cadence Gigabit Ethernet Controller\n");
	else
		aprint_normal(": Cadence Ethernet Controller\n");

	/* configure emac: */
	CEMAC_WRITE(ETH_CTL, 0);		// disable everything
	CEMAC_WRITE(ETH_IDR, -1);		// disable interrupts
	CEMAC_WRITE(ETH_RBQP, 0);		// clear receive
	CEMAC_WRITE(ETH_TBQP, 0);		// clear transmit
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		CEMAC_WRITE(ETH_CFG,
		    GEM_CFG_CLK_64 | GEM_CFG_GEN | ETH_CFG_SPD | ETH_CFG_FD);
	else
		CEMAC_WRITE(ETH_CFG,
		    ETH_CFG_CLK_32 | ETH_CFG_SPD | ETH_CFG_FD | ETH_CFG_BIG);
	//CEMAC_WRITE(ETH_TCR, 0);		// send nothing
	//(void)CEMAC_READ(ETH_ISR);
	u = CEMAC_READ(ETH_TSR);
	CEMAC_WRITE(ETH_TSR, (u & (ETH_TSR_UND | ETH_TSR_COMP | ETH_TSR_BNQ
				  | ETH_TSR_IDLE | ETH_TSR_RLE
				  | ETH_TSR_COL|ETH_TSR_OVR)));
	u = CEMAC_READ(ETH_RSR);
	CEMAC_WRITE(ETH_RSR, (u & (ETH_RSR_OVR|ETH_RSR_REC|ETH_RSR_BNA)));

	/* Fetch the Ethernet address from property if set. */
	enaddr = prop_dictionary_get(device_properties(self), "mac-address");

	if (enaddr != NULL) {
		KASSERT(prop_object_type(enaddr) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(enaddr) == ETHER_ADDR_LEN);
		memcpy(sc->sc_enaddr, prop_data_data_nocopy(enaddr),
		       ETHER_ADDR_LEN);
	} else {
		static const uint8_t hardcoded[ETHER_ADDR_LEN] = {
			0x00, 0x0d, 0x10, 0x81, 0x0c, 0x94
		};
		memcpy(sc->sc_enaddr, hardcoded, ETHER_ADDR_LEN);
	}

	cemac_init(sc);
}

static int
cemac_gctx(struct cemac_softc *sc)
{
	struct ifnet * ifp = &sc->sc_ethercom.ec_if;
	uint32_t tsr;

	tsr = CEMAC_READ(ETH_TSR);
	if (!ISSET(sc->cemac_flags, CEMAC_FLAG_GEM)) {
		// no space left
		if (!(tsr & ETH_TSR_BNQ))
			return 0;
	} else {
		if (tsr & GEM_TSR_TXGO)
			return 0;
	}
	CEMAC_WRITE(ETH_TSR, tsr);

	// free sent frames
	while (sc->txqc > (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM) ? 0 :
		(tsr & ETH_TSR_IDLE ? 0 : 1))) {
		int bi = sc->txqi % TX_QLEN;

		DPRINTFN(3,("%s: TDSC[%i].Addr 0x%08x\n",
			__FUNCTION__, bi, sc->TDSC[bi].Addr));
		DPRINTFN(3,("%s: TDSC[%i].Info 0x%08x\n",
			__FUNCTION__, bi, sc->TDSC[bi].Info));

		bus_dmamap_sync(sc->sc_dmat, sc->txq[bi].m_dmamap, 0,
		    sc->txq[bi].m->m_pkthdr.len, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->txq[bi].m_dmamap);
		m_freem(sc->txq[bi].m);
		DPRINTFN(2,("%s: freed idx #%i mbuf %p (txqc=%i)\n",
		    __FUNCTION__, bi, sc->txq[bi].m, sc->txqc));
		sc->txq[bi].m = NULL;
		sc->txqi = (bi + 1) % TX_QLEN;
		sc->txqc--;
	}

	// mark we're free
	if (ifp->if_flags & IFF_OACTIVE) {
		ifp->if_flags &= ~IFF_OACTIVE;
		/* Disable transmit-buffer-free interrupt */
		/*CEMAC_WRITE(ETH_IDR, ETH_ISR_TBRE);*/
	}

	return 1;
}

int
cemac_intr(void *arg)
{
	struct cemac_softc *sc = (struct cemac_softc *)arg;
	struct ifnet * ifp = &sc->sc_ethercom.ec_if;
	uint32_t imr, isr, ctl;
#ifdef	CEMAC_DEBUG
	uint32_t rsr;
#endif
	int bi;

	imr = ~CEMAC_READ(ETH_IMR);
	if (!(imr & (ETH_ISR_RCOM|ETH_ISR_TBRE|ETH_ISR_TIDLE|ETH_ISR_RBNA|ETH_ISR_ROVR|ETH_ISR_TCOM))) {
		// interrupt not enabled, can't be us
		return 0;
	}

	isr = CEMAC_READ(ETH_ISR);
	CEMAC_WRITE(ETH_ISR, isr);
	isr &= imr;
#ifdef	CEMAC_DEBUG
	rsr = CEMAC_READ(ETH_RSR);		// get receive status register
#endif
	DPRINTFN(2, ("%s: isr=0x%08X rsr=0x%08X imr=0x%08X\n", __FUNCTION__, isr, rsr, imr));

	if (isr & ETH_ISR_RBNA) {		// out of receive buffers
		CEMAC_WRITE(ETH_RSR, ETH_RSR_BNA);	// clear interrupt
		ctl = CEMAC_READ(ETH_CTL);		// get current control register value
		CEMAC_WRITE(ETH_CTL, ctl & ~ETH_CTL_RE);	// disable receiver
		CEMAC_WRITE(ETH_RSR, ETH_RSR_BNA);	// clear BNA bit
		CEMAC_WRITE(ETH_CTL, ctl |  ETH_CTL_RE);	// re-enable receiver
		ifp->if_ierrors++;
		ifp->if_ipackets++;
		DPRINTFN(1,("%s: out of receive buffers\n", __FUNCTION__));
	}
	if (isr & ETH_ISR_ROVR) {
		CEMAC_WRITE(ETH_RSR, ETH_RSR_OVR);	// clear interrupt
		ifp->if_ierrors++;
		ifp->if_ipackets++;
		DPRINTFN(1,("%s: receive overrun\n", __FUNCTION__));
	}

	if (isr & ETH_ISR_RCOM) {			// packet has been received!
		uint32_t nfo;
		DPRINTFN(2,("#2 RDSC[%i].INFO=0x%08X\n", sc->rxqi % RX_QLEN, sc->RDSC[sc->rxqi % RX_QLEN].Info));
		while (sc->RDSC[(bi = sc->rxqi % RX_QLEN)].Addr & ETH_RDSC_F_USED) {
			int fl, csum;
			struct mbuf *m;

			nfo = sc->RDSC[bi].Info;
		  	fl = (nfo & ETH_RDSC_I_LEN) - 4;
			DPRINTFN(2,("## nfo=0x%08X\n", nfo));

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m != NULL) MCLGET(m, M_DONTWAIT);
			if (m != NULL && (m->m_flags & M_EXT)) {
				bus_dmamap_sync(sc->sc_dmat, sc->rxq[bi].m_dmamap, 0,
						MCLBYTES, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat,
					sc->rxq[bi].m_dmamap);
				sc->rxq[bi].m->m_pkthdr.rcvif = ifp;
				sc->rxq[bi].m->m_pkthdr.len =
					sc->rxq[bi].m->m_len = fl;
				switch (nfo & ETH_RDSC_I_CHKSUM) {
				case ETH_RDSC_I_CHKSUM_IP:
					csum = M_CSUM_IPv4;
					break;
				case ETH_RDSC_I_CHKSUM_UDP:
					csum = M_CSUM_IPv4 | M_CSUM_UDPv4 |
					    M_CSUM_UDPv6;
					break;
				case ETH_RDSC_I_CHKSUM_TCP:
					csum = M_CSUM_IPv4 | M_CSUM_TCPv4 |
					    M_CSUM_TCPv6;
					break;
				default:
					csum = 0;
					break;
				}
				sc->rxq[bi].m->m_pkthdr.csum_flags = csum;
				bpf_mtap(ifp, sc->rxq[bi].m);
				DPRINTFN(2,("received %u bytes packet\n", fl));
                                (*ifp->if_input)(ifp, sc->rxq[bi].m);
				if (mtod(m, intptr_t) & 3)
					m_adj(m, mtod(m, intptr_t) & 3);
				sc->rxq[bi].m = m;
				bus_dmamap_load(sc->sc_dmat,
					sc->rxq[bi].m_dmamap,
					m->m_ext.ext_buf, MCLBYTES,
					NULL, BUS_DMA_NOWAIT);
				bus_dmamap_sync(sc->sc_dmat, sc->rxq[bi].m_dmamap, 0,
						MCLBYTES, BUS_DMASYNC_PREREAD);
				sc->RDSC[bi].Info = 0;
				sc->RDSC[bi].Addr =
					sc->rxq[bi].m_dmamap->dm_segs[0].ds_addr
					| (bi == (RX_QLEN-1) ? ETH_RDSC_F_WRAP : 0);
			} else {
				/* Drop packets until we can get replacement
				 * empty mbufs for the RXDQ.
				 */
				if (m != NULL)
					m_freem(m);
				ifp->if_ierrors++;
			}
			sc->rxqi++;
		}
	}

	if (cemac_gctx(sc) > 0 && IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		cemac_ifstart(ifp);
#if 0 // reloop
	irq = CEMAC_READ(IntStsC);
	if ((irq & (IntSts_RxSQ|IntSts_ECI)) != 0)
		goto begin;
#endif

	return (1);
}


static void
cemac_init(struct cemac_softc *sc)
{
	bus_dma_segment_t segs;
	int rsegs, err, i;
	struct ifnet * ifp = &sc->sc_ethercom.ec_if;
	uint32_t u;
#if 0
	int mdcdiv = DEFAULT_MDCDIV;
#endif

	callout_init(&sc->cemac_tick_ch, 0);

	// ok...
	CEMAC_WRITE(ETH_CTL, ETH_CTL_MPE);	// disable everything
	CEMAC_WRITE(ETH_IDR, -1);		// disable interrupts
	CEMAC_WRITE(ETH_RBQP, 0);		// clear receive
	CEMAC_WRITE(ETH_TBQP, 0);		// clear transmit
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		CEMAC_WRITE(ETH_CFG,
		    GEM_CFG_CLK_64 | ETH_CFG_SPD | ETH_CFG_FD | ETH_CFG_BIG);
	else
		CEMAC_WRITE(ETH_CFG,
		    ETH_CFG_CLK_32 | ETH_CFG_SPD | ETH_CFG_FD | ETH_CFG_BIG);
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM)) {
		CEMAC_WRITE(GEM_DMA_CFG,
		    __SHIFTIN((MCLBYTES + 63) / 64, GEM_DMA_CFG_RX_BUF_SIZE) |
		    __SHIFTIN(3, GEM_DMA_CFG_RX_PKTBUF_MEMSZ_SEL) |
		    GEM_DMA_CFG_TX_PKTBUF_MEMSZ_SEL |
		    __SHIFTIN(16, GEM_DMA_CFG_AHB_FIXED_BURST_LEN) |
		    GEM_DMA_CFG_DISC_WHEN_NO_AHB);
	}
//	CEMAC_WRITE(ETH_TCR, 0);			// send nothing
//	(void)CEMAC_READ(ETH_ISR);
	u = CEMAC_READ(ETH_TSR);
	CEMAC_WRITE(ETH_TSR, (u & (ETH_TSR_UND | ETH_TSR_COMP | ETH_TSR_BNQ
		    | ETH_TSR_IDLE | ETH_TSR_RLE
		    | ETH_TSR_COL|ETH_TSR_OVR)));
	u = CEMAC_READ(ETH_RSR);
	CEMAC_WRITE(ETH_RSR, (u & (ETH_RSR_OVR|ETH_RSR_REC|ETH_RSR_BNA)));

#if 0
	if (device_cfdata(sc->sc_dev)->cf_flags)
		mdcdiv = device_cfdata(sc->sc_dev)->cf_flags;
#endif
	/* set ethernet address */
	CEMAC_GEM_WRITE(SA1L, (sc->sc_enaddr[3] << 24)
	    | (sc->sc_enaddr[2] << 16) | (sc->sc_enaddr[1] << 8)
	    | (sc->sc_enaddr[0]));
	CEMAC_GEM_WRITE(SA1H, (sc->sc_enaddr[5] << 8)
	    | (sc->sc_enaddr[4]));
	CEMAC_GEM_WRITE(SA2L, 0);
	CEMAC_GEM_WRITE(SA2H, 0);
	CEMAC_GEM_WRITE(SA3L, 0);
	CEMAC_GEM_WRITE(SA3H, 0);
	CEMAC_GEM_WRITE(SA4L, 0);
	CEMAC_GEM_WRITE(SA4H, 0);

	/* Allocate a page of memory for receive queue descriptors */
	sc->rbqlen = (ETH_DSC_SIZE * (RX_QLEN + 1) * 2 + PAGE_SIZE - 1) / PAGE_SIZE;
	sc->rbqlen *= PAGE_SIZE;
	DPRINTFN(1,("%s: rbqlen=%i\n", __FUNCTION__, sc->rbqlen));

	err = bus_dmamem_alloc(sc->sc_dmat, sc->rbqlen, 0,
	    MAX(16384, PAGE_SIZE),	// see EMAC errata why forced to 16384 byte boundary
	    &segs, 1, &rsegs, BUS_DMA_WAITOK);
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamem_map\n", __FUNCTION__));
		err = bus_dmamem_map(sc->sc_dmat, &segs, 1, sc->rbqlen,
		    &sc->rbqpage, (BUS_DMA_WAITOK|BUS_DMA_COHERENT));
	}
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamap_create\n", __FUNCTION__));
		err = bus_dmamap_create(sc->sc_dmat, sc->rbqlen, 1,
		    sc->rbqlen, MAX(16384, PAGE_SIZE), BUS_DMA_WAITOK,
		    &sc->rbqpage_dmamap);
	}
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamap_load\n", __FUNCTION__));
		err = bus_dmamap_load(sc->sc_dmat, sc->rbqpage_dmamap,
		    sc->rbqpage, sc->rbqlen, NULL, BUS_DMA_WAITOK);
	}
	if (err != 0)
		panic("%s: Cannot get DMA memory", device_xname(sc->sc_dev));

	sc->rbqpage_dsaddr = sc->rbqpage_dmamap->dm_segs[0].ds_addr;
	memset(sc->rbqpage, 0, sc->rbqlen);

	/* Allocate a page of memory for transmit queue descriptors */
	sc->tbqlen = (ETH_DSC_SIZE * (TX_QLEN + 1) * 2 + PAGE_SIZE - 1) / PAGE_SIZE;
	sc->tbqlen *= PAGE_SIZE;
	DPRINTFN(1,("%s: tbqlen=%i\n", __FUNCTION__, sc->tbqlen));

	err = bus_dmamem_alloc(sc->sc_dmat, sc->tbqlen, 0,
	    MAX(16384, PAGE_SIZE),	// see EMAC errata why forced to 16384 byte boundary
	    &segs, 1, &rsegs, BUS_DMA_WAITOK);
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamem_map\n", __FUNCTION__));
		err = bus_dmamem_map(sc->sc_dmat, &segs, 1, sc->tbqlen,
		    &sc->tbqpage, (BUS_DMA_WAITOK|BUS_DMA_COHERENT));
	}
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamap_create\n", __FUNCTION__));
		err = bus_dmamap_create(sc->sc_dmat, sc->tbqlen, 1,
		    sc->tbqlen, MAX(16384, PAGE_SIZE), BUS_DMA_WAITOK,
		    &sc->tbqpage_dmamap);
	}
	if (err == 0) {
		DPRINTFN(1,("%s: -> bus_dmamap_load\n", __FUNCTION__));
		err = bus_dmamap_load(sc->sc_dmat, sc->tbqpage_dmamap,
		    sc->tbqpage, sc->tbqlen, NULL, BUS_DMA_WAITOK);
	}
	if (err != 0)
		panic("%s: Cannot get DMA memory", device_xname(sc->sc_dev));

	sc->tbqpage_dsaddr = sc->tbqpage_dmamap->dm_segs[0].ds_addr;
	memset(sc->tbqpage, 0, sc->tbqlen);

	/* Set up pointers to start of each queue in kernel addr space.
	 * Each descriptor queue or status queue entry uses 2 words
	 */
	sc->RDSC = (void *)sc->rbqpage;
	sc->TDSC = (void *)sc->tbqpage;

	/* init TX queue */
	for (i = 0; i < TX_QLEN; i++) {
		sc->TDSC[i].Addr = 0;
		sc->TDSC[i].Info = ETH_TDSC_I_USED |
		    (i == (TX_QLEN - 1) ? ETH_TDSC_I_WRAP : 0);
	}

	/* Populate the RXQ with mbufs */
	sc->rxqi = 0;
	for(i = 0; i < RX_QLEN; i++) {
		struct mbuf *m;

		err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, PAGE_SIZE,
		    BUS_DMA_WAITOK, &sc->rxq[i].m_dmamap);
		if (err) {
			panic("%s: dmamap_create failed: %i\n", __FUNCTION__, err);
		}
		MGETHDR(m, M_WAIT, MT_DATA);
		MCLGET(m, M_WAIT);
		sc->rxq[i].m = m;
		if (mtod(m, intptr_t) & 3) {
			m_adj(m, mtod(m, intptr_t) & 3);
		}
		err = bus_dmamap_load(sc->sc_dmat, sc->rxq[i].m_dmamap,
		    m->m_ext.ext_buf, MCLBYTES, NULL,
		    BUS_DMA_WAITOK);
		if (err) {
			panic("%s: dmamap_load failed: %i\n", __FUNCTION__, err);
		}
		sc->RDSC[i].Addr = sc->rxq[i].m_dmamap->dm_segs[0].ds_addr
		    | (i == (RX_QLEN-1) ? ETH_RDSC_F_WRAP : 0);
		sc->RDSC[i].Info = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->rxq[i].m_dmamap, 0,
		    MCLBYTES, BUS_DMASYNC_PREREAD);
	}

	/* prepare transmit queue */
	for (i = 0; i < TX_QLEN; i++) {
		err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
		    (BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW),
		    &sc->txq[i].m_dmamap);
		if (err)
			panic("ARGH #1");
		sc->txq[i].m = NULL;
	}

	/* Program each queue's start addr, cur addr, and len registers
	 * with the physical addresses.
	 */
	CEMAC_WRITE(ETH_RBQP, (uint32_t)sc->rbqpage_dsaddr);
	CEMAC_WRITE(ETH_TBQP, (uint32_t)sc->tbqpage_dsaddr);

	/* Divide HCLK by 32 for MDC clock */
	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = cemac_mii_readreg;
	sc->sc_mii.mii_writereg = cemac_mii_writereg;
	sc->sc_mii.mii_statchg = cemac_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, cemac_mediachange,
	    cemac_mediastatus);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

#if 0
	// enable / disable interrupts
	CEMAC_WRITE(ETH_IDR, -1);
	CEMAC_WRITE(ETH_IER, ETH_ISR_RCOM | ETH_ISR_TBRE | ETH_ISR_TIDLE
	    | ETH_ISR_RBNA | ETH_ISR_ROVR | ETH_ISR_TCOM);
//	(void)CEMAC_READ(ETH_ISR); // why

	// enable transmitter / receiver
	CEMAC_WRITE(ETH_CTL, ETH_CTL_TE | ETH_CTL_RE | ETH_CTL_ISR
	    | ETH_CTL_CSR | ETH_CTL_MPE);
#endif
	/*
	 * We can support hardware checksumming.
	 */
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |  
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx |
	    IFCAP_CSUM_TCPv6_Tx | IFCAP_CSUM_TCPv6_Rx |
	    IFCAP_CSUM_UDPv6_Tx | IFCAP_CSUM_UDPv6_Rx;

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	strcpy(ifp->if_xname, device_xname(sc->sc_dev));
        ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
        ifp->if_ioctl = cemac_ifioctl;
        ifp->if_start = cemac_ifstart;
        ifp->if_watchdog = cemac_ifwatchdog;
        ifp->if_init = cemac_ifinit;
        ifp->if_stop = cemac_ifstop;
        ifp->if_timer = 0;
	ifp->if_softc = sc;
        IFQ_SET_READY(&ifp->if_snd);
        if_attach(ifp);
        ether_ifattach(ifp, (sc)->sc_enaddr);
}

static int
cemac_mediachange(struct ifnet *ifp)
{
	if (ifp->if_flags & IFF_UP)
		cemac_ifinit(ifp);
	return (0);
}

static void
cemac_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cemac_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}


static int
cemac_mii_readreg(device_t self, int phy, int reg)
{
	struct cemac_softc *sc;

	sc = device_private(self);

	CEMAC_WRITE(ETH_MAN, (ETH_MAN_HIGH | ETH_MAN_RW_RD
			     | ((phy << ETH_MAN_PHYA_SHIFT) & ETH_MAN_PHYA)
			     | ((reg << ETH_MAN_REGA_SHIFT) & ETH_MAN_REGA)
			     | ETH_MAN_CODE_IEEE802_3));
	while (!(CEMAC_READ(ETH_SR) & ETH_SR_IDLE));

	return (CEMAC_READ(ETH_MAN) & ETH_MAN_DATA);
}

static void
cemac_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct cemac_softc *sc;

	sc = device_private(self);

	CEMAC_WRITE(ETH_MAN, (ETH_MAN_HIGH | ETH_MAN_RW_WR
			     | ((phy << ETH_MAN_PHYA_SHIFT) & ETH_MAN_PHYA)
			     | ((reg << ETH_MAN_REGA_SHIFT) & ETH_MAN_REGA)
			     | ETH_MAN_CODE_IEEE802_3
			     | (val & ETH_MAN_DATA)));
	while (!(CEMAC_READ(ETH_SR) & ETH_SR_IDLE)) ;
}


static void
cemac_statchg(struct ifnet *ifp)
{
        struct cemac_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
        uint32_t reg;

        /*
         * We must keep the MAC and the PHY in sync as
         * to the status of full-duplex!
         */
	reg = CEMAC_READ(ETH_CFG);
	reg &= ~ETH_CFG_FD;
        if (sc->sc_mii.mii_media_active & IFM_FDX)
                reg |= ETH_CFG_FD;

	reg &= ~ETH_CFG_SPD;
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		reg &= ~GEM_CFG_GEN;
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		break;
	case IFM_100_TX:
		reg |= ETH_CFG_SPD;
		break;
	case IFM_1000_T:
		reg |= ETH_CFG_SPD | GEM_CFG_GEN;
		break;
	default:
		break;
	}
	CEMAC_WRITE(ETH_CFG, reg);
}

static void
cemac_tick(void *arg)
{
	struct cemac_softc* sc = (struct cemac_softc *)arg;
	struct ifnet * ifp = &sc->sc_ethercom.ec_if;
	int s;

	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		ifp->if_collisions += CEMAC_READ(GEM_SCOL) + CEMAC_READ(GEM_MCOL);
	else
		ifp->if_collisions += CEMAC_READ(ETH_SCOL) + CEMAC_READ(ETH_MCOL);

	/* These misses are ok, they will happen if the RAM/CPU can't keep up */
	if (!ISSET(sc->cemac_flags, CEMAC_FLAG_GEM)) {
		uint32_t misses = CEMAC_READ(ETH_DRFC);
		if (misses > 0)
			aprint_normal_ifnet(ifp, "%d rx misses\n", misses);
	}

	s = splnet();
	if (cemac_gctx(sc) > 0 && IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		cemac_ifstart(ifp);
	splx(s);

	mii_tick(&sc->sc_mii);
	callout_reset(&sc->cemac_tick_ch, hz, cemac_tick, sc);
}


static int
cemac_ifioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct cemac_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();
	switch(cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error != ENETRESET)
			break;
		error = 0;

		if (cmd == SIOCSIFCAP) {
			error = (*ifp->if_init)(ifp);
		} else if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			cemac_setaddr(ifp);
		}
	}
	splx(s);
	return error;
}

static void
cemac_ifstart(struct ifnet *ifp)
{
	struct cemac_softc *sc = (struct cemac_softc *)ifp->if_softc;
	struct mbuf *m;
	bus_dma_segment_t *segs;
	int s, bi, err, nsegs;

	s = splnet();
start:
	if (cemac_gctx(sc) == 0) {
		/* Enable transmit-buffer-free interrupt */
		CEMAC_WRITE(ETH_IER, ETH_ISR_TBRE);
		ifp->if_flags |= IFF_OACTIVE;
		ifp->if_timer = 10;
		splx(s);
		return;
	}

	ifp->if_timer = 0;

	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL) {
		splx(s);
		return;
	}

	bi = (sc->txqi + sc->txqc) % TX_QLEN;
	if ((err = bus_dmamap_load_mbuf(sc->sc_dmat, sc->txq[bi].m_dmamap, m,
		BUS_DMA_NOWAIT)) ||
		sc->txq[bi].m_dmamap->dm_segs[0].ds_addr & 0x3 ||
		sc->txq[bi].m_dmamap->dm_nsegs > 1) {
		/* Copy entire mbuf chain to new single */
		struct mbuf *mn;

		if (err == 0)
			bus_dmamap_unload(sc->sc_dmat, sc->txq[bi].m_dmamap);

		MGETHDR(mn, M_DONTWAIT, MT_DATA);
		if (mn == NULL) goto stop;
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(mn, M_DONTWAIT);
			if ((mn->m_flags & M_EXT) == 0) {
				m_freem(mn);
				goto stop;
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(mn, void *));
		mn->m_pkthdr.len = mn->m_len = m->m_pkthdr.len;
		IFQ_DEQUEUE(&ifp->if_snd, m);
		m_freem(m);
		m = mn;
		bus_dmamap_load_mbuf(sc->sc_dmat, sc->txq[bi].m_dmamap, m,
		    BUS_DMA_NOWAIT);
	} else {
		IFQ_DEQUEUE(&ifp->if_snd, m);
	}

	bpf_mtap(ifp, m);

	nsegs = sc->txq[bi].m_dmamap->dm_nsegs;
	segs = sc->txq[bi].m_dmamap->dm_segs;
	if (nsegs > 1)
		panic("#### ARGH #2");

	sc->txq[bi].m = m;
	sc->txqc++;

	DPRINTFN(2,("%s: start sending idx #%i mbuf %p (txqc=%i, phys %p), len=%u\n",
		__FUNCTION__, bi, sc->txq[bi].m, sc->txqc, (void*)segs->ds_addr,
		(unsigned)m->m_pkthdr.len));
#ifdef	DIAGNOSTIC
	if (sc->txqc > TX_QLEN)
		panic("%s: txqc %i > %i", __FUNCTION__, sc->txqc, TX_QLEN);
#endif

	bus_dmamap_sync(sc->sc_dmat, sc->txq[bi].m_dmamap, 0,
		sc->txq[bi].m_dmamap->dm_mapsize,
		BUS_DMASYNC_PREWRITE);

	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM)) {
		sc->TDSC[bi].Addr = segs->ds_addr;
		sc->TDSC[bi].Info = __SHIFTIN(m->m_pkthdr.len, ETH_TDSC_I_LEN) |
		    ETH_TDSC_I_LAST_BUF | (bi == (TX_QLEN - 1) ? ETH_TDSC_I_WRAP : 0);

		DPRINTFN(3,("%s: TDSC[%i].Addr 0x%08x\n",
			__FUNCTION__, bi, sc->TDSC[bi].Addr));
		DPRINTFN(3,("%s: TDSC[%i].Info 0x%08x\n",
			__FUNCTION__, bi, sc->TDSC[bi].Info));

		uint32_t ctl = CEMAC_READ(ETH_CTL) | GEM_CTL_STARTTX;
		CEMAC_WRITE(ETH_CTL, ctl);
		DPRINTFN(3,("%s: ETH_CTL 0x%08x\n", __FUNCTION__, CEMAC_READ(ETH_CTL)));
	} else {
		CEMAC_WRITE(ETH_TAR, segs->ds_addr);
		CEMAC_WRITE(ETH_TCR, m->m_pkthdr.len);
	}
	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		goto start;
stop:

	splx(s);
	return;
}

static void
cemac_ifwatchdog(struct ifnet *ifp)
{
	struct cemac_softc *sc = (struct cemac_softc *)ifp->if_softc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	aprint_error_ifnet(ifp, "device timeout, CTL = 0x%08x, CFG = 0x%08x\n",
		CEMAC_READ(ETH_CTL), CEMAC_READ(ETH_CFG));
}

static int
cemac_ifinit(struct ifnet *ifp)
{
	struct cemac_softc *sc = ifp->if_softc;
	uint32_t dma, cfg;
	int s = splnet();

	callout_stop(&sc->cemac_tick_ch);

	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM)) {

		if (ifp->if_capenable &
		    (IFCAP_CSUM_IPv4_Tx |
			IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_UDPv4_Tx |
			IFCAP_CSUM_TCPv6_Tx | IFCAP_CSUM_UDPv6_Tx)) {
			dma = CEMAC_READ(GEM_DMA_CFG);
			dma |= GEM_DMA_CFG_CHKSUM_GEN_OFFLOAD_EN;
			CEMAC_WRITE(GEM_DMA_CFG, dma);
		}
		if (ifp->if_capenable &
		    (IFCAP_CSUM_IPv4_Rx |
			IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_UDPv4_Rx |
			IFCAP_CSUM_TCPv6_Rx | IFCAP_CSUM_UDPv6_Rx)) {
			cfg = CEMAC_READ(ETH_CFG);
			cfg |= GEM_CFG_RX_CHKSUM_OFFLD_EN;
			CEMAC_WRITE(ETH_CFG, cfg);
		}
	}

	// enable interrupts
	CEMAC_WRITE(ETH_IDR, -1);
	CEMAC_WRITE(ETH_IER, ETH_ISR_RCOM | ETH_ISR_TBRE | ETH_ISR_TIDLE
	    | ETH_ISR_RBNA | ETH_ISR_ROVR | ETH_ISR_TCOM);

	// enable transmitter / receiver
	CEMAC_WRITE(ETH_CTL, ETH_CTL_TE | ETH_CTL_RE | ETH_CTL_ISR
	    | ETH_CTL_CSR | ETH_CTL_MPE);

	mii_mediachg(&sc->sc_mii);
	callout_reset(&sc->cemac_tick_ch, hz, cemac_tick, sc);
        ifp->if_flags |= IFF_RUNNING;
	splx(s);
	return 0;
}

static void
cemac_ifstop(struct ifnet *ifp, int disable)
{
//	uint32_t u;
	struct cemac_softc *sc = ifp->if_softc;

#if 0
	CEMAC_WRITE(ETH_CTL, ETH_CTL_MPE);	// disable everything
	CEMAC_WRITE(ETH_IDR, -1);		// disable interrupts
//	CEMAC_WRITE(ETH_RBQP, 0);		// clear receive
	if (ISSET(sc->cemac_flags, CEMAC_FLAG_GEM))
		CEMAC_WRITE(ETH_CFG,
		    GEM_CFG_CLK_64 | ETH_CFG_SPD | ETH_CFG_FD | ETH_CFG_BIG);
	else
		CEMAC_WRITE(ETH_CFG,
		    ETH_CFG_CLK_32 | ETH_CFG_SPD | ETH_CFG_FD | ETH_CFG_BIG);
//	CEMAC_WRITE(ETH_TCR, 0);			// send nothing
//	(void)CEMAC_READ(ETH_ISR);
	u = CEMAC_READ(ETH_TSR);
	CEMAC_WRITE(ETH_TSR, (u & (ETH_TSR_UND | ETH_TSR_COMP | ETH_TSR_BNQ
				  | ETH_TSR_IDLE | ETH_TSR_RLE
				  | ETH_TSR_COL|ETH_TSR_OVR)));
	u = CEMAC_READ(ETH_RSR);
	CEMAC_WRITE(ETH_RSR, (u & (ETH_RSR_OVR|ETH_RSR_REC|ETH_RSR_BNA)));
#endif
	callout_stop(&sc->cemac_tick_ch);

	/* Down the MII. */
	mii_down(&sc->sc_mii);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
	sc->sc_mii.mii_media_status &= ~IFM_ACTIVE;
}

static void
cemac_setaddr(struct ifnet *ifp)
{
	struct cemac_softc *sc = ifp->if_softc;
	struct ethercom *ac = &sc->sc_ethercom;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint8_t ias[3][ETHER_ADDR_LEN];
	uint32_t h, nma = 0, hashes[2] = { 0, 0 };
	uint32_t ctl = CEMAC_READ(ETH_CTL);
	uint32_t cfg = CEMAC_READ(ETH_CFG);

	/* disable receiver temporarily */
	CEMAC_WRITE(ETH_CTL, ctl & ~ETH_CTL_RE);

	cfg &= ~(ETH_CFG_MTI | ETH_CFG_UNI | ETH_CFG_CAF | ETH_CFG_UNI);

	if (ifp->if_flags & IFF_PROMISC) {
		cfg |=  ETH_CFG_CAF;
	} else {
		cfg &= ~ETH_CFG_CAF;
	}

	// ETH_CFG_BIG?

	ifp->if_flags &= ~IFF_ALLMULTI;

	ETHER_FIRST_MULTI(step, ac, enm);
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
			cfg |= ETH_CFG_MTI;
			hashes[0] = 0xffffffffUL;
			hashes[1] = 0xffffffffUL;
			ifp->if_flags |= IFF_ALLMULTI;
			nma = 0;
			break;
		}

		if (nma < 3) {
			/* We can program 3 perfect address filters for mcast */
			memcpy(ias[nma], enm->enm_addrlo, ETHER_ADDR_LEN);
		} else {
			/*
			 * XXX: Datasheet is not very clear here, I'm not sure
			 * if I'm doing this right.  --joff
			 */
			h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

			/* Just want the 6 most-significant bits. */
			h = h >> 26;
#if 0
			hashes[h / 32] |=  (1 << (h % 32));
#else
			hashes[0] = 0xffffffffUL;
			hashes[1] = 0xffffffffUL;
#endif
			cfg |= ETH_CFG_MTI;
		}
		ETHER_NEXT_MULTI(step, enm);
		nma++;
	}

	// program...
	DPRINTFN(1,("%s: en0 %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
		sc->sc_enaddr[0], sc->sc_enaddr[1], sc->sc_enaddr[2],
		sc->sc_enaddr[3], sc->sc_enaddr[4], sc->sc_enaddr[5]));
	CEMAC_GEM_WRITE(SA1L, (sc->sc_enaddr[3] << 24)
	    | (sc->sc_enaddr[2] << 16) | (sc->sc_enaddr[1] << 8)
	    | (sc->sc_enaddr[0]));
	CEMAC_GEM_WRITE(SA1H, (sc->sc_enaddr[5] << 8)
	    | (sc->sc_enaddr[4]));
	if (nma > 0) {
		DPRINTFN(1,("%s: en1 %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
			ias[0][0], ias[0][1], ias[0][2],
			ias[0][3], ias[0][4], ias[0][5]));
		CEMAC_WRITE(ETH_SA2L, (ias[0][3] << 24)
		    | (ias[0][2] << 16) | (ias[0][1] << 8)
		    | (ias[0][0]));
		CEMAC_WRITE(ETH_SA2H, (ias[0][4] << 8)
		    | (ias[0][5]));
	}
	if (nma > 1) {
		DPRINTFN(1,("%s: en2 %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
			ias[1][0], ias[1][1], ias[1][2],
			ias[1][3], ias[1][4], ias[1][5]));
		CEMAC_WRITE(ETH_SA3L, (ias[1][3] << 24)
		    | (ias[1][2] << 16) | (ias[1][1] << 8)
		    | (ias[1][0]));
		CEMAC_WRITE(ETH_SA3H, (ias[1][4] << 8)
		    | (ias[1][5]));
	}
	if (nma > 2) {
		DPRINTFN(1,("%s: en3 %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
			ias[2][0], ias[2][1], ias[2][2],
			ias[2][3], ias[2][4], ias[2][5]));
		CEMAC_WRITE(ETH_SA4L, (ias[2][3] << 24)
		    | (ias[2][2] << 16) | (ias[2][1] << 8)
		    | (ias[2][0]));
		CEMAC_WRITE(ETH_SA4H, (ias[2][4] << 8)
		    | (ias[2][5]));
	}
	CEMAC_GEM_WRITE(HSH, hashes[0]);
	CEMAC_GEM_WRITE(HSL, hashes[1]);
	CEMAC_WRITE(ETH_CFG, cfg);
	CEMAC_WRITE(ETH_CTL, ctl | ETH_CTL_RE);
}
