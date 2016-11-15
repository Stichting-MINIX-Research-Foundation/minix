/*	$NetBSD: if_dge.c,v 1.40 2015/04/13 16:33:25 riastradh Exp $ */

/*
 * Copyright (c) 2004, SUNET, Swedish University Computer Network.
 * All rights reserved.
 *
 * Written by Anders Magnusson for SUNET, Swedish University Computer Network.
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
 *	SUNET, Swedish University Computer Network.
 * 4. The name of SUNET may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SUNET ``AS IS'' AND
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
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
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
 * Device driver for the Intel 82597EX Ten Gigabit Ethernet controller.
 *
 * TODO (in no specific order):
 *	HW VLAN support.
 *	TSE offloading (needs kernel changes...)
 *	RAIDC (receive interrupt delay adaptation)
 *	Use memory > 4GB.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_dge.c,v 1.40 2015/04/13 16:33:25 riastradh Exp $");



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

#include <netinet/in.h>			/* XXX for struct ip */
#include <netinet/in_systm.h>		/* XXX for struct ip */
#include <netinet/ip.h>			/* XXX for struct ip */
#include <netinet/tcp.h>		/* XXX for struct tcphdr */

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_dgereg.h>

/*
 * The receive engine may sometimes become off-by-one when writing back
 * chained descriptors.	 Avoid this by allocating a large chunk of
 * memory and use if instead (to avoid chained descriptors).
 * This only happens with chained descriptors under heavy load.
 */
#define DGE_OFFBYONE_RXBUG

#define DGE_EVENT_COUNTERS
#define DGE_DEBUG

#ifdef DGE_DEBUG
#define DGE_DEBUG_LINK		0x01
#define DGE_DEBUG_TX		0x02
#define DGE_DEBUG_RX		0x04
#define DGE_DEBUG_CKSUM		0x08
int	dge_debug = 0;

#define DPRINTF(x, y)	if (dge_debug & (x)) printf y
#else
#define DPRINTF(x, y)	/* nothing */
#endif /* DGE_DEBUG */

/*
 * Transmit descriptor list size. We allow up to 100 DMA segments per
 * packet (Intel reports of jumbo frame packets with as
 * many as 80 DMA segments when using 16k buffers).
 */
#define DGE_NTXSEGS		100
#define DGE_IFQUEUELEN		20000
#define DGE_TXQUEUELEN		2048
#define DGE_TXQUEUELEN_MASK	(DGE_TXQUEUELEN - 1)
#define DGE_TXQUEUE_GC		(DGE_TXQUEUELEN / 8)
#define DGE_NTXDESC		1024
#define DGE_NTXDESC_MASK		(DGE_NTXDESC - 1)
#define DGE_NEXTTX(x)		(((x) + 1) & DGE_NTXDESC_MASK)
#define DGE_NEXTTXS(x)		(((x) + 1) & DGE_TXQUEUELEN_MASK)

/*
 * Receive descriptor list size.
 * Packet is of size MCLBYTES, and for jumbo packets buffers may
 * be chained.	Due to the nature of the card (high-speed), keep this
 * ring large. With 2k buffers the ring can store 400 jumbo packets,
 * which at full speed will be received in just under 3ms.
 */
#define DGE_NRXDESC		2048
#define DGE_NRXDESC_MASK	(DGE_NRXDESC - 1)
#define DGE_NEXTRX(x)		(((x) + 1) & DGE_NRXDESC_MASK)
/*
 * # of descriptors between head and written descriptors.
 * This is to work-around two erratas.
 */
#define DGE_RXSPACE		10
#define DGE_PREVRX(x)		(((x) - DGE_RXSPACE) & DGE_NRXDESC_MASK)
/*
 * Receive descriptor fetch threshholds. These are values recommended
 * by Intel, do not touch them unless you know what you are doing.
 */
#define RXDCTL_PTHRESH_VAL	128
#define RXDCTL_HTHRESH_VAL	16
#define RXDCTL_WTHRESH_VAL	16


/*
 * Tweakable parameters; default values.
 */
#define FCRTH	0x30000 /* Send XOFF water mark */
#define FCRTL	0x28000 /* Send XON water mark */
#define RDTR	0x20	/* Interrupt delay after receive, .8192us units */
#define TIDV	0x20	/* Interrupt delay after send, .8192us units */

/*
 * Control structures are DMA'd to the i82597 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make serveral things
 * easier.
 */
struct dge_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct dge_tdes wcd_txdescs[DGE_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct dge_rdes wcd_rxdescs[DGE_NRXDESC];
};

#define DGE_CDOFF(x)	offsetof(struct dge_control_data, x)
#define DGE_CDTXOFF(x)	DGE_CDOFF(wcd_txdescs[(x)])
#define DGE_CDRXOFF(x)	DGE_CDOFF(wcd_rxdescs[(x)])

/*
 * The DGE interface have a higher max MTU size than normal jumbo frames.
 */
#define DGE_MAX_MTU	16288	/* Max MTU size for this interface */

/*
 * Software state for transmit jobs.
 */
struct dge_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndesc;			/* # of descriptors used */
};

/*
 * Software state for receive buffers.	Each descriptor gets a
 * 2k (MCLBYTES) buffer and a DMA map.	For packets which fill
 * more than one buffer, we chain them together.
 */
struct dge_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

/*
 * Software state per device.
 */
struct dge_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */

	int sc_flags;			/* flags; see below */
	int sc_bus_speed;		/* PCI/PCIX bus speed */
	int sc_pcix_offset;		/* PCIX capability register offset */

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pt;
	int sc_mmrbc;			/* Max PCIX memory read byte count */

	void *sc_ih;			/* interrupt cookie */

	struct ifmedia sc_media;

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	int		sc_align_tweak;

	/*
	 * Software state for the transmit and receive descriptors.
	 */
	struct dge_txsoft sc_txsoft[DGE_TXQUEUELEN];
	struct dge_rxsoft sc_rxsoft[DGE_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct dge_control_data *sc_control_data;
#define sc_txdescs	sc_control_data->wcd_txdescs
#define sc_rxdescs	sc_control_data->wcd_rxdescs

#ifdef DGE_EVENT_COUNTERS
	/* Event counters. */
	struct evcnt sc_ev_txsstall;	/* Tx stalled due to no txs */
	struct evcnt sc_ev_txdstall;	/* Tx stalled due to no txd */
	struct evcnt sc_ev_txforceintr; /* Tx interrupts forced */
	struct evcnt sc_ev_txdw;	/* Tx descriptor interrupts */
	struct evcnt sc_ev_txqe;	/* Tx queue empty interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_linkintr;	/* Link interrupts */

	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtusum;	/* TCP/UDP cksums checked in-bound */
	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtusum;	/* TCP/UDP cksums comp. out-bound */

	struct evcnt sc_ev_txctx_init;	/* Tx cksum context cache initialized */
	struct evcnt sc_ev_txctx_hit;	/* Tx cksum context cache hit */
	struct evcnt sc_ev_txctx_miss;	/* Tx cksum context cache miss */

	struct evcnt sc_ev_txseg[DGE_NTXSEGS]; /* Tx packets w/ N segments */
	struct evcnt sc_ev_txdrop;	/* Tx packets dropped (too many segs) */
#endif /* DGE_EVENT_COUNTERS */

	int	sc_txfree;		/* number of free Tx descriptors */
	int	sc_txnext;		/* next ready Tx descriptor */

	int	sc_txsfree;		/* number of free Tx jobs */
	int	sc_txsnext;		/* next free Tx job */
	int	sc_txsdirty;		/* dirty Tx jobs */

	uint32_t sc_txctx_ipcs;		/* cached Tx IP cksum ctx */
	uint32_t sc_txctx_tucs;		/* cached Tx TCP/UDP cksum ctx */

	int	sc_rxptr;		/* next ready Rx descriptor/queue ent */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	uint32_t sc_ctrl0;		/* prototype CTRL0 register */
	uint32_t sc_icr;		/* prototype interrupt bits */
	uint32_t sc_tctl;		/* prototype TCTL register */
	uint32_t sc_rctl;		/* prototype RCTL register */

	int sc_mchash_type;		/* multicast filter offset */

	uint16_t sc_eeprom[EEPROM_SIZE];

	krndsource_t rnd_source; /* random source */
#ifdef DGE_OFFBYONE_RXBUG
	void *sc_bugbuf;
	SLIST_HEAD(, rxbugentry) sc_buglist;
	bus_dmamap_t sc_bugmap;
	struct rxbugentry *sc_entry;
#endif
};

