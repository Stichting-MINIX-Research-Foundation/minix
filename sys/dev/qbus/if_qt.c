/*	$NetBSD: if_qt.c,v 1.18 2010/04/05 07:21:47 joerg Exp $	*/
/*
 * Copyright (c) 1992 Steven M. Schultz
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
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 *	@(#)if_qt.c     1.2 (2.11BSD) 2/20/93
 */
/*
 *
 * Modification History
 * 23-Feb-92 -- sms
 *	Rewrite the buffer handling so that fewer than the maximum number of
 *	buffers may be used (32 receive and 12 transmit buffers consume 66+kb
 *	of main system memory in addition to the internal structures in the
 *	networking code).  A freelist of available buffers is maintained now.
 *	When I/O operations complete the associated buffer is placed on the
 *	freelist (a single linked list for simplicity) and when an I/O is
 *	started a buffer is pulled off the list.
 *
 * 20-Feb-92 -- sms
 *	It works!  Darned board couldn't handle "short" rings - those rings
 *	where only half the entries were made available to the board (the
 *	ring descriptors were the full size, merely half the entries were
 * 	flagged as belonging always to the driver).  Grrrr.  Would have thought
 *	the board could skip over those entries reserved by the driver.
 *	Now to find a way not to have to allocated 32+12 times 1.5kb worth of
 *	buffers...
 *
 * 03-Feb-92 -- sms
 *	Released but still not working.  The driver now responds to arp and
 *	ping requests.  The board is apparently not returning ring descriptors
 *	to the driver so eventually we run out of buffers.  Back to the
 *	drawing board.
 *
 * 28-Dec-92 -- sms
 *	Still not released.  Hiatus in finding free time and thin-netting
 *	the systems (many thanks Terry!).
 *	Added logic to dynamically allocate a vector and initialize it.
 *
 * 23-Oct-92 -- sms
 *	The INIT block must (apparently) be quadword aligned [no thanks to
 *	the manual for not mentioning that fact].  The necessary alignment
 *	is achieved by allocating the INIT block from main memory ('malloc'
 *	guarantees click alignment) and mapping it as needed (which is _very_
 *	infrequently).  A check for quadword alignment of the ring descriptors
 *	was added - at present the descriptors are properly aligned, if this
 *	should change then something will have to be done (like do it "right").
 *	Darned alignment restrictions!
 *
 *	A couple of typos were corrected (missing parentheses, reversed
 *	arguments to printf calls, etc).
 *
 * 13-Oct-92 -- sms@wlv.iipo.gtegsc.com
 *	Created based on the DELQA-PLUS addendum to DELQA User's Guide.
 *	This driver ('qt') is selected at system configuration time.  If the
 *	board *	is not a DELQA-YM an error message will be printed and the
 *	interface will not be attached.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_qt.c,v 1.18 2010/04/05 07:21:47 joerg Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <sys/domain.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>


#include <sys/bus.h>

#include <dev/qbus/ubavar.h>
#include <dev/qbus/if_uba.h>
#include <dev/qbus/if_qtreg.h>

#define NRCV	QT_MAX_RCV	/* Receive descriptors (must be == 32) */
#define NXMT	QT_MAX_XMT	/* Transmit descriptors	(must be == 12) */
#if	NRCV != 32 || NXMT != 12
	hardware requires these sizes.
#endif

/*
 * Control data structures, must be in DMA-friendly memory.
 */
struct	qt_cdata {
	struct	qt_init qc_init;	/* Init block			*/
	struct	qt_rring qc_r[NRCV];	/* Receive descriptor ring	*/
	struct	qt_tring qc_t[NXMT];	/* Transmit descriptor ring	*/
};

struct	qt_softc {
	device_t sc_dev;		/* Configuration common part */
	struct	ethercom is_ec;		/* common part - must be first  */
	struct uba_softc *sc_uh;
	struct	evcnt sc_intrcnt;	/* Interrupt counting */
#define	is_if	is_ec.ec_if		/* network-visible interface	*/
	u_int8_t is_addr[ETHER_ADDR_LEN]; /* hardware Ethernet address	*/
	bus_space_tag_t	sc_iot;
	bus_addr_t	sc_ioh;

	struct	ubinfo	sc_ui;		/* control block address desc	*/
	struct	qt_cdata *sc_ib;	/* virt address of ctrl block	*/
	struct	qt_cdata *sc_pib;	/* phys address of ctrl block	*/

	struct	ifubinfo sc_ifuba;	/* UNIBUS resources */
	struct	ifrw sc_ifr[NRCV];	/* UNIBUS receive buffer maps */
	struct	ifxmt sc_ifw[NXMT];	/* UNIBUS receive buffer maps */

