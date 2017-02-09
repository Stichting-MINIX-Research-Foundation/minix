/*	$NetBSD: dp8390.c,v 1.82 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dp8390.c,v 1.82 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_ipkdb.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/bus.h>

#ifdef IPKDB_DP8390
#include <ipkdb/ipkdb.h>
#endif

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#ifdef DEBUG
int	dp8390_debug = 0;
#endif

static void dp8390_xmit(struct dp8390_softc *);

static void dp8390_read_hdr(struct dp8390_softc *, int, struct dp8390_ring *);
static int  dp8390_ring_copy(struct dp8390_softc *, int, void *, u_short);
static int  dp8390_write_mbuf(struct dp8390_softc *, struct mbuf *, int);

static int  dp8390_test_mem(struct dp8390_softc *);

/*
 * Standard media init routine for the dp8390.
 */
void
dp8390_media_init(struct dp8390_softc *sc)
{

	ifmedia_init(&sc->sc_media, 0, dp8390_mediachange, dp8390_mediastatus);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
}

/*
 * Do bus-independent setup.
 */
int
dp8390_config(struct dp8390_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int rv;

	rv = 1;

	if (sc->test_mem == NULL)
		sc->test_mem = dp8390_test_mem;
	if (sc->read_hdr == NULL)
		sc->read_hdr = dp8390_read_hdr;
	if (sc->recv_int == NULL)
		sc->recv_int = dp8390_rint;
	if (sc->ring_copy == NULL)
		sc->ring_copy = dp8390_ring_copy;
	if (sc->write_mbuf == NULL)
		sc->write_mbuf = dp8390_write_mbuf;

	/* Allocate one xmit buffer if < 16k, two buffers otherwise. */
	if ((sc->mem_size < 16384) ||
	    (sc->sc_flags & DP8390_NO_MULTI_BUFFERING))
		sc->txb_cnt = 1;
	else if (sc->mem_size < 8192 * 3)
		sc->txb_cnt = 2;
	else
		sc->txb_cnt = 3;

	sc->tx_page_start = sc->mem_start >> ED_PAGE_SHIFT;
	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + (sc->mem_size >> ED_PAGE_SHIFT);
	sc->mem_ring = sc->mem_start +
	    ((sc->txb_cnt * ED_TXBUF_SIZE) << ED_PAGE_SHIFT);
	sc->mem_end = sc->mem_start + sc->mem_size;

	/* Now zero memory and verify that it is clear. */
	if ((*sc->test_mem)(sc))
		goto out;

	/* Set interface to stopped condition (reset). */
	dp8390_stop(sc);

	/* Initialize ifnet structure. */
	strcpy(ifp->if_xname, device_xname(sc->sc_dev));
	ifp->if_softc = sc;
	ifp->if_start = dp8390_start;
	ifp->if_ioctl = dp8390_ioctl;
	if (ifp->if_watchdog == NULL)
		ifp->if_watchdog = dp8390_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Print additional info when attached. */
	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	/* Initialize media goo. */
	(*sc->sc_media_init)(sc);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	/* The attach is successful. */
	sc->sc_flags |= DP8390_ATTACHED;

	rv = 0;
 out:
	return rv;
}

/*
 * Media change callback.
 */
int
dp8390_mediachange(struct ifnet *ifp)
{
	struct dp8390_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return (*sc->sc_mediachange)(sc);
	return 0;
}

/*
 * Media status callback.
 */
void
dp8390_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct dp8390_softc *sc = ifp->if_softc;

	if (sc->sc_enabled == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
}

/*
 * Reset interface.
 */
void
dp8390_reset(struct dp8390_softc *sc)
{
	int s;

	s = splnet();
	dp8390_stop(sc);
	dp8390_init(sc);
	splx(s);
}

/*
 * Take interface offline.
 */
void
dp8390_stop(struct dp8390_softc *sc)
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	int n = 5000;

	/* Stop everything on the interface, and select page 0 registers. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks to
	 * 'n' (about 5ms).  It shouldn't even take 5us on modern DS8390's, but
	 * just in case it's an old one.
	 */
	while (((NIC_GET(regt, regh, ED_P0_ISR) & ED_ISR_RST) == 0) && --n)
		DELAY(1);

	if (sc->stop_card != NULL)
		(*sc->stop_card)(sc);
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */

void
dp8390_watchdog(struct ifnet *ifp)
{
	struct dp8390_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++sc->sc_ec.ec_if.if_oerrors;

	dp8390_reset(sc);
}

/*
 * Initialize device.
 */
