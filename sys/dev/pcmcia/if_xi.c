/*	$NetBSD: if_xi.c,v 1.75 2015/05/25 08:29:01 ozaki-r Exp $ */
/*	OpenBSD: if_xe.c,v 1.9 1999/09/16 11:28:42 niklas Exp 	*/

/*
 * Copyright (c) 2004 Charles M. Hannum.  All rights reserved.
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
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

/*
 * Copyright (c) 1999 Niklas Hallqvist, Brandon Creighton, Job de Haas
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
 *	This product includes software developed by Niklas Hallqvist,
 *	Brandon Creighton and Job de Haas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * A driver for Xircom CreditCard PCMCIA Ethernet adapters.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_xi.c,v 1.75 2015/05/25 08:29:01 ozaki-r Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

/*
 * Maximum number of bytes to read per interrupt.  Linux recommends
 * somewhere between 2000-22000.
 * XXX This is currently a hard maximum.
 */
#define MAX_BYTES_INTR 12000

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/pcmcia/if_xireg.h>
#include <dev/pcmcia/if_xivar.h>

#ifdef __GNUC__
#define INLINE	inline
#else
#define INLINE
#endif	/* __GNUC__ */

#define	XIDEBUG
#define	XIDEBUG_VALUE	0

#ifdef XIDEBUG
#define DPRINTF(cat, x) if (xidebug & (cat)) printf x

#define XID_CONFIG	0x01
#define XID_MII		0x02
#define XID_INTR	0x04
#define XID_FIFO	0x08
#define	XID_MCAST	0x10

#ifdef XIDEBUG_VALUE
int xidebug = XIDEBUG_VALUE;
#else
int xidebug = 0;
#endif
#else
#define DPRINTF(cat, x) (void)0
#endif

#define STATIC

STATIC int xi_enable(struct xi_softc *);
STATIC void xi_disable(struct xi_softc *);
STATIC void xi_cycle_power(struct xi_softc *);
STATIC int xi_ether_ioctl(struct ifnet *, u_long cmd, void *);
STATIC void xi_full_reset(struct xi_softc *);
STATIC void xi_init(struct xi_softc *);
STATIC int xi_ioctl(struct ifnet *, u_long, void *);
STATIC int xi_mdi_read(device_t, int, int);
STATIC void xi_mdi_write(device_t, int, int, int);
STATIC int xi_mediachange(struct ifnet *);
STATIC u_int16_t xi_get(struct xi_softc *);
STATIC void xi_reset(struct xi_softc *);
STATIC void xi_set_address(struct xi_softc *);
STATIC void xi_start(struct ifnet *);
STATIC void xi_statchg(struct ifnet *);
STATIC void xi_stop(struct xi_softc *);
STATIC void xi_watchdog(struct ifnet *);

void
xi_attach(struct xi_softc *sc, u_int8_t *myea)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

#if 0
	/*
	 * Configuration as advised by DINGO documentation.
	 * Dingo has some extra configuration registers in the CCR space.
	 */
	if (sc->sc_chipset >= XI_CHIPSET_DINGO) {
		struct pcmcia_mem_handle pcmh;
		int ccr_window;
		bus_size_t ccr_offset;

		/* get access to the DINGO CCR space */
		if (pcmcia_mem_alloc(psc->sc_pf, PCMCIA_CCR_SIZE_DINGO,
			&pcmh)) {
			DPRINTF(XID_CONFIG, ("xi: bad mem alloc\n"));
			goto fail;
		}
		if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR,
			psc->sc_pf->ccr_base, PCMCIA_CCR_SIZE_DINGO,
			&pcmh, &ccr_offset, &ccr_window)) {
			DPRINTF(XID_CONFIG, ("xi: bad mem map\n"));
			pcmcia_mem_free(psc->sc_pf, &pcmh);
			goto fail;
		}

		/* enable the second function - usually modem */
		bus_space_write_1(pcmh.memt, pcmh.memh,
		    ccr_offset + PCMCIA_CCR_DCOR0, PCMCIA_CCR_DCOR0_SFINT);
		bus_space_write_1(pcmh.memt, pcmh.memh,
		    ccr_offset + PCMCIA_CCR_DCOR1,
		    PCMCIA_CCR_DCOR1_FORCE_LEVIREQ | PCMCIA_CCR_DCOR1_D6);
		bus_space_write_1(pcmh.memt, pcmh.memh,
		    ccr_offset + PCMCIA_CCR_DCOR2, 0);
		bus_space_write_1(pcmh.memt, pcmh.memh,
		    ccr_offset + PCMCIA_CCR_DCOR3, 0);
		bus_space_write_1(pcmh.memt, pcmh.memh,
		    ccr_offset + PCMCIA_CCR_DCOR4, 0);

		/* We don't need them anymore and can free them (I think). */
		pcmcia_mem_unmap(psc->sc_pf, ccr_window);
		pcmcia_mem_free(psc->sc_pf, &pcmh);
	}
