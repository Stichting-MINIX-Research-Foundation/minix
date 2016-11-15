/*	$NetBSD: if_se.c,v 1.88 2015/08/24 23:13:15 pooka Exp $	*/

/*
 * Copyright (c) 1997 Ian W. Dall <ian.dall@dsto.defence.gov.au>
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
 *	This product includes software developed by Ian W. Dall.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Driver for Cabletron EA41x scsi ethernet adaptor.
 *
 * Written by Ian Dall <ian.dall@dsto.defence.gov.au> Feb 3, 1997
 *
 * Acknowledgement: Thanks are due to Philip L. Budne <budd@cs.bu.edu>
 * who reverse engineered the EA41x. In developing this code,
 * Phil's userland daemon "etherd", was refered to extensively in lieu
 * of accurate documentation for the device.
 *
 * This is a weird device! It doesn't conform to the scsi spec in much
 * at all. About the only standard command supported is inquiry. Most
 * commands are 6 bytes long, but the recv data is only 1 byte.  Data
 * must be received by periodically polling the device with the recv
 * command.
 *
 * This driver is also a bit unusual. It must look like a network
 * interface and it must also appear to be a scsi device to the scsi
 * system. Hence there are cases where there are two entry points. eg
 * sestart is to be called from the scsi subsytem and se_ifstart from
 * the network interface subsystem.  In addition, to facilitate scsi
 * commands issued by userland programs, there are open, close and
 * ioctl entry points. This allows a user program to, for example,
 * display the ea41x stats and download new code into the adaptor ---
 * functions which can't be performed through the ifconfig interface.
 * Normal operation does not require any special userland program.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_se.c,v 1.88 2015/08/24 23:13:15 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_atalk.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_ctron_ether.h>
#include <dev/scsipi/scsiconf.h>

#include <sys/mbuf.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#ifdef NETATALK
#include <netatalk/at.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#define SETIMEOUT	1000
#define	SEOUTSTANDING	4
#define	SERETRIES	4
#define SE_PREFIX	4
#define ETHER_CRC	4
#define SEMINSIZE	60

/* Make this big enough for an ETHERMTU packet in promiscuous mode. */
#define MAX_SNAP	(ETHERMTU + sizeof(struct ether_header) + \
			 SE_PREFIX + ETHER_CRC)

/* 10 full length packets appears to be the max ever returned. 16k is OK */
#define RBUF_LEN	(16 * 1024)

/* Tuning parameters:
 * The EA41x only returns a maximum of 10 packets (regardless of size).
 * We will attempt to adapt to polling fast enough to get RDATA_GOAL packets
 * per read
 */
#define RDATA_MAX 10
#define RDATA_GOAL 8

/* se_poll and se_poll0 are the normal polling rate and the minimum
 * polling rate respectively. se_poll0 should be chosen so that at
 * maximum ethernet speed, we will read nearly RDATA_MAX packets. se_poll
 * should be chosen for reasonable maximum latency.
 * In practice, if we are being saturated with min length packets, we
 * can't poll fast enough. Polling with zero delay actually
 * worsens performance. se_poll0 is enforced to be always at least 1
 */
#define SE_POLL 40		/* default in milliseconds */
#define SE_POLL0 10		/* default in milliseconds */
int se_poll = 0;		/* Delay in ticks set at attach time */
int se_poll0 = 0;
int se_max_received = 0;	/* Instrumentation */

#define	PROTOCMD(p, d) \
	((d) = (p))

#define	PROTOCMD_DECL(name) \
	static const struct scsi_ctron_ether_generic name

#define	PROTOCMD_DECL_SPECIAL(name) \
	static const struct __CONCAT(scsi_,name) name

/* Command initializers for commands using scsi_ctron_ether_generic */
PROTOCMD_DECL(ctron_ether_send)  = {CTRON_ETHER_SEND, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_add_proto) = {CTRON_ETHER_ADD_PROTO, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_get_addr) = {CTRON_ETHER_GET_ADDR, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_set_media) = {CTRON_ETHER_SET_MEDIA, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_set_addr) = {CTRON_ETHER_SET_ADDR, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_set_multi) = {CTRON_ETHER_SET_MULTI, 0, {0,0}, 0};
PROTOCMD_DECL(ctron_ether_remove_multi) =
    {CTRON_ETHER_REMOVE_MULTI, 0, {0,0}, 0};

