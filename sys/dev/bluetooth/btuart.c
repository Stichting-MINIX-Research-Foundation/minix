/*	$NetBSD: btuart.c,v 1.28 2015/08/20 14:40:17 christos Exp $	*/

/*-
 * Copyright (c) 2006, 2007 KIYOHARA Takashi
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
__KERNEL_RCSID(0, "$NetBSD: btuart.c,v 1.28 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/syslimits.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

#include "ioconf.h"

struct btuart_softc {
	device_t	sc_dev;
	struct tty *	sc_tp;		/* tty pointer */

	bool		sc_enabled;	/* device is enabled */
	struct hci_unit *sc_unit;	/* Bluetooth HCI handle */
	struct bt_stats	sc_stats;

	int		sc_state;	/* receive state */
	int		sc_want;	/* how much we want */
	struct mbuf *	sc_rxp;		/* incoming packet */

	bool		sc_xmit;	/* transmit is active */
	struct mbuf *	sc_txp;		/* outgoing packet */

	/* transmit queues */
	MBUFQ_HEAD()	sc_cmdq;
	MBUFQ_HEAD()	sc_aclq;
	MBUFQ_HEAD()	sc_scoq;
};

/* sc_state */
#define BTUART_RECV_PKT_TYPE	0	/* packet type */
#define BTUART_RECV_ACL_HDR	1	/* acl header */
#define BTUART_RECV_SCO_HDR	2	/* sco header */
#define BTUART_RECV_EVENT_HDR	3	/* event header */
#define BTUART_RECV_ACL_DATA	4	/* acl packet data */
#define BTUART_RECV_SCO_DATA	5	/* sco packet data */
#define BTUART_RECV_EVENT_DATA	6	/* event packet data */

static int btuart_match(device_t, cfdata_t, void *);
static void btuart_attach(device_t, device_t, void *);
static int btuart_detach(device_t, int);

static int btuartopen(dev_t, struct tty *);
static int btuartclose(struct tty *, int);
static int btuartioctl(struct tty *, u_long, void *, int, struct lwp *);
static int btuartinput(int, struct tty *);
static int btuartstart(struct tty *);

static int btuart_enable(device_t);
static void btuart_disable(device_t);
static void btuart_output_cmd(device_t, struct mbuf *);
static void btuart_output_acl(device_t, struct mbuf *);
static void btuart_output_sco(device_t, struct mbuf *);
static void btuart_stats(device_t, struct bt_stats *, int);

/*
 * It doesn't need to be exported, as only btuartattach() uses it,
 * but there's no "official" way to make it static.
 */
CFATTACH_DECL_NEW(btuart, sizeof(struct btuart_softc),
    btuart_match, btuart_attach, btuart_detach, NULL);

static struct linesw btuart_disc = {
	.l_name =	"btuart",
	.l_open =	btuartopen,
	.l_close =	btuartclose,
	.l_read =	ttyerrio,
	.l_write =	ttyerrio,
	.l_ioctl =	btuartioctl,
	.l_rint =	btuartinput,
	.l_start =	btuartstart,
	.l_modem =	ttymodem,
	.l_poll =	ttyerrpoll,
};

static const struct hci_if btuart_hci = {
	.enable =	btuart_enable,
	.disable =	btuart_disable,
	.output_cmd =	btuart_output_cmd,
	.output_acl =	btuart_output_acl,
	.output_sco =	btuart_output_sco,
	.get_stats =	btuart_stats,
	.ipl =		IPL_TTY,
};

/*****************************************************************************
 *
 *	autoconf(9) functions
 */

/*
 * pseudo-device attach routine.
 */
void
btuartattach(int num __unused)
{
	int error;

	error = ttyldisc_attach(&btuart_disc);
	if (error) {
		aprint_error("%s: unable to register line discipline, "
		    "error = %d\n", btuart_cd.cd_name, error);

		return;
	}

	error = config_cfattach_attach(btuart_cd.cd_name, &btuart_ca);
	if (error) {
		aprint_error("%s: unable to register cfattach, error = %d\n",
		    btuart_cd.cd_name, error);

		config_cfdriver_detach(&btuart_cd);
		(void) ttyldisc_detach(&btuart_disc);
	}
}

