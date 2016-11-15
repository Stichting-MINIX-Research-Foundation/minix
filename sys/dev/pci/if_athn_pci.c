/*	$NetBSD: if_athn_pci.c,v 1.10 2014/03/29 19:28:24 christos Exp $	*/
/*	$OpenBSD: if_athn_pci.c,v 1.11 2011/01/08 10:02:32 damien Exp $	*/

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
 * PCI front-end for Atheros 802.11a/g/n chipsets.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_athn_pci.c,v 1.10 2014/03/29 19:28:24 christos Exp $");

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

#define PCI_SUBSYSID_ATHEROS_COEX2WIRE		0x309b
#define PCI_SUBSYSID_ATHEROS_COEX3WIRE_SA	0x30aa
#define PCI_SUBSYSID_ATHEROS_COEX3WIRE_DA	0x30ab

#define ATHN_PCI_MMBA	PCI_BAR(0)	/* memory mapped base */

struct athn_pci_softc {
	struct athn_softc	psc_sc;

	/* PCI specific goo. */
	pci_chipset_tag_t	psc_pc;
	pcitag_t		psc_tag;
	pci_intr_handle_t	psc_pih;
	void			*psc_ih;
	bus_space_tag_t		psc_iot;
	bus_space_handle_t	psc_ioh;
	bus_size_t		psc_mapsz;
	int			psc_cap_off;
};

#define Static static

Static int	athn_pci_match(device_t, cfdata_t, void *);
Static void	athn_pci_attach(device_t, device_t, void *);
Static int	athn_pci_detach(device_t, int);
Static int	athn_pci_activate(device_t, enum devact);

CFATTACH_DECL_NEW(athn_pci, sizeof(struct athn_pci_softc), athn_pci_match,
    athn_pci_attach, athn_pci_detach, athn_pci_activate);

Static bool	athn_pci_resume(device_t, const pmf_qual_t *);
Static bool	athn_pci_suspend(device_t, const pmf_qual_t *);
Static uint32_t	athn_pci_read(struct athn_softc *, uint32_t);
Static void	athn_pci_write(struct athn_softc *, uint32_t, uint32_t);
Static void	athn_pci_write_barrier(struct athn_softc *);
Static void	athn_pci_disable_aspm(struct athn_softc *);

Static int
athn_pci_match(device_t parent, cfdata_t match, void *aux)
{
	static const struct {
		pci_vendor_id_t		apd_vendor;
		pci_product_id_t	apd_product;
	} athn_pci_devices[] = {
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
	struct pci_attach_args *pa = aux;
	size_t i;

	for (i = 0; i < __arraycount(athn_pci_devices); i++) {
		if (PCI_VENDOR(pa->pa_id) == athn_pci_devices[i].apd_vendor &&
		    PCI_PRODUCT(pa->pa_id) == athn_pci_devices[i].apd_product)
			/*
			 * Match better than 1, we prefer this driver
			 * over ath(4)
			 */
			return 10;
	}
	return 0;
}

Static void
athn_pci_attach(device_t parent, device_t self, void *aux)
{
	struct athn_pci_softc *psc = device_private(self);
	struct athn_softc *sc = &psc->psc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	pcireg_t memtype, reg;
	pci_product_id_t subsysid;
	int error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;

	sc->sc_ops.read = athn_pci_read;
	sc->sc_ops.write = athn_pci_write;
	sc->sc_ops.write_barrier = athn_pci_write_barrier;

	/*
	 * Get the offset of the PCI Express Capability Structure in PCI
	 * Configuration Space (Linux hardcodes it as 0x60.)
	 */
	error = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PCIEXPRESS,
	    &psc->psc_cap_off, NULL);
	if (error != 0) {	/* Found. */
		sc->sc_disable_aspm = athn_pci_disable_aspm;
		sc->sc_flags |= ATHN_FLAG_PCIE;
	}
	/*
	 * Noone knows why this shit is necessary but there are claims that
	 * not doing this may cause very frequent PCI FATAL interrupts from
	 * the card: http://bugzilla.kernel.org/show_bug.cgi?id=13483
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x40);
	if (reg & 0xff00)
		pci_conf_write(pa->pa_pc, pa->pa_tag, 0x40, reg & ~0xff00);

	/* Change latency timer; default value yields poor results. */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	reg |= 168 << PCI_LATTIMER_SHIFT;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, reg);

	/* Determine if bluetooth is also supported (combo chip.) */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	subsysid = PCI_PRODUCT(reg);
	if (subsysid == PCI_SUBSYSID_ATHEROS_COEX3WIRE_SA ||
	    subsysid == PCI_SUBSYSID_ATHEROS_COEX3WIRE_DA)
		sc->sc_flags |= ATHN_FLAG_BTCOEX3WIRE;
	else if (subsysid == PCI_SUBSYSID_ATHEROS_COEX2WIRE)
		sc->sc_flags |= ATHN_FLAG_BTCOEX2WIRE;

	/*
	 * Setup memory-mapping of PCI registers.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, ATHN_PCI_MMBA);
	if (memtype != PCI_MAPREG_TYPE_MEM &&
	    memtype != PCI_MAPREG_MEM_TYPE_64BIT) {
		aprint_error_dev(self, "bad pci register type %d\n",
		    (int)memtype);
		goto fail;
	}
	error = pci_mapreg_map(pa, ATHN_PCI_MMBA, memtype, 0, &psc->psc_iot,
	    &psc->psc_ioh, NULL, &psc->psc_mapsz);
	if (error != 0) {
		aprint_error_dev(self, "cannot map register space\n");
		goto fail;
	}

	/*
	 * Arrange interrupt line.
	 */
	if (pci_intr_map(pa, &psc->psc_pih) != 0) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		goto fail1;
	}

	intrstr = pci_intr_string(psc->psc_pc, psc->psc_pih, intrbuf, sizeof(intrbuf));
	psc->psc_ih = pci_intr_establish(psc->psc_pc, psc->psc_pih, IPL_NET,
	    athn_intr, sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		goto fail1;
	}

	ic->ic_ifp = &sc->sc_if;
	if (athn_attach(sc) != 0)
		goto fail2;

	aprint_verbose_dev(self, "interrupting at %s\n", intrstr);

	if (pmf_device_register(self, athn_pci_suspend, athn_pci_resume)) {
		pmf_class_network_register(self, &sc->sc_if);
		pmf_device_suspend(self, &sc->sc_qual);
	}
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	ieee80211_announce(ic);
	return;

 fail2:
	pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
	psc->psc_ih = NULL;
 fail1:
	bus_space_unmap(psc->psc_iot, psc->psc_ioh, psc->psc_mapsz);
	psc->psc_mapsz = 0;
 fail:
	return;
}

