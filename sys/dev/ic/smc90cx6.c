/*	$NetBSD: smc90cx6.c,v 1.64 2012/10/27 17:18:22 chs Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * Chip core driver for the SMC90c26 / SMC90c56 (and SMC90c66 in '56
 * compatibility mode) boards
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smc90cx6.c,v 1.64 2012/10/27 17:18:22 chs Exp $");

/* #define BAHSOFTCOPY */
#define BAHRETRANSMIT /**/

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_types.h>
#include <net/if_arc.h>

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
#include <sys/cpu.h>

#include <dev/ic/smc90cx6reg.h>
#include <dev/ic/smc90cx6var.h>

/* these should be elsewhere */

#define ARC_MIN_LEN 1
#define ARC_MIN_FORBID_LEN 254
#define ARC_MAX_FORBID_LEN 256
#define ARC_MAX_LEN 508
#define ARC_ADDR_LEN 1

/* for watchdog timer. This should be more than enough. */
#define ARCTIMEOUT (5*IFNET_SLOWHZ)

/*
 * This currently uses 2 bufs for tx, 2 for rx
 *
 * New rx protocol:
 *
 * rx has a fillcount variable. If fillcount > (NRXBUF-1),
 * rx can be switched off from rx hard int.
 * Else rx is restarted on the other receiver.
 * rx soft int counts down. if it is == (NRXBUF-1), it restarts
 * the receiver.
 * To ensure packet ordering (we need that for 1201 later), we have a counter
 * which is incremented modulo 256 on each receive and a per buffer
 * variable, which is set to the counter on filling. The soft int can
 * compare both values to determine the older packet.
 *
 * Transmit direction:
 *
 * bah_start checks tx_fillcount
 * case 2: return
 *
 * else fill tx_act ^ 1 && inc tx_fillcount
 *
 * check tx_fillcount again.
 * case 2: set IFF_OACTIVE to stop arc_output from filling us.
 * case 1: start tx
 *
 * tint clears IFF_OCATIVE, decrements and checks tx_fillcount
 * case 1: start tx on tx_act ^ 1, softcall bah_start
 * case 0: softcall bah_start
 *
 * #define fill(i) get mbuf && copy mbuf to chip(i)
 */

void	bah_init(struct bah_softc *);
void	bah_reset(struct bah_softc *);
void	bah_stop(struct bah_softc *);
void	bah_start(struct ifnet *);
int	bahintr(void *);
int	bah_ioctl(struct ifnet *, unsigned long, void *);
void	bah_watchdog(struct ifnet *);
void	bah_srint(void *vsc);
static	void bah_tint(struct bah_softc *, int);
void	bah_reconwatch(void *);

/* short notation */

#define GETREG(off)	bus_space_read_1(bst_r, regs, (off))
#define PUTREG(off, v)	bus_space_write_1(bst_r, regs, (off), (v))
#define GETMEM(off)	bus_space_read_1(bst_m, mem, (off))
#define PUTMEM(off, v)	bus_space_write_1(bst_m, mem, (off), (v))

void
bah_attach_subr(struct bah_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arccom.ac_if;
	int s;
	u_int8_t linkaddress;

	bus_space_tag_t bst_r = sc->sc_bst_r;
	bus_space_tag_t bst_m = sc->sc_bst_m;
	bus_space_handle_t regs = sc->sc_regs;
	bus_space_handle_t mem = sc->sc_mem;

#if (defined(BAH_DEBUG) && (BAH_DEBUG > 2))
	printf("\n%s: attach(0x%x, 0x%x, 0x%x)\n",
	    device_xname(sc->sc_dev), parent, self, aux);
#endif
	s = splhigh();

	/*
	 * read the arcnet address from the board
	 */

	(*sc->sc_reset)(sc, 1);

	do {
		delay(200);
	} while (!(GETREG(BAHSTAT) & BAH_POR));

	linkaddress = GETMEM(BAHMACOFF);

	printf(": link addr 0x%02x(%d)\n", linkaddress, linkaddress);

	/* clear the int mask... */

	sc->sc_intmask = 0;
	PUTREG(BAHSTAT, 0);

	PUTREG(BAHCMD, BAH_CONF(CONF_LONG));
	PUTREG(BAHCMD, BAH_CLR(CLR_POR|CLR_RECONFIG));
	sc->sc_recontime = sc->sc_reconcount = 0;

	/* and reenable kernel int level */
	splx(s);

	/*
	 * set interface to stopped condition (reset)
	 */
	bah_stop(sc);

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = bah_start;
	ifp->if_ioctl = bah_ioctl;
	ifp->if_timer = 0;
	ifp->if_watchdog  = bah_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;

	ifp->if_mtu = ARCMTU;

	arc_ifattach(ifp, linkaddress);

#ifdef BAHSOFTCOPY
	sc->sc_rxcookie = softint_establish(SOFTINT_NET, bah_srint, sc);
	sc->sc_txcookie = softint_establish(SOFTINT_NET,
		(void (*)(void *))bah_start, ifp);
#endif

	callout_init(&sc->sc_recon_ch, 0);
}

