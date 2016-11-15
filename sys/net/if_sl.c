/*	$NetBSD: if_sl.c,v 1.121 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 1987, 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_sl.c	8.9 (Berkeley) 1/9/95
 */

/*
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * N.B.: this belongs in netinet, not net, the way it stands now.
 * Should have a link-layer type designation, but wouldn't be
 * backwards-compatible.
 *
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 * W. Jolitz added slip abort.
 *
 * Hacked almost beyond recognition by Van Jacobson (van@helios.ee.lbl.gov).
 * Added priority queuing for "interactive" traffic; hooks for TCP
 * header compression; ICMP filtering (at 2400 baud, some cretin
 * pinging you can use up all your bandwidth).  Made low clist behavior
 * more robust and slightly less likely to hang serial line.
 * Sped up a bunch of things.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sl.c,v 1.121 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#if __NetBSD__
#include <sys/systm.h>
#include <sys/kauth.h>
#endif
#include <sys/cpu.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/slcompress.h>
#include <net/if_slvar.h>
#include <net/slip.h>
#include <net/ppp_defs.h>
#include <net/if_ppp.h>

#include <sys/time.h>
#include <net/bpf.h>

#include "ioconf.h"

/*
 * SLMAX is a hard limit on input packet size.  To simplify the code
 * and improve performance, we require that packets fit in an mbuf
 * cluster, and if we get a compressed packet, there's enough extra
 * room to expand the header into a max length tcp/ip header (128
 * bytes).  So, SLMAX can be at most
 *	MCLBYTES - 128
 *
 * SLMTU is a hard limit on output packet size.  To insure good
 * interactive response, SLMTU wants to be the smallest size that
 * amortizes the header cost.  (Remember that even with
 * type-of-service queuing, we have to wait for any in-progress
 * packet to finish.  I.e., we wait, on the average, 1/2 * mtu /
 * cps, where cps is the line speed in characters per second.
 * E.g., 533ms wait for a 1024 byte MTU on a 9600 baud line.  The
 * average compressed header size is 6-8 bytes so any MTU > 90
 * bytes will give us 90% of the line bandwidth.  A 100ms wait is
 * tolerable (500ms is not), so want an MTU around 296.  (Since TCP
 * will send 256 byte segments (to allow for 40 byte headers), the
 * typical packet size on the wire will be around 260 bytes).  In
 * 4.3tahoe+ systems, we can set an MTU in a route so we do that &
 * leave the interface MTU relatively high (so we don't IP fragment
 * when acting as a gateway to someone using a stupid MTU).
 *
 * Similar considerations apply to SLIP_HIWAT:  It's the amount of
 * data that will be queued 'downstream' of us (i.e., in clists
 * waiting to be picked up by the tty output interrupt).  If we
 * queue a lot of data downstream, it's immune to our t.o.s. queuing.
 * E.g., if SLIP_HIWAT is 1024, the interactive traffic in mixed
 * telnet/ftp will see a 1 sec wait, independent of the mtu (the
 * wait is dependent on the ftp window size but that's typically
 * 1k - 4k).  So, we want SLIP_HIWAT just big enough to amortize
 * the cost (in idle time on the wire) of the tty driver running
 * off the end of its clists & having to call back slstart for a
 * new packet.  For a tty interface with any buffering at all, this
 * cost will be zero.  Even with a totally brain dead interface (like
 * the one on a typical workstation), the cost will be <= 1 character
 * time.  So, setting SLIP_HIWAT to ~100 guarantees that we'll lose
 * at most 1% while maintaining good interactive response.
 */
#define	BUFOFFSET	(128+sizeof(struct ifnet **)+SLIP_HDRLEN)
#define	SLMAX		(MCLBYTES - BUFOFFSET)
#define	SLBUFSIZE	(SLMAX + BUFOFFSET)
#ifndef SLMTU
#define	SLMTU		296
#endif
#if (SLMTU < 3)
#error SLMTU way too small.
#endif
#define	SLIP_HIWAT	roundup(50, TTROUND)
#ifndef __NetBSD__					/* XXX - cgd */
#define	CLISTRESERVE	1024	/* Can't let clists get too low */
#endif	/* !__NetBSD__ */

