/* $NetBSD: if_plip.c,v 1.25 2014/06/05 23:48:16 rmind Exp $ */

/*-
 * Copyright (c) 1997 Poul-Henning Kamp
 * Copyright (c) 2003, 2004 Gary Thorpe <gathorpe@users.sourceforge.net>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From Id: lpt.c,v 1.55.2.1 1996/11/12 09:08:38 phk Exp
 * FreeBSD: src/sys/dev/ppbus/if_plip.c,v 1.19.2.1 2000/05/24 00:20:57 n_hibma Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_plip.c,v 1.25 2014/06/05 23:48:16 rmind Exp $");

/*
 * Parallel port TCP/IP interfaces added.  I looked at the driver from
 * MACH but this is a complete rewrite, and btw. incompatible, and it
 * should perform better too.  I have never run the MACH driver though.
 *
 * This driver sends two bytes (0x08, 0x00) in front of each packet,
 * to allow us to distinguish another format later.
 *
 * Now added an Linux/Crynwr compatibility mode which is enabled using
 * IF_LINK0 - Tim Wilkinson.
 *
 * TODO:
 *    Make HDLC/PPP mode, use IF_LLC1 to enable.
 *
 * Connect the two computers using a Laplink parallel cable to use this
 * feature:
 *
 *      +----------------------------------------+
 * 	|A-name	A-End	B-End	Descr.	Port/Bit |
 *      +----------------------------------------+
 *	|DATA0	2	15	Data	0/0x01   |
 *	|-ERROR	15	2	   	1/0x08   |
 *      +----------------------------------------+
 *	|DATA1	3	13	Data	0/0x02	 |
 *	|+SLCT	13	3	   	1/0x10   |
 *      +----------------------------------------+
 *	|DATA2	4	12	Data	0/0x04   |
 *	|+PE	12	4	   	1/0x20   |
 *      +----------------------------------------+
 *	|DATA3	5	10	Strobe	0/0x08   |
 *	|-ACK	10	5	   	1/0x40   |
 *      +----------------------------------------+
 *	|DATA4	6	11	Data	0/0x10   |
 *	|BUSY	11	6	   	1/~0x80  |
 *      +----------------------------------------+
 *	|GND	18-25	18-25	GND	-        |
 *      +----------------------------------------+
 *
 * Expect transfer-rates up to 75 kbyte/sec.
 *
 * If GCC could correctly grok
 *	register int port __asm("edx")
 * the code would be cleaner
 *
 * Poul-Henning Kamp <phk@freebsd.org>
 */

/*
 * Update for ppbus, PLIP support only - Nicolas Souchu
 */

#include "opt_inet.h"
#include "opt_plip.h"

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <sys/time.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in_var.h>
/* #include <netinet/in.h> */
#else
#error Cannot config lp/plip without inet
#endif

#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_device.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_var.h>

#include <machine/types.h>
#include <sys/intr.h>

#ifndef LPMTU			/* MTU for the lp# interfaces */
#define	LPMTU		1500
#endif

#ifndef LPMAXSPIN1		/* DELAY factor for the lp# interfaces */
#define	LPMAXSPIN1	8000	/* Spinning for remote intr to happen */
#endif

#ifndef LPMAXSPIN2		/* DELAY factor for the lp# interfaces */
#define	LPMAXSPIN2	500	/* Spinning for remote handshake to happen */
#endif

#ifndef LPMAXERRS		/* Max errors before !RUNNING */
#define	LPMAXERRS	100
#endif

#ifndef LPMAXRTRY
#define LPMAXRTRY	100	/* If channel busy, retry LPMAXRTRY
					consecutive times */
#endif

#define CLPIPHDRLEN	14	/* We send dummy ethernet addresses (two) + packet type in front of packet */
#define	CLPIP_SHAKE	0x80	/* This bit toggles between nibble reception */
#define MLPIPHDRLEN	CLPIPHDRLEN