/*
 * Initialize device
 *
 */
void
bah_init(struct bah_softc *sc)
{
	struct ifnet *ifp;
	int s;

	ifp = &sc->sc_arccom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		s = splnet();
		ifp->if_flags |= IFF_RUNNING;
		bah_reset(sc);
		bah_start(ifp);
		splx(s);
	}
}

/*
 * Reset the interface...
 *
 * this assumes that it is called inside a critical section...
 *
 */
void
bah_reset(struct bah_softc *sc)
{
	struct ifnet *ifp;
	uint8_t linkaddress;

	bus_space_tag_t bst_r = sc->sc_bst_r;
        bus_space_tag_t bst_m = sc->sc_bst_m;
	bus_space_handle_t regs = sc->sc_regs;
	bus_space_handle_t mem = sc->sc_mem;

	ifp = &sc->sc_arccom.ac_if;

#ifdef BAH_DEBUG
	printf("%s: reset\n", device_xname(sc->sc_dev));
#endif
	/* stop and restart hardware */

	(*sc->sc_reset)(sc, 1);
	do {
		DELAY(200);
	} while (!(GETREG(BAHSTAT) & BAH_POR));

	linkaddress = GETMEM(BAHMACOFF);

#if defined(BAH_DEBUG) && (BAH_DEBUG > 2)
	printf("%s: reset: card reset, link addr = 0x%02x (%ld)\n",
	    device_xname(sc->sc_dev), linkaddress, linkaddress);
#endif

	/* tell the routing level about the (possibly changed) link address */
	if_set_sadl(ifp, &linkaddress, sizeof(linkaddress), false);

	/* POR is NMI, but we need it below: */
	sc->sc_intmask = BAH_RECON|BAH_POR;
	PUTREG(BAHSTAT, sc->sc_intmask);
	PUTREG(BAHCMD, BAH_CONF(CONF_LONG));

#ifdef BAH_DEBUG
	printf("%s: reset: chip configured, status=0x%02x\n",
	    device_xname(sc->sc_dev), GETREG(BAHSTAT));
#endif
	PUTREG(BAHCMD, BAH_CLR(CLR_POR|CLR_RECONFIG));

#ifdef BAH_DEBUG
	printf("%s: reset: bits cleared, status=0x%02x\n",
	    device_xname(sc->sc_dev), GETREG(BAHSTAT));
#endif

	sc->sc_reconcount_excessive = ARC_EXCESSIVE_RECONS;

	/* start receiver */

	sc->sc_intmask  |= BAH_RI;
	sc->sc_rx_fillcount = 0;
	sc->sc_rx_act = 2;

	PUTREG(BAHCMD, BAH_RXBC(2));
	PUTREG(BAHSTAT, sc->sc_intmask);

#ifdef BAH_DEBUG
	printf("%s: reset: started receiver, status=0x%02x\n",
	    device_xname(sc->sc_dev), GETREG(BAHSTAT));
#endif

	/* and init transmitter status */
	sc->sc_tx_act = 0;
	sc->sc_tx_fillcount = 0;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	bah_start(ifp);
}

/*
 * Take interface offline
 */
void
bah_stop(struct bah_softc *sc)
{
	bus_space_tag_t bst_r = sc->sc_bst_r;
	bus_space_handle_t regs = sc->sc_regs;

	/* Stop the interrupts */
	PUTREG(BAHSTAT, 0);

	/* Stop the interface */
	(*sc->sc_reset)(sc, 0);

	/* Stop watchdog timer */
	sc->sc_arccom.ac_if.if_timer = 0;
}

/*
 * Start output on interface. Get another datagram to send
 * off the interface queue, and copy it to the
 * interface before starting the output
 *
 * this assumes that it is called inside a critical section...
 * XXX hm... does it still?
 *
 */
