/*	$NetBSD: elink3.c,v 1.136 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1996, 1997 Jonathan Stone <jonathan@NetBSD.org>
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: elink3.c,v 1.136 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
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
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#ifdef DEBUG
int epdebug = 0;
#endif

/*
 * XXX endian workaround for big-endian CPUs  with pcmcia:
 * if stream methods for bus_space_multi are not provided, define them
 * using non-stream bus_space_{read,write}_multi_.
 * Assumes host CPU is same endian-ness as bus.
 */
#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_read_multi_stream_2	bus_space_read_multi_2
#define bus_space_read_multi_stream_4	bus_space_read_multi_4
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_write_multi_stream_4	bus_space_write_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

/*
 * Structure to map media-present bits in boards to ifmedia codes and
 * printable media names. Used for table-driven ifmedia initialization.
 */
struct ep_media {
	int	epm_mpbit;		/* media present bit */
	const char *epm_name;		/* name of medium */
	int	epm_ifmedia;		/* ifmedia word for medium */
	int	epm_epmedia;		/* ELINKMEDIA_* constant */
};

/*
 * Media table for the Demon/Vortex/Boomerang chipsets.
 *
 * Note that MII on the Demon and Vortex (3c59x) indicates an external
 * MII connector (for connecting an external PHY) ... I think.  Treat
 * it as `manual' on these chips.
 *
 * Any Boomerang (3c90x) chips with MII really do have an internal
 * MII and real PHYs attached; no `native' media.
 */
const struct ep_media ep_vortex_media[] = {
	{ ELINK_PCI_10BASE_T,	"10baseT",	IFM_ETHER|IFM_10_T,
	  ELINKMEDIA_10BASE_T },
	{ ELINK_PCI_10BASE_T,	"10baseT-FDX",	IFM_ETHER|IFM_10_T|IFM_FDX,
	  ELINKMEDIA_10BASE_T },
	{ ELINK_PCI_AUI,	"10base5",	IFM_ETHER|IFM_10_5,
	  ELINKMEDIA_AUI },
	{ ELINK_PCI_BNC,	"10base2",	IFM_ETHER|IFM_10_2,
	  ELINKMEDIA_10BASE_2 },
	{ ELINK_PCI_100BASE_TX,	"100baseTX",	IFM_ETHER|IFM_100_TX,
	  ELINKMEDIA_100BASE_TX },
	{ ELINK_PCI_100BASE_TX,	"100baseTX-FDX",IFM_ETHER|IFM_100_TX|IFM_FDX,
	  ELINKMEDIA_100BASE_TX },
	{ ELINK_PCI_100BASE_FX,	"100baseFX",	IFM_ETHER|IFM_100_FX,
	  ELINKMEDIA_100BASE_FX },
	{ ELINK_PCI_100BASE_MII,"manual",	IFM_ETHER|IFM_MANUAL,
	  ELINKMEDIA_MII },
	{ ELINK_PCI_100BASE_T4,	"100baseT4",	IFM_ETHER|IFM_100_T4,
	  ELINKMEDIA_100BASE_T4 },
	{ 0,			NULL,		0,
	  0 },
};

/*
 * Media table for the older 3Com Etherlink III chipset, used
 * in the 3c509, 3c579, and 3c589.
 */
const struct ep_media ep_509_media[] = {
	{ ELINK_W0_CC_UTP,	"10baseT",	IFM_ETHER|IFM_10_T,
	  ELINKMEDIA_10BASE_T },
	{ ELINK_W0_CC_AUI,	"10base5",	IFM_ETHER|IFM_10_5,
	  ELINKMEDIA_AUI },
	{ ELINK_W0_CC_BNC,	"10base2",	IFM_ETHER|IFM_10_2,
	  ELINKMEDIA_10BASE_2 },
	{ 0,			NULL,		0,
	  0 },
};

void	ep_internalconfig(struct ep_softc *sc);
void	ep_vortex_probemedia(struct ep_softc *sc);
void	ep_509_probemedia(struct ep_softc *sc);

static void eptxstat(struct ep_softc *);
static int epstatus(struct ep_softc *);
int	epinit(struct ifnet *);
void	epstop(struct ifnet *, int);
int	epioctl(struct ifnet *, u_long, void *);
void	epstart(struct ifnet *);
void	epwatchdog(struct ifnet *);
void	epreset(struct ep_softc *);
static bool epshutdown(device_t, int);
void	epread(struct ep_softc *);
struct mbuf *epget(struct ep_softc *, int);
void	epmbuffill(void *);
void	epmbufempty(struct ep_softc *);
void	epsetfilter(struct ep_softc *);
void	ep_roadrunner_mii_enable(struct ep_softc *);
void	epsetmedia(struct ep_softc *);

/* ifmedia callbacks */
int	ep_media_change(struct ifnet *ifp);
void	ep_media_status(struct ifnet *ifp, struct ifmediareq *req);

/* MII callbacks */
int	ep_mii_readreg(device_t, int, int);
void	ep_mii_writereg(device_t, int, int, int);
void	ep_statchg(struct ifnet *);

void	ep_tick(void *);

static int epbusyeeprom(struct ep_softc *);
u_int16_t ep_read_eeprom(struct ep_softc *, u_int16_t);
static inline void ep_reset_cmd(struct ep_softc *sc, u_int cmd, u_int arg);
static inline void ep_finish_reset(bus_space_tag_t, bus_space_handle_t);
static inline void ep_discard_rxtop(bus_space_tag_t, bus_space_handle_t);
static inline int ep_w1_reg(struct ep_softc *, int);

/*
 * MII bit-bang glue.
 */
u_int32_t ep_mii_bitbang_read(device_t);
void ep_mii_bitbang_write(device_t, u_int32_t);

const struct mii_bitbang_ops ep_mii_bitbang_ops = {
	ep_mii_bitbang_read,
	ep_mii_bitbang_write,
	{
		PHYSMGMT_DATA,		/* MII_BIT_MDO */
		PHYSMGMT_DATA,		/* MII_BIT_MDI */
		PHYSMGMT_CLK,		/* MII_BIT_MDC */
		PHYSMGMT_DIR,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

/*
 * Some chips (3c515 [Corkscrew] and 3c574 [RoadRunner]) have
 * Window 1 registers offset!
 */
static inline int
ep_w1_reg(struct ep_softc *sc, int reg)
{

	switch (sc->ep_chipset) {
	case ELINK_CHIPSET_CORKSCREW:
		return (reg + 0x10);

	case ELINK_CHIPSET_ROADRUNNER:
		switch (reg) {
		case ELINK_W1_FREE_TX:
		case ELINK_W1_RUNNER_RDCTL:
		case ELINK_W1_RUNNER_WRCTL:
			return (reg);
		}
		return (reg + 0x10);
	}

	return (reg);
}

/*
 * Wait for any pending reset to complete.
 * On newer hardware we could poll SC_COMMAND_IN_PROGRESS,
 * but older hardware doesn't implement it and we must delay.
 */
static inline void
ep_finish_reset(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i;

	for (i = 0; i < 10000; i++) {
		if ((bus_space_read_2(iot, ioh, ELINK_STATUS) &
		    COMMAND_IN_PROGRESS) == 0)
			break;
		DELAY(10);
	}
}

/*
 * Issue a (reset) command, and be sure it has completed.
 * Used for global reset, TX_RESET, RX_RESET.
 */
static inline void
ep_reset_cmd(struct ep_softc *sc, u_int cmd, u_int arg)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_2(iot, ioh, cmd, arg);
	ep_finish_reset(iot, ioh);
}


static inline void
ep_discard_rxtop(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i;

	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_DISCARD_TOP_PACK);

        /*
	 * Spin for about 1 msec, to avoid forcing a DELAY() between
	 * every received packet (adding latency and  limiting pkt-recv rate).
	 * On PCI, at 4 30-nsec PCI bus cycles for a read, 8000 iterations
	 * is about right.
	 */
	for (i = 0; i < 8000; i++) {
		if ((bus_space_read_2(iot, ioh, ELINK_STATUS) &
		    COMMAND_IN_PROGRESS) == 0)
		    return;
	}

	/*  Didn't complete in a hurry. Do DELAY()s. */
	ep_finish_reset(iot, ioh);
}

