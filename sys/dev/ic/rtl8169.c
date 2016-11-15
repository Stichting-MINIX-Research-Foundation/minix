/*	$NetBSD: rtl8169.c,v 1.145 2015/08/28 13:20:46 nonaka Exp $	*/

/*
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtl8169.c,v 1.145 2015/08/28 13:20:46 nonaka Exp $");
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/re/if_re.c,v 1.20 2004/04/11 20:34:08 ru Exp $ */

/*
 * RealTek 8139C+/8169/8169S/8168/8110S PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * This driver is designed to support RealTek's next generation of
 * 10/100 and 10/100/1000 PCI ethernet controllers. There are currently
 * six devices in this family: the RTL8139C+, the RTL8169, the RTL8169S,
 * RTL8110S, the RTL8168 and the RTL8111.
 *
 * The 8139C+ is a 10/100 ethernet chip. It is backwards compatible
 * with the older 8139 family, however it also supports a special
 * C+ mode of operation that provides several new performance enhancing
 * features. These include:
 *
 *	o Descriptor based DMA mechanism. Each descriptor represents
 *	  a single packet fragment. Data buffers may be aligned on
 *	  any byte boundary.
 *
 *	o 64-bit DMA
 *
 *	o TCP/IP checksum offload for both RX and TX
 *
 *	o High and normal priority transmit DMA rings
 *
 *	o VLAN tag insertion and extraction
 *
 *	o TCP large send (segmentation offload)
 *
 * Like the 8139, the 8139C+ also has a built-in 10/100 PHY. The C+
 * programming API is fairly straightforward. The RX filtering, EEPROM
 * access and PHY access is the same as it is on the older 8139 series
 * chips.
 *
 * The 8169 is a 64-bit 10/100/1000 gigabit ethernet MAC. It has almost the
 * same programming API and feature set as the 8139C+ with the following
 * differences and additions:
 *
 *	o 1000Mbps mode
 *
 *	o Jumbo frames
 *
 *	o GMII and TBI ports/registers for interfacing with copper
 *	  or fiber PHYs
 *
 *      o RX and TX DMA rings can have up to 1024 descriptors
 *        (the 8139C+ allows a maximum of 64)
 *
 *	o Slight differences in register layout from the 8139C+
 *
 * The TX start and timer interrupt registers are at different locations
 * on the 8169 than they are on the 8139C+. Also, the status word in the
 * RX descriptor has a slightly different bit layout. The 8169 does not
 * have a built-in PHY. Most reference boards use a Marvell 88E1000 'Alaska'
 * copper gigE PHY.
 *
 * The 8169S/8110S 10/100/1000 devices have built-in copper gigE PHYs
 * (the 'S' stands for 'single-chip'). These devices have the same
 * programming API as the older 8169, but also have some vendor-specific
 * registers for the on-board PHY. The 8110S is a LAN-on-motherboard
 * part designed to be pin-compatible with the RealTek 8100 10/100 chip.
 *
 * This driver takes advantage of the RX and TX checksum offload and
 * VLAN tag insertion/extraction features. It also implements TX
 * interrupt moderation using the timer interrupt registers, which
 * significantly reduces TX interrupt load. There is also support
 * for jumbo frames, however the 8169/8169S/8110S can not transmit
 * jumbo frames larger than 7.5K, so the max MTU possible with this
 * driver is 7500 bytes.
 */


#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_vlanvar.h>

#include <netinet/in_systm.h>	/* XXX for IP_MAXPACKET */
#include <netinet/in.h>		/* XXX for IP_MAXPACKET */
#include <netinet/ip.h>		/* XXX for IP_MAXPACKET */

#include <net/bpf.h>
#include <sys/rndsource.h>

#include <sys/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

#include <dev/ic/rtl8169var.h>

static inline void re_set_bufaddr(struct re_desc *, bus_addr_t);

static int re_newbuf(struct rtk_softc *, int, struct mbuf *);
static int re_rx_list_init(struct rtk_softc *);
static int re_tx_list_init(struct rtk_softc *);
static void re_rxeof(struct rtk_softc *);
static void re_txeof(struct rtk_softc *);
static void re_tick(void *);
static void re_start(struct ifnet *);
static int re_ioctl(struct ifnet *, u_long, void *);
static int re_init(struct ifnet *);
static void re_stop(struct ifnet *, int);
static void re_watchdog(struct ifnet *);

static int re_enable(struct rtk_softc *);
static void re_disable(struct rtk_softc *);

static int re_gmii_readreg(device_t, int, int);
static void re_gmii_writereg(device_t, int, int, int);

static int re_miibus_readreg(device_t, int, int);
static void re_miibus_writereg(device_t, int, int, int);
static void re_miibus_statchg(struct ifnet *);

static void re_reset(struct rtk_softc *);

static inline void
re_set_bufaddr(struct re_desc *d, bus_addr_t addr)
{

	d->re_bufaddr_lo = htole32((uint32_t)addr);
	if (sizeof(bus_addr_t) == sizeof(uint64_t))
		d->re_bufaddr_hi = htole32((uint64_t)addr >> 32);
	else
		d->re_bufaddr_hi = 0;
}

static int
re_gmii_readreg(device_t dev, int phy, int reg)
{
	struct rtk_softc *sc = device_private(dev);
	uint32_t rval;
	int i;

	if (phy != 7)
		return 0;

	/* Let the rgephy driver read the GMEDIASTAT register */

	if (reg == RTK_GMEDIASTAT) {
		rval = CSR_READ_1(sc, RTK_GMEDIASTAT);
		return rval;
	}

	CSR_WRITE_4(sc, RTK_PHYAR, reg << 16);
	DELAY(1000);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RTK_PHYAR);
		if (rval & RTK_PHYAR_BUSY)
			break;
		DELAY(100);
	}

	if (i == RTK_TIMEOUT) {
		printf("%s: PHY read failed\n", device_xname(sc->sc_dev));
		return 0;
	}

	return rval & RTK_PHYAR_PHYDATA;
}

static void
re_gmii_writereg(device_t dev, int phy, int reg, int data)
{
	struct rtk_softc *sc = device_private(dev);
	uint32_t rval;
	int i;

	CSR_WRITE_4(sc, RTK_PHYAR, (reg << 16) |
	    (data & RTK_PHYAR_PHYDATA) | RTK_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RTK_PHYAR);
		if (!(rval & RTK_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RTK_TIMEOUT) {
		printf("%s: PHY write reg %x <- %x failed\n",
		    device_xname(sc->sc_dev), reg, data);
	}
}

static int
re_miibus_readreg(device_t dev, int phy, int reg)
{
	struct rtk_softc *sc = device_private(dev);
	uint16_t rval = 0;
	uint16_t re8139_reg = 0;
	int s;

	s = splnet();

	if ((sc->sc_quirk & RTKQ_8139CPLUS) == 0) {
		rval = re_gmii_readreg(dev, phy, reg);
		splx(s);
		return rval;
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return 0;
	}
	switch (reg) {
	case MII_BMCR:
		re8139_reg = RTK_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RTK_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RTK_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RTK_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RTK_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return 0;
	/*
	 * Allow the rlphy driver to read the media status
	 * register. If we have a link partner which does not
	 * support NWAY, this is the register which will tell
	 * us the results of parallel detection.
	 */
	case RTK_MEDIASTAT:
		rval = CSR_READ_1(sc, RTK_MEDIASTAT);
		splx(s);
		return rval;
	default:
		printf("%s: bad phy register\n", device_xname(sc->sc_dev));
		splx(s);
		return 0;
	}
	rval = CSR_READ_2(sc, re8139_reg);
	if ((sc->sc_quirk & RTKQ_8139CPLUS) != 0 && re8139_reg == RTK_BMCR) {
		/* 8139C+ has different bit layout. */
		rval &= ~(BMCR_LOOP | BMCR_ISO);
	}
	splx(s);
	return rval;
}

static void
re_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct rtk_softc *sc = device_private(dev);
	uint16_t re8139_reg = 0;
	int s;

	s = splnet();

	if ((sc->sc_quirk & RTKQ_8139CPLUS) == 0) {
		re_gmii_writereg(dev, phy, reg, data);
		splx(s);
		return;
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return;
	}
	switch (reg) {
	case MII_BMCR:
		re8139_reg = RTK_BMCR;
		if ((sc->sc_quirk & RTKQ_8139CPLUS) != 0) {
			/* 8139C+ has different bit layout. */
			data &= ~(BMCR_LOOP | BMCR_ISO);
		}
		break;
	case MII_BMSR:
		re8139_reg = RTK_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RTK_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RTK_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RTK_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return;
		break;
	default:
		printf("%s: bad phy register\n", device_xname(sc->sc_dev));
		splx(s);
		return;
	}
	CSR_WRITE_2(sc, re8139_reg, data);
	splx(s);
	return;
}

