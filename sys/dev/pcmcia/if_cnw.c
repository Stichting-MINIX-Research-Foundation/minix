/*	$NetBSD: if_cnw.c,v 1.56 2012/10/27 17:18:36 chs Exp $	*/

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Eriksson.
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
 * Copyright (c) 1996, 1997 Berkeley Software Design, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this notice is retained,
 * the conditions in the following notices are met, and terms applying
 * to contributors in the following notices also apply to Berkeley
 * Software Design, Inc.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *	Berkeley Software Design, Inc.
 * 4. Neither the name of the Berkeley Software Design, Inc. nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Paul Borman, December 1996
 *
 * This driver is derived from a generic frame work which is
 * Copyright(c) 1994,1995,1996
 * Yoichi Shinoda, Yoshitaka Tokugawa, WIDE Project, Wildboar Project
 * and Foretune.  All rights reserved.
 *
 * A linux driver was used as the "hardware reference manual" (i.e.,
 * to determine registers and a general outline of how the card works)
 * That driver is publically available and copyright
 *
 * John Markus Bjørndalen
 * Department of Computer Science
 * University of Tromsø
 * Norway
 * johnm@staff.cs.uit.no, http://www.cs.uit.no/~johnm/
 */

/*
 * This is a driver for the Xircom CreditCard Netwave (also known as
 * the Netwave Airsurfer) wireless LAN PCMCIA adapter.
 *
 * When this driver was developed, the Linux Netwave driver was used
 * as a hardware manual. That driver is Copyright (c) 1997 University
 * of Tromsø, Norway. It is part of the Linux pcmcia-cs package that
 * can be found at http://pcmcia-cs.sourceforge.net/. The most recent
 * version of the pcmcia-cs package when this driver was written was
 * 3.0.6.
 *
 * Unfortunately, a lot of explicit numeric constants were used in the
 * Linux driver. I have tried to use symbolic names whenever possible,
 * but since I don't have any real hardware documentation, there's
 * still one or two "magic numbers" :-(.
 *
 * Driver limitations: This driver doesn't do multicasting or receiver
 * promiscuity, because of missing hardware documentation. I couldn't
 * get receiver promiscuity to work, and I haven't even tried
 * multicast. Volunteers are welcome, of course :-).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_cnw.c,v 1.56 2012/10/27 17:18:36 chs Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <net/if.h>

#include <dev/pcmcia/if_cnwreg.h>
#include <dev/pcmcia/if_cnwioctl.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <net/if_dl.h>
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
 * Let these be patchable variables, initialized from macros that can
 * be set in the kernel config file. Someone with lots of spare time
 * could probably write a nice Netwave configuration program to do
 * this a little bit more elegantly :-).
 */
#ifndef CNW_DOMAIN
#define CNW_DOMAIN	0x100
#endif
int cnw_domain = CNW_DOMAIN;		/* Domain */
#ifndef CNW_SCRAMBLEKEY
#define CNW_SCRAMBLEKEY 0
#endif
int cnw_skey = CNW_SCRAMBLEKEY;		/* Scramble key */

/*
 * The card appears to work much better when we only allow one packet
 * "in the air" at a time.  This is done by not allowing another packet
 * on the card, even if there is room.  Turning this off will allow the
 * driver to stuff packets on the card as soon as a transmit buffer is
 * available.  This does increase the number of collisions, though.
 * We can que a second packet if there are transmit buffers available,
 * but we do not actually send the packet until the last packet has
 * been written.
 */
#define	ONE_AT_A_TIME

/*
 * Netwave cards choke if we try to use io memory address >= 0x400.
 * Even though, CIS tuple does not talk about this.
 * Use memory mapped access.
 */
#define MEMORY_MAPPED

int	cnw_match(device_t, cfdata_t, void *);
void	cnw_attach(device_t, device_t, void *);
int	cnw_detach(device_t, int);

int	cnw_activate(device_t, enum devact);

struct cnw_softc {
	device_t sc_dev;		    /* Device glue (must be first) */
	struct ethercom sc_ethercom;	    /* Ethernet common part */
	int sc_domain;			    /* Netwave domain */
	int sc_skey;			    /* Netwave scramble key */
	struct cnwstats sc_stats;

