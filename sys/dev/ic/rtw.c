/* $NetBSD: rtw.c,v 1.121 2014/02/25 18:30:09 pooka Exp $ */
/*-
 * Copyright (c) 2004, 2005, 2006, 2007 David Young.  All rights
 * reserved.
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
 * Device driver for the Realtek RTL8180 802.11 MAC/BBP.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtw.c,v 1.121 2014/02/25 18:30:09 pooka Exp $");


#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/sockio.h>

#include <machine/endian.h>
#include <sys/bus.h>
#include <sys/intr.h>	/* splnet */

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <net/bpf.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/rtwvar.h>
#include <dev/ic/rtwphyio.h>
#include <dev/ic/rtwphy.h>

#include <dev/ic/smc93cx6var.h>

static int rtw_rfprog_fallback = 0;
static int rtw_host_rfio = 0;

#ifdef RTW_DEBUG
int rtw_debug = 0;
static int rtw_rxbufs_limit = RTW_RXQLEN;
#endif /* RTW_DEBUG */

#define NEXT_ATTACH_STATE(sc, state) do {			\
	DPRINTF(sc, RTW_DEBUG_ATTACH,				\
	    ("%s: attach state %s\n", __func__, #state));	\
	sc->sc_attach_state = state;				\
} while (0)

int rtw_dwelltime = 200;	/* milliseconds */
static struct ieee80211_cipher rtw_cipher_wep;

static void rtw_disable_interrupts(struct rtw_regs *);
static void rtw_enable_interrupts(struct rtw_softc *);

static int rtw_init(struct ifnet *);

static void rtw_start(struct ifnet *);
static void rtw_reset_oactive(struct rtw_softc *);
static struct mbuf *rtw_beacon_alloc(struct rtw_softc *,
    struct ieee80211_node *);
static u_int rtw_txring_next(struct rtw_regs *, struct rtw_txdesc_blk *);

static void rtw_io_enable(struct rtw_softc *, uint8_t, int);
static int rtw_key_delete(struct ieee80211com *, const struct ieee80211_key *);
static int rtw_key_set(struct ieee80211com *, const struct ieee80211_key *,
    const u_int8_t[IEEE80211_ADDR_LEN]);
static void rtw_key_update_end(struct ieee80211com *);
static void rtw_key_update_begin(struct ieee80211com *);
static int rtw_wep_decap(struct ieee80211_key *, struct mbuf *, int);
static void rtw_wep_setkeys(struct rtw_softc *, struct ieee80211_key *, int);

static void rtw_led_attach(struct rtw_led_state *, void *);
static void rtw_led_detach(struct rtw_led_state *);
static void rtw_led_init(struct rtw_regs *);
static void rtw_led_slowblink(void *);
static void rtw_led_fastblink(void *);
static void rtw_led_set(struct rtw_led_state *, struct rtw_regs *, int);

static int rtw_sysctl_verify_rfio(SYSCTLFN_PROTO);
static int rtw_sysctl_verify_rfprog(SYSCTLFN_PROTO);
#ifdef RTW_DEBUG
static void rtw_dump_rings(struct rtw_softc *sc);
static void rtw_print_txdesc(struct rtw_softc *, const char *,
    struct rtw_txsoft *, struct rtw_txdesc_blk *, int);
static int rtw_sysctl_verify_debug(SYSCTLFN_PROTO);
static int rtw_sysctl_verify_rxbufs_limit(SYSCTLFN_PROTO);
#endif /* RTW_DEBUG */
#ifdef RTW_DIAG
static void rtw_txring_fixup(struct rtw_softc *sc, const char *fn, int ln);
#endif /* RTW_DIAG */

/*
 * Setup sysctl(3) MIB, hw.rtw.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_rtw, "sysctl rtw(4) subtree setup")
{
	int rc;
	const struct sysctlnode *cnode, *rnode;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "rtw",
	    "Realtek RTL818x 802.11 controls",
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

#ifdef RTW_DEBUG
	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable RTL818x debugging output"),
	    rtw_sysctl_verify_debug, 0, &rtw_debug, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* Limit rx buffers, for simulating resource exhaustion. */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "rxbufs_limit",
	    SYSCTL_DESCR("Set rx buffers limit"),
	    rtw_sysctl_verify_rxbufs_limit, 0, &rtw_rxbufs_limit, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;

#endif /* RTW_DEBUG */
	/* set fallback RF programming method */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "rfprog_fallback",
	    SYSCTL_DESCR("Set fallback RF programming method"),
	    rtw_sysctl_verify_rfprog, 0, &rtw_rfprog_fallback, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* force host to control RF I/O bus */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "host_rfio", SYSCTL_DESCR("Enable host control of RF I/O"),
	    rtw_sysctl_verify_rfio, 0, &rtw_host_rfio, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return;
err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
rtw_sysctl_verify(SYSCTLFN_ARGS, int lower, int upper)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t < lower || t > upper)
		return (EINVAL);

	*(int*)rnode->sysctl_data = t;

	return (0);
}

static int
rtw_sysctl_verify_rfprog(SYSCTLFN_ARGS)
{
	return rtw_sysctl_verify(SYSCTLFN_CALL(__UNCONST(rnode)), 0,
	    __SHIFTOUT(RTW_CONFIG4_RFTYPE_MASK, RTW_CONFIG4_RFTYPE_MASK));
}

static int
rtw_sysctl_verify_rfio(SYSCTLFN_ARGS)
{
	return rtw_sysctl_verify(SYSCTLFN_CALL(__UNCONST(rnode)), 0, 1);
}

#ifdef RTW_DEBUG
static int
rtw_sysctl_verify_debug(SYSCTLFN_ARGS)
{
	return rtw_sysctl_verify(SYSCTLFN_CALL(__UNCONST(rnode)),
	    0, RTW_DEBUG_MAX);
}

static int
rtw_sysctl_verify_rxbufs_limit(SYSCTLFN_ARGS)
{
	return rtw_sysctl_verify(SYSCTLFN_CALL(__UNCONST(rnode)),
	    0, RTW_RXQLEN);
}

static void
rtw_print_regs(struct rtw_regs *regs, const char *dvname, const char *where)
{
#define PRINTREG32(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %08x\n",	\
	    dvname, reg, RTW_READ(regs, reg)))

#define PRINTREG16(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %04x\n",	\
	    dvname, reg, RTW_READ16(regs, reg)))

#define PRINTREG8(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %02x\n",	\
	    dvname, reg, RTW_READ8(regs, reg)))

	RTW_DPRINTF(RTW_DEBUG_REGDUMP, ("%s: %s\n", dvname, where));

	PRINTREG32(regs, RTW_IDR0);
	PRINTREG32(regs, RTW_IDR1);
	PRINTREG32(regs, RTW_MAR0);
	PRINTREG32(regs, RTW_MAR1);
	PRINTREG32(regs, RTW_TSFTRL);
	PRINTREG32(regs, RTW_TSFTRH);
	PRINTREG32(regs, RTW_TLPDA);
	PRINTREG32(regs, RTW_TNPDA);
	PRINTREG32(regs, RTW_THPDA);
	PRINTREG32(regs, RTW_TCR);
	PRINTREG32(regs, RTW_RCR);
	PRINTREG32(regs, RTW_TINT);
	PRINTREG32(regs, RTW_TBDA);
	PRINTREG32(regs, RTW_ANAPARM);
	PRINTREG32(regs, RTW_BB);
	PRINTREG32(regs, RTW_PHYCFG);
	PRINTREG32(regs, RTW_WAKEUP0L);
	PRINTREG32(regs, RTW_WAKEUP0H);
	PRINTREG32(regs, RTW_WAKEUP1L);
	PRINTREG32(regs, RTW_WAKEUP1H);
	PRINTREG32(regs, RTW_WAKEUP2LL);
	PRINTREG32(regs, RTW_WAKEUP2LH);
	PRINTREG32(regs, RTW_WAKEUP2HL);
	PRINTREG32(regs, RTW_WAKEUP2HH);
	PRINTREG32(regs, RTW_WAKEUP3LL);
	PRINTREG32(regs, RTW_WAKEUP3LH);
	PRINTREG32(regs, RTW_WAKEUP3HL);
	PRINTREG32(regs, RTW_WAKEUP3HH);
	PRINTREG32(regs, RTW_WAKEUP4LL);
	PRINTREG32(regs, RTW_WAKEUP4LH);
	PRINTREG32(regs, RTW_WAKEUP4HL);
	PRINTREG32(regs, RTW_WAKEUP4HH);
	PRINTREG32(regs, RTW_DK0);
	PRINTREG32(regs, RTW_DK1);
	PRINTREG32(regs, RTW_DK2);
	PRINTREG32(regs, RTW_DK3);
	PRINTREG32(regs, RTW_RETRYCTR);
	PRINTREG32(regs, RTW_RDSAR);
	PRINTREG32(regs, RTW_FER);
	PRINTREG32(regs, RTW_FEMR);
	PRINTREG32(regs, RTW_FPSR);
	PRINTREG32(regs, RTW_FFER);

	/* 16-bit registers */
	PRINTREG16(regs, RTW_BRSR);
	PRINTREG16(regs, RTW_IMR);
	PRINTREG16(regs, RTW_ISR);
	PRINTREG16(regs, RTW_BCNITV);
	PRINTREG16(regs, RTW_ATIMWND);
	PRINTREG16(regs, RTW_BINTRITV);
	PRINTREG16(regs, RTW_ATIMTRITV);
	PRINTREG16(regs, RTW_CRC16ERR);
	PRINTREG16(regs, RTW_CRC0);
	PRINTREG16(regs, RTW_CRC1);
	PRINTREG16(regs, RTW_CRC2);
	PRINTREG16(regs, RTW_CRC3);
	PRINTREG16(regs, RTW_CRC4);
	PRINTREG16(regs, RTW_CWR);

	/* 8-bit registers */
	PRINTREG8(regs, RTW_CR);
	PRINTREG8(regs, RTW_9346CR);
	PRINTREG8(regs, RTW_CONFIG0);
	PRINTREG8(regs, RTW_CONFIG1);
	PRINTREG8(regs, RTW_CONFIG2);
	PRINTREG8(regs, RTW_MSR);
	PRINTREG8(regs, RTW_CONFIG3);
	PRINTREG8(regs, RTW_CONFIG4);
	PRINTREG8(regs, RTW_TESTR);
	PRINTREG8(regs, RTW_PSR);
	PRINTREG8(regs, RTW_SCR);
	PRINTREG8(regs, RTW_PHYDELAY);
	PRINTREG8(regs, RTW_CRCOUNT);
	PRINTREG8(regs, RTW_PHYADDR);
	PRINTREG8(regs, RTW_PHYDATAW);
	PRINTREG8(regs, RTW_PHYDATAR);
	PRINTREG8(regs, RTW_CONFIG5);
	PRINTREG8(regs, RTW_TPPOLL);

	PRINTREG16(regs, RTW_BSSID16);
	PRINTREG32(regs, RTW_BSSID32);
#undef PRINTREG32
#undef PRINTREG16
#undef PRINTREG8
}
#endif /* RTW_DEBUG */

void
rtw_continuous_tx_enable(struct rtw_softc *sc, int enable)
{
	struct rtw_regs *regs = &sc->sc_regs;

	uint32_t tcr;
	tcr = RTW_READ(regs, RTW_TCR);
	tcr &= ~RTW_TCR_LBK_MASK;
	if (enable)
		tcr |= RTW_TCR_LBK_CONT;
	else
		tcr |= RTW_TCR_LBK_NORMAL;
	RTW_WRITE(regs, RTW_TCR, tcr);
	RTW_SYNC(regs, RTW_TCR, RTW_TCR);
	rtw_set_access(regs, RTW_ACCESS_ANAPARM);
	rtw_txdac_enable(sc, !enable);
	rtw_set_access(regs, RTW_ACCESS_ANAPARM);/* XXX Voodoo from Linux. */
	rtw_set_access(regs, RTW_ACCESS_NONE);
}

#ifdef RTW_DEBUG
static const char *
rtw_access_string(enum rtw_access access)
{
	switch (access) {
	case RTW_ACCESS_NONE:
		return "none";
	case RTW_ACCESS_CONFIG:
		return "config";
	case RTW_ACCESS_ANAPARM:
		return "anaparm";
	default:
		return "unknown";
	}
}
#endif /* RTW_DEBUG */

static void
rtw_set_access1(struct rtw_regs *regs, enum rtw_access naccess)
{
	KASSERT(/* naccess >= RTW_ACCESS_NONE && */
	    naccess <= RTW_ACCESS_ANAPARM);
	KASSERT(/* regs->r_access >= RTW_ACCESS_NONE && */
	    regs->r_access <= RTW_ACCESS_ANAPARM);

	if (naccess == regs->r_access)
		return;

	switch (naccess) {
	case RTW_ACCESS_NONE:
		switch (regs->r_access) {
		case RTW_ACCESS_ANAPARM:
			rtw_anaparm_enable(regs, 0);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			rtw_config0123_enable(regs, 0);
			/*FALLTHROUGH*/
		case RTW_ACCESS_NONE:
			break;
		}
		break;
	case RTW_ACCESS_CONFIG:
		switch (regs->r_access) {
		case RTW_ACCESS_NONE:
			rtw_config0123_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			break;
		case RTW_ACCESS_ANAPARM:
			rtw_anaparm_enable(regs, 0);
			break;
		}
		break;
	case RTW_ACCESS_ANAPARM:
		switch (regs->r_access) {
		case RTW_ACCESS_NONE:
			rtw_config0123_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			rtw_anaparm_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_ANAPARM:
			break;
		}
		break;
	}
}

void
rtw_set_access(struct rtw_regs *regs, enum rtw_access access)
{
	rtw_set_access1(regs, access);
	RTW_DPRINTF(RTW_DEBUG_ACCESS,
	    ("%s: access %s -> %s\n", __func__,
	    rtw_access_string(regs->r_access),
	    rtw_access_string(access)));
	regs->r_access = access;
}

/*
 * Enable registers, switch register banks.
 */
void
rtw_config0123_enable(struct rtw_regs *regs, int enable)
{
	uint8_t ecr;
	ecr = RTW_READ8(regs, RTW_9346CR);
	ecr &= ~(RTW_9346CR_EEM_MASK | RTW_9346CR_EECS | RTW_9346CR_EESK);
	if (enable)
		ecr |= RTW_9346CR_EEM_CONFIG;
	else {
		RTW_WBW(regs, RTW_9346CR, MAX(RTW_CONFIG0, RTW_CONFIG3));
		ecr |= RTW_9346CR_EEM_NORMAL;
	}
	RTW_WRITE8(regs, RTW_9346CR, ecr);
	RTW_SYNC(regs, RTW_9346CR, RTW_9346CR);
}

/* requires rtw_config0123_enable(, 1) */
void
rtw_anaparm_enable(struct rtw_regs *regs, int enable)
{
	uint8_t cfg3;

	cfg3 = RTW_READ8(regs, RTW_CONFIG3);
	cfg3 |= RTW_CONFIG3_CLKRUNEN;
	if (enable)
		cfg3 |= RTW_CONFIG3_PARMEN;
	else
		cfg3 &= ~RTW_CONFIG3_PARMEN;
	RTW_WRITE8(regs, RTW_CONFIG3, cfg3);
	RTW_SYNC(regs, RTW_CONFIG3, RTW_CONFIG3);
}

/* requires rtw_anaparm_enable(, 1) */
void
rtw_txdac_enable(struct rtw_softc *sc, int enable)
{
	uint32_t anaparm;
	struct rtw_regs *regs = &sc->sc_regs;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	if (enable)
		anaparm &= ~RTW_ANAPARM_TXDACOFF;
	else
		anaparm |= RTW_ANAPARM_TXDACOFF;
	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

static inline int
rtw_chip_reset1(struct rtw_regs *regs, device_t dev)
{
	uint8_t cr;
	int i;

	RTW_WRITE8(regs, RTW_CR, RTW_CR_RST);

	RTW_WBR(regs, RTW_CR, RTW_CR);

	for (i = 0; i < 1000; i++) {
		if ((cr = RTW_READ8(regs, RTW_CR) & RTW_CR_RST) == 0) {
			RTW_DPRINTF(RTW_DEBUG_RESET,
			    ("%s: reset in %dus\n", device_xname(dev), i));
			return 0;
		}
		RTW_RBR(regs, RTW_CR, RTW_CR);
		DELAY(10); /* 10us */
	}

	aprint_error_dev(dev, "reset failed\n");
	return ETIMEDOUT;
}

static inline int
rtw_chip_reset(struct rtw_regs *regs, device_t dev)
{
	uint32_t tcr;

	/* from Linux driver */
	tcr = RTW_TCR_CWMIN | RTW_TCR_MXDMA_2048 |
	      __SHIFTIN(7, RTW_TCR_SRL_MASK) | __SHIFTIN(7, RTW_TCR_LRL_MASK);

	RTW_WRITE(regs, RTW_TCR, tcr);

	RTW_WBW(regs, RTW_CR, RTW_TCR);

	return rtw_chip_reset1(regs, dev);
}

static int
rtw_wep_decap(struct ieee80211_key *k, struct mbuf *m, int hdrlen)
{
	struct ieee80211_key keycopy;

	RTW_DPRINTF(RTW_DEBUG_KEY, ("%s:\n", __func__));

	keycopy = *k;
	keycopy.wk_flags &= ~IEEE80211_KEY_SWCRYPT;

	return (*ieee80211_cipher_wep.ic_decap)(&keycopy, m, hdrlen);
}

static int
rtw_key_delete(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	struct rtw_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_KEY, ("%s: delete key %u\n", __func__,
	    k->wk_keyix));

	KASSERT(k->wk_keyix < IEEE80211_WEP_NKID);

	if (k->wk_keylen != 0 &&
	    k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP)
		sc->sc_flags &= ~RTW_F_DK_VALID;

	return 1;
}

static int
rtw_key_set(struct ieee80211com *ic, const struct ieee80211_key *k,
    const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	struct rtw_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_KEY, ("%s: set key %u\n", __func__, k->wk_keyix));

	KASSERT(k->wk_keyix < IEEE80211_WEP_NKID);

	sc->sc_flags &= ~RTW_F_DK_VALID;

	return 1;
}

