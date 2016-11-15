/*	$NetBSD: bcsp.c,v 1.29 2015/08/20 14:40:17 christos Exp $	*/
/*
 * Copyright (c) 2007 KIYOHARA Takashi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bcsp.c,v 1.29 2015/08/20 14:40:17 christos Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

#include <dev/bluetooth/bcsp.h>

#include "ioconf.h"

#ifdef BCSP_DEBUG
#ifdef DPRINTF
#undef DPRINTF
#endif
#ifdef DPRINTFN
#undef DPRINTFN
#endif

#define DPRINTF(x)	printf x
#define DPRINTFN(n, x)	do { if (bcsp_debug > (n)) printf x; } while (0)
int bcsp_debug = 3;
#else
#undef DPRINTF
#undef DPRINTFN

#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct bcsp_softc {
	device_t sc_dev;

	struct tty *sc_tp;
	struct hci_unit *sc_unit;		/* Bluetooth HCI Unit */
	struct bt_stats sc_stats;

	int sc_flags;

	/* output queues */
	MBUFQ_HEAD()	sc_cmdq;
	MBUFQ_HEAD()	sc_aclq;
	MBUFQ_HEAD()	sc_scoq;

	int sc_baud;
	int sc_init_baud;

	/* variables of SLIP Layer */
	struct mbuf *sc_txp;			/* outgoing packet */
	struct mbuf *sc_rxp;			/* incoming packet */
	int sc_slip_txrsv;			/* reserved byte data */
	int sc_slip_rxexp;			/* expected byte data */
	void (*sc_transmit_callback)(struct bcsp_softc *, struct mbuf *);

	/* variables of Packet Integrity Layer */
	int sc_pi_txcrc;			/* use CRC, if true */

	/* variables of MUX Layer */
	bool sc_mux_send_ack;			/* flag for send_ack */
	bool sc_mux_choke;			/* Choke signal */
	struct timeval sc_mux_lastrx;		/* Last Rx Pkt Time */

	/* variables of Sequencing Layer */
	MBUFQ_HEAD() sc_seqq;			/* Sequencing Layer queue */
	MBUFQ_HEAD() sc_seq_retryq;		/* retry queue */
	uint32_t sc_seq_txseq;
	uint32_t sc_seq_txack;
	uint32_t sc_seq_expected_rxseq;
	uint32_t sc_seq_winspace;
	uint32_t sc_seq_retries;
	callout_t sc_seq_timer;
	uint32_t sc_seq_timeout;
	uint32_t sc_seq_winsize;
	uint32_t sc_seq_retry_limit;

	/* variables of Datagram Queue Layer */
	MBUFQ_HEAD() sc_dgq;			/* Datagram Queue Layer queue */

	/* variables of BCSP Link Establishment Protocol */
	bool sc_le_muzzled;
	bcsp_le_state_t sc_le_state;
	callout_t sc_le_timer;

	struct sysctllog *sc_log;		/* sysctl log */
};

/* sc_flags */
#define	BCSP_XMIT	(1 << 0)	/* transmit active */
#define	BCSP_ENABLED	(1 << 1)	/* is enabled */

static int bcsp_match(device_t, cfdata_t, void *);
static void bcsp_attach(device_t, device_t, void *);
static int bcsp_detach(device_t, int);

/* tty functions */
static int bcspopen(dev_t, struct tty *);
static int bcspclose(struct tty *, int);
static int bcspioctl(struct tty *, u_long, void *, int, struct lwp *);

static int bcsp_slip_transmit(struct tty *);
static int bcsp_slip_receive(int, struct tty *);

static void bcsp_pktintegrity_transmit(struct bcsp_softc *);
static void bcsp_pktintegrity_receive(struct bcsp_softc *, struct mbuf *);
static void bcsp_crc_update(uint16_t *, uint8_t);
static uint16_t bcsp_crc_reverse(uint16_t);

static void bcsp_mux_transmit(struct bcsp_softc *sc);
static void bcsp_mux_receive(struct bcsp_softc *sc, struct mbuf *m);
static __inline void bcsp_send_ack_command(struct bcsp_softc *sc);
static __inline struct mbuf *bcsp_create_ackpkt(void);
static __inline void bcsp_set_choke(struct bcsp_softc *, bool);

static void bcsp_sequencing_receive(struct bcsp_softc *, struct mbuf *);
static bool bcsp_tx_reliable_pkt(struct bcsp_softc *, struct mbuf *, u_int);
static __inline u_int bcsp_get_txack(struct bcsp_softc *);
static void bcsp_signal_rxack(struct bcsp_softc *, uint32_t);
static void bcsp_reliabletx_callback(struct bcsp_softc *, struct mbuf *);
static void bcsp_timer_timeout(void *);
static void bcsp_sequencing_reset(struct bcsp_softc *);

static void bcsp_datagramq_receive(struct bcsp_softc *, struct mbuf *);
static bool bcsp_tx_unreliable_pkt(struct bcsp_softc *, struct mbuf *, u_int);
static void bcsp_unreliabletx_callback(struct bcsp_softc *, struct mbuf *);

static int bcsp_start_le(struct bcsp_softc *);
static void bcsp_terminate_le(struct bcsp_softc *);
static void bcsp_input_le(struct bcsp_softc *, struct mbuf *);
static void bcsp_le_timeout(void *);

static void bcsp_start(struct bcsp_softc *);

/* bluetooth hci functions */
static int bcsp_enable(device_t);
static void bcsp_disable(device_t);
static void bcsp_output_cmd(device_t, struct mbuf *);
static void bcsp_output_acl(device_t, struct mbuf *);
static void bcsp_output_sco(device_t, struct mbuf *);
static void bcsp_stats(device_t, struct bt_stats *, int);

#ifdef BCSP_DEBUG
static void bcsp_packet_print(struct mbuf *m);
#endif


/*
 * It doesn't need to be exported, as only bcspattach() uses it,
 * but there's no "official" way to make it static.
 */
CFATTACH_DECL_NEW(bcsp, sizeof(struct bcsp_softc),
    bcsp_match, bcsp_attach, bcsp_detach, NULL);

static struct linesw bcsp_disc = {
	.l_name = "bcsp",
	.l_open = bcspopen,
	.l_close = bcspclose,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = bcspioctl,
	.l_rint = bcsp_slip_receive,
	.l_start = bcsp_slip_transmit,
	.l_modem = ttymodem,
	.l_poll = ttyerrpoll
};

static const struct hci_if bcsp_hci = {
	.enable = bcsp_enable,
	.disable = bcsp_disable,
	.output_cmd = bcsp_output_cmd,
	.output_acl = bcsp_output_acl,
	.output_sco = bcsp_output_sco,
	.get_stats = bcsp_stats,
	.ipl = IPL_TTY,
};

