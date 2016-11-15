/* $NetBSD: rtwphy.c,v 1.16 2013/11/15 14:52:11 nisimura Exp $ */
/*-
 * Copyright (c) 2004, 2005 David Young.  All rights reserved.
 *
 * Programmed for NetBSD by David Young.
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
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*
 * Control the Philips SA2400 RF front-end and the baseband processor
 * built into the Realtek RTL8180.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtwphy.c,v 1.16 2013/11/15 14:52:11 nisimura Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/max2820reg.h>
#include <dev/ic/sa2400reg.h>
#include <dev/ic/rtwvar.h>
#include <dev/ic/rtwphyio.h>
#include <dev/ic/rtwphy.h>

static int rtw_max2820_pwrstate(struct rtw_rf *, enum rtw_pwrstate);
static int rtw_sa2400_pwrstate(struct rtw_rf *, enum rtw_pwrstate);

#define	GCT_WRITE(__gr, __addr, __val, __label)				\
	do {								\
		if (rtw_rfbus_write(&(__gr)->gr_bus, RTW_RFCHIPID_GCT,	\
		    (__addr), (__val)) == -1)				\
			goto __label;					\
	} while(0)

static int
rtw_bbp_preinit(struct rtw_regs *regs, u_int antatten0, int dflantb,
    u_int freq)
{
	u_int antatten = antatten0;
	if (dflantb)
		antatten |= RTW_BBP_ANTATTEN_DFLANTB;
	if (freq == 2484) /* channel 14 */
		antatten |= RTW_BBP_ANTATTEN_CHAN14;
	return rtw_bbp_write(regs, RTW_BBP_ANTATTEN, antatten);
}

static int
rtw_bbp_init(struct rtw_regs *regs, struct rtw_bbpset *bb, int antdiv,
    int dflantb, uint8_t cs_threshold, u_int freq)
{
	int rc;
	uint32_t sys2, sys3;

	sys2 = bb->bb_sys2;
	if (antdiv)
		sys2 |= RTW_BBP_SYS2_ANTDIV;
	sys3 = bb->bb_sys3 |
	    __SHIFTIN(cs_threshold, RTW_BBP_SYS3_CSTHRESH_MASK);

#define	RTW_BBP_WRITE_OR_RETURN(reg, val) \
	if ((rc = rtw_bbp_write(regs, reg, val)) != 0) \
		return rc;

	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_SYS1,		bb->bb_sys1);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_TXAGC,		bb->bb_txagc);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_LNADET,		bb->bb_lnadet);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_IFAGCINI,	bb->bb_ifagcini);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_IFAGCLIMIT,	bb->bb_ifagclimit);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_IFAGCDET,	bb->bb_ifagcdet);

	if ((rc = rtw_bbp_preinit(regs, bb->bb_antatten, dflantb, freq)) != 0)
		return rc;

	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_TRL,		bb->bb_trl);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_SYS2,		sys2);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_SYS3,		sys3);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_CHESTLIM,	bb->bb_chestlim);
	RTW_BBP_WRITE_OR_RETURN(RTW_BBP_CHSQLIM,	bb->bb_chsqlim);
	return 0;
}

static int
rtw_sa2400_txpower(struct rtw_rf *rf, uint8_t opaque_txpower)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	struct rtw_rfbus *bus = &sa->sa_bus;

	return rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_TX,
	    opaque_txpower);
}

/* make sure we're using the same settings as the reference driver */
static void
verify_syna(u_int freq, uint32_t val)
{
#ifdef DIAGNOSTIC
	uint32_t expected_val = ~val;

	switch (freq) {
	case 2412:
		expected_val = 0x0000096c; /* ch 1 */
		break;
	case 2417:
		expected_val = 0x00080970; /* ch 2 */
		break;
	case 2422:
		expected_val = 0x00100974; /* ch 3 */
		break;
	case 2427:
		expected_val = 0x00180978; /* ch 4 */
		break;
	case 2432:
		expected_val = 0x00000980; /* ch 5 */
		break;
	case 2437:
		expected_val = 0x00080984; /* ch 6 */
		break;
	case 2442:
		expected_val = 0x00100988; /* ch 7 */
		break;
	case 2447:
		expected_val = 0x0018098c; /* ch 8 */
		break;
	case 2452:
		expected_val = 0x00000994; /* ch 9 */
		break;
	case 2457:
		expected_val = 0x00080998; /* ch 10 */
		break;
	case 2462:
		expected_val = 0x0010099c; /* ch 11 */
		break;
	case 2467:
		expected_val = 0x001809a0; /* ch 12 */
		break;
        case 2472:
		expected_val = 0x000009a8; /* ch 13 */
		break;
        case 2484:
		expected_val = 0x000009b4; /* ch 14 */
		break;
	}
	KASSERT(val == expected_val);
#endif
}

