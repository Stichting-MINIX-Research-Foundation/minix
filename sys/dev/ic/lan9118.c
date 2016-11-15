/*	$NetBSD: lan9118.c,v 1.20 2015/04/20 12:41:38 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
__KERNEL_RCSID(0, "$NetBSD: lan9118.c,v 1.20 2015/04/20 12:41:38 kiyohara Exp $");

/*
 * The LAN9118 Family
 * * The LAN9118 is targeted for 32-bit applications requiring high
 *   performance, and provides the highest level of performance possible for
 *   a non-PCI 10/100 Ethernet controller.
 *
 * * The LAN9117 is designed to provide the highest level of performance
 *   possible for 16-bit applications. It also has an external MII interface,
 *   which can be used to attach an external PHY.
 *
 * * The LAN9116 and LAN9115 are designed for performance-sensitive
 *   applications with less intensive performance requirements. The LAN9116
 *   is for 32-bit host processors, while the LAN9115 is for 16-bit
 *   applications, which may also require an external PHY. Both devices
 *   deliver superior levels of performance.
 *
 * The LAN9218 Family
 *   Also support HP Auto-MDIX.
 */

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <net/bpf.h>
#include <sys/rndsource.h>

#include <dev/ic/lan9118reg.h>
#include <dev/ic/lan9118var.h>


#ifdef SMSH_DEBUG
#define DPRINTF(x)	if (smsh_debug) printf x
#define DPRINTFN(n,x)	if (smsh_debug >= (n)) printf x
int smsh_debug = SMSH_DEBUG;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


static void lan9118_start(struct ifnet *);
static int lan9118_ioctl(struct ifnet *, u_long, void *);
static int lan9118_init(struct ifnet *);
static void lan9118_stop(struct ifnet *, int);
static void lan9118_watchdog(struct ifnet *);

static int lan9118_ifm_change(struct ifnet *);
static void lan9118_ifm_status(struct ifnet *, struct ifmediareq *);

static int lan9118_miibus_readreg(device_t, int, int);
static void lan9118_miibus_writereg(device_t, int, int, int);
static void lan9118_miibus_statchg(struct ifnet *);

static uint16_t lan9118_mii_readreg(struct lan9118_softc *, int, int);
static void lan9118_mii_writereg(struct lan9118_softc *, int, int, uint16_t);
static uint32_t lan9118_mac_readreg(struct lan9118_softc *, int);
static void lan9118_mac_writereg(struct lan9118_softc *, int, uint32_t);

static void lan9118_set_filter(struct lan9118_softc *);
static void lan9118_rxintr(struct lan9118_softc *);
static void lan9118_txintr(struct lan9118_softc *);

static void lan9118_tick(void *);

/* This values refer from Linux's smc911x.c */
static uint32_t afc_cfg[] = {
	/* 0 */ 0x00000000,
	/* 1 */ 0x00000000,
	/* 2 */ 0x008c4600 | LAN9118_AFC_CFG_BACK_DUR(10) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 3 */ 0x00824100 | LAN9118_AFC_CFG_BACK_DUR(9) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 4 */ 0x00783c00 | LAN9118_AFC_CFG_BACK_DUR(9) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 5 */ 0x006e3700 | LAN9118_AFC_CFG_BACK_DUR(8) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 6 */ 0x00643200 | LAN9118_AFC_CFG_BACK_DUR(8) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 7 */ 0x005a2d00 | LAN9118_AFC_CFG_BACK_DUR(7) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 8 */ 0x00502800 | LAN9118_AFC_CFG_BACK_DUR(7) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* 9 */ 0x00462300 | LAN9118_AFC_CFG_BACK_DUR(6) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* a */ 0x003c1e00 | LAN9118_AFC_CFG_BACK_DUR(6) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* b */ 0x00321900 | LAN9118_AFC_CFG_BACK_DUR(5) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* c */ 0x00241200 | LAN9118_AFC_CFG_BACK_DUR(4) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* d */ 0x00150700 | LAN9118_AFC_CFG_BACK_DUR(3) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* e */ 0x00060300 | LAN9118_AFC_CFG_BACK_DUR(2) |
		LAN9118_AFC_CFG_FCMULT | LAN9118_AFC_CFG_FCBRD |
		LAN9118_AFC_CFG_FCADD | LAN9118_AFC_CFG_FCANY,
	/* f */ 0x00000000,
};