void
dp8390_init(struct dp8390_softc *sc)
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint8_t mcaf[8];
	int i;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 * This init procedure is "mandatory"...don't change what or when
	 * things happen.
	 */

	/* Reset transmitter flags. */
	ifp->if_timer = 0;

	sc->txb_inuse = 0;
	sc->txb_new = 0;
	sc->txb_next_tx = 0;

	/* Set interface for page 0, remote DMA complete, stopped. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	if (sc->dcr_reg & ED_DCR_LS) {
		NIC_PUT(regt, regh, ED_P0_DCR, sc->dcr_reg);
	} else {
		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA, byte
		 * order=80x86, byte-wide DMA xfers,
		 */
		NIC_PUT(regt, regh, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}

	/* Clear remote byte count registers. */
	NIC_PUT(regt, regh, ED_P0_RBCR0, 0);
	NIC_PUT(regt, regh, ED_P0_RBCR1, 0);

	/* Tell RCR to do nothing for now. */
	NIC_PUT(regt, regh, ED_P0_RCR, ED_RCR_MON | sc->rcr_proto);

	/* Place NIC in internal loopback mode. */
	NIC_PUT(regt, regh, ED_P0_TCR, ED_TCR_LB0);

	/* Set lower bits of byte addressable framing to 0. */
	if (sc->is790)
		NIC_PUT(regt, regh, 0x09, 0);

	/* Initialize receive buffer ring. */
	NIC_PUT(regt, regh, ED_P0_BNRY, sc->rec_page_start);
	NIC_PUT(regt, regh, ED_P0_PSTART, sc->rec_page_start);
	NIC_PUT(regt, regh, ED_P0_PSTOP, sc->rec_page_stop);

	/*
	 * Enable the following interrupts: receive/transmit complete,
	 * receive/transmit error, and Receiver OverWrite.
	 *
	 * Counter overflow and Remote DMA complete are *not* enabled.
	 */
	NIC_PUT(regt, regh, ED_P0_IMR,
	    ED_IMR_PRXE | ED_IMR_PTXE | ED_IMR_RXEE | ED_IMR_TXEE |
	    ED_IMR_OVWE);

	/*
	 * Clear all interrupts.  A '1' in each bit position clears the
	 * corresponding flag.
	 */
	NIC_PUT(regt, regh, ED_P0_ISR, 0xff);

	/* Program command register for page 1. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	/* Copy out our station address. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		NIC_PUT(regt, regh, ED_P1_PAR0 + i, CLLADDR(ifp->if_sadl)[i]);

	/* Set multicast filter on chip. */
	dp8390_getmcaf(&sc->sc_ec, mcaf);
	for (i = 0; i < 8; i++)
		NIC_PUT(regt, regh, ED_P1_MAR0 + i, mcaf[i]);

	/*
	 * Set current page pointer to one page after the boundary pointer, as
	 * recommended in the National manual.
	 */
	sc->next_packet = sc->rec_page_start + 1;
	NIC_PUT(regt, regh, ED_P1_CURR, sc->next_packet);

	/* Program command register for page 0. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P1_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	/* Accept broadcast and multicast packets by default. */
	i = ED_RCR_AB | ED_RCR_AM | sc->rcr_proto;
	if (ifp->if_flags & IFF_PROMISC) {
		/*
		 * Set promiscuous mode.  Multicast filter was set earlier so
		 * that we should receive all multicast packets.
		 */
		i |= ED_RCR_PRO | ED_RCR_AR | ED_RCR_SEP;
	}
	NIC_PUT(regt, regh, ED_P0_RCR, i);

	/* Take interface out of loopback. */
	NIC_PUT(regt, regh, ED_P0_TCR, 0);

	/* Do any card-specific initialization, if applicable. */
	if (sc->init_card != NULL)
		(*sc->init_card)(sc);

	/* Fire up the interface. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	/* Set 'running' flag, and clear output active flag. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* ...and attempt to start output. */
	dp8390_start(ifp);
}

/*
 * This routine actually starts the transmission on the interface.
 */
static void
dp8390_xmit(struct dp8390_softc *sc)
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	u_short len;

#ifdef DIAGNOSTIC
	if ((sc->txb_next_tx + sc->txb_inuse) % sc->txb_cnt != sc->txb_new)
		panic("dp8390_xmit: desync, next_tx=%d inuse=%d cnt=%d new=%d",
		    sc->txb_next_tx, sc->txb_inuse, sc->txb_cnt, sc->txb_new);

	if (sc->txb_inuse == 0)
		panic("dp8390_xmit: no packets to xmit");
