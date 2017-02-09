/*	$NetBSD: if_rtk_cardbus.c,v 1.48 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 2000 Masanori Kanaoka
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
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * if_rtk_cardbus.c:
 *	Cardbus specific routines for Realtek 8139 ethernet adapter.
 *	Tested for
 *		- elecom-Laneed	LD-10/100CBA (Accton MPX5030)
 *		- MELCO		LPC3-TX-CB   (Realtek 8139)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_rtk_cardbus.c,v 1.48 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of Realtek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#define RTK_USEIOSPACE

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

/*
 * Various supported device vendors/types and their names.
 */
static const struct rtk_type rtk_cardbus_devs[] = {
	{ PCI_VENDOR_ACCTON, PCI_PRODUCT_ACCTON_MPX5030,
		RTK_8139, "Accton MPX 5030/5038 10/100BaseTX" },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DFE690TXD,
		RTK_8139, "D-Link DFE-690TXD 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8138,
		RTK_8139, "Realtek 8138 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8139,
		RTK_8139, "Realtek 8139 10/100BaseTX" },
	{ PCI_VENDOR_COREGA, PCI_PRODUCT_COREGA_CB_TXD,
		RTK_8139, "Corega FEther CB-TXD 10/100BaseTX" },
	{ PCI_VENDOR_COREGA, PCI_PRODUCT_COREGA_2CB_TXD,
		RTK_8139, "Corega FEther II CB-TXD 10/100BaseTX" },
	{ PCI_VENDOR_PLANEX, PCI_PRODUCT_PLANEX_FNW_3603_TX,
		RTK_8139, "Planex FNW-3603 10/100BaseTX" },
	{ PCI_VENDOR_PLANEX, PCI_PRODUCT_PLANEX_FNW_3800_TX,
		RTK_8139, "Planex 10/100BaseTX FNW-3800-TX" },
	{ PCI_VENDOR_ABOCOM, PCI_PRODUCT_ABOCOM_FE2000VX,
		RTK_8139, "AboCom FE2000VX 10/100BaseTX" },

	{ 0, 0, 0, NULL }
};

static int rtk_cardbus_match(device_t, cfdata_t, void *);
static void rtk_cardbus_attach(device_t, device_t, void *);
static int rtk_cardbus_detach(device_t, int);

struct rtk_cardbus_softc {
	struct rtk_softc sc_rtk;	/* real rtk softc */

	/* CardBus-specific goo. */
	void *sc_ih;
	cardbus_devfunc_t sc_ct;
	pcitag_t sc_tag;
	pcireg_t sc_csr;
	int sc_bar_reg;
	pcireg_t sc_bar_val;
};

CFATTACH_DECL_NEW(rtk_cardbus, sizeof(struct rtk_cardbus_softc),
    rtk_cardbus_match, rtk_cardbus_attach, rtk_cardbus_detach, rtk_activate);

const struct rtk_type *rtk_cardbus_lookup(const struct cardbus_attach_args *);

void rtk_cardbus_setup(struct rtk_cardbus_softc *);

int rtk_cardbus_enable(struct rtk_softc *);
void rtk_cardbus_disable(struct rtk_softc *);
void rtk_cardbus_power(struct rtk_softc *, int);

const struct rtk_type *
rtk_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct rtk_type *t;

	for (t = rtk_cardbus_devs; t->rtk_name != NULL; t++){
		if (PCI_VENDOR(ca->ca_id) == t->rtk_vid &&
		    PCI_PRODUCT(ca->ca_id) == t->rtk_did) {
			return t;
		}
	}
	return NULL;
}

int
rtk_cardbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (rtk_cardbus_lookup(ca) != NULL)
		return 1;

	return 0;
}


void
rtk_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct rtk_cardbus_softc *csc = device_private(self);
	struct rtk_softc *sc = &csc->sc_rtk;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	const struct rtk_type *t;
	bus_addr_t adr;

	sc->sc_dev = self;
	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	t = rtk_cardbus_lookup(ca);
	if (t == NULL) {
		aprint_error("\n");
		panic("%s: impossible", __func__);
	 }
	aprint_normal(": %s\n", t->rtk_name);

	/*
	 * Power management hooks.
	 */
	sc->sc_enable = rtk_cardbus_enable;
	sc->sc_disable = rtk_cardbus_disable;

	/*
	 * Map control/status registers.
	 */
	csc->sc_csr = PCI_COMMAND_MASTER_ENABLE;
#ifdef RTK_USEIOSPACE
	if (Cardbus_mapreg_map(ct, RTK_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->rtk_btag, &sc->rtk_bhandle, &adr, &sc->rtk_bsize) == 0) {
		csc->sc_csr |= PCI_COMMAND_IO_ENABLE;
		csc->sc_bar_reg = RTK_PCI_LOIO;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_IO;
	}