	/* PCMCIA-specific stuff */
	struct pcmcia_function *sc_pf;	    /* PCMCIA function */
#ifndef MEMORY_MAPPED
	struct pcmcia_io_handle sc_pcioh;   /* PCMCIA I/O space handle */
	int sc_iowin;			    /*   ...window */
	bus_space_tag_t sc_iot;		    /*   ...bus_space tag */
	bus_space_handle_t sc_ioh;	    /*   ...bus_space handle */
#endif
	struct pcmcia_mem_handle sc_pcmemh; /* PCMCIA memory handle */
	bus_size_t sc_memoff;		    /*   ...offset */
	int sc_memwin;			    /*   ...window */
	bus_space_tag_t sc_memt;	    /*   ...bus_space tag */
	bus_space_handle_t sc_memh;	    /*   ...bus_space handle */
	void *sc_ih;			    /* Interrupt cookie */
	struct timeval sc_txlast;	    /* When the last xmit was made */
	int sc_active;			    /* Currently xmitting a packet */

	int sc_resource;		    /* Resources alloc'ed on attach */
#define CNW_RES_PCIC	1
#define CNW_RES_IO	2
#define CNW_RES_MEM	4
#define CNW_RES_NET	8
};

CFATTACH_DECL_NEW(cnw, sizeof(struct cnw_softc),
    cnw_match, cnw_attach, cnw_detach, cnw_activate);

void cnw_reset(struct cnw_softc *);
void cnw_init(struct cnw_softc *);
int cnw_enable(struct cnw_softc *sc);
void cnw_disable(struct cnw_softc *sc);
void cnw_config(struct cnw_softc *sc, u_int8_t *);
void cnw_start(struct ifnet *);
void cnw_transmit(struct cnw_softc *, struct mbuf *);
struct mbuf *cnw_read(struct cnw_softc *);
void cnw_recv(struct cnw_softc *);
int cnw_intr(void *arg);
int cnw_ioctl(struct ifnet *, u_long, void *);
void cnw_watchdog(struct ifnet *);
static int cnw_setdomain(struct cnw_softc *, int);
static int cnw_setkey(struct cnw_softc *, int);

/* ---------------------------------------------------------------- */

/* Help routines */
static int wait_WOC(struct cnw_softc *, int);
static int read16(struct cnw_softc *, int);
static int cnw_cmd(struct cnw_softc *, int, int, int, int);

/*
 * Wait until the WOC (Write Operation Complete) bit in the
 * ASR (Adapter Status Register) is asserted.
 */
static int
wait_WOC(struct cnw_softc *sc, int line)
{
	int i, asr;

	for (i = 0; i < 5000; i++) {
#ifndef MEMORY_MAPPED
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		asr = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif
		if (asr & CNW_ASR_WOC)
			return (0);
		DELAY(100);
	}
	if (line > 0)
		printf("%s: wedged at line %d\n", device_xname(sc->sc_dev), line);
	return (1);
}
#define WAIT_WOC(sc) wait_WOC(sc, __LINE__)


/*
 * Read a 16 bit value from the card.
 */
static int
read16(struct cnw_softc *sc, int offset)
{
	int hi, lo;
	int offs = sc->sc_memoff + offset;

	/* This could presumably be done more efficient with
	 * bus_space_read_2(), but I don't know anything about the
	 * byte sex guarantees... Besides, this is pretty cheap as
	 * well :-)
	 */
	lo = bus_space_read_1(sc->sc_memt, sc->sc_memh, offs);
	hi = bus_space_read_1(sc->sc_memt, sc->sc_memh, offs + 1);
	return ((hi << 8) | lo);
}


/*
 * Send a command to the card by writing it to the command buffer.
 */
int
cnw_cmd(struct cnw_softc *sc, int cmd, int count, int arg1, int arg2)
{
	int ptr = sc->sc_memoff + CNW_EREG_CB;

	if (wait_WOC(sc, 0)) {
		printf("%s: wedged when issuing cmd 0x%x\n",
		    device_xname(sc->sc_dev), cmd);
		/*
		 * We'll continue anyway, as that's probably the best
		 * thing we can do; at least the user knows there's a
		 * problem, and can reset the interface with ifconfig
		 * down/up.
		 */
	}

	bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr, cmd);
	if (count > 0) {
		bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr + 1, arg1);
		if (count > 1)
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    ptr + 2, arg2);
	}
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    ptr + count + 1, CNW_CMD_EOC);
	return (0);
}
#define CNW_CMD0(sc, cmd) \
    do { cnw_cmd(sc, cmd, 0, 0, 0); } while (0)