#endif

	len = sc->txb_len[sc->txb_next_tx];

	/* Set NIC for page 0 register access. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(regt, regh);

	/* Set TX buffer start page. */
	NIC_PUT(regt, regh, ED_P0_TPSR,
	    sc->tx_page_start + sc->txb_next_tx * ED_TXBUF_SIZE);

	/* Set TX length. */
	NIC_PUT(regt, regh, ED_P0_TBCR0, len);
	NIC_PUT(regt, regh, ED_P0_TBCR1, len >> 8);

	/* Set page 0, remote DMA complete, transmit packet, and *start*. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

	/* Point to next transmit buffer slot and wrap if necessary. */
	if (++sc->txb_next_tx == sc->txb_cnt)
		sc->txb_next_tx = 0;

	/* Set a timer just in case we never hear from the board again. */
	ifp->if_timer = 2;
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
dp8390_start(struct ifnet *ifp)
{
	struct dp8390_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int buffer;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

 outloop:
	/* See if there is room to put another packet in the buffer. */
	if (sc->txb_inuse == sc->txb_cnt) {
		/* No room.  Indicate this to the outside world and exit. */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("dp8390_start: no header mbuf");

	/* Tap off here if there is a BPF listener. */
	bpf_mtap(ifp, m0);

	/* txb_new points to next open buffer slot. */
	buffer = sc->mem_start +
	    ((sc->txb_new * ED_TXBUF_SIZE) << ED_PAGE_SHIFT);

	len = (*sc->write_mbuf)(sc, m0, buffer);

	m_freem(m0);
	sc->txb_len[sc->txb_new] = len;

	/* Point to next buffer slot and wrap if necessary. */
	if (++sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	/* Start the first packet transmitting. */
	if (sc->txb_inuse++ == 0)
		dp8390_xmit(sc);

	/* Loop back to the top to possibly buffer more packets. */
	goto outloop;
}

/*
 * Ethernet interface receiver interrupt.
 */
void
dp8390_rint(struct dp8390_softc *sc)
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct dp8390_ring packet_hdr;
	int packet_ptr;
	uint16_t len;
	uint8_t boundary, current;
	uint8_t nlen;

 loop:
	/* Set NIC to page 1 registers to get 'current' pointer. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);
	NIC_BARRIER(regt, regh);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
	 * it points to where new data has been buffered.  The 'CURR' (current)
	 * register points to the logical end of the ring-buffer - i.e. it
	 * points to where additional new data will be added.  We loop here
	 * until the logical beginning equals the logical end (or in other
	 * words, until the ring-buffer is empty).
	 */
	current = NIC_GET(regt, regh, ED_P1_CURR);
	if (sc->next_packet == current)
		return;

	/* Set NIC to page 0 registers to update boundary register. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P1_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(regt, regh);

	do {
		/* Get pointer to this buffer's header structure. */
		packet_ptr = sc->mem_ring +
		    ((sc->next_packet - sc->rec_page_start) << ED_PAGE_SHIFT);

		(*sc->read_hdr)(sc, packet_ptr, &packet_hdr);
		len = packet_hdr.count;

		/*
		 * Try do deal with old, buggy chips that sometimes duplicate
		 * the low byte of the length into the high byte.  We do this
		 * by simply ignoring the high byte of the length and always
		 * recalculating it.
		 *
		 * NOTE: sc->next_packet is pointing at the current packet.
		 */
		if (packet_hdr.next_packet >= sc->next_packet)
			nlen = (packet_hdr.next_packet - sc->next_packet);
		else
			nlen = ((packet_hdr.next_packet - sc->rec_page_start) +
			    (sc->rec_page_stop - sc->next_packet));
		--nlen;
		if ((len & ED_PAGE_MASK) + sizeof(packet_hdr) > ED_PAGE_SIZE)
			--nlen;
		len = (len & ED_PAGE_MASK) | (nlen << ED_PAGE_SHIFT);
#ifdef DIAGNOSTIC
		if (len != packet_hdr.count) {
			aprint_verbose_dev(sc->sc_dev, "length does not match "
			    "next packet pointer\n");
			aprint_verbose_dev(sc->sc_dev, "len %04x nlen %04x "
			    "start %02x first %02x curr %02x next %02x "
			    "stop %02x\n", packet_hdr.count, len,
			    sc->rec_page_start, sc->next_packet, current,
			    packet_hdr.next_packet, sc->rec_page_stop);
		}
#endif

		/*
		 * Be fairly liberal about what we allow as a "reasonable"
		 * length so that a [crufty] packet will make it to BPF (and
		 * can thus be analyzed).  Note that all that is really
		 * important is that we have a length that will fit into one
		 * mbuf cluster or less; the upper layer protocols can then
		 * figure out the length from their own length field(s).
		 */
		if (len <= MCLBYTES &&
		    packet_hdr.next_packet >= sc->rec_page_start &&
		    packet_hdr.next_packet < sc->rec_page_stop) {
			/* Go get packet. */
			dp8390_read(sc,
			    packet_ptr + sizeof(struct dp8390_ring),
			    len - sizeof(struct dp8390_ring));
		} else {
			/* Really BAD.  The ring pointers are corrupted. */
			log(LOG_ERR, "%s: NIC memory corrupt - "
			    "invalid packet length %d\n",
			    device_xname(sc->sc_dev), len);
			++sc->sc_ec.ec_if.if_ierrors;
			dp8390_reset(sc);
			return;
		}

		/* Update next packet pointer. */
		sc->next_packet = packet_hdr.next_packet;

		/*
		 * Update NIC boundary pointer - being careful to keep it one
		 * buffer behind (as recommended by NS databook).
		 */
		boundary = sc->next_packet - 1;
		if (boundary < sc->rec_page_start)
			boundary = sc->rec_page_stop - 1;
		NIC_PUT(regt, regh, ED_P0_BNRY, boundary);
	} while (sc->next_packet != current);

	goto loop;
}