/*
 * Back-end attach and configure.
 */
int
epconfig(struct ep_softc *sc, u_short chipset, u_int8_t *enaddr)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t i;
	u_int8_t myla[ETHER_ADDR_LEN];

	callout_init(&sc->sc_mii_callout, 0);
	callout_init(&sc->sc_mbuf_callout, 0);

	sc->ep_chipset = chipset;

	/*
	 * We could have been groveling around in other register
	 * windows in the front-end; make sure we're in window 0
	 * to read the EEPROM.
	 */
	GO_WINDOW(0);

	if (enaddr == NULL) {
		/*
		 * Read the station address from the eeprom.
		 */
		for (i = 0; i < ETHER_ADDR_LEN / 2; i++) {
			u_int16_t x = ep_read_eeprom(sc, i);
			myla[(i << 1)] = x >> 8;
			myla[(i << 1) + 1] = x;
		}
		enaddr = myla;
	}

	/*
	 * Vortex-based (3c59x pci,eisa) and Boomerang (3c900) cards
	 * allow FDDI-sized (4500) byte packets.  Commands only take an
	 * 11-bit parameter, and  11 bits isn't enough to hold a full-size
	 * packet length.
	 * Commands to these cards implicitly upshift a packet size
	 * or threshold by 2 bits.
	 * To detect  cards with large-packet support, we probe by setting
	 * the transmit threshold register, then change windows and
	 * read back the threshold register directly, and see if the
	 * threshold value was shifted or not.
	 */
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_TX_AVAIL_THRESH | ELINK_LARGEWIN_PROBE);
	GO_WINDOW(5);
	i = bus_space_read_2(iot, ioh, ELINK_W5_TX_AVAIL_THRESH);
	GO_WINDOW(1);
	switch (i)  {
	case ELINK_LARGEWIN_PROBE:
	case (ELINK_LARGEWIN_PROBE & ELINK_LARGEWIN_MASK):
		sc->ep_pktlenshift = 0;
		break;

	case (ELINK_LARGEWIN_PROBE << 2):
		sc->ep_pktlenshift = 2;
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "wrote 0x%x to TX_AVAIL_THRESH, read back 0x%x. "
		    "Interface disabled\n",
		    ELINK_LARGEWIN_PROBE, (int) i);
		return (1);
	}

	/*
	 * Ensure Tx-available interrupts are enabled for
	 * start the interface.
	 * XXX should be in epinit()?
	 */
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_TX_AVAIL_THRESH | (1600 >> sc->ep_pktlenshift));

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = epstart;
	ifp->if_ioctl = epioctl;
	ifp->if_watchdog = epwatchdog;
	ifp->if_init = epinit;
	ifp->if_stop = epstop;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	/*
	 * Finish configuration:
	 * determine chipset if the front-end couldn't do so,
	 * show board details, set media.
	 */

	/*
	 * Print RAM size.  We also print the Ethernet address in here.
	 * It's extracted from the ifp, so we have to make sure it's
	 * been attached first.
	 */
	ep_internalconfig(sc);
	GO_WINDOW(0);

	/*
	 * Display some additional information, if pertinent.
	 */
	if (sc->ep_flags & ELINK_FLAGS_USEFIFOBUFFER)
		aprint_normal_dev(sc->sc_dev, "RoadRunner FIFO buffer enabled\n");

	/*
	 * Initialize our media structures and MII info.  We'll
	 * probe the MII if we discover that we have one.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = ep_mii_readreg;
	sc->sc_mii.mii_writereg = ep_mii_writereg;
	sc->sc_mii.mii_statchg = ep_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, ep_media_change,
	    ep_media_status);

	/*
	 * All CORKSCREW chips have MII.
	 */
	if (sc->ep_chipset == ELINK_CHIPSET_CORKSCREW)
		sc->ep_flags |= ELINK_FLAGS_MII;

	/*
	 * Now, determine which media we have.
	 */
	switch (sc->ep_chipset) {
	case ELINK_CHIPSET_ROADRUNNER:
		if (sc->ep_flags & ELINK_FLAGS_MII) {
			ep_roadrunner_mii_enable(sc);
			GO_WINDOW(0);
		}
		/* FALLTHROUGH */

	case ELINK_CHIPSET_CORKSCREW:
	case ELINK_CHIPSET_BOOMERANG:
		/*
		 * If the device has MII, probe it.  We won't be using
		 * any `native' media in this case, only PHYs.  If
		 * we don't, just treat the Boomerang like the Vortex.
		 */
		if (sc->ep_flags & ELINK_FLAGS_MII) {
			mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
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
			break;
		}
		/* FALLTHROUGH */

	case ELINK_CHIPSET_VORTEX:
		ep_vortex_probemedia(sc);
		break;

	default:
		ep_509_probemedia(sc);
		break;
	}

	GO_WINDOW(1);		/* Window 1 is operating window */

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	sc->tx_start_thresh = 20;	/* probably a good starting point. */

	/*  Establish callback to reset card when we reboot. */
	if (pmf_device_register1(sc->sc_dev, NULL, NULL, epshutdown))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	ep_reset_cmd(sc, ELINK_COMMAND, RX_RESET);
	ep_reset_cmd(sc, ELINK_COMMAND, TX_RESET);

	/* The attach is successful. */
	sc->sc_flags |= ELINK_FLAGS_ATTACHED;
	return (0);
}


/*
 * Show interface-model-independent info from window 3
 * internal-configuration register.
 */
void
ep_internalconfig(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	u_int config0;
	u_int config1;

	int  ram_size, ram_width, ram_split;
	/*
	 * NVRAM buffer Rx:Tx config names for busmastering cards
	 * (Demon, Vortex, and later).
	 */
	const char *const onboard_ram_config[] = {
		"5:3", "3:1", "1:1", "3:5" };

	GO_WINDOW(3);
	config0 = (u_int)bus_space_read_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG);
	config1 = (u_int)bus_space_read_2(iot, ioh,
	    ELINK_W3_INTERNAL_CONFIG + 2);
	GO_WINDOW(0);

	ram_size  = (config0 & CONFIG_RAMSIZE) >> CONFIG_RAMSIZE_SHIFT;
	ram_width = (config0 & CONFIG_RAMWIDTH) >> CONFIG_RAMWIDTH_SHIFT;

	ram_split  = (config1 & CONFIG_RAMSPLIT) >> CONFIG_RAMSPLIT_SHIFT;

	aprint_normal_dev(sc->sc_dev, "address %s, %dKB %s-wide FIFO, %s Rx:Tx split\n",
	       ether_sprintf(CLLADDR(sc->sc_ethercom.ec_if.if_sadl)),
	       8 << ram_size,
	       (ram_width) ? "word" : "byte",
	       onboard_ram_config[ram_split]);
}


