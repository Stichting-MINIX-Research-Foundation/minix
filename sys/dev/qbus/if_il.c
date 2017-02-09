/*	$NetBSD: if_il.c,v 1.28 2015/09/12 19:21:50 christos Exp $	*/
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_il.c	7.8 (Berkeley) 12/16/90
 */

/*
 * Interlan Ethernet Communications Controller interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_il.c,v 1.28 2015/09/12 19:21:50 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in.h>
#endif


#include <sys/bus.h>

#include <dev/qbus/ubareg.h>
#include <dev/qbus/ubavar.h>
#include <dev/qbus/if_uba.h>

#include <dev/qbus/if_il.h>
#include <dev/qbus/if_ilreg.h>

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * is_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 * We also have, for each interface, a UBA interface structure, which
 * contains information about the UNIBUS resources held by the interface:
 * map registers, buffered data paths, etc.  Information is cached in this
 * structure for use by the if_uba.c routines in running the interface
 * efficiently.
 */

struct	il_softc {
	device_t sc_dev;		/* Configuration common part */
	struct	ethercom sc_ec;		/* Ethernet common part */
#define	sc_if	sc_ec.ec_if		/* network-visible interface */
	struct	evcnt sc_cintrcnt;	/* Command interrupts */
	struct  evcnt sc_rintrcnt;	/* Receive interrupts */
	bus_space_tag_t sc_iot;
	bus_addr_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	struct	ubinfo sc_ui;

	struct	ifuba sc_ifuba;		/* UNIBUS resources */
	int	sc_flags;
#define	ILF_RCVPENDING	0x2		/* start rcv in ilcint */
#define	ILF_STATPENDING	0x4		/* stat cmd pending */
#define	ILF_RUNNING	0x8		/* board is running */
#define	ILF_SETADDR	0x10		/* physical address is changed */
	short	sc_lastcmd;		/* can't read csr, so must save it */
	short	sc_scaninterval;	/* interval of stat collection */
#define	ILWATCHINTERVAL	60		/* once every 60 seconds */
	union {
	    struct	il_stats isu_stats;	/* holds on-board statistics */
	    struct	ether_addr isu_maddrs[63];	/* multicast addrs */
	}	sc_isu;
#define sc_stats	sc_isu.isu_stats
#define sc_maddrs	sc_isu.isu_maddrs
	struct	il_stats sc_sum;	/* summation over time */
	int	sc_ubaddr;		/* mapping registers of is_stats */
};

static	int ilmatch(device_t, cfdata_t, void *);
static	void ilattach(device_t, device_t, void *);
static	void ilcint(void *);
static	void ilrint(void *);
static	void ilreset(device_t);
static	int ilwait(struct il_softc *, char *);
static	int ilinit(struct ifnet *);
static	void ilstart(struct ifnet *);
static	void ilwatch(struct ifnet *);
static	void iltotal(struct il_softc *);
static	void ilstop(struct ifnet *, int);

CFATTACH_DECL_NEW(il, sizeof(struct il_softc),
    ilmatch, ilattach, NULL, NULL);

#define IL_WCSR(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define IL_RCSR(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)
#define LOWORD(x)	((int)(x) & 0xffff)
#define HIWORD(x)	(((int)(x) >> 16) & 0x3)

int
ilmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct uba_attach_args *ua = aux;
	volatile int i;

	bus_space_write_2(ua->ua_iot, ua->ua_ioh, IL_CSR, ILC_OFFLINE|IL_CIE);
	DELAY(100000);
	i = bus_space_read_2(ua->ua_iot, ua->ua_ioh, IL_CSR); /* clear CDONE */

	return 1;
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.  A STATUS command is done to get the ethernet
 * address and other interesting data.
 */
