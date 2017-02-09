/*	$NetBSD: sc_vme.c,v 1.17 2008/07/06 13:29:50 tsutsui Exp $	*/

/*-
 * Copyright (c) 1996,2000,2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, David Jones, Gordon W. Ross, Jason R. Thorpe,
 * Paul Kranenburg, and Matt Fredette.
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
 * This file contains VME bus-dependent of the `sc' SCSI adapter.
 * This hardware is frequently found on Sun 2, Sun 3, and Sun 4 machines.
 *
 * The SCSI machinery on this adapter is implemented by an Sun custom
 * chipset, which is handled by the chipset driver in /sys/dev/ic/sunscpal.c
 */

/*
 * This driver originated as an MD implementation for the `si' VME driver.
 * The notes pertaining to that history are included below.
 *
 * David Jones wrote the initial version of this module for NetBSD/sun3,
 * which included support for the VME adapter only. (no reselection).
 *
 * Gordon Ross added support for the Sun 3 OBIO adapter, and re-worked
 * both the VME and OBIO code to support disconnect/reselect.
 * (Required figuring out the hardware "features" noted above.)
 *
 * The autoconfiguration boilerplate came from Adam Glass.
 *
 * Jason R. Thorpe ported the autoconfiguration and VME portions to
 * NetBSD/sparc, and added initial support for the 4/100 "SCSI Weird",
 * a wacky OBIO variant of the VME SCSI-3.  Many thanks to Chuck Cranor
 * for lots of helpful tips and suggestions.  Thanks also to Paul Kranenburg
 * and Chris Torek for bits of insight needed along the way.  Thanks to
 * David Gilbert and Andrew Gillham who risked filesystem life-and-limb
 * for the sake of testing.  Andrew Gillham helped work out the bugs
 * the 4/100 DMA code.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sc_vme.c,v 1.17 2008/07/06 13:29:50 tsutsui Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_debug.h>
#include <dev/scsipi/scsiconf.h>

#if !defined(DDB) && !defined(Debugger)
#define	Debugger()
#endif

#ifndef DEBUG
#define DEBUG XXX
#endif

#include <dev/ic/sunscpalvar.h>

#include <dev/vme/screg.h>

/*
 * Transfers smaller than this are done using PIO
 * (on assumption they're not worth DMA overhead)
 */
#define	MIN_DMA_LEN 128

int sunsc_vme_options = 0;

static int	sc_vme_match(device_t, cfdata_t, void *);
static void	sc_vme_attach(device_t, device_t, void *);
static int	sc_vme_intr(void *);

/* Auto-configuration glue. */
CFATTACH_DECL_NEW(sc_vme, sizeof(struct sunscpal_softc),
    sc_vme_match, sc_vme_attach, NULL, NULL);

static int
sc_vme_match(device_t parent, cfdata_t cf, void *aux)
{
	struct vme_attach_args	*va = aux;
	vme_chipset_tag_t	ct = va->va_vct;
        vme_am_t		mod;
        vme_addr_t		vme_addr;

	/* Make sure there is something there... */
	mod = VME_AM_A24 | VME_AM_MBO | VME_AM_SUPER | VME_AM_DATA;
	vme_addr = va->r[0].offset;

	if (vme_probe(ct, vme_addr, 1, mod, VME_D8, NULL, 0) != 0)
		return 0;

	/*
	 * If this is a VME SCSI board, we have to determine whether
	 * it is an "sc" (Sun2) or "si" (Sun3) SCSI board.  This can
	 * be determined using the fact that the "sc" board occupies
	 * 4K bytes in VME space but the "si" board occupies 2K bytes.
	 */
	return vme_probe(ct, vme_addr + 0x801, 1, mod, VME_D8, NULL, 0) == 0;
}

static void
sc_vme_attach(device_t parent, device_t self, void *aux)
{
	struct sunscpal_softc	*sc = device_private(self);
	struct vme_attach_args	*va = aux;
	vme_chipset_tag_t	ct = va->va_vct;
	bus_space_tag_t		bt;
	bus_space_handle_t	bh;
	vme_mapresc_t resc;
	vme_intr_handle_t	ih;
	vme_am_t		mod;
	int i;

	sc->sc_dev = self;
	sc->sunscpal_dmat = va->va_bdt;

	mod = VME_AM_A24 | VME_AM_MBO | VME_AM_SUPER | VME_AM_DATA;

	if (vme_space_map(ct, va->r[0].offset, SCREG_BANK_SZ,
	    mod, VME_D8, 0, &bt, &bh, &resc) != 0)
		panic("%s: vme_space_map", device_xname(self));

	sc->sunscpal_regt = bt;
	sc->sunscpal_regh = bh;

	vme_intr_map(ct, va->ilevel, va->ivector, &ih);
	vme_intr_establish(ct, ih, IPL_BIO, sc_vme_intr, sc);

	aprint_normal("\n");

	/*
	 * Initialize fields used by the MI code
	 */

	/* PAL register bank offsets */
	sc->sunscpal_data = SCREG_DATA;
	sc->sunscpal_cmd_stat = SCREG_CMD_STAT;
	sc->sunscpal_icr = SCREG_ICR;
	sc->sunscpal_dma_addr_h = SCREG_DMA_ADDR_H;
	sc->sunscpal_dma_addr_l = SCREG_DMA_ADDR_L;
	sc->sunscpal_dma_count = SCREG_DMA_COUNT;
	sc->sunscpal_intvec = SCREG_INTVEC;

	/* Miscellaneous. */
	sc->sc_min_dma_len = MIN_DMA_LEN;
	sc->sc_rev = SUNSCPAL_VARIANT_501_1045;

	/*
	 * Allocate DMA handles.
	 */
	i = SUNSCPAL_OPENINGS * sizeof(struct sunscpal_dma_handle);
	sc->sc_dma_handles = malloc(i, M_DEVBUF, M_NOWAIT);
	if (sc->sc_dma_handles == NULL)
		panic("sc: DMA handle malloc failed");

	for (i = 0; i < SUNSCPAL_OPENINGS; i++) {
		sc->sc_dma_handles[i].dh_flags = 0;

		/* Allocate a DMA handle */
		if (vme_dmamap_create(
		    ct,				/* VME chip tag */
		    SUNSCPAL_MAX_DMA_LEN,	/* size */
		    VME_AM_A24,			/* address modifier */
		    VME_D16,			/* data size */
		    0,				/* swap */
		    1,				/* nsegments */
		    SUNSCPAL_MAX_DMA_LEN,	/* maxsegsz */
		    0,				/* boundary */
		    BUS_DMA_NOWAIT,
		    &sc->sc_dma_handles[i].dh_dmamap) != 0) {

			aprint_error_dev(self, "DMA buffer map create error\n");
			return;
		}
	}

	/*
	 * Set up interrupts on the board.
	 */
	SUNSCPAL_WRITE_1(sc, sunscpal_intvec, va->ivector & 0xFF);

	/* Do the common attach stuff. */
	printf("%s", device_xname(self));
	sunscpal_attach(sc, (device_cfdata(self)->cf_flags ?
	    device_cfdata(self)->cf_flags : sunsc_vme_options));
}

static int
sc_vme_intr(void *arg)
{
	struct sunscpal_softc *sc = arg;
	int claimed;

	claimed = sunscpal_intr(sc);
#ifdef  DEBUG
	if (!claimed) {
        	printf("%s: spurious from SBC\n", __func__);
	}
#endif
	/* Yes, we DID cause this interrupt. */
	claimed = 1;

	return claimed;
}