#endif

	/* Reset and initialize the card. */
	xi_full_reset(sc);

	printf("%s: MAC address %s\n", device_xname(sc->sc_dev), ether_sprintf(myea));

	ifp = &sc->sc_ethercom.ec_if;
	/* Initialize the ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = xi_start;
	ifp->if_ioctl = xi_ioctl;
	ifp->if_watchdog = xi_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_NOTRAILERS | IFF_SIMPLEX | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* 802.1q capability */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, myea);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = xi_mdi_read;
	sc->sc_mii.mii_writereg = xi_mdi_write;
	sc->sc_mii.mii_statchg = xi_statchg;
	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, 0, xi_mediachange,
	    ether_mediastatus);
	DPRINTF(XID_MII | XID_CONFIG,
	    ("xi: bmsr %x\n", xi_mdi_read(sc->sc_dev, 0, 1)));

	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
		MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL)
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO, 0,
		    NULL);
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);

	rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);
}

int
xi_detach(device_t self, int flags)
{
	struct xi_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	DPRINTF(XID_CONFIG, ("xi_detach()\n"));

	xi_disable(sc);

	rnd_detach_source(&sc->sc_rnd_source);

	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

	return 0;
}

int
xi_intr(void *arg)
{
	struct xi_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int8_t esr, rsr, isr, rx_status;
	u_int16_t tx_status, recvcount = 0, tempint;

	DPRINTF(XID_CONFIG, ("xi_intr()\n"));

	if (sc->sc_enabled == 0 || !device_is_active(sc->sc_dev))
		return (0);

	ifp->if_timer = 0;	/* turn watchdog timer off */

	PAGE(sc, 0);
	if (sc->sc_chipset >= XI_CHIPSET_MOHAWK) {
		/* Disable interrupt (Linux does it). */
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, CR, 0);
	}

	esr = bus_space_read_1(sc->sc_bst, sc->sc_bsh, ESR);
	isr = bus_space_read_1(sc->sc_bst, sc->sc_bsh, ISR0);
	rsr = bus_space_read_1(sc->sc_bst, sc->sc_bsh, RSR);

	/* Check to see if card has been ejected. */
	if (isr == 0xff) {
#ifdef DIAGNOSTIC
		printf("%s: interrupt for dead card\n",
		    device_xname(sc->sc_dev));
#endif
		goto end;
	}
	DPRINTF(XID_INTR, ("xi: isr=%02x\n", isr));

	PAGE(sc, 0x40);
	rx_status =
	    bus_space_read_1(sc->sc_bst, sc->sc_bsh, RXST0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, RXST0, ~rx_status & 0xff);
	tx_status =
	    bus_space_read_1(sc->sc_bst, sc->sc_bsh, TXST0);
	tx_status |=
	    bus_space_read_1(sc->sc_bst, sc->sc_bsh, TXST1) << 8;
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, TXST0, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, TXST1, 0);
	DPRINTF(XID_INTR, ("xi: rx_status=%02x tx_status=%04x\n", rx_status,
	    tx_status));

	PAGE(sc, 0);
	while (esr & FULL_PKT_RCV) {
		if (!(rsr & RSR_RX_OK))
			break;

		/* Compare bytes read this interrupt to hard maximum. */
		if (recvcount > MAX_BYTES_INTR) {
			DPRINTF(XID_INTR,
			    ("xi: too many bytes this interrupt\n"));
			ifp->if_iqdrops++;
			/* Drop packet. */
			bus_space_write_2(sc->sc_bst, sc->sc_bsh, DO0,
			    DO_SKIP_RX_PKT);
		}
		tempint = xi_get(sc);	/* XXX doesn't check the error! */
		recvcount += tempint;
		ifp->if_ibytes += tempint;
		esr = bus_space_read_1(sc->sc_bst, sc->sc_bsh, ESR);
		rsr = bus_space_read_1(sc->sc_bst, sc->sc_bsh, RSR);
	}

	/* Packet too long? */
	if (rsr & RSR_TOO_LONG) {
		ifp->if_ierrors++;
		DPRINTF(XID_INTR, ("xi: packet too long\n"));
	}

	/* CRC error? */
	if (rsr & RSR_CRCERR) {
		ifp->if_ierrors++;
		DPRINTF(XID_INTR, ("xi: CRC error detected\n"));
	}

	/* Alignment error? */
	if (rsr & RSR_ALIGNERR) {
		ifp->if_ierrors++;
		DPRINTF(XID_INTR, ("xi: alignment error detected\n"));
	}

	/* Check for rx overrun. */
	if (rx_status & RX_OVERRUN) {
		ifp->if_ierrors++;
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, CR, CLR_RX_OVERRUN);
		DPRINTF(XID_INTR, ("xi: overrun cleared\n"));
	}

	/* Try to start more packets transmitting. */
	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		xi_start(ifp);

	/* Detected excessive collisions? */
	if ((tx_status & EXCESSIVE_COLL) && ifp->if_opackets > 0) {
		DPRINTF(XID_INTR, ("xi: excessive collisions\n"));
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, CR, RESTART_TX);
		ifp->if_oerrors++;
	}

	if ((tx_status & TX_ABORT) && ifp->if_opackets > 0)
		ifp->if_oerrors++;

	/* have handled the interrupt */
	rnd_add_uint32(&sc->sc_rnd_source, tx_status);