/* Command initializers for commands using their own structures */
PROTOCMD_DECL_SPECIAL(ctron_ether_recv) = {CTRON_ETHER_RECV};
PROTOCMD_DECL_SPECIAL(ctron_ether_set_mode) =
    {CTRON_ETHER_SET_MODE, 0, {0,0}, 0};

struct se_softc {
	device_t sc_dev;
	struct ethercom sc_ethercom;	/* Ethernet common part */
	struct scsipi_periph *sc_periph;/* contains our targ, lun, etc. */

	struct callout sc_ifstart_ch;
	struct callout sc_recv_ch;

	char *sc_tbuf;
	char *sc_rbuf;
	int protos;
#define PROTO_IP	0x01
#define PROTO_ARP	0x02
#define PROTO_REVARP	0x04
#define PROTO_AT	0x08
#define PROTO_AARP	0x10
	int sc_debug;
	int sc_flags;
#define SE_NEED_RECV 0x1
	int sc_last_timeout;
	int sc_enabled;
};

static int	sematch(device_t, cfdata_t, void *);
static void	seattach(device_t, device_t, void *);

static void	se_ifstart(struct ifnet *);
static void	sestart(struct scsipi_periph *);

static void	sedone(struct scsipi_xfer *, int);
static int	se_ioctl(struct ifnet *, u_long, void *);
static void	sewatchdog(struct ifnet *);

static inline u_int16_t ether_cmp(void *, void *);
static void	se_recv(void *);
static struct mbuf *se_get(struct se_softc *, char *, int);
static int	se_read(struct se_softc *, char *, int);
static int	se_reset(struct se_softc *);
static int	se_add_proto(struct se_softc *, int);
static int	se_get_addr(struct se_softc *, u_int8_t *);
static int	se_set_media(struct se_softc *, int);
static int	se_init(struct se_softc *);
static int	se_set_multi(struct se_softc *, u_int8_t *);
static int	se_remove_multi(struct se_softc *, u_int8_t *);
#if 0
static int	sc_set_all_multi(struct se_softc *, int);
#endif
static void	se_stop(struct se_softc *);
static inline int se_scsipi_cmd(struct scsipi_periph *periph,
			struct scsipi_generic *scsipi_cmd,
			int cmdlen, u_char *data_addr, int datalen,
			int retries, int timeout, struct buf *bp,
			int flags);
static void	se_delayed_ifstart(void *);
static int	se_set_mode(struct se_softc *, int, int);

int	se_enable(struct se_softc *);
void	se_disable(struct se_softc *);

CFATTACH_DECL_NEW(se, sizeof(struct se_softc),
    sematch, seattach, NULL, NULL);

extern struct cfdriver se_cd;

dev_type_open(seopen);
dev_type_close(seclose);
dev_type_ioctl(seioctl);

