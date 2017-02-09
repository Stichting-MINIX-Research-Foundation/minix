/*	$NetBSD: if_iy.c,v 1.94 2015/04/13 16:33:24 riastradh Exp $	*/
/* #define IYDEBUG */
/* #define IYMEMDEBUG */

/*-
 * Copyright (c) 1996,2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * Supported hardware:
 *
 * - Intel EtherExpress Pro/10.
 * - possibly other boards using the i82595 chip and no special tweaks.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_iy.c,v 1.94 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#include <net/if_ether.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif


#if defined(SIOCSIFMEDIA)
#include <net/if_media.h>
#endif

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i82595reg.h>

/* XXX why isn't this centralized? */
#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_read_stream_2		bus_space_read_2
#define bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

/*
 * Ethernet status, per interface.
 */
struct iy_softc {
	device_t sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct ethercom sc_ethercom;

	struct ifmedia iy_ifmedia;
	int iy_media;

	int mappedirq;

	int hard_vers;

	int promisc;

	int sram, tx_size, rx_size;

	int tx_start, tx_end, tx_last;
	int rx_start;

	int doing_mc_setup;
#ifdef IYDEBUG
	int sc_debug;
#endif

	krndsource_t rnd_source;
};

void iywatchdog(struct ifnet *);
int iyioctl(struct ifnet *, u_long, void *);
int iyintr(void *);
void iyinit(struct iy_softc *);
void iystop(struct iy_softc *);
void iystart(struct ifnet *);

void iy_intr_rx(struct iy_softc *);
void iy_intr_tx(struct iy_softc *);

void iyreset(struct iy_softc *);
void iy_readframe(struct iy_softc *, int);
void iy_drop_packet_buffer(struct iy_softc *);
void iy_find_mem_size(struct iy_softc *);
void iyrint(struct iy_softc *);
void iytint(struct iy_softc *);
void iyxmit(struct iy_softc *);
static void iy_mc_setup(struct iy_softc *);
static void iy_mc_reset(struct iy_softc *);
void iyget(struct iy_softc *, bus_space_tag_t, bus_space_handle_t, int);
void iyprobemem(struct iy_softc *);
static inline void eepromwritebit(bus_space_tag_t, bus_space_handle_t, int);
static inline int eepromreadbit(bus_space_tag_t, bus_space_handle_t);

#ifdef IYDEBUGX
void print_rbd(volatile struct iy_recv_buf_desc *);

int in_ifrint = 0;
int in_iftint = 0;
#endif

int iy_mediachange(struct ifnet *);
void iy_mediastatus(struct ifnet *, struct ifmediareq *);

int iyprobe(device_t, cfdata_t, void *);
void iyattach(device_t, device_t, void *);

static u_int16_t eepromread(bus_space_tag_t, bus_space_handle_t, int);

static int eepromreadall(bus_space_tag_t, bus_space_handle_t, u_int16_t *,
    int);

CFATTACH_DECL_NEW(iy, sizeof(struct iy_softc),
    iyprobe, iyattach, NULL, NULL);

static u_int8_t eepro_irqmap[] = EEPP_INTMAP;
static u_int8_t eepro_revirqmap[] = EEPP_RINTMAP;

int
iyprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	u_int16_t eaddr[8];
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t c, d;
	int irq;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	iot = ia->ia_iot;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, 16, 0, &ioh))
		return 0;

	/* try to find the round robin sig: */

	c = bus_space_read_1(iot, ioh, ID_REG);
	if ((c & ID_REG_MASK) != ID_REG_SIG)
		goto out;

	d = bus_space_read_1(iot, ioh, ID_REG);
	if ((d & ID_REG_MASK) != ID_REG_SIG)
		goto out;

	if (((d-c) & R_ROBIN_BITS) != 0x40)
		goto out;

	d = bus_space_read_1(iot, ioh, ID_REG);
	if ((d & ID_REG_MASK) != ID_REG_SIG)
		goto out;

	if (((d-c) & R_ROBIN_BITS) != 0x80)
		goto out;

	d = bus_space_read_1(iot, ioh, ID_REG);
	if ((d & ID_REG_MASK) != ID_REG_SIG)
		goto out;

	if (((d-c) & R_ROBIN_BITS) != 0xC0)
		goto out;

	d = bus_space_read_1(iot, ioh, ID_REG);
	if ((d & ID_REG_MASK) != ID_REG_SIG)
		goto out;

	if (((d-c) & R_ROBIN_BITS) != 0x00)
		goto out;

#ifdef IYDEBUG
		printf("iyprobe verified working ID reg.\n");
#endif

	if (eepromreadall(iot, ioh, eaddr, 8))
		goto out;

	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		irq = eepro_irqmap[eaddr[EEPPW1] & EEPP_Int];
	else
		irq = ia->ia_irq[0].ir_irq;

	if (irq >= sizeof(eepro_revirqmap))
		goto out;

	if (eepro_revirqmap[irq] == 0xff)
		goto out;

	/* now lets reset the chip */

	bus_space_write_1(iot, ioh, COMMAND_REG, RESET_CMD);
	delay(200);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = 16;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	ia->ia_niomem = 0;
	ia->ia_ndrq = 0;

	bus_space_unmap(iot, ioh, 16);
	return 1;		/* found */