static void
re_miibus_statchg(struct ifnet *ifp)
{

	return;
}

static void
re_reset(struct rtk_softc *sc)
{
	int i;

	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_RESET);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		DELAY(10);
		if ((CSR_READ_1(sc, RTK_COMMAND) & RTK_CMD_RESET) == 0)
			break;
	}
	if (i == RTK_TIMEOUT)
		printf("%s: reset never completed!\n",
		    device_xname(sc->sc_dev));

	/*
	 * NB: Realtek-supplied FreeBSD driver does this only for MACFG_3,
	 *     but also says "Rtl8169s sigle chip detected".
	 */
	if ((sc->sc_quirk & RTKQ_MACLDPS) != 0)
		CSR_WRITE_1(sc, RTK_LDPS, 1);

}

/*
 * The following routine is designed to test for a defect on some
 * 32-bit 8169 cards. Some of these NICs have the REQ64# and ACK64#
 * lines connected to the bus, however for a 32-bit only card, they
 * should be pulled high. The result of this defect is that the
 * NIC will not work right if you plug it into a 64-bit slot: DMA
 * operations will be done with 64-bit transfers, which will fail
 * because the 64-bit data lines aren't connected.
 *
 * There's no way to work around this (short of talking a soldering
 * iron to the board), however we can detect it. The method we use
 * here is to put the NIC into digital loopback mode, set the receiver
 * to promiscuous mode, and then try to send a frame. We then compare
 * the frame data we sent to what was received. If the data matches,
 * then the NIC is working correctly, otherwise we know the user has
 * a defective NIC which has been mistakenly plugged into a 64-bit PCI
 * slot. In the latter case, there's no way the NIC can work correctly,
 * so we print out a message on the console and abort the device attach.
 */

int
re_diag(struct rtk_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	struct mbuf *m0;
	struct ether_header *eh;
	struct re_rxsoft *rxs;
	struct re_desc *cur_rx;
	bus_dmamap_t dmamap;
	uint16_t status;
	uint32_t rxstat;
	int total_len, i, s, error = 0;
	static const uint8_t dst[] = { 0x00, 'h', 'e', 'l', 'l', 'o' };
	static const uint8_t src[] = { 0x00, 'w', 'o', 'r', 'l', 'd' };

	/* Allocate a single mbuf */

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return ENOBUFS;

	/*
	 * Initialize the NIC in test mode. This sets the chip up
	 * so that it can send and receive frames, but performs the
	 * following special functions:
	 * - Puts receiver in promiscuous mode
	 * - Enables digital loopback mode
	 * - Leaves interrupts turned off
	 */

	ifp->if_flags |= IFF_PROMISC;
	sc->re_testmode = 1;
	re_init(ifp);
	re_stop(ifp, 0);
	DELAY(100000);
	re_init(ifp);

	/* Put some data in the mbuf */

	eh = mtod(m0, struct ether_header *);
	memcpy(eh->ether_dhost, &dst, ETHER_ADDR_LEN);
	memcpy(eh->ether_shost, &src, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_IP);
	m0->m_pkthdr.len = m0->m_len = ETHER_MIN_LEN - ETHER_CRC_LEN;

	/*
	 * Queue the packet, start transmission.
	 */

	CSR_WRITE_2(sc, RTK_ISR, 0xFFFF);
	s = splnet();
	IF_ENQUEUE(&ifp->if_snd, m0);
	re_start(ifp);
	splx(s);
	m0 = NULL;

	/* Wait for it to propagate through the chip */

	DELAY(100000);
	for (i = 0; i < RTK_TIMEOUT; i++) {
		status = CSR_READ_2(sc, RTK_ISR);
		if ((status & (RTK_ISR_TIMEOUT_EXPIRED | RTK_ISR_RX_OK)) ==
		    (RTK_ISR_TIMEOUT_EXPIRED | RTK_ISR_RX_OK))
			break;
		DELAY(10);
	}
	if (i == RTK_TIMEOUT) {
		aprint_error_dev(sc->sc_dev,
		    "diagnostic failed, failed to receive packet "
		    "in loopback mode\n");
		error = EIO;
		goto done;
	}

	/*
	 * The packet should have been dumped into the first
	 * entry in the RX DMA ring. Grab it from there.
	 */

	rxs = &sc->re_ldata.re_rxsoft[0];
	dmamap = rxs->rxs_dmamap;
	bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, dmamap);

	m0 = rxs->rxs_mbuf;
	rxs->rxs_mbuf = NULL;
	eh = mtod(m0, struct ether_header *);

	RE_RXDESCSYNC(sc, 0, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	cur_rx = &sc->re_ldata.re_rx_list[0];
	rxstat = le32toh(cur_rx->re_cmdstat);
	total_len = rxstat & sc->re_rxlenmask;

	if (total_len != ETHER_MIN_LEN) {
		aprint_error_dev(sc->sc_dev,
		    "diagnostic failed, received short packet\n");
		error = EIO;
		goto done;
	}

	/* Test that the received packet data matches what we sent. */

	if (memcmp(&eh->ether_dhost, &dst, ETHER_ADDR_LEN) ||
	    memcmp(&eh->ether_shost, &src, ETHER_ADDR_LEN) ||
	    ntohs(eh->ether_type) != ETHERTYPE_IP) {
		aprint_error_dev(sc->sc_dev, "WARNING, DMA FAILURE!\n"
		    "expected TX data: %s/%s/0x%x\n"
		    "received RX data: %s/%s/0x%x\n"
		    "You may have a defective 32-bit NIC plugged "
		    "into a 64-bit PCI slot.\n"
		    "Please re-install the NIC in a 32-bit slot "
		    "for proper operation.\n"
		    "Read the re(4) man page for more details.\n" ,
		    ether_sprintf(dst),  ether_sprintf(src), ETHERTYPE_IP,
		    ether_sprintf(eh->ether_dhost),
		    ether_sprintf(eh->ether_shost), ntohs(eh->ether_type));
		error = EIO;
	}

 done:
	/* Turn interface off, release resources */

	sc->re_testmode = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(ifp, 0);
	if (m0 != NULL)
		m_freem(m0);

	return error;
}


