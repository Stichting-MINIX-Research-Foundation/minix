/*	$NetBSD: if_malo_pci.c,v 1.5 2014/03/29 19:28:25 christos Exp $	*/
/*	$OpenBSD: if_malo_pci.c,v 1.6 2010/08/28 23:19:29 deraadt Exp $ */

/*
 * Copyright (c) 2006 Marcus Glocker <mglocker@openbsd.org>
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
 * PCI front-end for the Marvell Libertas
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_malo_pci.c,v 1.5 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/bus.h>

#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <netinet/in.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/malovar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* Base Address Register */
#define MALO_PCI_BAR1	0x10
#define MALO_PCI_BAR2	0x14

static int malo_pci_match(device_t parent, cfdata_t match, void *aux);
static void	malo_pci_attach(device_t, device_t, void *);
static int	malo_pci_detach(device_t, int);
static bool malo_pci_suspend(device_t, const pmf_qual_t *);
static bool malo_pci_resume(device_t, const pmf_qual_t *);

struct malo_pci_softc {
	struct malo_softc	sc_malo;

	pci_chipset_tag_t        sc_pc;
	void 			*sc_ih;

	bus_size_t		 sc_mapsize1;
	bus_size_t		 sc_mapsize2;
};

CFATTACH_DECL_NEW(malo_pci, sizeof(struct malo_pci_softc),
    malo_pci_match, malo_pci_attach, malo_pci_detach, NULL);

static int
malo_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_MARVELL)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_MARVELL_88W8310:
	case PCI_PRODUCT_MARVELL_88W8335_1:
	case PCI_PRODUCT_MARVELL_88W8335_2:
		return (1);
	}

	return (0);
}

static void
malo_pci_attach(device_t parent, device_t self, void *aux)
{
	struct malo_pci_softc *psc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct malo_softc *sc = &psc->sc_malo;
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	pcireg_t memtype1, memtype2;
	int error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	psc->sc_pc = pa->pa_pc;

	aprint_normal("\n");
	aprint_normal_dev(self,"Marvell Libertas Wireless\n");

	/* map control / status registers */
	memtype1 = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MALO_PCI_BAR1);
	switch (memtype1) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		aprint_error_dev(self, "invalid base address register\n");
		return;
	}

	error = pci_mapreg_map(pa, MALO_PCI_BAR1,
	    memtype1, 0, &sc->sc_mem1_bt, &sc->sc_mem1_bh,
		NULL, &psc->sc_mapsize1);
	if (error != 0) {
		aprint_error_dev(self, "can't map 1st mem space\n");
		return;
	}

	/* map control / status registers */
	memtype2 = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MALO_PCI_BAR1);
	switch (memtype2) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		aprint_error_dev(self, "invalid base address register\n");
		return;
	}

	error = pci_mapreg_map(pa, MALO_PCI_BAR2,
	    memtype2, 0, &sc->sc_mem2_bt, &sc->sc_mem2_bh,
		NULL, &psc->sc_mapsize2);
	if (error != 0) {
		aprint_error_dev(self, "can't map 2nd mem space\n");
		return;
	}

	/* map interrupt */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}

	/* establish interrupt */
	intrstr = pci_intr_string(psc->sc_pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(psc->sc_pc, ih, IPL_NET, malo_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	malo_attach(sc);

	if (pmf_device_register(self, malo_pci_suspend, malo_pci_resume))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
malo_pci_detach(device_t self, int flags)
{
	struct malo_pci_softc *psc = device_private(self);
	struct malo_softc *sc = &psc->sc_malo;

	malo_detach(sc);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return (0);
}

static bool
malo_pci_suspend(device_t self, const pmf_qual_t *qual)
{
	struct malo_pci_softc *psc = device_private(self);
	struct malo_softc *sc = &psc->sc_malo;
	struct ifnet *ifp = &sc->sc_if;

	malo_stop(ifp, 1);

	return true;
}

static bool
malo_pci_resume(device_t self, const pmf_qual_t *qual)
{
	struct malo_pci_softc *psc = device_private(self);
	struct malo_softc *sc = &psc->sc_malo;
	struct ifnet *ifp = &sc->sc_if;

	if (ifp->if_flags & IFF_UP)
		malo_init(ifp);

	return true;
}
