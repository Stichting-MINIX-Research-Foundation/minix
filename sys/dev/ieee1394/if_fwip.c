/*	$NetBSD: if_fwip.c,v 1.26 2014/02/25 18:30:09 pooka Exp $	*/
/*-
 * Copyright (c) 2004
 *	Doug Rabson
 * Copyright (c) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 * $FreeBSD: src/sys/dev/firewire/if_fwip.c,v 1.18 2009/02/09 16:58:18 fjoe Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fwip.c,v 1.26 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_ieee1394.h>
#include <net/if_types.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/iec13213.h>
#include <dev/ieee1394/if_fwipvar.h>

/*
 * We really need a mechanism for allocating regions in the FIFO
 * address space. We pick a address in the OHCI controller's 'middle'
 * address space. This means that the controller will automatically
 * send responses for us, which is fine since we don't have any
 * important information to put in the response anyway.
 */
#define INET_FIFO	0xfffe00000000LL

#define FWIPDEBUG	if (fwipdebug) aprint_debug_ifnet
#define TX_MAX_QUEUE	(FWMAXQUEUE - 1)


struct fw_hwaddr {
	uint32_t		sender_unique_ID_hi;
	uint32_t		sender_unique_ID_lo;
	uint8_t			sender_max_rec;
	uint8_t			sspd;
	uint16_t		sender_unicast_FIFO_hi;
	uint32_t		sender_unicast_FIFO_lo;
};


static int fwipmatch(device_t, cfdata_t, void *);
static void fwipattach(device_t, device_t, void *);
static int fwipdetach(device_t, int);
static int fwipactivate(device_t, enum devact);

/* network interface */
static void fwip_start(struct ifnet *);
static int fwip_ioctl(struct ifnet *, u_long, void *);
static int fwip_init(struct ifnet *);
static void fwip_stop(struct ifnet *, int);

static void fwip_post_busreset(void *);
static void fwip_output_callback(struct fw_xfer *);
static void fwip_async_output(struct fwip_softc *, struct ifnet *);
static void fwip_stream_input(struct fw_xferq *);
static void fwip_unicast_input(struct fw_xfer *);

static int fwipdebug = 0;
static int broadcast_channel = 0xc0 | 0x1f; /*  tag | channel(XXX) */
static int tx_speed = 2;
static int rx_queue_len = FWMAXQUEUE;

/*
 * Setup sysctl(3) MIB, hw.fwip.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_fwip, "sysctl fwip(4) subtree setup")
{
	int rc, fwip_node_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "fwip",
	    SYSCTL_DESCR("fwip controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	fwip_node_num = node->sysctl_num;

	/* fwip RX queue length */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "rx_queue_len", SYSCTL_DESCR("Length of the receive queue"),
	    NULL, 0, &rx_queue_len,
	    0, CTL_HW, fwip_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* fwip RX queue length */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "if_fwip_debug", SYSCTL_DESCR("fwip driver debug flag"),
	    NULL, 0, &fwipdebug,
	    0, CTL_HW, fwip_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}


CFATTACH_DECL_NEW(fwip, sizeof(struct fwip_softc),
    fwipmatch, fwipattach, fwipdetach, fwipactivate);


static int
fwipmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct fw_attach_args *fwa = aux;

	if (strcmp(fwa->name, "fwip") == 0)
		return 1;
	return 0;
}