/*
 * SLIP ABORT ESCAPE MECHANISM:
 *	(inspired by HAYES modem escape arrangement)
 *	1sec escape 1sec escape 1sec escape { 1sec escape 1sec escape }
 *	within window time signals a "soft" exit from slip mode by remote end
 *	if the IFF_DEBUG flag is on.
 */
#define	ABT_ESC		'\033'	/* can't be t_intr - distant host must know it*/
#define	ABT_IDLE	1	/* in seconds - idle before an escape */
#define	ABT_COUNT	3	/* count of escapes for abort */
#define	ABT_WINDOW	(ABT_COUNT*2+2)	/* in seconds - time to count */

static int		sl_clone_create(struct if_clone *, int);
static int		sl_clone_destroy(struct ifnet *);

static LIST_HEAD(, sl_softc) sl_softc_list;

struct if_clone sl_cloner =
    IF_CLONE_INITIALIZER("sl", sl_clone_create, sl_clone_destroy);

#define FRAME_END		0xc0		/* Frame End */
#define FRAME_ESCAPE		0xdb		/* Frame Esc */
#define TRANS_FRAME_END		0xdc		/* transposed frame end */
#define TRANS_FRAME_ESCAPE	0xdd		/* transposed frame esc */

static void	slintr(void *);

static int	slinit(struct sl_softc *);
static struct mbuf *sl_btom(struct sl_softc *, int);

static int	slclose(struct tty *, int);
static int	slinput(int, struct tty *);
static int	slioctl(struct ifnet *, u_long, void *);
static int	slopen(dev_t, struct tty *);
static int	sloutput(struct ifnet *, struct mbuf *, const struct sockaddr *,
			 struct rtentry *);
static int	slstart(struct tty *);
static int	sltioctl(struct tty *, u_long, void *, int, struct lwp *);

static struct linesw slip_disc = {
	.l_name = "slip",
	.l_open = slopen,
	.l_close = slclose,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = sltioctl,
	.l_rint = slinput,
	.l_start = slstart,
	.l_modem = nullmodem,
	.l_poll = ttyerrpoll
};

void
slattach(int n __unused)
{

	if (ttyldisc_attach(&slip_disc) != 0)
		panic("slattach");
	LIST_INIT(&sl_softc_list);
	if_clone_attach(&sl_cloner);
}

static int
sl_clone_create(struct if_clone *ifc, int unit)
{
	struct sl_softc *sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAIT|M_ZERO);
	sc->sc_unit = unit;
	if_initname(&sc->sc_if, ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_mtu = SLMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT | SC_AUTOCOMP | IFF_MULTICAST;
	sc->sc_if.if_type = IFT_SLIP;
	sc->sc_if.if_ioctl = slioctl;
	sc->sc_if.if_output = sloutput;
	sc->sc_if.if_dlt = DLT_SLIP;
	sc->sc_fastq.ifq_maxlen = 32;
	IFQ_SET_READY(&sc->sc_if.if_snd);
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_SLIP, SLIP_HDRLEN);
	LIST_INSERT_HEAD(&sl_softc_list, sc, sc_iflist);
	return 0;
}

static int
sl_clone_destroy(struct ifnet *ifp)
{
	struct sl_softc *sc = (struct sl_softc *)ifp->if_softc;

	if (sc->sc_ttyp != NULL)
		return EBUSY;	/* Not removing it */

	LIST_REMOVE(sc, sc_iflist);

	bpf_detach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF);
	return 0;
}