	int	rindex;			/* Receive Completed Index	*/
	int	nxtrcv;			/* Next Receive Index		*/
	int	nrcv;			/* Number of Receives active	*/

	int	xnext;			/* Next descriptor to transmit	*/
	int	xlast;			/* Last descriptor transmitted	*/
	int	nxmit;			/* # packets in send queue	*/

	short	vector;			/* Interrupt vector assigned	*/
};

static	int qtmatch(device_t, cfdata_t, void *);
static	void qtattach(device_t, device_t, void *);
static	void qtintr(void *);
static	int qtinit(struct ifnet *);
static	int qtioctl(struct ifnet *, u_long, void *);
static	int qtturbo(struct qt_softc *);
static	void qtstart(struct ifnet *ifp);
static	void qtstop(struct ifnet *ifp, int disable);
static	void qtsrr(struct qt_softc *, int);
static	void qtrint(struct qt_softc *sc);
static	void qttint(struct qt_softc *sc);

/* static	void qtrestart(struct qt_softc *sc); */

CFATTACH_DECL_NEW(qt, sizeof(struct qt_softc),
    qtmatch, qtattach, NULL, NULL);

/*
 * Maximum packet size needs to include 4 bytes for the CRC
 * on received packets.
*/
#define MAXPACKETSIZE (ETHERMTU + sizeof (struct ether_header) + 4)
#define	MINPACKETSIZE 64

#define QT_WCSR(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define QT_RCSR(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)


#define loint(x)	((int)(x) & 0xffff)
#define hiint(x)	(((int)(x) >> 16) & 0x3f)

/*
 * Check if this card is a turbo delqa.
 */
int
qtmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct qt_softc ssc;
	struct qt_softc *sc = &ssc;
	struct uba_attach_args *ua = aux;
	struct uba_softc *uh = device_private(parent);
	struct qt_init *qi;
	struct ubinfo ui;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	if (qtturbo(sc) == 0)
		return 0; /* Not a turbo card */

	/* Force the card to interrupt */
	ui.ui_size = sizeof(struct qt_init);
	if (ubmemalloc(uh, &ui, 0))
		return 0; /* Failed */
	qi = (struct qt_init *)ui.ui_vaddr;
	memset(qi, 0, sizeof(struct qt_init));
	qi->vector = uh->uh_lastiv - 4;
	qi->options = INIT_OPTIONS_INT;

	QT_WCSR(CSR_IBAL, loint(ui.ui_baddr));
	QT_WCSR(CSR_IBAH, hiint(ui.ui_baddr));
	QT_WCSR(CSR_SRQR, 2);
	delay(100000); /* Wait some time for interrupt */
	QT_WCSR(CSR_SRQR, 3); /* Stop card */

	ubmemfree(uh, &ui);

	return 10;
}


/*
 * Interface exists.  More accurately, something exists at the CSR (see
 * sys/sys_net.c) -- there's no guarantee it's a DELQA-YM.
 *
 * The ring descriptors are initialized, the buffers allocated using first the
 * DMA region allocated at network load time and then later main memory.  The
 * INIT block is filled in and the device is poked/probed to see if it really
 * is a DELQA-YM.  If the device is not a -YM then a message is printed and
 * the 'if_attach' call is skipped.  For a -YM the START command is issued,
 * but the device is not marked as running|up - that happens at interrupt level
 * when the device interrupts to say it has started.
*/

void
qtattach(device_t parent, device_t self, void *aux)
{
	struct qt_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->is_if;
	struct uba_attach_args *ua = aux;

	sc->sc_dev = self;

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, qtintr, sc,
	    &sc->sc_intrcnt);
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
	    device_xname(sc->sc_dev), "intr");

	sc->sc_uh = device_private(parent);
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_uh->uh_lastiv -= 4;
	sc->vector = sc->sc_uh->uh_lastiv;