/* Ethernet interface interrupt processor. */
int
dp8390_intr(void *arg)
{
	struct dp8390_softc *sc = arg;
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint8_t isr;
	uint8_t rndisr;

	if (sc->sc_enabled == 0 ||
	    !device_is_active(sc->sc_dev))
		return 0;

	/* Set NIC to page 0 registers. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(regt, regh);

	isr = NIC_GET(regt, regh, ED_P0_ISR);
	if (isr == 0)
		return 0;

	rndisr = isr;

	/* Loop until there are no more new interrupts. */
	for (;;) {
		/*
		 * Reset all the bits that we are 'acknowledging' by writing a
		 * '1' to each bit position that was set.
		 * (Writing a '1' *clears* the bit.)
		 */
		NIC_PUT(regt, regh, ED_P0_ISR, isr);

		/* Work around for AX88190 bug */
		if ((sc->sc_flags & DP8390_DO_AX88190_WORKAROUND) != 0)
			while ((NIC_GET(regt, regh, ED_P0_ISR) & isr) != 0) {
				NIC_PUT(regt, regh, ED_P0_ISR, 0);
				NIC_PUT(regt, regh, ED_P0_ISR, isr);
			}

		/*
		 * Handle transmitter interrupts.  Handle these first because
		 * the receiver will reset the board under some conditions.
		 *
		 * If the chip was reset while a packet was transmitting, it
		 * may still deliver a TX interrupt.  In this case, just ignore
		 * the interrupt.
		 */
		if ((isr & (ED_ISR_PTX | ED_ISR_TXE)) != 0 &&
		    sc->txb_inuse != 0) {
			uint8_t collisions =
			    NIC_GET(regt, regh, ED_P0_NCR) & 0x0f;

			/*
			 * Check for transmit error.  If a TX completed with an
			 * error, we end up throwing the packet away.  Really
			 * the only error that is possible is excessive
			 * collisions, and in this case it is best to allow the
			 * automatic mechanisms of TCP to backoff the flow.  Of
			 * course, with UDP we're screwed, but this is expected
			 * when a network is heavily loaded.
			 */
			if ((isr & ED_ISR_TXE) != 0) {
				/*
				 * Excessive collisions (16).
				 */
				if ((NIC_GET(regt, regh, ED_P0_TSR)
				    & ED_TSR_ABT) && (collisions == 0)) {
					/*
					 * When collisions total 16, the P0_NCR
					 * will indicate 0, and the TSR_ABT is
					 * set.
					 */
					collisions = 16;
				}

				/* Update output errors counter. */
				++ifp->if_oerrors;
			} else {
				/*
				 * Throw away the non-error status bits.
				 *
				 * XXX
				 * It may be useful to detect loss of carrier
				 * and late collisions here.
				 */
				(void)NIC_GET(regt, regh, ED_P0_TSR);

				/*
				 * Update total number of successfully
				 * transmitted packets.
				 */
				++ifp->if_opackets;
			}

			/* Clear watchdog timer. */
			ifp->if_timer = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			/*
			 * Add in total number of collisions on last
			 * transmission.
			 */
			ifp->if_collisions += collisions;

			/*
			 * Decrement buffer in-use count if not zero (can only
			 * be zero if a transmitter interrupt occurred while not
			 * actually transmitting).
			 * If data is ready to transmit, start it transmitting,
			 * otherwise defer until after handling receiver.
			 */
			if (--sc->txb_inuse != 0)
				dp8390_xmit(sc);
		}

		/* Handle receiver interrupts. */
		if ((isr & (ED_ISR_PRX | ED_ISR_RXE | ED_ISR_OVW)) != 0) {
			/*
			 * Overwrite warning.  In order to make sure that a
			 * lockup of the local DMA hasn't occurred, we reset
			 * and re-init the NIC.  The NSC manual suggests only a
			 * partial reset/re-init is necessary - but some chips
			 * seem to want more.  The DMA lockup has been seen
			 * only with early rev chips - Methinks this bug was
			 * fixed in later revs.  -DG
			 */
			if ((isr & ED_ISR_OVW) != 0) {
				++ifp->if_ierrors;
#ifdef DIAGNOSTIC
				log(LOG_WARNING, "%s: warning - receiver "
				    "ring buffer overrun\n",
				    device_xname(sc->sc_dev));
#endif
				/* Stop/reset/re-init NIC. */
				dp8390_reset(sc);
			} else {
				/*
				 * Receiver Error.  One or more of: CRC error,
				 * frame alignment error FIFO overrun, or
				 * missed packet.
				 */
				if ((isr & ED_ISR_RXE) != 0) {
					++ifp->if_ierrors;
#ifdef DEBUG
					if (dp8390_debug) {
						printf("%s: receive error %x\n",
						    device_xname(sc->sc_dev),
						    NIC_GET(regt, regh,
							ED_P0_RSR));
					}
#endif
				}

				/*
				 * Go get the packet(s)
				 * XXX - Doing this on an error is dubious
				 * because there shouldn't be any data to get
				 * (we've configured the interface to not
				 * accept packets with errors).
				 */
				(*sc->recv_int)(sc);
			}
		}

		/*
		 * If it looks like the transmitter can take more data, attempt
		 * to start output on the interface.  This is done after
		 * handling the receiver to give the receiver priority.
		 */
		dp8390_start(ifp);

		/*
		 * Return NIC CR to standard state: page 0, remote DMA
		 * complete, start (toggling the TXP bit off, even if was just
		 * set in the transmit routine, is *okay* - it is 'edge'
		 * triggered from low to high).
		 */
		NIC_BARRIER(regt, regh);
		NIC_PUT(regt, regh, ED_P0_CR,
		    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
		NIC_BARRIER(regt, regh);

		/*
		 * If the Network Talley Counters overflow, read them to reset
		 * them.  It appears that old 8390's won't clear the ISR flag
		 * otherwise - resulting in an infinite loop.
		 */
		if ((isr & ED_ISR_CNT) != 0) {
			(void)NIC_GET(regt, regh, ED_P0_CNTR0);
			(void)NIC_GET(regt, regh, ED_P0_CNTR1);
			(void)NIC_GET(regt, regh, ED_P0_CNTR2);
		}

		isr = NIC_GET(regt, regh, ED_P0_ISR);
		if (isr == 0)
			goto out;
	}

 out:
	rnd_add_uint32(&sc->rnd_source, rndisr);
	return 1;
}

/*
 * Process an ioctl request.  This code needs some work - it looks pretty ugly.
 */
int
dp8390_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct dp8390_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = data;
	struct ifreq *ifr = data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		if ((error = dp8390_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;

		dp8390_init(sc);
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

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			dp8390_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			dp8390_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = dp8390_enable(sc)) != 0)
				break;
			dp8390_init(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			dp8390_stop(sc);
			dp8390_init(sc);
			break;
		default:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->sc_enabled == 0) {
			error = EIO;
			break;
		}

		/* Update our multicast list. */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING) {
				dp8390_stop(sc); /* XXX for ds_setmcaf? */
				dp8390_init(sc);
			}
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return error;
}