out:
	bus_space_unmap(iot, ioh, 16);
	return 0;
}

void
iyattach(device_t parent, device_t self, void *aux)
{
	struct iy_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	unsigned temp;
	u_int16_t eaddr[8];
	u_int8_t myaddr[ETHER_ADDR_LEN];
	int eirq;

	iot = ia->ia_iot;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, 16, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	sc->mappedirq = eepro_revirqmap[ia->ia_irq[0].ir_irq];

	/* now let's reset the chip */

	bus_space_write_1(iot, ioh, COMMAND_REG, RESET_CMD);
	delay(200);

	iyprobemem(sc);

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = iystart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS
	    | IFF_MULTICAST;

	sc->doing_mc_setup = 0;

	ifp->if_ioctl = iyioctl;
	ifp->if_watchdog = iywatchdog;

	IFQ_SET_READY(&ifp->if_snd);

	(void)eepromreadall(iot, ioh, eaddr, 8);
	sc->hard_vers = eaddr[EEPW6] & EEPP_BoardRev;

#ifdef DIAGNOSTICS
	if ((eaddr[EEPPEther0] !=
	     eepromread(iot, ioh, EEPPEther0a)) &&
	    (eaddr[EEPPEther1] !=
	     eepromread(iot, ioh, EEPPEther1a)) &&
	    (eaddr[EEPPEther2] !=
	     eepromread(iot, ioh, EEPPEther2a)))

		printf("EEPROM Ethernet address differs from copy\n");
#endif

        myaddr[1] = eaddr[EEPPEther0] & 0xFF;
        myaddr[0] = eaddr[EEPPEther0] >> 8;
        myaddr[3] = eaddr[EEPPEther1] & 0xFF;
        myaddr[2] = eaddr[EEPPEther1] >> 8;
        myaddr[5] = eaddr[EEPPEther2] & 0xFF;
        myaddr[4] = eaddr[EEPPEther2] >> 8;

	ifmedia_init(&sc->iy_ifmedia, 0, iy_mediachange, iy_mediastatus);
	ifmedia_add(&sc->iy_ifmedia, IFM_ETHER | IFM_10_2, 0, NULL);
	ifmedia_add(&sc->iy_ifmedia, IFM_ETHER | IFM_10_5, 0, NULL);
	ifmedia_add(&sc->iy_ifmedia, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(&sc->iy_ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->iy_ifmedia, IFM_ETHER | IFM_AUTO);
	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, myaddr);
	printf(": address %s, rev. %d, %d kB\n",
	    ether_sprintf(myaddr),
	    sc->hard_vers, sc->sram/1024);

	eirq = eepro_irqmap[eaddr[EEPPW1] & EEPP_Int];
	if (eirq != ia->ia_irq[0].ir_irq)
		printf("%s: EEPROM irq setting %d ignored\n",
		    device_xname(sc->sc_dev), eirq);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, iyintr, sc);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);

	temp = bus_space_read_1(iot, ioh, INT_NO_REG);
	bus_space_write_1(iot, ioh, INT_NO_REG, (temp & 0xf8) | sc->mappedirq);
}

void
iystop(struct iy_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
#ifdef IYDEBUG
	u_int p, v;
#endif

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, COMMAND_REG, RCV_DISABLE_CMD);

	bus_space_write_1(iot, ioh, INT_MASK_REG, ALL_INTS);
	bus_space_write_1(iot, ioh, STATUS_REG, ALL_INTS);

	bus_space_write_1(iot, ioh, COMMAND_REG, RESET_CMD);
	delay(200);
#ifdef IYDEBUG
	printf("%s: dumping tx chain (st 0x%x end 0x%x last 0x%x)\n",
		    device_xname(sc->sc_dev), sc->tx_start, sc->tx_end, sc->tx_last);
	p = sc->tx_last;
	if (!p)
		p = sc->tx_start;
	do {
		char sbuf[128];

		bus_space_write_2(iot, ioh, HOST_ADDR_REG, p);

		v = le16toh(bus_space_read_stream_2(iot, ioh, MEM_PORT_REG));
		snprintb(sbuf, sizeof(sbuf), "\020\006Ab\010Dn", v);
		printf("0x%04x: %s ", p, sbuf);

		v = le16toh(bus_space_read_stream_2(iot, ioh, MEM_PORT_REG));
		snprintb(sbuf, sizeof(sbuf), 
		    "\020\6MAX_COL\7HRT_BEAT\010TX_DEF\011UND_RUN"
		    "\012JERR\013LST_CRS\014LTCOL\016TX_OK\020COLL", v);
		printf("0x%s", sbuf);

		p = le16toh(bus_space_read_stream_2(iot, ioh, MEM_PORT_REG));
		printf(" 0x%04x", p);

		v = le16toh(bus_space_read_stream_2(iot, ioh, MEM_PORT_REG));
		snprintb(sbuf, sizeof(sbuf), "\020\020Ch", v);
		printf(" 0x%s\n", sbuf);

	} while (v & 0x8000);
#endif
	sc->tx_start = sc->tx_end = sc->rx_size;
	sc->tx_last = 0;
	sc->sc_ethercom.ec_if.if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
}

void
iyreset(struct iy_softc *sc)
{
	int s;
	s = splnet();
	iystop(sc);
	iyinit(sc);
	splx(s);
}

