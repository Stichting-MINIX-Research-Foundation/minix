/*	$NetBSD: mb86960.c,v 1.81 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mb86960.c,v 1.81 2015/04/13 16:33:24 riastradh Exp $");

/*
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 *
 * This version is intended to be a generic template for various
 * MB86960A/MB86965A based Ethernet cards.  It currently supports
 * Fujitsu FMV-180 series (i.e., FMV-181 and FMV-182) and Allied-
 * Telesis AT1700 series and RE2000 series.  There are some
 * unnecessary hooks embedded, which are primarily intended to support
 * other types of Ethernet cards, but the author is not sure whether
 * they are useful.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
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

#include <sys/bus.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

/* Standard driver entry points.  These can be static. */
void	mb86960_init(struct mb86960_softc *);
int	mb86960_ioctl(struct ifnet *, u_long, void *);
void	mb86960_start(struct ifnet *);
void	mb86960_reset(struct mb86960_softc *);
void	mb86960_watchdog(struct ifnet *);

/* Local functions.  Order of declaration is confused.  FIXME. */
int	mb86960_get_packet(struct mb86960_softc *, u_int);
void	mb86960_stop(struct mb86960_softc *);
void	mb86960_tint(struct mb86960_softc *, uint8_t);
void	mb86960_rint(struct mb86960_softc *, uint8_t);
static inline
void	mb86960_xmit(struct mb86960_softc *);
void	mb86960_write_mbufs(struct mb86960_softc *, struct mbuf *);
static inline
void	mb86960_droppacket(struct mb86960_softc *);
void	mb86960_getmcaf(struct ethercom *, uint8_t *);
void	mb86960_setmode(struct mb86960_softc *);
void	mb86960_loadmar(struct mb86960_softc *);

int	mb86960_mediachange(struct ifnet *);
void	mb86960_mediastatus(struct ifnet *, struct ifmediareq *);

#if FE_DEBUG >= 1
void	mb86960_dump(int, struct mb86960_softc *);
#endif

void
mb86960_attach(struct mb86960_softc *sc, uint8_t *myea)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Register values which depend on board design. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	if ((sc->sc_flags & FE_FLAGS_MB86960) != 0)
		sc->proto_dlcr7 |= FE_D7_ED_TEST; /* XXX */
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as following defaults:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * These values except TXBSIZE should be modified as per
	 * sc_flags which is set in MD attachments, because they
	 * are hard-wired on the board. Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB |
	    FE_D6_BBW_BYTE | FE_D6_SRAM_100ns;
	if (sc->sc_flags & FE_FLAGS_SBW_BYTE)
		sc->proto_dlcr6 |= FE_D6_SBW_BYTE;
	if (sc->sc_flags & FE_FLAGS_SRAM_150ns)
		sc->proto_dlcr6 &= ~FE_D6_SRAM_100ns;

	/*
	 * Minimum initialization of the hardware.
	 * We write into registers; hope I/O ports have no
	 * overlap with other boards.
	 */

	/* Initialize 86960. */
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "ethernet address shouldn't be NULL\n");
		panic("NULL ethernet address");
	}
#endif
	memcpy(sc->sc_enaddr, myea, sizeof(sc->sc_enaddr));

	/* Disable all interrupts. */
	bus_space_write_1(bst, bsh, FE_DLCR2, 0);
	bus_space_write_1(bst, bsh, FE_DLCR3, 0);
}

/*
 * Install interface into kernel networking data structures
 */
void
mb86960_config(struct mb86960_softc *sc, int *media, int nmedia, int defmedia)
{
	cfdata_t cf = device_cfdata(sc->sc_dev);
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int i;

	/* Stop the 86960. */
	mb86960_stop(sc);

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = mb86960_start;
	ifp->if_ioctl = mb86960_ioctl;
	ifp->if_watchdog = mb86960_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: mb86960_config()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

#if FE_SINGLE_TRANSMISSION
	/* Override txb config to allocate minimum. */
	sc->proto_dlcr6 &= ~FE_D6_TXBSIZ;
	sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
#endif

	/* Modify hardware config if it is requested. */
	if ((cf->cf_flags & FE_FLAGS_OVERRIDE_DLCR6) != 0)
		sc->proto_dlcr6 = cf->cf_flags & FE_FLAGS_DLCR6_VALUE;

	/* Find TX buffer size, based on the hardware dependent proto. */
	switch (sc->proto_dlcr6 & FE_D6_TXBSIZ) {
	case FE_D6_TXBSIZ_2x2KB:
		sc->txb_size = 2048;
		break;
	case FE_D6_TXBSIZ_2x4KB:
		sc->txb_size = 4096;
		break;
	case FE_D6_TXBSIZ_2x8KB:
		sc->txb_size = 8192;
		break;
	default:
		/* Oops, we can't work with single buffer configuration. */
#if FE_DEBUG >= 2
		log(LOG_WARNING, "%s: strange TXBSIZ config; fixing\n",
		    device_xname(sc->sc_dev));
#endif
		sc->proto_dlcr6 &= ~FE_D6_TXBSIZ;
		sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
		sc->txb_size = 2048;
		break;
	}

	/* Initialize media goo. */
	ifmedia_init(&sc->sc_media, 0, mb86960_mediachange,
	    mb86960_mediastatus);
	if (media != NULL) {
		for (i = 0; i < nmedia; i++)
			ifmedia_add(&sc->sc_media, media[i], 0, NULL);
		ifmedia_set(&sc->sc_media, defmedia);
	} else {
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	}

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	/* Print additional info when attached. */
	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(sc->sc_enaddr));

#if FE_DEBUG >= 3
	{
		int buf, txb, bbw, sbw, ram;

		buf = txb = bbw = sbw = ram = -1;
		switch (sc->proto_dlcr6 & FE_D6_BUFSIZ) {
		case FE_D6_BUFSIZ_8KB:
			buf = 8;
			break;
		case FE_D6_BUFSIZ_16KB:
			buf = 16;
			break;
		case FE_D6_BUFSIZ_32KB:
			buf = 32;
			break;
		case FE_D6_BUFSIZ_64KB:
			buf = 64;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_TXBSIZ) {
		case FE_D6_TXBSIZ_2x2KB:
			txb = 2;
			break;
		case FE_D6_TXBSIZ_2x4KB:
			txb = 4;
			break;
		case FE_D6_TXBSIZ_2x8KB:
			txb = 8;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_BBW) {
		case FE_D6_BBW_BYTE:
			bbw = 8;
			break;
		case FE_D6_BBW_WORD:
			bbw = 16;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_SBW) {
		case FE_D6_SBW_BYTE:
			sbw = 8;
			break;
		case FE_D6_SBW_WORD:
			sbw = 16;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_SRAM) {
		case FE_D6_SRAM_100ns:
			ram = 100;
			break;
		case FE_D6_SRAM_150ns:
			ram = 150;
			break;
		}
		aprint_debug_dev(sc->sc_dev,
		    "SRAM %dKB %dbit %dns, TXB %dKBx2, %dbit I/O\n",
		    buf, bbw, ram, txb, sbw);
	}
