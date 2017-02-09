/*	$NetBSD: iavc_pci.c,v 1.16 2014/03/29 19:28:24 christos Exp $	*/

/*
	char intrbuf[PCI_INTRSTR_LEN];
 * Copyright (c) 2001-2003 Cubical Solutions Ltd.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * capi/iavc/iavc_pci.c
 *		The AVM ISDN controllers' PCI bus attachment handling.
 *
 * $FreeBSD: src/sys/i4b/capi/iavc/iavc_pci.c,v 1.1.2.1 2001/08/10 14:08:34 obrien Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iavc_pci.c,v 1.16 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <net/if.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_l3l4.h>
#include <netisdn/i4b_capi.h>

#include <dev/ic/iavcvar.h>
#include <dev/ic/iavcreg.h>

struct iavc_pci_softc {
	struct iavc_softc sc_iavc;

	bus_addr_t mem_base;
	bus_size_t mem_size;
	bus_addr_t io_base;
	bus_size_t io_size;

	pci_chipset_tag_t sc_pc;

	void *sc_ih;		/* interrupt handler */
};
#define IAVC_PCI_IOBA	0x14
#define IAVC_PCI_MMBA	0x10

/* PCI driver linkage */

static const struct iavc_pci_product *find_cardname(struct pci_attach_args *);

static int iavc_pci_probe(device_t, cfdata_t, void *);
static void iavc_pci_attach(device_t, device_t, void *);

int iavc_pci_intr(void *);

CFATTACH_DECL_NEW(iavc_pci, sizeof(struct iavc_pci_softc),
    iavc_pci_probe, iavc_pci_attach, NULL, NULL);

static const struct iavc_pci_product {
	pci_vendor_id_t npp_vendor;
	pci_product_id_t npp_product;
	const char *name;
} iavc_pci_products[] = {
	{ PCI_VENDOR_AVM, PCI_PRODUCT_AVM_B1, "AVM B1 PCI" },
	{ PCI_VENDOR_AVM, PCI_PRODUCT_AVM_T1, "AVM T1 PCI" },
	{ 0, 0, NULL },
};

static const struct iavc_pci_product *
find_cardname(struct pci_attach_args * pa)
{
	const struct iavc_pci_product *pp = NULL;

	for (pp = iavc_pci_products; pp->npp_vendor; pp++) {
		if (PCI_VENDOR(pa->pa_id) == pp->npp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == pp->npp_product)
			return pp;
	}

	return NULL;
}

static int
iavc_pci_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (find_cardname(pa))
		return 1;

	return 0;
}

