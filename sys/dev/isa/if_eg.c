/*	$NetBSD: if_eg.c,v 1.88 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 1993 Dean Huxley <dean@fsa.ca>
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
 *      This product includes software developed by Dean Huxley.
 * 4. The name of Dean Huxley may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Support for 3Com 3c505 Etherlink+ card.
 */

/*
 * To do:
 * - multicast
 * - promiscuous
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_eg.c,v 1.88 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
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

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/if_egreg.h>
#include <dev/isa/elink.h>

/* for debugging convenience */
#ifdef EGDEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define EG_INLEN  	10
#define EG_BUFLEN	0x0670

#define EG_PCBLEN 64

/*
 * Ethernet software status per interface.
 */
struct eg_softc {
	device_t sc_dev;
	void *sc_ih;
	struct ethercom sc_ethercom;	/* Ethernet common part */
	bus_space_tag_t sc_iot;		/* bus space identifier */
	bus_space_handle_t sc_ioh;	/* i/o handle */
	u_int8_t eg_rom_major;		/* Cards ROM version (major number) */
	u_int8_t eg_rom_minor;		/* Cards ROM version (minor number) */
	short	 eg_ram;		/* Amount of RAM on the card */
	u_int8_t eg_pcb[EG_PCBLEN];	/* Primary Command Block buffer */
	u_int8_t eg_incount;		/* Number of buffers currently used */
	void *	eg_inbuf;		/* Incoming packet buffer */
	void *	eg_outbuf;		/* Outgoing packet buffer */

	krndsource_t rnd_source;
};

int egprobe(device_t, cfdata_t, void *);
void egattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(eg, sizeof(struct eg_softc),
    egprobe, egattach, NULL, NULL);

int egintr(void *);
void eginit(struct eg_softc *);
int egioctl(struct ifnet *, u_long, void *);
void egrecv(struct eg_softc *);
void egstart(struct ifnet *);
void egwatchdog(struct ifnet *);
void egreset(struct eg_softc *);
void egread(struct eg_softc *, void *, int);
struct mbuf *egget(struct eg_softc *, void *, int);
void egstop(struct eg_softc *);

static inline void egprintpcb(u_int8_t *);
static int egoutPCB(bus_space_tag_t, bus_space_handle_t, u_int8_t);
static int egreadPCBstat(bus_space_tag_t, bus_space_handle_t, u_int8_t);
static int egreadPCBready(bus_space_tag_t, bus_space_handle_t);
static int egwritePCB(bus_space_tag_t, bus_space_handle_t, u_int8_t *);
static int egreadPCB(bus_space_tag_t, bus_space_handle_t, u_int8_t *);

/*
 * Support stuff
 */

static inline void
egprintpcb(u_int8_t *pcb)
{
	int i;

	for (i = 0; i < pcb[1] + 2; i++)
		DPRINTF(("pcb[%2d] = %x\n", i, pcb[i]));
}

static int
egoutPCB(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t b)
{
	int i;

	for (i=0; i < 4000; i++) {
		if (bus_space_read_1(iot, ioh, EG_STATUS) & EG_STAT_HCRE) {
			bus_space_write_1(iot, ioh, EG_COMMAND, b);
			return 0;
		}
		delay(10);
	}
	DPRINTF(("egoutPCB failed\n"));
	return 1;
}

static int
egreadPCBstat(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t statb)
{
	int i;

	for (i=0; i < 5000; i++) {
		if ((bus_space_read_1(iot, ioh, EG_STATUS) &
		    EG_PCB_STAT) != EG_PCB_NULL)
			break;
		delay(10);
	}
	if ((bus_space_read_1(iot, ioh, EG_STATUS) & EG_PCB_STAT) == statb)
		return 0;
	return 1;
}

static int
egreadPCBready(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i;

	for (i=0; i < 10000; i++) {
		if (bus_space_read_1(iot, ioh, EG_STATUS) & EG_STAT_ACRF)
			return 0;
		delay(5);
	}
	DPRINTF(("PCB read not ready\n"));
	return 1;
}

