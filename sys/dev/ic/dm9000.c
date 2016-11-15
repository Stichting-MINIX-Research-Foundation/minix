/*	$NetBSD: dm9000.c,v 1.8 2015/06/12 17:24:02 macallan Exp $	*/

/*
 * Copyright (c) 2009 Paul Fleischer
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* based on sys/dev/ic/cs89x0.c */
/*
 * Copyright (c) 2004 Christopher Gilbert
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/dm9000var.h>
#include <dev/ic/dm9000reg.h>

#if 1
#undef DM9000_DEBUG
#undef DM9000_TX_DEBUG
#undef DM9000_TX_DATA_DEBUG
#undef DM9000_RX_DEBUG
#undef  DM9000_RX_DATA_DEBUG
#else
#define DM9000_DEBUG
#define  DM9000_TX_DEBUG
#define DM9000_TX_DATA_DEBUG
#define DM9000_RX_DEBUG
#define  DM9000_RX_DATA_DEBUG
#endif

#ifdef DM9000_DEBUG
#define DPRINTF(s) do {printf s; } while (/*CONSTCOND*/0)
#else
#define DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

#ifdef DM9000_TX_DEBUG
#define TX_DPRINTF(s) do {printf s; } while (/*CONSTCOND*/0)
#else
#define TX_DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

#ifdef DM9000_RX_DEBUG
#define RX_DPRINTF(s) do {printf s; } while (/*CONSTCOND*/0)
#else
#define RX_DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

#ifdef DM9000_RX_DATA_DEBUG
#define RX_DATA_DPRINTF(s) do {printf s; } while (/*CONSTCOND*/0)
#else
#define RX_DATA_DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

#ifdef DM9000_TX_DATA_DEBUG
#define TX_DATA_DPRINTF(s) do {printf s; } while (/*CONSTCOND*/0)
#else
#define TX_DATA_DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

/*** Internal PHY functions ***/
uint16_t dme_phy_read(struct dme_softc *, int );
void	dme_phy_write(struct dme_softc *, int, uint16_t);
void	dme_phy_init(struct dme_softc *);
void	dme_phy_reset(struct dme_softc *);
void	dme_phy_update_media(struct dme_softc *);
void	dme_phy_check_link(void *);

/*** Methods registered in struct ifnet ***/
void	dme_start_output(struct ifnet *);
int	dme_init(struct ifnet *);
int	dme_ioctl(struct ifnet *, u_long, void *);
void	dme_stop(struct ifnet *, int);

int	dme_mediachange(struct ifnet *);
void	dme_mediastatus(struct ifnet *, struct ifmediareq *);

/*** Internal methods ***/

/* Prepare data to be transmitted (i.e. dequeue and load it into the DM9000) */
void    dme_prepare(struct dme_softc *, struct ifnet *);

/* Transmit prepared data */
void    dme_transmit(struct dme_softc *);

/* Receive data */
void    dme_receive(struct dme_softc *, struct ifnet *);

/* Software Initialize/Reset of the DM9000 */
void    dme_reset(struct dme_softc *);

/* Configure multicast filter */
void	dme_set_addr_filter(struct dme_softc *);

/* Set media */
int	dme_set_media(struct dme_softc *, int );

/* Read/write packet data from/to DM9000 IC in various transfer sizes */
int	dme_pkt_read_2(struct dme_softc *, struct ifnet *, struct mbuf **);
int	dme_pkt_write_2(struct dme_softc *, struct mbuf *);
int	dme_pkt_read_1(struct dme_softc *, struct ifnet *, struct mbuf **);
int	dme_pkt_write_1(struct dme_softc *, struct mbuf *);
/* TODO: Implement 32 bit read/write functions */

uint16_t
dme_phy_read(struct dme_softc *sc, int reg)
{
	uint16_t val;
	/* Select Register to read*/
	dme_write(sc, DM9000_EPAR, DM9000_EPAR_INT_PHY +
	    (reg & DM9000_EPAR_EROA_MASK));
	/* Select read operation (DM9000_EPCR_ERPRR) from the PHY */
	dme_write(sc, DM9000_EPCR, DM9000_EPCR_ERPRR + DM9000_EPCR_EPOS_PHY);

	/* Wait until access to PHY has completed */
	while (dme_read(sc, DM9000_EPCR) & DM9000_EPCR_ERRE);

	/* Reset ERPRR-bit */
	dme_write(sc, DM9000_EPCR, DM9000_EPCR_EPOS_PHY);

	val = dme_read(sc, DM9000_EPDRL);
	val += dme_read(sc, DM9000_EPDRH) << 8;

	return val;
}