static void
iavc_pci_attach(device_t parent, device_t self, void *aux)
{
	struct iavc_pci_softc *psc = device_private(self);
	struct iavc_softc *sc = &psc->sc_iavc;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	const struct iavc_pci_product *pp;
	pci_intr_handle_t ih;
	const char *intrstr;
	int ret;
	char intrbuf[PCI_INTRSTR_LEN];

	pp = find_cardname(pa);
	if (pp == NULL)
		return;

	sc->sc_dev = self;
	sc->sc_t1 = 0;
	sc->sc_dma = 0;
	sc->dmat = pa->pa_dmat;

	if (pci_mapreg_map(pa, IAVC_PCI_IOBA, PCI_MAPREG_TYPE_IO, 0,
		&sc->sc_io_bt, &sc->sc_io_bh, &psc->io_base, &psc->io_size)) {
		aprint_error(": unable to map i/o registers\n");
		return;
	}

	if (pci_mapreg_map(pa, IAVC_PCI_MMBA, PCI_MAPREG_TYPE_MEM, 0,
	     &sc->sc_mem_bt, &sc->sc_mem_bh, &psc->mem_base, &psc->mem_size)) {
		aprint_error(": unable to map mem registers\n");
		return;
	}
	aprint_normal(": %s\n", pp->name);

	iavc_b1dma_reset(sc);

	if (pp->npp_product == PCI_PRODUCT_AVM_T1) {
		aprint_error_dev(sc->sc_dev, "sorry, PRI not yet supported\n");
		return;

#if 0
		sc->sc_capi.card_type = CARD_TYPEC_AVM_T1_PCI;
		sc->sc_capi.sc_nbch = NBCH_PRI;
		ret = iavc_t1_detect(sc);
		if (ret) {
			if (ret < 6) {
				aprint_error_dev(sc->sc_dev, "no card detected?\n");
			} else {
				aprint_error_dev(sc->sc_dev, "black box not on\n");
			}
			return;
		} else {
			sc->sc_dma = 1;
			sc->sc_t1 = 1;
		}
#endif

	} else if (pp->npp_product == PCI_PRODUCT_AVM_B1) {
		sc->sc_capi.card_type = CARD_TYPEC_AVM_B1_PCI;
		sc->sc_capi.sc_nbch = NBCH_BRI;
		ret = iavc_b1dma_detect(sc);
		if (ret) {
			ret = iavc_b1_detect(sc);
			if (ret) {
				aprint_error_dev(sc->sc_dev, "no card detected?\n");
				return;
			}
		} else {
			sc->sc_dma = 1;
		}
	}
	if (sc->sc_dma)
		iavc_b1dma_reset(sc);

#if 0
	/*
         * XXX: should really be done this way, but this freezes the card
         */
	if (sc->sc_t1)
		iavc_t1_reset(sc);
	else
		iavc_b1_reset(sc);
#endif

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, iavc_pci_intr, psc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	psc->sc_pc = pc;
	aprint_normal("%s: interrupting at %s\n", device_xname(sc->sc_dev), intrstr);

	memset(&sc->sc_txq, 0, sizeof(struct ifqueue));
	sc->sc_txq.ifq_maxlen = sc->sc_capi.sc_nbch * 4;

	sc->sc_intr = 0;
	sc->sc_state = IAVC_DOWN;
	sc->sc_blocked = 0;

	/* setup capi link */
	sc->sc_capi.load = iavc_load;
	sc->sc_capi.reg_appl = iavc_register;
	sc->sc_capi.rel_appl = iavc_release;
	sc->sc_capi.send = iavc_send;
	sc->sc_capi.ctx = (void *) sc;

	/* lock & load DMA for TX */
	if ((ret = bus_dmamem_alloc(sc->dmat, IAVC_DMA_SIZE, PAGE_SIZE, 0,
	    &sc->txseg, 1, &sc->ntxsegs, BUS_DMA_ALLOCNOW)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't allocate tx DMA memory, error = %d\n",
		    ret);
		goto fail1;
	}

	if ((ret = bus_dmamem_map(sc->dmat, &sc->txseg, sc->ntxsegs,
	    IAVC_DMA_SIZE, &sc->sc_sendbuf, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't map tx DMA memory, error = %d\n",
		    ret);
		goto fail2;
	}

	if ((ret = bus_dmamap_create(sc->dmat, IAVC_DMA_SIZE, 1,
	    IAVC_DMA_SIZE, 0, BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
	    &sc->tx_map)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't create tx DMA map, error = %d\n",
		    ret);
		goto fail3;
	}

	if ((ret = bus_dmamap_load(sc->dmat, sc->tx_map, sc->sc_sendbuf,
	    IAVC_DMA_SIZE, NULL, BUS_DMA_WRITE | BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't load tx DMA map, error = %d\n",
		    ret);
		goto fail4;
	}

	/* do the same for RX */
	if ((ret = bus_dmamem_alloc(sc->dmat, IAVC_DMA_SIZE, PAGE_SIZE, 0,
	    &sc->rxseg, 1, &sc->nrxsegs, BUS_DMA_ALLOCNOW)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't allocate rx DMA memory, error = %d\n",
		    ret);
		goto fail5;
	}

	if ((ret = bus_dmamem_map(sc->dmat, &sc->rxseg, sc->nrxsegs,
	    IAVC_DMA_SIZE, &sc->sc_recvbuf, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't map rx DMA memory, error = %d\n",
		    ret);
		goto fail6;
	}

	if ((ret = bus_dmamap_create(sc->dmat, IAVC_DMA_SIZE, 1, IAVC_DMA_SIZE,
	    0, BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT, &sc->rx_map)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't create rx DMA map, error = %d\n",
		    ret);
		goto fail7;
	}

	if ((ret = bus_dmamap_load(sc->dmat, sc->rx_map, sc->sc_recvbuf,
	    IAVC_DMA_SIZE, NULL, BUS_DMA_READ | BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't load rx DMA map, error = %d\n",
		    ret);
		goto fail8;
	}

	if (capi_ll_attach(&sc->sc_capi, device_xname(sc->sc_dev), pp->name)) {
		aprint_error_dev(sc->sc_dev, "capi attach failed\n");
		goto fail9;
	}
	return;

	/* release resources in case of failed attach */
fail9:
	bus_dmamap_unload(sc->dmat, sc->rx_map);
fail8:
	bus_dmamap_destroy(sc->dmat, sc->rx_map);
fail7:
	bus_dmamem_unmap(sc->dmat, sc->sc_recvbuf, IAVC_DMA_SIZE);
fail6:
	bus_dmamem_free(sc->dmat, &sc->rxseg, sc->nrxsegs);
fail5:
	bus_dmamap_unload(sc->dmat, sc->tx_map);
fail4:
	bus_dmamap_destroy(sc->dmat, sc->tx_map);
fail3:
	bus_dmamem_unmap(sc->dmat, sc->sc_sendbuf, IAVC_DMA_SIZE);
fail2:
	bus_dmamem_free(sc->dmat, &sc->txseg, sc->ntxsegs);
fail1:
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return;
}

int
iavc_pci_intr(void *arg)
{
	struct iavc_softc *sc = arg;

	return iavc_handle_intr(sc);
}

#if 0
static int
iavc_pci_detach(device_t self, int flags)
{
	struct iavc_pci_softc *psc = device_private(self);

	bus_space_unmap(psc->sc_iavc.sc_mem_bt, psc->sc_iavc.sc_mem_bh,
	    psc->mem_size);
	bus_space_free(psc->sc_iavc.sc_mem_bt, psc->sc_iavc.sc_mem_bh,
	    psc->mem_size);
	bus_space_unmap(psc->sc_iavc.sc_io_bt, psc->sc_iavc.sc_io_bh,
	    psc->io_size);
	bus_space_free(psc->sc_iavc.sc_io_bt, psc->sc_iavc.sc_io_bh,
	    psc->io_size);

	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	/* XXX: capi detach?!? */

	return 0;
}
#endif