/*
 * Autoconf match routine.
 */
static int
btuart_match(device_t self __unused, cfdata_t cfdata __unused,
	     void *arg __unused)
{

	/* pseudo-device; always present */
	return 1;
}

/*
 * Autoconf attach routine.
 * Called by config_attach_pseudo(9) when we open the line discipline.
 */
static void
btuart_attach(device_t parent __unused, device_t self, void *aux __unused)
{
	struct btuart_softc *sc = device_private(self);

	sc->sc_dev = self;

	MBUFQ_INIT(&sc->sc_cmdq);
	MBUFQ_INIT(&sc->sc_aclq);
	MBUFQ_INIT(&sc->sc_scoq);

	/* Attach Bluetooth unit */
	sc->sc_unit = hci_attach_pcb(&btuart_hci, self, 0);
	if (sc->sc_unit == NULL)
		aprint_error_dev(self, "HCI attach failed\n");
}

/*
 * Autoconf detach routine.
 * Called when we close the line discipline.
 */
static int
btuart_detach(device_t self, int flags __unused)
{
	struct btuart_softc *sc = device_private(self);

	btuart_disable(self);

	if (sc->sc_unit) {
		hci_detach_pcb(sc->sc_unit);
		sc->sc_unit = NULL;
	}

	return 0;
}

/*****************************************************************************
 *
 *	Line discipline functions.
 */

static int
btuartopen(dev_t devno __unused, struct tty *tp)
{
	struct btuart_softc *sc;
	device_t dev;
	cfdata_t cfdata;
	struct lwp *l = curlwp;		/* XXX */
	int error, unit, s;

	error = kauth_authorize_device(l->l_cred, KAUTH_DEVICE_BLUETOOTH_BTUART,
	    KAUTH_ARG(KAUTH_REQ_DEVICE_BLUETOOTH_BTUART_ADD), NULL, NULL, NULL);
	if (error)
		return (error);

	s = spltty();

	if (tp->t_linesw == &btuart_disc) {
		sc = tp->t_sc;
		if (sc != NULL) {
			splx(s);
			return EBUSY;
		}
	}

	KASSERT(tp->t_oproc != NULL);

	cfdata = malloc(sizeof(struct cfdata), M_DEVBUF, M_WAITOK);
	for (unit = 0; unit < btuart_cd.cd_ndevs; unit++)
		if (device_lookup(&btuart_cd, unit) == NULL)
			break;

	cfdata->cf_name = btuart_cd.cd_name;
	cfdata->cf_atname = btuart_cd.cd_name;
	cfdata->cf_unit = unit;
	cfdata->cf_fstate = FSTATE_STAR;

	dev = config_attach_pseudo(cfdata);
	if (dev == NULL) {
		free(cfdata, M_DEVBUF);
		splx(s);
		return EIO;
	}
	sc = device_private(dev);

	aprint_normal_dev(dev, "major %llu minor %llu\n",
	    (unsigned long long)major(tp->t_dev),
	    (unsigned long long)minor(tp->t_dev));

	sc->sc_tp = tp;
	tp->t_sc = sc;

	mutex_spin_enter(&tty_lock);
	ttyflush(tp, FREAD | FWRITE);
	mutex_spin_exit(&tty_lock);

	splx(s);

	return 0;
}