void
iyinit(struct iy_softc *sc)
{
	int i;
	unsigned temp;
	struct ifnet *ifp;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	ifp = &sc->sc_ethercom.ec_if;
#ifdef IYDEBUG
	printf("ifp is %p\n", ifp);
#endif

	bus_space_write_1(iot, ioh, 0, BANK_SEL(2));

	temp = bus_space_read_1(iot, ioh, EEPROM_REG);
	if (temp & 0x10)
		bus_space_write_1(iot, ioh, EEPROM_REG, temp & ~0x10);

	for (i=0; i<6; ++i) {
		bus_space_write_1(iot, ioh, I_ADD(i), CLLADDR(ifp->if_sadl)[i]);
	}

	temp = bus_space_read_1(iot, ioh, REG1);
	bus_space_write_1(iot, ioh, REG1,
	    temp | /* XMT_CHAIN_INT | XMT_CHAIN_ERRSTOP | */ RCV_DISCARD_BAD);

	if (ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		temp = MATCH_ALL;
	} else
		temp = MATCH_BRDCST;

	bus_space_write_1(iot, ioh, RECV_MODES_REG, temp);

#ifdef IYDEBUG
	{
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf), 
		    "\020\1PRMSC\2NOBRDST\3SEECRC\4LENGTH\5NOSaIns\6MultiIA",
		    temp);
			
		printf("%s: RECV_MODES set to %s\n", device_xname(sc->sc_dev), sbuf);
	}
#endif
	/* XXX VOODOO */
	temp = bus_space_read_1(iot, ioh, MEDIA_SELECT);
	bus_space_write_1(iot, ioh, MEDIA_SELECT, temp);
	/* XXX END OF VOODOO */


	delay(500000); /* for the hardware to test for the connector */

	temp = bus_space_read_1(iot, ioh, MEDIA_SELECT);
#ifdef IYDEBUG
	{
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf),
		    "\020\1LnkInDis\2PolCor\3TPE\4JabberDis\5NoAport\6BNC",
		    temp);
		printf("%s: media select was 0x%s ", device_xname(sc->sc_dev), sbuf);
	}
#endif
	temp = (temp & TEST_MODE_MASK);

	switch(IFM_SUBTYPE(sc->iy_ifmedia.ifm_media)) {
	case IFM_10_5:
		temp &= ~ (BNC_BIT | TPE_BIT);
		break;

	case IFM_10_2:
		temp = (temp & ~TPE_BIT) | BNC_BIT;
		break;

	case IFM_10_T:
		temp = (temp & ~BNC_BIT) | TPE_BIT;
		break;
	default:
		;
		/* nothing; leave as it is */
	}
	switch (temp & (BNC_BIT | TPE_BIT)) {
	case BNC_BIT:
		sc->iy_media = IFM_ETHER | IFM_10_2;
		break;
	case TPE_BIT:
		sc->iy_media = IFM_ETHER | IFM_10_T;
		break;
	default:
		sc->iy_media = IFM_ETHER | IFM_10_5;
	}

	bus_space_write_1(iot, ioh, MEDIA_SELECT, temp);
#ifdef IYDEBUG
	{
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf),
		    "\020\1LnkInDis\2PolCor\3TPE\4JabberDis\5NoAport\6BNC",
		    temp);
		printf("changed to 0x%s\n", sbuf);
	}
#endif

	bus_space_write_1(iot, ioh, 0, BANK_SEL(0));
	bus_space_write_1(iot, ioh, INT_MASK_REG, ALL_INTS);
	bus_space_write_1(iot, ioh, 0, BANK_SEL(1));

	temp = bus_space_read_1(iot, ioh, INT_NO_REG);
	bus_space_write_1(iot, ioh, INT_NO_REG, (temp & 0xf8) | sc->mappedirq);

#ifdef IYDEBUG
	{
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf),
		    "\020\4bad_irq\010flash/boot present", temp);
				
		printf("%s: int no was %s\n", device_xname(sc->sc_dev), sbuf);

		temp = bus_space_read_1(iot, ioh, INT_NO_REG);
		snprintb(sbuf, sizeof(sbuf),
		    "\020\4bad_irq\010flash/boot present", temp);
		printf("%s: int no now %s\n", device_xname(sc->sc_dev), sbuf);
	}
#endif

	bus_space_write_1(iot, ioh, RCV_LOWER_LIMIT_REG, 0);
	bus_space_write_1(iot, ioh, RCV_UPPER_LIMIT_REG, (sc->rx_size -2) >>8);
	bus_space_write_1(iot, ioh, XMT_LOWER_LIMIT_REG, sc->rx_size >>8);
	bus_space_write_1(iot, ioh, XMT_UPPER_LIMIT_REG, (sc->sram - 2) >>8);

	temp = bus_space_read_1(iot, ioh, REG1);
#ifdef IYDEBUG
	{
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf), "\020\2WORD_WIDTH\010INT_ENABLE",
		    temp);
			
		printf("%s: HW access is %s\n", device_xname(sc->sc_dev), sbuf);
	}
#endif
	bus_space_write_1(iot, ioh, REG1, temp | INT_ENABLE); /* XXX what about WORD_WIDTH? */

