/* $NetBSD: siisatavar.h,v 1.6 2010/07/26 15:41:33 jakllsch Exp $ */

/* from ahcisatavar.h */

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 *
 */

/*-
 * Copyright (c) 2007, 2008, 2009 Jonathan A. Kollasch.
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
 *
 */

#ifndef _IC_SIISATAVAR_H_
#define _IC_SIISATAVAR_H_

#include <dev/ic/siisatareg.h>
#include <dev/ata/atavar.h>

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_DETACH 0x20
#define DEBUG_DEBUG 0x80000000
#ifdef SIISATA_DEBUG
extern int siisata_debug_mask;
#define SIISATA_DEBUG_PRINT(args, level) \
	if (siisata_debug_mask & (level)) \
		printf args
#else
#define SIISATA_DEBUG_PRINT(args, level)
#endif

struct siisata_softc {
	struct atac_softc sc_atac;
	bus_space_tag_t sc_grt;
	bus_space_handle_t sc_grh;
	bus_size_t sc_grs;
	bus_space_tag_t sc_prt;
	bus_space_handle_t sc_prh;
	bus_size_t sc_prs;
	bus_dma_tag_t sc_dmat;

	struct ata_channel *sc_chanarray[SIISATA_MAX_PORTS];
	struct siisata_channel {
		struct ata_channel ata_channel;
		bus_space_handle_t sch_scontrol;
		bus_space_handle_t sch_sstatus;
		bus_space_handle_t sch_serror;

		bus_dma_segment_t sch_prb_seg;
		int sch_prb_nseg;
		bus_dmamap_t sch_prbd;
		/* command activation PRBs */
		struct siisata_prb *sch_prb[SIISATA_MAX_SLOTS];
		bus_addr_t sch_bus_prb[SIISATA_MAX_SLOTS];

		bus_dmamap_t sch_datad[SIISATA_MAX_SLOTS];

		uint32_t sch_active_slots;
	} sc_channels[SIISATA_MAX_PORTS];
};

#define SIISATANAME(sc) (device_xname((sc)->sc_atac.atac_dev))

#define GRREAD(sc, reg) bus_space_read_4((sc)->sc_grt, (sc)->sc_grh, (reg))
#define GRWRITE(sc, reg, val) bus_space_write_4((sc)->sc_grt, (sc)->sc_grh, (reg), (val))
#define PRREAD(sc, reg) bus_space_read_4((sc)->sc_prt, (sc)->sc_prh, (reg))
#define PRWRITE(sc, reg, val) bus_space_write_4((sc)->sc_prt, (sc)->sc_prh, (reg), (val))

#define SIISATA_PRB_SYNC(sc, schp, slot, op) bus_dmamap_sync((sc)->sc_dmat, \
    (schp)->sch_prbd, slot * SIISATA_CMD_SIZE, SIISATA_CMD_SIZE, (op))

#define SIISATA_NON_NCQ_SLOT 27

void siisata_attach(struct siisata_softc *);
int siisata_detach(struct siisata_softc *, int);
void siisata_resume(struct siisata_softc *);
int siisata_intr(void *);

#endif  /* !_IC_SIISATAVAR_H_ */
