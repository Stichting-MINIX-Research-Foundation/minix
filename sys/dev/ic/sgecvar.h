/*      $NetBSD: sgecvar.h,v 1.8 2008/03/11 05:34:01 matt Exp $ */
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
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

#ifndef _DEV_IC_SGECVAR_H_
#define _DEV_IC_SGECVAR_H_

#define RXDESCS	30	/* # of receive descriptors */
#define TXDESCS	60	/* # transmit descs */

/*
 * Descriptors must be an even number; optimisation thing.
 */
struct ze_cdata {
	struct ze_rdes	zc_recv[RXDESCS+2];	/* Receive descriptors */
	struct ze_tdes	zc_xmit[TXDESCS+2];	/* Transmit descriptors */
	u_int8_t	zc_setup[128];		/* Setup packet layout */
};

struct	ze_softc {
	device_t	sc_dev;		/* Configuration common part	*/
	struct evcnt	sc_intrcnt;	/* Interrupt counters		*/
	struct evcnt	sc_rxintrcnt;	/* Interrupt counters		*/
	struct evcnt	sc_txintrcnt;	/* Interrupt counters		*/
	struct evcnt	sc_txdraincnt;	/* Interrupt counters		*/
	struct evcnt	sc_nobufintrcnt; /* Interrupt counters		*/
	struct evcnt	sc_nointrcnt;	/* Interrupt counters		*/
	struct ethercom sc_ec;		/* Ethernet common part		*/
#define sc_if	sc_ec.ec_if		/* network-visible interface	*/
	bus_space_tag_t sc_iot;
	bus_addr_t	sc_ioh;
	bus_dma_tag_t	sc_dmat;
	struct ze_cdata *sc_zedata;	/* Descriptor struct		*/
	struct ze_cdata *sc_pzedata;	/* Physical address of above	*/
	bus_dmamap_t	sc_cmap;	/* Map for control structures	*/
	struct mbuf*	sc_txmbuf[TXDESCS];
	struct mbuf*	sc_rxmbuf[RXDESCS];
	bus_dmamap_t	sc_xmtmap[TXDESCS];
	bus_dmamap_t	sc_rcvmap[RXDESCS];
	int		sc_intvec;	/* Interrupt vector		*/
	int		sc_nexttx;
	int		sc_txcnt;
	int		sc_inq;
	int		sc_lastack;
	int		sc_nextrx;
	int		sc_setup;	/* Setup packet in queue	*/
	u_int8_t 	sc_enaddr[ETHER_ADDR_LEN];
};

void	sgec_attach(struct ze_softc *);
int	sgec_intr(struct ze_softc *);

#endif /* _DEV_IC_SGECVAR_H_ */