/*
 * Find supported media on 3c509-generation hardware that doesn't have
 * a "reset_options" register in window 3.
 * Use the config_cntrl register  in window 0 instead.
 * Used on original, 10Mbit ISA (3c509), 3c509B, and pre-Demon EISA cards
 * that implement  CONFIG_CTRL.  We don't have a good way to set the
 * default active medium; punt to ifconfig  instead.
 */
void
ep_509_probemedia(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifmedia *ifm = &sc->sc_mii.mii_media;
	u_int16_t ep_w0_config, port;
	const struct ep_media *epm;
	const char *sep = "", *defmedianame = NULL;
	int defmedia = 0;

	GO_WINDOW(0);
	ep_w0_config = bus_space_read_2(iot, ioh, ELINK_W0_CONFIG_CTRL);

	aprint_normal_dev(sc->sc_dev, "");

	/* Sanity check that there are any media! */
	if ((ep_w0_config & ELINK_W0_CC_MEDIAMASK) == 0) {
		aprint_error("no media present!\n");
		ifmedia_add(ifm, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(ifm, IFM_ETHER|IFM_NONE);
		return;
	}

	/*
	 * Get the default media from the EEPROM.
	 */
	port = ep_read_eeprom(sc, EEPROM_ADDR_CFG) >> 14;

#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	for (epm = ep_509_media; epm->epm_name != NULL; epm++) {
		if (ep_w0_config & epm->epm_mpbit) {
			/*
			 * This simple test works because 509 chipsets
			 * don't do full-duplex.
			 */
			if (epm->epm_epmedia == port || defmedia == 0) {
				defmedia = epm->epm_ifmedia;
				defmedianame = epm->epm_name;
			}
			ifmedia_add(ifm, epm->epm_ifmedia, epm->epm_epmedia,
			    NULL);
			PRINT(epm->epm_name);
		}
	}

#undef PRINT

#ifdef DIAGNOSTIC
	if (defmedia == 0)
		panic("ep_509_probemedia: impossible");
#endif

	aprint_normal(" (default %s)\n", defmedianame);
	ifmedia_set(ifm, defmedia);
}

/*
 * Find media present on large-packet-capable elink3 devices.
 * Show onboard configuration of large-packet-capable elink3 devices
 * (Demon, Vortex, Boomerang), which do not implement CONFIG_CTRL in window 0.
 * Use media and card-version info in window 3 instead.
 */
void
ep_vortex_probemedia(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifmedia *ifm = &sc->sc_mii.mii_media;
	const struct ep_media *epm;
	u_int config1;
	int reset_options;
	int default_media;	/* 3-bit encoding of default (EEPROM) media */
	int defmedia = 0;
	const char *sep = "", *defmedianame = NULL;

	GO_WINDOW(3);
	config1 = (u_int)bus_space_read_2(iot, ioh,
	    ELINK_W3_INTERNAL_CONFIG + 2);
	reset_options = (int)bus_space_read_2(iot, ioh, ELINK_W3_RESET_OPTIONS);
	GO_WINDOW(0);

	default_media = (config1 & CONFIG_MEDIAMASK) >> CONFIG_MEDIAMASK_SHIFT;

	aprint_normal_dev(sc->sc_dev, "");

	/* Sanity check that there are any media! */
	if ((reset_options & ELINK_PCI_MEDIAMASK) == 0) {
		aprint_error("no media present!\n");
		ifmedia_add(ifm, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(ifm, IFM_ETHER|IFM_NONE);
		return;
	}

#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	for (epm = ep_vortex_media; epm->epm_name != NULL; epm++) {
		if (reset_options & epm->epm_mpbit) {
			/*
			 * Default media is a little more complicated
			 * on the Vortex.  We support full-duplex which
			 * uses the same reset options bit.
			 *
			 * XXX Check EEPROM for default to FDX?
			 */
			if (epm->epm_epmedia == default_media) {
				if ((epm->epm_ifmedia & IFM_FDX) == 0) {
					defmedia = epm->epm_ifmedia;
					defmedianame = epm->epm_name;
				}
			} else if (defmedia == 0) {
				defmedia = epm->epm_ifmedia;
				defmedianame = epm->epm_name;
			}
			ifmedia_add(ifm, epm->epm_ifmedia, epm->epm_epmedia,
			    NULL);
			PRINT(epm->epm_name);
		}
	}

#undef PRINT

#ifdef DIAGNOSTIC
	if (defmedia == 0)
		panic("ep_vortex_probemedia: impossible");
#endif

	aprint_normal(" (default %s)\n", defmedianame);
	ifmedia_set(ifm, defmedia);
}

/*
 * One second timer, used to tick the MII.
 */
void
ep_tick(void *arg)
{
	struct ep_softc *sc = arg;
	int s;

#ifdef DIAGNOSTIC
	if ((sc->ep_flags & ELINK_FLAGS_MII) == 0)
		panic("ep_tick");
#endif

	if (!device_is_active(sc->sc_dev))
		return;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_mii_callout, hz, ep_tick, sc);
}

