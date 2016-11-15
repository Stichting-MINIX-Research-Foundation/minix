/*	$NetBSD: if_athn_cardbus.c,v 1.2 2013/04/03 14:20:02 christos Exp $	*/
/*	$OpenBSD: if_athn_cardbus.c,v 1.13 2011/01/08 10:02:32 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 * CardBus front-end for Atheros 802.11a/g/n chipsets.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_athn_cardbus.c,v 1.2 2013/04/03 14:20:02 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

/*
 * PCI configuration space registers
 */
#define ATHN_PCI_MMBA	PCI_BAR(0)	/* memory mapped base */

struct athn_cardbus_softc {
	struct athn_softc	csc_sc;

	/* CardBus specific goo. */
	cardbus_devfunc_t	csc_ct;
	pcitag_t		csc_tag;
	void			*csc_ih;
	bus_space_tag_t		csc_iot;
	bus_space_handle_t	csc_ioh;
	bus_size_t		csc_mapsz;
	pcireg_t		csc_bar_val;
};

#define Static static

Static int	athn_cardbus_match(device_t, cfdata_t, void *);
Static void	athn_cardbus_attach(device_t, device_t, void *);
Static int	athn_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(athn_cardbus, sizeof(struct athn_cardbus_softc),
    athn_cardbus_match, athn_cardbus_attach, athn_cardbus_detach, NULL);

Static uint32_t	athn_cardbus_read(struct athn_softc *, uint32_t);
Static bool	athn_cardbus_resume(device_t, const pmf_qual_t *l);
Static void	athn_cardbus_setup(struct athn_cardbus_softc *);
Static bool	athn_cardbus_suspend(device_t, const pmf_qual_t *);
Static void	athn_cardbus_write(struct athn_softc *, uint32_t, uint32_t);
Static void	athn_cardbus_write_barrier(struct athn_softc *);

#ifdef openbsd_power_management
Static int	athn_cardbus_enable(struct athn_softc *);
Static void	athn_cardbus_disable(struct athn_softc *);
Static void	athn_cardbus_power(struct athn_softc *, int);
#endif /* openbsd_power_management */

Static int
athn_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	static const struct {
		pci_vendor_id_t		vendor;
		pci_product_id_t	product;
	} athn_cardbus_devices[] = {
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5416 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5418 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9160 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9280 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9281 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9285 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR2427 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9227 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9287 },
		{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR9300 }
	};
	struct cardbus_attach_args *ca = aux;
	size_t i;

	for (i = 0; i < __arraycount(athn_cardbus_devices); i++) {
		if (PCI_VENDOR(ca->ca_id) == athn_cardbus_devices[i].vendor &&
		    PCI_VENDOR(ca->ca_id) == athn_cardbus_devices[i].product)
			return 1;
	}
	return 0;
}

Static void
athn_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct athn_cardbus_softc *csc = device_private(self);
	struct athn_softc *sc = &csc->csc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t base;
	int error;

	sc->sc_dmat = ca->ca_dmat;
	csc->csc_ct = ct;
	csc->csc_tag = ca->ca_tag;

	aprint_normal("\n");
	aprint_naive("\n");

#ifdef openbsd_power_management
	/* Power management hooks. */
	sc->sc_enable = athn_cardbus_enable;
	sc->sc_disable = athn_cardbus_disable;
	sc->sc_power = athn_cardbus_power;
#endif
	sc->sc_ops.read = athn_cardbus_read;
	sc->sc_ops.write = athn_cardbus_write;
	sc->sc_ops.write_barrier = athn_cardbus_write_barrier;

	/*
	 * Map control/status registers.
	 */
	error = Cardbus_mapreg_map(ct, ATHN_PCI_MMBA, PCI_MAPREG_TYPE_MEM, 0,
	    &csc->csc_iot, &csc->csc_ioh, &base, &csc->csc_mapsz);
	if (error != 0) {
		aprint_error_dev(self, "unable to map device (%d)\n", error);
		return;
	}
	csc->csc_bar_val = base | PCI_MAPREG_TYPE_MEM;

	/*
	 * Set up the PCI configuration registers.
	 */
	athn_cardbus_setup(csc);

	if (athn_attach(sc) != 0) {
		Cardbus_mapreg_unmap(ct, ATHN_PCI_MMBA, csc->csc_iot,
		    csc->csc_ioh, csc->csc_mapsz);
		return;
	}

	if (pmf_device_register(self,
	    athn_cardbus_suspend, athn_cardbus_resume)) {
		pmf_class_network_register(self, &sc->sc_if);
		pmf_device_suspend(self, &sc->sc_qual);
	} else
		aprint_error_dev(self, "couldn't establish power handler\n");

//	Cardbus_function_disable(ct);

	ieee80211_announce(ic);
}