/* ARGSUSED */
void
bcspattach(int num __unused)
{
	int error;

	error = ttyldisc_attach(&bcsp_disc);
	if (error) {
		aprint_error("%s: unable to register line discipline, "
		    "error = %d\n", bcsp_cd.cd_name, error);
		return;
	}

	error = config_cfattach_attach(bcsp_cd.cd_name, &bcsp_ca);
	if (error) {
		aprint_error("%s: unable to register cfattach, error = %d\n",
		    bcsp_cd.cd_name, error);
		config_cfdriver_detach(&bcsp_cd);
		(void) ttyldisc_detach(&bcsp_disc);
	}
}

/*
 * Autoconf match routine.
 *
 * XXX: unused: config_attach_pseudo(9) does not call ca_match.
 */
/* ARGSUSED */
static int
bcsp_match(device_t self __unused, cfdata_t cfdata __unused,
	   void *arg __unused)
{

	/* pseudo-device; always present */
	return 1;
}

/*
 * Autoconf attach routine.  Called by config_attach_pseudo(9) when we
 * open the line discipline.
 */
/* ARGSUSED */
static void
bcsp_attach(device_t parent __unused, device_t self, void *aux __unused)
{
	struct bcsp_softc *sc = device_private(self);
	const struct sysctlnode *node;
	int rc, bcsp_node_num;

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_dev = self;
	callout_init(&sc->sc_seq_timer, 0);
	callout_setfunc(&sc->sc_seq_timer, bcsp_timer_timeout, sc);
	callout_init(&sc->sc_le_timer, 0);
	callout_setfunc(&sc->sc_le_timer, bcsp_le_timeout, sc);
	sc->sc_seq_timeout = BCSP_SEQ_TX_TIMEOUT;
	sc->sc_seq_winsize = BCSP_SEQ_TX_WINSIZE;
	sc->sc_seq_retry_limit = BCSP_SEQ_TX_RETRY_LIMIT;
	MBUFQ_INIT(&sc->sc_seqq);
	MBUFQ_INIT(&sc->sc_seq_retryq);
	MBUFQ_INIT(&sc->sc_dgq);
	MBUFQ_INIT(&sc->sc_cmdq);
	MBUFQ_INIT(&sc->sc_aclq);
	MBUFQ_INIT(&sc->sc_scoq);

	/* Attach Bluetooth unit */
	sc->sc_unit = hci_attach_pcb(&bcsp_hci, self, 0);

	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    0, CTLTYPE_NODE, device_xname(self),
	    SYSCTL_DESCR("bcsp controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	bcsp_node_num = node->sysctl_num;
	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_BOOL,
	    "muzzled", SYSCTL_DESCR("muzzled for Link-establishment Layer"),
	    NULL, 0, &sc->sc_le_muzzled,
	    0, CTL_HW, bcsp_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "txcrc", SYSCTL_DESCR("txcrc for Packet Integrity Layer"),
	    NULL, 0, &sc->sc_pi_txcrc,
	    0, CTL_HW, bcsp_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "timeout", SYSCTL_DESCR("timeout for Sequencing Layer"),
	    NULL, 0, &sc->sc_seq_timeout,
	    0, CTL_HW, bcsp_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "winsize", SYSCTL_DESCR("winsize for Sequencing Layer"),
	    NULL, 0, &sc->sc_seq_winsize,
	    0, CTL_HW, bcsp_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	if ((rc = sysctl_createv(&sc->sc_log, 0, NULL, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "retry_limit", SYSCTL_DESCR("retry limit for Sequencing Layer"),
	    NULL, 0, &sc->sc_seq_retry_limit,
	    0, CTL_HW, bcsp_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	return;

err:
	aprint_error_dev(self, "sysctl_createv failed (rc = %d)\n", rc);
}

/*
 * Autoconf detach routine.  Called when we close the line discipline.
 */
/* ARGSUSED */
static int
bcsp_detach(device_t self, int flags __unused)
{
	struct bcsp_softc *sc = device_private(self);

	if (sc->sc_unit != NULL) {
		hci_detach_pcb(sc->sc_unit);
		sc->sc_unit = NULL;
	}

	callout_halt(&sc->sc_seq_timer, NULL);
	callout_destroy(&sc->sc_seq_timer);

	callout_halt(&sc->sc_le_timer, NULL);
	callout_destroy(&sc->sc_le_timer);

	return 0;
}


/*
 * Line discipline functions.
 */
/* ARGSUSED */
static int
bcspopen(dev_t device __unused, struct tty *tp)
{
	struct bcsp_softc *sc;
	device_t dev;
	cfdata_t cfdata;
	struct lwp *l = curlwp;		/* XXX */
	int error, unit, s;
	static char name[] = "bcsp";

	error = kauth_authorize_device(l->l_cred, KAUTH_DEVICE_BLUETOOTH_BCSP,
	    KAUTH_ARG(KAUTH_REQ_DEVICE_BLUETOOTH_BCSP_ADD), NULL, NULL, NULL);
	if (error)
		return (error);

	s = spltty();

	if (tp->t_linesw == &bcsp_disc) {
		sc = tp->t_sc;
		if (sc != NULL) {
			splx(s);
			return EBUSY;
		}
	}

	KASSERT(tp->t_oproc != NULL);

	cfdata = malloc(sizeof(struct cfdata), M_DEVBUF, M_WAITOK);
	for (unit = 0; unit < bcsp_cd.cd_ndevs; unit++)
		if (device_lookup(&bcsp_cd, unit) == NULL)
			break;
	cfdata->cf_name = name;
	cfdata->cf_atname = name;
	cfdata->cf_unit = unit;
	cfdata->cf_fstate = FSTATE_STAR;

	aprint_normal("%s%d at tty major %llu minor %llu",
	    name, unit, (unsigned long long)major(tp->t_dev),
	    (unsigned long long)minor(tp->t_dev));
	dev = config_attach_pseudo(cfdata);
	if (dev == NULL) {
		splx(s);
		return EIO;
	}
	sc = device_private(dev);

	mutex_spin_enter(&tty_lock);
	tp->t_sc = sc;
	sc->sc_tp = tp;
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);

	splx(s);

	sc->sc_slip_txrsv = BCSP_SLIP_PKTSTART;
	bcsp_sequencing_reset(sc);

	/* start link-establishment */
	bcsp_start_le(sc);

	return 0;
}

/* ARGSUSED */
static int
bcspclose(struct tty *tp, int flag __unused)
{
	struct bcsp_softc *sc = tp->t_sc;
	cfdata_t cfdata;
	int s;

	/* terminate link-establishment */
	bcsp_terminate_le(sc);

	s = spltty();

	MBUFQ_DRAIN(&sc->sc_dgq);
	bcsp_sequencing_reset(sc);

	mutex_spin_enter(&tty_lock);
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);	/* XXX */
	ttyldisc_release(tp->t_linesw);
	tp->t_linesw = ttyldisc_default();
	if (sc != NULL) {
		tp->t_sc = NULL;
		if (sc->sc_tp == tp) {
			cfdata = device_cfdata(sc->sc_dev);
			config_detach(sc->sc_dev, 0);
			free(cfdata, M_DEVBUF);
		}

	}
	splx(s);
	return 0;
}

/* ARGSUSED */
static int
bcspioctl(struct tty *tp, u_long cmd, void *data, int flag __unused,
	  struct lwp *l __unused)
{
	struct bcsp_softc *sc = tp->t_sc;
	int error;

	if (sc == NULL || tp != sc->sc_tp)
		return EPASSTHROUGH;

	error = 0;
	switch (cmd) {
	default:
		error = EPASSTHROUGH;
		break;
	}

	return error;
}


/*
 * UART Driver Layer is supported by com-driver.
 */

/*
 * BCSP SLIP Layer functions:
 *   Supports to transmit/receive a byte stream.
 *   SLIP protocol described in Internet standard RFC 1055.
 */
static int
bcsp_slip_transmit(struct tty *tp)
{
	struct bcsp_softc *sc = tp->t_sc;
	struct mbuf *m;
	int count, rlen;
	uint8_t *rptr;

	m = sc->sc_txp;
	if (m == NULL) {
		sc->sc_flags &= ~BCSP_XMIT;
		bcsp_mux_transmit(sc);
		return 0;
	}

	count = 0;
	rlen = 0;
	rptr = mtod(m, uint8_t *);

	if (sc->sc_slip_txrsv != 0) {
#ifdef BCSP_DEBUG
		if (sc->sc_slip_txrsv == BCSP_SLIP_PKTSTART)
			DPRINTFN(4, ("%s: slip transmit start\n",
			    device_xname(sc->sc_dev)));
		else
			DPRINTFN(4, ("0x%02x ", sc->sc_slip_txrsv));
#endif

		if (putc(sc->sc_slip_txrsv, &tp->t_outq) < 0)
			return 0;
		count++;

		if (sc->sc_slip_txrsv == BCSP_SLIP_ESCAPE_PKTEND ||
		    sc->sc_slip_txrsv == BCSP_SLIP_ESCAPE_ESCAPE) {
			rlen++;
			rptr++;
		}
		sc->sc_slip_txrsv = 0;
	}

	for(;;) {
		if (rlen >= m->m_len) {
			m = m->m_next;
			if (m == NULL) {
				if (putc(BCSP_SLIP_PKTEND, &tp->t_outq) < 0)
					break;

				DPRINTFN(4, ("\n%s: slip transmit end\n",
				    device_xname(sc->sc_dev)));

				m = sc->sc_txp;
				sc->sc_txp = NULL;
				sc->sc_slip_txrsv = BCSP_SLIP_PKTSTART;

				sc->sc_transmit_callback(sc, m);
				m = NULL;
				break;
			}

			rlen = 0;
			rptr = mtod(m, uint8_t *);
			continue;
		}

		if (*rptr == BCSP_SLIP_PKTEND) {
			if (putc(BCSP_SLIP_ESCAPE, &tp->t_outq) < 0)
				break;
			count++;
			DPRINTFN(4, (" esc "));

			if (putc(BCSP_SLIP_ESCAPE_PKTEND, &tp->t_outq) < 0) {
				sc->sc_slip_txrsv = BCSP_SLIP_ESCAPE_PKTEND;
				break;
			}
			DPRINTFN(4, ("0x%02x ", BCSP_SLIP_ESCAPE_PKTEND));
			rptr++;
		} else if (*rptr == BCSP_SLIP_ESCAPE) {
			if (putc(BCSP_SLIP_ESCAPE, &tp->t_outq) < 0)
				break;
			count++;
			DPRINTFN(4, (" esc "));

			if (putc(BCSP_SLIP_ESCAPE_ESCAPE, &tp->t_outq) < 0) {
				sc->sc_slip_txrsv = BCSP_SLIP_ESCAPE_ESCAPE;
				break;
			}
			DPRINTFN(4, ("0x%02x ", BCSP_SLIP_ESCAPE_ESCAPE));
			rptr++;
		} else {
			if (putc(*rptr++, &tp->t_outq) < 0)
				break;
			DPRINTFN(4, ("0x%02x ", *(rptr - 1)));
		}
		rlen++;
		count++;
	}
	if (m != NULL)
		m_adj(m, rlen);

	sc->sc_stats.byte_tx += count;

	if (tp->t_outq.c_cc != 0)
		(*tp->t_oproc)(tp);

	return 0;
}

static int
bcsp_slip_receive(int c, struct tty *tp)
{
	struct bcsp_softc *sc = tp->t_sc;
	struct mbuf *m = sc->sc_rxp;
	int discard = 0;
	const char *errstr;

	c &= TTY_CHARMASK;

	/* If we already started a packet, find the trailing end of it. */
	if (m) {
		while (m->m_next)
			m = m->m_next;

		if (M_TRAILINGSPACE(m) == 0) {
			/* extend mbuf */
			MGET(m->m_next, M_DONTWAIT, MT_DATA);
			if (m->m_next == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "out of memory\n");
				sc->sc_stats.err_rx++;
				return 0;	/* (lost sync) */
			}

			m = m->m_next;
			m->m_len = 0;
		}
	} else
		if (c != BCSP_SLIP_PKTSTART) {
			discard = 1;
			errstr = "not sync";
			goto discarded;
		}

	switch (c) {
	case BCSP_SLIP_PKTSTART /* or _PKTEND */:
		if (m == NULL) {
			/* BCSP_SLIP_PKTSTART */

			DPRINTFN(4, ("%s: slip receive start\n",
			    device_xname(sc->sc_dev)));

			/* new packet */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "out of memory\n");
				sc->sc_stats.err_rx++;
				return 0;	/* (lost sync) */
			}

			sc->sc_rxp = m;
			m->m_pkthdr.len = m->m_len = 0;
			sc->sc_slip_rxexp = 0;
		} else {
			/* BCSP_SLIP_PKTEND */

			if (m == sc->sc_rxp && m->m_len == 0) {
				DPRINTFN(4, ("%s: resynchronises\n",
				    device_xname(sc->sc_dev)));

				sc->sc_stats.byte_rx++;
				return 0;
			}

			DPRINTFN(4, ("%s%s: slip receive end\n",
			    (m->m_len % 16 != 0) ? "\n" :  "",
			    device_xname(sc->sc_dev)));

			bcsp_pktintegrity_receive(sc, sc->sc_rxp);
			sc->sc_rxp = NULL;
			sc->sc_slip_rxexp = BCSP_SLIP_PKTSTART;
		}
		sc->sc_stats.byte_rx++;
		return 0;

	case BCSP_SLIP_ESCAPE:

		DPRINTFN(4, ("  esc"));

		if (sc->sc_slip_rxexp == BCSP_SLIP_ESCAPE) {
			discard = 1;
			errstr = "waiting 0xdc or 0xdb"; 
		} else
			sc->sc_slip_rxexp = BCSP_SLIP_ESCAPE;
		break;

	default:
		DPRINTFN(4, (" 0x%02x%s",
		    c, (m->m_len % 16 == 15) ? "\n" :  ""));

		switch (sc->sc_slip_rxexp) {
		case BCSP_SLIP_PKTSTART:
			discard = 1;
			errstr = "waiting 0xc0";
			break;

		case BCSP_SLIP_ESCAPE:
			if (c == BCSP_SLIP_ESCAPE_PKTEND)
				mtod(m, uint8_t *)[m->m_len++] =
				    BCSP_SLIP_PKTEND;
			else if (c == BCSP_SLIP_ESCAPE_ESCAPE)
				mtod(m, uint8_t *)[m->m_len++] =
				    BCSP_SLIP_ESCAPE;
			else {
				discard = 1;
				errstr = "unknown escape";
			}
			sc->sc_slip_rxexp = 0;
			break;

		default:
			mtod(m, uint8_t *)[m->m_len++] = c;
		}
		sc->sc_rxp->m_pkthdr.len++;
	}
	if (discard) {
discarded:
#ifdef BCSP_DEBUG
		DPRINTFN(4, ("%s: receives unexpected byte 0x%02x: %s\n",
		    device_xname(sc->sc_dev), c, errstr));
#else
		__USE(errstr);
#endif
	}
	sc->sc_stats.byte_rx++;

	return 0;
}


