/* $NetBSD: mtd803.c,v 1.29 2014/08/10 16:44:35 tls Exp $ */

/*-
 *
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Peter Bex <Peter.Bex@student.kun.nl>.
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
 * TODO:
 * - Most importantly, get some bus_dmamap_syncs in the correct places.
 *    I don't have access to a computer with PCI other than i386, and i386
 *    is just such a machine where dmamap_syncs don't do anything.
 * - Powerhook for when resuming after standby.
 * - Watchdog stuff doesn't work yet, the system crashes.
 * - There seems to be a CardBus version of the card. (see datasheet)
 *    Perhaps a detach function is necessary then? (free buffs, stop rx/tx etc)
 * - When you enable the TXBUN (Tx buffer unavailable) interrupt, it gets
 *    raised every time a packet is sent. Strange, since everything works anyway
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mtd803.c,v 1.29 2014/08/10 16:44:35 tls Exp $");


#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/bus.h>

#include <dev/ic/mtd803reg.h>
#include <dev/ic/mtd803var.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Device driver for the MTD803 3-in-1 Fast Ethernet Controller
 * Written by Peter Bex (peter.bex@student.kun.nl)
 *
 * Datasheet at:   http://www.myson.com.tw   or   http://www.century-semi.com
 */

#define MTD_READ_1(sc, reg) \
	bus_space_read_1((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_1(sc, reg, data) \
	bus_space_write_1((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_READ_2(sc, reg) \
	bus_space_read_2((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_2(sc, reg, data) \
	bus_space_write_2((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_READ_4(sc, reg) \
	bus_space_read_4((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_4(sc, reg, data) \
	bus_space_write_4((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_SETBIT(sc, reg, x) \
	MTD_WRITE_4((sc), (reg), MTD_READ_4((sc), (reg)) | (x))
#define MTD_CLRBIT(sc, reg, x) \
	MTD_WRITE_4((sc), (reg), MTD_READ_4((sc), (reg)) & ~(x))

#define ETHER_CRC32(buf, len)	(ether_crc32_be((buf), (len)))

int mtd_mii_readreg(device_t, int, int);
void mtd_mii_writereg(device_t, int, int, int);
void mtd_mii_statchg(struct ifnet *);

void mtd_start(struct ifnet *);
void mtd_stop(struct ifnet *, int);
int mtd_ioctl(struct ifnet *, u_long, void *);
void mtd_setmulti(struct mtd_softc *);
void mtd_watchdog(struct ifnet *);

int mtd_init(struct ifnet *);
void mtd_reset(struct mtd_softc *);
void mtd_shutdown(void *);
int mtd_init_desc(struct mtd_softc *);
int mtd_put(struct mtd_softc *, int, struct mbuf *);
struct mbuf *mtd_get(struct mtd_softc *, int, int);

int mtd_rxirq(struct mtd_softc *);
int mtd_txirq(struct mtd_softc *);
int mtd_bufirq(struct mtd_softc *);


int
mtd_config(struct mtd_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int i;

	/* Read station address */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->eaddr[i] = MTD_READ_1(sc, MTD_PAR0 + i);

	/* Initialize ifnet structure */
	memcpy(ifp->if_xname, device_xname(sc->dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_init = mtd_init;
	ifp->if_start = mtd_start;
	ifp->if_stop = mtd_stop;
	ifp->if_ioctl = mtd_ioctl;
	ifp->if_watchdog = mtd_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Setup MII interface */
	sc->mii.mii_ifp = ifp;
	sc->mii.mii_readreg = mtd_mii_readreg;
	sc->mii.mii_writereg = mtd_mii_writereg;
	sc->mii.mii_statchg = mtd_mii_statchg;

	sc->ethercom.ec_mii = &sc->mii;
	ifmedia_init(&sc->mii.mii_media, 0, ether_mediachange,
	    ether_mediastatus);

	mii_attach(sc->dev, &sc->mii, 0xffffffff, MII_PHY_ANY, 0, 0);

	if (LIST_FIRST(&sc->mii.mii_phys) == NULL) {
		aprint_error_dev(sc->dev, "Unable to configure MII\n");
		return 1;
	} else {
		ifmedia_set(&sc->mii.mii_media, IFM_ETHER | IFM_AUTO);
	}

	if (mtd_init_desc(sc))
		return 1;

	/* Attach interface */
	if_attach(ifp);
	ether_ifattach(ifp, sc->eaddr);

	/* Initialise random source */
	rnd_attach_source(&sc->rnd_src, device_xname(sc->dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	/* Add shutdown hook to reset card when we reboot */
	sc->sd_hook = shutdownhook_establish(mtd_shutdown, sc);

	return 0;
}


/*
 * mtd_init
 * Must be called at splnet()
 */
int
mtd_init(struct ifnet *ifp)
{
	struct mtd_softc *sc = ifp->if_softc;

	mtd_reset(sc);

	/*
	 * Set cache alignment and burst length. Don't really know what these
	 * mean, so their values are probably suboptimal.
	 */
	MTD_WRITE_4(sc, MTD_BCR, MTD_BCR_BLEN16);

	MTD_WRITE_4(sc, MTD_RXTXR, MTD_TX_STFWD | MTD_TX_FDPLX);

	/* Promiscuous mode? */
	if (ifp->if_flags & IFF_PROMISC)
		MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_PROM);
	else
		MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_PROM);

	/* Broadcast mode? */
	if (ifp->if_flags & IFF_BROADCAST)
		MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_ABROAD);
	else
		MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_ABROAD);

	mtd_setmulti(sc);

	/* Enable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, MTD_IMR_MASK);
	MTD_WRITE_4(sc, MTD_ISR, MTD_ISR_ENABLE);

	/* Set descriptor base addresses */
	MTD_WRITE_4(sc, MTD_TXLBA, htole32(sc->desc_dma_map->dm_segs[0].ds_addr
				+ sizeof(struct mtd_desc) * MTD_NUM_RXD));
	MTD_WRITE_4(sc, MTD_RXLBA,
		htole32(sc->desc_dma_map->dm_segs[0].ds_addr));

	/* Enable receiver and transmitter */
	MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_ENABLE);
	MTD_SETBIT(sc, MTD_RXTXR, MTD_TX_ENABLE);

	/* Interface is running */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}


int
mtd_init_desc(struct mtd_softc *sc)
{
	int rseg, err, i;
	bus_dma_segment_t seg;
	bus_size_t size;

	/* Allocate memory for descriptors */
	size = (MTD_NUM_RXD + MTD_NUM_TXD) * sizeof(struct mtd_desc);

	/* Allocate DMA-safe memory */
	if ((err = bus_dmamem_alloc(sc->dma_tag, size, MTD_DMA_ALIGN,
			 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->dev, "unable to allocate DMA buffer, error = %d\n", err);
		return 1;
	}

	/* Map memory to kernel addressable space */
	if ((err = bus_dmamem_map(sc->dma_tag, &seg, 1, size,
		(void **)&sc->desc, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->dev, "unable to map DMA buffer, error = %d\n", err);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Create a DMA map */
	if ((err = bus_dmamap_create(sc->dma_tag, size, 1,
		size, 0, BUS_DMA_NOWAIT, &sc->desc_dma_map)) != 0) {
		aprint_error_dev(sc->dev, "unable to create DMA map, error = %d\n", err);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Load the DMA map */
	if ((err = bus_dmamap_load(sc->dma_tag, sc->desc_dma_map, sc->desc,
		size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->dev, "unable to load DMA map, error = %d\n",
			err);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Allocate memory for the buffers */
	size = MTD_NUM_RXD * MTD_RXBUF_SIZE + MTD_NUM_TXD * MTD_TXBUF_SIZE;

	/* Allocate DMA-safe memory */
	if ((err = bus_dmamem_alloc(sc->dma_tag, size, MTD_DMA_ALIGN,
			 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->dev, "unable to allocate DMA buffer, error = %d\n",
			err);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Map memory to kernel addressable space */
	if ((err = bus_dmamem_map(sc->dma_tag, &seg, 1, size,
		&sc->buf, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->dev, "unable to map DMA buffer, error = %d\n",
			err);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Create a DMA map */
	if ((err = bus_dmamap_create(sc->dma_tag, size, 1,
		size, 0, BUS_DMA_NOWAIT, &sc->buf_dma_map)) != 0) {
		aprint_error_dev(sc->dev, "unable to create DMA map, error = %d\n",
			err);
		bus_dmamem_unmap(sc->dma_tag, sc->buf, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Load the DMA map */
	if ((err = bus_dmamap_load(sc->dma_tag, sc->buf_dma_map, sc->buf,
		size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->dev, "unable to load DMA map, error = %d\n",
			err);
		bus_dmamap_destroy(sc->dma_tag, sc->buf_dma_map);
		bus_dmamem_unmap(sc->dma_tag, sc->buf, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (void *)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Descriptors are stored as a circular linked list */
	/* Fill in rx descriptors */
	for (i = 0; i < MTD_NUM_RXD; ++i) {
		sc->desc[i].stat = MTD_RXD_OWNER;
		if (i == MTD_NUM_RXD - 1) {	/* Last descriptor */
			/* Link back to first rx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr);
		} else {
			/* Link forward to next rx descriptor */
			sc->desc[i].next =
			htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+ (i + 1) * sizeof(struct mtd_desc));
		}
		sc->desc[i].conf = MTD_RXBUF_SIZE & MTD_RXD_CONF_BUFS;
		/* Set buffer's address */
		sc->desc[i].data = htole32(sc->buf_dma_map->dm_segs[0].ds_addr
					+ i * MTD_RXBUF_SIZE);
	}

	/* Fill in tx descriptors */
	for (/* i = MTD_NUM_RXD */; i < (MTD_NUM_TXD + MTD_NUM_RXD); ++i) {
		sc->desc[i].stat = 0;	/* At least, NOT MTD_TXD_OWNER! */
		if (i == (MTD_NUM_RXD + MTD_NUM_TXD - 1)) {	/* Last descr */
			/* Link back to first tx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+MTD_NUM_RXD * sizeof(struct mtd_desc));
		} else {
			/* Link forward to next tx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+ (i + 1) * sizeof(struct mtd_desc));
		}
		/* sc->desc[i].conf = MTD_TXBUF_SIZE & MTD_TXD_CONF_BUFS; */
		/* Set buffer's address */
		sc->desc[i].data = htole32(sc->buf_dma_map->dm_segs[0].ds_addr
					+ MTD_NUM_RXD * MTD_RXBUF_SIZE
					+ (i - MTD_NUM_RXD) * MTD_TXBUF_SIZE);
	}

	return 0;
}


void
mtd_mii_statchg(struct ifnet *ifp)
{
	/* Should we do something here? :) */
}


int
mtd_mii_readreg(device_t self, int phy, int reg)
{
	struct mtd_softc *sc = device_private(self);

	return (MTD_READ_2(sc, MTD_PHYBASE + reg * 2));
}


void
mtd_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct mtd_softc *sc = device_private(self);

	MTD_WRITE_2(sc, MTD_PHYBASE + reg * 2, val);
}


int
mtd_put(struct mtd_softc *sc, int index, struct mbuf *m)
{
	int len, tlen;
	char *buf = (char *)sc->buf + MTD_NUM_RXD * MTD_RXBUF_SIZE
			+ index * MTD_TXBUF_SIZE;
	struct mbuf *n;

	for (tlen = 0; m != NULL; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		} else if (tlen > MTD_TXBUF_SIZE) {
			/* XXX FIXME: No idea what to do here. */
			aprint_error_dev(sc->dev, "packet too large! Size = %i\n",
				tlen);
			MFREE(m, n);
			continue;
		}
		memcpy(buf, mtod(m, void *), len);
		buf += len;
		tlen += len;
		MFREE(m, n);
	}
	sc->desc[MTD_NUM_RXD + index].conf = MTD_TXD_CONF_PAD | MTD_TXD_CONF_CRC
		| MTD_TXD_CONF_IRQC
		| ((tlen << MTD_TXD_PKTS_SHIFT) & MTD_TXD_CONF_PKTS)
		| (tlen & MTD_TXD_CONF_BUFS);

	return tlen;
}


void
mtd_start(struct ifnet *ifp)
{
	struct mtd_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int first_tx = sc->cur_tx;

	/* Don't transmit when the interface is busy or inactive */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			break;

		bpf_mtap(ifp, m);

		/* Copy mbuf chain into tx buffer */
		(void)mtd_put(sc, sc->cur_tx, m);

		if (sc->cur_tx != first_tx)
			sc->desc[MTD_NUM_RXD + sc->cur_tx].stat = MTD_TXD_OWNER;

		if (++sc->cur_tx >= MTD_NUM_TXD)
			sc->cur_tx = 0;
	}
	/* Mark first & last descriptor */
	sc->desc[MTD_NUM_RXD + first_tx].conf |= MTD_TXD_CONF_FSD;

	if (sc->cur_tx == 0) {
		sc->desc[MTD_NUM_RXD + MTD_NUM_TXD - 1].conf |=MTD_TXD_CONF_LSD;
	} else {
		sc->desc[MTD_NUM_RXD + sc->cur_tx - 1].conf |= MTD_TXD_CONF_LSD;
	}

	/* Give first descriptor to chip to complete transaction */
	sc->desc[MTD_NUM_RXD + first_tx].stat = MTD_TXD_OWNER;

	/* Transmit polling demand */
	MTD_WRITE_4(sc, MTD_TXPDR, MTD_TXPDR_DEMAND);

	/* XXX FIXME: Set up a watchdog timer */
	/* ifp->if_timer = 5; */
}


void
mtd_stop(struct ifnet *ifp, int disable)
{
	struct mtd_softc *sc = ifp->if_softc;

	/* Disable transmitter and receiver */
	MTD_CLRBIT(sc, MTD_RXTXR, MTD_TX_ENABLE);
	MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_ENABLE);

	/* Disable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, 0x00000000);

	/* Must do more at disable??... */
	if (disable) {
		/* Delete tx and rx descriptor base addresses */
		MTD_WRITE_4(sc, MTD_RXLBA, 0x00000000);
		MTD_WRITE_4(sc, MTD_TXLBA, 0x00000000);
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}


void
mtd_watchdog(struct ifnet *ifp)
{
	struct mtd_softc *sc = ifp->if_softc;
	int s;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->dev));
	++sc->ethercom.ec_if.if_oerrors;

	mtd_stop(ifp, 0);

	s = splnet();
	mtd_init(ifp);
	splx(s);

	return;
}


int
mtd_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct mtd_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		/*
		 * Multicast list has changed; set the hardware
		 * filter accordingly.
		 */
		 if (ifp->if_flags & IFF_RUNNING)
			 mtd_setmulti(sc);
		 error = 0;
	}

	splx(s);
	return error;
}


struct mbuf *
mtd_get(struct mtd_softc *sc, int index, int totlen)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	struct mbuf *m, *m0, *newm;
	int len;
	char *buf = (char *)sc->buf + index * MTD_RXBUF_SIZE;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return NULL;

	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	m = m0;
	len = MHLEN;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (!(m->m_flags & M_EXT)) {
				m_freem(m0);
				return NULL;
			}
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
		memcpy(mtod(m, void *), buf, len);
		buf += len;

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == NULL) {
				m_freem(m0);
				return NULL;
			}
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return m0;
}


int
mtd_rxirq(struct mtd_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int len;
	struct mbuf *m;

	for (; !(sc->desc[sc->cur_rx].stat & MTD_RXD_OWNER);) {
		/* Error summary set? */
		if (sc->desc[sc->cur_rx].stat & MTD_RXD_ERRSUM) {
			aprint_error_dev(sc->dev, "received packet with errors\n");
			/* Give up packet, since an error occurred */
			sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;
			sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE &
							MTD_RXD_CONF_BUFS;
			++ifp->if_ierrors;
			if (++sc->cur_rx >= MTD_NUM_RXD)
				sc->cur_rx = 0;
			continue;
		}
		/* Get buffer length */
		len = (sc->desc[sc->cur_rx].stat & MTD_RXD_FLEN)
			>> MTD_RXD_FLEN_SHIFT;
		len -= ETHER_CRC_LEN;

		/* Check packet size */
		if (len <= sizeof(struct ether_header)) {
			aprint_error_dev(sc->dev, "invalid packet size %d; dropping\n",
				len);
			sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;
			sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE &
							MTD_RXD_CONF_BUFS;
			++ifp->if_ierrors;
			if (++sc->cur_rx >= MTD_NUM_RXD)
				sc->cur_rx = 0;
			continue;
		}

		m = mtd_get(sc, (sc->cur_rx), len);

		/* Give descriptor back to card */
		sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE & MTD_RXD_CONF_BUFS;
		sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;

		if (++sc->cur_rx >= MTD_NUM_RXD)
			sc->cur_rx = 0;

		if (m == NULL) {
			aprint_error_dev(sc->dev, "error pulling packet off interface\n");
			++ifp->if_ierrors;
			continue;
		}

		++ifp->if_ipackets;

		bpf_mtap(ifp, m);
		/* Pass the packet up */
		(*ifp->if_input)(ifp, m);
	}

	return 1;
}


int
mtd_txirq(struct mtd_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;

	/* Clear timeout */
	ifp->if_timer = 0;

	ifp->if_flags &= ~IFF_OACTIVE;
	++ifp->if_opackets;

	/* XXX FIXME If there is some queued, do an mtd_start? */

	return 1;
}


int
mtd_bufirq(struct mtd_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;

	/* Clear timeout */
	ifp->if_timer = 0;

	/* XXX FIXME: Do something here to make sure we get some buffers! */

	return 1;
}


int
mtd_irq_h(void *args)
{
	struct mtd_softc *sc = args;
	struct ifnet *ifp = &sc->ethercom.ec_if;
	u_int32_t status;
	int r = 0;

	if (!(ifp->if_flags & IFF_RUNNING) || !device_is_active(sc->dev))
		return 0;

	/* Disable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, 0x00000000);

	for(;;) {
		status = MTD_READ_4(sc, MTD_ISR);

		/* Add random seed before masking out bits */
		if (status)
			rnd_add_uint32(&sc->rnd_src, status);

		status &= MTD_ISR_MASK;
		if (!status)		/* We didn't ask for this */
			break;

		MTD_WRITE_4(sc, MTD_ISR, status);

		/* NOTE: Perhaps we should reset with some of these errors? */

		if (status & MTD_ISR_RXBUN) {
			aprint_error_dev(sc->dev, "receive buffer unavailable\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_RXERR) {
			aprint_error_dev(sc->dev, "receive error\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_TXBUN) {
			aprint_error_dev(sc->dev, "transmit buffer unavailable\n");
			++ifp->if_ierrors;
		}

		if ((status & MTD_ISR_PDF)) {
			aprint_error_dev(sc->dev, "parallel detection fault\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_FBUSERR) {
			aprint_error_dev(sc->dev, "fatal bus error\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_TARERR) {
			aprint_error_dev(sc->dev, "target error\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_MASTERR) {
			aprint_error_dev(sc->dev, "master error\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_PARERR) {
			aprint_error_dev(sc->dev, "parity error\n");
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_RXIRQ)	/* Receive interrupt */
			r |= mtd_rxirq(sc);

		if (status & MTD_ISR_TXIRQ)	/* Transmit interrupt */
			r |= mtd_txirq(sc);

		if (status & MTD_ISR_TXEARLY)	/* Transmit early */
			r |= mtd_txirq(sc);

		if (status & MTD_ISR_TXBUN)	/* Transmit buffer n/a */
			r |= mtd_bufirq(sc);

	}

	/* Enable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, MTD_IMR_MASK);

	return r;
}


void
mtd_setmulti(struct mtd_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	u_int32_t rxtx_stat;
	u_int32_t hash[2] = {0, 0};
	u_int32_t crc;
	struct ether_multi *enm;
	struct ether_multistep step;
	int mcnt = 0;

	/* Get old status */
	rxtx_stat = MTD_READ_4(sc, MTD_RXTXR);

	if ((ifp->if_flags & IFF_ALLMULTI) || (ifp->if_flags & IFF_PROMISC)) {
		rxtx_stat |= MTD_RX_AMULTI;
		MTD_WRITE_4(sc, MTD_RXTXR, rxtx_stat);
		MTD_WRITE_4(sc, MTD_MAR0, MTD_ALL_ADDR);
		MTD_WRITE_4(sc, MTD_MAR1, MTD_ALL_ADDR);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->ethercom, enm);
	while (enm != NULL) {
		/* We need the 6 most significant bits of the CRC */
		crc = ETHER_CRC32(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;

		hash[crc >> 5] |= 1 << (crc & 0xf);

		++mcnt;
		ETHER_NEXT_MULTI(step, enm);
	}

	/* Accept multicast bit needs to be on? */
	if (mcnt)
		rxtx_stat |= MTD_RX_AMULTI;
	else
		rxtx_stat &= ~MTD_RX_AMULTI;

	/* Write out the hash */
	MTD_WRITE_4(sc, MTD_MAR0, hash[0]);
	MTD_WRITE_4(sc, MTD_MAR1, hash[1]);
	MTD_WRITE_4(sc, MTD_RXTXR, rxtx_stat);
}


void
mtd_reset(struct mtd_softc *sc)
{
	int i;

	MTD_SETBIT(sc, MTD_BCR, MTD_BCR_RESET);

	/* Reset descriptor status */
	sc->cur_tx = 0;
	sc->cur_rx = 0;

	/* Wait until done with reset */
	for (i = 0; i < MTD_TIMEOUT; ++i) {
		DELAY(10);
		if (!(MTD_READ_4(sc, MTD_BCR) & MTD_BCR_RESET))
			break;
	}

	if (i == MTD_TIMEOUT) {
		aprint_error_dev(sc->dev, "reset timed out\n");
	}

	/* Wait a little so chip can stabilize */
	DELAY(1000);
}


void
mtd_shutdown (void *arg)
{
	struct mtd_softc *sc = arg;
	struct ifnet *ifp = &sc->ethercom.ec_if;

	rnd_detach_source(&sc->rnd_src);
	mtd_stop(ifp, 1);
}