static int
slinit(struct sl_softc *sc)
{

	if (sc->sc_mbuf == NULL) {
		sc->sc_mbuf = m_gethdr(M_WAIT, MT_DATA);
		m_clget(sc->sc_mbuf, M_WAIT);
	}
	sc->sc_ep = (u_char *)sc->sc_mbuf->m_ext.ext_buf +
	    sc->sc_mbuf->m_ext.ext_size;
	sc->sc_mp = sc->sc_pktstart = (u_char *)sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;

#ifdef INET
	sl_compress_init(&sc->sc_comp);
#endif

	return 1;
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
static int
slopen(dev_t dev, struct tty *tp)
{
	struct lwp *l = curlwp;		/* XXX */
	struct sl_softc *sc;
	int error;

	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE_SLIP,
	    KAUTH_REQ_NETWORK_INTERFACE_SLIP_ADD, NULL, NULL, NULL);
	if (error)
		return error;

	if (tp->t_linesw == &slip_disc)
		return 0;

	LIST_FOREACH(sc, &sl_softc_list, sc_iflist)
		if (sc->sc_ttyp == NULL) {
			sc->sc_si = softint_establish(SOFTINT_NET,
			    slintr, sc);
			if (sc->sc_si == NULL)
				return ENOMEM;
			if (slinit(sc) == 0) {
				softint_disestablish(sc->sc_si);
				return ENOBUFS;
			}
			tp->t_sc = (void *)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			mutex_spin_enter(&tty_lock);
			tp->t_state |= TS_ISOPEN | TS_XCLUDE;
			ttyflush(tp, FREAD | FWRITE);
			/*
			 * make sure tty output queue is large enough
			 * to hold a full-sized packet (including frame
			 * end, and a possible extra frame end).  full-sized
			 * packet occupies a max of 2*SLMAX bytes (because
			 * of possible escapes), and add two on for frame
			 * ends.
			 */
			if (tp->t_outq.c_cn < 2 * SLMAX + 2) {
				sc->sc_oldbufsize = tp->t_outq.c_cn;
				sc->sc_oldbufquot = tp->t_outq.c_cq != 0;

				clfree(&tp->t_outq);
				mutex_spin_exit(&tty_lock);
				error = clalloc(&tp->t_outq, 2 * SLMAX + 2, 0);
				if (error) {
					softint_disestablish(sc->sc_si);
					/*
					 * clalloc() might return -1 which
					 * is no good, so we need to return
					 * something else.
					 */
					return ENOMEM; /* XXX ?! */
				}
			} else {
				sc->sc_oldbufsize = sc->sc_oldbufquot = 0;
				mutex_spin_exit(&tty_lock);
			}
			return 0;
		}
	return ENXIO;
}

/*
 * Line specific close routine.
 * Detach the tty from the sl unit.
 */
static int
slclose(struct tty *tp, int flag)
{
	struct sl_softc *sc;
	int s;

	ttywflush(tp);
	sc = tp->t_sc;

	if (sc != NULL) {
		softint_disestablish(sc->sc_si);
		s = splnet();
		if_down(&sc->sc_if);
		IF_PURGE(&sc->sc_fastq);
		splx(s);

		s = spltty();
		ttyldisc_release(tp->t_linesw);
		tp->t_linesw = ttyldisc_default();
		tp->t_state = 0;

		sc->sc_ttyp = NULL;
		tp->t_sc = NULL;

		m_freem(sc->sc_mbuf);
		sc->sc_mbuf = NULL;
		sc->sc_ep = sc->sc_mp = sc->sc_pktstart = NULL;
		IF_PURGE(&sc->sc_inq);

		/*
		 * If necessary, install a new outq buffer of the
		 * appropriate size.
		 */
		if (sc->sc_oldbufsize != 0) {
			clfree(&tp->t_outq);
			clalloc(&tp->t_outq, sc->sc_oldbufsize,
			    sc->sc_oldbufquot);
		}
		splx(s);
	}

	return 0;
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
static int
sltioctl(struct tty *tp, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct sl_softc *sc = (struct sl_softc *)tp->t_sc;

	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_unit;	/* XXX */
		break;

	default:
		return EPASSTHROUGH;
	}
	return 0;
}

/*
 * Queue a packet.  Start transmission if not active.
 * Compression happens in slintr(); if we do it here, IP TOS
 * will cause us to not compress "background" packets, because
 * ordering gets trashed.  It can be done for all packets in slintr().
 */
