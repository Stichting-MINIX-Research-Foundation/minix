/*	$NetBSD: isa.c,v 1.138 2010/08/21 17:08:15 jmcneill Exp $	*/

/*-
 * Copyright (c) 1998, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by Jason R. Thorpe of Wasabi Systems, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isa.c,v 1.138 2010/08/21 17:08:15 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmareg.h>

#include "isadma.h"

#include "isapnp.h"
#if NISAPNP > 0
#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#endif

#include "locators.h"

int	isamatch(device_t, cfdata_t, void *);
void	isaattach(device_t, device_t, void *);
int	isadetach(device_t, int);
int	isarescan(device_t, const char *, const int *);
void	isachilddetached(device_t, device_t);
int	isaprint(void *, const char *);

CFATTACH_DECL2_NEW(isa, sizeof(struct isa_softc),
    isamatch, isaattach, isadetach, NULL, isarescan, isachilddetached);

void	isa_attach_knowndevs(struct isa_softc *);
void	isa_free_knowndevs(struct isa_softc *);

int	isasubmatch(device_t, cfdata_t, const int *, void *);
int	isasearch(device_t, cfdata_t, const int *, void *);

static int	isa_slotcount = -1;	/* -1 == don't know how many */

int
isamatch(device_t parent, cfdata_t cf, void *aux)
{
	/* XXX check other indicators */

	return (1);
}