/*
 * Now allocate the buffers and initialize the buffers.  This should _never_
 * fail because main memory is allocated after the DMA pool is used up.
*/

	sc->is_addr[0] = QT_RCSR(0);
	sc->is_addr[1] = QT_RCSR(2);
	sc->is_addr[2] = QT_RCSR(4);
	sc->is_addr[3] = QT_RCSR(6);
	sc->is_addr[4] = QT_RCSR(8);
	sc->is_addr[5] = QT_RCSR(10);

	strcpy(ifp->if_xname, device_xname(sc->sc_dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_MULTICAST;
	ifp->if_ioctl = qtioctl;
	ifp->if_start = qtstart;
	ifp->if_init = qtinit;
	ifp->if_stop = qtstop;
	IFQ_SET_READY(&ifp->if_snd);

	printf("\n%s: delqa-plus in Turbo mode, hardware address %s\n",
	    device_xname(sc->sc_dev), ether_sprintf(sc->is_addr));
	if_attach(ifp);
	ether_ifattach(ifp, sc->is_addr);
}

int
qtturbo(struct qt_softc *sc)
{
	int i;

/*
 * Issue the software reset.  Delay 150us.  The board should now be in
 * DELQA-Normal mode.  Set ITB and DEQTA select.  If both bits do not
 * stay turned on then the board is not a DELQA-YM.
*/
	QT_WCSR(CSR_ARQR, ARQR_SR);
	QT_WCSR(CSR_ARQR, 0);
	delay(150L);

	QT_WCSR(CSR_SRR, 0x8001);	/* MS | ITB */
	i = QT_RCSR(CSR_SRR);
	QT_WCSR(CSR_SRR, 0x8000);	/* Turn off ITB, set DELQA select */
	if (i != 0x8001)
		return(0);
/*
 * Board is a DELQA-YM.  Send the commands to enable Turbo mode.  Delay
 * 1 second, testing the SRR register every millisecond to see if the
 * board has shifted to Turbo mode.
*/
	QT_WCSR(CSR_XCR0, 0x0baf);
	QT_WCSR(CSR_XCR1, 0xff00);
	for	(i = 0; i < 1000; i++)
		{
		if	((QT_RCSR(CSR_SRR) & SRR_RESP) == 1)
			break;
		delay(1000L);
		}
	if	(i >= 1000)
		{
		printf("qt !Turbo\n");
		return(0);
		}
	return(1);
}

int
qtinit(struct ifnet *ifp)
{
	struct qt_softc *sc = ifp->if_softc;
	struct qt_init *iniblk;
	struct ifrw *ifrw;
	struct ifxmt *ifxp;
	struct	qt_rring *rp;
	struct	qt_tring *tp;
	int i, error;

	if (ifp->if_flags & IFF_RUNNING) {
		/* Cancel any pending I/O. */
		qtstop(ifp, 0);
	}

	if (sc->sc_ib == NULL) {
		if (if_ubaminit(&sc->sc_ifuba, sc->sc_uh,
		    MCLBYTES, sc->sc_ifr, NRCV, sc->sc_ifw, NXMT)) {
			printf("%s: can't initialize\n", device_xname(sc->sc_dev));
			ifp->if_flags &= ~IFF_UP;
			return 0;
		}
		sc->sc_ui.ui_size = sizeof(struct qt_cdata);
		if ((error = ubmemalloc(sc->sc_uh, &sc->sc_ui, 0))) {
			printf(": failed ubmemalloc(), error = %d\n", error);
			return error;
		}
		sc->sc_ib = (struct qt_cdata *)sc->sc_ui.ui_vaddr;
		sc->sc_pib = (struct qt_cdata *)sc->sc_ui.ui_baddr;

/*
 * Fill in most of the INIT block: vector, options (interrupt enable), ring
 * locations.  The physical address is copied from the ROMs as part of the
 * -YM testing proceedure.  The CSR is saved here rather than in qtinit()
 * because the qtturbo() routine needs it.
 *
 * The INIT block must be quadword aligned.  Using malloc() guarantees click
 * (64 byte) alignment.  Since the only time the INIT block is referenced is
 * at 'startup' or 'reset' time there is really no time penalty (and a modest
 * D space savings) involved.
*/
		memset(sc->sc_ib, 0, sizeof(struct qt_cdata));
		iniblk = &sc->sc_ib->qc_init;

		iniblk->vector = sc->vector;
		memcpy(iniblk->paddr, sc->is_addr, 6);

		iniblk->options = INIT_OPTIONS_INT;
		iniblk->rx_lo = loint(&sc->sc_pib->qc_r);
		iniblk->rx_hi = hiint(&sc->sc_pib->qc_r);
		iniblk->tx_lo = loint(&sc->sc_pib->qc_t);
		iniblk->tx_hi = hiint(&sc->sc_pib->qc_t);
	}
	iniblk = &sc->sc_ib->qc_init;
	iniblk->mode = ifp->if_flags & IFF_PROMISC ? INIT_MODE_PRO : 0;


/*
 * Now initialize the receive ring descriptors.  Because this routine can be
 * called with outstanding I/O operations we check the ring descriptors for
 * a non-zero 'rhost0' (or 'thost0') word and place those buffers back on
 * the free list.
*/
	for (i = 0; i < NRCV; i++) {
		rp = &sc->sc_ib->qc_r[i];
		ifrw = &sc->sc_ifr[i];
		rp->rmd1 = MCLBYTES;
		rp->rmd4 = loint(ifrw->ifrw_info);
		rp->rmd5 = hiint(ifrw->ifrw_info);
		rp->rmd3 = 0;			/* clear RMD3_OWN */
		}
	for (i = 0; i < NXMT; i++) {
		tp = &sc->sc_ib->qc_t[i];
		ifxp = &sc->sc_ifw[i];
		tp->tmd4 = loint(ifxp->ifw_info);
		tp->tmd5 = hiint(ifxp->ifw_info);
		tp->tmd3 = TMD3_OWN;
		}

	sc->xnext = sc->xlast = sc->nxmit = 0;
	sc->rindex = 0;
	sc->nxtrcv = 0;
	sc->nrcv = 0;

/*
 * Now we tell the device the address of the INIT block.  The device
 * _must_ be in the Turbo mode at this time.  The "START" command is
 * then issued to the device.  A 1 second timeout is then started.
 * When the interrupt occurs the IFF_UP|IFF_RUNNING state is entered and
 * full operations will proceed.  If the timeout expires without an interrupt
 * being received an error is printed, the flags cleared and the device left
 * marked down.
*/
	QT_WCSR(CSR_IBAL, loint(&sc->sc_pib->qc_init));
	QT_WCSR(CSR_IBAH, hiint(&sc->sc_pib->qc_init));
	QT_WCSR(CSR_SRQR, 2);

	sc->is_if.if_flags |= IFF_RUNNING;
	return 0;
	}

/*
 * Start output on interface.
 */

void
qtstart(struct ifnet *ifp)
	{
	int	len, nxmit;
	struct qt_softc *sc = ifp->if_softc;
	struct qt_tring *rp;
	struct	mbuf *m = NULL;

	for (nxmit = sc->nxmit; nxmit < NXMT; nxmit++) {
		IF_DEQUEUE(&sc->is_if.if_snd, m);
		if	(m == 0)
			break;

		rp = &sc->sc_ib->qc_t[sc->xnext];
		if ((rp->tmd3 & TMD3_OWN) == 0)
			panic("qtstart");

		bpf_mtap(ifp, m);

		len = if_ubaput(&sc->sc_ifuba, &sc->sc_ifw[sc->xnext], m);
		if (len < MINPACKETSIZE)
			len = MINPACKETSIZE;
		rp->tmd3 = len & TMD3_BCT;	/* set length,clear ownership */
		QT_WCSR(CSR_ARQR, ARQR_TRQ);	/* tell device it has buffer */

		if	(++sc->xnext >= NXMT)
			sc->xnext = 0;
	}
	if (sc->nxmit != nxmit)
		sc->nxmit = nxmit;
	/* XXX - set OACTIVE */
}

/*
 * General interrupt service routine.  Receive, transmit, device start
 * interrupts and timeouts come here.  Check for hard device errors and print a
 * message if any errors are found.  If we are waiting for the device to
 * START then check if the device is now running.
*/

void
qtintr(void *arg)
	{
	struct qt_softc *sc = arg;
	struct ifnet *ifp = &sc->is_if;
	short status;


	status = QT_RCSR(CSR_SRR);
	if	(status < 0)
		/* should we reset the device after a bunch of these errs? */
		qtsrr(sc, status);
	if ((ifp->if_flags & IFF_UP) == 0)
		return; /* Unwanted interrupt */
	qtrint(sc);
	qttint(sc);
	qtstart(&sc->is_ec.ec_if);
	}

/*
 * Transmit interrupt service.  Only called if there are outstanding transmit
 * requests which could have completed.  The DELQA-YM doesn't provide the
 * status bits telling the kind (receive, transmit) of interrupt.
*/

#define BBLMIS (TMD2_BBL|TMD2_MIS)

void
qttint(struct qt_softc *sc)
	{
	struct qt_tring *rp;

	while (sc->nxmit > 0)
		{
		rp = &sc->sc_ib->qc_t[sc->xlast];
		if ((rp->tmd3 & TMD3_OWN) == 0)
			break;
		sc->is_if.if_opackets++;
/*
 * Collisions don't count as output errors, but babbling and missing packets
 * do count as output errors.
*/
		if	(rp->tmd2 & TMD2_CER)
			sc->is_if.if_collisions++;
		if	((rp->tmd0 & TMD0_ERR1) ||
			 ((rp->tmd2 & TMD2_ERR2) && (rp->tmd2 & BBLMIS)))
			{
#ifdef QTDEBUG
			char buf[100];
			snprintb(buf, sizeof(buf), TMD2_BITS, rp->tmd2);
			printf("%s: tmd2 %s\n", device_xname(sc->sc_dev), buf);
#endif
			sc->is_if.if_oerrors++;
			}
		if_ubaend(&sc->sc_ifuba, &sc->sc_ifw[sc->xlast]);
		if	(++sc->xlast >= NXMT)
			sc->xlast = 0;
		sc->nxmit--;
		}
	}

/*
 * Receive interrupt service.  Pull packet off the interface and put into
 * a mbuf chain for processing later.
*/

void
qtrint(struct qt_softc *sc)
{
	struct qt_rring *rp;
	struct ifnet *ifp = &sc->is_ec.ec_if;
	struct mbuf *m;
	int	len;

	while	(sc->sc_ib->qc_r[(int)sc->rindex].rmd3 & RMD3_OWN)
		{
		rp = &sc->sc_ib->qc_r[(int)sc->rindex];
		if     ((rp->rmd0 & (RMD0_STP|RMD0_ENP)) != (RMD0_STP|RMD0_ENP))
			{
			printf("%s: chained packet\n", device_xname(sc->sc_dev));
			sc->is_if.if_ierrors++;
			goto rnext;
			}
		len = (rp->rmd1 & RMD1_MCNT) - 4;	/* -4 for CRC */
		sc->is_if.if_ipackets++;

		if	((rp->rmd0 & RMD0_ERR3) || (rp->rmd2 & RMD2_ERR4))
			{
#ifdef QTDEBUG
			char buf[100];
			snprintb(buf, sizeof(buf), RMD0_BITS, rp->rmd0);
			printf("%s: rmd0 %s\n", device_xname(sc->sc_dev), buf);
			snprintb(buf, sizeof(buf), RMD2_BITS, rp->rmd2);
			printf("%s: rmd2 %s\n", device_xname(sc->sc_dev), buf);
#endif
			sc->is_if.if_ierrors++;
			goto rnext;
			}
		m = if_ubaget(&sc->sc_ifuba, &sc->sc_ifr[(int)sc->rindex],
		    ifp, len);
		if (m == 0) {
			sc->is_if.if_ierrors++;
			goto rnext;
		}
		bpf_mtap(ifp, m);
		(*ifp->if_input)(ifp, m);
rnext:
		--sc->nrcv;
		rp->rmd3 = 0;
		rp->rmd1 = MCLBYTES;
		if	(++sc->rindex >= NRCV)
			sc->rindex = 0;
		}
	QT_WCSR(CSR_ARQR, ARQR_RRQ);	/* tell device it has buffer */
	}

int
qtioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			error = qtinit(ifp);
		else
			error = 0;
	}
	splx(s);
	return (error);
}