/*
 * Bring device up.
 *
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
int
epinit(struct ifnet *ifp)
{
	struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, error;
	const u_int8_t *addr;

	if (!sc->enabled && (error = epenable(sc)) != 0)
		return (error);

	/* Make sure any pending reset has completed before touching board */
	ep_finish_reset(iot, ioh);

	/*
	 * Cancel any pending I/O.
	 */
	epstop(ifp, 0);

	if (sc->bustype != ELINK_BUS_PCI && sc->bustype != ELINK_BUS_EISA
	    && sc->bustype != ELINK_BUS_MCA) {
		GO_WINDOW(0);
		bus_space_write_2(iot, ioh, ELINK_W0_CONFIG_CTRL, 0);
		bus_space_write_2(iot, ioh, ELINK_W0_CONFIG_CTRL,
		    ENABLE_DRQ_IRQ);
	}

	if (sc->bustype == ELINK_BUS_PCMCIA) {
		bus_space_write_2(iot, ioh, ELINK_W0_RESOURCE_CFG, 0x3f00);
	}

	GO_WINDOW(2);
	/* Reload the ether_addr. */
	addr = CLLADDR(ifp->if_sadl);
	for (i = 0; i < 6; i += 2)
		bus_space_write_2(iot, ioh, ELINK_W2_ADDR_0 + i,
		    (addr[i] << 0) | (addr[i + 1] << 8));

	/*
	 * Reset the station-address receive filter.
	 * A bug workaround for busmastering (Vortex, Demon) cards.
	 */
	for (i = 0; i < 6; i += 2)
		bus_space_write_2(iot, ioh, ELINK_W2_RECVMASK_0 + i, 0);

	ep_reset_cmd(sc, ELINK_COMMAND, RX_RESET);
	ep_reset_cmd(sc, ELINK_COMMAND, TX_RESET);

	GO_WINDOW(1);		/* Window 1 is operating window */
	for (i = 0; i < 31; i++)
		(void)bus_space_read_2(iot, ioh,
				       ep_w1_reg(sc, ELINK_W1_TX_STATUS));

	/* Set threshold for Tx-space available interrupt. */
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_TX_AVAIL_THRESH | (1600 >> sc->ep_pktlenshift));

	if (sc->ep_chipset == ELINK_CHIPSET_ROADRUNNER) {
		/*
		 * Enable options in the PCMCIA LAN COR register, via
		 * RoadRunner Window 1.
		 *
		 * XXX MAGIC CONSTANTS!
		 */
		u_int16_t cor;

		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_RDCTL, (1 << 11));

		cor = bus_space_read_2(iot, ioh, 0) & ~0x30;
		if (sc->ep_flags & ELINK_FLAGS_USESHAREDMEM)
			cor |= 0x10;
		if (sc->ep_flags & ELINK_FLAGS_FORCENOWAIT)
			cor |= 0x20;
		bus_space_write_2(iot, ioh, 0, cor);

		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_WRCTL, 0);
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_RDCTL, 0);

		if (sc->ep_flags & ELINK_FLAGS_MII) {
			ep_roadrunner_mii_enable(sc);
			GO_WINDOW(1);
		}
	}

	/* Enable interrupts. */
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_RD_0_MASK | WATCHED_INTERRUPTS);
	bus_space_write_2(iot, ioh, ELINK_COMMAND,
	    SET_INTR_MASK | WATCHED_INTERRUPTS);

	/*
	 * Attempt to get rid of any stray interrupts that occurred during
	 * configuration.  On the i386 this isn't possible because one may
	 * already be queued.  However, a single stray interrupt is
	 * unimportant.
	 */
	bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR | 0xff);

	epsetfilter(sc);
	epsetmedia(sc);

	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_ENABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_ENABLE);

	epmbuffill(sc);

	/* Interface is now `running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->ep_flags & ELINK_FLAGS_MII) {
		/* Start the one second clock. */
		callout_reset(&sc->sc_mii_callout, hz, ep_tick, sc);
	}

	/* Attempt to start output, if any. */
	epstart(ifp);

	return (0);
}


/*
 * Set multicast receive filter.
 * elink3 hardware has no selective multicast filter in hardware.
 * Enable reception of all multicasts and filter in software.
 */
void
epsetfilter(struct ep_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	GO_WINDOW(1);		/* Window 1 is operating window */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ELINK_COMMAND,
	    SET_RX_FILTER | FIL_INDIVIDUAL | FIL_BRDCST |
	    ((ifp->if_flags & IFF_MULTICAST) ? FIL_MULTICAST : 0) |
	    ((ifp->if_flags & IFF_PROMISC) ? FIL_PROMISC : 0));
}

int
ep_media_change(struct ifnet *ifp)
{
	struct ep_softc *sc = ifp->if_softc;

	if (sc->enabled && (ifp->if_flags & IFF_UP) != 0)
		epreset(sc);

	return (0);
}

/*
 * Reset and enable the MII on the RoadRunner.
 */
void
ep_roadrunner_mii_enable(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	GO_WINDOW(3);
	bus_space_write_2(iot, ioh, ELINK_W3_RESET_OPTIONS,
	    ELINK_PCI_100BASE_MII|ELINK_RUNNER_ENABLE_MII);
	delay(1000);
	bus_space_write_2(iot, ioh, ELINK_W3_RESET_OPTIONS,
	    ELINK_PCI_100BASE_MII|ELINK_RUNNER_MII_RESET|
	    ELINK_RUNNER_ENABLE_MII);
	ep_reset_cmd(sc, ELINK_COMMAND, TX_RESET);
	ep_reset_cmd(sc, ELINK_COMMAND, RX_RESET);
	delay(1000);
	bus_space_write_2(iot, ioh, ELINK_W3_RESET_OPTIONS,
	    ELINK_PCI_100BASE_MII|ELINK_RUNNER_ENABLE_MII);
}

/*
 * Set the card to use the specified media.
 */
void
epsetmedia(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* Turn everything off.  First turn off linkbeat and UTP. */
	GO_WINDOW(4);
	bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE, 0x0);

	/* Turn off coax */
	bus_space_write_2(iot, ioh, ELINK_COMMAND, STOP_TRANSCEIVER);
	delay(1000);

	/*
	 * If the device has MII, select it, and then tell the
	 * PHY which media to use.
	 */
	if (sc->ep_flags & ELINK_FLAGS_MII) {
		int config0, config1;

		GO_WINDOW(3);

		if (sc->ep_chipset == ELINK_CHIPSET_ROADRUNNER) {
			int resopt;

			resopt = bus_space_read_2(iot, ioh,
			    ELINK_W3_RESET_OPTIONS);
			bus_space_write_2(iot, ioh, ELINK_W3_RESET_OPTIONS,
			    resopt | ELINK_RUNNER_ENABLE_MII);
		}

		config0 = (u_int)bus_space_read_2(iot, ioh,
		    ELINK_W3_INTERNAL_CONFIG);
		config1 = (u_int)bus_space_read_2(iot, ioh,
		    ELINK_W3_INTERNAL_CONFIG + 2);

		config1 = config1 & ~CONFIG_MEDIAMASK;
		config1 |= (ELINKMEDIA_MII << CONFIG_MEDIAMASK_SHIFT);

		bus_space_write_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG, config0);
		bus_space_write_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG + 2,
		    config1);
		GO_WINDOW(1);	/* back to operating window */

		mii_mediachg(&sc->sc_mii);
		return;
	}

	/*
	 * Now turn on the selected media/transceiver.
	 */
	GO_WINDOW(4);
	switch (IFM_SUBTYPE(sc->sc_mii.mii_media.ifm_cur->ifm_media)) {
	case IFM_10_T:
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    JABBER_GUARD_ENABLE|LINKBEAT_ENABLE);
		break;

	case IFM_10_2:
		bus_space_write_2(iot, ioh, ELINK_COMMAND, START_TRANSCEIVER);
		DELAY(1000);	/* 50ms not enmough? */
		break;

	case IFM_100_TX:
	case IFM_100_FX:
	case IFM_100_T4:		/* XXX check documentation */
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    LINKBEAT_ENABLE);
		DELAY(1000);	/* not strictly necessary? */
		break;

	case IFM_10_5:
		bus_space_write_2(iot, ioh, ELINK_W4_MEDIA_TYPE,
		    SQE_ENABLE);
		DELAY(1000);	/* not strictly necessary? */
		break;

	case IFM_MANUAL:
		/*
		 * Nothing to do here; we are actually enabling the
		 * external PHY on the MII port.
		 */
		break;

	case IFM_NONE:
		printf("%s: interface disabled\n", device_xname(sc->sc_dev));
		return;

	default:
		panic("epsetmedia: impossible");
	}

	/*
	 * Tell the chip which port to use.
	 */
	switch (sc->ep_chipset) {
	case ELINK_CHIPSET_VORTEX:
	case ELINK_CHIPSET_BOOMERANG:
	    {
		int mctl, config0, config1;

		GO_WINDOW(3);
		config0 = (u_int)bus_space_read_2(iot, ioh,
		    ELINK_W3_INTERNAL_CONFIG);
		config1 = (u_int)bus_space_read_2(iot, ioh,
		    ELINK_W3_INTERNAL_CONFIG + 2);

		config1 = config1 & ~CONFIG_MEDIAMASK;
		config1 |= (sc->sc_mii.mii_media.ifm_cur->ifm_data <<
		    CONFIG_MEDIAMASK_SHIFT);

		bus_space_write_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG, config0);
		bus_space_write_2(iot, ioh, ELINK_W3_INTERNAL_CONFIG + 2,
		    config1);

		mctl = bus_space_read_2(iot, ioh, ELINK_W3_MAC_CONTROL);
		if (sc->sc_mii.mii_media.ifm_cur->ifm_media & IFM_FDX)
			mctl |= MAC_CONTROL_FDX;
		else
			mctl &= ~MAC_CONTROL_FDX;
		bus_space_write_2(iot, ioh, ELINK_W3_MAC_CONTROL, mctl);
		break;
	    }
	default:
	    {
		int w0_addr_cfg;

		GO_WINDOW(0);
		w0_addr_cfg = bus_space_read_2(iot, ioh, ELINK_W0_ADDRESS_CFG);
		w0_addr_cfg &= 0x3fff;
		bus_space_write_2(iot, ioh, ELINK_W0_ADDRESS_CFG, w0_addr_cfg |
		    (sc->sc_mii.mii_media.ifm_cur->ifm_data << 14));
		DELAY(1000);
		break;
	    }
	}

	GO_WINDOW(1);		/* Window 1 is operating window */
}