static void
rtw_key_update_begin(struct ieee80211com *ic)
{
#ifdef RTW_DEBUG
	struct ifnet *ifp = ic->ic_ifp;
	struct rtw_softc *sc = ifp->if_softc;
#endif

	DPRINTF(sc, RTW_DEBUG_KEY, ("%s:\n", __func__));
}

static void
rtw_tx_kick(struct rtw_regs *regs, uint8_t ringsel)
{
	uint8_t tppoll;

	tppoll = RTW_READ8(regs, RTW_TPPOLL);
	tppoll &= ~RTW_TPPOLL_SALL;
	tppoll |= ringsel & RTW_TPPOLL_ALL;
	RTW_WRITE8(regs, RTW_TPPOLL, tppoll);
	RTW_SYNC(regs, RTW_TPPOLL, RTW_TPPOLL);
}

static void
rtw_key_update_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rtw_softc *sc = ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_KEY, ("%s:\n", __func__));

	if ((sc->sc_flags & RTW_F_DK_VALID) != 0 ||
	    !device_is_active(sc->sc_dev))
		return;

	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 0);
	rtw_wep_setkeys(sc, ic->ic_nw_keys, ic->ic_def_txkey);
	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE,
	    (ifp->if_flags & IFF_RUNNING) != 0);
}

static bool
rtw_key_hwsupp(uint32_t flags, const struct ieee80211_key *k)
{
	if (k->wk_cipher->ic_cipher != IEEE80211_CIPHER_WEP)
		return false;

	return	((flags & RTW_C_RXWEP_40) != 0 && k->wk_keylen == 5) ||
		((flags & RTW_C_RXWEP_104) != 0 && k->wk_keylen == 13);
}

static void
rtw_wep_setkeys(struct rtw_softc *sc, struct ieee80211_key *wk, int txkey)
{
	uint8_t psr, scr;
	int i, keylen = 0;
	struct rtw_regs *regs;
	union rtw_keys *rk;

	regs = &sc->sc_regs;
	rk = &sc->sc_keys;

	(void)memset(rk, 0, sizeof(*rk));

	/* Temporarily use software crypto for all keys. */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (wk[i].wk_cipher == &rtw_cipher_wep)
			wk[i].wk_cipher = &ieee80211_cipher_wep;
	}

	rtw_set_access(regs, RTW_ACCESS_CONFIG);

	psr = RTW_READ8(regs, RTW_PSR);
	scr = RTW_READ8(regs, RTW_SCR);
	scr &= ~(RTW_SCR_KM_MASK | RTW_SCR_TXSECON | RTW_SCR_RXSECON);

	if ((sc->sc_ic.ic_flags & IEEE80211_F_PRIVACY) == 0)
		goto out;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (!rtw_key_hwsupp(sc->sc_flags, &wk[i]))
			continue;
		if (i == txkey) {
			keylen = wk[i].wk_keylen;
			break;
		}
		keylen = MAX(keylen, wk[i].wk_keylen);
	}

	if (keylen == 5)
		scr |= RTW_SCR_KM_WEP40 | RTW_SCR_RXSECON;
	else if (keylen == 13)
		scr |= RTW_SCR_KM_WEP104 | RTW_SCR_RXSECON;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (wk[i].wk_keylen != keylen ||
		    wk[i].wk_cipher->ic_cipher != IEEE80211_CIPHER_WEP)
			continue;
		/* h/w will decrypt, s/w still strips headers */
		wk[i].wk_cipher = &rtw_cipher_wep;
		(void)memcpy(rk->rk_keys[i], wk[i].wk_key, wk[i].wk_keylen);
	}

out:
	RTW_WRITE8(regs, RTW_PSR, psr & ~RTW_PSR_PSEN);

	bus_space_write_region_stream_4(regs->r_bt, regs->r_bh,
	    RTW_DK0, rk->rk_words, __arraycount(rk->rk_words));

	bus_space_barrier(regs->r_bt, regs->r_bh, RTW_DK0, sizeof(rk->rk_words),
	    BUS_SPACE_BARRIER_SYNC);

	RTW_DPRINTF(RTW_DEBUG_KEY,
	    ("%s.%d: scr %02" PRIx8 ", keylen %d\n", __func__, __LINE__, scr,
	     keylen));

	RTW_WBW(regs, RTW_DK0, RTW_PSR);
	RTW_WRITE8(regs, RTW_PSR, psr);
	RTW_WBW(regs, RTW_PSR, RTW_SCR);
	RTW_WRITE8(regs, RTW_SCR, scr);
	RTW_SYNC(regs, RTW_SCR, RTW_SCR);
	rtw_set_access(regs, RTW_ACCESS_NONE);
	sc->sc_flags |= RTW_F_DK_VALID;
}

static inline int
rtw_recall_eeprom(struct rtw_regs *regs, device_t dev)
{
	int i;
	uint8_t ecr;

	ecr = RTW_READ8(regs, RTW_9346CR);
	ecr = (ecr & ~RTW_9346CR_EEM_MASK) | RTW_9346CR_EEM_AUTOLOAD;
	RTW_WRITE8(regs, RTW_9346CR, ecr);

	RTW_WBR(regs, RTW_9346CR, RTW_9346CR);

	/* wait 25ms for completion */
	for (i = 0; i < 250; i++) {
		ecr = RTW_READ8(regs, RTW_9346CR);
		if ((ecr & RTW_9346CR_EEM_MASK) == RTW_9346CR_EEM_NORMAL) {
			RTW_DPRINTF(RTW_DEBUG_RESET,
			    ("%s: recall EEPROM in %dus\n", device_xname(dev),
			    i * 100));
			return 0;
		}
		RTW_RBR(regs, RTW_9346CR, RTW_9346CR);
		DELAY(100);
	}
	aprint_error_dev(dev, "recall EEPROM failed\n");
	return ETIMEDOUT;
}

static inline int
rtw_reset(struct rtw_softc *sc)
{
	int rc;
	uint8_t config1;

	sc->sc_flags &= ~RTW_F_DK_VALID;

	if ((rc = rtw_chip_reset(&sc->sc_regs, sc->sc_dev)) != 0)
		return rc;

	rc = rtw_recall_eeprom(&sc->sc_regs, sc->sc_dev);

	config1 = RTW_READ8(&sc->sc_regs, RTW_CONFIG1);
	RTW_WRITE8(&sc->sc_regs, RTW_CONFIG1, config1 & ~RTW_CONFIG1_PMEN);
	/* TBD turn off maximum power saving? */

	return 0;
}

static inline int
rtw_txdesc_dmamaps_create(bus_dma_tag_t dmat, struct rtw_txsoft *descs,
    u_int ndescs)
{
	int i, rc = 0;
	for (i = 0; i < ndescs; i++) {
		rc = bus_dmamap_create(dmat, MCLBYTES, RTW_MAXPKTSEGS, MCLBYTES,
		    0, 0, &descs[i].ts_dmamap);
		if (rc != 0)
			break;
	}
	return rc;
}

static inline int
rtw_rxdesc_dmamaps_create(bus_dma_tag_t dmat, struct rtw_rxsoft *descs,
    u_int ndescs)
{
	int i, rc = 0;
	for (i = 0; i < ndescs; i++) {
		rc = bus_dmamap_create(dmat, MCLBYTES, 1, MCLBYTES, 0, 0,
		    &descs[i].rs_dmamap);
		if (rc != 0)
			break;
	}
	return rc;
}

static inline void
rtw_rxdesc_dmamaps_destroy(bus_dma_tag_t dmat, struct rtw_rxsoft *descs,
    u_int ndescs)
{
	int i;
	for (i = 0; i < ndescs; i++) {
		if (descs[i].rs_dmamap != NULL)
			bus_dmamap_destroy(dmat, descs[i].rs_dmamap);
	}
}

static inline void
rtw_txdesc_dmamaps_destroy(bus_dma_tag_t dmat, struct rtw_txsoft *descs,
    u_int ndescs)
{
	int i;
	for (i = 0; i < ndescs; i++) {
		if (descs[i].ts_dmamap != NULL)
			bus_dmamap_destroy(dmat, descs[i].ts_dmamap);
	}
}

static inline void
rtw_srom_free(struct rtw_srom *sr)
{
	sr->sr_size = 0;
	if (sr->sr_content == NULL)
		return;
	free(sr->sr_content, M_DEVBUF);
	sr->sr_content = NULL;
}

static void
rtw_srom_defaults(struct rtw_srom *sr, uint32_t *flags,
    uint8_t *cs_threshold, enum rtw_rfchipid *rfchipid, uint32_t *rcr)
{
	*flags |= (RTW_F_DIGPHY|RTW_F_ANTDIV);
	*cs_threshold = RTW_SR_ENERGYDETTHR_DEFAULT;
	*rcr |= RTW_RCR_ENCS1;
	*rfchipid = RTW_RFCHIPID_PHILIPS;
}

static int
rtw_srom_parse(struct rtw_srom *sr, uint32_t *flags, uint8_t *cs_threshold,
    enum rtw_rfchipid *rfchipid, uint32_t *rcr, enum rtw_locale *locale,
    device_t dev)
{
	int i;
	const char *rfname, *paname;
	char scratch[sizeof("unknown 0xXX")];
	uint16_t srom_version;

	*flags &= ~(RTW_F_DIGPHY|RTW_F_DFLANTB|RTW_F_ANTDIV);
	*rcr &= ~(RTW_RCR_ENCS1 | RTW_RCR_ENCS2);

	srom_version = RTW_SR_GET16(sr, RTW_SR_VERSION);

	if (srom_version <= 0x0101) {
		aprint_error_dev(dev,
		    "SROM version %d.%d is not understood, "
		    "limping along with defaults\n",
		    srom_version >> 8, srom_version & 0xff);
		rtw_srom_defaults(sr, flags, cs_threshold, rfchipid, rcr);
		return 0;
	} else {
		aprint_verbose_dev(dev, "SROM version %d.%d\n",
		    srom_version >> 8, srom_version & 0xff);
	}

	uint8_t mac[IEEE80211_ADDR_LEN];
	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		mac[i] = RTW_SR_GET(sr, RTW_SR_MAC + i);
	__USE(mac);

	RTW_DPRINTF(RTW_DEBUG_ATTACH,
	    ("%s: EEPROM MAC %s\n", device_xname(dev), ether_sprintf(mac)));

	*cs_threshold = RTW_SR_GET(sr, RTW_SR_ENERGYDETTHR);

	if ((RTW_SR_GET(sr, RTW_SR_CONFIG2) & RTW_CONFIG2_ANT) != 0)
		*flags |= RTW_F_ANTDIV;

	/* Note well: the sense of the RTW_SR_RFPARM_DIGPHY bit seems
	 * to be reversed.
	 */
	if ((RTW_SR_GET(sr, RTW_SR_RFPARM) & RTW_SR_RFPARM_DIGPHY) == 0)
		*flags |= RTW_F_DIGPHY;
	if ((RTW_SR_GET(sr, RTW_SR_RFPARM) & RTW_SR_RFPARM_DFLANTB) != 0)
		*flags |= RTW_F_DFLANTB;

	*rcr |= __SHIFTIN(__SHIFTOUT(RTW_SR_GET(sr, RTW_SR_RFPARM),
	    RTW_SR_RFPARM_CS_MASK), RTW_RCR_ENCS1);

	if ((RTW_SR_GET(sr, RTW_SR_CONFIG0) & RTW_CONFIG0_WEP104) != 0)
		*flags |= RTW_C_RXWEP_104;

	*flags |= RTW_C_RXWEP_40;	/* XXX */

	*rfchipid = RTW_SR_GET(sr, RTW_SR_RFCHIPID);
	switch (*rfchipid) {
	case RTW_RFCHIPID_GCT:		/* this combo seen in the wild */
		rfname = "GCT GRF5101";
		paname = "Winspring WS9901";
		break;
	case RTW_RFCHIPID_MAXIM:
		rfname = "Maxim MAX2820";	/* guess */
		paname = "Maxim MAX2422";	/* guess */
		break;
	case RTW_RFCHIPID_INTERSIL:
		rfname = "Intersil HFA3873";	/* guess */
		paname = "Intersil <unknown>";
		break;
	case RTW_RFCHIPID_PHILIPS:	/* this combo seen in the wild */
		rfname = "Philips SA2400A";
		paname = "Philips SA2411";
		break;
	case RTW_RFCHIPID_RFMD:
		/* this is the same front-end as an atw(4)! */
		rfname = "RFMD RF2948B, "	/* mentioned in Realtek docs */
			 "LNA: RFMD RF2494, "	/* mentioned in Realtek docs */
			 "SYN: Silicon Labs Si4126";	/* inferred from
			 				 * reference driver
							 */
		paname = "RFMD RF2189";		/* mentioned in Realtek docs */
		break;
	case RTW_RFCHIPID_RESERVED:
		rfname = paname = "reserved";
		break;
	default:
		snprintf(scratch, sizeof(scratch), "unknown 0x%02x", *rfchipid);
		rfname = paname = scratch;
	}
	aprint_normal_dev(dev, "RF: %s, PA: %s\n", rfname, paname);

	switch (RTW_SR_GET(sr, RTW_SR_CONFIG0) & RTW_CONFIG0_GL_MASK) {
	case RTW_CONFIG0_GL_USA:
	case _RTW_CONFIG0_GL_USA:
		*locale = RTW_LOCALE_USA;
		break;
	case RTW_CONFIG0_GL_EUROPE:
		*locale = RTW_LOCALE_EUROPE;
		break;
	case RTW_CONFIG0_GL_JAPAN:
		*locale = RTW_LOCALE_JAPAN;
		break;
	default:
		*locale = RTW_LOCALE_UNKNOWN;
		break;
	}
	return 0;
}

/* Returns -1 on failure. */
static int
rtw_srom_read(struct rtw_regs *regs, uint32_t flags, struct rtw_srom *sr,
    device_t dev)
{
	int rc;
	struct seeprom_descriptor sd;
	uint8_t ecr;

	(void)memset(&sd, 0, sizeof(sd));

	ecr = RTW_READ8(regs, RTW_9346CR);

	if ((flags & RTW_F_9356SROM) != 0) {
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: 93c56 SROM\n",
		    device_xname(dev)));
		sr->sr_size = 256;
		sd.sd_chip = C56_66;
	} else {
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: 93c46 SROM\n",
		    device_xname(dev)));
		sr->sr_size = 128;
		sd.sd_chip = C46;
	}

	ecr &= ~(RTW_9346CR_EEDI | RTW_9346CR_EEDO | RTW_9346CR_EESK |
	    RTW_9346CR_EEM_MASK | RTW_9346CR_EECS);
	ecr |= RTW_9346CR_EEM_PROGRAM;

	RTW_WRITE8(regs, RTW_9346CR, ecr);

	sr->sr_content = malloc(sr->sr_size, M_DEVBUF, M_NOWAIT);

	if (sr->sr_content == NULL) {
		aprint_error_dev(dev, "unable to allocate SROM buffer\n");
		return ENOMEM;
	}

	(void)memset(sr->sr_content, 0, sr->sr_size);

	/* RTL8180 has a single 8-bit register for controlling the
	 * 93cx6 SROM.  There is no "ready" bit. The RTL8180
	 * input/output sense is the reverse of read_seeprom's.
	 */
	sd.sd_tag = regs->r_bt;
	sd.sd_bsh = regs->r_bh;
	sd.sd_regsize = 1;
	sd.sd_control_offset = RTW_9346CR;
	sd.sd_status_offset = RTW_9346CR;
	sd.sd_dataout_offset = RTW_9346CR;
	sd.sd_CK = RTW_9346CR_EESK;
	sd.sd_CS = RTW_9346CR_EECS;
	sd.sd_DI = RTW_9346CR_EEDO;
	sd.sd_DO = RTW_9346CR_EEDI;
	/* make read_seeprom enter EEPROM read/write mode */
	sd.sd_MS = ecr;
	sd.sd_RDY = 0;

	/* TBD bus barriers */
	if (!read_seeprom(&sd, sr->sr_content, 0, sr->sr_size/2)) {
		aprint_error_dev(dev, "could not read SROM\n");
		free(sr->sr_content, M_DEVBUF);
		sr->sr_content = NULL;
		return -1;	/* XXX */
	}

	/* end EEPROM read/write mode */
	RTW_WRITE8(regs, RTW_9346CR,
	    (ecr & ~RTW_9346CR_EEM_MASK) | RTW_9346CR_EEM_NORMAL);
	RTW_WBRW(regs, RTW_9346CR, RTW_9346CR);

	if ((rc = rtw_recall_eeprom(regs, dev)) != 0)
		return rc;

#ifdef RTW_DEBUG
	{
		int i;
		RTW_DPRINTF(RTW_DEBUG_ATTACH,
		    ("\n%s: serial ROM:\n\t", device_xname(dev)));
		for (i = 0; i < sr->sr_size/2; i++) {
			if (((i % 8) == 0) && (i != 0))
				RTW_DPRINTF(RTW_DEBUG_ATTACH, ("\n\t"));
			RTW_DPRINTF(RTW_DEBUG_ATTACH,
			    (" %04x", sr->sr_content[i]));
		}
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("\n"));
	}
#endif /* RTW_DEBUG */
	return 0;
}

static void
rtw_set_rfprog(struct rtw_regs *regs, enum rtw_rfchipid rfchipid,
    device_t dev)
{
	uint8_t cfg4;
	const char *method;

	cfg4 = RTW_READ8(regs, RTW_CONFIG4) & ~RTW_CONFIG4_RFTYPE_MASK;

	switch (rfchipid) {
	default:
		cfg4 |= __SHIFTIN(rtw_rfprog_fallback, RTW_CONFIG4_RFTYPE_MASK);
		method = "fallback";
		break;
	case RTW_RFCHIPID_INTERSIL:
		cfg4 |= RTW_CONFIG4_RFTYPE_INTERSIL;
		method = "Intersil";
		break;
	case RTW_RFCHIPID_PHILIPS:
		cfg4 |= RTW_CONFIG4_RFTYPE_PHILIPS;
		method = "Philips";
		break;
	case RTW_RFCHIPID_GCT:	/* XXX a guess */
	case RTW_RFCHIPID_RFMD:
		cfg4 |= RTW_CONFIG4_RFTYPE_RFMD;
		method = "RFMD";
		break;
	}

	RTW_WRITE8(regs, RTW_CONFIG4, cfg4);

	RTW_WBR(regs, RTW_CONFIG4, RTW_CONFIG4);

#ifdef RTW_DEBUG
	RTW_DPRINTF(RTW_DEBUG_INIT,
	    ("%s: %s RF programming method, %#02x\n", device_xname(dev), method,
	    RTW_READ8(regs, RTW_CONFIG4)));
#else
	__USE(method);
#endif
}

