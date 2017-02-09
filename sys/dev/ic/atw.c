/*	$NetBSD: atw.c,v 1.156 2013/11/22 00:01:09 riz Exp $  */

/*-
 * Copyright (c) 1998, 1999, 2000, 2002, 2003, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young, by Jason R. Thorpe, and by Charles M. Hannum.
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
 * Device driver for the ADMtek ADM8211 802.11 MAC/BBP.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atw.c,v 1.156 2013/11/22 00:01:09 riz Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kauth.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <lib/libkern/libkern.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/atwreg.h>
#include <dev/ic/rf3000reg.h>
#include <dev/ic/si4136reg.h>
#include <dev/ic/atwvar.h>
#include <dev/ic/smc93cx6var.h>

/* XXX TBD open questions
 *
 *
 * When should I set DSSS PAD in reg 0x15 of RF3000? In 1-2Mbps
 * modes only, or all modes (5.5-11 Mbps CCK modes, too?) Does the MAC
 * handle this for me?
 *
 */
/* device attachment
 *
 *    print TOFS[012]
 *
 * device initialization
 *
 *    clear ATW_FRCTL_MAXPSP to disable max power saving
 *    set ATW_TXBR_ALCUPDATE to enable ALC
 *    set TOFS[012]? (hope not)
 *    disable rx/tx
 *    set ATW_PAR_SWR (software reset)
 *    wait for ATW_PAR_SWR clear
 *    disable interrupts
 *    ack status register
 *    enable interrupts
 *
 * rx/tx initialization
 *
 *    disable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *    allocate and init descriptor rings
 *    write ATW_PAR_DSL (descriptor skip length)
 *    write descriptor base addrs: ATW_TDBD, ATW_TDBP, write ATW_RDB
 *    write ATW_NAR_SQ for one/both transmit descriptor rings
 *    write ATW_NAR_SQ for one/both transmit descriptor rings
 *    enable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *
 * rx/tx end
 *
 *    stop DMA
 *    disable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *    flush tx w/ ATW_NAR_HF
 *
 * scan
 *
 *    initialize rx/tx
 *
 * BSS join: (re)association response
 *
 *    set ATW_FRCTL_AID
 *
 * optimizations ???
 *
 */

#define ATW_REFSLAVE	/* slavishly do what the reference driver does */

int atw_pseudo_milli = 1;
int atw_magic_delay1 = 100 * 1000;
int atw_magic_delay2 = 100 * 1000;
/* more magic multi-millisecond delays (units: microseconds) */
int atw_nar_delay = 20 * 1000;
int atw_magic_delay4 = 10 * 1000;
int atw_rf_delay1 = 10 * 1000;
int atw_rf_delay2 = 5 * 1000;
int atw_plcphd_delay = 2 * 1000;
int atw_bbp_io_enable_delay = 20 * 1000;
int atw_bbp_io_disable_delay = 2 * 1000;
int atw_writewep_delay = 1000;
int atw_beacon_len_adjust = 4;
int atw_dwelltime = 200;
int atw_xindiv2 = 0;

#ifdef ATW_DEBUG
int atw_debug = 0;

#define ATW_DPRINTF(x)	if (atw_debug > 0) printf x
#define ATW_DPRINTF2(x)	if (atw_debug > 1) printf x
#define ATW_DPRINTF3(x)	if (atw_debug > 2) printf x
#define	DPRINTF(sc, x)	if ((sc)->sc_if.if_flags & IFF_DEBUG) printf x
#define	DPRINTF2(sc, x)	if ((sc)->sc_if.if_flags & IFF_DEBUG) ATW_DPRINTF2(x)
#define	DPRINTF3(sc, x)	if ((sc)->sc_if.if_flags & IFF_DEBUG) ATW_DPRINTF3(x)

static void	atw_dump_pkt(struct ifnet *, struct mbuf *);
static void	atw_print_regs(struct atw_softc *, const char *);

/* Note well: I never got atw_rf3000_read or atw_si4126_read to work. */
#	ifdef ATW_BBPDEBUG
static void	atw_rf3000_print(struct atw_softc *);
static int	atw_rf3000_read(struct atw_softc *sc, u_int, u_int *);
#	endif /* ATW_BBPDEBUG */

#	ifdef ATW_SYNDEBUG
static void	atw_si4126_print(struct atw_softc *);
static int	atw_si4126_read(struct atw_softc *, u_int, u_int *);
#	endif /* ATW_SYNDEBUG */
#define __atwdebugused	/* empty */
#else
#define ATW_DPRINTF(x)
#define ATW_DPRINTF2(x)
#define ATW_DPRINTF3(x)
#define	DPRINTF(sc, x)	/* nothing */
#define	DPRINTF2(sc, x)	/* nothing */
#define	DPRINTF3(sc, x)	/* nothing */
#define __atwdebugused	__unused
#endif

/* ifnet methods */
int	atw_init(struct ifnet *);
int	atw_ioctl(struct ifnet *, u_long, void *);
void	atw_start(struct ifnet *);
void	atw_stop(struct ifnet *, int);
void	atw_watchdog(struct ifnet *);

/* Device attachment */
void	atw_attach(struct atw_softc *);
int	atw_detach(struct atw_softc *);
static void atw_evcnt_attach(struct atw_softc *);
static void atw_evcnt_detach(struct atw_softc *);

/* Rx/Tx process */
int	atw_add_rxbuf(struct atw_softc *, int);
void	atw_idle(struct atw_softc *, u_int32_t);
void	atw_rxdrain(struct atw_softc *);
void	atw_txdrain(struct atw_softc *);

/* Device (de)activation and power state */
void	atw_reset(struct atw_softc *);

/* Interrupt handlers */
void	atw_linkintr(struct atw_softc *, u_int32_t);
void	atw_rxintr(struct atw_softc *);
void	atw_txintr(struct atw_softc *, uint32_t);

/* 802.11 state machine */
static int	atw_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	atw_next_scan(void *);
static void	atw_recv_mgmt(struct ieee80211com *, struct mbuf *,
		              struct ieee80211_node *, int, int, u_int32_t);
static int	atw_tune(struct atw_softc *);

/* Device initialization */
static void	atw_bbp_io_init(struct atw_softc *);
static void	atw_cfp_init(struct atw_softc *);
static void	atw_cmdr_init(struct atw_softc *);
static void	atw_ifs_init(struct atw_softc *);
static void	atw_nar_init(struct atw_softc *);
static void	atw_response_times_init(struct atw_softc *);
static void	atw_rf_reset(struct atw_softc *);
static void	atw_test1_init(struct atw_softc *);
static void	atw_tofs0_init(struct atw_softc *);
static void	atw_tofs2_init(struct atw_softc *);
static void	atw_txlmt_init(struct atw_softc *);
static void	atw_wcsr_init(struct atw_softc *);

/* Key management */
static int atw_key_delete(struct ieee80211com *, const struct ieee80211_key *);
static int atw_key_set(struct ieee80211com *, const struct ieee80211_key *,
	const u_int8_t[IEEE80211_ADDR_LEN]);
static void atw_key_update_begin(struct ieee80211com *);
static void atw_key_update_end(struct ieee80211com *);

/* RAM/ROM utilities */
static void	atw_clear_sram(struct atw_softc *);
static void	atw_write_sram(struct atw_softc *, u_int, u_int8_t *, u_int);
static int	atw_read_srom(struct atw_softc *);

/* BSS setup */
static void	atw_predict_beacon(struct atw_softc *);
static void	atw_start_beacon(struct atw_softc *, int);
static void	atw_write_bssid(struct atw_softc *);
static void	atw_write_ssid(struct atw_softc *);
static void	atw_write_sup_rates(struct atw_softc *);
static void	atw_write_wep(struct atw_softc *);

/* Media */
static int	atw_media_change(struct ifnet *);

static void	atw_filter_setup(struct atw_softc *);

/* 802.11 utilities */
static uint64_t			atw_get_tsft(struct atw_softc *);
static inline uint32_t	atw_last_even_tsft(uint32_t, uint32_t,
				                   uint32_t);
static struct ieee80211_node	*atw_node_alloc(struct ieee80211_node_table *);
static void			atw_node_free(struct ieee80211_node *);

/*
 * Tuner/transceiver/modem
 */
static void	atw_bbp_io_enable(struct atw_softc *, int);

/* RFMD RF3000 Baseband Processor */
static int	atw_rf3000_init(struct atw_softc *);
static int	atw_rf3000_tune(struct atw_softc *, u_int);
static int	atw_rf3000_write(struct atw_softc *, u_int, u_int);

/* Silicon Laboratories Si4126 RF/IF Synthesizer */
static void	atw_si4126_tune(struct atw_softc *, u_int);
static void	atw_si4126_write(struct atw_softc *, u_int, u_int);

const struct atw_txthresh_tab atw_txthresh_tab_lo[] = ATW_TXTHRESH_TAB_LO_RATE;
const struct atw_txthresh_tab atw_txthresh_tab_hi[] = ATW_TXTHRESH_TAB_HI_RATE;

const char *atw_tx_state[] = {
	"STOPPED",
	"RUNNING - read descriptor",
	"RUNNING - transmitting",
	"RUNNING - filling fifo",	/* XXX */
	"SUSPENDED",
	"RUNNING -- write descriptor",
	"RUNNING -- write last descriptor",
	"RUNNING - fifo full"
};

const char *atw_rx_state[] = {
	"STOPPED",
	"RUNNING - read descriptor",
	"RUNNING - check this packet, pre-fetch next",
	"RUNNING - wait for reception",
	"SUSPENDED",
	"RUNNING - write descriptor",
	"RUNNING - flush fifo",
	"RUNNING - fifo drain"
};

static inline int
is_running(struct ifnet *ifp)
{
	return (ifp->if_flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP);
}