/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
re_attach(struct rtk_softc *sc)
{
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct ifnet *ifp;
	int error = 0, i;

	if ((sc->sc_quirk & RTKQ_8139CPLUS) == 0) {
		uint32_t hwrev;

		/* Revision of 8169/8169S/8110s in bits 30..26, 23 */
		hwrev = CSR_READ_4(sc, RTK_TXCFG) & RTK_TXCFG_HWREV;
		switch (hwrev) {
		case RTK_HWREV_8169:
			sc->sc_quirk |= RTKQ_8169NONS;
			break;
		case RTK_HWREV_8169S:
		case RTK_HWREV_8110S:
		case RTK_HWREV_8169_8110SB:
		case RTK_HWREV_8169_8110SBL:
		case RTK_HWREV_8169_8110SC:
			sc->sc_quirk |= RTKQ_MACLDPS;
			break;
		case RTK_HWREV_8168_SPIN1:
		case RTK_HWREV_8168_SPIN2:
		case RTK_HWREV_8168_SPIN3:
			sc->sc_quirk |= RTKQ_MACSTAT;
			break;
		case RTK_HWREV_8168C:
		case RTK_HWREV_8168C_SPIN2:
		case RTK_HWREV_8168CP:
		case RTK_HWREV_8168D:
		case RTK_HWREV_8168DP:
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD |
			    RTKQ_MACSTAT | RTKQ_CMDSTOP;
			/*
			 * From FreeBSD driver:
			 *
			 * These (8168/8111) controllers support jumbo frame
			 * but it seems that enabling it requires touching
			 * additional magic registers. Depending on MAC
			 * revisions some controllers need to disable
			 * checksum offload. So disable jumbo frame until
			 * I have better idea what it really requires to
			 * make it support.
			 * RTL8168C/CP : supports up to 6KB jumbo frame.
			 * RTL8111C/CP : supports up to 9KB jumbo frame.
			 */
			sc->sc_quirk |= RTKQ_NOJUMBO;
			break;
		case RTK_HWREV_8168E:
		case RTK_HWREV_8168H:
		case RTK_HWREV_8168H_SPIN1:
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD |
			    RTKQ_MACSTAT | RTKQ_CMDSTOP | RTKQ_PHYWAKE_PM |
			    RTKQ_NOJUMBO;
			break;
		case RTK_HWREV_8168E_VL:
		case RTK_HWREV_8168F:
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD |
			    RTKQ_MACSTAT | RTKQ_CMDSTOP | RTKQ_NOJUMBO;
			break;
		case RTK_HWREV_8168G:
		case RTK_HWREV_8168G_SPIN1:
		case RTK_HWREV_8168G_SPIN2:
		case RTK_HWREV_8168G_SPIN4:
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD |
			    RTKQ_MACSTAT | RTKQ_CMDSTOP | RTKQ_NOJUMBO | 
			    RTKQ_RXDV_GATED;
			break;
		case RTK_HWREV_8100E:
		case RTK_HWREV_8100E_SPIN2:
		case RTK_HWREV_8101E:
			sc->sc_quirk |= RTKQ_NOJUMBO;
			break;
		case RTK_HWREV_8102E:
		case RTK_HWREV_8102EL:
		case RTK_HWREV_8103E:
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD |
			    RTKQ_MACSTAT | RTKQ_CMDSTOP | RTKQ_NOJUMBO;
			break;
		default:
			aprint_normal_dev(sc->sc_dev,
			    "Unknown revision (0x%08x)\n", hwrev);
			/* assume the latest features */
			sc->sc_quirk |= RTKQ_DESCV2 | RTKQ_NOEECMD;
			sc->sc_quirk |= RTKQ_NOJUMBO;
		}

		/* Set RX length mask */
		sc->re_rxlenmask = RE_RDESC_STAT_GFRAGLEN;
		sc->re_ldata.re_tx_desc_cnt = RE_TX_DESC_CNT_8169;
	} else {
		sc->sc_quirk |= RTKQ_NOJUMBO;

		/* Set RX length mask */
		sc->re_rxlenmask = RE_RDESC_STAT_FRAGLEN;
		sc->re_ldata.re_tx_desc_cnt = RE_TX_DESC_CNT_8139;
	}

	/* Reset the adapter. */
	re_reset(sc);

	/*
	 * RTL81x9 chips automatically read EEPROM to init MAC address,
	 * and some NAS override its MAC address per own configuration,
	 * so no need to explicitely read EEPROM and set ID registers.
	 */
#ifdef RE_USE_EECMD
	if ((sc->sc_quirk & RTKQ_NOEECMD) != 0) {
		/*
		 * Get station address from ID registers.
		 */
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			eaddr[i] = CSR_READ_1(sc, RTK_IDR0 + i);
	} else {
		uint16_t val;
		int addr_len;

		/*
		 * Get station address from the EEPROM.
		 */
		if (rtk_read_eeprom(sc, RTK_EE_ID, RTK_EEADDR_LEN1) == 0x8129)
			addr_len = RTK_EEADDR_LEN1;
		else
			addr_len = RTK_EEADDR_LEN0;

		/*
		 * Get station address from the EEPROM.
		 */
		for (i = 0; i < ETHER_ADDR_LEN / 2; i++) {
			val = rtk_read_eeprom(sc, RTK_EE_EADDR0 + i, addr_len);
			eaddr[(i * 2) + 0] = val & 0xff;
			eaddr[(i * 2) + 1] = val >> 8;
		}
	}
#else
	/*
	 * Get station address from ID registers.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		eaddr[i] = CSR_READ_1(sc, RTK_IDR0 + i);
#endif

	/* Take PHY out of power down mode. */
	if ((sc->sc_quirk & RTKQ_PHYWAKE_PM) != 0)
		CSR_WRITE_1(sc, RTK_PMCH, CSR_READ_1(sc, RTK_PMCH) | 0x80);

	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(eaddr));

	if (sc->re_ldata.re_tx_desc_cnt >
	    PAGE_SIZE / sizeof(struct re_desc)) {
		sc->re_ldata.re_tx_desc_cnt =
		    PAGE_SIZE / sizeof(struct re_desc);
	}

	aprint_verbose_dev(sc->sc_dev, "using %d tx descriptors\n",
	    sc->re_ldata.re_tx_desc_cnt);
	KASSERT(RE_NEXT_TX_DESC(sc, RE_TX_DESC_CNT(sc) - 1) == 0);

	/* Allocate DMA'able memory for the TX ring */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, RE_TX_LIST_SZ(sc),
	    RE_RING_ALIGN, 0, &sc->re_ldata.re_tx_listseg, 1,
	    &sc->re_ldata.re_tx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't allocate tx listseg, error = %d\n", error);
		goto fail_0;
	}

	/* Load the map for the TX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->re_ldata.re_tx_listseg,
	    sc->re_ldata.re_tx_listnseg, RE_TX_LIST_SZ(sc),
	    (void **)&sc->re_ldata.re_tx_list,
	    BUS_DMA_COHERENT | BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't map tx list, error = %d\n", error);
		goto fail_1;
	}
	memset(sc->re_ldata.re_tx_list, 0, RE_TX_LIST_SZ(sc));

	if ((error = bus_dmamap_create(sc->sc_dmat, RE_TX_LIST_SZ(sc), 1,
	    RE_TX_LIST_SZ(sc), 0, 0,
	    &sc->re_ldata.re_tx_list_map)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't create tx list map, error = %d\n", error);
		goto fail_2;
	}


	if ((error = bus_dmamap_load(sc->sc_dmat,
	    sc->re_ldata.re_tx_list_map, sc->re_ldata.re_tx_list,
	    RE_TX_LIST_SZ(sc), NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't load tx list, error = %d\n", error);
		goto fail_3;
	}

	/* Create DMA maps for TX buffers */
	for (i = 0; i < RE_TX_QLEN; i++) {
		error = bus_dmamap_create(sc->sc_dmat,
		    round_page(IP_MAXPACKET),
		    RE_TX_DESC_CNT(sc), RE_TDESC_CMD_FRAGLEN,
		    0, 0, &sc->re_ldata.re_txq[i].txq_dmamap);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "can't create DMA map for TX\n");
			goto fail_4;
		}
	}

	/* Allocate DMA'able memory for the RX ring */
	/* XXX see also a comment about RE_RX_DMAMEM_SZ in rtl81x9var.h */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    RE_RX_DMAMEM_SZ, RE_RING_ALIGN, 0, &sc->re_ldata.re_rx_listseg, 1,
	    &sc->re_ldata.re_rx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't allocate rx listseg, error = %d\n", error);
		goto fail_4;
	}

	/* Load the map for the RX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->re_ldata.re_rx_listseg,
	    sc->re_ldata.re_rx_listnseg, RE_RX_DMAMEM_SZ,
	    (void **)&sc->re_ldata.re_rx_list,
	    BUS_DMA_COHERENT | BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't map rx list, error = %d\n", error);
		goto fail_5;
	}
	memset(sc->re_ldata.re_rx_list, 0, RE_RX_DMAMEM_SZ);

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    RE_RX_DMAMEM_SZ, 1, RE_RX_DMAMEM_SZ, 0, 0,
	    &sc->re_ldata.re_rx_list_map)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't create rx list map, error = %d\n", error);
		goto fail_6;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat,
	    sc->re_ldata.re_rx_list_map, sc->re_ldata.re_rx_list,
	    RE_RX_DMAMEM_SZ, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't load rx list, error = %d\n", error);
		goto fail_7;
	}

	/* Create DMA maps for RX buffers */
	for (i = 0; i < RE_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, 0, &sc->re_ldata.re_rxsoft[i].rxs_dmamap);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "can't create DMA map for RX\n");
			goto fail_8;
		}
	}

	/*
	 * Record interface as attached. From here, we should not fail.
	 */
	sc->sc_flags |= RTK_ATTACHED;

	ifp = &sc->ethercom.ec_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
	sc->ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;
	ifp->if_start = re_start;
	ifp->if_stop = re_stop;

	/*
	 * IFCAP_CSUM_IPv4_Tx on re(4) is broken for small packets,
	 * so we have a workaround to handle the bug by padding
	 * such packets manually.
	 */
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx |
	    IFCAP_TSOv4;

	/*
	 * XXX
	 * Still have no idea how to make TSO work on 8168C, 8168CP,
	 * 8102E, 8111C and 8111CP.
	 */
	if ((sc->sc_quirk & RTKQ_DESCV2) != 0)
		ifp->if_capabilities &= ~IFCAP_TSOv4;

	ifp->if_watchdog = re_watchdog;
	ifp->if_init = re_init;
	ifp->if_snd.ifq_maxlen = RE_IFQ_MAXLEN;
	ifp->if_capenable = ifp->if_capabilities;
	IFQ_SET_READY(&ifp->if_snd);

	callout_init(&sc->rtk_tick_ch, 0);

	/* Do MII setup */
	sc->mii.mii_ifp = ifp;
	sc->mii.mii_readreg = re_miibus_readreg;
	sc->mii.mii_writereg = re_miibus_writereg;
	sc->mii.mii_statchg = re_miibus_statchg;
	sc->ethercom.ec_mii = &sc->mii;
	ifmedia_init(&sc->mii.mii_media, IFM_IMASK, ether_mediachange,
	    ether_mediastatus);
	mii_attach(sc->sc_dev, &sc->mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	ifmedia_set(&sc->mii.mii_media, IFM_ETHER | IFM_AUTO);

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, eaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (pmf_device_register(sc->sc_dev, NULL, NULL))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	return;

 fail_8:
	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < RE_RX_DESC_CNT; i++)
		if (sc->re_ldata.re_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->re_ldata.re_rxsoft[i].rxs_dmamap);

	/* Free DMA'able memory for the RX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->re_ldata.re_rx_list_map);
 fail_7:
	bus_dmamap_destroy(sc->sc_dmat, sc->re_ldata.re_rx_list_map);
 fail_6:
	bus_dmamem_unmap(sc->sc_dmat,
	    (void *)sc->re_ldata.re_rx_list, RE_RX_DMAMEM_SZ);
 fail_5:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->re_ldata.re_rx_listseg, sc->re_ldata.re_rx_listnseg);

 fail_4:
	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < RE_TX_QLEN; i++)
		if (sc->re_ldata.re_txq[i].txq_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->re_ldata.re_txq[i].txq_dmamap);

	/* Free DMA'able memory for the TX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->re_ldata.re_tx_list_map);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->re_ldata.re_tx_list_map);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (void *)sc->re_ldata.re_tx_list, RE_TX_LIST_SZ(sc));
 fail_1:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->re_ldata.re_tx_listseg, sc->re_ldata.re_tx_listnseg);
 fail_0:
	return;
}


/*
 * re_activate:
 *     Handle device activation/deactivation requests.
 */
