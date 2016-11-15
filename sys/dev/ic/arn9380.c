/*	$NetBSD: arn9380.c,v 1.3 2014/01/22 17:29:29 matt Exp $	*/
/*	$OpenBSD: ar9380.c,v 1.17 2012/10/20 09:54:20 stsp Exp $	*/

/*-
 * Copyright (c) 2011 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2010 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Driver for Atheros 802.11a/g/n chipsets.
 * Routines for AR9380 and AR9485 chipsets.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: arn9380.c,v 1.3 2014/01/22 17:29:29 matt Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/endian.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>

#include <dev/ic/arn9003reg.h>
#include <dev/ic/arn9380reg.h>

#include <dev/ic/arn9003.h>
#include <dev/ic/arn9380.h>

#define Static static

Static void	ar9380_get_correction(struct athn_softc *,
		    struct ieee80211_channel *, int, int *, int *);
Static void	ar9380_get_paprd_masks(struct athn_softc *,
		    struct ieee80211_channel *, uint32_t *, uint32_t *);
Static const uint8_t *
		ar9380_get_rom_template(struct athn_softc *, uint8_t);
Static void	ar9380_init_from_rom(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_set_correction(struct athn_softc *,
		    struct ieee80211_channel *);
Static int	ar9380_set_synth(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_set_txpower(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_setup(struct athn_softc *);
Static void	ar9380_spur_mitigate(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_spur_mitigate_cck(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_spur_mitigate_ofdm(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	ar9380_swap_rom(struct athn_softc *);

Static void	ar9485_init_swreg(struct athn_softc *);
#define		ar9485_pmu_read		AR_READ
Static int	ar9485_pmu_write(struct athn_softc *, uint32_t, uint32_t);

#ifdef notused
Static void	ar9380_init_swreg(struct athn_softc *);
#endif /* notused */

PUBLIC int
ar9380_attach(struct athn_softc *sc)
{

	sc->sc_ngpiopins = 17;
	sc->sc_ops.setup = ar9380_setup;
	sc->sc_ops.get_rom_template = ar9380_get_rom_template;
	sc->sc_ops.swap_rom = ar9380_swap_rom;
	sc->sc_ops.init_from_rom = ar9380_init_from_rom;
	sc->sc_ops.set_txpower = ar9380_set_txpower;
	sc->sc_ops.set_synth = ar9380_set_synth;
	sc->sc_ops.spur_mitigate = ar9380_spur_mitigate;
	sc->sc_ops.get_paprd_masks = ar9380_get_paprd_masks;
	sc->sc_cca_min_2g = AR9380_PHY_CCA_MIN_GOOD_VAL_2GHZ;
	sc->sc_cca_max_2g = AR9380_PHY_CCA_MAX_GOOD_VAL_2GHZ;
	sc->sc_cca_min_5g = AR9380_PHY_CCA_MIN_GOOD_VAL_5GHZ;
	sc->sc_cca_max_5g = AR9380_PHY_CCA_MAX_GOOD_VAL_5GHZ;
	if (AR_SREV_9485(sc)) {
		sc->sc_ini = &ar9485_1_1_ini;
		sc->sc_serdes = &ar9485_1_1_serdes;
	}
	else {
		sc->sc_ini = &ar9380_2_2_ini;
		sc->sc_serdes = &ar9380_2_2_serdes;
	}

	return ar9003_attach(sc);
}