static void
fwipattach(device_t parent, device_t self, void *aux)
{
	struct fwip_softc *sc = device_private(self);
	struct fw_attach_args *fwa = (struct fw_attach_args *)aux;
	struct fw_hwaddr *hwaddr;
	struct ifnet *ifp;

	aprint_naive("\n");
	aprint_normal(": IP over IEEE1394\n");

	sc->sc_fd.dev = self;
	sc->sc_eth.fwip_ifp = &sc->sc_eth.fwcom.fc_if;
	hwaddr = (struct fw_hwaddr *)&sc->sc_eth.fwcom.ic_hwaddr;

	ifp = sc->sc_eth.fwip_ifp;

	mutex_init(&sc->sc_fwb.fwb_mtx, MUTEX_DEFAULT, IPL_NET);
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NET);

	/* XXX */
	sc->sc_dma_ch = -1;

	sc->sc_fd.fc = fwa->fc;
	if (tx_speed < 0)
		tx_speed = sc->sc_fd.fc->speed;

	sc->sc_fd.post_explore = NULL;
	sc->sc_fd.post_busreset = fwip_post_busreset;
	sc->sc_eth.fwip = sc;

	/*
	 * Encode our hardware the way that arp likes it.
	 */
	hwaddr->sender_unique_ID_hi = htonl(sc->sc_fd.fc->eui.hi);
	hwaddr->sender_unique_ID_lo = htonl(sc->sc_fd.fc->eui.lo);
	hwaddr->sender_max_rec = sc->sc_fd.fc->maxrec;
	hwaddr->sspd = sc->sc_fd.fc->speed;
	hwaddr->sender_unicast_FIFO_hi = htons((uint16_t)(INET_FIFO >> 32));
	hwaddr->sender_unicast_FIFO_lo = htonl((uint32_t)INET_FIFO);

	/* fill the rest and attach interface */
	ifp->if_softc = &sc->sc_eth;

	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_start = fwip_start;
	ifp->if_ioctl = fwip_ioctl;
	ifp->if_init = fwip_init;
	ifp->if_stop = fwip_stop;
	ifp->if_flags = (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST);
	IFQ_SET_READY(&ifp->if_snd);
	IFQ_SET_MAXLEN(&ifp->if_snd, TX_MAX_QUEUE);

	if_attach(ifp);
	ieee1394_ifattach(ifp, (const struct ieee1394_hwaddr *)hwaddr);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
	else
		pmf_class_network_register(self, ifp);

	FWIPDEBUG(ifp, "interface created\n");
	return;
}

static int
fwipdetach(device_t self, int flags)
{
	struct fwip_softc *sc = device_private(self);
	struct ifnet *ifp = sc->sc_eth.fwip_ifp;

	fwip_stop(sc->sc_eth.fwip_ifp, 1);
	ieee1394_ifdetach(ifp);
	if_detach(ifp);
	mutex_destroy(&sc->sc_mtx);
	mutex_destroy(&sc->sc_fwb.fwb_mtx);
	return 0;
}

static int
fwipactivate(device_t self, enum devact act)
{
	struct fwip_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(sc->sc_eth.fwip_ifp);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
fwip_start(struct ifnet *ifp)
{
	struct fwip_softc *sc = ((struct fwip_eth_softc *)ifp->if_softc)->fwip;

	FWIPDEBUG(ifp, "starting\n");

	if (sc->sc_dma_ch < 0) {
		struct mbuf *m = NULL;

		FWIPDEBUG(ifp, "not ready\n");

		do {
			IF_DEQUEUE(&ifp->if_snd, m);
			if (m != NULL)
				m_freem(m);
			ifp->if_oerrors++;
		} while (m != NULL);

		return;
	}

	ifp->if_flags |= IFF_OACTIVE;

	if (ifp->if_snd.ifq_len != 0)
		fwip_async_output(sc, ifp);

	ifp->if_flags &= ~IFF_OACTIVE;
}

static int
fwip_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			fwip_stop(ifp, 0);
			break;
		case IFF_UP:
			fwip_init(ifp);
			break;
		default:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	default:
		error = ieee1394_ioctl(ifp, cmd, data);
		if (error == ENETRESET)
			error = 0;
		break;
	}

	splx(s);

	return error;
}

