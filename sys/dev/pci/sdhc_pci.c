/*	$NetBSD: sdhc_pci.c,v 1.12 2015/08/09 13:27:48 mlelstv Exp $	*/
/*	$OpenBSD: sdhc_pci.c,v 1.7 2007/10/30 18:13:45 chl Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdhc_pci.c,v 1.12 2015/08/09 13:27:48 mlelstv Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pmf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcvar.h>

/* PCI base address registers */
#define SDHC_PCI_BAR_START		PCI_MAPREG_START
#define SDHC_PCI_BAR_END		PCI_MAPREG_END

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

struct sdhc_pci_softc {
	struct sdhc_softc sc;
	pci_chipset_tag_t sc_pc;
	void *sc_ih;
};

static int sdhc_pci_match(device_t, cfdata_t, void *);
static void sdhc_pci_attach(device_t, device_t, void *);
static int sdhc_pci_detach(device_t, int);

CFATTACH_DECL_NEW(sdhc_pci, sizeof(struct sdhc_pci_softc),
    sdhc_pci_match, sdhc_pci_attach, sdhc_pci_detach, NULL);

#ifdef SDHC_DEBUG
#define	DPRINTF(s)	printf s
#else
#define	DPRINTF(s)	/**/
#endif

static const struct sdhc_pci_quirk {
	pci_vendor_id_t		vendor;
	pci_product_id_t	product;
	pci_vendor_id_t		subvendor;
	pci_product_id_t	subproduct;
	u_int			function;

	uint32_t		flags;
#define	SDHC_PCI_QUIRK_FORCE_DMA		(1U << 0)
#define	SDHC_PCI_QUIRK_TI_HACK			(1U << 1)
#define	SDHC_PCI_QUIRK_NO_PWR0			(1U << 2)
#define	SDHC_PCI_QUIRK_RICOH_LOWER_FREQ_HACK	(1U << 3)
#define	SDHC_PCI_QUIRK_RICOH_SLOW_SDR50_HACK	(1U << 4)
} sdhc_pci_quirk_table[] = {
	{
		PCI_VENDOR_TI,
		PCI_PRODUCT_TI_PCI72111SD,
		0xffff,
		0xffff,
		4,
		SDHC_PCI_QUIRK_TI_HACK
	},

	{
		PCI_VENDOR_TI,
		PCI_PRODUCT_TI_PCIXX12SD,
		0xffff,
		0xffff,
		3,
		SDHC_PCI_QUIRK_TI_HACK
	},

	{
		PCI_VENDOR_ENE,
		PCI_PRODUCT_ENE_CB712,
		0xffff,
		0xffff,
		0,
		SDHC_PCI_QUIRK_NO_PWR0
	},
	{
		PCI_VENDOR_RICOH,
		PCI_PRODUCT_RICOH_Rx5U823,
		0xffff,
		0xffff,
		0,
		SDHC_PCI_QUIRK_RICOH_SLOW_SDR50_HACK
	},
	{
		PCI_VENDOR_RICOH,
		PCI_PRODUCT_RICOH_Rx5C822,
		0xffff,
		0xffff,
		~0,
		SDHC_PCI_QUIRK_FORCE_DMA
	},

	{
		PCI_VENDOR_RICOH,
		PCI_PRODUCT_RICOH_Rx5U822,
		0xffff,
		0xffff,
		~0,
		SDHC_PCI_QUIRK_FORCE_DMA
	},
};

static void sdhc_pci_quirk_ti_hack(struct pci_attach_args *);
static void sdhc_pci_quirk_ricoh_lower_freq_hack(struct pci_attach_args *);

