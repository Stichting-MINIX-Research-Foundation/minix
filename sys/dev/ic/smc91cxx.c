/*	$NetBSD: smc91cxx.c,v 1.90 2015/08/30 04:11:40 dholland Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 Gardner Buchanan <gbuchanan@shl.com>
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
 *	This product includes software developed by Gardner Buchanan.
 * 4. The name of Gardner Buchanan may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *   from FreeBSD Id: if_sn.c,v 1.4 1996/03/18 15:47:16 gardner Exp
 */

/*
 * Core driver for the SMC 91Cxx family of Ethernet chips.
 *
 * Memory allocation interrupt logic is drived from an SMC 91C90 driver
 * written for NetBSD/amiga by Michael Hitch.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smc91cxx.c,v 1.90 2015/08/30 04:11:40 dholland Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/rndsource.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_multi_stream_2 bus_space_write_multi_2
#define bus_space_write_multi_stream_4 bus_space_write_multi_4
#define bus_space_read_multi_stream_2  bus_space_read_multi_2
#define bus_space_read_multi_stream_4  bus_space_read_multi_4

#define bus_space_write_stream_4 bus_space_write_4
#define bus_space_read_stream_4  bus_space_read_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

/* XXX Hardware padding doesn't work yet(?) */
#define	SMC91CXX_SW_PAD

const char *smc91cxx_idstrs[] = {
	NULL,				/* 0 */
	NULL,				/* 1 */
	NULL,				/* 2 */
	"SMC91C90/91C92",		/* 3 */
	"SMC91C94/91C96",		/* 4 */
	"SMC91C95",			/* 5 */
	NULL,				/* 6 */
	"SMC91C100",			/* 7 */
	"SMC91C100FD",			/* 8 */
	"SMC91C111",			/* 9 */
	NULL,				/* 10 */
	NULL,				/* 11 */
	NULL,				/* 12 */
	NULL,				/* 13 */
	NULL,				/* 14 */
	NULL,				/* 15 */
};

/* Supported media types. */
static const int smc91cxx_media[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_5,
};
#define	NSMC91CxxMEDIA	(sizeof(smc91cxx_media) / sizeof(smc91cxx_media[0]))

/*
 * MII bit-bang glue.
 */
u_int32_t smc91cxx_mii_bitbang_read(device_t);
void smc91cxx_mii_bitbang_write(device_t, u_int32_t);

static const struct mii_bitbang_ops smc91cxx_mii_bitbang_ops = {
	smc91cxx_mii_bitbang_read,
	smc91cxx_mii_bitbang_write,
	{
		MR_MDO,		/* MII_BIT_MDO */
		MR_MDI,		/* MII_BIT_MDI */
		MR_MCLK,	/* MII_BIT_MDC */
		MR_MDOE,	/* MII_BIT_DIR_HOST_PHY */
		0,		/* MII_BIT_DIR_PHY_HOST */
	}
};

/* MII callbacks */
int	smc91cxx_mii_readreg(device_t, int, int);
void	smc91cxx_mii_writereg(device_t, int, int, int);
void	smc91cxx_statchg(struct ifnet *);
void	smc91cxx_tick(void *);

int	smc91cxx_mediachange(struct ifnet *);
void	smc91cxx_mediastatus(struct ifnet *, struct ifmediareq *);

int	smc91cxx_set_media(struct smc91cxx_softc *, int);

void	smc91cxx_init(struct smc91cxx_softc *);
void	smc91cxx_read(struct smc91cxx_softc *);
void	smc91cxx_reset(struct smc91cxx_softc *);
void	smc91cxx_start(struct ifnet *);
uint8_t	smc91cxx_copy_tx_frame(struct smc91cxx_softc *, struct mbuf *);
void	smc91cxx_resume(struct smc91cxx_softc *);
void	smc91cxx_stop(struct smc91cxx_softc *);
void	smc91cxx_watchdog(struct ifnet *);
int	smc91cxx_ioctl(struct ifnet *, u_long, void *);

static inline int ether_cmp(const void *, const void *);
static inline int
ether_cmp(const void *va, const void *vb)
{
	const u_int8_t *a = va;
	const u_int8_t *b = vb;

	return ((a[5] != b[5]) || (a[4] != b[4]) || (a[3] != b[3]) ||
		(a[2] != b[2]) || (a[1] != b[1]) || (a[0] != b[0]));
}

static inline void
smc91cxx_intr_mask_write(bus_space_tag_t bst, bus_space_handle_t bsh,
	uint8_t mask)
{
	KDASSERT((mask & IM_ERCV_INT) == 0);
#ifdef SMC91CXX_NO_BYTE_WRITE
	bus_space_write_2(bst, bsh, INTR_STAT_REG_B, mask << 8);
#else
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B, mask);
#endif
	KDASSERT(!(bus_space_read_1(bst, bsh, INTR_MASK_REG_B) & IM_ERCV_INT));
}