#define LPIPHDRLEN	2	/* We send 0x08, 0x00 in front of packet */
#define	LPIP_SHAKE	0x40	/* This bit toggles between nibble reception */
#if !defined(MLPIPHDRLEN) || LPIPHDRLEN > MLPIPHDRLEN
#define MLPIPHDRLEN	LPIPHDRLEN
#endif

#define	LPIPTBLSIZE	256	/* Size of octet translation table */

#define LP_PRINTF	if (lpflag) printf

#ifdef PLIP_DEBUG
static int volatile lpflag = 1;
#else
static int volatile lpflag = 0;
#endif

/* Tx/Rsv tables for the lp interface */
static u_char *txmith;
#define txmitl (txmith+(1*LPIPTBLSIZE))
#define trecvh (txmith+(2*LPIPTBLSIZE))
#define trecvl (txmith+(3*LPIPTBLSIZE))
static u_char *ctxmith;
#define ctxmitl (ctxmith+(1*LPIPTBLSIZE))
#define ctrecvh (ctxmith+(2*LPIPTBLSIZE))
#define ctrecvl (ctxmith+(3*LPIPTBLSIZE))
static uint16_t lp_count = 0;

/* Autoconf functions */
static int lp_probe(device_t, cfdata_t, void *);
static void lp_attach(device_t, device_t, void *);
static int lp_detach(device_t, int);

/* Soft config data */
struct lp_softc {
	struct ppbus_device_softc ppbus_dev;
	struct ifnet sc_if;
	u_char *sc_ifbuf;
	unsigned short sc_iferrs;
	unsigned short sc_xmit_rtry;
	u_int8_t sc_dev_ok; /* Zero means ok */
};

/* Autoconf structure */
CFATTACH_DECL_NEW(plip, sizeof(struct lp_softc), lp_probe, lp_attach, lp_detach,
	NULL);

/* Functions for the lp interface */
static void lpinittables(void);
static void lpfreetables(void);
static int lpioctl(struct ifnet *, u_long, void *);
static int lpoutput(struct ifnet *, struct mbuf *, const struct sockaddr *,
	struct rtentry *);
static void lpstart(struct ifnet *);
static void lp_intr(void *);


static int
lp_probe(device_t parent, cfdata_t match, void *aux)
{
	struct ppbus_attach_args * args = aux;

	/* Fail if ppbus is not interrupt capable */
	if(args->capabilities & PPBUS_HAS_INTR)
		return 1;

	printf("%s(%s): not an interrupt-driven port.\n", __func__,
		device_xname(parent));
	return 0;
}

static void
lp_attach(device_t parent, device_t self, void *aux)
{
	struct lp_softc * lp = device_private(self);
	struct ifnet * ifp = &lp->sc_if;

	lp->ppbus_dev.sc_dev = self;
	lp->sc_dev_ok = 0;
	lp->sc_ifbuf = NULL;
	lp->sc_iferrs = 0;
	lp->sc_xmit_rtry = 0;

	ifp->if_softc = lp;
	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_mtu = LPMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_ioctl = lpioctl;
	ifp->if_output = lpoutput;
	ifp->if_start = lpstart;
	ifp->if_type = IFT_PARA;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	ifp->if_dlt = DLT_NULL;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);

	bpf_attach(ifp, DLT_NULL, sizeof(u_int32_t));

	if(lp_count++ == 0)
		lpinittables();
	printf("\n");
}