#ifdef IYDEBUG
	{
		char sbuf[128];

		temp = bus_space_read_1(iot, ioh, REG1);
		snprintb(sbuf, sizeof(sbuf), "\020\2WORD_WIDTH\010INT_ENABLE",
		    temp);
		printf("%s: HW access is %s\n", device_xname(sc->sc_dev), sbuf);
	}
#endif

	bus_space_write_1(iot, ioh, 0, BANK_SEL(0));

	bus_space_write_1(iot, ioh, INT_MASK_REG, ALL_INTS & ~(RX_BIT|TX_BIT));
	bus_space_write_1(iot, ioh, STATUS_REG, ALL_INTS); /* clear ints */

	bus_space_write_1(iot, ioh, RCV_COPY_THRESHOLD, 0);

	bus_space_write_2(iot, ioh, RCV_START_LOW, 0);
	bus_space_write_2(iot, ioh, RCV_STOP_LOW,  sc->rx_size - 2);
	sc->rx_start = 0;

	bus_space_write_1(iot, ioh, 0, SEL_RESET_CMD);
	delay(200);

	bus_space_write_2(iot, ioh, XMT_ADDR_REG, sc->rx_size);

	sc->tx_start = sc->tx_end = sc->rx_size;
	sc->tx_last = 0;

	bus_space_write_1(iot, ioh, 0, RCV_ENABLE_CMD);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

void
iystart(struct ifnet *ifp)
{
	struct iy_softc *sc;


	struct mbuf *m0, *m;
	u_int len, pad, last, end;
	u_int llen, residual;
	int avail;
	char *data;
	unsigned temp;
	u_int16_t resval, stat;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

#ifdef IYDEBUG
	printf("iystart called\n");
#endif
	sc = ifp->if_softc;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
                return;

	iy_intr_tx(sc);

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
#ifdef IYDEBUG
		printf("%s: trying to write another packet to the hardware\n",
		    device_xname(sc->sc_dev));
#endif

		/* We need to use m->m_pkthdr.len, so require the header */
		if ((m0->m_flags & M_PKTHDR) == 0)
			panic("iystart: no header mbuf");

		len = m0->m_pkthdr.len;
		pad = len & 1;

#ifdef IYDEBUG
		printf("%s: length is %d.\n", device_xname(sc->sc_dev), len);
#endif
		if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
			pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;
		}

        	if (len + pad > ETHER_MAX_LEN) {
        	        /* packet is obviously too large: toss it */
        	        ++ifp->if_oerrors;
        	        IFQ_DEQUEUE(&ifp->if_snd, m0);
        	        m_freem(m0);
			continue;
        	}

		bpf_mtap(ifp, m0);

		avail = sc->tx_start - sc->tx_end;
		if (avail <= 0)
			avail += sc->tx_size;

#ifdef IYDEBUG
		printf("%s: avail is %d.\n", device_xname(sc->sc_dev), avail);
#endif
		/*
		 * we MUST RUN at splnet here  ---
		 * XXX todo: or even turn off the boards ints ??? hm...
		 */

       		/* See if there is room to put another packet in the buffer. */

		if ((len+pad+2*I595_XMT_HDRLEN) > avail) {
#ifdef IYDEBUG
			printf("%s: len = %d, avail = %d, setting OACTIVE\n",
			    device_xname(sc->sc_dev), len, avail);
#endif
			/* mark interface as full ... */
			ifp->if_flags |= IFF_OACTIVE;

			/* and wait for any transmission result */
			bus_space_write_1(iot, ioh, 0, BANK_SEL(2));

			temp = bus_space_read_1(iot, ioh, REG1);
			bus_space_write_1(iot, ioh, REG1,
	    			temp & ~XMT_CHAIN_INT);

			bus_space_write_1(iot, ioh, 0, BANK_SEL(0));

			return;
		}

		/* we know it fits in the hardware now, so dequeue it */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		last = sc->tx_end;
		end = last + pad + len + I595_XMT_HDRLEN;

		if (end >= sc->sram) {
			if ((sc->sram - last) <= I595_XMT_HDRLEN) {
				/* keep header in one piece */
				last = sc->rx_size;
				end = last + pad + len + I595_XMT_HDRLEN;
			} else
				end -= sc->tx_size;
		}

		bus_space_write_2(iot, ioh, HOST_ADDR_REG, last);
		bus_space_write_stream_2(iot, ioh, MEM_PORT_REG,
			htole16(XMT_CMD));

		bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);
		bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);

		bus_space_write_stream_2(iot, ioh, MEM_PORT_REG,
			htole16(len + pad));

		residual = resval = 0;

		while ((m = m0)!=0) {
			data = mtod(m, void *);
			llen = m->m_len;
			if (residual) {
#ifdef IYDEBUG
				printf("%s: merging residual with next mbuf.\n",
				    device_xname(sc->sc_dev));
#endif
				resval |= *data << 8;
				bus_space_write_stream_2(iot, ioh,
					MEM_PORT_REG, resval);
				--llen;
				++data;
			}
			/*
			 * XXX ALIGNMENT LOSSAGE HERE.
			 */
			if (llen > 1)
				bus_space_write_multi_stream_2(iot, ioh,
					MEM_PORT_REG, (u_int16_t *) data,
					llen>>1);
			residual = llen & 1;
			if (residual) {
				resval = *(data + llen - 1);
#ifdef IYDEBUG
				printf("%s: got odd mbuf to send.\n",
				    device_xname(sc->sc_dev));
#endif
			}

			MFREE(m, m0);
		}

		if (residual)
			bus_space_write_stream_2(iot, ioh, MEM_PORT_REG,
				resval);

		pad >>= 1;
		while (pad-- > 0)
			bus_space_write_stream_2(iot, ioh, MEM_PORT_REG, 0);