#define DGE_RXCHAIN_RESET(sc)						\
do {									\
	(sc)->sc_rxtailp = &(sc)->sc_rxhead;				\
	*(sc)->sc_rxtailp = NULL;					\
	(sc)->sc_rxlen = 0;						\
} while (/*CONSTCOND*/0)

#define DGE_RXCHAIN_LINK(sc, m)						\
do {									\
	*(sc)->sc_rxtailp = (sc)->sc_rxtail = (m);			\
	(sc)->sc_rxtailp = &(m)->m_next;				\
} while (/*CONSTCOND*/0)

/* sc_flags */
#define DGE_F_BUS64		0x20	/* bus is 64-bit */
#define DGE_F_PCIX		0x40	/* bus is PCI-X */

#ifdef DGE_EVENT_COUNTERS
#define DGE_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define DGE_EVCNT_INCR(ev)	/* nothing */
#endif

#define CSR_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define CSR_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define DGE_CDTXADDR(sc, x)	((sc)->sc_cddma + DGE_CDTXOFF((x)))
#define DGE_CDRXADDR(sc, x)	((sc)->sc_cddma + DGE_CDRXOFF((x)))

#define DGE_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > DGE_NTXDESC) {				\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    DGE_CDTXOFF(__x), sizeof(struct dge_tdes) *		\
		    (DGE_NTXDESC - __x), (ops));			\
		__n -= (DGE_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    DGE_CDTXOFF(__x), sizeof(struct dge_tdes) * __n, (ops));	\
} while (/*CONSTCOND*/0)

#define DGE_CDRXSYNC(sc, x, ops)						\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	   DGE_CDRXOFF((x)), sizeof(struct dge_rdes), (ops));		\
} while (/*CONSTCOND*/0)

#ifdef DGE_OFFBYONE_RXBUG
#define DGE_INIT_RXDESC(sc, x)						\
do {									\
	struct dge_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	struct dge_rdes *__rxd = &(sc)->sc_rxdescs[(x)];		\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	__rxd->dr_baddrl = htole32(sc->sc_bugmap->dm_segs[0].ds_addr +	\
	    (mtod((__m), char *) - (char *)sc->sc_bugbuf));		\
	__rxd->dr_baddrh = 0;						\
	__rxd->dr_len = 0;						\
	__rxd->dr_cksum = 0;						\
	__rxd->dr_status = 0;						\
	__rxd->dr_errors = 0;						\
	__rxd->dr_special = 0;						\
	DGE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
									\
	CSR_WRITE((sc), DGE_RDT, (x));					\
} while (/*CONSTCOND*/0)
#else
#define DGE_INIT_RXDESC(sc, x)						\
do {									\
	struct dge_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	struct dge_rdes *__rxd = &(sc)->sc_rxdescs[(x)];		\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 *								\
	 * XXX BRAINDAMAGE ALERT!					\
	 * The stupid chip uses the same size for every buffer, which	\
	 * is set in the Receive Control register.  We are using the 2K \
	 * size option, but what we REALLY want is (2K - 2)!  For this	\
	 * reason, we can't "scoot" packets longer than the standard	\
	 * Ethernet MTU.  On strict-alignment platforms, if the total	\
	 * size exceeds (2K - 2) we set align_tweak to 0 and let	\
	 * the upper layer copy the headers.				\
	 */								\
	__m->m_data = __m->m_ext.ext_buf + (sc)->sc_align_tweak;	\
									\
	__rxd->dr_baddrl =						\
	    htole32(__rxs->rxs_dmamap->dm_segs[0].ds_addr +		\
		(sc)->sc_align_tweak);					\
	__rxd->dr_baddrh = 0;						\
	__rxd->dr_len = 0;						\
	__rxd->dr_cksum = 0;						\
	__rxd->dr_status = 0;						\
	__rxd->dr_errors = 0;						\
	__rxd->dr_special = 0;						\
	DGE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
									\
	CSR_WRITE((sc), DGE_RDT, (x));					\
} while (/*CONSTCOND*/0)
#endif

#ifdef DGE_OFFBYONE_RXBUG
/*
 * Allocation constants.  Much memory may be used for this.
 */
#ifndef DGE_BUFFER_SIZE
#define DGE_BUFFER_SIZE DGE_MAX_MTU
#endif
#define DGE_NBUFFERS	(4*DGE_NRXDESC)
#define DGE_RXMEM	(DGE_NBUFFERS*DGE_BUFFER_SIZE)

struct rxbugentry {
	SLIST_ENTRY(rxbugentry) rb_entry;
	int rb_slot;
};

static int
dge_alloc_rcvmem(struct dge_softc *sc)
{
	char *kva;
	bus_dma_segment_t seg;
	int i, rseg, state, error;
	struct rxbugentry *entry;

	state = error = 0;

	if (bus_dmamem_alloc(sc->sc_dmat, DGE_RXMEM, PAGE_SIZE, 0,
	     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't alloc rx buffers\n");
		return ENOBUFS;
	}

	state = 1;
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, DGE_RXMEM, (void **)&kva,
	    BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't map DMA buffers (%d bytes)\n",
		    (int)DGE_RXMEM);
		error = ENOBUFS;
		goto out;
	}

	state = 2;
	if (bus_dmamap_create(sc->sc_dmat, DGE_RXMEM, 1, DGE_RXMEM, 0,
	    BUS_DMA_NOWAIT, &sc->sc_bugmap)) {
		aprint_error_dev(sc->sc_dev, "can't create DMA map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 3;
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_bugmap,
	    kva, DGE_RXMEM, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't load DMA map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 4;
	sc->sc_bugbuf = (void *)kva;
	SLIST_INIT(&sc->sc_buglist);

	/*
	 * Now divide it up into DGE_BUFFER_SIZE pieces and save the addresses
	 * in an array.
	 */
	if ((entry = malloc(sizeof(*entry) * DGE_NBUFFERS,
	    M_DEVBUF, M_NOWAIT)) == NULL) {
		error = ENOBUFS;
		goto out;
	}
	sc->sc_entry = entry;
	for (i = 0; i < DGE_NBUFFERS; i++) {
		entry[i].rb_slot = i;
		SLIST_INSERT_HEAD(&sc->sc_buglist, &entry[i], rb_entry);
	}
out:
	if (error != 0) {
		switch (state) {
		case 4:
			bus_dmamap_unload(sc->sc_dmat, sc->sc_bugmap);
		case 3:
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_bugmap);
		case 2:
			bus_dmamem_unmap(sc->sc_dmat, kva, DGE_RXMEM);
		case 1:
			bus_dmamem_free(sc->sc_dmat, &seg, rseg);
			break;
		default:
			break;
		}
	}

	return error;
}

/*
 * Allocate a jumbo buffer.
 */
static void *
dge_getbuf(struct dge_softc *sc)
{
	struct rxbugentry *entry;

	entry = SLIST_FIRST(&sc->sc_buglist);

	if (entry == NULL) {
		printf("%s: no free RX buffers\n", device_xname(sc->sc_dev));
		return(NULL);
	}

	SLIST_REMOVE_HEAD(&sc->sc_buglist, rb_entry);
	return (char *)sc->sc_bugbuf + entry->rb_slot * DGE_BUFFER_SIZE;
}

/*
 * Release a jumbo buffer.
 */