#define CNW_CMD1(sc, cmd, arg1)	\
    do { cnw_cmd(sc, cmd, 1, arg1 , 0); } while (0)
#define CNW_CMD2(sc, cmd, arg1, arg2) \
    do { cnw_cmd(sc, cmd, 2, arg1, arg2); } while (0)

/* ---------------------------------------------------------------- */

/*
 * Reset the hardware.
 */
void
cnw_reset(struct cnw_softc *sc)
{
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: resetting\n", device_xname(sc->sc_dev));
#endif
	wait_WOC(sc, 0);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, CNW_PMR_RESET);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_PMR, CNW_PMR_RESET);
#endif
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_WOC);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, 0);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_PMR, 0);
#endif
}


/*
 * Initialize the card.
 */
void
cnw_init(struct cnw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const u_int8_t rxmode =
	    CNW_RXCONF_RXENA | CNW_RXCONF_BCAST | CNW_RXCONF_AMP;

	/* Reset the card */
	cnw_reset(sc);

	/* Issue a NOP to check the card */
	CNW_CMD0(sc, CNW_CMD_NOP);

	/* Set up receive configuration */
	CNW_CMD1(sc, CNW_CMD_SRC,
	    rxmode | ((ifp->if_flags & IFF_PROMISC) ? CNW_RXCONF_PRO : 0));

	/* Set up transmit configuration */
	CNW_CMD1(sc, CNW_CMD_STC, CNW_TXCONF_TXENA);

	/* Set domain */
	CNW_CMD2(sc, CNW_CMD_SMD, sc->sc_domain, sc->sc_domain >> 8);

	/* Set scramble key */
	CNW_CMD2(sc, CNW_CMD_SSK, sc->sc_skey, sc->sc_skey >> 8);

	/* Enable interrupts */
	WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    CNW_REG_IMR, CNW_IMR_IENA | CNW_IMR_RFU1);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_IMR,
	    CNW_IMR_IENA | CNW_IMR_RFU1);
#endif

	/* Enable receiver */
	CNW_CMD0(sc, CNW_CMD_ER);

	/* "Set the IENA bit in COR" */
	WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_COR,
	    CNW_COR_IENA | CNW_COR_LVLREQ);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_COR,
	    CNW_COR_IENA | CNW_COR_LVLREQ);
#endif
}


/*
 * Enable and initialize the card.
 */
int
cnw_enable(struct cnw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) != 0)
		return (0);

	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET, cnw_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt handler\n");
		return (EIO);
	}
	if (pcmcia_function_enable(sc->sc_pf) != 0) {
		aprint_error_dev(sc->sc_dev, "couldn't enable card\n");
		return (EIO);
	}
	sc->sc_resource |= CNW_RES_PCIC;
	cnw_init(sc);
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;
	return (0);
}


/*
 * Stop and disable the card.
 */
void
cnw_disable(struct cnw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	pcmcia_function_disable(sc->sc_pf);
	sc->sc_resource &= ~CNW_RES_PCIC;
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;
}


/*
 * Match the hardware we handle.
 */
int
cnw_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_CNW_801)
		return 1;
	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_CNW_802)
		return 1;
	return 0;
}


/*
 * Attach the card.
 */