Static void
ar9380_setup(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ar9380_eeprom *eep = sc->sc_eep;
	struct ar9380_base_eep_hdr *base = &eep->baseEepHeader;
	uint8_t type;

	if (base->opFlags & AR_OPFLAGS_11A)
		sc->sc_flags |= ATHN_FLAG_11A;
	if (base->opFlags & AR_OPFLAGS_11G)
		sc->sc_flags |= ATHN_FLAG_11G;
	if (base->opFlags & AR_OPFLAGS_11N)
		sc->sc_flags |= ATHN_FLAG_11N;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, eep->macAddr);
	sc->sc_led_pin = base->wlanLedGpio;

	/* Check if we have a hardware radio switch. */
	if (base->rfSilent & AR_EEP_RFSILENT_ENABLED) {
		sc->sc_flags |= ATHN_FLAG_RFSILENT;
		/* Get GPIO pin used by hardware radio switch. */
		sc->sc_rfsilent_pin = MS(base->rfSilent,
		    AR_EEP_RFSILENT_GPIO_SEL);
		/* Get polarity of hardware radio switch. */
		if (base->rfSilent & AR_EEP_RFSILENT_POLARITY)
			sc->sc_flags |= ATHN_FLAG_RFSILENT_REVERSED;
	}

	/* Set the number of HW key cache entries. */
	sc->sc_kc_entries = AR_KEYTABLE_SIZE;

	sc->sc_txchainmask = MS(base->txrxMask, AR_EEP_TX_MASK);
	sc->sc_rxchainmask = MS(base->txrxMask, AR_EEP_RX_MASK);

	/* Fast PLL clock is always supported. */
	sc->sc_flags |= ATHN_FLAG_FAST_PLL_CLOCK;

	/* Enable PA predistortion if supported. */
	if (base->featureEnable & AR_EEP_PAPRD)
		sc->sc_flags |= ATHN_FLAG_PAPRD;
	/*
	 * Some 3-stream chips may exceed the PCIe power requirements,
	 * requiring to reduce the number of Tx chains in some cases.
	 */
	if ((base->miscConfiguration & AR_EEP_CHAIN_MASK_REDUCE) &&
	    sc->sc_txchainmask == 0x7)
		sc->sc_flags |= ATHN_FLAG_3TREDUCE_CHAIN;

	/* Select initialization values based on ROM. */
	type = MS(eep->baseEepHeader.txrxgain, AR_EEP_RX_GAIN);
	if (!AR_SREV_9485(sc)) {
		if (type == AR_EEP_RX_GAIN_WO_XLNA)
			sc->sc_rx_gain = &ar9380_2_2_rx_gain_wo_xlna;
		else
			sc->sc_rx_gain = &ar9380_2_2_rx_gain;
	}
	else
		sc->sc_rx_gain = &ar9485_1_1_rx_gain;

	/* Select initialization values based on ROM. */
	type = MS(eep->baseEepHeader.txrxgain, AR_EEP_TX_GAIN);
	if (!AR_SREV_9485(sc)) {
		if (type == AR_EEP_TX_GAIN_HIGH_OB_DB)
			sc->sc_tx_gain = &ar9380_2_2_tx_gain_high_ob_db;
		else if (type == AR_EEP_TX_GAIN_LOW_OB_DB)
			sc->sc_tx_gain = &ar9380_2_2_tx_gain_low_ob_db;
		else if (type == AR_EEP_TX_GAIN_HIGH_POWER)
			sc->sc_tx_gain = &ar9380_2_2_tx_gain_high_power;
		else
			sc->sc_tx_gain = &ar9380_2_2_tx_gain;
	}
	else
		sc->sc_tx_gain = &ar9485_1_1_tx_gain;
}

Static const uint8_t *
ar9380_get_rom_template(struct athn_softc *sc, uint8_t ref)
{
	size_t i;

	/* Retrieve template ROM image for given reference. */
	for (i = 0; i < __arraycount(ar9380_rom_templates); i++)
		if (ar9380_rom_templates[i][1] == ref)
			return ar9380_rom_templates[i];
	return NULL;
}

Static void
ar9380_swap_rom(struct athn_softc *sc)
{
#if BYTE_ORDER == BIG_ENDIAN
	struct ar9380_eeprom *eep = sc->sc_eep;
	struct ar9380_base_eep_hdr *base = &eep->baseEepHeader;
	struct ar9380_modal_eep_header *modal;
	int i;

	base->regDmn[0] = bswap16(base->regDmn[0]);
	base->regDmn[1] = bswap16(base->regDmn[1]);
	base->swreg = bswap32(base->swreg);

	modal = &eep->modalHeader2G;
	modal->antCtrlCommon = bswap32(modal->antCtrlCommon);
	modal->antCtrlCommon2 = bswap32(modal->antCtrlCommon2);
	modal->papdRateMaskHt20 = bswap32(modal->papdRateMaskHt20);
	modal->papdRateMaskHt40 = bswap32(modal->papdRateMaskHt40);
	for (i = 0; i < AR9380_MAX_CHAINS; i++)
		modal->antCtrlChain[i] = bswap16(modal->antCtrlChain[i]);

	modal = &eep->modalHeader5G;
	modal->antCtrlCommon = bswap32(modal->antCtrlCommon);
	modal->antCtrlCommon2 = bswap32(modal->antCtrlCommon2);
	modal->papdRateMaskHt20 = bswap32(modal->papdRateMaskHt20);
	modal->papdRateMaskHt40 = bswap32(modal->papdRateMaskHt40);
	for (i = 0; i < AR9380_MAX_CHAINS; i++)
		modal->antCtrlChain[i] = bswap16(modal->antCtrlChain[i]);
#endif
}

Static void
ar9380_get_paprd_masks(struct athn_softc *sc, struct ieee80211_channel *c,
    uint32_t *ht20mask, uint32_t *ht40mask)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	const struct ar9380_modal_eep_header *modal;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		modal = &eep->modalHeader2G;
	else
		modal = &eep->modalHeader5G;
	*ht20mask = modal->papdRateMaskHt20;
	*ht40mask = modal->papdRateMaskHt40;
}

