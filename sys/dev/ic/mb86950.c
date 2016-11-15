/*	$NetBSD: mb86950.c,v 1.22 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

 /*
  * Portions copyright (c) 1995 Mika Kortelainen
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
  * 3. All advertising materials mentioning features or use of this software
  *    must display the following acknowledgement:
  *      This product includes software developed by  Mika Kortelainen
  * 4. The name of the author may not be used to endorse or promote products
  *    derived from this software without specific prior written permission
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

 /*
  * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
  * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
  */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mb86950.c,v 1.22 2015/04/13 16:33:24 riastradh Exp $");

/*
 * Device driver for Fujitsu mb86950 based Ethernet cards.
 * Adapted by Dave J. Barnes from various Internet sources including
 * mb86960.c (NetBSD), if_qn.c (NetBSD/Amiga), DOS Packet Driver (Brian Fisher,
 * Queens University), EtherBoot Driver (Ken Yap).
 */

/* XXX There are still rough edges......
 *
 * (1) There is no watchdog timer for the transmitter. It's doubtful that
 *     transmit from the chip could be restarted without a hardware reset
 *     though. (Fixed - not fully tested)
 *
 * (2) The media interface callback goo is broke.  No big deal since to change
 *     from aui to bnc on the old Tiara LANCard requires moving 8 board jumpers.
 *     Other cards (SMC ?) using the EtherStar chip may support media change
 *     via software. (Fixed - tested)
 *
 * (3) The maximum outstanding transmit packets is set to 4.  What
 *     is a good limit of outstanding transmit packets for the EtherStar?
 *     Is there a way to tell how many bytes are remaining to be
 *     transmitted? [no]
---
	When the EtherStar was designed, CPU power was a fraction
	of what it is now.  The single EtherStar transmit buffer
	was fine.  It was unlikely that the CPU could outrun the
	EtherStar. However, things in 2004 are quite different.
	sc->txb_size is used to keep the CPU from overrunning the
	EtherStar.  At most allow one packet transmitting and one
	going into the fifo.

---
    No, that isn't right either :(

 * (4) Multicast isn't supported.  Feel free to add multicast code
 *     if you know how to make the EtherStar do multicast.  Otherwise
 *     you'd have to use promiscuous mode and do multicast in software. OUCH!
 *
 * (5) There are no bus_space_barrier calls used. Are they needed? Maybe not.
 *
 * (6) Access to the fifo assumes word (16 bit) mode.  Cards configured for
 *     byte wide fifo access will require driver code changes.
 *
 * Only the minimum code necessary to make the Tiara LANCard work
 * has been tested. Other cards may require more work, especially
 * byte mode fifo and if DMA is used.
 *
 * djb / 2004
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
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

#include <dev/ic/mb86950reg.h>
#include <dev/ic/mb86950var.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

/* Standard driver entry points.  These can be static. */
int		mb86950_ioctl(struct ifnet *, u_long, void *);
void	mb86950_init(struct mb86950_softc *);
void	mb86950_start(struct ifnet *);
void	mb86950_watchdog(struct ifnet *);
void	mb86950_reset(struct mb86950_softc *);

/* Local functions. */
void	mb86950_stop(struct mb86950_softc *);
void	mb86950_tint(struct mb86950_softc *, u_int8_t);
void	mb86950_rint(struct mb86950_softc *, u_int8_t);
int		mb86950_get_fifo(struct mb86950_softc *, u_int);
ushort	mb86950_put_fifo(struct mb86950_softc *, struct mbuf *);
void	mb86950_drain_fifo(struct mb86950_softc *);

int		mb86950_mediachange(struct ifnet *);
void	mb86950_mediastatus(struct ifnet *, struct ifmediareq *);


#if ESTAR_DEBUG >= 1
void	mb86950_dump(int, struct mb86950_softc *);
#endif

/********************************************************************/

void
mb86950_attach(struct mb86950_softc *sc, u_int8_t *myea)
{

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		printf("%s: ethernet address shouldn't be NULL\n",
		    device_xname(sc->sc_dev));
		panic("NULL ethernet address");
	}