static inline void
rtw_init_channels(enum rtw_locale locale,
    struct ieee80211_channel (*chans)[IEEE80211_CHAN_MAX+1], device_t dev)
{
	int i;
	const char *name = NULL;
#define ADD_CHANNEL(_chans, _chan) do {			\
	(*_chans)[_chan].ic_flags = IEEE80211_CHAN_B;		\
	(*_chans)[_chan].ic_freq =				\
	    ieee80211_ieee2mhz(_chan, (*_chans)[_chan].ic_flags);\
} while (0)

	switch (locale) {
	case RTW_LOCALE_USA:	/* 1-11 */
		name = "USA";
		for (i = 1; i <= 11; i++)
			ADD_CHANNEL(chans, i);
		break;
	case RTW_LOCALE_JAPAN:	/* 1-14 */
		name = "Japan";
		ADD_CHANNEL(chans, 14);
		for (i = 1; i <= 14; i++)
			ADD_CHANNEL(chans, i);
		break;
	case RTW_LOCALE_EUROPE:	/* 1-13 */
		name = "Europe";
		for (i = 1; i <= 13; i++)
			ADD_CHANNEL(chans, i);
		break;
	default:			/* 10-11 allowed by most countries */
		name = "<unknown>";
		for (i = 10; i <= 11; i++)
			ADD_CHANNEL(chans, i);
		break;
	}
	aprint_normal_dev(dev, "Geographic Location %s\n", name);
#undef ADD_CHANNEL
}


static inline void
rtw_identify_country(struct rtw_regs *regs, enum rtw_locale *locale)
{
	uint8_t cfg0 = RTW_READ8(regs, RTW_CONFIG0);

	switch (cfg0 & RTW_CONFIG0_GL_MASK) {
	case RTW_CONFIG0_GL_USA:
	case _RTW_CONFIG0_GL_USA:
		*locale = RTW_LOCALE_USA;
		break;
	case RTW_CONFIG0_GL_JAPAN:
		*locale = RTW_LOCALE_JAPAN;
		break;
	case RTW_CONFIG0_GL_EUROPE:
		*locale = RTW_LOCALE_EUROPE;
		break;
	default:
		*locale = RTW_LOCALE_UNKNOWN;
		break;
	}
}

static inline int
rtw_identify_sta(struct rtw_regs *regs, uint8_t (*addr)[IEEE80211_ADDR_LEN],
    device_t dev)
{
	static const uint8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	uint32_t idr0 = RTW_READ(regs, RTW_IDR0),
	          idr1 = RTW_READ(regs, RTW_IDR1);

	(*addr)[0] = __SHIFTOUT(idr0, __BITS(0,  7));
	(*addr)[1] = __SHIFTOUT(idr0, __BITS(8,  15));
	(*addr)[2] = __SHIFTOUT(idr0, __BITS(16, 23));
	(*addr)[3] = __SHIFTOUT(idr0, __BITS(24 ,31));

	(*addr)[4] = __SHIFTOUT(idr1, __BITS(0,  7));
	(*addr)[5] = __SHIFTOUT(idr1, __BITS(8, 15));

	if (IEEE80211_ADDR_EQ(addr, empty_macaddr)) {
		aprint_error_dev(dev,
		    "could not get mac address, attach failed\n");
		return ENXIO;
	}

	aprint_normal_dev(dev, "802.11 address %s\n", ether_sprintf(*addr));

	return 0;
}

static uint8_t
rtw_chan2txpower(struct rtw_srom *sr, struct ieee80211com *ic,
    struct ieee80211_channel *chan)
{
	u_int idx = RTW_SR_TXPOWER1 + ieee80211_chan2ieee(ic, chan) - 1;
	KASSERT(idx >= RTW_SR_TXPOWER1 && idx <= RTW_SR_TXPOWER14);
	return RTW_SR_GET(sr, idx);
}

static void
rtw_txdesc_blk_init_all(struct rtw_txdesc_blk *tdb)
{
	int pri;
	/* nfree: the number of free descriptors in each ring.
	 * The beacon ring is a special case: I do not let the
	 * driver use all of the descriptors on the beacon ring.
	 * The reasons are two-fold:
	 *
	 * (1) A BEACON descriptor's OWN bit is (apparently) not
	 * updated, so the driver cannot easily know if the descriptor
	 * belongs to it, or if it is racing the NIC.  If the NIC
	 * does not OWN every descriptor, then the driver can safely
	 * update the descriptors when RTW_TBDA points at tdb_next.
	 *
	 * (2) I hope that the NIC will process more than one BEACON
	 * descriptor in a single beacon interval, since that will
	 * enable multiple-BSS support.  Since the NIC does not
	 * clear the OWN bit, there is no natural place for it to
	 * stop processing BEACON desciptors.  Maybe it will *not*
	 * stop processing them!  I do not want to chance the NIC
	 * looping around and around a saturated beacon ring, so
	 * I will leave one descriptor unOWNed at all times.
	 */
	u_int nfree[RTW_NTXPRI] =
	    {RTW_NTXDESCLO, RTW_NTXDESCMD, RTW_NTXDESCHI,
	     RTW_NTXDESCBCN - 1};

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tdb[pri].tdb_nfree = nfree[pri];
		tdb[pri].tdb_next = 0;
	}
}

static int
rtw_txsoft_blk_init(struct rtw_txsoft_blk *tsb)
{
	int i;
	struct rtw_txsoft *ts;

	SIMPLEQ_INIT(&tsb->tsb_dirtyq);
	SIMPLEQ_INIT(&tsb->tsb_freeq);
	for (i = 0; i < tsb->tsb_ndesc; i++) {
		ts = &tsb->tsb_desc[i];
		ts->ts_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&tsb->tsb_freeq, ts, ts_q);
	}
	tsb->tsb_tx_timer = 0;
	return 0;
}

static void
rtw_txsoft_blk_init_all(struct rtw_txsoft_blk *tsb)
{
	int pri;
	for (pri = 0; pri < RTW_NTXPRI; pri++)
		rtw_txsoft_blk_init(&tsb[pri]);
}

static inline void
rtw_rxdescs_sync(struct rtw_rxdesc_blk *rdb, int desc0, int nsync, int ops)
{
	KASSERT(nsync <= rdb->rdb_ndesc);
	/* sync to end of ring */
	if (desc0 + nsync > rdb->rdb_ndesc) {
		bus_dmamap_sync(rdb->rdb_dmat, rdb->rdb_dmamap,
		    offsetof(struct rtw_descs, hd_rx[desc0]),
		    sizeof(struct rtw_rxdesc) * (rdb->rdb_ndesc - desc0), ops);
		nsync -= (rdb->rdb_ndesc - desc0);
		desc0 = 0;
	}

	KASSERT(desc0 < rdb->rdb_ndesc);
	KASSERT(nsync <= rdb->rdb_ndesc);
	KASSERT(desc0 + nsync <= rdb->rdb_ndesc);

	/* sync what remains */
	bus_dmamap_sync(rdb->rdb_dmat, rdb->rdb_dmamap,
	    offsetof(struct rtw_descs, hd_rx[desc0]),
	    sizeof(struct rtw_rxdesc) * nsync, ops);
}

static void
rtw_txdescs_sync(struct rtw_txdesc_blk *tdb, u_int desc0, u_int nsync, int ops)
{
	/* sync to end of ring */
	if (desc0 + nsync > tdb->tdb_ndesc) {
		bus_dmamap_sync(tdb->tdb_dmat, tdb->tdb_dmamap,
		    tdb->tdb_ofs + sizeof(struct rtw_txdesc) * desc0,
		    sizeof(struct rtw_txdesc) * (tdb->tdb_ndesc - desc0),
		    ops);
		nsync -= (tdb->tdb_ndesc - desc0);
		desc0 = 0;
	}

	/* sync what remains */
	bus_dmamap_sync(tdb->tdb_dmat, tdb->tdb_dmamap,
	    tdb->tdb_ofs + sizeof(struct rtw_txdesc) * desc0,
	    sizeof(struct rtw_txdesc) * nsync, ops);
}