const struct cdevsw se_cdevsw = {
	.d_open = seopen,
	.d_close = seclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = seioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

const struct scsipi_periphsw se_switch = {
	NULL,			/* Use default error handler */
	sestart,		/* have a queue, served by this */
	NULL,			/* have no async handler */
	sedone,			/* deal with stats at interrupt time */
};

const struct scsipi_inquiry_pattern se_patterns[] = {
	{T_PROCESSOR, T_FIXED,
	 "CABLETRN",         "EA412",                 ""},
	{T_PROCESSOR, T_FIXED,
	 "Cabletrn",         "EA412",                 ""},
};

/*
 * Compare two Ether/802 addresses for equality, inlined and
 * unrolled for speed.
 * Note: use this like memcmp()
 */
static inline u_int16_t
ether_cmp(void *one, void *two)
{
	u_int16_t *a = (u_int16_t *) one;
	u_int16_t *b = (u_int16_t *) two;
	u_int16_t diff;

	diff = (a[0] - b[0]) | (a[1] - b[1]) | (a[2] - b[2]);

	return (diff);
}

#define ETHER_CMP	ether_cmp

static int
sematch(device_t parent, cfdata_t match, void *aux)
{
	struct scsipibus_attach_args *sa = aux;
	int priority;

	(void)scsipi_inqmatch(&sa->sa_inqbuf,
	    se_patterns, sizeof(se_patterns) / sizeof(se_patterns[0]),
	    sizeof(se_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
static void
seattach(device_t parent, device_t self, void *aux)
{
	struct se_softc *sc = device_private(self);
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_periph *periph = sa->sa_periph;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int8_t myaddr[ETHER_ADDR_LEN];

	sc->sc_dev = self;

	printf("\n");
	SC_DEBUG(periph, SCSIPI_DB2, ("seattach: "));

	callout_init(&sc->sc_ifstart_ch, 0);
	callout_init(&sc->sc_recv_ch, 0);


	/*
	 * Store information needed to contact our base driver
	 */
	sc->sc_periph = periph;
	periph->periph_dev = sc->sc_dev;
	periph->periph_switch = &se_switch;

	/* XXX increase openings? */

	se_poll = (SE_POLL * hz) / 1000;
	se_poll = se_poll? se_poll: 1;
	se_poll0 = (SE_POLL0 * hz) / 1000;
	se_poll0 = se_poll0? se_poll0: 1;

	/*
	 * Initialize and attach a buffer
	 */
	sc->sc_tbuf = malloc(ETHERMTU + sizeof(struct ether_header),
			     M_DEVBUF, M_NOWAIT);
	if (sc->sc_tbuf == 0)
		panic("seattach: can't allocate transmit buffer");

	sc->sc_rbuf = malloc(RBUF_LEN, M_DEVBUF, M_NOWAIT);/* A Guess */
	if (sc->sc_rbuf == 0)
		panic("seattach: can't allocate receive buffer");

	se_get_addr(sc, myaddr);

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), sizeof(ifp->if_xname));
	ifp->if_softc = sc;
	ifp->if_start = se_ifstart;
	ifp->if_ioctl = se_ioctl;
	ifp->if_watchdog = sewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, myaddr);
}


static inline int
se_scsipi_cmd(struct scsipi_periph *periph, struct scsipi_generic *cmd,
    int cmdlen, u_char *data_addr, int datalen, int retries, int timeout,
    struct buf *bp, int flags)
{
	int error;
	int s = splbio();

	error = scsipi_command(periph, cmd, cmdlen, data_addr,
	    datalen, retries, timeout, bp, flags);
	splx(s);
	return (error);
}

/* Start routine for calling from scsi sub system */
static void
sestart(struct scsipi_periph *periph)
{
	struct se_softc *sc = device_private(periph->periph_dev);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int s = splnet();

	se_ifstart(ifp);
	(void) splx(s);
}

static void
se_delayed_ifstart(void *v)
{
	struct ifnet *ifp = v;
	struct se_softc *sc = ifp->if_softc;
	int s;

	s = splnet();
	if (sc->sc_enabled) {
		ifp->if_flags &= ~IFF_OACTIVE;
		se_ifstart(ifp);
	}
	splx(s);
}

/*
 * Start transmission on the interface.
 * Always called at splnet().
 */
static void
se_ifstart(struct ifnet *ifp)
{
	struct se_softc *sc = ifp->if_softc;
	struct scsi_ctron_ether_generic send_cmd;
	struct mbuf *m, *m0;
	int len, error;
	u_char *cp;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)
		return;
	/* If BPF is listening on this interface, let it see the
	 * packet before we commit it to the wire.
	 */
	bpf_mtap(ifp, m0);

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("ctscstart: no header mbuf");
	len = m0->m_pkthdr.len;

	/* Mark the interface busy. */
	ifp->if_flags |= IFF_OACTIVE;

	/* Chain; copy into linear buffer we allocated at attach time. */
	cp = sc->sc_tbuf;
	for (m = m0; m != NULL; ) {
		memcpy(cp, mtod(m, u_char *), m->m_len);
		cp += m->m_len;
		MFREE(m, m0);
		m = m0;
	}
	if (len < SEMINSIZE) {
#ifdef SEDEBUG
		if (sc->sc_debug)
			printf("se: packet size %d (%zu) < %d\n", len,
			    cp - (u_char *)sc->sc_tbuf, SEMINSIZE);
#endif
		memset(cp, 0, SEMINSIZE - len);
		len = SEMINSIZE;
	}

	/* Fill out SCSI command. */
	PROTOCMD(ctron_ether_send, send_cmd);
	_lto2b(len, send_cmd.length);

	/* Send command to device. */
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&send_cmd, sizeof(send_cmd),
	    sc->sc_tbuf, len, SERETRIES,
	    SETIMEOUT, NULL, XS_CTL_NOSLEEP|XS_CTL_ASYNC|XS_CTL_DATA_OUT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "not queued, error %d\n", error);
		ifp->if_oerrors++;
		ifp->if_flags &= ~IFF_OACTIVE;
	} else
		ifp->if_opackets++;
	if (sc->sc_flags & SE_NEED_RECV) {
		sc->sc_flags &= ~SE_NEED_RECV;
		se_recv((void *) sc);
	}
}