static int
egwritePCB(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t *pcb)
{
	int i;
	u_int8_t len;

	bus_space_write_1(iot, ioh, EG_CONTROL,
	    (bus_space_read_1(iot, ioh, EG_CONTROL) & ~EG_PCB_STAT) | EG_PCB_NULL);

	len = pcb[1] + 2;
	for (i = 0; i < len; i++)
		egoutPCB(iot, ioh, pcb[i]);

	for (i=0; i < 4000; i++) {
		if (bus_space_read_1(iot, ioh, EG_STATUS) & EG_STAT_HCRE)
			break;
		delay(10);
	}

	bus_space_write_1(iot, ioh, EG_CONTROL,
	    (bus_space_read_1(iot, ioh, EG_CONTROL) & ~EG_PCB_STAT) | EG_PCB_DONE);

	egoutPCB(iot, ioh, len);

	if (egreadPCBstat(iot, ioh, EG_PCB_ACCEPT))
		return 1;
	return 0;
}

static int
egreadPCB(bus_space_tag_t iot, bus_space_handle_t ioh, u_int8_t *pcb)
{
	int i;

	bus_space_write_1(iot, ioh, EG_CONTROL,
	    (bus_space_read_1(iot, ioh, EG_CONTROL) & ~EG_PCB_STAT) | EG_PCB_NULL);

	memset(pcb, 0, EG_PCBLEN);

	if (egreadPCBready(iot, ioh))
		return 1;

	pcb[0] = bus_space_read_1(iot, ioh, EG_COMMAND);

	if (egreadPCBready(iot, ioh))
		return 1;

	pcb[1] = bus_space_read_1(iot, ioh, EG_COMMAND);

	if (pcb[1] > 62) {
		DPRINTF(("len %d too large\n", pcb[1]));
		return 1;
	}

	for (i = 0; i < pcb[1]; i++) {
		if (egreadPCBready(iot, ioh))
			return 1;
		pcb[2+i] = bus_space_read_1(iot, ioh, EG_COMMAND);
	}
	if (egreadPCBready(iot, ioh))
		return 1;
	if (egreadPCBstat(iot, ioh, EG_PCB_DONE))
		return 1;
	if (bus_space_read_1(iot, ioh, EG_COMMAND) != pcb[1] + 2) {
		return 1;
	}

	bus_space_write_1(iot, ioh, EG_CONTROL,
	    (bus_space_read_1(iot, ioh, EG_CONTROL) &
	    ~EG_PCB_STAT) | EG_PCB_ACCEPT);

	return 0;
}

/*
 * Real stuff
 */

int
egprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, rval;
	static u_int8_t pcb[EG_PCBLEN];

	rval = 0;

	/*
	 * XXX This probe is slow.  If there are no ISA expansion slots,
	 * then skip it.
	 */
	if (isa_get_slotcount() == 0)
		return (0);

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/* Disallow wildcarded IRQ. */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	if ((ia->ia_io[0].ir_addr & ~0x07f0) != 0) {
		DPRINTF(("Weird iobase %x\n", ia->ia_io[0].ir_addr));
		return 0;
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, 0x08, 0, &ioh)) {
		DPRINTF(("egprobe: can't map i/o space in probe\n"));
		return 0;
	}

	/* hard reset card */
	bus_space_write_1(iot, ioh, EG_CONTROL, EG_CTL_RESET);
	bus_space_write_1(iot, ioh, EG_CONTROL, 0);
	for (i = 0; i < 500; i++) {
		delay(1000);
		if ((bus_space_read_1(iot, ioh, EG_STATUS) &
		    EG_PCB_STAT) == EG_PCB_NULL)
			break;
	}
	if ((bus_space_read_1(iot, ioh, EG_STATUS) & EG_PCB_STAT) != EG_PCB_NULL) {
		DPRINTF(("egprobe: Reset failed\n"));
		goto out;
	}
	pcb[0] = EG_CMD_GETINFO; /* Get Adapter Info */
	pcb[1] = 0;
	if (egwritePCB(iot, ioh, pcb) != 0)
		goto out;

	if ((egreadPCB(iot, ioh, pcb) != 0) ||
	    pcb[0] != EG_RSP_GETINFO || /* Get Adapter Info Response */
	    pcb[1] != 0x0a) {
		egprintpcb(pcb);
		goto out;
	}

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = 0x08;

	ia->ia_nirq = 1;

	ia->ia_niomem = 0;
	ia->ia_ndrq = 0;

	rval = 1;

 out:
	bus_space_unmap(iot, ioh, 0x08);
	return rval;
}