/* freq is in MHz */
static int
rtw_sa2400_tune(struct rtw_rf *rf, u_int freq)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	struct rtw_rfbus *bus = &sa->sa_bus;
	int rc;
	uint32_t syna, synb, sync;

	/* XO = 44MHz, R = 11, hence N is in units of XO / R = 4MHz.
	 *
	 * The channel spacing (5MHz) is not divisible by 4MHz, so
	 * we set the fractional part of N to compensate.
	 */
	int n = freq / 4, nf = (freq % 4) * 2;

	syna = __SHIFTIN(nf, SA2400_SYNA_NF_MASK) | __SHIFTIN(n, SA2400_SYNA_N_MASK);
	verify_syna(freq, syna);

	/* Divide the 44MHz crystal down to 4MHz. Set the fractional
	 * compensation charge pump value to agree with the fractional
	 * modulus.
	 */
	synb = __SHIFTIN(11, SA2400_SYNB_R_MASK) | SA2400_SYNB_L_NORMAL |
	    SA2400_SYNB_ON | SA2400_SYNB_ONE |
	    __SHIFTIN(80, SA2400_SYNB_FC_MASK); /* agrees w/ SA2400_SYNA_FM = 0 */

	sync = SA2400_SYNC_CP_NORMAL;

	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_SYNA,
	    syna)) != 0)
		return rc;
	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_SYNB,
	    synb)) != 0)
		return rc;
	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_SYNC,
	    sync)) != 0)
		return rc;
	return rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_SYND, 0x0);
}

static int
rtw_sa2400_pwrstate(struct rtw_rf *rf, enum rtw_pwrstate power)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	struct rtw_rfbus *bus = &sa->sa_bus;
	uint32_t opmode;
	opmode = SA2400_OPMODE_DEFAULTS;
	switch (power) {
	case RTW_ON:
		opmode |= SA2400_OPMODE_MODE_TXRX;
		break;
	case RTW_SLEEP:
		opmode |= SA2400_OPMODE_MODE_WAIT;
		break;
	case RTW_OFF:
		opmode |= SA2400_OPMODE_MODE_SLEEP;
		break;
	}

	if (sa->sa_digphy)
		opmode |= SA2400_OPMODE_DIGIN;

	return rtw_rfbus_write(bus, RTW_RFCHIPID_PHILIPS, SA2400_OPMODE,
	    opmode);
}

static int
rtw_sa2400_manrx_init(struct rtw_sa2400 *sa)
{
	uint32_t manrx;

	/* XXX we are not supposed to be in RXMGC mode when we do
	 * this?
	 */
	manrx = SA2400_MANRX_AHSN;
	manrx |= SA2400_MANRX_TEN;
	manrx |= __SHIFTIN(1023, SA2400_MANRX_RXGAIN_MASK);

	return rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_MANRX,
	    manrx);
}

static int
rtw_sa2400_vcocal_start(struct rtw_sa2400 *sa, int start)
{
	uint32_t opmode;

	opmode = SA2400_OPMODE_DEFAULTS;
	if (start)
		opmode |= SA2400_OPMODE_MODE_VCOCALIB;
	else
		opmode |= SA2400_OPMODE_MODE_SLEEP;

	if (sa->sa_digphy)
		opmode |= SA2400_OPMODE_DIGIN;

	return rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_OPMODE,
	    opmode);
}

static int
rtw_sa2400_vco_calibration(struct rtw_sa2400 *sa)
{
	int rc;
	/* calibrate VCO */
	if ((rc = rtw_sa2400_vcocal_start(sa, 1)) != 0)
		return rc;
	DELAY(2200);	/* 2.2 milliseconds */
	/* XXX superfluous: SA2400 automatically entered SLEEP mode. */
	return rtw_sa2400_vcocal_start(sa, 0);
}

static int
rtw_sa2400_filter_calibration(struct rtw_sa2400 *sa)
{
	uint32_t opmode;

	opmode = SA2400_OPMODE_DEFAULTS | SA2400_OPMODE_MODE_FCALIB;
	if (sa->sa_digphy)
		opmode |= SA2400_OPMODE_DIGIN;

	return rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_OPMODE,
	    opmode);
}