int
re_activate(device_t self, enum devact act)
{
	struct rtk_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * re_detach:
 *     Detach a rtk interface.
 */
int
re_detach(struct rtk_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int i;

	/*
	 * Succeed now if there isn't any work to do.
	 */
	if ((sc->sc_flags & RTK_ATTACHED) == 0)
		return 0;

	/* Unhook our tick handler. */
	callout_stop(&sc->rtk_tick_ch);

	/* Detach all PHYs. */
	mii_detach(&sc->mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->mii.mii_media, IFM_INST_ANY);

	rnd_detach_source(&sc->rnd_source);
	ether_ifdetach(ifp);
	if_detach(ifp);

	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < RE_RX_DESC_CNT; i++)
		if (sc->re_ldata.re_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->re_ldata.re_rxsoft[i].rxs_dmamap);

	/* Free DMA'able memory for the RX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->re_ldata.re_rx_list_map);
	bus_dmamap_destroy(sc->sc_dmat, sc->re_ldata.re_rx_list_map);
	bus_dmamem_unmap(sc->sc_dmat,
	    (void *)sc->re_ldata.re_rx_list, RE_RX_DMAMEM_SZ);
	bus_dmamem_free(sc->sc_dmat,
	    &sc->re_ldata.re_rx_listseg, sc->re_ldata.re_rx_listnseg);

	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < RE_TX_QLEN; i++)
		if (sc->re_ldata.re_txq[i].txq_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->re_ldata.re_txq[i].txq_dmamap);

	/* Free DMA'able memory for the TX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->re_ldata.re_tx_list_map);
	bus_dmamap_destroy(sc->sc_dmat, sc->re_ldata.re_tx_list_map);
	bus_dmamem_unmap(sc->sc_dmat,
	    (void *)sc->re_ldata.re_tx_list, RE_TX_LIST_SZ(sc));
	bus_dmamem_free(sc->sc_dmat,
	    &sc->re_ldata.re_tx_listseg, sc->re_ldata.re_tx_listnseg);

	pmf_device_deregister(sc->sc_dev);

	/* we don't want to run again */
	sc->sc_flags &= ~RTK_ATTACHED;

	return 0;
}

/*
 * re_enable:
 *     Enable the RTL81X9 chip.
 */
static int
re_enable(struct rtk_softc *sc)
{

	if (RTK_IS_ENABLED(sc) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    device_xname(sc->sc_dev));
			return EIO;
		}
		sc->sc_flags |= RTK_ENABLED;
	}
	return 0;
}

/*
 * re_disable:
 *     Disable the RTL81X9 chip.
 */
static void
re_disable(struct rtk_softc *sc)
{

	if (RTK_IS_ENABLED(sc) && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_flags &= ~RTK_ENABLED;
	}
}

