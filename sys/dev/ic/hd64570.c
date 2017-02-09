/*	$NetBSD: hd64570.c,v 1.47 2014/06/05 23:48:16 rmind Exp $	*/

/*
 * Copyright (c) 1999 Christian E. Hopps
 * Copyright (c) 1998 Vixie Enterprises
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Vixie Enterprises nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY VIXIE ENTERPRISES AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL VIXIE ENTERPRISES OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for Vixie Enterprises by Michael Graff
 * <explorer@flame.org>.  To learn more about Vixie Enterprises, see
 * ``http://www.vix.com''.
 */

/*
 * TODO:
 *
 *	o  teach the receive logic about errors, and about long frames that
 *         span more than one input buffer.  (Right now, receive/transmit is
 *	   limited to one descriptor's buffer space, which is MTU + 4 bytes.
 *	   This is currently 1504, which is large enough to hold the HDLC
 *	   header and the packet itself.  Packets which are too long are
 *	   silently dropped on transmit and silently dropped on receive.
 *	o  write code to handle the msci interrupts, needed only for CD
 *	   and CTS changes.
 *	o  consider switching back to a "queue tx with DMA active" model which
 *	   should help sustain outgoing traffic
 *	o  through clever use of bus_dma*() functions, it should be possible
 *	   to map the mbuf's data area directly into a descriptor transmit
 *	   buffer, removing the need to allocate extra memory.  If, however,
 *	   we run out of descriptors for this, we will need to then allocate
 *	   one large mbuf, copy the fragmented chain into it, and put it onto
 *	   a single descriptor.
 *	o  use bus_dmamap_sync() with the right offset and lengths, rather
 *	   than cheating and always sync'ing the whole region.
 *
 *	o  perhaps allow rx and tx to be in more than one page
 *	   if not using DMA.  currently the assumption is that
 *	   rx uses a page and tx uses a page.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hd64570.c,v 1.47 2014/06/05 23:48:16 rmind Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#endif
#endif

#include <net/bpf.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/hd64570reg.h>
#include <dev/ic/hd64570var.h>

#define SCA_DEBUG_RX		0x0001
#define SCA_DEBUG_TX		0x0002
#define SCA_DEBUG_CISCO		0x0004
#define SCA_DEBUG_DMA		0x0008
#define SCA_DEBUG_RXPKT		0x0010
#define SCA_DEBUG_TXPKT		0x0020
#define SCA_DEBUG_INTR		0x0040
#define SCA_DEBUG_CLOCK		0x0080

#if 0
#define SCA_DEBUG_LEVEL	( 0xFFFF )
#else
#define SCA_DEBUG_LEVEL 0
#endif

u_int32_t sca_debug = SCA_DEBUG_LEVEL;

#if SCA_DEBUG_LEVEL > 0
#define SCA_DPRINTF(l, x) do { \
	if ((l) & sca_debug) \
		printf x;\
	} while (0)
#else
#define SCA_DPRINTF(l, x)
#endif

#if 0
#define SCA_USE_FASTQ		/* use a split queue, one for fast traffic */
#endif

static inline void msci_write_1(sca_port_t *, u_int, u_int8_t);
static inline u_int8_t msci_read_1(sca_port_t *, u_int);

static inline void dmac_write_1(sca_port_t *, u_int, u_int8_t);
static inline void dmac_write_2(sca_port_t *, u_int, u_int16_t);
static inline u_int8_t dmac_read_1(sca_port_t *, u_int);
static inline u_int16_t dmac_read_2(sca_port_t *, u_int);

static	void sca_msci_init(struct sca_softc *, sca_port_t *);
static	void sca_dmac_init(struct sca_softc *, sca_port_t *);
static void sca_dmac_rxinit(sca_port_t *);

static	int sca_dmac_intr(sca_port_t *, u_int8_t);
static	int sca_msci_intr(sca_port_t *, u_int8_t);

static	void sca_get_packets(sca_port_t *);
static	int sca_frame_avail(sca_port_t *);
static	void sca_frame_process(sca_port_t *);
static	void sca_frame_read_done(sca_port_t *);

static	void sca_port_starttx(sca_port_t *);

static	void sca_port_up(sca_port_t *);
static	void sca_port_down(sca_port_t *);

static	int sca_output(struct ifnet *, struct mbuf *, const struct sockaddr *,
			    struct rtentry *);
static	int sca_ioctl(struct ifnet *, u_long, void *);
static	void sca_start(struct ifnet *);
static	void sca_watchdog(struct ifnet *);

static struct mbuf *sca_mbuf_alloc(struct sca_softc *, void *, u_int);

#if SCA_DEBUG_LEVEL > 0
static	void sca_frame_print(sca_port_t *, sca_desc_t *, u_int8_t *);
#endif


#define	sca_read_1(sc, reg)		(sc)->sc_read_1(sc, reg)
#define	sca_read_2(sc, reg)		(sc)->sc_read_2(sc, reg)
#define	sca_write_1(sc, reg, val)	(sc)->sc_write_1(sc, reg, val)
#define	sca_write_2(sc, reg, val)	(sc)->sc_write_2(sc, reg, val)

#define	sca_page_addr(sc, addr)	((bus_addr_t)(u_long)(addr) & (sc)->scu_pagemask)

static inline void
msci_write_1(sca_port_t *scp, u_int reg, u_int8_t val)
{
	sca_write_1(scp->sca, scp->msci_off + reg, val);
}

static inline u_int8_t
msci_read_1(sca_port_t *scp, u_int reg)
{
	return sca_read_1(scp->sca, scp->msci_off + reg);
}

static inline void
dmac_write_1(sca_port_t *scp, u_int reg, u_int8_t val)
{
	sca_write_1(scp->sca, scp->dmac_off + reg, val);
}

static inline void
dmac_write_2(sca_port_t *scp, u_int reg, u_int16_t val)
{
	sca_write_2(scp->sca, scp->dmac_off + reg, val);
}

static inline u_int8_t
dmac_read_1(sca_port_t *scp, u_int reg)
{
	return sca_read_1(scp->sca, scp->dmac_off + reg);
}

static inline u_int16_t
dmac_read_2(sca_port_t *scp, u_int reg)
{
	return sca_read_2(scp->sca, scp->dmac_off + reg);
}

#if SCA_DEBUG_LEVEL > 0
/*
 * read the chain pointer
 */
static inline u_int16_t
sca_desc_read_chainp(struct sca_softc *sc, struct sca_desc *dp)
{
	if (sc->sc_usedma)
		return ((dp)->sd_chainp);
	return (bus_space_read_2(sc->scu_memt, sc->scu_memh,
	    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_chainp)));
}
#endif

/*
 * write the chain pointer
 */
static inline void
sca_desc_write_chainp(struct sca_softc *sc, struct sca_desc *dp, u_int16_t cp)
{
	if (sc->sc_usedma)
		(dp)->sd_chainp = cp;
	else
		bus_space_write_2(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp)
		    + offsetof(struct sca_desc, sd_chainp), cp);
}

#if SCA_DEBUG_LEVEL > 0
/*
 * read the buffer pointer
 */
static inline u_int32_t
sca_desc_read_bufp(struct sca_softc *sc, struct sca_desc *dp)
{
	u_int32_t address;

	if (sc->sc_usedma)
		address = dp->sd_bufp | dp->sd_hbufp << 16;
	else {
		address = bus_space_read_2(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_bufp));
		address |= bus_space_read_1(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp)
		    + offsetof(struct sca_desc, sd_hbufp)) << 16;
	}
	return (address);
}
#endif

/*
 * write the buffer pointer
 */
static inline void
sca_desc_write_bufp(struct sca_softc *sc, struct sca_desc *dp, u_int32_t bufp)
{
	if (sc->sc_usedma) {
		dp->sd_bufp = bufp & 0xFFFF;
		dp->sd_hbufp = (bufp & 0x00FF0000) >> 16;
	} else {
		bus_space_write_2(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_bufp),
		    bufp & 0xFFFF);
		bus_space_write_1(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_hbufp),
		    (bufp & 0x00FF0000) >> 16);
	}
}

/*
 * read the buffer length
 */