void
bah_start(struct ifnet *ifp)
{
	struct bah_softc *sc = ifp->if_softc;
	struct mbuf *m,*mp;

	bus_space_tag_t bst_r = sc->sc_bst_r;
	bus_space_handle_t regs = sc->sc_regs;
	bus_space_tag_t bst_m = sc->sc_bst_m;
	bus_space_handle_t mem = sc->sc_mem;

	int bah_ram_ptr;
	int len, tlen, offset, s, buffer;
#ifdef BAHTIMINGS
	u_long copystart, lencopy, perbyte;
#endif

#if defined(BAH_DEBUG) && (BAH_DEBUG > 3)
	printf("%s: start(0x%x)\n", device_xname(sc->sc_dev), ifp);
#endif

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	s = splnet();

	if (sc->sc_tx_fillcount >= 2) {
		splx(s);
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m);
	buffer = sc->sc_tx_act ^ 1;

	splx(s);

	if (m == 0)
		return;

	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire
	 *
	 * (can't give the copy in A2060 card RAM to bpf, because
	 * that RAM is just accessed as on every other byte)
	 */
	bpf_mtap(ifp, m);

#ifdef BAH_DEBUG
	if (m->m_len < ARC_HDRLEN)
		m = m_pullup(m, ARC_HDRLEN);/* gcc does structure padding */
	printf("%s: start: filling %ld from %ld to %ld type %ld\n",
	    device_xname(sc->sc_dev), buffer, mtod(m, u_char *)[0],
	    mtod(m, u_char *)[1], mtod(m, u_char *)[2]);
#else
	if (m->m_len < 2)
		m = m_pullup(m, 2);
#endif
	bah_ram_ptr = buffer*512;

	if (m == 0)
		return;

	/* write the addresses to RAM and throw them away */

	/*
	 * Hardware does this: Yet Another Microsecond Saved.
	 * (btw, timing code says usually 2 microseconds)
	 * PUTMEM(bah_ram_ptr + 0, mtod(m, u_char *)[0]);
	 */

	PUTMEM(bah_ram_ptr + 1, mtod(m, u_char *)[1]);
	m_adj(m, 2);

	/* get total length left at this point */
	tlen = m->m_pkthdr.len;
	if (tlen < ARC_MIN_FORBID_LEN) {
		offset = 256 - tlen;
		PUTMEM(bah_ram_ptr + 2, offset);
	} else {
		PUTMEM(bah_ram_ptr + 2, 0);
		if (tlen <= ARC_MAX_FORBID_LEN)
			offset = 255;		/* !!! */
		else {
			if (tlen > ARC_MAX_LEN)
				tlen = ARC_MAX_LEN;
			offset = 512 - tlen;
		}
		PUTMEM(bah_ram_ptr + 3, offset);

	}
	bah_ram_ptr += offset;

	/* lets loop through the mbuf chain */

	for (mp = m; mp; mp = mp->m_next) {
		if ((len = mp->m_len)) {		/* YAMS */
			bus_space_write_region_1(bst_m, mem, bah_ram_ptr,
			    mtod(mp, void *), len);

			bah_ram_ptr += len;
		}
	}

	sc->sc_broadcast[buffer] = (m->m_flags & M_BCAST) != 0;
	sc->sc_retransmits[buffer] = (m->m_flags & M_BCAST) ? 1 : 5;

	/* actually transmit the packet */
	s = splnet();

	if (++sc->sc_tx_fillcount > 1) {
		/*
		 * We are filled up to the rim. No more bufs for the moment,
		 * please.
		 */
		ifp->if_flags |= IFF_OACTIVE;
	} else {
#ifdef BAH_DEBUG
		printf("%s: start: starting transmitter on buffer %d\n",
		    device_xname(sc->sc_dev), buffer);
#endif
		/* Transmitter was off, start it */
		sc->sc_tx_act = buffer;

		/*
		 * We still can accept another buf, so don't:
		 * ifp->if_flags |= IFF_OACTIVE;
		 */
		sc->sc_intmask |= BAH_TA;
		PUTREG(BAHCMD, BAH_TX(buffer));
		PUTREG(BAHSTAT, sc->sc_intmask);

		sc->sc_arccom.ac_if.if_timer = ARCTIMEOUT;
	}
	splx(s);
	m_freem(m);

	/*
	 * After 10 times reading the docs, I realized
	 * that in the case the receiver NAKs the buffer request,
	 * the hardware retries till shutdown.
	 * This is integrated now in the code above.
	 */

	return;
}

