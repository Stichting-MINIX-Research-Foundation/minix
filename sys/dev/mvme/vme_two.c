/*	$NetBSD: vme_two.c,v 1.8 2009/03/14 15:36:19 dsl Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * VME support specific to the VMEchip2 found on all high-end MVME boards
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vme_two.c,v 1.8 2009/03/14 15:36:19 dsl Exp $");

#include "vmetwo.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/mvme/mvmebus.h>
#include <dev/mvme/vme_tworeg.h>
#include <dev/mvme/vme_twovar.h>

void vmetwo_master_range(struct vmetwo_softc *, int, struct mvmebus_range *);
void vmetwo_slave_range(struct vmetwo_softc *, int, vme_am_t,
	struct mvmebus_range *);

/* ARGSUSED */
void
vmetwo_init(struct vmetwo_softc *sc)
{
	u_int32_t reg;
	int i;

	/* Initialise stuff for the common mvmebus front-end */
	sc->sc_mvmebus.sc_chip = sc;
	sc->sc_mvmebus.sc_nmasters = VME2_NMASTERS;
	sc->sc_mvmebus.sc_masters = &sc->sc_master[0];
	sc->sc_mvmebus.sc_nslaves = VME2_NSLAVES;
	sc->sc_mvmebus.sc_slaves = &sc->sc_slave[0];
	sc->sc_mvmebus.sc_intr_establish = vmetwo_intr_establish;
	sc->sc_mvmebus.sc_intr_disestablish = vmetwo_intr_disestablish;

	/* Initialise interrupts */
	vmetwo_intr_init(sc);

	reg = vme2_lcsr_read(sc, VME2LCSR_BOARD_CONTROL);
	printf(": Type 2 VMEchip, scon jumper %s\n",
	    (reg & VME2_BOARD_CONTROL_SCON) ? "enabled" : "disabled");

	/*
	 * Figure out what bits of the VMEbus we can access.
	 * First record the `fixed' maps (if they're enabled)
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_IO_CONTROL);
	if (reg & VME2_IO_CONTROL_I1EN) {
		/* This range is fixed to A16, DATA */
		sc->sc_master[0].vr_am = VME_AM_A16 | MVMEBUS_AM_CAP_DATA;

		/* However, SUPER/USER is selectable... */
		if (reg & VME2_IO_CONTROL_I1SU)
			sc->sc_master[0].vr_am |= MVMEBUS_AM_CAP_SUPER;
		else
			sc->sc_master[0].vr_am |= MVMEBUS_AM_CAP_USER;

		/* As is the datasize */
		sc->sc_master[0].vr_datasize = VME_D32 | VME_D16;
		if (reg & VME2_IO_CONTROL_I1D16)
			sc->sc_master[0].vr_datasize &= ~VME_D32;

		sc->sc_master[0].vr_locstart = VME2_IO0_LOCAL_START;
		sc->sc_master[0].vr_mask = VME2_IO0_MASK;
		sc->sc_master[0].vr_vmestart = VME2_IO0_VME_START;
		sc->sc_master[0].vr_vmeend = VME2_IO0_VME_END;
	} else
		sc->sc_master[0].vr_am = MVMEBUS_AM_DISABLED;

	if (reg & VME2_IO_CONTROL_I2EN) {
		/* These two ranges are fixed to A24D16 and A32D16 */
		sc->sc_master[1].vr_am = VME_AM_A24;
		sc->sc_master[1].vr_datasize = VME_D16;
		sc->sc_master[2].vr_am = VME_AM_A32;
		sc->sc_master[2].vr_datasize = VME_D16;

		/* However, SUPER/USER is selectable */
		if (reg & VME2_IO_CONTROL_I2SU) {
			sc->sc_master[1].vr_am |= MVMEBUS_AM_CAP_SUPER;
			sc->sc_master[2].vr_am |= MVMEBUS_AM_CAP_SUPER;
		} else {
			sc->sc_master[1].vr_am |= MVMEBUS_AM_CAP_USER;
			sc->sc_master[2].vr_am |= MVMEBUS_AM_CAP_USER;
		}

		/* As is PROGRAM/DATA */
		if (reg & VME2_IO_CONTROL_I2PD) {
			sc->sc_master[1].vr_am |= MVMEBUS_AM_CAP_PROG;
			sc->sc_master[2].vr_am |= MVMEBUS_AM_CAP_PROG;
		} else {
			sc->sc_master[1].vr_am |= MVMEBUS_AM_CAP_DATA;
			sc->sc_master[2].vr_am |= MVMEBUS_AM_CAP_DATA;
		}

		sc->sc_master[1].vr_locstart = VME2_IO1_LOCAL_START;
		sc->sc_master[1].vr_mask = VME2_IO1_MASK;
		sc->sc_master[1].vr_vmestart = VME2_IO1_VME_START;
		sc->sc_master[1].vr_vmeend = VME2_IO1_VME_END;

		sc->sc_master[2].vr_locstart = VME2_IO2_LOCAL_START;
		sc->sc_master[2].vr_mask = VME2_IO2_MASK;
		sc->sc_master[2].vr_vmestart = VME2_IO2_VME_START;
		sc->sc_master[2].vr_vmeend = VME2_IO2_VME_END;
	} else {
		sc->sc_master[1].vr_am = MVMEBUS_AM_DISABLED;
		sc->sc_master[2].vr_am = MVMEBUS_AM_DISABLED;
	}

	/*
	 * Now read the progammable maps
	 */
	for (i = 0; i < VME2_MASTER_WINDOWS; i++)
		vmetwo_master_range(sc, i,
		    &(sc->sc_master[i + VME2_MASTER_PROG_START]));

	/* XXX: No A16 slave yet :XXX */
	sc->sc_slave[VME2_SLAVE_A16].vr_am = MVMEBUS_AM_DISABLED;

	for (i = 0; i < VME2_SLAVE_WINDOWS; i++) {
		vmetwo_slave_range(sc, i, VME_AM_A32,
		    &sc->sc_slave[i + VME2_SLAVE_PROG_START]);
		vmetwo_slave_range(sc, i, VME_AM_A24,
		    &sc->sc_slave[i + VME2_SLAVE_PROG_START + 2]);
	}

	mvmebus_attach(&sc->sc_mvmebus);
}