int
atw_activate(device_t self, enum devact act)
{
	struct atw_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

bool
atw_suspend(device_t self, const pmf_qual_t *qual)
{
	struct atw_softc *sc = device_private(self);

	atw_rxdrain(sc);
	sc->sc_flags &= ~ATWF_WEP_SRAM_VALID;

	return true;
}

/* Returns -1 on failure. */
static int
atw_read_srom(struct atw_softc *sc)
{
	struct seeprom_descriptor sd;
	uint32_t test0, fail_bits;

	(void)memset(&sd, 0, sizeof(sd));

	test0 = ATW_READ(sc, ATW_TEST0);

	switch (sc->sc_rev) {
	case ATW_REVISION_BA:
	case ATW_REVISION_CA:
		fail_bits = ATW_TEST0_EPNE;
		break;
	default:
		fail_bits = ATW_TEST0_EPNE|ATW_TEST0_EPSNM;
		break;
	}
	if ((test0 & fail_bits) != 0) {
		aprint_error_dev(sc->sc_dev, "bad or missing/bad SROM\n");
		return -1;
	}

	switch (test0 & ATW_TEST0_EPTYP_MASK) {
	case ATW_TEST0_EPTYP_93c66:
		ATW_DPRINTF(("%s: 93c66 SROM\n", device_xname(sc->sc_dev)));
		sc->sc_sromsz = 512;
		sd.sd_chip = C56_66;
		break;
	case ATW_TEST0_EPTYP_93c46:
		ATW_DPRINTF(("%s: 93c46 SROM\n", device_xname(sc->sc_dev)));
		sc->sc_sromsz = 128;
		sd.sd_chip = C46;
		break;
	default:
		printf("%s: unknown SROM type %" __PRIuBITS "\n",
		    device_xname(sc->sc_dev),
		    __SHIFTOUT(test0, ATW_TEST0_EPTYP_MASK));
		return -1;
	}

	sc->sc_srom = malloc(sc->sc_sromsz, M_DEVBUF, M_NOWAIT);

	if (sc->sc_srom == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to allocate SROM buffer\n");
		return -1;
	}

	(void)memset(sc->sc_srom, 0, sc->sc_sromsz);

	/* ADM8211 has a single 32-bit register for controlling the
	 * 93cx6 SROM.  Bit SRS enables the serial port. There is no
	 * "ready" bit. The ADM8211 input/output sense is the reverse
	 * of read_seeprom's.
	 */
	sd.sd_tag = sc->sc_st;
	sd.sd_bsh = sc->sc_sh;
	sd.sd_regsize = 4;
	sd.sd_control_offset = ATW_SPR;
	sd.sd_status_offset = ATW_SPR;
	sd.sd_dataout_offset = ATW_SPR;
	sd.sd_CK = ATW_SPR_SCLK;
	sd.sd_CS = ATW_SPR_SCS;
	sd.sd_DI = ATW_SPR_SDO;
	sd.sd_DO = ATW_SPR_SDI;
	sd.sd_MS = ATW_SPR_SRS;
	sd.sd_RDY = 0;

	if (!read_seeprom(&sd, sc->sc_srom, 0, sc->sc_sromsz/2)) {
		aprint_error_dev(sc->sc_dev, "could not read SROM\n");
		free(sc->sc_srom, M_DEVBUF);
		return -1;
	}
#ifdef ATW_DEBUG
	{
		int i;
		ATW_DPRINTF(("\nSerial EEPROM:\n\t"));
		for (i = 0; i < sc->sc_sromsz/2; i = i + 1) {
			if (((i % 8) == 0) && (i != 0)) {
				ATW_DPRINTF(("\n\t"));
			}
			ATW_DPRINTF((" 0x%x", sc->sc_srom[i]));
		}
		ATW_DPRINTF(("\n"));
	}
#endif /* ATW_DEBUG */
	return 0;
}

#ifdef ATW_DEBUG
static void
atw_print_regs(struct atw_softc *sc, const char *where)
{
#define PRINTREG(sc, reg) \
	ATW_DPRINTF2(("%s: reg[ " #reg " / %03x ] = %08x\n", \
	    device_xname(sc->sc_dev), reg, ATW_READ(sc, reg)))

	ATW_DPRINTF2(("%s: %s\n", device_xname(sc->sc_dev), where));

	PRINTREG(sc, ATW_PAR);
	PRINTREG(sc, ATW_FRCTL);
	PRINTREG(sc, ATW_TDR);
	PRINTREG(sc, ATW_WTDP);
	PRINTREG(sc, ATW_RDR);
	PRINTREG(sc, ATW_WRDP);
	PRINTREG(sc, ATW_RDB);
	PRINTREG(sc, ATW_CSR3A);
	PRINTREG(sc, ATW_TDBD);
	PRINTREG(sc, ATW_TDBP);
	PRINTREG(sc, ATW_STSR);
	PRINTREG(sc, ATW_CSR5A);
	PRINTREG(sc, ATW_NAR);
	PRINTREG(sc, ATW_CSR6A);
	PRINTREG(sc, ATW_IER);
	PRINTREG(sc, ATW_CSR7A);
	PRINTREG(sc, ATW_LPC);
	PRINTREG(sc, ATW_TEST1);
	PRINTREG(sc, ATW_SPR);
	PRINTREG(sc, ATW_TEST0);
	PRINTREG(sc, ATW_WCSR);
	PRINTREG(sc, ATW_WPDR);
	PRINTREG(sc, ATW_GPTMR);
	PRINTREG(sc, ATW_GPIO);
	PRINTREG(sc, ATW_BBPCTL);
	PRINTREG(sc, ATW_SYNCTL);
	PRINTREG(sc, ATW_PLCPHD);
	PRINTREG(sc, ATW_MMIWADDR);
	PRINTREG(sc, ATW_MMIRADDR1);
	PRINTREG(sc, ATW_MMIRADDR2);
	PRINTREG(sc, ATW_TXBR);
	PRINTREG(sc, ATW_CSR15A);
	PRINTREG(sc, ATW_ALCSTAT);
	PRINTREG(sc, ATW_TOFS2);
	PRINTREG(sc, ATW_CMDR);
	PRINTREG(sc, ATW_PCIC);
	PRINTREG(sc, ATW_PMCSR);
	PRINTREG(sc, ATW_PAR0);
	PRINTREG(sc, ATW_PAR1);
	PRINTREG(sc, ATW_MAR0);
	PRINTREG(sc, ATW_MAR1);
	PRINTREG(sc, ATW_ATIMDA0);
	PRINTREG(sc, ATW_ABDA1);
	PRINTREG(sc, ATW_BSSID0);
	PRINTREG(sc, ATW_TXLMT);
	PRINTREG(sc, ATW_MIBCNT);
	PRINTREG(sc, ATW_BCNT);
	PRINTREG(sc, ATW_TSFTH);
	PRINTREG(sc, ATW_TSC);
	PRINTREG(sc, ATW_SYNRF);
	PRINTREG(sc, ATW_BPLI);
	PRINTREG(sc, ATW_CAP0);
	PRINTREG(sc, ATW_CAP1);
	PRINTREG(sc, ATW_RMD);
	PRINTREG(sc, ATW_CFPP);
	PRINTREG(sc, ATW_TOFS0);
	PRINTREG(sc, ATW_TOFS1);
	PRINTREG(sc, ATW_IFST);
	PRINTREG(sc, ATW_RSPT);
	PRINTREG(sc, ATW_TSFTL);
	PRINTREG(sc, ATW_WEPCTL);
	PRINTREG(sc, ATW_WESK);
	PRINTREG(sc, ATW_WEPCNT);
	PRINTREG(sc, ATW_MACTEST);
	PRINTREG(sc, ATW_FER);
	PRINTREG(sc, ATW_FEMR);
	PRINTREG(sc, ATW_FPSR);
	PRINTREG(sc, ATW_FFER);
#undef PRINTREG
}
#endif /* ATW_DEBUG */

/*
 * Finish attaching an ADMtek ADM8211 MAC.  Called by bus-specific front-end.
 */
void
atw_attach(struct atw_softc *sc)
{
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int country_code, error, i, nrate, srom_major;
	u_int32_t reg;
	static const char *type_strings[] = {"Intersil (not supported)",
	    "RFMD", "Marvel (not supported)"};

	pmf_self_suspensor_init(sc->sc_dev, &sc->sc_suspensor, &sc->sc_qual);

	sc->sc_txth = atw_txthresh_tab_lo;

	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);

#ifdef ATW_DEBUG
	atw_print_regs(sc, "atw_attach");
#endif /* ATW_DEBUG */

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct atw_control_data), PAGE_SIZE, 0, &sc->sc_cdseg,
	    1, &sc->sc_cdnseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg,
	    sizeof(struct atw_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct atw_control_data), 1,
	    sizeof(struct atw_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control data DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct atw_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n", error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	sc->sc_ntxsegs = ATW_NTXSEGS;
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    sc->sc_ntxsegs, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create tx DMA map %d, error = %d\n", i,
			    error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < ATW_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create rx DMA map %d, error = %d\n", i,
			    error);
			goto fail_5;
		}
	}
	for (i = 0; i < ATW_NRXDESC; i++) {
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	switch (sc->sc_rev) {
	case ATW_REVISION_AB:
	case ATW_REVISION_AF:
		sc->sc_sramlen = ATW_SRAM_A_SIZE;
		break;
	case ATW_REVISION_BA:
	case ATW_REVISION_CA:
		sc->sc_sramlen = ATW_SRAM_B_SIZE;
		break;
	}

	/* Reset the chip to a known state. */
	atw_reset(sc);

	if (atw_read_srom(sc) == -1)
		return;

	sc->sc_rftype = __SHIFTOUT(sc->sc_srom[ATW_SR_CSR20],
	    ATW_SR_RFTYPE_MASK);

	sc->sc_bbptype = __SHIFTOUT(sc->sc_srom[ATW_SR_CSR20],
	    ATW_SR_BBPTYPE_MASK);

	if (sc->sc_rftype >= __arraycount(type_strings)) {
		aprint_error_dev(sc->sc_dev, "unknown RF\n");
		return;
	}
	if (sc->sc_bbptype >= __arraycount(type_strings)) {
		aprint_error_dev(sc->sc_dev, "unknown BBP\n");
		return;
	}

	printf("%s: %s RF, %s BBP", device_xname(sc->sc_dev),
	    type_strings[sc->sc_rftype], type_strings[sc->sc_bbptype]);

	/* XXX There exists a Linux driver which seems to use RFType = 0 for
	 * MARVEL. My bug, or theirs?
	 */

	reg = __SHIFTIN(sc->sc_rftype, ATW_SYNCTL_RFTYPE_MASK);

	switch (sc->sc_rftype) {
	case ATW_RFTYPE_INTERSIL:
		reg |= ATW_SYNCTL_CS1;
		break;
	case ATW_RFTYPE_RFMD:
		reg |= ATW_SYNCTL_CS0;
		break;
	case ATW_RFTYPE_MARVEL:
		break;
	}

	sc->sc_synctl_rd = reg | ATW_SYNCTL_RD;
	sc->sc_synctl_wr = reg | ATW_SYNCTL_WR;

	reg = __SHIFTIN(sc->sc_bbptype, ATW_BBPCTL_TYPE_MASK);

	switch (sc->sc_bbptype) {
	case ATW_BBPTYPE_INTERSIL:
		reg |= ATW_BBPCTL_TWI;
		break;
	case ATW_BBPTYPE_RFMD:
		reg |= ATW_BBPCTL_RF3KADDR_ADDR | ATW_BBPCTL_NEGEDGE_DO |
		    ATW_BBPCTL_CCA_ACTLO;
		break;
	case ATW_BBPTYPE_MARVEL:
		break;
	case ATW_C_BBPTYPE_RFMD:
		printf("%s: ADM8211C MAC/RFMD BBP not supported yet.\n",
		    device_xname(sc->sc_dev));
		break;
	}

	sc->sc_bbpctl_wr = reg | ATW_BBPCTL_WR;
	sc->sc_bbpctl_rd = reg | ATW_BBPCTL_RD;

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */
	sc->sc_flags |= ATWF_ATTACHED;

	ATW_DPRINTF((" SROM MAC %04x%04x%04x",
	    htole16(sc->sc_srom[ATW_SR_MAC00]),
	    htole16(sc->sc_srom[ATW_SR_MAC01]),
	    htole16(sc->sc_srom[ATW_SR_MAC10])));

	srom_major = __SHIFTOUT(sc->sc_srom[ATW_SR_FORMAT_VERSION],
	    ATW_SR_MAJOR_MASK);

	if (srom_major < 2)
		sc->sc_rf3000_options1 = 0;
	else if (sc->sc_rev == ATW_REVISION_BA) {
		sc->sc_rf3000_options1 =
		    __SHIFTOUT(sc->sc_srom[ATW_SR_CR28_CR03],
		    ATW_SR_CR28_MASK);
	} else
		sc->sc_rf3000_options1 = 0;

	sc->sc_rf3000_options2 = __SHIFTOUT(sc->sc_srom[ATW_SR_CTRY_CR29],
	    ATW_SR_CR29_MASK);

	country_code = __SHIFTOUT(sc->sc_srom[ATW_SR_CTRY_CR29],
	    ATW_SR_CTRY_MASK);

#define ADD_CHANNEL(_ic, _chan) do {					\
	_ic->ic_channels[_chan].ic_flags = IEEE80211_CHAN_B;		\
	_ic->ic_channels[_chan].ic_freq =				\
	    ieee80211_ieee2mhz(_chan, _ic->ic_channels[_chan].ic_flags);\
} while (0)

	/* Find available channels */
	switch (country_code) {
	case COUNTRY_MMK2:	/* 1-14 */
		ADD_CHANNEL(ic, 14);
		/*FALLTHROUGH*/
	case COUNTRY_ETSI:	/* 1-13 */
		for (i = 1; i <= 13; i++)
			ADD_CHANNEL(ic, i);
		break;
	case COUNTRY_FCC:	/* 1-11 */
	case COUNTRY_IC:	/* 1-11 */
		for (i = 1; i <= 11; i++)
			ADD_CHANNEL(ic, i);
		break;
	case COUNTRY_MMK:	/* 14 */
		ADD_CHANNEL(ic, 14);
		break;
	case COUNTRY_FRANCE:	/* 10-13 */
		for (i = 10; i <= 13; i++)
			ADD_CHANNEL(ic, i);
		break;
	default:	/* assume channels 10-11 */
	case COUNTRY_SPAIN:	/* 10-11 */
		for (i = 10; i <= 11; i++)
			ADD_CHANNEL(ic, i);
		break;
	}

	/* Read the MAC address. */
	reg = ATW_READ(sc, ATW_PAR0);
	ic->ic_myaddr[0] = __SHIFTOUT(reg, ATW_PAR0_PAB0_MASK);
	ic->ic_myaddr[1] = __SHIFTOUT(reg, ATW_PAR0_PAB1_MASK);
	ic->ic_myaddr[2] = __SHIFTOUT(reg, ATW_PAR0_PAB2_MASK);
	ic->ic_myaddr[3] = __SHIFTOUT(reg, ATW_PAR0_PAB3_MASK);
	reg = ATW_READ(sc, ATW_PAR1);
	ic->ic_myaddr[4] = __SHIFTOUT(reg, ATW_PAR1_PAB4_MASK);
	ic->ic_myaddr[5] = __SHIFTOUT(reg, ATW_PAR1_PAB5_MASK);

	if (IEEE80211_ADDR_EQ(ic->ic_myaddr, empty_macaddr)) {
		printf(" could not get mac address, attach failed\n");
		return;
	}

	printf(" 802.11 address %s\n", ether_sprintf(ic->ic_myaddr));

	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST |
	    IFF_NOTRAILERS;
	ifp->if_ioctl = atw_ioctl;
	ifp->if_start = atw_start;
	ifp->if_watchdog = atw_watchdog;
	ifp->if_init = atw_init;
	ifp->if_stop = atw_stop;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_PMGT | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_MONITOR;

	nrate = 0;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 2;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 4;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 11;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 22;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = nrate;

	/*
	 * Call MI attach routines.
	 */

	if_attach(ifp);
	ieee80211_ifattach(ic);

	atw_evcnt_attach(sc);

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = atw_newstate;

	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = atw_recv_mgmt;

	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = atw_node_free;

	sc->sc_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = atw_node_alloc;

	ic->ic_crypto.cs_key_delete = atw_key_delete;
	ic->ic_crypto.cs_key_set = atw_key_set;
	ic->ic_crypto.cs_key_update_begin = atw_key_update_begin;
	ic->ic_crypto.cs_key_update_end = atw_key_update_end;

	/* possibly we should fill in our own sc_send_prresp, since
	 * the ADM8211 is probably sending probe responses in ad hoc
	 * mode.
	 */

	/* complete initialization */
	ieee80211_media_init(ic, atw_media_change, ieee80211_media_status);
	callout_init(&sc->sc_scan_ch, 0);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_radiobpf);

	memset(&sc->sc_rxtapu, 0, sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.ar_ihdr.it_len = htole16(sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.ar_ihdr.it_present = htole32(ATW_RX_RADIOTAP_PRESENT);

	memset(&sc->sc_txtapu, 0, sizeof(sc->sc_txtapu));
	sc->sc_txtap.at_ihdr.it_len = htole16(sizeof(sc->sc_txtapu));
	sc->sc_txtap.at_ihdr.it_present = htole32(ATW_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < ATW_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct atw_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);
 fail_0:
	return;
}

static struct ieee80211_node *
atw_node_alloc(struct ieee80211_node_table *nt)
{
	struct atw_softc *sc = (struct atw_softc *)nt->nt_ic->ic_ifp->if_softc;
	struct ieee80211_node *ni = (*sc->sc_node_alloc)(nt);

	DPRINTF(sc, ("%s: alloc node %p\n", device_xname(sc->sc_dev), ni));
	return ni;
}

static void
atw_node_free(struct ieee80211_node *ni)
{
	struct atw_softc *sc = (struct atw_softc *)ni->ni_ic->ic_ifp->if_softc;

	DPRINTF(sc, ("%s: freeing node %p %s\n", device_xname(sc->sc_dev), ni,
	    ether_sprintf(ni->ni_bssid)));
	(*sc->sc_node_free)(ni);
}


static void
atw_test1_reset(struct atw_softc *sc)
{
	switch (sc->sc_rev) {
	case ATW_REVISION_BA:
		if (1 /* XXX condition on transceiver type */) {
			ATW_SET(sc, ATW_TEST1, ATW_TEST1_TESTMODE_MONITOR);
		}
		break;
	case ATW_REVISION_CA:
		ATW_CLR(sc, ATW_TEST1, ATW_TEST1_TESTMODE_MASK);
		break;
	default:
		break;
	}
}

/*
 * atw_reset:
 *
 *	Perform a soft reset on the ADM8211.
 */
void
atw_reset(struct atw_softc *sc)
{
	int i;
	uint32_t lpc __atwdebugused;

	ATW_WRITE(sc, ATW_NAR, 0x0);
	DELAY(atw_nar_delay);

	/* Reference driver has a cryptic remark indicating that this might
	 * power-on the chip.  I know that it turns off power-saving....
	 */
	ATW_WRITE(sc, ATW_FRCTL, 0x0);

	ATW_WRITE(sc, ATW_PAR, ATW_PAR_SWR);

	for (i = 0; i < 50000 / atw_pseudo_milli; i++) {
		if ((ATW_READ(sc, ATW_PAR) & ATW_PAR_SWR) == 0)
			break;
		DELAY(atw_pseudo_milli);
	}

	/* ... and then pause 100ms longer for good measure. */
	DELAY(atw_magic_delay1);

	DPRINTF2(sc, ("%s: atw_reset %d iterations\n", device_xname(sc->sc_dev), i));

	if (ATW_ISSET(sc, ATW_PAR, ATW_PAR_SWR))
		aprint_error_dev(sc->sc_dev, "reset failed to complete\n");

	/*
	 * Initialize the PCI Access Register.
	 */
	sc->sc_busmode = ATW_PAR_PBL_8DW;

	ATW_WRITE(sc, ATW_PAR, sc->sc_busmode);
	DPRINTF(sc, ("%s: ATW_PAR %08x busmode %08x\n", device_xname(sc->sc_dev),
	    ATW_READ(sc, ATW_PAR), sc->sc_busmode));

	atw_test1_reset(sc);

	/* Turn off maximum power saving, etc. */
	ATW_WRITE(sc, ATW_FRCTL, 0x0);

	DELAY(atw_magic_delay2);

	/* Recall EEPROM. */
	ATW_SET(sc, ATW_TEST0, ATW_TEST0_EPRLD);

	DELAY(atw_magic_delay4);

	lpc = ATW_READ(sc, ATW_LPC);

	DPRINTF(sc, ("%s: ATW_LPC %#08x\n", __func__, lpc));

	/* A reset seems to affect the SRAM contents, so put them into
	 * a known state.
	 */
	atw_clear_sram(sc);

	memset(sc->sc_bssid, 0xff, sizeof(sc->sc_bssid));
}

static void
atw_clear_sram(struct atw_softc *sc)
{
	memset(sc->sc_sram, 0, sizeof(sc->sc_sram));
	sc->sc_flags &= ~ATWF_WEP_SRAM_VALID;
	/* XXX not for revision 0x20. */
	atw_write_sram(sc, 0, sc->sc_sram, sc->sc_sramlen);
}

/* TBD atw_init
 *
 * set MAC based on ic->ic_bss->myaddr
 * write WEP keys
 * set TX rate
 */

/* Tell the ADM8211 to raise ATW_INTR_LINKOFF if 7 beacon intervals pass
 * without receiving a beacon with the preferred BSSID & SSID.
 * atw_write_bssid & atw_write_ssid set the BSSID & SSID.
 */
static void
atw_wcsr_init(struct atw_softc *sc)
{
	uint32_t wcsr;

	wcsr = ATW_READ(sc, ATW_WCSR);
	wcsr &= ~(ATW_WCSR_BLN_MASK|ATW_WCSR_LSOE|ATW_WCSR_MPRE|ATW_WCSR_LSOE);
	wcsr |= __SHIFTIN(7, ATW_WCSR_BLN_MASK);
	ATW_WRITE(sc, ATW_WCSR, wcsr);	/* XXX resets wake-up status bits */

	DPRINTF(sc, ("%s: %s reg[WCSR] = %08x\n",
	    device_xname(sc->sc_dev), __func__, ATW_READ(sc, ATW_WCSR)));
}

/* Turn off power management.  Set Rx store-and-forward mode. */
static void
atw_cmdr_init(struct atw_softc *sc)
{
	uint32_t cmdr;
	cmdr = ATW_READ(sc, ATW_CMDR);
	cmdr &= ~ATW_CMDR_APM;
	cmdr |= ATW_CMDR_RTE;
	cmdr &= ~ATW_CMDR_DRT_MASK;
	cmdr |= ATW_CMDR_DRT_SF;

	ATW_WRITE(sc, ATW_CMDR, cmdr);
}

static void
atw_tofs2_init(struct atw_softc *sc)
{
	uint32_t tofs2;
	/* XXX this magic can probably be figured out from the RFMD docs */
#ifndef ATW_REFSLAVE
	tofs2 = __SHIFTIN(4, ATW_TOFS2_PWR1UP_MASK)    | /* 8 ms = 4 * 2 ms */
	      __SHIFTIN(13, ATW_TOFS2_PWR0PAPE_MASK) | /* 13 us */
	      __SHIFTIN(8, ATW_TOFS2_PWR1PAPE_MASK)  | /* 8 us */
	      __SHIFTIN(5, ATW_TOFS2_PWR0TRSW_MASK)  | /* 5 us */
	      __SHIFTIN(12, ATW_TOFS2_PWR1TRSW_MASK) | /* 12 us */
	      __SHIFTIN(13, ATW_TOFS2_PWR0PE2_MASK)  | /* 13 us */
	      __SHIFTIN(4, ATW_TOFS2_PWR1PE2_MASK)   | /* 4 us */
	      __SHIFTIN(5, ATW_TOFS2_PWR0TXPE_MASK);  /* 5 us */
#else
	/* XXX new magic from reference driver source */
	tofs2 = __SHIFTIN(8, ATW_TOFS2_PWR1UP_MASK)    | /* 8 ms = 4 * 2 ms */
	      __SHIFTIN(8, ATW_TOFS2_PWR0PAPE_MASK) | /* 8 us */
	      __SHIFTIN(1, ATW_TOFS2_PWR1PAPE_MASK)  | /* 1 us */
	      __SHIFTIN(5, ATW_TOFS2_PWR0TRSW_MASK)  | /* 5 us */
	      __SHIFTIN(12, ATW_TOFS2_PWR1TRSW_MASK) | /* 12 us */
	      __SHIFTIN(13, ATW_TOFS2_PWR0PE2_MASK)  | /* 13 us */
	      __SHIFTIN(1, ATW_TOFS2_PWR1PE2_MASK)   | /* 1 us */
	      __SHIFTIN(8, ATW_TOFS2_PWR0TXPE_MASK);  /* 8 us */
#endif
	ATW_WRITE(sc, ATW_TOFS2, tofs2);
}

static void
atw_nar_init(struct atw_softc *sc)
{
	ATW_WRITE(sc, ATW_NAR, ATW_NAR_SF|ATW_NAR_PB);
}

static void
atw_txlmt_init(struct atw_softc *sc)
{
	ATW_WRITE(sc, ATW_TXLMT, __SHIFTIN(512, ATW_TXLMT_MTMLT_MASK) |
	                         __SHIFTIN(1, ATW_TXLMT_SRTYLIM_MASK));
}

static void
atw_test1_init(struct atw_softc *sc)
{
	uint32_t test1;

	test1 = ATW_READ(sc, ATW_TEST1);
	test1 &= ~(ATW_TEST1_DBGREAD_MASK|ATW_TEST1_CONTROL);
	/* XXX magic 0x1 */
	test1 |= __SHIFTIN(0x1, ATW_TEST1_DBGREAD_MASK) | ATW_TEST1_CONTROL;
	ATW_WRITE(sc, ATW_TEST1, test1);
}

static void
atw_rf_reset(struct atw_softc *sc)
{
	/* XXX this resets an Intersil RF front-end? */
	/* TBD condition on Intersil RFType? */
	ATW_WRITE(sc, ATW_SYNRF, ATW_SYNRF_INTERSIL_EN);
	DELAY(atw_rf_delay1);
	ATW_WRITE(sc, ATW_SYNRF, 0);
	DELAY(atw_rf_delay2);
}

/* Set 16 TU max duration for the contention-free period (CFP). */
static void
atw_cfp_init(struct atw_softc *sc)
{
	uint32_t cfpp;

	cfpp = ATW_READ(sc, ATW_CFPP);
	cfpp &= ~ATW_CFPP_CFPMD;
	cfpp |= __SHIFTIN(16, ATW_CFPP_CFPMD);
	ATW_WRITE(sc, ATW_CFPP, cfpp);
}

static void
atw_tofs0_init(struct atw_softc *sc)
{
	/* XXX I guess that the Cardbus clock is 22 MHz?
	 * I am assuming that the role of ATW_TOFS0_USCNT is
	 * to divide the bus clock to get a 1 MHz clock---the datasheet is not
	 * very clear on this point. It says in the datasheet that it is
	 * possible for the ADM8211 to accommodate bus speeds between 22 MHz
	 * and 33 MHz; maybe this is the way? I see a binary-only driver write
	 * these values. These values are also the power-on default.
	 */
	ATW_WRITE(sc, ATW_TOFS0,
	    __SHIFTIN(22, ATW_TOFS0_USCNT_MASK) |
	    ATW_TOFS0_TUCNT_MASK /* set all bits in TUCNT */);
}

/* Initialize interframe spacing: 802.11b slot time, SIFS, DIFS, EIFS. */
static void
atw_ifs_init(struct atw_softc *sc)
{
	uint32_t ifst;
	/* XXX EIFS=0x64, SIFS=110 are used by the reference driver.
	 * Go figure.
	 */
	ifst = __SHIFTIN(IEEE80211_DUR_DS_SLOT, ATW_IFST_SLOT_MASK) |
	      __SHIFTIN(22 * 10 /* IEEE80211_DUR_DS_SIFS */ /* # of 22 MHz cycles */,
	             ATW_IFST_SIFS_MASK) |
	      __SHIFTIN(IEEE80211_DUR_DS_DIFS, ATW_IFST_DIFS_MASK) |
	      __SHIFTIN(IEEE80211_DUR_DS_EIFS, ATW_IFST_EIFS_MASK);

	ATW_WRITE(sc, ATW_IFST, ifst);
}

static void
atw_response_times_init(struct atw_softc *sc)
{
	/* XXX More magic. Relates to ACK timing?  The datasheet seems to
	 * indicate that the MAC expects at least SIFS + MIRT microseconds
	 * to pass after it transmits a frame that requires a response;
	 * it waits at most SIFS + MART microseconds for the response.
	 * Surely this is not the ACK timeout?
	 */
	ATW_WRITE(sc, ATW_RSPT, __SHIFTIN(0xffff, ATW_RSPT_MART_MASK) |
	    __SHIFTIN(0xff, ATW_RSPT_MIRT_MASK));
}

/* Set up the MMI read/write addresses for the baseband. The Tx/Rx
 * engines read and write baseband registers after Rx and before
 * Tx, respectively.
 */
static void
atw_bbp_io_init(struct atw_softc *sc)
{
	uint32_t mmiraddr2;

	/* XXX The reference driver does this, but is it *really*
	 * necessary?
	 */
	switch (sc->sc_rev) {
	case ATW_REVISION_AB:
	case ATW_REVISION_AF:
		mmiraddr2 = 0x0;
		break;
	default:
		mmiraddr2 = ATW_READ(sc, ATW_MMIRADDR2);
		mmiraddr2 &=
		    ~(ATW_MMIRADDR2_PROREXT|ATW_MMIRADDR2_PRORLEN_MASK);
		break;
	}

	switch (sc->sc_bbptype) {
	case ATW_BBPTYPE_INTERSIL:
		ATW_WRITE(sc, ATW_MMIWADDR, ATW_MMIWADDR_INTERSIL);
		ATW_WRITE(sc, ATW_MMIRADDR1, ATW_MMIRADDR1_INTERSIL);
		mmiraddr2 |= ATW_MMIRADDR2_INTERSIL;
		break;
	case ATW_BBPTYPE_MARVEL:
		/* TBD find out the Marvel settings. */
		break;
	case ATW_BBPTYPE_RFMD:
	default:
		ATW_WRITE(sc, ATW_MMIWADDR, ATW_MMIWADDR_RFMD);
		ATW_WRITE(sc, ATW_MMIRADDR1, ATW_MMIRADDR1_RFMD);
		mmiraddr2 |= ATW_MMIRADDR2_RFMD;
		break;
	}
	ATW_WRITE(sc, ATW_MMIRADDR2, mmiraddr2);
	ATW_WRITE(sc, ATW_MACTEST, ATW_MACTEST_MMI_USETXCLK);
}

/*
 * atw_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
int
atw_init(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct atw_txsoft *txs;
	struct atw_rxsoft *rxs;
	int i, error = 0;

	if (device_is_active(sc->sc_dev)) {
		/*
		 * Cancel any pending I/O.
		 */
		atw_stop(ifp, 0);
	} else if (!pmf_device_subtree_resume(sc->sc_dev, &sc->sc_qual) ||
	           !device_is_active(sc->sc_dev))
		return 0;

	/*
	 * Reset the chip to a known state.
	 */
	atw_reset(sc);

	DPRINTF(sc, ("%s: channel %d freq %d flags 0x%04x\n",
	    __func__, ieee80211_chan2ieee(ic, ic->ic_curchan),
	    ic->ic_curchan->ic_freq, ic->ic_curchan->ic_flags));

	atw_wcsr_init(sc);

	atw_cmdr_init(sc);

	/* Set data rate for PLCP Signal field, 1Mbps = 10 x 100Kb/s.
	 *
	 * XXX Set transmit power for ATIM, RTS, Beacon.
	 */
	ATW_WRITE(sc, ATW_PLCPHD, __SHIFTIN(10, ATW_PLCPHD_SIGNAL_MASK) |
	    __SHIFTIN(0xb0, ATW_PLCPHD_SERVICE_MASK));

	atw_tofs2_init(sc);

	atw_nar_init(sc);

	atw_txlmt_init(sc);

	atw_test1_init(sc);

	atw_rf_reset(sc);

	atw_cfp_init(sc);

	atw_tofs0_init(sc);

	atw_ifs_init(sc);

	/* XXX Fall asleep after one second of inactivity.
	 * XXX A frame may only dribble in for 65536us.
	 */
	ATW_WRITE(sc, ATW_RMD,
	    __SHIFTIN(1, ATW_RMD_PCNT) | __SHIFTIN(0xffff, ATW_RMD_RMRD_MASK));

	atw_response_times_init(sc);

	atw_bbp_io_init(sc);

	ATW_WRITE(sc, ATW_STSR, 0xffffffff);

	if ((error = atw_rf3000_init(sc)) != 0)
		goto out;

	ATW_WRITE(sc, ATW_PAR, sc->sc_busmode);
	DPRINTF(sc, ("%s: ATW_PAR %08x busmode %08x\n", device_xname(sc->sc_dev),
	    ATW_READ(sc, ATW_PAR), sc->sc_busmode));

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < ATW_NTXDESC; i++) {
		sc->sc_txdescs[i].at_ctl = 0;
		/* no transmit chaining */
		sc->sc_txdescs[i].at_flags = 0 /* ATW_TXFLAG_TCH */;
		sc->sc_txdescs[i].at_buf2 =
		    htole32(ATW_CDTXADDR(sc, ATW_NEXTTX(i)));
	}
	/* use ring mode */
	sc->sc_txdescs[ATW_NTXDESC - 1].at_flags |= htole32(ATW_TXFLAG_TER);
	ATW_CDTXSYNC(sc, 0, ATW_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = ATW_NTXDESC;
	sc->sc_txnext = 0;

	/*
	 * Initialize the transmit job descriptors.
	 */
	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = atw_add_rxbuf(sc, i)) != 0) {
				aprint_error_dev(sc->sc_dev,
				    "unable to allocate or map rx buffer %d, "
				    "error = %d\n", i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				atw_rxdrain(sc);
				goto out;
			}
		} else
			atw_init_rxdesc(sc, i);
	}
	sc->sc_rxptr = 0;

	/*
	 * Initialize the interrupt mask and enable interrupts.
	 */
	/* normal interrupts */
	sc->sc_inten =  ATW_INTR_TCI | ATW_INTR_TDU | ATW_INTR_RCI |
	    ATW_INTR_NISS | ATW_INTR_LINKON | ATW_INTR_BCNTC;

	/* abnormal interrupts */
	sc->sc_inten |= ATW_INTR_TPS | ATW_INTR_TLT | ATW_INTR_TRT |
	    ATW_INTR_TUF | ATW_INTR_RDU | ATW_INTR_RPS | ATW_INTR_AISS |
	    ATW_INTR_FBE | ATW_INTR_LINKOFF | ATW_INTR_TSFTF | ATW_INTR_TSCZ;

	sc->sc_linkint_mask = ATW_INTR_LINKON | ATW_INTR_LINKOFF |
	    ATW_INTR_BCNTC | ATW_INTR_TSFTF | ATW_INTR_TSCZ;
	sc->sc_rxint_mask = ATW_INTR_RCI | ATW_INTR_RDU;
	sc->sc_txint_mask = ATW_INTR_TCI | ATW_INTR_TUF | ATW_INTR_TLT |
	    ATW_INTR_TRT;

	sc->sc_linkint_mask &= sc->sc_inten;
	sc->sc_rxint_mask &= sc->sc_inten;
	sc->sc_txint_mask &= sc->sc_inten;

	ATW_WRITE(sc, ATW_IER, sc->sc_inten);
	ATW_WRITE(sc, ATW_STSR, 0xffffffff);

	DPRINTF(sc, ("%s: ATW_IER %08x, inten %08x\n",
	    device_xname(sc->sc_dev), ATW_READ(sc, ATW_IER), sc->sc_inten));

	/*
	 * Give the transmit and receive rings to the ADM8211.
	 */
	ATW_WRITE(sc, ATW_RDB, ATW_CDRXADDR(sc, sc->sc_rxptr));
	ATW_WRITE(sc, ATW_TDBD, ATW_CDTXADDR(sc, sc->sc_txnext));

	sc->sc_txthresh = 0;
	sc->sc_opmode = ATW_NAR_SR | ATW_NAR_ST |
	    sc->sc_txth[sc->sc_txthresh].txth_opmode;

	/* common 802.11 configuration */
	ic->ic_flags &= ~IEEE80211_F_IBSSON;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_AHDEMO: /* XXX */
	case IEEE80211_M_IBSS:
		ic->ic_flags |= IEEE80211_F_IBSSON;
		/*FALLTHROUGH*/
	case IEEE80211_M_HOSTAP: /* XXX */
		break;
	case IEEE80211_M_MONITOR: /* XXX */
		break;
	}

	switch (ic->ic_opmode) {
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
#ifndef IEEE80211_NO_HOSTAP
		ic->ic_bss->ni_intval = ic->ic_lintval;
		ic->ic_bss->ni_rssi = 0;
		ic->ic_bss->ni_rstamp = 0;
#endif /* !IEEE80211_NO_HOSTAP */
		break;
	default:					/* XXX */
		break;
	}

	sc->sc_wepctl = 0;

	atw_write_ssid(sc);
	atw_write_sup_rates(sc);
	atw_write_wep(sc);

	ic->ic_state = IEEE80211_S_INIT;

	/*
	 * Set the receive filter.  This will start the transmit and
	 * receive processes.
	 */
	atw_filter_setup(sc);

	/*
	 * Start the receive process.
	 */
	ATW_WRITE(sc, ATW_RDR, 0x1);

	/*
	 * Note that the interface is now running.
	 */
	ifp->if_flags |= IFF_RUNNING;

	/* send no beacons, yet. */
	atw_start_beacon(sc, 0);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		error = ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
 out:
	if (error) {
		ifp->if_flags &= ~IFF_RUNNING;
		sc->sc_tx_timer = 0;
		ifp->if_timer = 0;
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	}
#ifdef ATW_DEBUG
	atw_print_regs(sc, "end of init");