static inline void
smc91cxx_intr_ack_write(bus_space_tag_t bst, bus_space_handle_t bsh,
	uint8_t ack, uint8_t mask)
{
#ifdef SMC91CXX_NO_BYTE_WRITE
	bus_space_write_2(bst, bsh, INTR_ACK_REG_B, ack | (mask << 8));
#else
	bus_space_write_1(bst, bsh, INTR_ACK_REG_B, ack);
#endif
	KDASSERT(!(bus_space_read_1(bst, bsh, INTR_MASK_REG_B) & IM_ERCV_INT));
}

void
smc91cxx_attach(struct smc91cxx_softc *sc, u_int8_t *myea)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifmedia *ifm = &sc->sc_mii.mii_media;
	const char *idstr;
	u_int32_t miicapabilities;
	u_int16_t tmp;
	u_int8_t enaddr[ETHER_ADDR_LEN];
	int i, aui, mult, scale, memsize;
	char pbuf[9];

	tmp = bus_space_read_2(bst, bsh, BANK_SELECT_REG_W);
	/* check magic number */
	if ((tmp & BSR_DETECT_MASK) != BSR_DETECT_VALUE) {
		aprint_error_dev(sc->sc_dev,
		     "failed to detect chip, bsr=%04x\n", tmp);
		return;
	}

	/* Make sure the chip is stopped. */
	smc91cxx_stop(sc);

	SMC_SELECT_BANK(sc, 3);
	tmp = bus_space_read_2(bst, bsh, REVISION_REG_W);
	sc->sc_chipid = RR_ID(tmp);
	idstr = smc91cxx_idstrs[sc->sc_chipid];

	aprint_normal_dev(sc->sc_dev, "");
	if (idstr != NULL)
		aprint_normal("%s, ", idstr);
	else
		aprint_normal("unknown chip id %d, ", sc->sc_chipid);
	aprint_normal("revision %d, ", RR_REV(tmp));

	SMC_SELECT_BANK(sc, 0);
	switch (sc->sc_chipid) {
	default:
		mult = MCR_MEM_MULT(bus_space_read_2(bst, bsh, MEM_CFG_REG_W));
		scale = MIR_SCALE_91C9x;
		break;

	case CHIP_91C111:
		mult = MIR_MULT_91C111;
		scale = MIR_SCALE_91C111;
	}
	memsize = bus_space_read_2(bst, bsh, MEM_INFO_REG_W) & MIR_TOTAL_MASK;
	if (memsize == 255)
		memsize++;
	memsize *= scale * mult;

	format_bytes(pbuf, sizeof(pbuf), memsize);
	aprint_normal("buffer size: %s\n", pbuf);

	/* Read the station address from the chip. */
	SMC_SELECT_BANK(sc, 1);
	if (myea == NULL) {
		myea = enaddr;
		for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
			tmp = bus_space_read_2(bst, bsh, IAR_ADDR0_REG_W + i);
			myea[i + 1] = (tmp >> 8) & 0xff;
			myea[i] = tmp & 0xff;
		}
	}
	aprint_normal_dev(sc->sc_dev, "MAC address %s, ",
	    ether_sprintf(myea));

	/* Initialize the ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = smc91cxx_start;
	ifp->if_ioctl = smc91cxx_ioctl;
	ifp->if_watchdog = smc91cxx_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, myea);

	/*
	 * Initialize our media structures and MII info.  We will
	 * probe the MII if we are on the SMC91Cxx
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = smc91cxx_mii_readreg;
	sc->sc_mii.mii_writereg = smc91cxx_mii_writereg;
	sc->sc_mii.mii_statchg = smc91cxx_statchg;
	ifmedia_init(ifm, IFM_IMASK, smc91cxx_mediachange, smc91cxx_mediastatus);

	SMC_SELECT_BANK(sc, 1);
	tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);

	miicapabilities = BMSR_MEDIAMASK|BMSR_ANEG;
	switch (sc->sc_chipid) {
	case CHIP_91100:
		/*
		 * The 91100 does not have full-duplex capabilities,
		 * even if the PHY does.
		 */
		miicapabilities &= ~(BMSR_100TXFDX | BMSR_10TFDX);
		/*FALLTHROUGH*/
	case CHIP_91100FD:
	case CHIP_91C111:
		if (tmp & CR_MII_SELECT) {
			aprint_normal("default media MII");
			if (sc->sc_chipid == CHIP_91C111) {
				aprint_normal(" (%s PHY)\n",
				    (tmp & CR_AUI_SELECT) ?
				    "external" : "internal");
				sc->sc_internal_phy = !(tmp & CR_AUI_SELECT);
			} else
				aprint_normal("\n");
			mii_attach(sc->sc_dev, &sc->sc_mii, miicapabilities,
			    MII_PHY_ANY, MII_OFFSET_ANY, 0);
			if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
				ifmedia_add(&sc->sc_mii.mii_media,
				    IFM_ETHER|IFM_NONE, 0, NULL);
				ifmedia_set(&sc->sc_mii.mii_media,
				    IFM_ETHER|IFM_NONE);
			} else {
				ifmedia_set(&sc->sc_mii.mii_media,
				    IFM_ETHER|IFM_AUTO);
			}
			sc->sc_flags |= SMC_FLAGS_HAS_MII;
			break;
		} else
		if (sc->sc_chipid == CHIP_91C111) {
			/*
			 * XXX: Should bring it out of low-power mode
			 */
			aprint_normal("EPH interface in low power mode\n");
			sc->sc_internal_phy = 0;
			return;
		}
		/*FALLTHROUGH*/
	default:
		aprint_normal("default media %s\n",
		    (aui = (tmp & CR_AUI_SELECT)) ?
		    "AUI" : "UTP");
		for (i = 0; i < NSMC91CxxMEDIA; i++)
			ifmedia_add(ifm, smc91cxx_media[i], 0, NULL);
		ifmedia_set(ifm, IFM_ETHER | (aui ? IFM_10_5 : IFM_10_T));
		break;
	}

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	callout_init(&sc->sc_mii_callout, 0);

	/* The attach is successful. */
	sc->sc_flags |= SMC_FLAGS_ATTACHED;
}

