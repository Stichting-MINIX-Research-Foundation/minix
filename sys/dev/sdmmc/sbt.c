/*	$NetBSD: sbt.c,v 1.4 2014/05/20 18:25:54 rmind Exp $	*/
/*	$OpenBSD: sbt.c,v 1.9 2007/06/19 07:59:57 uwe Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Driver for Type-A/B SDIO Bluetooth cards */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sbt.c,v 1.4 2014/05/20 18:25:54 rmind Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <netbt/hci.h>

#include <dev/sdmmc/sdmmcdevs.h>
#include <dev/sdmmc/sdmmcvar.h>

#define CSR_READ_1(sc, reg)       sdmmc_io_read_1((sc)->sc_sf, (reg))
#define CSR_WRITE_1(sc, reg, val) sdmmc_io_write_1((sc)->sc_sf, (reg), (val))

#define SBT_REG_DAT	0x00		/* receiver/transmitter data */
#define SBT_REG_RPC	0x10		/* read packet control */
#define  RPC_PCRRT	(1<<0)		/* packet read retry */
#define SBT_REG_WPC	0x11		/* write packet control */
#define  WPC_PCWRT	(1<<0)		/* packet write retry */
#define SBT_REG_RC	0x12		/* retry control status/set */
#define SBT_REG_ISTAT	0x13		/* interrupt status */
#define  ISTAT_INTRD	(1<<0)		/* packet available for read */
#define SBT_REG_ICLR	0x13		/* interrupt clear */
#define SBT_REG_IENA	0x14		/* interrupt enable */
#define SBT_REG_BTMODE	0x20		/* SDIO Bluetooth card mode */
#define  BTMODE_TYPEB	(1<<0)		/* 1=Type-B, 0=Type-A */

#define SBT_PKT_BUFSIZ	65540
#define SBT_RXTRY_MAX	5

struct sbt_softc {
	device_t sc_dev;		/* base device */
	int sc_flags;
	struct hci_unit *sc_unit;	/* Bluetooth HCI Unit */
	struct bt_stats sc_stats;
	struct sdmmc_function *sc_sf;	/* SDIO function */
	int sc_dying;			/* shutdown in progress */
	void *sc_ih;
	u_char *sc_buf;
	int sc_rxtry;

	/* transmit queues */
	MBUFQ_HEAD() sc_cmdq;
	MBUFQ_HEAD() sc_aclq;
	MBUFQ_HEAD() sc_scoq;
};

/* sc_flags */
#define SBT_XMIT	(1 << 0)	/* transmit is active */
#define SBT_ENABLED	(1 << 1)	/* device is enabled */

static int	sbt_match(device_t, cfdata_t, void *);
static void	sbt_attach(device_t, device_t, void *);
static int	sbt_detach(device_t, int);

CFATTACH_DECL_NEW(sbt, sizeof(struct sbt_softc),
    sbt_match, sbt_attach, sbt_detach, NULL);

static int	sbt_write_packet(struct sbt_softc *, u_char *, size_t);
static int	sbt_read_packet(struct sbt_softc *, u_char *, size_t *);
static void	sbt_start(struct sbt_softc *);

static int	sbt_intr(void *);

static int	sbt_enable(device_t);
static void	sbt_disable(device_t);
static void	sbt_start_cmd(device_t, struct mbuf *);
static void	sbt_start_acl(device_t, struct mbuf *);
static void	sbt_start_sco(device_t, struct mbuf *);
static void	sbt_stats(device_t, struct bt_stats *, int);

#undef DPRINTF	/* avoid redefine by bluetooth.h */
#ifdef SBT_DEBUG
int sbt_debug = 1;
#define DPRINTF(s)	printf s
#define DNPRINTF(n, s)	do { if ((n) <= sbt_debug) printf s; } while (0)
#else
#define DPRINTF(s)	do {} while (0)
#define DNPRINTF(n, s)	do {} while (0)
#endif

#define DEVNAME(sc)	device_xname((sc)->sc_dev)


/*
 * Autoconf glue
 */

static const struct sbt_product {
	uint16_t	sp_vendor;
	uint16_t	sp_product;
	const char	*sp_cisinfo[4];
} sbt_products[] = {
	{
		SDMMC_VENDOR_SOCKETCOM,
		SDMMC_PRODUCT_SOCKETCOM_BTCARD,
		SDMMC_CIS_SOCKETCOM_BTCARD
	},
};