void
qtsrr(struct qt_softc *sc, int srrbits)
{
	char buf[100];
	snprintb(buf, sizeof(buf), SRR_BITS, srrbits);
	printf("%s: srr=%s\n", device_xname(sc->sc_dev), buf);
}

/*
 * Stop activity on the interface.
 * Lose outstanding transmit requests. XXX - not good for multicast.
 */
void
qtstop(struct ifnet *ifp, int disable)
{
	struct qt_softc *sc = ifp->if_softc;
	int i;

	QT_WCSR(CSR_SRQR, 3);
	for (i = 0; i < 100; i++)
		if ((QT_RCSR(CSR_SRR) & SRR_RESP) == 3)
			break;
	if (QT_RCSR(CSR_SRR) & SRR_FES)
		qtsrr(sc, QT_RCSR(CSR_SRR));
	/* Forget already queued transmit requests */
	while (sc->nxmit > 0) {
		if_ubaend(&sc->sc_ifuba, &sc->sc_ifw[sc->xlast]);
		if (++sc->xlast >= NXMT)
			sc->xlast = 0;
		sc->nxmit--;
	}
	/* Handle late received packets */
	qtrint(sc);
	ifp->if_flags &= ~IFF_RUNNING;
}

#ifdef notyet
/*
 * Reset the device.  This moves it from DELQA-T mode to DELQA-Normal mode.
 * After the reset put the device back in -T mode.  Then call qtinit() to
 * reinitialize the ring structures and issue the 'timeout' for the "device
 * started interrupt".
*/

void
qtreset(sc)
	struct qt_softc *sc;
	{

	qtturbo(sc);
	qtinit(&sc->is_ec.ec_if);
	}
#endif