void
egattach(device_t parent, device_t self, void *aux)
{
	struct eg_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int8_t myaddr[ETHER_ADDR_LEN];

	sc->sc_dev = self;

	printf("\n");

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, 0x08, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	sc->eg_pcb[0] = EG_CMD_GETINFO; /* Get Adapter Info */
	sc->eg_pcb[1] = 0;
	if (egwritePCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(self, "error requesting adapter info\n");
		return;
	}
	if (egreadPCB(iot, ioh, sc->eg_pcb) != 0) {
		egprintpcb(sc->eg_pcb);
		aprint_error_dev(self, "error reading adapter info\n");
		return;
	}

	if (sc->eg_pcb[0] != EG_RSP_GETINFO || /* Get Adapter Info Response */
	    sc->eg_pcb[1] != 0x0a) {
		egprintpcb(sc->eg_pcb);
		aprint_error_dev(self, "bogus adapter info\n");
		return;
	}

	sc->eg_rom_major = sc->eg_pcb[3];
	sc->eg_rom_minor = sc->eg_pcb[2];
	sc->eg_ram = sc->eg_pcb[6] | (sc->eg_pcb[7] << 8);

	egstop(sc);

	sc->eg_pcb[0] = EG_CMD_GETEADDR; /* Get Station address */
	sc->eg_pcb[1] = 0;
	if (egwritePCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(self, "can't send Get Station Address\n");
		return;
	}
	if (egreadPCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(self, "can't read station address\n");
		egprintpcb(sc->eg_pcb);
		return;
	}

	/* check Get station address response */
	if (sc->eg_pcb[0] != EG_RSP_GETEADDR || sc->eg_pcb[1] != 0x06) {
		aprint_error_dev(self, "card responded with garbage (1)\n");
		egprintpcb(sc->eg_pcb);
		return;
	}
	memcpy(myaddr, &sc->eg_pcb[2], ETHER_ADDR_LEN);

	printf("%s: ROM v%d.%02d %dk address %s\n", device_xname(self),
	    sc->eg_rom_major, sc->eg_rom_minor, sc->eg_ram,
	    ether_sprintf(myaddr));

	sc->eg_pcb[0] = EG_CMD_SETEADDR; /* Set station address */
	if (egwritePCB(iot, ioh, sc->eg_pcb) != 0) {
		printf("%s: can't send Set Station Address\n", device_xname(self));
		return;
	}
	if (egreadPCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(self, "can't read Set Station Address status\n");
		egprintpcb(sc->eg_pcb);
		return;
	}
	if (sc->eg_pcb[0] != EG_RSP_SETEADDR || sc->eg_pcb[1] != 0x02 ||
	    sc->eg_pcb[2] != 0 || sc->eg_pcb[3] != 0) {
		aprint_error_dev(self, "card responded with garbage (2)\n");
		egprintpcb(sc->eg_pcb);
		return;
	}

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = egstart;
	ifp->if_ioctl = egioctl;
	ifp->if_watchdog = egwatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);

	/* Now we can attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, myaddr);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, egintr, sc);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);
}

void
eginit(struct eg_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* soft reset the board */
	bus_space_write_1(iot, ioh, EG_CONTROL, EG_CTL_FLSH);
	delay(100);
	bus_space_write_1(iot, ioh, EG_CONTROL, EG_CTL_ATTN);
	delay(100);
	bus_space_write_1(iot, ioh, EG_CONTROL, 0);
	delay(200);

	sc->eg_pcb[0] = EG_CMD_CONFIG82586; /* Configure 82586 */
	sc->eg_pcb[1] = 2;
	sc->eg_pcb[2] = 3; /* receive broadcast & multicast */
	sc->eg_pcb[3] = 0;
	if (egwritePCB(iot, ioh, sc->eg_pcb) != 0)
		aprint_error_dev(sc->sc_dev, "can't send Configure 82586\n");

	if (egreadPCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(sc->sc_dev, "can't read Configure 82586 status\n");
		egprintpcb(sc->eg_pcb);
	} else if (sc->eg_pcb[2] != 0 || sc->eg_pcb[3] != 0)
		aprint_error_dev(sc->sc_dev, "configure card command failed\n");

	if (sc->eg_inbuf == NULL) {
		sc->eg_inbuf = malloc(EG_BUFLEN, M_TEMP, M_NOWAIT);
		if (sc->eg_inbuf == NULL) {
			aprint_error_dev(sc->sc_dev, "can't allocate inbuf\n");
			panic("eginit");
		}
	}
	sc->eg_incount = 0;

	if (sc->eg_outbuf == NULL) {
		sc->eg_outbuf = malloc(EG_BUFLEN, M_TEMP, M_NOWAIT);
		if (sc->eg_outbuf == NULL) {
			aprint_error_dev(sc->sc_dev, "can't allocate outbuf\n");
			panic("eginit");
		}
	}

	bus_space_write_1(iot, ioh, EG_CONTROL, EG_CTL_CMDE);

	sc->eg_incount = 0;
	egrecv(sc);

	/* Interface is now `running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Attempt to start output, if any. */
	egstart(ifp);
}