int
lan9118_attach(struct lan9118_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint32_t val;
	int timo, i;

	if (sc->sc_flags & LAN9118_FLAGS_SWAP)
		/* byte swap mode */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_WORD_SWAP,
		    0xffffffff);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_BYTE_TEST);
	if (val != LAN9118_BYTE_TEST_VALUE) {
		aprint_error(": failed to detect chip\n");
		return EINVAL;
	}

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_ID_REV);
	sc->sc_id = LAN9118_ID_REV_ID(val);
	sc->sc_rev = LAN9118_ID_REV_REV(val);

#define LAN9xxx_ID(id)	\
 (IS_LAN9118(id) ? (id) : (IS_LAN9218(id) ? ((id) >> 4) + 0x100 : (id) & 0xfff))

	aprint_normal(": SMSC LAN9%03x Rev %d\n",
	    LAN9xxx_ID(sc->sc_id), sc->sc_rev);

	if (sc->sc_flags & LAN9118_FLAGS_SWAP)
		aprint_normal_dev(sc->sc_dev, "byte swap mode\n");

	timo = 3 * 1000 * 1000;	/* XXXX 3sec */
	do {
		val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_MAC_CSR_CMD);
		if (!(val & LAN9118_MAC_CSR_CMD_BUSY))
			break;
		delay(100);
	} while (timo -= 100);
	if (timo <= 0)
		aprint_error_dev(sc->sc_dev, "%s: command busy\n", __func__);
	if (!(sc->sc_flags & LAN9118_FLAGS_NO_EEPROM)) {
		/* Read auto-loaded MAC address */
		val = lan9118_mac_readreg(sc, LAN9118_ADDRL);
		sc->sc_enaddr[3] = (val >> 24) & 0xff;
		sc->sc_enaddr[2] = (val >> 16) & 0xff;
		sc->sc_enaddr[1] = (val >> 8) & 0xff;
		sc->sc_enaddr[0] = val & 0xff;
		val = lan9118_mac_readreg(sc, LAN9118_ADDRH);
		sc->sc_enaddr[5] = (val >> 8) & 0xff;
		sc->sc_enaddr[4] = val & 0xff;
	}
	aprint_normal_dev(sc->sc_dev, "MAC address %s\n",
	    ether_sprintf(sc->sc_enaddr));

	KASSERT(LAN9118_TX_FIF_SZ >= 2 && LAN9118_TX_FIF_SZ < 15);
	sc->sc_afc_cfg = afc_cfg[LAN9118_TX_FIF_SZ];

	/* Initialize the ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = lan9118_start;
	ifp->if_ioctl = lan9118_ioctl;
	ifp->if_init = lan9118_init;
	ifp->if_stop = lan9118_stop;
	ifp->if_watchdog = lan9118_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

#if 0	/* Not support 802.1Q VLAN-sized frames yet. */
	sc->sc_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;
#endif

	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    lan9118_ifm_change, lan9118_ifm_status);
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = lan9118_miibus_readreg;
	sc->sc_mii.mii_writereg = lan9118_miibus_writereg;
	sc->sc_mii.mii_statchg = lan9118_miibus_statchg;

	/*
	 * Number of instance of Internal PHY is always 0.  External PHY
	 * number that above.
	 */
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, 1, MII_OFFSET_ANY, 0);

	if (sc->sc_id == LAN9118_ID_9115 || sc->sc_id == LAN9118_ID_9117 ||
	    sc->sc_id == LAN9218_ID_9215 || sc->sc_id == LAN9218_ID_9217) {
		if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG) &
		    LAN9118_HW_CFG_EXT_PHY_DET) {
			/*
			 * We always have a internal PHY at phy1.
			 * In addition, external PHY is attached.
			 */
			DPRINTFN(1, ("%s: detect External PHY\n", __func__));

			/* Switch MII and SMI */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    LAN9118_HW_CFG,
			    LAN9118_HW_CFG_MBO |
			    LAN9118_HW_CFG_PHY_CLK_SEL_CD);
			delay(1);	/* Wait 5 cycle */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    LAN9118_HW_CFG,
			    LAN9118_HW_CFG_MBO |
			    LAN9118_HW_CFG_PHY_CLK_SEL_EMII |
			    LAN9118_HW_CFG_SMI_SEL |
			    LAN9118_HW_CFG_EXT_PHY_EN);
			delay(1);	/* Once wait more 5 cycle */

			/* Call mii_attach, avoid at phy1. */
			mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
			    0, MII_OFFSET_ANY, 0);
			for (i = 2; i < MII_NPHY; i++)
				mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
				    i, MII_OFFSET_ANY, 0);
		}
	}

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	callout_init(&sc->sc_tick, 0);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);
	return 0;
}

