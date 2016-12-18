/*	$NetBSD: am79900.c,v 1.24 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

/*-
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: am79900.c,v 1.24 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am79900reg.h>
#include <dev/ic/am79900var.h>

static void	am79900_meminit(struct lance_softc *);
static void	am79900_start(struct ifnet *);

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#ifdef LEDEBUG
static void	am79900_recv_print(struct lance_softc *, int);
static void	am79900_xmit_print(struct lance_softc *, int);
#endif

#define	ifp	(&sc->sc_ethercom.ec_if)

void
am79900_config(struct am79900_softc *sc)
{
	int mem, i;

	sc->lsc.sc_meminit = am79900_meminit;
	sc->lsc.sc_start = am79900_start;

	lance_config(&sc->lsc);

	mem = 0;
	sc->lsc.sc_initaddr = mem;
	mem += sizeof(struct leinit);
	sc->lsc.sc_rmdaddr = mem;
	mem += sizeof(struct lermd) * sc->lsc.sc_nrbuf;
	sc->lsc.sc_tmdaddr = mem;
	mem += sizeof(struct letmd) * sc->lsc.sc_ntbuf;
	for (i = 0; i < sc->lsc.sc_nrbuf; i++, mem += LEBLEN)
		sc->lsc.sc_rbufaddr[i] = mem;
	for (i = 0; i < sc->lsc.sc_ntbuf; i++, mem += LEBLEN)
		sc->lsc.sc_tbufaddr[i] = mem;

	if (mem > sc->lsc.sc_memsize)
		panic("%s: memsize", device_xname(sc->lsc.sc_dev));
}

/*
 * Set up the initialization block and the descriptor rings.
 */
static void
am79900_meminit(struct lance_softc *sc)
{
	u_long a;
	int bix;
	struct leinit init;
	struct lermd rmd;
	struct letmd tmd;
	uint8_t *myaddr;

	if (ifp->if_flags & IFF_PROMISC)
		init.init_mode = LE_MODE_NORMAL | LE_MODE_PROM;
	else
		init.init_mode = LE_MODE_NORMAL;
	if (sc->sc_initmodemedia == 1)
		init.init_mode |= LE_MODE_PSEL0;

	init.init_mode |= ((ffs(sc->sc_ntbuf) - 1) << 28)
	  | ((ffs(sc->sc_nrbuf) - 1) << 20);

	/*
	 * Update our private copy of the Ethernet address.
	 * We NEED the copy so we can ensure its alignment!
	 */
	memcpy(sc->sc_enaddr, CLLADDR(ifp->if_sadl), ETHER_ADDR_LEN);
	myaddr = sc->sc_enaddr;

	init.init_padr[0] = myaddr[0] | (myaddr[1] << 8)
	  | (myaddr[2] << 16) | (myaddr[3] << 24);
	init.init_padr[1] = myaddr[4] | (myaddr[5] << 8);
	lance_setladrf(&sc->sc_ethercom, init.init_ladrf);

	sc->sc_last_rd = 0;
	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;

	a = sc->sc_addr + LE_RMDADDR(sc, 0);
	init.init_rdra = a;

	a = sc->sc_addr + LE_TMDADDR(sc, 0);
	init.init_tdra = a;

	(*sc->sc_copytodesc)(sc, &init, LE_INITADDR(sc), sizeof(init));

	/*
	 * Set up receive ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_nrbuf; bix++) {
		a = sc->sc_addr + LE_RBUFADDR(sc, bix);
		rmd.rmd0 = a;
		rmd.rmd1 = LE_R1_OWN | LE_R1_ONES | (-LEBLEN & 0xfff);
		rmd.rmd2 = 0;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, LE_RMDADDR(sc, bix),
		    sizeof(rmd));
	}

	/*
	 * Set up transmit ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_ntbuf; bix++) {
		a = sc->sc_addr + LE_TBUFADDR(sc, bix);
		tmd.tmd0 = a;
		tmd.tmd1 = LE_T1_ONES;
		tmd.tmd2 = 0;
		tmd.tmd3 = 0;
		(*sc->sc_copytodesc)(sc, &tmd, LE_TMDADDR(sc, bix),
		    sizeof(tmd));
	}
}

static inline void
am79900_rint(struct lance_softc *sc)
{
	int bix;
	int rp;
	struct lermd rmd;

	bix = sc->sc_last_rd;

	/* Process all buffers with valid data. */
	for (;;) {
		rp = LE_RMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &rmd, rp, sizeof(rmd));

		if (rmd.rmd1 & LE_R1_OWN)
			break;

		if (rmd.rmd1 & LE_R1_ERR) {
			if (rmd.rmd1 & LE_R1_ENP) {
#ifdef LEDEBUG
				if ((rmd.rmd1 & LE_R1_OFLO) == 0) {
					if (rmd.rmd1 & LE_R1_FRAM)
						printf("%s: framing error\n",
						    device_xname(sc->sc_dev));
					if (rmd.rmd1 & LE_R1_CRC)
						printf("%s: crc mismatch\n",
						    device_xname(sc->sc_dev));
				}
#endif
			} else {
				if (rmd.rmd1 & LE_R1_OFLO)
					printf("%s: overflow\n",
					    device_xname(sc->sc_dev));
			}
			if (rmd.rmd1 & LE_R1_BUFF)
				printf("%s: receive buffer error\n",
				    device_xname(sc->sc_dev));
			ifp->if_ierrors++;
		} else if ((rmd.rmd1 & (LE_R1_STP | LE_R1_ENP)) !=
		    (LE_R1_STP | LE_R1_ENP)) {
			printf("%s: dropping chained buffer\n",
			    device_xname(sc->sc_dev));
			ifp->if_ierrors++;
		} else {
#ifdef LEDEBUG
			if (sc->sc_debug)
				am79900_recv_print(sc, sc->sc_last_rd);
#endif
			lance_read(sc, LE_RBUFADDR(sc, bix),
				   (rmd.rmd2  & 0xfff) - 4);
		}

		rmd.rmd1 = LE_R1_OWN | LE_R1_ONES | (-LEBLEN & 0xfff);
		rmd.rmd2 = 0;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, rp, sizeof(rmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("sc->sc_last_rd = %x, rmd: "
			       "adr %08x, flags/blen %08x\n",
				sc->sc_last_rd,
				rmd.rmd0, rmd.rmd1);
#endif

		if (++bix == sc->sc_nrbuf)
			bix = 0;
	}

	sc->sc_last_rd = bix;
}