void
cnw_attach(device_t parent, device_t self, void *aux)
{
	struct cnw_softc *sc = device_private(self);
	struct pcmcia_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int8_t macaddr[ETHER_ADDR_LEN];
	int i;
	bus_size_t memsize;

	sc->sc_dev = self;
	sc->sc_resource = 0;

	/* Enable the card */
	sc->sc_pf = pa->pf;
	pcmcia_function_init(sc->sc_pf, SIMPLEQ_FIRST(&sc->sc_pf->cfe_head));
	if (pcmcia_function_enable(sc->sc_pf)) {
		aprint_error_dev(self, "function enable failed\n");
		return;
	}
	sc->sc_resource |= CNW_RES_PCIC;

	/* Map I/O register and "memory" */
#ifndef MEMORY_MAPPED
	if (pcmcia_io_alloc(sc->sc_pf, 0, CNW_IO_SIZE, CNW_IO_SIZE,
	    &sc->sc_pcioh) != 0) {
		aprint_error_dev(self, "can't allocate i/o space\n");
		goto fail;
	}
	if (pcmcia_io_map(sc->sc_pf, PCMCIA_WIDTH_IO16, &sc->sc_pcioh,
	    &sc->sc_iowin) != 0) {
		aprint_error_dev(self, "can't map i/o space\n");
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
		goto fail;
	}
	sc->sc_iot = sc->sc_pcioh.iot;
	sc->sc_ioh = sc->sc_pcioh.ioh;
	sc->sc_resource |= CNW_RES_IO;
#endif
#ifndef MEMORY_MAPPED
	memsize = CNW_MEM_SIZE;
#else
	memsize = CNW_MEM_SIZE + CNW_IOM_SIZE;
#endif
	if (pcmcia_mem_alloc(sc->sc_pf, memsize, &sc->sc_pcmemh) != 0) {
		aprint_error_dev(self, "can't allocate memory\n");
		goto fail;
	}
	if (pcmcia_mem_map(sc->sc_pf, PCMCIA_WIDTH_MEM8|PCMCIA_MEM_COMMON,
	    CNW_MEM_ADDR, memsize, &sc->sc_pcmemh, &sc->sc_memoff,
	    &sc->sc_memwin) != 0) {
		aprint_error_dev(self, "can't map memory\n");
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pcmemh);
		goto fail;
	}
	sc->sc_memt = sc->sc_pcmemh.memt;
	sc->sc_memh = sc->sc_pcmemh.memh;
	sc->sc_resource |= CNW_RES_MEM;

	/* Finish setup of softc */
	sc->sc_domain = cnw_domain;
	sc->sc_skey = cnw_skey;

	/* Get MAC address */
	cnw_reset(sc);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		macaddr[i] = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_PA + i);
	printf("%s: address %s\n", device_xname(sc->sc_dev),
	    ether_sprintf(macaddr));

	/* Set up ifnet structure */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = cnw_start;
	ifp->if_ioctl = cnw_ioctl;
	ifp->if_watchdog = cnw_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX |
	    IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface */
	if_attach(ifp);
	ether_ifattach(ifp, macaddr);

	sc->sc_resource |= CNW_RES_NET;

	ifp->if_baudrate = IF_Mbps(1);

	/* Disable the card now, and turn it on when the interface goes up */
	pcmcia_function_disable(sc->sc_pf);
	sc->sc_resource &= ~CNW_RES_PCIC;
	return;

fail:
#ifndef MEMORY_MAPPED
	if ((sc->sc_resource & CNW_RES_IO) != 0) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
		sc->sc_resource &= ~CNW_RES_IO;
	}
#endif
	if ((sc->sc_resource & CNW_RES_PCIC) != 0) {
		pcmcia_function_disable(sc->sc_pf);
		sc->sc_resource &= ~CNW_RES_PCIC;
	}
}

/*
 * Start outputting on the interface.
 */
