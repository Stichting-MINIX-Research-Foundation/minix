/*	$NetBSD: if_strip.c,v 1.101 2015/08/24 22:21:26 pooka Exp $	*/
/*	from: NetBSD: if_sl.c,v 1.38 1996/02/13 22:00:23 christos Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 * This driver was contributed by Jonathan Stone.
 *
 * Starmode Radio IP interface (STRIP) for Metricom wireless radio.
 * This STRIP driver assumes address resolution of IP addresses to
 * Metricom MAC addresses is done via local link-level routes.
 * The link-level addresses are entered as an 8-digit packed BCD number.
 * To add a route for a radio at IP address 10.1.2.3, with radio
 * address '1234-5678', reachable via interface strip0, use the command
 *
 *	route add -host 10.1.2.3  -link strip0:12:34:56:78
 */


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
 *	@(#)if_sl.c	8.6 (Berkeley) 2/1/94
 */

/*
 * Derived from: Serial Line interface written by Rick Adams (rick@seismo.gov)
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
__KERNEL_RCSID(0, "$NetBSD: if_strip.c,v 1.101 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#if __NetBSD__
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kauth.h>
#endif
#include <sys/syslog.h>
#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_dl.h>
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
#include <net/if_stripvar.h>
#include <net/slip.h>

#ifdef __NetBSD__	/* XXX -- jrs */
typedef u_char ttychar_t;
#else
typedef char ttychar_t;
#endif

#include <sys/time.h>
#include <net/bpf.h>

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
 * amortizes the header cost.  Remember that even with
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
#define SLMTU		1100 /* XXX -- appromaximated. 1024 may be safer. */

#define STRIP_MTU_ONWIRE (SLMTU + 20 + STRIP_HDRLEN) /* (2*SLMTU+2 in sl.c */


#define	SLIP_HIWAT	roundup(50, TTROUND)

/* This is a NetBSD-1.0 or later kernel. */
#define CCOUNT(q)	((q)->c_cc)


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

static int		strip_clone_create(struct if_clone *, int);
static int		strip_clone_destroy(struct ifnet *);

static LIST_HEAD(, strip_softc) strip_softc_list;

struct if_clone strip_cloner =
    IF_CLONE_INITIALIZER("strip", strip_clone_create, strip_clone_destroy);

#define STRIP_FRAME_END		0x0D		/* carriage return */

static void	stripintr(void *);

static int	stripinit(struct strip_softc *);
static struct mbuf *strip_btom(struct strip_softc *, int);

/*
 * STRIP header: '*' + modem address (dddd-dddd) + '*' + mactype ('SIP0')
 * A Metricom packet looks like this: *<address>*<key><payload><CR>
 *   eg. *0000-1164*SIP0<payload><CR>
 *
 */

#define STRIP_ENCAP_SIZE(X) ((36) + (X)*65/64 + 2)
#define STRIP_HDRLEN 15
#define STRIP_MAC_ADDR_LEN 9

/*
 * Star mode packet header.
 * (may be used for encapsulations other than STRIP.)
 */
#define STARMODE_ADDR_LEN 11
struct st_header {
	u_char starmode_addr[STARMODE_ADDR_LEN];
	u_char starmode_type[4];
};

/*
 * Forward declarations for Metricom-specific functions.
 * Ideally, these would be in a library and shared across
 * different STRIP implementations: *BSD, Linux, etc.
 *
 */
static u_char* UnStuffData(u_char *src, u_char *end, u_char
				*dest, u_long dest_length);

static u_char* StuffData(u_char *src, u_long length, u_char *dest,
			      u_char **code_ptr_ptr);

static void RecvErr(const char *msg, struct strip_softc *sc);
static void RecvErr_Message(struct strip_softc *strip_info,
				u_char *sendername, const u_char *msg);
void	strip_resetradio(struct strip_softc *sc, struct tty *tp);
void	strip_proberadio(struct strip_softc *sc, struct tty *tp);
void	strip_watchdog(struct ifnet *ifp);
void	strip_sendbody(struct strip_softc *sc, struct mbuf *m);
int	strip_newpacket(struct strip_softc *sc, u_char *ptr, u_char *end);
void	strip_send(struct strip_softc *sc, struct mbuf *m0);

void	strip_timeout(void *x);

#ifdef DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif



/*
 * Radio reset macros.
 * The Metricom radios are not particularly well-designed for
 * use in packet mode (starmode).  There's no easy way to tell
 * when the radio is in starmode.  Worse, when the radios are reset
 * or power-cycled, they come back up in Hayes AT-emulation mode,
 * and there's no good way for this driver to tell.
 * We deal with this by peridically tickling the radio
 * with an invalid starmode command.  If the radio doesn't
 * respond with an error, the driver knows to reset the radio.
 */

/* Radio-reset finite state machine (if_watchdog) callback rate, in seconds */
#define STRIP_WATCHDOG_INTERVAL	5

/* Period between intrusive radio probes, in seconds */
#define ST_PROBE_INTERVAL 10

/* Grace period for radio to answer probe, in seconds */
#define ST_PROBERESPONSE_INTERVAL 2

/* Be less agressive about repeated resetting. */
#define STRIP_RESET_INTERVAL 5

/*
 * We received a response from the radio that indicates it's in
 * star mode.  Clear any pending probe or reset timer.
 * Don't  probe radio again for standard polling interval.
 */
#define CLEAR_RESET_TIMER(sc) \
 do {\
    (sc)->sc_state = ST_ALIVE;	\
    (sc)->sc_statetimo = time_second + ST_PROBE_INTERVAL;	\
} while (/*CONSTCOND*/ 0)

/*
 * we received a response from the radio that indicates it's crashed
 * out of starmode into Hayse mode. Reset it ASAP.
 */
#define FORCE_RESET(sc) \
 do {\
    (sc)->sc_statetimo = time_second - 1; \
    (sc)->sc_state = ST_DEAD;	\
    /*(sc)->sc_if.if_timer = 0;*/ \
 } while (/*CONSTCOND*/ 0)

#define RADIO_PROBE_TIMEOUT(sc) \
	 ((sc)-> sc_statetimo > time_second)

static int	stripclose(struct tty *, int);
static int	stripinput(int, struct tty *);
static int	stripioctl(struct ifnet *, u_long, void *);
static int	stripopen(dev_t, struct tty *);
static int	stripoutput(struct ifnet *,
		    struct mbuf *, const struct sockaddr *, struct rtentry *);
static int	stripstart(struct tty *);
static int	striptioctl(struct tty *, u_long, void *, int, struct lwp *);

static struct linesw strip_disc = {
	.l_name = "strip",
	.l_open = stripopen,
	.l_close = stripclose,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = striptioctl,
	.l_rint = stripinput,
	.l_start = stripstart,
	.l_modem = nullmodem,
	.l_poll = ttyerrpoll
};

void
stripattach(void)
{
	if (ttyldisc_attach(&strip_disc) != 0)
		panic("stripattach");
	LIST_INIT(&strip_softc_list);
	if_clone_attach(&strip_cloner);
}

static int
strip_clone_create(struct if_clone *ifc, int unit)
{
	struct strip_softc *sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAIT|M_ZERO);
	sc->sc_unit = unit;
	if_initname(&sc->sc_if, ifc->ifc_name, unit);
	callout_init(&sc->sc_timo_ch, 0);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_mtu = SLMTU;
	sc->sc_if.if_flags = 0;
#if 0
	sc->sc_if.if_flags |= SC_AUTOCOMP /* | IFF_POINTOPOINT | IFF_MULTICAST*/;
#endif
	sc->sc_if.if_type = IFT_SLIP;
	sc->sc_if.if_ioctl = stripioctl;
	sc->sc_if.if_output = stripoutput;
	sc->sc_if.if_dlt = DLT_SLIP;
	sc->sc_fastq.ifq_maxlen = 32;
	IFQ_SET_READY(&sc->sc_if.if_snd);

	sc->sc_if.if_watchdog = strip_watchdog;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_SLIP, SLIP_HDRLEN);
	LIST_INSERT_HEAD(&strip_softc_list, sc, sc_iflist);
	return 0;
}