static inline u_int16_t
sca_desc_read_buflen(struct sca_softc *sc, struct sca_desc *dp)
{
	if (sc->sc_usedma)
		return ((dp)->sd_buflen);
	return (bus_space_read_2(sc->scu_memt, sc->scu_memh,
	    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_buflen)));
}

/*
 * write the buffer length
 */
static inline void
sca_desc_write_buflen(struct sca_softc *sc, struct sca_desc *dp, u_int16_t len)
{
	if (sc->sc_usedma)
		(dp)->sd_buflen = len;
	else
		bus_space_write_2(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp)
		    + offsetof(struct sca_desc, sd_buflen), len);
}

/*
 * read the descriptor status
 */
static inline u_int8_t
sca_desc_read_stat(struct sca_softc *sc, struct sca_desc *dp)
{
	if (sc->sc_usedma)
		return ((dp)->sd_stat);
	return (bus_space_read_1(sc->scu_memt, sc->scu_memh,
	    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_stat)));
}

/*
 * write the descriptor status
 */
static inline void
sca_desc_write_stat(struct sca_softc *sc, struct sca_desc *dp, u_int8_t stat)
{
	if (sc->sc_usedma)
		(dp)->sd_stat = stat;
	else
		bus_space_write_1(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, dp) + offsetof(struct sca_desc, sd_stat),
		    stat);
}

void
sca_init(struct sca_softc *sc)
{
	/*
	 * Do a little sanity check:  check number of ports.
	 */
	if (sc->sc_numports < 1 || sc->sc_numports > 2)
		panic("sca can\'t handle more than 2 or less than 1 ports");

	/*
	 * disable DMA and MSCI interrupts
	 */
	sca_write_1(sc, SCA_DMER, 0);
	sca_write_1(sc, SCA_IER0, 0);
	sca_write_1(sc, SCA_IER1, 0);
	sca_write_1(sc, SCA_IER2, 0);

	/*
	 * configure interrupt system
	 */
	sca_write_1(sc, SCA_ITCR,
	    SCA_ITCR_INTR_PRI_MSCI | SCA_ITCR_ACK_NONE | SCA_ITCR_VOUT_IVR);
#if 0
	/* these are for the intrerrupt ack cycle which we don't use */
	sca_write_1(sc, SCA_IVR, 0x40);
	sca_write_1(sc, SCA_IMVR, 0x40);
#endif

	/*
	 * set wait control register to zero wait states
	 */
	sca_write_1(sc, SCA_PABR0, 0);
	sca_write_1(sc, SCA_PABR1, 0);
	sca_write_1(sc, SCA_WCRL, 0);
	sca_write_1(sc, SCA_WCRM, 0);
	sca_write_1(sc, SCA_WCRH, 0);

	/*
	 * disable DMA and reset status
	 */
	sca_write_1(sc, SCA_PCR, SCA_PCR_PR2);

	/*
	 * disable transmit DMA for all channels
	 */
	sca_write_1(sc, SCA_DSR0 + SCA_DMAC_OFF_0, 0);
	sca_write_1(sc, SCA_DCR0 + SCA_DMAC_OFF_0, SCA_DCR_ABRT);
	sca_write_1(sc, SCA_DSR1 + SCA_DMAC_OFF_0, 0);
	sca_write_1(sc, SCA_DCR1 + SCA_DMAC_OFF_0, SCA_DCR_ABRT);
	sca_write_1(sc, SCA_DSR0 + SCA_DMAC_OFF_1, 0);
	sca_write_1(sc, SCA_DCR0 + SCA_DMAC_OFF_1, SCA_DCR_ABRT);
	sca_write_1(sc, SCA_DSR1 + SCA_DMAC_OFF_1, 0);
	sca_write_1(sc, SCA_DCR1 + SCA_DMAC_OFF_1, SCA_DCR_ABRT);

	/*
	 * enable DMA based on channel enable flags for each channel
	 */
	sca_write_1(sc, SCA_DMER, SCA_DMER_EN);

	/*
	 * Should check to see if the chip is responding, but for now
	 * assume it is.
	 */
}

/*
 * initialize the port and attach it to the networking layer
 */
void
sca_port_attach(struct sca_softc *sc, u_int port)
{
	struct timeval now;
	sca_port_t *scp = &sc->sc_ports[port];
	struct ifnet *ifp;
	static u_int ntwo_unit = 0;

	scp->sca = sc;  /* point back to the parent */

	scp->sp_port = port;

	if (port == 0) {
		scp->msci_off = SCA_MSCI_OFF_0;
		scp->dmac_off = SCA_DMAC_OFF_0;
		if(sc->sc_parent != NULL)
			ntwo_unit = device_unit(sc->sc_parent) * 2 + 0;
		else
			ntwo_unit = 0;	/* XXX */
	} else {
		scp->msci_off = SCA_MSCI_OFF_1;
		scp->dmac_off = SCA_DMAC_OFF_1;
		if(sc->sc_parent != NULL)
			ntwo_unit = device_unit(sc->sc_parent) * 2 + 1;
		else
			ntwo_unit = 1;	/* XXX */
	}

	sca_msci_init(sc, scp);
	sca_dmac_init(sc, scp);

	/*
	 * attach to the network layer
	 */
	ifp = &scp->sp_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "ntwo%d", ntwo_unit);
	ifp->if_softc = scp;
	ifp->if_mtu = SCA_MTU;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_type = IFT_PTPSERIAL;
	ifp->if_hdrlen = HDLC_HDRLEN;
	ifp->if_ioctl = sca_ioctl;
	ifp->if_output = sca_output;
	ifp->if_watchdog = sca_watchdog;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	scp->linkq.ifq_maxlen = 5; /* if we exceed this we are hosed already */
#ifdef SCA_USE_FASTQ
	scp->fastq.ifq_maxlen = IFQ_MAXLEN;
#endif
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);
	bpf_attach(ifp, DLT_HDLC, HDLC_HDRLEN);

	if (sc->sc_parent == NULL)
		printf("%s: port %d\n", ifp->if_xname, port);
	else
		printf("%s at %s port %d\n",
		       ifp->if_xname, device_xname(sc->sc_parent), port);

	/*
	 * reset the last seen times on the cisco keepalive protocol
	 */
	getmicrotime(&now);
	scp->cka_lasttx = now.tv_usec;
	scp->cka_lastrx = 0;
}

#if 0
/*
 * returns log2(div), sets 'tmc' for the required freq 'hz'
 */
static u_int8_t
sca_msci_get_baud_rate_values(u_int32_t hz, u_int8_t *tmcp)
{
	u_int32_t tmc, div;
	u_int32_t clock;

	/* clock hz = (chipclock / tmc) / 2^(div); */
	/*
	 * TD == tmc * 2^(n)
	 *
	 * note:
	 * 1 <= TD <= 256		TD is inc of 1
	 * 2 <= TD <= 512		TD is inc of 2
	 * 4 <= TD <= 1024		TD is inc of 4
	 * ...
	 * 512 <= TD <= 256*512		TD is inc of 512
	 *
	 * so note there are overlaps.  We lose prec
	 * as div increases so we wish to minize div.
	 *
	 * basically we want to do
	 *
	 * tmc = chip / hz, but have tmc <= 256
	 */

	/* assume system clock is 9.8304MHz or 9830400Hz */
	clock = clock = 9830400 >> 1;

	/* round down */
	div = 0;
	while ((tmc = clock / hz) > 256 || (tmc == 256 && (clock / tmc) > hz)) {
		clock >>= 1;
		div++;
	}
	if (clock / tmc > hz)
		tmc++;
	if (!tmc)
		tmc = 1;

	if (div > SCA_RXS_DIV_512) {
		/* set to maximums */
		div = SCA_RXS_DIV_512;
		tmc = 0;
	}

	*tmcp = (tmc & 0xFF);	/* 0 == 256 */
	return (div & 0xFF);
}
#endif

/*
 * initialize the port's MSCI
 */
static void
sca_msci_init(struct sca_softc *sc, sca_port_t *scp)
{
	/* reset the channel */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RESET);

	msci_write_1(scp, SCA_MD00,
		     (  SCA_MD0_CRC_1
		      | SCA_MD0_CRC_CCITT
		      | SCA_MD0_CRC_ENABLE
		      | SCA_MD0_MODE_HDLC));
