/*	$NetBSD: nca_pci.c,v 1.2 2012/01/30 19:41:22 drochner Exp $  */

/*
 * Copyright (c) 2010 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI attachment for 5380-compatible Domex 536 SCSI controller,
 * found on Domex DMX-3191D.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nca_pci.c,v 1.2 2012/01/30 19:41:22 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

static int nca_pci_match(device_t, cfdata_t, void *);
static void nca_pci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(nca_pci, sizeof(struct ncr5380_softc),
    nca_pci_match, nca_pci_attach, NULL, NULL);

static int
nca_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_DOMEX &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_DOMEX_PCISCSI)
		return 1;
	return 0;
}

static void
nca_pci_attach(device_t parent, device_t self, void *aux)
{
	struct ncr5380_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;

	sc->sc_dev = self;

	pci_aprint_devinfo(pa, "SCSI controller");

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0,
            &sc->sc_regt, &sc->sc_regh, NULL, NULL)) {
                aprint_error_dev(self, "could not map IO space\n");
                return;
        }
	
	/* The Domex 536 seems to be driven by polling,
	 * don't bother mapping an interrupt handler.
	 */
	
	sc->sc_rev = NCR_VARIANT_CXD1180;
	sc->sci_r0 = 0;
	sc->sci_r1 = 1;
	sc->sci_r2 = 2;
	sc->sci_r3 = 3;
	sc->sci_r4 = 4;
	sc->sci_r5 = 5;
	sc->sci_r6 = 6;
	sc->sci_r7 = 7;

	sc->sc_pio_out = ncr5380_pio_out;
	sc->sc_pio_in =  ncr5380_pio_in;
	sc->sc_dma_alloc = NULL;
	sc->sc_dma_free  = NULL;
	sc->sc_dma_setup = NULL;
	sc->sc_dma_start = NULL;
	sc->sc_dma_poll  = NULL;
	sc->sc_dma_eop   = NULL;
	sc->sc_dma_stop  = NULL;
	sc->sc_intr_on   = NULL;
	sc->sc_intr_off  = NULL;

	sc->sc_flags |= NCR5380_FORCE_POLLING;

	sc->sc_min_dma_len = 0;

	sc->sc_adapter.adapt_request = ncr5380_scsipi_request;
	sc->sc_adapter.adapt_minphys = minphys;

	sc->sc_channel.chan_id = 7;

	ncr5380_attach(sc);
}