#endif

	/* The attach is successful. */
	sc->sc_stat |= FE_STAT_ATTACHED;
}

/*
 * Media change callback.
 */
int
mb86960_mediachange(struct ifnet *ifp)
{
	struct mb86960_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return (*sc->sc_mediachange)(sc);
	return 0;
}

/*
 * Media status callback.
 */
void
mb86960_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mb86960_softc *sc = ifp->if_softc;

	if ((sc->sc_stat & FE_STAT_ENABLED) == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
}

/*
 * Reset interface.
 */
void
mb86960_reset(struct mb86960_softc *sc)
{
	int s;

	s = splnet();
	mb86960_stop(sc);
	mb86960_init(sc);
	splx(s);
}

/*
 * Stop everything on the interface.
 *
 * All buffered packets, both transmitting and receiving,
 * if any, will be lost by stopping the interface.
 */
void
mb86960_stop(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: top of mb86960_stop()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Disable interrupts. */
	bus_space_write_1(bst, bsh, FE_DLCR2, 0x00);
	bus_space_write_1(bst, bsh, FE_DLCR3, 0x00);

	/* Stop interface hardware. */
	delay(200);
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Clear all interrupt status. */
	bus_space_write_1(bst, bsh, FE_DLCR0, 0xFF);
	bus_space_write_1(bst, bsh, FE_DLCR1, 0xFF);

	/* Put the chip in stand-by mode. */
	delay(200);
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_POWER_DOWN);
	delay(200);

	/* MAR loading can be delayed. */
	sc->filter_change = 0;

	/* Call a hook. */
	if (sc->stop_card)
		(*sc->stop_card)(sc);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: end of mb86960_stop()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
mb86960_watchdog(struct ifnet *ifp)
{
	struct mb86960_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
#if FE_DEBUG >= 3
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Record how many packets are lost by this accident. */
	sc->sc_ec.ec_if.if_oerrors += sc->txb_sched + sc->txb_count;

	mb86960_reset(sc);
}

/*
 * Drop (skip) a packet from receive buffer in 86960 memory.
 */
static inline void
mb86960_droppacket(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	bus_space_write_1(bst, bsh, FE_BMPR14, FE_B14_FILTER | FE_B14_SKIP);
}

/*
 * Initialize device.
 */
void
mb86960_init(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int i;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: top of mb86960_init()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Reset transmitter flags. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	sc->txb_free = sc->txb_size;
	sc->txb_count = 0;
	sc->txb_sched = 0;

	/* Do any card-specific initialization, if applicable. */
	if (sc->init_card)
		(*sc->init_card)(sc);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after init hook\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/*
	 * Make sure to disable the chip, also.
	 * This may also help re-programming the chip after
	 * hot insertion of PCMCIAs.
	 */
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Power up the chip and select register bank for DLCRs. */
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_DLCR | FE_D7_POWER_UP);
	delay(200);

	/* Feed the station address. */
	bus_space_write_region_1(bst, bsh, FE_DLCR8,
	    sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Select the BMPR bank for runtime register access. */
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);

	/* Initialize registers. */
	bus_space_write_1(bst, bsh, FE_DLCR0, 0xFF);	/* Clear all bits. */
	bus_space_write_1(bst, bsh, FE_DLCR1, 0xFF);	/* ditto. */
	bus_space_write_1(bst, bsh, FE_DLCR2, 0x00);
	bus_space_write_1(bst, bsh, FE_DLCR3, 0x00);
	bus_space_write_1(bst, bsh, FE_DLCR4, sc->proto_dlcr4);
	bus_space_write_1(bst, bsh, FE_DLCR5, sc->proto_dlcr5);
	bus_space_write_1(bst, bsh, FE_BMPR10, 0x00);
	bus_space_write_1(bst, bsh, FE_BMPR11, FE_B11_CTRL_SKIP);
	bus_space_write_1(bst, bsh, FE_BMPR12, 0x00);
	bus_space_write_1(bst, bsh, FE_BMPR13, sc->proto_bmpr13);
	bus_space_write_1(bst, bsh, FE_BMPR14, FE_B14_FILTER);
	bus_space_write_1(bst, bsh, FE_BMPR15, 0x00);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: just before enabling DLC\n",
	    device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Enable interrupts. */
	bus_space_write_1(bst, bsh, FE_DLCR2, FE_TMASK);
	bus_space_write_1(bst, bsh, FE_DLCR3, FE_RMASK);

	/* Enable transmitter and receiver. */
	delay(200);
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_ENABLE);
	delay(200);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: just after enabling DLC\n",
	    device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/*
	 * Make sure to empty the receive buffer.
	 *
	 * This may be redundant, but *if* the receive buffer were full
	 * at this point, the driver would hang.  I have experienced
	 * some strange hangups just after UP.  I hope the following
	 * code solve the problem.
	 *
	 * I have changed the order of hardware initialization.
	 * I think the receive buffer cannot have any packets at this
	 * point in this version.  The following code *must* be
	 * redundant now.  FIXME.
	 */
	for (i = 0; i < FE_MAX_RECV_COUNT; i++) {
		if (bus_space_read_1(bst, bsh, FE_DLCR5) & FE_D5_BUFEMP)
			break;
		mb86960_droppacket(sc);
	}
#if FE_DEBUG >= 1
	if (i >= FE_MAX_RECV_COUNT)
		log(LOG_ERR, "%s: cannot empty receive buffer\n",
		    device_xname(sc->sc_dev));
#endif
#if FE_DEBUG >= 3
	if (i < FE_MAX_RECV_COUNT)
		log(LOG_INFO, "%s: receive buffer emptied (%d)\n",
		    device_xname(sc->sc_dev), i);