end:
	/* Reenable interrupts. */
	PAGE(sc, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, CR, ENABLE_INT);

	return (1);
}

/*
 * Pull a packet from the card into an mbuf chain.
 */
STATIC u_int16_t
xi_get(struct xi_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *top, **mp, *m;
	u_int16_t pktlen, len, recvcount = 0;
	u_int8_t *data;

	DPRINTF(XID_CONFIG, ("xi_get()\n"));

	PAGE(sc, 0);
	pktlen =
	    bus_space_read_2(sc->sc_bst, sc->sc_bsh, RBC0) & RBC_COUNT_MASK;

	DPRINTF(XID_CONFIG, ("xi_get: pktlen=%d\n", pktlen));

	if (pktlen == 0) {
		/*
		 * XXX At least one CE2 sets RBC0 == 0 occasionally, and only
		 * when MPE is set.  It is not known why.
		 */
		return (0);
	}

	/* XXX should this be incremented now ? */
	recvcount += pktlen;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (recvcount);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = pktlen;
	len = MHLEN;
	top = NULL;
	mp = &top;

	while (pktlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return (recvcount);
			}
			len = MLEN;
		}
		if (pktlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (!(m->m_flags & M_EXT)) {
				m_freem(m);
				m_freem(top);
				return (recvcount);
			}
			len = MCLBYTES;
		}
		if (top == NULL) {
			char *newdata = (char *)ALIGN(m->m_data +
			    sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}
		len = min(pktlen, len);
		data = mtod(m, u_int8_t *);
		if (len > 1) {
		        len &= ~1;
			bus_space_read_multi_2(sc->sc_bst, sc->sc_bsh, EDP,
			    (u_int16_t *)data, len>>1);
		} else
			*data = bus_space_read_1(sc->sc_bst, sc->sc_bsh, EDP);
		m->m_len = len;
		pktlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	/* Skip Rx packet. */
	bus_space_write_2(sc->sc_bst, sc->sc_bsh, DO0, DO_SKIP_RX_PKT);

	if (top == NULL)
		return recvcount;

	/* Trim the CRC off the end of the packet. */
	m_adj(top, -ETHER_CRC_LEN);

	ifp->if_ipackets++;

	bpf_mtap(ifp, top);

	(*ifp->if_input)(ifp, top);
	return (recvcount);
}

