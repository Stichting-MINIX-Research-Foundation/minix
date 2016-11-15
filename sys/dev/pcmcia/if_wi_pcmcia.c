/* $NetBSD: if_wi_pcmcia.c,v 1.90 2013/03/30 03:22:14 christos Exp $ */

/*-
 * Copyright (c) 2001, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org), and by Charles M. Hannum.
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
 * PCMCIA attachment for Lucent & Intersil WaveLAN PCMCIA card
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_wi_pcmcia.c,v 1.90 2013/03/30 03:22:14 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_rssadapt.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/wi_ieee.h>
#include <dev/ic/wireg.h>
#include <dev/ic/wivar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#ifdef _MODULE
#define WI_PCMCIA_SPECTRUM24T_FW 1
#else
#include <opt_if_wi_pcmcia.h>
#endif

#if WI_PCMCIA_SPECTRUM24T_FW
#include <dev/microcode/wi/spectrum24t_cf.h>
#endif

static int	wi_pcmcia_match(device_t, cfdata_t, void *);
static int	wi_pcmcia_validate_config(struct pcmcia_config_entry *);
static void	wi_pcmcia_attach(device_t, device_t, void *);
static int	wi_pcmcia_detach(device_t, int);
static int	wi_pcmcia_enable(device_t, int);

#if WI_PCMCIA_SPECTRUM24T_FW
/* support to download firmware for symbol CF card */
static int	wi_pcmcia_load_firm(struct wi_softc *, const void *, int, const void *, int);
static int	wi_pcmcia_write_firm(struct wi_softc *, const void *, int, const void *, int);
static int	wi_pcmcia_set_hcr(struct wi_softc *, int);
#endif

struct wi_pcmcia_softc {
	struct wi_softc sc_wi;

	int sc_symbol_cf;			/* Spectrum24t CF card */

	struct pcmcia_function *sc_pf;		/* PCMCIA function */
	int sc_state;
#define	WI_PCMCIA_ATTACHED	3
};

CFATTACH_DECL_NEW(wi_pcmcia, sizeof(struct wi_pcmcia_softc),
    wi_pcmcia_match, wi_pcmcia_attach, wi_pcmcia_detach, wi_activate);