#endif

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after ERB loop\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Do we need this here? */
	bus_space_write_1(bst, bsh, FE_DLCR0, 0xFF);	/* Clear all bits. */
	bus_space_write_1(bst, bsh, FE_DLCR1, 0xFF);	/* ditto. */

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after FIXME\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* Set 'running' flag. */
	ifp->if_flags |= IFF_RUNNING;

	/*
	 * At this point, the interface is runnung properly,
	 * except that it receives *no* packets.  we then call
	 * mb86960_setmode() to tell the chip what packets to be
	 * received, based on the if_flags and multicast group
	 * list.  It completes the initialization process.
	 */
	mb86960_setmode(sc);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after setmode\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/* ...and attempt to start output. */
	mb86960_start(ifp);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: end of mb86960_init()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif
}

/*
 * This routine actually starts the transmission on the interface
 */
static inline void
mb86960_xmit(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/*
	 * Set a timer just in case we never hear from the board again.
	 * We use longer timeout for multiple packet transmission.
	 * I'm not sure this timer value is appropriate.  FIXME.
	 */
	sc->sc_ec.ec_if.if_timer = 1 + sc->txb_count;

	/* Update txb variables. */
	sc->txb_sched = sc->txb_count;
	sc->txb_count = 0;
	sc->txb_free = sc->txb_size;

#if FE_DELAYED_PADDING
	/* Omit the postponed padding process. */
	sc->txb_padding = 0;
#endif

	/* Start transmitter, passing packets in TX buffer. */
	bus_space_write_1(bst, bsh, FE_BMPR10, sc->txb_sched | FE_B10_START);
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
mb86960_start(struct ifnet *ifp)
{
	struct mb86960_softc *sc = ifp->if_softc;
	struct mbuf *m;

#if FE_DEBUG >= 1
	/* Just a sanity check. */
	if ((sc->txb_count == 0) != (sc->txb_free == sc->txb_size)) {
		/*
		 * Txb_count and txb_free co-works to manage the
		 * transmission buffer.  Txb_count keeps track of the
		 * used potion of the buffer, while txb_free does unused
		 * potion.  So, as long as the driver runs properly,
		 * txb_count is zero if and only if txb_free is same
		 * as txb_size (which represents whole buffer.)
		 */
		log(LOG_ERR, "%s: inconsistent txb variables (%d, %d)\n",
		    device_xname(sc->sc_dev), sc->txb_count, sc->txb_free);
		/*
		 * So, what should I do, then?
		 *
		 * We now know txb_count and txb_free contradicts.  We
		 * cannot, however, tell which is wrong.  More
		 * over, we cannot peek 86960 transmission buffer or
		 * reset the transmission buffer.  (In fact, we can
		 * reset the entire interface.  I don't want to do it.)
		 *
		 * If txb_count is incorrect, leaving it as is will cause
		 * sending of garbage after the next interrupt.  We have to
		 * avoid it.  Hence, we reset the txb_count here.  If
		 * txb_free was incorrect, resetting txb_count just loose
		 * some packets.  We can live with it.
		 */
		sc->txb_count = 0;
	}
#endif

#if FE_DEBUG >= 1
	/*
	 * First, see if there are buffered packets and an idle
	 * transmitter - should never happen at this point.
	 */
	if ((sc->txb_count > 0) && (sc->txb_sched == 0)) {
		log(LOG_ERR, "%s: transmitter idle with %d buffered packets\n",
		    device_xname(sc->sc_dev), sc->txb_count);
		mb86960_xmit(sc);
	}
#endif

	/*
	 * Stop accepting more transmission packets temporarily, when
	 * a filter change request is delayed.  Updating the MARs on
	 * 86960 flushes the transmisstion buffer, so it is delayed
	 * until all buffered transmission packets have been sent
	 * out.
	 */
	if (sc->filter_change) {
		/*
		 * Filter change request is delayed only when the DLC is
		 * working.  DLC soon raise an interrupt after finishing
		 * the work.
		 */
		goto indicate_active;
	}

	for (;;) {
		/*
		 * See if there is room to put another packet in the buffer.
		 * We *could* do better job by peeking the send queue to
		 * know the length of the next packet.  Current version just
		 * tests against the worst case (i.e., longest packet).  FIXME.
		 *
		 * When adding the packet-peek feature, don't forget adding a
		 * test on txb_count against QUEUEING_MAX.
		 * There is a little chance the packet count exceeds
		 * the limit.  Assume transmission buffer is 8KB (2x8KB
		 * configuration) and an application sends a bunch of small
		 * (i.e., minimum packet sized) packets rapidly.  An 8KB
		 * buffer can hold 130 blocks of 62 bytes long...
		 */
		if (sc->txb_free <
		    (ETHER_MAX_LEN - ETHER_CRC_LEN) + FE_TXLEN_SIZE) {
			/* No room. */
			goto indicate_active;
		}

#if FE_SINGLE_TRANSMISSION
		if (sc->txb_count > 0) {
			/* Just one packet per a transmission buffer. */
			goto indicate_active;
		}
#endif

		/*
		 * Get the next mbuf chain for a packet to send.
		 */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0) {
			/* No more packets to send. */
			goto indicate_inactive;
		}

		/* Tap off here if there is a BPF listener. */
		bpf_mtap(ifp, m);

		/*
		 * Copy the mbuf chain into the transmission buffer.
		 * txb_* variables are updated as necessary.
		 */
		mb86960_write_mbufs(sc, m);

		m_freem(m);

		/* Start transmitter if it's idle. */
		if (sc->txb_sched == 0)
			mb86960_xmit(sc);
	}

indicate_inactive:
	/*
	 * We are using the !OACTIVE flag to indicate to
	 * the outside world that we can accept an
	 * additional packet rather than that the
	 * transmitter is _actually_ active.  Indeed, the
	 * transmitter may be active, but if we haven't
	 * filled all the buffers with data then we still
	 * want to accept more.
	 */
	ifp->if_flags &= ~IFF_OACTIVE;
	return;

indicate_active:
	/*
	 * The transmitter is active, and there are no room for
	 * more outgoing packets in the transmission buffer.
	 */
	ifp->if_flags |= IFF_OACTIVE;
	return;
}

/*
 * Transmission interrupt handler
 * The control flow of this function looks silly.  FIXME.
 */