/*
 * Called from the scsibus layer via our scsi device switch.
 */
static void
sedone(struct scsipi_xfer *xs, int error)
{
	struct se_softc *sc = device_private(xs->xs_periph->periph_dev);
	struct scsipi_generic *cmd = xs->cmd;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int s;

	s = splnet();
	if(IS_SEND(cmd)) {
		if (xs->error == XS_BUSY) {
			printf("se: busy, retry txmit\n");
			callout_reset(&sc->sc_ifstart_ch, hz,
			    se_delayed_ifstart, ifp);
		} else {
			ifp->if_flags &= ~IFF_OACTIVE;
			/* the generic scsipi_done will call
			 * sestart (through scsipi_free_xs).
			 */
		}
	} else if(IS_RECV(cmd)) {
		/* RECV complete */
		/* pass data up. reschedule a recv */
		/* scsipi_free_xs will call start. Harmless. */
		if (error) {
			/* Reschedule after a delay */
			callout_reset(&sc->sc_recv_ch, se_poll,
			    se_recv, (void *)sc);
		} else {
			int n, ntimeo;
			n = se_read(sc, xs->data, xs->datalen - xs->resid);
			if (n > se_max_received)
				se_max_received = n;
			if (n == 0)
				ntimeo = se_poll;
			else if (n >= RDATA_MAX)
				ntimeo = se_poll0;
			else {
				ntimeo = sc->sc_last_timeout;
				ntimeo = (ntimeo * RDATA_GOAL)/n;
				ntimeo = (ntimeo < se_poll0?
					  se_poll0: ntimeo);
				ntimeo = (ntimeo > se_poll?
					  se_poll: ntimeo);
			}
			sc->sc_last_timeout = ntimeo;
			if (ntimeo == se_poll0  &&
			    IFQ_IS_EMPTY(&ifp->if_snd) == 0)
				/* Output is pending. Do next recv
				 * after the next send.  */
				sc->sc_flags |= SE_NEED_RECV;
			else {
				callout_reset(&sc->sc_recv_ch, ntimeo,
				    se_recv, (void *)sc);
  			}
		}
	}
	splx(s);
}

static void
se_recv(void *v)
{
	/* do a recv command */
	struct se_softc *sc = (struct se_softc *) v;
	struct scsi_ctron_ether_recv recv_cmd;
	int error;

	if (sc->sc_enabled == 0)
		return;

	PROTOCMD(ctron_ether_recv, recv_cmd);

	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&recv_cmd, sizeof(recv_cmd),
	    sc->sc_rbuf, RBUF_LEN, SERETRIES, SETIMEOUT, NULL,
	    XS_CTL_NOSLEEP|XS_CTL_ASYNC|XS_CTL_DATA_IN);
	if (error)
		callout_reset(&sc->sc_recv_ch, se_poll, se_recv, (void *)sc);
}

/*
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
static struct mbuf *
se_get(struct se_softc *sc, char *data, int totlen)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m, *m0, *newm;
	int len;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == 0)
		return (0);
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	len = MHLEN;
	m = m0;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0)
				goto bad;
			len = MCLBYTES;
		}

		if (m == m0) {
			char *newdata = (char *)
			    ALIGN(m->m_data + sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}

		m->m_len = len = min(totlen, len);
		memcpy(mtod(m, void *), data, len);
		data += len;

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == 0)
				goto bad;
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return (m0);

bad:
	m_freem(m0);
	return (0);
}

/*
 * Pass packets to higher levels.
 */