static void
rtw_txdescs_sync_all(struct rtw_txdesc_blk *tdb)
{
	int pri;
	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rtw_txdescs_sync(&tdb[pri], 0, tdb[pri].tdb_ndesc,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
}

static void
rtw_rxbufs_release(bus_dma_tag_t dmat, struct rtw_rxsoft *desc)
{
	int i;
	struct rtw_rxsoft *rs;

	for (i = 0; i < RTW_RXQLEN; i++) {
		rs = &desc[i];
		if (rs->rs_mbuf == NULL)
			continue;
		bus_dmamap_sync(dmat, rs->rs_dmamap, 0,
		    rs->rs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(dmat, rs->rs_dmamap);
		m_freem(rs->rs_mbuf);
		rs->rs_mbuf = NULL;
	}
}

static inline int
rtw_rxsoft_alloc(bus_dma_tag_t dmat, struct rtw_rxsoft *rs)
{
	int rc;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	if (rs->rs_mbuf != NULL)
		bus_dmamap_unload(dmat, rs->rs_dmamap);

	rs->rs_mbuf = NULL;

	rc = bus_dmamap_load_mbuf(dmat, rs->rs_dmamap, m, BUS_DMA_NOWAIT);
	if (rc != 0) {
		m_freem(m);
		return -1;
	}

	rs->rs_mbuf = m;

	return 0;
}

static int
rtw_rxsoft_init_all(bus_dma_tag_t dmat, struct rtw_rxsoft *desc,
    int *ndesc, device_t dev)
{
	int i, rc = 0;
	struct rtw_rxsoft *rs;

	for (i = 0; i < RTW_RXQLEN; i++) {
		rs = &desc[i];
		/* we're in rtw_init, so there should be no mbufs allocated */
		KASSERT(rs->rs_mbuf == NULL);
#ifdef RTW_DEBUG
		if (i == rtw_rxbufs_limit) {
			aprint_error_dev(dev, "TEST hit %d-buffer limit\n", i);
			rc = ENOBUFS;
			break;
		}
#endif /* RTW_DEBUG */
		if ((rc = rtw_rxsoft_alloc(dmat, rs)) != 0) {
			aprint_error_dev(dev,
			    "rtw_rxsoft_alloc failed, %d buffers, rc %d\n",
			    i, rc);
			break;
		}
	}
	*ndesc = i;
	return rc;
}

static inline void
rtw_rxdesc_init(struct rtw_rxdesc_blk *rdb, struct rtw_rxsoft *rs,
    int idx, int kick)
{
	int is_last = (idx == rdb->rdb_ndesc - 1);
	uint32_t ctl, octl, obuf;
	struct rtw_rxdesc *rd = &rdb->rdb_desc[idx];

	/* sync the mbuf before the descriptor */
	bus_dmamap_sync(rdb->rdb_dmat, rs->rs_dmamap, 0,
	    rs->rs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	obuf = rd->rd_buf;
	rd->rd_buf = htole32(rs->rs_dmamap->dm_segs[0].ds_addr);

	ctl = __SHIFTIN(rs->rs_mbuf->m_len, RTW_RXCTL_LENGTH_MASK) |
	    RTW_RXCTL_OWN | RTW_RXCTL_FS | RTW_RXCTL_LS;

	if (is_last)
		ctl |= RTW_RXCTL_EOR;

	octl = rd->rd_ctl;
	rd->rd_ctl = htole32(ctl);

#ifdef RTW_DEBUG
	RTW_DPRINTF(
	    kick ? (RTW_DEBUG_RECV_DESC | RTW_DEBUG_IO_KICK)
	         : RTW_DEBUG_RECV_DESC,
	    ("%s: rd %p buf %08x -> %08x ctl %08x -> %08x\n", __func__, rd,
	     le32toh(obuf), le32toh(rd->rd_buf), le32toh(octl),
	     le32toh(rd->rd_ctl)));
#else
	__USE(octl);
	__USE(obuf);
#endif

	/* sync the descriptor */
	bus_dmamap_sync(rdb->rdb_dmat, rdb->rdb_dmamap,
	    RTW_DESC_OFFSET(hd_rx, idx), sizeof(struct rtw_rxdesc),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
}

static void
rtw_rxdesc_init_all(struct rtw_rxdesc_blk *rdb, struct rtw_rxsoft *ctl, int kick)
{
	int i;
	struct rtw_rxsoft *rs;

	for (i = 0; i < rdb->rdb_ndesc; i++) {
		rs = &ctl[i];
		rtw_rxdesc_init(rdb, rs, i, kick);
	}
}

static void
rtw_io_enable(struct rtw_softc *sc, uint8_t flags, int enable)
{
	struct rtw_regs *regs = &sc->sc_regs;
	uint8_t cr;

	RTW_DPRINTF(RTW_DEBUG_IOSTATE, ("%s: %s 0x%02x\n", __func__,
	    enable ? "enable" : "disable", flags));

	cr = RTW_READ8(regs, RTW_CR);

	/* XXX reference source does not enable MULRW */
	/* enable PCI Read/Write Multiple */
	cr |= RTW_CR_MULRW;

	/* The receive engine will always start at RDSAR.  */
	if (enable && (flags & ~cr & RTW_CR_RE)) {
		struct rtw_rxdesc_blk *rdb;
		rdb = &sc->sc_rxdesc_blk;
		rdb->rdb_next = 0;
	}

	RTW_RBW(regs, RTW_CR, RTW_CR);	/* XXX paranoia? */
	if (enable)
		cr |= flags;
	else
		cr &= ~flags;
	RTW_WRITE8(regs, RTW_CR, cr);
	RTW_SYNC(regs, RTW_CR, RTW_CR);

#ifdef RTW_DIAG
	if (cr & RTW_CR_TE)
		rtw_txring_fixup(sc, __func__, __LINE__);
#endif
	if (cr & RTW_CR_TE) {
		rtw_tx_kick(&sc->sc_regs,
		    RTW_TPPOLL_HPQ | RTW_TPPOLL_NPQ | RTW_TPPOLL_LPQ);
	}
}

static void
rtw_intr_rx(struct rtw_softc *sc, uint16_t isr)
{
#define	IS_BEACON(__fc0)						\
    ((__fc0 & (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==\
     (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_BEACON))

	static const int ratetbl[4] = {2, 4, 11, 22};	/* convert rates:
							 * hardware -> net80211
							 */
	u_int next, nproc = 0;
	int hwrate, len, rate, rssi, sq;
	uint32_t hrssi, hstat, htsfth, htsftl;
	struct rtw_rxdesc *rd;
	struct rtw_rxsoft *rs;
	struct rtw_rxdesc_blk *rdb;
	struct mbuf *m;
	struct ifnet *ifp = &sc->sc_if;

	struct ieee80211_node *ni;
	struct ieee80211_frame_min *wh;

	rdb = &sc->sc_rxdesc_blk;

	for (next = rdb->rdb_next; ; next = rdb->rdb_next) {
		KASSERT(next < rdb->rdb_ndesc);

		rtw_rxdescs_sync(rdb, next, 1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		rd = &rdb->rdb_desc[next];
		rs = &sc->sc_rxsoft[next];

		hstat = le32toh(rd->rd_stat);
		hrssi = le32toh(rd->rd_rssi);
		htsfth = le32toh(rd->rd_tsfth);
		htsftl = le32toh(rd->rd_tsftl);

		RTW_DPRINTF(RTW_DEBUG_RECV_DESC,
		    ("%s: rxdesc[%d] hstat %08x hrssi %08x htsft %08x%08x\n",
		    __func__, next, hstat, hrssi, htsfth, htsftl));

		++nproc;

		/* still belongs to NIC */
		if ((hstat & RTW_RXSTAT_OWN) != 0) {
			rtw_rxdescs_sync(rdb, next, 1, BUS_DMASYNC_PREREAD);
			break;
		}

                /* ieee80211_input() might reset the receive engine
                 * (e.g. by indirectly calling rtw_tune()), so save
                 * the next pointer here and retrieve it again on
                 * the next round.
		 */
		rdb->rdb_next = (next + 1) % rdb->rdb_ndesc;

#ifdef RTW_DEBUG
#define PRINTSTAT(flag) do { \
	if ((hstat & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)
		if ((rtw_debug & RTW_DEBUG_RECV_DESC) != 0) {
			const char *delim = "<";
			printf("%s: ", device_xname(sc->sc_dev));
			if ((hstat & RTW_RXSTAT_DEBUG) != 0) {
				printf("status %08x", hstat);
				PRINTSTAT(RTW_RXSTAT_SPLCP);
				PRINTSTAT(RTW_RXSTAT_MAR);
				PRINTSTAT(RTW_RXSTAT_PAR);
				PRINTSTAT(RTW_RXSTAT_BAR);
				PRINTSTAT(RTW_RXSTAT_PWRMGT);
				PRINTSTAT(RTW_RXSTAT_CRC32);
				PRINTSTAT(RTW_RXSTAT_ICV);
				printf(">, ");
			}
		}
#endif /* RTW_DEBUG */

		if ((hstat & RTW_RXSTAT_IOERROR) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "DMA error/FIFO overflow %08" PRIx32 ", "
			    "rx descriptor %d\n", hstat, next);
			ifp->if_ierrors++;
			goto next;
		}

		len = __SHIFTOUT(hstat, RTW_RXSTAT_LENGTH_MASK);
		if (len < IEEE80211_MIN_LEN) {
			sc->sc_ic.ic_stats.is_rx_tooshort++;
			goto next;
		}
		if (len > rs->rs_mbuf->m_len) {
			aprint_error_dev(sc->sc_dev,
			    "rx frame too long, %d > %d, %08" PRIx32
			    ", desc %d\n",
			    len, rs->rs_mbuf->m_len, hstat, next);
			ifp->if_ierrors++;
			goto next;
		}

		hwrate = __SHIFTOUT(hstat, RTW_RXSTAT_RATE_MASK);
		if (hwrate >= __arraycount(ratetbl)) {
			aprint_error_dev(sc->sc_dev,
			    "unknown rate #%" __PRIuBITS "\n",
			    __SHIFTOUT(hstat, RTW_RXSTAT_RATE_MASK));
			ifp->if_ierrors++;
			goto next;
		}
		rate = ratetbl[hwrate];

#ifdef RTW_DEBUG
		RTW_DPRINTF(RTW_DEBUG_RECV_DESC,
		    ("rate %d.%d Mb/s, time %08x%08x\n", (rate * 5) / 10,
		     (rate * 5) % 10, htsfth, htsftl));
#endif /* RTW_DEBUG */

		/* if bad flags, skip descriptor */
		if ((hstat & RTW_RXSTAT_ONESEG) != RTW_RXSTAT_ONESEG) {
			aprint_error_dev(sc->sc_dev, "too many rx segments, "
			    "next=%d, %08" PRIx32 "\n", next, hstat);
			goto next;
		}

		bus_dmamap_sync(sc->sc_dmat, rs->rs_dmamap, 0,
		    rs->rs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = rs->rs_mbuf;

		/* if temporarily out of memory, re-use mbuf */
		switch (rtw_rxsoft_alloc(sc->sc_dmat, rs)) {
		case 0:
			break;
		case ENOBUFS:
			aprint_error_dev(sc->sc_dev,
			    "rtw_rxsoft_alloc(, %d) failed, dropping packet\n",
			    next);
			goto next;
		default:
			/* XXX shorten rx ring, instead? */
			aprint_error_dev(sc->sc_dev,
			    "could not load DMA map\n");
		}

		sq = __SHIFTOUT(hrssi, RTW_RXRSSI_SQ);

		if (sc->sc_rfchipid == RTW_RFCHIPID_PHILIPS)
			rssi = UINT8_MAX - sq;
		else {
			rssi = __SHIFTOUT(hrssi, RTW_RXRSSI_IMR_RSSI);
			/* TBD find out each front-end's LNA gain in the
			 * front-end's units
			 */
			if ((hrssi & RTW_RXRSSI_IMR_LNA) == 0)
				rssi |= 0x80;
		}

		/* Note well: now we cannot recycle the rs_mbuf unless
		 * we restore its original length.
		 */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		wh = mtod(m, struct ieee80211_frame_min *);

		if (!IS_BEACON(wh->i_fc[0]))
			sc->sc_led_state.ls_event |= RTW_LED_S_RX;

		sc->sc_tsfth = htsfth;

#ifdef RTW_DEBUG
		if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			ieee80211_dump_pkt(mtod(m, uint8_t *), m->m_pkthdr.len,
			    rate, rssi);
		}
#endif /* RTW_DEBUG */

		if (sc->sc_radiobpf != NULL) {
			struct rtw_rx_radiotap_header *rr = &sc->sc_rxtap;

			rr->rr_tsft =
			    htole64(((uint64_t)htsfth << 32) | htsftl);

			rr->rr_flags = IEEE80211_RADIOTAP_F_FCS;

			if ((hstat & RTW_RXSTAT_SPLCP) != 0)
				rr->rr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			if ((hstat & RTW_RXSTAT_CRC32) != 0)
				rr->rr_flags |= IEEE80211_RADIOTAP_F_BADFCS;

			rr->rr_rate = rate;

			if (sc->sc_rfchipid == RTW_RFCHIPID_PHILIPS)
				rr->rr_u.u_philips.p_antsignal = rssi;
			else {
				rr->rr_u.u_other.o_antsignal = rssi;
				rr->rr_u.u_other.o_barker_lock =
				    htole16(UINT8_MAX - sq);
			}

			bpf_mtap2(sc->sc_radiobpf,
			    rr, sizeof(sc->sc_rxtapu), m);
		}

		if ((hstat & RTW_RXSTAT_RES) != 0) {
			m_freem(m);
			goto next;
		}

		/* CRC is included with the packet; trim it off. */
		m_adj(m, -IEEE80211_CRC_LEN);

		/* TBD use _MAR, _BAR, _PAR flags as hints to _find_rxnode? */
		ni = ieee80211_find_rxnode(&sc->sc_ic, wh);
		ieee80211_input(&sc->sc_ic, m, ni, rssi, htsftl);
		ieee80211_free_node(ni);
next:
		rtw_rxdesc_init(rdb, rs, next, 0);
	}
#undef IS_BEACON
}

static void
rtw_txsoft_release(bus_dma_tag_t dmat, struct ieee80211com *ic,
    struct rtw_txsoft *ts)
{
	struct mbuf *m;
	struct ieee80211_node *ni;

	m = ts->ts_mbuf;
	ni = ts->ts_ni;
	KASSERT(m != NULL);
	KASSERT(ni != NULL);
	ts->ts_mbuf = NULL;
	ts->ts_ni = NULL;

	bus_dmamap_sync(dmat, ts->ts_dmamap, 0, ts->ts_dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dmat, ts->ts_dmamap);
	m_freem(m);
	ieee80211_free_node(ni);
}

static void
rtw_txsofts_release(bus_dma_tag_t dmat, struct ieee80211com *ic,
    struct rtw_txsoft_blk *tsb)
{
	struct rtw_txsoft *ts;

	while ((ts = SIMPLEQ_FIRST(&tsb->tsb_dirtyq)) != NULL) {
		rtw_txsoft_release(dmat, ic, ts);
		SIMPLEQ_REMOVE_HEAD(&tsb->tsb_dirtyq, ts_q);
		SIMPLEQ_INSERT_TAIL(&tsb->tsb_freeq, ts, ts_q);
	}
	tsb->tsb_tx_timer = 0;
}

static inline void
rtw_collect_txpkt(struct rtw_softc *sc, struct rtw_txdesc_blk *tdb,
    struct rtw_txsoft *ts, int ndesc)
{
	uint32_t hstat;
	int data_retry, rts_retry;
	struct rtw_txdesc *tdn;
	const char *condstring;
	struct ifnet *ifp = &sc->sc_if;

	rtw_txsoft_release(sc->sc_dmat, &sc->sc_ic, ts);

	tdb->tdb_nfree += ndesc;

	tdn = &tdb->tdb_desc[ts->ts_last];

	hstat = le32toh(tdn->td_stat);
	rts_retry = __SHIFTOUT(hstat, RTW_TXSTAT_RTSRETRY_MASK);
	data_retry = __SHIFTOUT(hstat, RTW_TXSTAT_DRC_MASK);

	ifp->if_collisions += rts_retry + data_retry;

	if ((hstat & RTW_TXSTAT_TOK) != 0)
		condstring = "ok";
	else {
		ifp->if_oerrors++;
		condstring = "error";
	}

#ifdef RTW_DEBUG
	DPRINTF(sc, RTW_DEBUG_XMIT_DESC,
	    ("%s: ts %p txdesc[%d, %d] %s tries rts %u data %u\n",
	    device_xname(sc->sc_dev), ts, ts->ts_first, ts->ts_last,
	    condstring, rts_retry, data_retry));
#else
	__USE(condstring);
#endif
}

static void
rtw_reset_oactive(struct rtw_softc *sc)
{
	short oflags;
	int pri;
	struct rtw_txsoft_blk *tsb;
	struct rtw_txdesc_blk *tdb;
	oflags = sc->sc_if.if_flags;
	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];
		tdb = &sc->sc_txdesc_blk[pri];
		if (!SIMPLEQ_EMPTY(&tsb->tsb_freeq) && tdb->tdb_nfree > 0)
			sc->sc_if.if_flags &= ~IFF_OACTIVE;
	}
	if (oflags != sc->sc_if.if_flags) {
		DPRINTF(sc, RTW_DEBUG_OACTIVE,
		    ("%s: reset OACTIVE\n", __func__));
	}
}

/* Collect transmitted packets. */
static bool
rtw_collect_txring(struct rtw_softc *sc, struct rtw_txsoft_blk *tsb,
    struct rtw_txdesc_blk *tdb, int force)
{
	bool collected = false;
	int ndesc;
	struct rtw_txsoft *ts;

#ifdef RTW_DEBUG
	rtw_dump_rings(sc);
#endif

	while ((ts = SIMPLEQ_FIRST(&tsb->tsb_dirtyq)) != NULL) {
		/* If we're clearing a failed transmission, only clear
		   up to the last packet the hardware has processed.  */
		if (ts->ts_first == rtw_txring_next(&sc->sc_regs, tdb))
			break;

		ndesc = 1 + ts->ts_last - ts->ts_first;
		if (ts->ts_last < ts->ts_first)
			ndesc += tdb->tdb_ndesc;

		KASSERT(ndesc > 0);

		rtw_txdescs_sync(tdb, ts->ts_first, ndesc,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		if (force) {
			int next;
#ifdef RTW_DIAG
			printf("%s: clearing packet, stats", __func__);
#endif
			for (next = ts->ts_first; ;
			    next = RTW_NEXT_IDX(tdb, next)) {
#ifdef RTW_DIAG
				printf(" %" PRIx32 "/%" PRIx32 "/%" PRIx32 "/%" PRIu32 "/%" PRIx32, le32toh(tdb->tdb_desc[next].td_stat), le32toh(tdb->tdb_desc[next].td_ctl1), le32toh(tdb->tdb_desc[next].td_buf), le32toh(tdb->tdb_desc[next].td_len), le32toh(tdb->tdb_desc[next].td_next));
#endif
				tdb->tdb_desc[next].td_stat &=
				    ~htole32(RTW_TXSTAT_OWN);
				if (next == ts->ts_last)
					break;
			}
			rtw_txdescs_sync(tdb, ts->ts_first, ndesc,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
#ifdef RTW_DIAG
			next = RTW_NEXT_IDX(tdb, next);
			printf(" -> end %u stat %" PRIx32 ", was %u\n", next,
			    le32toh(tdb->tdb_desc[next].td_stat),
			    rtw_txring_next(&sc->sc_regs, tdb));
#endif
		} else if ((tdb->tdb_desc[ts->ts_last].td_stat &
		    htole32(RTW_TXSTAT_OWN)) != 0) {
			rtw_txdescs_sync(tdb, ts->ts_last, 1,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		collected = true;

		rtw_collect_txpkt(sc, tdb, ts, ndesc);
		SIMPLEQ_REMOVE_HEAD(&tsb->tsb_dirtyq, ts_q);
		SIMPLEQ_INSERT_TAIL(&tsb->tsb_freeq, ts, ts_q);
	}

	/* no more pending transmissions, cancel watchdog */
	if (ts == NULL)
		tsb->tsb_tx_timer = 0;
	rtw_reset_oactive(sc);

	return collected;
}

static void
rtw_intr_tx(struct rtw_softc *sc, uint16_t isr)
{
	int pri;
	struct rtw_txsoft_blk	*tsb;
	struct rtw_txdesc_blk	*tdb;
	struct ifnet *ifp = &sc->sc_if;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];
		tdb = &sc->sc_txdesc_blk[pri];
		rtw_collect_txring(sc, tsb, tdb, 0);
	}

	if ((isr & RTW_INTR_TX) != 0)
		rtw_start(ifp);

	return;
}

static void
rtw_intr_beacon(struct rtw_softc *sc, uint16_t isr)
{
	u_int next;
	uint32_t tsfth, tsftl;
	struct ieee80211com *ic;
	struct rtw_txdesc_blk *tdb = &sc->sc_txdesc_blk[RTW_TXPRIBCN];
	struct rtw_txsoft_blk *tsb = &sc->sc_txsoft_blk[RTW_TXPRIBCN];
	struct mbuf *m;

	tsfth = RTW_READ(&sc->sc_regs, RTW_TSFTRH);
	tsftl = RTW_READ(&sc->sc_regs, RTW_TSFTRL);

	if ((isr & (RTW_INTR_TBDOK|RTW_INTR_TBDER)) != 0) {
		next = rtw_txring_next(&sc->sc_regs, tdb);
#ifdef RTW_DEBUG
		RTW_DPRINTF(RTW_DEBUG_BEACON,
		    ("%s: beacon ring %sprocessed, isr = %#04" PRIx16
		     ", next %u expected %u, %" PRIu64 "\n", __func__,
		     (next == tdb->tdb_next) ? "" : "un", isr, next,
		     tdb->tdb_next, (uint64_t)tsfth << 32 | tsftl));
#else
		__USE(next);
		__USE(tsfth);
		__USE(tsftl);
#endif
		if ((RTW_READ8(&sc->sc_regs, RTW_TPPOLL) & RTW_TPPOLL_BQ) == 0)
			rtw_collect_txring(sc, tsb, tdb, 1);
	}
	/* Start beacon transmission. */

	if ((isr & RTW_INTR_BCNINT) != 0 &&
	    sc->sc_ic.ic_state == IEEE80211_S_RUN &&
	    SIMPLEQ_EMPTY(&tsb->tsb_dirtyq)) {
		RTW_DPRINTF(RTW_DEBUG_BEACON,
		    ("%s: beacon prep. time, isr = %#04" PRIx16
		     ", %16" PRIu64 "\n", __func__, isr,
		     (uint64_t)tsfth << 32 | tsftl));
		ic = &sc->sc_ic;
		m = rtw_beacon_alloc(sc, ic->ic_bss);

		if (m == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate beacon\n");
			return;
		}
		m->m_pkthdr.rcvif = (void *)ieee80211_ref_node(ic->ic_bss);
		IF_ENQUEUE(&sc->sc_beaconq, m);
		rtw_start(&sc->sc_if);
	}
}

static void
rtw_intr_atim(struct rtw_softc *sc)
{
	/* TBD */
	return;
}

#ifdef RTW_DEBUG
static void
rtw_dump_rings(struct rtw_softc *sc)
{
	struct rtw_txdesc_blk *tdb;
	struct rtw_rxdesc *rd;
	struct rtw_rxdesc_blk *rdb;
	int desc, pri;

	if ((rtw_debug & RTW_DEBUG_IO_KICK) == 0)
		return;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tdb = &sc->sc_txdesc_blk[pri];
		printf("%s: txpri %d ndesc %d nfree %d\n", __func__, pri,
		    tdb->tdb_ndesc, tdb->tdb_nfree);
		for (desc = 0; desc < tdb->tdb_ndesc; desc++)
			rtw_print_txdesc(sc, ".", NULL, tdb, desc);
	}

	rdb = &sc->sc_rxdesc_blk;

	for (desc = 0; desc < RTW_RXQLEN; desc++) {
		rd = &rdb->rdb_desc[desc];
		printf("%s: %sctl %08x rsvd0/rssi %08x buf/tsftl %08x "
		    "rsvd1/tsfth %08x\n", __func__,
		    (desc >= rdb->rdb_ndesc) ? "UNUSED " : "",
		    le32toh(rd->rd_ctl), le32toh(rd->rd_rssi),
		    le32toh(rd->rd_buf), le32toh(rd->rd_tsfth));
	}
}
#endif /* RTW_DEBUG */

static void
rtw_hwring_setup(struct rtw_softc *sc)
{
	int pri;
	struct rtw_regs *regs = &sc->sc_regs;
	struct rtw_txdesc_blk *tdb;

	sc->sc_txdesc_blk[RTW_TXPRILO].tdb_basereg = RTW_TLPDA;
	sc->sc_txdesc_blk[RTW_TXPRILO].tdb_base = RTW_RING_BASE(sc, hd_txlo);
	sc->sc_txdesc_blk[RTW_TXPRIMD].tdb_basereg = RTW_TNPDA;
	sc->sc_txdesc_blk[RTW_TXPRIMD].tdb_base = RTW_RING_BASE(sc, hd_txmd);
	sc->sc_txdesc_blk[RTW_TXPRIHI].tdb_basereg = RTW_THPDA;
	sc->sc_txdesc_blk[RTW_TXPRIHI].tdb_base = RTW_RING_BASE(sc, hd_txhi);
	sc->sc_txdesc_blk[RTW_TXPRIBCN].tdb_basereg = RTW_TBDA;
	sc->sc_txdesc_blk[RTW_TXPRIBCN].tdb_base = RTW_RING_BASE(sc, hd_bcn);

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tdb = &sc->sc_txdesc_blk[pri];
		RTW_WRITE(regs, tdb->tdb_basereg, tdb->tdb_base);
		RTW_DPRINTF(RTW_DEBUG_XMIT_DESC,
		    ("%s: reg[tdb->tdb_basereg] <- %" PRIxPTR "\n", __func__,
		     (uintptr_t)tdb->tdb_base));
	}

	RTW_WRITE(regs, RTW_RDSAR, RTW_RING_BASE(sc, hd_rx));

	RTW_DPRINTF(RTW_DEBUG_RECV_DESC,
	    ("%s: reg[RDSAR] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_rx)));

	RTW_SYNC(regs, RTW_TLPDA, RTW_RDSAR);

}

static int
rtw_swring_setup(struct rtw_softc *sc)
{
	int rc;
	struct rtw_rxdesc_blk *rdb;

	rtw_txdesc_blk_init_all(&sc->sc_txdesc_blk[0]);

	rtw_txsoft_blk_init_all(&sc->sc_txsoft_blk[0]);

	rdb = &sc->sc_rxdesc_blk;
	if ((rc = rtw_rxsoft_init_all(sc->sc_dmat, sc->sc_rxsoft, &rdb->rdb_ndesc,
	     sc->sc_dev)) != 0 && rdb->rdb_ndesc == 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate rx buffers\n");
		return rc;
	}

	rdb = &sc->sc_rxdesc_blk;
	rtw_rxdescs_sync(rdb, 0, rdb->rdb_ndesc,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	rtw_rxdesc_init_all(rdb, sc->sc_rxsoft, 1);
	rdb->rdb_next = 0;

	rtw_txdescs_sync_all(&sc->sc_txdesc_blk[0]);
	return 0;
}

static void
rtw_txdesc_blk_init(struct rtw_txdesc_blk *tdb)
{
	int i;

	(void)memset(tdb->tdb_desc, 0,
	    sizeof(tdb->tdb_desc[0]) * tdb->tdb_ndesc);
	for (i = 0; i < tdb->tdb_ndesc; i++)
		tdb->tdb_desc[i].td_next = htole32(RTW_NEXT_DESC(tdb, i));
}

static u_int
rtw_txring_next(struct rtw_regs *regs, struct rtw_txdesc_blk *tdb)
{
	return (le32toh(RTW_READ(regs, tdb->tdb_basereg)) - tdb->tdb_base) /
	    sizeof(struct rtw_txdesc);
}

#ifdef RTW_DIAG
static void
rtw_txring_fixup(struct rtw_softc *sc, const char *fn, int ln)
{
	int pri;
	u_int next;
	struct rtw_txdesc_blk *tdb;
	struct rtw_regs *regs = &sc->sc_regs;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		int i;
		tdb = &sc->sc_txdesc_blk[pri];
		next = rtw_txring_next(regs, tdb);
		if (tdb->tdb_next == next)
			continue;
		for (i = 0; next != tdb->tdb_next;
		    next = RTW_NEXT_IDX(tdb, next), i++) {
			if ((tdb->tdb_desc[next].td_stat & htole32(RTW_TXSTAT_OWN)) == 0)
				break;
		}
		printf("%s:%d: tx-ring %d expected next %u, read %u+%d -> %s\n", fn,
		    ln, pri, tdb->tdb_next, next, i, tdb->tdb_next == next ? "okay" : "BAD");
		if (tdb->tdb_next == next)
			continue;
		tdb->tdb_next = MIN(next, tdb->tdb_ndesc - 1);
	}
}
#endif

static void
rtw_txdescs_reset(struct rtw_softc *sc)
{
	int pri;
	struct rtw_txsoft_blk	*tsb;
	struct rtw_txdesc_blk	*tdb;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];
		tdb = &sc->sc_txdesc_blk[pri];
		rtw_collect_txring(sc, tsb, tdb, 1);
#ifdef RTW_DIAG
		if (!SIMPLEQ_EMPTY(&tsb->tsb_dirtyq))
			printf("%s: packets left in ring %d\n", __func__, pri);
#endif
	}
}

static void
rtw_intr_ioerror(struct rtw_softc *sc, uint16_t isr)
{
	aprint_error_dev(sc->sc_dev, "tx fifo underflow\n");

	RTW_DPRINTF(RTW_DEBUG_BUGS, ("%s: cleaning up xmit, isr %" PRIx16
	    "\n", device_xname(sc->sc_dev), isr));

#ifdef RTW_DEBUG
	rtw_dump_rings(sc);
#endif /* RTW_DEBUG */

	/* Collect tx'd packets.  XXX let's hope this stops the transmit
	 * timeouts.
	 */
	rtw_txdescs_reset(sc);

#ifdef RTW_DEBUG
	rtw_dump_rings(sc);
#endif /* RTW_DEBUG */
}

static inline void
rtw_suspend_ticks(struct rtw_softc *sc)
{
	RTW_DPRINTF(RTW_DEBUG_TIMEOUT,
	    ("%s: suspending ticks\n", device_xname(sc->sc_dev)));
	sc->sc_do_tick = 0;
}

static inline void
rtw_resume_ticks(struct rtw_softc *sc)
{
	uint32_t tsftrl0, tsftrl1, next_tint;

	tsftrl0 = RTW_READ(&sc->sc_regs, RTW_TSFTRL);

	tsftrl1 = RTW_READ(&sc->sc_regs, RTW_TSFTRL);
	next_tint = tsftrl1 + 1000000;
	RTW_WRITE(&sc->sc_regs, RTW_TINT, next_tint);

	sc->sc_do_tick = 1;

#ifdef RTW_DEBUG
	RTW_DPRINTF(RTW_DEBUG_TIMEOUT,
	    ("%s: resume ticks delta %#08x now %#08x next %#08x\n",
	    device_xname(sc->sc_dev), tsftrl1 - tsftrl0, tsftrl1, next_tint));
#else
	__USE(tsftrl0);
#endif
}

static void
rtw_intr_timeout(struct rtw_softc *sc)
{
	RTW_DPRINTF(RTW_DEBUG_TIMEOUT, ("%s: timeout\n", device_xname(sc->sc_dev)));
	if (sc->sc_do_tick)
		rtw_resume_ticks(sc);
	return;
}

int
rtw_intr(void *arg)
{
	int i;
	struct rtw_softc *sc = arg;
	struct rtw_regs *regs = &sc->sc_regs;
	uint16_t isr;
	struct ifnet *ifp = &sc->sc_if;

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER)) {
		RTW_DPRINTF(RTW_DEBUG_INTR, ("%s: stray interrupt\n",
		    device_xname(sc->sc_dev)));
		return (0);
	}

	for (i = 0; i < 10; i++) {
		isr = RTW_READ16(regs, RTW_ISR);

		RTW_WRITE16(regs, RTW_ISR, isr);
		RTW_WBR(regs, RTW_ISR, RTW_ISR);

		if (sc->sc_intr_ack != NULL)
			(*sc->sc_intr_ack)(regs);

		if (isr == 0)
			break;

#ifdef RTW_DEBUG
#define PRINTINTR(flag) do { \
	if ((isr & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)

		if ((rtw_debug & RTW_DEBUG_INTR) != 0 && isr != 0) {
			const char *delim = "<";

			printf("%s: reg[ISR] = %x", device_xname(sc->sc_dev),
			    isr);

			PRINTINTR(RTW_INTR_TXFOVW);
			PRINTINTR(RTW_INTR_TIMEOUT);
			PRINTINTR(RTW_INTR_BCNINT);
			PRINTINTR(RTW_INTR_ATIMINT);
			PRINTINTR(RTW_INTR_TBDER);
			PRINTINTR(RTW_INTR_TBDOK);
			PRINTINTR(RTW_INTR_THPDER);
			PRINTINTR(RTW_INTR_THPDOK);
			PRINTINTR(RTW_INTR_TNPDER);
			PRINTINTR(RTW_INTR_TNPDOK);
			PRINTINTR(RTW_INTR_RXFOVW);
			PRINTINTR(RTW_INTR_RDU);
			PRINTINTR(RTW_INTR_TLPDER);
			PRINTINTR(RTW_INTR_TLPDOK);
			PRINTINTR(RTW_INTR_RER);
			PRINTINTR(RTW_INTR_ROK);

			printf(">\n");
		}
#undef PRINTINTR
#endif /* RTW_DEBUG */

		if ((isr & RTW_INTR_RX) != 0)
			rtw_intr_rx(sc, isr);
		if ((isr & RTW_INTR_TX) != 0)
			rtw_intr_tx(sc, isr);
		if ((isr & RTW_INTR_BEACON) != 0)
			rtw_intr_beacon(sc, isr);
		if ((isr & RTW_INTR_ATIMINT) != 0)
			rtw_intr_atim(sc);
		if ((isr & RTW_INTR_IOERROR) != 0)
			rtw_intr_ioerror(sc, isr);
		if ((isr & RTW_INTR_TIMEOUT) != 0)
			rtw_intr_timeout(sc);
	}

	return 1;
}

/* Must be called at splnet. */
static void
rtw_stop(struct ifnet *ifp, int disable)
{
	int pri;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_regs *regs = &sc->sc_regs;

	rtw_suspend_ticks(sc);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	if (device_has_power(sc->sc_dev)) {
		/* Disable interrupts. */
		RTW_WRITE16(regs, RTW_IMR, 0);

		RTW_WBW(regs, RTW_TPPOLL, RTW_IMR);

		/* Stop the transmit and receive processes. First stop DMA,
		 * then disable receiver and transmitter.
		 */
		RTW_WRITE8(regs, RTW_TPPOLL, RTW_TPPOLL_SALL);

		RTW_SYNC(regs, RTW_TPPOLL, RTW_IMR);

		rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 0);
	}

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rtw_txsofts_release(sc->sc_dmat, &sc->sc_ic,
		    &sc->sc_txsoft_blk[pri]);
	}

	rtw_rxbufs_release(sc->sc_dmat, &sc->sc_rxsoft[0]);

	/* Mark the interface as not running.  Cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		pmf_device_suspend(sc->sc_dev, &sc->sc_qual);

	return;
}

const char *
rtw_pwrstate_string(enum rtw_pwrstate power)
{
	switch (power) {
	case RTW_ON:
		return "on";
	case RTW_SLEEP:
		return "sleep";
	case RTW_OFF:
		return "off";
	default:
		return "unknown";
	}
}

/* XXX For Maxim, I am using the RFMD settings gleaned from the
 * reference driver, plus a magic Maxim "ON" value that comes from
 * the Realtek document "Windows PG for Rtl8180."
 */
static void
rtw_maxim_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	uint32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

/* XXX I am using the RFMD settings gleaned from the reference
 * driver.  They agree
 */
static void
rtw_rfmd_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	uint32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

static void
rtw_philips_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	uint32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_PHILIPS_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_PHILIPS_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		if (digphy) {
			anaparm |= RTW_ANAPARM_RFPOW_DIG_PHILIPS_ON;
			/* XXX guess */
			anaparm |= RTW_ANAPARM_TXDACOFF;
		} else
			anaparm |= RTW_ANAPARM_RFPOW_ANA_PHILIPS_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

static void
rtw_pwrstate0(struct rtw_softc *sc, enum rtw_pwrstate power, int before_rf,
    int digphy)
{
	struct rtw_regs *regs = &sc->sc_regs;

	rtw_set_access(regs, RTW_ACCESS_ANAPARM);

	(*sc->sc_pwrstate_cb)(regs, power, before_rf, digphy);

	rtw_set_access(regs, RTW_ACCESS_NONE);

	return;
}

static int
rtw_pwrstate(struct rtw_softc *sc, enum rtw_pwrstate power)
{
	int rc;

	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: %s->%s\n", __func__,
	    rtw_pwrstate_string(sc->sc_pwrstate), rtw_pwrstate_string(power)));

	if (sc->sc_pwrstate == power)
		return 0;

	rtw_pwrstate0(sc, power, 1, sc->sc_flags & RTW_F_DIGPHY);
	rc = rtw_rf_pwrstate(sc->sc_rf, power);
	rtw_pwrstate0(sc, power, 0, sc->sc_flags & RTW_F_DIGPHY);

	switch (power) {
	case RTW_ON:
		/* TBD set LEDs */
		break;
	case RTW_SLEEP:
		/* TBD */
		break;
	case RTW_OFF:
		/* TBD */
		break;
	}
	if (rc == 0)
		sc->sc_pwrstate = power;
	else
		sc->sc_pwrstate = RTW_OFF;
	return rc;
}

static int
rtw_tune(struct rtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_tx_radiotap_header *rt = &sc->sc_txtap;
	struct rtw_rx_radiotap_header *rr = &sc->sc_rxtap;
	u_int chan;
	int rc;
	int antdiv = sc->sc_flags & RTW_F_ANTDIV,
	    dflantb = sc->sc_flags & RTW_F_DFLANTB;

	chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	KASSERT(chan != IEEE80211_CHAN_ANY);

	rt->rt_chan_freq = htole16(ic->ic_curchan->ic_freq);
	rt->rt_chan_flags = htole16(ic->ic_curchan->ic_flags);

	rr->rr_chan_freq = htole16(ic->ic_curchan->ic_freq);
	rr->rr_chan_flags = htole16(ic->ic_curchan->ic_flags);

	if (chan == sc->sc_cur_chan) {
		RTW_DPRINTF(RTW_DEBUG_TUNE,
		    ("%s: already tuned chan #%d\n", __func__, chan));
		return 0;
	}

	rtw_suspend_ticks(sc);

	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 0);

	/* TBD wait for Tx to complete */

	KASSERT(device_has_power(sc->sc_dev));

	if ((rc = rtw_phy_init(&sc->sc_regs, sc->sc_rf,
	    rtw_chan2txpower(&sc->sc_srom, ic, ic->ic_curchan), sc->sc_csthr,
	        ic->ic_curchan->ic_freq, antdiv, dflantb, RTW_ON)) != 0) {
		/* XXX condition on powersaving */
		aprint_error_dev(sc->sc_dev, "phy init failed\n");
	}

	sc->sc_cur_chan = chan;

	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 1);

	rtw_resume_ticks(sc);

	return rc;
}