static int
btuartclose(struct tty *tp, int flag __unused)
{
	struct btuart_softc *sc = tp->t_sc;
	cfdata_t cfdata;
	int s;

	s = spltty();

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

static int
btuartioctl(struct tty *tp, u_long cmd, void *data __unused,
    int flag __unused, struct lwp *l __unused)
{
	struct btuart_softc *sc = tp->t_sc;
	int error;

	if (sc == NULL || tp != sc->sc_tp)
		return EPASSTHROUGH;

	switch(cmd) {
	default:
		error = EPASSTHROUGH;
		break;
	}

	return error;
}

static int
btuartinput(int c, struct tty *tp)
{
	struct btuart_softc *sc = tp->t_sc;
	struct mbuf *m = sc->sc_rxp;
	int space = 0;

	if (!sc->sc_enabled)
		return 0;

	c &= TTY_CHARMASK;

	/* If we already started a packet, find the trailing end of it. */
	if (m) {
		while (m->m_next)
			m = m->m_next;

		space = M_TRAILINGSPACE(m);
	}

	if (space == 0) {
		if (m == NULL) {
			/* new packet */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_dev(sc->sc_dev, "out of memory\n");
				sc->sc_stats.err_rx++;
				return 0;	/* (lost sync) */
			}

			sc->sc_rxp = m;
			m->m_pkthdr.len = m->m_len = 0;
			space = MHLEN;

			sc->sc_state = BTUART_RECV_PKT_TYPE;
			sc->sc_want = 1;
		} else {
			/* extend mbuf */
			MGET(m->m_next, M_DONTWAIT, MT_DATA);
			if (m->m_next == NULL) {
				aprint_error_dev(sc->sc_dev, "out of memory\n");
				sc->sc_stats.err_rx++;
				return 0;	/* (lost sync) */
			}

			m = m->m_next;
			m->m_len = 0;
			space = MLEN;

			if (sc->sc_want > MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (m->m_flags & M_EXT)
					space = MCLBYTES;
			}
		}
	}

	mtod(m, uint8_t *)[m->m_len++] = c;
	sc->sc_rxp->m_pkthdr.len++;
	sc->sc_stats.byte_rx++;

	sc->sc_want--;
	if (sc->sc_want > 0)
		return 0;	/* want more */

	switch (sc->sc_state) {
	case BTUART_RECV_PKT_TYPE:	/* Got packet type */

		switch (c) {
		case HCI_ACL_DATA_PKT:
			sc->sc_state = BTUART_RECV_ACL_HDR;
			sc->sc_want = sizeof(hci_acldata_hdr_t) - 1;
			break;

		case HCI_SCO_DATA_PKT:
			sc->sc_state = BTUART_RECV_SCO_HDR;
			sc->sc_want = sizeof(hci_scodata_hdr_t) - 1;
			break;

		case HCI_EVENT_PKT:
			sc->sc_state = BTUART_RECV_EVENT_HDR;
			sc->sc_want = sizeof(hci_event_hdr_t) - 1;
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "Unknown packet type=%#x!\n", c);
			sc->sc_stats.err_rx++;
			m_freem(sc->sc_rxp);
			sc->sc_rxp = NULL;
			return 0;	/* (lost sync) */
		}

		break;

	/*
	 * we assume (correctly of course :) that the packet headers all fit
	 * into a single pkthdr mbuf
	 */
	case BTUART_RECV_ACL_HDR:	/* Got ACL Header */
		sc->sc_state = BTUART_RECV_ACL_DATA;
		sc->sc_want = mtod(m, hci_acldata_hdr_t *)->length;
		sc->sc_want = le16toh(sc->sc_want);
		break;

	case BTUART_RECV_SCO_HDR:	/* Got SCO Header */
		sc->sc_state = BTUART_RECV_SCO_DATA;
		sc->sc_want =  mtod(m, hci_scodata_hdr_t *)->length;
		break;

	case BTUART_RECV_EVENT_HDR:	/* Got Event Header */
		sc->sc_state = BTUART_RECV_EVENT_DATA;
		sc->sc_want =  mtod(m, hci_event_hdr_t *)->length;
		break;

	case BTUART_RECV_ACL_DATA:	/* ACL Packet Complete */
		if (!hci_input_acl(sc->sc_unit, sc->sc_rxp))
			sc->sc_stats.err_rx++;

		sc->sc_stats.acl_rx++;
		sc->sc_rxp = m = NULL;
		break;

	case BTUART_RECV_SCO_DATA:	/* SCO Packet Complete */
		if (!hci_input_sco(sc->sc_unit, sc->sc_rxp))
			sc->sc_stats.err_rx++;

		sc->sc_stats.sco_rx++;
		sc->sc_rxp = m = NULL;
		break;

	case BTUART_RECV_EVENT_DATA:	/* Event Packet Complete */
		if (!hci_input_event(sc->sc_unit, sc->sc_rxp))
			sc->sc_stats.err_rx++;

		sc->sc_stats.evt_rx++;
		sc->sc_rxp = m = NULL;
		break;

	default:
		panic("%s: invalid state %d!\n",
		    device_xname(sc->sc_dev), sc->sc_state);
	}

	return 0;
}