Static int
ar9380_set_synth(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint32_t freq = c->ic_freq;
	uint32_t chansel, phy;

	if (IEEE80211_IS_CHAN_2GHZ(c)) {
		if (AR_SREV_9485(sc))
			chansel = ((freq << 16) - 215) / 15;
		else
			chansel = (freq << 16) / 15;
		AR_WRITE(sc, AR_PHY_SYNTH_CONTROL, AR9380_BMODE);
	}
	else {
		chansel = (freq << 15) / 15;
		chansel >>= 1;
		AR_WRITE(sc, AR_PHY_SYNTH_CONTROL, 0);
	}

	/* Enable Long Shift Select for synthesizer. */
	AR_SETBITS(sc, AR_PHY_65NM_CH0_SYNTH4,
	    AR_PHY_SYNTH4_LONG_SHIFT_SELECT);
	AR_WRITE_BARRIER(sc);

	/* Program synthesizer. */
	phy = (chansel << 2) | AR9380_FRACMODE;
	DPRINTFN(DBG_RF, sc, "AR_PHY_65NM_CH0_SYNTH7=0x%08x\n", phy);
	AR_WRITE(sc, AR_PHY_65NM_CH0_SYNTH7, phy);
	AR_WRITE_BARRIER(sc);
	/* Toggle Load Synth Channel bit. */
	AR_WRITE(sc, AR_PHY_65NM_CH0_SYNTH7, phy | AR9380_LOAD_SYNTH);
	AR_WRITE_BARRIER(sc);
	return 0;
}