bool
rtw_suspend(device_t self, const pmf_qual_t *qual)
{
	int rc;
	struct rtw_softc *sc = device_private(self);

	sc->sc_flags &= ~RTW_F_DK_VALID;

	if (!device_has_power(self))
		return false;

	/* turn off PHY */
	if ((rc = rtw_pwrstate(sc, RTW_OFF)) != 0) {
		aprint_error_dev(self, "failed to turn off PHY (%d)\n", rc);
		return false;
	}

	rtw_disable_interrupts(&sc->sc_regs);

	return true;
}

bool
rtw_resume(device_t self, const pmf_qual_t *qual)
{
	struct rtw_softc *sc = device_private(self);

	/* Power may have been removed, resetting WEP keys.
	 */
	sc->sc_flags &= ~RTW_F_DK_VALID;
	rtw_enable_interrupts(sc);

	return true;
}

static void
rtw_transmit_config(struct rtw_regs *regs)
{
	uint32_t tcr;

	tcr = RTW_READ(regs, RTW_TCR);

	tcr |= RTW_TCR_CWMIN;
	tcr &= ~RTW_TCR_MXDMA_MASK;
	tcr |= RTW_TCR_MXDMA_256;
	tcr |= RTW_TCR_SAT;		/* send ACK as fast as possible */
	tcr &= ~RTW_TCR_LBK_MASK;
	tcr |= RTW_TCR_LBK_NORMAL;	/* normal operating mode */

	/* set short/long retry limits */
	tcr &= ~(RTW_TCR_SRL_MASK|RTW_TCR_LRL_MASK);
	tcr |= __SHIFTIN(4, RTW_TCR_SRL_MASK) | __SHIFTIN(4, RTW_TCR_LRL_MASK);

	tcr &= ~RTW_TCR_CRC;	/* NIC appends CRC32 */

	RTW_WRITE(regs, RTW_TCR, tcr);
	RTW_SYNC(regs, RTW_TCR, RTW_TCR);
}

static void
rtw_disable_interrupts(struct rtw_regs *regs)
{
	RTW_WRITE16(regs, RTW_IMR, 0);
	RTW_WBW(regs, RTW_IMR, RTW_ISR);
	RTW_WRITE16(regs, RTW_ISR, 0xffff);
	RTW_SYNC(regs, RTW_IMR, RTW_ISR);
}

static void
rtw_enable_interrupts(struct rtw_softc *sc)
{
	struct rtw_regs *regs = &sc->sc_regs;

	sc->sc_inten = RTW_INTR_RX|RTW_INTR_TX|RTW_INTR_BEACON|RTW_INTR_ATIMINT;
	sc->sc_inten |= RTW_INTR_IOERROR|RTW_INTR_TIMEOUT;

	RTW_WRITE16(regs, RTW_IMR, sc->sc_inten);
	RTW_WBW(regs, RTW_IMR, RTW_ISR);
	RTW_WRITE16(regs, RTW_ISR, 0xffff);
	RTW_SYNC(regs, RTW_IMR, RTW_ISR);

	/* XXX necessary? */
	if (sc->sc_intr_ack != NULL)
		(*sc->sc_intr_ack)(regs);
}

static void
rtw_set_nettype(struct rtw_softc *sc, enum ieee80211_opmode opmode)
{
	uint8_t msr;

	/* I'm guessing that MSR is protected as CONFIG[0123] are. */
	rtw_set_access(&sc->sc_regs, RTW_ACCESS_CONFIG);

	msr = RTW_READ8(&sc->sc_regs, RTW_MSR) & ~RTW_MSR_NETYPE_MASK;

	switch (opmode) {
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_IBSS:
		msr |= RTW_MSR_NETYPE_ADHOC_OK;
		break;
	case IEEE80211_M_HOSTAP:
		msr |= RTW_MSR_NETYPE_AP_OK;
		break;
	case IEEE80211_M_MONITOR:
		/* XXX */
		msr |= RTW_MSR_NETYPE_NOLINK;
		break;
	case IEEE80211_M_STA:
		msr |= RTW_MSR_NETYPE_INFRA_OK;
		break;
	}
	RTW_WRITE8(&sc->sc_regs, RTW_MSR, msr);

	rtw_set_access(&sc->sc_regs, RTW_ACCESS_NONE);
}

#define	rtw_calchash(addr) \
	(ether_crc32_be((addr), IEEE80211_ADDR_LEN) >> 26)

static void
rtw_pktfilt_load(struct rtw_softc *sc)
{
	struct rtw_regs *regs = &sc->sc_regs;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ethercom *ec = &sc->sc_ec;
	struct ifnet *ifp = &sc->sc_if;
	int hash;
	uint32_t hashes[2] = { 0, 0 };
	struct ether_multi *enm;
	struct ether_multistep step;

	/* XXX might be necessary to stop Rx/Tx engines while setting filters */

	sc->sc_rcr &= ~RTW_RCR_PKTFILTER_MASK;
	sc->sc_rcr &= ~(RTW_RCR_MXDMA_MASK | RTW_RCR_RXFTH_MASK);

	sc->sc_rcr |= RTW_RCR_PKTFILTER_DEFAULT;
	/* MAC auto-reset PHY (huh?) */
	sc->sc_rcr |= RTW_RCR_ENMARP;
	/* DMA whole Rx packets, only.  Set Tx DMA burst size to 1024 bytes. */
	sc->sc_rcr |= RTW_RCR_MXDMA_1024 | RTW_RCR_RXFTH_WHOLE;

	switch (ic->ic_opmode) {
	case IEEE80211_M_MONITOR:
		sc->sc_rcr |= RTW_RCR_MONITOR;
		break;
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_IBSS:
		/* receive broadcasts in our BSS */
		sc->sc_rcr |= RTW_RCR_ADD3;
		break;
	default:
		break;
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Program the 64-bit multicast hash filter.
	 */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		/* XXX */
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}

		hash = rtw_calchash(enm->enm_addrlo);
		hashes[hash >> 5] |= (1 << (hash & 0x1f));
		ETHER_NEXT_MULTI(step, enm);
	}

	/* XXX accept all broadcast if scanning */
	if ((ifp->if_flags & IFF_BROADCAST) != 0)
		sc->sc_rcr |= RTW_RCR_AB;	/* accept all broadcast */

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rcr |= RTW_RCR_AB;	/* accept all broadcast */
		sc->sc_rcr |= RTW_RCR_ACRC32;	/* accept frames failing CRC */
		sc->sc_rcr |= RTW_RCR_AICV;	/* accept frames failing ICV */
		ifp->if_flags |= IFF_ALLMULTI;
	}

	if (ifp->if_flags & IFF_ALLMULTI)
		hashes[0] = hashes[1] = 0xffffffff;

	if ((hashes[0] | hashes[1]) != 0)
		sc->sc_rcr |= RTW_RCR_AM;	/* accept multicast */

	RTW_WRITE(regs, RTW_MAR0, hashes[0]);
	RTW_WRITE(regs, RTW_MAR1, hashes[1]);
	RTW_WRITE(regs, RTW_RCR, sc->sc_rcr);
	RTW_SYNC(regs, RTW_MAR0, RTW_RCR); /* RTW_MAR0 < RTW_MAR1 < RTW_RCR */

	DPRINTF(sc, RTW_DEBUG_PKTFILT,
	    ("%s: RTW_MAR0 %08x RTW_MAR1 %08x RTW_RCR %08x\n",
	    device_xname(sc->sc_dev), RTW_READ(regs, RTW_MAR0),
	    RTW_READ(regs, RTW_MAR1), RTW_READ(regs, RTW_RCR)));
}