/*
 * Change media according to request.
 */
int
smc91cxx_mediachange(struct ifnet *ifp)
{
	struct smc91cxx_softc *sc = ifp->if_softc;

	return (smc91cxx_set_media(sc, sc->sc_mii.mii_media.ifm_media));
}

int
smc91cxx_set_media(struct smc91cxx_softc *sc, int media)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;
	int rc;

	/*
	 * If the interface is not currently powered on, just return.
	 * When it is enabled later, smc91cxx_init() will properly set
	 * up the media for us.
	 */
	if ((sc->sc_flags & SMC_FLAGS_ENABLED) == 0)
		return (0);

	if (IFM_TYPE(media) != IFM_ETHER)
		return (EINVAL);

	if ((sc->sc_flags & SMC_FLAGS_HAS_MII) == 0 ||
	    (rc = mii_mediachg(&sc->sc_mii)) == ENXIO)
		rc = 0;

	switch (IFM_SUBTYPE(media)) {
	case IFM_10_T:
	case IFM_10_5:
		SMC_SELECT_BANK(sc, 1);
		tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);
		if (IFM_SUBTYPE(media) == IFM_10_5)
			tmp |= CR_AUI_SELECT;
		else
			tmp &= ~CR_AUI_SELECT;
		bus_space_write_2(bst, bsh, CONFIG_REG_W, tmp);
		delay(20000);	/* XXX is this needed? */
		break;

	default:
		return (EINVAL);
	}

	return rc;
}

/*
 * Notify the world which media we're using.
 */
void
smc91cxx_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;

	if ((sc->sc_flags & SMC_FLAGS_ENABLED) == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	/*
	 * If we have MII, go ask the PHY what's going on.
	 */
	if (sc->sc_flags & SMC_FLAGS_HAS_MII) {
		mii_pollstat(&sc->sc_mii);
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
		return;
	}

	SMC_SELECT_BANK(sc, 1);
	tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);
	ifmr->ifm_active =
	    IFM_ETHER | ((tmp & CR_AUI_SELECT) ? IFM_10_5 : IFM_10_T);
}

/*
 * Reset and initialize the chip.
 */
void
smc91cxx_init(struct smc91cxx_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;
	const u_int8_t *enaddr;
	int s, i;

	s = splnet();

	/*
	 * This resets the registers mostly to defaults, but doesn't
	 * affect the EEPROM.  The longest reset recovery time of those devices
	 * supported is the 91C111. Section 7.8 of its datasheet asks for 50ms.
	 */
	SMC_SELECT_BANK(sc, 0);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, RCR_SOFTRESET);
	delay(5);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, 0);
	delay(50000);

	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, 0);

	/* Set the Ethernet address. */
	SMC_SELECT_BANK(sc, 1);
	enaddr = (const u_int8_t *)CLLADDR(ifp->if_sadl);
	for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
		tmp = enaddr[i + 1] << 8 | enaddr[i];
		bus_space_write_2(bst, bsh, IAR_ADDR0_REG_W + i, tmp);
	}

	/*
	 * Set the control register to automatically release successfully
	 * transmitted packets (making the best use of our limited memory)
	 * and enable the EPH interrupt on certain TX errors.
	 */
	bus_space_write_2(bst, bsh, CONTROL_REG_W, (CTR_AUTO_RELEASE |
	    CTR_TE_ENABLE | CTR_CR_ENABLE | CTR_LE_ENABLE));

	/*
	 * Reset the MMU and wait for it to be un-busy.
	 */
	SMC_SELECT_BANK(sc, 2);
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_RESET);
	sc->sc_txpacketno = ARR_FAILED;
	for (;;) {
		tmp = bus_space_read_2(bst, bsh, MMU_CMD_REG_W);
		if (tmp == 0xffff) {
			/* card went away! */
			splx(s);
			return;
		}
		if ((tmp & MMUCR_BUSY) == 0)
			break;
	}

	/*
	 * Disable all interrupts.
	 */
	smc91cxx_intr_mask_write(bst, bsh, 0);

	/*
	 * On the 91c111, enable auto-negotiation, and set the LED
	 * status pins to something sane.
	 * XXX: Should be some way for MD code to decide the latter.
	 */
	SMC_SELECT_BANK(sc, 0);
	if (sc->sc_chipid == CHIP_91C111) {
		bus_space_write_2(bst, bsh, RX_PHY_CONTROL_REG_W,
		    RPC_ANEG |
		    (RPC_LS_LINK_DETECT << RPC_LSA_SHIFT) |
		    (RPC_LS_TXRX << RPC_LSB_SHIFT));
	}

	/*
	 * Set current media.
	 */
	smc91cxx_set_media(sc, sc->sc_mii.mii_media.ifm_cur->ifm_media);

	/*
	 * Set the receive filter.  We want receive enable and auto
	 * strip of CRC from received packet.  If we are in promisc. mode,
	 * then set that bit as well.
	 *
	 * XXX Initialize multicast filter.  For now, we just accept
	 * XXX all multicast.
	 */
	SMC_SELECT_BANK(sc, 0);

	tmp = RCR_ENABLE | RCR_STRIP_CRC | RCR_ALMUL;
	if (ifp->if_flags & IFF_PROMISC)
		tmp |= RCR_PROMISC;

	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, tmp);

	/*
	 * Set transmitter control to "enabled".
	 */
	tmp = TCR_ENABLE;