/*
 * Serial management for the MII.
 * The DELAY's below stem from the fact that the maximum frequency
 * acceptable on the MDC pin is 2.5 MHz and fast processors can easily
 * go much faster than that.
 */

/* Let the MII serial management be idle for one period. */
static INLINE void xi_mdi_idle(struct xi_softc *);
static INLINE void
xi_mdi_idle(struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Drive MDC low... */
	bus_space_write_1(bst, bsh, GP2, MDC_LOW);
	DELAY(1);

	/* and high again. */
	bus_space_write_1(bst, bsh, GP2, MDC_HIGH);
	DELAY(1);
}

/* Pulse out one bit of data. */
static INLINE void xi_mdi_pulse(struct xi_softc *, int);
static INLINE void
xi_mdi_pulse(struct xi_softc *sc, int data)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t bit = data ? MDIO_HIGH : MDIO_LOW;

	/* First latch the data bit MDIO with clock bit MDC low...*/
	bus_space_write_1(bst, bsh, GP2, bit | MDC_LOW);
	DELAY(1);

	/* then raise the clock again, preserving the data bit. */
	bus_space_write_1(bst, bsh, GP2, bit | MDC_HIGH);
	DELAY(1);
}

/* Probe one bit of data. */
static INLINE int xi_mdi_probe(struct xi_softc *sc);
static INLINE int
xi_mdi_probe(struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t x;

	/* Pull clock bit MDCK low... */
	bus_space_write_1(bst, bsh, GP2, MDC_LOW);
	DELAY(1);

	/* Read data and drive clock high again. */
	x = bus_space_read_1(bst, bsh, GP2);
	bus_space_write_1(bst, bsh, GP2, MDC_HIGH);
	DELAY(1);

	return (x & MDIO);
}

/* Pulse out a sequence of data bits. */
static INLINE void xi_mdi_pulse_bits(struct xi_softc *, u_int32_t, int);
static INLINE void
xi_mdi_pulse_bits(struct xi_softc *sc, u_int32_t data, int len)
{
	u_int32_t mask;

	for (mask = 1 << (len - 1); mask; mask >>= 1)
		xi_mdi_pulse(sc, data & mask);
}

/* Read a PHY register. */
STATIC int
xi_mdi_read(device_t self, int phy, int reg)
{
	struct xi_softc *sc = device_private(self);
	int i;
	u_int32_t mask;
	u_int32_t data = 0;

	PAGE(sc, 2);
	for (i = 0; i < 32; i++)	/* Synchronize. */
		xi_mdi_pulse(sc, 1);
	xi_mdi_pulse_bits(sc, 0x06, 4); /* Start + Read opcode */
	xi_mdi_pulse_bits(sc, phy, 5);	/* PHY address */
	xi_mdi_pulse_bits(sc, reg, 5);	/* PHY register */
	xi_mdi_idle(sc);		/* Turn around. */
	xi_mdi_probe(sc);		/* Drop initial zero bit. */

	for (mask = 1 << 15; mask; mask >>= 1) {
		if (xi_mdi_probe(sc))
			data |= mask;
	}
	xi_mdi_idle(sc);

	DPRINTF(XID_MII,
	    ("xi_mdi_read: phy %d reg %d -> %x\n", phy, reg, data));

	return (data);
}