static void
dge_freebuf(struct mbuf *m, void *buf, size_t size, void *arg)
{
	struct rxbugentry *entry;
	struct dge_softc *sc;
	int i, s;

	/* Extract the softc struct pointer. */
	sc = (struct dge_softc *)arg;

	if (sc == NULL)
		panic("dge_freebuf: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((char *)buf - (char *)sc->sc_bugbuf) / DGE_BUFFER_SIZE;

	if ((i < 0) || (i >= DGE_NBUFFERS))
		panic("dge_freebuf: asked to free buffer %d!", i);

	s = splvm();
	entry = sc->sc_entry + i;
	SLIST_INSERT_HEAD(&sc->sc_buglist, entry, rb_entry);

	if (__predict_true(m != NULL))
		pool_cache_put(mb_cache, m);
	splx(s);
}
#endif

static void	dge_start(struct ifnet *);
static void	dge_watchdog(struct ifnet *);
static int	dge_ioctl(struct ifnet *, u_long, void *);
static int	dge_init(struct ifnet *);
static void	dge_stop(struct ifnet *, int);

static bool	dge_shutdown(device_t, int);

static void	dge_reset(struct dge_softc *);
static void	dge_rxdrain(struct dge_softc *);
static int	dge_add_rxbuf(struct dge_softc *, int);

static void	dge_set_filter(struct dge_softc *);

static int	dge_intr(void *);
static void	dge_txintr(struct dge_softc *);
static void	dge_rxintr(struct dge_softc *);
static void	dge_linkintr(struct dge_softc *, uint32_t);

static int	dge_match(device_t, cfdata_t, void *);
static void	dge_attach(device_t, device_t, void *);

static int	dge_read_eeprom(struct dge_softc *sc);
static int	dge_eeprom_clockin(struct dge_softc *sc);
static void	dge_eeprom_clockout(struct dge_softc *sc, int bit);
static uint16_t	dge_eeprom_word(struct dge_softc *sc, int addr);
static int	dge_xgmii_mediachange(struct ifnet *);
static void	dge_xgmii_mediastatus(struct ifnet *, struct ifmediareq *);
static void	dge_xgmii_reset(struct dge_softc *);
static void	dge_xgmii_writereg(struct dge_softc *, int, int, int);


CFATTACH_DECL_NEW(dge, sizeof(struct dge_softc),
    dge_match, dge_attach, NULL, NULL);

#ifdef DGE_EVENT_COUNTERS
#if DGE_NTXSEGS > 100
#error Update dge_txseg_evcnt_names
#endif
static char (*dge_txseg_evcnt_names)[DGE_NTXSEGS][8 /* "txseg00" + \0 */];
#endif /* DGE_EVENT_COUNTERS */

static int
dge_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82597EX)
		return (1);

	return (0);
}