#ifndef SMC91CXX_SW_PAD
	/*
	 * Enable hardware padding of transmitted packets.
	 * XXX doesn't work?
	 */
	tmp |= TCR_PAD_ENABLE;
#endif

	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, tmp);

	/*
	 * Now, enable interrupts.
	 */
	SMC_SELECT_BANK(sc, 2);

	sc->sc_intmask = IM_EPH_INT | IM_RX_OVRN_INT | IM_RCV_INT;
	if (sc->sc_chipid == CHIP_91C111 && sc->sc_internal_phy) {
		sc->sc_intmask |= IM_MD_INT;
	}
	smc91cxx_intr_mask_write(bst, bsh, sc->sc_intmask);

	/* Interface is now running, with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->sc_flags & SMC_FLAGS_HAS_MII) {
		/* Start the one second clock. */
		callout_reset(&sc->sc_mii_callout, hz, smc91cxx_tick, sc);
	}

	/*
	 * Attempt to start any pending transmission.
	 */
	smc91cxx_start(ifp);

	splx(s);
}

/*
 * Start output on an interface.
 * Must be called at splnet or interrupt level.
 */
void
smc91cxx_start(struct ifnet *ifp)
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int len;
	struct mbuf *m;
	u_int16_t length, npages;
	u_int16_t oddbyte;
	u_int8_t packetno;
	int timo, pad;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

 again:
	/*
	 * Peek at the next packet.
	 */
	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL)
		return;

	/*
	 * Compute the frame length and set pad to give an overall even
	 * number of bytes.  Below, we assume that the packet length
	 * is even.
	 */
	for (len = 0; m != NULL; m = m->m_next)
		len += m->m_len;

	/*
	 * We drop packets that are too large.  Perhaps we should
	 * truncate them instead?
	 */
	if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN)) {
		printf("%s: large packet discarded\n",
		    device_xname(sc->sc_dev));
		ifp->if_oerrors++;
		IFQ_DEQUEUE(&ifp->if_snd, m);
		m_freem(m);
		goto readcheck;
	}

	pad = 0;
#ifdef SMC91CXX_SW_PAD
	/*
	 * Not using hardware padding; pad to ETHER_MIN_LEN.
	 */
	if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN))
		pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;
#endif

	length = pad + len;

	/*
	 * The MMU has a 256 byte page size.  The MMU expects us to
	 * ask for "npages - 1".  We include space for the status word,
	 * byte count, and control bytes in the allocation request.
	 */
	npages = ((length & ~1) + 6) >> 8;

	/*
	 * Now allocate the memory.
	 */
	SMC_SELECT_BANK(sc, 2);
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_ALLOC | npages);

	timo = MEMORY_WAIT_TIME;
	if (__predict_false((sc->sc_txpacketno & ARR_FAILED) == 0)) {
		packetno = sc->sc_txpacketno;
		sc->sc_txpacketno = ARR_FAILED;
	} else {
		do {
			if (bus_space_read_1(bst, bsh,
			    		     INTR_STAT_REG_B) & IM_ALLOC_INT)
				break;
			delay(1);
		} while (--timo);
	}

	packetno = bus_space_read_1(bst, bsh, ALLOC_RESULT_REG_B);

	if (packetno & ARR_FAILED || timo == 0) {
		/*
		 * No transmit memory is available.  Record the number
		 * of requested pages and enable the allocation completion
		 * interrupt.  Set up the watchdog timer in case we miss
		 * the interrupt.  Mark the interface as active so that
		 * no one else attempts to transmit while we're allocating
		 * memory.
		 */
		sc->sc_intmask |= IM_ALLOC_INT;
		smc91cxx_intr_mask_write(bst, bsh, sc->sc_intmask);
		ifp->if_timer = 5;
		ifp->if_flags |= IFF_OACTIVE;

		return;
	}

	/*
	 * We have a packet number - set the data window.
	 */
	bus_space_write_2(bst, bsh, PACKET_NUM_REG_B, packetno);

	/*
	 * Point to the beginning of the packet.
	 */
	bus_space_write_2(bst, bsh, POINTER_REG_W, PTR_AUTOINC /* | 0x0000 */);

	/*
	 * Send the packet length (+6 for stats, length, and control bytes)
	 * and the status word (set to zeros).
	 */
	bus_space_write_2(bst, bsh, DATA_REG_W, 0);
	bus_space_write_2(bst, bsh, DATA_REG_W, (length + 6) & 0x7ff);

	/*
	 * Get the packet from the kernel.  This will include the Ethernet
	 * frame header, MAC address, etc.
	 */
	IFQ_DEQUEUE(&ifp->if_snd, m);

	/*
	 * Push the packet out to the card.  The copying function only does
	 * whole words and returns the straggling byte (if any).
	 */
	oddbyte = smc91cxx_copy_tx_frame(sc, m);