#endif

	/* Initialize 86950. */
	mb86950_stop(sc);

	memcpy(sc->sc_enaddr, myea, sizeof(sc->sc_enaddr));

	sc->sc_stat |= ESTAR_STAT_ENABLED;
}

/*
 * Stop everything on the interface.
 *
 * All buffered packets, both transmitting and receiving,
 * if any, will be lost by stopping the interface.
 */
void
mb86950_stop(struct mb86950_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Stop interface hardware. */
	bus_space_write_1(bst, bsh, DLCR_CONFIG, DISABLE_DLC);
	delay(200);

	/* Disable interrupts. */
	bus_space_write_1(bst, bsh, DLCR_TX_INT_EN, 0);
	bus_space_write_1(bst, bsh, DLCR_RX_INT_EN, 0);

	/* Ack / Clear all interrupt status. */
	bus_space_write_1(bst, bsh, DLCR_TX_STAT, 0xff);
	bus_space_write_1(bst, bsh, DLCR_RX_STAT, 0xff);

	/* Clear DMA Bit */
    bus_space_write_2(bst, bsh, BMPR_DMA, 0);

    /* accept no packets */
	bus_space_write_1(bst, bsh, DLCR_TX_MODE, 0);
	bus_space_write_1(bst, bsh, DLCR_RX_MODE, 0);

    mb86950_drain_fifo(sc);

}

void
mb86950_drain_fifo(struct mb86950_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Read data until bus read error (i.e. buffer empty). */
	/* XXX There ought to be a better way, eats CPU and bothers the chip */
	while (!(bus_space_read_1(bst, bsh, DLCR_RX_STAT) & RX_BUS_RD_ERR))
		bus_space_read_2(bst, bsh, BMPR_FIFO);
	/* XXX */

	/* Clear Bus Rd Error */
	bus_space_write_1(bst, bsh, DLCR_RX_STAT, RX_BUS_RD_ERR);
}

/*
 * Install interface into kernel networking data structures
 */
void
mb86950_config(struct mb86950_softc *sc, int *media,
    int nmedia, int defmedia)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = mb86950_start;
	ifp->if_ioctl = mb86950_ioctl;
	ifp->if_watchdog = mb86950_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize media goo. */
	/* XXX The Tiara LANCard uses board jumpers to change media.
	 *       This code may have to be changed for other cards.
	 */
	ifmedia_init(&sc->sc_media, 0, mb86950_mediachange, mb86950_mediastatus);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_MANUAL, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_MANUAL);

	/* Attach the interface. */
	if_attach(ifp);

	/* Feed the chip the station address. */
	bus_space_write_region_1(bst, bsh, DLCR_NODE_ID, sc->sc_enaddr, ETHER_ADDR_LEN);

	ether_ifattach(ifp, sc->sc_enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

/* XXX No! This doesn't work - DLCR6 of the mb86950 is different

	bus_space_write_1(bst, bsh, DLCR_CONFIG, 0x0f);
	buf_config = bus_space_read_1(bst, bsh, DLCR_CONFIG);

	sc->txb_count = ((buf_config & 0x0c) ? 2 : 1);
	sc->txb_size = 1024 * (2 << ((buf_config & 0x0c) ? (((buf_config & 0x0c) >> 2) - 1) : 0));
	sc->txb_free = (sc->txb_size * sc->txb_count) / 1500;

  	sc->rxb_size = ((8 << (buf_config & 3)) * 1024) - (sc->txb_size * sc->txb_count);
	sc->rxb_max = sc->rxb_size / 64;

	printf("mb86950: Buffer Size %dKB with %d transmit buffer(s) %dKB each.\n",
		(8 << (buf_config & 3)), sc->txb_count,	(sc->txb_size / 1024));
	printf("         Transmit Buffer Space for %d maximum sized packet(s).\n",sc->txb_free);
	printf("         System Bus Width %d bits, Buffer Memory %d bits.\n",
		((buf_config & 0x20) ? 8 : 16),
		((buf_config & 0x10) ? 8 : 16));

*/

	/* Set reasonable values for number of packet flow control if not
	 * set elsewhere */
	if (sc->txb_num_pkt == 0) sc->txb_num_pkt = 1;
	if (sc->rxb_num_pkt == 0) sc->rxb_num_pkt = 100;

	/* Print additional info when attached. */
	printf("%s: Ethernet address %s\n", device_xname(sc->sc_dev),
	    ether_sprintf(sc->sc_enaddr));

	/* The attach is successful. */
	sc->sc_stat |= ESTAR_STAT_ATTACHED;
}