static const struct hci_if sbt_hci = {
	.enable = sbt_enable,
	.disable = sbt_disable,
	.output_cmd = sbt_start_cmd,
	.output_acl = sbt_start_acl,
	.output_sco = sbt_start_sco,
	.get_stats = sbt_stats,
	.ipl = IPL_TTY,			/* XXX */
};


static int
sbt_match(device_t parent, cfdata_t match, void *aux)
{
	struct sdmmc_attach_args *sa = aux;
	const struct sbt_product *sp;
	struct sdmmc_function *sf;
	int i;

	if (sa->sf == NULL)
		return 0;	/* not SDIO */

	sf = sa->sf->sc->sc_fn0;
	sp = &sbt_products[0];

	for (i = 0; i < sizeof(sbt_products) / sizeof(sbt_products[0]);
	     i++, sp = &sbt_products[i])
		if (sp->sp_vendor == sf->cis.manufacturer &&
		    sp->sp_product == sf->cis.product)
			return 1;
	return 0;
}

static void
sbt_attach(device_t parent, device_t self, void *aux)
{
	struct sbt_softc *sc = device_private(self);
	struct sdmmc_attach_args *sa = aux;

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_dev = self;
	sc->sc_sf = sa->sf;
	MBUFQ_INIT(&sc->sc_cmdq);
	MBUFQ_INIT(&sc->sc_aclq);
	MBUFQ_INIT(&sc->sc_scoq);

	(void)sdmmc_io_function_disable(sc->sc_sf);
	if (sdmmc_io_function_enable(sc->sc_sf)) {
		printf("%s: function not ready\n", DEVNAME(sc));
		return;
	}

	/* It may be Type-B, but we use it only in Type-A mode. */
	printf("%s: SDIO Bluetooth Type-A\n", DEVNAME(sc));

	sc->sc_buf = malloc(SBT_PKT_BUFSIZ, M_DEVBUF, M_NOWAIT | M_CANFAIL);
	if (sc->sc_buf == NULL) {
		printf("%s: can't allocate cmd buffer\n", DEVNAME(sc));
		return;
	}

	/* Enable the HCI packet transport read interrupt. */
	CSR_WRITE_1(sc, SBT_REG_IENA, ISTAT_INTRD);

	/* Enable the card interrupt for this function. */
	sc->sc_ih = sdmmc_intr_establish(parent, sbt_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf("%s: can't establish interrupt\n", DEVNAME(sc));
		return;
	}
	sdmmc_intr_enable(sc->sc_sf);

	/*
	 * Attach Bluetooth unit (machine-independent HCI).
	 */
	sc->sc_unit = hci_attach_pcb(&sbt_hci, self, 0);
}

static int
sbt_detach(device_t self, int flags)
{
	struct sbt_softc *sc = device_private(self);

	sc->sc_dying = 1;

	if (sc->sc_unit) {
		hci_detach_pcb(sc->sc_unit);
		sc->sc_unit = NULL;
	}

	if (sc->sc_ih != NULL)
		sdmmc_intr_disestablish(sc->sc_ih);

	return 0;
}


/*
 * Bluetooth HCI packet transport
 */

static int
sbt_write_packet(struct sbt_softc *sc, u_char *buf, size_t len)
{
	u_char hdr[3];
	size_t pktlen;
	int error = EIO;
	int retry = 3;

again:
	if (retry-- == 0) {
		DPRINTF(("%s: sbt_write_cmd: giving up\n", DEVNAME(sc)));
		return error;
	}

	/* Restart the current packet. */
	sdmmc_io_write_1(sc->sc_sf, SBT_REG_WPC, WPC_PCWRT);

	/* Write the packet length. */
	pktlen = len + 3;
	hdr[0] = pktlen & 0xff;
	hdr[1] = (pktlen >> 8) & 0xff;
	hdr[2] = (pktlen >> 16) & 0xff;
	error = sdmmc_io_write_multi_1(sc->sc_sf, SBT_REG_DAT, hdr, 3);
	if (error) {
		DPRINTF(("%s: sbt_write_packet: failed to send length\n",
		    DEVNAME(sc)));
		goto again;
	}

	error = sdmmc_io_write_multi_1(sc->sc_sf, SBT_REG_DAT, buf, len);
	if (error) {
		DPRINTF(("%s: sbt_write_packet: failed to send packet data\n",
		    DEVNAME(sc)));
		goto again;
	}
	return 0;
}