void
dme_phy_write(struct dme_softc *sc, int reg, uint16_t value)
{
	/* Select Register to write*/
	dme_write(sc, DM9000_EPAR, DM9000_EPAR_INT_PHY +
	    (reg & DM9000_EPAR_EROA_MASK));

	/* Write data to the two data registers */
	dme_write(sc, DM9000_EPDRL, value & 0xFF);
	dme_write(sc, DM9000_EPDRH, (value >> 8) & 0xFF);

	/* Select write operation (DM9000_EPCR_ERPRW) from the PHY */
	dme_write(sc, DM9000_EPCR, DM9000_EPCR_ERPRW + DM9000_EPCR_EPOS_PHY);

	/* Wait until access to PHY has completed */
	while(dme_read(sc, DM9000_EPCR) & DM9000_EPCR_ERRE);

	/* Reset ERPRR-bit */
	dme_write(sc, DM9000_EPCR, DM9000_EPCR_EPOS_PHY);
}

void
dme_phy_init(struct dme_softc *sc)
{
	u_int ifm_media = sc->sc_media.ifm_media;
	uint32_t bmcr, anar;

	bmcr = dme_phy_read(sc, DM9000_PHY_BMCR);
	anar = dme_phy_read(sc, DM9000_PHY_ANAR);

	anar = anar & ~DM9000_PHY_ANAR_10_HDX
		& ~DM9000_PHY_ANAR_10_FDX
		& ~DM9000_PHY_ANAR_TX_HDX
		& ~DM9000_PHY_ANAR_TX_FDX;

	switch (IFM_SUBTYPE(ifm_media)) {
	case IFM_AUTO:
		bmcr |= DM9000_PHY_BMCR_AUTO_NEG_EN;
		anar |= DM9000_PHY_ANAR_10_HDX |
			DM9000_PHY_ANAR_10_FDX |
			DM9000_PHY_ANAR_TX_HDX |
			DM9000_PHY_ANAR_TX_FDX;
		break;
	case IFM_10_T:
		//bmcr &= ~DM9000_PHY_BMCR_AUTO_NEG_EN;
		bmcr &= ~DM9000_PHY_BMCR_SPEED_SELECT;
		if (ifm_media & IFM_FDX)
			anar |= DM9000_PHY_ANAR_10_FDX;
		else
			anar |= DM9000_PHY_ANAR_10_HDX;
		break;
	case IFM_100_TX:
		//bmcr &= ~DM9000_PHY_BMCR_AUTO_NEG_EN;
		bmcr |= DM9000_PHY_BMCR_SPEED_SELECT;
		if (ifm_media & IFM_FDX)
			anar |= DM9000_PHY_ANAR_TX_FDX;
		else
			anar |= DM9000_PHY_ANAR_TX_HDX;

		break;
	}

	if(ifm_media & IFM_FDX) {
		bmcr |= DM9000_PHY_BMCR_DUPLEX_MODE;
	} else {
		bmcr &= ~DM9000_PHY_BMCR_DUPLEX_MODE;
	}

	dme_phy_write(sc, DM9000_PHY_BMCR, bmcr);
	dme_phy_write(sc, DM9000_PHY_ANAR, anar);
}

void
dme_phy_reset(struct dme_softc *sc)
{
	uint32_t reg;

	/* PHY Reset */
	dme_phy_write(sc, DM9000_PHY_BMCR, DM9000_PHY_BMCR_RESET);

	reg = dme_read(sc, DM9000_GPCR);
	dme_write(sc, DM9000_GPCR, reg & ~DM9000_GPCR_GPIO0_OUT);
	reg = dme_read(sc, DM9000_GPR);
	dme_write(sc, DM9000_GPR, reg | DM9000_GPR_PHY_PWROFF);

	dme_phy_init(sc);

	reg = dme_read(sc, DM9000_GPR);
	dme_write(sc, DM9000_GPR, reg & ~DM9000_GPR_PHY_PWROFF);
	reg = dme_read(sc, DM9000_GPCR);
	dme_write(sc, DM9000_GPCR, reg | DM9000_GPCR_GPIO0_OUT);

	dme_phy_update_media(sc);
}