Static void
ar9380_init_from_rom(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	const struct ar9380_modal_eep_header *modal;
	uint8_t db, margin, ant_div_ctrl;
	uint32_t reg;
	int i, maxchains;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		modal = &eep->modalHeader2G;
	else
		modal = &eep->modalHeader5G;

	/* Apply XPA bias level. */
	if (AR_SREV_9485(sc)) {
		reg = AR_READ(sc, AR9485_PHY_65NM_CH0_TOP2);
		reg = RW(reg, AR9485_PHY_65NM_CH0_TOP2_XPABIASLVL,
		    modal->xpaBiasLvl);
		AR_WRITE(sc, AR9485_PHY_65NM_CH0_TOP2, reg);
	}
	else {
		reg = AR_READ(sc, AR_PHY_65NM_CH0_TOP);
		reg = RW(reg, AR_PHY_65NM_CH0_TOP_XPABIASLVL,
		    modal->xpaBiasLvl & 0x3);
		AR_WRITE(sc, AR_PHY_65NM_CH0_TOP, reg);
		reg = AR_READ(sc, AR_PHY_65NM_CH0_THERM);
		reg = RW(reg, AR_PHY_65NM_CH0_THERM_XPABIASLVL_MSB,
		    modal->xpaBiasLvl >> 2);
		reg |= AR_PHY_65NM_CH0_THERM_XPASHORT2GND;
		AR_WRITE(sc, AR_PHY_65NM_CH0_THERM, reg);
	}

	/* Apply antenna control. */
	reg = AR_READ(sc, AR_PHY_SWITCH_COM);
	reg = RW(reg, AR_SWITCH_TABLE_COM_ALL, modal->antCtrlCommon);
	AR_WRITE(sc, AR_PHY_SWITCH_COM, reg);
	reg = AR_READ(sc, AR_PHY_SWITCH_COM_2);
	reg = RW(reg, AR_SWITCH_TABLE_COM_2_ALL, modal->antCtrlCommon2);
	AR_WRITE(sc, AR_PHY_SWITCH_COM_2, reg);

	maxchains = AR_SREV_9485(sc) ? 1 : AR9380_MAX_CHAINS;
	for (i = 0; i < maxchains; i++) {
		reg = AR_READ(sc, AR_PHY_SWITCH_CHAIN(i));
		reg = RW(reg, AR_SWITCH_TABLE_ALL, modal->antCtrlChain[i]);
		AR_WRITE(sc, AR_PHY_SWITCH_CHAIN(i), reg);
	}

	if (AR_SREV_9485(sc)) {
		ant_div_ctrl = eep->base_ext1.ant_div_control;
		reg = AR_READ(sc, AR_PHY_MC_GAIN_CTRL);
		reg = RW(reg, AR_PHY_MC_GAIN_CTRL_ANT_DIV_CTRL_ALL,
		    MS(ant_div_ctrl, AR_EEP_ANT_DIV_CTRL_ALL));
		if (ant_div_ctrl & AR_EEP_ANT_DIV_CTRL_ANT_DIV)
			reg |= AR_PHY_MC_GAIN_CTRL_ENABLE_ANT_DIV;
		else
			reg &= ~AR_PHY_MC_GAIN_CTRL_ENABLE_ANT_DIV;
		AR_WRITE(sc, AR_PHY_MC_GAIN_CTRL, reg);
		reg = AR_READ(sc, AR_PHY_CCK_DETECT);
		if (ant_div_ctrl & AR_EEP_ANT_DIV_CTRL_FAST_DIV)
			reg |= AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
		else
			reg &= ~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
		AR_WRITE(sc, AR_PHY_CCK_DETECT, reg);
	}

	if (eep->baseEepHeader.miscConfiguration & AR_EEP_DRIVE_STRENGTH) {
		/* Apply drive strength. */
		reg = AR_READ(sc, AR_PHY_65NM_CH0_BIAS1);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_0, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_1, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_2, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_3, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_4, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_5, 5);
		AR_WRITE(sc, AR_PHY_65NM_CH0_BIAS1, reg);

		reg = AR_READ(sc, AR_PHY_65NM_CH0_BIAS2);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_0, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_1, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_2, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_3, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_4, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_5, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_6, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_7, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_8, 5);
		AR_WRITE(sc, AR_PHY_65NM_CH0_BIAS2, reg);

		reg = AR_READ(sc, AR_PHY_65NM_CH0_BIAS4);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_0, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_1, 5);
		reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_2, 5);
		AR_WRITE(sc, AR_PHY_65NM_CH0_BIAS4, reg);
	}

	/* Apply attenuation settings. */
	maxchains = AR_SREV_9485(sc) ? 1 : AR9380_MAX_CHAINS;
	for (i = 0; i < maxchains; i++) {
		if (IEEE80211_IS_CHAN_5GHZ(c) &&
		    eep->base_ext2.xatten1DBLow[i] != 0) {
			if (c->ic_freq <= 5500) {
				db = athn_interpolate(c->ic_freq,
				    5180, eep->base_ext2.xatten1DBLow[i],
				    5500, modal->xatten1DB[i]);
			}
			else {
				db = athn_interpolate(c->ic_freq,
				    5500, modal->xatten1DB[i],
				    5785, eep->base_ext2.xatten1DBHigh[i]);
			}
		}
		else
			db = modal->xatten1DB[i];
		if (IEEE80211_IS_CHAN_5GHZ(c) &&
		    eep->base_ext2.xatten1MarginLow[i] != 0) {
			if (c->ic_freq <= 5500) {
				margin = athn_interpolate(c->ic_freq,
				    5180, eep->base_ext2.xatten1MarginLow[i],
				    5500, modal->xatten1Margin[i]);
			}
			else {
				margin = athn_interpolate(c->ic_freq,
				    5500, modal->xatten1Margin[i],
				    5785, eep->base_ext2.xatten1MarginHigh[i]);
			}
		}
		else
			margin = modal->xatten1Margin[i];
		reg = AR_READ(sc, AR_PHY_EXT_ATTEN_CTL(i));
		reg = RW(reg, AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, db);
		reg = RW(reg, AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN, margin);
		AR_WRITE(sc, AR_PHY_EXT_ATTEN_CTL(i), reg);
	}

	/* Initialize switching regulator. */
	if (AR_SREV_9485(sc))
		ar9485_init_swreg(sc);
	else
		ar9485_init_swreg(sc);

	/* Apply tuning capabilities. */
	if (AR_SREV_9485(sc) &&
	    (eep->baseEepHeader.featureEnable & AR_EEP_TUNING_CAPS)) {
		reg = AR_READ(sc, AR9485_PHY_CH0_XTAL);
		reg = RW(reg, AR9485_PHY_CH0_XTAL_CAPINDAC,
		    eep->baseEepHeader.params_for_tuning_caps[0]);
		reg = RW(reg, AR9485_PHY_CH0_XTAL_CAPOUTDAC,
		    eep->baseEepHeader.params_for_tuning_caps[0]);
		AR_WRITE(sc, AR9485_PHY_CH0_XTAL, reg);
	}
	AR_WRITE_BARRIER(sc);
}

#ifdef notused
Static void
ar9380_init_swreg(struct athn_softc *sc)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;

	if (eep->baseEepHeader.featureEnable & AR_EEP_INTERNAL_REGULATOR) {
		/* Internal regulator is ON. */
		AR_CLRBITS(sc, AR_RTC_REG_CONTROL1,
		    AR_RTC_REG_CONTROL1_SWREG_PROGRAM);
		AR_WRITE(sc, AR_RTC_REG_CONTROL0, eep->baseEepHeader.swreg);
		AR_SETBITS(sc, AR_RTC_REG_CONTROL1,
		    AR_RTC_REG_CONTROL1_SWREG_PROGRAM);
	}
	else
		AR_SETBITS(sc, AR_RTC_SLEEP_CLK, AR_RTC_FORCE_SWREG_PRD);
	AR_WRITE_BARRIER(sc);
}
#endif /* notused */