static int
rtw_sa2400_dc_calibration(struct rtw_sa2400 *sa)
{
	struct rtw_rf *rf = &sa->sa_rf;
	int rc;
	uint32_t dccal;

	(*rf->rf_continuous_tx_cb)(rf->rf_continuous_tx_arg, 1);

	dccal = SA2400_OPMODE_DEFAULTS | SA2400_OPMODE_MODE_TXRX;

	rc = rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_OPMODE,
	    dccal);
	if (rc != 0)
		return rc;

	DELAY(5);	/* DCALIB after being in Tx mode for 5
			 * microseconds
			 */

	dccal &= ~SA2400_OPMODE_MODE_MASK;
	dccal |= SA2400_OPMODE_MODE_DCALIB;

	rc = rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_OPMODE,
	   dccal);
	if (rc != 0)
		return rc;

	DELAY(20);	/* calibration takes at most 20 microseconds */

	(*rf->rf_continuous_tx_cb)(rf->rf_continuous_tx_arg, 0);

	return 0;
}

static int
rtw_sa2400_agc_init(struct rtw_sa2400 *sa)
{
	uint32_t agc;

	agc = __SHIFTIN(25, SA2400_AGC_MAXGAIN_MASK);
	agc |= __SHIFTIN(7, SA2400_AGC_BBPDELAY_MASK);
	agc |= __SHIFTIN(15, SA2400_AGC_LNADELAY_MASK);
	agc |= __SHIFTIN(27, SA2400_AGC_RXONDELAY_MASK);

	return rtw_rfbus_write(&sa->sa_bus, RTW_RFCHIPID_PHILIPS, SA2400_AGC,
	    agc);
}

static void
rtw_sa2400_destroy(struct rtw_rf *rf)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	memset(sa, 0, sizeof(*sa));
	free(sa, M_DEVBUF);
}

static int
rtw_sa2400_calibrate(struct rtw_rf *rf, u_int freq)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	int i, rc;

	/* XXX reference driver calibrates VCO twice. Is it a bug? */
	for (i = 0; i < 2; i++) {
		if ((rc = rtw_sa2400_vco_calibration(sa)) != 0)
			return rc;
	}
	/* VCO calibration erases synthesizer registers, so re-tune */
	if ((rc = rtw_sa2400_tune(rf, freq)) != 0)
		return rc;
	if ((rc = rtw_sa2400_filter_calibration(sa)) != 0)
		return rc;
	/* analog PHY needs DC calibration */
	if (!sa->sa_digphy)
		return rtw_sa2400_dc_calibration(sa);
	return 0;
}

static int
rtw_sa2400_init(struct rtw_rf *rf, u_int freq, uint8_t opaque_txpower,
    enum rtw_pwrstate power)
{
	struct rtw_sa2400 *sa = (struct rtw_sa2400 *)rf;
	int rc;

	if ((rc = rtw_sa2400_txpower(rf, opaque_txpower)) != 0)
		return rc;

	/* skip configuration if it's time to sleep or to power-down. */
	if (power == RTW_SLEEP || power == RTW_OFF)
		return rtw_sa2400_pwrstate(rf, power);

	/* go to sleep for configuration */
	if ((rc = rtw_sa2400_pwrstate(rf, RTW_SLEEP)) != 0)
		return rc;

	if ((rc = rtw_sa2400_tune(rf, freq)) != 0)
		return rc;
	if ((rc = rtw_sa2400_agc_init(sa)) != 0)
		return rc;
	if ((rc = rtw_sa2400_manrx_init(sa)) != 0)
		return rc;
	if ((rc = rtw_sa2400_calibrate(rf, freq)) != 0)
		return rc;

	/* enter Tx/Rx mode */
	return rtw_sa2400_pwrstate(rf, power);
}

