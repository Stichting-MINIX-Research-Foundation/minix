/*	$NetBSD: sdhc_cardbus.c,v 1.5 2012/12/20 14:37:00 jakllsch Exp $	*/

/*
 * Copyright (c) 2010 NONAKA Kimihiro <nonaka@netbsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdhc_cardbus.c,v 1.5 2012/12/20 14:37:00 jakllsch Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pmf.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcvar.h>

/* PCI interface classes */
#define SDHC_PCI_INTERFACE_NO_DMA	0x00
#define SDHC_PCI_INTERFACE_DMA		0x01
#define SDHC_PCI_INTERFACE_VENDOR	0x02

/*
 * 8-bit PCI configuration register that tells us how many slots there
 * are and which BAR entry corresponds to the first slot.
 */
#define SDHC_PCI_CONF_SLOT_INFO		0x40
#define SDHC_PCI_NUM_SLOTS(info)	((((info) >> 4) & 0x7) + 1)
#define SDHC_PCI_FIRST_BAR(info)	((info) & 0x7)

struct sdhc_cardbus_softc {
	struct sdhc_softc sc;
	cardbus_chipset_tag_t sc_cc;
	cardbus_function_tag_t sc_cf;
	cardbus_devfunc_t sc_ct;
	pcitag_t sc_tag;
	bus_space_tag_t sc_iot;		/* CardBus I/O space tag */
	bus_space_tag_t sc_memt;	/* CardBus MEM space tag */

	void *sc_ih;
};

static int sdhc_cardbus_match(device_t, cfdata_t, void *);
static void sdhc_cardbus_attach(device_t, device_t, void *);
static int sdhc_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(sdhc_cardbus, sizeof(struct sdhc_cardbus_softc),
    sdhc_cardbus_match, sdhc_cardbus_attach, sdhc_cardbus_detach, NULL);

#ifdef SDHC_DEBUG
#define	DPRINTF(s)	printf s
#else
#define	DPRINTF(s)	/**/
#endif

static int
sdhc_cardbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_CLASS(ca->ca_class) == PCI_CLASS_SYSTEM &&
	    PCI_SUBCLASS(ca->ca_class) == PCI_SUBCLASS_SYSTEM_SDHC)
		return 3;

	return 0;
}

static void
sdhc_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct sdhc_cardbus_softc *sc = device_private(self);
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t csr;
	pcireg_t slotinfo;
	char devinfo[256];
	int nslots;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t size;

	sc->sc.sc_dev = self;
	sc->sc.sc_dmat = ca->ca_dmat;
	sc->sc.sc_host = NULL;

	sc->sc_cc = cc;
	sc->sc_cf = cf;
	sc->sc_ct = ct;
	sc->sc_tag = ca->ca_tag;

	sc->sc_iot = ca->ca_iot;
	sc->sc_memt = ca->ca_memt;

	pci_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	aprint_normal(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(ca->ca_class));
	aprint_naive("\n");

	/*
	 * Map and attach all hosts supported by the host controller.
	 */
	slotinfo = Cardbus_conf_read(ct, ca->ca_tag, SDHC_PCI_CONF_SLOT_INFO);
	nslots = SDHC_PCI_NUM_SLOTS(slotinfo);
	KASSERT(nslots == 1);

	/* Allocate an array big enough to hold all the possible hosts */
	sc->sc.sc_host = malloc(sizeof(struct sdhc_host *) * nslots,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc.sc_host == NULL) {
		aprint_error_dev(self, "couldn't alloc memory\n");
		goto err;
	}

	/* Enable the device. */
	csr = Cardbus_conf_read(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG);
	Cardbus_conf_write(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE);

	/* Establish the interrupt. */
	sc->sc_ih = Cardbus_intr_establish(ct, IPL_SDMMC, sdhc_intr, &sc->sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		goto err;
	}

	/* Enable use of DMA if supported by the interface. */
	if ((PCI_INTERFACE(ca->ca_class) == SDHC_PCI_INTERFACE_DMA))
		SET(sc->sc.sc_flags, SDHC_FLAG_USE_DMA);

	if (Cardbus_mapreg_map(ct, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM, 0,
	    &iot, &ioh, NULL, &size)) {
		aprint_error_dev(self, "couldn't map register\n");
		goto err;
	}

	if (sdhc_host_found(&sc->sc, iot, ioh, size) != 0) {
		aprint_error_dev(self, "couldn't initialize host\n");
		goto err;
	}

	if (!pmf_device_register1(self, sdhc_suspend, sdhc_resume,
	    sdhc_shutdown)) {
		aprint_error_dev(self, "couldn't establish powerhook\n");
	}

	return;

err:
	if (sc->sc_ih != NULL)
		Cardbus_intr_disestablish(ct, sc->sc_ih);
	if (sc->sc.sc_host != NULL)
		free(sc->sc.sc_host, M_DEVBUF);
}

static int
sdhc_cardbus_detach(device_t self, int flags)
{
	struct sdhc_cardbus_softc *sc = device_private(self);
	struct cardbus_devfunc *ct = sc->sc_ct;
	int rv;

	rv = sdhc_detach(&sc->sc, flags);
	if (rv)
		return rv;
	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_host != NULL) {
		free(sc->sc.sc_host, M_DEVBUF);
		sc->sc.sc_host = NULL;
	}
	return 0;
}