Static int
ar9485_pmu_write(struct athn_softc *sc, uint32_t addr, uint32_t val)
{
	int ntries;

	AR_WRITE(sc, addr, val);
	/* Wait for write to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (AR_READ(sc, addr) == val)
			return 0;
		AR_WRITE(sc, addr, val);	/* Insist. */
		AR_WRITE_BARRIER(sc);
		DELAY(10);
	}
	return ETIMEDOUT;
}

Static void
ar9485_init_swreg(struct athn_softc *sc)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	uint32_t reg;

	ar9485_pmu_write(sc, AR_PHY_PMU2,
	    ar9485_pmu_read(sc, AR_PHY_PMU2) & ~AR_PHY_PMU2_PGM);

	if (eep->baseEepHeader.featureEnable & AR_EEP_INTERNAL_REGULATOR) {
		ar9485_pmu_write(sc, AR_PHY_PMU1, 0x131dc17a);

		reg = ar9485_pmu_read(sc, AR_PHY_PMU2);
		reg = (reg & ~0xffc00000) | 0x10000000;
		ar9485_pmu_write(sc, AR_PHY_PMU2, reg);
	}
	else {
		ar9485_pmu_write(sc, AR_PHY_PMU1,
		    ar9485_pmu_read(sc, AR_PHY_PMU1) | AR_PHY_PMU1_PWD);
	}

	ar9485_pmu_write(sc, AR_PHY_PMU2,
	    ar9485_pmu_read(sc, AR_PHY_PMU2) | AR_PHY_PMU2_PGM);
}

/*
 * NB: It is safe to call this function for 5GHz channels.
 */
Static void
ar9380_spur_mitigate_cck(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	static const int16_t freqs[] = { 2420, 2440, 2464, 2480 };
	size_t i;
	int spur, freq;
	uint32_t reg;

	for (i = 0; i < __arraycount(freqs); i++) {
		spur = freqs[i] - c->ic_freq;
		if (abs(spur) < 10)	/* +/- 10MHz range. */
			break;
	}
	if (i == __arraycount(freqs)) {
		/* Disable CCK spur mitigation. */
		reg = AR_READ(sc, AR_PHY_AGC_CONTROL);
		reg = RW(reg, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x5);
		AR_WRITE(sc, AR_PHY_AGC_CONTROL, reg);
		reg = AR_READ(sc, AR_PHY_CCK_SPUR_MIT);
		reg = RW(reg, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, 0);
		reg &= ~AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT;
		AR_WRITE(sc, AR_PHY_CCK_SPUR_MIT, reg);
		AR_WRITE_BARRIER(sc);
		return;
	}
	freq = (spur * 524288) / 11;

	reg = AR_READ(sc, AR_PHY_AGC_CONTROL);
	reg = RW(reg, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x7);
	AR_WRITE(sc, AR_PHY_AGC_CONTROL, reg);

	reg = AR_READ(sc, AR_PHY_CCK_SPUR_MIT);
	reg = RW(reg, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, freq);
	reg = RW(reg, AR_PHY_CCK_SPUR_MIT_SPUR_RSSI_THR, 0x7f);
	reg = RW(reg, AR_PHY_CCK_SPUR_MIT_SPUR_FILTER_TYPE, 0x2);
	reg |= AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT;
	AR_WRITE(sc, AR_PHY_CCK_SPUR_MIT, reg);
	AR_WRITE_BARRIER(sc);
}

Static void
ar9380_spur_mitigate_ofdm(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	const uint8_t *spurchans;
	uint32_t reg;
	int idx, spur_delta_phase, spur_off, range, i;
	int freq, spur, spur_freq_sd, spur_subchannel_sd;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		spurchans = eep->modalHeader2G.spurChans;
	else
		spurchans = eep->modalHeader5G.spurChans;
	if (spurchans[0] == 0)
		return;

	/* Disable OFDM spur mitigation. */
	AR_CLRBITS(sc, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER);

	reg = AR_READ(sc, AR_PHY_TIMING11);
	reg = RW(reg, AR_PHY_TIMING11_SPUR_FREQ_SD, 0);
	reg = RW(reg, AR_PHY_TIMING11_SPUR_DELTA_PHASE, 0);
	reg &= ~AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC;
	reg &= ~AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR;
	AR_WRITE(sc, AR_PHY_TIMING11, reg);

	AR_CLRBITS(sc, AR_PHY_SFCORR_EXT,
	    AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD);

	AR_CLRBITS(sc, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI);

	reg = AR_READ(sc, AR_PHY_SPUR_REG);
	reg = RW(reg, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0);
	reg &= ~AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI;
	reg &= ~AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT;
	reg &= ~AR_PHY_SPUR_REG_ENABLE_MASK_PPM;
	AR_WRITE(sc, AR_PHY_SPUR_REG, reg);
	AR_WRITE_BARRIER(sc);

	freq = c->ic_freq;
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		range = 19;	/* +/- 19MHz range. */
		if (AR_READ(sc, AR_PHY_GEN_CTRL) & AR_PHY_GC_DYN2040_PRI_CH)
			freq += 10;
		else
			freq -= 10;
	}
	else