static int
sloutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rtp)
{
	struct sl_softc *sc = ifp->if_softc;
	struct ip *ip;
	struct ifqueue *ifq = NULL;
	int s, error;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	/*
	 * `Cannot happen' (see slioctl).  Someday we will extend
	 * the line protocol to support other address families.
	 */
	if (dst->sa_family != AF_INET) {
		printf("%s: af%d not supported\n", sc->sc_if.if_xname,
		    dst->sa_family);
		m_freem(m);
		sc->sc_if.if_noproto++;
		return EAFNOSUPPORT;
	}

	if (sc->sc_ttyp == NULL) {
		m_freem(m);
		return ENETDOWN;	/* sort of */
	}
	if ((sc->sc_ttyp->t_state & TS_CARR_ON) == 0 &&
	    (sc->sc_ttyp->t_cflag & CLOCAL) == 0) {
		m_freem(m);
		printf("%s: no carrier and not local\n", sc->sc_if.if_xname);
		return EHOSTUNREACH;
	}
	ip = mtod(m, struct ip *);
#ifdef INET
	if (sc->sc_if.if_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return ENETRESET;		/* XXX ? */
	}
#endif

	s = spltty();
	if (sc->sc_oqlen && sc->sc_ttyp->t_outq.c_cc == sc->sc_oqlen) {
		struct bintime bt;

		/* if output's been stalled for too long, and restart */
		getbinuptime(&bt);
		bintime_sub(&bt, &sc->sc_lastpacket);
		if (bt.sec > 0) {
			sc->sc_otimeout++;
			slstart(sc->sc_ttyp);
		}
	}
	splx(s);

	s = splnet();
#ifdef INET
	if ((ip->ip_tos & IPTOS_LOWDELAY) != 0)
		ifq = &sc->sc_fastq;
#endif
	if ((error = ifq_enqueue2(ifp, ifq, m ALTQ_COMMA
	    ALTQ_DECL(&pktattr))) != 0) {
		splx(s);
		return error;
	}
	getbinuptime(&sc->sc_lastpacket);
	splx(s);

	s = spltty();
	if ((sc->sc_oqlen = sc->sc_ttyp->t_outq.c_cc) == 0)
		slstart(sc->sc_ttyp);
	splx(s);

	return 0;
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 */
static int
slstart(struct tty *tp)
{
	struct sl_softc *sc = tp->t_sc;

	/*
	 * If there is more in the output queue, just send it now.
	 * We are being called in lieu of ttstart and must do what
	 * it would.
	 */
	if (tp->t_outq.c_cc != 0) {
		(*tp->t_oproc)(tp);
		if (tp->t_outq.c_cc > SLIP_HIWAT)
			return 0;
	}

	/*
	 * This happens briefly when the line shuts down.
	 */
	if (sc == NULL)
		return 0;
	softint_schedule(sc->sc_si);
	return 0;
}

/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
sl_btom(struct sl_softc *sc, int len)
{
	struct mbuf *m;

	/*
	 * Allocate a new input buffer and swap.
	 */
	m = sc->sc_mbuf;
	MGETHDR(sc->sc_mbuf, M_DONTWAIT, MT_DATA);
	if (sc->sc_mbuf == NULL) {
		sc->sc_mbuf = m;
		return NULL;
	}
	MCLGET(sc->sc_mbuf, M_DONTWAIT);
	if ((sc->sc_mbuf->m_flags & M_EXT) == 0) {
		m_freem(sc->sc_mbuf);
		sc->sc_mbuf = m;
		return NULL;
	}
	sc->sc_ep = (u_char *)sc->sc_mbuf->m_ext.ext_buf +
	    sc->sc_mbuf->m_ext.ext_size;

	m->m_data = sc->sc_pktstart;

	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return m;
}

/*
 * tty interface receiver interrupt.
 */
static int
slinput(int c, struct tty *tp)
{
	struct sl_softc *sc;
	struct mbuf *m;
	int len;

	tk_nin++;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc == NULL)
		return 0;
	if ((c & TTY_ERRORMASK) || ((tp->t_state & TS_CARR_ON) == 0 &&
	    (tp->t_cflag & CLOCAL) == 0)) {
		sc->sc_flags |= SC_ERROR;
		return 0;
	}
	c &= TTY_CHARMASK;

	++sc->sc_if.if_ibytes;

	if (sc->sc_if.if_flags & IFF_DEBUG) {
		if (c == ABT_ESC) {
			/*
			 * If we have a previous abort, see whether
			 * this one is within the time limit.
			 */
			if (sc->sc_abortcount &&
			    time_second >= sc->sc_starttime + ABT_WINDOW)
				sc->sc_abortcount = 0;
			/*
			 * If we see an abort after "idle" time, count it;
			 * record when the first abort escape arrived.
			 */
			if (time_second >= sc->sc_lasttime + ABT_IDLE) {
				if (++sc->sc_abortcount == 1)
					sc->sc_starttime = time_second;
				if (sc->sc_abortcount >= ABT_COUNT) {
					slclose(tp, 0);
					return 0;
				}
			}
		} else
			sc->sc_abortcount = 0;
		sc->sc_lasttime = time_second;
	}

	switch (c) {

	case TRANS_FRAME_ESCAPE:
		if (sc->sc_escape)
			c = FRAME_ESCAPE;
		break;

	case TRANS_FRAME_END:
		if (sc->sc_escape)
			c = FRAME_END;
		break;

	case FRAME_ESCAPE:
		sc->sc_escape = 1;
		return 0;

	case FRAME_END:
		if (sc->sc_flags & SC_ERROR) {
			sc->sc_flags &= ~SC_ERROR;
			goto newpack;
		}
		len = sc->sc_mp - sc->sc_pktstart;
		if (len < 3)
			/* less than min length packet - ignore */
			goto newpack;

		m = sl_btom(sc, len);
		if (m == NULL)
			goto error;

		IF_ENQUEUE(&sc->sc_inq, m);
		softint_schedule(sc->sc_si);
		goto newpack;
	}
	if (sc->sc_mp < sc->sc_ep) {
		*sc->sc_mp++ = c;
		sc->sc_escape = 0;
		return 0;
	}

	/* can't put lower; would miss an extra frame */
	sc->sc_flags |= SC_ERROR;

error:
	sc->sc_if.if_ierrors++;
newpack:
	sc->sc_mp = sc->sc_pktstart = (u_char *)sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;
	sc->sc_escape = 0;

	return 0;
}