#ifdef SMC91CXX_SW_PAD
	if (pad > 1 && (pad & 1)) {
		bus_space_write_2(bst, bsh, DATA_REG_W, oddbyte);
		oddbyte = 0;
		pad -= 1;
	}

	/*
	 * Push out padding.
	 */
	while (pad > 1) {
		bus_space_write_2(bst, bsh, DATA_REG_W, 0);
		pad -= 2;
	}
#endif

	/*
	 * Push out control byte and unused packet byte.  The control byte
	 * denotes whether this is an odd or even length packet, and that
	 * no special CRC handling is necessary.
	 */
	bus_space_write_2(bst, bsh, DATA_REG_W,
	    oddbyte | ((length & 1) ? (CTLB_ODD << 8) : 0));

	/*
	 * Enable transmit interrupts and let the chip go.  Set a watchdog
	 * in case we miss the interrupt.
	 */
	sc->sc_intmask |= IM_TX_INT | IM_TX_EMPTY_INT;
	smc91cxx_intr_mask_write(bst, bsh, sc->sc_intmask);

	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_ENQUEUE);

	ifp->if_timer = 5;

	/* Hand off a copy to the bpf. */
	bpf_mtap(ifp, m);

	ifp->if_opackets++;
	m_freem(m);

 readcheck:
	/*
	 * Check for incoming packets.  We don't want to overflow the small
	 * RX FIFO.  If nothing has arrived, attempt to queue another
	 * transmit packet.
	 */
	if (bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W) & FIFO_REMPTY)
		goto again;
}

/*
 * Squirt a (possibly misaligned) mbuf to the device
 */
uint8_t
smc91cxx_copy_tx_frame(struct smc91cxx_softc *sc, struct mbuf *m0)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct mbuf *m;
	int len, leftover;
	u_int16_t dbuf;
	u_int8_t *p;
#ifdef DIAGNOSTIC
	u_int8_t *lim;
#endif

	/* start out with no leftover data */
	leftover = 0;
	dbuf = 0;

	/* Process the chain of mbufs */
	for (m = m0; m != NULL; m = m->m_next) {
		/*
		 * Process all of the data in a single mbuf.
		 */
		p = mtod(m, u_int8_t *);
		len = m->m_len;
#ifdef DIAGNOSTIC
		lim = p + len;
#endif

		while (len > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or realignment).
				 * Buffer the next byte, and write it and
				 * the leftover data out.
				 */
				dbuf |= *p++ << 8;
				len--;
				bus_space_write_2(bst, bsh, DATA_REG_W, dbuf);
				leftover = 0;
			} else if ((long) p & 1) {
				/*
				 * Misaligned data.  Buffer the next byte.
				 */
				dbuf = *p++;
				len--;
				leftover = 1;
			} else {
				/*
				 * Aligned data.  This is the case we like.
				 *
				 * Write-region out as much as we can, then
				 * buffer the remaining byte (if any).
				 */
				leftover = len & 1;
				len &= ~1;
				bus_space_write_multi_stream_2(bst, bsh,
				    DATA_REG_W, (u_int16_t *)p, len >> 1);
				p += len;

				if (leftover)
					dbuf = *p++;
				len = 0;
			}
		}
		if (len < 0)
			panic("smc91cxx_copy_tx_frame: negative len");
#ifdef DIAGNOSTIC
		if (p != lim)
			panic("smc91cxx_copy_tx_frame: p != lim");
#endif
	}

	return dbuf;
}

/*
 * Interrupt service routine.
 */