void
vmetwo_master_range(struct vmetwo_softc *sc, int range, struct mvmebus_range *vr)
{
	u_int32_t start, end, attr;
	u_int32_t reg;

	/*
	 * First, check if the range is actually enabled...
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_MASTER_ENABLE);
	if ((reg & VME2_MASTER_ENABLE(range)) == 0) {
		vr->vr_am = MVMEBUS_AM_DISABLED;
		return;
	}

	/*
	 * Fetch and record the range's attributes
	 */
	attr = vme2_lcsr_read(sc, VME2LCSR_MASTER_ATTR);
	attr >>= VME2_MASTER_ATTR_AM_SHIFT(range);

	/*
	 * Fix up the datasizes available through this range
	 */
	vr->vr_datasize = VME_D32 | VME_D16;
	if (attr & VME2_MASTER_ATTR_D16)
		vr->vr_datasize &= ~VME_D32;
	attr &= VME2_MASTER_ATTR_AM_MASK;

	vr->vr_am = (attr & VME_AM_ADRSIZEMASK) | MVMEBUS_AM2CAP(attr);
	switch (vr->vr_am & VME_AM_ADRSIZEMASK) {
	case VME_AM_A32:
	default:
		vr->vr_mask = 0xffffffffu;
		break;

	case VME_AM_A24:
		vr->vr_mask = 0x00ffffffu;
		break;

	case VME_AM_A16:
		vr->vr_mask = 0x0000ffffu;
		break;
	}

	/*
	 * XXX
	 * It would be nice if users of the MI VMEbus code could pass down
	 * whether they can tolerate Write-Posting to their device(s).
	 * XXX
	 */

	/*
	 * Fetch the local-bus start and end addresses for the range
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_MASTER_ADDRESS(range));
	start = (reg & VME2_MAST_ADDRESS_START_MASK);
	start <<= VME2_MAST_ADDRESS_START_SHIFT;
	vr->vr_locstart = start & ~vr->vr_mask;
	end = (reg & VME2_MAST_ADDRESS_END_MASK);
	end <<= VME2_MAST_ADDRESS_END_SHIFT;
	end |= 0xffffu;
	end += 1;

	/*
	 * Local->VMEbus map '4' has optional translation bits, so
	 * the VMEbus start and end addresses may need to be adjusted.
	 */
	if (range == 3 && (reg = vme2_lcsr_read(sc, VME2LCSR_MAST4_TRANS))!=0) {
		uint32_t addr, sel, len = end - start;

		reg = vme2_lcsr_read(sc, VME2LCSR_MAST4_TRANS);
		reg &= VME2_MAST4_TRANS_SELECT_MASK;
		sel = reg << VME2_MAST4_TRANS_SELECT_SHIFT;

		reg = vme2_lcsr_read(sc, VME2LCSR_MAST4_TRANS);
		reg &= VME2_MAST4_TRANS_ADDRESS_MASK;
		addr = reg << VME2_MAST4_TRANS_ADDRESS_SHIFT;

		start = (addr & sel) | (start & (~sel));
		end = start + len;
		vr->vr_mask &= len - 1;
	}

	/* XXX Deal with overlap of onboard RAM address space */
	/* XXX Then again, 167-Bug warns about this at setup time ... */

	/*
	 * Fixup the addresses this range corresponds to
	 */
	vr->vr_vmestart = start & vr->vr_mask;
	vr->vr_vmeend = (end - 1) & vr->vr_mask;
}