struct rtw_rf *
rtw_sa2400_create(struct rtw_regs *regs, rtw_rf_write_t rf_write, int digphy)
{
	struct rtw_sa2400 *sa;
	struct rtw_rfbus *bus;
	struct rtw_rf *rf;
	struct rtw_bbpset *bb;

	sa = malloc(sizeof(*sa), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sa == NULL)
		return NULL;

	sa->sa_digphy = digphy;

	rf = &sa->sa_rf;
	bus = &sa->sa_bus;

	rf->rf_init = rtw_sa2400_init;
	rf->rf_destroy = rtw_sa2400_destroy;
	rf->rf_txpower = rtw_sa2400_txpower;
	rf->rf_tune = rtw_sa2400_tune;
	rf->rf_pwrstate = rtw_sa2400_pwrstate;
	bb = &rf->rf_bbpset;

	/* XXX magic */
	bb->bb_antatten = RTW_BBP_ANTATTEN_PHILIPS_MAGIC;
	bb->bb_chestlim =	0x00;
	bb->bb_chsqlim =	0xa0;
	bb->bb_ifagcdet =	0x64;
	bb->bb_ifagcini =	0x90;
	bb->bb_ifagclimit =	0x1a;
	bb->bb_lnadet =		0xe0;
	bb->bb_sys1 =		0x98;
	bb->bb_sys2 =		0x47;
	bb->bb_sys3 =		0x90;
	bb->bb_trl =		0x88;
	bb->bb_txagc =		0x38;

	bus->b_regs = regs;
	bus->b_write = rf_write;

	return &sa->sa_rf;
}

static int
rtw_grf5101_txpower(struct rtw_rf *rf, uint8_t opaque_txpower)
{
	struct rtw_grf5101 *gr = (struct rtw_grf5101 *)rf;

	GCT_WRITE(gr, 0x15, 0, err);
	GCT_WRITE(gr, 0x06, opaque_txpower, err);
	GCT_WRITE(gr, 0x15, 0x10, err);
	GCT_WRITE(gr, 0x15, 0x00, err);
	return 0;
err:
	return -1;
}

static int
rtw_grf5101_pwrstate(struct rtw_rf *rf, enum rtw_pwrstate power)
{
	struct rtw_grf5101 *gr = (struct rtw_grf5101 *)rf;
	switch (power) {
	case RTW_OFF:
	case RTW_SLEEP:
		GCT_WRITE(gr, 0x07, 0x0000, err);
		GCT_WRITE(gr, 0x1f, 0x0045, err);
		GCT_WRITE(gr, 0x1f, 0x0005, err);
		GCT_WRITE(gr, 0x00, 0x08e4, err);
	default:
		break;
	case RTW_ON:
		GCT_WRITE(gr, 0x1f, 0x0001, err);
		DELAY(10);
		GCT_WRITE(gr, 0x1f, 0x0001, err);
		DELAY(10);
		GCT_WRITE(gr, 0x1f, 0x0041, err);
		DELAY(10);
		GCT_WRITE(gr, 0x1f, 0x0061, err);
		DELAY(10);
		GCT_WRITE(gr, 0x00, 0x0ae4, err);
		DELAY(10);
		GCT_WRITE(gr, 0x07, 0x1000, err);
		DELAY(100);
		break;
	}

	return 0;
err:
	return -1;
}

static int
rtw_grf5101_tune(struct rtw_rf *rf, u_int freq)
{
	int channel;
	struct rtw_grf5101 *gr = (struct rtw_grf5101 *)rf;

	if (freq == 2484)
		channel = 14;
	else if ((channel = (freq - 2412) / 5 + 1) < 1 || channel > 13) {
		RTW_DPRINTF(RTW_DEBUG_PHY,
		    ("%s: invalid channel %d (freq %d)\n", __func__, channel,
		     freq));
		return -1;
	}

	GCT_WRITE(gr, 0x07, 0, err);
	GCT_WRITE(gr, 0x0b, channel - 1, err);
	GCT_WRITE(gr, 0x07, 0x1000, err);
	return 0;
err:
	return -1;
}

static int
rtw_grf5101_init(struct rtw_rf *rf, u_int freq, uint8_t opaque_txpower,
    enum rtw_pwrstate power)
{
	int rc;
	struct rtw_grf5101 *gr = (struct rtw_grf5101 *)rf;

	/*
         * These values have been derived from the rtl8180-sa2400
         * Linux driver.  It is unknown what they all do, GCT refuse
         * to release any documentation so these are more than
         * likely sub optimal settings
	 */

	GCT_WRITE(gr, 0x01, 0x1a23, err);
	GCT_WRITE(gr, 0x02, 0x4971, err);
	GCT_WRITE(gr, 0x03, 0x41de, err);
	GCT_WRITE(gr, 0x04, 0x2d80, err);

	GCT_WRITE(gr, 0x05, 0x61ff, err);

	GCT_WRITE(gr, 0x06, 0x0, err);