static int
sbt_read_packet(struct sbt_softc *sc, u_char *buf, size_t *lenp)
{
	u_char hdr[3];
	size_t len;
	int error;

	error = sdmmc_io_read_multi_1(sc->sc_sf, SBT_REG_DAT, hdr, 3);
	if (error) {
		DPRINTF(("%s: sbt_read_packet: failed to read length\n",
		    DEVNAME(sc)));
		goto out;
	}
	len = (hdr[0] | (hdr[1] << 8) | (hdr[2] << 16)) - 3;
	if (len > *lenp) {
		DPRINTF(("%s: sbt_read_packet: len %u > %u\n",
		    DEVNAME(sc), len, *lenp));
		error = ENOBUFS;
		goto out;
	}

	DNPRINTF(2,("%s: sbt_read_packet: reading len %u bytes\n",
	    DEVNAME(sc), len));
	error = sdmmc_io_read_multi_1(sc->sc_sf, SBT_REG_DAT, buf, len);
	if (error) {
		DPRINTF(("%s: sbt_read_packet: failed to read packet data\n",
		    DEVNAME(sc)));
		goto out;
	}

out:
	if (error) {
		if (sc->sc_rxtry >= SBT_RXTRY_MAX) {
			/* Drop and request the next packet. */
			sc->sc_rxtry = 0;
			CSR_WRITE_1(sc, SBT_REG_RPC, 0);
		} else {
			/* Request the current packet again. */
			sc->sc_rxtry++;
			CSR_WRITE_1(sc, SBT_REG_RPC, RPC_PCRRT);
		}
		return error;
	}

	/* acknowledge read packet */
	CSR_WRITE_1(sc, SBT_REG_RPC, 0);

	*lenp = len;
	return 0;
}

/*
 * Interrupt handling
 */