int
smc91cxx_intr(void *arg)
{
	struct smc91cxx_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t mask, interrupts, status;
	u_int16_t packetno, tx_status, card_stats;
	u_int16_t v;

	if ((sc->sc_flags & SMC_FLAGS_ENABLED) == 0 ||
	    !device_is_active(sc->sc_dev))
		return (0);

	SMC_SELECT_BANK(sc, 2);

	/*
	 * Obtain the current interrupt status and mask.
	 */
	v = bus_space_read_2(bst, bsh, INTR_STAT_REG_B);

	/*
	 * Get the set of interrupt which occurred and eliminate any
	 * which are not enabled.
	 */
	mask = v >> 8;
	interrupts = v & 0xff;
	KDASSERT(mask == sc->sc_intmask);
	status = interrupts & mask;

	/* Ours? */
	if (status == 0)
		return (0);

	/*
	 * It's ours; disable all interrupts while we process them.
	 */
	smc91cxx_intr_mask_write(bst, bsh, 0);

	/*
	 * Receive overrun interrupts.
	 */
	if (status & IM_RX_OVRN_INT) {
		smc91cxx_intr_ack_write(bst, bsh, IM_RX_OVRN_INT, 0);
		ifp->if_ierrors++;
	}

	/*
	 * Receive interrupts.
	 */
	if (status & IM_RCV_INT) {
		smc91cxx_read(sc);
	}

	/*
	 * Memory allocation interrupts.
	 */
	if (status & IM_ALLOC_INT) {
		/* Disable this interrupt. */
		mask &= ~IM_ALLOC_INT;
		sc->sc_intmask &= ~IM_ALLOC_INT;

		/*
		 * Save allocated packet number for use in start
		 */
		packetno = bus_space_read_1(bst, bsh, ALLOC_RESULT_REG_B);
		KASSERT(sc->sc_txpacketno & ARR_FAILED);
		sc->sc_txpacketno = packetno;

		/*
		 * We can transmit again!
		 */
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * Transmit complete interrupt.  Handle transmission error messages.
	 * This will only be called on error condition because of AUTO RELEASE
	 * mode.
	 */
	if (status & IM_TX_INT) {
		smc91cxx_intr_ack_write(bst, bsh, IM_TX_INT, 0);

		packetno = bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W) &
		    FIFO_TX_MASK;

		/*
		 * Select this as the packet to read from.
		 */
		bus_space_write_2(bst, bsh, PACKET_NUM_REG_B, packetno);

		/*
		 * Position the pointer to the beginning of the packet, wait
		 * for preload.
		 */
		bus_space_write_2(bst, bsh, POINTER_REG_W,
		    PTR_AUTOINC | PTR_READ /* | 0x0000 */);
		delay(1);

		/*
		 * Fetch the TX status word.  This will be a copy of
		 * the EPH_STATUS_REG_W at the time of the transmission
		 * failure.
		 */
		tx_status = bus_space_read_2(bst, bsh, DATA_REG_W);

		if (tx_status & EPHSR_TX_SUC) {
			static struct timeval txsuc_last;
			static int txsuc_count;
			if (ppsratecheck(&txsuc_last, &txsuc_count, 1))
				printf("%s: successful packet caused TX"
				    " interrupt?!\n", device_xname(sc->sc_dev));
		} else
			ifp->if_oerrors++;

		if (tx_status & EPHSR_LATCOL)
			ifp->if_collisions++;

		/* Disable this interrupt (start will reenable if needed). */
		mask &= ~IM_TX_INT;
		sc->sc_intmask &= ~IM_TX_INT;

		/*
		 * Some of these errors disable the transmitter; reenable it.
		 */
		SMC_SELECT_BANK(sc, 0);
#ifdef SMC91CXX_SW_PAD
		bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, TCR_ENABLE);
#else
		bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W,
		    TCR_ENABLE | TCR_PAD_ENABLE);
#endif

		/*
		 * Kill the failed packet and wait for the MMU to unbusy.
		 */
		SMC_SELECT_BANK(sc, 2);
		while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
			/* XXX bound this loop! */ ;
		bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_FREEPKT);

		ifp->if_timer = 0;
	}

	/*
	 * Transmit underrun interrupts.  We use this opportunity to
	 * update transmit statistics from the card.
	 */
	if (status & IM_TX_EMPTY_INT) {
		smc91cxx_intr_ack_write(bst, bsh, IM_TX_EMPTY_INT, 0);

		/* Disable this interrupt. */
		mask &= ~IM_TX_EMPTY_INT;
		sc->sc_intmask &= ~IM_TX_EMPTY_INT;

		SMC_SELECT_BANK(sc, 0);
		card_stats = bus_space_read_2(bst, bsh, COUNTER_REG_W);

		/* Single collisions. */
		ifp->if_collisions += card_stats & ECR_COLN_MASK;

		/* Multiple collisions. */
		ifp->if_collisions += (card_stats & ECR_MCOLN_MASK) >> 4;

		SMC_SELECT_BANK(sc, 2);

		ifp->if_timer = 0;
	}

	/*
	 * Internal PHY status change
	 */
	if (sc->sc_chipid == CHIP_91C111 && sc->sc_internal_phy &&
	    (status & IM_MD_INT)) {

		/*
		 * Internal PHY status change
		 */
		smc91cxx_intr_ack_write(bst, bsh, IM_MD_INT, 0);
		mii_pollstat(&sc->sc_mii);
	}

	/*
	 * Other errors.  Reset the interface.
	 */
	if (status & IM_EPH_INT) {
		smc91cxx_stop(sc);
		smc91cxx_init(sc);
	}

	/*
	 * Attempt to queue more packets for transmission.
	 */
	smc91cxx_start(ifp);

	/*
	 * Reenable the interrupts we wish to receive now that processing
	 * is complete.
	 */
	mask |= sc->sc_intmask;
	smc91cxx_intr_mask_write(bst, bsh, mask);

	if (status)
		rnd_add_uint32(&sc->rnd_source, status);

	return (1);
}