void
isaattach(device_t parent, device_t self, void *aux)
{
	struct isa_softc *sc = device_private(self);
	struct isabus_attach_args *iba = aux;
	static const int wildcard[ISACF_NLOCS] = {
		ISACF_PORT_DEFAULT, ISACF_SIZE_DEFAULT,
		ISACF_IOMEM_DEFAULT, ISACF_IOSIZ_DEFAULT,
		ISACF_IRQ_DEFAULT, ISACF_DRQ_DEFAULT, ISACF_DRQ2_DEFAULT
	};

	TAILQ_INIT(&sc->sc_knowndevs);
	sc->sc_dynamicdevs = 0;

	sc->sc_dev = self;

	isa_attach_hook(parent, self, iba);
	aprint_naive("\n");
	aprint_normal("\n");

	/* XXX Add code to fetch known-devices. */

	sc->sc_iot = iba->iba_iot;
	sc->sc_memt = iba->iba_memt;
	sc->sc_dmat = iba->iba_dmat;
	sc->sc_ic = iba->iba_ic;

#if NISAPNP > 0
	/*
	 * Reset isapnp cards that the bios configured for us
	 */
	isapnp_isa_attach_hook(sc);
#endif

#if NISADMA > 0
	/*
	 * Initialize our DMA state.
	 */
	isa_dmainit(sc->sc_ic, sc->sc_iot, sc->sc_dmat, self);
#endif

	/* Attach all direct-config children. */
	isa_attach_knowndevs(sc);

	/*
	 * If we don't support dynamic hello/goodbye of devices,
	 * then free the knowndevs info now.
	 */
	if (sc->sc_dynamicdevs == 0)
		isa_free_knowndevs(sc);

	/* Attach all indirect-config children. */
	isarescan(self, "isa", wildcard);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
isadetach(device_t self, int flags)
{
	struct isa_softc *sc = device_private(self);
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	pmf_device_deregister(self);

	isa_free_knowndevs(sc);

#if NISADMA > 0
	isa_dmadestroy(sc->sc_ic);
#endif
	isa_detach_hook(sc->sc_ic, self);

	return 0;
}

int
isarescan(device_t self, const char *ifattr, const int *locators)
{
	prop_dictionary_t dict;
	int locs[ISACF_NLOCS];
	bool no_legacy_devices = false;

	dict = device_properties(self);
	if (prop_dictionary_get_bool(dict, "no-legacy-devices",
	    &no_legacy_devices) == true) {
		aprint_debug_dev(self, "platform reports no legacy devices\n");
		return 0;
	}

	memcpy(locs, locators, sizeof(locs));

	/*
	 * XXX Bus independent code calling this function does not
	 * know the locator default values. It assumes "-1" for now.
	 * (should be made available by "config" one day)
	 * So fixup where the "-1" is not correct.
	 */
	if (locs[ISACF_SIZE] == -1)
		locs[ISACF_SIZE] = ISACF_SIZE_DEFAULT;
	if (locs[ISACF_IOSIZ] == -1)
		locs[ISACF_IOSIZ] = ISACF_IOSIZ_DEFAULT;

	config_search_loc(isasearch, self, ifattr, locs, NULL);
	return (0);
}

void
isachilddetached(device_t self, device_t child)
{
	struct isa_knowndev *ik;
	struct isa_softc *sc = device_private(self);

	TAILQ_FOREACH(ik, &sc->sc_knowndevs, ik_list) {
		if (ik->ik_claimed == child)
			ik->ik_claimed = NULL;
	}
}

void
isa_attach_knowndevs(struct isa_softc *sc)
{
	struct isa_attach_args ia;
	struct isa_knowndev *ik;

	if (TAILQ_EMPTY(&sc->sc_knowndevs))
		return;

	TAILQ_FOREACH(ik, &sc->sc_knowndevs, ik_list) {
		if (ik->ik_claimed != NULL)
			continue;

		ia.ia_iot = sc->sc_iot;
		ia.ia_memt = sc->sc_memt;
		ia.ia_dmat = sc->sc_dmat;
		ia.ia_ic = sc->sc_ic;

		ia.ia_pnpname = ik->ik_pnpname;
		ia.ia_pnpcompatnames = ik->ik_pnpcompatnames;

		ia.ia_io = ik->ik_io;
		ia.ia_nio = ik->ik_nio;

		ia.ia_iomem = ik->ik_iomem;
		ia.ia_niomem = ik->ik_niomem;

		ia.ia_irq = ik->ik_irq;
		ia.ia_nirq = ik->ik_nirq;

		ia.ia_drq = ik->ik_drq;
		ia.ia_ndrq = ik->ik_ndrq;

		ia.ia_aux = NULL;

		/* XXX should setup locator array */

		ik->ik_claimed = config_found_sm_loc(sc->sc_dev,
		    "isa", 0, &ia, isaprint, isasubmatch);
	}
}

void
isa_free_knowndevs(struct isa_softc *sc)
{
	struct isa_knowndev *ik;
	struct isa_pnpname *ipn;

#define	FREEIT(x)	if (x != NULL) free(x, M_DEVBUF)

	while ((ik = TAILQ_FIRST(&sc->sc_knowndevs)) != NULL) {
		TAILQ_REMOVE(&sc->sc_knowndevs, ik, ik_list);
		FREEIT(ik->ik_pnpname);
		while ((ipn = ik->ik_pnpcompatnames) != NULL) {
			ik->ik_pnpcompatnames = ipn->ipn_next;
			free(ipn->ipn_name, M_DEVBUF);
			free(ipn, M_DEVBUF);
		}
		FREEIT(ik->ik_io);
		FREEIT(ik->ik_iomem);
		FREEIT(ik->ik_irq);
		FREEIT(ik->ik_drq);
		free(ik, M_DEVBUF);
	}

#undef FREEIT
}

static int
checkattachargs(struct isa_attach_args *ia, const int *loc)
{
	int i;

	if (ia->ia_nio == 0) {
		if (loc[ISACF_PORT] != ISACF_PORT_DEFAULT)
			return (0);
	} else {
		if (loc[ISACF_PORT] != ISACF_PORT_DEFAULT &&
		    loc[ISACF_PORT] != ia->ia_io[0].ir_addr)
			return (0);
	}

	if (ia->ia_niomem == 0) {
		if (loc[ISACF_IOMEM] != ISACF_IOMEM_DEFAULT)
			return (0);
	} else {
		if (loc[ISACF_IOMEM] != ISACF_IOMEM_DEFAULT &&
		    loc[ISACF_IOMEM] != ia->ia_iomem[0].ir_addr)
			return (0);
	}

	if (ia->ia_nirq == 0) {
		if (loc[ISACF_IRQ] != ISACF_IRQ_DEFAULT)
			return (0);
	} else {
		if (loc[ISACF_IRQ] != ISACF_IRQ_DEFAULT &&
		    loc[ISACF_IRQ] != ia->ia_irq[0].ir_irq)
			return (0);
	}

	if (ia->ia_ndrq == 0) {
		if (loc[ISACF_DRQ] != ISACF_DRQ_DEFAULT)
			return (0);
		if (loc[ISACF_DRQ2] != ISACF_DRQ2_DEFAULT)
			return (0);
	} else {
		for (i = 0; i < 2; i++) {
			if (i == ia->ia_ndrq)
				break;
			if (loc[ISACF_DRQ + i] != ISACF_DRQ_DEFAULT &&
			    loc[ISACF_DRQ + i] != ia->ia_drq[i].ir_drq)
				return (0);
		}
		for (; i < 2; i++) {
			if (loc[ISACF_DRQ + i] != ISACF_DRQ_DEFAULT)
				return (0);
		}
	}

	return (1);
}

int
isasubmatch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct isa_attach_args *ia = aux;

	if (!checkattachargs(ia, cf->cf_loc))
		return (0);

	return (config_match(parent, cf, aux));
}

