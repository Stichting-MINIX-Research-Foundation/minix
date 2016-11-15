/*	$NetBSD: if_dmc.c,v 1.23 2014/06/05 23:48:16 rmind Exp $	*/
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
 *	@(#)if_dmc.c	7.10 (Berkeley) 12/16/90
 */

/*
 * DMC11 device driver, internet version
 *
 *	Bill Nesheim
 *	Cornell University
 *
 *	Lou Salkind
 *	New York University
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_dmc.c,v 1.23 2014/06/05 23:48:16 rmind Exp $");

#undef DMCDEBUG	/* for base table dump on fatal error */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#include <sys/bus.h>

#include <dev/qbus/ubareg.h>
#include <dev/qbus/ubavar.h>
#include <dev/qbus/if_uba.h>

#include <dev/qbus/if_dmcreg.h>


/*
 * output timeout value, sec.; should depend on line speed.
 */
static int dmc_timeout = 20;

#define NRCV 7
#define NXMT 3
#define NCMDS	(NRCV+NXMT+4)	/* size of command queue */

#define DMC_WBYTE(csr, val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, csr, val)
#define DMC_WWORD(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define DMC_RBYTE(csr) \
	bus_space_read_1(sc->sc_iot, sc->sc_ioh, csr)
#define DMC_RWORD(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)


#ifdef DMCDEBUG
#define printd if(dmcdebug)printf
int dmcdebug = 0;
#endif

/* error reporting intervals */
#define DMC_RPNBFS	50
#define DMC_RPDSC	1
#define DMC_RPTMO	10
#define DMC_RPDCK	10

struct  dmc_command {
	char	qp_cmd;		/* command */
	short	qp_ubaddr;	/* buffer address */
	short	qp_cc;		/* character count || XMEM */
	struct	dmc_command *qp_next;	/* next command on queue */
};

struct dmcbufs {
	int	ubinfo;		/* from uballoc */
	short	cc;		/* buffer size */
	short	flags;		/* access control */
};
#define	DBUF_OURS	0	/* buffer is available */
#define	DBUF_DMCS	1	/* buffer claimed by somebody */
#define	DBUF_XMIT	4	/* transmit buffer */
#define	DBUF_RCV	8	/* receive buffer */


/*
 * DMC software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * sc_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 * We also have, for each interface, a  set of 7 UBA interface structures
 * for each, which
 * contain information about the UNIBUS resources held by the interface:
 * map registers, buffered data paths, etc.  Information is cached in this
 * structure for use by the if_uba.c routines in running the interface
 * efficiently.
 */
struct dmc_softc {
	device_t sc_dev;		/* Configuration common part */
	struct	ifnet sc_if;		/* network-visible interface */
	short	sc_oused;		/* output buffers currently in use */
	short	sc_iused;		/* input buffers given to DMC */
	short	sc_flag;		/* flags */
	struct	ubinfo sc_ui;		/* UBA mapping info for base table */
	int	sc_errors[4];		/* non-fatal error counters */
	bus_space_tag_t sc_iot;
	bus_addr_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	struct	evcnt sc_rintrcnt;	/* Interrupt counting */
	struct	evcnt sc_tintrcnt;	/* Interrupt counting */
#define sc_datck sc_errors[0]
#define sc_timeo sc_errors[1]
#define sc_nobuf sc_errors[2]
#define sc_disc  sc_errors[3]
	struct	dmcbufs sc_rbufs[NRCV];	/* receive buffer info */
	struct	dmcbufs sc_xbufs[NXMT];	/* transmit buffer info */
	struct	ifubinfo sc_ifuba;	/* UNIBUS resources */
	struct	ifrw sc_ifr[NRCV];	/* UNIBUS receive buffer maps */
	struct	ifxmt sc_ifw[NXMT];	/* UNIBUS receive buffer maps */
	/* command queue stuff */
	struct	dmc_command sc_cmdbuf[NCMDS];
	struct	dmc_command *sc_qhead;	/* head of command queue */
	struct	dmc_command *sc_qtail;	/* tail of command queue */
	struct	dmc_command *sc_qactive;	/* command in progress */
	struct	dmc_command *sc_qfreeh;	/* head of list of free cmd buffers */
	struct	dmc_command *sc_qfreet;	/* tail of list of free cmd buffers */
	/* end command queue stuff */
	struct dmc_base {
		short	d_base[128];		/* DMC base table */
	} dmc_base;
};

static  int dmcmatch(device_t, cfdata_t, void *);
static  void dmcattach(device_t, device_t, void *);
static  int dmcinit(struct ifnet *);
static  void dmcrint(void *);
static  void dmcxint(void *);
static  void dmcdown(struct dmc_softc *sc);
static  void dmcrestart(struct dmc_softc *);
static  void dmcload(struct dmc_softc *, int, u_short, u_short);
static  void dmcstart(struct ifnet *);
static  void dmctimeout(struct ifnet *);
static  int dmcioctl(struct ifnet *, u_long, void *);
static  int dmcoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
static  void dmcreset(device_t);

CFATTACH_DECL_NEW(dmc, sizeof(struct dmc_softc),
    dmcmatch, dmcattach, NULL, NULL);

/* flags */
#define DMC_RUNNING	0x01		/* device initialized */
#define DMC_BMAPPED	0x02		/* base table mapped */
#define DMC_RESTART	0x04		/* software restart in progress */
#define DMC_ONLINE	0x08		/* device running (had a RDYO) */


/* queue manipulation macros */
#define	QUEUE_AT_HEAD(qp, head, tail) \
	(qp)->qp_next = (head); \
	(head) = (qp); \
	if ((tail) == (struct dmc_command *) 0) \
		(tail) = (head)

#define QUEUE_AT_TAIL(qp, head, tail) \
	if ((tail)) \
		(tail)->qp_next = (qp); \
	else \
		(head) = (qp); \
	(qp)->qp_next = (struct dmc_command *) 0; \
	(tail) = (qp)

#define DEQUEUE(head, tail) \
	(head) = (head)->qp_next;\
	if ((head) == (struct dmc_command *) 0)\
		(tail) = (head)

int
dmcmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct dmc_softc ssc;
	struct dmc_softc *sc = &ssc;
	int i;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;

	DMC_WBYTE(DMC_BSEL1, DMC_MCLR);
	for (i = 100000; i && (DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0; i--)
		;
	if ((DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0) {
		printf("dmcprobe: can't start device\n" );
		return (0);
	}
	DMC_WBYTE(DMC_BSEL0, DMC_RQI|DMC_IEI);
	/* let's be paranoid */
	DMC_WBYTE(DMC_BSEL0, DMC_RBYTE(DMC_BSEL0) | DMC_RQI|DMC_IEI);
	DELAY(1000000);
	DMC_WBYTE(DMC_BSEL1, DMC_MCLR);
	for (i = 100000; i && (DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0; i--)
		;
	return (1);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
dmcattach(device_t parent, device_t self, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct dmc_softc *sc = device_private(self);

	sc->sc_dev = self;
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

	strlcpy(sc->sc_if.if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	sc->sc_if.if_mtu = DMCMTU;
	sc->sc_if.if_init = dmcinit;
	sc->sc_if.if_output = dmcoutput;
	sc->sc_if.if_ioctl = dmcioctl;
	sc->sc_if.if_watchdog = dmctimeout;
	sc->sc_if.if_flags = IFF_POINTOPOINT;
	sc->sc_if.if_softc = sc;
	IFQ_SET_READY(&sc->sc_if.if_snd);

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, dmcrint, sc,
	    &sc->sc_rintrcnt);
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec+4, dmcxint, sc,
	    &sc->sc_tintrcnt);
	uba_reset_establish(dmcreset, sc->sc_dev);
	evcnt_attach_dynamic(&sc->sc_rintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");
	evcnt_attach_dynamic(&sc->sc_tintrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");

	if_attach(&sc->sc_if);
}

/*
 * Reset of interface after UNIBUS reset.
 * If interface is on specified UBA, reset its state.
 */
void
dmcreset(device_t dev)
{
	struct dmc_softc *sc = device_private(dev);

	sc->sc_flag = 0;
	sc->sc_if.if_flags &= ~IFF_RUNNING;
	dmcinit(&sc->sc_if);
}

/*
 * Initialization of interface; reinitialize UNIBUS usage.
 */
int
dmcinit(struct ifnet *ifp)
{
	struct dmc_softc *sc = ifp->if_softc;
	struct ifrw *ifrw;
	struct ifxmt *ifxp;
	struct dmcbufs *rp;
	struct dmc_command *qp;
	struct ifaddr *ifa;
	cfdata_t ui = device_cfdata(sc->sc_dev);
	int base;
	int s;

	/*
	 * Check to see that an address has been set
	 * (both local and destination for an address family).
	 */
	IFADDR_FOREACH(ifa, ifp)
		if (ifa->ifa_addr->sa_family && ifa->ifa_dstaddr->sa_family)
			break;
	if (ifa == (struct ifaddr *) 0)
		return 0;

	if ((DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0) {
		printf("dmcinit: DMC not running\n");
		ifp->if_flags &= ~IFF_UP;
		return 0;
	}
	/* map base table */
	if ((sc->sc_flag & DMC_BMAPPED) == 0) {
		sc->sc_ui.ui_size = sizeof(struct dmc_base);
		sc->sc_ui.ui_vaddr = (void *)&sc->dmc_base;
		uballoc(device_private(device_parent(sc->sc_dev)), &sc->sc_ui, 0);
		sc->sc_flag |= DMC_BMAPPED;
	}
	/* initialize UNIBUS resources */
	sc->sc_iused = sc->sc_oused = 0;
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		if (if_ubaminit(&sc->sc_ifuba,
		    device_private(device_parent(sc->sc_dev)),
		    sizeof(struct dmc_header) + DMCMTU,
		    sc->sc_ifr, NRCV, sc->sc_ifw, NXMT) == 0) {
			aprint_error_dev(sc->sc_dev, "can't allocate uba resources\n");
			ifp->if_flags &= ~IFF_UP;
			return 0;
		}
		ifp->if_flags |= IFF_RUNNING;
	}
	sc->sc_flag &= ~DMC_ONLINE;
	sc->sc_flag |= DMC_RUNNING;
	/*
	 * Limit packets enqueued until we see if we're on the air.
	 */
	ifp->if_snd.ifq_maxlen = 3;

	/* initialize buffer pool */
	/* receives */
	ifrw = &sc->sc_ifr[0];
	for (rp = &sc->sc_rbufs[0]; rp < &sc->sc_rbufs[NRCV]; rp++) {
		rp->ubinfo = ifrw->ifrw_info;
		rp->cc = DMCMTU + sizeof (struct dmc_header);
		rp->flags = DBUF_OURS|DBUF_RCV;
		ifrw++;
	}
	/* transmits */
	ifxp = &sc->sc_ifw[0];
	for (rp = &sc->sc_xbufs[0]; rp < &sc->sc_xbufs[NXMT]; rp++) {
		rp->ubinfo = ifxp->ifw_info;
		rp->cc = 0;
		rp->flags = DBUF_OURS|DBUF_XMIT;
		ifxp++;
	}

	/* set up command queues */
	sc->sc_qfreeh = sc->sc_qfreet
		 = sc->sc_qhead = sc->sc_qtail = sc->sc_qactive =
		(struct dmc_command *)0;
	/* set up free command buffer list */
	for (qp = &sc->sc_cmdbuf[0]; qp < &sc->sc_cmdbuf[NCMDS]; qp++) {
		QUEUE_AT_HEAD(qp, sc->sc_qfreeh, sc->sc_qfreet);
	}

	/* base in */
	base = sc->sc_ui.ui_baddr;
	dmcload(sc, DMC_BASEI, (u_short)base, (base>>2) & DMC_XMEM);
	/* specify half duplex operation, flags tell if primary */
	/* or secondary station */
	if (ui->cf_flags == 0)
		/* use DDCMP mode in full duplex */
		dmcload(sc, DMC_CNTLI, 0, 0);
	else if (ui->cf_flags == 1)
		/* use MAINTENENCE mode */
		dmcload(sc, DMC_CNTLI, 0, DMC_MAINT );
	else if (ui->cf_flags == 2)
		/* use DDCMP half duplex as primary station */
		dmcload(sc, DMC_CNTLI, 0, DMC_HDPLX);
	else if (ui->cf_flags == 3)
		/* use DDCMP half duplex as secondary station */
		dmcload(sc, DMC_CNTLI, 0, DMC_HDPLX | DMC_SEC);

	/* enable operation done interrupts */
	while ((DMC_RBYTE(DMC_BSEL2) & DMC_IEO) == 0)
		DMC_WBYTE(DMC_BSEL2, DMC_RBYTE(DMC_BSEL2) | DMC_IEO);
	s = splnet();
	/* queue first NRCV buffers for DMC to fill */
	for (rp = &sc->sc_rbufs[0]; rp < &sc->sc_rbufs[NRCV]; rp++) {
		rp->flags |= DBUF_DMCS;
		dmcload(sc, DMC_READ, rp->ubinfo,
			(((rp->ubinfo>>2)&DMC_XMEM) | rp->cc));
		sc->sc_iused++;
	}
	splx(s);
	return 0;
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 *
 * Must be called at spl 5
 */
void
dmcstart(struct ifnet *ifp)
{
	struct dmc_softc *sc = ifp->if_softc;
	struct mbuf *m;
	struct dmcbufs *rp;
	int n;

	/*
	 * Dequeue up to NXMT requests and map them to the UNIBUS.
	 * If no more requests, or no dmc buffers available, just return.
	 */
	n = 0;
	for (rp = &sc->sc_xbufs[0]; rp < &sc->sc_xbufs[NXMT]; rp++ ) {
		/* find an available buffer */
		if ((rp->flags & DBUF_DMCS) == 0) {
			IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
			if (m == 0)
				return;
			/* mark it dmcs */
			rp->flags |= (DBUF_DMCS);
			/*
			 * Have request mapped to UNIBUS for transmission
			 * and start the output.
			 */
			rp->cc = if_ubaput(&sc->sc_ifuba, &sc->sc_ifw[n], m);
			rp->cc &= DMC_CCOUNT;
			if (++sc->sc_oused == 1)
				sc->sc_if.if_timer = dmc_timeout;
			dmcload(sc, DMC_WRITE, rp->ubinfo,
				rp->cc | ((rp->ubinfo>>2)&DMC_XMEM));
		}
		n++;
	}
}

/*
 * Utility routine to load the DMC device registers.
 */
void
dmcload(struct dmc_softc *sc, int type, u_short w0, u_short w1)
{
	struct dmc_command *qp;
	int sps;

	sps = splnet();

	/* grab a command buffer from the free list */
	if ((qp = sc->sc_qfreeh) == (struct dmc_command *)0)
		panic("dmc command queue overflow");
	DEQUEUE(sc->sc_qfreeh, sc->sc_qfreet);

	/* fill in requested info */
	qp->qp_cmd = (type | DMC_RQI);
	qp->qp_ubaddr = w0;
	qp->qp_cc = w1;

	if (sc->sc_qactive) {	/* command in progress */
		if (type == DMC_READ) {
			QUEUE_AT_HEAD(qp, sc->sc_qhead, sc->sc_qtail);
		} else {
			QUEUE_AT_TAIL(qp, sc->sc_qhead, sc->sc_qtail);
		}
	} else {	/* command port free */
		sc->sc_qactive = qp;
		DMC_WBYTE(DMC_BSEL0, qp->qp_cmd);
		dmcrint(sc);
	}
	splx(sps);
}

/*
 * DMC interface receiver interrupt.
 * Ready to accept another command,
 * pull one off the command queue.
 */
void
dmcrint(void *arg)
{
	struct dmc_softc *sc = arg;
	struct dmc_command *qp;
	int n;

	if ((qp = sc->sc_qactive) == (struct dmc_command *) 0) {
		printf("%s: dmcrint no command\n", device_xname(sc->sc_dev));
		return;
	}
	while (DMC_RBYTE(DMC_BSEL0) & DMC_RDYI) {
		DMC_WWORD(DMC_SEL4, qp->qp_ubaddr);
		DMC_WWORD(DMC_SEL6, qp->qp_cc);
		DMC_WBYTE(DMC_BSEL0, DMC_RBYTE(DMC_BSEL0) & ~(DMC_IEI|DMC_RQI));
		/* free command buffer */
		QUEUE_AT_HEAD(qp, sc->sc_qfreeh, sc->sc_qfreet);
		while (DMC_RBYTE(DMC_BSEL0) & DMC_RDYI) {
			/*
			 * Can't check for RDYO here 'cause
			 * this routine isn't reentrant!
			 */
			DELAY(5);
		}
		/* move on to next command */
		if ((sc->sc_qactive = sc->sc_qhead) == (struct dmc_command *)0)
			break;		/* all done */
		/* more commands to do, start the next one */
		qp = sc->sc_qactive;
		DEQUEUE(sc->sc_qhead, sc->sc_qtail);
		DMC_WBYTE(DMC_BSEL0, qp->qp_cmd);
		n = RDYSCAN;
		while (n-- > 0)
			if ((DMC_RBYTE(DMC_BSEL0) & DMC_RDYI) ||
			    (DMC_RBYTE(DMC_BSEL2) & DMC_RDYO))
				break;
	}
	if (sc->sc_qactive) {
		DMC_WBYTE(DMC_BSEL0, DMC_RBYTE(DMC_BSEL0) & (DMC_IEI|DMC_RQI));
		/* VMS does it twice !*$%@# */
		DMC_WBYTE(DMC_BSEL0, DMC_RBYTE(DMC_BSEL0) & (DMC_IEI|DMC_RQI));
	}

}

/*
 * DMC interface transmitter interrupt.
 * A transfer may have completed, check for errors.
 * If it was a read, notify appropriate protocol.
 * If it was a write, pull the next one off the queue.
 */
void
dmcxint(void *a)
{
	struct dmc_softc *sc = a;

	struct ifnet *ifp;
	struct mbuf *m;
	int arg, pkaddr, cmd, len, s;
	struct ifrw *ifrw;
	struct dmcbufs *rp;
	struct ifxmt *ifxp;
	struct dmc_header *dh;
	char buf[64];

	ifp = &sc->sc_if;

	while (DMC_RBYTE(DMC_BSEL2) & DMC_RDYO) {

		cmd = DMC_RBYTE(DMC_BSEL2) & 0xff;
		arg = DMC_RWORD(DMC_SEL6) & 0xffff;
		/* reconstruct UNIBUS address of buffer returned to us */
		pkaddr = ((arg&DMC_XMEM)<<2) | (DMC_RWORD(DMC_SEL4) & 0xffff);
		/* release port */
		DMC_WBYTE(DMC_BSEL2, DMC_RBYTE(DMC_BSEL2) & ~DMC_RDYO);
		switch (cmd & 07) {

		case DMC_OUR:
			/*
			 * A read has completed.
			 * Pass packet to type specific
			 * higher-level input routine.
			 */
			ifp->if_ipackets++;
			/* find location in dmcuba struct */
			ifrw= &sc->sc_ifr[0];
			for (rp = &sc->sc_rbufs[0]; rp < &sc->sc_rbufs[NRCV]; rp++) {
				if(rp->ubinfo == pkaddr)
					break;
				ifrw++;
			}
			if (rp >= &sc->sc_rbufs[NRCV])
				panic("dmc rcv");
			if ((rp->flags & DBUF_DMCS) == 0)
				aprint_error_dev(sc->sc_dev, "done unalloc rbuf\n");

			len = (arg & DMC_CCOUNT) - sizeof (struct dmc_header);
			if (len < 0 || len > DMCMTU) {
				ifp->if_ierrors++;
#ifdef DMCDEBUG
				printd("%s: bad rcv pkt addr 0x%x len 0x%x\n",
				    device_xname(sc->sc_dev), pkaddr, len);
#endif
				goto setup;
			}
			/*
			 * Deal with trailer protocol: if type is trailer
			 * get true type from first 16-bit word past data.
			 * Remember that type was trailer by setting off.
			 */
			dh = (struct dmc_header *)ifrw->ifrw_addr;
			dh->dmc_type = ntohs((u_short)dh->dmc_type);
			if (len == 0)
				goto setup;

			/*
			 * Pull packet off interface.  Off is nonzero if
			 * packet has trailing header; dmc_get will then
			 * force this header information to be at the front,
			 * but we still have to drop the type and length
			 * which are at the front of any trailer data.
			 */
			m = if_ubaget(&sc->sc_ifuba, ifrw, ifp, len);
			if (m == 0)
				goto setup;
			/* Shave off dmc_header */
			m_adj(m, sizeof(struct dmc_header));
			switch (dh->dmc_type) {
#ifdef INET
			case DMC_IPTYPE:
				break;
#endif
			default:
				m_freem(m);
				goto setup;
			}

			s = splnet();
			if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
				m_freem(m);
			}
			splx(s);

	setup:
			/* is this needed? */
			rp->ubinfo = ifrw->ifrw_info;

			dmcload(sc, DMC_READ, rp->ubinfo,
			    ((rp->ubinfo >> 2) & DMC_XMEM) | rp->cc);
			break;

		case DMC_OUX:
			/*
			 * A write has completed, start another
			 * transfer if there is more data to send.
			 */
			ifp->if_opackets++;
			/* find associated dmcbuf structure */
			ifxp = &sc->sc_ifw[0];
			for (rp = &sc->sc_xbufs[0]; rp < &sc->sc_xbufs[NXMT]; rp++) {
				if(rp->ubinfo == pkaddr)
					break;
				ifxp++;
			}
			if (rp >= &sc->sc_xbufs[NXMT]) {
				aprint_error_dev(sc->sc_dev, "bad packet address 0x%x\n",
				    pkaddr);
				break;
			}
			if ((rp->flags & DBUF_DMCS) == 0)
				aprint_error_dev(sc->sc_dev, "unallocated packet 0x%x\n",
				    pkaddr);
			/* mark buffer free */
			if_ubaend(&sc->sc_ifuba, ifxp);
			rp->flags &= ~DBUF_DMCS;
			if (--sc->sc_oused == 0)
				sc->sc_if.if_timer = 0;
			else
				sc->sc_if.if_timer = dmc_timeout;
			if ((sc->sc_flag & DMC_ONLINE) == 0) {
				extern int ifqmaxlen;

				/*
				 * We're on the air.
				 * Open the queue to the usual value.
				 */
				sc->sc_flag |= DMC_ONLINE;
				ifp->if_snd.ifq_maxlen = ifqmaxlen;
			}
			break;

		case DMC_CNTLO:
			arg &= DMC_CNTMASK;
			if (arg & DMC_FATAL) {
				if (arg != DMC_START) {
					snprintb(buf, sizeof(buf), CNTLO_BITS,
					    arg);
					log(LOG_ERR,
					    "%s: fatal error, flags=%s\n",
					    device_xname(sc->sc_dev), buf);
				}
				dmcrestart(sc);
				break;
			}
			/* ACCUMULATE STATISTICS */
			switch(arg) {
			case DMC_NOBUFS:
				ifp->if_ierrors++;
				if ((sc->sc_nobuf++ % DMC_RPNBFS) == 0)
					goto report;
				break;
			case DMC_DISCONN:
				if ((sc->sc_disc++ % DMC_RPDSC) == 0)
					goto report;
				break;
			case DMC_TIMEOUT:
				if ((sc->sc_timeo++ % DMC_RPTMO) == 0)
					goto report;
				break;
			case DMC_DATACK:
				ifp->if_oerrors++;
				if ((sc->sc_datck++ % DMC_RPDCK) == 0)
					goto report;
				break;
			default:
				goto report;
			}
			break;
		report:
#ifdef DMCDEBUG
			snprintb(buf, sizeof(buf), CNTLO_BITS, arg);
			printd("%s: soft error, flags=%s\n",
			    device_xname(sc->sc_dev), buf);
#endif
			if ((sc->sc_flag & DMC_RESTART) == 0) {
				/*
				 * kill off the dmc to get things
				 * going again by generating a
				 * procedure error
				 */
				sc->sc_flag |= DMC_RESTART;
				arg = sc->sc_ui.ui_baddr;
				dmcload(sc, DMC_BASEI, arg, (arg>>2)&DMC_XMEM);
			}
			break;

		default:
			printf("%s: bad control %o\n",
			    device_xname(sc->sc_dev), cmd);
			break;
		}
	}
	dmcstart(ifp);
}

/*
 * DMC output routine.
 * Encapsulate a packet of type family for the dmc.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 */
int
dmcoutput(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst,
    struct rtentry *rt)
{
	int type, error, s;
	struct mbuf *m = m0;
	struct dmc_header *dh;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & IFF_UP) == 0) {
		error = ENETDOWN;
		goto bad;
	}

	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {
#ifdef	INET
	case AF_INET:
		type = DMC_IPTYPE;
		break;
#endif

	case AF_UNSPEC:
		dh = (struct dmc_header *)dst->sa_data;
		type = dh->dmc_type;
		break;

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

	/*
	 * Add local network header
	 * (there is space for a uba on a vax to step on)
	 */
	M_PREPEND(m, sizeof(struct dmc_header), M_DONTWAIT);
	if (m == 0) {
		error = ENOBUFS;
		goto bad;
	}
	dh = mtod(m, struct dmc_header *);
	dh->dmc_type = htons((u_short)type);

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	dmcstart(ifp);
	splx(s);
	return (0);

bad:
	m_freem(m0);
	return (error);
}


/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
dmcioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int s = splnet(), error = 0;
	register struct dmc_softc *sc = ifp->if_softc;

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			dmcinit(ifp);
		break;

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			dmcinit(ifp);
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    sc->sc_flag & DMC_RUNNING)
			dmcdown(sc);
		else if (ifp->if_flags & IFF_UP &&
		    (sc->sc_flag & DMC_RUNNING) == 0)
			dmcrestart(sc);
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
	}
	splx(s);
	return (error);
}

/*
 * Restart after a fatal error.
 * Clear device and reinitialize.
 */
void
dmcrestart(struct dmc_softc *sc)
{
	int s, i;

#ifdef DMCDEBUG
	/* dump base table */
	printf("%s base table:\n", device_xname(sc->sc_dev));
	for (i = 0; i < sizeof (struct dmc_base); i++)
		printf("%o\n" ,dmc_base[unit].d_base[i]);
#endif

	dmcdown(sc);

	/*
	 * Let the DMR finish the MCLR.	 At 1 Mbit, it should do so
	 * in about a max of 6.4 milliseconds with diagnostics enabled.
	 */
	for (i = 100000; i && (DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0; i--)
		;
	/* Did the timer expire or did the DMR finish? */
	if ((DMC_RBYTE(DMC_BSEL1) & DMC_RUN) == 0) {
		log(LOG_ERR, "%s: M820 Test Failed\n", device_xname(sc->sc_dev));
		return;
	}

	/* restart DMC */
	dmcinit(&sc->sc_if);
	sc->sc_flag &= ~DMC_RESTART;
	s = splnet();
	dmcstart(&sc->sc_if);
	splx(s);
	sc->sc_if.if_collisions++;	/* why not? */
}

/*
 * Reset a device and mark down.
 * Flush output queue and drop queue limit.
 */
void
dmcdown(struct dmc_softc *sc)
{
	struct ifxmt *ifxp;

	DMC_WBYTE(DMC_BSEL1, DMC_MCLR);
	sc->sc_flag &= ~(DMC_RUNNING | DMC_ONLINE);

	for (ifxp = sc->sc_ifw; ifxp < &sc->sc_ifw[NXMT]; ifxp++) {
#ifdef notyet
		if (ifxp->ifw_xtofree) {
			(void) m_freem(ifxp->ifw_xtofree);
			ifxp->ifw_xtofree = 0;
		}
#endif
	}
	IF_PURGE(&sc->sc_if.if_snd);
}

/*
 * Watchdog timeout to see that transmitted packets don't
 * lose interrupts.  The device has to be online (the first
 * transmission may block until the other side comes up).
 */
void
dmctimeout(struct ifnet *ifp)
{
	struct dmc_softc *sc = ifp->if_softc;
	char buf1[64], buf2[64];

	if (sc->sc_flag & DMC_ONLINE) {
		snprintb(buf1, sizeof(buf1), DMC0BITS,
		    DMC_RBYTE(DMC_BSEL0) & 0xff);
		snprintb(buf2, sizeof(buf2), DMC2BITS,
		    DMC_RBYTE(DMC_BSEL2) & 0xff);
		log(LOG_ERR, "%s: output timeout, bsel0=%s bsel2=%s\n",
		    device_xname(sc->sc_dev), buf1, buf2);
		dmcrestart(sc);
	}
}
