/*	$NetBSD: iwic_pci.c,v 1.19 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Jan 16 10:53:03 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iwic_pci.c,v 1.19 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <net/if.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <netisdn/i4b_global.h>

#include <dev/pci/iwicreg.h>
#include <dev/pci/iwicvar.h>

/* Winbond PCI Configuration Space */

#define IWIC_PCI_IOBA (PCI_MAPREG_START+0x04)

static int iwic_pci_intr(void *sc);
static int iwic_pci_probe(device_t  dev, cfdata_t  match, void *aux);
static void iwic_pci_attach(device_t  parent, device_t  dev, void *aux);
static int iwic_pci_activate(device_t  dev, enum devact);

CFATTACH_DECL_NEW(iwic_pci, sizeof(struct iwic_softc),
    iwic_pci_probe, iwic_pci_attach, NULL, iwic_pci_activate);

static int iwic_attach_bri(struct iwic_softc * sc);

/*---------------------------------------------------------------------------*
 * PCI ID list for ASUSCOM card got from Asuscom in March 2000:
 *
 * Vendor ID: 0675 Device ID: 1702
 * Vendor ID: 0675 Device ID: 1703
 * Vendor ID: 0675 Device ID: 1707
 * Vendor ID: 10CF Device ID: 105E
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 2000
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 1702
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 1707
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 1043 SubDevice ID: 1702
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 1043 SubDevice ID: 1707
 * Vendor ID: 1050 Device ID: 6692 SubVendor: 0675 SubDevice ID: 1702
 *---------------------------------------------------------------------------*/

static struct winids {
	pcireg_t type;
	int sv;
	int sd;
	const char *desc;
} win_ids[] = {
	{
		PCI_ID_CODE(PCI_VENDOR_WINBOND,PCI_PRODUCT_WINBOND_W6692),
		0x144F,
		0x1707,
		"Planet PCI ISDN Adapter (Model IA128P-STDV)"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_WINBOND,PCI_PRODUCT_WINBOND_W6692),
		-1,
		-1,
		"Generic Winbond W6692 ISDN PCI"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_DYNALINK,PCI_PRODUCT_DYNALINK_IS64PH),
		-1,
		-1,
		"ASUSCOM P-IN100-ST-D"
	},
#if 0
	/* XXX */
	{
		PCI_ID_CODE(PCI_VENDOR_DYNALINK,0x1703),
		-1,
		-1,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_DYNALINK,0x1707),
		-1,
		-1,
		"ASUSCOM P-IN100-ST-D"
	},
	{
#ifndef PCI_VENDOR_CITICORP
#define PCI_VENDOR_CITICORP	PCI_VENDOR_FUJITSU4
#endif
		PCI_ID_CODE(PCI_VENDOR_CITICORP,0x105E),
		-1,
		-1,
		"ASUSCOM P-IN100-ST-D"
	},
#endif
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x2000,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x1702,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x1707,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x1443,
		0x1702,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x1443,
		0x1707,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x2000,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x2000,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		PCI_ID_CODE(PCI_VENDOR_ASUSTEK,PCI_PRODUCT_ASUSTEK_HFCPCI),
		0x144F,
		0x2000,
		"ASUSCOM P-IN100-ST-D"
	},
	{
		0x00000000,
		0,
		0,
		NULL
	}
};

static const char *
iwic_find_card(const struct pci_attach_args * pa)
{
	pcireg_t type = pa->pa_id;
	pcireg_t reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	int sv = PCI_VENDOR(reg);
	int sd = PCI_PRODUCT(reg);
	struct winids *wip = win_ids;

	while (wip->type) {
		if (wip->type == type) {
			if (((wip->sv == -1) && (wip->sd == -1)) ||
			    ((wip->sv == sv) && (wip->sd == sd)))
				break;
		}
		++wip;
	}

	if (wip->desc)
		return wip->desc;

	return 0;
}
/*---------------------------------------------------------------------------*
 *	iwic PCI probe
 *---------------------------------------------------------------------------*/
static int
iwic_pci_probe(device_t  dev,
	cfdata_t  match, void *aux)
{
	if (iwic_find_card(aux))
		return 1;

	return 0;
}
/*---------------------------------------------------------------------------*
 *	PCI attach
 *---------------------------------------------------------------------------*/
static void
iwic_pci_attach(device_t  parent, device_t  dev, void *aux)
{
	struct iwic_softc *sc = device_private(dev);
	struct iwic_bchan *bchan;
	pci_intr_handle_t ih;
	const char *intrstr;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = dev;
	sc->sc_cardname = iwic_find_card(pa);

	if (!sc->sc_cardname)
		return;		/* Huh? */

	printf(": %s\n", sc->sc_cardname);

	if (pci_mapreg_map(pa, IWIC_PCI_IOBA, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_io_bt, &sc->sc_io_bh, &sc->sc_iobase, &sc->sc_iosize)) {
		aprint_error_dev(sc->sc_dev, "unable to map registers\n");
		return;
	}
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, iwic_pci_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	sc->sc_pc = pc;
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* disable interrupts */
	IWIC_WRITE(sc, IWIC_IMASK, 0xff);
	IWIC_READ(sc, ISTA);

	iwic_dchan_init(sc);

	bchan = &sc->sc_bchan[IWIC_BCH_A];
	bchan->offset = B1_CHAN_OFFSET;
	bchan->channel = IWIC_BCH_A;
	bchan->state = ST_IDLE;

	iwic_bchannel_setup(sc, IWIC_BCH_A, BPROT_NONE, 0);

	bchan = &sc->sc_bchan[IWIC_BCH_B];
	bchan->offset = B2_CHAN_OFFSET;
	bchan->channel = IWIC_BCH_B;
	bchan->state = ST_IDLE;

	iwic_bchannel_setup(sc, IWIC_BCH_B, BPROT_NONE, 0);

	iwic_init_linktab(sc);

	iwic_attach_bri(sc);
}
/*---------------------------------------------------------------------------*
 *	IRQ handler
 *---------------------------------------------------------------------------*/