void
dme_phy_update_media(struct dme_softc *sc)
{
	u_int ifm_media = sc->sc_media.ifm_media;
	uint32_t reg;

	if (IFM_SUBTYPE(ifm_media) == IFM_AUTO) {
		/* If auto-negotiation is used, ensures that it is completed
		 before trying to extract any media information. */
		reg = dme_phy_read(sc, DM9000_PHY_BMSR);
		if ((reg & DM9000_PHY_BMSR_AUTO_NEG_AB) == 0) {
			/* Auto-negotation not possible, therefore there is no
			   reason to try obtain any media information. */
			return;
		}

		/* Then loop until the negotiation is completed. */
		while ((reg & DM9000_PHY_BMSR_AUTO_NEG_COM) == 0) {
			/* TODO: Bail out after a finite number of attempts
			 in case something goes wrong. */
			preempt();
			reg = dme_phy_read(sc, DM9000_PHY_BMSR);
		}
	}


	sc->sc_media_active = IFM_ETHER;
	reg = dme_phy_read(sc, DM9000_PHY_BMCR);

	if (reg & DM9000_PHY_BMCR_SPEED_SELECT) {
		sc->sc_media_active |= IFM_100_TX;
	} else {
		sc->sc_media_active |= IFM_10_T;
	}

	if (reg & DM9000_PHY_BMCR_DUPLEX_MODE) {
		sc->sc_media_active |= IFM_FDX;
	}
}

void
dme_phy_check_link(void *arg)
{
	struct dme_softc *sc = arg;
	uint32_t reg;
	int s;

	s = splnet();

	reg = dme_read(sc, DM9000_NSR) & DM9000_NSR_LINKST;

	if( reg )
		reg = IFM_ETHER | IFM_AVALID | IFM_ACTIVE;
	else {
		reg = IFM_ETHER | IFM_AVALID;
		sc->sc_media_active = IFM_NONE;
	}

	if ( (sc->sc_media_status != reg) && (reg & IFM_ACTIVE)) {
		dme_phy_reset(sc);
	}

	sc->sc_media_status = reg;

	callout_schedule(&sc->sc_link_callout, mstohz(2000));
	splx(s);
}

int
dme_set_media(struct dme_softc *sc, int media)
{
	int s;

	s = splnet();
	sc->sc_media.ifm_media = media;
	dme_phy_reset(sc);

	splx(s);

	return 0;
}

int
dme_attach(struct dme_softc *sc, const uint8_t *enaddr)
{
	struct ifnet	*ifp = &sc->sc_ethercom.ec_if;
	uint8_t		b[2];
	uint16_t	io_mode;

	dme_read_c(sc, DM9000_VID0, b, 2);
#if BYTE_ORDER == BIG_ENDIAN
	sc->sc_vendor_id = (b[0] << 8) | b[1];
#else
	sc->sc_vendor_id = b[0] | (b[1] << 8);
#endif
	dme_read_c(sc, DM9000_PID0, b, 2);
#if BYTE_ORDER == BIG_ENDIAN
	sc->sc_product_id = (b[0] << 8) | b[1];
#else
	sc->sc_product_id = b[0] | (b[1] << 8);
#endif
	/* TODO: Check the vendor ID as well */
	if (sc->sc_product_id != 0x9000) {
		panic("dme_attach: product id mismatch (0x%hx != 0x9000)",
		    sc->sc_product_id);
	}

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = dme_start_output;
	ifp->if_init = dme_init;
	ifp->if_ioctl = dme_ioctl;
	ifp->if_stop = dme_stop;
	ifp->if_watchdog = NULL;	/* no watchdog at this stage */
	ifp->if_flags = IFF_SIMPLEX | IFF_NOTRAILERS | IFF_BROADCAST |
			IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize ifmedia structures. */
	ifmedia_init(&sc->sc_media, 0, dme_mediachange, dme_mediastatus);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_100_TX, 0, NULL);

	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if (enaddr != NULL)
		memcpy(sc->sc_enaddr, enaddr, sizeof(sc->sc_enaddr));
	/* TODO: Support an EEPROM attached to the DM9000 chip */

	callout_init(&sc->sc_link_callout, 0);
	callout_setfunc(&sc->sc_link_callout, dme_phy_check_link, sc);

	sc->sc_media_status = 0;

	/* Configure DM9000 with the MAC address */
	dme_write_c(sc, DM9000_PAB0, sc->sc_enaddr, 6);

#ifdef DM9000_DEBUG
	{
		uint8_t macAddr[6];
		dme_read_c(sc, DM9000_PAB0, macAddr, 6);
		printf("DM9000 configured with MAC address: ");
		for (int i = 0; i < 6; i++) {
			printf("%02X:", macAddr[i]);
		}
		printf("\n");
	}