/*
 * BCSP Packet Integrity Layer functions:
 *   handling Payload Length, Checksum, CRC.
 */
static void
bcsp_pktintegrity_transmit(struct bcsp_softc *sc)
{
	struct mbuf *m = sc->sc_txp;
	bcsp_hdr_t *hdrp = mtod(m, bcsp_hdr_t *);
	int pldlen;

	DPRINTFN(3, ("%s: pi transmit\n", device_xname(sc->sc_dev)));

	pldlen = m->m_pkthdr.len - sizeof(bcsp_hdr_t);

	if (sc->sc_pi_txcrc)
		hdrp->flags |= BCSP_FLAGS_CRC_PRESENT;

	BCSP_SET_PLEN(hdrp, pldlen);
	BCSP_SET_CSUM(hdrp);

	if (sc->sc_pi_txcrc) {
		struct mbuf *_m;
		int n = 0;
		uint16_t crc = 0xffff;
		uint8_t *buf;

		for (_m = m; _m != NULL; _m = _m->m_next) {
			buf = mtod(_m, uint8_t *);
			for (n = 0; n < _m->m_len; n++)
				bcsp_crc_update(&crc, *(buf + n));
		}
		crc = htobe16(bcsp_crc_reverse(crc));
		m_copyback(m, m->m_pkthdr.len, sizeof(crc), &crc);
	}

#ifdef BCSP_DEBUG
	if (bcsp_debug == 4)
		bcsp_packet_print(m);
#endif

	bcsp_slip_transmit(sc->sc_tp);
}