void
ilattach(device_t parent, device_t self, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct il_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;
	int error;

	sc->sc_dev = self;
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

	/*
	 * Map interrupt vectors and reset function.
	 */
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, ilcint,
	    sc, &sc->sc_cintrcnt);
	evcnt_attach_dynamic(&sc->sc_cintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec-4, ilrint,
	    sc, &sc->sc_rintrcnt);
	evcnt_attach_dynamic(&sc->sc_rintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");
	uba_reset_establish(ilreset, sc->sc_dev);

	/*
	 * Reset the board and map the statistics
	 * buffer onto the Unibus.
	 */
	IL_WCSR(IL_CSR, ILC_RESET);
	(void)ilwait(sc, "reset");
	sc->sc_ui.ui_size = sizeof(struct il_stats);
	sc->sc_ui.ui_vaddr = (void *)&sc->sc_stats;
	if ((error = uballoc(device_private(parent), &sc->sc_ui, 0)))
		return printf(": failed uballoc, error = %d\n", error);

	IL_WCSR(IL_BAR, LOWORD(sc->sc_ui.ui_baddr));
	IL_WCSR(IL_BCR, sizeof(struct il_stats));
	IL_WCSR(IL_CSR, ((sc->sc_ui.ui_baddr >> 2) & IL_EUA)|ILC_STAT);
	(void)ilwait(sc, "status");
	ubfree(device_private(parent), &sc->sc_ui);
	printf("%s: module=%s firmware=%s\n", device_xname(sc->sc_dev),
		sc->sc_stats.ils_module, sc->sc_stats.ils_firmware);
	printf("%s: hardware address %s\n", device_xname(sc->sc_dev),
		ether_sprintf(sc->sc_stats.ils_addr));

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST;
	ifp->if_init = ilinit;
	ifp->if_stop = ilstop;
	ifp->if_ioctl = ether_ioctl;
	ifp->if_start = ilstart;
	ifp->if_watchdog = ilwatch;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_stats.ils_addr);
}

void
ilstop(struct ifnet *ifp, int a)
{
	struct il_softc *sc = ifp->if_softc;

	IL_WCSR(IL_CSR, ILC_RESET);
}


int
ilwait(struct il_softc *sc, char *op)
{

	while ((IL_RCSR(IL_CSR)&IL_CDONE) == 0)
		;
	if (IL_RCSR(IL_CSR)&IL_STATUS) {
		char bits[64];

		snprintb(bits, sizeof(bits), IL_BITS, IL_RCSR(IL_CSR));
		aprint_error_dev(sc->sc_dev, "%s failed, csr=%s\n", op, bits);
		return (-1);
	}
	return (0);
}

/*
 * Reset of interface after UNIBUS reset.
 * If interface is on specified uba, reset its state.
 */
void
ilreset(device_t dev)
{
	struct il_softc *sc = (void *)dev;

	printf(" %s", device_xname(sc->sc_dev));
	sc->sc_if.if_flags &= ~IFF_RUNNING;
	sc->sc_flags &= ~ILF_RUNNING;
	ilinit(&sc->sc_if);
}

/*
 * Initialization of interface; clear recorded pending
 * operations, and reinitialize UNIBUS usage.
 */