/*
 * Get currently-selected media from card.
 * (if_media callback, may be called before interface is brought up).
 */
void
ep_media_status(struct ifnet *ifp, struct ifmediareq *req)
{
	struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (sc->enabled == 0) {
		req->ifm_active = IFM_ETHER|IFM_NONE;
		req->ifm_status = 0;
		return;
	}

	/*
	 * If we have MII, go ask the PHY what's going on.
	 */
	if (sc->ep_flags & ELINK_FLAGS_MII) {
		mii_pollstat(&sc->sc_mii);
		req->ifm_active = sc->sc_mii.mii_media_active;
		req->ifm_status = sc->sc_mii.mii_media_status;
		return;
	}

	/*
	 * Ok, at this point we claim that our active media is
	 * the currently selected media.  We'll update our status
	 * if our chipset allows us to detect link.
	 */
	req->ifm_active = sc->sc_mii.mii_media.ifm_cur->ifm_media;
	req->ifm_status = 0;

	switch (sc->ep_chipset) {
	case ELINK_CHIPSET_VORTEX:
	case ELINK_CHIPSET_BOOMERANG:
		GO_WINDOW(4);
		req->ifm_status = IFM_AVALID;
		if (bus_space_read_2(iot, ioh, ELINK_W4_MEDIA_TYPE) &
		    LINKBEAT_DETECT)
			req->ifm_status |= IFM_ACTIVE;
		GO_WINDOW(1);	/* back to operating window */
		break;
	}
}



/*
 * Start outputting on the interface.
 * Always called as splnet().
 */
void
epstart(struct ifnet *ifp)
{
	struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct mbuf *m, *m0;
	int sh, len, pad;
	bus_size_t txreg;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

startagain:
	/* Sneak a peek at the next packet */
	IFQ_POLL(&ifp->if_snd, m0);
	if (m0 == 0)
		return;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("epstart: no header mbuf");
	len = m0->m_pkthdr.len;

	pad = (4 - len) & 3;

	/*
	 * The 3c509 automatically pads short packets to minimum ethernet
	 * length, but we drop packets that are too large. Perhaps we should
	 * truncate them instead?
	 */
	if (len + pad > ETHER_MAX_LEN) {
		/* packet is obviously too large: toss it */
		++ifp->if_oerrors;
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		m_freem(m0);
		goto readcheck;
	}

	if (bus_space_read_2(iot, ioh, ep_w1_reg(sc, ELINK_W1_FREE_TX)) <
	    len + pad + 4) {
		bus_space_write_2(iot, ioh, ELINK_COMMAND,
		    SET_TX_AVAIL_THRESH |
		    ((len + pad + 4) >> sc->ep_pktlenshift));
		/* not enough room in FIFO */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	} else {
		bus_space_write_2(iot, ioh, ELINK_COMMAND,
		    SET_TX_AVAIL_THRESH | ELINK_THRESH_DISABLE);
	}

	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)		/* not really needed */
		return;

	bus_space_write_2(iot, ioh, ELINK_COMMAND, SET_TX_START_THRESH |
	    ((len / 4 + sc->tx_start_thresh) /* >> sc->ep_pktlenshift*/));

	bpf_mtap(ifp, m0);

	/*
	 * Do the output at a high interrupt priority level so that an
	 * interrupt from another device won't cause a FIFO underrun.
	 * We choose splsched() since that blocks essentially everything
	 * except for interrupts from serial devices (which typically
	 * lose data if their interrupt isn't serviced fast enough).
	 *
	 * XXX THIS CAN CAUSE CLOCK DRIFT!
	 */
	sh = splsched();

	txreg = ep_w1_reg(sc, ELINK_W1_TX_PIO_WR_1);

	if (sc->ep_flags & ELINK_FLAGS_USEFIFOBUFFER) {
		/*
		 * Prime the FIFO buffer counter (number of 16-bit
		 * words about to be written to the FIFO).
		 *
		 * NOTE: NO OTHER ACCESS CAN BE PERFORMED WHILE THIS
		 * COUNTER IS NON-ZERO!
		 */
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_WRCTL,
		    (len + pad) >> 1);
	}

	bus_space_write_2(iot, ioh, txreg, len);
	bus_space_write_2(iot, ioh, txreg, 0xffff); /* Second is meaningless */
	if (ELINK_IS_BUS_32(sc->bustype)) {
		for (m = m0; m;) {
			if (m->m_len > 3)  {
				/* align our reads from core */
				if (mtod(m, u_long) & 3)  {
					u_long count =
					    4 - (mtod(m, u_long) & 3);
					bus_space_write_multi_1(iot, ioh,
					    txreg, mtod(m, u_int8_t *), count);
					m->m_data =
					    (void *)(mtod(m, u_long) + count);
					m->m_len -= count;
				}
				bus_space_write_multi_stream_4(iot, ioh,
				    txreg, mtod(m, u_int32_t *), m->m_len >> 2);
				m->m_data = (void *)(mtod(m, u_long) +
					(u_long)(m->m_len & ~3));
				m->m_len -= m->m_len & ~3;
			}
			if (m->m_len)  {
				bus_space_write_multi_1(iot, ioh,
				    txreg, mtod(m, u_int8_t *), m->m_len);
			}
			MFREE(m, m0);
			m = m0;
		}
	} else {
		for (m = m0; m;) {
			if (m->m_len > 1)  {
				if (mtod(m, u_long) & 1)  {
					bus_space_write_1(iot, ioh,
					    txreg, *(mtod(m, u_int8_t *)));
					m->m_data =
					    (void *)(mtod(m, u_long) + 1);
					m->m_len -= 1;
				}
				bus_space_write_multi_stream_2(iot, ioh,
				    txreg, mtod(m, u_int16_t *),
				    m->m_len >> 1);
			}
			if (m->m_len & 1)  {
				bus_space_write_1(iot, ioh, txreg,
				     *(mtod(m, u_int8_t *) + m->m_len - 1));
			}
			MFREE(m, m0);
			m = m0;
		}
	}
	while (pad--)
		bus_space_write_1(iot, ioh, txreg, 0);

	splx(sh);

	++ifp->if_opackets;

