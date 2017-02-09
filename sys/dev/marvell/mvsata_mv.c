/*	$NetBSD: mvsata_mv.c,v 1.7 2014/03/15 13:33:48 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvsata_mv.c,v 1.7 2014/03/15 13:33:48 kiyohara Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#include <dev/ic/mvsatareg.h>
#include <dev/ic/mvsatavar.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include "locators.h"


#define MVSATAHC_SIZE			0x8000

#define MVSATAHC_NWINDOW		4

#define MVSATAHC_MICR			0x20 /* Main Interrupt Cause */
#define MVSATAHC_MIMR			0x24 /* Main Interrupt Mask */
#define MVSATAHC_MI_SATAERR(p)			(1 << ((p) * 2))
#define MVSATAHC_MI_SATADONE(p)			(1 << (((p) * 2) + 1))
#define MVSATAHC_MI_SATADMADONE(p)		(1 << ((p) + 4))
#define MVSATAHC_MI_SATACOALDONE		(1 << 8)
#define MVSATAHC_WCR(n)			(0x30 + (n) * 0x10) /* WinN Control */
#define MVSATAHC_WCR_WINEN			(1 << 0)
#define MVSATAHC_WCR_TARGET(t)			(((t) & 0xf) << 4)
#define MVSATAHC_WCR_ATTR(a)			(((a) & 0xff) << 8)
#define MVSATAHC_WCR_SIZE(s)			(((s) - 1) & 0xffff0000)
#define MVSATAHC_WBR(n)			(0x34 + (n) * 0x10) /* WinN Base */
#define MVSATAHC_WBR_BASE(b)			((b) & 0xffff0000)


static int mvsatahc_match(device_t, cfdata_t, void *);
static void mvsatahc_attach(device_t, device_t, void *);

static int mvsatahc_intr(void *);

static void mvsatahc_enable_intr(struct mvsata_port *, int);
static void mvsatahc_wininit(struct mvsata_softc *, enum marvell_tags *);

CFATTACH_DECL_NEW(mvsata_gt, sizeof(struct mvsata_softc),
    mvsatahc_match, mvsatahc_attach, NULL, NULL);
CFATTACH_DECL_NEW(mvsata_mbus, sizeof(struct mvsata_softc),
    mvsatahc_match, mvsatahc_attach, NULL, NULL);


struct mvsata_product mvsata_products[] = {
#if 0
	/* Discovery VI */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV64660, ?, ?, gen2?, 0 },
#endif

	/* Orion */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F5082, 1, 1, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F5182, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F6082, 1, 1, gen2e, 0 },

	/* Kirkwood */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F6192, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F6281, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88F6282, 1, 2, gen2e, 0 },

	/* Discovery Innovation */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78100, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78200, 1, 2, gen2e, 0 },

	/* Armada XP */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78130, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78160, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78230, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78260, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV78460, 1, 2, gen2e, 0 },

	/* Armada 370 */
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV6707, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV6710, 1, 2, gen2e, 0 },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_MV6W11, 1, 2, gen2e, 0 },
};


/* ARGSUSED */
static int
mvsatahc_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;
	int i;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		    return 0;

	for (i = 0; i < __arraycount(mvsata_products); i++)
		if (mva->mva_model == mvsata_products[i].model) {
			mva->mva_size = MVSATAHC_SIZE;
			return 1;
		}
	return 0;
}

/* ARGSUSED */
static void
mvsatahc_attach(device_t parent, device_t self, void *aux)
{
	struct mvsata_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	uint32_t mask;
	int port, i;

	aprint_normal(": Marvell Serial-ATA Host Controller (SATAHC)\n");
	aprint_naive("\n");

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_model = mva->mva_model;
	sc->sc_iot = mva->mva_iot;
        if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	sc->sc_dmat = mva->mva_dmat;
	sc->sc_enable_intr = mvsatahc_enable_intr;

	mvsatahc_wininit(sc, mva->mva_tags);

	for (i = 0; i < __arraycount(mvsata_products); i++)
		if (mva->mva_model == mvsata_products[i].model)
			break;
	KASSERT(i < __arraycount(mvsata_products));

	if (mvsata_attach(sc, &mvsata_products[i], NULL, NULL, 0) != 0)
		return;

	marvell_intr_establish(mva->mva_irq, IPL_BIO, mvsatahc_intr, sc);
	mask = 0;
	for (port = 0; port < sc->sc_port; port++)
		mask |=
		    MVSATAHC_MI_SATAERR(port) |
		    MVSATAHC_MI_SATADONE(port);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSATAHC_MIMR, mask);
}

static int
mvsatahc_intr(void *arg)
{
	struct mvsata_softc *sc = (struct mvsata_softc *)arg;
	struct mvsata_hc *mvhc = &sc->sc_hcs[0];
	uint32_t cause, handled = 0;

	cause = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSATAHC_MICR);
	if (cause & MVSATAHC_MI_SATAERR(0))
		handled |= mvsata_error(mvhc->hc_ports[0]);
	if (cause & MVSATAHC_MI_SATAERR(1))
		handled |= mvsata_error(mvhc->hc_ports[1]);
	if (cause & (MVSATAHC_MI_SATADONE(0) | MVSATAHC_MI_SATADONE(1)))
		handled |= mvsata_intr(mvhc);

	return handled;
}


static void
mvsatahc_enable_intr(struct mvsata_port *mvport, int on)
{
	struct mvsata_softc *sc =
	    device_private(mvport->port_ata_channel.ch_atac->atac_dev);
	uint32_t mask;

	mask = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSATAHC_MIMR);
	if (on)
		mask |= MVSATAHC_MI_SATADONE(mvport->port);
	else
		mask &= ~MVSATAHC_MI_SATADONE(mvport->port);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSATAHC_MIMR, mask);
}

static void
mvsatahc_wininit(struct mvsata_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_wdcdev.sc_atac.atac_dev);
	uint64_t base;
	uint32_t size;
	int window, target, attr, rv, i;

	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MVSATAHC_NWINDOW;
	    i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;
		if (base > 0xffffffffULL) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "tag %d address 0x%llx not support\n",
			    tags[i], base);
			continue;
		}

		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVSATAHC_WCR(window),
		    MVSATAHC_WCR_WINEN |
		    MVSATAHC_WCR_TARGET(target) |
		    MVSATAHC_WCR_ATTR(attr) |
		    MVSATAHC_WCR_SIZE(size));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVSATAHC_WBR(window), MVSATAHC_WBR_BASE(base));
		window++;
	}
	for (; window < MVSATAHC_NWINDOW; window++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVSATAHC_WCR(window), 0);
}