/*
 * Media change callback.
 */
int
mb86950_mediachange(struct ifnet *ifp)
{

	struct mb86950_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return ((*sc->sc_mediachange)(sc));

	return (0);
}

/*
 * Media status callback.
 */
void
mb86950_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mb86950_softc *sc = ifp->if_softc;

	if ((sc->sc_stat & ESTAR_STAT_ENABLED) == 0) {
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
mb86950_reset(struct mb86950_softc *sc)
{
	int s;

	s = splnet();
	log(LOG_ERR, "%s: device reset\n", device_xname(sc->sc_dev));
	mb86950_stop(sc);
	mb86950_init(sc);
	splx(s);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
mb86950_watchdog(struct ifnet *ifp)
{
	struct mb86950_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t tstat;

	/* verbose watchdog messages for debugging timeouts */
    if ((tstat = bus_space_read_1(bst, bsh, DLCR_TX_STAT)) != 0) {
		if (tstat & TX_CR_LOST) {
			if ((tstat & (TX_COL | TX_16COL)) == 0) {
				 log(LOG_ERR, "%s: carrier lost\n",
				    device_xname(sc->sc_dev));
			} else {
				log(LOG_ERR, "%s: excessive collisions\n",
				    device_xname(sc->sc_dev));
			}
		}
		else if ((tstat & (TX_UNDERFLO | TX_BUS_WR_ERR)) != 0) {
			log(LOG_ERR, "%s: tx fifo underflow/overflow\n",
			    device_xname(sc->sc_dev));
		} else {
			log(LOG_ERR, "%s: transmit error\n",
			    device_xname(sc->sc_dev));
		}
	} else {
		log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	}

	/* Don't know how many packets are lost by this accident.
	 *  ... So just errors = errors + 1
	 */
	ifp->if_oerrors++;

	mb86950_reset(sc);

}

/*
 ******************** IOCTL
 * Process an ioctl request.
 */
int
mb86950_ioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct mb86950_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;

	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		/* XXX deprecated ? What should I use instead? */
		if ((error = mb86950_enable(sc)) != 0)
			break;

		ifp->if_flags |= IFF_UP;

		mb86950_init(sc);
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
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			mb86950_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			mb86950_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = mb86950_enable(sc)) != 0)
				break;
			mb86950_init(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
#if 0
			/* Setmode not supported */
			mb86950_setmode(sc);
#endif
			break;
		case 0:
			break;
		}

#if ESTAR_DEBUG >= 1
		/* "ifconfig fe0 debug" to print register dump. */
		if (ifp->if_flags & IFF_DEBUG) {
			log(LOG_INFO, "%s: SIOCSIFFLAGS(DEBUG)\n",
			    device_xname(sc->sc_dev));
			mb86950_dump(LOG_DEBUG, sc);
		}
#endif
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
	return (error);
}

/*
 * Initialize device.
 */
void
mb86950_init(struct mb86950_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	/* Reset transmitter flags. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	sc->txb_sched = 0;

	bus_space_write_1(bst, bsh, DLCR_TX_MODE, LBC);
	bus_space_write_1(bst, bsh, DLCR_RX_MODE, NORMAL_MODE);

	/* Enable interrupts. */
	bus_space_write_1(bst, bsh, DLCR_TX_INT_EN, TX_MASK);
	bus_space_write_1(bst, bsh, DLCR_RX_INT_EN, RX_MASK);

	/* Enable transmitter and receiver. */
	bus_space_write_1(bst, bsh, DLCR_CONFIG, ENABLE_DLC);
	delay(200);

	/* Set 'running' flag. */
	ifp->if_flags |= IFF_RUNNING;

	/* ...and attempt to start output. */
	mb86950_start(ifp);

}