#endif
		range = 10;	/* +/- 10MHz range. */
	for (i = 0; i < AR9380_EEPROM_MODAL_SPURS; i++) {
		spur = spurchans[i];
		if (spur == 0)
			return;
		/* Convert to frequency. */
		if (IEEE80211_IS_CHAN_2GHZ(c))
			spur = 2300 + spur;
		else
			spur = 4900 + (spur * 5);
		spur -= freq;
		if (abs(spur) < range)
			break;
	}
	if (i == AR9380_EEPROM_MODAL_SPURS)
		return;

	/* Enable OFDM spur mitigation. */
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		spur_delta_phase = (spur * 131072) / 5;
		reg = AR_READ(sc, AR_PHY_GEN_CTRL);
		if (spur < 0) {
			spur_subchannel_sd =
			    (reg & AR_PHY_GC_DYN2040_PRI_CH) == 0;
			spur_off = spur + 10;
		}
		else {
			spur_subchannel_sd =
			    (reg & AR_PHY_GC_DYN2040_PRI_CH) != 0;
			spur_off = spur - 10;
		}
	}
	else
#endif
	{
		spur_delta_phase = (spur * 262144) / 5;
		spur_subchannel_sd = 0;
		spur_off = spur;
	}
	spur_freq_sd = (spur_off * 512) / 11;

	AR_SETBITS(sc, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER);

	reg = AR_READ(sc, AR_PHY_TIMING11);
	reg = RW(reg, AR_PHY_TIMING11_SPUR_FREQ_SD, spur_freq_sd);
	reg = RW(reg, AR_PHY_TIMING11_SPUR_DELTA_PHASE, spur_delta_phase);
	reg |= AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC;
	reg |= AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR;
	AR_WRITE(sc, AR_PHY_TIMING11, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR_EXT);
	if (spur_subchannel_sd)
		reg |= AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD;
	else
		reg &= ~AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD;
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, reg);

	AR_SETBITS(sc, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI);

	reg = AR_READ(sc, AR_PHY_SPUR_REG);
	reg = RW(reg, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0xff);
	reg = RW(reg, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH, 34);
	reg |= AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI;
	if (AR_READ(sc, AR_PHY_MODE) & AR_PHY_MODE_DYNAMIC)
		reg |= AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT;
	reg |= AR_PHY_SPUR_REG_ENABLE_MASK_PPM;
	AR_WRITE(sc, AR_PHY_SPUR_REG, reg);

	idx = (spur * 16) / 5;
	if (idx < 0)
		idx--;

	/* Write pilot mask. */
	AR_SETBITS(sc, AR_PHY_TIMING4,
	    AR_PHY_TIMING4_ENABLE_PILOT_MASK |
	    AR_PHY_TIMING4_ENABLE_CHAN_MASK);

	reg = AR_READ(sc, AR_PHY_PILOT_SPUR_MASK);
	reg = RW(reg, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, idx);
	reg = RW(reg, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0x0c);
	AR_WRITE(sc, AR_PHY_PILOT_SPUR_MASK, reg);

	reg = AR_READ(sc, AR_PHY_SPUR_MASK_A);
	reg = RW(reg, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, idx);
	reg = RW(reg, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0xa0);
	AR_WRITE(sc, AR_PHY_SPUR_MASK_A, reg);

	reg = AR_READ(sc, AR_PHY_CHAN_SPUR_MASK);
	reg = RW(reg, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, idx);
	reg = RW(reg, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0x0c);
	AR_WRITE(sc, AR_PHY_CHAN_SPUR_MASK, reg);
	AR_WRITE_BARRIER(sc);
}

