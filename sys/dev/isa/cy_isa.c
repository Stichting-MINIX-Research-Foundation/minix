/*	$NetBSD: cy_isa.c,v 1.23 2008/03/26 17:50:32 matt Exp $	*/

/*
 * cy.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cy_isa.c,v 1.23 2008/03/26 17:50:32 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>
#include <dev/ic/cyvar.h>

int	cy_isa_probe(device_t, cfdata_t, void *);
void	cy_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cy_isa, sizeof(struct cy_softc),
    cy_isa_probe, cy_isa_attach, NULL, NULL);

int
cy_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct cy_softc sc;
	int found;

	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	sc.sc_memt = ia->ia_memt;
	sc.sc_bustype = CY_BUSTYPE_ISA;

	/* Disallow wildcarded memory address. */
	if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM)
		return 0;
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return 0;

	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr, CY_MEMSIZE, 0,
	    &sc.sc_bsh) != 0)
		return 0;

	found = cy_find(&sc);

	bus_space_unmap(ia->ia_memt, sc.sc_bsh, CY_MEMSIZE);

	if (found) {
		ia->ia_niomem = 1;
		ia->ia_iomem[0].ir_size = CY_MEMSIZE;

		ia->ia_nirq = 1;

		ia->ia_nio = 0;
		ia->ia_ndrq = 0;
	}
	return (found);
}

void
cy_isa_attach(device_t parent, device_t self, void *aux)
{
	struct cy_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_memt = ia->ia_memt;
	sc->sc_bustype = CY_BUSTYPE_ISA;

	printf(": Cyclades-Y multiport serial\n");

	if (bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr, CY_MEMSIZE, 0,
	    &sc->sc_bsh) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map device registers\n");
		return;
	}

	if (cy_find(sc) == 0) {
		aprint_error_dev(sc->sc_dev, "unable to find CD1400s\n");
		return;
	}

	cy_attach(sc);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_TTY, cy_intr, sc);
	if (sc->sc_ih == NULL)
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt\n");
}