static int
lp_detach(device_t self, int flags)
{
	int error = 0;
	struct lp_softc * lp = device_private(self);
	device_t ppbus = device_parent(self);

	if(lp->sc_dev_ok) {
		if(!(flags & DETACH_QUIET))
			LP_PRINTF("%s(%s): device not properly attached! "
				"Skipping detach....\n", __func__,
				device_xname(self));
		return error;
	}

	/* If interface is up, bring it down and release ppbus */
	if(lp->sc_if.if_flags & IFF_RUNNING) {
		ppbus_wctr(ppbus, 0x00);
		if_detach(&lp->sc_if);
		error = ppbus_remove_handler(ppbus, lp_intr);
		if(error) {
			if(!(flags & DETACH_QUIET))
				LP_PRINTF("%s(%s): unable to remove interrupt "
					"callback.\n", __func__,
					device_xname(self));
			if(!(flags & DETACH_FORCE))
				return error;
		}
		error = ppbus_release_bus(ppbus, self, 0, 0);
		if(error) {
			if(!(flags & DETACH_QUIET))
				LP_PRINTF("%s(%s): error releasing bus %s.\n",
					__func__, device_xname(self),
					device_xname(ppbus));
			if(!(flags & DETACH_FORCE))
				return error;
		}
	}

	if(lp->sc_ifbuf)
		free(lp->sc_ifbuf, M_DEVBUF);

	if(--lp_count == 0)
		lpfreetables();
	return error;
}

/*
 * Build the translation tables for the LPIP (BSD unix) protocol.
 * We don't want to calculate these nasties in our tight loop, so we
 * precalculate them when we initialize.
 */
static void
lpinittables (void)
{
	int i;

	if (!txmith)
		txmith = malloc(4*LPIPTBLSIZE, M_DEVBUF, M_WAITOK);

	if (!ctxmith)
		ctxmith = malloc(4*LPIPTBLSIZE, M_DEVBUF, M_WAITOK);

	for(i = 0; i < LPIPTBLSIZE; i++) {
		ctxmith[i] = (i & 0xF0) >> 4;
		ctxmitl[i] = 0x10 | (i & 0x0F);
		ctrecvh[i] = (i & 0x78) << 1;
		ctrecvl[i] = (i & 0x78) >> 3;
	}

	for(i = 0; i < LPIPTBLSIZE; i++) {
		txmith[i] = ((i & 0x80) >> 3) | ((i & 0x70) >> 4) | 0x08;
		txmitl[i] = ((i & 0x08) << 1) | (i & 0x07);
		trecvh[i] = ((~i) & 0x80) | ((i & 0x38) << 1);
		trecvl[i] = (((~i) & 0x80) >> 4) | ((i & 0x38) >> 3);
	}
}

/* Free translation tables */
static void
lpfreetables (void)
{
	if (txmith)
		free(txmith, M_DEVBUF);
	if (ctxmith)
		free(ctxmith, M_DEVBUF);
	txmith = ctxmith = NULL;
}