static int
sbt_intr(void *arg)
{
	struct sbt_softc *sc = arg;
	struct mbuf *m = NULL;
	u_int8_t status;
	size_t len;
	int s;

	s = splsdmmc();

	status = CSR_READ_1(sc, SBT_REG_ISTAT);
	CSR_WRITE_1(sc, SBT_REG_ICLR, status);

	if ((status & ISTAT_INTRD) == 0)
		return 0;	/* shared SDIO card interrupt? */

	len = SBT_PKT_BUFSIZ;
	if (sbt_read_packet(sc, sc->sc_buf, &len) != 0 || len == 0) {
		DPRINTF(("%s: sbt_intr: read failed\n", DEVNAME(sc)));
		goto eoi;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		DPRINTF(("%s: sbt_intr: MGETHDR failed\n", DEVNAME(sc)));
		goto eoi;
	}

	m->m_pkthdr.len = m->m_len = MHLEN;
	m_copyback(m, 0, len, sc->sc_buf);
	if (m->m_pkthdr.len == MAX(MHLEN, len)) {
		m->m_pkthdr.len = len;
		m->m_len = MIN(MHLEN, m->m_pkthdr.len);
	} else {
		DPRINTF(("%s: sbt_intr: m_copyback failed\n", DEVNAME(sc)));
		m_free(m);
		m = NULL;
	}

eoi:
	if (m != NULL) {
		switch (sc->sc_buf[0]) {
		case HCI_ACL_DATA_PKT:
			DNPRINTF(1,("%s: recv ACL packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_acl(sc->sc_unit, m);
			break;
		case HCI_SCO_DATA_PKT:
			DNPRINTF(1,("%s: recv SCO packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_sco(sc->sc_unit, m);
			break;
		case HCI_EVENT_PKT:
			DNPRINTF(1,("%s: recv EVENT packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_event(sc->sc_unit, m);
			break;
		default:
			DPRINTF(("%s: recv 0x%x packet (%d bytes)\n",
			    DEVNAME(sc), sc->sc_buf[0], m->m_pkthdr.len));
			sc->sc_stats.err_rx++;
			m_free(m);
			break;
		}
	} else
		sc->sc_stats.err_rx++;

	splx(s);

	/* Claim this interrupt. */
	return 1;
}


/*
 * Bluetooth HCI unit functions
 */

static int
sbt_enable(device_t self)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	if (sc->sc_flags & SBT_ENABLED)
		return 0;

	s = spltty();

	sc->sc_flags |= SBT_ENABLED;
	sc->sc_flags &= ~SBT_XMIT;

	splx(s);

	return 0;
}

static void
sbt_disable(device_t self)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	if (!(sc->sc_flags & SBT_ENABLED))
		return;

	s = spltty();

#ifdef notyet			/* XXX */
	if (sc->sc_rxp) {
		m_freem(sc->sc_rxp);
		sc->sc_rxp = NULL;
	}

	if (sc->sc_txp) {
		m_freem(sc->sc_txp);
		sc->sc_txp = NULL;
	}
#endif

	MBUFQ_DRAIN(&sc->sc_cmdq);
	MBUFQ_DRAIN(&sc->sc_aclq);
	MBUFQ_DRAIN(&sc->sc_scoq);

	sc->sc_flags &= ~SBT_ENABLED;

	splx(s);
}

static void
sbt_start(struct sbt_softc *sc)
{
	struct mbuf *m;
	int len;
#ifdef SBT_DEBUG
	const char *what;
#endif

	KASSERT((sc->sc_flags & SBT_XMIT) == 0);

	if (sc->sc_dying)
		return;

	if (MBUFQ_FIRST(&sc->sc_cmdq)) {
		MBUFQ_DEQUEUE(&sc->sc_cmdq, m);
		sc->sc_stats.cmd_tx++;
#ifdef SBT_DEBUG
		what = "CMD";
#endif
		goto start;
	}

	if (MBUFQ_FIRST(&sc->sc_scoq)) {
		MBUFQ_DEQUEUE(&sc->sc_scoq, m);
		sc->sc_stats.sco_tx++;
#ifdef SBT_DEBUG
		what = "SCO";
#endif
		goto start;
	}

	if (MBUFQ_FIRST(&sc->sc_aclq)) {
		MBUFQ_DEQUEUE(&sc->sc_aclq, m);
		sc->sc_stats.acl_tx++;
#ifdef SBT_DEBUG
		what = "ACL";
#endif
		goto start;
	}

	/* Nothing to send */
	return;

start:
	DNPRINTF(1,("%s: xmit %s packet (%d bytes)\n", DEVNAME(sc),
	    what, m->m_pkthdr.len));

	sc->sc_flags |= SBT_XMIT;

	len = m->m_pkthdr.len;
	m_copydata(m, 0, len, sc->sc_buf);
	m_freem(m);

	if (sbt_write_packet(sc, sc->sc_buf, len))
		DPRINTF(("%s: sbt_write_packet failed\n", DEVNAME(sc)));

	sc->sc_flags &= ~SBT_XMIT;
}

static void
sbt_start_cmd(device_t self, struct mbuf *m)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & SBT_ENABLED);

	M_SETCTX(m, NULL);

	s = spltty();

	MBUFQ_ENQUEUE(&sc->sc_cmdq, m);
	if ((sc->sc_flags & SBT_XMIT) == 0)
		sbt_start(sc);

	splx(s);
}

static void
sbt_start_acl(device_t self, struct mbuf *m)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & SBT_ENABLED);

	M_SETCTX(m, NULL);

	s = spltty();

	MBUFQ_ENQUEUE(&sc->sc_aclq, m);
	if ((sc->sc_flags & SBT_XMIT) == 0)
		sbt_start(sc);

	splx(s);
}

static void
sbt_start_sco(device_t self, struct mbuf *m)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_flags & SBT_ENABLED);

	s = spltty();

	MBUFQ_ENQUEUE(&sc->sc_scoq, m);
	if ((sc->sc_flags & SBT_XMIT) == 0)
		sbt_start(sc);

	splx(s);
}

static void
sbt_stats(device_t self, struct bt_stats *dest, int flush)
{
	struct sbt_softc *sc = device_private(self);
	int s;

	s = spltty();

	memcpy(dest, &sc->sc_stats, sizeof(struct bt_stats));

	if (flush)
		memset(&sc->sc_stats, 0, sizeof(struct bt_stats));

	splx(s);
}