#endif /* ATW_DEBUG */

	return (error);
}

/* enable == 1: host control of RF3000/Si4126 through ATW_SYNCTL.
 *           0: MAC control of RF3000/Si4126.
 *
 * Applies power, or selects RF front-end? Sets reset condition.
 *
 * TBD support non-RFMD BBP, non-SiLabs synth.
 */
static void
atw_bbp_io_enable(struct atw_softc *sc, int enable)
{
	if (enable) {
		ATW_WRITE(sc, ATW_SYNRF,
		    ATW_SYNRF_SELRF|ATW_SYNRF_PE1|ATW_SYNRF_PHYRST);
		DELAY(atw_bbp_io_enable_delay);
	} else {
		ATW_WRITE(sc, ATW_SYNRF, 0);
		DELAY(atw_bbp_io_disable_delay); /* shorter for some reason */
	}
}

static int
atw_tune(struct atw_softc *sc)
{
	int rc;
	u_int chan;
	struct ieee80211com *ic = &sc->sc_ic;

	chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	if (chan == IEEE80211_CHAN_ANY)
		panic("%s: chan == IEEE80211_CHAN_ANY\n", __func__);

	if (chan == sc->sc_cur_chan)
		return 0;

	DPRINTF(sc, ("%s: chan %d -> %d\n", device_xname(sc->sc_dev),
	    sc->sc_cur_chan, chan));

	atw_idle(sc, ATW_NAR_SR|ATW_NAR_ST);

	atw_si4126_tune(sc, chan);
	if ((rc = atw_rf3000_tune(sc, chan)) != 0)
		printf("%s: failed to tune channel %d\n", device_xname(sc->sc_dev),
		    chan);

	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
	DELAY(atw_nar_delay);
	ATW_WRITE(sc, ATW_RDR, 0x1);

	if (rc == 0) {
		sc->sc_cur_chan = chan;
		sc->sc_rxtap.ar_chan_freq = sc->sc_txtap.at_chan_freq =
		    htole16(ic->ic_curchan->ic_freq);
		sc->sc_rxtap.ar_chan_flags = sc->sc_txtap.at_chan_flags =
		    htole16(ic->ic_curchan->ic_flags);
	}

	return rc;
}