/*
 * Retrieve packet from buffer memory and send to the next level up via
 * ether_input().  If there is a BPF listener, give a copy to BPF, too.
 */
void
dp8390_read(struct dp8390_softc *sc, int buf, u_short len)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m;

	/* Pull packet off interface. */
	m = dp8390_get(sc, buf, len);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);
}


/*
 * Supporting routines.
 */

/*
 * Compute the multicast address filter from the list of multicast addresses we
 * need to listen to.
 */
void
dp8390_getmcaf(struct ethercom *ec, uint8_t *af)
{
	struct ifnet *ifp = &ec->ec_if;
	struct ether_multi *enm;
	uint32_t crc;
	int i;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		for (i = 0; i < 8; i++)
			af[i] = 0xff;
		return;
	}
	for (i = 0; i < 8; i++)
		af[i] = 0;
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			for (i = 0; i < 8; i++)
				af[i] = 0xff;
			return;
		}

		crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 3] |= 1 << (crc & 0x7);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
}

/*
 * Copy data from receive buffer to a new mbuf chain allocating mbufs
 * as needed.  Return pointer to first mbuf in chain.
 * sc = dp8390 info (softc)
 * src = pointer in dp8390 ring buffer
 * total_len = amount of data to copy
 */
struct mbuf *
dp8390_get(struct dp8390_softc *sc, int src, u_short total_len)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m, *m0, *newm;
	u_short len;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return NULL;
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = total_len;
	len = MHLEN;
	m = m0;

	while (total_len > 0) {
		if (total_len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0)
				goto bad;
			len = MCLBYTES;
		}

		/*
		 * Make sure the data after the Ethernet header is aligned.
		 */
		if (m == m0) {
			char *newdata = (char *)
			    ALIGN(m->m_data + sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}

		m->m_len = len = min(total_len, len);
		src = (*sc->ring_copy)(sc, src, mtod(m, void *), len);

		total_len -= len;
		if (total_len > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == NULL)
				goto bad;
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return m0;

 bad:
	m_freem(m0);
	return NULL;
}