static int
se_read(struct se_softc *sc, char *data, int datalen)
{
	struct mbuf *m;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int n;

	n = 0;
	while (datalen >= 2) {
		int len = _2btol(data);
		data += 2;
		datalen -= 2;

		if (len == 0)
			break;
#ifdef SEDEBUG
		if (sc->sc_debug) {
			printf("se_read: datalen = %d, packetlen = %d, proto = 0x%04x\n", datalen, len,
			 ntohs(((struct ether_header *)data)->ether_type));
		}
#endif
		if (len <= sizeof(struct ether_header) ||
		    len > MAX_SNAP) {
#ifdef SEDEBUG
			printf("%s: invalid packet size %d; dropping\n",
			       device_xname(sc->sc_dev), len);
#endif
			ifp->if_ierrors++;
			goto next_packet;
		}

		/* Don't need crc. Must keep ether header for BPF */
		m = se_get(sc, data, len - ETHER_CRC);
		if (m == 0) {
#ifdef SEDEBUG
			if (sc->sc_debug)
				printf("se_read: se_get returned null\n");
#endif
			ifp->if_ierrors++;
			goto next_packet;
		}
		if ((ifp->if_flags & IFF_PROMISC) != 0) {
			m_adj(m, SE_PREFIX);
		}
		ifp->if_ipackets++;

		/*
		 * Check if there's a BPF listener on this interface.
		 * If so, hand off the raw packet to BPF.
		 */
		bpf_mtap(ifp, m);

		/* Pass the packet up. */
		(*ifp->if_input)(ifp, m);

	next_packet:
		data += len;
		datalen -= len;
		n++;
	}
	return (n);
}


static void
sewatchdog(struct ifnet *ifp)
{
	struct se_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;

	se_reset(sc);
}

static int
se_reset(struct se_softc *sc)
{
	int error;
	int s = splnet();
#if 0
	/* Maybe we don't *really* want to reset the entire bus
	 * because the ctron isn't working. We would like to send a
	 * "BUS DEVICE RESET" message, but don't think the ctron
	 * understands it.
	 */
	error = se_scsipi_cmd(sc->sc_periph, 0, 0, 0, 0, SERETRIES, 2000, NULL,
	    XS_CTL_RESET);
#endif
	error = se_init(sc);
	splx(s);
	return (error);
}

static int
se_add_proto(struct se_softc *sc, int proto)
{
	int error;
	struct scsi_ctron_ether_generic add_proto_cmd;
	u_int8_t data[2];
	_lto2b(proto, data);
#ifdef SEDEBUG
	if (sc->sc_debug)
		printf("se: adding proto 0x%02x%02x\n", data[0], data[1]);
#endif

	PROTOCMD(ctron_ether_add_proto, add_proto_cmd);
	_lto2b(sizeof(data), add_proto_cmd.length);
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&add_proto_cmd, sizeof(add_proto_cmd),
	    data, sizeof(data), SERETRIES, SETIMEOUT, NULL,
	    XS_CTL_DATA_OUT);
	return (error);
}

static int
se_get_addr(struct se_softc *sc, u_int8_t *myaddr)
{
	int error;
	struct scsi_ctron_ether_generic get_addr_cmd;

	PROTOCMD(ctron_ether_get_addr, get_addr_cmd);
	_lto2b(ETHER_ADDR_LEN, get_addr_cmd.length);
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&get_addr_cmd, sizeof(get_addr_cmd),
	    myaddr, ETHER_ADDR_LEN, SERETRIES, SETIMEOUT, NULL,
	    XS_CTL_DATA_IN);
	printf("%s: ethernet address %s\n", device_xname(sc->sc_dev),
	    ether_sprintf(myaddr));
	return (error);
}


static int
se_set_media(struct se_softc *sc, int type)
{
	int error;
	struct scsi_ctron_ether_generic set_media_cmd;

	PROTOCMD(ctron_ether_set_media, set_media_cmd);
	set_media_cmd.byte3 = type;
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&set_media_cmd, sizeof(set_media_cmd),
	    0, 0, SERETRIES, SETIMEOUT, NULL, 0);
	return (error);
}

static int
se_set_mode(struct se_softc *sc, int len, int mode)
{
	int error;
	struct scsi_ctron_ether_set_mode set_mode_cmd;

	PROTOCMD(ctron_ether_set_mode, set_mode_cmd);
	set_mode_cmd.mode = mode;
	_lto2b(len, set_mode_cmd.length);
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&set_mode_cmd, sizeof(set_mode_cmd),
	    0, 0, SERETRIES, SETIMEOUT, NULL, 0);
	return (error);
}