void
mb86960_tint(struct mb86960_softc *sc, uint8_t tstat)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int left;
	int col;

	/*
	 * Handle "excessive collision" interrupt.
	 */
	if (tstat & FE_D0_COLL16) {
		/*
		 * Find how many packets (including this collided one)
		 * are left unsent in transmission buffer.
		 */
		left = bus_space_read_1(bst, bsh, FE_BMPR10);

#if FE_DEBUG >= 2
		log(LOG_WARNING, "%s: excessive collision (%d/%d)\n",
		    device_xname(sc->sc_dev), left, sc->txb_sched);
#endif
#if FE_DEBUG >= 3
		mb86960_dump(LOG_INFO, sc);
#endif

		/*
		 * Update statistics.
		 */
		ifp->if_collisions += 16;
		ifp->if_oerrors++;
		ifp->if_opackets += sc->txb_sched - left;

		/*
		 * Collision statistics has been updated.
		 * Clear the collision flag on 86960 now to avoid confusion.
		 */
		bus_space_write_1(bst, bsh, FE_DLCR0, FE_D0_COLLID);

		/*
		 * Restart transmitter, skipping the
		 * collided packet.
		 *
		 * We *must* skip the packet to keep network running
		 * properly.  Excessive collision error is an
		 * indication of the network overload.  If we
		 * tried sending the same packet after excessive
		 * collision, the network would be filled with
		 * out-of-time packets.  Packets belonging
		 * to reliable transport (such as TCP) are resent
		 * by some upper layer.
		 */
		bus_space_write_1(bst, bsh, FE_BMPR11,
		    FE_B11_CTRL_SKIP | FE_B11_MODE1);
		sc->txb_sched = left - 1;
	}

	/*
	 * Handle "transmission complete" interrupt.
	 */
	if (tstat & FE_D0_TXDONE) {
		/*
		 * Add in total number of collisions on last
		 * transmission.  We also clear "collision occurred" flag
		 * here.
		 *
		 * 86960 has a design flow on collision count on multiple
		 * packet transmission.  When we send two or more packets
		 * with one start command (that's what we do when the
		 * transmission queue is clauded), 86960 informs us number
		 * of collisions occurred on the last packet on the
		 * transmission only.  Number of collisions on previous
		 * packets are lost.  I have told that the fact is clearly
		 * stated in the Fujitsu document.
		 *
		 * I considered not to mind it seriously.  Collision
		 * count is not so important, anyway.  Any comments?  FIXME.
		 */

		if (bus_space_read_1(bst, bsh, FE_DLCR0) & FE_D0_COLLID) {
			/* Clear collision flag. */
			bus_space_write_1(bst, bsh, FE_DLCR0, FE_D0_COLLID);

			/* Extract collision count from 86960. */
			col = bus_space_read_1(bst, bsh, FE_DLCR4) & FE_D4_COL;
			if (col == 0) {
				/*
				 * Status register indicates collisions,
				 * while the collision count is zero.
				 * This can happen after multiple packet
				 * transmission, indicating that one or more
				 * previous packet(s) had been collided.
				 *
				 * Since the accurate number of collisions
				 * has been lost, we just guess it as 1;
				 * Am I too optimistic?  FIXME.
				 */
				col = 1;
			} else
				col >>= FE_D4_COL_SHIFT;
			ifp->if_collisions += col;
#if FE_DEBUG >= 4
			log(LOG_WARNING, "%s: %d collision%s (%d)\n",
			    device_xname(sc->sc_dev), col, col == 1 ? "" : "s",
			    sc->txb_sched);
#endif
		}

		/*
		 * Update total number of successfully
		 * transmitted packets.
		 */
		ifp->if_opackets += sc->txb_sched;
		sc->txb_sched = 0;
	}

	if (sc->txb_sched == 0) {
		/*
		 * The transmitter is no more active.
		 * Reset output active flag and watchdog timer.
		 */
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;

		/*
		 * If more data is ready to transmit in the buffer, start
		 * transmitting them.  Otherwise keep transmitter idle,
		 * even if more data is queued.  This gives receive
		 * process a slight priority.
		 */
		if (sc->txb_count > 0)
			mb86960_xmit(sc);
	}
}

/*
 * Ethernet interface receiver interrupt.
 */
void
mb86960_rint(struct mb86960_softc *sc, uint8_t rstat)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	u_int status, len;
	int i;

	/*
	 * Update statistics if this interrupt is caused by an error.
	 */
	if (rstat & (FE_D1_OVRFLO | FE_D1_CRCERR | FE_D1_ALGERR |
	    FE_D1_SRTPKT)) {
#if FE_DEBUG >= 3
		char sbuf[sizeof(FE_D1_ERRBITS) + 64];

		snprintb(sbuf, sizeof(sbuf), FE_D1_ERRBITS, rstat);
		log(LOG_WARNING, "%s: receive error: %s\n",
		    device_xname(sc->sc_dev), sbuf);
#endif
		ifp->if_ierrors++;
	}

	/*
	 * MB86960 has a flag indicating "receive queue empty."
	 * We just loop checking the flag to pull out all received
	 * packets.
	 *
	 * We limit the number of iterrations to avoid infinite loop.
	 * It can be caused by a very slow CPU (some broken
	 * peripheral may insert incredible number of wait cycles)
	 * or, worse, by a broken MB86960 chip.
	 */
	for (i = 0; i < FE_MAX_RECV_COUNT; i++) {
		/* Stop the iterration if 86960 indicates no packets. */
		if (bus_space_read_1(bst, bsh, FE_DLCR5) & FE_D5_BUFEMP)
			break;

		/*
		 * Extract receive packet status from the receive
		 * packet header.
		 */
		if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
			status = bus_space_read_1(bst, bsh, FE_BMPR8);
			(void)bus_space_read_1(bst, bsh, FE_BMPR8);
		} else
			status = bus_space_read_2(bst, bsh, FE_BMPR8);

#if FE_DEBUG >= 4
		log(LOG_INFO, "%s: receive status = %02x\n",
		    device_xname(sc->sc_dev), status);