#ifdef IYDEBUG
		printf("%s: new last = 0x%x, end = 0x%x.\n",
		    device_xname(sc->sc_dev), last, end);
		printf("%s: old start = 0x%x, end = 0x%x, last = 0x%x\n",
		    device_xname(sc->sc_dev), sc->tx_start, sc->tx_end, sc->tx_last);
#endif

		if (sc->tx_start != sc->tx_end) {
			bus_space_write_2(iot, ioh, HOST_ADDR_REG,
				sc->tx_last + XMT_COUNT);

			/*
			 * XXX We keep stat in le order, to potentially save
			 * a byte swap.
			 */
			stat = bus_space_read_stream_2(iot, ioh, MEM_PORT_REG);

			bus_space_write_2(iot, ioh, HOST_ADDR_REG,
				sc->tx_last + XMT_CHAIN);

			bus_space_write_stream_2(iot, ioh, MEM_PORT_REG,
				htole16(last));

			bus_space_write_stream_2(iot, ioh, MEM_PORT_REG,
				stat | htole16(CHAIN));
#ifdef IYDEBUG
			printf("%s: setting 0x%x to 0x%x\n",
			    device_xname(sc->sc_dev), sc->tx_last + XMT_COUNT,
			    le16toh(stat) | CHAIN);
#endif
		}
		stat = bus_space_read_2(iot, ioh, MEM_PORT_REG); /* dummy read */

		/* XXX todo: enable ints here if disabled */

		++ifp->if_opackets;

		if (sc->tx_start == sc->tx_end) {
			bus_space_write_2(iot, ioh, XMT_ADDR_REG, last);
			bus_space_write_1(iot, ioh, 0, XMT_CMD);
			sc->tx_start = last;
#ifdef IYDEBUG
			printf("%s: writing 0x%x to XAR and giving XCMD\n",
			    device_xname(sc->sc_dev), last);
#endif
		} else {
			bus_space_write_1(iot, ioh, 0, RESUME_XMT_CMD);
#ifdef IYDEBUG
			printf("%s: giving RESUME_XCMD\n",
			    device_xname(sc->sc_dev));
#endif
		}
		sc->tx_last = last;
		sc->tx_end = end;
	}
	/* and wait only for end of transmission chain */
	bus_space_write_1(iot, ioh, 0, BANK_SEL(2));

	temp = bus_space_read_1(iot, ioh, REG1);
	bus_space_write_1(iot, ioh, REG1, temp | XMT_CHAIN_INT);

	bus_space_write_1(iot, ioh, 0, BANK_SEL(0));
}


static inline void
eepromwritebit(bus_space_tag_t iot, bus_space_handle_t ioh, int what)
{
	bus_space_write_1(iot, ioh, EEPROM_REG, what);
	delay(1);
	bus_space_write_1(iot, ioh, EEPROM_REG, what|EESK);
	delay(1);
	bus_space_write_1(iot, ioh, EEPROM_REG, what);
	delay(1);
}

static inline int
eepromreadbit(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int b;

	bus_space_write_1(iot, ioh, EEPROM_REG, EECS|EESK);
	delay(1);
	b = bus_space_read_1(iot, ioh, EEPROM_REG);
	bus_space_write_1(iot, ioh, EEPROM_REG, EECS);
	delay(1);

	return ((b & EEDO) != 0);
}