static void
dge_attach(device_t parent, device_t self, void *aux)
{
	struct dge_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_dma_segment_t seg;
	int i, rseg, error;
	uint8_t enaddr[ETHER_ADDR_LEN];
	pcireg_t preg, memtype;
	uint32_t reg;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pa->pa_pc;
	sc->sc_pt = pa->pa_tag;

	pci_aprint_devinfo_fancy(pa, "Ethernet controller",
		"Intel i82597EX 10GbE-LR Ethernet", 1);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, DGE_PCI_BAR);
        if (pci_mapreg_map(pa, DGE_PCI_BAR, memtype, 0,
            &sc->sc_st, &sc->sc_sh, NULL, NULL)) {
                aprint_error_dev(sc->sc_dev, "unable to map device registers\n");
                return;
        }

	/* Enable bus mastering */
	preg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	preg |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, preg);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, dge_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/*
	 * Determine a few things about the bus we're connected to.
	 */
	reg = CSR_READ(sc, DGE_STATUS);
	if (reg & STATUS_BUS64)
		sc->sc_flags |= DGE_F_BUS64;

	sc->sc_flags |= DGE_F_PCIX;
	if (pci_get_capability(pa->pa_pc, pa->pa_tag,
			       PCI_CAP_PCIX,
			       &sc->sc_pcix_offset, NULL) == 0)
		aprint_error_dev(sc->sc_dev, "unable to find PCIX "
		    "capability\n");

	if (sc->sc_flags & DGE_F_PCIX) {
		switch (reg & STATUS_PCIX_MSK) {
		case STATUS_PCIX_66:
			sc->sc_bus_speed = 66;
			break;
		case STATUS_PCIX_100:
			sc->sc_bus_speed = 100;
			break;
		case STATUS_PCIX_133:
			sc->sc_bus_speed = 133;
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown PCIXSPD %d; assuming 66MHz\n",
			    reg & STATUS_PCIX_MSK);
			sc->sc_bus_speed = 66;
		}
	} else
		sc->sc_bus_speed = (reg & STATUS_BUS64) ? 66 : 33;
	aprint_verbose_dev(sc->sc_dev, "%d-bit %dMHz %s bus\n",
	    (sc->sc_flags & DGE_F_BUS64) ? 64 : 32, sc->sc_bus_speed,
	    (sc->sc_flags & DGE_F_PCIX) ? "PCIX" : "PCI");

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct dge_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct dge_control_data), (void **)&sc->sc_control_data,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct dge_control_data), 1,
	    sizeof(struct dge_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct dge_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

#ifdef DGE_OFFBYONE_RXBUG
	if (dge_alloc_rcvmem(sc) != 0)
		return; /* Already complained */
#endif
	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < DGE_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, DGE_MAX_MTU,
		    DGE_NTXSEGS, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create Tx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < DGE_NRXDESC; i++) {
#ifdef DGE_OFFBYONE_RXBUG
		if ((error = bus_dmamap_create(sc->sc_dmat, DGE_BUFFER_SIZE, 1,
		    DGE_BUFFER_SIZE, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
#else
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
#endif
			aprint_error_dev(sc->sc_dev, "unable to create Rx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/*
	 * Set bits in ctrl0 register.
	 * Should get the software defined pins out of EEPROM?
	 */
	sc->sc_ctrl0 |= CTRL0_RPE | CTRL0_TPE; /* XON/XOFF */
	sc->sc_ctrl0 |= CTRL0_SDP3_DIR | CTRL0_SDP2_DIR | CTRL0_SDP1_DIR |
	    CTRL0_SDP0_DIR | CTRL0_SDP3 | CTRL0_SDP2 | CTRL0_SDP0;

	/*
	 * Reset the chip to a known state.
	 */
	dge_reset(sc);

	/*
	 * Reset the PHY.
	 */
	dge_xgmii_reset(sc);

	/*
	 * Read in EEPROM data.
	 */
	if (dge_read_eeprom(sc)) {
		aprint_error_dev(sc->sc_dev, "couldn't read EEPROM\n");
		return;
	}

	/*
	 * Get the ethernet address.
	 */
	enaddr[0] = sc->sc_eeprom[EE_ADDR01] & 0377;
	enaddr[1] = sc->sc_eeprom[EE_ADDR01] >> 8;
	enaddr[2] = sc->sc_eeprom[EE_ADDR23] & 0377;
	enaddr[3] = sc->sc_eeprom[EE_ADDR23] >> 8;
	enaddr[4] = sc->sc_eeprom[EE_ADDR45] & 0377;
	enaddr[5] = sc->sc_eeprom[EE_ADDR45] >> 8;

	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(enaddr));

	/*
	 * Setup media stuff.
	 */
        ifmedia_init(&sc->sc_media, IFM_IMASK, dge_xgmii_mediachange,
            dge_xgmii_mediastatus);
        ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_10G_LR, 0, NULL);
        ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10G_LR);

	ifp = &sc->sc_ethercom.ec_if;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = dge_ioctl;
	ifp->if_start = dge_start;
	ifp->if_watchdog = dge_watchdog;
	ifp->if_init = dge_init;
	ifp->if_stop = dge_stop;
	IFQ_SET_MAXLEN(&ifp->if_snd, max(DGE_IFQUEUELEN, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ethercom.ec_capabilities |=
	    ETHERCAP_JUMBO_MTU | ETHERCAP_VLAN_MTU;

	/*
	 * We can perform TCPv4 and UDPv4 checkums in-bound.
	 */
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

#ifdef DGE_EVENT_COUNTERS
	/* Fix segment event naming */
	if (dge_txseg_evcnt_names == NULL) {
		dge_txseg_evcnt_names =
		    malloc(sizeof(*dge_txseg_evcnt_names), M_DEVBUF, M_WAITOK);
		for (i = 0; i < DGE_NTXSEGS; i++)
			snprintf((*dge_txseg_evcnt_names)[i],
			    sizeof((*dge_txseg_evcnt_names)[i]), "txseg%d", i);
	}

	/* Attach event counters. */
	evcnt_attach_dynamic(&sc->sc_ev_txsstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txsstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txdstall");
	evcnt_attach_dynamic(&sc->sc_ev_txforceintr, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txforceintr");
	evcnt_attach_dynamic(&sc->sc_ev_txdw, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "txdw");
	evcnt_attach_dynamic(&sc->sc_ev_txqe, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "txqe");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "rxintr");
	evcnt_attach_dynamic(&sc->sc_ev_linkintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "linkintr");

	evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "rxipsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxtusum, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "rxtusum");
	evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txipsum");
	evcnt_attach_dynamic(&sc->sc_ev_txtusum, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txtusum");

	evcnt_attach_dynamic(&sc->sc_ev_txctx_init, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txctx init");
	evcnt_attach_dynamic(&sc->sc_ev_txctx_hit, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txctx hit");
	evcnt_attach_dynamic(&sc->sc_ev_txctx_miss, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txctx miss");

	for (i = 0; i < DGE_NTXSEGS; i++)
		evcnt_attach_dynamic(&sc->sc_ev_txseg[i], EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), (*dge_txseg_evcnt_names)[i]);

	evcnt_attach_dynamic(&sc->sc_ev_txdrop, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txdrop");

#endif /* DGE_EVENT_COUNTERS */

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	if (pmf_device_register1(self, NULL, NULL, dge_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < DGE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < DGE_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct dge_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * dge_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static bool
dge_shutdown(device_t self, int howto)
{
	struct dge_softc *sc;

	sc = device_private(self);
	dge_stop(&sc->sc_ethercom.ec_if, 1);

	return true;
}

/*
 * dge_tx_cksum:
 *
 *	Set up TCP/IP checksumming parameters for the
 *	specified packet.
 */
static int
dge_tx_cksum(struct dge_softc *sc, struct dge_txsoft *txs, uint8_t *fieldsp)
{
	struct mbuf *m0 = txs->txs_mbuf;
	struct dge_ctdes *t;
	uint32_t ipcs, tucs;
	struct ether_header *eh;
	int offset, iphl;
	uint8_t fields = 0;

	/*
	 * XXX It would be nice if the mbuf pkthdr had offset
	 * fields for the protocol headers.
	 */

	eh = mtod(m0, struct ether_header *);
	switch (htons(eh->ether_type)) {
	case ETHERTYPE_IP:
		offset = ETHER_HDR_LEN;
		break;

	case ETHERTYPE_VLAN:
		offset = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		break;

	default:
		/*
		 * Don't support this protocol or encapsulation.
		 */
		*fieldsp = 0;
		return (0);
	}

	iphl = M_CSUM_DATA_IPv4_IPHL(m0->m_pkthdr.csum_data);

	/*
	 * NOTE: Even if we're not using the IP or TCP/UDP checksum
	 * offload feature, if we load the context descriptor, we
	 * MUST provide valid values for IPCSS and TUCSS fields.
	 */

	if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4) {
		DGE_EVCNT_INCR(&sc->sc_ev_txipsum);
		fields |= TDESC_POPTS_IXSM;
		ipcs = DGE_TCPIP_IPCSS(offset) |
		    DGE_TCPIP_IPCSO(offset + offsetof(struct ip, ip_sum)) |
		    DGE_TCPIP_IPCSE(offset + iphl - 1);
	} else if (__predict_true(sc->sc_txctx_ipcs != 0xffffffff)) {
		/* Use the cached value. */
		ipcs = sc->sc_txctx_ipcs;
	} else {
		/* Just initialize it to the likely value anyway. */
		ipcs = DGE_TCPIP_IPCSS(offset) |
		    DGE_TCPIP_IPCSO(offset + offsetof(struct ip, ip_sum)) |
		    DGE_TCPIP_IPCSE(offset + iphl - 1);
	}
	DPRINTF(DGE_DEBUG_CKSUM,
	    ("%s: CKSUM: offset %d ipcs 0x%x\n",
	    device_xname(sc->sc_dev), offset, ipcs));

	offset += iphl;

	if (m0->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		DGE_EVCNT_INCR(&sc->sc_ev_txtusum);
		fields |= TDESC_POPTS_TXSM;
		tucs = DGE_TCPIP_TUCSS(offset) |
		   DGE_TCPIP_TUCSO(offset + M_CSUM_DATA_IPv4_OFFSET(m0->m_pkthdr.csum_data)) |
		   DGE_TCPIP_TUCSE(0) /* rest of packet */;
	} else if (__predict_true(sc->sc_txctx_tucs != 0xffffffff)) {
		/* Use the cached value. */
		tucs = sc->sc_txctx_tucs;
	} else {
		/* Just initialize it to a valid TCP context. */
		tucs = DGE_TCPIP_TUCSS(offset) |
		    DGE_TCPIP_TUCSO(offset + offsetof(struct tcphdr, th_sum)) |
		    DGE_TCPIP_TUCSE(0) /* rest of packet */;
	}

	DPRINTF(DGE_DEBUG_CKSUM,
	    ("%s: CKSUM: offset %d tucs 0x%x\n",
	    device_xname(sc->sc_dev), offset, tucs));

	if (sc->sc_txctx_ipcs == ipcs &&
	    sc->sc_txctx_tucs == tucs) {
		/* Cached context is fine. */
		DGE_EVCNT_INCR(&sc->sc_ev_txctx_hit);
	} else {
		/* Fill in the context descriptor. */
#ifdef DGE_EVENT_COUNTERS
		if (sc->sc_txctx_ipcs == 0xffffffff &&
		    sc->sc_txctx_tucs == 0xffffffff)
			DGE_EVCNT_INCR(&sc->sc_ev_txctx_init);
		else
			DGE_EVCNT_INCR(&sc->sc_ev_txctx_miss);
#endif
		t = (struct dge_ctdes *)&sc->sc_txdescs[sc->sc_txnext];
		t->dc_tcpip_ipcs = htole32(ipcs);
		t->dc_tcpip_tucs = htole32(tucs);
		t->dc_tcpip_cmdlen = htole32(TDESC_DTYP_CTD);
		t->dc_tcpip_seg = 0;
		DGE_CDTXSYNC(sc, sc->sc_txnext, 1, BUS_DMASYNC_PREWRITE);

		sc->sc_txctx_ipcs = ipcs;
		sc->sc_txctx_tucs = tucs;

		sc->sc_txnext = DGE_NEXTTX(sc->sc_txnext);
		txs->txs_ndesc++;
	}

	*fieldsp = fields;

	return (0);
}

/*
 * dge_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
dge_start(struct ifnet *ifp)
{
	struct dge_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct dge_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx = -1, ofree, seg;
	uint32_t cksumcmd;
	uint8_t cksumfields;

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
		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: have packet to transmit: %p\n",
		    device_xname(sc->sc_dev), m0));

		/* Get a work queue entry. */
		if (sc->sc_txsfree < DGE_TXQUEUE_GC) {
			dge_txintr(sc);
			if (sc->sc_txsfree == 0) {
				DPRINTF(DGE_DEBUG_TX,
				    ("%s: TX: no free job descriptors\n",
					device_xname(sc->sc_dev)));
				DGE_EVCNT_INCR(&sc->sc_ev_txsstall);
				break;
			}
		}

		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the allotted number of segments, or we
		 * were short on resources.  For the too-many-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				DGE_EVCNT_INCR(&sc->sc_ev_txdrop);
				printf("%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    device_xname(sc->sc_dev));
				IFQ_DEQUEUE(&ifp->if_snd, m0);
				m_freem(m0);
				continue;
			}
			/*
			 * Short on resources, just stop for now.
			 */
			DPRINTF(DGE_DEBUG_TX,
			    ("%s: TX: dmamap load failed: %d\n",
			    device_xname(sc->sc_dev), error));
			break;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring due to the semantics of the
		 * TDT register, plus one more in the event we need
		 * to re-load checksum offload context.
		 */
		if (dmamap->dm_nsegs > (sc->sc_txfree - 2)) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * pack on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 */
			DPRINTF(DGE_DEBUG_TX,
			    ("%s: TX: need %d descriptors, have %d\n",
			    device_xname(sc->sc_dev), dmamap->dm_nsegs,
			    sc->sc_txfree - 1));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			DGE_EVCNT_INCR(&sc->sc_ev_txdstall);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: packet has %d DMA segments\n",
		    device_xname(sc->sc_dev), dmamap->dm_nsegs));

		DGE_EVCNT_INCR(&sc->sc_ev_txseg[dmamap->dm_nsegs - 1]);

		/*
		 * Store a pointer to the packet so that we can free it
		 * later.
		 *
		 * Initially, we consider the number of descriptors the
		 * packet uses the number of DMA segments.  This may be
		 * incremented by 1 if we do checksum offload (a descriptor
		 * is used to set the checksum context).
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_ndesc = dmamap->dm_nsegs;

		/*
		 * Set up checksum offload parameters for
		 * this packet.
		 */
		if (m0->m_pkthdr.csum_flags &
		    (M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4)) {
			if (dge_tx_cksum(sc, txs, &cksumfields) != 0) {
				/* Error message already displayed. */
				bus_dmamap_unload(sc->sc_dmat, dmamap);
				continue;
			}
		} else {
			cksumfields = 0;
		}

		cksumcmd = TDESC_DCMD_IDE | TDESC_DTYP_DATA;

		/*
		 * Initialize the transmit descriptor.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = DGE_NEXTTX(nexttx)) {
			/*
			 * Note: we currently only use 32-bit DMA
			 * addresses.
			 */
			sc->sc_txdescs[nexttx].dt_baddrh = 0;
			sc->sc_txdescs[nexttx].dt_baddrl =
			    htole32(dmamap->dm_segs[seg].ds_addr);
			sc->sc_txdescs[nexttx].dt_ctl =
			    htole32(cksumcmd | dmamap->dm_segs[seg].ds_len);
			sc->sc_txdescs[nexttx].dt_status = 0;
			sc->sc_txdescs[nexttx].dt_popts = cksumfields;
			sc->sc_txdescs[nexttx].dt_vlan = 0;
			lasttx = nexttx;

			DPRINTF(DGE_DEBUG_TX,
			    ("%s: TX: desc %d: low 0x%08lx, len 0x%04lx\n",
			    device_xname(sc->sc_dev), nexttx,
			    (unsigned long)le32toh(dmamap->dm_segs[seg].ds_addr),
			    (unsigned long)le32toh(dmamap->dm_segs[seg].ds_len)));
		}

		KASSERT(lasttx != -1);

		/*
		 * Set up the command byte on the last descriptor of
		 * the packet.  If we're in the interrupt delay window,
		 * delay the interrupt.
		 */
		sc->sc_txdescs[lasttx].dt_ctl |=
		    htole32(TDESC_DCMD_EOP | TDESC_DCMD_RS);

		txs->txs_lastdesc = lasttx;

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: desc %d: cmdlen 0x%08x\n", device_xname(sc->sc_dev),
		    lasttx, le32toh(sc->sc_txdescs[lasttx].dt_ctl)));

		/* Sync the descriptors we're using. */
		DGE_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		CSR_WRITE(sc, DGE_TDT, nexttx);

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: TDT -> %d\n", device_xname(sc->sc_dev), nexttx));

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: finished transmitting packet, job %d\n",
		    device_xname(sc->sc_dev), sc->sc_txsnext));

		/* Advance the tx pointer. */
		sc->sc_txfree -= txs->txs_ndesc;
		sc->sc_txnext = nexttx;

		sc->sc_txsfree--;
		sc->sc_txsnext = DGE_NEXTTXS(sc->sc_txsnext);

		/* Pass the packet to any BPF listeners. */
		bpf_mtap(ifp, m0);
	}

	if (sc->sc_txsfree == 0 || sc->sc_txfree <= 2) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * dge_watchdog:		[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
dge_watchdog(struct ifnet *ifp)
{
	struct dge_softc *sc = ifp->if_softc;

	/*
	 * Since we're using delayed interrupts, sweep up
	 * before we report an error.
	 */
	dge_txintr(sc);

	if (sc->sc_txfree != DGE_NTXDESC) {
		printf("%s: device timeout (txfree %d txsfree %d txnext %d)\n",
		    device_xname(sc->sc_dev), sc->sc_txfree, sc->sc_txsfree,
		    sc->sc_txnext);
		ifp->if_oerrors++;

		/* Reset the interface. */
		(void) dge_init(ifp);
	}

	/* Try to get more packets going. */
	dge_start(ifp);
}

/*
 * dge_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
dge_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct dge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	pcireg_t preg;
	int s, error, mmrbc;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > DGE_MAX_MTU)
			error = EINVAL;
		else if ((error = ifioctl_common(ifp, cmd, data)) != ENETRESET)
			break;
		else if (ifp->if_flags & IFF_UP)
			error = (*ifp->if_init)(ifp);
		else
			error = 0;
		break;

        case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		/* extract link flags */
		if ((ifp->if_flags & IFF_LINK0) == 0 &&
		    (ifp->if_flags & IFF_LINK1) == 0)
			mmrbc = PCIX_MMRBC_512;
		else if ((ifp->if_flags & IFF_LINK0) == 0 &&
		    (ifp->if_flags & IFF_LINK1) != 0)
			mmrbc = PCIX_MMRBC_1024;
		else if ((ifp->if_flags & IFF_LINK0) != 0 &&
		    (ifp->if_flags & IFF_LINK1) == 0)
			mmrbc = PCIX_MMRBC_2048;
		else
			mmrbc = PCIX_MMRBC_4096;
		if (mmrbc != sc->sc_mmrbc) {
			preg = pci_conf_read(sc->sc_pc, sc->sc_pt,DGE_PCIX_CMD);
			preg &= ~PCIX_MMRBC_MSK;
			preg |= mmrbc;
			pci_conf_write(sc->sc_pc, sc->sc_pt,DGE_PCIX_CMD, preg);
			sc->sc_mmrbc = mmrbc;
		}
                /* FALLTHROUGH */
	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCSIFCAP)
			error = (*ifp->if_init)(ifp);
		else if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			dge_set_filter(sc);
		}
		break;
	}

	/* Try to get more packets going. */
	dge_start(ifp);

	splx(s);
	return (error);
}