/*
 * Read a packet from the card and pass it up to the kernel.
 * NOTE!  WE EXPECT TO BE IN REGISTER WINDOW 2!
 */
void
smc91cxx_read(struct smc91cxx_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ether_header *eh;
	struct mbuf *m;
	u_int16_t status, packetno, packetlen;
	u_int8_t *data;
	u_int32_t dr;
	bool first = true;

 again:
	/*
	 * Set data pointer to the beginning of the packet.  Since
	 * PTR_RCV is set, the packet number will be found automatically
	 * in FIFO_PORTS_REG_W, FIFO_RX_MASK.
	 */
	packetno = bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W);
	if (packetno & FIFO_REMPTY) {
		if (first) {
			aprint_error_dev(sc->sc_dev,
			    "receive interrupt on empty fifo\n");
		}
		return;
	}
	first = false;

	bus_space_write_2(bst, bsh, POINTER_REG_W,
	    PTR_READ | PTR_RCV | PTR_AUTOINC /* | 0x0000 */);
	delay(1);

	/*
	 * First two words are status and packet length.
	 */
	dr = bus_space_read_4(bst, bsh, DATA_REG_W);
	status = (u_int16_t)dr;
	packetlen = (u_int16_t)(dr >> 16);

	packetlen &= RLEN_MASK;
	if (packetlen < ETHER_MIN_LEN - ETHER_CRC_LEN + 6 || packetlen > 1534) {
		ifp->if_ierrors++;
		goto out;
	}

	/*
	 * The packet length includes 3 extra words: status, length,
	 * and an extra word that includes the control byte.
	 */
	packetlen -= 6;

	/*
	 * Account for receive errors and discard.
	 */
	if (status & RS_ERRORS) {
		ifp->if_ierrors++;
		goto out;
	}

	/*
	 * Adjust for odd-length packet.
	 */
	if (status & RS_ODDFRAME)
		packetlen++;

	/*
	 * Allocate a header mbuf.
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto out;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = packetlen;

	/*
	 * Always put the packet in a cluster.
	 * XXX should chain small mbufs if less than threshold.
	 */
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		ifp->if_ierrors++;
		aprint_error_dev(sc->sc_dev,
		     "can't allocate cluster for incoming packet\n");
		goto out;
	}

	/*
	 * Pull the packet off the interface.  Make sure the payload
	 * is aligned.
	 */
	if ((sc->sc_flags & SMC_FLAGS_32BIT_READ) == 0) {
		m->m_data = (char *) ALIGN(mtod(m, char *) +
		    sizeof(struct ether_header)) - sizeof(struct ether_header);

		eh = mtod(m, struct ether_header *);
		data = mtod(m, u_int8_t *);
		KASSERT(trunc_page((uintptr_t)data) ==
			trunc_page((uintptr_t)data + packetlen - 1));
		if (packetlen > 1)
			bus_space_read_multi_stream_2(bst, bsh, DATA_REG_W,
			    (u_int16_t *)data, packetlen >> 1);
		if (packetlen & 1) {
			data += packetlen & ~1;
			*data = bus_space_read_1(bst, bsh, DATA_REG_B);
		}
	} else {
		m->m_data = (void *) ALIGN(mtod(m, void *));
		eh = mtod(m, struct ether_header *);
		data = mtod(m, u_int8_t *);
		KASSERT(trunc_page((uintptr_t)data) ==
			trunc_page((uintptr_t)data + packetlen - 1));
		if (packetlen > 3)
			bus_space_read_multi_stream_4(bst, bsh, DATA_REG_W,
			    (u_int32_t *)data, packetlen >> 2);
		if (packetlen & 3) {
			data += packetlen & ~3;
			*((u_int32_t *)data) =
			    bus_space_read_stream_4(bst, bsh, DATA_REG_W);
		}
	}

	ifp->if_ipackets++;

	/*
	 * Make sure to behave as IFF_SIMPLEX in all cases.
	 * This is to cope with SMC91C92 (Megahertz XJ10BT), which
	 * loops back packets to itself on promiscuous mode.
	 * (should be ensured by chipset configuration)
	 */
	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		/*
		 * Drop packet looped back from myself.
		 */
		if (ether_cmp(eh->ether_shost, CLLADDR(ifp->if_sadl)) == 0) {
			m_freem(m);
			goto out;
		}
	}

	m->m_pkthdr.len = m->m_len = packetlen;

	/*
	 * Hand the packet off to bpf listeners.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);

 out:
	/*
	 * Tell the card to free the memory occupied by this packet.
	 */
	while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
		/* XXX bound this loop! */ ;
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_RELEASE);

	/*
	 * Check for another packet.
	 */
	goto again;
}