static int
fwip_init(struct ifnet *ifp)
{
	struct fwip_softc *sc = ((struct fwip_eth_softc *)ifp->if_softc)->fwip;
	struct firewire_comm *fc;
	struct fw_xferq *xferq;
	struct fw_xfer *xfer;
	struct mbuf *m;
	int i;

	FWIPDEBUG(ifp, "initializing\n");

	fc = sc->sc_fd.fc;
	if (sc->sc_dma_ch < 0) {
		sc->sc_dma_ch = fw_open_isodma(fc, /* tx */0);
		if (sc->sc_dma_ch < 0)
			return ENXIO;
		xferq = fc->ir[sc->sc_dma_ch];
		xferq->flag |=
		    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_STREAM;
		xferq->flag &= ~0xff;
		xferq->flag |= broadcast_channel & 0xff;
		/* register fwip_input handler */
		xferq->sc = (void *) sc;
		xferq->hand = fwip_stream_input;
		xferq->bnchunk = rx_queue_len;
		xferq->bnpacket = 1;
		xferq->psize = MCLBYTES;
		xferq->queued = 0;
		xferq->buf = NULL;
		xferq->bulkxfer = (struct fw_bulkxfer *) malloc(
			sizeof(struct fw_bulkxfer) * xferq->bnchunk,
							M_FW, M_WAITOK);
		if (xferq->bulkxfer == NULL) {
			aprint_error_ifnet(ifp, "if_fwip: malloc failed\n");
			return ENOMEM;
		}
		STAILQ_INIT(&xferq->stvalid);
		STAILQ_INIT(&xferq->stfree);
		STAILQ_INIT(&xferq->stdma);
		xferq->stproc = NULL;
		for (i = 0; i < xferq->bnchunk; i++) {
			m = m_getcl(M_WAITOK, MT_DATA, M_PKTHDR);
			xferq->bulkxfer[i].mbuf = m;
			if (m != NULL) {
				m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
				STAILQ_INSERT_TAIL(&xferq->stfree,
						&xferq->bulkxfer[i], link);
			} else
				aprint_error_ifnet(ifp,
				    "fwip_as_input: m_getcl failed\n");
		}

		sc->sc_fwb.start = INET_FIFO;
		sc->sc_fwb.end = INET_FIFO + 16384; /* S3200 packet size */

		/* pre-allocate xfer */
		STAILQ_INIT(&sc->sc_fwb.xferlist);
		for (i = 0; i < rx_queue_len; i++) {
			xfer = fw_xfer_alloc(M_FW);
			if (xfer == NULL)
				break;
			m = m_getcl(M_WAITOK, MT_DATA, M_PKTHDR);
			xfer->recv.payload = mtod(m, uint32_t *);
			xfer->recv.pay_len = MCLBYTES;
			xfer->hand = fwip_unicast_input;
			xfer->fc = fc;
			xfer->sc = (void *) sc;
			xfer->mbuf = m;
			STAILQ_INSERT_TAIL(&sc->sc_fwb.xferlist, xfer, link);
		}
		fw_bindadd(fc, &sc->sc_fwb);

		STAILQ_INIT(&sc->sc_xferlist);
		for (i = 0; i < TX_MAX_QUEUE; i++) {
			xfer = fw_xfer_alloc(M_FW);
			if (xfer == NULL)
				break;
			xfer->send.spd = tx_speed;
			xfer->fc = sc->sc_fd.fc;
			xfer->sc = (void *)sc;
			xfer->hand = fwip_output_callback;
			STAILQ_INSERT_TAIL(&sc->sc_xferlist, xfer, link);
		}
	} else
		xferq = fc->ir[sc->sc_dma_ch];

	sc->sc_last_dest.hi = 0;
	sc->sc_last_dest.lo = 0;

	/* start dma */
	if ((xferq->flag & FWXFERQ_RUNNING) == 0)
		fc->irx_enable(fc, sc->sc_dma_ch);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

#if 0
	/* attempt to start output */
	fwip_start(ifp);
#endif
	return 0;
}