/*
 * dge_intr:
 *
 *	Interrupt service routine.
 */
static int
dge_intr(void *arg)
{
	struct dge_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t icr;
	int wantinit, handled = 0;

	for (wantinit = 0; wantinit == 0;) {
		icr = CSR_READ(sc, DGE_ICR);
		if ((icr & sc->sc_icr) == 0)
			break;

		rnd_add_uint32(&sc->rnd_source, icr);

		handled = 1;

#if defined(DGE_DEBUG) || defined(DGE_EVENT_COUNTERS)
		if (icr & (ICR_RXDMT0|ICR_RXT0)) {
			DPRINTF(DGE_DEBUG_RX,
			    ("%s: RX: got Rx intr 0x%08x\n",
			    device_xname(sc->sc_dev),
			    icr & (ICR_RXDMT0|ICR_RXT0)));
			DGE_EVCNT_INCR(&sc->sc_ev_rxintr);
		}
#endif
		dge_rxintr(sc);

#if defined(DGE_DEBUG) || defined(DGE_EVENT_COUNTERS)
		if (icr & ICR_TXDW) {
			DPRINTF(DGE_DEBUG_TX,
			    ("%s: TX: got TXDW interrupt\n",
			    device_xname(sc->sc_dev)));
			DGE_EVCNT_INCR(&sc->sc_ev_txdw);
		}
		if (icr & ICR_TXQE)
			DGE_EVCNT_INCR(&sc->sc_ev_txqe);
#endif
		dge_txintr(sc);

		if (icr & (ICR_LSC|ICR_RXSEQ)) {
			DGE_EVCNT_INCR(&sc->sc_ev_linkintr);
			dge_linkintr(sc, icr);
		}

		if (icr & ICR_RXO) {
			printf("%s: Receive overrun\n", device_xname(sc->sc_dev));
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			dge_init(ifp);

		/* Try to get more packets going. */
		dge_start(ifp);
	}

	return (handled);
}

/*
 * dge_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
dge_txintr(struct dge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct dge_txsoft *txs;
	uint8_t status;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txsdirty; sc->sc_txsfree != DGE_TXQUEUELEN;
	     i = DGE_NEXTTXS(i), sc->sc_txsfree++) {
		txs = &sc->sc_txsoft[i];

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: checking job %d\n", device_xname(sc->sc_dev), i));

		DGE_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_dmamap->dm_nsegs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status =
		    sc->sc_txdescs[txs->txs_lastdesc].dt_status;
		if ((status & TDESC_STA_DD) == 0) {
			DGE_CDTXSYNC(sc, txs->txs_lastdesc, 1,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		DPRINTF(DGE_DEBUG_TX,
		    ("%s: TX: job %d done: descs %d..%d\n",
		    device_xname(sc->sc_dev), i, txs->txs_firstdesc,
		    txs->txs_lastdesc));

		ifp->if_opackets++;
		sc->sc_txfree += txs->txs_ndesc;
		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txsdirty = i;
	DPRINTF(DGE_DEBUG_TX,
	    ("%s: TX: txsdirty -> %d\n", device_xname(sc->sc_dev), i));

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txsfree == DGE_TXQUEUELEN)
		ifp->if_timer = 0;
}

/*
 * dge_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
dge_rxintr(struct dge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct dge_rxsoft *rxs;
	struct mbuf *m;
	int i, len;
	uint8_t status, errors;

	for (i = sc->sc_rxptr;; i = DGE_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		DPRINTF(DGE_DEBUG_RX,
		    ("%s: RX: checking descriptor %d\n",
		    device_xname(sc->sc_dev), i));

		DGE_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = sc->sc_rxdescs[i].dr_status;
		errors = sc->sc_rxdescs[i].dr_errors;
		len = le16toh(sc->sc_rxdescs[i].dr_len);

		if ((status & RDESC_STS_DD) == 0) {
			/*
			 * We have processed all of the receive descriptors.
			 */
			DGE_CDRXSYNC(sc, i, BUS_DMASYNC_PREREAD);
			break;
		}

		if (__predict_false(sc->sc_rxdiscard)) {
			DPRINTF(DGE_DEBUG_RX,
			    ("%s: RX: discarding contents of descriptor %d\n",
			    device_xname(sc->sc_dev), i));
			DGE_INIT_RXDESC(sc, i);
			if (status & RDESC_STS_EOP) {
				/* Reset our state. */
				DPRINTF(DGE_DEBUG_RX,
				    ("%s: RX: resetting rxdiscard -> 0\n",
				    device_xname(sc->sc_dev)));
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = rxs->rxs_mbuf;

		/*
		 * Add a new receive buffer to the ring.
		 */
		if (dge_add_rxbuf(sc, i) != 0) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			DGE_INIT_RXDESC(sc, i);
			if ((status & RDESC_STS_EOP) == 0)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			DGE_RXCHAIN_RESET(sc);
			DPRINTF(DGE_DEBUG_RX,
			    ("%s: RX: Rx buffer allocation failed, "
			    "dropping packet%s\n", device_xname(sc->sc_dev),
			    sc->sc_rxdiscard ? " (discard)" : ""));
			continue;
		}
		DGE_INIT_RXDESC(sc, DGE_PREVRX(i)); /* Write the descriptor */

		DGE_RXCHAIN_LINK(sc, m);

		m->m_len = len;

		DPRINTF(DGE_DEBUG_RX,
		    ("%s: RX: buffer at %p len %d\n",
		    device_xname(sc->sc_dev), m->m_data, len));

		/*
		 * If this is not the end of the packet, keep
		 * looking.
		 */
		if ((status & RDESC_STS_EOP) == 0) {
			sc->sc_rxlen += len;
			DPRINTF(DGE_DEBUG_RX,
			    ("%s: RX: not yet EOP, rxlen -> %d\n",
			    device_xname(sc->sc_dev), sc->sc_rxlen));
			continue;
		}

		/*
		 * Okay, we have the entire packet now...
		 */
		*sc->sc_rxtailp = NULL;
		m = sc->sc_rxhead;
		len += sc->sc_rxlen;

		DGE_RXCHAIN_RESET(sc);

		DPRINTF(DGE_DEBUG_RX,
		    ("%s: RX: have entire packet, len -> %d\n",
		    device_xname(sc->sc_dev), len));

		/*
		 * If an error occurred, update stats and drop the packet.
		 */
		if (errors &
		     (RDESC_ERR_CE|RDESC_ERR_SE|RDESC_ERR_P|RDESC_ERR_RXE)) {
			ifp->if_ierrors++;
			if (errors & RDESC_ERR_SE)
				printf("%s: symbol error\n",
				    device_xname(sc->sc_dev));
			else if (errors & RDESC_ERR_P)
				printf("%s: parity error\n",
				    device_xname(sc->sc_dev));
			else if (errors & RDESC_ERR_CE)
				printf("%s: CRC error\n",
				    device_xname(sc->sc_dev));
			m_freem(m);
			continue;
		}

		/*
		 * No errors.  Receive the packet.
		 */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

		/*
		 * Set up checksum info for this packet.
		 */
		if (status & RDESC_STS_IPCS) {
			DGE_EVCNT_INCR(&sc->sc_ev_rxipsum);
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (errors & RDESC_ERR_IPE)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		}
		if (status & RDESC_STS_TCPCS) {
			/*
			 * Note: we don't know if this was TCP or UDP,
			 * so we just set both bits, and expect the
			 * upper layers to deal.
			 */
			DGE_EVCNT_INCR(&sc->sc_ev_rxtusum);
			m->m_pkthdr.csum_flags |= M_CSUM_TCPv4|M_CSUM_UDPv4;
			if (errors & RDESC_ERR_TCPE)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}

		ifp->if_ipackets++;

		/* Pass this up to any BPF listeners. */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;

	DPRINTF(DGE_DEBUG_RX,
	    ("%s: RX: rxptr -> %d\n", device_xname(sc->sc_dev), i));
}