static void
bcsp_pktintegrity_receive(struct bcsp_softc *sc, struct mbuf *m)
{
	bcsp_hdr_t *hdrp = mtod(m, bcsp_hdr_t *);
	u_int pldlen;
	int discard = 0;
	uint16_t crc = 0xffff;
	const char *errstr 

	DPRINTFN(3, ("%s: pi receive\n", device_xname(sc->sc_dev)));
#ifdef BCSP_DEBUG
	if (bcsp_debug == 4)
		bcsp_packet_print(m);
#endif

	KASSERT(m->m_len >= sizeof(bcsp_hdr_t));

	pldlen = m->m_pkthdr.len - sizeof(bcsp_hdr_t) -
	    ((hdrp->flags & BCSP_FLAGS_CRC_PRESENT) ? sizeof(crc) : 0);
	if (pldlen > 0xfff) {
		discard = 1;
		errstr = "Payload Length";
		goto discarded;
	}
	if (hdrp->csum != BCSP_GET_CSUM(hdrp)) {
		discard = 1;
		errstr = "Checksum";
		goto discarded;
	}
	if (BCSP_GET_PLEN(hdrp) != pldlen) {
		discard = 1;
		errstr = "Payload Length";
		goto discarded;
	}
	if (hdrp->flags & BCSP_FLAGS_CRC_PRESENT) {
		struct mbuf *_m;
		int i, n;
		uint16_t crc0;
		uint8_t *buf;

		i = 0;
		n = 0;
		for (_m = m; _m != NULL; _m = _m->m_next) {
			buf = mtod(m, uint8_t *);
			for (n = 0;
			    n < _m->m_len && i < sizeof(bcsp_hdr_t) + pldlen;
			    n++, i++)
				bcsp_crc_update(&crc, *(buf + n));
		}

		m_copydata(_m, n, sizeof(crc0), &crc0);
		if (be16toh(crc0) != bcsp_crc_reverse(crc)) {
			discard = 1;
			errstr = "CRC";
		} else
			/* Shaves CRC */
			m_adj(m, (int)(0 - sizeof(crc)));
	}

	if (discard) {
discarded:
#ifdef BCSP_DEBUG
		DPRINTFN(3, ("%s: receives unexpected packet: %s\n",
		    device_xname(sc->sc_dev), errstr));
#else
		__USE(errstr);
#endif
		m_freem(m);
	} else
		bcsp_mux_receive(sc, m);
}

static const uint16_t crctbl[] = {
	0x0000, 0x1081, 0x2102, 0x3183,
	0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xa50a, 0xb58b,
	0xc60c, 0xd68d, 0xe70e, 0xf78f,
};

static void
bcsp_crc_update(uint16_t *crc, uint8_t d)
{
	uint16_t reg = *crc;

	reg = (reg >> 4) ^ crctbl[(reg ^ d) & 0x000f];
	reg = (reg >> 4) ^ crctbl[(reg ^ (d >> 4)) & 0x000f];

	*crc = reg;
}

static uint16_t
bcsp_crc_reverse(uint16_t crc)
{
	uint16_t b, rev;

	for (b = 0, rev = 0; b < 16; b++) {
		rev = rev << 1;
		rev |= (crc & 1);
		crc = crc >> 1;
	}

	return rev;
}


/*
 * BCSP MUX Layer functions
 */