/*
 * Process an ioctl request.
 */
int
smc91cxx_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		if ((error = smc91cxx_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		smc91cxx_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif
		default:
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
			 * If interface is marked down and it is running,
			 * stop it.
			 */
			smc91cxx_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			smc91cxx_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped,
			 * start it.
			 */
			if ((error = smc91cxx_enable(sc)) != 0)
				break;
			smc91cxx_init(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Reset the interface to pick up changes in any
			 * other flags that affect hardware registers.
			 */
			smc91cxx_reset(sc);
			break;
		case 0:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((sc->sc_flags & SMC_FLAGS_ENABLED) == 0) {
			error = EIO;
			break;
		}

		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				smc91cxx_reset(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return (error);
}

/*
 * Reset the interface.
 */
void
smc91cxx_reset(struct smc91cxx_softc *sc)
{
	int s;

	s = splnet();
	smc91cxx_stop(sc);
	smc91cxx_init(sc);
	splx(s);
}

/*
 * Watchdog timer.
 */
void
smc91cxx_watchdog(struct ifnet *ifp)
{
	struct smc91cxx_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;
	smc91cxx_reset(sc);
}

/*
 * Stop output on the interface.
 */
void
smc91cxx_stop(struct smc91cxx_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/*
	 * Clear interrupt mask; disable all interrupts.
	 */
	SMC_SELECT_BANK(sc, 2);
	smc91cxx_intr_mask_write(bst, bsh, 0);

	/*
	 * Disable transmitter and receiver.
	 */
	SMC_SELECT_BANK(sc, 0);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, 0);
	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, 0);

	/*
	 * Cancel watchdog timer.
	 */
	sc->sc_ec.ec_if.if_timer = 0;
}

/*
 * Enable power on the interface.
 */
int
smc91cxx_enable(struct smc91cxx_softc *sc)
{

	if ((sc->sc_flags & SMC_FLAGS_ENABLED) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
	}

	sc->sc_flags |= SMC_FLAGS_ENABLED;
	return (0);
}

/*
 * Disable power on the interface.
 */
void
smc91cxx_disable(struct smc91cxx_softc *sc)
{

	if ((sc->sc_flags & SMC_FLAGS_ENABLED) != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_flags &= ~SMC_FLAGS_ENABLED;
	}
}

int
smc91cxx_activate(device_t self, enum devact act)
{
	struct smc91cxx_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
smc91cxx_detach(device_t self, int flags)
{
	struct smc91cxx_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	/* Succeed now if there's no work to do. */
	if ((sc->sc_flags & SMC_FLAGS_ATTACHED) == 0)
		return (0);

	/* smc91cxx_disable() checks SMC_FLAGS_ENABLED */
	smc91cxx_disable(sc);

	/* smc91cxx_attach() never fails */

	/* Delete all media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	rnd_detach_source(&sc->rnd_source);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (0);
}

u_int32_t
smc91cxx_mii_bitbang_read(device_t self)
{
	struct smc91cxx_softc *sc = device_private(self);

	/* We're already in bank 3. */
	return (bus_space_read_2(sc->sc_bst, sc->sc_bsh, MGMT_REG_W));
}

void
smc91cxx_mii_bitbang_write(device_t self, u_int32_t val)
{
	struct smc91cxx_softc *sc = device_private(self);

	/* We're already in bank 3. */
	bus_space_write_2(sc->sc_bst, sc->sc_bsh, MGMT_REG_W, val);
}

int
smc91cxx_mii_readreg(device_t self, int phy, int reg)
{
	struct smc91cxx_softc *sc = device_private(self);
	int val;

	SMC_SELECT_BANK(sc, 3);

	val = mii_bitbang_readreg(self, &smc91cxx_mii_bitbang_ops, phy, reg);

	SMC_SELECT_BANK(sc, 2);

	return (val);
}

void
smc91cxx_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct smc91cxx_softc *sc = device_private(self);

	SMC_SELECT_BANK(sc, 3);

	mii_bitbang_writereg(self, &smc91cxx_mii_bitbang_ops, phy, reg, val);

	SMC_SELECT_BANK(sc, 2);
}

void
smc91cxx_statchg(struct ifnet *ifp)
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int mctl;

	SMC_SELECT_BANK(sc, 0);
	mctl = bus_space_read_2(bst, bsh, TXMIT_CONTROL_REG_W);
	if (sc->sc_mii.mii_media_active & IFM_FDX)
		mctl |= TCR_SWFDUP;
	else
		mctl &= ~TCR_SWFDUP;
	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, mctl);
	SMC_SELECT_BANK(sc, 2);	/* back to operating window */
}

/*
 * One second timer, used to tick the MII.
 */
void
smc91cxx_tick(void *arg)
{
	struct smc91cxx_softc *sc = arg;
	int s;

#ifdef DIAGNOSTIC
	if ((sc->sc_flags & SMC_FLAGS_HAS_MII) == 0)
		panic("smc91cxx_tick");
#endif

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_mii_callout, hz, smc91cxx_tick, sc);
}