Static void
ar9380_spur_mitigate(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{

	/* NB: We call spur_mitigate_cck for 5GHz too, just to disable it. */
	ar9380_spur_mitigate_cck(sc, c, extc);
	ar9380_spur_mitigate_ofdm(sc, c, extc);
}

Static void
ar9380_set_txpower(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	uint8_t tpow_cck[4], tpow_ofdm[4];
	uint8_t tpow_ht20[14], tpow_ht40[14];
	int16_t power[ATHN_POWER_COUNT];

	if (IEEE80211_IS_CHAN_2GHZ(c)) {
		/* Get CCK target powers. */
		ar9003_get_lg_tpow(sc, c, AR_CTL_11B,
		    eep->calTargetFbinCck, eep->calTargetPowerCck,
		    AR9380_NUM_2G_CCK_TARGET_POWERS, tpow_cck);

		/* Get OFDM target powers. */
		ar9003_get_lg_tpow(sc, c, AR_CTL_11G,
		    eep->calTargetFbin2G, eep->calTargetPower2G,
		    AR9380_NUM_2G_20_TARGET_POWERS, tpow_ofdm);

		/* Get HT-20 target powers. */
		ar9003_get_ht_tpow(sc, c, AR_CTL_2GHT20,
		    eep->calTargetFbin2GHT20, eep->calTargetPower2GHT20,
		    AR9380_NUM_2G_20_TARGET_POWERS, tpow_ht20);

		if (extc != NULL) {
			/* Get HT-40 target powers. */
			ar9003_get_ht_tpow(sc, c, AR_CTL_2GHT40,
			    eep->calTargetFbin2GHT40,
			    eep->calTargetPower2GHT40,
			    AR9380_NUM_2G_40_TARGET_POWERS, tpow_ht40);
		}
	}
	else {
		/* Get OFDM target powers. */
		ar9003_get_lg_tpow(sc, c, AR_CTL_11A,
		    eep->calTargetFbin5G, eep->calTargetPower5G,
		    AR9380_NUM_5G_20_TARGET_POWERS, tpow_ofdm);

		/* Get HT-20 target powers. */
		ar9003_get_ht_tpow(sc, c, AR_CTL_5GHT20,
		    eep->calTargetFbin5GHT20, eep->calTargetPower5GHT20,
		    AR9380_NUM_5G_20_TARGET_POWERS, tpow_ht20);

		if (extc != NULL) {
			/* Get HT-40 target powers. */
			ar9003_get_ht_tpow(sc, c, AR_CTL_5GHT40,
			    eep->calTargetFbin5GHT40,
			    eep->calTargetPower5GHT40,
			    AR9380_NUM_5G_40_TARGET_POWERS, tpow_ht40);
		}
	}

	memset(power, 0, sizeof(power));
	/* Shuffle target powers accross transmit rates. */
	power[ATHN_POWER_OFDM6 ] =
	power[ATHN_POWER_OFDM9 ] =
	power[ATHN_POWER_OFDM12] =
	power[ATHN_POWER_OFDM18] =
	power[ATHN_POWER_OFDM24] = tpow_ofdm[0];
	power[ATHN_POWER_OFDM36] = tpow_ofdm[1];
	power[ATHN_POWER_OFDM48] = tpow_ofdm[2];
	power[ATHN_POWER_OFDM54] = tpow_ofdm[3];
	if (IEEE80211_IS_CHAN_2GHZ(c)) {
		power[ATHN_POWER_CCK1_LP ] =
		power[ATHN_POWER_CCK2_LP ] =
		power[ATHN_POWER_CCK2_SP ] =
		power[ATHN_POWER_CCK55_LP] = tpow_cck[0];
		power[ATHN_POWER_CCK55_SP] = tpow_cck[1];
		power[ATHN_POWER_CCK11_LP] = tpow_cck[2];
		power[ATHN_POWER_CCK11_SP] = tpow_cck[3];
	}
	/* Next entry covers MCS0, MCS8 and MCS16. */
	power[ATHN_POWER_HT20( 0)] = tpow_ht20[ 0];
	/* Next entry covers MCS1-3, MCS9-11 and MCS17-19. */
	power[ATHN_POWER_HT20( 1)] = tpow_ht20[ 1];
	power[ATHN_POWER_HT20( 4)] = tpow_ht20[ 2];
	power[ATHN_POWER_HT20( 5)] = tpow_ht20[ 3];
	power[ATHN_POWER_HT20( 6)] = tpow_ht20[ 4];
	power[ATHN_POWER_HT20( 7)] = tpow_ht20[ 5];
	power[ATHN_POWER_HT20(12)] = tpow_ht20[ 6];
	power[ATHN_POWER_HT20(13)] = tpow_ht20[ 7];
	power[ATHN_POWER_HT20(14)] = tpow_ht20[ 8];
	power[ATHN_POWER_HT20(15)] = tpow_ht20[ 9];
	power[ATHN_POWER_HT20(20)] = tpow_ht20[10];
	power[ATHN_POWER_HT20(21)] = tpow_ht20[11];
	power[ATHN_POWER_HT20(22)] = tpow_ht20[12];
	power[ATHN_POWER_HT20(23)] = tpow_ht20[13];
	if (extc != NULL) {
		/* Next entry covers MCS0, MCS8 and MCS16. */
		power[ATHN_POWER_HT40( 0)] = tpow_ht40[ 0];
		/* Next entry covers MCS1-3, MCS9-11 and MCS17-19. */
		power[ATHN_POWER_HT40( 1)] = tpow_ht40[ 1];
		power[ATHN_POWER_HT40( 4)] = tpow_ht40[ 2];
		power[ATHN_POWER_HT40( 5)] = tpow_ht40[ 3];
		power[ATHN_POWER_HT40( 6)] = tpow_ht40[ 4];
		power[ATHN_POWER_HT40( 7)] = tpow_ht40[ 5];
		power[ATHN_POWER_HT40(12)] = tpow_ht40[ 6];
		power[ATHN_POWER_HT40(13)] = tpow_ht40[ 7];
		power[ATHN_POWER_HT40(14)] = tpow_ht40[ 8];
		power[ATHN_POWER_HT40(15)] = tpow_ht40[ 9];
		power[ATHN_POWER_HT40(20)] = tpow_ht40[10];
		power[ATHN_POWER_HT40(21)] = tpow_ht40[11];
		power[ATHN_POWER_HT40(22)] = tpow_ht40[12];
		power[ATHN_POWER_HT40(23)] = tpow_ht40[13];
	}

	/* Write transmit power values to hardware. */
	ar9003_write_txpower(sc, power);

	/* Apply transmit power correction. */
	ar9380_set_correction(sc, c);
}

Static void
ar9380_get_correction(struct athn_softc *sc, struct ieee80211_channel *c,
    int chain, int *corr, int *temp)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	const struct ar9380_cal_data_per_freq_op_loop *pierdata;
	const uint8_t *pierfreq;
	uint8_t fbin;
	int lo, hi, npiers;

	if (IEEE80211_IS_CHAN_2GHZ(c)) {
		pierfreq = eep->calFreqPier2G;
		pierdata = eep->calPierData2G[chain];
		npiers = AR9380_NUM_2G_CAL_PIERS;
	}
	else {
		pierfreq = eep->calFreqPier5G;
		pierdata = eep->calPierData5G[chain];
		npiers = AR9380_NUM_5G_CAL_PIERS;
	}
	/* Find channel in ROM pier table. */
	fbin = athn_chan2fbin(c);
	athn_get_pier_ival(fbin, pierfreq, npiers, &lo, &hi);

	*corr = athn_interpolate(fbin,
	    pierfreq[lo], pierdata[lo].refPower,
	    pierfreq[hi], pierdata[hi].refPower);
	*temp = athn_interpolate(fbin,
	    pierfreq[lo], pierdata[lo].tempMeas,
	    pierfreq[hi], pierdata[hi].tempMeas);
}

