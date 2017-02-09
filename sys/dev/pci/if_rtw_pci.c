/* $NetBSD: if_rtw_pci.c,v 1.23 2014/03/29 19:28:25 christos Exp $ */

/*-
 * Copyright (c) 2004, 2005, 2010 David Young.  All rights reserved.
 *
 * Adapted for the RTL8180 by David Young.
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
/*-
 * Copyright (c) 1998, 1999, 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; Charles M. Hannum; and David Young.
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
 * PCI bus front-end for the Realtek RTL8180 802.11 MAC/BBP chip.
 *
 * Derived from the ADMtek ADM8211 PCI bus front-end.
 *
 * Derived from the ``Tulip'' PCI bus front-end.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_rtw_pci.c,v 1.23 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/rtwvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the RTL8180.
 */
#define RTW_PCI_IOBA PCI_BAR(0)	/* i/o mapped base */
#define RTW_PCI_MMBA PCI_BAR(1)	/* memory mapped base */

struct rtw_pci_softc {
	struct rtw_softc	psc_rtw;

	pcireg_t		psc_csr;
	void			*psc_ih;
	pci_chipset_tag_t	psc_pc;
	pci_intr_handle_t	psc_pih;
	pcitag_t		psc_tag;
};

static void	rtw_pci_attach(device_t, device_t, void *);
static int	rtw_pci_detach(device_t, int);
#if 0
static void	rtw_pci_funcregen(struct rtw_regs *, int);
#endif
static const struct rtw_pci_product *
		rtw_pci_lookup(const struct pci_attach_args *);
static int	rtw_pci_match(device_t, cfdata_t, void *);
static bool	rtw_pci_resume(device_t, const pmf_qual_t *);
static int	rtw_pci_setup(struct rtw_pci_softc *);
static bool	rtw_pci_suspend(device_t, const pmf_qual_t *);

CFATTACH_DECL3_NEW(rtw_pci, sizeof(struct rtw_pci_softc),
    rtw_pci_match, rtw_pci_attach, rtw_pci_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

static const struct rtw_pci_product {
	u_int32_t	rpp_vendor;	/* PCI vendor ID */
	u_int32_t	rpp_product;	/* PCI product ID */
	const char	*rpp_product_name;
} rtw_pci_products[] = {
	{ PCI_VENDOR_REALTEK,		PCI_PRODUCT_REALTEK_RT8180,
	  "Realtek RTL8180 802.11 MAC/BBP" },
#ifdef RTW_DEBUG
#if 0   /* These came from openbsd, netbsd doesn't have the definitions. */
	{ PCI_VENDOR_REALTEK,		PCI_PRODUCT_REALTEK_RT8185,
	  "Realtek RTL8185 802.11 MAC/BBP" },
	{ PCI_VENDOR_BELKIN2,		PCI_PRODUCT_BELKIN2_F5D7010,
	  "Belkin F5D7010" },
#endif
#endif
	{ PCI_VENDOR_BELKIN,		PCI_PRODUCT_BELKIN_F5D6001,
	  "Belkin F5D6001" },
	{ PCI_VENDOR_BELKIN,		PCI_PRODUCT_BELKIN_F5D6020V3,
	  "Belkin F5D6020v3" },
	{PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DWL610,
	  "DWL-610 D-Link Air 802.11b (RTL8180 MAC/BBP)"},
	{ 0,				0,				NULL },
};

static const struct rtw_pci_product *
rtw_pci_lookup(const struct pci_attach_args *pa)
{
	const struct rtw_pci_product *rpp;

	for (rpp = rtw_pci_products; rpp->rpp_product_name != NULL; rpp++) {
		if (PCI_VENDOR(pa->pa_id) == rpp->rpp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == rpp->rpp_product)
			return rpp;
	}
	return NULL;
}

static int
rtw_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (rtw_pci_lookup(pa) != NULL)
		return 1;

	return 0;
}