Static int
athn_pci_detach(device_t self, int flags)
{
	struct athn_pci_softc *psc = device_private(self);
	struct athn_softc *sc = &psc->psc_sc;

	if (psc->psc_ih != NULL) {
		athn_detach(sc);
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
		psc->psc_ih = NULL;
	}
	if (psc->psc_mapsz > 0) {
		bus_space_unmap(psc->psc_iot, psc->psc_ioh, psc->psc_mapsz);
		psc->psc_mapsz = 0;
	}
	return 0;
}

Static int
athn_pci_activate(device_t self, enum devact act)
{
	struct athn_pci_softc *psc = device_private(self);
	struct athn_softc *sc = &psc->psc_sc;

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(sc->sc_ic.ic_ifp);
		break;
	}
	return 0;
}

Static bool
athn_pci_suspend(device_t self, const pmf_qual_t *qual)
{
	struct athn_pci_softc *psc = device_private(self);
	struct athn_softc *sc = &psc->psc_sc;

	athn_suspend(sc);
	if (psc->psc_ih != NULL) {
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
		psc->psc_ih = NULL;
	}
	return true;
}

Static bool
athn_pci_resume(device_t self, const pmf_qual_t *qual)
{
	struct athn_pci_softc *psc = device_private(self);
	struct athn_softc *sc = &psc->psc_sc;
	pcireg_t reg;

	/*
	 * XXX: see comment in athn_attach().
	 */
	reg = pci_conf_read(psc->psc_pc, psc->psc_tag, 0x40);
	if (reg & 0xff00)
		pci_conf_write(psc->psc_pc, psc->psc_tag, 0x40, reg & ~0xff00);

	psc->psc_ih = pci_intr_establish(psc->psc_pc, psc->psc_pih, IPL_NET,
	    athn_intr, sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return false;
	}
	return athn_resume(sc);
}

Static uint32_t
athn_pci_read(struct athn_softc *sc, uint32_t addr)
{
	struct athn_pci_softc *psc = (struct athn_pci_softc *)sc;

	return bus_space_read_4(psc->psc_iot, psc->psc_ioh, addr);
}

Static void
athn_pci_write(struct athn_softc *sc, uint32_t addr, uint32_t val)
{
	struct athn_pci_softc *psc = (struct athn_pci_softc *)sc;

	bus_space_write_4(psc->psc_iot, psc->psc_ioh, addr, val);
}

Static void
athn_pci_write_barrier(struct athn_softc *sc)
{
	struct athn_pci_softc *psc = (struct athn_pci_softc *)sc;

	bus_space_barrier(psc->psc_iot, psc->psc_ioh, 0, psc->psc_mapsz,
	    BUS_SPACE_BARRIER_WRITE);
}

Static void
athn_pci_disable_aspm(struct athn_softc *sc)
{
	struct athn_pci_softc *psc = (struct athn_pci_softc *)sc;
	pcireg_t reg;

	/* Disable PCIe Active State Power Management (ASPM). */
	reg = pci_conf_read(psc->psc_pc, psc->psc_tag,
	    psc->psc_cap_off + PCIE_LCSR);
	reg &= ~(PCIE_LCSR_ASPM_L0S | PCIE_LCSR_ASPM_L1);
	pci_conf_write(psc->psc_pc, psc->psc_tag,
	    psc->psc_cap_off + PCIE_LCSR, reg);
}