static const struct pcmcia_product wi_pcmcia_products[] = {
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_SMC_2632W },

	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_NANOSPEED_PRISM2 },

	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_NEC_CMZ_RT_WP },

	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_NTT_ME_WLAN },

	{ PCMCIA_VENDOR_LUCENT, PCMCIA_PRODUCT_LUCENT_HERMES,
	  PCMCIA_CIS_INVALID },

	{ PCMCIA_VENDOR_LUCENT, PCMCIA_PRODUCT_LUCENT_HERMES2,
	  PCMCIA_CIS_INVALID },

	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CRWE737A,
	  PCMCIA_CIS_3COM_3CRWE737A },

	{ PCMCIA_VENDOR_ALVARION,
	  PCMCIA_PRODUCT_ALVARION_BREEZENET,
	  PCMCIA_CIS_ALVARION_BREEZENET },

	{ PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCC_11,
	  PCMCIA_CIS_COREGA_WIRELESS_LAN_PCC_11 },

	{ PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCA_11,
	  PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCA_11 },

	{ PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCB_11,
	  PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCB_11 },

	{ PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCL_11,
	  PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCL_11 },

	{ PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_WLCFL_11,
	  PCMCIA_CIS_COREGA_WIRELESS_LAN_WLCFL_11 },

	{ PCMCIA_VENDOR_INTEL, PCMCIA_PRODUCT_INTEL_PRO_WLAN_2011,
	  PCMCIA_CIS_INTEL_PRO_WLAN_2011 },

	{ PCMCIA_VENDOR_INTERSIL2, PCMCIA_PRODUCT_INTERSIL2_PRISM2,
	  PCMCIA_CIS_INTERSIL2_PRISM2 },

	{ PCMCIA_VENDOR_INTERSIL2, PCMCIA_PRODUCT_GEMTEK_WLAN,
	  PCMCIA_CIS_GEMTEK_WLAN },

	{ PCMCIA_VENDOR_SAMSUNG, PCMCIA_PRODUCT_SAMSUNG_SWL_2000N,
	  PCMCIA_CIS_SAMSUNG_SWL_2000N },

	{ PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI300_IEEE,
	  PCMCIA_CIS_ELSA_XI300_IEEE },

	{ PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI325_IEEE,
	  PCMCIA_CIS_ELSA_XI325_IEEE },

	{ PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI800_IEEE,
	  PCMCIA_CIS_ELSA_XI800_IEEE },

	{ PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_SMC2531WB,
	  PCMCIA_CIS_ELSA_SMC2531WB },

	{ PCMCIA_VENDOR_COMPAQ, PCMCIA_PRODUCT_COMPAQ_NC5004,
	  PCMCIA_CIS_COMPAQ_NC5004 },

	{ PCMCIA_VENDOR_CONTEC, PCMCIA_PRODUCT_CONTEC_FX_DS110_PCC,
	  PCMCIA_CIS_CONTEC_FX_DS110_PCC },

	{ PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CD011WL,
	  PCMCIA_CIS_TDK_LAK_CD011WL },

	{ PCMCIA_VENDOR_IODATA2, PCMCIA_PRODUCT_IODATA2_WNB11PCM,
	  PCMCIA_CIS_IODATA2_WNB11PCM },

	{ PCMCIA_VENDOR_IODATA2, PCMCIA_PRODUCT_IODATA2_WCF12,
	  PCMCIA_CIS_IODATA2_WCF12 },

	{ PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_WLI_PCM_S11,
	  PCMCIA_CIS_BUFFALO_WLI_PCM_S11 },

	{ PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_WLI_CF_S11G,
	  PCMCIA_CIS_BUFFALO_WLI_CF_S11G },

	{ PCMCIA_VENDOR_EMTAC, PCMCIA_PRODUCT_EMTAC_WLAN,
	  PCMCIA_CIS_EMTAC_WLAN },

	{ PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37100P,
	  PCMCIA_CIS_INTERSIL_ISL37100P },

	{ PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37110P,
	  PCMCIA_CIS_INTERSIL_ISL37110P },

	{ PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37300P,
	  PCMCIA_CIS_INTERSIL_ISL37300P },

	{ PCMCIA_VENDOR_SIMPLETECH, PCMCIA_PRODUCT_SIMPLETECH_SPECTRUM24,
	  PCMCIA_CIS_SIMPLETECH_SPECTRUM24 },

	{ PCMCIA_VENDOR_ERICSSON, PCMCIA_PRODUCT_ERICSSON_WIRELESSLAN,
	  PCMCIA_CIS_ERICSSON_WIRELESSLAN },

	{ PCMCIA_VENDOR_SYMBOL, PCMCIA_PRODUCT_SYMBOL_LA4100,
	  PCMCIA_CIS_SYMBOL_LA4100 },

	{ PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_IWN,
	  PCMCIA_CIS_NANOSPEED_PRISM2 },

	{ PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_IWN3,
	  PCMCIA_CIS_LINKSYS2_IWN3 },

	{ PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_WCF11,
	  PCMCIA_CIS_LINKSYS2_WCF11 },

	{ PCMCIA_VENDOR_MICROSOFT, PCMCIA_PRODUCT_MICROSOFT_MN_520,
	  PCMCIA_CIS_MICROSOFT_MN_520 },

	{ PCMCIA_VENDOR_PLANEX, PCMCIA_PRODUCT_PLANEX_GWNS11H,
	  PCMCIA_CIS_PLANEX_GWNS11H },

	{ PCMCIA_VENDOR_BAY, PCMCIA_PRODUCT_BAY_EMOBILITY_11B,
	  PCMCIA_CIS_BAY_EMOBILITY_11B },

	{ PCMCIA_VENDOR_ACTIONTEC, PCMCIA_PRODUCT_ACTIONTEC_PRISM,
	  PCMCIA_CIS_ACTIONTEC_PRISM },

	{ PCMCIA_VENDOR_DLINK_2, PCMCIA_PRODUCT_DLINK_DWL650H,
	  PCMCIA_CIS_DLINK_DWL650H },

	{ PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_WL110,
	  PCMCIA_CIS_FUJITSU_WL110 },

	{ PCMCIA_VENDOR_ARTEM, PCMCIA_PRODUCT_ARTEM_ONAIR,
	  PCMCIA_CIS_ARTEM_ONAIR },

	{ PCMCIA_VENDOR_ASUSTEK, PCMCIA_PRODUCT_ASUSTEK_WL_100,
	  PCMCIA_CIS_ASUSTEK_WL_100 },

	{ PCMCIA_VENDOR_PROXIM, PCMCIA_PRODUCT_PROXIM_RANGELANDS_8430,
	  PCMCIA_CIS_PROXIM_RANGELANDS_8430 },

	{ PCMCIA_VENDOR_SIEMENS, PCMCIA_PRODUCT_SIEMENS_SS1021,
	  PCMCIA_CIS_SIEMENS_SS1021 },
};
static const size_t wi_pcmcia_nproducts =
    sizeof(wi_pcmcia_products) / sizeof(wi_pcmcia_products[0]);

static int
wi_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, wi_pcmcia_products, wi_pcmcia_nproducts,
	    sizeof(wi_pcmcia_products[0]), NULL))
		return 1;
	return 0;
}