/* Write a PHY register. */
STATIC void
xi_mdi_write(device_t self, int phy, int reg, int value)
{
	struct xi_softc *sc = device_private(self);
	int i;

	PAGE(sc, 2);
	for (i = 0; i < 32; i++)	/* Synchronize. */
		xi_mdi_pulse(sc, 1);
	xi_mdi_pulse_bits(sc, 0x05, 4); /* Start + Write opcode */
	xi_mdi_pulse_bits(sc, phy, 5);	/* PHY address */
	xi_mdi_pulse_bits(sc, reg, 5);	/* PHY register */
	xi_mdi_pulse_bits(sc, 0x02, 2); /* Turn around. */
	xi_mdi_pulse_bits(sc, value, 16);	/* Write the data */
	xi_mdi_idle(sc);		/* Idle away. */

	DPRINTF(XID_MII,
	    ("xi_mdi_write: phy %d reg %d val %x\n", phy, reg, value));
}

STATIC void
xi_statchg(struct ifnet *ifp)
{
	/* XXX Update ifp->if_baudrate */
}

/*
 * Change media according to request.
 */
STATIC int
xi_mediachange(struct ifnet *ifp)
{
	int s;

	DPRINTF(XID_CONFIG, ("xi_mediachange()\n"));

	if (ifp->if_flags & IFF_UP) {
		s = splnet();
		xi_init(ifp->if_softc);
		splx(s);
	}
	return (0);
}

STATIC void
xi_reset(struct xi_softc *sc)
{
	int s;

	DPRINTF(XID_CONFIG, ("xi_reset()\n"));

	s = splnet();
	xi_stop(sc);
	xi_init(sc);
	splx(s);
}

STATIC void
xi_watchdog(struct ifnet *ifp)
{
	struct xi_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;

	xi_reset(sc);
}

STATIC void
xi_stop(register struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	DPRINTF(XID_CONFIG, ("xi_stop()\n"));

	PAGE(sc, 0x40);
	bus_space_write_1(bst, bsh, CMD0, DISABLE_RX);

	/* Disable interrupts. */
	PAGE(sc, 0);
	bus_space_write_1(bst, bsh, CR, 0);

	PAGE(sc, 1);
	bus_space_write_1(bst, bsh, IMR0, 0);

	/* Cancel watchdog timer. */
	sc->sc_ethercom.ec_if.if_timer = 0;
}

STATIC int
xi_enable(struct xi_softc *sc)
{
	int error;

	if (!sc->sc_enabled) {
		error = (*sc->sc_enable)(sc);
		if (error)
			return (error);
		sc->sc_enabled = 1;
		xi_full_reset(sc);
	}
	return (0);
}

STATIC void
xi_disable(struct xi_softc *sc)
{

	if (sc->sc_enabled) {
		sc->sc_enabled = 0;
		(*sc->sc_disable)(sc);
	}
}