void
cnw_start(struct ifnet *ifp)
{
	struct cnw_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int lif;
	int asr;
#ifdef ONE_AT_A_TIME
	struct timeval now;
#endif

#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_start\n", ifp->if_xname);
	if (ifp->if_flags & IFF_OACTIVE)
		printf("%s: cnw_start reentered\n", ifp->if_xname);
#endif

	ifp->if_flags |= IFF_OACTIVE;

	for (;;) {
#ifdef ONE_AT_A_TIME
		microtime(&now);
		now.tv_sec -= sc->sc_txlast.tv_sec;
		now.tv_usec -= sc->sc_txlast.tv_usec;
		if (now.tv_usec < 0) {
			now.tv_usec += 1000000;
			now.tv_sec--;
		}

		/*
		 * Don't ship this packet out until the last
		 * packet has left the building.
		 * If we have not tried to send a packet for 1/5
		 * a second then we assume we lost an interrupt,
		 * lets go on and send the next packet anyhow.
		 *
		 * I suppose we could check to see if it is okay
		 * to put additional packets on the card (beyond
		 * the one already waiting to be sent) but I don't
		 * think we would get any improvement in speed as
		 * we should have ample time to put the next packet
		 * on while this one is going out.
		 */
		if (sc->sc_active && now.tv_sec == 0 && now.tv_usec < 200000)
			break;
#endif

		/* Make sure the link integrity field is on */
		WAIT_WOC(sc);
		lif = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_LIF);
		if (lif == 0) {
#ifdef CNW_DEBUG
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: link integrity %d\n", ifp->if_xname, lif);
#endif
			break;
		}

		/* Is there any buffer space available on the card? */
		WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		asr = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif
		if (!(asr & CNW_ASR_TXBA)) {
#ifdef CNW_DEBUG
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: no buffer space\n", ifp->if_xname);
#endif
			break;
		}

		sc->sc_stats.nws_tx++;

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0)
			break;

		bpf_mtap(ifp, m0);

		cnw_transmit(sc, m0);
		++ifp->if_opackets;
		ifp->if_timer = 3; /* start watchdog timer */

		microtime(&sc->sc_txlast);
		sc->sc_active = 1;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
}

/*
 * Transmit a packet.
 */
void
cnw_transmit(struct cnw_softc *sc, struct mbuf *m0)
{
	int buffer, bufsize, bufoffset, bufptr, bufspace, len, mbytes, n;
	struct mbuf *m;
	u_int8_t *mptr;

	/* Get buffer info from card */
	buffer = read16(sc, CNW_EREG_TDP);
	bufsize = read16(sc, CNW_EREG_TDP + 2);
	bufoffset = read16(sc, CNW_EREG_TDP + 4);
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_transmit b=0x%x s=%d o=0x%x\n",
		    device_xname(sc->sc_dev), buffer, bufsize, bufoffset);
#endif

	/* Copy data from mbuf chain to card buffers */
	bufptr = sc->sc_memoff + buffer + bufoffset;
	bufspace = bufsize;
	len = 0;
	for (m = m0; m; ) {
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len;
		len += mbytes;
		while (mbytes > 0) {
			if (bufspace == 0) {
				buffer = read16(sc, buffer);
				bufptr = sc->sc_memoff + buffer + bufoffset;
				bufspace = bufsize;
#ifdef CNW_DEBUG
				if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
					printf("%s:   next buffer @0x%x\n",
					    device_xname(sc->sc_dev), buffer);
#endif
			}
			n = mbytes <= bufspace ? mbytes : bufspace;
			bus_space_write_region_1(sc->sc_memt, sc->sc_memh,
			    bufptr, mptr, n);
			bufptr += n;
			bufspace -= n;
			mptr += n;
			mbytes -= n;
		}
		MFREE(m, m0);
		m = m0;
	}

	/* Issue transmit command */
	CNW_CMD2(sc, CNW_CMD_TL, len, len >> 8);
}


/*
 * Pull a packet from the card into an mbuf chain.
 */
struct mbuf *
cnw_read(struct cnw_softc *sc)
{
	struct mbuf *m, *top, **mp;
	int totbytes, buffer, bufbytes, bufptr, mbytes, n;
	u_int8_t *mptr;

	WAIT_WOC(sc);
	totbytes = read16(sc, CNW_EREG_RDP);
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: recv %d bytes\n", device_xname(sc->sc_dev), totbytes);
#endif
	buffer = CNW_EREG_RDP + 2;
	bufbytes = 0;
	bufptr = 0; /* XXX make gcc happy */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = &sc->sc_ethercom.ec_if;
	m->m_pkthdr.len = totbytes;
	mbytes = MHLEN;
	top = 0;
	mp = &top;

	while (totbytes > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			mbytes = MLEN;
		}
		if (totbytes >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				return (0);
			}
			mbytes = MCLBYTES;
		}
		if (!top) {
			int pad = ALIGN(sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			m->m_data += pad;
			mbytes -= pad;
		}
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len = min(totbytes, mbytes);
		totbytes -= mbytes;
		while (mbytes > 0) {
			if (bufbytes == 0) {
				buffer = read16(sc, buffer);
				bufbytes = read16(sc, buffer + 2);
				bufptr = sc->sc_memoff + buffer +
				    read16(sc, buffer + 4);
#ifdef CNW_DEBUG
				if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
					printf("%s:   %d bytes @0x%x+0x%lx\n",
					    device_xname(sc->sc_dev), bufbytes,
					    buffer, (u_long)(bufptr - buffer -
					    sc->sc_memoff));
#endif
			}
			n = mbytes <= bufbytes ? mbytes : bufbytes;
			bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
			    bufptr, mptr, n);
			bufbytes -= n;
			bufptr += n;
			mbytes -= n;
			mptr += n;
		}
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}