Static void
ar9380_set_correction(struct athn_softc *sc, struct ieee80211_channel *c)
{
	const struct ar9380_eeprom *eep = sc->sc_eep;
	const struct ar9380_modal_eep_header *modal;
	uint32_t reg;
	int8_t slope;
	int i, corr, temp, temp0;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		modal = &eep->modalHeader2G;
	else
		modal = &eep->modalHeader5G;

	temp0 = 0;	/* XXX: gcc */
	for (i = 0; i < AR9380_MAX_CHAINS; i++) {
		ar9380_get_correction(sc, c, i, &corr, &temp);
		if (i == 0)
			temp0 = temp;

		reg = AR_READ(sc, AR_PHY_TPC_11_B(i));
		reg = RW(reg, AR_PHY_TPC_11_OLPC_GAIN_DELTA, corr);
		AR_WRITE(sc, AR_PHY_TPC_11_B(i), reg);

		/* Enable open loop power control. */
		reg = AR_READ(sc, AR_PHY_TPC_6_B(i));
		reg = RW(reg, AR_PHY_TPC_6_ERROR_EST_MODE, 3);
		AR_WRITE(sc, AR_PHY_TPC_6_B(i), reg);
	}

	/* Enable temperature compensation. */
	if (IEEE80211_IS_CHAN_5GHZ(c) &&
	    eep->base_ext2.tempSlopeLow != 0) {
		if (c->ic_freq <= 5500) {
			slope = athn_interpolate(c->ic_freq,
			    5180, eep->base_ext2.tempSlopeLow,
			    5500, modal->tempSlope);
		}
		else {
			slope = athn_interpolate(c->ic_freq,
			    5500, modal->tempSlope,
			    5785, eep->base_ext2.tempSlopeHigh);
		}
	}
	else
		slope = modal->tempSlope;

	reg = AR_READ(sc, AR_PHY_TPC_19);
	reg = RW(reg, AR_PHY_TPC_19_ALPHA_THERM, slope);
	AR_WRITE(sc, AR_PHY_TPC_19, reg);

	reg = AR_READ(sc, AR_PHY_TPC_18);
	reg = RW(reg, AR_PHY_TPC_18_THERM_CAL, temp0);
	AR_WRITE(sc, AR_PHY_TPC_18, reg);
	AR_WRITE_BARRIER(sc);
}