#if 0
	/* immediately send receive reset so the above takes */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RXRESET);
#endif

	msci_write_1(scp, SCA_MD10, SCA_MD1_NOADDRCHK);
	msci_write_1(scp, SCA_MD20,
		     (SCA_MD2_DUPLEX | SCA_MD2_ADPLLx8 | SCA_MD2_NRZ));

	/* be safe and do it again */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RXRESET);

	/* setup underrun and idle control, and initial RTS state */
	msci_write_1(scp, SCA_CTL0,
	     (SCA_CTL_IDLC_PATTERN
	     | SCA_CTL_UDRNC_AFTER_FCS
	     | SCA_CTL_RTS_LOW));

	/* reset the transmitter */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_TXRESET);

	/*
	 * set the clock sources
	 */
	msci_write_1(scp, SCA_RXS0, scp->sp_rxs);
	msci_write_1(scp, SCA_TXS0, scp->sp_txs);
	msci_write_1(scp, SCA_TMC0, scp->sp_tmc);

	/* set external clock generate as requested */
	sc->sc_clock_callback(sc->sc_aux, scp->sp_port, scp->sp_eclock);

	/*
	 * XXX don't pay attention to CTS or CD changes right now.  I can't
	 * simulate one, and the transmitter will try to transmit even if
	 * CD isn't there anyway, so nothing bad SHOULD happen.
	 */
#if 0
	msci_write_1(scp, SCA_IE00, 0);
	msci_write_1(scp, SCA_IE10, 0); /* 0x0c == CD and CTS changes only */
#else
	/* this would deliver transmitter underrun to ST1/ISR1 */
	msci_write_1(scp, SCA_IE10, SCA_ST1_UDRN);
	msci_write_1(scp, SCA_IE00, SCA_ST0_TXINT);
#endif
	msci_write_1(scp, SCA_IE20, 0);

	msci_write_1(scp, SCA_FIE0, 0);

	msci_write_1(scp, SCA_SA00, 0);
	msci_write_1(scp, SCA_SA10, 0);

	msci_write_1(scp, SCA_IDL0, 0x7e);

	msci_write_1(scp, SCA_RRC0, 0x0e);
	/* msci_write_1(scp, SCA_TRC00, 0x10); */
	/*
	 * the correct values here are important for avoiding underruns
	 * for any value less than or equal to TRC0 txrdy is activated
	 * which will start the dmac transfer to the fifo.
	 * for buffer size >= TRC1 + 1 txrdy is cleared which will stop DMA.
	 *
	 * thus if we are using a very fast clock that empties the fifo
	 * quickly, delays in the dmac starting to fill the fifo can
	 * lead to underruns so we want a fairly full fifo to still
	 * cause the dmac to start.  for cards with on board ram this
	 * has no effect on system performance.  For cards that DMA
	 * to/from system memory it will cause more, shorter,
	 * bus accesses rather than fewer longer ones.
	 */
	msci_write_1(scp, SCA_TRC00, 0x00);
	msci_write_1(scp, SCA_TRC10, 0x1f);
}

/*
 * Take the memory for the port and construct two circular linked lists of
 * descriptors (one tx, one rx) and set the pointers in these descriptors
 * to point to the buffer space for this port.
 */
static void
sca_dmac_init(struct sca_softc *sc, sca_port_t *scp)
{
	sca_desc_t *desc;
	u_int32_t desc_p;
	u_int32_t buf_p;
	int i;

	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam, 0, sc->scu_allocsize,
		    BUS_DMASYNC_PREWRITE);
	else {
		/*
		 * XXX assumes that all tx desc and bufs in same page
		 */
		sc->scu_page_on(sc);
		sc->scu_set_page(sc, scp->sp_txdesc_p);
	}

	desc = scp->sp_txdesc;
	desc_p = scp->sp_txdesc_p;
	buf_p = scp->sp_txbuf_p;
	scp->sp_txcur = 0;
	scp->sp_txinuse = 0;

#ifdef DEBUG
	/* make sure that we won't wrap */
	if ((desc_p & 0xffff0000) !=
	    ((desc_p + sizeof(*desc) * scp->sp_ntxdesc) & 0xffff0000))
		panic("sca: tx descriptors cross architecural boundary");
	if ((buf_p & 0xff000000) !=
	    ((buf_p + SCA_BSIZE * scp->sp_ntxdesc) & 0xff000000))
		panic("sca: tx buffers cross architecural boundary");
#endif

	for (i = 0 ; i < scp->sp_ntxdesc ; i++) {
		/*
		 * desc_p points to the physcial address of the NEXT desc
		 */
		desc_p += sizeof(sca_desc_t);

		sca_desc_write_chainp(sc, desc, desc_p & 0x0000ffff);
		sca_desc_write_bufp(sc, desc, buf_p);
		sca_desc_write_buflen(sc, desc, SCA_BSIZE);
		sca_desc_write_stat(sc, desc, 0);

		desc++;  /* point to the next descriptor */
		buf_p += SCA_BSIZE;
	}

	/*
	 * "heal" the circular list by making the last entry point to the
	 * first.
	 */
	sca_desc_write_chainp(sc, desc - 1, scp->sp_txdesc_p & 0x0000ffff);

	/*
	 * Now, initialize the transmit DMA logic
	 *
	 * CPB == chain pointer base address
	 */
	dmac_write_1(scp, SCA_DSR1, 0);
	dmac_write_1(scp, SCA_DCR1, SCA_DCR_ABRT);
	dmac_write_1(scp, SCA_DMR1, SCA_DMR_TMOD | SCA_DMR_NF);
	/* XXX1
	dmac_write_1(scp, SCA_DIR1,
		     (SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF));
	 */
	dmac_write_1(scp, SCA_DIR1,
		     (SCA_DIR_EOM | SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF));
	dmac_write_1(scp, SCA_CPB1,
		     (u_int8_t)((scp->sp_txdesc_p & 0x00ff0000) >> 16));

	/*
	 * now, do the same thing for receive descriptors
	 *
	 * XXX assumes that all rx desc and bufs in same page
	 */
	if (!sc->sc_usedma)
		sc->scu_set_page(sc, scp->sp_rxdesc_p);

	desc = scp->sp_rxdesc;
	desc_p = scp->sp_rxdesc_p;
	buf_p = scp->sp_rxbuf_p;

#ifdef DEBUG
	/* make sure that we won't wrap */
	if ((desc_p & 0xffff0000) !=
	    ((desc_p + sizeof(*desc) * scp->sp_nrxdesc) & 0xffff0000))
		panic("sca: rx descriptors cross architecural boundary");
	if ((buf_p & 0xff000000) !=
	    ((buf_p + SCA_BSIZE * scp->sp_nrxdesc) & 0xff000000))
		panic("sca: rx buffers cross architecural boundary");
#endif

	for (i = 0 ; i < scp->sp_nrxdesc; i++) {
		/*
		 * desc_p points to the physcial address of the NEXT desc
		 */
		desc_p += sizeof(sca_desc_t);

		sca_desc_write_chainp(sc, desc, desc_p & 0x0000ffff);
		sca_desc_write_bufp(sc, desc, buf_p);
		/* sca_desc_write_buflen(sc, desc, SCA_BSIZE); */
		sca_desc_write_buflen(sc, desc, 0);
		sca_desc_write_stat(sc, desc, 0);

		desc++;  /* point to the next descriptor */
		buf_p += SCA_BSIZE;
	}

	/*
	 * "heal" the circular list by making the last entry point to the
	 * first.
	 */
	sca_desc_write_chainp(sc, desc - 1, scp->sp_rxdesc_p & 0x0000ffff);

	sca_dmac_rxinit(scp);

	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam,
		    0, sc->scu_allocsize, BUS_DMASYNC_POSTWRITE);
	else
		sc->scu_page_off(sc);
}

/*
 * reset and reinitialize the receive DMA logic
 */