/*
 * Default driver support functions.
 *
 * NOTE: all support functions assume 8-bit shared memory.
 */
/*
 * Zero NIC buffer memory and verify that it is clear.
 */
static int
dp8390_test_mem(struct dp8390_softc *sc)
{
	bus_space_tag_t buft = sc->sc_buft;
	bus_space_handle_t bufh = sc->sc_bufh;
	int i;

	bus_space_set_region_1(buft, bufh, sc->mem_start, 0, sc->mem_size);

	for (i = 0; i < sc->mem_size; ++i) {
		if (bus_space_read_1(buft, bufh, sc->mem_start + i)) {
			printf(": failed to clear NIC buffer at offset %x - "
			    "check configuration\n", (sc->mem_start + i));
			return 1;
		}
	}

	return 0;
}

/*
 * Read a packet header from the ring, given the source offset.
 */
static void
dp8390_read_hdr(struct dp8390_softc *sc, int src, struct dp8390_ring *hdrp)
{
	bus_space_tag_t buft = sc->sc_buft;
	bus_space_handle_t bufh = sc->sc_bufh;

	/*
	 * The byte count includes a 4 byte header that was added by
	 * the NIC.
	 */
	hdrp->rsr = bus_space_read_1(buft, bufh, src);
	hdrp->next_packet = bus_space_read_1(buft, bufh, src + 1);
	hdrp->count = bus_space_read_1(buft, bufh, src + 2) |
	    (bus_space_read_1(buft, bufh, src + 3) << 8);
}

/*
 * Copy `amount' bytes from a packet in the ring buffer to a linear
 * destination buffer, given a source offset and destination address.
 * Takes into account ring-wrap.
 */
static int
dp8390_ring_copy(struct dp8390_softc *sc, int src, void *dst, u_short amount)
{
	bus_space_tag_t buft = sc->sc_buft;
	bus_space_handle_t bufh = sc->sc_bufh;
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		bus_space_read_region_1(buft, bufh, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst = (char *)dst + tmp_amount;
	}
	bus_space_read_region_1(buft, bufh, src, dst, amount);

	return src + amount;
}

/*
 * Copy a packet from an mbuf to the transmit buffer on the card.
 *
 * Currently uses an extra buffer/extra memory copy, unless the whole
 * packet fits in one mbuf.
 */
static int
dp8390_write_mbuf(struct dp8390_softc *sc, struct mbuf *m, int buf)
{
	bus_space_tag_t buft = sc->sc_buft;
	bus_space_handle_t bufh = sc->sc_bufh;
	uint8_t *data;
	int len, totlen = 0;

	for (; m ; m = m->m_next) {
		data = mtod(m, uint8_t *);
		len = m->m_len;
		if (len > 0) {
			bus_space_write_region_1(buft, bufh, buf, data, len);
			totlen += len;
			buf += len;
		}
	}
	if (totlen < ETHER_MIN_LEN - ETHER_CRC_LEN) {
		bus_space_set_region_1(buft, bufh, buf, 0,
		    ETHER_MIN_LEN - ETHER_CRC_LEN - totlen);
		totlen = ETHER_MIN_LEN - ETHER_CRC_LEN;
	}
	return totlen;
}

/*
 * Enable power on the interface.
 */
int
dp8390_enable(struct dp8390_softc *sc)
{

	if (sc->sc_enabled == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "device enable failed\n");
			return EIO;
		}
	}

	sc->sc_enabled = 1;
	return 0;
}

/*
 * Disable power on the interface.
 */