static uint32_t
sdhc_pci_lookup_quirk_flags(struct pci_attach_args *pa)
{
	const struct sdhc_pci_quirk *q;
	pcireg_t id;
	pci_vendor_id_t vendor;
	pci_product_id_t product;
	int i;

	for (i = 0; i < __arraycount(sdhc_pci_quirk_table); i++) {
		q = &sdhc_pci_quirk_table[i];

		if ((PCI_VENDOR(pa->pa_id) == q->vendor)
		 && (PCI_PRODUCT(pa->pa_id) == q->product)) {
			if ((q->function != ~0)
			 && (pa->pa_function != q->function))
				continue;

			if ((q->subvendor == 0xffff)
			 && (q->subproduct == 0xffff))
				return (q->flags);

			id = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    PCI_SUBSYS_ID_REG);
			vendor = PCI_VENDOR(id);
			product = PCI_PRODUCT(id);

			if ((q->subvendor != 0xffff)
			 && (q->subproduct != 0xffff)) {
				if ((vendor == q->subvendor)
				 && (product == q->subproduct))
					return (q->flags);
			} else if (q->subvendor != 0xffff) {
				if (product == q->subproduct)
					return (q->flags);
			} else {
				if (vendor == q->subvendor)
					return (q->flags);
			}
		}
	}
	return (0);
}

static int
sdhc_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SYSTEM &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SYSTEM_SDHC)
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_RICOH &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RICOH_Rx5U822 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RICOH_Rx5U823))
		return (1);
	return (0);
}

static void
sdhc_pci_attach(device_t parent, device_t self, void *aux)
{
	struct sdhc_pci_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pci_intr_handle_t ih;
	pcireg_t csr;
	pcireg_t slotinfo;
	char const *intrstr;
	int nslots;
	int reg;
	int cnt;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t size;
	uint32_t flags;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc.sc_dev = self;
	sc->sc.sc_dmat = pa->pa_dmat;
	sc->sc.sc_host = NULL;

	sc->sc_pc = pc;

	pci_aprint_devinfo(pa, NULL);

	/* Some controllers needs special treatment. */
	flags = sdhc_pci_lookup_quirk_flags(pa);
	if (ISSET(flags, SDHC_PCI_QUIRK_TI_HACK))
		sdhc_pci_quirk_ti_hack(pa);
	if (ISSET(flags, SDHC_PCI_QUIRK_FORCE_DMA))
		SET(sc->sc.sc_flags, SDHC_FLAG_FORCE_DMA);
	if (ISSET(flags, SDHC_PCI_QUIRK_NO_PWR0))
		SET(sc->sc.sc_flags, SDHC_FLAG_NO_PWR0);
	if (ISSET(flags, SDHC_PCI_QUIRK_RICOH_LOWER_FREQ_HACK))
		sdhc_pci_quirk_ricoh_lower_freq_hack(pa);
	if (ISSET(flags, SDHC_PCI_QUIRK_RICOH_SLOW_SDR50_HACK))
		SET(sc->sc.sc_flags, SDHC_FLAG_SLOW_SDR50);

	/*
	 * Map and attach all hosts supported by the host controller.
	 */
	slotinfo = pci_conf_read(pc, tag, SDHC_PCI_CONF_SLOT_INFO);
	nslots = SDHC_PCI_NUM_SLOTS(slotinfo);

	/* Allocate an array big enough to hold all the possible hosts */
	sc->sc.sc_host = malloc(sizeof(struct sdhc_host *) * nslots,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc.sc_host == NULL) {
		aprint_error_dev(self, "couldn't alloc memory\n");
		goto err;
	}

	/* Enable the device. */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		goto err;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_SDMMC, sdhc_intr, &sc->sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		goto err;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* Enable use of DMA if supported by the interface. */
	if ((PCI_INTERFACE(pa->pa_class) == SDHC_PCI_INTERFACE_DMA))
		SET(sc->sc.sc_flags, SDHC_FLAG_USE_DMA);

	/* XXX: handle 64-bit BARs */
	cnt = 0;
	for (reg = SDHC_PCI_BAR_START + SDHC_PCI_FIRST_BAR(slotinfo) *
		 sizeof(uint32_t);
	     reg < SDHC_PCI_BAR_END && nslots > 0;
	     reg += sizeof(uint32_t), nslots--) {
		if (pci_mapreg_map(pa, reg, PCI_MAPREG_TYPE_MEM, 0,
		    &iot, &ioh, NULL, &size)) {
			continue;
		}

		cnt++;
		if (sdhc_host_found(&sc->sc, iot, ioh, size) != 0) {
			/* XXX: sc->sc_host leak */
			aprint_error_dev(self,
			    "couldn't initialize host (0x%x)\n", reg);
		}
	}
	if (cnt == 0) {
		aprint_error_dev(self, "couldn't map register\n");
		goto err;
	}

	if (!pmf_device_register1(self, sdhc_suspend, sdhc_resume,
	    sdhc_shutdown)) {
		aprint_error_dev(self, "couldn't establish powerhook\n");
	}

	return;

err:
	if (sc->sc.sc_host != NULL) {
		free(sc->sc.sc_host, M_DEVBUF);
		sc->sc.sc_host = NULL;
	}
}