static void
sca_dmac_rxinit(sca_port_t *scp)
{
	/*
	 * ... and the receive DMA logic ...
	 */
	dmac_write_1(scp, SCA_DSR0, 0);  /* disable DMA */
	dmac_write_1(scp, SCA_DCR0, SCA_DCR_ABRT);

	dmac_write_1(scp, SCA_DMR0, SCA_DMR_TMOD | SCA_DMR_NF);
	dmac_write_2(scp, SCA_BFLL0, SCA_BSIZE);

	/* reset descriptors to initial state */
	scp->sp_rxstart = 0;
	scp->sp_rxend = scp->sp_nrxdesc - 1;

	/*
	 * CPB == chain pointer base
	 * CDA == current descriptor address
	 * EDA == error descriptor address (overwrite position)
	 *	because cda can't be eda when starting we always
	 *	have a single buffer gap between cda and eda
	 */
	dmac_write_1(scp, SCA_CPB0,
	    (u_int8_t)((scp->sp_rxdesc_p & 0x00ff0000) >> 16));
	dmac_write_2(scp, SCA_CDAL0, (u_int16_t)(scp->sp_rxdesc_p & 0xffff));
	dmac_write_2(scp, SCA_EDAL0, (u_int16_t)
	    (scp->sp_rxdesc_p + (sizeof(sca_desc_t) * scp->sp_rxend)));

	/*
	 * enable receiver DMA
	 */
	dmac_write_1(scp, SCA_DIR0,
		     (SCA_DIR_EOT | SCA_DIR_EOM | SCA_DIR_BOF | SCA_DIR_COF));
	dmac_write_1(scp, SCA_DSR0, SCA_DSR_DE);
}

/*
 * Queue the packet for our start routine to transmit
 */
static int
sca_output(
    struct ifnet *ifp,
    struct mbuf *m,
    const struct sockaddr *dst,
    struct rtentry *rt0)
{
	struct hdlc_header *hdlc;
	struct ifqueue *ifq = NULL;
	int s, error, len;
	short mflags;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	error = 0;

	if ((ifp->if_flags & IFF_UP) != IFF_UP) {
		error = ENETDOWN;
		goto bad;
	}

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	/*
	 * determine address family, and priority for this packet
	 */
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
#ifdef SCA_USE_FASTQ
		if ((mtod(m, struct ip *)->ip_tos & IPTOS_LOWDELAY)
		    == IPTOS_LOWDELAY)
			ifq = &((sca_port_t *)ifp->if_softc)->fastq;
#endif
		/*
		 * Add cisco serial line header. If there is no
		 * space in the first mbuf, allocate another.
		 */
		M_PREPEND(m, sizeof(struct hdlc_header), M_DONTWAIT);
		if (m == 0)
			return (ENOBUFS);
		hdlc = mtod(m, struct hdlc_header *);
		hdlc->h_proto = htons(HDLC_PROTOCOL_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		/*
		 * Add cisco serial line header. If there is no
		 * space in the first mbuf, allocate another.
		 */
		M_PREPEND(m, sizeof(struct hdlc_header), M_DONTWAIT);
		if (m == 0)
			return (ENOBUFS);
		hdlc = mtod(m, struct hdlc_header *);
		hdlc->h_proto = htons(HDLC_PROTOCOL_IPV6);
		break;
#endif
	default:
		printf("%s: address family %d unsupported\n",
		       ifp->if_xname, dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

	/* finish */
	if ((m->m_flags & (M_BCAST | M_MCAST)) != 0)
		hdlc->h_addr = CISCO_MULTICAST;
	else
		hdlc->h_addr = CISCO_UNICAST;
	hdlc->h_resv = 0;

	/*
	 * queue the packet.  If interactive, use the fast queue.
	 */
	mflags = m->m_flags;
	len = m->m_pkthdr.len;
	s = splnet();
	if (ifq != NULL) {
		if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			m_freem(m);
			error = ENOBUFS;
		} else
			IF_ENQUEUE(ifq, m);
	} else
		IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error != 0) {
		splx(s);
		ifp->if_oerrors++;
		ifp->if_collisions++;
		return (error);
	}
	ifp->if_obytes += len;
	if (mflags & M_MCAST)
		ifp->if_omcasts++;

	sca_start(ifp);
	splx(s);

	return (error);

 bad:
	if (m)
		m_freem(m);
	return (error);
}

static int
sca_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifreq *ifr;
	struct ifaddr *ifa;
	int error;
	int s;

	s = splnet();

	ifr = (struct ifreq *)data;
	ifa = (struct ifaddr *)data;
	error = 0;

	switch (cmd) {
	case SIOCINITIFADDR:
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
#endif
#ifdef INET6
		case AF_INET6:
#endif
#if defined(INET) || defined(INET6)
			ifp->if_flags |= IFF_UP;
			sca_port_up(ifp->if_softc);
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFDSTADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			break;
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6)
			break;
#endif
		error = EAFNOSUPPORT;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* XXX need multicast group management code */
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifr->ifr_flags & IFF_UP) {
			ifp->if_flags |= IFF_UP;
			sca_port_up(ifp->if_softc);
		} else {
			ifp->if_flags &= ~IFF_UP;
			sca_port_down(ifp->if_softc);
		}

		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
	}

	splx(s);
	return error;
}

/*
 * start packet transmission on the interface
 *
 * MUST BE CALLED AT splnet()
 */
static void
sca_start(struct ifnet *ifp)
{
	sca_port_t *scp = ifp->if_softc;
	struct sca_softc *sc = scp->sca;
	struct mbuf *m, *mb_head;
	sca_desc_t *desc;
	u_int8_t *buf, stat;
	u_int32_t buf_p;
	int nexttx;
	int trigger_xmit;
	u_int len;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: enter start\n"));

	/*
	 * can't queue when we are full or transmitter is busy
	 */
#ifdef oldcode
	if ((scp->sp_txinuse >= (scp->sp_ntxdesc - 1))
	    || ((ifp->if_flags & IFF_OACTIVE) == IFF_OACTIVE))
		return;
#else
	if (scp->sp_txinuse
	    || ((ifp->if_flags & IFF_OACTIVE) == IFF_OACTIVE))
		return;
#endif
	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: txinuse %d\n", scp->sp_txinuse));

	/*
	 * XXX assume that all tx desc and bufs in same page
	 */
	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam,
		    0, sc->scu_allocsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	else {
		sc->scu_page_on(sc);
		sc->scu_set_page(sc, scp->sp_txdesc_p);
	}

	trigger_xmit = 0;

 txloop:
	IF_DEQUEUE(&scp->linkq, mb_head);
	if (mb_head == NULL)
#ifdef SCA_USE_FASTQ
		IF_DEQUEUE(&scp->fastq, mb_head);
	if (mb_head == NULL)
#endif
		IFQ_DEQUEUE(&ifp->if_snd, mb_head);
	if (mb_head == NULL)
		goto start_xmit;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: got mbuf\n"));
#ifdef oldcode
	if (scp->txinuse != 0) {
		/* Kill EOT interrupts on the previous descriptor. */
		desc = &scp->sp_txdesc[scp->txcur];
		stat = sca_desc_read_stat(sc, desc);
		sca_desc_write_stat(sc, desc, stat & ~SCA_DESC_EOT);

		/* Figure out what the next free descriptor is. */
		nexttx = (scp->sp_txcur + 1) % scp->sp_ntxdesc;
	} else
		nexttx = 0;
#endif	/* oldcode */

	if (scp->sp_txinuse)
		nexttx = (scp->sp_txcur + 1) % scp->sp_ntxdesc;
	else
		nexttx = 0;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: nexttx %d\n", nexttx));

	buf = scp->sp_txbuf + SCA_BSIZE * nexttx;
	buf_p = scp->sp_txbuf_p + SCA_BSIZE * nexttx;

	/* XXX hoping we can delay the desc write till after we don't drop. */
	desc = &scp->sp_txdesc[nexttx];

	/* XXX isn't this set already?? */
	sca_desc_write_bufp(sc, desc, buf_p);
	len = 0;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: buf %x buf_p %x\n", (u_int)buf, buf_p));

#if 0	/* uncomment this for a core in cc1 */
X
#endif
	/*
	 * Run through the chain, copying data into the descriptor as we
	 * go.  If it won't fit in one transmission block, drop the packet.
	 * No, this isn't nice, but most of the time it _will_ fit.
	 */
	for (m = mb_head ; m != NULL ; m = m->m_next) {
		if (m->m_len != 0) {
			len += m->m_len;
			if (len > SCA_BSIZE) {
				m_freem(mb_head);
				goto txloop;
			}
			SCA_DPRINTF(SCA_DEBUG_TX,
			    ("TX: about to mbuf len %d\n", m->m_len));

			if (sc->sc_usedma)
				memcpy(buf, mtod(m, u_int8_t *), m->m_len);
			else
				bus_space_write_region_1(sc->scu_memt,
				    sc->scu_memh, sca_page_addr(sc, buf_p),
				    mtod(m, u_int8_t *), m->m_len);
			buf += m->m_len;
			buf_p += m->m_len;
		}
	}

	/* set the buffer, the length, and mark end of frame and end of xfer */
	sca_desc_write_buflen(sc, desc, len);
	sca_desc_write_stat(sc, desc, SCA_DESC_EOM);

	ifp->if_opackets++;

	/*
	 * Pass packet to bpf if there is a listener.
	 */
	bpf_mtap(ifp, mb_head);

	m_freem(mb_head);

	scp->sp_txcur = nexttx;
	scp->sp_txinuse++;
	trigger_xmit = 1;

	SCA_DPRINTF(SCA_DEBUG_TX,
	    ("TX: inuse %d index %d\n", scp->sp_txinuse, scp->sp_txcur));

	/*
	 * XXX so didn't this used to limit us to 1?! - multi may be untested
	 * sp_ntxdesc used to be hard coded to 2 with claim of a too hard
	 * to find bug
	 */
#ifdef oldcode
	if (scp->sp_txinuse < (scp->sp_ntxdesc - 1))
#endif
	if (scp->sp_txinuse < scp->sp_ntxdesc)
		goto txloop;

 start_xmit:
	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: trigger_xmit %d\n", trigger_xmit));

	if (trigger_xmit != 0) {
		/* set EOT on final descriptor */
		desc = &scp->sp_txdesc[scp->sp_txcur];
		stat = sca_desc_read_stat(sc, desc);
		sca_desc_write_stat(sc, desc, stat | SCA_DESC_EOT);
	}

	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam, 0,
		    sc->scu_allocsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (trigger_xmit != 0)
		sca_port_starttx(scp);

	if (!sc->sc_usedma)
		sc->scu_page_off(sc);
}

