/*	$NetBSD: rtl81x9.c,v 1.96 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *
 *	FreeBSD Id: if_rl.c,v 1.17 1999/06/19 20:17:37 wpaul Exp
 */

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the RealTek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The RealTek 8139 PCI NIC redefines the meaning of 'low end.' This is
 * probably the worst PCI ethernet controller ever made, with the possible
 * exception of the FEAST chip made by SMC. The 8139 supports bus-master
 * DMA, but it has a terrible interface that nullifies any performance
 * gains that bus-master DMA usually offers.
 *
 * For transmission, the chip offers a series of four TX descriptor
 * registers. Each transmit frame must be in a contiguous buffer, aligned
 * on a longword (32-bit) boundary. This means we almost always have to
 * do mbuf copies in order to transmit a frame, except in the unlikely
 * case where a) the packet fits into a single mbuf, and b) the packet
 * is 32-bit aligned within the mbuf's data area. The presence of only
 * four descriptor registers means that we can never have more than four
 * packets queued for transmission at any one time.
 *
 * Reception is not much better. The driver has to allocate a single large
 * buffer area (up to 64K in size) into which the chip will DMA received
 * frames. Because we don't know where within this region received packets
 * will begin or end, we have no choice but to copy data from the buffer
 * area into mbufs in order to pass the packets up to the higher protocol
 * levels.
 *
 * It's impossible given this rotten design to really achieve decent
 * performance at 100Mbps, unless you happen to have a 400MHz PII or
 * some equally overmuscled CPU to drive it.
 *
 * On the bright side, the 8139 does have a built-in PHY, although
 * rather than using an MDIO serial interface like most other NICs, the
 * PHY registers are directly accessible through the 8139's register
 * space. The 8139 supports autonegotiation, as well as a 64-bit multicast
 * filter.
 *
 * The 8129 chip is an older version of the 8139 that uses an external PHY
 * chip. The 8129 has a serial MDIO interface for accessing the MII where
 * the 8139 lets you directly access the on-board PHY registers. We need
 * to select which interface to use depending on the chip type.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtl81x9.c,v 1.96 2015/04/13 16:33:24 riastradh Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <sys/rndsource.h>

#include <sys/bus.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

static void rtk_reset(struct rtk_softc *);
static void rtk_rxeof(struct rtk_softc *);
static void rtk_txeof(struct rtk_softc *);
static void rtk_start(struct ifnet *);
static int rtk_ioctl(struct ifnet *, u_long, void *);
static int rtk_init(struct ifnet *);
static void rtk_stop(struct ifnet *, int);

static void rtk_watchdog(struct ifnet *);

static void rtk_eeprom_putbyte(struct rtk_softc *, int, int);
static void rtk_mii_sync(struct rtk_softc *);
static void rtk_mii_send(struct rtk_softc *, uint32_t, int);
static int rtk_mii_readreg(struct rtk_softc *, struct rtk_mii_frame *);
static int rtk_mii_writereg(struct rtk_softc *, struct rtk_mii_frame *);

static int rtk_phy_readreg(device_t, int, int);
static void rtk_phy_writereg(device_t, int, int, int);
static void rtk_phy_statchg(struct ifnet *);
static void rtk_tick(void *);

static int rtk_enable(struct rtk_softc *);
static void rtk_disable(struct rtk_softc *);

static void rtk_list_tx_init(struct rtk_softc *);

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RTK_EECMD,			\
		CSR_READ_1(sc, RTK_EECMD) | (x))

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RTK_EECMD,			\
		CSR_READ_1(sc, RTK_EECMD) & ~(x))

#define EE_DELAY()	DELAY(100)

#define ETHER_PAD_LEN (ETHER_MIN_LEN - ETHER_CRC_LEN)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
rtk_eeprom_putbyte(struct rtk_softc *sc, int addr, int addr_len)
{
	int d, i;

	d = (RTK_EECMD_READ << addr_len) | addr;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = RTK_EECMD_LEN + addr_len; i > 0; i--) {
		if (d & (1 << (i - 1))) {
			EE_SET(RTK_EE_DATAIN);
		} else {
			EE_CLR(RTK_EE_DATAIN);
		}
		EE_DELAY();
		EE_SET(RTK_EE_CLK);
		EE_DELAY();
		EE_CLR(RTK_EE_CLK);
		EE_DELAY();
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
uint16_t
rtk_read_eeprom(struct rtk_softc *sc, int addr, int addr_len)
{
	uint16_t word;
	int i;

	/* Enter EEPROM access mode. */
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_PROGRAM);
	EE_DELAY();
	EE_SET(RTK_EE_SEL);

	/*
	 * Send address of word we want to read.
	 */
	rtk_eeprom_putbyte(sc, addr, addr_len);

	/*
	 * Start reading bits from EEPROM.
	 */
	word = 0;
	for (i = 16; i > 0; i--) {
		EE_SET(RTK_EE_CLK);
		EE_DELAY();
		if (CSR_READ_1(sc, RTK_EECMD) & RTK_EE_DATAOUT)
			word |= 1 << (i - 1);
		EE_CLR(RTK_EE_CLK);
		EE_DELAY();
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_OFF);

	return word;
}