	GCT_WRITE(gr, 0x08, 0x7533, err);
	GCT_WRITE(gr, 0x09, 0xc401, err);
	GCT_WRITE(gr, 0x0a, 0x0, err);
	GCT_WRITE(gr, 0x0c, 0x1c7, err);
	GCT_WRITE(gr, 0x0d, 0x29d3, err);
	GCT_WRITE(gr, 0x0e, 0x2e8, err);
	GCT_WRITE(gr, 0x10, 0x192, err);
	GCT_WRITE(gr, 0x11, 0x248, err);
	GCT_WRITE(gr, 0x12, 0x0, err);
	GCT_WRITE(gr, 0x13, 0x20c4, err);
	GCT_WRITE(gr, 0x14, 0xf4fc, err);
	GCT_WRITE(gr, 0x15, 0x0, err);
	GCT_WRITE(gr, 0x16, 0x1500, err);

	if ((rc = rtw_grf5101_txpower(rf, opaque_txpower)) != 0)
		return rc;

	if ((rc = rtw_grf5101_tune(rf, freq)) != 0)
		return rc;

	return 0;
err:
	return -1;
}

static void
rtw_grf5101_destroy(struct rtw_rf *rf)
{
	struct rtw_grf5101 *gr = (struct rtw_grf5101 *)rf;
	memset(gr, 0, sizeof(*gr));
	free(gr, M_DEVBUF);
}

struct rtw_rf *
rtw_grf5101_create(struct rtw_regs *regs, rtw_rf_write_t rf_write,
    int digphy)
{
	struct rtw_grf5101 *gr;
	struct rtw_rfbus *bus;
	struct rtw_rf *rf;
	struct rtw_bbpset *bb;

	gr = malloc(sizeof(*gr), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (gr == NULL)
		return NULL;

	rf = &gr->gr_rf;
	bus = &gr->gr_bus;

	rf->rf_init = rtw_grf5101_init;
	rf->rf_destroy = rtw_grf5101_destroy;
	rf->rf_txpower = rtw_grf5101_txpower;
	rf->rf_tune = rtw_grf5101_tune;
	rf->rf_pwrstate = rtw_grf5101_pwrstate;
	bb = &rf->rf_bbpset;

	/* XXX magic */
	bb->bb_antatten = RTW_BBP_ANTATTEN_GCT_MAGIC;
	bb->bb_chestlim =       0x00;
	bb->bb_chsqlim =        0xa0;
	bb->bb_ifagcdet =       0x64;
	bb->bb_ifagcini =       0x90;
	bb->bb_ifagclimit =     0x1e;
	bb->bb_lnadet =         0xc0;
	bb->bb_sys1 =           0xa8;
	bb->bb_sys2 =           0x47;
	bb->bb_sys3 =           0x9b;
	bb->bb_trl =            0x88;
	bb->bb_txagc =          0x08;

	bus->b_regs = regs;
	bus->b_write = rf_write;

	return &gr->gr_rf;
}

/* freq is in MHz */
static int
rtw_max2820_tune(struct rtw_rf *rf, u_int freq)
{
	struct rtw_max2820 *mx = (struct rtw_max2820 *)rf;
	struct rtw_rfbus *bus = &mx->mx_bus;

	if (freq < 2400 || freq > 2499)
		return -1;

	return rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_CHANNEL,
	    __SHIFTIN(freq - 2400, MAX2820_CHANNEL_CF_MASK));
}

static void
rtw_max2820_destroy(struct rtw_rf *rf)
{
	struct rtw_max2820 *mx = (struct rtw_max2820 *)rf;
	memset(mx, 0, sizeof(*mx));
	free(mx, M_DEVBUF);
}

static int
rtw_max2820_init(struct rtw_rf *rf, u_int freq, uint8_t opaque_txpower,
    enum rtw_pwrstate power)
{
	struct rtw_max2820 *mx = (struct rtw_max2820 *)rf;
	struct rtw_rfbus *bus = &mx->mx_bus;
	int rc;

	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_TEST,
	    MAX2820_TEST_DEFAULT)) != 0)
		return rc;

	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_ENABLE,
	    MAX2820_ENABLE_DEFAULT)) != 0)
		return rc;

	/* skip configuration if it's time to sleep or to power-down. */
	if ((rc = rtw_max2820_pwrstate(rf, power)) != 0)
		return rc;
	else if (power == RTW_OFF || power == RTW_SLEEP)
		return 0;

	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_SYNTH,
	    MAX2820_SYNTH_R_44MHZ)) != 0)
		return rc;

	if ((rc = rtw_max2820_tune(rf, freq)) != 0)
		return rc;

	/* XXX The MAX2820 datasheet indicates that 1C and 2C should not
	 * be changed from 7, however, the reference driver sets them
	 * to 4 and 1, respectively.
	 */
	if ((rc = rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_RECEIVE,
	    MAX2820_RECEIVE_DL_DEFAULT |
	    __SHIFTIN(4, MAX2820A_RECEIVE_1C_MASK) |
	    __SHIFTIN(1, MAX2820A_RECEIVE_2C_MASK))) != 0)
		return rc;

	return rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_TRANSMIT,
	    MAX2820_TRANSMIT_PA_DEFAULT);
}