/*
 * Arcnet interface receiver soft interrupt:
 * get the stuff out of any filled buffer we find.
 */
void
bah_srint(void *vsc)
{
	struct bah_softc *sc = (struct bah_softc *)vsc;
	int buffer, len, len1, amount, offset, s, type;
	int bah_ram_ptr;
	struct mbuf *m, *dst, *head;
	struct arc_header *ah;
	struct ifnet *ifp;

	bus_space_tag_t bst_r = sc->sc_bst_r;
        bus_space_tag_t bst_m = sc->sc_bst_m;
	bus_space_handle_t regs = sc->sc_regs;
	bus_space_handle_t mem = sc->sc_mem;

	ifp = &sc->sc_arccom.ac_if;
	head = 0;

	s = splnet();
	buffer = sc->sc_rx_act ^ 1;
	splx(s);

	/* Allocate header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == 0) {
		/*
	 	 * in case s.th. goes wrong with mem, drop it
	 	 * to make sure the receiver can be started again
		 * count it as input error (we dont have any other
		 * detectable)
	 	 */
		ifp->if_ierrors++;
		goto cleanup;
	}

	m->m_pkthdr.rcvif = ifp;

	/*
	 * Align so that IP packet will be longword aligned. Here we
	 * assume that m_data of new packet is longword aligned.
	 * When implementing PHDS, we might have to change it to 2,
	 * (2*sizeof(ulong) - ARC_HDRNEWLEN)), packet type dependent.
	 */

	bah_ram_ptr = buffer*512;
	offset = GETMEM(bah_ram_ptr + 2);
	if (offset)
		len = 256 - offset;
	else {
		offset = GETMEM(bah_ram_ptr + 3);
		len = 512 - offset;
	}
	if (len+2 >= MINCLSIZE)
		MCLGET(m, M_DONTWAIT);

	if (m == 0) {
		ifp->if_ierrors++;
		goto cleanup;
	}

	type = GETMEM(bah_ram_ptr + offset);
	m->m_data += 1 + arc_isphds(type);

	head = m;
	ah = mtod(head, struct arc_header *);

	ah->arc_shost = GETMEM(bah_ram_ptr + 0);
	ah->arc_dhost = GETMEM(bah_ram_ptr + 1);

	m->m_pkthdr.len = len+2; /* whole packet length */
	m->m_len = 2;		 /* mbuf filled with ARCnet addresses */
	bah_ram_ptr += offset;	/* ram buffer continues there */

	while (len > 0) {

		len1 = len;
		amount = M_TRAILINGSPACE(m);

		if (amount == 0) {
			dst = m;
			MGET(m, M_DONTWAIT, MT_DATA);

			if (m == 0) {
				ifp->if_ierrors++;
				goto cleanup;
			}

			if (len1 >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);

			m->m_len = 0;
			dst->m_next = m;
			amount = M_TRAILINGSPACE(m);
		}

		if (amount < len1)
			len1 = amount;

		bus_space_read_region_1(bst_m, mem, bah_ram_ptr,
		    mtod(m, u_char *) + m->m_len, len1);

		m->m_len += len1;
		bah_ram_ptr += len1;
		len -= len1;
	}

	bpf_mtap(ifp, head);

	(*sc->sc_arccom.ac_if.if_input)(&sc->sc_arccom.ac_if, head);

	head = NULL;
	ifp->if_ipackets++;

cleanup:

	if (head != NULL)
		m_freem(head);

	/* mark buffer as invalid by source id 0 */
	bus_space_write_1(bst_m, mem, buffer*512, 0);
	s = splnet();

	if (--sc->sc_rx_fillcount == 2 - 1) {

		/* was off, restart it on buffer just emptied */
		sc->sc_rx_act = buffer;
		sc->sc_intmask |= BAH_RI;

		/* this also clears the RI flag interrupt: */
		PUTREG(BAHCMD, BAH_RXBC(buffer));
		PUTREG(BAHSTAT, sc->sc_intmask);

#ifdef BAH_DEBUG
		printf("%s: srint: restarted rx on buf %ld\n",
		    device_xname(sc->sc_dev), buffer);
#endif
	}
	splx(s);
}