static u_int16_t
eepromread(bus_space_tag_t iot, bus_space_handle_t ioh, int offset)
{
	volatile int i;
	volatile int j;
	volatile u_int16_t readval;

	bus_space_write_1(iot, ioh, 0, BANK_SEL(2));
	delay(1);
	bus_space_write_1(iot, ioh, EEPROM_REG, EECS); /* XXXX??? */
	delay(1);

	eepromwritebit(iot, ioh, EECS|EEDI);
	eepromwritebit(iot, ioh, EECS|EEDI);
	eepromwritebit(iot, ioh, EECS);

	for (j=5; j>=0; --j) {
		if ((offset>>j) & 1)
			eepromwritebit(iot, ioh, EECS|EEDI);
		else
			eepromwritebit(iot, ioh, EECS);
	}

	for (readval=0, i=0; i<16; ++i) {
		readval<<=1;
		readval |= eepromreadbit(iot, ioh);
	}

	bus_space_write_1(iot, ioh, EEPROM_REG, 0|EESK);
	delay(1);
	bus_space_write_1(iot, ioh, EEPROM_REG, 0);

	bus_space_write_1(iot, ioh, COMMAND_REG, BANK_SEL(0));

	return readval;
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
void
iywatchdog(struct ifnet *ifp)
{
	struct iy_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++sc->sc_ethercom.ec_if.if_oerrors;
	iyreset(sc);
}

/*
 * What to do upon receipt of an interrupt.
 */
int
iyintr(void *arg)
{
	struct iy_softc *sc;
	struct ifnet *ifp;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	u_short status;

	sc = arg;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	ifp = &sc->sc_ethercom.ec_if;

	status = bus_space_read_1(iot, ioh, STATUS_REG);
#ifdef IYDEBUG
	if (status & ALL_INTS) {
		char sbuf[128];

		snprintb(sbuf, sizeof(sbuf), "\020\1RX_STP\2RX\3TX\4EXEC",
		    status);
		printf("%s: got interrupt %s", device_xname(sc->sc_dev), sbuf);

		if (status & EXEC_INT) {
			snprintb(sbuf, sizeof(sbuf), 
			     "\020\6ABORT", bus_space_read_1(iot, ioh, 0));
			printf(" event %s\n", sbuf);
		} else
			printf("\n");
	}
#endif
	if ((status & (RX_INT | TX_INT)) == 0)
		return 0;

	if (status & RX_INT) {
		iy_intr_rx(sc);
		bus_space_write_1(iot, ioh, STATUS_REG, RX_INT);
	}
	if (status & TX_INT) {
		/* Tell feeders we may be able to accept more data... */
		ifp->if_flags &= ~IFF_OACTIVE;
		/* and get more data. */
		iystart(ifp);
		bus_space_write_1(iot, ioh, STATUS_REG, TX_INT);
	}

	rnd_add_uint32(&sc->rnd_source, status);

	return 1;
}

void
iyget(struct iy_softc *sc, bus_space_tag_t iot, bus_space_handle_t ioh, int rxlen)
{
	struct mbuf *m, *top, **mp;
	struct ifnet *ifp;
	int len;

	ifp = &sc->sc_ethercom.ec_if;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		goto dropped;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = rxlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (rxlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				goto dropped;
			}
			len = MLEN;
		}
		if (rxlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				goto dropped;
			}
			len = MCLBYTES;
		}
		len = min(rxlen, len);
		/*
		 * XXX ALIGNMENT LOSSAGE HERE.
		 */
		if (len > 1) {
			len &= ~1;

			bus_space_read_multi_stream_2(iot, ioh, MEM_PORT_REG,
			    mtod(m, u_int16_t *), len/2);
		} else {
#ifdef IYDEBUG
			printf("%s: received odd mbuf\n", device_xname(sc->sc_dev));
#endif
			*(mtod(m, char *)) = bus_space_read_stream_2(iot, ioh,
			    MEM_PORT_REG);
		}
		m->m_len = len;
		rxlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	if (top == NULL)
		return;

	/* XXX receive the top here */
	++ifp->if_ipackets;


	bpf_mtap(ifp, top);
	(*ifp->if_input)(ifp, top);
	return;

dropped:
	++ifp->if_ierrors;
	return;
}

void
iy_intr_rx(struct iy_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	u_int rxadrs, rxevnt, rxstatus, rxnext, rxlen;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	rxadrs = sc->rx_start;
	bus_space_write_2(iot, ioh, HOST_ADDR_REG, rxadrs);
	rxevnt = le16toh(bus_space_read_stream_2(iot, ioh, MEM_PORT_REG));
	rxnext = 0;

	while (rxevnt == RCV_DONE) {
		rxstatus = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
		rxnext = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
		rxlen = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
#ifdef IYDEBUG
		{
			char sbuf[128];

			snprintb(sbuf, sizeof(sbuf),
			    "\020\1RCLD\2IA_MCH\010SHORT\011OVRN\013ALGERR"
			    "\014CRCERR\015LENERR\016RCVOK\020TYP", rxstatus);

			printf("%s: pck at 0x%04x stat %s next 0x%x len 0x%x\n",
			    device_xname(sc->sc_dev), rxadrs, sbuf, rxnext, rxlen);
		}
#else
		__USE(rxstatus);
#endif
		iyget(sc, iot, ioh, rxlen);

		/* move stop address */
		bus_space_write_2(iot, ioh, RCV_STOP_LOW,
			    rxnext == 0 ? sc->rx_size - 2 : rxnext - 2);

		bus_space_write_2(iot, ioh, HOST_ADDR_REG, rxnext);
		rxadrs = rxnext;
		rxevnt = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
	}
	sc->rx_start = rxnext;
}

void
iy_intr_tx(struct iy_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ifnet *ifp;
	u_int txstatus, txstat2, txlen, txnext;

	ifp = &sc->sc_ethercom.ec_if;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	while (sc->tx_start != sc->tx_end) {
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, sc->tx_start);
		txstatus = le16toh(bus_space_read_stream_2(iot, ioh,
			MEM_PORT_REG));

		if ((txstatus & (TX_DONE|CMD_MASK)) != (TX_DONE|XMT_CMD))
			break;

		txstat2 = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
		txnext = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
		txlen = le16toh(bus_space_read_stream_2(iot, ioh,
				MEM_PORT_REG));
#ifdef IYDEBUG
		{
			char sbuf[128];

			snprintb(sbuf, sizeof(sbuf),
			    "\020\6MAX_COL\7HRT_BEAT\010TX_DEF"
			    "\011UND_RUN\012JERR\013LST_CRS"
			    "\014LTCOL\016TX_OK\020COLL", txstat2);

			printf("txstat 0x%x stat2 0x%s next 0x%x len 0x%x\n",
			       txstatus, sbuf, txnext, txlen);
		}
#endif
		if (txlen & CHAIN)
			sc->tx_start = txnext;
		else
			sc->tx_start = sc->tx_end;
		ifp->if_flags &= ~IFF_OACTIVE;

		if (txstat2 & 0x0020)
			ifp->if_collisions += 16;
		else
			ifp->if_collisions += txstat2 & 0x000f;

		if ((txstat2 & 0x2000) == 0)
			++ifp->if_oerrors;
	}
}