void
dp8390_disable(struct dp8390_softc *sc)
{

	if (sc->sc_enabled != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
}

int
dp8390_activate(device_t self, enum devact act)
{
	struct dp8390_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
dp8390_detach(struct dp8390_softc *sc, int flags)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	/* Succeed now if there's no work to do. */
	if ((sc->sc_flags & DP8390_ATTACHED) == 0)
		return 0;

	/* dp8390_disable() checks sc->sc_enabled */
	dp8390_disable(sc);

	if (sc->sc_media_fini != NULL)
		(*sc->sc_media_fini)(sc);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);

	rnd_detach_source(&sc->rnd_source);
	ether_ifdetach(ifp);
	if_detach(ifp);

	return 0;
}

#ifdef IPKDB_DP8390
static void dp8390_ipkdb_hwinit(struct ipkdb_if *);
static void dp8390_ipkdb_init(struct ipkdb_if *);
static void dp8390_ipkdb_leave(struct ipkdb_if *);
static int dp8390_ipkdb_rcv(struct ipkdb_if *, uint8_t *, int);
static void dp8390_ipkdb_send(struct ipkdb_if *, uint8_t *, int);

/*
 * This is essentially similar to dp8390_config above.
 */
int
dp8390_ipkdb_attach(struct ipkdb_if *kip)
{
	struct dp8390_softc *sc = kip->port;

	if (sc->mem_size < 8192 * 2)
		sc->txb_cnt = 1;
	else if (sc->mem_size < 8192 * 3)
		sc->txb_cnt = 2;
	else
		sc->txb_cnt = 3;

	sc->tx_page_start = sc->mem_start >> ED_PAGE_SHIFT;
	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + (sc->mem_size >> ED_PAGE_SHIFT);
	sc->mem_ring = sc->mem_start +
	    ((sc->txb_cnt * ED_TXBUF_SIZE) << ED_PAGE_SHIFT);
	sc->mem_end = sc->mem_start + sc->mem_size;

	dp8390_stop(sc);

	kip->start = dp8390_ipkdb_init;
	kip->leave = dp8390_ipkdb_leave;
	kip->receive = dp8390_ipkdb_rcv;
	kip->send = dp8390_ipkdb_send;

	return 0;
}

/*
 * Similar to dp8390_init above.
 */
static void
dp8390_ipkdb_hwinit(struct ipkdb_if *kip)
{
	struct dp8390_softc *sc = kip->port;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	int i;

	sc->txb_inuse = 0;
	sc->txb_new = 0;
	sc->txb_next_tx = 0;
	dp8390_stop(sc);

	if (sc->dcr_reg & ED_DCR_LS)
		NIC_PUT(regt, regh, ED_P0_DCR, sc->dcr_reg);
	else
		NIC_PUT(regt, regh, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	NIC_PUT(regt, regh, ED_P0_RBCR0, 0);
	NIC_PUT(regt, regh, ED_P0_RBCR1, 0);
	NIC_PUT(regt, regh, ED_P0_RCR, ED_RCR_MON | sc->rcr_proto);
	NIC_PUT(regt, regh, ED_P0_TCR, ED_TCR_LB0);
	if (sc->is790)
		NIC_PUT(regt, regh, 0x09, 0);
	NIC_PUT(regt, regh, ED_P0_BNRY, sc->rec_page_start);
	NIC_PUT(regt, regh, ED_P0_PSTART, sc->rec_page_start);
	NIC_PUT(regt, regh, ED_P0_PSTOP, sc->rec_page_stop);
	NIC_PUT(regt, regh, ED_P0_IMR, 0);
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_ISR, 0xff);

	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	for (i = 0; i < sizeof kip->myenetaddr; i++)
		NIC_PUT(regt, regh, ED_P1_PAR0 + i, kip->myenetaddr[i]);
	/* multicast filter? */

	sc->next_packet = sc->rec_page_start + 1;
	NIC_PUT(regt, regh, ED_P1_CURR, sc->next_packet);

	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P1_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	/* promiscuous mode? */
	NIC_PUT(regt, regh, ED_P0_RCR, ED_RCR_AB | ED_RCR_AM | sc->rcr_proto);
	NIC_PUT(regt, regh, ED_P0_TCR, 0);

	/* card-specific initialization? */

	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	ifp->if_flags &= ~IFF_OACTIVE;
}

static void
dp8390_ipkdb_init(struct ipkdb_if *kip)
{
	struct dp8390_softc *sc = kip->port;
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	uint8_t cmd;

	cmd = NIC_GET(regt, regh, ED_P0_CR) & ~(ED_CR_PAGE_3 | ED_CR_STA);

	/* Select page 0 */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR, cmd | ED_CR_PAGE_0 | ED_CR_STP);
	NIC_BARRIER(regt, regh);

	/* If not started, init chip */
	if ((cmd & ED_CR_STP) != 0)
		dp8390_ipkdb_hwinit(kip);

	/* If output active, wait for packets to drain */
	while (sc->txb_inuse) {
		while ((cmd = (NIC_GET(regt, regh, ED_P0_ISR) &
		    (ED_ISR_PTX | ED_ISR_TXE))) == 0)
			DELAY(1);
		NIC_PUT(regt, regh, ED_P0_ISR, cmd);
		if (--sc->txb_inuse)
			dp8390_xmit(sc);
	}
}