static void
sca_watchdog(struct ifnet *ifp)
{
}

int
sca_hardintr(struct sca_softc *sc)
{
	u_int8_t isr0, isr1, isr2;
	int	ret;

	ret = 0;  /* non-zero means we processed at least one interrupt */

	SCA_DPRINTF(SCA_DEBUG_INTR, ("sca_hardintr entered\n"));

	while (1) {
		/*
		 * read SCA interrupts
		 */
		isr0 = sca_read_1(sc, SCA_ISR0);
		isr1 = sca_read_1(sc, SCA_ISR1);
		isr2 = sca_read_1(sc, SCA_ISR2);

		if (isr0 == 0 && isr1 == 0 && isr2 == 0)
			break;

		SCA_DPRINTF(SCA_DEBUG_INTR,
			    ("isr0 = %02x, isr1 = %02x, isr2 = %02x\n",
			     isr0, isr1, isr2));

		/*
		 * check DMAC interrupt
		 */
		if (isr1 & 0x0f)
			ret += sca_dmac_intr(&sc->sc_ports[0],
					     isr1 & 0x0f);

		if (isr1 & 0xf0)
			ret += sca_dmac_intr(&sc->sc_ports[1],
			     (isr1 & 0xf0) >> 4);

		/*
		 * mcsi intterupts
		 */
		if (isr0 & 0x0f)
			ret += sca_msci_intr(&sc->sc_ports[0], isr0 & 0x0f);

		if (isr0 & 0xf0)
			ret += sca_msci_intr(&sc->sc_ports[1],
			    (isr0 & 0xf0) >> 4);

#if 0 /* We don't GET timer interrupts, we have them disabled (msci IE20) */
		if (isr2)
			ret += sca_timer_intr(sc, isr2);
#endif
	}

	return (ret);
}

static int
sca_dmac_intr(sca_port_t *scp, u_int8_t isr)
{
	u_int8_t	 dsr;
	int		 ret;

	ret = 0;

	/*
	 * Check transmit channel
	 */
	if (isr & (SCA_ISR1_DMAC_TX0A | SCA_ISR1_DMAC_TX0B)) {
		SCA_DPRINTF(SCA_DEBUG_INTR,
		    ("TX INTERRUPT port %d\n", scp->sp_port));

		dsr = 1;
		while (dsr != 0) {
			ret++;
			/*
			 * reset interrupt
			 */
			dsr = dmac_read_1(scp, SCA_DSR1);
			dmac_write_1(scp, SCA_DSR1,
				     dsr | SCA_DSR_DEWD);

			/*
			 * filter out the bits we don't care about
			 */
			dsr &= ( SCA_DSR_COF | SCA_DSR_BOF | SCA_DSR_EOT);
			if (dsr == 0)
				break;

			/*
			 * check for counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("%s: TXDMA counter overflow\n",
				       scp->sp_if.if_xname);

				scp->sp_if.if_flags &= ~IFF_OACTIVE;
				scp->sp_txcur = 0;
				scp->sp_txinuse = 0;
			}

			/*
			 * check for buffer overflow
			 */
			if (dsr & SCA_DSR_BOF) {
				printf("%s: TXDMA buffer overflow, cda 0x%04x, eda 0x%04x, cpb 0x%02x\n",
				       scp->sp_if.if_xname,
				       dmac_read_2(scp, SCA_CDAL1),
				       dmac_read_2(scp, SCA_EDAL1),
				       dmac_read_1(scp, SCA_CPB1));

				/*
				 * Yikes.  Arrange for a full
				 * transmitter restart.
				 */
				scp->sp_if.if_flags &= ~IFF_OACTIVE;
				scp->sp_txcur = 0;
				scp->sp_txinuse = 0;
			}

			/*
			 * check for end of transfer, which is not
			 * an error. It means that all data queued
			 * was transmitted, and we mark ourself as
			 * not in use and stop the watchdog timer.
			 */
			if (dsr & SCA_DSR_EOT) {
				SCA_DPRINTF(SCA_DEBUG_TX,
			    ("Transmit completed. cda %x eda %x dsr %x\n",
				    dmac_read_2(scp, SCA_CDAL1),
				    dmac_read_2(scp, SCA_EDAL1),
				    dsr));

				scp->sp_if.if_flags &= ~IFF_OACTIVE;
				scp->sp_txcur = 0;
				scp->sp_txinuse = 0;

				/*
				 * check for more packets
				 */
				sca_start(&scp->sp_if);
			}
		}
	}
	/*
	 * receive channel check
	 */
	if (isr & (SCA_ISR1_DMAC_RX0A | SCA_ISR1_DMAC_RX0B)) {
		SCA_DPRINTF(SCA_DEBUG_INTR, ("RX INTERRUPT port %d\n",
		    (scp == &scp->sca->sc_ports[0] ? 0 : 1)));

		dsr = 1;
		while (dsr != 0) {
			ret++;

			dsr = dmac_read_1(scp, SCA_DSR0);
			dmac_write_1(scp, SCA_DSR0, dsr | SCA_DSR_DEWD);

			/*
			 * filter out the bits we don't care about
			 */
			dsr &= (SCA_DSR_EOM | SCA_DSR_COF
				| SCA_DSR_BOF | SCA_DSR_EOT);
			if (dsr == 0)
				break;

			/*
			 * End of frame
			 */
			if (dsr & SCA_DSR_EOM) {
				SCA_DPRINTF(SCA_DEBUG_RX, ("Got a frame!\n"));

				sca_get_packets(scp);
			}

			/*
			 * check for counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("%s: RXDMA counter overflow\n",
				       scp->sp_if.if_xname);

				sca_dmac_rxinit(scp);
			}

			/*
			 * check for end of transfer, which means we
			 * ran out of descriptors to receive into.
			 * This means the line is much faster than
			 * we can handle.
			 */
			if (dsr & (SCA_DSR_BOF | SCA_DSR_EOT)) {
				printf("%s: RXDMA buffer overflow\n",
				       scp->sp_if.if_xname);

				sca_dmac_rxinit(scp);
			}
		}
	}

	return ret;
}