#endif

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

#ifdef DM9000_DEBUG
	{
		uint8_t network_state;
		network_state = dme_read(sc, DM9000_NSR);
		printf("DM9000 Link status: ");
		if (network_state & DM9000_NSR_LINKST) {
			if (network_state & DM9000_NSR_SPEED)
				printf("10Mbps");
			else
				printf("100Mbps");
		} else {
			printf("Down");
		}
		printf("\n");
	}
#endif

	io_mode = (dme_read(sc, DM9000_ISR) &
	    DM9000_IOMODE_MASK) >> DM9000_IOMODE_SHIFT;

	DPRINTF(("DM9000 Operation Mode: "));
	switch( io_mode) {
	case DM9000_MODE_16BIT:
		DPRINTF(("16-bit mode"));
		sc->sc_data_width = 2;
		sc->sc_pkt_write = dme_pkt_write_2;
		sc->sc_pkt_read = dme_pkt_read_2;
		break;
	case DM9000_MODE_32BIT:
		DPRINTF(("32-bit mode"));
		sc->sc_data_width = 4;
		panic("32bit mode is unsupported\n");
		break;
	case DM9000_MODE_8BIT:
		DPRINTF(("8-bit mode"));
		sc->sc_data_width = 1;
		sc->sc_pkt_write = dme_pkt_write_1;
		sc->sc_pkt_read = dme_pkt_read_1;
		break;
	default:
		DPRINTF(("Invalid mode"));
		break;
	}
	DPRINTF(("\n"));

	callout_schedule(&sc->sc_link_callout, mstohz(2000));

	return 0;
}

int dme_intr(void *arg)
{
	struct dme_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint8_t status;


	DPRINTF(("dme_intr: Begin\n"));

	/* Disable interrupts */
	dme_write(sc, DM9000_IMR, DM9000_IMR_PAR );

	status = dme_read(sc, DM9000_ISR);
	dme_write(sc, DM9000_ISR, status);

	if (status & DM9000_ISR_PRS) {
		if (ifp->if_flags & IFF_RUNNING )
			dme_receive(sc, ifp);
	}
	if (status & DM9000_ISR_PTS) {
		uint8_t nsr;
		uint8_t tx_status = 0x01; /* Initialize to an error value */

		/* A packet has been transmitted */
		sc->txbusy = 0;

		nsr = dme_read(sc, DM9000_NSR);

		if (nsr & DM9000_NSR_TX1END) {
			tx_status = dme_read(sc, DM9000_TSR1);
			TX_DPRINTF(("dme_intr: Sent using channel 0\n"));
		} else if (nsr & DM9000_NSR_TX2END) {
			tx_status = dme_read(sc, DM9000_TSR2);
			TX_DPRINTF(("dme_intr: Sent using channel 1\n"));
		}

		if (tx_status == 0x0) {
			/* Frame successfully sent */
			ifp->if_opackets++;
		} else {
			ifp->if_oerrors++;
		}

		/* If we have nothing ready to transmit, prepare something */
		if (!sc->txready) {
			dme_prepare(sc, ifp);
		}

		if (sc->txready)
			dme_transmit(sc);

		/* Prepare the next frame */
		dme_prepare(sc, ifp);

	}
#ifdef notyet
	if (status & DM9000_ISR_LNKCHNG) {
	}
#endif

	/* Enable interrupts again */
	dme_write(sc, DM9000_IMR, DM9000_IMR_PAR | DM9000_IMR_PRM |
		 DM9000_IMR_PTM);

	DPRINTF(("dme_intr: End\n"));

	return 1;
}

void
dme_start_output(struct ifnet *ifp)
{
	struct dme_softc *sc;

	sc = ifp->if_softc;

	DPRINTF(("dme_start_output: Begin\n"));

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING) {
		printf("No output\n");
		return;
	}

	if (sc->txbusy && sc->txready) {
		panic("DM9000: Internal error, trying to send without"
		    " any empty queue\n");
	}

	dme_prepare(sc, ifp);

	if (sc->txbusy == 0) {
		/* We are ready to transmit right away */
		dme_transmit(sc);
		dme_prepare(sc, ifp); /* Prepare next one */
	} else {
		/* We need to wait until the current packet has
		 * been transmitted.
		 */
		ifp->if_flags |= IFF_OACTIVE;
	}

	DPRINTF(("dme_start_output: End\n"));
}