int
lan9118_intr(void *arg)
{
	struct lan9118_softc *sc = (struct lan9118_softc *)arg;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint32_t int_sts, int_en, datum = 0;
	int handled = 0;

	for (;;) {
		int_sts =
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_STS);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_STS,
		    int_sts);
		int_en =
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_EN);

		DPRINTFN(3, ("%s: int_sts=0x%x, int_en=0x%x\n",
		    __func__, int_sts, int_en));

		if (!(int_sts & int_en))
			break;
		datum = int_sts;

#if 0	/* not yet... */
		if (int_sts & LAN9118_INT_PHY_INT) { /* PHY */
			/* Shall we need? */
		}
		if (int_sts & LAN9118_INT_PME_INT) { /*Power Management Event*/
			/* not yet... */
		}
#endif
		if (int_sts & LAN9118_INT_RXE) {
			ifp->if_ierrors++;
			aprint_error_ifnet(ifp, "Receive Error\n");
		}
		if (int_sts & LAN9118_INT_TSFL) /* TX Status FIFO Level */
			lan9118_txintr(sc);
		if (int_sts & LAN9118_INT_RXDF_INT) {
			ifp->if_ierrors++;
			aprint_error_ifnet(ifp, "RX Dropped Frame Interrupt\n");
		}
		if (int_sts & LAN9118_INT_RSFF) {
			ifp->if_ierrors++;
			aprint_error_ifnet(ifp, "RX Status FIFO Full\n");
		}
		if (int_sts & LAN9118_INT_RSFL) /* RX Status FIFO Level */
			 lan9118_rxintr(sc);
	}

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		lan9118_start(ifp);

	rnd_add_uint32(&sc->rnd_source, datum);

	return handled;
}


static void
lan9118_start(struct ifnet *ifp)
{
	struct lan9118_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	unsigned tdfree, totlen, dso;
	uint32_t txa, txb;
	uint8_t *p;
	int n;

	DPRINTFN(3, ("%s\n", __func__));

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	totlen = 0;
	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		tdfree = LAN9118_TX_FIFO_INF_TDFREE(bus_space_read_4(sc->sc_iot,
		    sc->sc_ioh, LAN9118_TX_FIFO_INF));
		if (tdfree < 2036) {
			/*
			 * 2036 is the possible maximum FIFO consumption
			 * for the most fragmented frame.
			 */
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/*
		 * Check mbuf chain -- "middle" buffers must be >= 4 bytes
		 * and maximum # of buffers is 86.
		 */
		m = m0;
		n = 0;
		while (m) {
			if (m->m_len < 4 || ++n > 86) {
				/* Copy mbuf chain. */
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == NULL)
					goto discard;   /* discard packet */
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					m_freem(m);
					goto discard;	/* discard packet */
				}
				m_copydata(m0, 0, m0->m_pkthdr.len,
				    mtod(m, void *));
				m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
				m_freem(m0);
				m0 = m;
				break;
			}
			m = m->m_next;
		}

		m = m0;
		totlen = m->m_pkthdr.len;
		p = mtod(m, uint8_t *);
		dso = (unsigned)p & 0x3;
		txa =
		    LAN9118_TXC_A_BEA_4B	|
		    LAN9118_TXC_A_DSO(dso)	|
		    LAN9118_TXC_A_FS		|
		    LAN9118_TXC_A_BS(m->m_len);
		txb = LAN9118_TXC_B_PL(totlen);
		while (m->m_next != NULL) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    LAN9118_TXDFIFOP, txa);
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    LAN9118_TXDFIFOP, txb);
			bus_space_write_multi_4(sc->sc_iot, sc->sc_ioh,
			    LAN9118_TXDFIFOP, (uint32_t *)(p - dso),
			    (m->m_len + dso + 3) >> 2);

			m = m->m_next;
			p = mtod(m, uint8_t *);
			dso = (unsigned)p & 0x3;
			txa =
			    LAN9118_TXC_A_BEA_4B	|
			    LAN9118_TXC_A_DSO(dso)	|
			    LAN9118_TXC_A_BS(m->m_len);
		}
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_TXDFIFOP,
		    txa | LAN9118_TXC_A_LS);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_TXDFIFOP,
		    txb);
		bus_space_write_multi_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_TXDFIFOP, (uint32_t *)(p - dso),
		    (m->m_len + dso + 3) >> 2);