#ifdef ATW_SYNDEBUG
static void
atw_si4126_print(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	u_int addr, val;

	val = 0;

	if (atw_debug < 3 || (ifp->if_flags & IFF_DEBUG) == 0)
		return;

	for (addr = 0; addr <= 8; addr++) {
		printf("%s: synth[%d] = ", device_xname(sc->sc_dev), addr);
		if (atw_si4126_read(sc, addr, &val) == 0) {
			printf("<unknown> (quitting print-out)\n");
			break;
		}
		printf("%05x\n", val);
	}
}
#endif /* ATW_SYNDEBUG */

/* Tune to channel chan by adjusting the Si4126 RF/IF synthesizer.
 *
 * The RF/IF synthesizer produces two reference frequencies for
 * the RF2948B transceiver.  The first frequency the RF2948B requires
 * is two times the so-called "intermediate frequency" (IF). Since
 * a SAW filter on the radio fixes the IF at 374 MHz, I program the
 * Si4126 to generate IF LO = 374 MHz x 2 = 748 MHz.  The second
 * frequency required by the transceiver is the radio frequency
 * (RF). This is a superheterodyne transceiver; for f(chan) the
 * center frequency of the channel we are tuning, RF = f(chan) -
 * IF.
 *
 * XXX I am told by SiLabs that the Si4126 will accept a broader range
 * of XIN than the 2-25 MHz mentioned by the datasheet, even *without*
 * XINDIV2 = 1.  I've tried this (it is necessary to double R) and it
 * works, but I have still programmed for XINDIV2 = 1 to be safe.
 */
static void
atw_si4126_tune(struct atw_softc *sc, u_int chan)
{
	u_int mhz;
	u_int R;
	u_int32_t gpio;
	u_int16_t gain;

#ifdef ATW_SYNDEBUG
	atw_si4126_print(sc);
#endif /* ATW_SYNDEBUG */

	if (chan == 14)
		mhz = 2484;
	else
		mhz = 2412 + 5 * (chan - 1);

	/* Tune IF to 748 MHz to suit the IF LO input of the
	 * RF2494B, which is 2 x IF. No need to set an IF divider
         * because an IF in 526 MHz - 952 MHz is allowed.
	 *
	 * XIN is 44.000 MHz, so divide it by two to get allowable
	 * range of 2-25 MHz. SiLabs tells me that this is not
	 * strictly necessary.
	 */

	if (atw_xindiv2)
		R = 44;
	else
		R = 88;

	/* Power-up RF, IF synthesizers. */
	atw_si4126_write(sc, SI4126_POWER,
	    SI4126_POWER_PDIB|SI4126_POWER_PDRB);

	/* set LPWR, too? */
	atw_si4126_write(sc, SI4126_MAIN,
	    (atw_xindiv2) ? SI4126_MAIN_XINDIV2 : 0);

	/* Set the phase-locked loop gain.  If RF2 N > 2047, then
	 * set KP2 to 1.
	 *
	 * REFDIF This is different from the reference driver, which
	 * always sets SI4126_GAIN to 0.
	 */
	gain = __SHIFTIN(((mhz - 374) > 2047) ? 1 : 0, SI4126_GAIN_KP2_MASK);

	atw_si4126_write(sc, SI4126_GAIN, gain);

	/* XIN = 44 MHz.
	 *
	 * If XINDIV2 = 1, IF = N/(2 * R) * XIN.  I choose N = 1496,
	 * R = 44 so that 1496/(2 * 44) * 44 MHz = 748 MHz.
	 *
	 * If XINDIV2 = 0, IF = N/R * XIN.  I choose N = 1496, R = 88
	 * so that 1496/88 * 44 MHz = 748 MHz.
	 */
	atw_si4126_write(sc, SI4126_IFN, 1496);

	atw_si4126_write(sc, SI4126_IFR, R);

#ifndef ATW_REFSLAVE
	/* Set RF1 arbitrarily. DO NOT configure RF1 after RF2, because
	 * then RF1 becomes the active RF synthesizer, even on the Si4126,
	 * which has no RF1!
	 */
	atw_si4126_write(sc, SI4126_RF1R, R);

	atw_si4126_write(sc, SI4126_RF1N, mhz - 374);
#endif

	/* N/R * XIN = RF. XIN = 44 MHz. We desire RF = mhz - IF,
	 * where IF = 374 MHz.  Let's divide XIN to 1 MHz. So R = 44.
	 * Now let's multiply it to mhz. So mhz - IF = N.
	 */
	atw_si4126_write(sc, SI4126_RF2R, R);

	atw_si4126_write(sc, SI4126_RF2N, mhz - 374);

	/* wait 100us from power-up for RF, IF to settle */
	DELAY(100);

	gpio = ATW_READ(sc, ATW_GPIO);
	gpio &= ~(ATW_GPIO_EN_MASK|ATW_GPIO_O_MASK|ATW_GPIO_I_MASK);
	gpio |= __SHIFTIN(1, ATW_GPIO_EN_MASK);

	if ((sc->sc_if.if_flags & IFF_LINK1) != 0 && chan != 14) {
		/* Set a Prism RF front-end to a special mode for channel 14?
		 *
		 * Apparently the SMC2635W needs this, although I don't think
		 * it has a Prism RF.
		 */
		gpio |= __SHIFTIN(1, ATW_GPIO_O_MASK);
	}
	ATW_WRITE(sc, ATW_GPIO, gpio);

#ifdef ATW_SYNDEBUG
	atw_si4126_print(sc);
#endif /* ATW_SYNDEBUG */
}

/* Baseline initialization of RF3000 BBP: set CCA mode and enable antenna
 * diversity.
 *
 * !!!
 * !!! Call this w/ Tx/Rx suspended, atw_idle(, ATW_NAR_ST|ATW_NAR_SR).
 * !!!
 */
static int
atw_rf3000_init(struct atw_softc *sc)
{
	int rc = 0;

	atw_bbp_io_enable(sc, 1);

	/* CCA is acquisition sensitive */
	rc = atw_rf3000_write(sc, RF3000_CCACTL,
	    __SHIFTIN(RF3000_CCACTL_MODE_BOTH, RF3000_CCACTL_MODE_MASK));

	if (rc != 0)
		goto out;

	/* enable diversity */
	rc = atw_rf3000_write(sc, RF3000_DIVCTL, RF3000_DIVCTL_ENABLE);

	if (rc != 0)
		goto out;

	/* sensible setting from a binary-only driver */
	rc = atw_rf3000_write(sc, RF3000_GAINCTL,
	    __SHIFTIN(0x1d, RF3000_GAINCTL_TXVGC_MASK));

	if (rc != 0)
		goto out;

	/* magic from a binary-only driver */
	rc = atw_rf3000_write(sc, RF3000_LOGAINCAL,
	    __SHIFTIN(0x38, RF3000_LOGAINCAL_CAL_MASK));

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_HIGAINCAL, RF3000_HIGAINCAL_DSSSPAD);

	if (rc != 0)
		goto out;

	/* XXX Reference driver remarks that Abocom sets this to 50.
	 * Meaning 0x50, I think....  50 = 0x32, which would set a bit
	 * in the "reserved" area of register RF3000_OPTIONS1.
	 */
	rc = atw_rf3000_write(sc, RF3000_OPTIONS1, sc->sc_rf3000_options1);

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_OPTIONS2, sc->sc_rf3000_options2);

	if (rc != 0)
		goto out;

out:
	atw_bbp_io_enable(sc, 0);
	return rc;
}

#ifdef ATW_BBPDEBUG
static void
atw_rf3000_print(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	u_int addr, val;

	if (atw_debug < 3 || (ifp->if_flags & IFF_DEBUG) == 0)
		return;

	for (addr = 0x01; addr <= 0x15; addr++) {
		printf("%s: bbp[%d] = \n", device_xname(sc->sc_dev), addr);
		if (atw_rf3000_read(sc, addr, &val) != 0) {
			printf("<unknown> (quitting print-out)\n");
			break;
		}
		printf("%08x\n", val);
	}
}
#endif /* ATW_BBPDEBUG */

/* Set the power settings on the BBP for channel `chan'. */
static int
atw_rf3000_tune(struct atw_softc *sc, u_int chan)
{
	int rc = 0;
	u_int32_t reg;
	u_int16_t txpower, lpf_cutoff, lna_gs_thresh;

	txpower = sc->sc_srom[ATW_SR_TXPOWER(chan)];
	lpf_cutoff = sc->sc_srom[ATW_SR_LPF_CUTOFF(chan)];
	lna_gs_thresh = sc->sc_srom[ATW_SR_LNA_GS_THRESH(chan)];

	/* odd channels: LSB, even channels: MSB */
	if (chan % 2 == 1) {
		txpower &= 0xFF;
		lpf_cutoff &= 0xFF;
		lna_gs_thresh &= 0xFF;
	} else {
		txpower >>= 8;
		lpf_cutoff >>= 8;
		lna_gs_thresh >>= 8;
	}

#ifdef ATW_BBPDEBUG
	atw_rf3000_print(sc);
#endif /* ATW_BBPDEBUG */

	DPRINTF(sc, ("%s: chan %d txpower %02x, lpf_cutoff %02x, "
	    "lna_gs_thresh %02x\n",
	    device_xname(sc->sc_dev), chan, txpower, lpf_cutoff, lna_gs_thresh));

	atw_bbp_io_enable(sc, 1);

	if ((rc = atw_rf3000_write(sc, RF3000_GAINCTL,
	    __SHIFTIN(txpower, RF3000_GAINCTL_TXVGC_MASK))) != 0)
		goto out;

	if ((rc = atw_rf3000_write(sc, RF3000_LOGAINCAL, lpf_cutoff)) != 0)
		goto out;

	if ((rc = atw_rf3000_write(sc, RF3000_HIGAINCAL, lna_gs_thresh)) != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_OPTIONS1, 0x0);

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_OPTIONS2, RF3000_OPTIONS2_LNAGS_DELAY);

	if (rc != 0)
		goto out;

#ifdef ATW_BBPDEBUG
	atw_rf3000_print(sc);
#endif /* ATW_BBPDEBUG */

out:
	atw_bbp_io_enable(sc, 0);

	/* set beacon, rts, atim transmit power */
	reg = ATW_READ(sc, ATW_PLCPHD);
	reg &= ~ATW_PLCPHD_SERVICE_MASK;
	reg |= __SHIFTIN(__SHIFTIN(txpower, RF3000_GAINCTL_TXVGC_MASK),
	    ATW_PLCPHD_SERVICE_MASK);
	ATW_WRITE(sc, ATW_PLCPHD, reg);
	DELAY(atw_plcphd_delay);

	return rc;
}

/* Write a register on the RF3000 baseband processor using the
 * registers provided by the ADM8211 for this purpose.
 *
 * Return 0 on success.
 */
static int
atw_rf3000_write(struct atw_softc *sc, u_int addr, u_int val)
{
	u_int32_t reg;
	int i;

	reg = sc->sc_bbpctl_wr |
	     __SHIFTIN(val & 0xff, ATW_BBPCTL_DATA_MASK) |
	     __SHIFTIN(addr & 0x7f, ATW_BBPCTL_ADDR_MASK);

	for (i = 20000 / atw_pseudo_milli; --i >= 0; ) {
		ATW_WRITE(sc, ATW_BBPCTL, reg);
		DELAY(2 * atw_pseudo_milli);
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_WR) == 0)
			break;
	}

	if (i < 0) {
		printf("%s: BBPCTL still busy\n", device_xname(sc->sc_dev));
		return ETIMEDOUT;
	}
	return 0;
}

/* Read a register on the RF3000 baseband processor using the registers
 * the ADM8211 provides for this purpose.
 *
 * The 7-bit register address is addr.  Record the 8-bit data in the register
 * in *val.
 *
 * Return 0 on success.
 *
 * XXX This does not seem to work. The ADM8211 must require more or
 * different magic to read the chip than to write it. Possibly some
 * of the magic I have derived from a binary-only driver concerns
 * the "chip address" (see the RF3000 manual).
 */
#ifdef ATW_BBPDEBUG
static int
atw_rf3000_read(struct atw_softc *sc, u_int addr, u_int *val)
{
	u_int32_t reg;
	int i;

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_RD|ATW_BBPCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: start atw_rf3000_read, BBPCTL busy\n",
		    device_xname(sc->sc_dev));
		return ETIMEDOUT;
	}

	reg = sc->sc_bbpctl_rd | __SHIFTIN(addr & 0x7f, ATW_BBPCTL_ADDR_MASK);

	ATW_WRITE(sc, ATW_BBPCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_RD) == 0)
			break;
	}

	ATW_CLR(sc, ATW_BBPCTL, ATW_BBPCTL_RD);

	if (i < 0) {
		printf("%s: atw_rf3000_read wrote %08x; BBPCTL still busy\n",
		    device_xname(sc->sc_dev), reg);
		return ETIMEDOUT;
	}
	if (val != NULL)
		*val = __SHIFTOUT(reg, ATW_BBPCTL_DATA_MASK);
	return 0;
}
#endif /* ATW_BBPDEBUG */