static struct mbuf *
rtw_beacon_alloc(struct rtw_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m;
	struct ieee80211_beacon_offsets	boff;

	if ((m = ieee80211_beacon_alloc(ic, ni, &boff)) != NULL) {
		RTW_DPRINTF(RTW_DEBUG_BEACON,
		    ("%s: m %p len %u\n", __func__, m, m->m_len));
	}
	return m;
}

/* Must be called at splnet. */
static int
rtw_init(struct ifnet *ifp)
{
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_regs *regs = &sc->sc_regs;
	int rc;

	if (device_is_active(sc->sc_dev)) {
		/* Cancel pending I/O and reset. */
		rtw_stop(ifp, 0);
	} else if (!pmf_device_resume(sc->sc_dev, &sc->sc_qual) ||
	           !device_is_active(sc->sc_dev))
		return 0;

	DPRINTF(sc, RTW_DEBUG_TUNE, ("%s: channel %d freq %d flags 0x%04x\n",
	    __func__, ieee80211_chan2ieee(ic, ic->ic_curchan),
	    ic->ic_curchan->ic_freq, ic->ic_curchan->ic_flags));

	if ((rc = rtw_pwrstate(sc, RTW_OFF)) != 0)
		goto out;

	if ((rc = rtw_swring_setup(sc)) != 0)
		goto out;

	rtw_transmit_config(regs);

	rtw_set_access(regs, RTW_ACCESS_CONFIG);

	RTW_WRITE8(regs, RTW_MSR, 0x0);	/* no link */
	RTW_WBW(regs, RTW_MSR, RTW_BRSR);

	/* long PLCP header, 1Mb/2Mb basic rate */
	RTW_WRITE16(regs, RTW_BRSR, RTW_BRSR_MBR8180_2MBPS);
	RTW_SYNC(regs, RTW_BRSR, RTW_BRSR);

	rtw_set_access(regs, RTW_ACCESS_ANAPARM);
	rtw_set_access(regs, RTW_ACCESS_NONE);

	/* XXX from reference sources */
	RTW_WRITE(regs, RTW_FEMR, 0xffff);
	RTW_SYNC(regs, RTW_FEMR, RTW_FEMR);

	rtw_set_rfprog(regs, sc->sc_rfchipid, sc->sc_dev);

	RTW_WRITE8(regs, RTW_PHYDELAY, sc->sc_phydelay);
	/* from Linux driver */
	RTW_WRITE8(regs, RTW_CRCOUNT, RTW_CRCOUNT_MAGIC);

	RTW_SYNC(regs, RTW_PHYDELAY, RTW_CRCOUNT);

	rtw_enable_interrupts(sc);

	rtw_pktfilt_load(sc);

	rtw_hwring_setup(sc);

	rtw_wep_setkeys(sc, ic->ic_nw_keys, ic->ic_def_txkey);

	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 1);

	ifp->if_flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	RTW_WRITE16(regs, RTW_BSSID16, 0x0);
	RTW_WRITE(regs, RTW_BSSID32, 0x0);

	rtw_resume_ticks(sc);

	rtw_set_nettype(sc, IEEE80211_M_MONITOR);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		return ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		return ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

out:
	aprint_error_dev(sc->sc_dev, "interface not running\n");
	return rc;
}

static inline void
rtw_led_init(struct rtw_regs *regs)
{
	uint8_t cfg0, cfg1;

	rtw_set_access(regs, RTW_ACCESS_CONFIG);

	cfg0 = RTW_READ8(regs, RTW_CONFIG0);
	cfg0 |= RTW_CONFIG0_LEDGPOEN;
	RTW_WRITE8(regs, RTW_CONFIG0, cfg0);

	cfg1 = RTW_READ8(regs, RTW_CONFIG1);
	RTW_DPRINTF(RTW_DEBUG_LED,
	    ("%s: read %" PRIx8 " from reg[CONFIG1]\n", __func__, cfg1));

	cfg1 &= ~RTW_CONFIG1_LEDS_MASK;
	cfg1 |= RTW_CONFIG1_LEDS_TX_RX;
	RTW_WRITE8(regs, RTW_CONFIG1, cfg1);

	rtw_set_access(regs, RTW_ACCESS_NONE);
}

/*
 * IEEE80211_S_INIT: 		LED1 off
 *
 * IEEE80211_S_AUTH,
 * IEEE80211_S_ASSOC,
 * IEEE80211_S_SCAN: 		LED1 blinks @ 1 Hz, blinks at 5Hz for tx/rx
 *
 * IEEE80211_S_RUN: 		LED1 on, blinks @ 5Hz for tx/rx
 */
static void
rtw_led_newstate(struct rtw_softc *sc, enum ieee80211_state nstate)
{
	struct rtw_led_state *ls;

	ls = &sc->sc_led_state;

	switch (nstate) {
	case IEEE80211_S_INIT:
		rtw_led_init(&sc->sc_regs);
		aprint_debug_dev(sc->sc_dev, "stopping blink\n");
		callout_stop(&ls->ls_slow_ch);
		callout_stop(&ls->ls_fast_ch);
		ls->ls_slowblink = 0;
		ls->ls_actblink = 0;
		ls->ls_default = 0;
		break;
	case IEEE80211_S_SCAN:
		aprint_debug_dev(sc->sc_dev, "scheduling blink\n");
		callout_schedule(&ls->ls_slow_ch, RTW_LED_SLOW_TICKS);
		callout_schedule(&ls->ls_fast_ch, RTW_LED_FAST_TICKS);
		/*FALLTHROUGH*/
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		ls->ls_default = RTW_LED1;
		ls->ls_actblink = RTW_LED1;
		ls->ls_slowblink = RTW_LED1;
		break;
	case IEEE80211_S_RUN:
		ls->ls_slowblink = 0;
		break;
	}
	rtw_led_set(ls, &sc->sc_regs, sc->sc_hwverid);
}

static void
rtw_led_set(struct rtw_led_state *ls, struct rtw_regs *regs, int hwverid)
{
	uint8_t led_condition;
	bus_size_t ofs;
	uint8_t mask, newval, val;

	led_condition = ls->ls_default;

	if (ls->ls_state & RTW_LED_S_SLOW)
		led_condition ^= ls->ls_slowblink;
	if (ls->ls_state & (RTW_LED_S_RX|RTW_LED_S_TX))
		led_condition ^= ls->ls_actblink;

	RTW_DPRINTF(RTW_DEBUG_LED,
	    ("%s: LED condition %" PRIx8 "\n", __func__, led_condition));

	switch (hwverid) {
	default:
	case 'F':
		ofs = RTW_PSR;
		newval = mask = RTW_PSR_LEDGPO0 | RTW_PSR_LEDGPO1;
		if (led_condition & RTW_LED0)
			newval &= ~RTW_PSR_LEDGPO0;
		if (led_condition & RTW_LED1)
			newval &= ~RTW_PSR_LEDGPO1;
		break;
	case 'D':
		ofs = RTW_9346CR;
		mask = RTW_9346CR_EEM_MASK | RTW_9346CR_EEDI | RTW_9346CR_EECS;
		newval = RTW_9346CR_EEM_PROGRAM;
		if (led_condition & RTW_LED0)
			newval |= RTW_9346CR_EEDI;
		if (led_condition & RTW_LED1)
			newval |= RTW_9346CR_EECS;
		break;
	}
	val = RTW_READ8(regs, ofs);
	RTW_DPRINTF(RTW_DEBUG_LED,
	    ("%s: read %" PRIx8 " from reg[%#02" PRIxPTR "]\n", __func__, val,
	     (uintptr_t)ofs));
	val &= ~mask;
	val |= newval;
	RTW_WRITE8(regs, ofs, val);
	RTW_DPRINTF(RTW_DEBUG_LED,
	    ("%s: wrote %" PRIx8 " to reg[%#02" PRIxPTR "]\n", __func__, val,
	     (uintptr_t)ofs));
	RTW_SYNC(regs, ofs, ofs);
}

static void
rtw_led_fastblink(void *arg)
{
	int ostate, s;
	struct rtw_softc *sc = (struct rtw_softc *)arg;
	struct rtw_led_state *ls = &sc->sc_led_state;

	s = splnet();
	ostate = ls->ls_state;
	ls->ls_state ^= ls->ls_event;

	if ((ls->ls_event & RTW_LED_S_TX) == 0)
		ls->ls_state &= ~RTW_LED_S_TX;

	if ((ls->ls_event & RTW_LED_S_RX) == 0)
		ls->ls_state &= ~RTW_LED_S_RX;

	ls->ls_event = 0;

	if (ostate != ls->ls_state)
		rtw_led_set(ls, &sc->sc_regs, sc->sc_hwverid);
	splx(s);

	aprint_debug_dev(sc->sc_dev, "scheduling fast blink\n");
	callout_schedule(&ls->ls_fast_ch, RTW_LED_FAST_TICKS);
}

static void
rtw_led_slowblink(void *arg)
{
	int s;
	struct rtw_softc *sc = (struct rtw_softc *)arg;
	struct rtw_led_state *ls = &sc->sc_led_state;

	s = splnet();
	ls->ls_state ^= RTW_LED_S_SLOW;
	rtw_led_set(ls, &sc->sc_regs, sc->sc_hwverid);
	splx(s);
	aprint_debug_dev(sc->sc_dev, "scheduling slow blink\n");
	callout_schedule(&ls->ls_slow_ch, RTW_LED_SLOW_TICKS);
}

static void
rtw_led_detach(struct rtw_led_state *ls)
{
	callout_destroy(&ls->ls_fast_ch);
	callout_destroy(&ls->ls_slow_ch);
}

static void
rtw_led_attach(struct rtw_led_state *ls, void *arg)
{
	callout_init(&ls->ls_fast_ch, 0);
	callout_init(&ls->ls_slow_ch, 0);
	callout_setfunc(&ls->ls_fast_ch, rtw_led_fastblink, arg);
	callout_setfunc(&ls->ls_slow_ch, rtw_led_slowblink, arg);
}

static int
rtw_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int rc = 0, s;
	struct rtw_softc *sc = ifp->if_softc;

	s = splnet();
	if (cmd == SIOCSIFFLAGS) {
		if ((rc = ifioctl_common(ifp, cmd, data)) != 0)
			;
		else switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP:
			rc = rtw_init(ifp);
			RTW_PRINT_REGS(&sc->sc_regs, ifp->if_xname, __func__);
			break;
		case IFF_UP|IFF_RUNNING:
			if (device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
				rtw_pktfilt_load(sc);
			RTW_PRINT_REGS(&sc->sc_regs, ifp->if_xname, __func__);
			break;
		case IFF_RUNNING:
			RTW_PRINT_REGS(&sc->sc_regs, ifp->if_xname, __func__);
			rtw_stop(ifp, 1);
			break;
		default:
			break;
		}
	} else if ((rc = ieee80211_ioctl(&sc->sc_ic, cmd, data)) != ENETRESET)
		;	/* nothing to do */
	else if (cmd == SIOCADDMULTI || cmd == SIOCDELMULTI) {
		/* reload packet filter if running */
		if (ifp->if_flags & IFF_RUNNING)
			rtw_pktfilt_load(sc);
		rc = 0;
	} else if ((ifp->if_flags & IFF_UP) != 0)
		rc = rtw_init(ifp);
	else
		rc = 0;
	splx(s);
	return rc;
}

/* Select a transmit ring with at least one h/w and s/w descriptor free.
 * Return 0 on success, -1 on failure.
 */
static inline int
rtw_txring_choose(struct rtw_softc *sc, struct rtw_txsoft_blk **tsbp,
    struct rtw_txdesc_blk **tdbp, int pri)
{
	struct rtw_txsoft_blk *tsb;
	struct rtw_txdesc_blk *tdb;

	KASSERT(pri >= 0 && pri < RTW_NTXPRI);

	tsb = &sc->sc_txsoft_blk[pri];
	tdb = &sc->sc_txdesc_blk[pri];

	if (SIMPLEQ_EMPTY(&tsb->tsb_freeq) || tdb->tdb_nfree == 0) {
		if (tsb->tsb_tx_timer == 0)
			tsb->tsb_tx_timer = 5;
		*tsbp = NULL;
		*tdbp = NULL;
		return -1;
	}
	*tsbp = tsb;
	*tdbp = tdb;
	return 0;
}

static inline struct mbuf *
rtw_80211_dequeue(struct rtw_softc *sc, struct ifqueue *ifq, int pri,
    struct rtw_txsoft_blk **tsbp, struct rtw_txdesc_blk **tdbp,
    struct ieee80211_node **nip, short *if_flagsp)
{
	struct mbuf *m;

	if (IF_IS_EMPTY(ifq))
		return NULL;
	if (rtw_txring_choose(sc, tsbp, tdbp, pri) == -1) {
		DPRINTF(sc, RTW_DEBUG_XMIT_RSRC, ("%s: no ring %d descriptor\n",
		    __func__, pri));
		*if_flagsp |= IFF_OACTIVE;
		sc->sc_if.if_timer = 1;
		return NULL;
	}
	IF_DEQUEUE(ifq, m);
	*nip = (struct ieee80211_node *)m->m_pkthdr.rcvif;
	m->m_pkthdr.rcvif = NULL;
	KASSERT(*nip != NULL);
	return m;
}

/* Point *mp at the next 802.11 frame to transmit.  Point *tsbp
 * at the driver's selection of transmit control block for the packet.
 */
static inline int
rtw_dequeue(struct ifnet *ifp, struct rtw_txsoft_blk **tsbp,
    struct rtw_txdesc_blk **tdbp, struct mbuf **mp,
    struct ieee80211_node **nip)
{
	int pri;
	struct ether_header *eh;
	struct mbuf *m0;
	struct rtw_softc *sc;
	short *if_flagsp;

	*mp = NULL;

	sc = (struct rtw_softc *)ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_XMIT,
	    ("%s: enter %s\n", device_xname(sc->sc_dev), __func__));

	if_flagsp = &ifp->if_flags;

	if (sc->sc_ic.ic_state == IEEE80211_S_RUN &&
	    (*mp = rtw_80211_dequeue(sc, &sc->sc_beaconq, RTW_TXPRIBCN, tsbp,
		                     tdbp, nip, if_flagsp)) != NULL) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: dequeue beacon frame\n",
		    __func__));
		return 0;
	}

	if ((*mp = rtw_80211_dequeue(sc, &sc->sc_ic.ic_mgtq, RTW_TXPRIMD, tsbp,
		                     tdbp, nip, if_flagsp)) != NULL) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: dequeue mgt frame\n",
		    __func__));
		return 0;
	}

	if (sc->sc_ic.ic_state != IEEE80211_S_RUN) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: not running\n", __func__));
		return 0;
	}

	IFQ_POLL(&ifp->if_snd, m0);
	if (m0 == NULL) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: no frame ready\n",
		    __func__));
		return 0;
	}

	pri = ((m0->m_flags & M_PWR_SAV) != 0) ? RTW_TXPRIHI : RTW_TXPRIMD;

	if (rtw_txring_choose(sc, tsbp, tdbp, pri) == -1) {
		DPRINTF(sc, RTW_DEBUG_XMIT_RSRC, ("%s: no ring %d descriptor\n",
		    __func__, pri));
		*if_flagsp |= IFF_OACTIVE;
		sc->sc_if.if_timer = 1;
		return 0;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: no frame ready\n",
		    __func__));
		return 0;
	}
	DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: dequeue data frame\n", __func__));
	ifp->if_opackets++;
	bpf_mtap(ifp, m0);
	eh = mtod(m0, struct ether_header *);
	*nip = ieee80211_find_txnode(&sc->sc_ic, eh->ether_dhost);
	if (*nip == NULL) {
		/* NB: ieee80211_find_txnode does stat+msg */
		m_freem(m0);
		return -1;
	}
	if ((m0 = ieee80211_encap(&sc->sc_ic, m0, *nip)) == NULL) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: encap error\n", __func__));
		ifp->if_oerrors++;
		return -1;
	}
	DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: leave\n", __func__));
	*mp = m0;
	return 0;
}

static int
rtw_seg_too_short(bus_dmamap_t dmamap)
{
	int i;
	for (i = 0; i < dmamap->dm_nsegs; i++) {
		if (dmamap->dm_segs[i].ds_len < 4)
			return 1;
	}
	return 0;
}