static int
sdhc_pci_detach(device_t self, int flags)
{
	struct sdhc_pci_softc * const sc = device_private(self);
	int rv;

	rv = sdhc_detach(&sc->sc, flags);
	if (rv)
		return rv;

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_host != NULL) {
		free(sc->sc.sc_host, M_DEVBUF);
		sc->sc.sc_host = NULL;
	}
	
	return rv;
}

static void
sdhc_pci_conf_write(struct pci_attach_args *pa, int reg, uint8_t val)
{
	pcireg_t r;

	r = pci_conf_read(pa->pa_pc, pa->pa_tag, reg & ~0x3);
	r &= ~(0xff << ((reg & 0x3) * 8));
	r |= (val << ((reg & 0x3) * 8));
	pci_conf_write(pa->pa_pc, pa->pa_tag, reg & ~0x3, r);
}

/* TI specific register */
#define SDHC_PCI_GENERAL_CTL		0x4c
#define  MMC_SD_DIS			0x02

static void
sdhc_pci_quirk_ti_hack(struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag;
	pcireg_t id, reg;

	/* Look at func - 1 for the flash device */
	tag = pci_make_tag(pc, pa->pa_bus, pa->pa_device, pa->pa_function - 1);
	id = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(id) != PCI_VENDOR_TI) {
		return;
	}
	switch (PCI_PRODUCT(id)) {
	case PCI_PRODUCT_TI_PCI72111FM:
	case PCI_PRODUCT_TI_PCIXX12FM:
		break;
	default:
		return;
	}

	/*
	 * Disable MMC/SD on the flash media controller so the
	 * SD host takes over.
	 */
	reg = pci_conf_read(pc, tag, SDHC_PCI_GENERAL_CTL);
	reg |= MMC_SD_DIS;
	pci_conf_write(pc, tag, SDHC_PCI_GENERAL_CTL, reg);
}

/* Ricoh specific register */
#define SDHC_PCI_MODE_KEY		0xf9
#define SDHC_PCI_MODE			0x150
#define  SDHC_PCI_MODE_SD20		0x10
#define SDHC_PCI_BASE_FREQ_KEY		0xfc
#define SDHC_PCI_BASE_FREQ		0xe1

/* Some RICOH controllers need to be bumped into the right mode. */
static void
sdhc_pci_quirk_ricoh_lower_freq_hack(struct pci_attach_args *pa)
{
	/* Enable SD2.0 mode. */
	sdhc_pci_conf_write(pa, SDHC_PCI_MODE_KEY, 0xfc);
	sdhc_pci_conf_write(pa, SDHC_PCI_MODE, SDHC_PCI_MODE_SD20);
	sdhc_pci_conf_write(pa, SDHC_PCI_MODE_KEY, 0x00);

	/*
	 * Some SD/MMC cards don't work with the default base
	 * clock frequency of 200MHz.  Lower it to 50Hz.
	 */
	sdhc_pci_conf_write(pa, SDHC_PCI_BASE_FREQ_KEY, 0x01);
	sdhc_pci_conf_write(pa, SDHC_PCI_BASE_FREQ, 50);
	sdhc_pci_conf_write(pa, SDHC_PCI_BASE_FREQ_KEY, 0x00);
printf("quirked\n");
}