int
ilinit(struct ifnet *ifp)
{
	struct il_softc *sc = ifp->if_softc;
	int s;

	if (sc->sc_flags & ILF_RUNNING)
		return 0;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		if (if_ubainit(&sc->sc_ifuba,
		    device_private(device_parent(sc->sc_dev)),
		    ETHER_MAX_LEN)) {
			aprint_error_dev(sc->sc_dev, "can't initialize\n");
			sc->sc_if.if_flags &= ~IFF_UP;
			return 0;
		}
		sc->sc_ui.ui_size = sizeof(sc->sc_isu);
		sc->sc_ui.ui_vaddr = (void *)&sc->sc_isu;
		uballoc(device_private(device_parent(sc->sc_dev)), &sc->sc_ui, 0);
	}
	sc->sc_scaninterval = ILWATCHINTERVAL;
	ifp->if_timer = sc->sc_scaninterval;

	/*
	 * Turn off source address insertion (it's faster this way),
	 * and set board online.  Former doesn't work if board is
	 * already online (happens on ubareset), so we put it offline
	 * first.
	 */
	s = splnet();
	IL_WCSR(IL_CSR, ILC_RESET);
	if (ilwait(sc, "hardware diag")) {
		sc->sc_if.if_flags &= ~IFF_UP;
		goto out;
	}
	IL_WCSR(IL_CSR, ILC_CISA);
	while ((IL_RCSR(IL_CSR) & IL_CDONE) == 0)
		;
	/*
	 * If we must reprogram this board's physical ethernet
	 * address (as for secondary XNS interfaces), we do so
	 * before putting it on line, and starting receive requests.
	 * If you try this on an older 1010 board, it will total
	 * wedge the board.
	 */
	if (sc->sc_flags & ILF_SETADDR) {
		memcpy(&sc->sc_isu, CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN);
		IL_WCSR(IL_BAR, LOWORD(sc->sc_ui.ui_baddr));
		IL_WCSR(IL_BCR, ETHER_ADDR_LEN);
		IL_WCSR(IL_CSR, ((sc->sc_ui.ui_baddr >> 2) & IL_EUA)|ILC_LDPA);
		if (ilwait(sc, "setaddr"))
			goto out;
		IL_WCSR(IL_BAR, LOWORD(sc->sc_ui.ui_baddr));
		IL_WCSR(IL_BCR, sizeof (struct il_stats));
		IL_WCSR(IL_CSR, ((sc->sc_ui.ui_baddr >> 2) & IL_EUA)|ILC_STAT);
		if (ilwait(sc, "verifying setaddr"))
			goto out;
		if (memcmp(sc->sc_stats.ils_addr,
		    CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN) != 0) {
			aprint_error_dev(sc->sc_dev, "setaddr didn't work\n");
			goto out;
		}
	}
#ifdef MULTICAST
	if (is->is_if.if_flags & IFF_PROMISC) {
		addr->il_csr = ILC_PRMSC;
		if (ilwait(ui, "all multi"))
			goto out;
	} else if (is->is_if.if_flags & IFF_ALLMULTI) {
	too_many_multis:
		addr->il_csr = ILC_ALLMC;
		if (ilwait(ui, "all multi"))
			goto out;
	} else {
		int i;
		register struct ether_addr *ep = is->is_maddrs;
		struct ether_multi *enm;
		struct ether_multistep step;
		/*
		 * Step through our list of multicast addresses.  If we have
		 * too many multicast addresses, or if we have to listen to
		 * a range of multicast addresses, turn on reception of all
		 * multicasts.
		 */
		i = 0;
		ETHER_FIRST_MULTI(step, &is->is_ac, enm);
		while (enm != NULL) {
			if (++i > 63 && k != 0) {
				break;
			}
			*ep++ = *(struct ether_addr *)enm->enm_addrlo;
			ETHER_NEXT_MULTI(step, enm);
		}
		if (i = 0) {
			/* no multicasts! */
		} else if (i <= 63) {
			addr->il_bar = is->is_ubaddr & 0xffff;
			addr->il_bcr = i * sizeof (struct ether_addr);
			addr->il_csr = ((is->is_ubaddr >> 2) & IL_EUA)|
						LC_LDGRPS;
			if (ilwait(ui, "load multi"))
				goto out;
		} else {
		    is->is_if.if_flags |= IFF_ALLMULTI;
		    goto too_many_multis;
		}
	}
#endif /* MULTICAST */
	/*
	 * Set board online.
	 * Hang receive buffer and start any pending
	 * writes by faking a transmit complete.
	 * Receive bcr is not a multiple of 8 so buffer
	 * chaining can't happen.
	 */
	IL_WCSR(IL_CSR, ILC_ONLINE);
	while ((IL_RCSR(IL_CSR) & IL_CDONE) == 0)
		;

	IL_WCSR(IL_BAR, LOWORD(sc->sc_ifuba.ifu_r.ifrw_info));
	IL_WCSR(IL_BCR, sizeof(struct il_rheader) + ETHERMTU + 6);
	IL_WCSR(IL_CSR,
	    ((sc->sc_ifuba.ifu_r.ifrw_info >> 2) & IL_EUA)|ILC_RCV|IL_RIE);
	while ((IL_RCSR(IL_CSR) & IL_CDONE) == 0)
		;
	ifp->if_flags |= IFF_RUNNING | IFF_OACTIVE;
	sc->sc_flags |= ILF_RUNNING;
	sc->sc_lastcmd = 0;
	ilcint(sc);