/* TBD factor with atw_start */
static struct mbuf *
rtw_dmamap_load_txbuf(bus_dma_tag_t dmat, bus_dmamap_t dmam, struct mbuf *chain,
    u_int ndescfree, device_t dev)
{
	int first, rc;
	struct mbuf *m, *m0;

	m0 = chain;

	/*
	 * Load the DMA map.  Copy and try (once) again if the packet
	 * didn't fit in the alloted number of segments.
	 */
	for (first = 1;
	     ((rc = bus_dmamap_load_mbuf(dmat, dmam, m0,
			  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) != 0 ||
	      dmam->dm_nsegs > ndescfree || rtw_seg_too_short(dmam)) && first;
	     first = 0) {
		if (rc == 0) {
#ifdef RTW_DIAGxxx
			if (rtw_seg_too_short(dmam)) {
				printf("%s: short segment, mbuf lengths:", __func__);
				for (m = m0; m; m = m->m_next)
					printf(" %d", m->m_len);
				printf("\n");
			}
#endif
			bus_dmamap_unload(dmat, dmam);
		}
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			aprint_error_dev(dev, "unable to allocate Tx mbuf\n");
			break;
		}
		if (m0->m_pkthdr.len > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				aprint_error_dev(dev,
				    "cannot allocate Tx cluster\n");
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
	if (rc != 0) {
		aprint_error_dev(dev, "cannot load Tx buffer, rc = %d\n", rc);
		m_freem(m0);
		return NULL;
	} else if (rtw_seg_too_short(dmam)) {
		aprint_error_dev(dev,
		    "cannot load Tx buffer, segment too short\n");
		bus_dmamap_unload(dmat, dmam);
		m_freem(m0);
		return NULL;
	} else if (dmam->dm_nsegs > ndescfree) {
		aprint_error_dev(dev, "too many tx segments\n");
		bus_dmamap_unload(dmat, dmam);
		m_freem(m0);
		return NULL;
	}
	return m0;
}

#ifdef RTW_DEBUG
static void
rtw_print_txdesc(struct rtw_softc *sc, const char *action,
    struct rtw_txsoft *ts, struct rtw_txdesc_blk *tdb, int desc)
{
	struct rtw_txdesc *td = &tdb->tdb_desc[desc];
	DPRINTF(sc, RTW_DEBUG_XMIT_DESC, ("%s: %p %s txdesc[%d] next %#08x "
	    "buf %#08x ctl0 %#08x ctl1 %#08x len %#08x\n",
	    device_xname(sc->sc_dev), ts, action, desc,
	    le32toh(td->td_buf), le32toh(td->td_next),
	    le32toh(td->td_ctl0), le32toh(td->td_ctl1),
	    le32toh(td->td_len)));
}
#endif /* RTW_DEBUG */

static void
rtw_start(struct ifnet *ifp)
{
	int desc, i, lastdesc, npkt, rate;
	uint32_t proto_ctl0, ctl0, ctl1;
	bus_dmamap_t		dmamap;
	struct ieee80211com	*ic;
	struct ieee80211_duration *d0;
	struct ieee80211_frame_min	*wh;
	struct ieee80211_node	*ni = NULL;	/* XXX: GCC */
	struct mbuf		*m0;
	struct rtw_softc	*sc;
	struct rtw_txsoft_blk	*tsb = NULL;	/* XXX: GCC */
	struct rtw_txdesc_blk	*tdb = NULL;	/* XXX: GCC */
	struct rtw_txsoft	*ts;
	struct rtw_txdesc	*td;
	struct ieee80211_key	*k;

	sc = (struct rtw_softc *)ifp->if_softc;
	ic = &sc->sc_ic;

	DPRINTF(sc, RTW_DEBUG_XMIT,
	    ("%s: enter %s\n", device_xname(sc->sc_dev), __func__));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		goto out;

	/* XXX do real rate control */
	proto_ctl0 = RTW_TXCTL0_RTSRATE_1MBPS;

	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0)
		proto_ctl0 |= RTW_TXCTL0_SPLCP;

	for (;;) {
		if (rtw_dequeue(ifp, &tsb, &tdb, &m0, &ni) == -1)
			continue;
		if (m0 == NULL)
			break;

		wh = mtod(m0, struct ieee80211_frame_min *);

		if ((wh->i_fc[1] & IEEE80211_FC1_WEP) != 0 &&
		    (k = ieee80211_crypto_encap(ic, ni, m0)) == NULL) {
			m_freem(m0);
			break;
		} else
			k = NULL;

		ts = SIMPLEQ_FIRST(&tsb->tsb_freeq);

		dmamap = ts->ts_dmamap;

		m0 = rtw_dmamap_load_txbuf(sc->sc_dmat, dmamap, m0,
		    tdb->tdb_nfree, sc->sc_dev);

		if (m0 == NULL || dmamap->dm_nsegs == 0) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: fail dmamap load\n", __func__));
			goto post_dequeue_err;
		}

		/* Note well: rtw_dmamap_load_txbuf may have created
		 * a new chain, so we must find the header once
		 * more.
		 */
		wh = mtod(m0, struct ieee80211_frame_min *);

		/* XXX do real rate control */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT)
			rate = 2;
		else
			rate = MAX(2, ieee80211_get_rate(ni));

#ifdef RTW_DEBUG
		if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			ieee80211_dump_pkt(mtod(m0, uint8_t *),
			    (dmamap->dm_nsegs == 1) ? m0->m_pkthdr.len
			                            : sizeof(wh),
			    rate, 0);
		}
#endif /* RTW_DEBUG */
		ctl0 = proto_ctl0 |
		    __SHIFTIN(m0->m_pkthdr.len, RTW_TXCTL0_TPKTSIZE_MASK);

		switch (rate) {
		default:
		case 2:
			ctl0 |= RTW_TXCTL0_RATE_1MBPS;
			break;
		case 4:
			ctl0 |= RTW_TXCTL0_RATE_2MBPS;
			break;
		case 11:
			ctl0 |= RTW_TXCTL0_RATE_5MBPS;
			break;
		case 22:
			ctl0 |= RTW_TXCTL0_RATE_11MBPS;
			break;
		}
		/* XXX >= ? Compare after fragmentation? */
		if (m0->m_pkthdr.len > ic->ic_rtsthreshold)
			ctl0 |= RTW_TXCTL0_RTSEN;

                /* XXX Sometimes writes a bogus keyid; h/w doesn't
                 * seem to care, since we don't activate h/w Tx
                 * encryption.
		 */
		if (k != NULL &&
		    k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP) {
			ctl0 |= __SHIFTIN(k->wk_keyix, RTW_TXCTL0_KEYID_MASK) &
			    RTW_TXCTL0_KEYID_MASK;
		}

		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT) {
			ctl0 &= ~(RTW_TXCTL0_SPLCP | RTW_TXCTL0_RTSEN);
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			    IEEE80211_FC0_SUBTYPE_BEACON)
				ctl0 |= RTW_TXCTL0_BEACON;
		}

		if (ieee80211_compute_duration(wh, k, m0->m_pkthdr.len,
		    ic->ic_flags, ic->ic_fragthreshold,
		    rate, &ts->ts_d0, &ts->ts_dn, &npkt,
		    (ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) == -1) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: fail compute duration\n", __func__));
			goto post_load_err;
		}

		d0 = &ts->ts_d0;

		*(uint16_t*)wh->i_dur = htole16(d0->d_data_dur);

		ctl1 = __SHIFTIN(d0->d_plcp_len, RTW_TXCTL1_LENGTH_MASK) |
		    __SHIFTIN(d0->d_rts_dur, RTW_TXCTL1_RTSDUR_MASK);

		if (d0->d_residue)
			ctl1 |= RTW_TXCTL1_LENGEXT;

		/* TBD fragmentation */

		ts->ts_first = tdb->tdb_next;

		rtw_txdescs_sync(tdb, ts->ts_first, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREWRITE);

		KASSERT(ts->ts_first < tdb->tdb_ndesc);

		bpf_mtap3(ic->ic_rawbpf, m0);

		if (sc->sc_radiobpf != NULL) {
			struct rtw_tx_radiotap_header *rt = &sc->sc_txtap;

			rt->rt_rate = rate;

			bpf_mtap2(sc->sc_radiobpf, rt, sizeof(sc->sc_txtapu),
			    m0);
		}

		for (i = 0, lastdesc = desc = ts->ts_first;
		     i < dmamap->dm_nsegs;
		     i++, desc = RTW_NEXT_IDX(tdb, desc)) {
			if (dmamap->dm_segs[i].ds_len > RTW_TXLEN_LENGTH_MASK) {
				DPRINTF(sc, RTW_DEBUG_XMIT_DESC,
				    ("%s: seg too long\n", __func__));
				goto post_load_err;
			}
			td = &tdb->tdb_desc[desc];
			td->td_ctl0 = htole32(ctl0);
			td->td_ctl1 = htole32(ctl1);
			td->td_buf = htole32(dmamap->dm_segs[i].ds_addr);
			td->td_len = htole32(dmamap->dm_segs[i].ds_len);
			td->td_next = htole32(RTW_NEXT_DESC(tdb, desc));
			if (i != 0)
				td->td_ctl0 |= htole32(RTW_TXCTL0_OWN);
			lastdesc = desc;
#ifdef RTW_DEBUG
			rtw_print_txdesc(sc, "load", ts, tdb, desc);
#endif /* RTW_DEBUG */
		}

		KASSERT(desc < tdb->tdb_ndesc);

		ts->ts_ni = ni;
		KASSERT(ni != NULL);
		ts->ts_mbuf = m0;
		ts->ts_last = lastdesc;
		tdb->tdb_desc[ts->ts_last].td_ctl0 |= htole32(RTW_TXCTL0_LS);
		tdb->tdb_desc[ts->ts_first].td_ctl0 |=
		   htole32(RTW_TXCTL0_FS);

#ifdef RTW_DEBUG
		rtw_print_txdesc(sc, "FS on", ts, tdb, ts->ts_first);
		rtw_print_txdesc(sc, "LS on", ts, tdb, ts->ts_last);
#endif /* RTW_DEBUG */

		tdb->tdb_nfree -= dmamap->dm_nsegs;
		tdb->tdb_next = desc;

		rtw_txdescs_sync(tdb, ts->ts_first, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		tdb->tdb_desc[ts->ts_first].td_ctl0 |=
		    htole32(RTW_TXCTL0_OWN);

#ifdef RTW_DEBUG
		rtw_print_txdesc(sc, "OWN on", ts, tdb, ts->ts_first);
#endif /* RTW_DEBUG */

		rtw_txdescs_sync(tdb, ts->ts_first, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		SIMPLEQ_REMOVE_HEAD(&tsb->tsb_freeq, ts_q);
		SIMPLEQ_INSERT_TAIL(&tsb->tsb_dirtyq, ts, ts_q);

		if (tsb != &sc->sc_txsoft_blk[RTW_TXPRIBCN])
			sc->sc_led_state.ls_event |= RTW_LED_S_TX;
		tsb->tsb_tx_timer = 5;
		ifp->if_timer = 1;
		rtw_tx_kick(&sc->sc_regs, tsb->tsb_poll);
	}
out:
	DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: leave\n", __func__));
	return;
post_load_err:
	bus_dmamap_unload(sc->sc_dmat, dmamap);
	m_freem(m0);
post_dequeue_err:
	ieee80211_free_node(ni);
	return;
}

static void
rtw_idle(struct rtw_regs *regs)
{
	int active;
	uint8_t tppoll;

	/* request stop DMA; wait for packets to stop transmitting. */

	RTW_WRITE8(regs, RTW_TPPOLL, RTW_TPPOLL_SALL);
	RTW_WBR(regs, RTW_TPPOLL, RTW_TPPOLL);

	for (active = 0; active < 300 &&
	     (tppoll = RTW_READ8(regs, RTW_TPPOLL) & RTW_TPPOLL_ACTIVE) != 0;
	     active++)
		DELAY(10);
	printf("%s: transmit DMA idle in %dus, tppoll %02" PRIx8 "\n", __func__,
	    active * 10, tppoll);
}

static void
rtw_watchdog(struct ifnet *ifp)
{
	int pri, tx_timeouts = 0;
	struct rtw_softc *sc;
	struct rtw_txsoft_blk *tsb;

	sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (!device_is_active(sc->sc_dev))
		return;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];

		if (tsb->tsb_tx_timer == 0)
			continue;
		else if (--tsb->tsb_tx_timer == 0) {
			if (SIMPLEQ_EMPTY(&tsb->tsb_dirtyq))
				continue;
			else if (rtw_collect_txring(sc, tsb,
			    &sc->sc_txdesc_blk[pri], 0))
				continue;
			printf("%s: transmit timeout, priority %d\n",
			    ifp->if_xname, pri);
			ifp->if_oerrors++;
			if (pri != RTW_TXPRIBCN)
				tx_timeouts++;
		} else
			ifp->if_timer = 1;
	}

	if (tx_timeouts > 0) {
		/* Stop Tx DMA, disable xmtr, flush Tx rings, enable xmtr,
		 * reset s/w tx-ring pointers, and start transmission.
		 *
		 * TBD Stop/restart just the broken rings?
		 */
		rtw_idle(&sc->sc_regs);
		rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 0);
		rtw_txdescs_reset(sc);
		rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 1);
		rtw_start(ifp);
	}
	ieee80211_watchdog(&sc->sc_ic);
	return;
}

static void
rtw_next_scan(void *arg)
{
	struct ieee80211com *ic = arg;
	int s;

	/* don't call rtw_start w/o network interrupts blocked */
	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
	splx(s);
}

static void
rtw_join_bss(struct rtw_softc *sc, uint8_t *bssid, uint16_t intval0)
{
	uint16_t bcnitv, bintritv, intval;
	int i;
	struct rtw_regs *regs = &sc->sc_regs;

	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		RTW_WRITE8(regs, RTW_BSSID + i, bssid[i]);

	RTW_SYNC(regs, RTW_BSSID16, RTW_BSSID32);

	rtw_set_access(regs, RTW_ACCESS_CONFIG);

	intval = MIN(intval0, __SHIFTOUT_MASK(RTW_BCNITV_BCNITV_MASK));

	bcnitv = RTW_READ16(regs, RTW_BCNITV) & ~RTW_BCNITV_BCNITV_MASK;
	bcnitv |= __SHIFTIN(intval, RTW_BCNITV_BCNITV_MASK);
	RTW_WRITE16(regs, RTW_BCNITV, bcnitv);
	/* interrupt host 1ms before the TBTT */
	bintritv = RTW_READ16(regs, RTW_BINTRITV) & ~RTW_BINTRITV_BINTRITV;
	bintritv |= __SHIFTIN(1000, RTW_BINTRITV_BINTRITV);
	RTW_WRITE16(regs, RTW_BINTRITV, bintritv);
	/* magic from Linux */
	RTW_WRITE16(regs, RTW_ATIMWND, __SHIFTIN(1, RTW_ATIMWND_ATIMWND));
	RTW_WRITE16(regs, RTW_ATIMTRITV, __SHIFTIN(2, RTW_ATIMTRITV_ATIMTRITV));
	rtw_set_access(regs, RTW_ACCESS_NONE);

	rtw_io_enable(sc, RTW_CR_RE | RTW_CR_TE, 1);
}

/* Synchronize the hardware state with the software state. */
static int
rtw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	enum ieee80211_state ostate;
	int error;

	ostate = ic->ic_state;

	aprint_debug_dev(sc->sc_dev, "%s: l.%d\n", __func__, __LINE__);
	rtw_led_newstate(sc, nstate);

	aprint_debug_dev(sc->sc_dev, "%s: l.%d\n", __func__, __LINE__);
	if (nstate == IEEE80211_S_INIT) {
		callout_stop(&sc->sc_scan_ch);
		sc->sc_cur_chan = IEEE80211_CHAN_ANY;
		return (*sc->sc_mtbl.mt_newstate)(ic, nstate, arg);
	}

	if (ostate == IEEE80211_S_INIT && nstate != IEEE80211_S_INIT)
		rtw_pwrstate(sc, RTW_ON);

	if ((error = rtw_tune(sc)) != 0)
		return error;

	switch (nstate) {
	case IEEE80211_S_INIT:
		panic("%s: unexpected state IEEE80211_S_INIT\n", __func__);
		break;
	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			(void)memset(ic->ic_bss->ni_bssid, 0,
			    IEEE80211_ADDR_LEN);
			rtw_set_nettype(sc, IEEE80211_M_MONITOR);
		}

		callout_reset(&sc->sc_scan_ch, rtw_dwelltime * hz / 1000,
		    rtw_next_scan, ic);

		break;
	case IEEE80211_S_RUN:
		switch (ic->ic_opmode) {
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_IBSS:
			rtw_set_nettype(sc, IEEE80211_M_MONITOR);
			/*FALLTHROUGH*/
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_STA:
			rtw_join_bss(sc, ic->ic_bss->ni_bssid,
			    ic->ic_bss->ni_intval);
			break;
		case IEEE80211_M_MONITOR:
			break;
		}
		rtw_set_nettype(sc, ic->ic_opmode);
		break;
	case IEEE80211_S_ASSOC:
	case IEEE80211_S_AUTH:
		break;
	}

	if (nstate != IEEE80211_S_SCAN)
		callout_stop(&sc->sc_scan_ch);

	return (*sc->sc_mtbl.mt_newstate)(ic, nstate, arg);
}

/* Extend a 32-bit TSF timestamp to a 64-bit timestamp. */
static uint64_t
rtw_tsf_extend(struct rtw_regs *regs, uint32_t rstamp)
{
	uint32_t tsftl, tsfth;

	tsfth = RTW_READ(regs, RTW_TSFTRH);
	tsftl = RTW_READ(regs, RTW_TSFTRL);
	if (tsftl < rstamp)	/* Compensate for rollover. */
		tsfth--;
	return ((uint64_t)tsfth << 32) | rstamp;
}

static void
rtw_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, uint32_t rstamp)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;

	(*sc->sc_mtbl.mt_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    ic->ic_state == IEEE80211_S_RUN &&
		    device_is_active(sc->sc_dev)) {
			uint64_t tsf = rtw_tsf_extend(&sc->sc_regs, rstamp);
			if (le64toh(ni->ni_tstamp.tsf) >= tsf)
				(void)ieee80211_ibss_merge(ni);
		}
		break;
	default:
		break;
	}
	return;
}

static struct ieee80211_node *
rtw_node_alloc(struct ieee80211_node_table *nt)
{
	struct ifnet *ifp = nt->nt_ic->ic_ifp;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	struct ieee80211_node *ni = (*sc->sc_mtbl.mt_node_alloc)(nt);

	DPRINTF(sc, RTW_DEBUG_NODE,
	    ("%s: alloc node %p\n", device_xname(sc->sc_dev), ni));
	return ni;
}

static void
rtw_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_NODE,
	    ("%s: freeing node %p %s\n", device_xname(sc->sc_dev), ni,
	    ether_sprintf(ni->ni_bssid)));
	(*sc->sc_mtbl.mt_node_free)(ni);
}

static int
rtw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
		    (IFF_RUNNING|IFF_UP))
			rtw_init(ifp);		/* XXX lose error */
		error = 0;
	}
	return error;
}