static int
sca_msci_intr(sca_port_t *scp, u_int8_t isr)
{
	u_int8_t st1, trc0;

	/* get and clear the specific interrupt -- should act on it :)*/
	if ((st1 = msci_read_1(scp, SCA_ST10))) {
		/* clear the interrupt */
		msci_write_1(scp, SCA_ST10, st1);

		if (st1 & SCA_ST1_UDRN) {
			/* underrun -- try to increase ready control */
			trc0 = msci_read_1(scp, SCA_TRC00);
			if (trc0 == 0x1f)
				printf("TX: underrun - fifo depth maxed\n");
			else {
				if ((trc0 += 2) > 0x1f)
					trc0 = 0x1f;
				SCA_DPRINTF(SCA_DEBUG_TX,
				   ("TX: udrn - incr fifo to %d\n", trc0));
				msci_write_1(scp, SCA_TRC00, trc0);
			}
		}
	}
	return (0);
}

static void
sca_get_packets(sca_port_t *scp)
{
	struct sca_softc *sc;

	SCA_DPRINTF(SCA_DEBUG_RX, ("RX: sca_get_packets\n"));

	sc = scp->sca;
	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam,
		    0, sc->scu_allocsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	else {
		/*
		 * XXX this code is unable to deal with rx stuff
		 * in more than 1 page
		 */
		sc->scu_page_on(sc);
		sc->scu_set_page(sc, scp->sp_rxdesc_p);
	}

	/* process as many frames as are available */
	while (sca_frame_avail(scp)) {
		sca_frame_process(scp);
		sca_frame_read_done(scp);
	}

	if (sc->sc_usedma)
		bus_dmamap_sync(sc->scu_dmat, sc->scu_dmam,
		    0, sc->scu_allocsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	else
		sc->scu_page_off(sc);
}

/*
 * Starting with the first descriptor we wanted to read into, up to but
 * not including the current SCA read descriptor, look for a packet.
 *
 * must be called at splnet()
 */
static int
sca_frame_avail(sca_port_t *scp)
{
	u_int16_t cda;
	u_int32_t desc_p;	/* physical address (lower 16 bits) */
	sca_desc_t *desc;
	u_int8_t rxstat;
	int cdaidx, toolong;

	/*
	 * Read the current descriptor from the SCA.
	 */
	cda = dmac_read_2(scp, SCA_CDAL0);

	/*
	 * calculate the index of the current descriptor
	 */
	desc_p = (scp->sp_rxdesc_p & 0xFFFF);
	desc_p = cda - desc_p;
	cdaidx = desc_p / sizeof(sca_desc_t);

	SCA_DPRINTF(SCA_DEBUG_RX,
	    ("RX: cda %x desc_p %x cdaidx %u, nrxdesc %d rxstart %d\n",
	    cda, desc_p, cdaidx, scp->sp_nrxdesc, scp->sp_rxstart));

	/* note confusion */
	if (cdaidx >= scp->sp_nrxdesc)
		panic("current descriptor index out of range");

	/* see if we have a valid frame available */
	toolong = 0;
	for (; scp->sp_rxstart != cdaidx; sca_frame_read_done(scp)) {
		/*
		 * We might have a valid descriptor.  Set up a pointer
		 * to the kva address for it so we can more easily examine
		 * the contents.
		 */
		desc = &scp->sp_rxdesc[scp->sp_rxstart];
		rxstat = sca_desc_read_stat(scp->sca, desc);

		SCA_DPRINTF(SCA_DEBUG_RX, ("port %d RX: idx %d rxstat %x\n",
		    scp->sp_port, scp->sp_rxstart, rxstat));

		SCA_DPRINTF(SCA_DEBUG_RX, ("port %d RX: buflen %d\n",
		    scp->sp_port, sca_desc_read_buflen(scp->sca, desc)));

		/*
		 * check for errors
		 */
		if (rxstat & SCA_DESC_ERRORS) {
			/*
			 * consider an error condition the end
			 * of a frame
			 */
			scp->sp_if.if_ierrors++;
			toolong = 0;
			continue;
		}

		/*
		 * if we aren't skipping overlong frames
		 * we are done, otherwise reset and look for
		 * another good frame
		 */
		if (rxstat & SCA_DESC_EOM) {
			if (!toolong)
				return (1);
			toolong = 0;
		} else if (!toolong) {
			/*
			 * we currently don't deal with frames
			 * larger than a single buffer (fixed MTU)
			 */
			scp->sp_if.if_ierrors++;
			toolong = 1;
		}
		SCA_DPRINTF(SCA_DEBUG_RX, ("RX: idx %d no EOM\n",
		    scp->sp_rxstart));
	}

	SCA_DPRINTF(SCA_DEBUG_RX, ("RX: returning none\n"));
	return 0;
}

/*
 * Pass the packet up to the kernel if it is a packet we want to pay
 * attention to.
 *
 * MUST BE CALLED AT splnet()
 */
static void
sca_frame_process(sca_port_t *scp)
{
	pktqueue_t *pktq = NULL;
	struct ifqueue *ifq = NULL;
	struct hdlc_header *hdlc;
	struct cisco_pkt *cisco;
	sca_desc_t *desc;
	struct mbuf *m;
	u_int8_t *bufp;
	u_int16_t len;
	u_int32_t t;
	int isr = 0;

	t = time_uptime * 1000;
	desc = &scp->sp_rxdesc[scp->sp_rxstart];
	bufp = scp->sp_rxbuf + SCA_BSIZE * scp->sp_rxstart;
	len = sca_desc_read_buflen(scp->sca, desc);

	SCA_DPRINTF(SCA_DEBUG_RX,
	    ("RX: desc %lx bufp %lx len %d\n", (bus_addr_t)desc,
	    (bus_addr_t)bufp, len));

#if SCA_DEBUG_LEVEL > 0
	if (sca_debug & SCA_DEBUG_RXPKT)
		sca_frame_print(scp, desc, bufp);
#endif
	/*
	 * skip packets that are too short
	 */
	if (len < sizeof(struct hdlc_header)) {
		scp->sp_if.if_ierrors++;
		return;
	}

	m = sca_mbuf_alloc(scp->sca, bufp, len);
	if (m == NULL) {
		SCA_DPRINTF(SCA_DEBUG_RX, ("RX: no mbuf!\n"));
		return;
	}

	/*
	 * read and then strip off the HDLC information
	 */
	m = m_pullup(m, sizeof(struct hdlc_header));
	if (m == NULL) {
		SCA_DPRINTF(SCA_DEBUG_RX, ("RX: no m_pullup!\n"));
		return;
	}

	bpf_mtap(&scp->sp_if, m);

	scp->sp_if.if_ipackets++;

	hdlc = mtod(m, struct hdlc_header *);
	switch (ntohs(hdlc->h_proto)) {
#ifdef INET
	case HDLC_PROTOCOL_IP:
		SCA_DPRINTF(SCA_DEBUG_RX, ("Received IP packet\n"));
		m->m_pkthdr.rcvif = &scp->sp_if;
		m->m_pkthdr.len -= sizeof(struct hdlc_header);
		m->m_data += sizeof(struct hdlc_header);
		m->m_len -= sizeof(struct hdlc_header);
		pktq = ip_pktq;
		break;
#endif	/* INET */
#ifdef INET6
	case HDLC_PROTOCOL_IPV6:
		SCA_DPRINTF(SCA_DEBUG_RX, ("Received IP packet\n"));
		m->m_pkthdr.rcvif = &scp->sp_if;
		m->m_pkthdr.len -= sizeof(struct hdlc_header);
		m->m_data += sizeof(struct hdlc_header);
		m->m_len -= sizeof(struct hdlc_header);
		pktq = ip6_pktq;
		break;
#endif	/* INET6 */
	case CISCO_KEEPALIVE:
		SCA_DPRINTF(SCA_DEBUG_CISCO,
			    ("Received CISCO keepalive packet\n"));

		if (len < CISCO_PKT_LEN) {
			SCA_DPRINTF(SCA_DEBUG_CISCO,
				    ("short CISCO packet %d, wanted %d\n",
				     len, CISCO_PKT_LEN));
			scp->sp_if.if_ierrors++;
			goto dropit;
		}

		m = m_pullup(m, sizeof(struct cisco_pkt));
		if (m == NULL) {
			SCA_DPRINTF(SCA_DEBUG_RX, ("RX: no m_pullup!\n"));
			return;
		}

		cisco = (struct cisco_pkt *)
		    (mtod(m, u_int8_t *) + HDLC_HDRLEN);
		m->m_pkthdr.rcvif = &scp->sp_if;

		switch (ntohl(cisco->type)) {
		case CISCO_ADDR_REQ:
			printf("Got CISCO addr_req, ignoring\n");
			scp->sp_if.if_ierrors++;
			goto dropit;

		case CISCO_ADDR_REPLY:
			printf("Got CISCO addr_reply, ignoring\n");
			scp->sp_if.if_ierrors++;
			goto dropit;

		case CISCO_KEEPALIVE_REQ:

			SCA_DPRINTF(SCA_DEBUG_CISCO,
				    ("Received KA, mseq %d,"
				     " yseq %d, rel 0x%04x, t0"
				     " %04x, t1 %04x\n",
				     ntohl(cisco->par1), ntohl(cisco->par2),
				     ntohs(cisco->rel), ntohs(cisco->time0),
				     ntohs(cisco->time1)));

			scp->cka_lastrx = ntohl(cisco->par1);
			scp->cka_lasttx++;

			/*
			 * schedule the transmit right here.
			 */
			cisco->par2 = cisco->par1;
			cisco->par1 = htonl(scp->cka_lasttx);
			cisco->time0 = htons((u_int16_t)(t >> 16));
			cisco->time1 = htons((u_int16_t)(t & 0x0000ffff));

			ifq = &scp->linkq;
			if (IF_QFULL(ifq)) {
				IF_DROP(ifq);
				goto dropit;
			}
			IF_ENQUEUE(ifq, m);

			sca_start(&scp->sp_if);

			/* since start may have reset this fix */
			if (!scp->sca->sc_usedma) {
				scp->sca->scu_set_page(scp->sca,
				    scp->sp_rxdesc_p);
				scp->sca->scu_page_on(scp->sca);
			}
			return;
		default:
			SCA_DPRINTF(SCA_DEBUG_CISCO,
				    ("Unknown CISCO keepalive protocol 0x%04x\n",
				     ntohl(cisco->type)));

			scp->sp_if.if_noproto++;
			goto dropit;
		}
		return;
	default:
		SCA_DPRINTF(SCA_DEBUG_RX,
			    ("Unknown/unexpected ethertype 0x%04x\n",
			     ntohs(hdlc->h_proto)));
		scp->sp_if.if_noproto++;
		goto dropit;
	}

	/* Queue the packet */
	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			scp->sp_if.if_iqdrops++;
			goto dropit;
		}
		return;
	}
	if (!IF_QFULL(ifq)) {
		IF_ENQUEUE(ifq, m);
		schednetisr(isr);
	} else {
		IF_DROP(ifq);
		scp->sp_if.if_iqdrops++;
		goto dropit;
	}
	return;