readcheck:
	if ((bus_space_read_2(iot, ioh, ep_w1_reg(sc, ELINK_W1_RX_STATUS)) &
	    ERR_INCOMPLETE) == 0) {
		/* We received a complete packet. */
		u_int16_t status = bus_space_read_2(iot, ioh, ELINK_STATUS);

		if ((status & INTR_LATCH) == 0) {
			/*
			 * No interrupt, read the packet and continue
			 * Is  this supposed to happen? Is my motherboard
			 * completely busted?
			 */
			epread(sc);
		} else {
			/* Got an interrupt, return so that it gets serviced. */
			return;
		}
	} else {
		/* Check if we are stuck and reset [see XXX comment] */
		if (epstatus(sc)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				    device_xname(sc->sc_dev));
			epreset(sc);
		}
	}

	goto startagain;
}


/*
 * XXX: The 3c509 card can get in a mode where both the fifo status bit
 *	FIFOS_RX_OVERRUN and the status bit ERR_INCOMPLETE are set
 *	We detect this situation and we reset the adapter.
 *	It happens at times when there is a lot of broadcast traffic
 *	on the cable (once in a blue moon).
 */
static int
epstatus(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t fifost;

	/*
	 * Check the FIFO status and act accordingly
	 */
	GO_WINDOW(4);
	fifost = bus_space_read_2(iot, ioh, ELINK_W4_FIFO_DIAG);
	GO_WINDOW(1);

	if (fifost & FIFOS_RX_UNDERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX underrun\n", device_xname(sc->sc_dev));
		epreset(sc);
		return 0;
	}

	if (fifost & FIFOS_RX_STATUS_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX Status overrun\n", device_xname(sc->sc_dev));
		return 1;
	}

	if (fifost & FIFOS_RX_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX overrun\n", device_xname(sc->sc_dev));
		return 1;
	}

	if (fifost & FIFOS_TX_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: TX overrun\n", device_xname(sc->sc_dev));
		epreset(sc);
		return 0;
	}

	return 0;
}


static void
eptxstat(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	/*
	 * We need to read+write TX_STATUS until we get a 0 status
	 * in order to turn off the interrupt flag.
	 */
	while ((i = bus_space_read_2(iot, ioh,
	     ep_w1_reg(sc, ELINK_W1_TX_STATUS))) & TXS_COMPLETE) {
		bus_space_write_2(iot, ioh, ep_w1_reg(sc, ELINK_W1_TX_STATUS),
		    0x0);

		if (i & TXS_JABBER) {
			++sc->sc_ethercom.ec_if.if_oerrors;
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: jabber (%x)\n",
				       device_xname(sc->sc_dev), i);
			epreset(sc);
		} else if (i & TXS_UNDERRUN) {
			++sc->sc_ethercom.ec_if.if_oerrors;
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: fifo underrun (%x) @%d\n",
				       device_xname(sc->sc_dev), i,
				       sc->tx_start_thresh);
			if (sc->tx_succ_ok < 100)
				    sc->tx_start_thresh = min(ETHER_MAX_LEN,
					    sc->tx_start_thresh + 20);
			sc->tx_succ_ok = 0;
			epreset(sc);
		} else if (i & TXS_MAX_COLLISION) {
			++sc->sc_ethercom.ec_if.if_collisions;
			bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_ENABLE);
			sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
		} else
			sc->tx_succ_ok = (sc->tx_succ_ok+1) & 127;
	}
}

int
epintr(void *arg)
{
	struct ep_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int16_t status;
	int ret = 0;

	if (sc->enabled == 0 || !device_is_active(sc->sc_dev))
		return (0);


	for (;;) {
		status = bus_space_read_2(iot, ioh, ELINK_STATUS);

		if ((status & WATCHED_INTERRUPTS) == 0) {
			if ((status & INTR_LATCH) == 0) {
#if 0
				printf("%s: intr latch cleared\n",
				       device_xname(sc->sc_dev));
#endif
				break;
			}
		}

		ret = 1;

		/*
		 * Acknowledge any interrupts.  It's important that we do this
		 * first, since there would otherwise be a race condition.
		 * Due to the i386 interrupt queueing, we may get spurious
		 * interrupts occasionally.
		 */
		bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR |
		    (status & (INTR_LATCH | ALL_INTERRUPTS)));

#if 0
		status = bus_space_read_2(iot, ioh, ELINK_STATUS);

		printf("%s: intr%s%s%s%s\n", device_xname(sc->sc_dev),
		       (status & RX_COMPLETE)?" RX_COMPLETE":"",
		       (status & TX_COMPLETE)?" TX_COMPLETE":"",
		       (status & TX_AVAIL)?" TX_AVAIL":"",
		       (status & CARD_FAILURE)?" CARD_FAILURE":"");
#endif

		if (status & RX_COMPLETE) {
			epread(sc);
		}
		if (status & TX_AVAIL) {
			sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
			epstart(&sc->sc_ethercom.ec_if);
		}
		if (status & CARD_FAILURE) {
			printf("%s: adapter failure (%x)\n",
			    device_xname(sc->sc_dev), status);
#if 1
			epinit(ifp);
#else
			epreset(sc);
#endif
			return (1);
		}
		if (status & TX_COMPLETE) {
			eptxstat(sc);
			epstart(ifp);
		}

		if (status)
			rnd_add_uint32(&sc->rnd_source, status);
	}

	/* no more interrupts */
	return (ret);
}

void
epread(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;
	int len;

	len = bus_space_read_2(iot, ioh, ep_w1_reg(sc, ELINK_W1_RX_STATUS));

again:
	if (ifp->if_flags & IFF_DEBUG) {
		int err = len & ERR_MASK;
		const char *s = NULL;

		if (len & ERR_INCOMPLETE)
			s = "incomplete packet";
		else if (err == ERR_OVERRUN)
			s = "packet overrun";
		else if (err == ERR_RUNT)
			s = "runt packet";
		else if (err == ERR_ALIGNMENT)
			s = "bad alignment";
		else if (err == ERR_CRC)
			s = "bad crc";
		else if (err == ERR_OVERSIZE)
			s = "oversized packet";
		else if (err == ERR_DRIBBLE)
			s = "dribble bits";

		if (s)
			printf("%s: %s\n", device_xname(sc->sc_dev), s);
	}

	if (len & ERR_INCOMPLETE)
		return;

	if (len & ERR_RX) {
		++ifp->if_ierrors;
		goto abort;
	}

	len &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

	/* Pull packet off interface. */
	m = epget(sc, len);
	if (m == 0) {
		ifp->if_ierrors++;
		goto abort;
	}

	++ifp->if_ipackets;

	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	bpf_mtap(ifp, m);

	(*ifp->if_input)(ifp, m);

	/*
	 * In periods of high traffic we can actually receive enough
	 * packets so that the fifo overrun bit will be set at this point,
	 * even though we just read a packet. In this case we
	 * are not going to receive any more interrupts. We check for
	 * this condition and read again until the fifo is not full.
	 * We could simplify this test by not using epstatus(), but
	 * rechecking the RX_STATUS register directly. This test could
	 * result in unnecessary looping in cases where there is a new
	 * packet but the fifo is not full, but it will not fix the
	 * stuck behavior.
	 *
	 * Even with this improvement, we still get packet overrun errors
	 * which are hurting performance. Maybe when I get some more time
	 * I'll modify epread() so that it can handle RX_EARLY interrupts.
	 */
	if (epstatus(sc)) {
		len = bus_space_read_2(iot, ioh,
		    ep_w1_reg(sc, ELINK_W1_RX_STATUS));
		/* Check if we are stuck and reset [see XXX comment] */
		if (len & ERR_INCOMPLETE) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				    device_xname(sc->sc_dev));
			epreset(sc);
			return;
		}
		goto again;
	}

	return;