static int
wi_pcmcia_enable(device_t self, int onoff)
{
	struct wi_pcmcia_softc *psc = device_private(self);
	struct wi_softc *sc = &psc->sc_wi;
	struct pcmcia_function *pf = psc->sc_pf;
	int error;

	if (onoff) {
		/* establish the interrupt. */
		sc->sc_ih = pcmcia_intr_establish(pf, IPL_NET, wi_intr, sc);
		if (sc->sc_ih == NULL)
			return EIO;

		error = pcmcia_function_enable(pf);
		if (error) {
			pcmcia_intr_disestablish(pf, sc->sc_ih);
			sc->sc_ih = NULL;
			return EIO;
		}

		if (psc->sc_symbol_cf) {
#if WI_PCMCIA_SPECTRUM24T_FW
			if (wi_pcmcia_load_firm(sc,
			    spectrum24t_primsym, sizeof(spectrum24t_primsym),
			    spectrum24t_secsym, sizeof(spectrum24t_secsym))) {
				aprint_error_dev(sc->sc_dev,
				    "couldn't load firmware\n");
				wi_pcmcia_enable(self, 0);
				return EIO;
			}
#else
			aprint_error_dev(sc->sc_dev,
			    "firmware load not configured\n");
			return EIO;
#endif
		}
		DELAY(1000);
	} else {
		pcmcia_function_disable(psc->sc_pf);
		if (sc->sc_ih != NULL) {
			pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
			sc->sc_ih = NULL;
		}
	}

	return 0;
}

static int
wi_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace != 1 ||
	    cfe->iospace[0].length < WI_IOSIZE)
		return EINVAL;
	cfe->num_memspace = 0;
	return 0;
}

static void
wi_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct wi_pcmcia_softc *psc = device_private(self);
	struct wi_softc *sc = &psc->sc_wi;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int haveaddr;
	int error;

	aprint_naive("\n");

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, wi_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_iot = cfe->iospace[0].handle.iot;
	sc->sc_ioh = cfe->iospace[0].handle.ioh;

	if (pa->manufacturer == PCMCIA_VENDOR_SYMBOL &&
	    pa->product == PCMCIA_PRODUCT_SYMBOL_LA4100)
		psc->sc_symbol_cf = 1;
	/*
	 * XXX: Sony PEGA-WL100 CF card has a same vendor/product id as
	 *	Intel PCMCIA card.  It may be incorrect to detect by the
	 *	initial value of COR register.
	 */
	if (pa->manufacturer == PCMCIA_VENDOR_INTEL &&
	    pa->product == PCMCIA_PRODUCT_INTEL_PRO_WLAN_2011 &&
	    CSR_READ_2(sc, WI_COR) == WI_COR_IOMODE)
		psc->sc_symbol_cf = 1;

	error = wi_pcmcia_enable(self, 1);
	if (error)
		goto fail;

	sc->sc_pci = 0;
	sc->sc_enable = wi_pcmcia_enable;

	printf("%s:", device_xname(self));

	haveaddr = pa->pf->pf_funce_lan_nidlen == IEEE80211_ADDR_LEN;
	if (wi_attach(sc, haveaddr ? pa->pf->pf_funce_lan_nid : 0) != 0) {
		aprint_error_dev(self, "failed to attach controller\n");
		goto fail2;
	}

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	wi_pcmcia_enable(self, 0);
	psc->sc_state = WI_PCMCIA_ATTACHED;
	return;

fail2:
	wi_pcmcia_enable(self, 0);
fail:
	pcmcia_function_unconfigure(pa->pf);
}

static int
wi_pcmcia_detach(device_t self, int flags)
{
	struct wi_pcmcia_softc *psc = device_private(self);
	struct wi_softc *sc = &psc->sc_wi;
	int error;

	if (psc->sc_state != WI_PCMCIA_ATTACHED)
		return 0;

	error = wi_detach(&psc->sc_wi);
	if (error != 0)
		return error;

	if (sc->sc_ih != NULL)
		pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);

	pcmcia_function_unconfigure(psc->sc_pf);

	return 0;
}

/*
 * Special routines to download firmware for Symbol CF card.
 * XXX: This should be modified generic into any PRISM-2 based card.
 */

#define	WI_SBCF_PDIADDR		0x3100

/* unaligned load little endian */
#define	GETLE32(p)	((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24))
#define	GETLE16(p)	((p)[0] | ((p)[1]<<8))