static void
dp8390_ipkdb_leave(struct ipkdb_if *kip)
{
	struct dp8390_softc *sc = kip->port;
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	ifp->if_timer = 0;
}

/*
 * Similar to dp8390_intr above.
 */
static int
dp8390_ipkdb_rcv(struct ipkdb_if *kip, uint8_t *buf, int poll)
{
	struct dp8390_softc *sc = kip->port;
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	uint8_t bnry, current, isr;
	int len, nlen, packet_ptr;
	struct dp8390_ring packet_hdr;

	/* Switch to page 0. */
	NIC_BARRIER(regt, regh);
	NIC_PUT(regt, regh, ED_P0_CR,
	    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
	NIC_BARRIER(regt, regh);

	for (;;) {
		isr = NIC_GET(regt, regh, ED_P0_ISR);
		NIC_PUT(regt, regh, ED_P0_ISR, isr);

		if (isr & (ED_ISR_PRX | ED_ISR_TXE)) {
			NIC_GET(regt, regh, ED_P0_NCR);
			NIC_GET(regt, regh, ED_P0_TSR);
		}

		if (isr & ED_ISR_OVW) {
			dp8390_ipkdb_hwinit(kip);
			continue;
		}

		if (isr & ED_ISR_CNT) {
			NIC_GET(regt, regh, ED_P0_CNTR0);
			NIC_GET(regt, regh, ED_P0_CNTR1);
			NIC_GET(regt, regh, ED_P0_CNTR2);
		}

		/* Similar to dp8390_rint above. */
		NIC_BARRIER(regt, regh);
		NIC_PUT(regt, regh, ED_P0_CR,
		    sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);
		NIC_BARRIER(regt, regh);

		current = NIC_GET(regt, regh, ED_P1_CURR);

		NIC_BARRIER(regt, regh);
		NIC_PUT(regt, regh, ED_P1_CR,
		    sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
		NIC_BARRIER(regt, regh);

		if (sc->next_packet == current) {
			if (poll)
				return 0;
			continue;
		}

		packet_ptr = sc->mem_ring +
		    ((sc->next_packet - sc->rec_page_start) << ED_PAGE_SHIFT);
		sc->read_hdr(sc, packet_ptr, &packet_hdr);
		len = packet_hdr.count;
		nlen = packet_hdr.next_packet - sc->next_packet;
		if (nlen < 0)
			nlen += sc->rec_page_stop - sc->rec_page_start;
		nlen--;
		if ((len & ED_PAGE_MASK) + sizeof(packet_hdr) > ED_PAGE_SIZE)
			nlen--;
		len = (len & ED_PAGE_MASK) | (nlen << ED_PAGE_SHIFT);
		len -= sizeof(packet_hdr);

		if (len <= ETHERMTU &&
		    packet_hdr.next_packet >= sc->rec_page_start &&
		    packet_hdr.next_packet < sc->rec_page_stop) {
			sc->ring_copy(sc, packet_ptr + sizeof(packet_hdr),
			    buf, len);
			sc->next_packet = packet_hdr.next_packet;
			bnry = sc->next_packet - 1;
			if (bnry < sc->rec_page_start)
				bnry = sc->rec_page_stop - 1;
			NIC_PUT(regt, regh, ED_P0_BNRY, bnry);
			return len;
		}

		dp8390_ipkdb_hwinit(kip);
	}
}

static void
dp8390_ipkdb_send(struct ipkdb_if *kip, uint8_t *buf, int l)
{
	struct dp8390_softc *sc = kip->port;
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct mbuf mb;

	mb.m_next = NULL;
	mb.m_pkthdr.len = mb.m_len = l;
	mb.m_data = buf;
	mb.m_flags = M_EXT | M_PKTHDR;
	mb.m_type = MT_DATA;

	l = sc->write_mbuf(sc, &mb,
	    sc->mem_start + ((sc->txb_new * ED_TXBUF_SIZE) << ED_PAGE_SHIFT));
	sc->txb_len[sc->txb_new] = max(l, ETHER_MIN_LEN - ETHER_CRC_LEN);

	if (++sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	sc->txb_inuse++;
	dp8390_xmit(sc);

	while ((NIC_GET(regt, regh, ED_P0_ISR) &
	    (ED_ISR_PTX | ED_ISR_TXE)) == 0)
		DELAY(1);

	sc->txb_inuse--;
}
#endif
