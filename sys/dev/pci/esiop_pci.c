/*	$NetBSD: esiop_pci.c,v 1.18 2010/11/13 13:52:05 uebayasi Exp $	*/

/*
 * Copyright (c) 2002 Manuel Bouyer.
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

/* SYM53c8xx PCI-SCSI I/O Processors driver: PCI front-end */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esiop_pci.c,v 1.18 2010/11/13 13:52:05 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>

#include <dev/ic/siopvar_common.h>
#include <dev/pci/siop_pci_common.h>
#include <dev/ic/esiopvar.h>

struct esiop_pci_softc {
	struct esiop_softc esiop;
	struct siop_pci_common_softc esiop_pci;
};

static int
esiop_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct siop_product_desc *pp;

	/* look if it's a known product */
	pp = siop_lookup_product(pa->pa_id, PCI_REVISION(pa->pa_class));
	if (pp == NULL)
		return 0;
	/* we need 10 scratch registers and load/store */
	if ((pp->features & SF_CHIP_10REGS) == 0)
		return 0;
	if ((pp->features & SF_CHIP_LS) == 0)
		return 0;
	return 2;
}

static void
esiop_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct esiop_pci_softc *sc = device_private(self);

	sc->esiop.sc_c.sc_dev = self;
	if (siop_pci_attach_common(&sc->esiop_pci, &sc->esiop.sc_c,
	    pa, esiop_intr) == 0)
		return;

	esiop_attach(&sc->esiop);
}


CFATTACH_DECL_NEW(esiop_pci, sizeof(struct esiop_pci_softc),
    esiop_pci_match, esiop_pci_attach, NULL, NULL);