dropit:
	if (m)
		m_freem(m);
	return;
}

#if SCA_DEBUG_LEVEL > 0
/*
 * do a hex dump of the packet received into descriptor "desc" with
 * data buffer "p"
 */
static void
sca_frame_print(sca_port_t *scp, sca_desc_t *desc, u_int8_t *p)
{
	int i;
	int nothing_yet = 1;
	struct sca_softc *sc;
	u_int len;

	sc = scp->sca;
	printf("desc va %p: chainp 0x%x bufp 0x%0x stat 0x%0x len %d\n",
	       desc,
	       sca_desc_read_chainp(sc, desc),
	       sca_desc_read_bufp(sc, desc),
	       sca_desc_read_stat(sc, desc),
	       (len = sca_desc_read_buflen(sc, desc)));

	for (i = 0 ; i < len && i < 256; i++) {
		if (nothing_yet == 1 &&
		    (sc->sc_usedma ? *p
			: bus_space_read_1(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, p))) == 0) {
			p++;
			continue;
		}
		nothing_yet = 0;
		if (i % 16 == 0)
			printf("\n");
		printf("%02x ",
		    (sc->sc_usedma ? *p
		    : bus_space_read_1(sc->scu_memt, sc->scu_memh,
		    sca_page_addr(sc, p))));
		p++;
	}

	if (i % 16 != 1)
		printf("\n");
}
#endif

/*
 * adjust things because we have just read the current starting
 * frame
 *
 * must be called at splnet()
 */
static void
sca_frame_read_done(sca_port_t *scp)
{
	u_int16_t edesc_p;

	/* update where our indicies are */
	scp->sp_rxend = scp->sp_rxstart;
	scp->sp_rxstart = (scp->sp_rxstart + 1) % scp->sp_nrxdesc;

	/* update the error [end] descriptor */
	edesc_p = (u_int16_t)scp->sp_rxdesc_p +
	    (sizeof(sca_desc_t) * scp->sp_rxend);
	dmac_write_2(scp, SCA_EDAL0, edesc_p);
}

/*
 * set a port to the "up" state
 */
static void
sca_port_up(sca_port_t *scp)
{
	struct sca_softc *sc = scp->sca;
	struct timeval now;
#if 0
	u_int8_t ier0, ier1;
#endif

	/*
	 * reset things
	 */
#if 0
	msci_write_1(scp, SCA_CMD0, SCA_CMD_TXRESET);
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RXRESET);
#endif
	/*
	 * clear in-use flag
	 */
	scp->sp_if.if_flags &= ~IFF_OACTIVE;
	scp->sp_if.if_flags |= IFF_RUNNING;

	/*
	 * raise DTR
	 */
	sc->sc_dtr_callback(sc->sc_aux, scp->sp_port, 1);

	/*
	 * raise RTS
	 */
	msci_write_1(scp, SCA_CTL0,
	     (msci_read_1(scp, SCA_CTL0) & ~SCA_CTL_RTS_MASK)
	     | SCA_CTL_RTS_HIGH);

#if 0
	/*
	 * enable interrupts (no timer IER2)
	 */
	ier0 = SCA_IER0_MSCI_RXRDY0 | SCA_IER0_MSCI_TXRDY0
	    | SCA_IER0_MSCI_RXINT0 | SCA_IER0_MSCI_TXINT0;
	ier1 = SCA_IER1_DMAC_RX0A | SCA_IER1_DMAC_RX0B
	    | SCA_IER1_DMAC_TX0A | SCA_IER1_DMAC_TX0B;
	if (scp->sp_port == 1) {
		ier0 <<= 4;
		ier1 <<= 4;
	}
	sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) | ier0);
	sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) | ier1);
#else
	if (scp->sp_port == 0) {
		sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) | 0x0f);
		sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) | 0x0f);
	} else {
		sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) | 0xf0);
		sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) | 0xf0);
	}
#endif

	/*
	 * enable transmit and receive
	 */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_TXENABLE);
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RXENABLE);

	/*
	 * reset internal state
	 */
	scp->sp_txinuse = 0;
	scp->sp_txcur = 0;
	getmicrotime(&now);
	scp->cka_lasttx = now.tv_usec;
	scp->cka_lastrx = 0;
}

/*
 * set a port to the "down" state
 */
static void
sca_port_down(sca_port_t *scp)
{
	struct sca_softc *sc = scp->sca;
#if 0
	u_int8_t ier0, ier1;
#endif

	/*
	 * lower DTR
	 */
	sc->sc_dtr_callback(sc->sc_aux, scp->sp_port, 0);

	/*
	 * lower RTS
	 */
	msci_write_1(scp, SCA_CTL0,
	     (msci_read_1(scp, SCA_CTL0) & ~SCA_CTL_RTS_MASK)
	     | SCA_CTL_RTS_LOW);

	/*
	 * disable interrupts
	 */
#if 0
	ier0 = SCA_IER0_MSCI_RXRDY0 | SCA_IER0_MSCI_TXRDY0
	    | SCA_IER0_MSCI_RXINT0 | SCA_IER0_MSCI_TXINT0;
	ier1 = SCA_IER1_DMAC_RX0A | SCA_IER1_DMAC_RX0B
	    | SCA_IER1_DMAC_TX0A | SCA_IER1_DMAC_TX0B;
	if (scp->sp_port == 1) {
		ier0 <<= 4;
		ier1 <<= 4;
	}
	sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) & ~ier0);
	sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) & ~ier1);