static void
rtw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct rtw_softc *sc = ifp->if_softc;

	if (!device_is_active(sc->sc_dev)) {
		imr->ifm_active = IFM_IEEE80211 | IFM_NONE;
		imr->ifm_status = 0;
		return;
	}
	ieee80211_media_status(ifp, imr);
}

static inline void
rtw_setifprops(struct ifnet *ifp, const char *dvname, void *softc)
{
	(void)strlcpy(ifp->if_xname, dvname, IFNAMSIZ);
	ifp->if_softc = softc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST |
	    IFF_NOTRAILERS;
	ifp->if_ioctl = rtw_ioctl;
	ifp->if_start = rtw_start;
	ifp->if_watchdog = rtw_watchdog;
	ifp->if_init = rtw_init;
	ifp->if_stop = rtw_stop;
}

static inline void
rtw_set80211props(struct ieee80211com *ic)
{
	int nrate;
	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_PMGT | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_MONITOR | IEEE80211_C_WEP;

	nrate = 0;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] =
	    IEEE80211_RATE_BASIC | 2;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] =
	    IEEE80211_RATE_BASIC | 4;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 11;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 22;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = nrate;
}

static inline void
rtw_set80211methods(struct rtw_mtbl *mtbl, struct ieee80211com *ic)
{
	mtbl->mt_newstate = ic->ic_newstate;
	ic->ic_newstate = rtw_newstate;

	mtbl->mt_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = rtw_recv_mgmt;

	mtbl->mt_node_free = ic->ic_node_free;
	ic->ic_node_free = rtw_node_free;

	mtbl->mt_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = rtw_node_alloc;

	ic->ic_crypto.cs_key_delete = rtw_key_delete;
	ic->ic_crypto.cs_key_set = rtw_key_set;
	ic->ic_crypto.cs_key_update_begin = rtw_key_update_begin;
	ic->ic_crypto.cs_key_update_end = rtw_key_update_end;
}

static inline void
rtw_init_radiotap(struct rtw_softc *sc)
{
	uint32_t present;

	memset(&sc->sc_rxtapu, 0, sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.rr_ihdr.it_len = htole16(sizeof(sc->sc_rxtapu));

	if (sc->sc_rfchipid == RTW_RFCHIPID_PHILIPS)
		present = htole32(RTW_PHILIPS_RX_RADIOTAP_PRESENT);
	else
		present = htole32(RTW_RX_RADIOTAP_PRESENT);
	sc->sc_rxtap.rr_ihdr.it_present = present;

	memset(&sc->sc_txtapu, 0, sizeof(sc->sc_txtapu));
	sc->sc_txtap.rt_ihdr.it_len = htole16(sizeof(sc->sc_txtapu));
	sc->sc_txtap.rt_ihdr.it_present = htole32(RTW_TX_RADIOTAP_PRESENT);
}

static int
rtw_txsoft_blk_setup(struct rtw_txsoft_blk *tsb, u_int qlen)
{
	SIMPLEQ_INIT(&tsb->tsb_dirtyq);
	SIMPLEQ_INIT(&tsb->tsb_freeq);
	tsb->tsb_ndesc = qlen;
	tsb->tsb_desc = malloc(qlen * sizeof(*tsb->tsb_desc), M_DEVBUF,
	    M_NOWAIT);
	if (tsb->tsb_desc == NULL)
		return ENOMEM;
	return 0;
}

static void
rtw_txsoft_blk_cleanup_all(struct rtw_softc *sc)
{
	int pri;
	struct rtw_txsoft_blk *tsb;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];
		free(tsb->tsb_desc, M_DEVBUF);
		tsb->tsb_desc = NULL;
	}
}

static int
rtw_txsoft_blk_setup_all(struct rtw_softc *sc)
{
	int pri, rc = 0;
	int qlen[RTW_NTXPRI] =
	     {RTW_TXQLENLO, RTW_TXQLENMD, RTW_TXQLENHI, RTW_TXQLENBCN};
	struct rtw_txsoft_blk *tsbs;

	tsbs = sc->sc_txsoft_blk;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rc = rtw_txsoft_blk_setup(&tsbs[pri], qlen[pri]);
		if (rc != 0)
			break;
	}
	tsbs[RTW_TXPRILO].tsb_poll = RTW_TPPOLL_LPQ | RTW_TPPOLL_SLPQ;
	tsbs[RTW_TXPRIMD].tsb_poll = RTW_TPPOLL_NPQ | RTW_TPPOLL_SNPQ;
	tsbs[RTW_TXPRIHI].tsb_poll = RTW_TPPOLL_HPQ | RTW_TPPOLL_SHPQ;
	tsbs[RTW_TXPRIBCN].tsb_poll = RTW_TPPOLL_BQ | RTW_TPPOLL_SBQ;
	return rc;
}

static void
rtw_txdesc_blk_setup(struct rtw_txdesc_blk *tdb, struct rtw_txdesc *desc,
    u_int ndesc, bus_addr_t ofs, bus_addr_t physbase)
{
	tdb->tdb_ndesc = ndesc;
	tdb->tdb_desc = desc;
	tdb->tdb_physbase = physbase;
	tdb->tdb_ofs = ofs;

	(void)memset(tdb->tdb_desc, 0,
	    sizeof(tdb->tdb_desc[0]) * tdb->tdb_ndesc);

	rtw_txdesc_blk_init(tdb);
	tdb->tdb_next = 0;
}

static void
rtw_txdesc_blk_setup_all(struct rtw_softc *sc)
{
	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRILO],
	    &sc->sc_descs->hd_txlo[0], RTW_NTXDESCLO,
	    RTW_RING_OFFSET(hd_txlo), RTW_RING_BASE(sc, hd_txlo));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIMD],
	    &sc->sc_descs->hd_txmd[0], RTW_NTXDESCMD,
	    RTW_RING_OFFSET(hd_txmd), RTW_RING_BASE(sc, hd_txmd));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIHI],
	    &sc->sc_descs->hd_txhi[0], RTW_NTXDESCHI,
	    RTW_RING_OFFSET(hd_txhi), RTW_RING_BASE(sc, hd_txhi));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIBCN],
	    &sc->sc_descs->hd_bcn[0], RTW_NTXDESCBCN,
	    RTW_RING_OFFSET(hd_bcn), RTW_RING_BASE(sc, hd_bcn));
}

static struct rtw_rf *
rtw_rf_attach(struct rtw_softc *sc, enum rtw_rfchipid rfchipid, int digphy)
{
	rtw_rf_write_t rf_write;
	struct rtw_rf *rf;

	switch (rfchipid) {
	default:
		rf_write = rtw_rf_hostwrite;
		break;
	case RTW_RFCHIPID_INTERSIL:
	case RTW_RFCHIPID_PHILIPS:
	case RTW_RFCHIPID_GCT:	/* XXX a guess */
	case RTW_RFCHIPID_RFMD:
		rf_write = (rtw_host_rfio) ? rtw_rf_hostwrite : rtw_rf_macwrite;
		break;
	}

	switch (rfchipid) {
	case RTW_RFCHIPID_GCT:
		rf = rtw_grf5101_create(&sc->sc_regs, rf_write, 0);
		sc->sc_pwrstate_cb = rtw_maxim_pwrstate;
		break;
	case RTW_RFCHIPID_MAXIM:
		rf = rtw_max2820_create(&sc->sc_regs, rf_write, 0);
		sc->sc_pwrstate_cb = rtw_maxim_pwrstate;
		break;
	case RTW_RFCHIPID_PHILIPS:
		rf = rtw_sa2400_create(&sc->sc_regs, rf_write, digphy);
		sc->sc_pwrstate_cb = rtw_philips_pwrstate;
		break;
	case RTW_RFCHIPID_RFMD:
		/* XXX RFMD has no RF constructor */
		sc->sc_pwrstate_cb = rtw_rfmd_pwrstate;
		/*FALLTHROUGH*/
	default:
		return NULL;
	}
	rf->rf_continuous_tx_cb =
	    (rtw_continuous_tx_cb_t)rtw_continuous_tx_enable;
	rf->rf_continuous_tx_arg = (void *)sc;
	return rf;
}

/* Revision C and later use a different PHY delay setting than
 * revisions A and B.
 */
static uint8_t
rtw_check_phydelay(struct rtw_regs *regs, uint32_t old_rcr)
{
#define REVAB (RTW_RCR_MXDMA_UNLIMITED | RTW_RCR_AICV)
#define REVC (REVAB | RTW_RCR_RXFTH_WHOLE)

	uint8_t phydelay = __SHIFTIN(0x6, RTW_PHYDELAY_PHYDELAY);

	RTW_WRITE(regs, RTW_RCR, REVAB);
	RTW_WBW(regs, RTW_RCR, RTW_RCR);
	RTW_WRITE(regs, RTW_RCR, REVC);

	RTW_WBR(regs, RTW_RCR, RTW_RCR);
	if ((RTW_READ(regs, RTW_RCR) & REVC) == REVC)
		phydelay |= RTW_PHYDELAY_REVC_MAGIC;

	RTW_WRITE(regs, RTW_RCR, old_rcr);	/* restore RCR */
	RTW_SYNC(regs, RTW_RCR, RTW_RCR);

	return phydelay;
#undef REVC
}

void
rtw_attach(struct rtw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_txsoft_blk *tsb;
	int pri, rc;

	pmf_self_suspensor_init(sc->sc_dev, &sc->sc_suspensor, &sc->sc_qual);

	rtw_cipher_wep = ieee80211_cipher_wep;
	rtw_cipher_wep.ic_decap = rtw_wep_decap;

	NEXT_ATTACH_STATE(sc, DETACHED);

	switch (RTW_READ(&sc->sc_regs, RTW_TCR) & RTW_TCR_HWVERID_MASK) {
	case RTW_TCR_HWVERID_F:
		sc->sc_hwverid = 'F';
		break;
	case RTW_TCR_HWVERID_D:
		sc->sc_hwverid = 'D';
		break;
	default:
		sc->sc_hwverid = '?';
		break;
	}
	aprint_verbose_dev(sc->sc_dev, "hardware version %c\n",
	    sc->sc_hwverid);

	rc = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct rtw_descs),
	    RTW_DESC_ALIGNMENT, 0, &sc->sc_desc_segs, 1, &sc->sc_desc_nsegs,
	    0);

	if (rc != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate hw descriptors, error %d\n", rc);
		goto err;
	}

	NEXT_ATTACH_STATE(sc, FINISH_DESC_ALLOC);

	rc = bus_dmamem_map(sc->sc_dmat, &sc->sc_desc_segs,
	    sc->sc_desc_nsegs, sizeof(struct rtw_descs),
	    (void **)&sc->sc_descs, BUS_DMA_COHERENT);

	if (rc != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map hw descriptors, error %d\n", rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESC_MAP);

	rc = bus_dmamap_create(sc->sc_dmat, sizeof(struct rtw_descs), 1,
	    sizeof(struct rtw_descs), 0, 0, &sc->sc_desc_dmamap);

	if (rc != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create DMA map for hw descriptors, error %d\n",
		    rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESCMAP_CREATE);

	sc->sc_rxdesc_blk.rdb_dmat = sc->sc_dmat;
	sc->sc_rxdesc_blk.rdb_dmamap = sc->sc_desc_dmamap;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		sc->sc_txdesc_blk[pri].tdb_dmat = sc->sc_dmat;
		sc->sc_txdesc_blk[pri].tdb_dmamap = sc->sc_desc_dmamap;
	}

	rc = bus_dmamap_load(sc->sc_dmat, sc->sc_desc_dmamap, sc->sc_descs,
	    sizeof(struct rtw_descs), NULL, 0);

	if (rc != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load DMA map for hw descriptors, error %d\n",
		    rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESCMAP_LOAD);

	if (rtw_txsoft_blk_setup_all(sc) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_TXCTLBLK_SETUP);

	rtw_txdesc_blk_setup_all(sc);

	NEXT_ATTACH_STATE(sc, FINISH_TXDESCBLK_SETUP);

	sc->sc_rxdesc_blk.rdb_desc = &sc->sc_descs->hd_rx[0];

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		tsb = &sc->sc_txsoft_blk[pri];

		if ((rc = rtw_txdesc_dmamaps_create(sc->sc_dmat,
		    &tsb->tsb_desc[0], tsb->tsb_ndesc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not load DMA map for hw tx descriptors, "
			    "error %d\n", rc);
			goto err;
		}
	}

	NEXT_ATTACH_STATE(sc, FINISH_TXMAPS_CREATE);
	if ((rc = rtw_rxdesc_dmamaps_create(sc->sc_dmat, &sc->sc_rxsoft[0],
	                                    RTW_RXQLEN)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load DMA map for hw rx descriptors, error %d\n",
		    rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_RXMAPS_CREATE);

	/* Reset the chip to a known state. */
	if (rtw_reset(sc) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_RESET);

	sc->sc_rcr = RTW_READ(&sc->sc_regs, RTW_RCR);

	if ((sc->sc_rcr & RTW_RCR_9356SEL) != 0)
		sc->sc_flags |= RTW_F_9356SROM;

	if (rtw_srom_read(&sc->sc_regs, sc->sc_flags, &sc->sc_srom,
	    sc->sc_dev) != 0)
		goto err;

	NEXT_ATTACH_STATE(sc, FINISH_READ_SROM);

	if (rtw_srom_parse(&sc->sc_srom, &sc->sc_flags, &sc->sc_csthr,
	    &sc->sc_rfchipid, &sc->sc_rcr, &sc->sc_locale,
	    sc->sc_dev) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "attach failed, malformed serial ROM\n");
		goto err;
	}

	aprint_verbose_dev(sc->sc_dev, "%s PHY\n",
	    ((sc->sc_flags & RTW_F_DIGPHY) != 0) ? "digital" : "analog");

	aprint_verbose_dev(sc->sc_dev, "carrier-sense threshold %u\n",
	    sc->sc_csthr);

	NEXT_ATTACH_STATE(sc, FINISH_PARSE_SROM);

	sc->sc_rf = rtw_rf_attach(sc, sc->sc_rfchipid,
	    sc->sc_flags & RTW_F_DIGPHY);

	if (sc->sc_rf == NULL) {
		aprint_verbose_dev(sc->sc_dev,
		    "attach failed, could not attach RF\n");
		goto err;
	}

	NEXT_ATTACH_STATE(sc, FINISH_RF_ATTACH);

	sc->sc_phydelay = rtw_check_phydelay(&sc->sc_regs, sc->sc_rcr);

	RTW_DPRINTF(RTW_DEBUG_ATTACH,
	    ("%s: PHY delay %d\n", device_xname(sc->sc_dev), sc->sc_phydelay));

	if (sc->sc_locale == RTW_LOCALE_UNKNOWN)
		rtw_identify_country(&sc->sc_regs, &sc->sc_locale);

	rtw_init_channels(sc->sc_locale, &sc->sc_ic.ic_channels, sc->sc_dev);

	if (rtw_identify_sta(&sc->sc_regs, &sc->sc_ic.ic_myaddr,
	    sc->sc_dev) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_ID_STA);

	rtw_setifprops(ifp, device_xname(sc->sc_dev), (void*)sc);

	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ic.ic_ifp = ifp;
	rtw_set80211props(&sc->sc_ic);

	rtw_led_attach(&sc->sc_led_state, (void *)sc);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ieee80211_ifattach(&sc->sc_ic);

	rtw_set80211methods(&sc->sc_mtbl, &sc->sc_ic);

	/* possibly we should fill in our own sc_send_prresp, since
	 * the RTL8180 is probably sending probe responses in ad hoc
	 * mode.
	 */

	/* complete initialization */
	ieee80211_media_init(&sc->sc_ic, rtw_media_change, rtw_media_status);
	callout_init(&sc->sc_scan_ch, 0);

	rtw_init_radiotap(sc);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_radiobpf);

	NEXT_ATTACH_STATE(sc, FINISHED);

	ieee80211_announce(ic);
	return;
err:
	rtw_detach(sc);
	return;
}

int
rtw_detach(struct rtw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	int pri, s;

	s = splnet();

	switch (sc->sc_attach_state) {
	case FINISHED:
		rtw_stop(ifp, 1);

		pmf_device_deregister(sc->sc_dev);
		callout_stop(&sc->sc_scan_ch);
		ieee80211_ifdetach(&sc->sc_ic);
		if_detach(ifp);
		rtw_led_detach(&sc->sc_led_state);
		/*FALLTHROUGH*/
	case FINISH_ID_STA:
	case FINISH_RF_ATTACH:
		rtw_rf_destroy(sc->sc_rf);
		sc->sc_rf = NULL;
		/*FALLTHROUGH*/
	case FINISH_PARSE_SROM:
	case FINISH_READ_SROM:
		rtw_srom_free(&sc->sc_srom);
		/*FALLTHROUGH*/
	case FINISH_RESET:
	case FINISH_RXMAPS_CREATE:
		rtw_rxdesc_dmamaps_destroy(sc->sc_dmat, &sc->sc_rxsoft[0],
		    RTW_RXQLEN);
		/*FALLTHROUGH*/
	case FINISH_TXMAPS_CREATE:
		for (pri = 0; pri < RTW_NTXPRI; pri++) {
			rtw_txdesc_dmamaps_destroy(sc->sc_dmat,
			    sc->sc_txsoft_blk[pri].tsb_desc,
			    sc->sc_txsoft_blk[pri].tsb_ndesc);
		}
		/*FALLTHROUGH*/
	case FINISH_TXDESCBLK_SETUP:
	case FINISH_TXCTLBLK_SETUP:
		rtw_txsoft_blk_cleanup_all(sc);
		/*FALLTHROUGH*/
	case FINISH_DESCMAP_LOAD:
		bus_dmamap_unload(sc->sc_dmat, sc->sc_desc_dmamap);
		/*FALLTHROUGH*/
	case FINISH_DESCMAP_CREATE:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_desc_dmamap);
		/*FALLTHROUGH*/
	case FINISH_DESC_MAP:
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_descs,
		    sizeof(struct rtw_descs));
		/*FALLTHROUGH*/
	case FINISH_DESC_ALLOC:
		bus_dmamem_free(sc->sc_dmat, &sc->sc_desc_segs,
		    sc->sc_desc_nsegs);
		/*FALLTHROUGH*/
	case DETACHED:
		NEXT_ATTACH_STATE(sc, DETACHED);
		break;
	}
	splx(s);
	return 0;
}