void
egrecv(struct eg_softc *sc)
{

	while (sc->eg_incount < EG_INLEN) {
		sc->eg_pcb[0] = EG_CMD_RECVPACKET;
		sc->eg_pcb[1] = 0x08;
		sc->eg_pcb[2] = 0; /* address not used.. we send zero */
		sc->eg_pcb[3] = 0;
		sc->eg_pcb[4] = 0;
		sc->eg_pcb[5] = 0;
		sc->eg_pcb[6] = EG_BUFLEN & 0xff; /* our buffer size */
		sc->eg_pcb[7] = (EG_BUFLEN >> 8) & 0xff;
		sc->eg_pcb[8] = 0; /* timeout, 0 == none */
		sc->eg_pcb[9] = 0;
		if (egwritePCB(sc->sc_iot, sc->sc_ioh, sc->eg_pcb) != 0)
			break;
		sc->eg_incount++;
	}
}

void
egstart(struct ifnet *ifp)
{
	struct eg_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct mbuf *m0, *m;
	char *buffer;
	int len;
	u_int16_t *ptr;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

loop:
	/* Dequeue the next datagram. */
	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)
		return;

	ifp->if_flags |= IFF_OACTIVE;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0) {
		aprint_error_dev(sc->sc_dev, "no header mbuf\n");
		panic("egstart");
	}
	len = max(m0->m_pkthdr.len, ETHER_MIN_LEN - ETHER_CRC_LEN);

	bpf_mtap(ifp, m0);

	sc->eg_pcb[0] = EG_CMD_SENDPACKET;
	sc->eg_pcb[1] = 0x06;
	sc->eg_pcb[2] = 0; /* address not used, we send zero */
	sc->eg_pcb[3] = 0;
	sc->eg_pcb[4] = 0;
	sc->eg_pcb[5] = 0;
	sc->eg_pcb[6] = len; /* length of packet */
	sc->eg_pcb[7] = len >> 8;
	if (egwritePCB(iot, ioh, sc->eg_pcb) != 0) {
		aprint_error_dev(sc->sc_dev, "can't send Send Packet command\n");
		ifp->if_oerrors++;
		ifp->if_flags &= ~IFF_OACTIVE;
		m_freem(m0);
		goto loop;
	}

	buffer = sc->eg_outbuf;
	for (m = m0; m != 0; m = m->m_next) {
		memcpy(buffer, mtod(m, void *), m->m_len);
		buffer += m->m_len;
	}
	if (len > m0->m_pkthdr.len)
		memset(buffer, 0, len - m0->m_pkthdr.len);

	/* set direction bit: host -> adapter */
	bus_space_write_1(iot, ioh, EG_CONTROL,
	    bus_space_read_1(iot, ioh, EG_CONTROL) & ~EG_CTL_DIR);

	for (ptr = (u_int16_t *) sc->eg_outbuf; len > 0; len -= 2) {
		bus_space_write_2(iot, ioh, EG_DATA, *ptr++);
		while (!(bus_space_read_1(iot, ioh, EG_STATUS) & EG_STAT_HRDY))
			; /* XXX need timeout here */
	}

	m_freem(m0);
}

