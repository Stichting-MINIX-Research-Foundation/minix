/* $NetBSD: if_atw_cardbus.c,v 1.36 2011/08/01 11:20:27 drochner Exp $ */

/*-
 * Copyright (c) 1999, 2000, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.  This code was adapted for the ADMtek ADM8211
 * by David Young.
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
 * CardBus bus front-end for the ADMtek ADM8211 802.11 MAC/BBP driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_atw_cardbus.c,v 1.36 2011/08/01 11:20:27 drochner Exp $");

#include "opt_inet.h"

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

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/atwreg.h>
#include <dev/ic/rf3000reg.h>
#include <dev/ic/si4136reg.h>
#include <dev/ic/atwvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the ADM8211.
 */
#define ATW_PCI_IOBA PCI_BAR(0)	/* i/o mapped base */
#define ATW_PCI_MMBA PCI_BAR(1)	/* memory mapped base */

struct atw_cardbus_softc {
	struct atw_softc sc_atw;

	/* CardBus-specific goo. */
	void			*sc_ih;		/* interrupt handle */
	cardbus_devfunc_t	sc_ct;		/* our CardBus devfuncs */
	pcitag_t		sc_tag;		/* our CardBus tag */
	pcireg_t		sc_csr;		/* CSR bits */
	bus_size_t		sc_mapsize;	/* the size of mapped bus space
						 * region
						 */

	int			sc_bar_reg;	/* which BAR to use */
	pcireg_t		sc_bar_val;	/* value of the BAR */
};

static int	atw_cardbus_match(device_t, cfdata_t, void *);
static void	atw_cardbus_attach(device_t, device_t, void *);
static int	atw_cardbus_detach(device_t, int);

CFATTACH_DECL3_NEW(atw_cardbus, sizeof(struct atw_cardbus_softc),
    atw_cardbus_match, atw_cardbus_attach, atw_cardbus_detach, atw_activate,
    NULL, NULL, DVF_DETACH_SHUTDOWN);

static void	atw_cardbus_setup(struct atw_cardbus_softc *);

static bool	atw_cardbus_suspend(device_t, const pmf_qual_t *);
static bool	atw_cardbus_resume(device_t, const pmf_qual_t *);

static const struct atw_cardbus_product *atw_cardbus_lookup
   (const struct cardbus_attach_args *);

static const struct atw_cardbus_product {
	u_int32_t	 acp_vendor;	/* PCI vendor ID */
	u_int32_t	 acp_product;	/* PCI product ID */
	const char	*acp_product_name;
} atw_cardbus_products[] = {
	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_ADM8211,
	  "ADMtek ADM8211 802.11 MAC/BBP" },

	{ 0,				0,	NULL },
};

static const struct atw_cardbus_product *
atw_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct atw_cardbus_product *acp;

	for (acp = atw_cardbus_products; acp->acp_product_name != NULL; acp++) {
		if (PCI_VENDOR(ca->ca_id) == acp->acp_vendor &&
		    PCI_PRODUCT(ca->ca_id) == acp->acp_product)
			return acp;
	}
	return NULL;
}

static int
atw_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (atw_cardbus_lookup(ca) != NULL)
		return 1;

	return 0;
}

static void
atw_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct atw_cardbus_softc *csc = device_private(self);
	struct atw_softc *sc = &csc->sc_atw;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	const struct atw_cardbus_product *acp;
#if 0
	int i;
#define	FUNCREG(__x)	{#__x, (__x)}
	struct {
		const char *name;
		bus_size_t ofs;
	} funcregs[] = {
		FUNCREG(ATW_FER), FUNCREG(ATW_FEMR), FUNCREG(ATW_FPSR),
		FUNCREG(ATW_FFER)
	};