int
isaprint(void *aux, const char *isa)
{
	struct isa_attach_args *ia = aux;
	const char *sep;
	int i;

	/*
	 * This block of code only fires if we have a direct-config'd
	 * device for which there is no driver match.
	 */
	if (isa != NULL) {
		struct isa_pnpname *ipn;

		if (ia->ia_pnpname != NULL)
			aprint_normal("%s", ia->ia_pnpname);
		if ((ipn = ia->ia_pnpcompatnames) != NULL) {
			aprint_normal(" (");	/* ) */
			for (sep = ""; ipn != NULL;
			     ipn = ipn->ipn_next, sep = " ") {
				aprint_normal("%s%s", sep, ipn->ipn_name);
			}
	/* ( */		aprint_normal(")");
		}
		aprint_normal(" at %s", isa);
	}

	if (ia->ia_nio) {
		sep = "";
		aprint_normal(" port ");
		for (i = 0; i < ia->ia_nio; i++) {
			if (ia->ia_io[i].ir_size == 0)
				continue;
			aprint_normal("%s0x%x", sep, ia->ia_io[i].ir_addr);
			if (ia->ia_io[i].ir_size > 1)
				aprint_normal("-0x%x", ia->ia_io[i].ir_addr +
				    ia->ia_io[i].ir_size - 1);
			sep = ",";
		}
	}

	if (ia->ia_niomem) {
		sep = "";
		aprint_normal(" iomem ");
		for (i = 0; i < ia->ia_niomem; i++) {
			if (ia->ia_iomem[i].ir_size == 0)
				continue;
			aprint_normal("%s0x%x", sep, ia->ia_iomem[i].ir_addr);
			if (ia->ia_iomem[i].ir_size > 1)
				aprint_normal("-0x%x", ia->ia_iomem[i].ir_addr +
				    ia->ia_iomem[i].ir_size - 1);
			sep = ",";
		}
	}

	if (ia->ia_nirq) {
		sep = "";
		aprint_normal(" irq ");
		for (i = 0; i < ia->ia_nirq; i++) {
			if (ia->ia_irq[i].ir_irq == ISACF_IRQ_DEFAULT)
				continue;
			aprint_normal("%s%d", sep, ia->ia_irq[i].ir_irq);
			sep = ",";
		}
	}

	if (ia->ia_ndrq) {
		sep = "";
		aprint_normal(" drq ");
		for (i = 0; i < ia->ia_ndrq; i++) {
			if (ia->ia_drq[i].ir_drq == ISACF_DRQ_DEFAULT)
				continue;
			aprint_normal("%s%d", sep, ia->ia_drq[i].ir_drq);
			sep = ",";
		}
	}

	return (UNCONF);
}

int
isasearch(device_t parent, cfdata_t cf, const int *slocs, void *aux)
{
	struct isa_io res_io[1];
	struct isa_iomem res_mem[1];
	struct isa_irq res_irq[1];
	struct isa_drq res_drq[2];
	struct isa_softc *sc = device_private(parent);
	struct isa_attach_args ia;
	int flocs[ISACF_NLOCS];
	int tryagain;

	do {
		ia.ia_pnpname = NULL;
		ia.ia_pnpcompatnames = NULL;

		res_io[0].ir_addr = cf->cf_loc[ISACF_PORT];
		res_io[0].ir_size = 0;

		res_mem[0].ir_addr = cf->cf_loc[ISACF_IOMEM];
		res_mem[0].ir_size = cf->cf_loc[ISACF_IOSIZ];

		res_irq[0].ir_irq =
		    cf->cf_loc[ISACF_IRQ] == 2 ? 9 : cf->cf_loc[ISACF_IRQ];

		res_drq[0].ir_drq = cf->cf_loc[ISACF_DRQ];
		res_drq[1].ir_drq = cf->cf_loc[ISACF_DRQ2];

		ia.ia_iot = sc->sc_iot;
		ia.ia_memt = sc->sc_memt;
		ia.ia_dmat = sc->sc_dmat;
		ia.ia_ic = sc->sc_ic;

		ia.ia_io = res_io;
		ia.ia_nio = 1;

		ia.ia_iomem = res_mem;
		ia.ia_niomem = 1;

		ia.ia_irq = res_irq;
		ia.ia_nirq = 1;

		ia.ia_drq = res_drq;
		ia.ia_ndrq = 2;

		if (!checkattachargs(&ia, slocs))
			return (0);

		tryagain = 0;
		if (config_match(parent, cf, &ia) > 0) {
			/*
			 * This is not necessary for detach, but might
			 * still be useful to collect device information.
			 */
			flocs[ISACF_PORT] = ia.ia_io[0].ir_addr;
			flocs[ISACF_SIZE] = ia.ia_io[0].ir_size;
			flocs[ISACF_IOMEM] = ia.ia_iomem[0].ir_addr;
			flocs[ISACF_IOSIZ] = ia.ia_iomem[0].ir_size;
			flocs[ISACF_IRQ] = ia.ia_irq[0].ir_irq;
			flocs[ISACF_DRQ] = ia.ia_drq[0].ir_drq;
			flocs[ISACF_DRQ2] = ia.ia_drq[1].ir_drq;
			config_attach_loc(parent, cf, flocs, &ia, isaprint);
			tryagain = (cf->cf_fstate == FSTATE_STAR);
		}
	} while (tryagain);

	return (0);
}

const char *
isa_intr_typename(int type)
{

	switch (type) {
	case IST_NONE:
		return ("none");
	case IST_PULSE:
		return ("pulsed");
	case IST_EDGE:
		return ("edge-triggered");
	case IST_LEVEL:
		return ("level-triggered");
	default:
		panic("isa_intr_typename: invalid type %d", type);
	}
}

int
isa_get_slotcount(void)
{

	return isa_slotcount;
}

void
isa_set_slotcount(int arg)
{

	isa_slotcount = arg;
}