void
dme_prepare(struct dme_softc *sc, struct ifnet *ifp)
{
	struct mbuf *bufChain;
	uint16_t length;

	TX_DPRINTF(("dme_prepare: Entering\n"));

	if (sc->txready)
		panic("dme_prepare: Someone called us with txready set\n");

	IFQ_DEQUEUE(&ifp->if_snd, bufChain);
	if (bufChain == NULL) {
		TX_DPRINTF(("dme_prepare: Nothing to transmit\n"));
		ifp->if_flags &= ~IFF_OACTIVE; /* Clear OACTIVE bit */
		return; /* Nothing to transmit */
	}

	/* Element has now been removed from the queue, so we better send it */

	if (ifp->if_bpf)
		bpf_mtap(ifp, bufChain);

	/* Setup the DM9000 to accept the writes, and then write each buf in
	   the chain. */

	TX_DATA_DPRINTF(("dme_prepare: Writing data: "));
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->dme_io, DM9000_MWCMD);
	length = sc->sc_pkt_write(sc, bufChain);
	TX_DATA_DPRINTF(("\n"));

	if (length % sc->sc_data_width != 0) {
		panic("dme_prepare: length is not compatible with IO_MODE");
	}

	sc->txready_length = length;
	sc->txready = 1;

	TX_DPRINTF(("dme_prepare: txbusy: %d\ndme_prepare: "
		"txready: %d, txready_length: %d\n",
		sc->txbusy, sc->txready, sc->txready_length));

	m_freem(bufChain);

	TX_DPRINTF(("dme_prepare: Leaving\n"));
}

int
dme_init(struct ifnet *ifp)
{
	int s;
	struct dme_softc *sc = ifp->if_softc;

	dme_stop(ifp, 0);

	s = splnet();

	dme_reset(sc);

	sc->sc_ethercom.ec_if.if_flags |= IFF_RUNNING;
	sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
	sc->sc_ethercom.ec_if.if_timer = 0;

	splx(s);

	return 0;
}

int
dme_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct dme_softc *sc = ifp->if_softc;
	struct ifreq *ifr = data;
	int s, error = 0;

	s = splnet();

	switch(cmd) {
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if (ifp->if_flags && IFF_RUNNING) {
				/* Address list has changed, reconfigure
				   filter */
				dme_set_addr_filter(sc);
			}
			error = 0;
		}
		break;
	}

	splx(s);
	return error;
}

void
dme_stop(struct ifnet *ifp, int disable)
{
	struct dme_softc *sc = ifp->if_softc;

	/* Not quite sure what to do when called with disable == 0 */
	if (disable) {
		/* Disable RX */
		dme_write(sc, DM9000_RCR, 0x0);
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

int
dme_mediachange(struct ifnet *ifp)
{
	struct dme_softc *sc = ifp->if_softc;

	return dme_set_media(sc, sc->sc_media.ifm_cur->ifm_media);
}

void
dme_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct dme_softc *sc = ifp->if_softc;

	ifmr->ifm_active = sc->sc_media_active;
	ifmr->ifm_status = sc->sc_media_status;
}

void
dme_transmit(struct dme_softc *sc)
{

	TX_DPRINTF(("dme_transmit: PRE: txready: %d, txbusy: %d\n",
		sc->txready, sc->txbusy));

	dme_write(sc, DM9000_TXPLL, sc->txready_length & 0xff);
	dme_write(sc, DM9000_TXPLH, (sc->txready_length >> 8) & 0xff );

	/* Request to send the packet */
	dme_read(sc, DM9000_ISR);

	dme_write(sc, DM9000_TCR, DM9000_TCR_TXREQ);

	sc->txready = 0;
	sc->txbusy = 1;
	sc->txready_length = 0;
}

void
dme_receive(struct dme_softc *sc, struct ifnet *ifp)
{
	uint8_t ready = 0x01;

	DPRINTF(("inside dme_receive\n"));

	while (ready == 0x01) {
		/* Packet received, retrieve it */

		/* Read without address increment to get the ready byte without
		   moving past it. */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->dme_io, DM9000_MRCMDX);
		/* Dummy ready */
		ready = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data);
		ready = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data);
		ready &= 0x03;	/* we only want bits 1:0 */
		if (ready == 0x01) {
			uint8_t		rx_status;
			struct mbuf	*m;

			/* Read with address increment. */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
					  sc->dme_io, DM9000_MRCMD);

			rx_status = sc->sc_pkt_read(sc, ifp, &m);
			if (m == NULL) {
				/* failed to allocate a receive buffer */
				ifp->if_ierrors++;
				RX_DPRINTF(("dme_receive: "
					"Error allocating buffer\n"));
			} else if (rx_status & (DM9000_RSR_CE | DM9000_RSR_PLE)) {
				/* Error while receiving the packet,
				 * discard it and keep track of counters
				 */
				ifp->if_ierrors++;
				RX_DPRINTF(("dme_receive: "
					"Error reciving packet\n"));
			} else if (rx_status & DM9000_RSR_LCS) {
				ifp->if_collisions++;
			} else {
				if (ifp->if_bpf)
					bpf_mtap(ifp, m);
				ifp->if_ipackets++;
				(*ifp->if_input)(ifp, m);
			}

		} else if (ready != 0x00) {
			/* Should this be logged somehow? */
			printf("%s: Resetting chip\n",
			       device_xname(sc->sc_dev));
			dme_reset(sc);
		}
	}
}