#endif

		/*
		 * If there was an error, update statistics and drop
		 * the packet, unless the interface is in promiscuous
		 * mode.
		 */
		if ((status & FE_RXSTAT_GOODPKT) == 0) {
			if ((ifp->if_flags & IFF_PROMISC) == 0) {
				ifp->if_ierrors++;
				mb86960_droppacket(sc);
				continue;
			}
		}

		/*
		 * Extract the packet length from the receive packet header.
		 * It is a sum of a header (14 bytes) and a payload.
		 * CRC has been stripped off by the 86960.
		 */
		if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
			len  = bus_space_read_1(bst, bsh, FE_BMPR8);
			len |= bus_space_read_1(bst, bsh, FE_BMPR8) << 8;
		} else
			len = bus_space_read_2(bst, bsh, FE_BMPR8);

		/*
		 * MB86965 checks the packet length and drop big packet
		 * before passing it to us.  There are no chance we can
		 * get [crufty] packets.  Hence, if the length exceeds
		 * the specified limit, it means some serious failure,
		 * such as out-of-sync on receive buffer management.
		 *
		 * Is this statement true?  FIXME.
		 */
		if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN) ||
		    len < ETHER_HDR_LEN) {
#if FE_DEBUG >= 2
			log(LOG_WARNING,
			    "%s: received a %s packet? (%u bytes)\n",
			    device_xname(sc->sc_dev),
			    len < ETHER_HDR_LEN ? "partial" : "big", len);
#endif
			ifp->if_ierrors++;
			mb86960_droppacket(sc);
			continue;
		}

		/*
		 * Check for a short (RUNT) packet.  We *do* check
		 * but do nothing other than print a message.
		 * Short packets are illegal, but does nothing bad
		 * if it carries data for upper layer.
		 */
#if FE_DEBUG >= 2
		if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
			log(LOG_WARNING,
			    "%s: received a short packet? (%u bytes)\n",
			    device_xname(sc->sc_dev), len);
		}
#endif

		/*
		 * Go get a packet.
		 */
		if (mb86960_get_packet(sc, len) == 0) {
			/* Skip a packet, updating statistics. */
#if FE_DEBUG >= 2
			log(LOG_WARNING,
			    "%s: out of mbufs; dropping packet (%u bytes)\n",
			    device_xname(sc->sc_dev), len);
#endif
			ifp->if_ierrors++;
			mb86960_droppacket(sc);

			/*
			 * We stop receiving packets, even if there are
			 * more in the buffer.  We hope we can get more
			 * mbufs next time.
			 */
			return;
		}

		/* Successfully received a packet.  Update stat. */
		ifp->if_ipackets++;
	}
}

/*
 * Ethernet interface interrupt processor
 */
int
mb86960_intr(void *arg)
{
	struct mb86960_softc *sc = arg;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint8_t tstat, rstat;

	if ((sc->sc_stat & FE_STAT_ENABLED) == 0 ||
	    !device_is_active(sc->sc_dev))
		return 0;

#if FE_DEBUG >= 4
	log(LOG_INFO, "%s: mb86960_intr()\n", device_xname(sc->sc_dev));
	mb86960_dump(LOG_INFO, sc);
#endif

	/*
	 * Get interrupt conditions, masking unneeded flags.
	 */
	tstat = bus_space_read_1(bst, bsh, FE_DLCR0) & FE_TMASK;
	rstat = bus_space_read_1(bst, bsh, FE_DLCR1) & FE_RMASK;
	if (tstat == 0 && rstat == 0)
		return 0;

	/*
	 * Loop until there are no more new interrupt conditions.
	 */
	for (;;) {
		/*
		 * Reset the conditions we are acknowledging.
		 */
		bus_space_write_1(bst, bsh, FE_DLCR0, tstat);
		bus_space_write_1(bst, bsh, FE_DLCR1, rstat);

		/*
		 * Handle transmitter interrupts. Handle these first because
		 * the receiver will reset the board under some conditions.
		 */
		if (tstat != 0)
			mb86960_tint(sc, tstat);

		/*
		 * Handle receiver interrupts.
		 */
		if (rstat != 0)
			mb86960_rint(sc, rstat);

		/*
		 * Update the multicast address filter if it is
		 * needed and possible.  We do it now, because
		 * we can make sure the transmission buffer is empty,
		 * and there is a good chance that the receive queue
		 * is empty.  It will minimize the possibility of
		 * packet lossage.
		 */
		if (sc->filter_change &&
		    sc->txb_count == 0 && sc->txb_sched == 0) {
			mb86960_loadmar(sc);
			ifp->if_flags &= ~IFF_OACTIVE;
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver interrupt to give the
		 * receive operation priority.
		 */
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			mb86960_start(ifp);

		if (rstat != 0 || tstat != 0)
			rnd_add_uint32(&sc->rnd_source, rstat + tstat);

		/*
		 * Get interrupt conditions, masking unneeded flags.
		 */
		tstat = bus_space_read_1(bst, bsh, FE_DLCR0) & FE_TMASK;
		rstat = bus_space_read_1(bst, bsh, FE_DLCR1) & FE_RMASK;
		if (tstat == 0 && rstat == 0)
			return 1;
	}
}

/*
 * Process an ioctl request.  This code needs some work - it looks pretty ugly.
 */
int
mb86960_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct mb86960_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: ioctl(%lx)\n", device_xname(sc->sc_dev), cmd);
#endif

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		if ((error = mb86960_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;

		mb86960_init(sc);
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
			mb86960_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			mb86960_disable(sc);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = mb86960_enable(sc)) != 0)
				break;
			mb86960_init(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			mb86960_setmode(sc);
			break;
		case 0:
			break;
		}
#if FE_DEBUG >= 1
		/* "ifconfig fe0 debug" to print register dump. */
		if (ifp->if_flags & IFF_DEBUG) {
			log(LOG_INFO, "%s: SIOCSIFFLAGS(DEBUG)\n",
			    device_xname(sc->sc_dev));
			mb86960_dump(LOG_DEBUG, sc);
		}
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((sc->sc_stat & FE_STAT_ENABLED) == 0) {
			error = EIO;
			break;
		}

		/* Update our multicast list. */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				mb86960_setmode(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return error;
}

/*
 * Retrieve packet from receive buffer and send to the next level up via
 * ether_input(). If there is a BPF listener, give a copy to BPF, too.
 * Returns 0 if success, -1 if error (i.e., mbuf allocation failure).
 */
int
mb86960_get_packet(struct mb86960_softc *sc, u_int len)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct mbuf *m;

	/* Allocate a header mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return 0;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;

	/* The following silliness is to make NFS happy. */
#define	EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define	EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes) to allocate a cluster.  For a
	 * packet of a size between (MHLEN - 2) to (MINCLSIZE - 2),
	 * our code violates the rule...
	 * On the other hand, the current code is short, simple,
	 * and fast, however.  It does no harmful thing, just waists
	 * some memory.  Any comments?  FIXME.
	 */

	/* Attach a cluster if this packet doesn't fit in a normal mbuf. */
	if (len > MHLEN - EOFF) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return 0;
		}
	}

	/*
	 * The following assumes there is room for the ether header in the
	 * header mbuf.
	 */
	m->m_data += EOFF;

	/* Set the length of this packet. */
	m->m_len = len;

	/* Get a packet. */
	if (sc->sc_flags & FE_FLAGS_SBW_BYTE)
		bus_space_read_multi_1(bst, bsh, FE_BMPR8,
		    mtod(m, uint8_t *), len);
	else
		bus_space_read_multi_stream_2(bst, bsh, FE_BMPR8,
		    mtod(m, uint16_t *), (len + 1) >> 1);

	/*
	 * Check if there's a BPF listener on this interface.  If so, hand off
	 * the raw packet to bpf.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);
	return 1;
}

/*
 * Write an mbuf chain to the transmission buffer memory using 16 bit PIO.
 * Returns number of bytes actually written, including length word.
 *
 * If an mbuf chain is too long for an Ethernet frame, it is not sent.
 * Packets shorter than Ethernet minimum are legal, and we pad them
 * before sending out.  An exception is "partial" packets which are
 * shorter than mandatory Ethernet header.
 *
 * I wrote a code for an experimental "delayed padding" technique.
 * When employed, it postpones the padding process for short packets.
 * If xmit() occurred at the moment, the padding process is omitted, and
 * garbages are sent as pad data.  If next packet is stored in the
 * transmission buffer before xmit(), write_mbuf() pads the previous
 * packet before transmitting new packet.  This *may* gain the
 * system performance (slightly).
 */