static int
rtw_max2820_txpower(struct rtw_rf *rf, uint8_t opaque_txpower)
{
	/* TBD */
	return 0;
}

static int
rtw_max2820_pwrstate(struct rtw_rf *rf, enum rtw_pwrstate power)
{
	uint32_t enable;
	struct rtw_max2820 *mx;
	struct rtw_rfbus *bus;

	mx = (struct rtw_max2820 *)rf;
	bus = &mx->mx_bus;

	switch (power) {
	case RTW_OFF:
	case RTW_SLEEP:
	default:
		enable = 0x0;
		break;
	case RTW_ON:
		enable = MAX2820_ENABLE_DEFAULT;
		break;
	}
	return rtw_rfbus_write(bus, RTW_RFCHIPID_MAXIM, MAX2820_ENABLE, enable);
}

struct rtw_rf *
rtw_max2820_create(struct rtw_regs *regs, rtw_rf_write_t rf_write, int is_a)
{
	struct rtw_max2820 *mx;
	struct rtw_rfbus *bus;
	struct rtw_rf *rf;
	struct rtw_bbpset *bb;

	mx = malloc(sizeof(*mx), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mx == NULL)
		return NULL;

	mx->mx_is_a = is_a;

	rf = &mx->mx_rf;
	bus = &mx->mx_bus;

	rf->rf_init = rtw_max2820_init;
	rf->rf_destroy = rtw_max2820_destroy;
	rf->rf_txpower = rtw_max2820_txpower;
	rf->rf_tune = rtw_max2820_tune;
	rf->rf_pwrstate = rtw_max2820_pwrstate;
	bb = &rf->rf_bbpset;

	/* XXX magic */
	bb->bb_antatten = RTW_BBP_ANTATTEN_MAXIM_MAGIC;
	bb->bb_chestlim =	0;
	bb->bb_chsqlim =	159;
	bb->bb_ifagcdet =	100;
	bb->bb_ifagcini =	144;
	bb->bb_ifagclimit =	26;
	bb->bb_lnadet =		248;
	bb->bb_sys1 =		136;
	bb->bb_sys2 =		71;
	bb->bb_sys3 =		155;
	bb->bb_trl =		136;
	bb->bb_txagc =		8;

	bus->b_regs = regs;
	bus->b_write = rf_write;

	return &mx->mx_rf;
}

/* freq is in MHz */
int
rtw_phy_init(struct rtw_regs *regs, struct rtw_rf *rf, uint8_t opaque_txpower,
    uint8_t cs_threshold, u_int freq, int antdiv, int dflantb,
    enum rtw_pwrstate power)
{
	int rc;
	RTW_DPRINTF(RTW_DEBUG_PHY,
	    ("%s: txpower %u csthresh %u freq %u antdiv %u dflantb %u "
	     "pwrstate %s\n", __func__, opaque_txpower, cs_threshold, freq,
	     antdiv, dflantb, rtw_pwrstate_string(power)));

	/* XXX is this really necessary? */
	if ((rc = rtw_rf_txpower(rf, opaque_txpower)) != 0)
		return rc;
	if ((rc = rtw_bbp_preinit(regs, rf->rf_bbpset.bb_antatten, dflantb,
	    freq)) != 0)
		return rc;
	if ((rc = rtw_rf_tune(rf, freq)) != 0)
		return rc;
	/* initialize RF  */
	if ((rc = rtw_rf_init(rf, freq, opaque_txpower, power)) != 0)
		return rc;
#if 0	/* what is this redundant tx power setting here for? */
	if ((rc = rtw_rf_txpower(rf, opaque_txpower)) != 0)
		return rc;
#endif
	return rtw_bbp_init(regs, &rf->rf_bbpset, antdiv, dflantb,
	    cs_threshold, freq);
}