static void
bcsp_mux_transmit(struct bcsp_softc *sc)
{
	struct mbuf *m;
	bcsp_hdr_t *hdrp;

	DPRINTFN(2, ("%s: mux transmit: sc_flags=0x%x, choke=%d",
	    device_xname(sc->sc_dev), sc->sc_flags, sc->sc_mux_choke));

	if (sc->sc_mux_choke) {
		struct mbuf *_m = NULL;

		/* In this case, send only Link Establishment packet */
		for (m = MBUFQ_FIRST(&sc->sc_dgq); m != NULL;
		    _m = m, m = MBUFQ_NEXT(m)) {
			hdrp = mtod(m, bcsp_hdr_t *);
			if (hdrp->ident == BCSP_CHANNEL_LE) {
				if (m == MBUFQ_FIRST(&sc->sc_dgq))
					MBUFQ_DEQUEUE(&sc->sc_dgq, m);
				else {
					if (m->m_nextpkt == NULL)
						sc->sc_dgq.mq_last =
						    &_m->m_nextpkt;
					_m->m_nextpkt = m->m_nextpkt;
					m->m_nextpkt = NULL;
				}
				goto transmit;
			}
		}
		DPRINTFN(2, ("\n"));
		return;
	}

	/*
	 * The MUX Layer always gives priority to packets from the Datagram
	 * Queue Layer over the Sequencing Layer.
	 */
	if (MBUFQ_FIRST(&sc->sc_dgq)) {
		MBUFQ_DEQUEUE(&sc->sc_dgq, m);
		goto transmit;
	}
	if (MBUFQ_FIRST(&sc->sc_seqq)) {
		MBUFQ_DEQUEUE(&sc->sc_seqq, m);
		hdrp = mtod(m, bcsp_hdr_t *);
		hdrp->flags |= BCSP_FLAGS_PROTOCOL_REL;		/* Reliable */
		goto transmit;
	}
	bcsp_start(sc);
	if (sc->sc_mux_send_ack == true) {
		m = bcsp_create_ackpkt();
		if (m != NULL)
			goto transmit;
		aprint_error_dev(sc->sc_dev, "out of memory\n");
		sc->sc_stats.err_tx++;
	}

	/* Nothing to send */
	DPRINTFN(2, ("\n"));
	return;

transmit:
	DPRINTFN(2, (", txack=%d, send_ack=%d\n",
	    bcsp_get_txack(sc), sc->sc_mux_send_ack));

	hdrp = mtod(m, bcsp_hdr_t *);
	hdrp->flags |=
	    (bcsp_get_txack(sc) << BCSP_FLAGS_ACK_SHIFT) & BCSP_FLAGS_ACK_MASK;
	if (sc->sc_mux_send_ack == true)
		sc->sc_mux_send_ack = false;

#ifdef BCSP_DEBUG
	if (bcsp_debug == 3)
		bcsp_packet_print(m);
#endif

	sc->sc_txp = m;
	bcsp_pktintegrity_transmit(sc);
}

static void
bcsp_mux_receive(struct bcsp_softc *sc, struct mbuf *m)
{
	bcsp_hdr_t *hdrp = mtod(m, bcsp_hdr_t *);
	const u_int rxack = BCSP_FLAGS_ACK(hdrp->flags);

	DPRINTFN(2, ("%s: mux receive: flags=0x%x, ident=%d, rxack=%d\n",
	    device_xname(sc->sc_dev), hdrp->flags, hdrp->ident, rxack));
#ifdef BCSP_DEBUG
	if (bcsp_debug == 3)
		bcsp_packet_print(m);
#endif

	bcsp_signal_rxack(sc, rxack);

	microtime(&sc->sc_mux_lastrx);

	/* if the Ack Packet received then discard */
	if (BCSP_FLAGS_SEQ(hdrp->flags) == 0 &&
	    hdrp->ident == BCSP_IDENT_ACKPKT &&
	    BCSP_GET_PLEN(hdrp) == 0) {
		m_freem(m);
		return;
	}

	if (hdrp->flags & BCSP_FLAGS_PROTOCOL_REL)
		bcsp_sequencing_receive(sc, m);
	else
		bcsp_datagramq_receive(sc, m);
}

static __inline void
bcsp_send_ack_command(struct bcsp_softc *sc)
{

	DPRINTFN(2, ("%s: mux send_ack_command\n", device_xname(sc->sc_dev)));

	sc->sc_mux_send_ack = true;
}

static __inline struct mbuf *
bcsp_create_ackpkt(void)
{
	struct mbuf *m;
	bcsp_hdr_t *hdrp;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		m->m_pkthdr.len = m->m_len = sizeof(bcsp_hdr_t);
		hdrp = mtod(m, bcsp_hdr_t *);
		/*
		 * An Ack Packet has the following fields:
		 *	Ack Field:			txack (not set yet)
		 *	Seq Field:			0
		 *	Protocol Identifier Field:	0
		 *	Protocol Type Field:		Any value
		 *	Payload Length Field:		0
		 */
		memset(hdrp, 0, sizeof(bcsp_hdr_t));
	}
	return m;
}

static __inline void
bcsp_set_choke(struct bcsp_softc *sc, bool choke)
{

	DPRINTFN(2, ("%s: mux set choke=%d\n", device_xname(sc->sc_dev), choke));

	sc->sc_mux_choke = choke;
}


/*
 * BCSP Sequencing Layer functions
 */
static void
bcsp_sequencing_receive(struct bcsp_softc *sc, struct mbuf *m)
{
	bcsp_hdr_t hdr;
	uint32_t rxseq;

	m_copydata(m, 0, sizeof(bcsp_hdr_t), &hdr);
	rxseq = BCSP_FLAGS_SEQ(hdr.flags);

	DPRINTFN(1, ("%s: seq receive: rxseq=%d, expected %d\n",
	    device_xname(sc->sc_dev), rxseq, sc->sc_seq_expected_rxseq));
#ifdef BCSP_DEBUG
	if (bcsp_debug == 2)
		bcsp_packet_print(m);
#endif

	/*
	 * We remove the header of BCSP and add the 'uint8_t type' of
	 * hci_*_hdr_t to the head. 
	 */
	m_adj(m, sizeof(bcsp_hdr_t) - sizeof(uint8_t));

	if (rxseq != sc->sc_seq_expected_rxseq) {
		m_freem(m);

		/* send ack packet, if needly */
		bcsp_mux_transmit(sc);

		return;
	}

	switch (hdr.ident) {
	case BCSP_CHANNEL_HCI_CMDEVT:
		*(mtod(m, uint8_t *)) = HCI_EVENT_PKT;
		if (!hci_input_event(sc->sc_unit, m))
			sc->sc_stats.err_rx++;

		sc->sc_stats.evt_rx++;
		break;

	case BCSP_CHANNEL_HCI_ACL:
		*(mtod(m, uint8_t *)) = HCI_ACL_DATA_PKT;
		if (!hci_input_acl(sc->sc_unit, m))
			sc->sc_stats.err_rx++;

		sc->sc_stats.acl_rx++;
		break;

	case BCSP_CHANNEL_HCI_SCO:
		*(mtod(m, uint8_t *)) = HCI_SCO_DATA_PKT;
		if (!hci_input_sco(sc->sc_unit, m))
			sc->sc_stats.err_rx++;

		sc->sc_stats.sco_rx++;
		break;

	case BCSP_CHANNEL_HQ:
	case BCSP_CHANNEL_DEVMGT:
	case BCSP_CHANNEL_L2CAP:
	case BCSP_CHANNEL_RFCOMM:
	case BCSP_CHANNEL_SDP:
	case BCSP_CHANNEL_DFU:
	case BCSP_CHANNEL_VM:
	default:
		aprint_error_dev(sc->sc_dev,
		    "received reliable packet with not support channel %d\n",
		    hdr.ident);
		m_freem(m);
		break;
	}

	sc->sc_seq_expected_rxseq =
	    (sc->sc_seq_expected_rxseq + 1) & BCSP_FLAGS_SEQ_MASK;
	sc->sc_seq_txack = sc->sc_seq_expected_rxseq;
	bcsp_send_ack_command(sc);
}