void
dme_reset(struct dme_softc *sc)
{
	uint8_t var;

	/* We only re-initialized the PHY in this function the first time it is
	   called. */
	if( !sc->sc_phy_initialized) {
		/* PHY Reset */
		dme_phy_write(sc, DM9000_PHY_BMCR, DM9000_PHY_BMCR_RESET);

		/* PHY Power Down */
		var = dme_read(sc, DM9000_GPR);
		dme_write(sc, DM9000_GPR, var | DM9000_GPR_PHY_PWROFF);
	}

	/* Reset the DM9000 twice, as described in section 2 of the Programming
	   Guide.
	   The PHY is initialized and enabled between those two resets.
	 */

	/* Software Reset*/
	dme_write(sc, DM9000_NCR,
	    DM9000_NCR_RST | DM9000_NCR_LBK_MAC_INTERNAL);
	delay(20);
	dme_write(sc, DM9000_NCR, 0x0);

	if( !sc->sc_phy_initialized) {
		/* PHY Initialization */
		dme_phy_init(sc);

		/* PHY Enable */
		var = dme_read(sc, DM9000_GPR);
		dme_write(sc, DM9000_GPR, var & ~DM9000_GPR_PHY_PWROFF);
		var = dme_read(sc, DM9000_GPCR);
		dme_write(sc, DM9000_GPCR, var | DM9000_GPCR_GPIO0_OUT);

		dme_write(sc, DM9000_NCR,
			  DM9000_NCR_RST | DM9000_NCR_LBK_MAC_INTERNAL);
		delay(20);
		dme_write(sc, DM9000_NCR, 0x0);
	}

	/* Select internal PHY, no wakeup event, no collosion mode,
	 * normal loopback mode.
	 */
	dme_write(sc, DM9000_NCR, DM9000_NCR_LBK_NORMAL );

	/* Will clear TX1END, TX2END, and WAKEST fields by reading DM9000_NSR*/
	dme_read(sc, DM9000_NSR);

	/* Enable wraparound of read/write pointer, packet received latch,
	 * and packet transmitted latch.
	 */
	dme_write(sc, DM9000_IMR,
	    DM9000_IMR_PAR | DM9000_IMR_PRM | DM9000_IMR_PTM);

	/* Setup multicast address filter, and enable RX. */
	dme_set_addr_filter(sc);

	/* Obtain media information from PHY */
	dme_phy_update_media(sc);

	sc->txbusy = 0;
	sc->txready = 0;
	sc->sc_phy_initialized = 1;
}

void
dme_set_addr_filter(struct dme_softc *sc)
{
	struct ether_multi	*enm;
	struct ether_multistep	step;
	struct ethercom		*ec;
	struct ifnet		*ifp;
	uint16_t		af[4];
	int			i;

	ec = &sc->sc_ethercom;
	ifp = &ec->ec_if;

	if (ifp->if_flags & IFF_PROMISC) {
		dme_write(sc, DM9000_RCR, DM9000_RCR_RXEN  |
					  DM9000_RCR_WTDIS |
					  DM9000_RCR_PRMSC);
		ifp->if_flags |= IFF_ALLMULTI;
		return;
	}

	af[0] = af[1] = af[2] = af[3] = 0x0000;
	ifp->if_flags &= ~IFF_ALLMULTI;

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		uint16_t hash;
		if (memcpy(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo))) {
			/*
	                 * We must listen to a range of multicast addresses.
	                 * For now, just accept all multicasts, rather than
	                 * trying to set only those filter bits needed to match
	                 * the range.  (At this time, the only use of address
	                 * ranges is for IP multicast routing, for which the
	                 * range is big enough to require all bits set.)
	                 */
			ifp->if_flags |= IFF_ALLMULTI;
			af[0] = af[1] = af[2] = af[3] = 0xffff;
			break;
		} else {
			hash = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN) & 0x3F;
			af[(uint16_t)(hash>>4)] |= (uint16_t)(1 << (hash % 16));
			ETHER_NEXT_MULTI(step, enm);
		}
	}

	/* Write the multicast address filter */
	for(i=0; i<4; i++) {
		dme_write(sc, DM9000_MAB0+i*2, af[i] & 0xFF);
		dme_write(sc, DM9000_MAB0+i*2+1, (af[i] >> 8) & 0xFF);
	}

	/* Setup RX controls */
	dme_write(sc, DM9000_RCR, DM9000_RCR_RXEN | DM9000_RCR_WTDIS);
}