int
iyioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct iy_softc *sc;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	sc = ifp->if_softc;
	ifa = (struct ifaddr *)data;
	ifr = (struct ifreq *)data;

#ifdef IYDEBUG
	printf("iyioctl called with ifp %p (%s) cmd 0x%lx data %p\n",
	    ifp, ifp->if_xname, cmd, data);
#endif

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;

		iyinit(sc);
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
		sc->promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			iystop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			iyinit(sc);
			break;
		default:
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			iystop(sc);
			iyinit(sc);
			break;
		}
#ifdef IYDEBUGX
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IFY_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING) {
				/* XXX can't make it work otherwise */
				iyreset(sc);
				iy_mc_reset(sc);
			}
			error = 0;
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->iy_ifmedia, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
	}
	splx(s);
	return error;
}

int
iy_mediachange(struct ifnet *ifp)
{
	struct iy_softc *sc = ifp->if_softc;

	if (IFM_TYPE(sc->iy_ifmedia.ifm_media) != IFM_ETHER)
	    return EINVAL;
	switch(IFM_SUBTYPE(sc->iy_ifmedia.ifm_media)) {
	case IFM_10_5:
	case IFM_10_2:
	case IFM_10_T:
	case IFM_AUTO:
	    iystop(sc);
	    iyinit(sc);
	    return 0;
	default:
	    return EINVAL;
	}
}

void
iy_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct iy_softc *sc = ifp->if_softc;

	ifmr->ifm_active = sc->iy_media;
	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}


static void
iy_mc_setup(struct iy_softc *sc)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ethercom *ecp;
	struct ifnet *ifp;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int avail, last /*, end*/ , len;
	int timeout;
	volatile u_int16_t dum;
	u_int8_t temp;


	ecp = &sc->sc_ethercom;
	ifp = &ecp->ec_if;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	len = 6 * ecp->ec_multicnt;

	avail = sc->tx_start - sc->tx_end;
	if (avail <= 0)
		avail += sc->tx_size;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: iy_mc_setup called, %d addresses, "
		    "%d/%d bytes needed/avail\n", ifp->if_xname,
		    ecp->ec_multicnt, len + I595_XMT_HDRLEN, avail);

	last = sc->rx_size;

	bus_space_write_1(iot, ioh, 0, BANK_SEL(2));
	bus_space_write_1(iot, ioh, RECV_MODES_REG, MATCH_BRDCST);
	/* XXX VOODOO */
	temp = bus_space_read_1(iot, ioh, MEDIA_SELECT);
	bus_space_write_1(iot, ioh, MEDIA_SELECT, temp);
	/* XXX END OF VOODOO */
	bus_space_write_1(iot, ioh, 0, BANK_SEL(0));
	bus_space_write_2(iot, ioh, HOST_ADDR_REG, last);
	bus_space_write_stream_2(iot, ioh, MEM_PORT_REG, htole16(MC_SETUP_CMD));
	bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);
	bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);
	bus_space_write_stream_2(iot, ioh, MEM_PORT_REG, htole16(len));

	ETHER_FIRST_MULTI(step, ecp, enm);
	while(enm) {
		/*
		 * XXX ALIGNMENT LOSSAGE HERE?
		 */
		bus_space_write_multi_stream_2(iot, ioh, MEM_PORT_REG,
		    (u_int16_t *) enm->enm_addrlo, 3);

		ETHER_NEXT_MULTI(step, enm);
	}
	dum = bus_space_read_2(iot, ioh, MEM_PORT_REG); /* dummy read */
	__USE(dum);
	bus_space_write_2(iot, ioh, XMT_ADDR_REG, last);
	bus_space_write_1(iot, ioh, 0, MC_SETUP_CMD);


	sc->tx_start =  sc->rx_size;
	sc->tx_end = sc->rx_size + I595_XMT_HDRLEN + len;

	for (timeout=0; timeout<100; timeout++) {
		DELAY(2);
		if ((bus_space_read_1(iot, ioh, STATUS_REG) & EXEC_INT) == 0)
			continue;

		temp = bus_space_read_1(iot, ioh, 0);
		bus_space_write_1(iot, ioh, STATUS_REG, EXEC_INT);
#ifdef DIAGNOSTIC
		if (temp & 0x20) {
			aprint_error_dev(sc->sc_dev, "mc setup failed, %d usec\n",
			    timeout * 2);
		} else if (((temp & 0x0f) == 0x03) &&
			    (ifp->if_flags & IFF_DEBUG)) {
				printf("%s: mc setup done, %d usec\n",
			    device_xname(sc->sc_dev), timeout * 2);
		}
#endif
		break;
	}
	sc->tx_start = sc->tx_end;
	ifp->if_flags &= ~IFF_OACTIVE;

}