#if WI_PCMCIA_SPECTRUM24T_FW
static int
wi_pcmcia_load_firm(struct wi_softc *sc, const void *primsym, int primlen,
    const void *secsym, int seclen)
{
	u_int8_t ebuf[256];
	int i;

	/* load primary code and run it */
	wi_pcmcia_set_hcr(sc, WI_HCR_EEHOLD);
	if (wi_pcmcia_write_firm(sc, primsym, primlen, NULL, 0))
		return EIO;
	wi_pcmcia_set_hcr(sc, WI_HCR_RUN);
	for (i = 0; ; i++) {
		if (i == 10)
			return ETIMEDOUT;
		tsleep(sc, PWAIT, "wiinit", 1);
		if (CSR_READ_2(sc, WI_CNTL) == WI_CNTL_AUX_ENA_STAT)
			break;
		/* write the magic key value to unlock aux port */
		CSR_WRITE_2(sc, WI_PARAM0, WI_AUX_KEY0);
		CSR_WRITE_2(sc, WI_PARAM1, WI_AUX_KEY1);
		CSR_WRITE_2(sc, WI_PARAM2, WI_AUX_KEY2);
		CSR_WRITE_2(sc, WI_CNTL, WI_CNTL_AUX_ENA_CNTL);
	}

	/* issue read EEPROM command: XXX copied from wi_cmd() */
	CSR_WRITE_2(sc, WI_PARAM0, 0);
	CSR_WRITE_2(sc, WI_PARAM1, 0);
	CSR_WRITE_2(sc, WI_PARAM2, 0);
	CSR_WRITE_2(sc, WI_COMMAND, WI_CMD_READEE);
        for (i = 0; i < WI_TIMEOUT; i++) {
                if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD)
                        break;
                DELAY(1);
        }
        CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);

	CSR_WRITE_2(sc, WI_AUX_PAGE, WI_SBCF_PDIADDR / WI_AUX_PGSZ);
	CSR_WRITE_2(sc, WI_AUX_OFFSET, WI_SBCF_PDIADDR % WI_AUX_PGSZ);
	CSR_READ_MULTI_STREAM_2(sc, WI_AUX_DATA,
	    (u_int16_t *)ebuf, sizeof(ebuf) / 2);
	if (GETLE16(ebuf) > sizeof(ebuf))
		return EIO;
	if (wi_pcmcia_write_firm(sc, secsym, seclen, ebuf + 4, GETLE16(ebuf)))
		return EIO;
	return 0;
}

static int
wi_pcmcia_write_firm(struct wi_softc *sc, const void *buf, int buflen,
    const void *ebuf, int ebuflen)
{
	const u_int8_t *p, *ep, *q, *eq;
	char *endp;
	u_int32_t addr, id, eid;
	int i, len, elen, nblk, pdrlen;

	/*
	 * Parse the header of the firmware image.
	 */
	p = buf;
	ep = p + buflen;
	while (p < ep && *p++ != ' ');	/* FILE: */
	while (p < ep && *p++ != ' ');	/* filename */
	while (p < ep && *p++ != ' ');	/* type of the firmware */
	nblk = strtoul(p, &endp, 10);
	p = (void *)endp;
	pdrlen = strtoul(p + 1, &endp, 10);
	p = (void *)endp;
	while (p < ep && *p++ != 0x1a);	/* skip rest of header */

	/*
	 * Block records: address[4], length[2], data[length];
	 */
	for (i = 0; i < nblk; i++) {
		addr = GETLE32(p);	p += 4;
		len  = GETLE16(p);	p += 2;
		CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
		CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
		CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
		    (const u_int16_t *)p, len / 2);
		p += len;
	}

	/*
	 * PDR: id[4], address[4], length[4];
	 */
	for (i = 0; i < pdrlen; ) {
		id   = GETLE32(p);	p += 4; i += 4;
		addr = GETLE32(p);	p += 4; i += 4;
		len  = GETLE32(p);	p += 4; i += 4;
		/* replace PDR entry with the values from EEPROM, if any */
		for (q = ebuf, eq = q + ebuflen; q < eq; q += elen * 2) {
			elen = GETLE16(q);	q += 2;
			eid  = GETLE16(q);	q += 2;
			elen--;		/* elen includes eid */
			if (eid == 0)
				break;
			if (eid != id)
				continue;
			CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
			CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
			CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
			    (const u_int16_t *)q, len / 2);
			break;
		}
	}
	return 0;
}

static int
wi_pcmcia_set_hcr(struct wi_softc *sc, int mode)
{
	u_int16_t hcr;

	CSR_WRITE_2(sc, WI_COR, WI_COR_RESET);
	tsleep(sc, PWAIT, "wiinit", 1);
	hcr = CSR_READ_2(sc, WI_HCR);
	hcr = (hcr & WI_HCR_4WIRE) | (mode & ~WI_HCR_4WIRE);
	CSR_WRITE_2(sc, WI_HCR, hcr);
	tsleep(sc, PWAIT, "wiinit", 1);
	CSR_WRITE_2(sc, WI_COR, WI_COR_IOMODE);
	tsleep(sc, PWAIT, "wiinit", 1);
	return 0;
}
#endif