int
dme_pkt_write_2(struct dme_softc *sc, struct mbuf *bufChain)
{
	int left_over_count = 0; /* Number of bytes from previous mbuf, which
				    need to be written with the next.*/
	uint16_t left_over_buf = 0;
	int length = 0;
	struct mbuf *buf;
	uint8_t *write_ptr;

	/* We expect that the DM9000 has been setup to accept writes before
	   this function is called. */

	for (buf = bufChain; buf != NULL; buf = buf->m_next) {
		int to_write = buf->m_len;

		length += to_write;

		write_ptr = buf->m_data;
		while (to_write > 0 ||
		       (buf->m_next == NULL && left_over_count > 0)
		       ) {
			if (left_over_count > 0) {
				uint8_t b = 0;
				DPRINTF(("dme_pkt_write_16: "
					 "Writing left over byte\n"));

				if (to_write > 0) {
					b = *write_ptr;
					to_write--;
					write_ptr++;

					DPRINTF(("Took single byte\n"));
				} else {
					DPRINTF(("Leftover in last run\n"));
					length++;
				}

				/* Does shift direction depend on endianess? */
				left_over_buf = left_over_buf | (b << 8);

				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
						  sc->dme_data, left_over_buf);
				TX_DATA_DPRINTF(("%02X ", left_over_buf));
				left_over_count = 0;
			} else if ((long)write_ptr % 2 != 0) {
				/* Misaligned data */
				DPRINTF(("dme_pkt_write_16: "
					 "Detected misaligned data\n"));
				left_over_buf = *write_ptr;
				left_over_count = 1;
				write_ptr++;
				to_write--;
			} else {
				int i;
				uint16_t *dptr = (uint16_t *)write_ptr;

				/* A block of aligned data. */
				for(i = 0; i < to_write / 2; i++) {
					/* buf will be half-word aligned
					 * all the time
					 */
					bus_space_write_2(sc->sc_iot,
					    sc->sc_ioh, sc->dme_data, *dptr);
					TX_DATA_DPRINTF(("%02X %02X ",
					    *dptr & 0xFF, (*dptr >> 8) & 0xFF));
					dptr++;
				}

				write_ptr += i * 2;
				if (to_write % 2 != 0) {
					DPRINTF(("dme_pkt_write_16: "
						 "to_write %% 2: %d\n",
						 to_write % 2));
					left_over_count = 1;
					/* XXX: Does this depend on
					 * the endianess?
					 */
					left_over_buf = *write_ptr;

					write_ptr++;
					to_write--;
					DPRINTF(("dme_pkt_write_16: "
						 "to_write (after): %d\n",
						 to_write));
					DPRINTF(("dme_pkt_write_16: i * 2: %d\n",
						 i*2));
				}
				to_write -= i * 2;
			}
		} /* while(...) */
	} /* for(...) */

	return length;
}