/*
 * dge_linkintr:
 *
 *	Helper; handle link interrupts.
 */
static void
dge_linkintr(struct dge_softc *sc, uint32_t icr)
{
	uint32_t status;

	if (icr & ICR_LSC) {
		status = CSR_READ(sc, DGE_STATUS);
		if (status & STATUS_LINKUP) {
			DPRINTF(DGE_DEBUG_LINK, ("%s: LINK: LSC -> up\n",
			    device_xname(sc->sc_dev)));
		} else {
			DPRINTF(DGE_DEBUG_LINK, ("%s: LINK: LSC -> down\n",
			    device_xname(sc->sc_dev)));
		}
	} else if (icr & ICR_RXSEQ) {
		DPRINTF(DGE_DEBUG_LINK,
		    ("%s: LINK: Receive sequence error\n",
		    device_xname(sc->sc_dev)));
	}
	/* XXX - fix errata */
}

/*
 * dge_reset:
 *
 *	Reset the i82597 chip.
 */
static void
dge_reset(struct dge_softc *sc)
{
	int i;

	/*
	 * Do a chip reset.
	 */
	CSR_WRITE(sc, DGE_CTRL0, CTRL0_RST | sc->sc_ctrl0);

	delay(10000);

	for (i = 0; i < 1000; i++) {
		if ((CSR_READ(sc, DGE_CTRL0) & CTRL0_RST) == 0)
			break;
		delay(20);
	}

	if (CSR_READ(sc, DGE_CTRL0) & CTRL0_RST)
		printf("%s: WARNING: reset failed to complete\n",
		    device_xname(sc->sc_dev));
        /*
         * Reset the EEPROM logic.
         * This will cause the chip to reread its default values,
	 * which doesn't happen otherwise (errata).
         */
        CSR_WRITE(sc, DGE_CTRL1, CTRL1_EE_RST);
        delay(10000);
}