void
mb86950_start(struct ifnet *ifp)
{
	struct mb86950_softc *sc = ifp->if_softc;
    bus_space_tag_t bst = sc->sc_bst;
    bus_space_handle_t bsh = sc->sc_bsh;
	struct mbuf *m;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
		return;

	/* Tap off here if there is a BPF listener. */
	bpf_mtap(ifp, m);

	/* Send the packet to the mb86950 */
	len = mb86950_put_fifo(sc,m);
	m_freem(m);

	/* XXX bus_space_barrier here ? */
	if (bus_space_read_1(bst, bsh, DLCR_TX_STAT) & (TX_UNDERFLO | TX_BUS_WR_ERR)) {
		log(LOG_ERR, "%s: tx fifo underflow/overflow\n", device_xname(sc->sc_dev));
	}

	bus_space_write_2(bst, bsh, BMPR_TX_LENGTH, len | TRANSMIT_START);

	bus_space_write_1(bst, bsh, DLCR_TX_INT_EN, TX_MASK);
	/* XXX                          */
	sc->txb_sched++;

	/* We have space for 'n' transmit packets of size 'mtu. */
	if (sc->txb_sched > sc->txb_num_pkt) {
		ifp->if_flags |= IFF_OACTIVE;
		ifp->if_timer = 2;
	}
}

/*
 * Send packet - copy packet from mbuf to the fifo
 */
u_short
mb86950_put_fifo(struct mb86950_softc *sc, struct mbuf *m)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_short *data;
	u_char savebyte[2];
	int len, len1, wantbyte;
	u_short totlen;

	memset(savebyte, 0, sizeof(savebyte));	/* XXX gcc */

	totlen = wantbyte = 0;

	for (; m != NULL; m = m->m_next) {
		data = mtod(m, u_short *);
		len = m->m_len;
		if (len > 0) {
			totlen += len;

			/* Finish the last word. */
			if (wantbyte) {
				savebyte[1] = *((u_char *)data);
				bus_space_write_2(bst, bsh, BMPR_FIFO, *savebyte);
				data = (u_short *)((u_char *)data + 1);
				len--;
				wantbyte = 0;
			}
			/* Output contiguous words. */
			if (len > 1) {
				len1 = len/2;
				bus_space_write_multi_stream_2(bst, bsh, BMPR_FIFO, data, len1);
				data += len1;
				len &= 1;
			}
			/* Save last byte, if necessary. */
			if (len == 1) {
				savebyte[0] = *((u_char *)data);
				wantbyte = 1;
			}
		}
	}

	if (wantbyte) {
		savebyte[1] = 0;
		bus_space_write_2(bst, bsh, BMPR_FIFO, *savebyte);
	}

	if (totlen < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {

		/* Fill the rest of the packet with zeros. */
		/* XXX Replace this mess with something else, eats CPU */
		/* The zero fill and last byte ought to be combined somehow */
		for(len = totlen + 1; len < (ETHER_MIN_LEN - ETHER_CRC_LEN); len += 2)
	  		bus_space_write_2(bst, bsh, BMPR_FIFO, 0);
		/* XXX                                       */

		totlen = (ETHER_MIN_LEN - ETHER_CRC_LEN);
	}

	return (totlen);
}

/*
 * Handle interrupts.
 * Ethernet interface interrupt processor
 */