static void
iy_mc_reset(struct iy_softc *sc)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ethercom *ecp;
	struct ifnet *ifp;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int16_t temp;

	ecp = &sc->sc_ethercom;
	ifp = &ecp->ec_if;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	if (ecp->ec_multicnt > 63) {
		ifp->if_flags |= IFF_ALLMULTI;

	} else if (ecp->ec_multicnt > 0) {
		/*
		 * Step through the list of addresses.
		 */
		ETHER_FIRST_MULTI(step, ecp, enm);
		while(enm) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto setupmulti;
			}
			ETHER_NEXT_MULTI(step, enm);
		}
		/* OK, we really need to do it now: */
#if 0
		if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE))
		    != IFF_RUNNING) {
			ifp->if_flags |= IFF_OACTIVE;
			sc->want_mc_setup = 1;
                	return;
		}
#endif
		iy_mc_setup(sc);
	} else {
		ifp->if_flags &= ~IFF_ALLMULTI;
	}

setupmulti:
	bus_space_write_1(iot, ioh, 0, BANK_SEL(2));
	if (ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		temp = MATCH_ALL;
	} else
		temp = MATCH_BRDCST;

	bus_space_write_1(iot, ioh, RECV_MODES_REG, temp);
	/* XXX VOODOO */
	temp = bus_space_read_1(iot, ioh, MEDIA_SELECT);
	bus_space_write_1(iot, ioh, MEDIA_SELECT, temp);
	/* XXX END OF VOODOO */

	/* XXX TBD: setup hardware for all multicasts */
	bus_space_write_1(iot, ioh, 0, BANK_SEL(0));
	return;
}

#ifdef IYDEBUGX
void
print_rbd(volatile struct ie_recv_buf_desc *rbd)
{
	printf("RBD at %08lx:\nactual %04x, next %04x, buffer %08x\n"
	    "length %04x, mbz %04x\n", (u_long)rbd, rbd->ie_rbd_actual,
	    rbd->ie_rbd_next, rbd->ie_rbd_buffer, rbd->ie_rbd_length,
	    rbd->mbz);
}
#endif

void
iyprobemem(struct iy_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int testing;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, COMMAND_REG, BANK_SEL(0));
	delay(1);
	bus_space_write_2(iot, ioh, HOST_ADDR_REG, 4096-2);
	bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);

	for (testing=65536; testing >= 4096; testing >>= 1) {
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, testing-2);
		bus_space_write_2(iot, ioh, MEM_PORT_REG, 0xdead);
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, testing-2);
		if (bus_space_read_2(iot, ioh, MEM_PORT_REG) != 0xdead) {
#ifdef IYMEMDEBUG
			printf("%s: Didn't keep 0xdead at 0x%x\n",
			    device_xname(sc->sc_dev), testing-2);
#endif
			continue;
		}

		bus_space_write_2(iot, ioh, HOST_ADDR_REG, testing-2);
		bus_space_write_2(iot, ioh, MEM_PORT_REG, 0xbeef);
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, testing-2);
		if (bus_space_read_2(iot, ioh, MEM_PORT_REG) != 0xbeef) {
#ifdef IYMEMDEBUG
			printf("%s: Didn't keep 0xbeef at 0x%x\n",
			    device_xname(sc->sc_dev), testing-2);
#endif
			continue;
		}

		bus_space_write_2(iot, ioh, HOST_ADDR_REG, 0);
		bus_space_write_2(iot, ioh, MEM_PORT_REG, 0);
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, testing >> 1);
		bus_space_write_2(iot, ioh, MEM_PORT_REG, testing >> 1);
		bus_space_write_2(iot, ioh, HOST_ADDR_REG, 0);
		if (bus_space_read_2(iot, ioh, MEM_PORT_REG) == (testing >> 1)) {
#ifdef IYMEMDEBUG
			printf("%s: 0x%x alias of 0x0\n",
			    device_xname(sc->sc_dev), testing >> 1);
#endif
			continue;
		}

		break;
	}

	sc->sram = testing;

	switch(testing) {
		case 65536:
			/* 4 NFS packets + overhead RX, 2 NFS + overhead TX  */
			sc->rx_size = 44*1024;
			break;

		case 32768:
			/* 2 NFS packets + overhead RX, 1 NFS + overhead TX  */
			sc->rx_size = 22*1024;
			break;

		case 16384:
			/* 1 NFS packet + overhead RX, 4 big packets TX */
			sc->rx_size = 10*1024;
			break;
		default:
			sc->rx_size = testing/2;
			break;
	}
	sc->tx_size = testing - sc->rx_size;
}

static int
eepromreadall(bus_space_tag_t iot, bus_space_handle_t ioh, u_int16_t *wordp, int maxi)
{
	int i;
	u_int16_t checksum, tmp;

	checksum = 0;

	for (i=0; i<EEPP_LENGTH; ++i) {
		tmp = eepromread(iot, ioh, i);
		checksum += tmp;
		if (i<maxi)
			wordp[i] = tmp;
	}

	if (checksum != EEPP_CHKSUM) {
#ifdef IYDEBUG
		printf("wrong EEPROM checksum 0x%x should be 0x%x\n",
		    checksum, EEPP_CHKSUM);
#endif
		return 1;
	}
	return 0;
}
