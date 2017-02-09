/*	$NetBSD: mca.c,v 1.31 2011/06/03 07:39:30 matt Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * Copyright (c) 1996-1999 Scott D. Telford.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Scott Telford <s.telford@ed.ac.uk>.
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
 * MCA Bus device
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mca.c,v 1.31 2011/06/03 07:39:30 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/mca_machdep.h>

#include <dev/mca/mcareg.h>
#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#include "locators.h"

int	mca_match(device_t, cfdata_t, void *);
void	mca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mca, 0,
    mca_match, mca_attach, NULL, NULL);

int	mca_print(void *, const char *);

int
mca_match(device_t parent, cfdata_t cf, void *aux)
{
	struct mcabus_attach_args *mba = aux;

	/* sanity (only mca0 supported currently) */
	if (mba->mba_bus < 0 || mba->mba_bus > 0)
		return (0);

	/* XXX check other indicators? */

	return (1);
}

int
mca_print(void *aux, const char *pnp)
{
	register struct mca_attach_args *ma = aux;
	char devinfo[256];

	if (pnp) {
		mca_devinfo(ma->ma_id, devinfo, sizeof(devinfo));
		aprint_normal("%s slot %d: %s", pnp, ma->ma_slot + 1, devinfo);
	}

	/*
	 * Print "configured" for Memory Extension boards - there is no
	 * meaningfull driver for them, they "just work".
	 */
	switch(ma->ma_id) {
	case MCA_PRODUCT_HRAM: case MCA_PRODUCT_IQRAM: case MCA_PRODUCT_MICRAM:
	case MCA_PRODUCT_ASTRAM: case MCA_PRODUCT_KINGRAM:
	case MCA_PRODUCT_KINGRAM8: case MCA_PRODUCT_KINGRAM16:
	case MCA_PRODUCT_KINGRAM609: case MCA_PRODUCT_HYPRAM:
	case MCA_PRODUCT_QRAM1: case MCA_PRODUCT_QRAM2: case MCA_PRODUCT_EVERAM:
	case MCA_PRODUCT_BOCARAM: case MCA_PRODUCT_IBMRAM1:
	case MCA_PRODUCT_IBMRAM2: case MCA_PRODUCT_IBMRAM3:
	case MCA_PRODUCT_IBMRAM4: case MCA_PRODUCT_IBMRAM5:
	case MCA_PRODUCT_IBMRAM6: case MCA_PRODUCT_IBMRAM7:
		aprint_normal(": memory configured\n");
		return (QUIET);
	default:
		return (UNCONF);
	}
}

void
mca_attach(device_t parent, device_t self, void *aux)
{
	struct mcabus_attach_args *mba = aux;
	bus_space_tag_t iot, memt;
	bus_dma_tag_t dmat;
	mca_chipset_tag_t mc;
	int slot;

	mca_attach_hook(parent, self, mba);
	printf("\n");

	iot = mba->mba_iot;
	memt = mba->mba_memt;
	mc = mba->mba_mc;
	dmat = mba->mba_dmat;

	/*
	 * Search for and attach subdevices.
	 *
	 * NB: In the adapter setup register, slots are numbered from 0,
	 * but officially they are numbered from 1.
	 * We use the former convention internally and the latter for text
	 * messages and in config files.
	 */

	for (slot = 0; slot < MCA_MAX_SLOTS; slot++) {
		struct mca_attach_args ma;
		int reg;
		int locs[MCACF_NLOCS];

		ma.ma_iot = iot;
		ma.ma_memt = memt;
		ma.ma_dmat = dmat;
		ma.ma_mc = mc;
		ma.ma_slot = slot;

		for(reg = 0; reg < 8; reg++)
			ma.ma_pos[reg]=mca_conf_read(mc, slot, reg);

		ma.ma_id = ma.ma_pos[0] + (ma.ma_pos[1] << 8);
		if (ma.ma_id == 0xffff)	/* no adapter here */
			continue;

		locs[MCACF_SLOT] = slot;

		if (ma.ma_pos[2] & MCA_POS2_ENABLE
		    || mca_match_disabled(ma.ma_id))
			config_found_sm_loc(self, "mca", locs, &ma,
					    mca_print, config_stdsubmatch);
		else {
			mca_print(&ma, device_xname(self));
			printf(" disabled\n");
		}
	}
}