static void
fwip_stop(struct ifnet *ifp, int disable)
{
	struct fwip_softc *sc = ((struct fwip_eth_softc *)ifp->if_softc)->fwip;
	struct firewire_comm *fc = sc->sc_fd.fc;
	struct fw_xferq *xferq;
	struct fw_xfer *xfer, *next;
	int i;

	if (sc->sc_dma_ch >= 0) {
		xferq = fc->ir[sc->sc_dma_ch];

		if (xferq->flag & FWXFERQ_RUNNING)
			fc->irx_disable(fc, sc->sc_dma_ch);
		xferq->flag &=
			~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
			FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
		xferq->hand = NULL;

		for (i = 0; i < xferq->bnchunk; i++)
			m_freem(xferq->bulkxfer[i].mbuf);
		free(xferq->bulkxfer, M_FW);

		fw_bindremove(fc, &sc->sc_fwb);
		for (xfer = STAILQ_FIRST(&sc->sc_fwb.xferlist); xfer != NULL;
		    xfer = next) {
			next = STAILQ_NEXT(xfer, link);
			fw_xfer_free(xfer);
		}

		for (xfer = STAILQ_FIRST(&sc->sc_xferlist); xfer != NULL;
		    xfer = next) {
			next = STAILQ_NEXT(xfer, link);
			fw_xfer_free(xfer);
		}

		xferq->bulkxfer = NULL;
		sc->sc_dma_ch = -1;
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

static void
fwip_post_busreset(void *arg)
{
	struct fwip_softc *sc = arg;
	struct crom_src *src;
	struct crom_chunk *root;

	src = sc->sc_fd.fc->crom_src;
	root = sc->sc_fd.fc->crom_root;

	/* RFC2734 IPv4 over IEEE1394 */
	memset(&sc->sc_unit4, 0, sizeof(struct crom_chunk));
	crom_add_chunk(src, root, &sc->sc_unit4, CROM_UDIR);
	crom_add_entry(&sc->sc_unit4, CSRKEY_SPEC, CSRVAL_IETF);
	crom_add_simple_text(src, &sc->sc_unit4, &sc->sc_spec4, "IANA");
	crom_add_entry(&sc->sc_unit4, CSRKEY_VER, 1);
	crom_add_simple_text(src, &sc->sc_unit4, &sc->sc_ver4, "IPv4");

	/* RFC3146 IPv6 over IEEE1394 */
	memset(&sc->sc_unit6, 0, sizeof(struct crom_chunk));
	crom_add_chunk(src, root, &sc->sc_unit6, CROM_UDIR);
	crom_add_entry(&sc->sc_unit6, CSRKEY_SPEC, CSRVAL_IETF);
	crom_add_simple_text(src, &sc->sc_unit6, &sc->sc_spec6, "IANA");
	crom_add_entry(&sc->sc_unit6, CSRKEY_VER, 2);
	crom_add_simple_text(src, &sc->sc_unit6, &sc->sc_ver6, "IPv6");

	sc->sc_last_dest.hi = 0;
	sc->sc_last_dest.lo = 0;
	ieee1394_drain(sc->sc_eth.fwip_ifp);
}

static void
fwip_output_callback(struct fw_xfer *xfer)
{
	struct fwip_softc *sc = (struct fwip_softc *)xfer->sc;
	struct ifnet *ifp;

	ifp = sc->sc_eth.fwip_ifp;
	/* XXX error check */
	FWIPDEBUG(ifp, "resp = %d\n", xfer->resp);
	if (xfer->resp != 0)
		ifp->if_oerrors++;

	m_freem(xfer->mbuf);
	fw_xfer_unload(xfer);

	mutex_enter(&sc->sc_mtx);
	STAILQ_INSERT_TAIL(&sc->sc_xferlist, xfer, link);
	mutex_exit(&sc->sc_mtx);

	/* for queue full */
	if (ifp->if_snd.ifq_head != NULL)
		fwip_start(ifp);
}

/* Async. stream output */
static void
fwip_async_output(struct fwip_softc *sc, struct ifnet *ifp)
{
	struct firewire_comm *fc = sc->sc_fd.fc;
	struct mbuf *m;
	struct m_tag *mtag;
	struct fw_hwaddr *destfw;
	struct fw_xfer *xfer;
	struct fw_xferq *xferq;
	struct fw_pkt *fp;
	uint16_t nodeid;
	int error;
	int i = 0;

	xfer = NULL;
	xferq = fc->atq;
	while ((xferq->queued < xferq->maxq - 1) &&
	    (ifp->if_snd.ifq_head != NULL)) {
		mutex_enter(&sc->sc_mtx);
		if (STAILQ_EMPTY(&sc->sc_xferlist)) {
			mutex_exit(&sc->sc_mtx);
#if 0
			aprint_normal("if_fwip: lack of xfer\n");
#endif
			break;
		}
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			mutex_exit(&sc->sc_mtx);
			break;
		}
		xfer = STAILQ_FIRST(&sc->sc_xferlist);
		STAILQ_REMOVE_HEAD(&sc->sc_xferlist, link);
		mutex_exit(&sc->sc_mtx);

		/*
		 * Dig out the link-level address which
		 * firewire_output got via arp or neighbour
		 * discovery. If we don't have a link-level address,
		 * just stick the thing on the broadcast channel.
		 */
		mtag = m_tag_find(m, MTAG_FIREWIRE_HWADDR, 0);
		if (mtag == NULL)
			destfw = 0;
		else
			destfw = (struct fw_hwaddr *) (mtag + 1);

		/*
		 * Put the mbuf in the xfer early in case we hit an
		 * error case below - fwip_output_callback will free
		 * the mbuf.
		 */
		xfer->mbuf = m;

		/*
		 * We use the arp result (if any) to add a suitable firewire
		 * packet header before handing off to the bus.
		 */
		fp = &xfer->send.hdr;
		nodeid = FWLOCALBUS | fc->nodeid;
		if ((m->m_flags & M_BCAST) || !destfw) {
			/*
			 * Broadcast packets are sent as GASP packets with
			 * specifier ID 0x00005e, version 1 on the broadcast
			 * channel. To be conservative, we send at the
			 * slowest possible speed.
			 */
			uint32_t *p;

			M_PREPEND(m, 2 * sizeof(uint32_t), M_DONTWAIT);
			p = mtod(m, uint32_t *);
			fp->mode.stream.len = m->m_pkthdr.len;
			fp->mode.stream.chtag = broadcast_channel;
			fp->mode.stream.tcode = FWTCODE_STREAM;
			fp->mode.stream.sy = 0;
			xfer->send.spd = 0;
			p[0] = htonl(nodeid << 16);
			p[1] = htonl((0x5e << 24) | 1);
		} else {
			/*
			 * Unicast packets are sent as block writes to the
			 * target's unicast fifo address. If we can't
			 * find the node address, we just give up. We
			 * could broadcast it but that might overflow
			 * the packet size limitations due to the
			 * extra GASP header. Note: the hardware
			 * address is stored in network byte order to
			 * make life easier for ARP.
			 */
			struct fw_device *fd;
			struct fw_eui64 eui;

			eui.hi = ntohl(destfw->sender_unique_ID_hi);
			eui.lo = ntohl(destfw->sender_unique_ID_lo);
			if (sc->sc_last_dest.hi != eui.hi ||
			    sc->sc_last_dest.lo != eui.lo) {
				fd = fw_noderesolve_eui64(fc, &eui);
				if (!fd) {
					/* error */
					ifp->if_oerrors++;
					/* XXX set error code */
					fwip_output_callback(xfer);
					continue;

				}
				sc->sc_last_hdr.mode.wreqb.dst =
				    FWLOCALBUS | fd->dst;
				sc->sc_last_hdr.mode.wreqb.tlrt = 0;
				sc->sc_last_hdr.mode.wreqb.tcode =
				    FWTCODE_WREQB;
				sc->sc_last_hdr.mode.wreqb.pri = 0;
				sc->sc_last_hdr.mode.wreqb.src = nodeid;
				sc->sc_last_hdr.mode.wreqb.dest_hi =
					ntohs(destfw->sender_unicast_FIFO_hi);
				sc->sc_last_hdr.mode.wreqb.dest_lo =
					ntohl(destfw->sender_unicast_FIFO_lo);
				sc->sc_last_hdr.mode.wreqb.extcode = 0;
				sc->sc_last_dest = eui;
			}

			fp->mode.wreqb = sc->sc_last_hdr.mode.wreqb;
			fp->mode.wreqb.len = m->m_pkthdr.len;
			xfer->send.spd = min(destfw->sspd, fc->speed);
		}

		xfer->send.pay_len = m->m_pkthdr.len;

		error = fw_asyreq(fc, -1, xfer);
		if (error == EAGAIN) {
			/*
			 * We ran out of tlabels - requeue the packet
			 * for later transmission.
			 */
			xfer->mbuf = 0;
			mutex_enter(&sc->sc_mtx);
			STAILQ_INSERT_TAIL(&sc->sc_xferlist, xfer, link);
			mutex_exit(&sc->sc_mtx);
			IF_PREPEND(&ifp->if_snd, m);
			break;
		}
		if (error) {
			/* error */
			ifp->if_oerrors++;
			/* XXX set error code */
			fwip_output_callback(xfer);
			continue;
		} else {
			ifp->if_opackets++;
			i++;
		}
	}
#if 0
	if (i > 1)
		aprint_normal("%d queued\n", i);
#endif
	if (i > 0)
		xferq->start(fc);
}

/* Async. stream output */
static void
fwip_stream_input(struct fw_xferq *xferq)
{
	struct mbuf *m, *m0;
	struct m_tag *mtag;
	struct ifnet *ifp;
	struct fwip_softc *sc;
	struct fw_bulkxfer *sxfer;
	struct fw_pkt *fp;
	uint16_t src;
	uint32_t *p;

	sc = (struct fwip_softc *)xferq->sc;
	ifp = sc->sc_eth.fwip_ifp;
	while ((sxfer = STAILQ_FIRST(&xferq->stvalid)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->stvalid, link);
		fp = mtod(sxfer->mbuf, struct fw_pkt *);
		if (sc->sc_fd.fc->irx_post != NULL)
			sc->sc_fd.fc->irx_post(sc->sc_fd.fc, fp->mode.ld);
		m = sxfer->mbuf;

		/* insert new rbuf */
		sxfer->mbuf = m0 = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m0 != NULL) {
			m0->m_len = m0->m_pkthdr.len = m0->m_ext.ext_size;
			STAILQ_INSERT_TAIL(&xferq->stfree, sxfer, link);
		} else
			aprint_error_ifnet(ifp,
			    "fwip_as_input: m_getcl failed\n");

		/*
		 * We must have a GASP header - leave the
		 * encapsulation sanity checks to the generic
		 * code. Remeber that we also have the firewire async
		 * stream header even though that isn't accounted for
		 * in mode.stream.len.
		 */
		if (sxfer->resp != 0 ||
		    fp->mode.stream.len < 2 * sizeof(uint32_t)) {
			m_freem(m);
			ifp->if_ierrors++;
			continue;
		}
		m->m_len = m->m_pkthdr.len = fp->mode.stream.len
			+ sizeof(fp->mode.stream);

		/*
		 * If we received the packet on the broadcast channel,
		 * mark it as broadcast, otherwise we assume it must
		 * be multicast.
		 */
		if (fp->mode.stream.chtag == broadcast_channel)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;

		/*
		 * Make sure we recognise the GASP specifier and
		 * version.
		 */
		p = mtod(m, uint32_t *);
		if ((((ntohl(p[1]) & 0xffff) << 8) | ntohl(p[2]) >> 24) !=
								0x00005e ||
		    (ntohl(p[2]) & 0xffffff) != 1) {
			FWIPDEBUG(ifp, "Unrecognised GASP header %#08x %#08x\n",
			    ntohl(p[1]), ntohl(p[2]));
			m_freem(m);
			ifp->if_ierrors++;
			continue;
		}

		/*
		 * Record the sender ID for possible BPF usage.
		 */
		src = ntohl(p[1]) >> 16;
		if (ifp->if_bpf) {
			mtag = m_tag_get(MTAG_FIREWIRE_SENDER_EUID,
			    2 * sizeof(uint32_t), M_NOWAIT);
			if (mtag) {
				/* bpf wants it in network byte order */
				struct fw_device *fd;
				uint32_t *p2 = (uint32_t *) (mtag + 1);

				fd = fw_noderesolve_nodeid(sc->sc_fd.fc,
				    src & 0x3f);
				if (fd) {
					p2[0] = htonl(fd->eui.hi);
					p2[1] = htonl(fd->eui.lo);
				} else {
					p2[0] = 0;
					p2[1] = 0;
				}
				m_tag_prepend(m, mtag);
			}
		}

		/*
		 * Trim off the GASP header
		 */
		m_adj(m, 3*sizeof(uint32_t));
		m->m_pkthdr.rcvif = ifp;
		ieee1394_input(ifp, m, src);
		ifp->if_ipackets++;
	}
	if (STAILQ_FIRST(&xferq->stfree) != NULL)
		sc->sc_fd.fc->irx_enable(sc->sc_fd.fc, sc->sc_dma_ch);
}