/* Write a register on the Si4126 RF/IF synthesizer using the registers
 * provided by the ADM8211 for that purpose.
 *
 * val is 18 bits of data, and val is the 4-bit address of the register.
 *
 * Return 0 on success.
 */
static void
atw_si4126_write(struct atw_softc *sc, u_int addr, u_int val)
{
	uint32_t bits, mask, reg;
	const int nbits = 22;

	KASSERT((addr & ~__SHIFTOUT_MASK(SI4126_TWI_ADDR_MASK)) == 0);
	KASSERT((val & ~__SHIFTOUT_MASK(SI4126_TWI_DATA_MASK)) == 0);

	bits = __SHIFTIN(val, SI4126_TWI_DATA_MASK) |
	       __SHIFTIN(addr, SI4126_TWI_ADDR_MASK);

	reg = ATW_SYNRF_SELSYN;
	/* reference driver: reset Si4126 serial bus to initial
	 * conditions?
	 */
	ATW_WRITE(sc, ATW_SYNRF, reg | ATW_SYNRF_LEIF);
	ATW_WRITE(sc, ATW_SYNRF, reg);

	for (mask = __BIT(nbits - 1); mask != 0; mask >>= 1) {
		if ((bits & mask) != 0)
			reg |= ATW_SYNRF_SYNDATA;
		else
			reg &= ~ATW_SYNRF_SYNDATA;
		ATW_WRITE(sc, ATW_SYNRF, reg);
		ATW_WRITE(sc, ATW_SYNRF, reg | ATW_SYNRF_SYNCLK);
		ATW_WRITE(sc, ATW_SYNRF, reg);
	}
	ATW_WRITE(sc, ATW_SYNRF, reg | ATW_SYNRF_LEIF);
	ATW_WRITE(sc, ATW_SYNRF, 0x0);
}

/* Read 18-bit data from the 4-bit address addr in Si4126
 * RF synthesizer and write the data to *val. Return 0 on success.
 *
 * XXX This does not seem to work. The ADM8211 must require more or
 * different magic to read the chip than to write it.
 */
#ifdef ATW_SYNDEBUG
static int
atw_si4126_read(struct atw_softc *sc, u_int addr, u_int *val)
{
	u_int32_t reg;
	int i;

	KASSERT((addr & ~__SHIFTOUT_MASK(SI4126_TWI_ADDR_MASK)) == 0);

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_RD|ATW_SYNCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: start atw_si4126_read, SYNCTL busy\n",
		    device_xname(sc->sc_dev));
		return ETIMEDOUT;
	}

	reg = sc->sc_synctl_rd | __SHIFTIN(addr, ATW_SYNCTL_DATA_MASK);

	ATW_WRITE(sc, ATW_SYNCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_RD) == 0)
			break;
	}

	ATW_CLR(sc, ATW_SYNCTL, ATW_SYNCTL_RD);

	if (i < 0) {
		printf("%s: atw_si4126_read wrote %#08x, SYNCTL still busy\n",
		    device_xname(sc->sc_dev), reg);
		return ETIMEDOUT;
	}
	if (val != NULL)
		*val = __SHIFTOUT(ATW_READ(sc, ATW_SYNCTL),
		                       ATW_SYNCTL_DATA_MASK);
	return 0;
}
#endif /* ATW_SYNDEBUG */

/* XXX is the endianness correct? test. */
#define	atw_calchash(addr) \
	(ether_crc32_le((addr), IEEE80211_ADDR_LEN) & __BITS(5, 0))

/*
 * atw_filter_setup:
 *
 *	Set the ADM8211's receive filter.
 */
static void
atw_filter_setup(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ethercom *ec = &sc->sc_ec;
	struct ifnet *ifp = &sc->sc_if;
	int hash;
	u_int32_t hashes[2];
	struct ether_multi *enm;
	struct ether_multistep step;

	/* According to comments in tlp_al981_filter_setup
	 * (dev/ic/tulip.c) the ADMtek AL981 does not like for its
	 * multicast filter to be set while it is running.  Hopefully
	 * the ADM8211 is not the same!
	 */
	if ((ifp->if_flags & IFF_RUNNING) != 0)
		atw_idle(sc, ATW_NAR_SR);

	sc->sc_opmode &= ~(ATW_NAR_PB|ATW_NAR_PR|ATW_NAR_MM);
	ifp->if_flags &= ~IFF_ALLMULTI;

	/* XXX in scan mode, do not filter packets.  Maybe this is
	 * unnecessary.
	 */
	if (ic->ic_state == IEEE80211_S_SCAN ||
	    (ifp->if_flags & IFF_PROMISC) != 0) {
		sc->sc_opmode |= ATW_NAR_PR | ATW_NAR_PB;
		goto allmulti;
	}

	hashes[0] = hashes[1] = 0x0;

	/*
	 * Program the 64-bit multicast hash filter.
	 */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		hash = atw_calchash(enm->enm_addrlo);
		hashes[hash >> 5] |= 1 << (hash & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
		sc->sc_opmode |= ATW_NAR_MM;
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

allmulti:
	sc->sc_opmode |= ATW_NAR_MM;
	ifp->if_flags |= IFF_ALLMULTI;
	hashes[0] = hashes[1] = 0xffffffff;

setit:
	ATW_WRITE(sc, ATW_MAR0, hashes[0]);
	ATW_WRITE(sc, ATW_MAR1, hashes[1]);
	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
	DELAY(atw_nar_delay);
	ATW_WRITE(sc, ATW_RDR, 0x1);

	DPRINTF(sc, ("%s: ATW_NAR %08x opmode %08x\n", device_xname(sc->sc_dev),
	    ATW_READ(sc, ATW_NAR), sc->sc_opmode));
}

/* Tell the ADM8211 our preferred BSSID. The ADM8211 must match
 * a beacon's BSSID and SSID against the preferred BSSID and SSID
 * before it will raise ATW_INTR_LINKON. When the ADM8211 receives
 * no beacon with the preferred BSSID and SSID in the number of
 * beacon intervals given in ATW_BPLI, then it raises ATW_INTR_LINKOFF.
 */
static void
atw_write_bssid(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int8_t *bssid;

	bssid = ic->ic_bss->ni_bssid;

	ATW_WRITE(sc, ATW_BSSID0,
	    __SHIFTIN(bssid[0], ATW_BSSID0_BSSIDB0_MASK) |
	    __SHIFTIN(bssid[1], ATW_BSSID0_BSSIDB1_MASK) |
	    __SHIFTIN(bssid[2], ATW_BSSID0_BSSIDB2_MASK) |
	    __SHIFTIN(bssid[3], ATW_BSSID0_BSSIDB3_MASK));

	ATW_WRITE(sc, ATW_ABDA1,
	    (ATW_READ(sc, ATW_ABDA1) &
	    ~(ATW_ABDA1_BSSIDB4_MASK|ATW_ABDA1_BSSIDB5_MASK)) |
	    __SHIFTIN(bssid[4], ATW_ABDA1_BSSIDB4_MASK) |
	    __SHIFTIN(bssid[5], ATW_ABDA1_BSSIDB5_MASK));

	DPRINTF(sc, ("%s: BSSID %s -> ", device_xname(sc->sc_dev),
	    ether_sprintf(sc->sc_bssid)));
	DPRINTF(sc, ("%s\n", ether_sprintf(bssid)));

	memcpy(sc->sc_bssid, bssid, sizeof(sc->sc_bssid));
}

/* Write buflen bytes from buf to SRAM starting at the SRAM's ofs'th
 * 16-bit word.
 */
static void
atw_write_sram(struct atw_softc *sc, u_int ofs, u_int8_t *buf, u_int buflen)
{
	u_int i;
	u_int8_t *ptr;

	memcpy(&sc->sc_sram[ofs], buf, buflen);

	KASSERT(ofs % 2 == 0 && buflen % 2 == 0);

	KASSERT(buflen + ofs <= sc->sc_sramlen);

	ptr = &sc->sc_sram[ofs];

	for (i = 0; i < buflen; i += 2) {
		ATW_WRITE(sc, ATW_WEPCTL, ATW_WEPCTL_WR |
		    __SHIFTIN((ofs + i) / 2, ATW_WEPCTL_TBLADD_MASK));
		DELAY(atw_writewep_delay);

		ATW_WRITE(sc, ATW_WESK,
		    __SHIFTIN((ptr[i + 1] << 8) | ptr[i], ATW_WESK_DATA_MASK));
		DELAY(atw_writewep_delay);
	}
	ATW_WRITE(sc, ATW_WEPCTL, sc->sc_wepctl); /* restore WEP condition */

	if (sc->sc_if.if_flags & IFF_DEBUG) {
		int n_octets = 0;
		printf("%s: wrote %d bytes at 0x%x wepctl 0x%08x\n",
		    device_xname(sc->sc_dev), buflen, ofs, sc->sc_wepctl);
		for (i = 0; i < buflen; i++) {
			printf(" %02x", ptr[i]);
			if (++n_octets % 24 == 0)
				printf("\n");
		}
		if (n_octets % 24 != 0)
			printf("\n");
	}
}

static int
atw_key_delete(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	struct atw_softc *sc = ic->ic_ifp->if_softc;
	u_int keyix = k->wk_keyix;

	DPRINTF(sc, ("%s: delete key %u\n", __func__, keyix));

	if (keyix >= IEEE80211_WEP_NKID)
		return 0;
	if (k->wk_keylen != 0)
		sc->sc_flags &= ~ATWF_WEP_SRAM_VALID;

	return 1;
}

static int
atw_key_set(struct ieee80211com *ic, const struct ieee80211_key *k,
	const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	struct atw_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, ("%s: set key %u\n", __func__, k->wk_keyix));

	if (k->wk_keyix >= IEEE80211_WEP_NKID)
		return 0;

	sc->sc_flags &= ~ATWF_WEP_SRAM_VALID;

	return 1;
}

static void
atw_key_update_begin(struct ieee80211com *ic)
{
#ifdef ATW_DEBUG
	struct ifnet *ifp = ic->ic_ifp;
	struct atw_softc *sc = ifp->if_softc;
#endif

	DPRINTF(sc, ("%s:\n", __func__));
}

static void
atw_key_update_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct atw_softc *sc = ifp->if_softc;

	DPRINTF(sc, ("%s:\n", __func__));

	if ((sc->sc_flags & ATWF_WEP_SRAM_VALID) != 0)
		return;
	if (!device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
		return;
	atw_idle(sc, ATW_NAR_SR | ATW_NAR_ST);
	atw_write_wep(sc);
	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
	DELAY(atw_nar_delay);
	ATW_WRITE(sc, ATW_RDR, 0x1);
}

/* Write WEP keys from the ieee80211com to the ADM8211's SRAM. */
static void
atw_write_wep(struct atw_softc *sc)
{
#if 0
	struct ieee80211com *ic = &sc->sc_ic;
	u_int32_t reg;
	int i;
#endif
	/* SRAM shared-key record format: key0 flags key1 ... key12 */
	u_int8_t buf[IEEE80211_WEP_NKID]
	            [1 /* key[0] */ + 1 /* flags */ + 12 /* key[1 .. 12] */];

	sc->sc_wepctl = 0;
	ATW_WRITE(sc, ATW_WEPCTL, sc->sc_wepctl);

	memset(&buf[0][0], 0, sizeof(buf));

#if 0
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (ic->ic_nw_keys[i].wk_keylen > 5) {
			buf[i][1] = ATW_WEP_ENABLED | ATW_WEP_104BIT;
		} else if (ic->ic_nw_keys[i].wk_keylen != 0) {
			buf[i][1] = ATW_WEP_ENABLED;
		} else {
			buf[i][1] = 0;
			continue;
		}
		buf[i][0] = ic->ic_nw_keys[i].wk_key[0];
		memcpy(&buf[i][2], &ic->ic_nw_keys[i].wk_key[1],
		    ic->ic_nw_keys[i].wk_keylen - 1);
	}

	reg = ATW_READ(sc, ATW_MACTEST);
	reg |= ATW_MACTEST_MMI_USETXCLK | ATW_MACTEST_FORCE_KEYID;
	reg &= ~ATW_MACTEST_KEYID_MASK;
	reg |= __SHIFTIN(ic->ic_def_txkey, ATW_MACTEST_KEYID_MASK);
	ATW_WRITE(sc, ATW_MACTEST, reg);

	if ((ic->ic_flags & IEEE80211_F_PRIVACY) != 0)
		sc->sc_wepctl |= ATW_WEPCTL_WEPENABLE;

	switch (sc->sc_rev) {
	case ATW_REVISION_AB:
	case ATW_REVISION_AF:
		/* Bypass WEP on Rx. */
		sc->sc_wepctl |= ATW_WEPCTL_WEPRXBYP;
		break;
	default:
		break;
	}
#endif

	atw_write_sram(sc, ATW_SRAM_ADDR_SHARED_KEY, (u_int8_t*)&buf[0][0],
	    sizeof(buf));

	sc->sc_flags |= ATWF_WEP_SRAM_VALID;
}

static void
atw_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct atw_softc *sc = (struct atw_softc *)ic->ic_ifp->if_softc;

	/* The ADM8211A answers probe requests. TBD ADM8211B/C. */
	if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ)
		return;

	(*sc->sc_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    ic->ic_state == IEEE80211_S_RUN) {
			if (le64toh(ni->ni_tstamp.tsf) >= atw_get_tsft(sc))
				(void)ieee80211_ibss_merge(ni);
		}
		break;
	default:
		break;
	}
	return;
}

/* Write the SSID in the ieee80211com to the SRAM on the ADM8211.
 * In ad hoc mode, the SSID is written to the beacons sent by the
 * ADM8211. In both ad hoc and infrastructure mode, beacons received
 * with matching SSID affect ATW_INTR_LINKON/ATW_INTR_LINKOFF
 * indications.
 */
static void
atw_write_ssid(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	/* 34 bytes are reserved in ADM8211 SRAM for the SSID, but
	 * it only expects the element length, not its ID.
	 */
	u_int8_t buf[roundup(1 /* length */ + IEEE80211_NWID_LEN, 2)];

	memset(buf, 0, sizeof(buf));
	buf[0] = ic->ic_bss->ni_esslen;
	memcpy(&buf[1], ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen);

	atw_write_sram(sc, ATW_SRAM_ADDR_SSID, buf,
	    roundup(1 + ic->ic_bss->ni_esslen, 2));
}

/* Write the supported rates in the ieee80211com to the SRAM of the ADM8211.
 * In ad hoc mode, the supported rates are written to beacons sent by the
 * ADM8211.
 */
static void
atw_write_sup_rates(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	/* 14 bytes are probably (XXX) reserved in the ADM8211 SRAM for
	 * supported rates
	 */
	u_int8_t buf[roundup(1 /* length */ + IEEE80211_RATE_SIZE, 2)];

	memset(buf, 0, sizeof(buf));

	buf[0] = ic->ic_bss->ni_rates.rs_nrates;

	memcpy(&buf[1], ic->ic_bss->ni_rates.rs_rates,
	    ic->ic_bss->ni_rates.rs_nrates);

	atw_write_sram(sc, ATW_SRAM_ADDR_SUPRATES, buf, sizeof(buf));
}