static int
iwic_pci_intr(void *p)
{
	struct iwic_softc *sc = p;

	while (1) {
		int irq_stat = IWIC_READ(sc, ISTA);

		if (irq_stat == 0)
			break;

		if (irq_stat & (ISTA_D_RME | ISTA_D_RMR | ISTA_D_XFR)) {
			iwic_dchan_xfer_irq(sc, irq_stat);
		}
		if (irq_stat & ISTA_D_EXI) {
			iwic_dchan_xirq(sc);
		}
		if (irq_stat & ISTA_B1_EXI) {
			iwic_bchan_xirq(sc, 0);
		}
		if (irq_stat & ISTA_B2_EXI) {
			iwic_bchan_xirq(sc, 1);
		}
	}

	return 0;
}

static int
iwic_pci_activate(device_t  dev, enum devact act)
{
	int error = EOPNOTSUPP;

	return error;
}

int iwic_ph_data_req(isdn_layer1token t, struct mbuf * m, int freeflag);
int iwic_ph_activate_req(isdn_layer1token t);
int iwic_mph_command_req(isdn_layer1token t, int command, void *parm);

struct isdn_layer1_isdnif_driver iwic_bri_driver = {
	iwic_ph_data_req,
	iwic_ph_activate_req,
	iwic_mph_command_req
};

void iwic_set_link(void *, int channel, const struct isdn_l4_driver_functions * l4_driver, void *l4_driver_softc);
isdn_link_t *iwic_ret_linktab(void *, int channel);

/* XXX Should be prototyped in some header, not here XXX */
void n_connect_request(struct call_desc * cd);
void n_connect_response(struct call_desc * cd, int response, int cause);
void n_disconnect_request(struct call_desc * cd, int cause);
void n_alert_request(struct call_desc * cd);
void n_mgmt_command(struct isdn_l3_driver * drv, int cmd, void *parm);

const struct isdn_l3_driver_functions iwic_l3_driver = {
	iwic_ret_linktab,
	iwic_set_link,
	n_connect_request,
	n_connect_response,
	n_disconnect_request,
	n_alert_request,
	NULL,
	NULL,
	n_mgmt_command
};

static int
iwic_attach_bri(struct iwic_softc * sc)
{
	struct isdn_l3_driver *drv;

	drv = isdn_attach_isdnif(device_xname(sc->sc_dev), sc->sc_cardname,
	    &sc->sc_l2, &iwic_l3_driver, NBCH_BRI);

	sc->sc_l3token = drv;
	sc->sc_l2.driver = &iwic_bri_driver;
	sc->sc_l2.l1_token = sc;
	sc->sc_l2.drv = drv;

	isdn_layer2_status_ind(&sc->sc_l2, drv, STI_ATTACH, 1);

	isdn_isdnif_ready(drv->isdnif);

	return 1;
}

int
iwic_ph_data_req(isdn_layer1token t, struct mbuf * m, int freeflag)
{
	struct iwic_softc *sc = t;

	return iwic_dchan_data_req(sc, m, freeflag);
}

int
iwic_ph_activate_req(isdn_layer1token t)
{
	struct iwic_softc *sc = t;

	iwic_next_state(sc, EV_PHAR);

	return 0;
}

int
iwic_mph_command_req(isdn_layer1token t, int command, void *parm)
{
	struct iwic_softc *sc = t;

	switch (command) {
	case CMR_DOPEN:	/* Daemon running */
		NDBGL1(L1_PRIM, "CMR_DOPEN");
		IWIC_WRITE(sc, IWIC_IMASK, IMASK_XINT0 | IMASK_XINT1);
		break;

	case CMR_DCLOSE:	/* Daemon not running */
		NDBGL1(L1_PRIM, "CMR_DCLOSE");
		IWIC_WRITE(sc, IWIC_IMASK, 0xff);
		break;

	case CMR_SETTRACE:
		sc->sc_trace = (int)(intptr_t)parm;
		NDBGL1(L1_PRIM, "CMR_SETTRACE, parm = %d", sc->sc_trace);
		break;

	default:
		NDBGL1(L1_PRIM, "unknown command = %d", command);
		break;
	}

	return 0;
}

isdn_link_t *
iwic_ret_linktab(void *t, int channel)
{
	struct l2_softc *l2sc = t;
	struct iwic_softc *sc = l2sc->l1_token;
	struct iwic_bchan *bchan = &sc->sc_bchan[channel];

	return &bchan->iwic_isdn_linktab;
}

void
iwic_set_link(void *t, int channel, const struct isdn_l4_driver_functions * l4_driver, void *l4_driver_softc)
{
	struct l2_softc *l2sc = t;
	struct iwic_softc *sc = l2sc->l1_token;
	struct iwic_bchan *bchan = &sc->sc_bchan[channel];

	bchan->l4_driver = l4_driver;
	bchan->l4_driver_softc = l4_driver_softc;
}