int
mb86950_intr(void *arg)
{
	struct mb86950_softc *sc = arg;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	u_int8_t tstat, rstat;

	/* Get interrupt status. */
	tstat = bus_space_read_1(bst, bsh, DLCR_TX_STAT);
	rstat = bus_space_read_1(bst, bsh, DLCR_RX_STAT);

	if (tstat == 0 && rstat == 0) return (0);

	/* Disable etherstar interrupts so that we won't miss anything. */
	bus_space_write_1(bst, bsh, DLCR_TX_INT_EN, 0);
	bus_space_write_1(bst, bsh, DLCR_RX_INT_EN, 0);

	/*
	 * Handle transmitter interrupts. Handle these first because
	 * the receiver will reset the board under some conditions.
	 */
	if (tstat != 0) {

		mb86950_tint(sc, tstat);

		/* acknowledge transmit interrupt status. */
		bus_space_write_1(bst, bsh, DLCR_TX_STAT, tstat);

	}

	/* Handle receiver interrupts. */
	if (rstat != 0) {

		mb86950_rint(sc, rstat);

		/* acknowledge receive interrupt status. */
		bus_space_write_1(bst, bsh, DLCR_RX_STAT, rstat);

	}

	/* If tx still pending reset tx interrupt mask */
	if (sc->txb_sched > 0)
		bus_space_write_1(bst, bsh, DLCR_TX_INT_EN, TX_MASK);

	/*
	 * If it looks like the transmitter can take more data,
	 * attempt to start output on the interface. This is done
	 * after handling the receiver interrupt to give the
	 * receive operation priority.
	 */

	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		mb86950_start(ifp);

	/* Set receive interrupts back */
	bus_space_write_1(bst, bsh, DLCR_RX_INT_EN, RX_MASK);

	return(1);
}

/* Transmission interrupt handler */
void
mb86950_tint(struct mb86950_softc *sc, u_int8_t tstat)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int col;

	if (tstat & (TX_UNDERFLO | TX_BUS_WR_ERR)) {
		/* XXX What do we need to do here? reset ? */
		ifp->if_oerrors++;
	}

	/* excessive collision */
	if (tstat & TX_16COL) {
		ifp->if_collisions += 16;
		/* 16 collisions means that the packet has been thrown away. */
		if (sc->txb_sched > 0)
			sc->txb_sched--;
	}

	/* transmission complete. */
	if (tstat & TX_DONE) {
		/* successfully transmitted packets ++. */
		ifp->if_opackets++;
		if (sc->txb_sched > 0)
			sc->txb_sched--;

		/* Collision count valid only when TX_DONE is set */
		if (tstat & TX_COL) {
			col = (bus_space_read_1(bst, bsh, DLCR_TX_MODE) & COL_MASK) >> 4;
			ifp->if_collisions = ifp->if_collisions + col;
		}
	}

	if (sc->txb_sched == 0) {
		 /* Reset output active flag and stop timer. */
		 ifp->if_flags &= ~IFF_OACTIVE;
		 ifp->if_timer = 0;
	}
}

/* receiver interrupt. */
void
mb86950_rint(struct mb86950_softc *sc, u_int8_t rstat)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	u_int status, len;
	int i;

	 /* Update statistics if this interrupt is caused by an error. */
	 if (rstat & RX_ERR_MASK) {

		/* tried to read past end of fifo, should be harmless
		 * count everything else
		 */
		if ((rstat & RX_BUS_RD_ERR) == 0) {
			ifp->if_ierrors++;
		}
	}

	/*
	 * mb86950 has a flag indicating "receive buffer empty."
	 * We just loop checking the flag to pull out all received
	 * packets.
	 *
	 * We limit the number of iterrations to avoid infinite loop.
	 * It can be caused by a very slow CPU (some broken
	 * peripheral may insert incredible number of wait cycles)
	 * or, worse, by a broken mb86950 chip.
	 */
	for (i = 0; i < sc->rxb_num_pkt; i++) {
		/* Stop the iterration if 86950 indicates no packets. */
		if (bus_space_read_1(bst, bsh, DLCR_RX_MODE) & RX_BUF_EMTY)
			break;

		/* receive packet status */
		status = bus_space_read_2(bst, bsh, BMPR_FIFO);

		/* bad packet? */
		if ((status & GOOD_PKT) == 0) {
			ifp->if_ierrors++;
			mb86950_drain_fifo(sc);
			continue;
		}

		/* Length valid ? */
		len = bus_space_read_2(bst, bsh, BMPR_FIFO);

		if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN) || len < ETHER_HDR_LEN) {
			ifp->if_ierrors++;
			mb86950_drain_fifo(sc);
			continue;
		}

		if (mb86950_get_fifo(sc, len) != 0) {
			/* No mbufs? Drop packet. */
			ifp->if_ierrors++;
			mb86950_drain_fifo(sc);
			return;
		}

		/* Successfully received a packet.  Update stat. */
		ifp->if_ipackets++;
	}
}