/* Start/stop sending beacons. */
void
atw_start_beacon(struct atw_softc *sc, int start)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t chan;
	uint32_t bcnt, bpli, cap0, cap1, capinfo;
	size_t len;

	if (!device_is_active(sc->sc_dev))
		return;

	/* start beacons */
	len = sizeof(struct ieee80211_frame) +
	    8 /* timestamp */ + 2 /* beacon interval */ +
	    2 /* capability info */ +
	    2 + ic->ic_bss->ni_esslen /* SSID element */ +
	    2 + ic->ic_bss->ni_rates.rs_nrates /* rates element */ +
	    3 /* DS parameters */ +
	    IEEE80211_CRC_LEN;

	bcnt = ATW_READ(sc, ATW_BCNT) & ~ATW_BCNT_BCNT_MASK;
	cap0 = ATW_READ(sc, ATW_CAP0) & ~ATW_CAP0_CHN_MASK;
	cap1 = ATW_READ(sc, ATW_CAP1) & ~ATW_CAP1_CAPI_MASK;

	ATW_WRITE(sc, ATW_BCNT, bcnt);
	ATW_WRITE(sc, ATW_CAP1, cap1);

	if (!start)
		return;

	/* TBD use ni_capinfo */

	capinfo = 0;
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;

	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		len += 4; /* IBSS parameters */
		capinfo |= IEEE80211_CAPINFO_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		/* XXX 6-byte minimum TIM */
		len += atw_beacon_len_adjust;
		capinfo |= IEEE80211_CAPINFO_ESS;
		break;
	default:
		return;
	}

	/* set listen interval
	 * XXX do software units agree w/ hardware?
	 */
	bpli = __SHIFTIN(ic->ic_bss->ni_intval, ATW_BPLI_BP_MASK) |
	    __SHIFTIN(ic->ic_lintval / ic->ic_bss->ni_intval, ATW_BPLI_LI_MASK);

	chan = ieee80211_chan2ieee(ic, ic->ic_curchan);

	bcnt |= __SHIFTIN(len, ATW_BCNT_BCNT_MASK);
	cap0 |= __SHIFTIN(chan, ATW_CAP0_CHN_MASK);
	cap1 |= __SHIFTIN(capinfo, ATW_CAP1_CAPI_MASK);

	ATW_WRITE(sc, ATW_BCNT, bcnt);
	ATW_WRITE(sc, ATW_BPLI, bpli);
	ATW_WRITE(sc, ATW_CAP0, cap0);
	ATW_WRITE(sc, ATW_CAP1, cap1);

	DPRINTF(sc, ("%s: atw_start_beacon reg[ATW_BCNT] = %08x\n",
	    device_xname(sc->sc_dev), bcnt));

	DPRINTF(sc, ("%s: atw_start_beacon reg[ATW_CAP1] = %08x\n",
	    device_xname(sc->sc_dev), cap1));
}

/* Return the 32 lsb of the last TSFT divisible by ival. */
static inline uint32_t
atw_last_even_tsft(uint32_t tsfth, uint32_t tsftl, uint32_t ival)
{
	/* Following the reference driver's lead, I compute
	 *
	 *   (uint32_t)((((uint64_t)tsfth << 32) | tsftl) % ival)
	 *
	 * without using 64-bit arithmetic, using the following
	 * relationship:
	 *
	 *     (0x100000000 * H + L) % m
	 *   = ((0x100000000 % m) * H + L) % m
	 *   = (((0xffffffff + 1) % m) * H + L) % m
	 *   = ((0xffffffff % m + 1 % m) * H + L) % m
	 *   = ((0xffffffff % m + 1) * H + L) % m
	 */
	return ((0xFFFFFFFF % ival + 1) * tsfth + tsftl) % ival;
}

static uint64_t
atw_get_tsft(struct atw_softc *sc)
{
	int i;
	uint32_t tsfth, tsftl;
	for (i = 0; i < 2; i++) {
		tsfth = ATW_READ(sc, ATW_TSFTH);
		tsftl = ATW_READ(sc, ATW_TSFTL);
		if (ATW_READ(sc, ATW_TSFTH) == tsfth)
			break;
	}
	return ((uint64_t)tsfth << 32) | tsftl;
}

/* If we've created an IBSS, write the TSF time in the ADM8211 to
 * the ieee80211com.
 *
 * Predict the next target beacon transmission time (TBTT) and
 * write it to the ADM8211.
 */
static void
atw_predict_beacon(struct atw_softc *sc)
{
#define TBTTOFS 20 /* TU */

	struct ieee80211com *ic = &sc->sc_ic;
	uint64_t tsft;
	uint32_t ival, past_even, tbtt, tsfth, tsftl;
	union {
		uint64_t	word;
		uint8_t		tstamp[8];
	} u;

	if ((ic->ic_opmode == IEEE80211_M_HOSTAP) ||
	    ((ic->ic_opmode == IEEE80211_M_IBSS) &&
	     (ic->ic_flags & IEEE80211_F_SIBSS))) {
		tsft = atw_get_tsft(sc);
		u.word = htole64(tsft);
		(void)memcpy(&ic->ic_bss->ni_tstamp, &u.tstamp[0],
		    sizeof(ic->ic_bss->ni_tstamp));
	} else
		tsft = le64toh(ic->ic_bss->ni_tstamp.tsf);

	ival = ic->ic_bss->ni_intval * IEEE80211_DUR_TU;

	tsftl = tsft & 0xFFFFFFFF;
	tsfth = tsft >> 32;

	/* We sent/received the last beacon `past' microseconds
	 * after the interval divided the TSF timer.
	 */
	past_even = tsftl - atw_last_even_tsft(tsfth, tsftl, ival);

	/* Skip ten beacons so that the TBTT cannot pass before
	 * we've programmed it.  Ten is an arbitrary number.
	 */
	tbtt = past_even + ival * 10;

	ATW_WRITE(sc, ATW_TOFS1,
	    __SHIFTIN(1, ATW_TOFS1_TSFTOFSR_MASK) |
	    __SHIFTIN(TBTTOFS, ATW_TOFS1_TBTTOFS_MASK) |
	    __SHIFTIN(__SHIFTOUT(tbtt - TBTTOFS * IEEE80211_DUR_TU,
	        ATW_TBTTPRE_MASK), ATW_TOFS1_TBTTPRE_MASK));
#undef TBTTOFS
}

static void
atw_next_scan(void *arg)
{
	struct atw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	/* don't call atw_start w/o network interrupts blocked */
	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
	splx(s);
}

/* Synchronize the hardware state with the software state. */
static int
atw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct atw_softc *sc = ifp->if_softc;
	int error = 0;

	callout_stop(&sc->sc_scan_ch);

	switch (nstate) {
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		atw_write_bssid(sc);
		error = atw_tune(sc);
		break;
	case IEEE80211_S_INIT:
		callout_stop(&sc->sc_scan_ch);
		sc->sc_cur_chan = IEEE80211_CHAN_ANY;
		atw_start_beacon(sc, 0);
		break;
	case IEEE80211_S_SCAN:
		error = atw_tune(sc);
		callout_reset(&sc->sc_scan_ch, atw_dwelltime * hz / 1000,
		    atw_next_scan, sc);
		break;
	case IEEE80211_S_RUN:
		error = atw_tune(sc);
		atw_write_bssid(sc);
		atw_write_ssid(sc);
		atw_write_sup_rates(sc);

		if (ic->ic_opmode == IEEE80211_M_AHDEMO ||
		    ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		/* set listen interval
		 * XXX do software units agree w/ hardware?
		 */
		ATW_WRITE(sc, ATW_BPLI,
		    __SHIFTIN(ic->ic_bss->ni_intval, ATW_BPLI_BP_MASK) |
		    __SHIFTIN(ic->ic_lintval / ic->ic_bss->ni_intval,
			   ATW_BPLI_LI_MASK));

		DPRINTF(sc, ("%s: reg[ATW_BPLI] = %08x\n", device_xname(sc->sc_dev),
		    ATW_READ(sc, ATW_BPLI)));

		atw_predict_beacon(sc);

		switch (ic->ic_opmode) {
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_IBSS:
			atw_start_beacon(sc, 1);
			break;
		case IEEE80211_M_MONITOR:
		case IEEE80211_M_STA:
			break;
		}

		break;
	}
	return (error != 0) ? error : (*sc->sc_newstate)(ic, nstate, arg);
}

/*
 * atw_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
atw_add_rxbuf(struct atw_softc *sc, int idx)
{
	struct atw_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't load rx DMA map %d, error = %d\n",
		    idx, error);
		panic("atw_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	atw_init_rxdesc(sc, idx);

	return (0);
}

/*
 * Release any queued transmit buffers.
 */
void
atw_txdrain(struct atw_softc *sc)
{
	struct atw_txsoft *txs;

	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
		sc->sc_txfree += txs->txs_ndescs;
	}

	KASSERT((sc->sc_if.if_flags & IFF_RUNNING) == 0 ||
	        !(SIMPLEQ_EMPTY(&sc->sc_txfreeq) ||
		  sc->sc_txfree != ATW_NTXDESC));
	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	sc->sc_tx_timer = 0;
}

/*
 * atw_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
void
atw_stop(struct ifnet *ifp, int disable)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	if (device_is_active(sc->sc_dev)) {
		/* Disable interrupts. */
		ATW_WRITE(sc, ATW_IER, 0);

		/* Stop the transmit and receive processes. */
		ATW_WRITE(sc, ATW_NAR, 0);
		DELAY(atw_nar_delay);
		ATW_WRITE(sc, ATW_TDBD, 0);
		ATW_WRITE(sc, ATW_TDBP, 0);
		ATW_WRITE(sc, ATW_RDB, 0);
	}

	sc->sc_opmode = 0;

	atw_txdrain(sc);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;

	if (disable)
		pmf_device_suspend(sc->sc_dev, &sc->sc_qual);
}

/*
 * atw_rxdrain:
 *
 *	Drain the receive queue.
 */
void
atw_rxdrain(struct atw_softc *sc)
{
	struct atw_rxsoft *rxs;
	int i;

	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL)
			continue;
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
		m_freem(rxs->rxs_mbuf);
		rxs->rxs_mbuf = NULL;
	}
}

/*
 * atw_detach:
 *
 *	Detach an ADM8211 interface.
 */
int
atw_detach(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct atw_rxsoft *rxs;
	struct atw_txsoft *txs;
	int i;

	/*
	 * Succeed now if there isn't any work to do.
	 */
	if ((sc->sc_flags & ATWF_ATTACHED) == 0)
		return (0);

	pmf_device_deregister(sc->sc_dev);

	callout_stop(&sc->sc_scan_ch);

	ieee80211_ifdetach(&sc->sc_ic);
	if_detach(ifp);

	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rxs->rxs_dmamap);
	}
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, txs->txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sizeof(struct atw_control_data));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);

	if (sc->sc_srom)
		free(sc->sc_srom, M_DEVBUF);

	atw_evcnt_detach(sc);

	return (0);
}

/* atw_shutdown: make sure the interface is stopped at reboot time. */
bool
atw_shutdown(device_t self, int flags)
{
	struct atw_softc *sc = device_private(self);

	atw_stop(&sc->sc_if, 1);
	return true;
}

#if 0
static void
atw_workaround1(struct atw_softc *sc)
{
	uint32_t test1;

	test1 = ATW_READ(sc, ATW_TEST1);

	sc->sc_misc_ev.ev_count++;

	if ((test1 & ATW_TEST1_RXPKT1IN) != 0) {
		sc->sc_rxpkt1in_ev.ev_count++;
		return;
	}
	if (__SHIFTOUT(test1, ATW_TEST1_RRA_MASK) ==
	    __SHIFTOUT(test1, ATW_TEST1_RWA_MASK)) {
		sc->sc_rxamatch_ev.ev_count++;
		return;
	}
	sc->sc_workaround1_ev.ev_count++;
	(void)atw_init(&sc->sc_if);
}
#endif