#else
	if (scp->sp_port == 0) {
		sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) & 0xf0);
		sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) & 0xf0);
	} else {
		sca_write_1(sc, SCA_IER0, sca_read_1(sc, SCA_IER0) & 0x0f);
		sca_write_1(sc, SCA_IER1, sca_read_1(sc, SCA_IER1) & 0x0f);
	}
#endif

	/*
	 * disable transmit and receive
	 */
	msci_write_1(scp, SCA_CMD0, SCA_CMD_RXDISABLE);
	msci_write_1(scp, SCA_CMD0, SCA_CMD_TXDISABLE);

	/*
	 * no, we're not in use anymore
	 */
	scp->sp_if.if_flags &= ~(IFF_OACTIVE|IFF_RUNNING);
}

/*
 * disable all DMA and interrupts for all ports at once.
 */
void
sca_shutdown(struct sca_softc *sca)
{
	/*
	 * disable DMA and interrupts
	 */
	sca_write_1(sca, SCA_DMER, 0);
	sca_write_1(sca, SCA_IER0, 0);
	sca_write_1(sca, SCA_IER1, 0);
}

/*
 * If there are packets to transmit, start the transmit DMA logic.
 */
static void
sca_port_starttx(sca_port_t *scp)
{
	u_int32_t	startdesc_p, enddesc_p;
	int enddesc;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: starttx\n"));

	if (((scp->sp_if.if_flags & IFF_OACTIVE) == IFF_OACTIVE)
	    || scp->sp_txinuse == 0)
		return;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: setting oactive\n"));

	scp->sp_if.if_flags |= IFF_OACTIVE;

	/*
	 * We have something to do, since we have at least one packet
	 * waiting, and we are not already marked as active.
	 */
	enddesc = (scp->sp_txcur + 1) % scp->sp_ntxdesc;
	startdesc_p = scp->sp_txdesc_p;
	enddesc_p = scp->sp_txdesc_p + sizeof(sca_desc_t) * enddesc;

	SCA_DPRINTF(SCA_DEBUG_TX, ("TX: start %x end %x\n",
	    startdesc_p, enddesc_p));

	dmac_write_2(scp, SCA_EDAL1, (u_int16_t)(enddesc_p & 0x0000ffff));
	dmac_write_2(scp, SCA_CDAL1,
		     (u_int16_t)(startdesc_p & 0x0000ffff));

	/*
	 * enable the DMA
	 */
	dmac_write_1(scp, SCA_DSR1, SCA_DSR_DE);
}

/*
 * allocate an mbuf at least long enough to hold "len" bytes.
 * If "p" is non-NULL, copy "len" bytes from it into the new mbuf,
 * otherwise let the caller handle copying the data in.
 */
static struct mbuf *
sca_mbuf_alloc(struct sca_softc *sc, void *p, u_int len)
{
	struct mbuf *m;

	/*
	 * allocate an mbuf and copy the important bits of data
	 * into it.  If the packet won't fit in the header,
	 * allocate a cluster for it and store it there.
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	if (len > MHLEN) {
		if (len > MCLBYTES) {
			m_freem(m);
			return NULL;
		}
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return NULL;
		}
	}
	if (p != NULL) {
		/* XXX do we need to sync here? */
		if (sc->sc_usedma)
			memcpy(mtod(m, void *), p, len);
		else
			bus_space_read_region_1(sc->scu_memt, sc->scu_memh,
			    sca_page_addr(sc, p), mtod(m, u_int8_t *), len);
	}
	m->m_len = len;
	m->m_pkthdr.len = len;

	return (m);
}

/*
 * get the base clock
 */
void
sca_get_base_clock(struct sca_softc *sc)
{
	struct timeval btv, ctv, dtv;
	u_int64_t bcnt;
	u_int32_t cnt;
	u_int16_t subcnt;

	/* disable the timer, set prescale to 0 */
	sca_write_1(sc, SCA_TCSR0, 0);
	sca_write_1(sc, SCA_TEPR0, 0);

	/* reset the counter */
	(void)sca_read_1(sc, SCA_TCSR0);
	subcnt = sca_read_2(sc, SCA_TCNTL0);

	/* count to max */
	sca_write_2(sc, SCA_TCONRL0, 0xffff);

	cnt = 0;
	microtime(&btv);
	/* start the timer -- no interrupt enable */
	sca_write_1(sc, SCA_TCSR0, SCA_TCSR_TME);
	for (;;) {
		microtime(&ctv);

		/* end around 3/4 of a second */
		timersub(&ctv, &btv, &dtv);
		if (dtv.tv_usec >= 750000)
			break;

		/* spin */
		while (!(sca_read_1(sc, SCA_TCSR0) & SCA_TCSR_CMF))
			;
		/* reset the timer */
		(void)sca_read_2(sc, SCA_TCNTL0);
		cnt++;
	}

	/* stop the timer */
	sca_write_1(sc, SCA_TCSR0, 0);

	subcnt = sca_read_2(sc, SCA_TCNTL0);
	/* add the slop in and get the total timer ticks */
	cnt = (cnt << 16) | subcnt;

	/* cnt is 1/8 the actual time */
	bcnt = cnt * 8;
	/* make it proportional to 3/4 of a second */
	bcnt *= (u_int64_t)750000;
	bcnt /= (u_int64_t)dtv.tv_usec;
	cnt = bcnt;

	/* make it Hz */
	cnt *= 4;
	cnt /= 3;

	SCA_DPRINTF(SCA_DEBUG_CLOCK,
	    ("sca: unadjusted base %lu Hz\n", (u_long)cnt));

	/*
	 * round to the nearest 200 -- this allows for +-3 ticks error
	 */
	sc->sc_baseclock = ((cnt + 100) / 200) * 200;
}

/*
 * print the information about the clock on the ports
 */
void
sca_print_clock_info(struct sca_softc *sc)
{
	struct sca_port *scp;
	u_int32_t mhz, div;
	int i;

	printf("%s: base clock %d Hz\n", device_xname(sc->sc_parent),
	    sc->sc_baseclock);

	/* print the information about the port clock selection */
	for (i = 0; i < sc->sc_numports; i++) {
		scp = &sc->sc_ports[i];
		mhz = sc->sc_baseclock / (scp->sp_tmc ? scp->sp_tmc : 256);
		div = scp->sp_rxs & SCA_RXS_DIV_MASK;

		printf("%s: rx clock: ", scp->sp_if.if_xname);
		switch (scp->sp_rxs & SCA_RXS_CLK_MASK) {
		case SCA_RXS_CLK_LINE:
			printf("line");
			break;
		case SCA_RXS_CLK_LINE_SN:
			printf("line with noise suppression");
			break;
		case SCA_RXS_CLK_INTERNAL:
			printf("internal %d Hz", (mhz >> div));
			break;
		case SCA_RXS_CLK_ADPLL_OUT:
			printf("adpll using internal %d Hz", (mhz >> div));
			break;
		case SCA_RXS_CLK_ADPLL_IN:
			printf("adpll using line clock");
			break;
		}
		printf("  tx clock: ");
		div = scp->sp_txs & SCA_TXS_DIV_MASK;
		switch (scp->sp_txs & SCA_TXS_CLK_MASK) {
		case SCA_TXS_CLK_LINE:
			printf("line\n");
			break;
		case SCA_TXS_CLK_INTERNAL:
			printf("internal %d Hz\n", (mhz >> div));
			break;
		case SCA_TXS_CLK_RXCLK:
			printf("rxclock\n");
			break;
		}
		if (scp->sp_eclock)
			printf("%s: outputting line clock\n",
			    scp->sp_if.if_xname);
	}
}