out:
	splx(s);
	return 0;
}

/*
 * Start output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 */
void
ilstart(struct ifnet *ifp)
{
	struct il_softc *sc = ifp->if_softc;
	int len;
	struct mbuf *m;
	short csr;

	IFQ_DEQUEUE(&ifp->if_snd, m);
	if (m == 0) {
		if ((sc->sc_flags & ILF_STATPENDING) == 0)
			return;
		IL_WCSR(IL_BAR, LOWORD(sc->sc_ui.ui_baddr));
		IL_WCSR(IL_BCR, sizeof (struct il_stats));
		csr = ((sc->sc_ui.ui_baddr >> 2) & IL_EUA)|ILC_STAT|IL_RIE|IL_CIE;
		sc->sc_flags &= ~ILF_STATPENDING;
		goto startcmd;
	}
	len = if_wubaput(&sc->sc_ifuba, m);
#ifdef notdef
	if (sc->sc_ifuba.ifu_flags & UBA_NEEDBDP)
		UBAPURGE(is->is_ifuba.ifu_uba, is->is_ifuba.ifu_w.ifrw_bdp);
#endif
	IL_WCSR(IL_BAR, LOWORD(sc->sc_ifuba.ifu_w.ifrw_info));
	IL_WCSR(IL_BCR, len);
	csr =
	  ((sc->sc_ifuba.ifu_w.ifrw_info >> 2) & IL_EUA)|ILC_XMIT|IL_CIE|IL_RIE;

startcmd:
	sc->sc_lastcmd = csr & IL_CMD;
	IL_WCSR(IL_CSR, csr);
	ifp->if_flags |= IFF_OACTIVE;
	return;
}

/*
 * Command done interrupt.
 */
void
ilcint(void *arg)
{
	struct il_softc *sc = arg;
	short csr;

	if ((sc->sc_if.if_flags & IFF_OACTIVE) == 0) {
		char bits[64];

		snprintb(bits, sizeof(bits), IL_BITS, IL_RCSR(IL_CSR));
		aprint_error_dev(sc->sc_dev,
				 "stray xmit interrupt, csr=%s\n", bits);
		return;
	}

	csr = IL_RCSR(IL_CSR);
	/*
	 * Hang receive buffer if it couldn't
	 * be done earlier (in ilrint).
	 */
	if (sc->sc_flags & ILF_RCVPENDING) {
		int s;

		IL_WCSR(IL_BAR, LOWORD(sc->sc_ifuba.ifu_r.ifrw_info));
		IL_WCSR(IL_BCR, sizeof(struct il_rheader) + ETHERMTU + 6);
		IL_WCSR(IL_CSR,
		  ((sc->sc_ifuba.ifu_r.ifrw_info>>2) & IL_EUA)|ILC_RCV|IL_RIE);
		s = splhigh();
		while ((IL_RCSR(IL_CSR) & IL_CDONE) == 0)
			;
		splx(s);
		sc->sc_flags &= ~ILF_RCVPENDING;
	}
	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	csr &= IL_STATUS;
	switch (sc->sc_lastcmd) {

	case ILC_XMIT:
		sc->sc_if.if_opackets++;
		if (csr > ILERR_RETRIES)
			sc->sc_if.if_oerrors++;
		break;

	case ILC_STAT:
		if (csr == ILERR_SUCCESS)
			iltotal(sc);
		break;
	}
	if_wubaend(&sc->sc_ifuba);
	ilstart(&sc->sc_if);
}

/*
 * Ethernet interface receiver interrupt.
 * If input error just drop packet.
 * Otherwise purge input buffered data path and examine
 * packet to determine type.  If can't determine length
 * from type, then have to drop packet.  Othewise decapsulate
 * packet based on type and pass to type specific higher-level
 * input routine.
 */