static int
strip_clone_destroy(struct ifnet *ifp)
{
	struct strip_softc *sc = (struct strip_softc *)ifp->if_softc;

	if (sc->sc_ttyp != NULL)
		return EBUSY;	/* Not removing it */

	LIST_REMOVE(sc, sc_iflist);

	bpf_detach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF);
	return 0;
}

static int
stripinit(struct strip_softc *sc)
{
	u_char *p;

	if (sc->sc_mbuf == NULL) {
		sc->sc_mbuf = m_get(M_WAIT, MT_DATA);
		m_clget(sc->sc_mbuf, M_WAIT);
	}
	sc->sc_ep = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    sc->sc_mbuf->m_ext.ext_size;
	sc->sc_mp = sc->sc_pktstart = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;

	/* Get contiguous buffer in which to de-bytestuff/rll-decode input */
	if (sc->sc_rxbuf == NULL) {
		p = (u_char *)malloc(MCLBYTES, M_DEVBUF, M_WAITOK);
		if (p)
			sc->sc_rxbuf = p + SLBUFSIZE - SLMAX;
		else {
			printf("%s: can't allocate input buffer\n",
			    sc->sc_if.if_xname);
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}

	/* Get contiguous buffer in which to bytestuff/rll-encode output */
	if (sc->sc_txbuf == NULL) {
		p = (u_char *)malloc(MCLBYTES, M_DEVBUF, M_WAITOK);
		if (p)
			sc->sc_txbuf = (u_char *)p + SLBUFSIZE - SLMAX;
		else {
			printf("%s: can't allocate buffer\n",
			    sc->sc_if.if_xname);

			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}

#ifdef INET
	sl_compress_init(&sc->sc_comp);
#endif

	/* Initialize radio probe/reset state machine */
	sc->sc_state = ST_DEAD;		/* assumet the worst. */
	sc->sc_statetimo = time_second; /* do reset immediately */

	return (1);
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
int
stripopen(dev_t dev, struct tty *tp)
{
	struct lwp *l = curlwp;		/* XXX */
	struct strip_softc *sc;
	int error;

	error = kauth_authorize_network(l->l_cred,
	    KAUTH_NETWORK_INTERFACE_STRIP,
	    KAUTH_REQ_NETWORK_INTERFACE_STRIP_ADD, NULL, NULL, NULL);
	if (error)
		return (error);

	if (tp->t_linesw == &strip_disc)
		return (0);

	LIST_FOREACH(sc, &strip_softc_list, sc_iflist) {
		if (sc->sc_ttyp == NULL) {
			sc->sc_si = softint_establish(SOFTINT_NET,
			    stripintr, sc);
			if (stripinit(sc) == 0) {
				softint_disestablish(sc->sc_si);
				return (ENOBUFS);
			}
			mutex_spin_enter(&tty_lock);
			tp->t_sc = (void *)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			ttyflush(tp, FREAD | FWRITE);
			/*
			 * Make sure tty output queue is large enough
			 * to hold a full-sized packet (including frame
			 * end, and a possible extra frame end).
			 * A   full-sized   of 65/64) *SLMTU bytes (because
			 * of escapes and clever RLL bytestuffing),
			 * plus frame header, and add two on for frame ends.
			 */
			if (tp->t_outq.c_cn < STRIP_MTU_ONWIRE) {
				sc->sc_oldbufsize = tp->t_outq.c_cn;
				sc->sc_oldbufquot = tp->t_outq.c_cq != 0;

				mutex_spin_exit(&tty_lock);
				clfree(&tp->t_outq);
				error = clalloc(&tp->t_outq, 3*SLMTU, 0);
				if (error) {
					softint_disestablish(sc->sc_si);
					/*
					 * clalloc() might return -1 which
					 * is no good, so we need to return
					 * something else.
					 */
					return (ENOMEM);
				}
				mutex_spin_enter(&tty_lock);
			} else 
				sc->sc_oldbufsize = sc->sc_oldbufquot = 0;
			strip_resetradio(sc, tp);
			mutex_spin_exit(&tty_lock);

			/*
			 * Start the watchdog timer to get the radio
			 * "probe-for-death"/reset machine going.
			 */
			sc->sc_if.if_timer = STRIP_WATCHDOG_INTERVAL;

			return (0);
		}
	}
	return (ENXIO);
}

/*
 * Line specific close routine.
 * Detach the tty from the strip unit.
 */
static int
stripclose(struct tty *tp, int flag)
{
	struct strip_softc *sc;
	int s;

	ttywflush(tp);
	sc = tp->t_sc;

	if (sc != NULL) {
		softint_disestablish(sc->sc_si);
		s = splnet();
		/*
		 * Cancel watchdog timer, which stops the "probe-for-death"/
		 * reset machine.
		 */
		sc->sc_if.if_timer = 0;
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

		/* XXX */
		free((void *)(sc->sc_rxbuf - SLBUFSIZE + SLMAX), M_DEVBUF);
		sc->sc_rxbuf = NULL;

		/* XXX */
		free((void *)(sc->sc_txbuf - SLBUFSIZE + SLMAX), M_DEVBUF);
		sc->sc_txbuf = NULL;

		if (sc->sc_flags & SC_TIMEOUT) {
			callout_stop(&sc->sc_timo_ch);
			sc->sc_flags &= ~SC_TIMEOUT;
		}

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

	return (0);
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
int
striptioctl(struct tty *tp, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct strip_softc *sc = (struct strip_softc *)tp->t_sc;

	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_unit;
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

/*
 * Take an mbuf chain  containing a STRIP packet (no link-level header),
 * byte-stuff (escape) it, and enqueue it on the tty send queue.
 */
void
strip_sendbody(struct strip_softc *sc, struct mbuf *m)
{
	struct tty *tp = sc->sc_ttyp;
	u_char *dp = sc->sc_txbuf;
	struct mbuf *m2;
	int len;
	u_char *rllstate_ptr = NULL;

	while (m) {
		if (m->m_len != 0) {
			/*
			 * Byte-stuff/run-length encode this mbuf's data
			 * into the output buffer.
			 * XXX Note that chained calls to stuffdata()
			 * require that the stuffed data be left in the
			 * output buffer until the entire packet is encoded.
			 */
			dp = StuffData(mtod(m, u_char *), m->m_len, dp,
			    &rllstate_ptr);
		}
		MFREE(m, m2);
		m = m2;
	}

	/*
	 * Put the entire stuffed packet into the tty output queue.
	 */
	len = dp - sc->sc_txbuf;
	if (b_to_q((ttychar_t *)sc->sc_txbuf, len, &tp->t_outq)) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
			addlog("%s: tty output overflow\n",
			    sc->sc_if.if_xname);
		return;
	}
	sc->sc_if.if_obytes += len;
}

/*
 * Send a STRIP packet.  Must be called at spltty().
 */
void
strip_send(struct strip_softc *sc, struct mbuf *m0)
{
	struct tty *tp = sc->sc_ttyp;
	struct st_header *hdr;

	/*
	 * Send starmode header (unstuffed).
	 */
	hdr = mtod(m0, struct st_header *);
	if (b_to_q((ttychar_t *)hdr, STRIP_HDRLEN, &tp->t_outq)) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
		  	addlog("%s: outq overflow writing header\n",
				 sc->sc_if.if_xname);
		m_freem(m0);
		return;
	}

	m_adj(m0, sizeof(struct st_header));

	/* Byte-stuff and run-length encode the remainder of the packet. */
	strip_sendbody(sc, m0);

	if (putc(STRIP_FRAME_END, &tp->t_outq)) {
		/*
		 * Not enough room.  Remove a char to make room
		 * and end the packet normally.
		 * If you get many collisions (more than one or two
		 * a day) you probably do not have enough clists
		 * and you should increase "nclist" in param.c.
		 */
		(void) unputc(&tp->t_outq);
		(void) putc(STRIP_FRAME_END, &tp->t_outq);
		sc->sc_if.if_collisions++;
	} else {
		++sc->sc_if.if_obytes;
		sc->sc_if.if_opackets++;
	}

	/*
	 * If a radio probe is due now, append it to this packet rather
	 * than waiting until the watchdog routine next runs.
	 */
	if (time_second >= sc->sc_statetimo && sc->sc_state == ST_ALIVE)
		strip_proberadio(sc, tp);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Compression happens in stripintr(); if we do it here, IP TOS
 * will cause us to not compress "background" packets, because
 * ordering gets trashed.  It can be done for all packets in stripintr().
 */
int
stripoutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rt)
{
	struct strip_softc *sc = ifp->if_softc;
	struct ip *ip;
	struct st_header *shp;
	const u_char *dldst;		/* link-level next-hop */
	struct ifqueue *ifq;
	int s, error;
	u_char dl_addrbuf[STARMODE_ADDR_LEN+1];
	ALTQ_DECL(struct altq_pktattr pktattr;)

	/*
	 * Verify tty line is up and alive.
	 */
	if (sc->sc_ttyp == NULL) {
		m_freem(m);
		return (ENETDOWN);	/* sort of */
	}
	if ((sc->sc_ttyp->t_state & TS_CARR_ON) == 0 &&
	    (sc->sc_ttyp->t_cflag & CLOCAL) == 0) {
		m_freem(m);
		return (EHOSTUNREACH);
	}

#ifdef DEBUG
	if (rt) {
	   	printf("stripout, rt: dst af%d gw af%d",
		    rt_getkey(rt)->sa_family, rt->rt_gateway->sa_family);
		if (rt_getkey(rt)->sa_family == AF_INET)
			printf(" dst %x",
			    satocsin(rt_getkey(rt))->sin_addr.s_addr);
		printf("\n");
	}
#endif
	switch (dst->sa_family) {
	case AF_INET:
		/* assume rt is never NULL */
		if (rt == NULL || rt->rt_gateway->sa_family != AF_LINK ||
		    satocsdl(rt->rt_gateway)->sdl_alen != ifp->if_addrlen) {
		  	DPRINTF(("strip: could not arp starmode addr %x\n",
			    satocsin(dst)->sin_addr.s_addr));
			m_freem(m);
			return (EHOSTUNREACH);
		}
		dldst = CLLADDR(satocsdl(rt->rt_gateway));
		break;

	case AF_LINK:
		dldst = CLLADDR(satocsdl(dst));
		break;

	default:
		/*
		 * `Cannot happen' (see stripioctl).  Someday we will extend
		 * the line protocol to support other address families.
		 */
		printf("%s: af %d not supported\n", sc->sc_if.if_xname,
		    dst->sa_family);
		m_freem(m);
		sc->sc_if.if_noproto++;
		return (EAFNOSUPPORT);
	}

	ip = mtod(m, struct ip *);
#ifdef INET
	if (sc->sc_if.if_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return (ENETRESET);		/* XXX ? */
	}
	if ((ip->ip_tos & IPTOS_LOWDELAY) != 0
#ifdef ALTQ
	    && ALTQ_IS_ENABLED(&ifp->if_snd) == 0
#endif
	    )
		ifq = &sc->sc_fastq;
	else
#endif
		ifq = NULL;

	/*
	 * Add local net header.  If no space in first mbuf,
	 * add another.
	 */
	M_PREPEND(m, sizeof(struct st_header), M_DONTWAIT);
	if (m == 0) {
	  	DPRINTF(("strip: could not prepend starmode header\n"));
	  	return (ENOBUFS);
	}

	/*
	 * Unpack BCD route entry into an ASCII starmode address.
	 */
	dl_addrbuf[0] = '*';

	dl_addrbuf[1] = ((dldst[0] >> 4) & 0x0f) + '0';
	dl_addrbuf[2] = ((dldst[0]     ) & 0x0f) + '0';

	dl_addrbuf[3] = ((dldst[1] >> 4) & 0x0f) + '0';
	dl_addrbuf[4] = ((dldst[1]     ) & 0x0f) + '0';

	dl_addrbuf[5] = '-';

	dl_addrbuf[6] = ((dldst[2] >> 4) & 0x0f) + '0';
	dl_addrbuf[7] = ((dldst[2]     ) & 0x0f) + '0';

	dl_addrbuf[8] = ((dldst[3] >> 4) & 0x0f) + '0';
	dl_addrbuf[9] = ((dldst[3]     ) & 0x0f) + '0';

	dl_addrbuf[10] = '*';
	dl_addrbuf[11] = 0;
	dldst = dl_addrbuf;

	shp = mtod(m, struct st_header *);
	memcpy(&shp->starmode_type, "SIP0", sizeof(shp->starmode_type));

 	memcpy(shp->starmode_addr, dldst, sizeof(shp->starmode_addr));

	s = spltty();
	if (sc->sc_oqlen && sc->sc_ttyp->t_outq.c_cc == sc->sc_oqlen) {
		struct bintime bt;

		/* if output's been stalled for too long, and restart */
		getbinuptime(&bt);
		bintime_sub(&bt, &sc->sc_lastpacket);
		if (bt.sec > 0) {
			DPRINTF(("stripoutput: stalled, resetting\n"));
			sc->sc_otimeout++;
			stripstart(sc->sc_ttyp);
		}
	}
	splx(s);

	s = splnet();
	if ((error = ifq_enqueue2(ifp, ifq, m ALTQ_COMMA
	    ALTQ_DECL(&pktattr))) != 0) {
		splx(s);
		return error;
	}
	getbinuptime(&sc->sc_lastpacket);
	splx(s);

	s = spltty();
	stripstart(sc->sc_ttyp);
	splx(s);

	return (0);
}


/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 *
 */
int
stripstart(struct tty *tp)
{
	struct strip_softc *sc = tp->t_sc;

	/*
	 * If there is more in the output queue, just send it now.
	 * We are being called in lieu of ttstart and must do what
	 * it would.
	 */
	if (tp->t_outq.c_cc != 0) {
		(*tp->t_oproc)(tp);
		if (tp->t_outq.c_cc > SLIP_HIWAT)
			return (0);
	}

	/*
	 * This happens briefly when the line shuts down.
	 */
	if (sc == NULL)
		return (0);
	softint_schedule(sc->sc_si);
	return (0);
}

/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
strip_btom(struct strip_softc *sc, int len)
{
	struct mbuf *m;

	/*
	 * Allocate a new input buffer and swap.
	 */
	m = sc->sc_mbuf;
	MGETHDR(sc->sc_mbuf, M_DONTWAIT, MT_DATA);
	if (sc->sc_mbuf == NULL) {
		sc->sc_mbuf = m;
		return (NULL);
	}
	MCLGET(sc->sc_mbuf, M_DONTWAIT);
	if ((sc->sc_mbuf->m_flags & M_EXT) == 0) {
		m_freem(sc->sc_mbuf);
		sc->sc_mbuf = m;
		return (NULL);
	}
	sc->sc_ep = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    sc->sc_mbuf->m_ext.ext_size;

	m->m_data = sc->sc_pktstart;

	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return (m);
}

/*
 * tty interface receiver interrupt.
 *
 * Called with a single char from the tty receiver interrupt; put
 * the char into the buffer containing a partial packet. If the
 * char is a packet delimiter, decapsulate the packet, wrap it in
 * an mbuf, and put it on the protocol input queue.
*/
int
stripinput(int c, struct tty *tp)
{
	struct strip_softc *sc;
	struct mbuf *m;
	int len;

	tk_nin++;
	sc = (struct strip_softc *)tp->t_sc;
	if (sc == NULL)
		return (0);
	if (c & TTY_ERRORMASK || ((tp->t_state & TS_CARR_ON) == 0 &&
	    (tp->t_cflag & CLOCAL) == 0)) {
		sc->sc_flags |= SC_ERROR;
		DPRINTF(("strip: input, error %x\n", c));	 /* XXX */
		return (0);
	}
	c &= TTY_CHARMASK;

	++sc->sc_if.if_ibytes;

	/*
	 * Accumulate characters until we see a frame terminator (\r).
	 */
	switch (c) {

	case '\n':
		/*
		 * Error message strings from the modem are terminated with
		 * \r\n. This driver interprets the  \r as a packet terminator.
		 * If the first character in a packet is a \n, drop it.
		 * (it can never be the first char of a vaild frame).
		 */
		if (sc->sc_mp - sc->sc_pktstart == 0)
			break;

	/* Fall through to */

	default:
		if (sc->sc_mp < sc->sc_ep) {
			*sc->sc_mp++ = c;
		} else {
			sc->sc_flags |= SC_ERROR;
			goto error;
		}
		return (0);

	case STRIP_FRAME_END:
		break;
	}


	/*
	 * We only reach here if we see a CR delimiting a packet.
	 */


	len = sc->sc_mp - sc->sc_pktstart;

#ifdef XDEBUG
 	if (len < 15 || sc->sc_flags & SC_ERROR)
	  	printf("stripinput: end of pkt, len %d, err %d\n",
		    len, sc->sc_flags & SC_ERROR); /*XXX*/
#endif
	if(sc->sc_flags & SC_ERROR) {
		sc->sc_flags &= ~SC_ERROR;
		addlog("%s: sc error flag set. terminating packet\n",
			sc->sc_if.if_xname);
		goto newpack;
	}

	/*
	 * We have a frame.
	 * Process an IP packet, ARP packet, AppleTalk packet,
	 * AT command resposne, or Starmode error.
	 */
	len = strip_newpacket(sc, sc->sc_pktstart, sc->sc_mp);
	if (len <= 1)
		/* less than min length packet - ignore */
		goto newpack;

	m = strip_btom(sc, len);
	if (m == NULL)
		goto error;

	IF_ENQUEUE(&sc->sc_inq, m);
	softint_schedule(sc->sc_si);
	goto newpack;

error:
	sc->sc_if.if_ierrors++;

newpack:
	sc->sc_mp = sc->sc_pktstart = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;

	return (0);
}

static void
stripintr(void *arg)
{
	struct strip_softc *sc = arg;
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
		struct mbuf *bpf_m;

		/*
		 * Do not remove the packet from the queue if it
		 * doesn't look like it will fit into the current
		 * serial output queue (STRIP_MTU_ONWIRE, or
		 * Starmode header + 20 bytes + 4 bytes in case we
		 * have to probe the radio).
		 */
		s = spltty();
		if (tp->t_outq.c_cn - tp->t_outq.c_cc <
		    STRIP_MTU_ONWIRE + 4) {
			splx(s);
			break;
		}
		splx(s);

		/*
		 * Get a packet and send it to the radio.
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
		 * stripoutput() because the packets will be out of
		 * order if we are using TOS queueing, and the
		 * connection ID compression will get munged when
		 * this happens.
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
				    sl_compress_tcp(m, ip,
				    &sc->sc_comp, 1);
		}
#endif
		if (bpf_m != NULL)
			bpf_mtap_sl_out(&sc->sc_if, mtod(m, u_char *), bpf_m);
		getbinuptime(&sc->sc_lastpacket);

		s = spltty();
		strip_send(sc, m);

		/*
		 * We now have characters in the output queue,
		 * kick the serial port.
		 */
		if (tp->t_outq.c_cc != 0)
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
			 * will end up copying garbage in come
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
int
stripioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

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

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
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

	default:
		error = ifioctl_common(ifp, cmd, data);
	}
	splx(s);
	return (error);
}


/*
 * Strip subroutines
 */

/*
 * Set a radio into starmode.
 * Must be called at spltty().
 */
void
strip_resetradio(struct strip_softc *sc, struct tty *tp)
{
#if 0
	static ttychar_t InitString[] =
		"\r\n\r\n\r\nat\r\n\r\n\r\nate0dt**starmode\r\n**\r\n";
#else
	static ttychar_t InitString[] =
		"\r\rat\r\r\rate0q1dt**starmode\r**\r";
#endif
	int i;

	/*
	 * XXX Perhaps flush  tty output queue?
	 */

	if ((i = b_to_q(InitString, sizeof(InitString) - 1, &tp->t_outq))) {
		printf("resetradio: %d chars didn't fit in tty queue\n", i);
		return;
	}
	sc->sc_if.if_obytes += sizeof(InitString) - 1;

	/*
	 * Assume the radio is still dead, so we can detect repeated
	 * resets (perhaps the radio is disconnected, powered off, or
	 * is so badlyhung it needs  powercycling.
	 */
	sc->sc_state = ST_DEAD;
	getbinuptime(&sc->sc_lastpacket);
	sc->sc_statetimo = time_second + STRIP_RESET_INTERVAL;

	/*
	 * XXX Does calling the tty output routine now help resets?
	 */
	(*sc->sc_ttyp->t_oproc)(tp);
}


/*
 * Send an invalid starmode packet to the radio, to induce an error message
 * indicating the radio is in starmode.
 * Update the state machine to indicate a response is expected.
 * Either the radio answers, which will be caught by the parser,
 * or the watchdog will start resetting.
 *
 * NOTE: drops chars directly on the tty output queue.
 * should be caled at spl >= spltty.
 */
void
strip_proberadio(struct strip_softc *sc, struct tty *tp)
{

	int overflow;
	const char *strip_probestr = "**";

	if (sc->sc_if.if_flags & IFF_DEBUG)
		addlog("%s: attempting to probe radio\n", sc->sc_if.if_xname);

	overflow = b_to_q((const ttychar_t *)strip_probestr, 2, &tp->t_outq);
	if (overflow == 0) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
			addlog("%s:: sent probe  to radio\n",
			       sc->sc_if.if_xname);
		/* Go to probe-sent state, set timeout accordingly. */
		sc->sc_state = ST_PROBE_SENT;
		sc->sc_statetimo = time_second + ST_PROBERESPONSE_INTERVAL;
	} else {
		addlog("%s: incomplete probe, tty queue %d bytes overfull\n",
			sc->sc_if.if_xname, overflow);
	}
}


#ifdef DEBUG
static const char *strip_statenames[] = {
	"Alive",
	"Probe sent, awaiting answer",
	"Probe not answered, resetting"
};
#endif


/*
 * Timeout routine -- try to start more output.
 * Will be needed to make strip work on ptys.
 */
void
strip_timeout(void *x)
{
    struct strip_softc *sc = (struct strip_softc *) x;
    struct tty *tp =  sc->sc_ttyp;
    int s;

    s = spltty();
    sc->sc_flags &= ~SC_TIMEOUT;
    stripstart(tp);
    splx(s);
}


/*
 * Strip watchdog routine.
 * The radio hardware is balky. When sent long packets or bursts of small
 * packets, the radios crash and reboots into Hayes-emulation mode.
 * The transmit-side machinery, the error parser, and strip_watchdog()
 * implement a simple finite state machine.
 *
 * We attempt to send a probe to the radio every ST_PROBE seconds. There
 * is no direct way to tell if the radio is in starmode, so we send it a
 * malformed starmode packet -- a frame with no destination address --
 * and expect to an "name missing" error response from the radio within
 * 1 second. If we hear such a response, we assume the radio is alive
 * for the next ST_PROBE seconds.
 * If we don't hear a starmode-error response from  the radio, we reset it.
 *
 * Probes, and parsing of error responses,  are normally done inside the send
 * and receive side respectively. This watchdog routine examines the
 * state-machine variables. If there are no packets to send to the radio
 * during an entire probe interval, strip_output  will not be called,
 * so we send a probe on its behalf.
 */
void
strip_watchdog(struct ifnet *ifp)
{
	struct strip_softc *sc = ifp->if_softc;
	struct tty *tp =  sc->sc_ttyp;

	/*
	 * Just punt if the line has been closed.
	 */
	if (tp == NULL)
		return;

#ifdef DEBUG
	if (ifp->if_flags & IFF_DEBUG)
		addlog("\n%s: in watchdog, state %s timeout %lld\n",
		       ifp->if_xname,
 		       ((unsigned) sc->sc_state < 3) ?
		       strip_statenames[sc->sc_state] : "<<illegal state>>",
		       (long long)(sc->sc_statetimo - time_second));
#endif

	/*
	 * If time in this state hasn't yet expired, return.
	 */
	if ((ifp->if_flags & IFF_UP) ==  0 || sc->sc_statetimo > time_second) {
		goto done;
	}

	/*
	 * The time in the current state has expired.
	 * Take appropriate action and advance FSA to the next state.
	 */
	switch (sc->sc_state) {
	      case ST_ALIVE:
		/*
		 * A probe is due but we haven't piggybacked one on a packet.
		 * Send a probe now.
		 */
		strip_proberadio(sc, sc->sc_ttyp);
		(*tp->t_oproc)(tp);
		break;

	      case ST_PROBE_SENT:
		/*
		 * Probe sent but no response within timeout. Reset.
		 */
		addlog("%s: no answer to probe, resetting radio\n",
		       ifp->if_xname);
		strip_resetradio(sc, sc->sc_ttyp);
		ifp->if_oerrors++;
		break;

	      case ST_DEAD:
		/*
		 * The radio has been sent a reset but didn't respond.
		 * XXX warn user to remove AC adaptor and battery,
		 * wait  5 secs, and replace.
		 */
		addlog("%s: radio reset but not responding, Trying again\n",
		       ifp->if_xname);
		strip_resetradio(sc, sc->sc_ttyp);
		ifp->if_oerrors++;
		break;

	      default:
		/* Cannot happen. To be safe, do  a reset. */
		addlog("%s: %s %d, resetting\n",
		       sc->sc_if.if_xname,
		       "radio-reset finite-state machine in invalid state",
		       sc->sc_state);
		strip_resetradio(sc, sc->sc_ttyp);
		sc->sc_state = ST_DEAD;
		break;
	}

      done:
	ifp->if_timer = STRIP_WATCHDOG_INTERVAL;
	return;
}


/*
 * The following bytestuffing and run-length encoding/decoding
 * functions are taken, with permission from Stuart Cheshire,
 * from the MosquitonNet strip driver for Linux.
 * XXX Linux style left intact, to ease folding in updates from
 * the Mosquitonet group.
 */


/*
 * Process a received packet.
 */
int
strip_newpacket(struct strip_softc *sc, u_char *ptr, u_char *end)
{
	int len = ptr - end;
	u_char *name, *name_end;
	u_int packetlen;

	/* Ignore empty lines */
	if (len == 0) return 0;

	/* Catch 'OK' responses which show radio has fallen out of starmode */
	if (len >= 2 && ptr[0] == 'O' && ptr[1] == 'K') {
		printf("%s: Radio is back in AT command mode: will reset\n",
		    sc->sc_if.if_xname);
		FORCE_RESET(sc);		/* Do reset ASAP */
	return 0;
	}

	/* Check for start of address marker, and then skip over it */
	if (*ptr != '*') {
		/* Catch other error messages */
		if (ptr[0] == 'E' && ptr[1] == 'R' && ptr[2] == 'R' && ptr[3] == '_')
			RecvErr_Message(sc, NULL, ptr+4);
			 /* XXX what should the message above be? */
		else {
			RecvErr("No initial *", sc);
			addlog("(len = %d)\n", len);
		     }
		return 0;
	}

	/* skip the '*' */
	ptr++;

	/* Skip the return address */
	name = ptr;
	while (ptr < end && *ptr != '*')
		ptr++;

	/* Check for end of address marker, and skip over it */
	if (ptr == end) {
		RecvErr("No second *", sc);
		return 0;
	}
	name_end = ptr++;

	/* Check for SRIP key, and skip over it */
	if (ptr[0] != 'S' || ptr[1] != 'I' || ptr[2] != 'P' || ptr[3] != '0') {
		if (ptr[0] == 'E' && ptr[1] == 'R' && ptr[2] == 'R' &&
		    ptr[3] == '_') {
			*name_end = 0;
			RecvErr_Message(sc, name, ptr+4);
		 }
		else RecvErr("No SRIP key", sc);
		return 0;
	}
	ptr += 4;

	/* Decode start of the IP packet header */
	ptr = UnStuffData(ptr, end, sc->sc_rxbuf, 4);
	if (ptr == 0) {
		RecvErr("Runt packet (hdr)", sc);
		return 0;
	}

	/*
	 * The STRIP bytestuff/RLL encoding has no explicit length
	 * of the decoded packet.  Decode start of IP header, get the
	 * IP header length and decode that many bytes in total.
	 */
	packetlen = ((uint16_t)sc->sc_rxbuf[2] << 8) | sc->sc_rxbuf[3];

#ifdef DIAGNOSTIC
#if 0
	printf("Packet %02x.%02x.%02x.%02x\n",
		sc->sc_rxbuf[0], sc->sc_rxbuf[1],
		sc->sc_rxbuf[2], sc->sc_rxbuf[3]);
	printf("Got %d byte packet\n", packetlen);
#endif
#endif

	/* Decode remainder of the IP packer */
	ptr = UnStuffData(ptr, end, sc->sc_rxbuf+4, packetlen-4);
	if (ptr == 0) {
		RecvErr("Short packet", sc);
		return 0;
	}

	/* XXX redundant copy */
	memcpy(sc->sc_pktstart, sc->sc_rxbuf, packetlen );
	return (packetlen);
}


/*
 * Stuffing scheme:
 * 00    Unused (reserved character)
 * 01-3F Run of 2-64 different characters
 * 40-7F Run of 1-64 different characters plus a single zero at the end
 * 80-BF Run of 1-64 of the same character
 * C0-FF Run of 1-64 zeroes (ASCII 0)
*/
typedef enum
{
	Stuff_Diff      = 0x00,
	Stuff_DiffZero  = 0x40,
	Stuff_Same      = 0x80,
	Stuff_Zero      = 0xC0,
	Stuff_NoCode    = 0xFF,		/* Special code, meaning no code selected */

	Stuff_CodeMask  = 0xC0,
	Stuff_CountMask = 0x3F,
	Stuff_MaxCount  = 0x3F,
	Stuff_Magic     = 0x0D		/* The value we are eliminating */
} StuffingCode;

/*
 * StuffData encodes the data starting at "src" for "length" bytes.
 * It writes it to the buffer pointed to by "dest" (which must be at least
 * as long as 1 + 65/64 of the input length). The output may be up to 1.6%
 * larger than the input for pathological input, but will usually be smaller.
 * StuffData returns the new value of the dest pointer as its result.
 *
 * "code_ptr_ptr" points to a "u_char *" which is used to hold
 * encoding state between calls, allowing an encoded packet to be
 * incrementally built up from small parts.
 * On the first call, the "u_char *" pointed to should be initialized
 * to NULL;  between subsequent calls the calling routine should leave
 * the value alone and simply pass it back unchanged so that the
 * encoder can recover its current state.
 */

#define StuffData_FinishBlock(X) \
	(*code_ptr = (X) ^ Stuff_Magic, code = Stuff_NoCode)

static u_char*
StuffData(u_char *src, u_long length, u_char *dest, u_char **code_ptr_ptr)
{
	u_char *end = src + length;
	u_char *code_ptr = *code_ptr_ptr;
	u_char code = Stuff_NoCode, count = 0;

	if (!length) return (dest);

	if (code_ptr) {	/* Recover state from last call, if applicable */
		code  = (*code_ptr ^ Stuff_Magic) & Stuff_CodeMask;
		count = (*code_ptr ^ Stuff_Magic) & Stuff_CountMask;
	}

	while (src < end) {
		switch (code) {
		/*
		 * Stuff_NoCode: If no current code, select one
		 */
		case Stuff_NoCode:
		  	code_ptr = dest++;	/* Record where we're going to put this code */
			count = 0;		/* Reset the count (zero means one instance) */
							/* Tentatively start a new block */
			if (*src == 0) {
				code = Stuff_Zero;
				src++;
			} else {
				code = Stuff_Same;
				*dest++ = *src++ ^ Stuff_Magic;
			}
			/* Note: We optimistically assume run of same -- which will be */
			/* fixed later in Stuff_Same if it turns out not to be true. */
			break;

		/*
		 * Stuff_Zero: We already have at least one zero encoded
		 */
		case Stuff_Zero:

			/* If another zero, count it, else finish this code block */
			if (*src == 0) {
				count++;
				src++;
			} else
				StuffData_FinishBlock(Stuff_Zero + count);
			break;

		/*
		 * Stuff_Same: We already have at least one byte encoded
		 */
		case Stuff_Same:
			/* If another one the same, count it */
			if ((*src ^ Stuff_Magic) == code_ptr[1]) {
				count++;
				src++;
				break;
			}
			/* else, this byte does not match this block. */
			/* If we already have two or more bytes encoded, finish this code block */
			if (count) {
				StuffData_FinishBlock(Stuff_Same + count);
				break;
			}
			/* else, we only have one so far, so switch to Stuff_Diff code */
			code = Stuff_Diff; /* and fall through to Stuff_Diff case below */

		case Stuff_Diff:	/* Stuff_Diff: We have at least two *different* bytes encoded */
			/* If this is a zero, must encode a Stuff_DiffZero, and begin a new block */
			if (*src == 0)
				StuffData_FinishBlock(Stuff_DiffZero + count);
			/* else, if we have three in a row, it is worth starting a Stuff_Same block */
			else if ((*src ^ Stuff_Magic) == dest[-1] && dest[-1] == dest[-2])
				{
				code += count-2;
				if (code == Stuff_Diff)
					code = Stuff_Same;
				StuffData_FinishBlock(code);
				code_ptr = dest-2;
				/* dest[-1] already holds the correct value */
				count = 2;		/* 2 means three bytes encoded */
				code = Stuff_Same;
				}
			/* else, another different byte, so add it to the block */
			else {
				*dest++ = *src ^ Stuff_Magic;
				count++;
			}
			src++;	/* Consume the byte */
			break;
		}

		if (count == Stuff_MaxCount)
			StuffData_FinishBlock(code + count);
		}
	if (code == Stuff_NoCode)
		*code_ptr_ptr = NULL;
	else {
		*code_ptr_ptr = code_ptr;
		StuffData_FinishBlock(code + count);
	}

	return (dest);
}



/*
 * UnStuffData decodes the data at "src", up to (but not including)
 * "end".  It writes the decoded data into the buffer pointed to by
 * "dst", up to a  maximum of "dst_length", and returns the new
 * value of "src" so that a follow-on call can read more data,
 * continuing from where the first left off.
 *
 * There are three types of results:
 * 1. The source data runs out before extracting "dst_length" bytes:
 *    UnStuffData returns NULL to indicate failure.
 * 2. The source data produces exactly "dst_length" bytes:
 *    UnStuffData returns new_src = end to indicate that all bytes
 *    were consumed.
 * 3. "dst_length" bytes are extracted, with more
 *     remaining. UnStuffData returns new_src < end to indicate that
 *     there are more bytes to be read.
 *
 * Note: The decoding may be dstructive, in that it may alter the
 * source data in the process of decoding it (this is necessary to
 * allow a follow-on  call to resume correctly).
 */

static u_char*
UnStuffData(u_char *src, u_char *end, u_char *dst, u_long dst_length)
{
	u_char *dst_end = dst + dst_length;

	/* Sanity check */
	if (!src || !end || !dst || !dst_length)
		return (NULL);

	while (src < end && dst < dst_end)
	{
		int count = (*src ^ Stuff_Magic) & Stuff_CountMask;
		switch ((*src ^ Stuff_Magic) & Stuff_CodeMask)
			{
			case Stuff_Diff:
				if (src+1+count >= end)
					return (NULL);
				do
				{
					*dst++ = *++src ^ Stuff_Magic;
				}
				while(--count >= 0 && dst < dst_end);
				if (count < 0)
					src += 1;
				else
				 if (count == 0)
					*src = Stuff_Same ^ Stuff_Magic;
				else
					*src = (Stuff_Diff + count) ^ Stuff_Magic;
				break;
			case Stuff_DiffZero:
				if (src+1+count >= end)
					return (NULL);
				do
				{
					*dst++ = *++src ^ Stuff_Magic;
				}
				while(--count >= 0 && dst < dst_end);
				if (count < 0)
					*src = Stuff_Zero ^ Stuff_Magic;
				else
					*src = (Stuff_DiffZero + count) ^ Stuff_Magic;
				break;
			case Stuff_Same:
				if (src+1 >= end)
					return (NULL);
				do
				{
					*dst++ = src[1] ^ Stuff_Magic;
				}
				while(--count >= 0 && dst < dst_end);
				if (count < 0)
					src += 2;
				else
					*src = (Stuff_Same + count) ^ Stuff_Magic;
				break;
			case Stuff_Zero:
				do
				{
					*dst++ = 0;
				}
				while(--count >= 0 && dst < dst_end);
				if (count < 0)
					src += 1;
				else
					*src = (Stuff_Zero + count) ^ Stuff_Magic;
				break;
			}
	}

	if (dst < dst_end)
		return (NULL);
	else
		return (src);
}



/*
 * Log an error mesesage (for a packet received with errors?)
 * from the STRIP driver.
 */
static void
RecvErr(const char *msg, struct strip_softc *sc)
{
#define MAX_RecErr	80
	u_char *ptr = sc->sc_pktstart;
	u_char *end = sc->sc_mp;
	u_char pkt_text[MAX_RecErr], *p = pkt_text;
	*p++ = '\"';
	while (ptr < end && p < &pkt_text[MAX_RecErr-4]) {
		if (*ptr == '\\') {
			*p++ = '\\';
			*p++ = '\\';
		} else if (*ptr >= 32 && *ptr <= 126)
			*p++ = *ptr;
		else {
			snprintf(p, sizeof(pkt_text) - (p - pkt_text),
			    "\\%02x", *ptr);
			p += 3;
		}
		ptr++;
	}

	if (ptr == end) *p++ = '\"';
	*p++ = 0;
	addlog("%s: %13s : %s\n", sc->sc_if.if_xname, msg, pkt_text);

	sc->sc_if.if_ierrors++;
}


/*
 * Parse an error message from the radio.
 */
static void
RecvErr_Message(struct strip_softc *strip_info, u_char *sendername,
    const u_char *msg)
{
	static const char ERR_001[] = "001"; /* Not in StarMode! */
	static const char ERR_002[] = "002"; /* Remap handle */
	static const char ERR_003[] = "003"; /* Can't resolve name */
	static const char ERR_004[] = "004"; /* Name too small or missing */
	static const char ERR_005[] = "005"; /* Bad count specification */
	static const char ERR_006[] = "006"; /* Header too big */
	static const char ERR_007[] = "007"; /* Body too big */
	static const char ERR_008[] = "008"; /* Bad character in name */
	static const char ERR_009[] = "009"; /* No count or line terminator */

	char * if_name;

	if_name = strip_info->sc_if.if_xname;

	if (!strncmp(msg, ERR_001, sizeof(ERR_001)-1))
	{
		RecvErr("radio error message:", strip_info);
		addlog("%s: Radio %s is not in StarMode\n",
			if_name, sendername);
	}
	else if (!strncmp(msg, ERR_002, sizeof(ERR_002)-1))
	{
		RecvErr("radio error message:", strip_info);
#ifdef notyet		/*Kernel doesn't have scanf!*/
		int handle;
		u_char newname[64];
		sscanf(msg, "ERR_002 Remap handle &%d to name %s", &handle, newname);
		addlog("%s: Radio name %s is handle %d\n",
			if_name, newname, handle);
#endif
	}
	else if (!strncmp(msg, ERR_003, sizeof(ERR_003)-1))
	{
		RecvErr("radio error message:", strip_info);
		addlog("%s: Destination radio name is unknown\n", if_name);
	}
	else if (!strncmp(msg, ERR_004, sizeof(ERR_004)-1)) {
		/*
		 * The radio reports it got a badly-framed starmode packet
		 * from us; so it must me in starmode.
		 */
		if (strip_info->sc_if.if_flags & IFF_DEBUG)
			addlog("%s: radio responded to probe\n", if_name);
		if (strip_info->sc_state == ST_DEAD) {
			/* A successful reset... */
			addlog("%s: Radio back in starmode\n", if_name);
		}
		CLEAR_RESET_TIMER(strip_info);
	}
	else if (!strncmp(msg, ERR_005, sizeof(ERR_005)-1))
        	RecvErr("radio error message:", strip_info);
	else if (!strncmp(msg, ERR_006, sizeof(ERR_006)-1))
        	RecvErr("radio error message:", strip_info);
	else if (!strncmp(msg, ERR_007, sizeof(ERR_007)-1))
	 {
		/*
		 *	Note: This error knocks the radio back into
		 *	command mode.
		 */
		RecvErr("radio error message:", strip_info);
		printf("%s: Error! Packet size too big for radio.",
		    if_name);
		FORCE_RESET(strip_info);
	}
	else if (!strncmp(msg, ERR_008, sizeof(ERR_008)-1))
	{
		RecvErr("radio error message:", strip_info);
		printf("%s: Radio name contains illegal character\n",
		    if_name);
	}
	else if (!strncmp(msg, ERR_009, sizeof(ERR_009)-1))
        	RecvErr("radio error message:", strip_info);
	else {
		addlog("failed to parse ]%3s[\n", msg);
		RecvErr("unparsed radio error message:", strip_info);
	}
}