/*
 * dge_init:		[ifnet interface function]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
dge_init(struct ifnet *ifp)
{
	struct dge_softc *sc = ifp->if_softc;
	struct dge_rxsoft *rxs;
	int i, error = 0;
	uint32_t reg;

	/*
	 * *_HDR_ALIGNED_P is constant 1 if __NO_STRICT_ALIGMENT is set.
	 * There is a small but measurable benefit to avoiding the adjusment
	 * of the descriptor so that the headers are aligned, for normal mtu,
	 * on such platforms.  One possibility is that the DMA itself is
	 * slightly more efficient if the front of the entire packet (instead
	 * of the front of the headers) is aligned.
	 *
	 * Note we must always set align_tweak to 0 if we are using
	 * jumbo frames.
	 */
#ifdef __NO_STRICT_ALIGNMENT
	sc->sc_align_tweak = 0;
#else
	if ((ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN) > (MCLBYTES - 2))
		sc->sc_align_tweak = 0;
	else
		sc->sc_align_tweak = 2;
#endif /* __NO_STRICT_ALIGNMENT */

	/* Cancel any pending I/O. */
	dge_stop(ifp, 0);

	/* Reset the chip to a known state. */
	dge_reset(sc);

	/* Initialize the transmit descriptor ring. */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	DGE_CDTXSYNC(sc, 0, DGE_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = DGE_NTXDESC;
	sc->sc_txnext = 0;

	sc->sc_txctx_ipcs = 0xffffffff;
	sc->sc_txctx_tucs = 0xffffffff;

	CSR_WRITE(sc, DGE_TDBAH, 0);
	CSR_WRITE(sc, DGE_TDBAL, DGE_CDTXADDR(sc, 0));
	CSR_WRITE(sc, DGE_TDLEN, sizeof(sc->sc_txdescs));
	CSR_WRITE(sc, DGE_TDH, 0);
	CSR_WRITE(sc, DGE_TDT, 0);
	CSR_WRITE(sc, DGE_TIDV, TIDV);

#if 0
	CSR_WRITE(sc, DGE_TXDCTL, TXDCTL_PTHRESH(0) |
	    TXDCTL_HTHRESH(0) | TXDCTL_WTHRESH(0));
#endif
	CSR_WRITE(sc, DGE_RXDCTL,
	    RXDCTL_PTHRESH(RXDCTL_PTHRESH_VAL) |
	    RXDCTL_HTHRESH(RXDCTL_HTHRESH_VAL) |
	    RXDCTL_WTHRESH(RXDCTL_WTHRESH_VAL));

	/* Initialize the transmit job descriptors. */
	for (i = 0; i < DGE_TXQUEUELEN; i++)
		sc->sc_txsoft[i].txs_mbuf = NULL;
	sc->sc_txsfree = DGE_TXQUEUELEN;
	sc->sc_txsnext = 0;
	sc->sc_txsdirty = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	CSR_WRITE(sc, DGE_RDBAH, 0);
	CSR_WRITE(sc, DGE_RDBAL, DGE_CDRXADDR(sc, 0));
	CSR_WRITE(sc, DGE_RDLEN, sizeof(sc->sc_rxdescs));
	CSR_WRITE(sc, DGE_RDH, DGE_RXSPACE);
	CSR_WRITE(sc, DGE_RDT, 0);
	CSR_WRITE(sc, DGE_RDTR, RDTR | 0x80000000);
	CSR_WRITE(sc, DGE_FCRTL, FCRTL | FCRTL_XONE);
	CSR_WRITE(sc, DGE_FCRTH, FCRTH);

	for (i = 0; i < DGE_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = dge_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				dge_rxdrain(sc);
				goto out;
			}
		}
		DGE_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = DGE_RXSPACE;
	sc->sc_rxdiscard = 0;
	DGE_RXCHAIN_RESET(sc);

	if (sc->sc_ethercom.ec_capabilities & ETHERCAP_JUMBO_MTU) {
		sc->sc_ctrl0 |= CTRL0_JFE;
		CSR_WRITE(sc, DGE_MFS, ETHER_MAX_LEN_JUMBO << 16);
	}

	/* Write the control registers. */
	CSR_WRITE(sc, DGE_CTRL0, sc->sc_ctrl0);

	/*
	 * Set up checksum offload parameters.
	 */
	reg = CSR_READ(sc, DGE_RXCSUM);
	if (ifp->if_capenable & IFCAP_CSUM_IPv4_Rx)
		reg |= RXCSUM_IPOFL;
	else
		reg &= ~RXCSUM_IPOFL;
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_UDPv4_Rx))
		reg |= RXCSUM_IPOFL | RXCSUM_TUOFL;
	else {
		reg &= ~RXCSUM_TUOFL;
		if ((ifp->if_capenable & IFCAP_CSUM_IPv4_Rx) == 0)
			reg &= ~RXCSUM_IPOFL;
	}
	CSR_WRITE(sc, DGE_RXCSUM, reg);

	/*
	 * Set up the interrupt registers.
	 */
	CSR_WRITE(sc, DGE_IMC, 0xffffffffU);
	sc->sc_icr = ICR_TXDW | ICR_LSC | ICR_RXSEQ | ICR_RXDMT0 |
	    ICR_RXO | ICR_RXT0;

	CSR_WRITE(sc, DGE_IMS, sc->sc_icr);

	/*
	 * Set up the transmit control register.
	 */
	sc->sc_tctl = TCTL_TCE|TCTL_TPDE|TCTL_TXEN;
	CSR_WRITE(sc, DGE_TCTL, sc->sc_tctl);

	/*
	 * Set up the receive control register; we actually program
	 * the register when we set the receive filter.  Use multicast
	 * address offset type 0.
	 */
	sc->sc_mchash_type = 0;

	sc->sc_rctl = RCTL_RXEN | RCTL_RDMTS_12 | RCTL_RPDA_MC |
	    RCTL_CFF | RCTL_SECRC | RCTL_MO(sc->sc_mchash_type);

#ifdef DGE_OFFBYONE_RXBUG
	sc->sc_rctl |= RCTL_BSIZE_16k;
#else
	switch(MCLBYTES) {
	case 2048:
		sc->sc_rctl |= RCTL_BSIZE_2k;
		break;
	case 4096:
		sc->sc_rctl |= RCTL_BSIZE_4k;
		break;
	case 8192:
		sc->sc_rctl |= RCTL_BSIZE_8k;
		break;
	case 16384:
		sc->sc_rctl |= RCTL_BSIZE_16k;
		break;
	default:
		panic("dge_init: MCLBYTES %d unsupported", MCLBYTES);
	}
#endif

	/* Set the receive filter. */
	/* Also sets RCTL */
	dge_set_filter(sc);

	/* ...all done! */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	return (error);
}

/*
 * dge_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
dge_rxdrain(struct dge_softc *sc)
{
	struct dge_rxsoft *rxs;
	int i;

	for (i = 0; i < DGE_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * dge_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
static void
dge_stop(struct ifnet *ifp, int disable)
{
	struct dge_softc *sc = ifp->if_softc;
	struct dge_txsoft *txs;
	int i;

	/* Stop the transmit and receive processes. */
	CSR_WRITE(sc, DGE_TCTL, 0);
	CSR_WRITE(sc, DGE_RCTL, 0);

	/* Release any queued transmit buffers. */
	for (i = 0; i < DGE_TXQUEUELEN; i++) {
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
		dge_rxdrain(sc);
}