abort:
	ep_discard_rxtop(iot, ioh);

}

struct mbuf *
epget(struct ep_softc *sc, int totlen)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;
	bus_size_t rxreg;
	int len, remaining;
	int s;
	void *newdata;
	u_long offset;

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;
	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			return 0;
	} else {
		/* If the queue is no longer full, refill. */
		if (sc->last_mb == sc->next_mb)
			callout_reset(&sc->sc_mbuf_callout, 1, epmbuffill, sc);

		/* Convert one of our saved mbuf's. */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
		memset(&m->m_pkthdr, 0, sizeof(m->m_pkthdr));
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;

	/*
	 * Allocate big enough space to hold whole packet, to avoid
	 * allocating new mbufs on splsched().
	 */
	if (totlen + ALIGNBYTES > len) {
		if (totlen + ALIGNBYTES > MCLBYTES) {
			len = ALIGN(totlen + ALIGNBYTES);
			MEXTMALLOC(m, len, M_DONTWAIT);
		} else {
			len = MCLBYTES;
			MCLGET(m, M_DONTWAIT);
		}
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return 0;
		}
	}

	/* align the struct ip header */
	newdata = (char *)ALIGN(m->m_data + sizeof(struct ether_header))
	    - sizeof(struct ether_header);
	m->m_data = newdata;
	m->m_len = totlen;

	rxreg = ep_w1_reg(sc, ELINK_W1_RX_PIO_RD_1);
	remaining = totlen;
	offset = mtod(m, u_long);

	/*
	 * We read the packet at a high interrupt priority level so that
	 * an interrupt from another device won't cause the card's packet
	 * buffer to overflow.  We choose splsched() since that blocks
	 * essentially everything except for interrupts from serial
	 * devices (which typically lose data if their interrupt isn't
	 * serviced fast enough).
	 *
	 * XXX THIS CAN CAUSE CLOCK DRIFT!
	 */
	s = splsched();

	if (sc->ep_flags & ELINK_FLAGS_USEFIFOBUFFER) {
		/*
		 * Prime the FIFO buffer counter (number of 16-bit
		 * words about to be read from the FIFO).
		 *
		 * NOTE: NO OTHER ACCESS CAN BE PERFORMED WHILE THIS
		 * COUNTER IS NON-ZERO!
		 */
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_RDCTL, totlen >> 1);
	}

	if (ELINK_IS_BUS_32(sc->bustype)) {
		/*
		 * Read bytes up to the point where we are aligned.
		 * (We can align to 4 bytes, rather than ALIGNBYTES,
		 * here because we're later reading 4-byte chunks.)
		 */
		if ((remaining > 3) && (offset & 3))  {
			int count = (4 - (offset & 3));
			bus_space_read_multi_1(iot, ioh,
			    rxreg, (u_int8_t *) offset, count);
			offset += count;
			remaining -= count;
		}
		if (remaining > 3) {
			bus_space_read_multi_stream_4(iot, ioh,
			    rxreg, (u_int32_t *) offset,
				    remaining >> 2);
			offset += remaining & ~3;
			remaining &= 3;
		}
		if (remaining)  {
			bus_space_read_multi_1(iot, ioh,
			    rxreg, (u_int8_t *) offset, remaining);
		}
	} else {
		if ((remaining > 1) && (offset & 1))  {
			bus_space_read_multi_1(iot, ioh,
			    rxreg, (u_int8_t *) offset, 1);
			remaining -= 1;
			offset += 1;
		}
		if (remaining > 1) {
			bus_space_read_multi_stream_2(iot, ioh,
			    rxreg, (u_int16_t *) offset,
			    remaining >> 1);
			offset += remaining & ~1;
		}
		if (remaining & 1)  {
				bus_space_read_multi_1(iot, ioh,
			    rxreg, (u_int8_t *) offset, remaining & 1);
		}
	}

	ep_discard_rxtop(iot, ioh);

	if (sc->ep_flags & ELINK_FLAGS_USEFIFOBUFFER)
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_RDCTL, 0);
	splx(s);

	return (m);
}

int
epioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ep_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->enabled == 0) {
			error = EIO;
			break;
		}

	default:
		error = ether_ioctl(ifp, cmd, data);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				epreset(sc);
			error = 0;
		}
		break;
	}

	splx(s);
	return (error);
}

void
epreset(struct ep_softc *sc)
{
	int s;

	s = splnet();
	epinit(&sc->sc_ethercom.ec_if);
	splx(s);
}

void
epwatchdog(struct ifnet *ifp)
{
	struct ep_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", device_xname(sc->sc_dev));
	++sc->sc_ethercom.ec_if.if_oerrors;

	epreset(sc);
}

void
epstop(struct ifnet *ifp, int disable)
{
	struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (sc->ep_flags & ELINK_FLAGS_MII) {
		/* Stop the one second clock. */
		callout_stop(&sc->sc_mbuf_callout);

		/* Down the MII. */
		mii_down(&sc->sc_mii);
	}

	if (sc->ep_chipset == ELINK_CHIPSET_ROADRUNNER) {
		/*
		 * Clear the FIFO buffer count, thus halting
		 * any currently-running transactions.
		 */
		GO_WINDOW(1);		/* sanity */
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_WRCTL, 0);
		bus_space_write_2(iot, ioh, ELINK_W1_RUNNER_RDCTL, 0);
	}

	bus_space_write_2(iot, ioh, ELINK_COMMAND, RX_DISABLE);
	ep_discard_rxtop(iot, ioh);

	bus_space_write_2(iot, ioh, ELINK_COMMAND, TX_DISABLE);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, STOP_TRANSCEIVER);

	ep_reset_cmd(sc, ELINK_COMMAND, RX_RESET);
	ep_reset_cmd(sc, ELINK_COMMAND, TX_RESET);

	bus_space_write_2(iot, ioh, ELINK_COMMAND, ACK_INTR | INTR_LATCH);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, SET_RD_0_MASK);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, SET_INTR_MASK);
	bus_space_write_2(iot, ioh, ELINK_COMMAND, SET_RX_FILTER);

	epmbufempty(sc);

	if (disable)
		epdisable(sc);

	ifp->if_flags &= ~IFF_RUNNING;
}