void
vmetwo_slave_range(struct vmetwo_softc *sc, int range, vme_am_t am, struct mvmebus_range *vr)
{
	u_int32_t reg;

	/*
	 * First, check if the range is actually enabled.
	 * Note that bit 1 of `range' is used to indicte if we're
	 * looking for an A24 range (set) or an A32 range (clear).
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_SLAVE_CTRL);

	if (am == VME_AM_A32 && (reg & VME2_SLAVE_AMSEL_A32(range))) {
		vr->vr_am = VME_AM_A32;
		vr->vr_mask = 0xffffffffu;
	} else
	if (am == VME_AM_A24 && (reg & VME2_SLAVE_AMSEL_A24(range))) {
		vr->vr_am = VME_AM_A24;
		vr->vr_mask = 0x00ffffffu;
	} else {
		/* The range is not enabled */
		vr->vr_am = MVMEBUS_AM_DISABLED;
		return;
	}

	if ((reg & VME2_SLAVE_AMSEL_DAT(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_DATA;

	if ((reg & VME2_SLAVE_AMSEL_PGM(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_PROG;

	if ((reg & VME2_SLAVE_AMSEL_USR(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_USER;

	if ((reg & VME2_SLAVE_AMSEL_SUP(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_SUPER;

	if ((reg & VME2_SLAVE_AMSEL_BLK(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_BLK;

	if ((reg & VME2_SLAVE_AMSEL_BLKD64(range)) != 0)
		vr->vr_am |= MVMEBUS_AM_CAP_BLKD64;

	vr->vr_datasize = VME_D32 | VME_D16 | VME_D8;

	/*
	 * Record the VMEbus start and end addresses of the slave image
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_SLAVE_ADDRESS(range));
	vr->vr_vmestart = reg & VME2_SLAVE_ADDRESS_START_MASK;
	vr->vr_vmestart <<= VME2_SLAVE_ADDRESS_START_SHIFT;
	vr->vr_vmestart &= vr->vr_mask;
	vr->vr_vmeend = reg & VME2_SLAVE_ADDRESS_END_MASK;
	vr->vr_vmeend <<= VME2_SLAVE_ADDRESS_END_SHIFT;
	vr->vr_vmeend &= vr->vr_mask;
	vr->vr_vmeend |= 0xffffu;

	/*
	 * Now figure out the local-bus address
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_SLAVE_CTRL);
	if ((reg & VME2_SLAVE_CTRL_ADDER(range)) != 0) {
		reg = vme2_lcsr_read(sc, VME2LCSR_SLAVE_TRANS(range));
		reg &= VME2_SLAVE_TRANS_ADDRESS_MASK;
		reg <<= VME2_SLAVE_TRANS_ADDRESS_SHIFT;
		vr->vr_locstart = vr->vr_vmestart + reg;
	} else {
		u_int32_t sel, addr;

		reg = vme2_lcsr_read(sc, VME2LCSR_SLAVE_TRANS(range));
		sel = reg & VME2_SLAVE_TRANS_SELECT_MASK;
		sel <<= VME2_SLAVE_TRANS_SELECT_SHIFT;
		addr = reg & VME2_SLAVE_TRANS_ADDRESS_MASK;
		addr <<= VME2_SLAVE_TRANS_ADDRESS_SHIFT;

		vr->vr_locstart = addr & sel;
		vr->vr_locstart |= vr->vr_vmestart & (~sel);
	}
}