/*
 * MII access routines are provided for the 8129, which
 * doesn't have a built-in PHY. For the 8139, we fake things
 * up by diverting rtk_phy_readreg()/rtk_phy_writereg() to the
 * direct access PHY registers.
 */
#define MII_SET(x)					\
	CSR_WRITE_1(sc, RTK_MII,			\
		CSR_READ_1(sc, RTK_MII) | (x))

#define MII_CLR(x)					\
	CSR_WRITE_1(sc, RTK_MII,			\
		CSR_READ_1(sc, RTK_MII) & ~(x))

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
rtk_mii_sync(struct rtk_softc *sc)
{
	int i;

	MII_SET(RTK_MII_DIR|RTK_MII_DATAOUT);

	for (i = 0; i < 32; i++) {
		MII_SET(RTK_MII_CLK);
		DELAY(1);
		MII_CLR(RTK_MII_CLK);
		DELAY(1);
	}
}

/*
 * Clock a series of bits through the MII.
 */
static void
rtk_mii_send(struct rtk_softc *sc, uint32_t bits, int cnt)
{
	int i;

	MII_CLR(RTK_MII_CLK);

	for (i = cnt; i > 0; i--) {
		if (bits & (1 << (i - 1))) {
			MII_SET(RTK_MII_DATAOUT);
		} else {
			MII_CLR(RTK_MII_DATAOUT);
		}
		DELAY(1);
		MII_CLR(RTK_MII_CLK);
		DELAY(1);
		MII_SET(RTK_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int
rtk_mii_readreg(struct rtk_softc *sc, struct rtk_mii_frame *frame)
{
	int i, ack, s;

	s = splnet();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = RTK_MII_STARTDELIM;
	frame->mii_opcode = RTK_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;

	CSR_WRITE_2(sc, RTK_MII, 0);

	/*
	 * Turn on data xmit.
	 */
	MII_SET(RTK_MII_DIR);

	rtk_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	rtk_mii_send(sc, frame->mii_stdelim, 2);
	rtk_mii_send(sc, frame->mii_opcode, 2);
	rtk_mii_send(sc, frame->mii_phyaddr, 5);
	rtk_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((RTK_MII_CLK|RTK_MII_DATAOUT));
	DELAY(1);
	MII_SET(RTK_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	MII_CLR(RTK_MII_DIR);

	/* Check for ack */
	MII_CLR(RTK_MII_CLK);
	DELAY(1);
	ack = CSR_READ_2(sc, RTK_MII) & RTK_MII_DATAIN;
	MII_SET(RTK_MII_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for (i = 0; i < 16; i++) {
			MII_CLR(RTK_MII_CLK);
			DELAY(1);
			MII_SET(RTK_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 16; i > 0; i--) {
		MII_CLR(RTK_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, RTK_MII) & RTK_MII_DATAIN)
				frame->mii_data |= 1 << (i - 1);
			DELAY(1);
		}
		MII_SET(RTK_MII_CLK);
		DELAY(1);
	}

 fail:
	MII_CLR(RTK_MII_CLK);
	DELAY(1);
	MII_SET(RTK_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return 1;
	return 0;
}

/*
 * Write to a PHY register through the MII.
 */
static int
rtk_mii_writereg(struct rtk_softc *sc, struct rtk_mii_frame *frame)
{
	int s;

	s = splnet();
	/*
	 * Set up frame for TX.
	 */
	frame->mii_stdelim = RTK_MII_STARTDELIM;
	frame->mii_opcode = RTK_MII_WRITEOP;
	frame->mii_turnaround = RTK_MII_TURNAROUND;

	/*
	 * Turn on data output.
	 */
	MII_SET(RTK_MII_DIR);

	rtk_mii_sync(sc);

	rtk_mii_send(sc, frame->mii_stdelim, 2);
	rtk_mii_send(sc, frame->mii_opcode, 2);
	rtk_mii_send(sc, frame->mii_phyaddr, 5);
	rtk_mii_send(sc, frame->mii_regaddr, 5);
	rtk_mii_send(sc, frame->mii_turnaround, 2);
	rtk_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(RTK_MII_CLK);
	DELAY(1);
	MII_CLR(RTK_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(RTK_MII_DIR);

	splx(s);

	return 0;
}

static int
rtk_phy_readreg(device_t self, int phy, int reg)
{
	struct rtk_softc *sc = device_private(self);
	struct rtk_mii_frame frame;
	int rval;
	int rtk8139_reg;

	if ((sc->sc_quirk & RTKQ_8129) == 0) {
		if (phy != 7)
			return 0;

		switch (reg) {
		case MII_BMCR:
			rtk8139_reg = RTK_BMCR;
			break;
		case MII_BMSR:
			rtk8139_reg = RTK_BMSR;
			break;
		case MII_ANAR:
			rtk8139_reg = RTK_ANAR;
			break;
		case MII_ANER:
			rtk8139_reg = RTK_ANER;
			break;
		case MII_ANLPAR:
			rtk8139_reg = RTK_LPAR;
			break;
		default:
#if 0
			printf("%s: bad phy register\n", device_xname(self));
#endif
			return 0;
		}
		rval = CSR_READ_2(sc, rtk8139_reg);
		return rval;
	}

	memset(&frame, 0, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	rtk_mii_readreg(sc, &frame);

	return frame.mii_data;
}

static void
rtk_phy_writereg(device_t self, int phy, int reg, int data)
{
	struct rtk_softc *sc = device_private(self);
	struct rtk_mii_frame frame;
	int rtk8139_reg;

	if ((sc->sc_quirk & RTKQ_8129) == 0) {
		if (phy != 7)
			return;

		switch (reg) {
		case MII_BMCR:
			rtk8139_reg = RTK_BMCR;
			break;
		case MII_BMSR:
			rtk8139_reg = RTK_BMSR;
			break;
		case MII_ANAR:
			rtk8139_reg = RTK_ANAR;
			break;
		case MII_ANER:
			rtk8139_reg = RTK_ANER;
			break;
		case MII_ANLPAR:
			rtk8139_reg = RTK_LPAR;
			break;
		default:
#if 0
			printf("%s: bad phy register\n", device_xname(self));
#endif
			return;
		}
		CSR_WRITE_2(sc, rtk8139_reg, data);
		return;
	}

	memset(&frame, 0, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	rtk_mii_writereg(sc, &frame);
}

static void
rtk_phy_statchg(struct ifnet *ifp)
{

	/* Nothing to do. */
}

#define	rtk_calchash(addr) \
	(ether_crc32_be((addr), ETHER_ADDR_LEN) >> 26)

/*
 * Program the 64-bit multicast hash filter.
 */
void
rtk_setmulti(struct rtk_softc *sc)
{
	struct ifnet *ifp;
	uint32_t hashes[2] = { 0, 0 };
	uint32_t rxfilt;
	struct ether_multi *enm;
	struct ether_multistep step;
	int h, mcnt;

	ifp = &sc->ethercom.ec_if;

	rxfilt = CSR_READ_4(sc, RTK_RXCFG);

	if (ifp->if_flags & IFF_PROMISC) {
 allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		rxfilt |= RTK_RXCFG_RX_MULTI;
		CSR_WRITE_4(sc, RTK_RXCFG, rxfilt);
		CSR_WRITE_4(sc, RTK_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, RTK_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, RTK_MAR0, 0);
	CSR_WRITE_4(sc, RTK_MAR4, 0);

	/* now program new ones */
	ETHER_FIRST_MULTI(step, &sc->ethercom, enm);
	mcnt = 0;
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = rtk_calchash(enm->enm_addrlo);
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (mcnt)
		rxfilt |= RTK_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RTK_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RTK_RXCFG, rxfilt);

	/*
	 * For some unfathomable reason, RealTek decided to reverse
	 * the order of the multicast hash registers in the PCI Express
	 * parts. This means we have to write the hash pattern in reverse
	 * order for those devices.
	 */
	if ((sc->sc_quirk & RTKQ_PCIE) != 0) {
		CSR_WRITE_4(sc, RTK_MAR0, bswap32(hashes[1]));
		CSR_WRITE_4(sc, RTK_MAR4, bswap32(hashes[0]));
	} else {
		CSR_WRITE_4(sc, RTK_MAR0, hashes[0]);
		CSR_WRITE_4(sc, RTK_MAR4, hashes[1]);
	}
}

void
rtk_reset(struct rtk_softc *sc)
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
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
rtk_attach(struct rtk_softc *sc)
{
	device_t self = sc->sc_dev;
	struct ifnet *ifp;
	struct rtk_tx_desc *txd;
	uint16_t val;
	uint8_t eaddr[ETHER_ADDR_LEN];
	int error;
	int i, addr_len;

	callout_init(&sc->rtk_tick_ch, 0);

	/*
	 * Check EEPROM type 9346 or 9356.
	 */
	if (rtk_read_eeprom(sc, RTK_EE_ID, RTK_EEADDR_LEN1) == 0x8129)
		addr_len = RTK_EEADDR_LEN1;
	else
		addr_len = RTK_EEADDR_LEN0;

	/*
	 * Get station address.
	 */
	val = rtk_read_eeprom(sc, RTK_EE_EADDR0, addr_len);
	eaddr[0] = val & 0xff;
	eaddr[1] = val >> 8;
	val = rtk_read_eeprom(sc, RTK_EE_EADDR1, addr_len);
	eaddr[2] = val & 0xff;
	eaddr[3] = val >> 8;
	val = rtk_read_eeprom(sc, RTK_EE_EADDR2, addr_len);
	eaddr[4] = val & 0xff;
	eaddr[5] = val >> 8;

	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    RTK_RXBUFLEN + 16, PAGE_SIZE, 0, &sc->sc_dmaseg, 1, &sc->sc_dmanseg,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(self,
		    "can't allocate recv buffer, error = %d\n", error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dmaseg, sc->sc_dmanseg,
	    RTK_RXBUFLEN + 16, (void **)&sc->rtk_rx_buf,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self,
		    "can't map recv buffer, error = %d\n", error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    RTK_RXBUFLEN + 16, 1, RTK_RXBUFLEN + 16, 0, BUS_DMA_NOWAIT,
	    &sc->recv_dmamap)) != 0) {
		aprint_error_dev(self,
		    "can't create recv buffer DMA map, error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->recv_dmamap,
	    sc->rtk_rx_buf, RTK_RXBUFLEN + 16,
	    NULL, BUS_DMA_READ|BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(self,
		    "can't load recv buffer DMA map, error = %d\n", error);
		goto fail_3;
	}

	for (i = 0; i < RTK_TX_LIST_CNT; i++) {
		txd = &sc->rtk_tx_descs[i];
		if ((error = bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &txd->txd_dmamap)) != 0) {
			aprint_error_dev(self,
			    "can't create snd buffer DMA map, error = %d\n",
			    error);
			goto fail_4;
		}
		txd->txd_txaddr = RTK_TXADDR0 + (i * 4);
		txd->txd_txstat = RTK_TXSTAT0 + (i * 4);
	}
	SIMPLEQ_INIT(&sc->rtk_tx_free);
	SIMPLEQ_INIT(&sc->rtk_tx_dirty);

	/*
	 * From this point forward, the attachment cannot fail. A failure
	 * before this releases all resources thar may have been
	 * allocated.
	 */
	sc->sc_flags |= RTK_ATTACHED;

	/* Reset the adapter. */
	rtk_reset(sc);

	aprint_normal_dev(self, "Ethernet address %s\n", ether_sprintf(eaddr));

	ifp = &sc->ethercom.ec_if;
	ifp->if_softc = sc;
	strcpy(ifp->if_xname, device_xname(self));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rtk_ioctl;
	ifp->if_start = rtk_start;
	ifp->if_watchdog = rtk_watchdog;
	ifp->if_init = rtk_init;
	ifp->if_stop = rtk_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Do ifmedia setup.
	 */
	sc->mii.mii_ifp = ifp;
	sc->mii.mii_readreg = rtk_phy_readreg;
	sc->mii.mii_writereg = rtk_phy_writereg;
	sc->mii.mii_statchg = rtk_phy_statchg;
	sc->ethercom.ec_mii = &sc->mii;
	ifmedia_init(&sc->mii.mii_media, IFM_IMASK, ether_mediachange,
	    ether_mediastatus);
	mii_attach(self, &sc->mii, 0xffffffff,
	    MII_PHY_ANY, MII_OFFSET_ANY, 0);

	/* Choose a default media. */
	if (LIST_FIRST(&sc->mii.mii_phys) == NULL) {
		ifmedia_add(&sc->mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->mii.mii_media, IFM_ETHER|IFM_NONE);
	} else {
		ifmedia_set(&sc->mii.mii_media, IFM_ETHER|IFM_AUTO);
	}

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, eaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	return;
 fail_4:
	for (i = 0; i < RTK_TX_LIST_CNT; i++) {
		txd = &sc->rtk_tx_descs[i];
		if (txd->txd_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat, txd->txd_dmamap);
	}
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->recv_dmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, sc->rtk_rx_buf,
	    RTK_RXBUFLEN + 16);
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dmaseg, sc->sc_dmanseg);
 fail_0:
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static void
rtk_list_tx_init(struct rtk_softc *sc)
{
	struct rtk_tx_desc *txd;
	int i;

	while ((txd = SIMPLEQ_FIRST(&sc->rtk_tx_dirty)) != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->rtk_tx_dirty, txd_q);
	while ((txd = SIMPLEQ_FIRST(&sc->rtk_tx_free)) != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->rtk_tx_free, txd_q);

	for (i = 0; i < RTK_TX_LIST_CNT; i++) {
		txd = &sc->rtk_tx_descs[i];
		CSR_WRITE_4(sc, txd->txd_txaddr, 0);
		SIMPLEQ_INSERT_TAIL(&sc->rtk_tx_free, txd, txd_q);
	}
}

/*
 * rtk_activate:
 *     Handle device activation/deactivation requests.
 */
int
rtk_activate(device_t self, enum devact act)
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
 * rtk_detach:
 *     Detach a rtk interface.
 */
int
rtk_detach(struct rtk_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	struct rtk_tx_desc *txd;
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

	for (i = 0; i < RTK_TX_LIST_CNT; i++) {
		txd = &sc->rtk_tx_descs[i];
		if (txd->txd_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat, txd->txd_dmamap);
	}
	bus_dmamap_destroy(sc->sc_dmat, sc->recv_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, sc->rtk_rx_buf,
	    RTK_RXBUFLEN + 16);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dmaseg, sc->sc_dmanseg);

	/* we don't want to run again */
	sc->sc_flags &= ~RTK_ATTACHED;

	return 0;
}

/*
 * rtk_enable:
 *     Enable the RTL81X9 chip.
 */
int
rtk_enable(struct rtk_softc *sc)
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
 * rtk_disable:
 *     Disable the RTL81X9 chip.
 */
void
rtk_disable(struct rtk_softc *sc)
{

	if (RTK_IS_ENABLED(sc) && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_flags &= ~RTK_ENABLED;
	}
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * You know there's something wrong with a PCI bus-master chip design.
 *
 * The receive operation is badly documented in the datasheet, so I'll
 * attempt to document it here. The driver provides a buffer area and
 * places its base address in the RX buffer start address register.
 * The chip then begins copying frames into the RX buffer. Each frame
 * is preceded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 *
 * Note: to make the Alpha happy, the frame payload needs to be aligned
 * on a 32-bit boundary. To achieve this, we copy the data to mbuf
 * shifted forward 2 bytes.
 */
static void
rtk_rxeof(struct rtk_softc *sc)
{
	struct mbuf *m;
	struct ifnet *ifp;
	uint8_t *rxbufpos, *dst;
	u_int total_len, wrap;
	uint32_t rxstat;
	uint16_t cur_rx, new_rx;
	uint16_t limit;
	uint16_t rx_bytes, max_bytes;

	ifp = &sc->ethercom.ec_if;

	cur_rx = (CSR_READ_2(sc, RTK_CURRXADDR) + 16) % RTK_RXBUFLEN;

	/* Do not try to read past this point. */
	limit = CSR_READ_2(sc, RTK_CURRXBUF) % RTK_RXBUFLEN;

	if (limit < cur_rx)
		max_bytes = (RTK_RXBUFLEN - cur_rx) + limit;
	else
		max_bytes = limit - cur_rx;
	rx_bytes = 0;

	while ((CSR_READ_1(sc, RTK_COMMAND) & RTK_CMD_EMPTY_RXBUF) == 0) {
		rxbufpos = sc->rtk_rx_buf + cur_rx;
		bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap, cur_rx,
		    RTK_RXSTAT_LEN, BUS_DMASYNC_POSTREAD);
		rxstat = le32toh(*(uint32_t *)rxbufpos);
		bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap, cur_rx,
		    RTK_RXSTAT_LEN, BUS_DMASYNC_PREREAD);

		/*
		 * Here's a totally undocumented fact for you. When the
		 * RealTek chip is in the process of copying a packet into
		 * RAM for you, the length will be 0xfff0. If you spot a
		 * packet header with this value, you need to stop. The
		 * datasheet makes absolutely no mention of this and
		 * RealTek should be shot for this.
		 */
		total_len = rxstat >> 16;
		if (total_len == RTK_RXSTAT_UNFINISHED)
			break;

		if ((rxstat & RTK_RXSTAT_RXOK) == 0 ||
		    total_len < ETHER_MIN_LEN ||
		    total_len > (MCLBYTES - RTK_ETHER_ALIGN)) {
			ifp->if_ierrors++;

			/*
			 * submitted by:[netbsd-pcmcia:00484]
			 *	Takahiro Kambe <taca@sky.yamashina.kyoto.jp>
			 * obtain from:
			 *     FreeBSD if_rl.c rev 1.24->1.25
			 *
			 */
#if 0
			if (rxstat & (RTK_RXSTAT_BADSYM|RTK_RXSTAT_RUNT|
			    RTK_RXSTAT_GIANT|RTK_RXSTAT_CRCERR|
			    RTK_RXSTAT_ALIGNERR)) {
				CSR_WRITE_2(sc, RTK_COMMAND, RTK_CMD_TX_ENB);
				CSR_WRITE_2(sc, RTK_COMMAND,
				    RTK_CMD_TX_ENB|RTK_CMD_RX_ENB);
				CSR_WRITE_4(sc, RTK_RXCFG, RTK_RXCFG_CONFIG);
				CSR_WRITE_4(sc, RTK_RXADDR,
				    sc->recv_dmamap->dm_segs[0].ds_addr);
				cur_rx = 0;
			}
			break;
#else
			rtk_init(ifp);
			return;
#endif
		}

		/* No errors; receive the packet. */
		rx_bytes += total_len + RTK_RXSTAT_LEN;

		/*
		 * Avoid trying to read more bytes than we know
		 * the chip has prepared for us.
		 */
		if (rx_bytes > max_bytes)
			break;

		/*
		 * Skip the status word, wrapping around to the beginning
		 * of the Rx area, if necessary.
		 */
		cur_rx = (cur_rx + RTK_RXSTAT_LEN) % RTK_RXBUFLEN;
		rxbufpos = sc->rtk_rx_buf + cur_rx;

		/*
		 * Compute the number of bytes at which the packet
		 * will wrap to the beginning of the ring buffer.
		 */
		wrap = RTK_RXBUFLEN - cur_rx;

		/*
		 * Compute where the next pending packet is.
		 */
		if (total_len > wrap)
			new_rx = total_len - wrap;
		else
			new_rx = cur_rx + total_len;
		/* Round up to 32-bit boundary. */
		new_rx = roundup2(new_rx, sizeof(uint32_t)) % RTK_RXBUFLEN;

		/*
		 * The RealTek chip includes the CRC with every
		 * incoming packet; trim it off here.
		 */
		total_len -= ETHER_CRC_LEN;

		/*
		 * Now allocate an mbuf (and possibly a cluster) to hold
		 * the packet. Note we offset the packet 2 bytes so that
		 * data after the Ethernet header will be 4-byte aligned.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			printf("%s: unable to allocate Rx mbuf\n",
			    device_xname(sc->sc_dev));
			ifp->if_ierrors++;
			goto next_packet;
		}
		if (total_len > (MHLEN - RTK_ETHER_ALIGN)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				printf("%s: unable to allocate Rx cluster\n",
				    device_xname(sc->sc_dev));
				ifp->if_ierrors++;
				m_freem(m);
				m = NULL;
				goto next_packet;
			}
		}
		m->m_data += RTK_ETHER_ALIGN;	/* for alignment */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;
		dst = mtod(m, void *);

		/*
		 * If the packet wraps, copy up to the wrapping point.
		 */
		if (total_len > wrap) {
			bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap,
			    cur_rx, wrap, BUS_DMASYNC_POSTREAD);
			memcpy(dst, rxbufpos, wrap);
			bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap,
			    cur_rx, wrap, BUS_DMASYNC_PREREAD);
			cur_rx = 0;
			rxbufpos = sc->rtk_rx_buf;
			total_len -= wrap;
			dst += wrap;
		}

		/*
		 * ...and now the rest.
		 */
		bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap,
		    cur_rx, total_len, BUS_DMASYNC_POSTREAD);
		memcpy(dst, rxbufpos, total_len);
		bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap,
		    cur_rx, total_len, BUS_DMASYNC_PREREAD);

 next_packet:
		CSR_WRITE_2(sc, RTK_CURRXADDR, (new_rx - 16) % RTK_RXBUFLEN);
		cur_rx = new_rx;

		if (m == NULL)
			continue;

		ifp->if_ipackets++;

		bpf_mtap(ifp, m);
		/* pass it on. */
		(*ifp->if_input)(ifp, m);
	}
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void
rtk_txeof(struct rtk_softc *sc)
{
	struct ifnet *ifp;
	struct rtk_tx_desc *txd;
	uint32_t txstat;

	ifp = &sc->ethercom.ec_if;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded.
	 */
	while ((txd = SIMPLEQ_FIRST(&sc->rtk_tx_dirty)) != NULL) {
		txstat = CSR_READ_4(sc, txd->txd_txstat);
		if ((txstat & (RTK_TXSTAT_TX_OK|
		    RTK_TXSTAT_TX_UNDERRUN|RTK_TXSTAT_TXABRT)) == 0)
			break;

		SIMPLEQ_REMOVE_HEAD(&sc->rtk_tx_dirty, txd_q);

		bus_dmamap_sync(sc->sc_dmat, txd->txd_dmamap, 0,
		    txd->txd_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txd->txd_dmamap);
		m_freem(txd->txd_mbuf);
		txd->txd_mbuf = NULL;

		ifp->if_collisions += (txstat & RTK_TXSTAT_COLLCNT) >> 24;

		if (txstat & RTK_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else {
			ifp->if_oerrors++;

			/*
			 * Increase Early TX threshold if underrun occurred.
			 * Increase step 64 bytes.
			 */
			if (txstat & RTK_TXSTAT_TX_UNDERRUN) {
#ifdef DEBUG
				printf("%s: transmit underrun;",
				    device_xname(sc->sc_dev));
#endif
				if (sc->sc_txthresh < RTK_TXTH_MAX) {
					sc->sc_txthresh += 2;
#ifdef DEBUG
					printf(" new threshold: %d bytes",
					    sc->sc_txthresh * 32);
#endif
				}
#ifdef DEBUG
				printf("\n");
#endif
			}
			if (txstat & (RTK_TXSTAT_TXABRT|RTK_TXSTAT_OUTOFWIN))
				CSR_WRITE_4(sc, RTK_TXCFG, RTK_TXCFG_CONFIG);
		}
		SIMPLEQ_INSERT_TAIL(&sc->rtk_tx_free, txd, txd_q);
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	/* Clear the timeout timer if there is no pending packet. */
	if (SIMPLEQ_EMPTY(&sc->rtk_tx_dirty))
		ifp->if_timer = 0;

}

int
rtk_intr(void *arg)
{
	struct rtk_softc *sc;
	struct ifnet *ifp;
	uint16_t status;
	int handled;

	sc = arg;
	ifp = &sc->ethercom.ec_if;

	if (!device_has_power(sc->sc_dev))
		return 0;

	/* Disable interrupts. */
	CSR_WRITE_2(sc, RTK_IMR, 0x0000);

	handled = 0;
	for (;;) {

		status = CSR_READ_2(sc, RTK_ISR);

		if (status == 0xffff)
			break; /* Card is gone... */

		if (status)
			CSR_WRITE_2(sc, RTK_ISR, status);

		if ((status & RTK_INTRS) == 0)
			break;

		handled = 1;

		if (status & RTK_ISR_RX_OK)
			rtk_rxeof(sc);

		if (status & RTK_ISR_RX_ERR)
			rtk_rxeof(sc);

		if (status & (RTK_ISR_TX_OK|RTK_ISR_TX_ERR))
			rtk_txeof(sc);

		if (status & RTK_ISR_SYSTEM_ERR) {
			rtk_reset(sc);
			rtk_init(ifp);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, RTK_IMR, RTK_INTRS);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		rtk_start(ifp);

	rnd_add_uint32(&sc->rnd_source, status);

	return handled;
}

/*
 * Main transmit routine.
 */

static void
rtk_start(struct ifnet *ifp)
{
	struct rtk_softc *sc;
	struct rtk_tx_desc *txd;
	struct mbuf *m_head, *m_new;
	int error, len;

	sc = ifp->if_softc;

	while ((txd = SIMPLEQ_FIRST(&sc->rtk_tx_free)) != NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		m_new = NULL;

		/*
		 * Load the DMA map.  If this fails, the packet didn't
		 * fit in one DMA segment, and we need to copy.  Note,
		 * the packet must also be aligned.
		 * if the packet is too small, copy it too, so we're sure
		 * so have enough room for the pad buffer.
		 */
		if ((mtod(m_head, uintptr_t) & 3) != 0 ||
		    m_head->m_pkthdr.len < ETHER_PAD_LEN ||
		    bus_dmamap_load_mbuf(sc->sc_dmat, txd->txd_dmamap,
			m_head, BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m_new, M_DONTWAIT, MT_DATA);
			if (m_new == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    device_xname(sc->sc_dev));
				break;
			}
			if (m_head->m_pkthdr.len > MHLEN) {
				MCLGET(m_new, M_DONTWAIT);
				if ((m_new->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n",
					    device_xname(sc->sc_dev));
					m_freem(m_new);
					break;
				}
			}
			m_copydata(m_head, 0, m_head->m_pkthdr.len,
			    mtod(m_new, void *));
			m_new->m_pkthdr.len = m_new->m_len =
			    m_head->m_pkthdr.len;
			if (m_head->m_pkthdr.len < ETHER_PAD_LEN) {
				memset(
				    mtod(m_new, char *) + m_head->m_pkthdr.len,
				    0, ETHER_PAD_LEN - m_head->m_pkthdr.len);
				m_new->m_pkthdr.len = m_new->m_len =
				    ETHER_PAD_LEN;
			}
			error = bus_dmamap_load_mbuf(sc->sc_dmat,
			    txd->txd_dmamap, m_new,
			    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n",
				    device_xname(sc->sc_dev), error);
				break;
			}
		}
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m_head);
		if (m_new != NULL) {
			m_freem(m_head);
			m_head = m_new;
		}
		txd->txd_mbuf = m_head;

		SIMPLEQ_REMOVE_HEAD(&sc->rtk_tx_free, txd_q);
		SIMPLEQ_INSERT_TAIL(&sc->rtk_tx_dirty, txd, txd_q);

		/*
		 * Transmit the frame.
		 */
		bus_dmamap_sync(sc->sc_dmat,
		    txd->txd_dmamap, 0, txd->txd_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		len = txd->txd_dmamap->dm_segs[0].ds_len;

		CSR_WRITE_4(sc, txd->txd_txaddr,
		    txd->txd_dmamap->dm_segs[0].ds_addr);
		CSR_WRITE_4(sc, txd->txd_txstat,
		    RTK_TXSTAT_THRESH(sc->sc_txthresh) | len);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}

	/*
	 * We broke out of the loop because all our TX slots are
	 * full. Mark the NIC as busy until it drains some of the
	 * packets from the queue.
	 */
	if (SIMPLEQ_EMPTY(&sc->rtk_tx_free))
		ifp->if_flags |= IFF_OACTIVE;
}

static int
rtk_init(struct ifnet *ifp)
{
	struct rtk_softc *sc = ifp->if_softc;
	int error, i;
	uint32_t rxcfg;

	if ((error = rtk_enable(sc)) != 0)
		goto out;

	/*
	 * Cancel pending I/O.
	 */
	rtk_stop(ifp, 0);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, RTK_IDR0 + i, CLLADDR(ifp->if_sadl)[i]);
	}

	/* Init the RX buffer pointer register. */
	bus_dmamap_sync(sc->sc_dmat, sc->recv_dmamap, 0,
	    sc->recv_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	CSR_WRITE_4(sc, RTK_RXADDR, sc->recv_dmamap->dm_segs[0].ds_addr);

	/* Init TX descriptors. */
	rtk_list_tx_init(sc);

	/* Init Early TX threshold. */
	sc->sc_txthresh = RTK_TXTH_256;
	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB|RTK_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	CSR_WRITE_4(sc, RTK_TXCFG, RTK_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RTK_RXCFG, RTK_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RTK_RXCFG);
	rxcfg |= RTK_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxcfg |= RTK_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RTK_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxcfg |= RTK_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RTK_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	rtk_setmulti(sc);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, RTK_IMR, RTK_INTRS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RTK_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB|RTK_CMD_RX_ENB);

	CSR_WRITE_1(sc, RTK_CFG1, RTK_CFG1_DRVLOAD|RTK_CFG1_FULLDUPLEX);

	/*
	 * Set current media.
	 */
	if ((error = ether_mediachange(ifp)) != 0)
		goto out;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->rtk_tick_ch, hz, rtk_tick, sc);

 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	}
	return error;
}