static inline void
fwip_unicast_input_recycle(struct fwip_softc *sc, struct fw_xfer *xfer)
{
	struct mbuf *m;

	/*
	 * We have finished with a unicast xfer. Allocate a new
	 * cluster and stick it on the back of the input queue.
	 */
	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		aprint_error_dev(sc->sc_fd.dev,
		    "fwip_unicast_input_recycle: m_getcl failed\n");
	xfer->recv.payload = mtod(m, uint32_t *);
	xfer->recv.pay_len = MCLBYTES;
	xfer->mbuf = m;
	mutex_enter(&sc->sc_fwb.fwb_mtx);
	STAILQ_INSERT_TAIL(&sc->sc_fwb.xferlist, xfer, link);
	mutex_exit(&sc->sc_fwb.fwb_mtx);
}

static void
fwip_unicast_input(struct fw_xfer *xfer)
{
	uint64_t address;
	struct mbuf *m;
	struct m_tag *mtag;
	struct ifnet *ifp;
	struct fwip_softc *sc;
	struct fw_pkt *fp;
	int rtcode;

	sc = (struct fwip_softc *)xfer->sc;
	ifp = sc->sc_eth.fwip_ifp;
	m = xfer->mbuf;
	xfer->mbuf = 0;
	fp = &xfer->recv.hdr;

	/*
	 * Check the fifo address - we only accept addresses of
	 * exactly INET_FIFO.
	 */
	address = ((uint64_t)fp->mode.wreqb.dest_hi << 32)
		| fp->mode.wreqb.dest_lo;
	if (fp->mode.wreqb.tcode != FWTCODE_WREQB) {
		rtcode = FWRCODE_ER_TYPE;
	} else if (address != INET_FIFO) {
		rtcode = FWRCODE_ER_ADDR;
	} else {
		rtcode = FWRCODE_COMPLETE;
	}

	/*
	 * Pick up a new mbuf and stick it on the back of the receive
	 * queue.
	 */
	fwip_unicast_input_recycle(sc, xfer);

	/*
	 * If we've already rejected the packet, give up now.
	 */
	if (rtcode != FWRCODE_COMPLETE) {
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}

	if (ifp->if_bpf) {
		/*
		 * Record the sender ID for possible BPF usage.
		 */
		mtag = m_tag_get(MTAG_FIREWIRE_SENDER_EUID,
		    2 * sizeof(uint32_t), M_NOWAIT);
		if (mtag) {
			/* bpf wants it in network byte order */
			struct fw_device *fd;
			uint32_t *p = (uint32_t *) (mtag + 1);

			fd = fw_noderesolve_nodeid(sc->sc_fd.fc,
			    fp->mode.wreqb.src & 0x3f);
			if (fd) {
				p[0] = htonl(fd->eui.hi);
				p[1] = htonl(fd->eui.lo);
			} else {
				p[0] = 0;
				p[1] = 0;
			}
			m_tag_prepend(m, mtag);
		}
	}

	/*
	 * Hand off to the generic encapsulation code. We don't use
	 * ifp->if_input so that we can pass the source nodeid as an
	 * argument to facilitate link-level fragment reassembly.
	 */
	m->m_len = m->m_pkthdr.len = fp->mode.wreqb.len;
	m->m_pkthdr.rcvif = ifp;
	ieee1394_input(ifp, m, fp->mode.wreqb.src);
	ifp->if_ipackets++;
}
