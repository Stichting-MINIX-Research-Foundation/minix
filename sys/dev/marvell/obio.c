/*	$NetBSD: obio.c,v 1.15 2010/07/11 08:43:36 kiyohara Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: obio.c,v 1.15 2010/07/11 08:43:36 kiyohara Exp $");

#include "opt_marvell.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/extent.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <dev/pci/pcivar.h>
#include <dev/marvell/gtreg.h>
#include <dev/marvell/gtvar.h>
#include <dev/marvell/gtdevbusvar.h>
#include <dev/marvell/marvellvar.h>

#include <prop/proplib.h>

#ifdef DEBUG
#include <sys/systm.h>	/* for Debugger() */
#endif

#include "locators.h"


static int obio_match(device_t, cfdata_t, void *);
static void obio_attach(device_t, device_t, void *);

static int obio_cfprint(void *, const char *);
static int obio_cfsearch(device_t, cfdata_t, const int *, void *);


struct obio_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;
};

CFATTACH_DECL_NEW(obio, sizeof(struct obio_softc),
    obio_match, obio_attach, NULL, NULL);


/* ARGSUSED */
int
obio_match(device_t parent, cfdata_t cf, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, cf->cf_name) != 0)
		return 0;

#define NUM_OBIO	5
	if (mva->mva_unit == MVA_UNIT_DEFAULT ||
	    mva->mva_unit > NUM_OBIO)
		return 0;

	return 1;
}

/* ARGSUSED */
void
obio_attach(device_t parent, device_t self, void *aux)
{
	struct obio_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	prop_data_t bst;
	uint32_t datal, datah;

	aprint_naive("\n");
	aprint_normal(": Device Bus\n");

	sc->sc_dev = self;

	if (gt_devbus_addr(parent, mva->mva_unit, &datal, &datah)) {
		aprint_error_dev(self, "unknown unit number %d\n",
		    mva->mva_unit);
		return;
	}

	if (GT_LowAddr_GET(datal) > GT_HighAddr_GET(datah)) {
		aprint_normal_dev(self, "disabled\n");
		return;
	}

	bst = prop_dictionary_get(device_properties(sc->sc_dev), "bus-tag");
	if (bst != NULL) {
		KASSERT(prop_object_type(bst) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(bst) == sizeof(bus_space_tag_t));
		memcpy(&sc->sc_iot, prop_data_data_nocopy(bst),
		    sizeof(bus_space_tag_t));
	} else
		sc->sc_iot = mva->mva_iot;
	if (sc->sc_iot == NULL) {
		aprint_normal_dev(self, "unused\n");
		return;
	}

	aprint_normal_dev(self, "addr %#x-%#x\n",
	    GT_LowAddr_GET(datal), GT_HighAddr_GET(datah));

        config_search_ia(obio_cfsearch, self, "obio", NULL);
}


int
obio_cfprint(void *aux, const char *pnp)
{
	struct obio_attach_args *oa = aux;

	if (pnp)
		aprint_normal("%s at %s", oa->oa_name, pnp);
	aprint_normal(" addr %#x size %#x", oa->oa_offset, oa->oa_size);
	if (oa->oa_irq != OBIOCF_IRQ_DEFAULT)
		aprint_normal(" irq %d", oa->oa_irq);

	return UNCONF;
}


/* ARGSUSED */
int
obio_cfsearch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct obio_softc *sc = device_private(parent);
	struct obio_attach_args oa;

	oa.oa_name = cf->cf_name;
	oa.oa_memt = sc->sc_iot;
	oa.oa_offset = cf->cf_loc[OBIOCF_OFFSET];
	oa.oa_size = cf->cf_loc[OBIOCF_SIZE];
	oa.oa_irq = cf->cf_loc[OBIOCF_IRQ];

	if (config_match(parent, cf, &oa) > 0)
		config_attach(parent, cf, &oa, obio_cfprint);

	return 0;
}