STATIC void
xi_init(struct xi_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	DPRINTF(XID_CONFIG, ("xi_init()\n"));

	/* Setup the ethernet interrupt mask. */
	PAGE(sc, 1);
	bus_space_write_1(bst, bsh, IMR0,
	    ISR_TX_OFLOW | ISR_PKT_TX | ISR_MAC_INT | /* ISR_RX_EARLY | */
	    ISR_RX_FULL | ISR_RX_PKT_REJ | ISR_FORCED_INT);
	if (sc->sc_chipset < XI_CHIPSET_DINGO) {
		/* XXX What is this?  Not for Dingo at least. */
		/* Unmask TX underrun detection */
		bus_space_write_1(bst, bsh, IMR1, 1);
	}

	/* Enable interrupts. */
	PAGE(sc, 0);
	bus_space_write_1(bst, bsh, CR, ENABLE_INT);

	xi_set_address(sc);

	PAGE(sc, 0x40);
	bus_space_write_1(bst, bsh, CMD0, ENABLE_RX | ONLINE);

	PAGE(sc, 0);

	/* Set current media. */
	mii_mediachg(&sc->sc_mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	xi_start(ifp);
}

/*
 * Start outputting on the interface.
 * Always called as splnet().
 */
STATIC void
xi_start(struct ifnet *ifp)
{
	struct xi_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	unsigned int s, len, pad = 0;
	struct mbuf *m0, *m;
	u_int16_t space;

	DPRINTF(XID_CONFIG, ("xi_start()\n"));

	/* Don't transmit if interface is busy or not running. */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING) {
		DPRINTF(XID_CONFIG, ("xi: interface busy or not running\n"));
		return;
	}

	/* Peek at the next packet. */
	IFQ_POLL(&ifp->if_snd, m0);
	if (m0 == 0)
		return;

	/* We need to use m->m_pkthdr.len, so require the header. */
	if (!(m0->m_flags & M_PKTHDR))
		panic("xi_start: no header mbuf");

	len = m0->m_pkthdr.len;

#if 1
	/* Pad to ETHER_MIN_LEN - ETHER_CRC_LEN. */
	if (len < ETHER_MIN_LEN - ETHER_CRC_LEN)
		pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;
#else
	pad = 0;
#endif

	PAGE(sc, 0);

	bus_space_write_2(bst, bsh, TRS, (u_int16_t)len + pad + 2);
	space = bus_space_read_2(bst, bsh, TSO) & 0x7fff;
	if (len + pad + 2 > space) {
		DPRINTF(XID_FIFO,
		    ("xi: not enough space in output FIFO (%d > %d)\n",
		    len + pad + 2, space));
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m0);

	bpf_mtap(ifp, m0);

	/*
	 * Do the output at splhigh() so that an interrupt from another device
	 * won't cause a FIFO underrun.
	 */
	s = splhigh();

	bus_space_write_2(bst, bsh, EDP, (u_int16_t)len + pad);
	for (m = m0; m; ) {
		if (m->m_len > 1)
			bus_space_write_multi_2(bst, bsh, EDP,
			    mtod(m, u_int16_t *), m->m_len>>1);
		if (m->m_len & 1) {
			DPRINTF(XID_CONFIG, ("xi: XXX odd!\n"));
			bus_space_write_1(bst, bsh, EDP,
			    *(mtod(m, u_int8_t *) + m->m_len - 1));
		}
		MFREE(m, m0);
		m = m0;
	}
	DPRINTF(XID_CONFIG, ("xi: len=%d pad=%d total=%d\n", len, pad, len+pad+4));
	if (sc->sc_chipset >= XI_CHIPSET_MOHAWK)
		bus_space_write_1(bst, bsh, CR, TX_PKT | ENABLE_INT);
	else {
		for (; pad > 1; pad -= 2)
			bus_space_write_2(bst, bsh, EDP, 0);
		if (pad == 1)
			bus_space_write_1(bst, bsh, EDP, 0);
	}

	splx(s);

	ifp->if_timer = 5;
	++ifp->if_opackets;
}

STATIC int
xi_ether_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct xi_softc *sc = ifp->if_softc;
	int error;

	DPRINTF(XID_CONFIG, ("xi_ether_ioctl()\n"));

	switch (cmd) {
	case SIOCINITIFADDR:
		if ((error = xi_enable(sc)) != 0)
			break;

		ifp->if_flags |= IFF_UP;

		xi_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif	/* INET */


		default:
			break;
		}
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

STATIC int
xi_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct xi_softc *sc = ifp->if_softc;
	int s, error = 0;

	DPRINTF(XID_CONFIG, ("xi_ioctl()\n"));

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		error = xi_ether_ioctl(ifp, cmd, data);
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
			xi_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			xi_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped,
			 * start it.
			 */
			if ((error = xi_enable(sc)) != 0)
				break;
			xi_init(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Reset the interface to pick up changes in any
			 * other flags that affect hardware registers.
			 */
			xi_set_address(sc);
			break;
		case 0:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->sc_enabled == 0) {
			error = EIO;
			break;
		}
		/*FALLTHROUGH*/
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				xi_set_address(sc);
			error = 0;
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return (error);
}