inline static void
bah_tint(struct bah_softc *sc, int isr)
{
	struct ifnet *ifp;

	bus_space_tag_t bst_r = sc->sc_bst_r;
	bus_space_handle_t regs = sc->sc_regs;


	int buffer;
#ifdef BAHTIMINGS
	int clknow;
#endif

	ifp = &(sc->sc_arccom.ac_if);
	buffer = sc->sc_tx_act;

	/*
	 * retransmit code:
	 * Normal situations first for fast path:
	 * If acknowledgement received ok or broadcast, we're ok.
	 * else if
	 */

	if (isr & BAH_TMA || sc->sc_broadcast[buffer])
		sc->sc_arccom.ac_if.if_opackets++;
#ifdef BAHRETRANSMIT
	else if (ifp->if_flags & IFF_LINK2 && ifp->if_timer > 0
	    && --sc->sc_retransmits[buffer] > 0) {
		/* retransmit same buffer */
		PUTREG(BAHCMD, BAH_TX(buffer));
		return;
	}
#endif
	else
		ifp->if_oerrors++;


	/* We know we can accept another buffer at this point. */
	ifp->if_flags &= ~IFF_OACTIVE;

	if (--sc->sc_tx_fillcount > 0) {

		/*
		 * start tx on other buffer.
		 * This also clears the int flag
		 */
		buffer ^= 1;
		sc->sc_tx_act = buffer;

		/*
		 * already given:
		 * sc->sc_intmask |= BAH_TA;
		 * PUTREG(BAHSTAT, sc->sc_intmask);
		 */
		PUTREG(BAHCMD, BAH_TX(buffer));
		/* init watchdog timer */
		ifp->if_timer = ARCTIMEOUT;

#if defined(BAH_DEBUG) && (BAH_DEBUG > 1)
		printf("%s: tint: starting tx on buffer %d, status 0x%02x\n",
		    device_xname(sc->sc_dev), buffer, GETREG(BAHSTAT));
#endif
	} else {
		/* have to disable TX interrupt */
		sc->sc_intmask &= ~BAH_TA;
		PUTREG(BAHSTAT, sc->sc_intmask);
		/* ... and watchdog timer */
		ifp->if_timer = 0;

#ifdef BAH_DEBUG
		printf("%s: tint: no more buffers to send, status 0x%02x\n",
		    device_xname(sc->sc_dev), GETREG(BAHSTAT));
#endif
	}

	/* XXXX TODO */
#ifdef BAHSOFTCOPY
	/* schedule soft int to fill a new buffer for us */
	softint_schedule(sc->sc_txcookie);
#else
	/* call it directly */
	bah_start(ifp);
#endif
}

/*
 * Our interrupt routine
 */