static bool
bcsp_tx_reliable_pkt(struct bcsp_softc *sc, struct mbuf *m, u_int protocol_id)
{
	bcsp_hdr_t *hdrp;
	struct mbuf *_m;
	u_int pldlen;
	int s;

	DPRINTFN(1, ("%s: seq transmit:"
	    "protocol_id=%d, winspace=%d, txseq=%d\n", device_xname(sc->sc_dev),
	    protocol_id, sc->sc_seq_winspace, sc->sc_seq_txseq));

	for (pldlen = 0, _m = m; _m != NULL; _m = _m->m_next) {
		if (_m->m_len < 0)
			goto out;
		pldlen += _m->m_len;
	}
	if (pldlen > 0xfff)
		goto out;
	if (protocol_id == BCSP_IDENT_ACKPKT || protocol_id > 15)
		goto out;

	if (sc->sc_seq_winspace == 0)
		goto out;

	M_PREPEND(m, sizeof(bcsp_hdr_t), M_DONTWAIT);
	if (m == NULL) {
		aprint_error_dev(sc->sc_dev, "out of memory\n");
		return false;
	}
	KASSERT(m->m_len >= sizeof(bcsp_hdr_t));

	hdrp = mtod(m, bcsp_hdr_t *);
	memset(hdrp, 0, sizeof(bcsp_hdr_t));
	hdrp->flags |= sc->sc_seq_txseq;
	hdrp->ident = protocol_id;

	callout_schedule(&sc->sc_seq_timer, sc->sc_seq_timeout);

	s = splserial();
	MBUFQ_ENQUEUE(&sc->sc_seqq, m);
	splx(s);
	sc->sc_transmit_callback = bcsp_reliabletx_callback;

#ifdef BCSP_DEBUG
	if (bcsp_debug == 2)
		bcsp_packet_print(m);
#endif

	sc->sc_seq_txseq = (sc->sc_seq_txseq + 1) & BCSP_FLAGS_SEQ_MASK;
	sc->sc_seq_winspace--;
	_m = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
	if (_m == NULL) {
		aprint_error_dev(sc->sc_dev, "out of memory\n");
		goto out;
	}
	MBUFQ_ENQUEUE(&sc->sc_seq_retryq, _m);
	bcsp_mux_transmit(sc);

	return true;
out:
	m_freem(m);
	return false;
}

#if 0
static bool
bcsp_rx_reliable_pkt(struct bcsp_softc *sc, struct mbuf *m, u_int protocol_id)
{

	return false;
}

/* XXXX:  I can't understand meaning this function... */
static __inline void
bcsp_link_failed(struct bcsp_softc *sc)
{

	return (sc->sc_seq_retries >= sc->sc_seq_retry_limit);
}
#endif

static __inline u_int
bcsp_get_txack(struct bcsp_softc *sc)
{

	return sc->sc_seq_txack;
}

static void
bcsp_signal_rxack(struct bcsp_softc *sc, uint32_t rxack)
{
	bcsp_hdr_t *hdrp;
	struct mbuf *m;
	uint32_t seqno = (rxack - 1) & BCSP_FLAGS_SEQ_MASK;
	int s;

	DPRINTFN(1, ("%s: seq signal rxack: rxack=%d\n",
	    device_xname(sc->sc_dev), rxack));

	s = splserial();
	m = MBUFQ_FIRST(&sc->sc_seq_retryq);
	while (m != NULL) {
		hdrp = mtod(m, bcsp_hdr_t *);
		if (BCSP_FLAGS_SEQ(hdrp->flags) == seqno) {
			struct mbuf *m0;

			for (m0 = MBUFQ_FIRST(&sc->sc_seq_retryq);
			    m0 != MBUFQ_NEXT(m);
			    m0 = MBUFQ_FIRST(&sc->sc_seq_retryq)) {
				MBUFQ_DEQUEUE(&sc->sc_seq_retryq, m0);
				m_freem(m0);
				sc->sc_seq_winspace++;
			}
			break;
		}
		m = MBUFQ_NEXT(m);
	}
	splx(s);
	sc->sc_seq_retries = 0;

	if (sc->sc_seq_winspace == sc->sc_seq_winsize)
		callout_stop(&sc->sc_seq_timer);
	else
		callout_schedule(&sc->sc_seq_timer, sc->sc_seq_timeout);
}

static void
bcsp_reliabletx_callback(struct bcsp_softc *sc, struct mbuf *m)
{

	m_freem(m);
}

static void
bcsp_timer_timeout(void *arg)
{
	struct bcsp_softc *sc = arg;
	struct mbuf *m, *_m;
	int s, i = 0;

	DPRINTFN(1, ("%s: seq timeout: retries=%d\n",
	    device_xname(sc->sc_dev), sc->sc_seq_retries));

	s = splserial();
	for (m = MBUFQ_FIRST(&sc->sc_seq_retryq); m != NULL;
	    m = MBUFQ_NEXT(m)) {
		_m = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (_m == NULL) {
			aprint_error_dev(sc->sc_dev, "out of memory\n");
			return;
		}
		MBUFQ_ENQUEUE(&sc->sc_seqq, _m);
		i++;
	}
	splx(s);

	if (i != 0) {
		if (++sc->sc_seq_retries < sc->sc_seq_retry_limit)
			callout_schedule(&sc->sc_seq_timer, sc->sc_seq_timeout);
		else {
			aprint_error_dev(sc->sc_dev,
			    "reached the retry limit."
			    " restart the link-establishment\n");
			bcsp_sequencing_reset(sc);
			bcsp_start_le(sc);
			return;
		}
	}
	bcsp_mux_transmit(sc);
}

static void
bcsp_sequencing_reset(struct bcsp_softc *sc)
{
	int s;

	s = splserial();
	MBUFQ_DRAIN(&sc->sc_seqq);
	MBUFQ_DRAIN(&sc->sc_seq_retryq);
	splx(s);


	sc->sc_seq_txseq = 0;
	sc->sc_seq_txack = 0;
	sc->sc_seq_winspace = sc->sc_seq_winsize;
	sc->sc_seq_retries = 0;
	callout_stop(&sc->sc_seq_timer);

	sc->sc_mux_send_ack = false;

	/* XXXX: expected_rxseq should be set by MUX Layer */
	sc->sc_seq_expected_rxseq = 0;
}


/*
 * BCSP Datagram Queue Layer functions
 */