/*
 * Handle received packets.
 */
void
cnw_recv(struct cnw_softc *sc)
{
	int rser;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;

	for (;;) {
		WAIT_WOC(sc);
		rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_RSER);
		if (!(rser & CNW_RSER_RXAVAIL))
			return;

		/* Pull packet off card */
		m = cnw_read(sc);

		/* Acknowledge packet */
		CNW_CMD0(sc, CNW_CMD_SRP);

		/* Did we manage to get the packet from the interface? */
		if (m == 0) {
			++ifp->if_ierrors;
			return;
		}
		++ifp->if_ipackets;

		bpf_mtap(ifp, m);

		/* Pass the packet up. */
		(*ifp->if_input)(ifp, m);
	}
}


/*
 * Interrupt handler.
 */
int
cnw_intr(void *arg)
{
	struct cnw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int ret, status, rser, tser;

	if ((sc->sc_ethercom.ec_if.if_flags & IFF_RUNNING) == 0 ||
	    !device_is_active(sc->sc_dev))
		return (0);
	ifp->if_timer = 0;	/* stop watchdog timer */

	ret = 0;
	for (;;) {
		WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
		status = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    CNW_REG_CCSR);
#else
		status = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_CCSR);
#endif
		if (!(status & 0x02))
			/* No more commands, or shared IRQ */
			return (ret);
		ret = 1;
#ifndef MEMORY_MAPPED
		status = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		status = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif

		/* Anything to receive? */
		if (status & CNW_ASR_RXRDY) {
			sc->sc_stats.nws_rx++;
			cnw_recv(sc);
		}

		/* Receive error */
		if (status & CNW_ASR_RXERR) {
			/*
			 * I get a *lot* of spurious receive errors
			 * (many per second), even when the interface
			 * is quiescent, so we don't increment
			 * if_ierrors here.
			 */
			rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_RSER);

			/* RX statistics */
			sc->sc_stats.nws_rxerr++;
			if (rser & CNW_RSER_RXBIG)
				sc->sc_stats.nws_rxframe++;
			if (rser & CNW_RSER_RXCRC)
				sc->sc_stats.nws_rxcrcerror++;
			if (rser & CNW_RSER_RXOVERRUN)
				sc->sc_stats.nws_rxoverrun++;
			if (rser & CNW_RSER_RXOVERFLOW)
				sc->sc_stats.nws_rxoverflow++;
			if (rser & CNW_RSER_RXERR)
				sc->sc_stats.nws_rxerrors++;
			if (rser & CNW_RSER_RXAVAIL)
				sc->sc_stats.nws_rxavail++;

			/* Clear error bits in RSER */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_RSERW,
			    CNW_RSER_RXERR |
			    (rser & (CNW_RSER_RXCRC | CNW_RSER_RXBIG)));
			/* Clear RXERR in ASR */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_RXERR);
		}

		/* Transmit done */
		if (status & CNW_ASR_TXDN) {
			tser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
						CNW_EREG_TSER);

			/* TX statistics */
			if (tser & CNW_TSER_TXERR)
				sc->sc_stats.nws_txerrors++;
			if (tser & CNW_TSER_TXNOAP)
				sc->sc_stats.nws_txlostcd++;
			if (tser & CNW_TSER_TXGU)
				sc->sc_stats.nws_txabort++;

			if (tser & CNW_TSER_TXOK) {
				sc->sc_stats.nws_txokay++;
				sc->sc_stats.nws_txretries[status & 0xf]++;
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
				    sc->sc_memoff + CNW_EREG_TSERW,
				    CNW_TSER_TXOK | CNW_TSER_RTRY);
			}

			if (tser & CNW_TSER_ERROR) {
				++ifp->if_oerrors;
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
				    sc->sc_memoff + CNW_EREG_TSERW,
				    (tser & CNW_TSER_ERROR) |
				    CNW_TSER_RTRY);
			}

			sc->sc_active = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			/* Continue to send packets from the queue */
			cnw_start(&sc->sc_ethercom.ec_if);
		}

	}
}