void
mb86960_write_mbufs(struct mb86960_softc *sc, struct mbuf *m)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int totlen, len;
#if FE_DEBUG >= 2
	struct mbuf *mp;
#endif

#if FE_DELAYED_PADDING
	/* Do the "delayed padding." */
	if (sc->txb_padding > 0) {
		if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
			for (len = sc->txb_padding; len > 0; len--)
				bus_space_write_1(bst, bsh, FE_BMPR8, 0);
		} else {
			for (len = sc->txb_padding >> 1; len > 0; len--)
				bus_space_write_2(bst, bsh, FE_BMPR8, 0);
		}
		sc->txb_padding = 0;
	}
#endif

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m->m_flags & M_PKTHDR) == 0)
	  	panic("mb86960_write_mbufs: no header mbuf");

#if FE_DEBUG >= 2
	/* First, count up the total number of bytes to copy. */
	for (totlen = 0, mp = m; mp != 0; mp = mp->m_next)
		totlen += mp->m_len;
	/* Check if this matches the one in the packet header. */
	if (totlen != m->m_pkthdr.len)
		log(LOG_WARNING, "%s: packet length mismatch? (%d/%d)\n",
		    device_xname(sc->sc_dev), totlen, m->m_pkthdr.len);
#else
	/* Just use the length value in the packet header. */
	totlen = m->m_pkthdr.len;
#endif

#if FE_DEBUG >= 1
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if (totlen > (ETHER_MAX_LEN - ETHER_CRC_LEN) ||
	    totlen < ETHER_HDR_LEN) {
		log(LOG_ERR, "%s: got a %s packet (%u bytes) to send\n",
		    device_xname(sc->sc_dev),
		    totlen < ETHER_HDR_LEN ? "partial" : "big", totlen);
		sc->sc_ec.ec_if.if_oerrors++;
		return;
	}
#endif

	/*
	 * Put the length word for this frame.
	 * Does 86960 accept odd length?  -- Yes.
	 * Do we need to pad the length to minimum size by ourselves?
	 * -- Generally yes.  But for (or will be) the last
	 * packet in the transmission buffer, we can skip the
	 * padding process.  It may gain performance slightly.  FIXME.
	 */
	len = max(totlen, (ETHER_MIN_LEN - ETHER_CRC_LEN));
	if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
		bus_space_write_1(bst, bsh, FE_BMPR8, len);
		bus_space_write_1(bst, bsh, FE_BMPR8, len >> 8);
	} else {
		bus_space_write_2(bst, bsh, FE_BMPR8, len);
		/* roundup packet length since we will use word access */
		totlen = (totlen + 1) & ~1;
	}

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number
	 * if the chip is set in SBW_WORD mode.
	 */
	sc->txb_free -= FE_TXLEN_SIZE +
	    max(totlen, (ETHER_MIN_LEN - ETHER_CRC_LEN));
	sc->txb_count++;

#if FE_DELAYED_PADDING
	/* Postpone the packet padding if necessary. */
	if (totlen < (ETHER_MIN_LEN - ETHER_CRC_LEN))
		sc->txb_padding = (ETHER_MIN_LEN - ETHER_CRC_LEN) - totlen;
#endif

	/*
	 * Transfer the data from mbuf chain to the transmission buffer.
	 * If the MB86960 is configured in word mode, data needs to be
	 * transferred as words, and only words.
	 * So that we require some extra code to patch over odd-length
	 * or unaligned mbufs.
	 */
	if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
		/* It's simple in byte mode. */
		for (; m != NULL; m = m->m_next) {
			if (m->m_len) {
				bus_space_write_multi_1(bst, bsh, FE_BMPR8,
				    mtod(m, uint8_t *), m->m_len);
			}
		}
	} else {
		/* a bit trickier in word mode. */
		uint8_t *data, savebyte[2];
		int leftover;

		leftover = 0;
		savebyte[0] = savebyte[1] = 0;

		for (; m != NULL; m = m->m_next) {
			len = m->m_len;
			if (len == 0)
				continue;
			data = mtod(m, uint8_t *);
			while (len > 0) {
				if (leftover) {
					/*
					 * Data left over (from mbuf or
					 * realignment). Buffer the next
					 * byte, and write it and the
					 * leftover data out.
					 */
					savebyte[1] = *data++;
					len--;
					bus_space_write_stream_2(bst, bsh,
					   FE_BMPR8, *(uint16_t *)savebyte);
					leftover = 0;
				} else if (BUS_SPACE_ALIGNED_POINTER(data,
				    uint16_t) == 0) {
					/*
					 * Unaligned data; buffer the next byte.
					 */
					savebyte[0] = *data++;
					len--;
					leftover = 1;
				} else {
					/*
					 * Aligned data; output contiguous
					 * words as much as we can, then
					 * buffer the remaining byte, if any.
					 */
					leftover = len & 1;
					len &= ~1;
					bus_space_write_multi_stream_2(bst, bsh,
					    FE_BMPR8, (uint16_t *)data,
					    len >> 1);
					data += len;
					if (leftover)
						savebyte[0] = *data++;
					len = 0;
				}
			}
			if (len < 0)
				panic("mb86960_write_mbufs: negative len");
		}
		if (leftover) {
			savebyte[1] = 0;
			bus_space_write_stream_2(bst, bsh, FE_BMPR8,
			    *(uint16_t *)savebyte);
		}
	}