STATIC void
xi_set_address(struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ethercom *ether = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multistep step;
	struct ether_multi *enm;
	int page, num;
	int i;
	u_int8_t x;
	const u_int8_t *enaddr;
	u_int8_t indaddr[64];

	DPRINTF(XID_CONFIG, ("xi_set_address()\n"));

	enaddr = (const u_int8_t *)CLLADDR(ifp->if_sadl);
	if (sc->sc_chipset >= XI_CHIPSET_MOHAWK)
		for (i = 0; i < 6; i++)
			indaddr[i] = enaddr[5 - i];
	else
		for (i = 0; i < 6; i++)
			indaddr[i] = enaddr[i];
	num = 1;

	if (ether->ec_multicnt > 9) {
		ifp->if_flags |= IFF_ALLMULTI;
		goto done;
	}

	ETHER_FIRST_MULTI(step, ether, enm);
	for (; enm; num++) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * The multicast address is really a range;
			 * it's easier just to accept all multicasts.
			 * XXX should we be setting IFF_ALLMULTI here?
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			goto done;
		}
		if (sc->sc_chipset >= XI_CHIPSET_MOHAWK)
			for (i = 0; i < 6; i++)
				indaddr[num * 6 + i] = enm->enm_addrlo[5 - i];
		else
			for (i = 0; i < 6; i++)
				indaddr[num * 6 + i] = enm->enm_addrlo[i];
		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;

done:
	if (num < 10)
		memset(&indaddr[num * 6], 0xff, 6 * (10 - num));

	for (page = 0; page < 8; page++) {
#ifdef XIDEBUG
		if (xidebug & XID_MCAST) {
			printf("page %d before:", page);
			for (i = 0; i < 8; i++)
				printf(" %02x", indaddr[page * 8 + i]);
			printf("\n");
		}
#endif

		PAGE(sc, 0x50 + page);
		bus_space_write_region_1(bst, bsh, IA, &indaddr[page * 8],
		    page == 7 ? 4 : 8);
		/*
		 * XXX
		 * Without this delay, the address registers on my CE2 get
		 * trashed the first and I have to cycle it.  I have no idea
		 * why.  - mycroft, 2004/08/09
		 */
		DELAY(50);

#ifdef XIDEBUG
		if (xidebug & XID_MCAST) {
			bus_space_read_region_1(bst, bsh, IA,
			    &indaddr[page * 8], page == 7 ? 4 : 8);
			printf("page %d after: ", page);
			for (i = 0; i < 8; i++)
				printf(" %02x", indaddr[page * 8 + i]);
			printf("\n");
		}
#endif
	}

	PAGE(sc, 0x42);
	x = SWC1_IND_ADDR;
	if (ifp->if_flags & IFF_PROMISC)
		x |= SWC1_PROMISC;
	if (ifp->if_flags & (IFF_ALLMULTI|IFF_PROMISC))
		x |= SWC1_MCAST_PROM;
	if (!LIST_FIRST(&sc->sc_mii.mii_phys))
		x |= SWC1_AUTO_MEDIA;
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, SWC1, x);
}

STATIC void
xi_cycle_power(struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	DPRINTF(XID_CONFIG, ("xi_cycle_power()\n"));

	PAGE(sc, 4);
	DELAY(1);
	bus_space_write_1(bst, bsh, GP1, 0);
	tsleep(&xi_cycle_power, PWAIT, "xipwr1", hz * 40 / 1000);
	if (sc->sc_chipset >= XI_CHIPSET_MOHAWK)
		bus_space_write_1(bst, bsh, GP1, POWER_UP);
	else
		/* XXX What is bit 2 (aka AIC)? */
		bus_space_write_1(bst, bsh, GP1, POWER_UP | 4);
	tsleep(&xi_cycle_power, PWAIT, "xipwr2", hz * 20 / 1000);
}

