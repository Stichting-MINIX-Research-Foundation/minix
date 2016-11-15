/*	$NetBSD: tpm_isa.c,v 1.2 2012/02/06 04:29:47 christos Exp $	*/

/*
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Jörg Höxer
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tpm_isa.c,v 1.2 2012/02/06 04:29:47 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/pmf.h>

#include <dev/ic/tpmreg.h>
#include <dev/ic/tpmvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

static int	tpm_isa_match(device_t, cfdata_t, void *);
static void	tpm_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tpm_isa, sizeof(struct tpm_softc),
    tpm_isa_match, tpm_isa_attach, NULL, NULL);

extern struct cfdriver tpm_cd;

static int
tpm_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t bt = ia->ia_memt;
	bus_space_handle_t bh;
	int rv;

	/* There can be only one. */
	if (tpm_cd.cd_devs && tpm_cd.cd_devs[0])
		return 0;

	if (tpm_legacy_probe(ia->ia_iot, ia->ia_io[0].ir_addr)) {
		ia->ia_io[0].ir_size = 2;
		return 1;
	}

	if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM)
		return 0;

	/* XXX: integer locator sign extension */
	if (bus_space_map(bt, (unsigned int)ia->ia_iomem[0].ir_addr, TPM_SIZE,
	    0, &bh))
		return 0;

	if ((rv = tpm_tis12_probe(bt, bh))) {
		ia->ia_nio = 0;
		ia->ia_io[0].ir_size = 0;
		ia->ia_iomem[0].ir_size = TPM_SIZE;
	}
	ia->ia_ndrq = 0;

	bus_space_unmap(bt, bh, TPM_SIZE);
	return rv;
}

static void
tpm_isa_attach(device_t parent, device_t self, void *aux)
{
	struct tpm_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_addr_t iobase;
	bus_size_t size;
	int rv;

	sc->sc_dev = self;

	if (tpm_legacy_probe(ia->ia_iot, ia->ia_io[0].ir_addr)) {
		sc->sc_bt = ia->ia_iot;
		iobase = (unsigned int)ia->ia_io[0].ir_addr;
		size = ia->ia_io[0].ir_size;
		sc->sc_batm = ia->ia_iot;
		sc->sc_init = tpm_legacy_init;
		sc->sc_start = tpm_legacy_start;
		sc->sc_read = tpm_legacy_read;
		sc->sc_write = tpm_legacy_write;
		sc->sc_end = tpm_legacy_end;
	} else {
		sc->sc_bt = ia->ia_memt;
		iobase = (unsigned int)ia->ia_iomem[0].ir_addr;
		size = TPM_SIZE;
		sc->sc_init = tpm_tis12_init;
		sc->sc_start = tpm_tis12_start;
		sc->sc_read = tpm_tis12_read;
		sc->sc_write = tpm_tis12_write;
		sc->sc_end = tpm_tis12_end;
	}

	if (bus_space_map(sc->sc_bt, iobase, size, 0, &sc->sc_bh)) {
		aprint_error_dev(sc->sc_dev, "cannot map registers\n");
		return;
	}

	if ((rv = (*sc->sc_init)(sc, ia->ia_irq[0].ir_irq,
	    device_xname(sc->sc_dev))) != 0) {
		bus_space_unmap(sc->sc_bt, sc->sc_bh, size);
		return;
	}

	/*
	 * Only setup interrupt handler when we have a vector and the
	 * chip is TIS 1.2 compliant.
	 */
	if (sc->sc_init == tpm_tis12_init &&
	    ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
	    (sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_TTY, tpm_intr, sc)) == NULL) {
		bus_space_unmap(sc->sc_bt, sc->sc_bh, TPM_SIZE);
		aprint_error_dev(sc->sc_dev, "cannot establish interrupt\n");
		return;
	}

	if (!pmf_device_register(sc->sc_dev, tpm_suspend, tpm_resume))
		aprint_error_dev(sc->sc_dev, "Cannot set power mgmt handler\n");
}