#if FE_DELAYED_PADDING == 0
	/*
	 * Pad the packet to the minimum length if necessary.
	 */
	len = (ETHER_MIN_LEN - ETHER_CRC_LEN) - totlen;
	if (len > 0) {
		if (sc->sc_flags & FE_FLAGS_SBW_BYTE) {
			while (len-- > 0)
				bus_space_write_1(bst, bsh, FE_BMPR8, 0);
		} else {
			len >>= 1;
			while (len-- > 0)
				bus_space_write_2(bst, bsh, FE_BMPR8, 0);
		}
	}
#endif
}

/*
 * Compute the multicast address filter from the
 * list of multicast addresses we need to listen to.
 */
void
mb86960_getmcaf(struct ethercom *ec, uint8_t *af)
{
	struct ifnet *ifp = &ec->ec_if;
	struct ether_multi *enm;
	uint32_t crc;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if ((ifp->if_flags & IFF_PROMISC) != 0)
		goto allmulti;

	memset(af, 0, FE_FILTER_LEN);
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		crc = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 3] |= 1 << (crc & 7);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	memset(af, 0xff, FE_FILTER_LEN);
}

/*
 * Calculate a new "multicast packet filter" and put the 86960
 * receiver in appropriate mode.
 */
void
mb86960_setmode(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int flags = sc->sc_ec.ec_if.if_flags;

	/*
	 * If the interface is not running, we postpone the update
	 * process for receive modes and multicast address filter
	 * until the interface is restarted.  It reduces some
	 * complicated job on maintaining chip states.  (Earlier versions
	 * of this driver had a bug on that point...)
	 *
	 * To complete the trick, mb86960_init() calls mb86960_setmode() after
	 * restarting the interface.
	 */
	if ((flags & IFF_RUNNING) == 0)
		return;

	/*
	 * Promiscuous mode is handled separately.
	 */
	if ((flags & IFF_PROMISC) != 0) {
		/*
		 * Program 86960 to receive all packets on the segment
		 * including those directed to other stations.
		 * Multicast filter stored in MARs are ignored
		 * under this setting, so we don't need to update it.
		 *
		 * Promiscuous mode is used solely by BPF, and BPF only
		 * listens to valid (no error) packets.  So, we ignore
		 * errornous ones even in this mode.
		 */
		bus_space_write_1(bst, bsh, FE_DLCR5,
		    sc->proto_dlcr5 | FE_D5_AFM0 | FE_D5_AFM1);
		sc->filter_change = 0;

#if FE_DEBUG >= 3
		log(LOG_INFO, "%s: promiscuous mode\n",
		    device_xname(sc->sc_dev));
#endif
		return;
	}

	/*
	 * Turn the chip to the normal (non-promiscuous) mode.
	 */
	bus_space_write_1(bst, bsh, FE_DLCR5, sc->proto_dlcr5 | FE_D5_AFM1);

	/*
	 * Find the new multicast filter value.
	 */
	mb86960_getmcaf(&sc->sc_ec, sc->filter);
	sc->filter_change = 1;

#if FE_DEBUG >= 3
	log(LOG_INFO,
	    "%s: address filter: [%02x %02x %02x %02x %02x %02x %02x %02x]\n",
	    device_xname(sc->sc_dev),
	    sc->filter[0], sc->filter[1], sc->filter[2], sc->filter[3],
	    sc->filter[4], sc->filter[5], sc->filter[6], sc->filter[7]);
#endif

	/*
	 * We have to update the multicast filter in the 86960, A.S.A.P.
	 *
	 * Note that the DLC (Data Linc Control unit, i.e. transmitter
	 * and receiver) must be stopped when feeding the filter, and
	 * DLC trashes all packets in both transmission and receive
	 * buffers when stopped.
	 *
	 * ... Are the above sentenses correct?  I have to check the
	 *     manual of the MB86960A.  FIXME.
	 *
	 * To reduce the packet lossage, we delay the filter update
	 * process until buffers are empty.
	 */
	if (sc->txb_sched == 0 && sc->txb_count == 0 &&
	    (bus_space_read_1(bst, bsh, FE_DLCR1) & FE_D1_PKTRDY) == 0) {
		/*
		 * Buffers are (apparently) empty.  Load
		 * the new filter value into MARs now.
		 */
		mb86960_loadmar(sc);
	} else {
		/*
		 * Buffers are not empty.  Mark that we have to update
		 * the MARs.  The new filter will be loaded by mb86960_intr()
		 * later.
		 */
#if FE_DEBUG >= 4
		log(LOG_INFO, "%s: filter change delayed\n",
		    device_xname(sc->sc_dev));
#endif
	}
}

/*
 * Load a new multicast address filter into MARs.
 *
 * The caller must have splnet'ed befor mb86960_loadmar.
 * This function starts the DLC upon return.  So it can be called only
 * when the chip is working, i.e., from the driver's point of view, when
 * a device is RUNNING.  (I mistook the point in previous versions.)
 */
void
mb86960_loadmar(struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Stop the DLC (transmitter and receiver). */
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);

	/* Select register bank 1 for MARs. */
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP);

	/* Copy filter value into the registers. */
	bus_space_write_region_1(bst, bsh, FE_MAR8, sc->filter, FE_FILTER_LEN);

	/* Restore the bank selection for BMPRs (i.e., runtime registers). */
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);

	/* Restart the DLC. */
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_ENABLE);

	/* We have just updated the filter. */
	sc->filter_change = 0;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: address filter changed\n", device_xname(sc->sc_dev));
#endif
}

/*
 * Enable power on the interface.
 */
int
mb86960_enable(struct mb86960_softc *sc)
{

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: mb86960_enable()\n", device_xname(sc->sc_dev));
#endif

	if ((sc->sc_stat & FE_STAT_ENABLED) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return EIO;
		}
	}

	sc->sc_stat |= FE_STAT_ENABLED;
	return 0;
}