discard:
		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);

		m_freem(m0);
	}
	if (totlen > 0)
		ifp->if_timer = 5;
}

static int
lan9118_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct lan9118_softc *sc = ifp->if_softc;
	struct ifreq *ifr = data;
	struct mii_data *mii = &sc->sc_mii;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFFLAGS:
		DPRINTFN(2, ("%s: IFFLAGS\n", __func__));
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			lan9118_stop(ifp, 0);
			break;
		case IFF_UP:
			lan9118_init(ifp);
			break;
		case IFF_UP|IFF_RUNNING:
			lan9118_set_filter(sc);
			break;
		default:
			break;
		}
		error = 0;
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		DPRINTFN(2, ("%s: MEDIA\n", __func__));
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;

	default:
		DPRINTFN(2, ("%s: ETHER\n", __func__));
		error = ether_ioctl(ifp, command, data);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				lan9118_set_filter(sc);
				DPRINTFN(2, ("%s set_filter called\n",
				    __func__));
			}
			error = 0;
		}
		break;
	}

	splx(s);

	return error;
}

static int
lan9118_init(struct ifnet *ifp)
{
	struct lan9118_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_mii.mii_media;
	uint32_t reg, hw_cfg, mac_cr;
	int timo, s;

	DPRINTFN(2, ("%s\n", __func__));

	s = splnet();

	/* wait for PMT_CTRL[READY] */
	timo = mstohz(5000);	/* XXXX 5sec */
	while (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_PMT_CTRL) &
	    LAN9118_PMT_CTRL_READY)) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_BYTE_TEST,
		    0xbad0c0de);
		tsleep(&sc, PRIBIO, "lan9118_pmt_ready", 1);
		if (--timo <= 0) {
			splx(s);
			return EBUSY;
		}
	}

	/* Soft Reset */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG,
	    LAN9118_HW_CFG_SRST);
	do {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG);
		if (reg & LAN9118_HW_CFG_SRST_TO) {
			aprint_error_dev(sc->sc_dev,
			    "soft reset timeouted out\n");
			splx(s);
			return ETIMEDOUT;
		}
	} while (reg & LAN9118_HW_CFG_SRST);

	/* Set MAC and PHY CSRs */

	if (sc->sc_flags & LAN9118_FLAGS_SWAP)
		/* need byte swap */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_WORD_SWAP,
		    0xffffffff);

	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_E2P_CMD) &
	    LAN9118_E2P_CMD_EPCB);
	if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_E2P_CMD) &
	    LAN9118_E2P_CMD_MACAL)) {
		lan9118_mac_writereg(sc, LAN9118_ADDRL,
		    sc->sc_enaddr[0] |
		    sc->sc_enaddr[1] << 8 |
		    sc->sc_enaddr[2] << 16 |
		    sc->sc_enaddr[3] << 24);
		lan9118_mac_writereg(sc, LAN9118_ADDRH,
		    sc->sc_enaddr[4] | sc->sc_enaddr[5] << 8);
	}

	if (ifm->ifm_media & IFM_FLOW) {
		lan9118_mac_writereg(sc, LAN9118_FLOW,
		    LAN9118_FLOW_FCPT(1) | LAN9118_FLOW_FCEN);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_AFC_CFG,
		    sc->sc_afc_cfg);
	}

	lan9118_ifm_change(ifp);
	hw_cfg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG);
	hw_cfg &= ~LAN9118_HW_CFG_TX_FIF_MASK;
	hw_cfg |= LAN9118_HW_CFG_TX_FIF_SZ(LAN9118_TX_FIF_SZ);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG, hw_cfg);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_GPIO_CFG,
	    LAN9118_GPIO_CFG_LEDX_EN(2)  |
	    LAN9118_GPIO_CFG_LEDX_EN(1)  |
	    LAN9118_GPIO_CFG_LEDX_EN(0)  |
	    LAN9118_GPIO_CFG_GPIOBUFN(2) |
	    LAN9118_GPIO_CFG_GPIOBUFN(1) |
	    LAN9118_GPIO_CFG_GPIOBUFN(0));

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_IRQ_CFG,
	    LAN9118_IRQ_CFG_IRQ_EN);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_STS,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_STS));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_FIFO_INT,
	    LAN9118_FIFO_INT_TXSL(0) | LAN9118_FIFO_INT_RXSL(0));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_EN,