int
atw_intr(void *arg)
{
	struct atw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	u_int32_t status, rxstatus, txstatus, linkstatus;
	int handled = 0, txthresh;

#ifdef DEBUG
	if (!device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
		panic("%s: atw_intr: not enabled", device_xname(sc->sc_dev));
#endif

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
		return (0);

	for (;;) {
		status = ATW_READ(sc, ATW_STSR);

		if (status)
			ATW_WRITE(sc, ATW_STSR, status);

#ifdef ATW_DEBUG
#define PRINTINTR(flag) do { \
	if ((status & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)

		if (atw_debug > 1 && status) {
			const char *delim = "<";

			printf("%s: reg[STSR] = %x",
			    device_xname(sc->sc_dev), status);

			PRINTINTR(ATW_INTR_FBE);
			PRINTINTR(ATW_INTR_LINKOFF);
			PRINTINTR(ATW_INTR_LINKON);
			PRINTINTR(ATW_INTR_RCI);
			PRINTINTR(ATW_INTR_RDU);
			PRINTINTR(ATW_INTR_REIS);
			PRINTINTR(ATW_INTR_RPS);
			PRINTINTR(ATW_INTR_TCI);
			PRINTINTR(ATW_INTR_TDU);
			PRINTINTR(ATW_INTR_TLT);
			PRINTINTR(ATW_INTR_TPS);
			PRINTINTR(ATW_INTR_TRT);
			PRINTINTR(ATW_INTR_TUF);
			PRINTINTR(ATW_INTR_BCNTC);
			PRINTINTR(ATW_INTR_ATIME);
			PRINTINTR(ATW_INTR_TBTT);
			PRINTINTR(ATW_INTR_TSCZ);
			PRINTINTR(ATW_INTR_TSFTF);
			printf(">\n");
		}
#undef PRINTINTR
#endif /* ATW_DEBUG */

		if ((status & sc->sc_inten) == 0)
			break;

		handled = 1;

		rxstatus = status & sc->sc_rxint_mask;
		txstatus = status & sc->sc_txint_mask;
		linkstatus = status & sc->sc_linkint_mask;

		if (linkstatus) {
			atw_linkintr(sc, linkstatus);
		}

		if (rxstatus) {
			/* Grab any new packets. */
			atw_rxintr(sc);

			if (rxstatus & ATW_INTR_RDU) {
				printf("%s: receive ring overrun\n",
				    device_xname(sc->sc_dev));
				/* Get the receive process going again. */
				ATW_WRITE(sc, ATW_RDR, 0x1);
			}
		}

		if (txstatus) {
			/* Sweep up transmit descriptors. */
			atw_txintr(sc, txstatus);

			if (txstatus & ATW_INTR_TLT) {
				DPRINTF(sc, ("%s: tx lifetime exceeded\n",
				    device_xname(sc->sc_dev)));
				(void)atw_init(&sc->sc_if);
			}

			if (txstatus & ATW_INTR_TRT) {
				DPRINTF(sc, ("%s: tx retry limit exceeded\n",
				    device_xname(sc->sc_dev)));
			}

			/* If Tx under-run, increase our transmit threshold
			 * if another is available.
			 */
			txthresh = sc->sc_txthresh + 1;
			if ((txstatus & ATW_INTR_TUF) &&
			    sc->sc_txth[txthresh].txth_name != NULL) {
				/* Idle the transmit process. */
				atw_idle(sc, ATW_NAR_ST);

				sc->sc_txthresh = txthresh;
				sc->sc_opmode &= ~(ATW_NAR_TR_MASK|ATW_NAR_SF);
				sc->sc_opmode |=
				    sc->sc_txth[txthresh].txth_opmode;
				printf("%s: transmit underrun; new "
				    "threshold: %s\n", device_xname(sc->sc_dev),
				    sc->sc_txth[txthresh].txth_name);

				/* Set the new threshold and restart
				 * the transmit process.
				 */
				ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
				DELAY(atw_nar_delay);
				ATW_WRITE(sc, ATW_TDR, 0x1);
				/* XXX Log every Nth underrun from
				 * XXX now on?
				 */
			}
		}

		if (status & (ATW_INTR_TPS|ATW_INTR_RPS)) {
			if (status & ATW_INTR_TPS)
				printf("%s: transmit process stopped\n",
				    device_xname(sc->sc_dev));
			if (status & ATW_INTR_RPS)
				printf("%s: receive process stopped\n",
				    device_xname(sc->sc_dev));
			(void)atw_init(ifp);
			break;
		}

		if (status & ATW_INTR_FBE) {
			aprint_error_dev(sc->sc_dev, "fatal bus error\n");
			(void)atw_init(ifp);
			break;
		}

		/*
		 * Not handled:
		 *
		 *	Transmit buffer unavailable -- normal
		 *	condition, nothing to do, really.
		 *
		 *	Early receive interrupt -- not available on
		 *	all chips, we just use RI.  We also only
		 *	use single-segment receive DMA, so this
		 *	is mostly useless.
		 *
		 *      TBD others
		 */
	}

	/* Try to get more packets going. */
	atw_start(ifp);

	return (handled);
}

/*
 * atw_idle:
 *
 *	Cause the transmit and/or receive processes to go idle.
 *
 *      XXX It seems that the ADM8211 will not signal the end of the Rx/Tx
 *	process in STSR if I clear SR or ST after the process has already
 *	ceased. Fair enough. But the Rx process status bits in ATW_TEST0
 *      do not seem to be too reliable. Perhaps I have the sense of the
 *	Rx bits switched with the Tx bits?
 */
void
atw_idle(struct atw_softc *sc, u_int32_t bits)
{
	u_int32_t ackmask = 0, opmode, stsr, test0;
	int i, s;

	s = splnet();

	opmode = sc->sc_opmode & ~bits;

	if (bits & ATW_NAR_SR)
		ackmask |= ATW_INTR_RPS;

	if (bits & ATW_NAR_ST) {
		ackmask |= ATW_INTR_TPS;
		/* set ATW_NAR_HF to flush TX FIFO. */
		opmode |= ATW_NAR_HF;
	}

	ATW_WRITE(sc, ATW_NAR, opmode);
	DELAY(atw_nar_delay);

	for (i = 0; i < 1000; i++) {
		stsr = ATW_READ(sc, ATW_STSR);
		if ((stsr & ackmask) == ackmask)
			break;
		DELAY(10);
	}

	ATW_WRITE(sc, ATW_STSR, stsr & ackmask);

	if ((stsr & ackmask) == ackmask)
		goto out;

	test0 = ATW_READ(sc, ATW_TEST0);

	if ((bits & ATW_NAR_ST) != 0 && (stsr & ATW_INTR_TPS) == 0 &&
	    (test0 & ATW_TEST0_TS_MASK) != ATW_TEST0_TS_STOPPED) {
		printf("%s: transmit process not idle [%s]\n",
		    device_xname(sc->sc_dev),
		    atw_tx_state[__SHIFTOUT(test0, ATW_TEST0_TS_MASK)]);
		printf("%s: bits %08x test0 %08x stsr %08x\n",
		    device_xname(sc->sc_dev), bits, test0, stsr);
	}

	if ((bits & ATW_NAR_SR) != 0 && (stsr & ATW_INTR_RPS) == 0 &&
	    (test0 & ATW_TEST0_RS_MASK) != ATW_TEST0_RS_STOPPED) {
		DPRINTF2(sc, ("%s: receive process not idle [%s]\n",
		    device_xname(sc->sc_dev),
		    atw_rx_state[__SHIFTOUT(test0, ATW_TEST0_RS_MASK)]));
		DPRINTF2(sc, ("%s: bits %08x test0 %08x stsr %08x\n",
		    device_xname(sc->sc_dev), bits, test0, stsr));
	}
out:
	if ((bits & ATW_NAR_ST) != 0)
		atw_txdrain(sc);
	splx(s);
	return;
}

/*
 * atw_linkintr:
 *
 *	Helper; handle link-status interrupts.
 */
void
atw_linkintr(struct atw_softc *sc, u_int32_t linkstatus)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	if (linkstatus & ATW_INTR_LINKON) {
		DPRINTF(sc, ("%s: link on\n", device_xname(sc->sc_dev)));
		sc->sc_rescan_timer = 0;
	} else if (linkstatus & ATW_INTR_LINKOFF) {
		DPRINTF(sc, ("%s: link off\n", device_xname(sc->sc_dev)));
		if (ic->ic_opmode != IEEE80211_M_STA)
			return;
		sc->sc_rescan_timer = 3;
		sc->sc_if.if_timer = 1;
	}
}

#if 0
static inline int
atw_hw_decrypted(struct atw_softc *sc, struct ieee80211_frame_min *wh)
{
	if ((sc->sc_ic.ic_flags & IEEE80211_F_PRIVACY) == 0)
		return 0;
	if ((wh->i_fc[1] & IEEE80211_FC1_WEP) == 0)
		return 0;
	return (sc->sc_wepctl & ATW_WEPCTL_WEPRXBYP) == 0;
}
#endif

/*
 * atw_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
void
atw_rxintr(struct atw_softc *sc)
{
	static int rate_tbl[] = {2, 4, 11, 22, 44};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame_min *wh;
	struct ifnet *ifp = &sc->sc_if;
	struct atw_rxsoft *rxs;
	struct mbuf *m;
	u_int32_t rxstat;
	int i, len, rate, rate0;
	u_int32_t rssi, ctlrssi;

	for (i = sc->sc_rxptr;; i = sc->sc_rxptr) {
		rxs = &sc->sc_rxsoft[i];

		ATW_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = le32toh(sc->sc_rxdescs[i].ar_stat);
		ctlrssi = le32toh(sc->sc_rxdescs[i].ar_ctlrssi);
		rate0 = __SHIFTOUT(rxstat, ATW_RXSTAT_RXDR_MASK);

		if (rxstat & ATW_RXSTAT_OWN) {
			ATW_CDRXSYNC(sc, i, BUS_DMASYNC_PREREAD);
			break;
		}

		sc->sc_rxptr = ATW_NEXTRX(i);

		DPRINTF3(sc,
		    ("%s: rx stat %08x ctlrssi %08x buf1 %08x buf2 %08x\n",
		    device_xname(sc->sc_dev),
		    rxstat, ctlrssi,
		    le32toh(sc->sc_rxdescs[i].ar_buf1),
		    le32toh(sc->sc_rxdescs[i].ar_buf2)));

		/*
		 * Make sure the packet fits in one buffer.  This should
		 * always be the case.
		 */
		if ((rxstat & (ATW_RXSTAT_FS|ATW_RXSTAT_LS)) !=
		    (ATW_RXSTAT_FS|ATW_RXSTAT_LS)) {
			printf("%s: incoming packet spilled, resetting\n",
			    device_xname(sc->sc_dev));
			(void)atw_init(ifp);
			return;
		}

		/*
		 * If an error occurred, update stats, clear the status
		 * word, and leave the packet buffer in place.  It will
		 * simply be reused the next time the ring comes around.
		 */
		if ((rxstat & (ATW_RXSTAT_DE | ATW_RXSTAT_RXTOE)) != 0) {
#define	PRINTERR(bit, str)						\
			if (rxstat & (bit))				\
				aprint_error_dev(sc->sc_dev, "receive error: %s\n",	\
				    str)
			ifp->if_ierrors++;
			PRINTERR(ATW_RXSTAT_DE, "descriptor error");
			PRINTERR(ATW_RXSTAT_RXTOE, "time-out");
#if 0
			PRINTERR(ATW_RXSTAT_SFDE, "PLCP SFD error");
			PRINTERR(ATW_RXSTAT_SIGE, "PLCP signal error");
			PRINTERR(ATW_RXSTAT_CRC16E, "PLCP CRC16 error");
			PRINTERR(ATW_RXSTAT_ICVE, "WEP ICV error");
#endif
#undef PRINTERR
			atw_init_rxdesc(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.  Note the ADM8211
		 * includes the CRC in promiscuous mode.
		 */
		len = __SHIFTOUT(rxstat, ATW_RXSTAT_FL_MASK);

		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		m = rxs->rxs_mbuf;
		if (atw_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			atw_init_rxdesc(sc, i);
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = MIN(m->m_ext.ext_size, len);

		rate = (rate0 < __arraycount(rate_tbl)) ? rate_tbl[rate0] : 0;

		/* The RSSI comes straight from a register in the
		 * baseband processor.  I know that for the RF3000,
		 * the RSSI register also contains the antenna-selection
		 * bits.  Mask those off.
		 *
		 * TBD Treat other basebands.
		 * TBD Use short-preamble bit and such in RF3000_RXSTAT.
		 */
		if (sc->sc_bbptype == ATW_BBPTYPE_RFMD)
			rssi = ctlrssi & RF3000_RSSI_MASK;
		else
			rssi = ctlrssi;

		/* Pass this up to any BPF listeners. */
		if (sc->sc_radiobpf != NULL) {
			struct atw_rx_radiotap_header *tap = &sc->sc_rxtap;

			tap->ar_rate = rate;

			/* TBD verify units are dB */
			tap->ar_antsignal = (int)rssi;
			if (sc->sc_opmode & ATW_NAR_PR)
				tap->ar_flags = IEEE80211_RADIOTAP_F_FCS;
			else
				tap->ar_flags = 0;

			if ((rxstat & ATW_RXSTAT_CRC32E) != 0)
				tap->ar_flags |= IEEE80211_RADIOTAP_F_BADFCS;

			bpf_mtap2(sc->sc_radiobpf, tap, sizeof(sc->sc_rxtapu),
			    m);
 		}

		sc->sc_recv_ev.ev_count++;

		if ((rxstat & (ATW_RXSTAT_CRC16E|ATW_RXSTAT_CRC32E|ATW_RXSTAT_ICVE|ATW_RXSTAT_SFDE|ATW_RXSTAT_SIGE)) != 0) {
			if (rxstat & ATW_RXSTAT_CRC16E)
				sc->sc_crc16e_ev.ev_count++;
			if (rxstat & ATW_RXSTAT_CRC32E)
				sc->sc_crc32e_ev.ev_count++;
			if (rxstat & ATW_RXSTAT_ICVE)
				sc->sc_icve_ev.ev_count++;
			if (rxstat & ATW_RXSTAT_SFDE)
				sc->sc_sfde_ev.ev_count++;
			if (rxstat & ATW_RXSTAT_SIGE)
				sc->sc_sige_ev.ev_count++;
			ifp->if_ierrors++;
			m_freem(m);
			continue;
		}

		if (sc->sc_opmode & ATW_NAR_PR)
			m_adj(m, -IEEE80211_CRC_LEN);

		wh = mtod(m, struct ieee80211_frame_min *);
		ni = ieee80211_find_rxnode(ic, wh);
#if 0
		if (atw_hw_decrypted(sc, wh)) {
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
			DPRINTF(sc, ("%s: hw decrypted\n", __func__));
		}
#endif
		ieee80211_input(ic, m, ni, (int)rssi, 0);
		ieee80211_free_node(ni);
	}
}

/*
 * atw_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
void
atw_txintr(struct atw_softc *sc, uint32_t status)
{
	static char txstat_buf[sizeof("ffffffff<>" ATW_TXSTAT_FMT)];
	struct ifnet *ifp = &sc->sc_if;
	struct atw_txsoft *txs;
	u_int32_t txstat;

	DPRINTF3(sc, ("%s: atw_txintr: sc_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags));

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		ATW_CDTXSYNC(sc, txs->txs_lastdesc, 1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

#ifdef ATW_DEBUG
		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			int i;
			printf("    txsoft %p transmit chain:\n", txs);
			ATW_CDTXSYNC(sc, txs->txs_firstdesc,
			    txs->txs_ndescs - 1,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			for (i = txs->txs_firstdesc;; i = ATW_NEXTTX(i)) {
				printf("     descriptor %d:\n", i);
				printf("       at_status:   0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_stat));
				printf("       at_flags:      0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_flags));
				printf("       at_buf1: 0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_buf1));
				printf("       at_buf2: 0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_buf2));
				if (i == txs->txs_lastdesc)
					break;
			}
			ATW_CDTXSYNC(sc, txs->txs_firstdesc,
			    txs->txs_ndescs - 1, BUS_DMASYNC_PREREAD);
		}
#endif

		txstat = le32toh(sc->sc_txdescs[txs->txs_lastdesc].at_stat);
		if (txstat & ATW_TXSTAT_OWN) {
			ATW_CDTXSYNC(sc, txs->txs_lastdesc, 1,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;

		sc->sc_txfree += txs->txs_ndescs;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		KASSERT(!SIMPLEQ_EMPTY(&sc->sc_txfreeq) && sc->sc_txfree != 0);
		sc->sc_tx_timer = 0;
		ifp->if_flags &= ~IFF_OACTIVE;

		if ((ifp->if_flags & IFF_DEBUG) != 0 &&
		    (txstat & ATW_TXSTAT_ERRMASK) != 0) {
			snprintb(txstat_buf, sizeof(txstat_buf),
			    ATW_TXSTAT_FMT, txstat & ATW_TXSTAT_ERRMASK);
			printf("%s: txstat %s %" __PRIuBITS "\n",
			    device_xname(sc->sc_dev), txstat_buf,
			    __SHIFTOUT(txstat, ATW_TXSTAT_ARC_MASK));
		}

		sc->sc_xmit_ev.ev_count++;

		/*
		 * Check for errors and collisions.
		 */
		if (txstat & ATW_TXSTAT_TUF)
			sc->sc_tuf_ev.ev_count++;
		if (txstat & ATW_TXSTAT_TLT)
			sc->sc_tlt_ev.ev_count++;
		if (txstat & ATW_TXSTAT_TRT)
			sc->sc_trt_ev.ev_count++;
		if (txstat & ATW_TXSTAT_TRO)
			sc->sc_tro_ev.ev_count++;
		if (txstat & ATW_TXSTAT_SOFBR)
			sc->sc_sofbr_ev.ev_count++;

		if ((txstat & ATW_TXSTAT_ES) == 0)
			ifp->if_collisions +=
			    __SHIFTOUT(txstat, ATW_TXSTAT_ARC_MASK);
		else
			ifp->if_oerrors++;

		ifp->if_opackets++;
	}

	KASSERT(txs != NULL || (ifp->if_flags & IFF_OACTIVE) == 0);
}