Static int
athn_cardbus_detach(device_t self, int flags)
{
	struct athn_cardbus_softc *csc = device_private(self);
	struct athn_softc *sc = &csc->csc_sc;
	cardbus_devfunc_t ct = csc->csc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	athn_detach(sc);

	pmf_device_deregister(self);

	/* Unhook the interrupt handler. */
	if (csc->csc_ih != NULL)
		cardbus_intr_disestablish(cc, cf, csc->csc_ih);

	/* Release bus space and close window. */
	Cardbus_mapreg_unmap(ct, ATHN_PCI_MMBA, csc->csc_iot, csc->csc_ioh,
	    csc->csc_mapsz);

	return 0;
}

Static void
athn_cardbus_setup(struct athn_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->csc_ct;
#ifdef unneeded	/* XXX */
	pci_chipset_tag_t pc = csc->csc_pc;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
#endif
	pcireg_t reg;
	int rc;

	if ((rc = cardbus_set_powerstate(ct, csc->csc_tag, PCI_PWR_D0)) != 0)
		aprint_debug("%s: cardbus_set_powerstate %d\n", __func__, rc);

	/* Program the BAR. */
	Cardbus_conf_write(ct, csc->csc_tag, ATHN_PCI_MMBA, csc->csc_bar_val);

#ifdef unneeded	/* XXX */
	/* Make sure the right access type is on the cardbus bridge. */
	(*cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);
#endif
	/* Enable the appropriate bits in the PCI CSR. */
	reg = Cardbus_conf_read(ct, csc->csc_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	Cardbus_conf_write(ct, csc->csc_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Noone knows why this shit is necessary but there are claims that
	 * not doing this may cause very frequent PCI FATAL interrupts from
	 * the card: http://bugzilla.kernel.org/show_bug.cgi?id=13483
	 */
	reg = Cardbus_conf_read(ct, csc->csc_tag, 0x40);
	if (reg & 0xff00)
		Cardbus_conf_write(ct, csc->csc_tag, 0x40, reg & ~0xff00);

	/* Change latency timer; default value yields poor results. */
	reg = Cardbus_conf_read(ct, csc->csc_tag, PCI_BHLC_REG);
	reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	reg |= 168 << PCI_LATTIMER_SHIFT;
	Cardbus_conf_write(ct, csc->csc_tag, PCI_BHLC_REG, reg);
}

Static uint32_t
athn_cardbus_read(struct athn_softc *sc, uint32_t addr)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;

	return bus_space_read_4(csc->csc_iot, csc->csc_ioh, addr);
}

Static void
athn_cardbus_write(struct athn_softc *sc, uint32_t addr, uint32_t val)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;

	bus_space_write_4(csc->csc_iot, csc->csc_ioh, addr, val);
}

Static void
athn_cardbus_write_barrier(struct athn_softc *sc)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;

	bus_space_barrier(csc->csc_iot, csc->csc_ioh, 0, csc->csc_mapsz,
	    BUS_SPACE_BARRIER_WRITE);
}

Static bool
athn_cardbus_suspend(device_t self, const pmf_qual_t *qual)
{
	struct athn_cardbus_softc *csc = device_private(self);

	athn_suspend(&csc->csc_sc);
	if (csc->csc_ih != NULL) {
		Cardbus_intr_disestablish(csc->csc_ct, csc->csc_ih);
		csc->csc_ih = NULL;
	}
	return true;
}

Static bool
athn_cardbus_resume(device_t self, const pmf_qual_t *qual)
{
	struct athn_cardbus_softc *csc = device_private(self);

	csc->csc_ih = Cardbus_intr_establish(csc->csc_ct, IPL_NET, athn_intr,
	    &csc->csc_sc);

	if (csc->csc_ih == NULL) {
		aprint_error_dev(self,
		    "unable to establish interrupt\n");
		return false;
	}
	return athn_resume(&csc->csc_sc);
}

/************************************************************************
 * XXX: presumably the pmf_* stuff handles this.
 */
#ifdef openbsd_power_management
Static int
athn_cardbus_enable(struct athn_softc *sc)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->csc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Power on the socket. */
	Cardbus_function_enable(ct);

	/* Setup the PCI configuration registers. */
	athn_cardbus_setup(csc);

	/* Map and establish the interrupt handler. */
	csc->csc_ih = cardbus_intr_establish(cc, cf, IPL_NET, athn_intr, sc);
	if (csc->csc_ih == NULL) {
		printf("%s: could not establish interrupt at %d\n",
		    device_xname(sc->sc_dev), csc->csc_intrline);
		Cardbus_function_disable(ct);
		return 1;
	}
	return 0;
}

Static void
athn_cardbus_disable(struct athn_softc *sc)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->csc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Unhook the interrupt handler. */
	cardbus_intr_disestablish(cc, cf, csc->csc_ih);
	csc->csc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}

Static void
athn_cardbus_power(struct athn_softc *sc, int why)
{
	struct athn_cardbus_softc *csc = (struct athn_cardbus_softc *)sc;

	if (why == DVACT_RESUME) {
		/* Restore the PCI configuration registers. */
		athn_cardbus_setup(csc);
	}
}
#endif /* openbsd_power_management */
