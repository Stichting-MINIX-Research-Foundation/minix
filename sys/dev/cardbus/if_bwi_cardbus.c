/*	$NetBSD: if_bwi_cardbus.c,v 1.2 2012/04/12 12:52:58 nakayama Exp $ */
/*	$OpenBSD: if_bwi_cardbus.c,v 1.13 2010/08/06 05:26:24 mglocker Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
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
 * CardBus front end
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_bwi_cardbus.c,v 1.2 2012/04/12 12:52:58 nakayama Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/ic/bwireg.h>
#include <dev/ic/bwivar.h>

struct bwi_cardbus_softc {
	struct bwi_softc	 csc_bwi;

	/* cardbus specific goo */
	cardbus_devfunc_t	 csc_ct;
	pcitag_t		 csc_tag;

	bus_size_t		 csc_mapsize;
	pcireg_t		 csc_bar_val;
};

static int	bwi_cardbus_match(device_t, cfdata_t, void *);
static void	bwi_cardbus_attach(device_t, device_t, void *);
static int	bwi_cardbus_detach(device_t, int);
static void	bwi_cardbus_setup(struct bwi_cardbus_softc *);
static int	bwi_cardbus_enable(struct bwi_softc *, int);
static void	bwi_cardbus_disable(struct bwi_softc *, int);
static void	bwi_cardbus_conf_write(void *, uint32_t, uint32_t);
static uint32_t	bwi_cardbus_conf_read(void *, uint32_t);

CFATTACH_DECL_NEW(bwi_cardbus, sizeof (struct bwi_cardbus_softc),
    bwi_cardbus_match, bwi_cardbus_attach, bwi_cardbus_detach, NULL);

static int
bwi_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_BROADCOM) {
		switch (PCI_PRODUCT(ca->ca_id)) {
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
		default:
			return (0);
		}
	}

	return (0);
}

static void
bwi_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct bwi_cardbus_softc *csc = device_private(self);
	struct cardbus_attach_args *ca = aux;
	struct bwi_softc *sc = &csc->csc_bwi;
	cardbus_devfunc_t ct = ca->ca_ct;
	pcireg_t reg;
	bus_addr_t base;
	int error;

	aprint_naive("\n");
	aprint_normal(": Broadcom Wireless\n");

	sc->sc_dev = self;
	sc->sc_dmat = ca->ca_dmat;
	csc->csc_ct = ct;
	csc->csc_tag = ca->ca_tag;

	/* power management hooks */
	sc->sc_enable = bwi_cardbus_enable;
	sc->sc_disable = bwi_cardbus_disable;

	/* map control/status registers */
	error = Cardbus_mapreg_map(ct, BWI_PCIR_BAR,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->sc_mem_bt,
	    &sc->sc_mem_bh, &base, &csc->csc_mapsize);
	if (error != 0) {
		aprint_error_dev(self, "can't map mem space\n");
		return;
	}
	csc->csc_bar_val = base | PCI_MAPREG_TYPE_MEM;

	/* set up the PCI configuration registers */
	bwi_cardbus_setup(csc);

	/* we need to access Cardbus config space from the driver */
	sc->sc_conf_read = bwi_cardbus_conf_read;
	sc->sc_conf_write = bwi_cardbus_conf_write;

	reg = (sc->sc_conf_read)(sc, PCI_SUBSYS_ID_REG);

	sc->sc_pci_revid = PCI_REVISION(ca->ca_class);
	sc->sc_pci_did = PCI_PRODUCT(ca->ca_id);
	sc->sc_pci_subvid = PCI_VENDOR(reg);
	sc->sc_pci_subdid = PCI_PRODUCT(reg);

	if (pmf_device_register(self, bwi_suspend, bwi_resume))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	error = bwi_attach(sc);
	if (error != 0)
		bwi_cardbus_detach(self, 0);

	Cardbus_function_disable(ct);
}

static int
bwi_cardbus_detach(device_t self, int flags)
{
	struct bwi_cardbus_softc *csc = device_private(self);
	struct bwi_softc *sc = &csc->csc_bwi;
	cardbus_devfunc_t ct = csc->csc_ct;

#ifdef DIAGNOSTIC
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(self));
#endif

	pmf_device_deregister(self);

	bwi_detach(sc);

	/* unhook the interrupt handler */
	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	/* release bus space and close window */
	Cardbus_mapreg_unmap(ct, BWI_PCIR_BAR, sc->sc_mem_bt,
	    sc->sc_mem_bh, csc->csc_mapsize);

	return (0);
}

static void
bwi_cardbus_setup(struct bwi_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->csc_ct;
	pcireg_t reg;

	/* program the BAR */
	Cardbus_conf_write(ct, csc->csc_tag, BWI_PCIR_BAR, csc->csc_bar_val);

	/* enable the appropriate bits in the PCI CSR */
	reg = Cardbus_conf_read(ct, csc->csc_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	Cardbus_conf_write(ct, csc->csc_tag, PCI_COMMAND_STATUS_REG, reg);
}

static int
bwi_cardbus_enable(struct bwi_softc *sc, int pmf)
{
	struct bwi_cardbus_softc *csc = (struct bwi_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->csc_ct;

	if (!pmf) {
		/* power on the socket */
		Cardbus_function_enable(ct);

		/* setup the PCI configuration registers */
		bwi_cardbus_setup(csc);
	}

	/* map and establish the interrupt handler */
	sc->sc_ih = Cardbus_intr_establish(ct, IPL_NET, bwi_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to establish interrupt\n");
		if (!pmf)
			Cardbus_function_disable(ct);
		return (1);
	}

	return (0);
}

static void
bwi_cardbus_disable(struct bwi_softc *sc, int pmf)
{
	struct bwi_cardbus_softc *csc = (struct bwi_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->csc_ct;

	/* unhook the interrupt handler */
	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (!pmf) {
		/* power down the socket */
		Cardbus_function_disable(ct);
	}
}

static void
bwi_cardbus_conf_write(void *sc, uint32_t reg, uint32_t val)
{
	struct bwi_cardbus_softc *csc = (struct bwi_cardbus_softc *)sc;

	Cardbus_conf_write(csc->csc_ct, csc->csc_tag, reg, val);
}

static uint32_t
bwi_cardbus_conf_read(void *sc, uint32_t reg)
{
	struct bwi_cardbus_softc *csc = (struct bwi_cardbus_softc *)sc;

	return Cardbus_conf_read(csc->csc_ct, csc->csc_tag, reg);
}
