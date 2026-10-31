/*	$NetBSD: pckbc_acpi.c,v 1.36 2017/10/28 04:53:55 riastradh Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ACPI attachment for the PC Keyboard Controller driver.
 *
 * This is a little wonky.  The keyboard controller actually
 * has 2 ACPI nodes: one for the controller and the keyboard
 * interrupt, and one for the aux port (mouse) interrupt.
 *
 * For this reason, we actually attach *two* instances of this
 * driver.  After both of them have been found, then we attach
 * sub-devices.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pckbc_acpi.c,v 1.36 2017/10/28 04:53:55 riastradh Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>

#include <dev/isa/isareg.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>

#include "ioconf.h"

static int	pckbc_acpi_match(device_t, cfdata_t, void *);
static void	pckbc_acpi_attach(device_t, device_t, void *);

struct pckbc_acpi_softc {
	struct pckbc_softc sc_pckbc;

	isa_chipset_tag_t sc_ic;
	int sc_irq;
	int sc_ist;
	pckbc_slot_t sc_slot;
};

/* Save first port: */
static struct pckbc_acpi_softc *first;

CFATTACH_DECL_NEW(pckbc_acpi, sizeof(struct pckbc_acpi_softc),
    pckbc_acpi_match, pckbc_acpi_attach, NULL, NULL);

static void	pckbc_acpi_intr_establish(struct pckbc_softc *, pckbc_slot_t);
static void	pckbc_acpi_finish_attach(device_t);

/*
 * Supported Device IDs
 */

static const char * const pckbc_acpi_ids_kbd[] = {
	"PNP03??",	/* Standard PC KBD port */
	NULL
};

static const char * const pckbc_acpi_ids_ms[] = {
	"PNP0F03",
	"PNP0F0E",
	"PNP0F12",
	"PNP0F13",
	"PNP0F19",
	"PNP0F1B",
	"PNP0F1C",
	"SYN0302",
	NULL
};

/*
 * pckbc_acpi_match: autoconf(9) match routine
 */
static int
pckbc_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	int rv;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	rv = acpi_match_hid(aa->aa_node->ad_devinfo, pckbc_acpi_ids_kbd);
	if (rv)
		return rv;
	rv = acpi_match_hid(aa->aa_node->ad_devinfo, pckbc_acpi_ids_ms);
	if (rv)
		return rv;
	return 0;
}

static void
pckbc_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct pckbc_acpi_softc *psc = device_private(self);
	struct pckbc_softc *sc = &psc->sc_pckbc;
	struct pckbc_internal *t;
	struct acpi_attach_args *aa = aux;
	bus_space_handle_t ioh_d, ioh_c;
	struct acpi_resources res;
	struct acpi_io *io0, *io1, *ioswap;
	struct acpi_irq *irq;
	ACPI_STATUS rv;

	sc->sc_dv = self;
	psc->sc_ic = aa->aa_ic;

	if (acpi_match_hid(aa->aa_node->ad_devinfo, pckbc_acpi_ids_kbd)) {
		psc->sc_slot = PCKBC_KBD_SLOT;
	} else if (acpi_match_hid(aa->aa_node->ad_devinfo, pckbc_acpi_ids_ms)) {
		psc->sc_slot = PCKBC_AUX_SLOT;
	} else {
		aprint_error(": unknown port!\n");
		panic("pckbc_acpi_attach: impossible");
	}

	aprint_normal(" (%s port)", pckbc_slot_names[psc->sc_slot]);

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dv, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "unable to find irq resource\n");
		goto out;
	}
	psc->sc_irq = irq->ar_irq;
	psc->sc_ist = (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL;

	if (psc->sc_slot == PCKBC_KBD_SLOT)
		first = psc;

	if ((!first || !first->sc_pckbc.id) &&
	    (psc->sc_slot == PCKBC_KBD_SLOT)) {

		io0 = acpi_res_io(&res, 0);
		io1 = acpi_res_io(&res, 1);
		if (io0 == NULL || io1 == NULL) {
			aprint_error_dev(self,
			    "unable to find i/o resources\n");
			goto out;
		}

		/*
		 * JDM: Some firmware doesn't report resources in the order we
		 * expect; sort IO resources here (lowest first)
		 */
		if (io0->ar_base > io1->ar_base) {
			ioswap = io0;
			io0 = io1;
			io1 = ioswap;
		}

		if (pckbc_is_console(aa->aa_iot, io0->ar_base)) {
			t = &pckbc_consdata;
			ioh_d = t->t_ioh_d;
			ioh_c = t->t_ioh_c;
			pckbc_console_attached = 1;
			/* t->t_cmdbyte was initialized by cnattach */
		} else {
			if (bus_space_map(aa->aa_iot, io0->ar_base,
					  io0->ar_length, 0, &ioh_d) ||
			    bus_space_map(aa->aa_iot, io1->ar_base,
					  io1->ar_length, 0, &ioh_c)) {
				aprint_error_dev(self,
				    "unable to map registers\n");
				goto out;
			}

			t = malloc(sizeof(struct pckbc_internal),
			    M_DEVBUF, M_WAITOK|M_ZERO);
			t->t_iot = aa->aa_iot;
			t->t_ioh_d = ioh_d;
			t->t_ioh_c = ioh_c;
			t->t_addr = io0->ar_base;
			t->t_cmdbyte = KC8_CPU;	/* Enable ports */
			callout_init(&t->t_cleanup, 0);
		}

		t->t_sc = &first->sc_pckbc;
		first->sc_pckbc.id = t;

		if (!pmf_device_register(self, NULL, pckbc_resume))
			aprint_error_dev(self,
			    "couldn't establish power handler\n");

		first->sc_pckbc.intr_establish = pckbc_acpi_intr_establish;
		config_defer(first->sc_pckbc.sc_dv, pckbc_acpi_finish_attach);
	}

out:
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
	acpi_resource_cleanup(&res);
}

static void
pckbc_acpi_intr_establish(struct pckbc_softc *sc, pckbc_slot_t slot)
{
	struct pckbc_acpi_softc *psc;
	isa_chipset_tag_t ic = NULL;
	void *rv = NULL;
	int irq = 0, ist = 0; /* XXX: gcc */
	int i;

	/*
	 * Note we're always called with sc == first.
	 */
	for (i = 0; i < pckbc_cd.cd_ndevs; i++) {
		psc = device_lookup_private(&pckbc_cd, i);
		if (psc && psc->sc_slot == slot) {
			irq = psc->sc_irq;
			ist = psc->sc_ist;
			ic = psc->sc_ic;
			break;
		}
	}
	if (i < pckbc_cd.cd_ndevs) {
		char intr_xname[64];
		snprintf(intr_xname, sizeof(intr_xname), "%s %s",
		    device_xname(psc->sc_pckbc.sc_dv), pckbc_slot_names[slot]);

		rv = isa_intr_establish_xname(ic, irq, ist, IPL_TTY, pckbcintr,
		    sc, intr_xname);
	}
	if (rv == NULL) {
		aprint_error_dev(sc->sc_dv,
		    "unable to establish interrupt for %s slot\n",
		    pckbc_slot_names[slot]);
	} else {
		aprint_normal_dev(sc->sc_dv, "using irq %d for %s slot\n",
		    irq, pckbc_slot_names[slot]);
	}
}

static void
pckbc_acpi_finish_attach(device_t dv)
{

	pckbc_attach(device_private(dv));
}