static inline void
am79900_tint(struct lance_softc *sc)
{
	int bix;
	struct letmd tmd;

	bix = sc->sc_first_td;

	for (;;) {
		if (sc->sc_no_td <= 0)
			break;

		(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, bix),
		    sizeof(tmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("trans tmd: "
			    "adr %08x, flags/blen %08x\n",
			    tmd.tmd0, tmd.tmd1);
#endif

		if (tmd.tmd1 & LE_T1_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;

		if (tmd.tmd1 & LE_T1_ERR) {
			if (tmd.tmd2 & LE_T2_BUFF)
				printf("%s: transmit buffer error\n",
				    device_xname(sc->sc_dev));
			else if (tmd.tmd2 & LE_T2_UFLO)
				printf("%s: underflow\n",
				    device_xname(sc->sc_dev));
			if (tmd.tmd2 & (LE_T2_BUFF | LE_T2_UFLO)) {
				lance_reset(sc);
				return;
			}
			if (tmd.tmd2 & LE_T2_LCAR) {
				sc->sc_havecarrier = 0;
				if (sc->sc_nocarrier)
					(*sc->sc_nocarrier)(sc);
				else
					printf("%s: lost carrier\n",
					    device_xname(sc->sc_dev));
			}
			if (tmd.tmd2 & LE_T2_LCOL)
				ifp->if_collisions++;
			if (tmd.tmd2 & LE_T2_RTRY) {
#ifdef LEDEBUG
				printf("%s: excessive collisions\n",
				    device_xname(sc->sc_dev));
#endif
				ifp->if_collisions += 16;
			}
			ifp->if_oerrors++;
		} else {
			if (tmd.tmd1 & LE_T1_ONE)
				ifp->if_collisions++;
			else if (tmd.tmd1 & LE_T1_MORE)
				/* Real number is unknown. */
				ifp->if_collisions += 2;
			ifp->if_opackets++;
		}

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		--sc->sc_no_td;
	}

	sc->sc_first_td = bix;

	am79900_start(ifp);

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;
}

/*
 * Controller interrupt.
 */
int
am79900_intr(void *arg)
{
	struct lance_softc *sc = arg;
	uint16_t isr;

	isr = (*sc->sc_rdcsr)(sc, LE_CSR0) | sc->sc_saved_csr0;
	sc->sc_saved_csr0 = 0;
#if defined(LEDEBUG) && LEDEBUG > 1
	if (sc->sc_debug)
		printf("%s: am79900_intr entering with isr=%04x\n",
		    device_xname(sc->sc_dev), isr);
#endif
	if ((isr & LE_C0_INTR) == 0)
		return (0);

	(*sc->sc_wrcsr)(sc, LE_CSR0,
	    isr & (LE_C0_INEA | LE_C0_BABL | LE_C0_MISS | LE_C0_MERR |
		   LE_C0_RINT | LE_C0_TINT | LE_C0_IDON));
	if (isr & LE_C0_ERR) {
		if (isr & LE_C0_BABL) {
#ifdef LEDEBUG
			printf("%s: babble\n", device_xname(sc->sc_dev));
#endif
			ifp->if_oerrors++;
		}
#if 0
		if (isr & LE_C0_CERR) {
			printf("%s: collision error\n",
			    device_xname(sc->sc_dev));
			ifp->if_collisions++;
		}
#endif
		if (isr & LE_C0_MISS) {
#ifdef LEDEBUG
			printf("%s: missed packet\n", device_xname(sc->sc_dev));
#endif
			ifp->if_ierrors++;
		}
		if (isr & LE_C0_MERR) {
			printf("%s: memory error\n", device_xname(sc->sc_dev));
			lance_reset(sc);
			return (1);
		}
	}

	if ((isr & LE_C0_RXON) == 0) {
		printf("%s: receiver disabled\n", device_xname(sc->sc_dev));
		ifp->if_ierrors++;
		lance_reset(sc);
		return (1);
	}
	if ((isr & LE_C0_TXON) == 0) {
		printf("%s: transmitter disabled\n", device_xname(sc->sc_dev));
		ifp->if_oerrors++;
		lance_reset(sc);
		return (1);
	}

	/*
	 * Pretend we have carrier; if we don't this will be cleared
	 * shortly.
	 */
	sc->sc_havecarrier = 1;

	if (isr & LE_C0_RINT)
		am79900_rint(sc);
	if (isr & LE_C0_TINT)
		am79900_tint(sc);

	rnd_add_uint32(&sc->rnd_source, isr);

	return (1);
}

#undef	ifp

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue, and map it to the
 * interface before starting the output.
 * Called only at splnet or interrupt level.
 */
static void
am79900_start(struct ifnet *ifp)
{
	struct lance_softc *sc = ifp->if_softc;
	int bix;
	struct mbuf *m;
	struct letmd tmd;
	int rp;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

	for (;;) {
		rp = LE_TMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &tmd, rp, sizeof(tmd));

		if (tmd.tmd1 & LE_T1_OWN) {
			ifp->if_flags |= IFF_OACTIVE;
			printf("missing buffer, no_td = %d, last_td = %d\n",
			    sc->sc_no_td, sc->sc_last_td);
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		/*
		 * If BPF is listening on this interface, let it see the packet
		 * before we commit it to the wire.
		 */
		bpf_mtap(ifp, m);

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = lance_put(sc, LE_TBUFADDR(sc, bix), m);

#ifdef LEDEBUG
		if (len > ETHERMTU + sizeof(struct ether_header))
			printf("packet length %d\n", len);
#endif

		ifp->if_timer = 5;

		/*
		 * Init transmit registers, and set transmit start flag.
		 */
		tmd.tmd1 = LE_T1_OWN | LE_T1_STP | LE_T1_ENP | LE_T1_ONES | (-len & 0xfff);
		tmd.tmd2 = 0;
		tmd.tmd3 = 0;

		(*sc->sc_copytodesc)(sc, &tmd, rp, sizeof(tmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			am79900_xmit_print(sc, sc->sc_last_td);
#endif

		(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INEA | LE_C0_TDMD);

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		if (++sc->sc_no_td == sc->sc_ntbuf) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

	}

	sc->sc_last_td = bix;
}

#ifdef LEDEBUG
static void
am79900_recv_print(struct lance_softc *sc, int no)
{
	struct lermd rmd;
	uint16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &rmd, LE_RMDADDR(sc, no), sizeof(rmd));
	len = (rmd.rmd2  & 0xfff) - 4;
	printf("%s: receive buffer %d, len = %d\n",
	    device_xname(sc->sc_dev), no, len);
	printf("%s: status %04x\n", device_xname(sc->sc_dev),
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: adr %08x, flags/blen %08x\n",
	    device_xname(sc->sc_dev), rmd.rmd0, rmd.rmd1);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_RBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", device_xname(sc->sc_dev),
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
			ntohs(eh.ether_type));
	}
}

static void
am79900_xmit_print(struct lance_softc *sc, int no)
{
	struct letmd tmd;
	uint16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, no), sizeof(tmd));
	len = -(tmd.tmd1 & 0xfff);
	printf("%s: transmit buffer %d, len = %d\n",
	    device_xname(sc->sc_dev), no, len);
	printf("%s: status %04x\n", device_xname(sc->sc_dev),
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: adr %08x, flags/blen %08x\n",
	    device_xname(sc->sc_dev), tmd.tmd0, tmd.tmd1);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_TBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", device_xname(sc->sc_dev),
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
		    ntohs(eh.ether_type));
	}
}
#endif /* LEDEBUG */