static void
slintr(void *arg)
{
	struct sl_softc *sc = arg;
	struct tty *tp = sc->sc_ttyp;
	struct mbuf *m;
	int s, len;
	u_char *pktstart;
#ifdef INET
	u_char c;
#endif
	u_char chdr[CHDR_LEN];

	KASSERT(tp != NULL);

	/*
	 * Output processing loop.
	 */
	mutex_enter(softnet_lock);
	for (;;) {
#ifdef INET
		struct ip *ip;
#endif
		struct mbuf *m2;
		struct mbuf *bpf_m;

		/*
		 * Do not remove the packet from the queue if it
		 * doesn't look like it will fit into the current
		 * serial output queue.  With a packet full of
		 * escapes, this could be as bad as MTU*2+2.
		 */
		s = spltty();
		if (tp->t_outq.c_cn - tp->t_outq.c_cc <
		    2 * sc->sc_if.if_mtu + 2) {
			splx(s);
			break;
		}
		splx(s);

		/*
		 * Get a packet and send it to the interface.
		 */
		s = splnet();
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m)
			sc->sc_if.if_omcasts++;	/* XXX */
		else
			IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
		splx(s);

		if (m == NULL)
			break;

		/*
		 * We do the header compression here rather than in
		 * sloutput() because the packets will be out of order
		 * if we are using TOS queueing, and the connection
		 * ID compression will get munged when this happens.
		 */
		if (sc->sc_if.if_bpf) {
			/*
			 * We need to save the TCP/IP header before
			 * it's compressed.  To avoid complicated
			 * code, we just make a deep copy of the
			 * entire packet (since this is a serial
			 * line, packets should be short and/or the
			 * copy should be negligible cost compared
			 * to the packet transmission time).
			 */
			bpf_m = m_dup(m, 0, M_COPYALL, M_DONTWAIT);
		} else
			bpf_m = NULL;
#ifdef INET
		if ((ip = mtod(m, struct ip *))->ip_p == IPPROTO_TCP) {
			if (sc->sc_if.if_flags & SC_COMPRESS)
				*mtod(m, u_char *) |=
				    sl_compress_tcp(m, ip, &sc->sc_comp, 1);
		}
#endif
		if (bpf_m)
			bpf_mtap_sl_out(&sc->sc_if, mtod(m, u_char *), bpf_m);
		getbinuptime(&sc->sc_lastpacket);

		s = spltty();

		/*
		 * The extra FRAME_END will start up a new packet,
		 * and thus will flush any accumulated garbage.  We
		 * do this whenever the line may have been idle for
		 * some time.
		 */
		if (tp->t_outq.c_cc == 0) {
			sc->sc_if.if_obytes++;
			(void)putc(FRAME_END, &tp->t_outq);
		}

		while (m) {
			u_char *bp, *cp, *ep;

			bp = cp = mtod(m, u_char *);
			ep = cp + m->m_len;
			while (cp < ep) {
				/*
				 * Find out how many bytes in the
				 * string we can handle without
				 * doing something special.
				 */
				while (cp < ep) {
					switch (*cp++) {
					case FRAME_ESCAPE:
					case FRAME_END:
						cp--;
						goto out;
					}
				}
				out:
				if (cp > bp) {
					/*
					 * Put N characters at once
					 * into the tty output queue.
					 */
					if (b_to_q(bp, cp - bp, &tp->t_outq))
						break;
					sc->sc_if.if_obytes += cp - bp;
				}
				/*
				 * If there are characters left in
				 * the mbuf, the first one must be
				 * special..  Put it out in a different
				 * form.
				 */
				if (cp < ep) {
					if (putc(FRAME_ESCAPE, &tp->t_outq))
						break;
					if (putc(*cp++ == FRAME_ESCAPE ?
					    TRANS_FRAME_ESCAPE :
					    TRANS_FRAME_END,
					    &tp->t_outq)) {
						(void)unputc(&tp->t_outq);
						break;
					}
					sc->sc_if.if_obytes += 2;
				}
				bp = cp;
			}
			MFREE(m, m2);
			m = m2;
		}

		if (putc(FRAME_END, &tp->t_outq)) {
			/*
			 * Not enough room.  Remove a char to make
			 * room and end the packet normally.  If
			 * you get many collisions (more than one
			 * or two a day), you probably do not have
			 * enough clists and you should increase
			 * "nclist" in param.c
			 */
			(void)unputc(&tp->t_outq);
			(void)putc(FRAME_END, &tp->t_outq);
			sc->sc_if.if_collisions++;
		} else {
			sc->sc_if.if_obytes++;
			sc->sc_if.if_opackets++;
		}

		/*
		 * We now have characters in the output queue,
		 * kick the serial port.
		 */
		(*tp->t_oproc)(tp);
		splx(s);
	}

	/*
	 * Input processing loop.
	 */
	for (;;) {
		s = spltty();
		IF_DEQUEUE(&sc->sc_inq, m);
		splx(s);
		if (m == NULL)
			break;
		pktstart = mtod(m, u_char *);
		len = m->m_pkthdr.len;
		if (sc->sc_if.if_bpf) {
			/*
			 * Save the compressed header, so we
			 * can tack it on later.  Note that we
			 * will end up copying garbage in some
			 * cases but this is okay.  We remember
			 * where the buffer started so we can
			 * compute the new header length.
			 */
			memcpy(chdr, pktstart, CHDR_LEN);
		}
#ifdef INET
		if ((c = (*pktstart & 0xf0)) != (IPVERSION << 4)) {
			if (c & 0x80)
				c = TYPE_COMPRESSED_TCP;
			else if (c == TYPE_UNCOMPRESSED_TCP)
				*pktstart &= 0x4f; /* XXX */
			/*
			 * We've got something that's not an IP
			 * packet.  If compression is enabled,
			 * try to decompress it.  Otherwise, if
			 * `auto-enable' compression is on and
			 * it's a reasonable packet, decompress
			 * it and then enable compression.
			 * Otherwise, drop it.
			 */
			if (sc->sc_if.if_flags & SC_COMPRESS) {
				len = sl_uncompress_tcp(&pktstart, len,
				    (u_int)c, &sc->sc_comp);
				if (len <= 0) {
					m_freem(m);
					continue;
				}
			} else if ((sc->sc_if.if_flags & SC_AUTOCOMP) &&
			    c == TYPE_UNCOMPRESSED_TCP && len >= 40) {
				len = sl_uncompress_tcp(&pktstart, len,
				    (u_int)c, &sc->sc_comp);
				if (len <= 0) {
					m_freem(m);
					continue;
				}
				sc->sc_if.if_flags |= SC_COMPRESS;
			} else {
				m_freem(m);
				continue;
			}
		}
#endif
		m->m_data = (void *) pktstart;
		m->m_pkthdr.len = m->m_len = len;
		if (sc->sc_if.if_bpf) {
			bpf_mtap_sl_in(&sc->sc_if, chdr, &m);
			if (m == NULL)
				continue;
		}
		/*
		 * If the packet will fit into a single
		 * header mbuf, copy it into one, to save
		 * memory.
		 */
		if (m->m_pkthdr.len < MHLEN) {
			struct mbuf *n;
			int pktlen;

			MGETHDR(n, M_DONTWAIT, MT_DATA);
			pktlen = m->m_pkthdr.len;
			M_MOVE_PKTHDR(n, m);
			memcpy(mtod(n, void *), mtod(m, void *), pktlen);
			n->m_len = m->m_len;
			m_freem(m);
			m = n;
		}

		sc->sc_if.if_ipackets++;
		getbinuptime(&sc->sc_lastpacket);

#ifdef INET
		s = splnet();
		if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
			sc->sc_if.if_ierrors++;
			sc->sc_if.if_iqdrops++;
			m_freem(m);
		}
		splx(s);