int
dme_pkt_read_2(struct dme_softc *sc, struct ifnet *ifp, struct mbuf **outBuf)
{
	uint8_t rx_status;
	struct mbuf *m;
	uint16_t data;
	uint16_t frame_length;
	uint16_t i;
	uint16_t *buf;

	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh, sc->dme_data);

	rx_status = data & 0xFF;
	frame_length = bus_space_read_2(sc->sc_iot,
					sc->sc_ioh, sc->dme_data);
	if (frame_length > ETHER_MAX_LEN) {
		printf("Got frame of length: %d\n", frame_length);
		printf("ETHER_MAX_LEN is: %d\n", ETHER_MAX_LEN);
		panic("Something is rotten");
	}
	RX_DPRINTF(("dme_receive: "
		    "rx_statux: 0x%x, frame_length: %d\n",
		    rx_status, frame_length));


	m = dme_alloc_receive_buffer(ifp, frame_length);
	if (m == NULL) {
		/*
		 * didn't get a receive buffer, so we read the rest of the
		 * packet, throw it away and return an error
		 */
		for (i = 0; i < frame_length; i += 2 ) {
			data = bus_space_read_2(sc->sc_iot,
					sc->sc_ioh, sc->dme_data);
		}
		*outBuf = NULL;
		return 0;
	}

	buf = mtod(m, uint16_t*);

	RX_DPRINTF(("dme_receive: "));

	for (i = 0; i < frame_length; i += 2 ) {
		data = bus_space_read_2(sc->sc_iot,
					sc->sc_ioh, sc->dme_data);
		if ( (frame_length % 2 != 0) &&
		     (i == frame_length - 1) ) {
			data = data & 0xff;
			RX_DPRINTF((" L "));
		}
		*buf = data;
		buf++;
		RX_DATA_DPRINTF(("%02X %02X ", data & 0xff,
				 (data >> 8) & 0xff));
	}

	RX_DATA_DPRINTF(("\n"));
	RX_DPRINTF(("Read %d bytes\n", i));

	*outBuf = m;
	return rx_status;
}

int
dme_pkt_write_1(struct dme_softc *sc, struct mbuf *bufChain)
{
	int length = 0, i;
	struct mbuf *buf;
	uint8_t *write_ptr;

	/* We expect that the DM9000 has been setup to accept writes before
	   this function is called. */

	for (buf = bufChain; buf != NULL; buf = buf->m_next) {
		int to_write = buf->m_len;

		length += to_write;

		write_ptr = buf->m_data;
		for (i = 0; i < to_write; i++) {
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    sc->dme_data, *write_ptr);
			write_ptr++;
		}
	} /* for(...) */

	return length;
}

int
dme_pkt_read_1(struct dme_softc *sc, struct ifnet *ifp, struct mbuf **outBuf)
{
	uint8_t rx_status;
	struct mbuf *m;
	uint8_t *buf;
	uint16_t frame_length;
	uint16_t i, reg;
	uint8_t data;

	reg = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data);
	reg |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data) << 8;
	rx_status = reg & 0xFF;

	reg = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data);
	reg |= bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data) << 8;
	frame_length = reg;

	if (frame_length > ETHER_MAX_LEN) {
		printf("Got frame of length: %d\n", frame_length);
		printf("ETHER_MAX_LEN is: %d\n", ETHER_MAX_LEN);
		panic("Something is rotten");
	}
	RX_DPRINTF(("dme_receive: "
		    "rx_statux: 0x%x, frame_length: %d\n",
		    rx_status, frame_length));


	m = dme_alloc_receive_buffer(ifp, frame_length);
	if (m == NULL) {
		/*
		 * didn't get a receive buffer, so we read the rest of the
		 * packet, throw it away and return an error
		 */
		for (i = 0; i < frame_length; i++ ) {
			data = bus_space_read_2(sc->sc_iot,
					sc->sc_ioh, sc->dme_data);
		}
		*outBuf = NULL;
		return 0;
	}

	buf = mtod(m, uint8_t *);

	RX_DPRINTF(("dme_receive: "));

	for (i = 0; i< frame_length; i += 1 ) {
		data = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data);
		*buf = data;
		buf++;
		RX_DATA_DPRINTF(("%02X ", data));
	}

	RX_DATA_DPRINTF(("\n"));
	RX_DPRINTF(("Read %d bytes\n", i));

	*outBuf = m;
	return rx_status;
}

struct mbuf*
dme_alloc_receive_buffer(struct ifnet *ifp, unsigned int frame_length)
{
	struct dme_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int pad;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) return NULL;

	m->m_pkthdr.rcvif = ifp;
	/* Ensure that we always allocate an even number of
	 * bytes in order to avoid writing beyond the buffer
	 */
	m->m_pkthdr.len = frame_length + (frame_length % sc->sc_data_width);
	pad = ALIGN(sizeof(struct ether_header)) -
		sizeof(struct ether_header);
	/* All our frames have the CRC attached */
	m->m_flags |= M_HASFCS;
	if (m->m_pkthdr.len + pad > MHLEN )
		MCLGET(m, M_DONTWAIT);

	m->m_data += pad;
	m->m_len = frame_length + (frame_length % sc->sc_data_width);

	return m;
}