/*
 * Before reboots, reset card completely.
 */
static bool
epshutdown(device_t self, int howto)
{
	struct ep_softc *sc = device_private(self);
	int s = splnet();

	if (sc->enabled) {
		epstop(&sc->sc_ethercom.ec_if, 0);
		ep_reset_cmd(sc, ELINK_COMMAND, GLOBAL_RESET);
		epdisable(sc);
		sc->enabled = 0;
	}
	splx(s);

	return true;
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by bus_space_read_2().  Hence; we read 16 times getting one
 * bit of data with each read.
 *
 * NOTE: the caller must provide an i/o handle for ELINK_ID_PORT!
 */
u_int16_t
epreadeeprom(bus_space_tag_t iot, bus_space_handle_t ioh, int offset)
{
	u_int16_t data = 0;
	int i;

	bus_space_write_2(iot, ioh, 0, 0x80 + offset);
	delay(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (bus_space_read_2(iot, ioh, 0) & 1);
	return (data);
}

static int
epbusyeeprom(struct ep_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_size_t eecmd;
	int i = 100, j;
	uint16_t busybit;

	if (sc->bustype == ELINK_BUS_PCMCIA) {
		delay(1000);
		return 0;
	}

	if (sc->ep_chipset == ELINK_CHIPSET_CORKSCREW) {
		eecmd = CORK_ASIC_EEPROM_COMMAND;
		busybit = CORK_EEPROM_BUSY;
	} else {
		eecmd = ELINK_W0_EEPROM_COMMAND;
		busybit = EEPROM_BUSY;
	}

	j = 0;		/* bad GCC flow analysis */
	while (i--) {
		j = bus_space_read_2(iot, ioh, eecmd);
		if (j & busybit)
			delay(100);
		else
			break;
	}
	if (i == 0) {
		aprint_normal("\n");
		aprint_error_dev(sc->sc_dev, "eeprom failed to come ready\n");
		return (1);
	}
	if (sc->ep_chipset != ELINK_CHIPSET_CORKSCREW &&
	    (j & EEPROM_TST_MODE) != 0) {
		/* XXX PnP mode? */
		printf("\n%s: erase pencil mark!\n", device_xname(sc->sc_dev));
		return (1);
	}
	return (0);
}

u_int16_t
ep_read_eeprom(struct ep_softc *sc, u_int16_t offset)
{
	bus_size_t eecmd, eedata;
	u_int16_t readcmd;

	if (sc->ep_chipset == ELINK_CHIPSET_CORKSCREW) {
		eecmd = CORK_ASIC_EEPROM_COMMAND;
		eedata = CORK_ASIC_EEPROM_DATA;
	} else {
		eecmd = ELINK_W0_EEPROM_COMMAND;
		eedata = ELINK_W0_EEPROM_DATA;
	}

	/*
	 * RoadRunner has a larger EEPROM, so a different read command
	 * is required.
	 */
	if (sc->ep_chipset == ELINK_CHIPSET_ROADRUNNER)
		readcmd = READ_EEPROM_RR;
	else
		readcmd = READ_EEPROM;

	if (epbusyeeprom(sc))
		return (0);		/* XXX why is eeprom busy? */

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, eecmd, readcmd | offset);

	if (epbusyeeprom(sc))
		return (0);		/* XXX why is eeprom busy? */

	return (bus_space_read_2(sc->sc_iot, sc->sc_ioh, eedata));
}

void
epmbuffill(void *v)
{
	struct ep_softc *sc = v;
	struct mbuf *m;
	int s, i;

	s = splnet();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == 0) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0)
				break;
			sc->mb[i] = m;
		}
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->last_mb = i;
	/* If the queue was not filled, try again. */
	if (sc->last_mb != sc->next_mb)
		callout_reset(&sc->sc_mbuf_callout, 1, epmbuffill, sc);
	splx(s);
}

void
epmbufempty(struct ep_softc *sc)
{
	int s, i;

	s = splnet();
	for (i = 0; i < MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	callout_stop(&sc->sc_mbuf_callout);
	splx(s);
}

int
epenable(struct ep_softc *sc)
{

	if (sc->enabled == 0 && sc->enable != NULL) {
		if ((*sc->enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
	}

	sc->enabled = 1;
	return (0);
}

void
epdisable(struct ep_softc *sc)
{

	if (sc->enabled != 0 && sc->disable != NULL) {
		(*sc->disable)(sc);
		sc->enabled = 0;
	}
}

/*
 * ep_activate:
 *
 *	Handle device activation/deactivation requests.
 */
int
ep_activate(device_t self, enum devact act)
{
	struct ep_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * ep_detach:
 *
 *	Detach a elink3 interface.
 */
int
ep_detach(device_t self, int flags)
{
	struct ep_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* Succeed now if there's no work to do. */
	if ((sc->sc_flags & ELINK_FLAGS_ATTACHED) == 0)
		return (0);

	epdisable(sc);

	callout_stop(&sc->sc_mii_callout);
	callout_stop(&sc->sc_mbuf_callout);

	if (sc->ep_flags & ELINK_FLAGS_MII) {
		/* Detach all PHYs */
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	}

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	rnd_detach_source(&sc->rnd_source);
	ether_ifdetach(ifp);
	if_detach(ifp);

	pmf_device_deregister(sc->sc_dev);

	return (0);
}

u_int32_t
ep_mii_bitbang_read(device_t self)
{
	struct ep_softc *sc = device_private(self);

	/* We're already in Window 4. */
	return (bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    ELINK_W4_BOOM_PHYSMGMT));
}

void
ep_mii_bitbang_write(device_t self, u_int32_t val)
{
	struct ep_softc *sc = device_private(self);

	/* We're already in Window 4. */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    ELINK_W4_BOOM_PHYSMGMT, val);
}

int
ep_mii_readreg(device_t self, int phy, int reg)
{
	struct ep_softc *sc = device_private(self);
	int val;

	GO_WINDOW(4);

	val = mii_bitbang_readreg(self, &ep_mii_bitbang_ops, phy, reg);

	GO_WINDOW(1);

	return (val);
}

void
ep_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct ep_softc *sc = device_private(self);

	GO_WINDOW(4);

	mii_bitbang_writereg(self, &ep_mii_bitbang_ops, phy, reg, val);

	GO_WINDOW(1);
}

void
ep_statchg(struct ifnet *ifp)
{
	struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int mctl;

	GO_WINDOW(3);
	mctl = bus_space_read_2(iot, ioh, ELINK_W3_MAC_CONTROL);
	if (sc->sc_mii.mii_media_active & IFM_FDX)
		mctl |= MAC_CONTROL_FDX;
	else
		mctl &= ~MAC_CONTROL_FDX;
	bus_space_write_2(iot, ioh, ELINK_W3_MAC_CONTROL, mctl);
	GO_WINDOW(1);	/* back to operating window */
}

void
ep_power(int why, void *arg)
{
	struct ep_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int s;

	s = splnet();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		epstop(ifp, 1);
		break;
	case PWR_RESUME:
		if (ifp->if_flags & IFF_UP) {
			(void)epinit(ifp);
		}
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);
}