/* Process an ioctl request. */
static int
lpioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct lp_softc * sc = ifp->if_softc;
	device_t dev = sc->ppbus_dev.sc_dev;
	device_t ppbus = device_parent(dev);
	struct ifaddr * ifa = (struct ifaddr *)data;
	struct ifreq * ifr = (struct ifreq *)data;
	u_char * ptr;
	int error, s;

	error = 0;
	s = splnet();

	if(sc->sc_dev_ok) {
		LP_PRINTF("%s(%s): device not properly attached!", __func__,
			device_xname(dev));
		error = ENODEV;
		goto end;
	}

	switch (cmd) {

	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCINITIFADDR:
		if (ifa->ifa_addr->sa_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		ifp->if_flags |= IFF_UP;
	/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == IFF_UP) {
			if((error = ppbus_request_bus(ppbus, dev, 0, 0)))
				break;
			error = ppbus_set_mode(ppbus, PPBUS_COMPATIBLE, 0);
			if(error)
				break;

			error = ppbus_add_handler(ppbus, lp_intr, dev);
			if(error) {
				LP_PRINTF("%s(%s): unable to register interrupt"
					" callback.\n", __func__,
					device_xname(dev));
				ppbus_release_bus(ppbus, dev, 0, 0);
				break;
			}

			/* Allocate a buffer if necessary */
			if(sc->sc_ifbuf == NULL) {
				sc->sc_ifbuf = malloc(sc->sc_if.if_mtu +
					MLPIPHDRLEN, M_DEVBUF, M_NOWAIT);
				if (!sc->sc_ifbuf) {
					error = ENOBUFS;
					ppbus_release_bus(ppbus, dev, 0, 0);
					break;
				}
			}

			ppbus_wctr(ppbus, IRQENABLE);
			ifp->if_flags |= IFF_RUNNING;
		}
		if((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == IFF_RUNNING) {
			ppbus_remove_handler(ppbus, lp_intr);
			error = ppbus_release_bus(ppbus, dev, 0, 0);
			ifp->if_flags &= ~IFF_RUNNING;
		}
		/* Go quiescent */
		ppbus_wdtr(ppbus, 0);
		break;

	case SIOCSIFMTU:
		if(sc->sc_if.if_mtu == ifr->ifr_mtu)
			break;
		ptr = sc->sc_ifbuf;
		sc->sc_ifbuf = malloc(ifr->ifr_mtu+MLPIPHDRLEN, M_DEVBUF,
			M_NOWAIT);
		if (!sc->sc_ifbuf) {
			sc->sc_ifbuf = ptr;
			error = ENOBUFS;
			break;
		}
		if(ptr)
			free(ptr,M_DEVBUF);
		/*FALLTHROUGH*/
	case SIOCGIFMTU:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
		case AF_INET:
			break;
		default:
			return EAFNOSUPPORT;
		}
		break;

	case SIOCGIFMEDIA:
		/*
		 * No ifmedia support at this stage; maybe use it
		 * in future for eg. protocol selection.
		 */
	default:
		LP_PRINTF("LP:ioctl(0x%lx)\n", cmd);
		error = ifioctl_common(ifp, cmd, data);
	}

end:
	splx(s);
	return error;
}

static inline int
clpoutbyte (u_char byte, int spin, device_t ppbus)
{
	int s = spin;
	ppbus_wdtr(ppbus, ctxmitl[byte]);
	while (ppbus_rstr(ppbus) & CLPIP_SHAKE) {
		if (--s == 0) {
			return 1;
		}
	}
	s = spin;
	ppbus_wdtr(ppbus, ctxmith[byte]);
	while (!(ppbus_rstr(ppbus) & CLPIP_SHAKE)) {
		if (--s == 0) {
			return 1;
		}
	}
	return 0;
}

static inline int
clpinbyte (int spin, device_t ppbus)
{
	u_char c, cl;
	int s = spin;

	while(ppbus_rstr(ppbus) & CLPIP_SHAKE) {
		if(!--s) {
			return -1;
		}
	}
	cl = ppbus_rstr(ppbus);
	ppbus_wdtr(ppbus, 0x10);

	s = spin;
	while(!(ppbus_rstr(ppbus) & CLPIP_SHAKE)) {
		if(!--s) {
			return -1;
		}
	}
	c = ppbus_rstr(ppbus);
	ppbus_wdtr(ppbus, 0x00);

	return (ctrecvl[cl] | ctrecvh[c]);
}

static void
lptap(struct ifnet *ifp, struct mbuf *m)
{
	/*
	 * Send a packet through bpf. We need to prepend the address family
	 * as a four byte field. Cons up a dummy header to pacify bpf. This
	 * is safe because bpf will only read from the mbuf (i.e., it won't
	 * try to free it or keep a pointer to it).
	 */
	u_int32_t af = AF_INET;
	struct mbuf m0;

	m0.m_next = m;
	m0.m_len = sizeof(u_int32_t);
	m0.m_data = (char *)&af;
	bpf_mtap(ifp, &m0);
}