#else
	if (Cardbus_mapreg_map(ct, RTK_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->rtk_btag, &sc->rtk_bhandle, &adr, &sc->rtk_bsize) == 0) {
		csc->sc_csr |= PCI_COMMAND_MEM_ENABLE;
		csc->sc_bar_reg = RTK_PCI_LOMEM;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_MEM;
	}
#endif
	else {
		aprint_error_dev(self, " unable to map deviceregisters\n");
		return;
	}
	/*
	 * Handle power management nonsense and initialize the
	 * configuration registers.
	 */
	rtk_cardbus_setup(csc);

	rtk_attach(sc);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->ethercom.ec_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * Power down the socket.
	 */
	Cardbus_function_disable(csc->sc_ct);
}

int
rtk_cardbus_detach(device_t self, int flags)
{
	struct rtk_cardbus_softc *csc = device_private(self);
	struct rtk_softc *sc = &csc->sc_rtk;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#ifdef DIAGNOSTIC
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(self));
#endif
	rv = rtk_detach(sc);
	if (rv)
		return rv;
	/*
	 * Unhook the interrupt handler.
	 */
	if (csc->sc_ih != NULL)
		Cardbus_intr_disestablish(ct, csc->sc_ih);

	/*
	 * Release bus space and close window.
	 */
	if (csc->sc_bar_reg != 0)
		Cardbus_mapreg_unmap(ct, csc->sc_bar_reg,
		    sc->rtk_btag, sc->rtk_bhandle, sc->rtk_bsize);

	return 0;
}

void
rtk_cardbus_setup(struct rtk_cardbus_softc *csc)
{
	struct rtk_softc *sc = &csc->sc_rtk;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg, command;
	int pmreg;

	/*
	 * Handle power management nonsense.
	 */
	if (cardbus_get_capability(cc, cf, csc->sc_tag,
	    PCI_CAP_PWRMGMT, &pmreg, 0)) {
		command = Cardbus_conf_read(ct, csc->sc_tag,
		    pmreg + PCI_PMCSR);
		if (command & PCI_PMCSR_STATE_MASK) {
			pcireg_t iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = Cardbus_conf_read(ct, csc->sc_tag,
			    RTK_PCI_LOIO);
			membase = Cardbus_conf_read(ct, csc->sc_tag,
			    RTK_PCI_LOMEM);
			irq = Cardbus_conf_read(ct, csc->sc_tag,
			    PCI_INTERRUPT_REG);

			/* Reset the power state. */
			aprint_normal_dev(sc->sc_dev,
			    "chip is in D%d power mode -- setting to D0\n",
			    command & PCI_PMCSR_STATE_MASK);
			command &= ~PCI_PMCSR_STATE_MASK;
			Cardbus_conf_write(ct, csc->sc_tag,
			    pmreg + PCI_PMCSR, command);

			/* Restore PCI config data. */
			Cardbus_conf_write(ct, csc->sc_tag,
			    RTK_PCI_LOIO, iobase);
			Cardbus_conf_write(ct, csc->sc_tag,
			    RTK_PCI_LOMEM, membase);
			Cardbus_conf_write(ct, csc->sc_tag,
			    PCI_INTERRUPT_REG, irq);
		}
	}

	/* Program the BAR */
	Cardbus_conf_write(ct, csc->sc_tag, csc->sc_bar_reg, csc->sc_bar_val);

	/* Enable the appropriate bits in the CARDBUS CSR. */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	reg |= csc->sc_csr;
	Cardbus_conf_write(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Make sure the latency timer is set to some reasonable
	 * value.
	 */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(reg) < 0x20) {
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (0x20 << PCI_LATTIMER_SHIFT);
		Cardbus_conf_write(ct, csc->sc_tag, PCI_BHLC_REG, reg);
	}
}

int
rtk_cardbus_enable(struct rtk_softc *sc)
{
	struct rtk_cardbus_softc *csc = (struct rtk_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/*
	 * Power on the socket.
	 */
	Cardbus_function_enable(ct);

	/*
	 * Set up the PCI configuration registers.
	 */
	rtk_cardbus_setup(csc);

	/*
	 * Map and establish the interrupt.
	 */
	csc->sc_ih = Cardbus_intr_establish(ct, IPL_NET, rtk_intr, sc);
	if (csc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to establish interrupt\n");
		Cardbus_function_disable(csc->sc_ct);
		return 1;
	}
	return 0;
}

void
rtk_cardbus_disable(struct rtk_softc *sc)
{
	struct rtk_cardbus_softc *csc = (struct rtk_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/* Unhook the interrupt handler. */
	Cardbus_intr_disestablish(ct, csc->sc_ih);
	csc->sc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}