static int
se_init(struct se_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct scsi_ctron_ether_generic set_addr_cmd;
	uint8_t enaddr[ETHER_ADDR_LEN];
	int error;

	if (ifp->if_flags & IFF_PROMISC) {
		error = se_set_mode(sc, MAX_SNAP, 1);
	}
	else
		error = se_set_mode(sc, ETHERMTU + sizeof(struct ether_header),
		    0);
	if (error != 0)
		return (error);

	PROTOCMD(ctron_ether_set_addr, set_addr_cmd);
	_lto2b(ETHER_ADDR_LEN, set_addr_cmd.length);
	memcpy(enaddr, CLLADDR(ifp->if_sadl), sizeof(enaddr));
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&set_addr_cmd, sizeof(set_addr_cmd),
	    enaddr, ETHER_ADDR_LEN, SERETRIES, SETIMEOUT, NULL,
	    XS_CTL_DATA_OUT);
	if (error != 0)
		return (error);

	if ((sc->protos & PROTO_IP) &&
	    (error = se_add_proto(sc, ETHERTYPE_IP)) != 0)
		return (error);
	if ((sc->protos & PROTO_ARP) &&
	    (error = se_add_proto(sc, ETHERTYPE_ARP)) != 0)
		return (error);
	if ((sc->protos & PROTO_REVARP) &&
	    (error = se_add_proto(sc, ETHERTYPE_REVARP)) != 0)
		return (error);
#ifdef NETATALK
	if ((sc->protos & PROTO_AT) &&
	    (error = se_add_proto(sc, ETHERTYPE_ATALK)) != 0)
		return (error);
	if ((sc->protos & PROTO_AARP) &&
	    (error = se_add_proto(sc, ETHERTYPE_AARP)) != 0)
		return (error);
#endif

	if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) == IFF_UP) {
		ifp->if_flags |= IFF_RUNNING;
		se_recv(sc);
		ifp->if_flags &= ~IFF_OACTIVE;
		se_ifstart(ifp);
	}
	return (error);
}

static int
se_set_multi(struct se_softc *sc, u_int8_t *addr)
{
	struct scsi_ctron_ether_generic set_multi_cmd;
	int error;

	if (sc->sc_debug)
		printf("%s: set_set_multi: %s\n", device_xname(sc->sc_dev),
		    ether_sprintf(addr));

	PROTOCMD(ctron_ether_set_multi, set_multi_cmd);
	_lto2b(sizeof(addr), set_multi_cmd.length);
	/* XXX sizeof(addr) is the size of the pointer.  Surely it
	 * is too small? --dyoung
	 */
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&set_multi_cmd, sizeof(set_multi_cmd),
	    addr, sizeof(addr), SERETRIES, SETIMEOUT, NULL, XS_CTL_DATA_OUT);
	return (error);
}

static int
se_remove_multi(struct se_softc *sc, u_int8_t *addr)
{
	struct scsi_ctron_ether_generic remove_multi_cmd;
	int error;

	if (sc->sc_debug)
		printf("%s: se_remove_multi: %s\n", device_xname(sc->sc_dev),
		    ether_sprintf(addr));

	PROTOCMD(ctron_ether_remove_multi, remove_multi_cmd);
	_lto2b(sizeof(addr), remove_multi_cmd.length);
	/* XXX sizeof(addr) is the size of the pointer.  Surely it
	 * is too small? --dyoung
	 */
	error = se_scsipi_cmd(sc->sc_periph,
	    (void *)&remove_multi_cmd, sizeof(remove_multi_cmd),
	    addr, sizeof(addr), SERETRIES, SETIMEOUT, NULL, XS_CTL_DATA_OUT);
	return (error);
}