#if 0	/* not yet... */
	    LAN9118_INT_PHY_INT | /* PHY */
	    LAN9118_INT_PME_INT | /* Power Management Event */
#endif
	    LAN9118_INT_RXE     | /* Receive Error */
	    LAN9118_INT_TSFL    | /* TX Status FIFO Level */
	    LAN9118_INT_RXDF_INT| /* RX Dropped Frame Interrupt */
	    LAN9118_INT_RSFF    | /* RX Status FIFO Full */
	    LAN9118_INT_RSFL);	  /* RX Status FIFO Level */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_RX_CFG,
	    LAN9118_RX_CFG_RXDOFF(2));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_TX_CFG,
	    LAN9118_TX_CFG_TX_ON);
	mac_cr = lan9118_mac_readreg(sc, LAN9118_MAC_CR);
	lan9118_mac_writereg(sc, LAN9118_MAC_CR,
	    mac_cr | LAN9118_MAC_CR_TXEN | LAN9118_MAC_CR_RXEN);

	lan9118_set_filter(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->sc_tick, hz, lan9118_tick, sc);

	splx(s);

	return 0;
}

static void
lan9118_stop(struct ifnet *ifp, int disable)
{
	struct lan9118_softc *sc = ifp->if_softc;
	uint32_t cr;

	DPRINTFN(2, ("%s\n", __func__));

	/* Disable IRQ */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_EN, 0);

	/* Stopping transmitter */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_TX_CFG,
	    LAN9118_TX_CFG_STOP_TX);
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_TX_CFG) &
	    (LAN9118_TX_CFG_TX_ON | LAN9118_TX_CFG_STOP_TX));

	/* Purge TX Status/Data FIFOs */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_TX_CFG,
	    LAN9118_TX_CFG_TXS_DUMP | LAN9118_TX_CFG_TXD_DUMP);

	/* Stopping receiver, also clear TXEN */
	cr = lan9118_mac_readreg(sc, LAN9118_MAC_CR);
	cr &= ~(LAN9118_MAC_CR_TXEN | LAN9118_MAC_CR_RXEN);
	lan9118_mac_writereg(sc, LAN9118_MAC_CR, cr);
	while (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_INT_STS) &
	    LAN9118_INT_RXSTOP_INT));

	/* Clear RX Status/Data FIFOs */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_RX_CFG,
	    LAN9118_RX_CFG_RX_DUMP);

	callout_stop(&sc->sc_tick);
}

static void
lan9118_watchdog(struct ifnet *ifp)
{
	struct lan9118_softc *sc = ifp->if_softc;

	/*
	 * Reclaim first as there is a possibility of losing Tx completion
	 * interrupts.
	 */
	lan9118_txintr(sc);

	aprint_error_ifnet(ifp, "watchdog timeout\n");
	ifp->if_oerrors++;

	lan9118_init(ifp);
}


static int
lan9118_ifm_change(struct ifnet *ifp)
{
	struct lan9118_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia *ifm = &mii->mii_media;
	struct ifmedia_entry *ife = ifm->ifm_cur;
	uint32_t pmt_ctrl;

	DPRINTFN(3, ("%s: ifm inst %d\n", __func__, IFM_INST(ife->ifm_media)));

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG,
	    LAN9118_HW_CFG_MBO | LAN9118_HW_CFG_PHY_CLK_SEL_CD);
	delay(1);	/* Wait 5 cycle */

	if (IFM_INST(ife->ifm_media) != 0) {
		/* Use External PHY */

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG,
		    LAN9118_HW_CFG_MBO			|
		    LAN9118_HW_CFG_PHY_CLK_SEL_EMII	|
		    LAN9118_HW_CFG_SMI_SEL		|
		    LAN9118_HW_CFG_EXT_PHY_EN);
		delay(1);
		return mii_mediachg(&sc->sc_mii);
	}

	/* Setup Internal PHY */

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_HW_CFG,
	    LAN9118_HW_CFG_MBO |
	    LAN9118_HW_CFG_PHY_CLK_SEL_IPHY);
	delay(1);

	/* Reset PHY */
	pmt_ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_PMT_CTRL);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_PMT_CTRL,
	    pmt_ctrl | LAN9118_PMT_CTRL_PHY_RST);
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_PMT_CTRL) &
	    LAN9118_PMT_CTRL_PHY_RST);

	mii_mediachg(&sc->sc_mii);
	return 0;
}

