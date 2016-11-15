/*	$NetBSD: if_bwi_pci.c,v 1.14 2014/03/29 19:28:24 christos Exp $	*/
/*	$OpenBSD: if_bwi_pci.c,v 1.6 2008/02/14 22:10:02 brad Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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
 * Broadcom AirForce BCM43xx IEEE 802.11b/g wireless network driver
 * PCI front end
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_bwi_pci.c,v 1.14 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/bwivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* Base Address Register */
#define BWI_PCI_BAR0 PCI_BAR(0)

static int	bwi_pci_match(device_t, cfdata_t, void *);
static void	bwi_pci_attach(device_t, device_t, void *);
static int	bwi_pci_detach(device_t, int);
static void	bwi_pci_conf_write(void *, uint32_t, uint32_t);
static uint32_t	bwi_pci_conf_read(void *, uint32_t);

struct bwi_pci_softc {
	struct bwi_softc	 psc_bwi;

	pci_chipset_tag_t        psc_pc;
	pcitag_t		 psc_pcitag;

	bus_size_t		 psc_mapsize;
};

CFATTACH_DECL_NEW(bwi_pci, sizeof(struct bwi_pci_softc),
    bwi_pci_match, bwi_pci_attach, bwi_pci_detach, NULL);

static int
bwi_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_BROADCOM)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_BROADCOM_BCM4303:
	case PCI_PRODUCT_BROADCOM_BCM4306:
	case PCI_PRODUCT_BROADCOM_BCM4306_2:
	case PCI_PRODUCT_BROADCOM_BCM4307:
	case PCI_PRODUCT_BROADCOM_BCM4309:
	case PCI_PRODUCT_BROADCOM_BCM4311:
	case PCI_PRODUCT_BROADCOM_BCM4312:
	case PCI_PRODUCT_BROADCOM_BCM4318:
	case PCI_PRODUCT_BROADCOM_BCM4319:
	case PCI_PRODUCT_BROADCOM_BCM4322:
	case PCI_PRODUCT_BROADCOM_BCM43XG:
	case PCI_PRODUCT_BROADCOM_BCM4328:
		return (1);
	}

	return (0);
}

static void
bwi_pci_attach(device_t parent, device_t self, void *aux)
{
	struct bwi_pci_softc *psc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct bwi_softc *sc = &psc->psc_bwi;
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	pcireg_t memtype, reg;
	int error = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive("\n");
	aprint_normal(": Broadcom Wireless\n");

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_pcitag = pa->pa_tag;

	/* map control / status registers */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BWI_PCI_BAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		aprint_error_dev(self, "invalid base address register\n");
		return;
	}

	if (pci_mapreg_map(pa, BWI_PCI_BAR0,
	    memtype, 0, &sc->sc_mem_bt, &sc->sc_mem_bh,
	    NULL, &psc->psc_mapsize) != 0)
	{
		aprint_error_dev(self, "could not map mem space\n");
		return;
	}

	/* map interrupt */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "could not map interrupt\n");
		goto fail;
	}

	/* establish interrupt */
	intrstr = pci_intr_string(psc->psc_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(psc->psc_pc, ih, IPL_NET, bwi_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* we need to access PCI config space from the driver */
	sc->sc_conf_write = bwi_pci_conf_write;
	sc->sc_conf_read = bwi_pci_conf_read;

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	sc->sc_pci_revid = PCI_REVISION(pa->pa_class);
	sc->sc_pci_did = PCI_PRODUCT(pa->pa_id);
	sc->sc_pci_subvid = PCI_VENDOR(reg);
	sc->sc_pci_subdid = PCI_PRODUCT(reg);

	if (!pmf_device_register(self, bwi_suspend, bwi_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	error = bwi_attach(sc);
	if (error)
		goto fail;
	return;

fail:
	if (sc->sc_ih) {
		pci_intr_disestablish(psc->psc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (psc->psc_mapsize) {
		bus_space_unmap(sc->sc_mem_bt, sc->sc_mem_bh, psc->psc_mapsize);
		psc->psc_mapsize = 0;
	}
	return;
}

int
bwi_pci_detach(device_t self, int flags)
{
	struct bwi_pci_softc *psc = device_private(self);
	struct bwi_softc *sc = &psc->psc_bwi;

	pmf_device_deregister(self);

	bwi_detach(sc);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(psc->psc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	return (0);
}

static void
bwi_pci_conf_write(void *sc, uint32_t reg, uint32_t val)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)sc;

	pci_conf_write(psc->psc_pc, psc->psc_pcitag, reg, val);
}

static uint32_t
bwi_pci_conf_read(void *sc, uint32_t reg)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)sc;

	return (pci_conf_read(psc->psc_pc, psc->psc_pcitag, reg));
}