STATIC void
xi_full_reset(struct xi_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t x;

	DPRINTF(XID_CONFIG, ("xi_full_reset()\n"));

	/* Do an as extensive reset as possible on all functions. */
	xi_cycle_power(sc);
	bus_space_write_1(bst, bsh, CR, SOFT_RESET);
	tsleep(&xi_full_reset, PWAIT, "xirst1", hz * 20 / 1000);
	bus_space_write_1(bst, bsh, CR, 0);
	tsleep(&xi_full_reset, PWAIT, "xirst2", hz * 20 / 1000);
	PAGE(sc, 4);
	if (sc->sc_chipset >= XI_CHIPSET_MOHAWK) {
		/*
		 * Drive GP1 low to power up ML6692 and GP2 high to power up
		 * the 10MHz chip.  XXX What chip is that?  The phy?
		 */
		bus_space_write_1(bst, bsh, GP0, GP1_OUT | GP2_OUT | GP2_WR);
	}
	tsleep(&xi_full_reset, PWAIT, "xirst3", hz * 500 / 1000);

	/* Get revision information.  XXX Symbolic constants. */
	sc->sc_rev = bus_space_read_1(bst, bsh, BV) &
	    ((sc->sc_chipset >= XI_CHIPSET_MOHAWK) ? 0x70 : 0x30) >> 4;
	DPRINTF(XID_CONFIG, ("xi: rev=%02x\n", sc->sc_rev));

	/* Media selection.  XXX Maybe manual overriding too? */
	if (sc->sc_chipset < XI_CHIPSET_MOHAWK) {
		/*
		 * XXX I have no idea what this really does, it is from the
		 * Linux driver.
		 */
		bus_space_write_1(bst, bsh, GP0, GP1_OUT);
	}
	tsleep(&xi_full_reset, PWAIT, "xirst4", hz * 40 / 1000);

	/*
	 * Disable source insertion.
	 * XXX Dingo does not have this bit, but Linux does it unconditionally.
	 */
	if (sc->sc_chipset < XI_CHIPSET_DINGO) {
		PAGE(sc, 0x42);
		bus_space_write_1(bst, bsh, SWC0, 0x20);
	}

	/* Set the local memory dividing line. */
	if (sc->sc_rev != 1) {
		PAGE(sc, 2);
		/* XXX Symbolic constant preferrable. */
		bus_space_write_2(bst, bsh, RBS0, 0x2000);
	}

	/*
	 * Apparently the receive byte pointer can be bad after a reset, so
	 * we hardwire it correctly.
	 */
	PAGE(sc, 0);
	bus_space_write_2(bst, bsh, DO0, DO_CHG_OFFSET);

	/* Setup ethernet MAC registers. XXX Symbolic constants. */
	PAGE(sc, 0x40);
	bus_space_write_1(bst, bsh, RX0MSK,
	    PKT_TOO_LONG | CRC_ERR | RX_OVERRUN | RX_ABORT | RX_OK);
	bus_space_write_1(bst, bsh, TX0MSK,
	    CARRIER_LOST | EXCESSIVE_COLL | TX_UNDERRUN | LATE_COLLISION |
	    SQE | TX_ABORT | TX_OK);
	if (sc->sc_chipset < XI_CHIPSET_DINGO)
		/* XXX From Linux, dunno what 0xb0 means. */
		bus_space_write_1(bst, bsh, TX1MSK, 0xb0);
	bus_space_write_1(bst, bsh, RXST0, 0);
	bus_space_write_1(bst, bsh, TXST0, 0);
	bus_space_write_1(bst, bsh, TXST1, 0);

	PAGE(sc, 2);

	/* Enable MII function if available. */
	x = 0;
	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		x |= SELECT_MII;
	bus_space_write_1(bst, bsh, MSR, x);
	tsleep(&xi_full_reset, PWAIT, "xirst5", hz * 20 / 1000);

	/* Configure the LED registers. */
	/* XXX This is not good for 10base2. */
	bus_space_write_1(bst, bsh, LED,
	    (LED_TX_ACT << LED1_SHIFT) | (LED_10MB_LINK << LED0_SHIFT));
	if (sc->sc_chipset >= XI_CHIPSET_DINGO)
		bus_space_write_1(bst, bsh, LED3, LED_100MB_LINK << LED3_SHIFT);

	/*
	 * The Linux driver says this:
	 * We should switch back to page 0 to avoid a bug in revision 0
	 * where regs with offset below 8 can't be read after an access
	 * to the MAC registers.
	 */
	PAGE(sc, 0);
}