static int
re_newbuf(struct rtk_softc *sc, int idx, struct mbuf *m)
{
	struct mbuf *n = NULL;
	bus_dmamap_t map;
	struct re_desc *d;
	struct re_rxsoft *rxs;
	uint32_t cmdstat;
	int error;

	if (m == NULL) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return ENOBUFS;

		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			return ENOBUFS;
		}
		m = n;
	} else
		m->m_data = m->m_ext.ext_buf;

	/*
	 * Initialize mbuf length fields and fixup
	 * alignment so that the frame payload is
	 * longword aligned.
	 */
	m->m_len = m->m_pkthdr.len = MCLBYTES - RE_ETHER_ALIGN;
	m->m_data += RE_ETHER_ALIGN;

	rxs = &sc->re_ldata.re_rxsoft[idx];
	map = rxs->rxs_dmamap;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);

	if (error)
		goto out;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	d = &sc->re_ldata.re_rx_list[idx];
#ifdef DIAGNOSTIC
	RE_RXDESCSYNC(sc, idx, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	cmdstat = le32toh(d->re_cmdstat);
	RE_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD);
	if (cmdstat & RE_RDESC_STAT_OWN) {
		panic("%s: tried to map busy RX descriptor",
		    device_xname(sc->sc_dev));
	}
#endif

	rxs->rxs_mbuf = m;

	d->re_vlanctl = 0;
	cmdstat = map->dm_segs[0].ds_len;
	if (idx == (RE_RX_DESC_CNT - 1))
		cmdstat |= RE_RDESC_CMD_EOR;
	re_set_bufaddr(d, map->dm_segs[0].ds_addr);
	d->re_cmdstat = htole32(cmdstat);
	RE_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	cmdstat |= RE_RDESC_CMD_OWN;
	d->re_cmdstat = htole32(cmdstat);
	RE_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return 0;
 out:
	if (n != NULL)
		m_freem(n);
	return ENOMEM;
}