static int
rtk_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct rtk_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();
	error = ether_ioctl(ifp, command, data);
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed.  Set the
			 * hardware filter accordingly.
			 */
			rtk_setmulti(sc);
		}
		error = 0;
	}
	splx(s);

	return error;
}

static void
rtk_watchdog(struct ifnet *ifp)
{
	struct rtk_softc *sc;

	sc = ifp->if_softc;

	printf("%s: watchdog timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;
	rtk_txeof(sc);
	rtk_rxeof(sc);
	rtk_init(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
rtk_stop(struct ifnet *ifp, int disable)
{
	struct rtk_softc *sc = ifp->if_softc;
	struct rtk_tx_desc *txd;

	callout_stop(&sc->rtk_tick_ch);

	mii_down(&sc->mii);

	CSR_WRITE_1(sc, RTK_COMMAND, 0x00);
	CSR_WRITE_2(sc, RTK_IMR, 0x0000);

	/*
	 * Free the TX list buffers.
	 */
	while ((txd = SIMPLEQ_FIRST(&sc->rtk_tx_dirty)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->rtk_tx_dirty, txd_q);
		bus_dmamap_unload(sc->sc_dmat, txd->txd_dmamap);
		m_freem(txd->txd_mbuf);
		txd->txd_mbuf = NULL;
		CSR_WRITE_4(sc, txd->txd_txaddr, 0);
	}

	if (disable)
		rtk_disable(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

static void
rtk_tick(void *arg)
{
	struct rtk_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->mii);
	splx(s);

	callout_reset(&sc->rtk_tick_ch, hz, rtk_tick, sc);
}