#endif
	}
	mutex_exit(softnet_lock);
}

/*
 * Process an ioctl request.
 */
static int
slioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s = splnet(), error = 0;
	struct sl_softc *sc = ifp->if_softc;
	struct ppp_stats *psp;
	struct ppp_comp_stats *pcp;

	switch (cmd) {

	case SIOCINITIFADDR:
		if (ifa->ifa_addr->sa_family == AF_INET)
			ifp->if_flags |= IFF_UP;
		else
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFMTU:
		if ((ifr->ifr_mtu < 3) || (ifr->ifr_mtu > SLMAX)) {
		    error = EINVAL;
		    break;
		}
		/*FALLTHROUGH*/
	case SIOCGIFMTU:
		if ((error = ifioctl_common(&sc->sc_if, cmd, data)) == ENETRESET)
			error = 0;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCGPPPSTATS:
		psp = &((struct ifpppstatsreq *) data)->stats;
		(void)memset(psp, 0, sizeof(*psp));
		psp->p.ppp_ibytes = sc->sc_if.if_ibytes;
		psp->p.ppp_ipackets = sc->sc_if.if_ipackets;
		psp->p.ppp_ierrors = sc->sc_if.if_ierrors;
		psp->p.ppp_obytes = sc->sc_if.if_obytes;
		psp->p.ppp_opackets = sc->sc_if.if_opackets;
		psp->p.ppp_oerrors = sc->sc_if.if_oerrors;
#ifdef INET
		psp->vj.vjs_packets = sc->sc_comp.sls_packets;
		psp->vj.vjs_compressed = sc->sc_comp.sls_compressed;
		psp->vj.vjs_searches = sc->sc_comp.sls_searches;
		psp->vj.vjs_misses = sc->sc_comp.sls_misses;
		psp->vj.vjs_uncompressedin = sc->sc_comp.sls_uncompressedin;
		psp->vj.vjs_compressedin = sc->sc_comp.sls_compressedin;
		psp->vj.vjs_errorin = sc->sc_comp.sls_errorin;
		psp->vj.vjs_tossed = sc->sc_comp.sls_tossed;
#endif
		break;

	case SIOCGPPPCSTATS:
		pcp = &((struct ifpppcstatsreq *) data)->stats;
		(void)memset(pcp, 0, sizeof(*pcp));
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}
	splx(s);
	return error;
}