#undef FUNCREG
#endif
	bus_addr_t adr;

	sc->sc_dev = self;
	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	acp = atw_cardbus_lookup(ca);
	if (acp == NULL) {
		printf("\n");
		panic("atw_cardbus_attach: impossible");
	}

	/* Get revision info. */
	sc->sc_rev = PCI_REVISION(ca->ca_class);

	printf(": %s, revision %d.%d\n", acp->acp_product_name,
	    (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf);

#if 0
	printf("%s: signature %08x\n", device_xname(self),
	    (rev >> 4) & 0xf, rev & 0xf,
	    Cardbus_conf_read(ct, csc->sc_tag, 0x80));
#endif

	/*
	 * Map the device.
	 */
	csc->sc_csr = PCI_COMMAND_MASTER_ENABLE |
	              PCI_COMMAND_PARITY_ENABLE |
		      PCI_COMMAND_SERR_ENABLE;
	if (Cardbus_mapreg_map(ct, ATW_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
#if 0
		printf("%s: atw_cardbus_attach mapped %d bytes mem space\n",
		    device_xname(self), csc->sc_mapsize);
#endif
		csc->sc_csr |= PCI_COMMAND_MEM_ENABLE;
		csc->sc_bar_reg = ATW_PCI_MMBA;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_MEM;
	} else if (Cardbus_mapreg_map(ct, ATW_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
#if 0
		printf("%s: atw_cardbus_attach mapped %d bytes I/O space\n",
		    device_xname(self), csc->sc_mapsize);
#endif
		csc->sc_csr |= PCI_COMMAND_IO_ENABLE;
		csc->sc_bar_reg = ATW_PCI_IOBA;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_IO;
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}

	/*
	 * Bring the chip out of powersave mode and initialize the
	 * configuration registers.
	 */
	atw_cardbus_setup(csc);

#if 0
	/*
	 * The CardBus cards will make it to store-and-forward mode as
	 * soon as you put them under any kind of load, so just start
	 * out there.
	 */
	sc->sc_txthresh = 3; /* TBD name constant */
#endif

#if 0
	for (i = 0; i < __arraycount(funcregs); i++) {
		aprint_error_dev(sc->sc_dev, "%s %" PRIx32 "\n",
		    funcregs[i].name, ATW_READ(sc, funcregs[i].ofs));
	}
#endif

	ATW_WRITE(sc, ATW_FEMR, 0);
	ATW_WRITE(sc, ATW_FER, ATW_READ(sc, ATW_FER));

	/*
	 * Bus-independent attach.
	 */
	atw_attach(sc);

	if (pmf_device_register1(sc->sc_dev, atw_cardbus_suspend,
	    atw_cardbus_resume, atw_shutdown))
		pmf_class_network_register(sc->sc_dev, &sc->sc_if);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	/*
	 * Power down the socket.
	 */
	pmf_device_suspend(sc->sc_dev, &sc->sc_qual);
}

static int
atw_cardbus_detach(device_t self, int flags)
{
	struct atw_cardbus_softc *csc = device_private(self);
	struct atw_softc *sc = &csc->sc_atw;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#if defined(DIAGNOSTIC)
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(self));
#endif

	rv = atw_detach(sc);
	if (rv != 0)
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
		    sc->sc_st, sc->sc_sh, csc->sc_mapsize);

	return 0;
}

static bool
atw_cardbus_resume(device_t self, const pmf_qual_t *qual)
{
	struct atw_cardbus_softc *csc = device_private(self);
	struct atw_softc *sc = &csc->sc_atw;
	cardbus_devfunc_t ct = csc->sc_ct;

	/*
	 * Map and establish the interrupt.
	 */
	csc->sc_ih = Cardbus_intr_establish(ct, IPL_NET, atw_intr, sc);
	if (csc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt\n");
		return false;
	}

	return true;
}

static bool
atw_cardbus_suspend(device_t self, const pmf_qual_t *qual)
{
	struct atw_cardbus_softc *csc = device_private(self);
	cardbus_devfunc_t ct = csc->sc_ct;

	/* Unhook the interrupt handler. */
	Cardbus_intr_disestablish(ct, csc->sc_ih);
	csc->sc_ih = NULL;

	return atw_suspend(self, qual);
}

static void
atw_cardbus_setup(struct atw_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	pcireg_t csr;
	int rc;

	if ((rc = cardbus_set_powerstate(ct, csc->sc_tag, PCI_PWR_D0)) != 0)
		aprint_debug("%s: cardbus_set_powerstate %d\n", __func__, rc);

	/* Program the BAR. */
	Cardbus_conf_write(ct, csc->sc_tag, csc->sc_bar_reg,
	    csc->sc_bar_val);

	/* Enable the appropriate bits in the PCI CSR. */
	csr = Cardbus_conf_read(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	csr |= csc->sc_csr;
	Cardbus_conf_write(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG, csr);
}