int
egintr(void *arg)
{
	struct eg_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, len, serviced;
	u_int16_t *ptr;

	serviced = 0;

	while (bus_space_read_1(iot, ioh, EG_STATUS) & EG_STAT_ACRF) {
		egreadPCB(iot, ioh, sc->eg_pcb);
		switch (sc->eg_pcb[0]) {
		case EG_RSP_RECVPACKET:
			len = sc->eg_pcb[6] | (sc->eg_pcb[7] << 8);

			/* Set direction bit : Adapter -> host */
			bus_space_write_1(iot, ioh, EG_CONTROL,
			    bus_space_read_1(iot, ioh, EG_CONTROL) | EG_CTL_DIR);

			for (ptr = (u_int16_t *) sc->eg_inbuf;
			    len > 0; len -= 2) {
				while (!(bus_space_read_1(iot, ioh, EG_STATUS) &
				    EG_STAT_HRDY))
					;
				*ptr++ = bus_space_read_2(iot, ioh, EG_DATA);
			}

			len = sc->eg_pcb[8] | (sc->eg_pcb[9] << 8);
			egread(sc, sc->eg_inbuf, len);

			sc->eg_incount--;
			egrecv(sc);
			serviced = 1;
			break;

		case EG_RSP_SENDPACKET:
			if (sc->eg_pcb[6] || sc->eg_pcb[7]) {
				DPRINTF(("%s: packet dropped\n",
				    device_xname(sc->sc_dev)));
				sc->sc_ethercom.ec_if.if_oerrors++;
			} else
				sc->sc_ethercom.ec_if.if_opackets++;
			sc->sc_ethercom.ec_if.if_collisions +=
			    sc->eg_pcb[8] & 0xf;
			sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
			egstart(&sc->sc_ethercom.ec_if);
			serviced = 1;
			break;

		/* XXX byte-order and type-size bugs here... */
		case EG_RSP_GETSTATS:
			DPRINTF(("%s: Card Statistics\n",
			    device_xname(sc->sc_dev)));
			memcpy(&i, &sc->eg_pcb[2], sizeof(i));
			DPRINTF(("Receive Packets %d\n", i));
			memcpy(&i, &sc->eg_pcb[6], sizeof(i));
			DPRINTF(("Transmit Packets %d\n", i));
			DPRINTF(("CRC errors %d\n",
			    *(short *) &sc->eg_pcb[10]));
			DPRINTF(("alignment errors %d\n",
			    *(short *) &sc->eg_pcb[12]));
			DPRINTF(("no resources errors %d\n",
			    *(short *) &sc->eg_pcb[14]));
			DPRINTF(("overrun errors %d\n",
			    *(short *) &sc->eg_pcb[16]));
			serviced = 1;
			break;

		default:
			printf("%s: egintr: Unknown response %x??\n",
			    device_xname(sc->sc_dev), sc->eg_pcb[0]);
			egprintpcb(sc->eg_pcb);
			break;
		}

		rnd_add_uint32(&sc->rnd_source, sc->eg_pcb[0]);
	}

	return serviced;
}

/*
 * Pass a packet up to the higher levels.
 */
void
egread(struct eg_softc *sc, void *buf, int len)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHER_MAX_LEN) {
		aprint_error_dev(sc->sc_dev, "invalid packet size %d; dropping\n", len);
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = egget(sc, buf, len);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);
}

/*
 * convert buf into mbufs
 */
struct mbuf *
egget(struct eg_softc *sc, void *buf, int totlen)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m, *m0, *newm;
	int len;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == 0)
		return (0);
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	len = MHLEN;
	m = m0;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0)
				goto bad;
			len = MCLBYTES;
		}

		m->m_len = len = min(totlen, len);
		memcpy(mtod(m, void *), buf, len);
		buf = (char *)buf + len;

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == 0)
				goto bad;
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return (m0);

bad:
	m_freem(m0);
	return (0);
}

int
egioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct eg_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;

		eginit(sc);
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
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			egstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			eginit(sc);
			break;
		default:
			sc->eg_pcb[0] = EG_CMD_GETSTATS;
			sc->eg_pcb[1] = 0;
			if (egwritePCB(sc->sc_iot, sc->sc_ioh, sc->eg_pcb) != 0) {
				DPRINTF(("write error\n"));
			}
			/*
			 * XXX deal with flags changes:
			 * IFF_MULTICAST, IFF_PROMISC,
			 * IFF_LINK0, IFF_LINK1,
			 */
			break;
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return error;
}

void
egreset(struct eg_softc *sc)
{
	int s;

	DPRINTF(("%s: egreset()\n", device_xname(sc->sc_dev)));
	s = splnet();
	egstop(sc);
	eginit(sc);
	splx(s);
}

void
egwatchdog(struct ifnet *ifp)
{
	struct eg_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	sc->sc_ethercom.ec_if.if_oerrors++;

	egreset(sc);
}

void
egstop(struct eg_softc *sc)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, EG_CONTROL, 0);
}