static int
re_tx_list_init(struct rtk_softc *sc)
{
	int i;

	memset(sc->re_ldata.re_tx_list, 0, RE_TX_LIST_SZ(sc));
	for (i = 0; i < RE_TX_QLEN; i++) {
		sc->re_ldata.re_txq[i].txq_mbuf = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat,
	    sc->re_ldata.re_tx_list_map, 0,
	    sc->re_ldata.re_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->re_ldata.re_txq_prodidx = 0;
	sc->re_ldata.re_txq_considx = 0;
	sc->re_ldata.re_txq_free = RE_TX_QLEN;
	sc->re_ldata.re_tx_free = RE_TX_DESC_CNT(sc);
	sc->re_ldata.re_tx_nextfree = 0;

	return 0;
}

static int
re_rx_list_init(struct rtk_softc *sc)
{
	int i;

	memset(sc->re_ldata.re_rx_list, 0, RE_RX_LIST_SZ);

	for (i = 0; i < RE_RX_DESC_CNT; i++) {
		if (re_newbuf(sc, i, NULL) == ENOBUFS)
			return ENOBUFS;
	}

	sc->re_ldata.re_rx_prodidx = 0;
	sc->re_head = sc->re_tail = NULL;

	return 0;
}

/*
 * RX handler for C+ and 8169. For the gigE chips, we support
 * the reception of jumbo frames that have been fragmented
 * across multiple 2K mbuf cluster buffers.
 */
static void
re_rxeof(struct rtk_softc *sc)
{
	struct mbuf *m;
	struct ifnet *ifp;
	int i, total_len;
	struct re_desc *cur_rx;
	struct re_rxsoft *rxs;
	uint32_t rxstat, rxvlan;

	ifp = &sc->ethercom.ec_if;

	for (i = sc->re_ldata.re_rx_prodidx;; i = RE_NEXT_RX_DESC(sc, i)) {
		cur_rx = &sc->re_ldata.re_rx_list[i];
		RE_RXDESCSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		rxstat = le32toh(cur_rx->re_cmdstat);
		rxvlan = le32toh(cur_rx->re_vlanctl);
		RE_RXDESCSYNC(sc, i, BUS_DMASYNC_PREREAD);
		if ((rxstat & RE_RDESC_STAT_OWN) != 0) {
			break;
		}
		total_len = rxstat & sc->re_rxlenmask;
		rxs = &sc->re_ldata.re_rxsoft[i];
		m = rxs->rxs_mbuf;

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->sc_dmat,
		    rxs->rxs_dmamap, 0, rxs->rxs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

		if ((rxstat & RE_RDESC_STAT_EOF) == 0) {
			m->m_len = MCLBYTES - RE_ETHER_ALIGN;
			if (sc->re_head == NULL)
				sc->re_head = sc->re_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->re_tail->m_next = m;
				sc->re_tail = m;
			}
			re_newbuf(sc, i, NULL);
			continue;
		}

		/*
		 * NOTE: for the 8139C+, the frame length field
		 * is always 12 bits in size, but for the gigE chips,
		 * it is 13 bits (since the max RX frame length is 16K).
		 * Unfortunately, all 32 bits in the status word
		 * were already used, so to make room for the extra
		 * length bit, RealTek took out the 'frame alignment
		 * error' bit and shifted the other status bits
		 * over one slot. The OWN, EOR, FS and LS bits are
		 * still in the same places. We have already extracted
		 * the frame length and checked the OWN bit, so rather
		 * than using an alternate bit mapping, we shift the
		 * status bits one space to the right so we can evaluate
		 * them using the 8169 status as though it was in the
		 * same format as that of the 8139C+.
		 */
		if ((sc->sc_quirk & RTKQ_8139CPLUS) == 0)
			rxstat >>= 1;

		if (__predict_false((rxstat & RE_RDESC_STAT_RXERRSUM) != 0)) {
#ifdef RE_DEBUG
			printf("%s: RX error (rxstat = 0x%08x)",
			    device_xname(sc->sc_dev), rxstat);
			if (rxstat & RE_RDESC_STAT_FRALIGN)
				printf(", frame alignment error");
			if (rxstat & RE_RDESC_STAT_BUFOFLOW)
				printf(", out of buffer space");
			if (rxstat & RE_RDESC_STAT_FIFOOFLOW)
				printf(", FIFO overrun");
			if (rxstat & RE_RDESC_STAT_GIANT)
				printf(", giant packet");
			if (rxstat & RE_RDESC_STAT_RUNT)
				printf(", runt packet");
			if (rxstat & RE_RDESC_STAT_CRCERR)
				printf(", CRC error");
			printf("\n");
#endif
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->re_head != NULL) {
				m_freem(sc->re_head);
				sc->re_head = sc->re_tail = NULL;
			}
			re_newbuf(sc, i, m);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (__predict_false(re_newbuf(sc, i, NULL) != 0)) {
			ifp->if_ierrors++;
			if (sc->re_head != NULL) {
				m_freem(sc->re_head);
				sc->re_head = sc->re_tail = NULL;
			}
			re_newbuf(sc, i, m);
			continue;
		}

		if (sc->re_head != NULL) {
			m->m_len = total_len % (MCLBYTES - RE_ETHER_ALIGN);
			/*
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->re_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->re_tail->m_next = m;
			}
			m = sc->re_head;
			sc->re_head = sc->re_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming */
		if ((sc->sc_quirk & RTKQ_DESCV2) == 0) {
			/* Check IP header checksum */
			if ((rxstat & RE_RDESC_STAT_PROTOID) != 0) {
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
				if (rxstat & RE_RDESC_STAT_IPSUMBAD)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4_BAD;

				/* Check TCP/UDP checksum */
				if (RE_TCPPKT(rxstat)) {
					m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
					if (rxstat & RE_RDESC_STAT_TCPSUMBAD)
						m->m_pkthdr.csum_flags |=
						    M_CSUM_TCP_UDP_BAD;
				} else if (RE_UDPPKT(rxstat)) {
					m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
					if (rxstat & RE_RDESC_STAT_UDPSUMBAD) {
						/*
						 * XXX: 8139C+ thinks UDP csum
						 * 0xFFFF is bad, force software
						 * calculation.
						 */
						if (sc->sc_quirk & RTKQ_8139CPLUS)
							m->m_pkthdr.csum_flags
							    &= ~M_CSUM_UDPv4;
						else
							m->m_pkthdr.csum_flags
							    |= M_CSUM_TCP_UDP_BAD;
					}
				}
			}
		} else {
			/* Check IPv4 header checksum */
			if ((rxvlan & RE_RDESC_VLANCTL_IPV4) != 0) {
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
				if (rxstat & RE_RDESC_STAT_IPSUMBAD)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4_BAD;

				/* Check TCPv4/UDPv4 checksum */
				if (RE_TCPPKT(rxstat)) {
					m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
					if (rxstat & RE_RDESC_STAT_TCPSUMBAD)
						m->m_pkthdr.csum_flags |=
						    M_CSUM_TCP_UDP_BAD;
				} else if (RE_UDPPKT(rxstat)) {
					m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
					if (rxstat & RE_RDESC_STAT_UDPSUMBAD)
						m->m_pkthdr.csum_flags |=
						    M_CSUM_TCP_UDP_BAD;
				}
			}
			/* XXX Check TCPv6/UDPv6 checksum? */
		}

		if (rxvlan & RE_RDESC_VLANCTL_TAG) {
			VLAN_INPUT_TAG(ifp, m,
			     bswap16(rxvlan & RE_RDESC_VLANCTL_DATA),
			     continue);
		}
		bpf_mtap(ifp, m);
		(*ifp->if_input)(ifp, m);
	}

	sc->re_ldata.re_rx_prodidx = i;
}

static void
re_txeof(struct rtk_softc *sc)
{
	struct ifnet *ifp;
	struct re_txq *txq;
	uint32_t txstat;
	int idx, descidx;

	ifp = &sc->ethercom.ec_if;

	for (idx = sc->re_ldata.re_txq_considx;
	    sc->re_ldata.re_txq_free < RE_TX_QLEN;
	    idx = RE_NEXT_TXQ(sc, idx), sc->re_ldata.re_txq_free++) {
		txq = &sc->re_ldata.re_txq[idx];
		KASSERT(txq->txq_mbuf != NULL);

		descidx = txq->txq_descidx;
		RE_TXDESCSYNC(sc, descidx,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		txstat =
		    le32toh(sc->re_ldata.re_tx_list[descidx].re_cmdstat);
		RE_TXDESCSYNC(sc, descidx, BUS_DMASYNC_PREREAD);
		KASSERT((txstat & RE_TDESC_CMD_EOF) != 0);
		if (txstat & RE_TDESC_CMD_OWN) {
			break;
		}

		sc->re_ldata.re_tx_free += txq->txq_nsegs;
		KASSERT(sc->re_ldata.re_tx_free <= RE_TX_DESC_CNT(sc));
		bus_dmamap_sync(sc->sc_dmat, txq->txq_dmamap,
		    0, txq->txq_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txq->txq_dmamap);
		m_freem(txq->txq_mbuf);
		txq->txq_mbuf = NULL;

		if (txstat & (RE_TDESC_STAT_EXCESSCOL | RE_TDESC_STAT_COLCNT))
			ifp->if_collisions++;
		if (txstat & RE_TDESC_STAT_TXERRSUM)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;
	}

	sc->re_ldata.re_txq_considx = idx;

	if (sc->re_ldata.re_txq_free > RE_NTXDESC_RSVD)
		ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->re_ldata.re_txq_free < RE_TX_QLEN) {
		CSR_WRITE_4(sc, RTK_TIMERCNT, 1);
		if ((sc->sc_quirk & RTKQ_PCIE) != 0) {
			/*
			 * Some chips will ignore a second TX request
			 * issued while an existing transmission is in
			 * progress. If the transmitter goes idle but
			 * there are still packets waiting to be sent,
			 * we need to restart the channel here to flush
			 * them out. This only seems to be required with
			 * the PCIe devices.
			 */
			CSR_WRITE_1(sc, RTK_GTXSTART, RTK_TXSTART_START);
		}
	} else
		ifp->if_timer = 0;
}

static void
re_tick(void *arg)
{
	struct rtk_softc *sc = arg;
	int s;

	/* XXX: just return for 8169S/8110S with rev 2 or newer phy */
	s = splnet();

	mii_tick(&sc->mii);
	splx(s);

	callout_reset(&sc->rtk_tick_ch, hz, re_tick, sc);
}

int
re_intr(void *arg)
{
	struct rtk_softc *sc = arg;
	struct ifnet *ifp;
	uint16_t status;
	int handled = 0;

	if (!device_has_power(sc->sc_dev))
		return 0;

	ifp = &sc->ethercom.ec_if;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	for (;;) {

		status = CSR_READ_2(sc, RTK_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status) {
			handled = 1;
			CSR_WRITE_2(sc, RTK_ISR, status);
		}

		if ((status & RTK_INTRS_CPLUS) == 0)
			break;

		if (status & (RTK_ISR_RX_OK | RTK_ISR_RX_ERR))
			re_rxeof(sc);

		if (status & (RTK_ISR_TIMEOUT_EXPIRED | RTK_ISR_TX_ERR |
		    RTK_ISR_TX_DESC_UNAVAIL))
			re_txeof(sc);

		if (status & RTK_ISR_SYSTEM_ERR) {
			re_init(ifp);
		}

		if (status & RTK_ISR_LINKCHG) {
			callout_stop(&sc->rtk_tick_ch);
			re_tick(sc);
		}
	}

	if (handled && !IFQ_IS_EMPTY(&ifp->if_snd))
		re_start(ifp);

	rnd_add_uint32(&sc->rnd_source, status);

	return handled;
}



/*
 * Main transmit routine for C+ and gigE NICs.
 */

static void
re_start(struct ifnet *ifp)
{
	struct rtk_softc *sc;
	struct mbuf *m;
	bus_dmamap_t map;
	struct re_txq *txq;
	struct re_desc *d;
	struct m_tag *mtag;
	uint32_t cmdstat, re_flags, vlanctl;
	int ofree, idx, error, nsegs, seg;
	int startdesc, curdesc, lastdesc;
	bool pad;

	sc = ifp->if_softc;
	ofree = sc->re_ldata.re_txq_free;

	for (idx = sc->re_ldata.re_txq_prodidx;; idx = RE_NEXT_TXQ(sc, idx)) {

		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (sc->re_ldata.re_txq_free == 0 ||
		    sc->re_ldata.re_tx_free == 0) {
			/* no more free slots left */
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * Set up checksum offload. Note: checksum offload bits must
		 * appear in all descriptors of a multi-descriptor transmit
		 * attempt. (This is according to testing done with an 8169
		 * chip. I'm not sure if this is a requirement or a bug.)
		 */

		vlanctl = 0;
		if ((m->m_pkthdr.csum_flags & M_CSUM_TSOv4) != 0) {
			uint32_t segsz = m->m_pkthdr.segsz;

			re_flags = RE_TDESC_CMD_LGSEND |
			    (segsz << RE_TDESC_CMD_MSSVAL_SHIFT);
		} else {
			/*
			 * set RE_TDESC_CMD_IPCSUM if any checksum offloading
			 * is requested.  otherwise, RE_TDESC_CMD_TCPCSUM/
			 * RE_TDESC_CMD_UDPCSUM doesn't make effects.
			 */
			re_flags = 0;
			if ((m->m_pkthdr.csum_flags &
			    (M_CSUM_IPv4 | M_CSUM_TCPv4 | M_CSUM_UDPv4))
			    != 0) {
				if ((sc->sc_quirk & RTKQ_DESCV2) == 0) {
					re_flags |= RE_TDESC_CMD_IPCSUM;
					if (m->m_pkthdr.csum_flags &
					    M_CSUM_TCPv4) {
						re_flags |=
						    RE_TDESC_CMD_TCPCSUM;
					} else if (m->m_pkthdr.csum_flags &
					    M_CSUM_UDPv4) {
						re_flags |=
						    RE_TDESC_CMD_UDPCSUM;
					}
				} else {
					vlanctl |= RE_TDESC_VLANCTL_IPCSUM;
					if (m->m_pkthdr.csum_flags &
					    M_CSUM_TCPv4) {
						vlanctl |=
						    RE_TDESC_VLANCTL_TCPCSUM;
					} else if (m->m_pkthdr.csum_flags &
					    M_CSUM_UDPv4) {
						vlanctl |=
						    RE_TDESC_VLANCTL_UDPCSUM;
					}
				}
			}
		}

		txq = &sc->re_ldata.re_txq[idx];
		map = txq->txq_dmamap;
		error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);

		if (__predict_false(error)) {
			/* XXX try to defrag if EFBIG? */
			printf("%s: can't map mbuf (error %d)\n",
			    device_xname(sc->sc_dev), error);

			IFQ_DEQUEUE(&ifp->if_snd, m);
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}

		nsegs = map->dm_nsegs;
		pad = false;
		if (__predict_false(m->m_pkthdr.len <= RE_IP4CSUMTX_PADLEN &&
		    (re_flags & RE_TDESC_CMD_IPCSUM) != 0 &&
		    (sc->sc_quirk & RTKQ_DESCV2) == 0)) {
			pad = true;
			nsegs++;
		}

		if (nsegs > sc->re_ldata.re_tx_free) {
			/*
			 * Not enough free descriptors to transmit this packet.
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, map);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);

		/*
		 * Make sure that the caches are synchronized before we
		 * ask the chip to start DMA for the packet data.
		 */
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Set up hardware VLAN tagging. Note: vlan tag info must
		 * appear in all descriptors of a multi-descriptor
		 * transmission attempt.
		 */
		if ((mtag = VLAN_OUTPUT_TAG(&sc->ethercom, m)) != NULL)
			vlanctl |= bswap16(VLAN_TAG_VALUE(mtag)) |
			    RE_TDESC_VLANCTL_TAG;

		/*
		 * Map the segment array into descriptors.
		 * Note that we set the start-of-frame and
		 * end-of-frame markers for either TX or RX,
		 * but they really only have meaning in the TX case.
		 * (In the RX case, it's the chip that tells us
		 *  where packets begin and end.)
		 * We also keep track of the end of the ring
		 * and set the end-of-ring bits as needed,
		 * and we set the ownership bits in all except
		 * the very first descriptor. (The caller will
		 * set this descriptor later when it start
		 * transmission or reception.)
		 */
		curdesc = startdesc = sc->re_ldata.re_tx_nextfree;
		lastdesc = -1;
		for (seg = 0; seg < map->dm_nsegs;
		    seg++, curdesc = RE_NEXT_TX_DESC(sc, curdesc)) {
			d = &sc->re_ldata.re_tx_list[curdesc];
#ifdef DIAGNOSTIC
			RE_TXDESCSYNC(sc, curdesc,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			cmdstat = le32toh(d->re_cmdstat);
			RE_TXDESCSYNC(sc, curdesc, BUS_DMASYNC_PREREAD);
			if (cmdstat & RE_TDESC_STAT_OWN) {
				panic("%s: tried to map busy TX descriptor",
				    device_xname(sc->sc_dev));
			}
#endif

			d->re_vlanctl = htole32(vlanctl);
			re_set_bufaddr(d, map->dm_segs[seg].ds_addr);
			cmdstat = re_flags | map->dm_segs[seg].ds_len;
			if (seg == 0)
				cmdstat |= RE_TDESC_CMD_SOF;
			else
				cmdstat |= RE_TDESC_CMD_OWN;
			if (curdesc == (RE_TX_DESC_CNT(sc) - 1))
				cmdstat |= RE_TDESC_CMD_EOR;
			if (seg == nsegs - 1) {
				cmdstat |= RE_TDESC_CMD_EOF;
				lastdesc = curdesc;
			}
			d->re_cmdstat = htole32(cmdstat);
			RE_TXDESCSYNC(sc, curdesc,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}
		if (__predict_false(pad)) {
			d = &sc->re_ldata.re_tx_list[curdesc];
			d->re_vlanctl = htole32(vlanctl);
			re_set_bufaddr(d, RE_TXPADDADDR(sc));
			cmdstat = re_flags |
			    RE_TDESC_CMD_OWN | RE_TDESC_CMD_EOF |
			    (RE_IP4CSUMTX_PADLEN + 1 - m->m_pkthdr.len);
			if (curdesc == (RE_TX_DESC_CNT(sc) - 1))
				cmdstat |= RE_TDESC_CMD_EOR;
			d->re_cmdstat = htole32(cmdstat);
			RE_TXDESCSYNC(sc, curdesc,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			lastdesc = curdesc;
			curdesc = RE_NEXT_TX_DESC(sc, curdesc);
		}
		KASSERT(lastdesc != -1);

		/* Transfer ownership of packet to the chip. */

		sc->re_ldata.re_tx_list[startdesc].re_cmdstat |=
		    htole32(RE_TDESC_CMD_OWN);
		RE_TXDESCSYNC(sc, startdesc,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* update info of TX queue and descriptors */
		txq->txq_mbuf = m;
		txq->txq_descidx = lastdesc;
		txq->txq_nsegs = nsegs;

		sc->re_ldata.re_txq_free--;
		sc->re_ldata.re_tx_free -= nsegs;
		sc->re_ldata.re_tx_nextfree = curdesc;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m);
	}

	if (sc->re_ldata.re_txq_free < ofree) {
		/*
		 * TX packets are enqueued.
		 */
		sc->re_ldata.re_txq_prodidx = idx;

		/*
		 * Start the transmitter to poll.
		 *
		 * RealTek put the TX poll request register in a different
		 * location on the 8169 gigE chip. I don't know why.
		 */
		if ((sc->sc_quirk & RTKQ_8139CPLUS) != 0)
			CSR_WRITE_1(sc, RTK_TXSTART, RTK_TXSTART_START);
		else
			CSR_WRITE_1(sc, RTK_GTXSTART, RTK_TXSTART_START);

		/*
		 * Use the countdown timer for interrupt moderation.
		 * 'TX done' interrupts are disabled. Instead, we reset the
		 * countdown timer, which will begin counting until it hits
		 * the value in the TIMERINT register, and then trigger an
		 * interrupt. Each time we write to the TIMERCNT register,
		 * the timer count is reset to 0.
		 */
		CSR_WRITE_4(sc, RTK_TIMERCNT, 1);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}
}

static int
re_init(struct ifnet *ifp)
{
	struct rtk_softc *sc = ifp->if_softc;
	uint32_t rxcfg = 0;
	uint16_t cfg;
	int error;
#ifdef RE_USE_EECMD
	const uint8_t *enaddr;
	uint32_t reg;
#endif

	if ((error = re_enable(sc)) != 0)
		goto out;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	re_stop(ifp, 0);

	re_reset(sc);

	/*
	 * Enable C+ RX and TX mode, as well as VLAN stripping and
	 * RX checksum offload. We must configure the C+ register
	 * before all others.
	 */
	cfg = RE_CPLUSCMD_PCI_MRW;

	/*
	 * XXX: For old 8169 set bit 14.
	 *      For 8169S/8110S and above, do not set bit 14.
	 */
	if ((sc->sc_quirk & RTKQ_8169NONS) != 0)
		cfg |= (0x1 << 14);

	if ((sc->ethercom.ec_capenable & ETHERCAP_VLAN_HWTAGGING) != 0)
		cfg |= RE_CPLUSCMD_VLANSTRIP;
	if ((ifp->if_capenable & (IFCAP_CSUM_IPv4_Rx |
	     IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_UDPv4_Rx)) != 0)
		cfg |= RE_CPLUSCMD_RXCSUM_ENB;
	if ((sc->sc_quirk & RTKQ_MACSTAT) != 0) {
		cfg |= RE_CPLUSCMD_MACSTAT_DIS;
		cfg |= RE_CPLUSCMD_TXENB;
	} else
		cfg |= RE_CPLUSCMD_RXENB | RE_CPLUSCMD_TXENB;

	CSR_WRITE_2(sc, RTK_CPLUS_CMD, cfg);

	/* XXX: from Realtek-supplied Linux driver. Wholly undocumented. */
	if ((sc->sc_quirk & RTKQ_8139CPLUS) == 0)
		CSR_WRITE_2(sc, RTK_IM, 0x0000);

	DELAY(10000);

#ifdef RE_USE_EECMD
	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_WRITECFG);
	enaddr = CLLADDR(ifp->if_sadl);
	reg = enaddr[0] | (enaddr[1] << 8) |
	    (enaddr[2] << 16) | (enaddr[3] << 24);
	CSR_WRITE_4(sc, RTK_IDR0, reg);
	reg = enaddr[4] | (enaddr[5] << 8);
	CSR_WRITE_4(sc, RTK_IDR4, reg);
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_OFF);
#endif

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

	/*
	 * Load the addresses of the RX and TX lists into the chip.
	 */
	CSR_WRITE_4(sc, RTK_RXLIST_ADDR_HI,
	    RE_ADDR_HI(sc->re_ldata.re_rx_list_map->dm_segs[0].ds_addr));
	CSR_WRITE_4(sc, RTK_RXLIST_ADDR_LO,
	    RE_ADDR_LO(sc->re_ldata.re_rx_list_map->dm_segs[0].ds_addr));

	CSR_WRITE_4(sc, RTK_TXLIST_ADDR_HI,
	    RE_ADDR_HI(sc->re_ldata.re_tx_list_map->dm_segs[0].ds_addr));
	CSR_WRITE_4(sc, RTK_TXLIST_ADDR_LO,
	    RE_ADDR_LO(sc->re_ldata.re_tx_list_map->dm_segs[0].ds_addr));

	if (sc->sc_quirk & RTKQ_RXDV_GATED) {
		CSR_WRITE_4(sc, RTK_MISC,
		    CSR_READ_4(sc, RTK_MISC) & ~RTK_MISC_RXDV_GATED_EN);
	}
		
	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB | RTK_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	if (sc->re_testmode && (sc->sc_quirk & RTKQ_8169NONS) != 0) {
		/* test mode is needed only for old 8169 */
		CSR_WRITE_4(sc, RTK_TXCFG,
		    RE_TXCFG_CONFIG | RTK_LOOPTEST_ON);
	} else
		CSR_WRITE_4(sc, RTK_TXCFG, RE_TXCFG_CONFIG);

	CSR_WRITE_1(sc, RTK_EARLY_TX_THRESH, 16);

	CSR_WRITE_4(sc, RTK_RXCFG, RE_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RTK_RXCFG);
	rxcfg |= RTK_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxcfg |= RTK_RXCFG_RX_ALLPHYS;
	else
		rxcfg &= ~RTK_RXCFG_RX_ALLPHYS;
	CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		rxcfg |= RTK_RXCFG_RX_BROAD;
	else
		rxcfg &= ~RTK_RXCFG_RX_BROAD;
	CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);

	/*
	 * Program the multicast filter, if necessary.
	 */
	rtk_setmulti(sc);

	/*
	 * Enable interrupts.
	 */
	if (sc->re_testmode)
		CSR_WRITE_2(sc, RTK_IMR, 0);
	else
		CSR_WRITE_2(sc, RTK_IMR, RTK_INTRS_CPLUS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RTK_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB | RTK_CMD_RX_ENB);
#endif

	/*
	 * Initialize the timer interrupt register so that
	 * a timer interrupt will be generated once the timer
	 * reaches a certain number of ticks. The timer is
	 * reloaded on each transmit. This gives us TX interrupt
	 * moderation, which dramatically improves TX frame rate.
	 */

	if ((sc->sc_quirk & RTKQ_8139CPLUS) != 0)
		CSR_WRITE_4(sc, RTK_TIMERINT, 0x400);
	else {
		CSR_WRITE_4(sc, RTK_TIMERINT_8169, 0x800);

		/*
		 * For 8169 gigE NICs, set the max allowed RX packet
		 * size so we can receive jumbo frames.
		 */
		CSR_WRITE_2(sc, RTK_MAXRXPKTLEN, 16383);
	}

	if (sc->re_testmode)
		return 0;

	CSR_WRITE_1(sc, RTK_CFG1, RTK_CFG1_DRVLOAD);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->rtk_tick_ch, hz, re_tick, sc);

 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		printf("%s: interface not running\n",
		    device_xname(sc->sc_dev));
	}

	return error;
}

static int
re_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct rtk_softc *sc = ifp->if_softc;
	struct ifreq *ifr = data;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFMTU:
		/*
		 * Disable jumbo frames if it's not supported.
		 */
		if ((sc->sc_quirk & RTKQ_NOJUMBO) != 0 &&
		    ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
			break;
		}

		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ETHERMTU_JUMBO)
			error = EINVAL;
		else if ((error = ifioctl_common(ifp, command, data)) ==
		    ENETRESET)
			error = 0;
		break;
	default:
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			break;

		error = 0;

		if (command == SIOCSIFCAP)
			error = (*ifp->if_init)(ifp);
		else if (command != SIOCADDMULTI && command != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING)
			rtk_setmulti(sc);
		break;
	}

	splx(s);

	return error;
}