/*
 * Handle device ioctls.
 */
int
cnw_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct cnw_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;
	struct lwp *l = curlwp;	/*XXX*/

	switch (cmd) {
	case SIOCINITIFADDR:
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCGCNWDOMAIN:
	case SIOCGCNWSTATS:
		break;
	case SIOCSCNWDOMAIN:
	case SIOCSCNWKEY:
		error = kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, KAUTH_ARG(cmd),
		    NULL);
		if (error)
			return (error);
		break;
	case SIOCGCNWSTATUS:
		error = kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_GETPRIV, ifp, KAUTH_ARG(cmd),
		    NULL);
		if (error)
			return (error);
		break;
	default:
		return (EINVAL);
	}

	s = splnet();

	switch (cmd) {

	case SIOCINITIFADDR:
		if (!(ifp->if_flags & IFF_RUNNING) &&
		    (error = cnw_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		cnw_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_ethercom.ec_if, ifa);
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
			 * The interface is marked down and it is running, so
			 * stop it.
			 */
			cnw_disable(sc);
			break;
		case IFF_UP:
			/*
			 * The interface is marked up and it is stopped, so
			 * start it.
			 */
			error = cnw_enable(sc);
			break;
		default:
			/* IFF_PROMISC may be changed */
			cnw_init(sc);
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update our multicast list. */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				cnw_init(sc);
			error = 0;
		}
		break;

	case SIOCGCNWDOMAIN:
		ifr->ifr_domain = sc->sc_domain;
		break;

	case SIOCSCNWDOMAIN:
		error = cnw_setdomain(sc, ifr->ifr_domain);
		break;

	case SIOCSCNWKEY:
		error = cnw_setkey(sc, ifr->ifr_key);
		break;

	case SIOCGCNWSTATUS:
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;
		bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_CB,
		    ((struct cnwstatus *)data)->data,
		    sizeof(((struct cnwstatus *)data)->data));
		break;

	case SIOCGCNWSTATS:
		memcpy((void *)&(((struct cnwistats *)data)->stats),
		    (void *)&sc->sc_stats, sizeof(struct cnwstats));
			break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return (error);
}


/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
cnw_watchdog(struct ifnet *ifp)
{
	struct cnw_softc *sc = ifp->if_softc;

	printf("%s: device timeout; card reset\n", device_xname(sc->sc_dev));
	++ifp->if_oerrors;
	cnw_init(sc);
}

int
cnw_setdomain(struct cnw_softc *sc, int domain)
{
	int s;

	if (domain & ~0x1ff)
		return EINVAL;

	s = splnet();
	CNW_CMD2(sc, CNW_CMD_SMD, domain, domain >> 8);
	splx(s);

	sc->sc_domain = domain;
	return 0;
}

int
cnw_setkey(struct cnw_softc *sc, int key)
{
	int s;

	if (key & ~0xffff)
		return EINVAL;

	s = splnet();
	CNW_CMD2(sc, CNW_CMD_SSK, key, key >> 8);
	splx(s);

	sc->sc_skey = key;
	return 0;
}

int
cnw_activate(device_t self, enum devact act)
{
	struct cnw_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
cnw_detach(device_t self, int flags)
{
	struct cnw_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* cnw_disable() checks IFF_RUNNING */
	cnw_disable(sc);

	if ((sc->sc_resource & CNW_RES_NET) != 0) {
		ether_ifdetach(ifp);
		if_detach(ifp);
	}

#ifndef MEMORY_MAPPED
	/* unmap and free our i/o windows */
	if ((sc->sc_resource & CNW_RES_IO) != 0) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
	}
#endif

	/* unmap and free our memory windows */
	if ((sc->sc_resource & CNW_RES_MEM) != 0) {
		pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwin);
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pcmemh);
	}

	return (0);
}