void
ilrint(void *arg)
{
	struct il_softc *sc = arg;
	struct il_rheader *il;
	struct mbuf *m;
	int len, s;

	sc->sc_if.if_ipackets++;
#ifdef notyet
	if (sc->sc_ifuba.ifu_flags & UBA_NEEDBDP)
		UBAPURGE(is->is_ifuba.ifu_uba, is->is_ifuba.ifu_r.ifrw_bdp);
#endif
	il = (struct il_rheader *)(sc->sc_ifuba.ifu_r.ifrw_addr);
	len = il->ilr_length - sizeof(struct il_rheader);
	if ((il->ilr_status&(ILFSTAT_A|ILFSTAT_C)) || len < 46 ||
	    len > ETHERMTU) {
		sc->sc_if.if_ierrors++;
#ifdef notdef
		if (sc->sc_if.if_ierrors % 100 == 0)
			printf("il%d: += 100 input errors\n", unit);
#endif
		goto setup;
	}

	if (len == 0)
		goto setup;

	/*
	 * Pull packet off interface.
	 */
	m = if_rubaget(&sc->sc_ifuba, &sc->sc_if, len);
	if (m == NULL)
		goto setup;

	/* Shave off status hdr */
	m_adj(m, 4);
	(*sc->sc_if.if_input)(&sc->sc_if, m);
setup:
	/*
	 * Reset for next packet if possible.
	 * If waiting for transmit command completion, set flag
	 * and wait until command completes.
	 */
	if (sc->sc_if.if_flags & IFF_OACTIVE) {
		sc->sc_flags |= ILF_RCVPENDING;
		return;
	}
	IL_WCSR(IL_BAR, LOWORD(sc->sc_ifuba.ifu_r.ifrw_info));
	IL_WCSR(IL_BCR, sizeof(struct il_rheader) + ETHERMTU + 6);
	IL_WCSR(IL_CSR,
	    ((sc->sc_ifuba.ifu_r.ifrw_info >> 2) & IL_EUA)|ILC_RCV|IL_RIE);
	s = splhigh();
	while ((IL_RCSR(IL_CSR) & IL_CDONE) == 0)
		;
	splx(s);
}
/*
 * Watchdog routine, request statistics from board.
 */
void
ilwatch(struct ifnet *ifp)
{
	struct il_softc *sc = ifp->if_softc;
	int s;

	if (sc->sc_flags & ILF_STATPENDING) {
		ifp->if_timer = sc->sc_scaninterval;
		return;
	}
	s = splnet();
	sc->sc_flags |= ILF_STATPENDING;
	if ((sc->sc_if.if_flags & IFF_OACTIVE) == 0)
		ilstart(ifp);
	splx(s);
	ifp->if_timer = sc->sc_scaninterval;
}

/*
 * Total up the on-board statistics.
 */
void
iltotal(struct il_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	u_short *interval, *sum, *end;

	interval = &sc->sc_stats.ils_frames;
	sum = &sc->sc_sum.ils_frames;
	end = sc->sc_sum.ils_fill2;
	while (sum < end)
		*sum++ += *interval++;
	sc->sc_if.if_collisions = sc->sc_sum.ils_collis;
	if ((sc->sc_flags & ILF_SETADDR) &&
	    (memcmp(sc->sc_stats.ils_addr, CLLADDR(ifp->if_sadl),
		    ETHER_ADDR_LEN) != 0)) {
		log(LOG_ERR, "%s: physaddr reverted\n", device_xname(sc->sc_dev));
		sc->sc_flags &= ~ILF_RUNNING;
		ilinit(&sc->sc_if);
	}
}

#ifdef notyet
/*
 * set ethernet address for unit
 */
void
il_setaddr(u_char *physaddr, struct il_softc *sc)
{
	if (! (sc->sc_flags & ILF_RUNNING))
		return;

	memcpy((void *)is->is_addr, (void *)physaddr, sizeof is->is_addr);
	sc->sc_flags &= ~ILF_RUNNING;
	sc->sc_flags |= ILF_SETADDR;
	ilinit(&sc->sc_if);
}
#endif