/*
 * dge_add_rxbuf:
 *
 *	Add a receive buffer to the indiciated descriptor.
 */
static int
dge_add_rxbuf(struct dge_softc *sc, int idx)
{
	struct dge_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;
#ifdef DGE_OFFBYONE_RXBUG
	void *buf;
#endif

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

#ifdef DGE_OFFBYONE_RXBUG
	if ((buf = dge_getbuf(sc)) == NULL)
		return ENOBUFS;

	m->m_len = m->m_pkthdr.len = DGE_BUFFER_SIZE;
	MEXTADD(m, buf, DGE_BUFFER_SIZE, M_DEVBUF, dge_freebuf, sc);
	m->m_flags |= M_EXT_RW;

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap, buf,
	    DGE_BUFFER_SIZE, NULL, BUS_DMA_READ|BUS_DMA_NOWAIT);
#else
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, rxs->rxs_dmamap, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
#endif
	if (error) {
		printf("%s: unable to load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("dge_add_rxbuf");	/* XXX XXX XXX */
	}
	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * dge_set_ral:
 *
 *	Set an entry in the receive address list.
 */
static void
dge_set_ral(struct dge_softc *sc, const uint8_t *enaddr, int idx)
{
	uint32_t ral_lo, ral_hi;

	if (enaddr != NULL) {
		ral_lo = enaddr[0] | (enaddr[1] << 8) | (enaddr[2] << 16) |
		    (enaddr[3] << 24);
		ral_hi = enaddr[4] | (enaddr[5] << 8);
		ral_hi |= RAH_AV;
	} else {
		ral_lo = 0;
		ral_hi = 0;
	}
	CSR_WRITE(sc, RA_ADDR(DGE_RAL, idx), ral_lo);
	CSR_WRITE(sc, RA_ADDR(DGE_RAH, idx), ral_hi);
}

/*
 * dge_mchash:
 *
 *	Compute the hash of the multicast address for the 4096-bit
 *	multicast filter.
 */
static uint32_t
dge_mchash(struct dge_softc *sc, const uint8_t *enaddr)
{
	static const int lo_shift[4] = { 4, 3, 2, 0 };
	static const int hi_shift[4] = { 4, 5, 6, 8 };
	uint32_t hash;

	hash = (enaddr[4] >> lo_shift[sc->sc_mchash_type]) |
	    (((uint16_t) enaddr[5]) << hi_shift[sc->sc_mchash_type]);

	return (hash & 0xfff);
}

/*
 * dge_set_filter:
 *
 *	Set up the receive filter.
 */
static void
dge_set_filter(struct dge_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t hash, reg, bit;
	int i;

	sc->sc_rctl &= ~(RCTL_BAM | RCTL_UPE | RCTL_MPE);

	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rctl |= RCTL_BAM;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rctl |= RCTL_UPE;
		goto allmulti;
	}

	/*
	 * Set the station address in the first RAL slot, and
	 * clear the remaining slots.
	 */
	dge_set_ral(sc, CLLADDR(ifp->if_sadl), 0);
	for (i = 1; i < RA_TABSIZE; i++)
		dge_set_ral(sc, NULL, i);

	/* Clear out the multicast table. */
	for (i = 0; i < MC_TABSIZE; i++)
		CSR_WRITE(sc, DGE_MTA + (i << 2), 0);

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

		hash = dge_mchash(sc, enm->enm_addrlo);

		reg = (hash >> 5) & 0x7f;
		bit = hash & 0x1f;

		hash = CSR_READ(sc, DGE_MTA + (reg << 2));
		hash |= 1U << bit;

		CSR_WRITE(sc, DGE_MTA + (reg << 2), hash);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_rctl |= RCTL_MPE;

 setit:
	CSR_WRITE(sc, DGE_RCTL, sc->sc_rctl);
}

/*
 * Read in the EEPROM info and verify checksum.
 */
int
dge_read_eeprom(struct dge_softc *sc)
{
	uint16_t cksum;
	int i;

	cksum = 0;
	for (i = 0; i < EEPROM_SIZE; i++) {
		sc->sc_eeprom[i] = dge_eeprom_word(sc, i);
		cksum += sc->sc_eeprom[i];
	}
	return cksum != EEPROM_CKSUM;
}


/*
 * Read a 16-bit word from address addr in the serial EEPROM.
 */
uint16_t
dge_eeprom_word(struct dge_softc *sc, int addr)
{
	uint32_t reg;
	uint16_t rval = 0;
	int i;

	reg = CSR_READ(sc, DGE_EECD) & ~(EECD_SK|EECD_DI|EECD_CS);

	/* Lower clock pulse (and data in to chip) */
	CSR_WRITE(sc, DGE_EECD, reg);
	/* Select chip */
	CSR_WRITE(sc, DGE_EECD, reg|EECD_CS);

	/* Send read command */
	dge_eeprom_clockout(sc, 1);
	dge_eeprom_clockout(sc, 1);
	dge_eeprom_clockout(sc, 0);

	/* Send address */
	for (i = 5; i >= 0; i--)
		dge_eeprom_clockout(sc, (addr >> i) & 1);

	/* Read data */
	for (i = 0; i < 16; i++) {
		rval <<= 1;
		rval |= dge_eeprom_clockin(sc);
	}

	/* Deselect chip */
	CSR_WRITE(sc, DGE_EECD, reg);

	return rval;
}

/*
 * Clock out a single bit to the EEPROM.
 */
void
dge_eeprom_clockout(struct dge_softc *sc, int bit)
{
	int reg;

	reg = CSR_READ(sc, DGE_EECD) & ~(EECD_DI|EECD_SK);
	if (bit)
		reg |= EECD_DI;

	CSR_WRITE(sc, DGE_EECD, reg);
	delay(2);
	CSR_WRITE(sc, DGE_EECD, reg|EECD_SK);
	delay(2);
	CSR_WRITE(sc, DGE_EECD, reg);
	delay(2);
}

/*
 * Clock in a single bit from EEPROM.
 */
int
dge_eeprom_clockin(struct dge_softc *sc)
{
	int reg, rv;

	reg = CSR_READ(sc, DGE_EECD) & ~(EECD_DI|EECD_DO|EECD_SK);

	CSR_WRITE(sc, DGE_EECD, reg|EECD_SK); /* Raise clock */
	delay(2);
	rv = (CSR_READ(sc, DGE_EECD) & EECD_DO) != 0; /* Get bit */
	CSR_WRITE(sc, DGE_EECD, reg); /* Lower clock */
	delay(2);

	return rv;
}

static void
dge_xgmii_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct dge_softc *sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER|IFM_10G_LR;

	if (CSR_READ(sc, DGE_STATUS) & STATUS_LINKUP)
		ifmr->ifm_status |= IFM_ACTIVE;
}

static inline int
phwait(struct dge_softc *sc, int p, int r, int d, int type)
{
        int i, mdic;

        CSR_WRITE(sc, DGE_MDIO,
	    MDIO_PHY(p) | MDIO_REG(r) | MDIO_DEV(d) | type | MDIO_CMD);
        for (i = 0; i < 10; i++) {
                delay(10);
                if (((mdic = CSR_READ(sc, DGE_MDIO)) & MDIO_CMD) == 0)
                        break;
        }
        return mdic;
}

static void
dge_xgmii_writereg(struct dge_softc *sc, int phy, int reg, int val)
{
	int mdic;

	CSR_WRITE(sc, DGE_MDIRW, val);
	if (((mdic = phwait(sc, phy, reg, 1, MDIO_ADDR)) & MDIO_CMD)) {
		printf("%s: address cycle timeout; phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
		return;
	}
	if (((mdic = phwait(sc, phy, reg, 1, MDIO_WRITE)) & MDIO_CMD)) {
		printf("%s: write cycle timeout; phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
		return;
	}
}

static void
dge_xgmii_reset(struct dge_softc *sc)
{
	dge_xgmii_writereg(sc, 0, 0, BMCR_RESET);
}

static int
dge_xgmii_mediachange(struct ifnet *ifp)
{
	return 0;
}