/*
 * Disable power on the interface.
 */
void
mb86960_disable(struct mb86960_softc *sc)
{

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: mb86960_disable()\n", device_xname(sc->sc_dev));
#endif

	if ((sc->sc_stat & FE_STAT_ENABLED) != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_stat &= ~FE_STAT_ENABLED;
	}
}

/*
 * mbe_activate:
 *
 *	Handle device activation/deactivation requests.
 */
int
mb86960_activate(device_t self, enum devact act)
{
	struct mb86960_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * mb86960_detach:
 *
 *	Detach a MB86960 interface.
 */
int
mb86960_detach(struct mb86960_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;

	/* Succeed now if there's no work to do. */
	if ((sc->sc_stat & FE_STAT_ATTACHED) == 0)
		return 0;

	/* Delete all media. */
	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);

	/* Unhook the entropy source. */
	rnd_detach_source(&sc->rnd_source);

	ether_ifdetach(ifp);
	if_detach(ifp);

	mb86960_disable(sc);
	return 0;
}

/*
 * Routines to read all bytes from the config EEPROM (93C06) through MB86965A.
 */
void
mb86965_read_eeprom(bus_space_tag_t iot, bus_space_handle_t ioh, uint8_t *data)
{
	int addr, op, bit;
	uint16_t val;

	/* Read bytes from EEPROM; two bytes per an iteration. */
	for (addr = 0; addr < FE_EEPROM_SIZE / 2; addr++) {
		/* Reset the EEPROM interface. */
		bus_space_write_1(iot, ioh, FE_BMPR16, 0x00);
		bus_space_write_1(iot, ioh, FE_BMPR17, 0x00);
		bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);

		/* Send start bit. */
		bus_space_write_1(iot, ioh, FE_BMPR17, FE_B17_DATA);
		FE_EEPROM_DELAY();
		bus_space_write_1(iot, ioh,
		    FE_BMPR16, FE_B16_SELECT | FE_B16_CLOCK);
		FE_EEPROM_DELAY();
		bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);

		/* Send read command and read address. */
		op = 0x80 | addr;	/* READ instruction */
		for (bit = 8; bit > 0; bit--) {
			bus_space_write_1(iot, ioh, FE_BMPR17,
			    (op & (1 << (bit - 1))) ? FE_B17_DATA : 0);
			FE_EEPROM_DELAY();
			bus_space_write_1(iot, ioh,
			    FE_BMPR16, FE_B16_SELECT | FE_B16_CLOCK);
			FE_EEPROM_DELAY();
			bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);
		}
		bus_space_write_1(iot, ioh, FE_BMPR17, 0x00);

		/* Read two bytes in each address */
		val = 0;
		for (bit = 16; bit > 0; bit--) {
			FE_EEPROM_DELAY();
			bus_space_write_1(iot, ioh,
			    FE_BMPR16, FE_B16_SELECT | FE_B16_CLOCK);
			FE_EEPROM_DELAY();
			if (bus_space_read_1(iot, ioh, FE_BMPR17) &
			    FE_B17_DATA)
				val |= 1 << (bit - 1);
			bus_space_write_1(iot, ioh,
			    FE_BMPR16, FE_B16_SELECT);
		}
		data[addr * 2]     = val >> 8;
		data[addr * 2 + 1] = val & 0xff;
	}

	/* Make sure the EEPROM is turned off. */
	bus_space_write_1(iot, ioh, FE_BMPR16, 0);
	bus_space_write_1(iot, ioh, FE_BMPR17, 0);

#if FE_DEBUG >= 3
	/* Report what we got. */
	log(LOG_INFO, "mb86965_read_eeprom: "
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

#if FE_DEBUG >= 1
void
mb86960_dump(int level, struct mb86960_softc *sc)
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	uint8_t save_dlcr7;

	save_dlcr7 = bus_space_read_1(bst, bsh, FE_DLCR7);

	log(level, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_DLCR0),
	    bus_space_read_1(bst, bsh, FE_DLCR1),
	    bus_space_read_1(bst, bsh, FE_DLCR2),
	    bus_space_read_1(bst, bsh, FE_DLCR3),
	    bus_space_read_1(bst, bsh, FE_DLCR4),
	    bus_space_read_1(bst, bsh, FE_DLCR5),
	    bus_space_read_1(bst, bsh, FE_DLCR6),
	    bus_space_read_1(bst, bsh, FE_DLCR7));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_DLCR);
	log(level, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_DLCR8),
	    bus_space_read_1(bst, bsh, FE_DLCR9),
	    bus_space_read_1(bst, bsh, FE_DLCR10),
	    bus_space_read_1(bst, bsh, FE_DLCR11),
	    bus_space_read_1(bst, bsh, FE_DLCR12),
	    bus_space_read_1(bst, bsh, FE_DLCR13),
	    bus_space_read_1(bst, bsh, FE_DLCR14),
	    bus_space_read_1(bst, bsh, FE_DLCR15));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_MAR);
	log(level, "\tMAR  = %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_MAR8),
	    bus_space_read_1(bst, bsh, FE_MAR9),
	    bus_space_read_1(bst, bsh, FE_MAR10),
	    bus_space_read_1(bst, bsh, FE_MAR11),
	    bus_space_read_1(bst, bsh, FE_MAR12),
	    bus_space_read_1(bst, bsh, FE_MAR13),
	    bus_space_read_1(bst, bsh, FE_MAR14),
	    bus_space_read_1(bst, bsh, FE_MAR15));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_BMPR);
	log(level,
	    "\tBMPR = xx xx %02x %02x %02x %02x %02x %02x %02x %02x xx %02x\n",
	    bus_space_read_1(bst, bsh, FE_BMPR10),
	    bus_space_read_1(bst, bsh, FE_BMPR11),
	    bus_space_read_1(bst, bsh, FE_BMPR12),
	    bus_space_read_1(bst, bsh, FE_BMPR13),
	    bus_space_read_1(bst, bsh, FE_BMPR14),
	    bus_space_read_1(bst, bsh, FE_BMPR15),
	    bus_space_read_1(bst, bsh, FE_BMPR16),
	    bus_space_read_1(bst, bsh, FE_BMPR17),
	    bus_space_read_1(bst, bsh, FE_BMPR19));

	bus_space_write_1(bst, bsh, FE_DLCR7, save_dlcr7);
}
#endif