int
bahintr(void *arg)
{
	struct bah_softc *sc = arg;

	bus_space_tag_t bst_r = sc->sc_bst_r;
        bus_space_tag_t bst_m = sc->sc_bst_m;
	bus_space_handle_t regs = sc->sc_regs;
	bus_space_handle_t mem = sc->sc_mem;

	u_char isr, maskedisr;
	int buffer;
	u_long newsec;

	isr = GETREG(BAHSTAT);
	maskedisr = isr & sc->sc_intmask;
	if (!maskedisr)
		return (0);
	do {

#if defined(BAH_DEBUG) && (BAH_DEBUG>1)
		printf("%s: intr: status 0x%02x, intmask 0x%02x\n",
		    device_xname(sc->sc_dev), isr, sc->sc_intmask);
#endif

		if (maskedisr & BAH_POR) {
		  	/*
			 * XXX We should never see this. Don't bother to store
			 * the address.
			 * sc->sc_arccom.ac_anaddr = GETMEM(BAHMACOFF);
			 */
			PUTREG(BAHCMD, BAH_CLR(CLR_POR));
			log(LOG_WARNING,
			    "%s: intr: got spurious power on reset int\n",
			    device_xname(sc->sc_dev));
		}

		if (maskedisr & BAH_RECON) {
			/*
			 * we dont need to:
			 * PUTREG(BAHCMD, BAH_CONF(CONF_LONG));
			 */
			PUTREG(BAHCMD, BAH_CLR(CLR_RECONFIG));
			sc->sc_arccom.ac_if.if_collisions++;

			/*
			 * If less than 2 seconds per reconfig:
			 *	If ARC_EXCESSIVE_RECONFIGS
			 *	since last burst, complain and set threshold for
			 *	warnings to ARC_EXCESSIVE_RECONS_REWARN.
			 *
			 * This allows for, e.g., new stations on the cable, or
			 * cable switching as long as it is over after
			 * (normally) 16 seconds.
			 *
			 * XXX TODO: check timeout bits in status word and
			 * double time if necessary.
			 */

			callout_stop(&sc->sc_recon_ch);
			newsec = time_second;
			if ((newsec - sc->sc_recontime <= 2) &&
			    (++sc->sc_reconcount == ARC_EXCESSIVE_RECONS)) {
				log(LOG_WARNING,
				    "%s: excessive token losses, "
				    "cable problem?\n", device_xname(sc->sc_dev));
			}
			sc->sc_recontime = newsec;
			callout_reset(&sc->sc_recon_ch, 15 * hz,
			    bah_reconwatch, (void *)sc);
		}

		if (maskedisr & BAH_RI) {
#if defined(BAH_DEBUG) && (BAH_DEBUG > 1)
			printf("%s: intr: hard rint, act %ld\n",
			    device_xname(sc->sc_dev), sc->sc_rx_act);
#endif

			buffer = sc->sc_rx_act;
			/* look if buffer is marked invalid: */
			if (GETMEM(buffer*512) == 0) {
				/*
				 * invalid marked buffer (or illegally
				 * configured sender)
				 */
				log(LOG_WARNING,
				    "%s: spurious RX interrupt or sender 0 "
				    " (ignored)\n", device_xname(sc->sc_dev));
				/*
				 * restart receiver on same buffer.
				 * XXX maybe better reset interface?
				 */
				PUTREG(BAHCMD, BAH_RXBC(buffer));
			} else {
				if (++sc->sc_rx_fillcount > 1) {
					sc->sc_intmask &= ~BAH_RI;
					PUTREG(BAHSTAT, sc->sc_intmask);
				} else {
					buffer ^= 1;
					sc->sc_rx_act = buffer;

					/*
					 * Start receiver on other receive
					 * buffer. This also clears the RI
					 * interrupt flag.
					 */
					PUTREG(BAHCMD, BAH_RXBC(buffer));
					/* in RX intr, so mask is ok for RX */

#ifdef BAH_DEBUG
					printf("%s: strt rx for buf %ld, "
					    "stat 0x%02x\n",
					    device_xname(sc->sc_dev), sc->sc_rx_act,
					    GETREG(BAHSTAT));
#endif
				}

#ifdef BAHSOFTCOPY
				/*
				 * this one starts a soft int to copy out
				 * of the hw
				 */
				softint_schedule(sc->sc_rxcookie);
#else
				/* this one does the copy here */
				bah_srint(sc);
#endif
			}
		}
		if (maskedisr & BAH_TA) {
			bah_tint(sc, isr);
		}
		isr = GETREG(BAHSTAT);
		maskedisr = isr & sc->sc_intmask;
	} while (maskedisr);

	return (1);
}

void
bah_reconwatch(void *arg)
{
	struct bah_softc *sc = arg;

	if (sc->sc_reconcount >= ARC_EXCESSIVE_RECONS) {
		sc->sc_reconcount = 0;
		log(LOG_WARNING, "%s: token valid again.\n",
		    device_xname(sc->sc_dev));
	}
	sc->sc_reconcount = 0;
}


/*
 * Process an ioctl request.
 * This code needs some work - it looks pretty ugly.
 */
int
bah_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct bah_softc *sc;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error;

	error = 0;
	sc = ifp->if_softc;
	ifa = (struct ifaddr *)data;
	ifr = (struct ifreq *)data;
	s = splnet();

#if defined(BAH_DEBUG) && (BAH_DEBUG > 2)
	printf("%s: ioctl() called, cmd = 0x%x\n",
	    device_xname(sc->sc_dev), cmd);
#endif

	switch (cmd) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		bah_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif
		default:
			break;
		}

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			bah_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			bah_init(sc);
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
		case AF_INET:
		case AF_INET6:
			error = 0;
			break;
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
	}

	splx(s);
	return (error);
}

/*
 * watchdog routine for transmitter.
 *
 * We need this, because else a receiver whose hardware is alive, but whose
 * software has not enabled the Receiver, would make our hardware wait forever
 * Discovered this after 20 times reading the docs.
 *
 * Only thing we do is disable transmitter. We'll get an transmit timeout,
 * and the int handler will have to decide not to retransmit (in case
 * retransmission is implemented).
 *
 * This one assumes being called inside splnet()
 */

void
bah_watchdog(struct ifnet *ifp)
{
	struct bah_softc *sc = ifp->if_softc;

	bus_space_tag_t bst_r = sc->sc_bst_r;
	bus_space_handle_t regs = sc->sc_regs;

	PUTREG(BAHCMD, BAH_TXDIS);
	return;
}