static void
rtw_pci_attach(device_t parent, device_t self, void *aux)
{
	struct rtw_pci_softc *psc = device_private(self);
	struct rtw_softc *sc = &psc->psc_rtw;
	struct rtw_regs *regs = &sc->sc_regs;
	struct pci_attach_args *pa = aux;
	const char *intrstr = NULL;
	const struct rtw_pci_product *rpp;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;

	rpp = rtw_pci_lookup(pa);
	if (rpp == NULL) {
		printf("\n");
		panic("rtw_pci_attach: impossible");
	}

	/*
	 * Get revision info, and set some chip-specific variables.
	 */
	sc->sc_rev = PCI_REVISION(pa->pa_class);
	aprint_normal(": %s, revision %d.%d signature %08x\n",
	    rpp->rpp_product_name,
	    (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf,
	    pci_conf_read(psc->psc_pc, psc->psc_tag, 0x80));

	/*
	 * Map the device.
	 */
	psc->psc_csr = PCI_COMMAND_MASTER_ENABLE |
	              PCI_COMMAND_PARITY_ENABLE |
		      PCI_COMMAND_SERR_ENABLE;
	if (pci_mapreg_map(pa, RTW_PCI_MMBA, PCI_MAPREG_TYPE_MEM, 0,
	    &regs->r_bt, &regs->r_bh, NULL, &regs->r_sz) == 0) {
		RTW_DPRINTF(RTW_DEBUG_ATTACH,
		    ("%s: %s mapped %" PRIuMAX " bytes mem space\n",
		     device_xname(self), __func__, (uintmax_t)regs->r_sz));
		psc->psc_csr |= PCI_COMMAND_MEM_ENABLE;
	} else if (pci_mapreg_map(pa, RTW_PCI_IOBA, PCI_MAPREG_TYPE_IO, 0,
	    &regs->r_bt, &regs->r_bh, NULL, &regs->r_sz) == 0) {
		RTW_DPRINTF(RTW_DEBUG_ATTACH,
		    ("%s: %s mapped %" PRIuMAX " bytes I/O space\n",
		     device_xname(self), __func__, (uintmax_t)regs->r_sz));
		psc->psc_csr |= PCI_COMMAND_IO_ENABLE;
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}

	/*
	 * Bring the chip out of powersave mode and initialize the
	 * configuration registers.
	 */
	if (rtw_pci_setup(psc) != 0)
		return;

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &psc->psc_pih)) {
		aprint_error_dev(self, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(psc->psc_pc, psc->psc_pih, intrbuf, sizeof(intrbuf));
	psc->psc_ih = pci_intr_establish(psc->psc_pc, psc->psc_pih, IPL_NET,
	    rtw_intr, sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Finish off the attach.
	 */
	rtw_attach(sc);

	if (pmf_device_register(self, rtw_pci_suspend, rtw_pci_resume)) {
		pmf_class_network_register(self, &sc->sc_if);
		/*
		 * Power down the socket.
		 */
		pmf_device_suspend(self, &sc->sc_qual);
	} else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
rtw_pci_detach(device_t self, int flags)
{
	struct rtw_pci_softc *psc = device_private(self);
	struct rtw_softc *sc = &psc->psc_rtw;
	struct rtw_regs *regs = &sc->sc_regs;
	int rc;

	if ((rc = rtw_detach(sc)) != 0)
		return rc;
	if (psc->psc_ih != NULL)
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
	bus_space_unmap(regs->r_bt, regs->r_bh, regs->r_sz);

	return 0;
}

static bool
rtw_pci_resume(device_t self, const pmf_qual_t *qual)
{
	struct rtw_pci_softc *psc = device_private(self);
	struct rtw_softc *sc = &psc->psc_rtw;

	/* Establish the interrupt. */
	psc->psc_ih = pci_intr_establish(psc->psc_pc, psc->psc_pih, IPL_NET,
	    rtw_intr, sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt\n");
		return false;
	}

	return rtw_resume(self, qual);
}

static bool
rtw_pci_suspend(device_t self, const pmf_qual_t *qual)
{
	struct rtw_pci_softc *psc = device_private(self);

	if (!rtw_suspend(self, qual))
		return false;

	/* Unhook the interrupt handler. */
	pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
	psc->psc_ih = NULL;
	return true;
}

static int
rtw_pci_setup(struct rtw_pci_softc *psc)
{
	pcitag_t tag = psc->psc_tag;
	pcireg_t bhlc, csr, lattimer;
	device_t self = psc->psc_rtw.sc_dev;
	int rc;

	/* power up chip */
	rc = pci_activate(psc->psc_pc, psc->psc_tag, self, NULL);

	if (rc != 0 && rc != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate (%d)\n", rc);
		return rc;
	}

	/* I believe the datasheet tries to warn us that the RTL8180
	 * wants for 16 (0x10) to divide the latency timer.
	 */
	bhlc = pci_conf_read(psc->psc_pc, tag, PCI_BHLC_REG);
	lattimer = rounddown(PCI_LATTIMER(bhlc), 0x10);
	if (PCI_LATTIMER(bhlc) != lattimer) {
		bhlc &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		bhlc |= (lattimer << PCI_LATTIMER_SHIFT);
		pci_conf_write(psc->psc_pc, tag, PCI_BHLC_REG, bhlc);
	}

	/* Enable the appropriate bits in the PCI CSR. */
	csr = pci_conf_read(psc->psc_pc, tag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	csr |= psc->psc_csr;
	pci_conf_write(psc->psc_pc, tag, PCI_COMMAND_STATUS_REG, csr);

	return 0;
}