/* Soft interrupt handler called by hardware interrupt handler */
static void
lp_intr (void *arg)
{
	device_t dev = (device_t)arg;
        device_t ppbus = device_parent(dev);
	struct lp_softc * sc = device_private(dev);
	struct ifnet * ifp = &sc->sc_if;
	struct mbuf *top;
	int len, s, j;
	u_char *bp;
	u_char c, cl;

	s = splnet();

	/* Do nothing if device not properly attached */
	if(sc->sc_dev_ok) {
		LP_PRINTF("%s(%s): device not properly attached!", __func__,
			device_xname(dev));
		goto done;
	}

	/* Do nothing if interface is not up */
	if((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		goto done;

	/* If other side is no longer transmitting, do nothing */
	if(!(ppbus_rstr(ppbus) & LPIP_SHAKE))
		goto done;

	/* Disable interrupts until we finish */
	ppbus_wctr(ppbus, ~IRQENABLE);

	top = NULL;
	bp = sc->sc_ifbuf;
	/* Linux/crynwyr protocol receiving */
	if(ifp->if_flags & IFF_LINK0) {
		/* Ack. the request */
		ppbus_wdtr(ppbus, 0x01);

		/* Get the packet length */
		j = clpinbyte(LPMAXSPIN2, ppbus);
		if(j == -1)
			goto err;
		len = j;
		j = clpinbyte(LPMAXSPIN2, ppbus);
		if(j == -1)
			goto err;
		len = len + (j << 8);
		if(len > ifp->if_mtu + MLPIPHDRLEN)
			goto err;

		while(len--) {
			j = clpinbyte(LPMAXSPIN2, ppbus);
			if (j == -1) {
				goto err;
			}
			*bp++ = j;
		}
		/* Get and ignore checksum */
		j = clpinbyte(LPMAXSPIN2, ppbus);
		if(j == -1) {
			goto err;
		}

		/* Return to idle state */
		ppbus_wdtr(ppbus, 0);
		len = bp - sc->sc_ifbuf;
		if (len <= CLPIPHDRLEN)
			goto err;
		len -= CLPIPHDRLEN;
		top = m_devget(sc->sc_ifbuf + CLPIPHDRLEN, len, 0, ifp, NULL);
	}
	/* FreeBSD protocol receiving */
	else {
		len = ifp->if_mtu + LPIPHDRLEN;
		while(len--) {
			cl = ppbus_rstr(ppbus);
			ppbus_wdtr(ppbus, 0x08);

			j = LPMAXSPIN2;
			while((ppbus_rstr(ppbus) & LPIP_SHAKE)) {
				if(!--j) goto err;
			}

			c = ppbus_rstr(ppbus);
			ppbus_wdtr(ppbus, 0);

			*bp++= trecvh[cl] | trecvl[c];

			j = LPMAXSPIN2;
			while(!((cl=ppbus_rstr(ppbus)) & LPIP_SHAKE)) {
				if(cl != c &&
					(((cl = ppbus_rstr(ppbus)) ^ 0xb8) &
					0xf8) == (c & 0xf8))
					goto end;
				if(!--j) goto err;
			}
		}

end:
		len = bp - sc->sc_ifbuf;
		if(len <= LPIPHDRLEN)
			goto err;
		len -= LPIPHDRLEN;
		top = m_devget(sc->sc_ifbuf + LPIPHDRLEN, len, 0, ifp, NULL);
	}

	if (top == NULL) {
		ifp->if_iqdrops++;
		goto err;
	}
	if (ifp->if_bpf) {
		lptap(ifp, top);
	}
	if (__predict_false(!pktq_enqueue(ip_pktq, top, 0))) {
		ifp->if_iqdrops++;
		m_freem(top);
		goto err;
	}
	ifp->if_ipackets++;
	ifp->if_ibytes += len;
	sc->sc_iferrs = 0;

	goto done;

err:
	/* Return to idle state */
	ppbus_wdtr(ppbus, 0);
	ifp->if_ierrors++;
	sc->sc_iferrs++;
	LP_PRINTF("R");
	/* Disable interface if there are too many errors */
	if(sc->sc_iferrs > LPMAXERRS) {
		aprint_error_dev(dev, "Too many consecutive errors, going off-line.\n");
		ppbus_wctr(ppbus, ~IRQENABLE);
		if_down(ifp);
		sc->sc_iferrs = 0;
	}

done:
	/* Re-enable interrupts */
	ppbus_wctr(ppbus, IRQENABLE);
	/* If interface is not active, send some packets */
	if((ifp->if_flags & IFF_OACTIVE) == 0)
		lpstart(ifp);
	splx(s);
	return;
}

static inline int
lpoutbyte(u_char byte, int spin, device_t ppbus)
{
	int s = spin;
	ppbus_wdtr(ppbus, txmith[byte]);
	while(!(ppbus_rstr(ppbus) & LPIP_SHAKE)) {
		if(--s == 0)
			return 1;
	}
	s = spin;
	ppbus_wdtr(ppbus, txmitl[byte]);
	while(ppbus_rstr(ppbus) & LPIP_SHAKE) {
		if(--s == 0)
			return 1;
	}
	return 0;
}

/* Queue a packet for delivery */
static int
lpoutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	struct rtentry *rt)
{
	struct lp_softc * sc = ifp->if_softc;
	device_t dev = sc->ppbus_dev.sc_dev;
	device_t ppbus = device_parent(dev);
	ALTQ_DECL(struct altq_pktattr pktattr;)
	int err;
	int s;

	s = splnet();

	if(sc->sc_dev_ok) {
		LP_PRINTF("%s(%s): device not properly attached!", __func__,
			device_xname(dev));
		err = ENODEV;
		goto endoutput;
	}

	if((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		err = ENETDOWN;
		goto endoutput;
	}

	/* Only support INET */
	if(dst->sa_family != AF_INET) {
		LP_PRINTF("%s: af%d not supported\n", ifp->if_xname,
		    dst->sa_family);
		ifp->if_noproto++;
		err = EAFNOSUPPORT;
		goto endoutput;
	}

	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, err);
	if(err == 0) {
		if((ifp->if_flags & IFF_OACTIVE) == 0)
			lpstart(ifp);
	}
	else {
		ifp->if_oerrors++;
		sc->sc_iferrs++;
		LP_PRINTF("Q");

		/* Disable interface if there are too many errors */
		if(sc->sc_iferrs > LPMAXERRS) {
			aprint_error_dev(dev, "Too many errors, going off-line.\n");
			ppbus_wctr(ppbus, ~IRQENABLE);
			if_down(ifp);
			sc->sc_iferrs = 0;
		}
	}

endoutput:
	if((err != 0) && (err != ENOBUFS))
		m_freem(m);
	splx(s);
	return err;
}

/* Send routine: send packets over PLIP cable. Call at splnet(). */
void
lpstart(struct ifnet * ifp)
{
	struct lp_softc * lp = ifp->if_softc;
	device_t dev = lp->ppbus_dev.sc_dev;
	device_t ppbus = device_parent(dev);
	struct mbuf * mm;
	struct mbuf * m;
	u_char * cp;
	int err, i, len, spin, count;
	u_char str, chksum;

	if(lp->sc_dev_ok) {
		LP_PRINTF("%s(%s): device not properly attached!", __func__,
			device_xname(dev));
		return;
	}

	if((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		return;
	}

	ifp->if_flags |= IFF_OACTIVE;

	/* Go quiescent */
	ppbus_wdtr(ppbus, 0);

	/* Output loop */
	for(;;) {
		/* Check if there are packets to send */
		if(IFQ_IS_EMPTY(&ifp->if_snd)) {
			goto final;
		}
		/* Try to send a packet, dequeue it later if successful */
		IFQ_POLL(&ifp->if_snd, m);
		if(m == NULL)
			goto final;

		str = ppbus_rstr(ppbus);
		/* Wait until other side is not transmitting */
		if((str & LPIP_SHAKE) ||
			((ifp->if_flags & IFF_LINK0) && !(str & CLPIP_SHAKE))) {
			LP_PRINTF("&");
			if(++lp->sc_xmit_rtry > LPMAXRTRY) {
				aprint_error_dev(dev, "Too many retries while channel "
					"busy, going off-line.\n");
				ppbus_wctr(ppbus, ~IRQENABLE);
				if_down(ifp);
				lp->sc_xmit_rtry = 0;
			}
			goto final;
		}
		lp->sc_xmit_rtry = 0;

		/* Disable interrupt generation */
		ppbus_wctr(ppbus, ~IRQENABLE);

		err = 1;

		/* Output packet for Linux/crynwyr compatible protocol */
		if(ifp->if_flags & IFF_LINK0) {
			/* Calculate packet length */
			count = 14;		/* Ethernet header len */
			for(mm = m; mm; mm = mm->m_next) {
				count += mm->m_len;
			}

			/* Alert other end to pending packet */
			spin = LPMAXSPIN1;
			ppbus_wdtr(ppbus, 0x08);
			while((ppbus_rstr(ppbus) & 0x08) == 0) {
				if (--spin == 0) {
					goto nend;
				}
			}

			if(clpoutbyte(count & 0xFF, LPMAXSPIN1, ppbus))
				goto nend;
			if(clpoutbyte((count >> 8) & 0xFF, LPMAXSPIN1, ppbus))
				goto nend;

			/* Send dummy ethernet header */
			chksum = 0;
			for(i = 0; i < 12; i++) {
				if(clpoutbyte(i, LPMAXSPIN1, ppbus))
					goto nend;
				chksum += i;
			}

			if(clpoutbyte(0x08, LPMAXSPIN1, ppbus))
				goto nend;
			if(clpoutbyte(0x00, LPMAXSPIN1, ppbus))
				goto nend;
			chksum += 0x08 + 0x00;		/* Add into checksum */

			mm = m;
			do {
				cp = mtod(mm, u_char *);
				len = mm->m_len;
				while(len--) {
					if(clpoutbyte(*cp, LPMAXSPIN2, ppbus))
						goto nend;
					chksum += *cp++;
				}
			} while ((mm = mm->m_next));

			/* Send checksum */
			if(clpoutbyte(chksum, LPMAXSPIN2, ppbus))
				goto nend;

			/* No errors */
			err = 0;
			/* Go quiescent */
			ppbus_wdtr(ppbus, 0);
		}
		/* Output packet for FreeBSD compatible protocol */
		else {
			/* We need a sensible value if we abort */
			cp = NULL;

			if(lpoutbyte(0x08, LPMAXSPIN1, ppbus))
				goto end;
			if(lpoutbyte(0x00, LPMAXSPIN2, ppbus))
				goto end;

			mm = m;
			do {
				cp = mtod(mm,u_char *);
				len = mm->m_len;
				while(len--)
					if(lpoutbyte(*cp++, LPMAXSPIN2, ppbus))
						goto end;
			} while ((mm = mm->m_next));

			/* no errors were encountered */
			err = 0;

end:
			if(cp)
				ppbus_wdtr(ppbus, txmitl[*(--cp)] ^ 0x17);
			else
				ppbus_wdtr(ppbus, txmitl['\0'] ^ 0x17);
		}

nend:
		/* Re-enable interrupt generation */
		ppbus_wctr(ppbus, IRQENABLE);

		if(err) {
			/* Go quiescent */
			ppbus_wdtr(ppbus, 0);

			ifp->if_oerrors++;
			lp->sc_iferrs++;
			LP_PRINTF("X");

			/* Disable interface if there are too many errors */
			if(lp->sc_iferrs > LPMAXERRS) {
				aprint_error_dev(dev, "Too many errors, going off-line.\n");
				ppbus_wctr(ppbus, ~IRQENABLE);
				if_down(ifp);
				lp->sc_iferrs = 0;
				goto final;
			}
		}
		else {
			/* Dequeue packet on success */
			IFQ_DEQUEUE(&ifp->if_snd, m);
			if(ifp->if_bpf)
				lptap(ifp, m);
			ifp->if_opackets++;
			ifp->if_obytes += m->m_pkthdr.len;
			m_freem(m);
		}
	}

final:
	ifp->if_flags &= ~IFF_OACTIVE;
	return;
}