static void
lan9118_ifm_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct lan9118_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	DPRINTFN(3, ("%s\n", __func__));

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}


static int
lan9118_miibus_readreg(device_t dev, int phy, int reg)
{

	return lan9118_mii_readreg(device_private(dev), phy, reg);
}
static void
lan9118_miibus_writereg(device_t dev, int phy, int reg, int val)
{

	lan9118_mii_writereg(device_private(dev), phy, reg, val);
}

static void
lan9118_miibus_statchg(struct ifnet *ifp)
{
	struct lan9118_softc *sc = ifp->if_softc;
	u_int cr;

	cr = lan9118_mac_readreg(sc, LAN9118_MAC_CR);
	if (IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) {
		cr &= ~LAN9118_MAC_CR_RCVOWN;
		cr |= LAN9118_MAC_CR_FDPX;
	} else {
		cr |= LAN9118_MAC_CR_RCVOWN;
		cr &= ~LAN9118_MAC_CR_FDPX;
	}
	lan9118_mac_writereg(sc, LAN9118_MAC_CR, cr);
}


static uint16_t
lan9118_mii_readreg(struct lan9118_softc *sc, int phy, int reg)
{
	uint32_t acc;

	while (lan9118_mac_readreg(sc, LAN9118_MII_ACC) &
	    LAN9118_MII_ACC_MIIBZY);
	acc = LAN9118_MII_ACC_PHYA(phy) | LAN9118_MII_ACC_MIIRINDA(reg);
	lan9118_mac_writereg(sc, LAN9118_MII_ACC, acc);
	while (lan9118_mac_readreg(sc, LAN9118_MII_ACC) &
	    LAN9118_MII_ACC_MIIBZY);
	return lan9118_mac_readreg(sc, LAN9118_MII_DATA);
}

static void
lan9118_mii_writereg(struct lan9118_softc *sc, int phy, int reg, uint16_t val)
{
	uint32_t acc;

	while (lan9118_mac_readreg(sc, LAN9118_MII_ACC) &
	    LAN9118_MII_ACC_MIIBZY);
	acc = LAN9118_MII_ACC_PHYA(phy) | LAN9118_MII_ACC_MIIRINDA(reg) |
	    LAN9118_MII_ACC_MIIWNR;
	lan9118_mac_writereg(sc, LAN9118_MII_DATA, val);
	lan9118_mac_writereg(sc, LAN9118_MII_ACC, acc);
	while (lan9118_mac_readreg(sc, LAN9118_MII_ACC) &
	    LAN9118_MII_ACC_MIIBZY);
}

static uint32_t
lan9118_mac_readreg(struct lan9118_softc *sc, int reg)
{
	uint32_t cmd;
	int timo = 3 * 1000 * 1000;	/* XXXX: 3sec */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_MAC_CSR_CMD,
	    LAN9118_MAC_CSR_CMD_BUSY | LAN9118_MAC_CSR_CMD_R | reg);
	do {
		cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_MAC_CSR_CMD);
		if (!(cmd & LAN9118_MAC_CSR_CMD_BUSY))
			break;
		delay(100);
	} while (timo -= 100);
	if (timo <= 0)
		aprint_error_dev(sc->sc_dev, "%s: command busy\n", __func__);
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, LAN9118_MAC_CSR_DATA);
}

static void
lan9118_mac_writereg(struct lan9118_softc *sc, int reg, uint32_t val)
{
	uint32_t cmd;
	int timo = 3 * 1000 * 1000;	/* XXXX: 3sec */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_MAC_CSR_DATA, val);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_MAC_CSR_CMD,
	    LAN9118_MAC_CSR_CMD_BUSY | LAN9118_MAC_CSR_CMD_W | reg);
	do {
		cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_MAC_CSR_CMD);
		if (!(cmd & LAN9118_MAC_CSR_CMD_BUSY))
			break;
		delay(100);
	} while (timo -= 100);
	if (timo <= 0)
		aprint_error_dev(sc->sc_dev, "%s: command busy\n", __func__);
}