static void
re_watchdog(struct ifnet *ifp)
{
	struct rtk_softc *sc;
	int s;

	sc = ifp->if_softc;
	s = splnet();
	printf("%s: watchdog timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;

	re_txeof(sc);
	re_rxeof(sc);

	re_init(ifp);

	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
re_stop(struct ifnet *ifp, int disable)
{
	int i;
	struct rtk_softc *sc = ifp->if_softc;

	callout_stop(&sc->rtk_tick_ch);

	mii_down(&sc->mii);

	if ((sc->sc_quirk & RTKQ_CMDSTOP) != 0)
		CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_STOPREQ | RTK_CMD_TX_ENB |
		    RTK_CMD_RX_ENB);
	else
		CSR_WRITE_1(sc, RTK_COMMAND, 0x00);
	DELAY(1000);
	CSR_WRITE_2(sc, RTK_IMR, 0x0000);
	CSR_WRITE_2(sc, RTK_ISR, 0xFFFF);

	if (sc->re_head != NULL) {
		m_freem(sc->re_head);
		sc->re_head = sc->re_tail = NULL;
	}

	/* Free the TX list buffers. */
	for (i = 0; i < RE_TX_QLEN; i++) {
		if (sc->re_ldata.re_txq[i].txq_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->re_ldata.re_txq[i].txq_dmamap);
			m_freem(sc->re_ldata.re_txq[i].txq_mbuf);
			sc->re_ldata.re_txq[i].txq_mbuf = NULL;
		}
	}

	/* Free the RX list buffers. */
	for (i = 0; i < RE_RX_DESC_CNT; i++) {
		if (sc->re_ldata.re_rxsoft[i].rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->re_ldata.re_rxsoft[i].rxs_dmamap);
			m_freem(sc->re_ldata.re_rxsoft[i].rxs_mbuf);
			sc->re_ldata.re_rxsoft[i].rxs_mbuf = NULL;
		}
	}

	if (disable)
		re_disable(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}