static int
btuartstart(struct tty *tp)
{
	struct btuart_softc *sc = tp->t_sc;
	struct mbuf *m;
	int count, rlen;
	uint8_t *rptr;

	if (!sc->sc_enabled)
		return 0;

	m = sc->sc_txp;
	if (m == NULL) {
		if (MBUFQ_FIRST(&sc->sc_cmdq)) {
			MBUFQ_DEQUEUE(&sc->sc_cmdq, m);
			sc->sc_stats.cmd_tx++;
		} else if (MBUFQ_FIRST(&sc->sc_scoq)) {
			MBUFQ_DEQUEUE(&sc->sc_scoq, m);
			sc->sc_stats.sco_tx++;
		} else if (MBUFQ_FIRST(&sc->sc_aclq)) {
			MBUFQ_DEQUEUE(&sc->sc_aclq, m);
			sc->sc_stats.acl_tx++;
		} else {
			sc->sc_xmit = false;
			return 0; /* no more to send */
		}

		sc->sc_txp = m;
		sc->sc_xmit = true;
	}

	count = 0;
	rlen = 0;
	rptr = mtod(m, uint8_t *);

	for(;;) {
		if (rlen >= m->m_len) {
			m = m->m_next;
			if (m == NULL) {
				m = sc->sc_txp;
				sc->sc_txp = NULL;

				if (M_GETCTX(m, void *) == NULL)
					m_freem(m);
				else if (!hci_complete_sco(sc->sc_unit, m))
					sc->sc_stats.err_tx++;

				break;
			}

			rlen = 0;
			rptr = mtod(m, uint8_t *);
			continue;
		}

		if (putc(*rptr++, &tp->t_outq) < 0) {
			m_adj(m, rlen);
			break;
		}
		rlen++;
		count++;
	}

	sc->sc_stats.byte_tx += count;

	if (tp->t_outq.c_cc != 0)
		(*tp->t_oproc)(tp);

	return 0;
}

/*****************************************************************************
 *
 *	bluetooth(9) functions
 */

static int
btuart_enable(device_t self)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	if (sc->sc_enabled)
		return 0;

	s = spltty();

	sc->sc_enabled = true;
	sc->sc_xmit = false;

	splx(s);

	return 0;
}

static void
btuart_disable(device_t self)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	if (!sc->sc_enabled)
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

	sc->sc_enabled = false;

	splx(s);
}

static void
btuart_output_cmd(device_t self, struct mbuf *m)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	M_SETCTX(m, NULL);

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_cmdq, m);
	if (!sc->sc_xmit)
		btuartstart(sc->sc_tp);

	splx(s);
}

static void
btuart_output_acl(device_t self, struct mbuf *m)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	M_SETCTX(m, NULL);

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_aclq, m);
	if (!sc->sc_xmit)
		btuartstart(sc->sc_tp);

	splx(s);
}

static void
btuart_output_sco(device_t self, struct mbuf *m)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_enabled);

	s = spltty();
	MBUFQ_ENQUEUE(&sc->sc_scoq, m);
	if (!sc->sc_xmit)
		btuartstart(sc->sc_tp);

	splx(s);
}

static void
btuart_stats(device_t self, struct bt_stats *dest, int flush)
{
	struct btuart_softc *sc = device_private(self);
	int s;

	s = spltty();

	memcpy(dest, &sc->sc_stats, sizeof(struct bt_stats));

	if (flush)
		memset(&sc->sc_stats, 0, sizeof(struct bt_stats));

	splx(s);
}