static void
bcsp_datagramq_receive(struct bcsp_softc *sc, struct mbuf *m)
{
	bcsp_hdr_t hdr;

	DPRINTFN(1, ("%s: dgq receive\n", device_xname(sc->sc_dev)));
#ifdef BCSP_DEBUG
	if (bcsp_debug == 2)
		bcsp_packet_print(m);
#endif

	m_copydata(m, 0, sizeof(bcsp_hdr_t), &hdr);

	switch (hdr.ident) {
	case BCSP_CHANNEL_LE:
		m_adj(m, sizeof(bcsp_hdr_t));
		bcsp_input_le(sc, m);
		break;

	case BCSP_CHANNEL_HCI_SCO:
		/*
		 * We remove the header of BCSP and add the 'uint8_t type' of
		 * hci_scodata_hdr_t to the head. 
		 */
		m_adj(m, sizeof(bcsp_hdr_t) - sizeof(uint8_t));
		*(mtod(m, uint8_t *)) = HCI_SCO_DATA_PKT;
		if (!hci_input_sco(sc->sc_unit, m))
			sc->sc_stats.err_rx++;

		sc->sc_stats.sco_rx++;
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "received unreliable packet with not support channel %d\n",
		    hdr.ident);
		m_freem(m);
		break;
	}
}

static bool
bcsp_tx_unreliable_pkt(struct bcsp_softc *sc, struct mbuf *m, u_int protocol_id)
{
	bcsp_hdr_t *hdrp;
	struct mbuf *_m;
	u_int pldlen;
	int s;

	DPRINTFN(1, ("%s: dgq transmit: protocol_id=%d,",
	    device_xname(sc->sc_dev), protocol_id));

	for (pldlen = 0, _m = m; _m != NULL; _m = m->m_next) {
		if (_m->m_len < 0)
			goto out;
		pldlen += _m->m_len;
	}
	DPRINTFN(1, (" pldlen=%d\n", pldlen));
	if (pldlen > 0xfff)
		goto out;
	if (protocol_id == BCSP_IDENT_ACKPKT || protocol_id > 15)
		goto out;

	M_PREPEND(m, sizeof(bcsp_hdr_t), M_DONTWAIT);
	if (m == NULL) {
		aprint_error_dev(sc->sc_dev, "out of memory\n");
		return false;
	}
	KASSERT(m->m_len >= sizeof(bcsp_hdr_t));

	hdrp = mtod(m, bcsp_hdr_t *);
	memset(hdrp, 0, sizeof(bcsp_hdr_t));
	hdrp->ident = protocol_id;

	s = splserial();
	MBUFQ_ENQUEUE(&sc->sc_dgq, m);
	splx(s);
	sc->sc_transmit_callback = bcsp_unreliabletx_callback;

#ifdef BCSP_DEBUG
	if (bcsp_debug == 2)
		bcsp_packet_print(m);
#endif

	bcsp_mux_transmit(sc);

	return true;
out:
	m_freem(m);
	return false;
}

#if 0
static bool
bcsp_rx_unreliable_pkt(struct bcsp_softc *sc, struct mbuf *m, u_int protocol_id)
{

	return false;
}
#endif

static void
bcsp_unreliabletx_callback(struct bcsp_softc *sc, struct mbuf *m)
{

	if (M_GETCTX(m, void *) == NULL)
		m_freem(m);
	else if (!hci_complete_sco(sc->sc_unit, m))
		sc->sc_stats.err_tx++;
}


/*
 * BlueCore Link Establishment Protocol functions
 */
static const uint8_t sync[] = BCSP_LE_SYNC;
static const uint8_t syncresp[] = BCSP_LE_SYNCRESP;
static const uint8_t conf[] = BCSP_LE_CONF;
static const uint8_t confresp[] = BCSP_LE_CONFRESP;

static int
bcsp_start_le(struct bcsp_softc *sc)
{

	DPRINTF(("%s: start link-establish\n", device_xname(sc->sc_dev)));

	bcsp_set_choke(sc, true);

	if (!sc->sc_le_muzzled) {
		struct mbuf *m;

		m = m_gethdr(M_WAIT, MT_DATA);
		m->m_pkthdr.len = m->m_len = 0;
		m_copyback(m, 0, sizeof(sync), sync);
		if (!bcsp_tx_unreliable_pkt(sc, m, BCSP_CHANNEL_LE)) {
			aprint_error_dev(sc->sc_dev,
			    "le-packet transmit failed\n");
			return EINVAL;
		}
	}
	callout_schedule(&sc->sc_le_timer, BCSP_LE_TSHY_TIMEOUT);

	sc->sc_le_state = le_state_shy;
	return 0;
}

static void
bcsp_terminate_le(struct bcsp_softc *sc)
{
	struct mbuf *m;

	/* terminate link-establishment */
	callout_stop(&sc->sc_le_timer);
	bcsp_set_choke(sc, true);
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		aprint_error_dev(sc->sc_dev, "out of memory\n");
	else {
		/* length of le packets is 4 */
		m->m_pkthdr.len = m->m_len = 0;
		m_copyback(m, 0, sizeof(sync), sync);
		if (!bcsp_tx_unreliable_pkt(sc, m, BCSP_CHANNEL_LE))
			aprint_error_dev(sc->sc_dev,
			    "link-establishment terminations failed\n");
	}
}

static void
bcsp_input_le(struct bcsp_softc *sc, struct mbuf *m)
{
	uint32_t *rcvpkt;
	int i;
	const uint8_t *rplypkt;
	static struct {
		const char *type;
		const uint8_t *datap;
	} pkt[] = {
		{ "sync",	sync },
		{ "sync-resp",	syncresp },
		{ "conf",	conf },
		{ "conf-resp",	confresp },

		{ NULL, 0 }
	};

	DPRINTFN(0, ("%s: le input: state %d, muzzled %d\n",
	    device_xname(sc->sc_dev), sc->sc_le_state, sc->sc_le_muzzled));
#ifdef BCSP_DEBUG
	if (bcsp_debug == 1)
		bcsp_packet_print(m);
#endif

	rcvpkt = mtod(m, uint32_t *);
	i = 0;

	/* length of le packets is 4 */
	if (m->m_len == sizeof(uint32_t))
		for (i = 0; pkt[i].type != NULL; i++)
			if (*(const uint32_t *)pkt[i].datap == *rcvpkt)
				break;
	if (m->m_len != sizeof(uint32_t) || pkt[i].type == NULL) {
		aprint_error_dev(sc->sc_dev, "received unknown packet\n");
		m_freem(m);
		return;
	}

	rplypkt = NULL;
	switch (sc->sc_le_state) {
	case le_state_shy:
		if (*rcvpkt == *(const uint32_t *)sync) {
			sc->sc_le_muzzled = false;
			rplypkt = syncresp;
		} else if (*rcvpkt == *(const uint32_t *)syncresp) {
			DPRINTF(("%s: state change to curious\n",
			    device_xname(sc->sc_dev)));

			rplypkt = conf;
			callout_schedule(&sc->sc_le_timer,
			    BCSP_LE_TCONF_TIMEOUT);
			sc->sc_le_state = le_state_curious;
		} else
			aprint_error_dev(sc->sc_dev,
			    "received an unknown packet at shy\n");
		break;

	case le_state_curious:
		if (*rcvpkt == *(const uint32_t *)sync)
			rplypkt = syncresp;
		else if (*rcvpkt == *(const uint32_t *)conf)
			rplypkt = confresp;
		else if (*rcvpkt == *(const uint32_t *)confresp) {
			DPRINTF(("%s: state change to garrulous:\n",
			    device_xname(sc->sc_dev)));

			bcsp_set_choke(sc, false);
			callout_stop(&sc->sc_le_timer);
			sc->sc_le_state = le_state_garrulous;
		} else
			aprint_error_dev(sc->sc_dev,
			    "received unknown packet at curious\n");
		break;

	case le_state_garrulous:
		if (*rcvpkt == *(const uint32_t *)conf)
			rplypkt = confresp;
		else if (*rcvpkt == *(const uint32_t *)sync) {
			/* XXXXX */
			aprint_error_dev(sc->sc_dev,
			    "received sync! peer to reset?\n");

			bcsp_sequencing_reset(sc);
			rplypkt = sync;
			sc->sc_le_state = le_state_shy;
		} else
			aprint_error_dev(sc->sc_dev,
			    "received unknown packet at garrulous\n");
		break;
	}

	m_freem(m);

	if (rplypkt != NULL) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			aprint_error_dev(sc->sc_dev, "out of memory\n");
		else {
			/* length of le packets is 4 */
			m->m_pkthdr.len = m->m_len = 0;
			m_copyback(m, 0, 4, rplypkt);
			if (!bcsp_tx_unreliable_pkt(sc, m, BCSP_CHANNEL_LE))
				aprint_error_dev(sc->sc_dev,
				    "le-packet transmit failed\n");
		}
	}
}