/*
 * Receive packet.
 * Retrieve packet from receive buffer and send to the next level up via
 * ether_input(). If there is a BPF listener, give a copy to BPF, too.
 * Returns 0 if success, -1 if error (i.e., mbuf allocation failure).
 */
int
mb86950_get_fifo(struct mb86950_softc *sc, u_int len)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m;

	/* Allocate a header mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (-1);

	/*
	 * Round len to even value.
	 */
	if (len & 1)
		len++;

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;

	/* The following silliness is to make NFS happy. */
#define	EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define	EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes) to allocate a cluster.  For a
	 * packet of a size between (MHLEN - 2) to (MINCLSIZE - 2),
	 * our code violates the rule...
	 * On the other hand, the current code is short, simple,
	 * and fast, however.  It does no harmful thing, just wastes
	 * some memory.  Any comments?  FIXME.
	 */

	/* Attach a cluster if this packet doesn't fit in a normal mbuf. */
	if (len > MHLEN - EOFF) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return (-1);
		}
	}

	/*
	 * The following assumes there is room for the ether header in the
	 * header mbuf.
	 */
	m->m_data += EOFF;

	/* Set the length of this packet. */
	m->m_len = len;

	/* Get a packet. */
	bus_space_read_multi_stream_2(bst, bsh, BMPR_FIFO, mtod(m, u_int16_t *), (len + 1) >> 1);

	/*
	 * Check if there's a BPF listener on this interface.  If so, hand off
	 * the raw packet to bpf.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);
	return (0);
}

/*
 * Enable power on the interface.
 */
int
mb86950_enable(struct mb86950_softc *sc)
{

	if ((sc->sc_stat & ESTAR_STAT_ENABLED) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
	}

	sc->sc_stat |= ESTAR_STAT_ENABLED;
	return (0);
}

/*
 * Disable power on the interface.
 */
void
mb86950_disable(struct mb86950_softc *sc)
{

	if ((sc->sc_stat & ESTAR_STAT_ENABLED) != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_stat &= ~ESTAR_STAT_ENABLED;
	}
}

/*
 * mbe_activate:
 *
 *	Handle device activation/deactivation requests.
 */
int
mb86950_activate(device_t self, enum devact act)
{
	struct mb86950_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * mb86950_detach:
 *
 *	Detach a mb86950 interface.
 */
int
mb86950_detach(struct mb86950_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	/* Succeed now if there's no work to do. */
	if ((sc->sc_stat & ESTAR_STAT_ATTACHED) == 0)
		return (0);

	/* Delete all media. */
	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);

	/* Unhook the entropy source. */
	rnd_detach_source(&sc->rnd_source);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (0);
}

#if ESTAR_DEBUG >= 1
void
mb86950_dump(int level, struct mb86950_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	log(level, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, DLCR_TX_STAT),
	    bus_space_read_1(bst, bsh, DLCR_TX_INT_EN),
	    bus_space_read_1(bst, bsh, DLCR_RX_STAT),
	    bus_space_read_1(bst, bsh, DLCR_RX_INT_EN),
	    bus_space_read_1(bst, bsh, DLCR_TX_MODE),
	    bus_space_read_1(bst, bsh, DLCR_RX_MODE),
	    bus_space_read_1(bst, bsh, DLCR_CONFIG));

	/* XXX BMPR2, 4 write only ?
	log(level, "\tBMPR = xxxx %04x %04x\n",
		bus_space_read_2(bst, bsh, BMPR_TX_LENGTH),
		bus_space_read_2(bst, bsh, BMPR_DMA));
	*/
}
#endif