static void
lan9118_set_filter(struct lan9118_softc *sc)
{
	struct ether_multistep step;
	struct ether_multi *enm;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint32_t mac_cr, h, hashes[2] = { 0, 0 };

	mac_cr = lan9118_mac_readreg(sc, LAN9118_MAC_CR);
	if (ifp->if_flags & IFF_PROMISC) {
		lan9118_mac_writereg(sc, LAN9118_MAC_CR,
		    mac_cr | LAN9118_MAC_CR_PRMS);
		return;
	}

	mac_cr &= ~(LAN9118_MAC_CR_PRMS | LAN9118_MAC_CR_MCPAS |
	    LAN9118_MAC_CR_BCAST | LAN9118_MAC_CR_HPFILT);
	if (!(ifp->if_flags & IFF_BROADCAST))
		mac_cr |= LAN9118_MAC_CR_BCAST;

	if (ifp->if_flags & IFF_ALLMULTI)
		mac_cr |= LAN9118_MAC_CR_MCPAS;
	else {
		ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
		while (enm != NULL) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN) != 0) {
				/*
				 * We must listen to a range of multicast
				 * addresses.  For now, just accept all
				 * multicasts, rather than trying to set
				 * only those filter bits needed to match
				 * the range.  (At this time, the only use
				 * of address ranges is for IP multicast
				 * routing, for which the range is big enough
				 * to require all bits set.)
				 */
				ifp->if_flags |= IFF_ALLMULTI;
				mac_cr |= LAN9118_MAC_CR_MCPAS;
				break;
			}
			h = ether_crc32_le(enm->enm_addrlo,
			    ETHER_ADDR_LEN) >> 26;
			hashes[h >> 5] |= 1 << (h & 0x1f);

			mac_cr |= LAN9118_MAC_CR_HPFILT;
			ETHER_NEXT_MULTI(step, enm);
		}
		if (mac_cr & LAN9118_MAC_CR_HPFILT) {
			lan9118_mac_writereg(sc, LAN9118_HASHH, hashes[1]);
			lan9118_mac_writereg(sc, LAN9118_HASHL, hashes[0]);
		}
	}
	lan9118_mac_writereg(sc, LAN9118_MAC_CR, mac_cr);
	return;
}

static void
lan9118_rxintr(struct lan9118_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m;
	uint32_t rx_fifo_inf, rx_status;
	int pktlen;
	const int pad = ETHER_HDR_LEN % sizeof(uint32_t);

	DPRINTFN(3, ("%s\n", __func__));

	for (;;) {
		rx_fifo_inf = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_RX_FIFO_INF);
		if (LAN9118_RX_FIFO_INF_RXSUSED(rx_fifo_inf) == 0)
			break;

		rx_status = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_RXSFIFOP);
		pktlen = LAN9118_RXS_PKTLEN(rx_status);
		DPRINTFN(3, ("%s: rx_status=0x%x(pktlen %d)\n",
		    __func__, rx_status, pktlen));
		if (rx_status & (LAN9118_RXS_ES | LAN9118_RXS_LENERR |
		    LAN9118_RXS_RWTO | LAN9118_RXS_MIIERR | LAN9118_RXS_DBIT)) {
			if (rx_status & LAN9118_RXS_LENERR)
				aprint_error_dev(sc->sc_dev, "Length Error\n");
			if (rx_status & LAN9118_RXS_RUNTF)
				aprint_error_dev(sc->sc_dev, "Runt Frame\n");
			if (rx_status & LAN9118_RXS_FTL)
				aprint_error_dev(sc->sc_dev,
				    "Frame Too Long\n");
			if (rx_status & LAN9118_RXS_RWTO)
				aprint_error_dev(sc->sc_dev,
				    "Receive Watchdog time-out\n");
			if (rx_status & LAN9118_RXS_MIIERR)
				aprint_error_dev(sc->sc_dev, "MII Error\n");
			if (rx_status & LAN9118_RXS_DBIT)
				aprint_error_dev(sc->sc_dev, "Drabbling Bit\n");
			if (rx_status & LAN9118_RXS_COLS)
				aprint_error_dev(sc->sc_dev,
				    "Collision Seen\n");
			if (rx_status & LAN9118_RXS_CRCERR)
				aprint_error_dev(sc->sc_dev, "CRC Error\n");

dropit:
			ifp->if_ierrors++;
			/*
			 * Receive Data FIFO Fast Forward
			 * When performing a fast-forward, there must be at
			 * least 4 DWORDs of data in the RX data FIFO for the
			 * packet being discarded.
			 */
			if (pktlen >= 4 * sizeof(uint32_t)) {
				uint32_t rx_dp_ctl;

				bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				    LAN9118_RX_DP_CTL,
				    LAN9118_RX_DP_CTL_RX_FFWD);
				/* DP_FFWD bit is self clearing */
				do {
					rx_dp_ctl = bus_space_read_4(sc->sc_iot,
					    sc->sc_ioh, LAN9118_RX_DP_CTL);
				} while (rx_dp_ctl & LAN9118_RX_DP_CTL_RX_FFWD);
			} else {
				/* For less than 4 DWORDs do not use RX_FFWD. */
				uint32_t garbage[4];

				bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh,
				    LAN9118_RXDFIFOP, garbage,
				    roundup(pktlen, 4) >> 2);
			}
			continue;
		}

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			goto dropit;
		if (pktlen > (MHLEN - pad)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				goto dropit;
			}
		}

		/* STRICT_ALIGNMENT */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LAN9118_RX_CFG,
		    LAN9118_RX_CFG_RXEA_4B | LAN9118_RX_CFG_RXDOFF(pad));
		bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh, LAN9118_RXDFIFOP,
		    mtod(m, uint32_t *),
		    roundup(pad + pktlen, sizeof(uint32_t)) >> 2);
		m->m_data += pad;

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = (pktlen - ETHER_CRC_LEN);

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}
}