/*
 * atw_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
void
atw_watchdog(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;
	if (!device_is_active(sc->sc_dev))
		return;

	if (sc->sc_rescan_timer != 0 && --sc->sc_rescan_timer == 0)
		(void)ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	if (sc->sc_tx_timer != 0 && --sc->sc_tx_timer == 0 &&
	    !SIMPLEQ_EMPTY(&sc->sc_txdirtyq)) {
		printf("%s: transmit timeout\n", ifp->if_xname);
		ifp->if_oerrors++;
		(void)atw_init(ifp);
		atw_start(ifp);
	}
	if (sc->sc_tx_timer != 0 || sc->sc_rescan_timer != 0)
		ifp->if_timer = 1;
	ieee80211_watchdog(ic);
}

static void
atw_evcnt_detach(struct atw_softc *sc)
{
	evcnt_detach(&sc->sc_sige_ev);
	evcnt_detach(&sc->sc_sfde_ev);
	evcnt_detach(&sc->sc_icve_ev);
	evcnt_detach(&sc->sc_crc32e_ev);
	evcnt_detach(&sc->sc_crc16e_ev);
	evcnt_detach(&sc->sc_recv_ev);

	evcnt_detach(&sc->sc_tuf_ev);
	evcnt_detach(&sc->sc_tro_ev);
	evcnt_detach(&sc->sc_trt_ev);
	evcnt_detach(&sc->sc_tlt_ev);
	evcnt_detach(&sc->sc_sofbr_ev);
	evcnt_detach(&sc->sc_xmit_ev);

	evcnt_detach(&sc->sc_rxpkt1in_ev);
	evcnt_detach(&sc->sc_rxamatch_ev);
	evcnt_detach(&sc->sc_workaround1_ev);
	evcnt_detach(&sc->sc_misc_ev);
}

static void
atw_evcnt_attach(struct atw_softc *sc)
{
	evcnt_attach_dynamic(&sc->sc_recv_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "recv");
	evcnt_attach_dynamic(&sc->sc_crc16e_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "CRC16 error");
	evcnt_attach_dynamic(&sc->sc_crc32e_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "CRC32 error");
	evcnt_attach_dynamic(&sc->sc_icve_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "ICV error");
	evcnt_attach_dynamic(&sc->sc_sfde_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "PLCP SFD error");
	evcnt_attach_dynamic(&sc->sc_sige_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "PLCP Signal Field error");

	evcnt_attach_dynamic(&sc->sc_xmit_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "xmit");
	evcnt_attach_dynamic(&sc->sc_tuf_ev, EVCNT_TYPE_MISC,
	    &sc->sc_xmit_ev, sc->sc_if.if_xname, "transmit underflow");
	evcnt_attach_dynamic(&sc->sc_tro_ev, EVCNT_TYPE_MISC,
	    &sc->sc_xmit_ev, sc->sc_if.if_xname, "transmit overrun");
	evcnt_attach_dynamic(&sc->sc_trt_ev, EVCNT_TYPE_MISC,
	    &sc->sc_xmit_ev, sc->sc_if.if_xname, "retry count exceeded");
	evcnt_attach_dynamic(&sc->sc_tlt_ev, EVCNT_TYPE_MISC,
	    &sc->sc_xmit_ev, sc->sc_if.if_xname, "lifetime exceeded");
	evcnt_attach_dynamic(&sc->sc_sofbr_ev, EVCNT_TYPE_MISC,
	    &sc->sc_xmit_ev, sc->sc_if.if_xname, "packet size mismatch");

	evcnt_attach_dynamic(&sc->sc_misc_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "misc");
	evcnt_attach_dynamic(&sc->sc_workaround1_ev, EVCNT_TYPE_MISC,
	    &sc->sc_misc_ev, sc->sc_if.if_xname, "workaround #1");
	evcnt_attach_dynamic(&sc->sc_rxamatch_ev, EVCNT_TYPE_MISC,
	    &sc->sc_misc_ev, sc->sc_if.if_xname, "rra equals rwa");
	evcnt_attach_dynamic(&sc->sc_rxpkt1in_ev, EVCNT_TYPE_MISC,
	    &sc->sc_misc_ev, sc->sc_if.if_xname, "rxpkt1in set");
}

#ifdef ATW_DEBUG
static void
atw_dump_pkt(struct ifnet *ifp, struct mbuf *m0)
{
	struct atw_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int i, noctets = 0;

	printf("%s: %d-byte packet\n", device_xname(sc->sc_dev),
	    m0->m_pkthdr.len);

	for (m = m0; m; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		for (i = 0; i < m->m_len; i++) {
			printf(" %02x", ((u_int8_t*)m->m_data)[i]);
			if (++noctets % 24 == 0)
				printf("\n");
		}
	}
	printf("%s%s: %d bytes emitted\n",
	    (noctets % 24 != 0) ? "\n" : "", device_xname(sc->sc_dev), noctets);
}
#endif /* ATW_DEBUG */

/*
 * atw_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
void
atw_start(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211_key *k;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame_min *whm;
	struct ieee80211_frame *wh;
	struct atw_frame *hh;
	uint16_t hdrctl;
	struct mbuf *m0, *m;
	struct atw_txsoft *txs;
	struct atw_txdesc *txd;
	int npkt, rate;
	bus_dmamap_t dmamap;
	int ctl, error, firsttx, nexttx, lasttx, first, ofree, seg;

	DPRINTF2(sc, ("%s: atw_start: sc_flags 0x%08x, if_flags 0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_flags, ifp->if_flags));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;
	firsttx = lasttx = sc->sc_txnext;

	DPRINTF2(sc, ("%s: atw_start: txfree %d, txnext %d\n",
	    device_xname(sc->sc_dev), ofree, firsttx));

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txfreeq)) != NULL &&
	       sc->sc_txfree != 0) {

		hdrctl = htole16(ATW_HDRCTL_UNKNOWN1);

		/*
		 * Grab a packet off the management queue, if it
		 * is not empty. Otherwise, from the data queue.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
		} else if (ic->ic_state != IEEE80211_S_RUN)
			break; /* send no data until associated */
		else {
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			bpf_mtap(ifp, m0);
			ni = ieee80211_find_txnode(ic,
			    mtod(m0, struct ether_header *)->ether_dhost);
			if (ni == NULL) {
				ifp->if_oerrors++;
				break;
			}
			if ((m0 = ieee80211_encap(ic, m0, ni)) == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		rate = MAX(ieee80211_get_rate(ni), 2);

		whm = mtod(m0, struct ieee80211_frame_min *);

		if ((whm->i_fc[1] & IEEE80211_FC1_WEP) == 0)
			k = NULL;
		else if ((k = ieee80211_crypto_encap(ic, ni, m0)) == NULL) {
			m_freem(m0);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}
#if 0
		if (IEEE80211_IS_MULTICAST(wh->i_addr1) &&
		    m0->m_pkthdr.len > ic->ic_fragthreshold)
			hdrctl |= htole16(ATW_HDRCTL_MORE_FRAG);
#endif

		if (m0->m_pkthdr.len + IEEE80211_CRC_LEN >= ic->ic_rtsthreshold)
			hdrctl |= htole16(ATW_HDRCTL_RTSCTS);

		if (ieee80211_compute_duration(whm, k, m0->m_pkthdr.len,
		    ic->ic_flags, ic->ic_fragthreshold, rate,
		    &txs->txs_d0, &txs->txs_dn, &npkt, 0) == -1) {
			DPRINTF2(sc, ("%s: fail compute duration\n", __func__));
			m_freem(m0);
			break;
		}

		/* XXX Misleading if fragmentation is enabled.  Better
		 * to fragment in software?
		 */
		*(uint16_t *)whm->i_dur = htole16(txs->txs_d0.d_rts_dur);

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap3(ic->ic_rawbpf, m0);

		if (sc->sc_radiobpf != NULL) {
			struct atw_tx_radiotap_header *tap = &sc->sc_txtap;

			tap->at_rate = rate;

			bpf_mtap2(sc->sc_radiobpf, tap, sizeof(sc->sc_txtapu),
			    m0);
		}

		M_PREPEND(m0, offsetof(struct atw_frame, atw_ihdr), M_DONTWAIT);

		if (ni != NULL)
			ieee80211_free_node(ni);

		if (m0 == NULL) {
			ifp->if_oerrors++;
			break;
		}

		/* just to make sure. */
		m0 = m_pullup(m0, sizeof(struct atw_frame));

		if (m0 == NULL) {
			ifp->if_oerrors++;
			break;
		}

		hh = mtod(m0, struct atw_frame *);
		wh = &hh->atw_ihdr;

		/* Copy everything we need from the 802.11 header:
		 * Frame Control; address 1, address 3, or addresses
		 * 3 and 4. NIC fills in BSSID, SA.
		 */
		if (wh->i_fc[1] & IEEE80211_FC1_DIR_TODS) {
			if (wh->i_fc[1] & IEEE80211_FC1_DIR_FROMDS)
				panic("%s: illegal WDS frame",
				    device_xname(sc->sc_dev));
			memcpy(hh->atw_dst, wh->i_addr3, IEEE80211_ADDR_LEN);
		} else
			memcpy(hh->atw_dst, wh->i_addr1, IEEE80211_ADDR_LEN);

		*(u_int16_t*)hh->atw_fc = *(u_int16_t*)wh->i_fc;

		/* initialize remaining Tx parameters */
		memset(&hh->u, 0, sizeof(hh->u));

		hh->atw_rate = rate * 5;
		/* XXX this could be incorrect if M_FCS. _encap should
		 * probably strip FCS just in case it sticks around in
		 * bridged packets.
		 */
		hh->atw_service = 0x00; /* XXX guess */
		hh->atw_paylen = htole16(m0->m_pkthdr.len -
		    sizeof(struct atw_frame));

		/* never fragment multicast frames */
		if (IEEE80211_IS_MULTICAST(hh->atw_dst))
			hh->atw_fragthr = htole16(IEEE80211_FRAG_MAX);
		else {
			if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
			    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
				hdrctl |= htole16(ATW_HDRCTL_SHORT_PREAMBLE);
			hh->atw_fragthr = htole16(ic->ic_fragthreshold);
		}

		hh->atw_rtylmt = 3;
#if 0
		if (do_encrypt) {
			hdrctl |= htole16(ATW_HDRCTL_WEP);
			hh->atw_keyid = ic->ic_def_txkey;
		}
#endif

		hh->atw_head_plcplen = htole16(txs->txs_d0.d_plcp_len);
		hh->atw_tail_plcplen = htole16(txs->txs_dn.d_plcp_len);
		if (txs->txs_d0.d_residue)
			hh->atw_head_plcplen |= htole16(0x8000);
		if (txs->txs_dn.d_residue)
			hh->atw_tail_plcplen |= htole16(0x8000);
		hh->atw_head_dur = htole16(txs->txs_d0.d_rts_dur);
		hh->atw_tail_dur = htole16(txs->txs_dn.d_rts_dur);

		hh->atw_hdrctl = hdrctl;
		hh->atw_fragnum = npkt << 4;
#ifdef ATW_DEBUG

		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			printf("%s: dst = %s, rate = 0x%02x, "
			    "service = 0x%02x, paylen = 0x%04x\n",
			    device_xname(sc->sc_dev), ether_sprintf(hh->atw_dst),
			    hh->atw_rate, hh->atw_service, hh->atw_paylen);

			printf("%s: fc[0] = 0x%02x, fc[1] = 0x%02x, "
			    "dur1 = 0x%04x, dur2 = 0x%04x, "
			    "dur3 = 0x%04x, rts_dur = 0x%04x\n",
			    device_xname(sc->sc_dev), hh->atw_fc[0], hh->atw_fc[1],
			    hh->atw_tail_plcplen, hh->atw_head_plcplen,
			    hh->atw_tail_dur, hh->atw_head_dur);

			printf("%s: hdrctl = 0x%04x, fragthr = 0x%04x, "
			    "fragnum = 0x%02x, rtylmt = 0x%04x\n",
			    device_xname(sc->sc_dev), hh->atw_hdrctl,
			    hh->atw_fragthr, hh->atw_fragnum, hh->atw_rtylmt);

			printf("%s: keyid = %d\n",
			    device_xname(sc->sc_dev), hh->atw_keyid);

			atw_dump_pkt(ifp, m0);
		}
#endif /* ATW_DEBUG */

		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  Copy and try (once) again if the packet
		 * didn't fit in the alloted number of segments.
		 */
		for (first = 1;
		     (error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		                  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) != 0 && first;
		     first = 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_dev(sc->sc_dev, "unable to allocate Tx mbuf\n");
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					aprint_error_dev(sc->sc_dev, "unable to allocate Tx "
					    "cluster\n");
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			m_freem(m0);
			m0 = m;
			m = NULL;
		}
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "unable to load Tx buffer, "
			    "error = %d\n", error);
			m_freem(m0);
			break;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > sc->sc_txfree) {
			/*
			 * Not enough free descriptors to transmit
			 * this packet.  Unload the DMA map and
			 * drop the packet.  Notify the upper layer
			 * that there are no more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX it is worth it?
			 */
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			m_freem(m0);
			break;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* XXX arbitrary retry limit; 8 because I have seen it in
		 * use already and maybe 0 means "no tries" !
		 */
		ctl = htole32(__SHIFTIN(8, ATW_TXCTL_TL_MASK));

		DPRINTF2(sc, ("%s: TXDR <- max(10, %d)\n",
		    device_xname(sc->sc_dev), rate * 5));
		ctl |= htole32(__SHIFTIN(MAX(10, rate * 5), ATW_TXCTL_TXDR_MASK));

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = ATW_NEXTTX(nexttx)) {
			/*
			 * If this is the first descriptor we're
			 * enqueueing, don't set the OWN bit just
			 * yet.  That could cause a race condition.
			 * We'll do it below.
			 */
			txd = &sc->sc_txdescs[nexttx];
			txd->at_ctl = ctl |
			    ((nexttx == firsttx) ? 0 : htole32(ATW_TXCTL_OWN));

			txd->at_buf1 = htole32(dmamap->dm_segs[seg].ds_addr);
			txd->at_flags =
			    htole32(__SHIFTIN(dmamap->dm_segs[seg].ds_len,
			                   ATW_TXFLAG_TBS1_MASK)) |
			    ((nexttx == (ATW_NTXDESC - 1))
			        ? htole32(ATW_TXFLAG_TER) : 0);
			lasttx = nexttx;
		}

		/* Set `first segment' and `last segment' appropriately. */
		sc->sc_txdescs[sc->sc_txnext].at_flags |=
		    htole32(ATW_TXFLAG_FS);
		sc->sc_txdescs[lasttx].at_flags |= htole32(ATW_TXFLAG_LS);

#ifdef ATW_DEBUG
		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			printf("     txsoft %p transmit chain:\n", txs);
			for (seg = sc->sc_txnext;; seg = ATW_NEXTTX(seg)) {
				printf("     descriptor %d:\n", seg);
				printf("       at_ctl:   0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_ctl));
				printf("       at_flags:      0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_flags));
				printf("       at_buf1: 0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_buf1));
				printf("       at_buf2: 0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_buf2));
				if (seg == lasttx)
					break;
			}
		}
#endif

		/* Sync the descriptors we're using. */
		ATW_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;
		txs->txs_ndescs = dmamap->dm_nsegs;

		/* Advance the tx pointer. */
		sc->sc_txfree -= dmamap->dm_nsegs;
		sc->sc_txnext = nexttx;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
		SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);
	}

	if (sc->sc_txfree != ofree) {
		DPRINTF2(sc, ("%s: packets enqueued, IC on %d, OWN on %d\n",
		    device_xname(sc->sc_dev), lasttx, firsttx));
		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued.
		 */
		sc->sc_txdescs[lasttx].at_flags |= htole32(ATW_TXFLAG_IC);
		ATW_CDTXSYNC(sc, lasttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * The entire packet chain is set up.  Give the
		 * first descriptor to the chip now.
		 */
		sc->sc_txdescs[firsttx].at_ctl |= htole32(ATW_TXCTL_OWN);
		ATW_CDTXSYNC(sc, firsttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Wake up the transmitter. */
		ATW_WRITE(sc, ATW_TDR, 0x1);

		if (txs == NULL || sc->sc_txfree == 0)
			ifp->if_flags |= IFF_OACTIVE;

		/* Set a watchdog timer in case the chip flakes out. */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

/*
 * atw_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
int
atw_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211req *ireq;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP|IFF_RUNNING:
			/*
			 * To avoid rescanning another access point,
			 * do not call atw_init() here.  Instead,
			 * only reflect media settings.
			 */
			if (device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
				atw_filter_setup(sc);
			break;
		case IFF_UP:
			error = atw_init(ifp);
			break;
		case IFF_RUNNING:
			atw_stop(ifp, 1);
			break;
		case 0:
			break;
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				atw_filter_setup(sc); /* do not rescan */
			error = 0;
		}
		break;
	case SIOCS80211:
		ireq = data;
		if (ireq->i_type == IEEE80211_IOC_FRAGTHRESHOLD) {
			if ((error = kauth_authorize_network(curlwp->l_cred,
			    KAUTH_NETWORK_INTERFACE,
			    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp,
			    (void *)cmd, NULL)) != 0)
				break;
			if (!(IEEE80211_FRAG_MIN <= ireq->i_val &&
			      ireq->i_val <= IEEE80211_FRAG_MAX))
				error = EINVAL;
			else
				sc->sc_ic.ic_fragthreshold = ireq->i_val;
			break;
		}
		/*FALLTHROUGH*/
	default:
		error = ieee80211_ioctl(&sc->sc_ic, cmd, data);
		if (error == ENETRESET || error == ERESTART) {
			if (is_running(ifp))
				error = atw_init(ifp);
			else
				error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	if (device_is_active(sc->sc_dev))
		atw_start(ifp);

	splx(s);
	return (error);
}

static int
atw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if (is_running(ifp))
			error = atw_init(ifp);
		else
			error = 0;
	}
	return error;
}