static void
bcsp_le_timeout(void *arg)
{
	struct bcsp_softc *sc = arg;
	struct mbuf *m;
	int timeout;
	const uint8_t *sndpkt = NULL;

	DPRINTFN(0, ("%s: le timeout: state %d, muzzled %d\n",
	    device_xname(sc->sc_dev), sc->sc_le_state, sc->sc_le_muzzled));

	switch (sc->sc_le_state) {
	case le_state_shy:
		if (!sc->sc_le_muzzled)
			sndpkt = sync;
		timeout = BCSP_LE_TSHY_TIMEOUT;
		break;

	case le_state_curious:
		sndpkt = conf;
		timeout = BCSP_LE_TCONF_TIMEOUT;
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "timeout happen at unknown state %d\n", sc->sc_le_state);
		return;
	}

	if (sndpkt != NULL) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			aprint_error_dev(sc->sc_dev, "out of memory\n");
		else {
			/* length of le packets is 4 */
			m->m_pkthdr.len = m->m_len = 0;
			m_copyback(m, 0, 4, sndpkt);
			if (!bcsp_tx_unreliable_pkt(sc, m, BCSP_CHANNEL_LE))
				aprint_error_dev(sc->sc_dev,
				    "le-packet transmit failed\n");
		}
	}

	callout_schedule(&sc->sc_le_timer, timeout);
}


/*
 * BlueCore Serial Protocol functions.
 */
static int
bcsp_enable(device_t self)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	if (sc->sc_flags & BCSP_ENABLED)
		return 0;

	s = spltty();

	sc->sc_flags |= BCSP_ENABLED;
	sc->sc_flags &= ~BCSP_XMIT;

	splx(s);

	return 0;
}

static void
bcsp_disable(device_t self)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	if ((sc->sc_flags & BCSP_ENABLED) == 0)
		return;

	s = spltty();

	if (sc->sc_rxp) {
		m_freem(sc->sc_rxp);
		sc->sc_rxp = NULL;
	}

	if (sc->sc_txp) {
		m_freem(sc->sc_txp);
		sc->sc_txp = NULL;
	}

	MBUFQ_DRAIN(&sc->sc_cmdq);
	MBUFQ_DRAIN(&sc->sc_aclq);
	MBUFQ_DRAIN(&sc->sc_scoq);

	sc->sc_flags &= ~BCSP_ENABLED;
	splx(s);
}

static void
bcsp_start(struct bcsp_softc *sc)
{
	struct mbuf *m;

	KASSERT((sc->sc_flags & BCSP_XMIT) == 0);
	KASSERT(sc->sc_txp == NULL);

	if (MBUFQ_FIRST(&sc->sc_aclq)) {
		MBUFQ_DEQUEUE(&sc->sc_aclq, m);
		sc->sc_stats.acl_tx++;
		sc->sc_flags |= BCSP_XMIT;
		bcsp_tx_reliable_pkt(sc, m, BCSP_CHANNEL_HCI_ACL);
	}

	if (MBUFQ_FIRST(&sc->sc_cmdq)) {
		MBUFQ_DEQUEUE(&sc->sc_cmdq, m);
		sc->sc_stats.cmd_tx++;
		sc->sc_flags |= BCSP_XMIT;
		bcsp_tx_reliable_pkt(sc, m, BCSP_CHANNEL_HCI_CMDEVT);
	}

	if (MBUFQ_FIRST(&sc->sc_scoq)) {
		MBUFQ_DEQUEUE(&sc->sc_scoq, m);
		sc->sc_stats.sco_tx++;
		/* XXXX: We can transmit with reliable */
		sc->sc_flags |= BCSP_XMIT;
		bcsp_tx_unreliable_pkt(sc, m, BCSP_CHANNEL_HCI_SCO);
	}

	return;
}

static void
bcsp_output_cmd(device_t self, struct mbuf *m)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & BCSP_ENABLED);

	m_adj(m, sizeof(uint8_t));
	M_SETCTX(m, NULL);

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_cmdq, m);
	if ((sc->sc_flags & BCSP_XMIT) == 0)
		bcsp_start(sc);

	splx(s);
}

static void
bcsp_output_acl(device_t self, struct mbuf *m)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & BCSP_ENABLED);

	m_adj(m, sizeof(uint8_t));
	M_SETCTX(m, NULL);

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_aclq, m);
	if ((sc->sc_flags & BCSP_XMIT) == 0)
		bcsp_start(sc);

	splx(s);
}

static void
bcsp_output_sco(device_t self, struct mbuf *m)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & BCSP_ENABLED);

	m_adj(m, sizeof(uint8_t));

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_scoq, m);
	if ((sc->sc_flags & BCSP_XMIT) == 0)
		bcsp_start(sc);

	splx(s);
}

static void
bcsp_stats(device_t self, struct bt_stats *dest, int flush)
{
	struct bcsp_softc *sc = device_private(self);
	int s;

	s = spltty();
	memcpy(dest, &sc->sc_stats, sizeof(struct bt_stats));

	if (flush)
		memset(&sc->sc_stats, 0, sizeof(struct bt_stats));

	splx(s);
}


#ifdef BCSP_DEBUG
static void
bcsp_packet_print(struct mbuf *m)
{
	int i;
	uint8_t *p;

	for ( ; m != NULL; m = m->m_next) {
		p = mtod(m, uint8_t *);
		for (i = 0; i < m->m_len; i++) {
			if (i % 16 == 0)
				printf(" ");
			printf(" %02x", *(p + i));
			if (i % 16 == 15)
				printf("\n");
		}
		printf("\n");
	}
}
#endif