static void
lan9118_txintr(struct lan9118_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint32_t tx_fifo_inf, tx_status;
	int fdx = IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX;
	int tdfree;

	DPRINTFN(3, ("%s\n", __func__));

	for (;;) {
		tx_fifo_inf = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_TX_FIFO_INF);
		if (LAN9118_TX_FIFO_INF_TXSUSED(tx_fifo_inf) == 0)
			break;

		tx_status = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    LAN9118_TXSFIFOP);
		DPRINTFN(3, ("%s: tx_status=0x%x\n", __func__, tx_status));
		if (tx_status & LAN9118_TXS_ES) {
			if (tx_status & LAN9118_TXS_LOC)
				aprint_error_dev(sc->sc_dev,
				    "Loss Of Carrier\n");
			if ((tx_status & LAN9118_TXS_NC) && !fdx)
				aprint_error_dev(sc->sc_dev, "No Carrier\n");
			if (tx_status & LAN9118_TXS_LCOL)
				aprint_error_dev(sc->sc_dev,
				    "Late Collision\n");
			if (tx_status & LAN9118_TXS_ECOL) {
				/* Rearch 16 collision */
				ifp->if_collisions += 16;
				aprint_error_dev(sc->sc_dev,
				    "Excessive Collision\n");
			}
			if (LAN9118_TXS_COLCNT(tx_status) != 0)
				aprint_error_dev(sc->sc_dev,
				    "Collision Count: %d\n",
				    LAN9118_TXS_COLCNT(tx_status));
			if (tx_status & LAN9118_TXS_ED)
				aprint_error_dev(sc->sc_dev,
				    "Excessive Deferral\n");
			if (tx_status & LAN9118_TXS_DEFERRED)
				aprint_error_dev(sc->sc_dev, "Deferred\n");
			ifp->if_oerrors++;
		} else
			ifp->if_opackets++;
	}

	tdfree = LAN9118_TX_FIFO_INF_TDFREE(tx_fifo_inf);
	if (tdfree == LAN9118_TX_DATA_FIFO_SIZE)
		/* FIFO empty */
		ifp->if_timer = 0;
	if (tdfree >= 2036)
		/*
		 * 2036 is the possible maximum FIFO consumption
		 * for the most fragmented frame.
		 */
		ifp->if_flags &= ~IFF_OACTIVE;
}

void
lan9118_tick(void *v)
{
	struct lan9118_softc *sc = v;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	callout_schedule(&sc->sc_tick, hz);
	splx(s);
}