#if 0	/* not used  --thorpej */
static int
sc_set_all_multi(struct se_softc *sc, int set)
{
	int error = 0;
	u_int8_t *addr;
	struct ethercom *ac = &sc->sc_ethercom;
	struct ether_multi *enm;
	struct ether_multistep step;

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (ETHER_CMP(enm->enm_addrlo, enm->enm_addrhi)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			/* We have no way of adding a range to this device.
			 * stepping through all addresses in the range is
			 * typically not possible. The only real alternative
			 * is to go into promicuous mode and filter by hand.
			 */
			return (ENODEV);

		}

		addr = enm->enm_addrlo;
		if ((error = set ? se_set_multi(sc, addr) :
		    se_remove_multi(sc, addr)) != 0)
			return (error);
		ETHER_NEXT_MULTI(step, enm);
	}
	return (error);
}
#endif /* not used */

static void
se_stop(struct se_softc *sc)
{

	/* Don't schedule any reads */
	callout_stop(&sc->sc_recv_ch);

	/* How can we abort any scsi cmds in progress? */
}


/*
 * Process an ioctl request.
 */
static int
se_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct se_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr *sa;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		if ((error = se_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;

		if ((error = se_set_media(sc, CMEDIA_AUTOSENSE)) != 0)
			break;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			sc->protos |= (PROTO_IP | PROTO_ARP | PROTO_REVARP);
			if ((error = se_init(sc)) != 0)
				break;
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NETATALK
		case AF_APPLETALK:
			sc->protos |= (PROTO_AT | PROTO_AARP);
			if ((error = se_init(sc)) != 0)
				break;
			break;
#endif
		default:
			error = se_init(sc);
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
			se_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			se_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = se_enable(sc)) != 0)
				break;
			error = se_init(sc);
			break;
		default:
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			if (sc->sc_enabled)
				error = se_init(sc);
			break;
		}
#ifdef SEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = 1;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sa = sockaddr_dup(ifreq_getaddr(cmd, ifr), M_NOWAIT);
		if (sa == NULL) {
			error = ENOBUFS;
			break;
		}
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				error = (cmd == SIOCADDMULTI) ?
				   se_set_multi(sc, sa->sa_data) :
				   se_remove_multi(sc, sa->sa_data);
			} else
				error = 0;
		}
		sockaddr_free(sa);
		break;

	default:

		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return (error);
}

/*
 * Enable the network interface.
 */
int
se_enable(struct se_softc *sc)
{
	struct scsipi_periph *periph = sc->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;
	int error = 0;

	if (sc->sc_enabled == 0 &&
	    (error = scsipi_adapter_addref(adapt)) == 0)
		sc->sc_enabled = 1;
	else
		aprint_error_dev(sc->sc_dev, "device enable failed\n");

	return (error);
}

/*
 * Disable the network interface.
 */
void
se_disable(struct se_softc *sc)
{
	struct scsipi_periph *periph = sc->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;

	if (sc->sc_enabled != 0) {
		scsipi_adapter_delref(adapt);
		sc->sc_enabled = 0;
	}
}

#define	SEUNIT(z)	(minor(z))
/*
 * open the device.
 */
int
seopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int unit, error;
	struct se_softc *sc;
	struct scsipi_periph *periph;
	struct scsipi_adapter *adapt;

	unit = SEUNIT(dev);
	sc = device_lookup_private(&se_cd, unit);
	if (sc == NULL)
		return (ENXIO);

	periph = sc->sc_periph;
	adapt = periph->periph_channel->chan_adapter;

	if ((error = scsipi_adapter_addref(adapt)) != 0)
		return (error);

	SC_DEBUG(periph, SCSIPI_DB1,
	    ("scopen: dev=0x%"PRIx64" (unit %d (of %d))\n", dev, unit,
	    se_cd.cd_ndevs));

	periph->periph_flags |= PERIPH_OPEN;

	SC_DEBUG(periph, SCSIPI_DB3, ("open complete\n"));
	return (0);
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int
seclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct se_softc *sc = device_lookup_private(&se_cd, SEUNIT(dev));
	struct scsipi_periph *periph = sc->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;

	SC_DEBUG(sc->sc_periph, SCSIPI_DB1, ("closing\n"));

	scsipi_wait_drain(periph);

	scsipi_adapter_delref(adapt);
	periph->periph_flags &= ~PERIPH_OPEN;

	return (0);
}

/*
 * Perform special action on behalf of the user
 * Only does generic scsi ioctls.
 */
int
seioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct se_softc *sc = device_lookup_private(&se_cd, SEUNIT(dev));

	return (scsipi_do_ioctl(sc->sc_periph, dev, cmd, addr, flag, l));
}
