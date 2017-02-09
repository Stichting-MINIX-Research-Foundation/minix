/*	$NetBSD: gtidmac.c,v 1.11 2014/03/15 13:33:48 kiyohara Exp $	*/
/*
 * Copyright (c) 2008, 2012 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gtidmac.c,v 1.11 2014/03/15 13:33:48 kiyohara Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <sys/kmem.h>

#include <uvm/uvm_param.h>	/* For PAGE_SIZE */

#include <dev/dmover/dmovervar.h>

#include <dev/marvell/gtidmacreg.h>
#include <dev/marvell/gtidmacvar.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include <prop/proplib.h>

#include "locators.h"

#ifdef GTIDMAC_DEBUG
#define DPRINTF(x)	if (gtidmac_debug) printf x
int gtidmac_debug = 0;
#else
#define DPRINTF(x)
#endif

#define GTIDMAC_NDESC		64
#define GTIDMAC_MAXCHAN		8
#define MVXORE_NDESC		128
#define MVXORE_MAXCHAN		2

#define GTIDMAC_NSEGS		((GTIDMAC_MAXXFER + PAGE_SIZE - 1) / PAGE_SIZE)
#define MVXORE_NSEGS		((MVXORE_MAXXFER + PAGE_SIZE - 1) / PAGE_SIZE)


struct gtidmac_softc;

struct gtidmac_function {
	int (*chan_alloc)(void *, bus_dmamap_t **, bus_dmamap_t **, void *);
	void (*chan_free)(void *, int);
	int (*dma_setup)(void *, int, int, bus_dmamap_t *, bus_dmamap_t *,
			 bus_size_t);
	void (*dma_start)(void *, int,
			  void (*dma_done_cb)(void *, int, bus_dmamap_t *,
						      bus_dmamap_t *, int));
	uint32_t (*dma_finish)(void *, int, int);
};

struct gtidmac_dma_desc {
	int dd_index;
	union {
		struct gtidmac_desc *idmac_vaddr;
		struct mvxore_desc *xore_vaddr;
	} dd_vaddr;
#define dd_idmac_vaddr	dd_vaddr.idmac_vaddr
#define dd_xore_vaddr	dd_vaddr.xore_vaddr
	paddr_t dd_paddr;
	SLIST_ENTRY(gtidmac_dma_desc) dd_next;
};

struct gtidmac_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	bus_dma_tag_t sc_dmat;
	struct gtidmac_dma_desc *sc_dd_buffer;
	bus_dma_segment_t sc_pattern_segment;
	struct {
		u_char pbuf[16];	/* 16byte/pattern */
	} *sc_pbuf;			/*   x256 pattern */

	int sc_gtidmac_nchan;
	struct gtidmac_desc *sc_dbuf;
	bus_dmamap_t sc_dmap;
	SLIST_HEAD(, gtidmac_dma_desc) sc_dlist;
	struct {
		bus_dmamap_t chan_in;		/* In dmamap */
		bus_dmamap_t chan_out;		/* Out dmamap */
		uint64_t chan_totalcnt;		/* total transfered byte */
		int chan_ddidx;
		void *chan_running;		/* opaque object data */
		void (*chan_dma_done)(void *, int, bus_dmamap_t *,
				      bus_dmamap_t *, int);
	} sc_cdesc[GTIDMAC_MAXCHAN];
	struct gtidmac_intr_arg {
		struct gtidmac_softc *ia_sc;
		uint32_t ia_cause;
		uint32_t ia_mask;
		uint32_t ia_eaddr;
		uint32_t ia_eselect;
	} sc_intrarg[GTIDMAC_NINTRRUPT];

	int sc_mvxore_nchan;
	struct mvxore_desc *sc_dbuf_xore;
	bus_dmamap_t sc_dmap_xore;
	SLIST_HEAD(, gtidmac_dma_desc) sc_dlist_xore;
	struct {
		bus_dmamap_t chan_in[MVXORE_NSRC];	/* In dmamap */
		bus_dmamap_t chan_out;			/* Out dmamap */
		uint64_t chan_totalcnt;			/* total transfered */
		int chan_ddidx;
		void *chan_running;			/* opaque object data */
		void (*chan_dma_done)(void *, int, bus_dmamap_t *,
				      bus_dmamap_t *, int);
	} sc_cdesc_xore[MVXORE_MAXCHAN];

	struct dmover_backend sc_dmb;
	struct dmover_backend sc_dmb_xore;
	int sc_dmb_busy;
};
struct gtidmac_softc *gtidmac_softc = NULL;

static int gtidmac_match(device_t, struct cfdata *, void *);
static void gtidmac_attach(device_t, device_t, void *);

static int gtidmac_intr(void *);
static int mvxore_port0_intr(void *);
static int mvxore_port1_intr(void *);
static int mvxore_intr(struct gtidmac_softc *, int);

static void gtidmac_process(struct dmover_backend *);
static void gtidmac_dmover_run(struct dmover_backend *);
static void gtidmac_dmover_done(void *, int, bus_dmamap_t *, bus_dmamap_t *,
				int);
static __inline int gtidmac_dmmap_load(struct gtidmac_softc *, bus_dmamap_t,
				dmover_buffer_type, dmover_buffer *, int);
static __inline void gtidmac_dmmap_unload(struct gtidmac_softc *, bus_dmamap_t, int);

static uint32_t gtidmac_finish(void *, int, int);
static uint32_t mvxore_finish(void *, int, int);

static void gtidmac_wininit(struct gtidmac_softc *, enum marvell_tags *);
static void mvxore_wininit(struct gtidmac_softc *, enum marvell_tags *);

static int gtidmac_buffer_setup(struct gtidmac_softc *);
static int mvxore_buffer_setup(struct gtidmac_softc *);

#ifdef GTIDMAC_DEBUG
static void gtidmac_dump_idmacreg(struct gtidmac_softc *, int);
static void gtidmac_dump_idmacdesc(struct gtidmac_softc *,
				   struct gtidmac_dma_desc *, uint32_t, int);
static void gtidmac_dump_xorereg(struct gtidmac_softc *, int);
static void gtidmac_dump_xoredesc(struct gtidmac_softc *,
				  struct gtidmac_dma_desc *, uint32_t, int);
#endif


static struct gtidmac_function gtidmac_functions = {
	.chan_alloc = gtidmac_chan_alloc,
	.chan_free = gtidmac_chan_free,
	.dma_setup = gtidmac_setup,
	.dma_start = gtidmac_start,
	.dma_finish = gtidmac_finish,
};

static struct gtidmac_function mvxore_functions = {
	.chan_alloc = mvxore_chan_alloc,
	.chan_free = mvxore_chan_free,
	.dma_setup = mvxore_setup,
	.dma_start = mvxore_start,
	.dma_finish = mvxore_finish,
};

static const struct dmover_algdesc gtidmac_algdescs[] = {
	{
		.dad_name = DMOVER_FUNC_ZERO,
		.dad_data = &gtidmac_functions,
		.dad_ninputs = 0
	},
	{
		.dad_name = DMOVER_FUNC_FILL8,
		.dad_data = &gtidmac_functions,
		.dad_ninputs = 0
	},
	{
		.dad_name = DMOVER_FUNC_COPY,
		.dad_data = &gtidmac_functions,
		.dad_ninputs = 1
	},
};

static const struct dmover_algdesc mvxore_algdescs[] = {
#if 0
	/*
	 * As for these operations, there are a lot of restrictions.  It is
	 * necessary to use IDMAC.
	 */
	{
		.dad_name = DMOVER_FUNC_ZERO,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 0
	},
	{
		.dad_name = DMOVER_FUNC_FILL8,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 0
	},
#endif
	{
		.dad_name = DMOVER_FUNC_COPY,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 1
	},
	{
		.dad_name = DMOVER_FUNC_ISCSI_CRC32C,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 1
	},
	{
		.dad_name = DMOVER_FUNC_XOR2,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 2
	},
	{
		.dad_name = DMOVER_FUNC_XOR3,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 3
	},
	{
		.dad_name = DMOVER_FUNC_XOR4,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 4
	},
	{
		.dad_name = DMOVER_FUNC_XOR5,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 5
	},
	{
		.dad_name = DMOVER_FUNC_XOR6,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 6
	},
	{
		.dad_name = DMOVER_FUNC_XOR7,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 7
	},
	{
		.dad_name = DMOVER_FUNC_XOR8,
		.dad_data = &mvxore_functions,
		.dad_ninputs = 8
	},
};

static struct {
	int model;
	int idmac_nchan;
	int idmac_irq;
	int xore_nchan;
	int xore_irq;
} channels[] = {
	/*
	 * Marvell System Controllers:
	 * need irqs in attach_args.
	 */
	{ MARVELL_DISCOVERY,		8, -1, 0, -1 },
	{ MARVELL_DISCOVERY_II,		8, -1, 0, -1 },
	{ MARVELL_DISCOVERY_III,	8, -1, 0, -1 },
#if 0
	{ MARVELL_DISCOVERY_LT,		4, -1, 2, -1 },
	{ MARVELL_DISCOVERY_V,		4, -1, 2, -1 },
	{ MARVELL_DISCOVERY_VI,		4, -1, 2, -1 },		????
#endif

	/*
	 * Marvell System on Chips:
	 * No need irqs in attach_args.  We always connecting to interrupt-pin
	 * statically.
	 */
	{ MARVELL_ORION_1_88F1181,	4, 24, 0, -1 },
	{ MARVELL_ORION_2_88F1281,	4, 24, 0, -1 },
	{ MARVELL_ORION_1_88F5082,	4, 24, 0, -1 },
	{ MARVELL_ORION_1_88F5180N,	4, 24, 0, -1 },
	{ MARVELL_ORION_1_88F5181,	4, 24, 0, -1 },
	{ MARVELL_ORION_1_88F5182,	4, 24, 2, 30 },
	{ MARVELL_ORION_2_88F5281,	4, 24, 0, -1 },
	{ MARVELL_ORION_1_88W8660,	4, 24, 0, -1 },
	{ MARVELL_KIRKWOOD_88F6180,	0, -1, 4, 5 },
	{ MARVELL_KIRKWOOD_88F6192,	0, -1, 4, 5 },
	{ MARVELL_KIRKWOOD_88F6281,	0, -1, 4, 5 },
	{ MARVELL_KIRKWOOD_88F6282,	0, -1, 4, 5 },
	{ MARVELL_ARMADAXP_MV78130,	4, 33, 2, 51 },
	{ MARVELL_ARMADAXP_MV78130,	0, -1, 2, 94 },
	{ MARVELL_ARMADAXP_MV78160,	4, 33, 2, 51 },
	{ MARVELL_ARMADAXP_MV78160,	0, -1, 2, 94 },
	{ MARVELL_ARMADAXP_MV78230,	4, 33, 2, 51 },
	{ MARVELL_ARMADAXP_MV78230,	0, -1, 2, 94 },
	{ MARVELL_ARMADAXP_MV78260,	4, 33, 2, 51 },
	{ MARVELL_ARMADAXP_MV78260,	0, -1, 2, 94 },
	{ MARVELL_ARMADAXP_MV78460,	4, 33, 2, 51 },
	{ MARVELL_ARMADAXP_MV78460,	0, -1, 2, 94 },
};

struct gtidmac_winacctbl *gtidmac_winacctbl;
struct gtidmac_winacctbl *mvxore_winacctbl;

CFATTACH_DECL_NEW(gtidmac_gt, sizeof(struct gtidmac_softc),
    gtidmac_match, gtidmac_attach, NULL, NULL);
CFATTACH_DECL_NEW(gtidmac_mbus, sizeof(struct gtidmac_softc),
    gtidmac_match, gtidmac_attach, NULL, NULL);


/* ARGSUSED */
static int
gtidmac_match(device_t parent, struct cfdata *match, void *aux)
{
	struct marvell_attach_args *mva = aux;
	int unit, i;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT)
		return 0;
	unit = 0;
	for (i = 0; i < __arraycount(channels); i++)
		if (mva->mva_model == channels[i].model) {
			if (mva->mva_unit == unit) {
				mva->mva_size = GTIDMAC_SIZE;
				return 1;
			}
			unit++;
		}
	return 0;
}

/* ARGSUSED */
static void
gtidmac_attach(device_t parent, device_t self, void *aux)
{
	struct gtidmac_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	prop_dictionary_t dict = device_properties(self);
	uint32_t idmac_irq, xore_irq, dmb_speed;
	int unit, idmac_nchan, xore_nchan, nsegs, i, j, n;

	unit = 0;
	for (i = 0; i < __arraycount(channels); i++)
		if (mva->mva_model == channels[i].model) {
			if (mva->mva_unit == unit)
				break;
			unit++;
		}
	idmac_nchan = channels[i].idmac_nchan;
	idmac_irq = channels[i].idmac_irq;
	if (idmac_nchan != 0) {
		if (idmac_irq == -1)
			idmac_irq = mva->mva_irq;
		if (idmac_irq == -1)
			/* Discovery */
			if (!prop_dictionary_get_uint32(dict,
			    "idmac-irq", &idmac_irq)) {
				aprint_error(": no idmac-irq property\n");
				return;
			}
	}
	xore_nchan = channels[i].xore_nchan;
	xore_irq = channels[i].xore_irq;
	if (xore_nchan != 0) {
		if (xore_irq == -1)
			xore_irq = mva->mva_irq;
		if (xore_irq == -1)
			/* Discovery LT/V/VI */
			if (!prop_dictionary_get_uint32(dict,
			    "xore-irq", &xore_irq)) {
				aprint_error(": no xore-irq property\n");
				return;
			}
	}

	aprint_naive("\n");
	aprint_normal(": Marvell IDMA Controller%s\n",
	    xore_nchan ? "/XOR Engine" : "");
	if (idmac_nchan > 0)
		aprint_normal_dev(self,
		    "IDMA Controller %d channels, intr %d...%d\n",
		    idmac_nchan, idmac_irq, idmac_irq + GTIDMAC_NINTRRUPT - 1);
	if (xore_nchan > 0)
		aprint_normal_dev(self,
		    "XOR Engine %d channels, intr %d...%d\n",
		    xore_nchan, xore_irq, xore_irq + xore_nchan - 1);

	sc->sc_dev = self;
	sc->sc_iot = mva->mva_iot;

	/* Map I/O registers */
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}

	/*
	 * Initialise DMA descriptors and associated metadata
	 */
	sc->sc_dmat = mva->mva_dmat;
	n = idmac_nchan * GTIDMAC_NDESC + xore_nchan * MVXORE_NDESC;
	sc->sc_dd_buffer =
	    kmem_alloc(sizeof(struct gtidmac_dma_desc) * n, KM_SLEEP);
	if (sc->sc_dd_buffer == NULL) {
		aprint_error_dev(self, "can't allocate memory\n");
		goto fail1;
	}
	/* pattern buffer */
	if (bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &sc->sc_pattern_segment, 1, &nsegs, BUS_DMA_NOWAIT)) {
		aprint_error_dev(self,
		    "bus_dmamem_alloc failed: pattern buffer\n");
		goto fail2;
	}
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_pattern_segment, 1, PAGE_SIZE,
	    (void **)&sc->sc_pbuf, BUS_DMA_NOWAIT)) {
		aprint_error_dev(self,
		    "bus_dmamem_map failed: pattern buffer\n");
		goto fail3;
	}
	for (i = 0; i < 0x100; i++)
		for (j = 0; j < sizeof(sc->sc_pbuf[i].pbuf); j++)
			sc->sc_pbuf[i].pbuf[j] = i;

	if (!prop_dictionary_get_uint32(dict, "dmb_speed", &dmb_speed)) {
		aprint_error_dev(self, "no dmb_speed property\n");
		dmb_speed = 10;	/* More than fast swdmover perhaps. */
	}

	/* IDMAC DMA descriptor buffer */
	sc->sc_gtidmac_nchan = idmac_nchan;
	if (sc->sc_gtidmac_nchan > 0) {
		if (gtidmac_buffer_setup(sc) != 0)
			goto fail4;

		if (mva->mva_model != MARVELL_DISCOVERY)
			gtidmac_wininit(sc, mva->mva_tags);

		/* Setup interrupt */
		for (i = 0; i < GTIDMAC_NINTRRUPT; i++) {
			j = i * idmac_nchan / GTIDMAC_NINTRRUPT;

			sc->sc_intrarg[i].ia_sc = sc;
			sc->sc_intrarg[i].ia_cause = GTIDMAC_ICR(j);
			sc->sc_intrarg[i].ia_eaddr = GTIDMAC_EAR(j);
			sc->sc_intrarg[i].ia_eselect = GTIDMAC_ESR(j);
			marvell_intr_establish(idmac_irq + i, IPL_BIO,
			    gtidmac_intr, &sc->sc_intrarg[i]);
		}

		/* Register us with dmover. */
		sc->sc_dmb.dmb_name = device_xname(self);
		sc->sc_dmb.dmb_speed = dmb_speed;
		sc->sc_dmb.dmb_cookie = sc;
		sc->sc_dmb.dmb_algdescs = gtidmac_algdescs;
		sc->sc_dmb.dmb_nalgdescs = __arraycount(gtidmac_algdescs);
		sc->sc_dmb.dmb_process = gtidmac_process;
		dmover_backend_register(&sc->sc_dmb);
		sc->sc_dmb_busy = 0;
	}

	/* XORE DMA descriptor buffer */
	sc->sc_mvxore_nchan = xore_nchan;
	if (sc->sc_mvxore_nchan > 0) {
		if (mvxore_buffer_setup(sc) != 0)
			goto fail5;

		/* Setup interrupt */
		for (i = 0; i < sc->sc_mvxore_nchan; i++)
			marvell_intr_establish(xore_irq + i, IPL_BIO,
			    (i & 0x2) ? mvxore_port1_intr : mvxore_port0_intr,
			    sc);

		mvxore_wininit(sc, mva->mva_tags);

		/* Register us with dmover. */
		sc->sc_dmb_xore.dmb_name = device_xname(sc->sc_dev);
		sc->sc_dmb_xore.dmb_speed = dmb_speed;
		sc->sc_dmb_xore.dmb_cookie = sc;
		sc->sc_dmb_xore.dmb_algdescs = mvxore_algdescs;
		sc->sc_dmb_xore.dmb_nalgdescs =
		    __arraycount(mvxore_algdescs);
		sc->sc_dmb_xore.dmb_process = gtidmac_process;
		dmover_backend_register(&sc->sc_dmb_xore);
	}

	gtidmac_softc = sc;

	return;

fail5:
	for (i = sc->sc_gtidmac_nchan - 1; i >= 0; i--) {
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdesc[i].chan_in);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdesc[i].chan_out);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap);
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_dbuf,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC);
	bus_dmamem_free(sc->sc_dmat,
	    sc->sc_dmap->dm_segs, sc->sc_dmap->dm_nsegs);
fail4:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_pbuf, PAGE_SIZE);
fail3:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_pattern_segment, 1);
fail2:
	kmem_free(sc->sc_dd_buffer, sizeof(struct gtidmac_dma_desc) * n);
fail1:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, mva->mva_size);
	return;
}


static int
gtidmac_intr(void *arg)
{
	struct gtidmac_intr_arg *ia = arg;
	struct gtidmac_softc *sc = ia->ia_sc;
	uint32_t cause;
	int handled = 0, chan, error;

	cause = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ia->ia_cause);
	DPRINTF(("IDMAC intr: cause=0x%x\n", cause));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ia->ia_cause, ~cause);

	chan = 0;
	while (cause) {
		error = 0;
		if (cause & GTIDMAC_I_ADDRMISS) {
			aprint_error_dev(sc->sc_dev, "Address Miss");
			error = EINVAL;
		}
		if (cause & GTIDMAC_I_ACCPROT) {
			aprint_error_dev(sc->sc_dev,
			    "Access Protect Violation");
			error = EACCES;
		}
		if (cause & GTIDMAC_I_WRPROT) {
			aprint_error_dev(sc->sc_dev, "Write Protect");
			error = EACCES;
		}
		if (cause & GTIDMAC_I_OWN) {
			aprint_error_dev(sc->sc_dev, "Ownership Violation");
			error = EINVAL;
		}

#define GTIDMAC_I_ERROR		  \
	   (GTIDMAC_I_ADDRMISS	| \
	    GTIDMAC_I_ACCPROT	| \
	    GTIDMAC_I_WRPROT	| \
	    GTIDMAC_I_OWN)
		if (cause & GTIDMAC_I_ERROR) {
			uint32_t sel;
			int select;

			sel = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    ia->ia_eselect) & GTIDMAC_ESR_SEL;
			select = sel - chan * GTIDMAC_I_BITS;
			if (select >= 0 && select < GTIDMAC_I_BITS) {
				uint32_t ear;

				ear = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    ia->ia_eaddr);
				aprint_error(": Error Address 0x%x\n", ear);
			} else
				aprint_error(": lost Error Address\n");
		}

		if (cause & (GTIDMAC_I_COMP | GTIDMAC_I_ERROR)) {
			sc->sc_cdesc[chan].chan_dma_done(
			    sc->sc_cdesc[chan].chan_running, chan,
			    &sc->sc_cdesc[chan].chan_in,
			    &sc->sc_cdesc[chan].chan_out, error);
			handled++;
		}

		cause >>= GTIDMAC_I_BITS;
	}
	DPRINTF(("IDMAC intr: %shandled\n", handled ? "" : "not "));

	return handled;
}

static int
mvxore_port0_intr(void *arg)
{
	struct gtidmac_softc *sc = arg;

	return mvxore_intr(sc, 0);
}

static int
mvxore_port1_intr(void *arg)
{
	struct gtidmac_softc *sc = arg;

	return mvxore_intr(sc, 1);
}

static int
mvxore_intr(struct gtidmac_softc *sc, int port)
{
	uint32_t cause;
	int handled = 0, chan, error;

	cause =
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEICR(sc, port));
	DPRINTF(("XORE port %d intr: cause=0x%x\n", port, cause));
printf("XORE port %d intr: cause=0x%x\n", port, cause);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    MVXORE_XEICR(sc, port), ~cause);

	chan = 0;
	while (cause) {
		error = 0;
		if (cause & MVXORE_I_ADDRDECODE) {
			aprint_error_dev(sc->sc_dev, "Failed address decoding");
			error = EINVAL;
		}
		if (cause & MVXORE_I_ACCPROT) {
			aprint_error_dev(sc->sc_dev,
			    "Access Protect Violation");
			error = EACCES;
		}
		if (cause & MVXORE_I_WRPROT) {
			aprint_error_dev(sc->sc_dev, "Write Protect");
			error = EACCES;
		}
		if (cause & MVXORE_I_OWN) {
			aprint_error_dev(sc->sc_dev, "Ownership Violation");
			error = EINVAL;
		}
		if (cause & MVXORE_I_INTPARITY) {
			aprint_error_dev(sc->sc_dev, "Parity Error");
			error = EIO;
		}
		if (cause & MVXORE_I_XBAR) {
			aprint_error_dev(sc->sc_dev, "Crossbar Parity Error");
			error = EINVAL;
		}

#define MVXORE_I_ERROR		  \
	   (MVXORE_I_ADDRDECODE	| \
	    MVXORE_I_ACCPROT	| \
	    MVXORE_I_WRPROT	| \
	    MVXORE_I_OWN	| \
	    MVXORE_I_INTPARITY	| \
	    MVXORE_I_XBAR)
		if (cause & MVXORE_I_ERROR) {
			uint32_t type;
			int event;

			type = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XEECR(sc, port));
			type &= MVXORE_XEECR_ERRORTYPE_MASK;
			event = type - chan * MVXORE_I_BITS;
			if (event >= 0 && event < MVXORE_I_BITS) {
				uint32_t xeear;

				xeear = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    MVXORE_XEEAR(sc, port));
				aprint_error(": Error Address 0x%x\n", xeear);
			} else
				aprint_error(": lost Error Address\n");
		}

		if (cause & (MVXORE_I_EOC | MVXORE_I_ERROR)) {
			sc->sc_cdesc_xore[chan].chan_dma_done(
			    sc->sc_cdesc_xore[chan].chan_running, chan,
			    sc->sc_cdesc_xore[chan].chan_in,
			    &sc->sc_cdesc_xore[chan].chan_out, error);
			handled++;
		}

		cause >>= MVXORE_I_BITS;
	}
printf("XORE port %d intr: %shandled\n", port, handled ? "" : "not ");
	DPRINTF(("XORE port %d intr: %shandled\n",
	    port, handled ? "" : "not "));

	return handled;
}


/*
 * dmover(9) backend function.
 */
static void
gtidmac_process(struct dmover_backend *dmb)
{
	struct gtidmac_softc *sc = dmb->dmb_cookie;
	int s;

	/* If the backend is currently idle, go process the queue. */
	s = splbio();
	if (!sc->sc_dmb_busy)
		gtidmac_dmover_run(dmb);
	splx(s);
}

static void
gtidmac_dmover_run(struct dmover_backend *dmb)
{
	struct gtidmac_softc *sc = dmb->dmb_cookie;
	struct dmover_request *dreq;
	const struct dmover_algdesc *algdesc;
	struct gtidmac_function *df;
	bus_dmamap_t *dmamap_in, *dmamap_out;
	int chan, ninputs, error, i;

	sc->sc_dmb_busy = 1;

	for (;;) {
		dreq = TAILQ_FIRST(&dmb->dmb_pendreqs);
		if (dreq == NULL)
			break;
		algdesc = dreq->dreq_assignment->das_algdesc;
		df = algdesc->dad_data;
		chan = (*df->chan_alloc)(sc, &dmamap_in, &dmamap_out, dreq);
		if (chan == -1)
			return;

		dmover_backend_remque(dmb, dreq);
		dreq->dreq_flags |= DMOVER_REQ_RUNNING;

		/* XXXUNLOCK */

		error = 0;

		/* Load in/out buffers of dmover to bus_dmamap. */
		ninputs = dreq->dreq_assignment->das_algdesc->dad_ninputs;
		if (ninputs == 0) {
			int pno = 0;

			if (algdesc->dad_name == DMOVER_FUNC_FILL8)
				pno = dreq->dreq_immediate[0];

			i = 0;
			error = bus_dmamap_load(sc->sc_dmat, *dmamap_in,
			    &sc->sc_pbuf[pno], sizeof(sc->sc_pbuf[pno]), NULL,
			    BUS_DMA_NOWAIT | BUS_DMA_STREAMING | BUS_DMA_WRITE);
			if (error == 0) {
				bus_dmamap_sync(sc->sc_dmat, *dmamap_in, 0,
				    sizeof(uint32_t), BUS_DMASYNC_PREWRITE);

				/*
				 * We will call gtidmac_dmmap_unload() when
				 * becoming an error.
				 */
				i = 1;
			}
		} else
			for (i = 0; i < ninputs; i++) {
				error = gtidmac_dmmap_load(sc,
				    *(dmamap_in + i), dreq->dreq_inbuf_type,
				    &dreq->dreq_inbuf[i], 0/*write*/);
				if (error != 0)
					break;
			}
		if (algdesc->dad_name != DMOVER_FUNC_ISCSI_CRC32C) {
			if (error == 0)
				error = gtidmac_dmmap_load(sc, *dmamap_out,
				    dreq->dreq_outbuf_type, &dreq->dreq_outbuf,
				    1/*read*/);

			if (error == 0) {
				/*
				 * The size of outbuf is always believed to be
				 * DMA transfer size in dmover request.
				 */
				error = (*df->dma_setup)(sc, chan, ninputs,
				    dmamap_in, dmamap_out,
				    (*dmamap_out)->dm_mapsize);
				if (error != 0)
					gtidmac_dmmap_unload(sc, *dmamap_out,
					    1);
			}
		} else
			if (error == 0)
				error = (*df->dma_setup)(sc, chan, ninputs,
				    dmamap_in, dmamap_out,
				    (*dmamap_in)->dm_mapsize);

		/* XXXLOCK */

		if (error != 0) {
			for (; i-- > 0;)
				gtidmac_dmmap_unload(sc, *(dmamap_in + i), 0);
			(*df->chan_free)(sc, chan);

			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			dreq->dreq_error = error;
			/* XXXUNLOCK */
			dmover_done(dreq);
			/* XXXLOCK */
			continue;
		}

		(*df->dma_start)(sc, chan, gtidmac_dmover_done);
		break;
	}

	/* All done */
	sc->sc_dmb_busy = 0;
}

static void
gtidmac_dmover_done(void *object, int chan, bus_dmamap_t *dmamap_in,
		    bus_dmamap_t *dmamap_out, int error)
{
	struct gtidmac_softc *sc;
	struct dmover_request *dreq = object;
	struct dmover_backend *dmb;
	struct gtidmac_function *df;
	uint32_t result;
	int ninputs, i;

	KASSERT(dreq != NULL);

	dmb = dreq->dreq_assignment->das_backend;
	df = dreq->dreq_assignment->das_algdesc->dad_data;
	ninputs = dreq->dreq_assignment->das_algdesc->dad_ninputs;
	sc = dmb->dmb_cookie;

	result = (*df->dma_finish)(sc, chan, error);
	for (i = 0; i < ninputs; i++)
		gtidmac_dmmap_unload(sc, *(dmamap_in + i), 0);
	if (dreq->dreq_assignment->das_algdesc->dad_name ==
	    DMOVER_FUNC_ISCSI_CRC32C)
		memcpy(dreq->dreq_immediate, &result, sizeof(result));
	else
		gtidmac_dmmap_unload(sc, *dmamap_out, 1);

	(*df->chan_free)(sc, chan);

	if (error) {
		dreq->dreq_error = error;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
	}

	dmover_done(dreq);

	/*
	 * See if we can start some more dmover(9) requests.
	 *
	 * Note: We're already at splbio() here.
	 */
	if (!sc->sc_dmb_busy)
		gtidmac_dmover_run(dmb);
}

static __inline int
gtidmac_dmmap_load(struct gtidmac_softc *sc, bus_dmamap_t dmamap,
		   dmover_buffer_type dmbuf_type, dmover_buffer *dmbuf,
		   int read)
{
	int error, flags;

	flags = BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    read ? BUS_DMA_READ : BUS_DMA_WRITE;

	switch (dmbuf_type) {
	case DMOVER_BUF_LINEAR:
		error = bus_dmamap_load(sc->sc_dmat, dmamap,
		    dmbuf->dmbuf_linear.l_addr, dmbuf->dmbuf_linear.l_len,
		    NULL, flags);
		break;

	case DMOVER_BUF_UIO:
		if ((read && dmbuf->dmbuf_uio->uio_rw != UIO_READ) ||
		    (!read && dmbuf->dmbuf_uio->uio_rw == UIO_READ))
			return (EINVAL);

		error = bus_dmamap_load_uio(sc->sc_dmat, dmamap,
		    dmbuf->dmbuf_uio, flags);
		break;

	default:
		error = EINVAL;
	}

	if (error == 0)
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    read ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	return error;
}

static __inline void
gtidmac_dmmap_unload(struct gtidmac_softc *sc, bus_dmamap_t dmamap, int read)
{

	bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
	    read ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->sc_dmat, dmamap);
}


void *
gtidmac_tag_get(void)
{

	return gtidmac_softc;
}

/*
 * IDMAC functions
 */
int
gtidmac_chan_alloc(void *tag, bus_dmamap_t **dmamap_in,
		   bus_dmamap_t **dmamap_out, void *object)
{
	struct gtidmac_softc *sc = tag;
	int chan;

/* maybe need lock */

	for (chan = 0; chan < sc->sc_gtidmac_nchan; chan++)
		if (sc->sc_cdesc[chan].chan_running == NULL)
			break;
	if (chan >= sc->sc_gtidmac_nchan)
		return -1;


	sc->sc_cdesc[chan].chan_running = object;

/* unlock */

	*dmamap_in = &sc->sc_cdesc[chan].chan_in;
	*dmamap_out = &sc->sc_cdesc[chan].chan_out;

	return chan;
}

void
gtidmac_chan_free(void *tag, int chan)
{
	struct gtidmac_softc *sc = tag;

/* maybe need lock */

	sc->sc_cdesc[chan].chan_running = NULL;

/* unlock */
}

/* ARGSUSED */
int
gtidmac_setup(void *tag, int chan, int ninputs, bus_dmamap_t *dmamap_in,
	      bus_dmamap_t *dmamap_out, bus_size_t size)
{
	struct gtidmac_softc *sc = tag;
	struct gtidmac_dma_desc *dd, *fstdd, *nxtdd;
	struct gtidmac_desc *desc;
	uint32_t ccl, bcnt, ires, ores;
	int n = 0, iidx, oidx;

	KASSERT(ninputs == 0 || ninputs == 1);

	ccl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCLR(chan));
#ifdef DIAGNOSTIC
	if (ccl & GTIDMAC_CCLR_CHANACT)
		panic("gtidmac_setup: chan%d already active", chan);
#endif

	/* We always Chain-mode and max (16M - 1)byte/desc */
	ccl = (GTIDMAC_CCLR_DESCMODE_16M				|
#ifdef GTIDMAC_DEBUG
	    GTIDMAC_CCLR_CDEN						|
#endif
	    GTIDMAC_CCLR_TRANSFERMODE_B /* Transfer Mode: Block */	|
	    GTIDMAC_CCLR_INTMODE_NULL   /* Intr Mode: Next Desc NULL */	|
	    GTIDMAC_CCLR_CHAINMODE_C    /* Chain Mode: Chaind */);
	if (size != (*dmamap_in)->dm_mapsize) {
		ccl |= GTIDMAC_CCLR_SRCHOLD;
		if ((*dmamap_in)->dm_mapsize == 8)
			ccl |= GTIDMAC_CCLR_SBL_8B;
		else if ((*dmamap_in)->dm_mapsize == 16)
			ccl |= GTIDMAC_CCLR_SBL_16B;
		else if ((*dmamap_in)->dm_mapsize == 32)
			ccl |= GTIDMAC_CCLR_SBL_32B;
		else if ((*dmamap_in)->dm_mapsize == 64)
			ccl |= GTIDMAC_CCLR_SBL_64B;
		else if ((*dmamap_in)->dm_mapsize == 128)
			ccl |= GTIDMAC_CCLR_SBL_128B;
		else
			panic("gtidmac_setup: chan%d source:"
			    " unsupport hold size", chan);
	} else
		ccl |= GTIDMAC_CCLR_SBL_128B;
	if (size != (*dmamap_out)->dm_mapsize) {
		ccl |= GTIDMAC_CCLR_DESTHOLD;
		if ((*dmamap_out)->dm_mapsize == 8)
			ccl |= GTIDMAC_CCLR_DBL_8B;
		else if ((*dmamap_out)->dm_mapsize == 16)
			ccl |= GTIDMAC_CCLR_DBL_16B;
		else if ((*dmamap_out)->dm_mapsize == 32)
			ccl |= GTIDMAC_CCLR_DBL_32B;
		else if ((*dmamap_out)->dm_mapsize == 64)
			ccl |= GTIDMAC_CCLR_DBL_64B;
		else if ((*dmamap_out)->dm_mapsize == 128)
			ccl |= GTIDMAC_CCLR_DBL_128B;
		else
			panic("gtidmac_setup: chan%d destination:"
			    " unsupport hold size", chan);
	} else
		ccl |= GTIDMAC_CCLR_DBL_128B;

	fstdd = SLIST_FIRST(&sc->sc_dlist);
	if (fstdd == NULL) {
		aprint_error_dev(sc->sc_dev, "no descriptor\n");
		return ENOMEM;
	}
	SLIST_REMOVE_HEAD(&sc->sc_dlist, dd_next);
	sc->sc_cdesc[chan].chan_ddidx = fstdd->dd_index;

	dd = fstdd;
	ires = ores = 0;
	iidx = oidx = 0;
	while (1 /*CONSTCOND*/) {
		if (ccl & GTIDMAC_CCLR_SRCHOLD) {
			if (ccl & GTIDMAC_CCLR_DESTHOLD)
				bcnt = size;	/* src/dst hold */
			else
				bcnt = (*dmamap_out)->dm_segs[oidx].ds_len;
		} else if (ccl & GTIDMAC_CCLR_DESTHOLD)
			bcnt = (*dmamap_in)->dm_segs[iidx].ds_len;
		else
			bcnt = min((*dmamap_in)->dm_segs[iidx].ds_len - ires,
			    (*dmamap_out)->dm_segs[oidx].ds_len - ores);

		desc = dd->dd_idmac_vaddr;
		desc->bc.mode16m.bcnt =
		    bcnt | GTIDMAC_CIDMABCR_BCLEFT | GTIDMAC_CIDMABCR_OWN;
		desc->srcaddr = (*dmamap_in)->dm_segs[iidx].ds_addr + ires;
		desc->dstaddr = (*dmamap_out)->dm_segs[oidx].ds_addr + ores;

		n += bcnt;
		if (n >= size)
			break;
		if (!(ccl & GTIDMAC_CCLR_SRCHOLD)) {
			ires += bcnt;
			if (ires >= (*dmamap_in)->dm_segs[iidx].ds_len) {
				ires = 0;
				iidx++;
				KASSERT(iidx < (*dmamap_in)->dm_nsegs);
			}
		}
		if (!(ccl & GTIDMAC_CCLR_DESTHOLD)) {
			ores += bcnt;
			if (ores >= (*dmamap_out)->dm_segs[oidx].ds_len) {
				ores = 0;
				oidx++;
				KASSERT(oidx < (*dmamap_out)->dm_nsegs);
			}
		}

		nxtdd = SLIST_FIRST(&sc->sc_dlist);
		if (nxtdd == NULL) {
			aprint_error_dev(sc->sc_dev, "no descriptor\n");
			return ENOMEM;
		}
		SLIST_REMOVE_HEAD(&sc->sc_dlist, dd_next);

		desc->nextdp = (uint32_t)nxtdd->dd_paddr;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
#ifdef GTIDMAC_DEBUG
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
#else
		    BUS_DMASYNC_PREWRITE);
#endif

		SLIST_INSERT_AFTER(dd, nxtdd, dd_next);
		dd = nxtdd;
	}
	desc->nextdp = (uint32_t)NULL;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap, dd->dd_index * sizeof(*desc),
#ifdef GTIDMAC_DEBUG
	    sizeof(*desc), BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
#else
	    sizeof(*desc), BUS_DMASYNC_PREWRITE);
#endif

	/* Set paddr of descriptor to Channel Next Descriptor Pointer */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CNDPR(chan),
	    fstdd->dd_paddr);

#if BYTE_ORDER == LITTLE_ENDIAN
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCHR(chan),
	    GTIDMAC_CCHR_DESCBYTESWAP | GTIDMAC_CCHR_ENDIAN_LE);
#else
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCHR(chan),
	    GTIDMAC_CCHR_DESCBYTESWAP | GTIDMAC_CCHR_ENDIAN_BE);
#endif
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCLR(chan), ccl);

#ifdef GTIDMAC_DEBUG
	gtidmac_dump_idmacdesc(sc, fstdd, ccl, 0/*pre*/);
#endif

	sc->sc_cdesc[chan].chan_totalcnt += size;

	return 0;
}

void
gtidmac_start(void *tag, int chan,
	      void (*dma_done_cb)(void *, int, bus_dmamap_t *, bus_dmamap_t *,
				  int))
{
	struct gtidmac_softc *sc = tag;
	uint32_t ccl;

	DPRINTF(("%s:%d: starting\n", device_xname(sc->sc_dev), chan));

#ifdef GTIDMAC_DEBUG
	gtidmac_dump_idmacreg(sc, chan);
#endif

	sc->sc_cdesc[chan].chan_dma_done = dma_done_cb;

	ccl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCLR(chan));
	/* Start and 'Fetch Next Descriptor' */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCLR(chan),
	    ccl | GTIDMAC_CCLR_CHANEN | GTIDMAC_CCLR_FETCHND);
}

static uint32_t
gtidmac_finish(void *tag, int chan, int error)
{
	struct gtidmac_softc *sc = tag;
	struct gtidmac_dma_desc *dd, *fstdd, *nxtdd;
	struct gtidmac_desc *desc;

	fstdd = &sc->sc_dd_buffer[sc->sc_cdesc[chan].chan_ddidx];

#ifdef GTIDMAC_DEBUG
	if (error || gtidmac_debug > 1) {
		uint32_t ccl;

		gtidmac_dump_idmacreg(sc, chan);
		ccl = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    GTIDMAC_CCLR(chan));
		gtidmac_dump_idmacdesc(sc, fstdd, ccl, 1/*post*/);
	}
#endif

	dd = fstdd;
	do {
		desc = dd->dd_idmac_vaddr;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
#ifdef GTIDMAC_DEBUG
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#else
		    BUS_DMASYNC_POSTWRITE);
#endif

		nxtdd = SLIST_NEXT(dd, dd_next);
		SLIST_INSERT_HEAD(&sc->sc_dlist, dd, dd_next);
		dd = nxtdd;
	} while (desc->nextdp);

	return 0;
}

/*
 * XORE functions
 */
int
mvxore_chan_alloc(void *tag, bus_dmamap_t **dmamap_in,
		  bus_dmamap_t **dmamap_out, void *object)
{
	struct gtidmac_softc *sc = tag;
	int chan;

/* maybe need lock */

	for (chan = 0; chan < sc->sc_mvxore_nchan; chan++)
		if (sc->sc_cdesc_xore[chan].chan_running == NULL)
			break;
	if (chan >= sc->sc_mvxore_nchan)
		return -1;


	sc->sc_cdesc_xore[chan].chan_running = object;

/* unlock */

	*dmamap_in = sc->sc_cdesc_xore[chan].chan_in;
	*dmamap_out = &sc->sc_cdesc_xore[chan].chan_out;

	return chan;
}

void
mvxore_chan_free(void *tag, int chan)
{
	struct gtidmac_softc *sc = tag;

/* maybe need lock */

	sc->sc_cdesc_xore[chan].chan_running = NULL;

/* unlock */
}

/* ARGSUSED */
int
mvxore_setup(void *tag, int chan, int ninputs, bus_dmamap_t *dmamap_in,
	     bus_dmamap_t *dmamap_out, bus_size_t size)
{
	struct gtidmac_softc *sc = tag;
	struct gtidmac_dma_desc *dd, *fstdd, *nxtdd;
	struct mvxore_desc *desc;
	uint32_t xexc, bcnt, cmd, lastcmd;
	int n = 0, i;
	uint32_t ires[MVXORE_NSRC] = { 0, 0, 0, 0, 0, 0, 0, 0 }, ores = 0;
	int iidx[MVXORE_NSRC] = { 0, 0, 0, 0, 0, 0, 0, 0 }, oidx = 0;

#ifdef DIAGNOSTIC
	uint32_t xexact;

	xexact =
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXACTR(sc, chan));
	if ((xexact & MVXORE_XEXACTR_XESTATUS_MASK) ==
	    MVXORE_XEXACTR_XESTATUS_ACT)
		panic("mvxore_setup: chan%d already active."
		    " mvxore not support hot insertion", chan);
#endif

	xexc =
	    (MVXORE_XEXCR_REGACCPROTECT	|
	     MVXORE_XEXCR_DBL_128B	|
	     MVXORE_XEXCR_SBL_128B);
	cmd = lastcmd = 0;
	if (ninputs > 1) {
		xexc |= MVXORE_XEXCR_OM_XOR;
		lastcmd = cmd = (1 << ninputs) - 1;
	} else if (ninputs == 1) {
		if ((*dmamap_out)->dm_nsegs == 0) {
			xexc |= MVXORE_XEXCR_OM_CRC32;
			lastcmd = MVXORE_DESC_CMD_CRCLAST;
		} else
			xexc |= MVXORE_XEXCR_OM_DMA;
	} else if (ninputs == 0) {
		if ((*dmamap_out)->dm_nsegs != 1) {
			aprint_error_dev(sc->sc_dev,
			    "XORE not supports %d DMA segments\n",
			    (*dmamap_out)->dm_nsegs);
			return EINVAL;
		}

		if ((*dmamap_in)->dm_mapsize == 0) {
			xexc |= MVXORE_XEXCR_OM_ECC;

			/* XXXXX: Maybe need to set Timer Mode registers? */

#if 0
		} else if ((*dmamap_in)->dm_mapsize == 8 ||
		    (*dmamap_in)->dm_mapsize == 16) { /* in case dmover */
			uint64_t pattern;

			/* XXXX: Get pattern data */

			KASSERT((*dmamap_in)->dm_mapsize == 8 ||
			    (void *)((uint32_t)(*dmamap_in)->_dm_origbuf &
						~PAGE_MASK) == sc->sc_pbuf);
			pattern = *(uint64_t *)(*dmamap_in)->_dm_origbuf;

			/* XXXXX: XORE has a IVR.  We should get this first. */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEIVRL,
			    pattern);
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEIVRH,
			    pattern >> 32);

			xexc |= MVXORE_XEXCR_OM_MEMINIT;
#endif
		} else {
			aprint_error_dev(sc->sc_dev,
			    "XORE not supports DMA mapsize %zd\n",
			    (*dmamap_in)->dm_mapsize);
			return EINVAL;
		}
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXDPR(sc, chan), (*dmamap_out)->dm_segs[0].ds_addr);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXBSR(sc, chan), (*dmamap_out)->dm_mapsize);

		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXCR(sc, chan), xexc);
		sc->sc_cdesc_xore[chan].chan_totalcnt += size;

		return 0;
	}

	/* Make descriptor for DMA/CRC32/XOR */

	fstdd = SLIST_FIRST(&sc->sc_dlist_xore);
	if (fstdd == NULL) {
		aprint_error_dev(sc->sc_dev, "no xore descriptor\n");
		return ENOMEM;
	}
	SLIST_REMOVE_HEAD(&sc->sc_dlist_xore, dd_next);
	sc->sc_cdesc_xore[chan].chan_ddidx =
	    fstdd->dd_index + GTIDMAC_NDESC * sc->sc_gtidmac_nchan;

	dd = fstdd;
	while (1 /*CONSTCOND*/) {
		desc = dd->dd_xore_vaddr;
		desc->stat = MVXORE_DESC_STAT_OWN;
		desc->cmd = cmd;
		if ((*dmamap_out)->dm_nsegs != 0) {
			desc->dstaddr =
			    (*dmamap_out)->dm_segs[oidx].ds_addr + ores;
			bcnt = (*dmamap_out)->dm_segs[oidx].ds_len - ores;
		} else {
			desc->dstaddr = 0;
			bcnt = MVXORE_MAXXFER;	/* XXXXX */
		}
		for (i = 0; i < ninputs; i++) {
			desc->srcaddr[i] =
			    (*dmamap_in[i]).dm_segs[iidx[i]].ds_addr + ires[i];
			bcnt = min(bcnt,
			    (*dmamap_in[i]).dm_segs[iidx[i]].ds_len - ires[i]);
		}
		desc->bcnt = bcnt;

		n += bcnt;
		if (n >= size)
			break;
		ores += bcnt;
		if ((*dmamap_out)->dm_nsegs != 0 &&
		    ores >= (*dmamap_out)->dm_segs[oidx].ds_len) {
			ores = 0;
			oidx++;
			KASSERT(oidx < (*dmamap_out)->dm_nsegs);
		}
		for (i = 0; i < ninputs; i++) {
			ires[i] += bcnt;
			if (ires[i] >=
			    (*dmamap_in[i]).dm_segs[iidx[i]].ds_len) {
				ires[i] = 0;
				iidx[i]++;
				KASSERT(iidx[i] < (*dmamap_in[i]).dm_nsegs);
			}
		}

		nxtdd = SLIST_FIRST(&sc->sc_dlist_xore);
		if (nxtdd == NULL) {
			aprint_error_dev(sc->sc_dev, "no xore descriptor\n");
			return ENOMEM;
		}
		SLIST_REMOVE_HEAD(&sc->sc_dlist_xore, dd_next);

		desc->nextda = (uint32_t)nxtdd->dd_paddr;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		SLIST_INSERT_AFTER(dd, nxtdd, dd_next);
		dd = nxtdd;
	}
	desc->cmd = lastcmd;
	desc->nextda = (uint32_t)NULL;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
	    dd->dd_index * sizeof(*desc), sizeof(*desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Set paddr of descriptor to Channel Next Descriptor Pointer */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXNDPR(sc, chan),
	    fstdd->dd_paddr);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXCR(sc, chan), xexc);

#ifdef GTIDMAC_DEBUG
	gtidmac_dump_xoredesc(sc, fstdd, xexc, 0/*pre*/);
#endif

	sc->sc_cdesc_xore[chan].chan_totalcnt += size;

	return 0;
}

void
mvxore_start(void *tag, int chan,
	     void (*dma_done_cb)(void *, int, bus_dmamap_t *, bus_dmamap_t *,
				 int))
{
	struct gtidmac_softc *sc = tag;
	uint32_t xexact;

	DPRINTF(("%s:%d: xore starting\n", device_xname(sc->sc_dev), chan));

#ifdef GTIDMAC_DEBUG
	gtidmac_dump_xorereg(sc, chan);
#endif

	sc->sc_cdesc_xore[chan].chan_dma_done = dma_done_cb;

	xexact =
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXACTR(sc, chan));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXACTR(sc, chan),
	    xexact | MVXORE_XEXACTR_XESTART);
}

static uint32_t
mvxore_finish(void *tag, int chan, int error)
{
	struct gtidmac_softc *sc = tag;
	struct gtidmac_dma_desc *dd, *fstdd, *nxtdd;
	struct mvxore_desc *desc;
	uint32_t xexc;

#ifdef GTIDMAC_DEBUG
	if (error || gtidmac_debug > 1)
		gtidmac_dump_xorereg(sc, chan);
#endif

	xexc = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXCR(sc, chan));
	if ((xexc & MVXORE_XEXCR_OM_MASK) == MVXORE_XEXCR_OM_ECC ||
	    (xexc & MVXORE_XEXCR_OM_MASK) == MVXORE_XEXCR_OM_MEMINIT)
		return 0;

	fstdd = &sc->sc_dd_buffer[sc->sc_cdesc_xore[chan].chan_ddidx];

#ifdef GTIDMAC_DEBUG
	if (error || gtidmac_debug > 1)
		gtidmac_dump_xoredesc(sc, fstdd, xexc, 1/*post*/);
#endif

	dd = fstdd;
	do {
		desc = dd->dd_xore_vaddr;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		nxtdd = SLIST_NEXT(dd, dd_next);
		SLIST_INSERT_HEAD(&sc->sc_dlist_xore, dd, dd_next);
		dd = nxtdd;
	} while (desc->nextda);

	if ((xexc & MVXORE_XEXCR_OM_MASK) == MVXORE_XEXCR_OM_CRC32)
		return desc->result;
	return 0;
}

static void
gtidmac_wininit(struct gtidmac_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t size, cxap, en, winacc;
	int window, target, attr, rv, i, j;

	en = 0xff;
	cxap = 0;
	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < GTIDMAC_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (base > 0xffffffffULL) {
			if (window >= GTIDMAC_NREMAP) {
				aprint_error_dev(sc->sc_dev,
				    "can't remap window %d\n", window);
				continue;
			}
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    GTIDMAC_HARXR(window), (base >> 32) & 0xffffffff);
		}
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_BARX(window),
		    GTIDMAC_BARX_TARGET(target)	|
		    GTIDMAC_BARX_ATTR(attr)	|
		    GTIDMAC_BARX_BASE(base));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_SRX(window),
		    GTIDMAC_SRX_SIZE(size));
		en &= ~GTIDMAC_BAER_EN(window);

		winacc = GTIDMAC_CXAPR_WINACC_FA;
		if (gtidmac_winacctbl != NULL)
			for (j = 0;
			    gtidmac_winacctbl[j].tag != MARVELL_TAG_UNDEFINED;
			    j++) {
				if (gtidmac_winacctbl[j].tag != tags[i])
					continue;

				switch (gtidmac_winacctbl[j].winacc) {
				case GTIDMAC_WINACC_NOACCESSALLOWED:
					winacc = GTIDMAC_CXAPR_WINACC_NOAA;
					break;
				case GTIDMAC_WINACC_READONLY:
					winacc = GTIDMAC_CXAPR_WINACC_RO;
					break;
				case GTIDMAC_WINACC_FULLACCESS:
				default: /* XXXX: default is full access */
					break;
				}
				break;
			}
		cxap |= GTIDMAC_CXAPR_WINACC(window, winacc);

		window++;
	}
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_BAER, en);

	for (i = 0; i < GTIDMAC_NACCPROT; i++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CXAPR(i),
		    cxap);
}

static void
mvxore_wininit(struct gtidmac_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t target, attr, size, xexwc, winacc;
	int window, rv, i, j, p;

	xexwc = 0;
	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MVXORE_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (base > 0xffffffffULL) {
			if (window >= MVXORE_NREMAP) {
				aprint_error_dev(sc->sc_dev,
				    "can't remap window %d\n", window);
				continue;
			}
			for (p = 0; p < sc->sc_mvxore_nchan >> 1; p++)
				bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				    MVXORE_XEHARRX(sc, p, window),
				    (base >> 32) & 0xffffffff);
		}

		for (p = 0; p < sc->sc_mvxore_nchan >> 1; p++) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XEBARX(sc, p, window),
			    MVXORE_XEBARX_TARGET(target) |
			    MVXORE_XEBARX_ATTR(attr) |
			    MVXORE_XEBARX_BASE(base));
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XESMRX(sc, p, window),
			    MVXORE_XESMRX_SIZE(size));
		}

		winacc = MVXORE_XEXWCR_WINACC_FA;
		if (mvxore_winacctbl != NULL)
			for (j = 0;
			    mvxore_winacctbl[j].tag != MARVELL_TAG_UNDEFINED;
			    j++) {
				if (gtidmac_winacctbl[j].tag != tags[i])
					continue;

				switch (gtidmac_winacctbl[j].winacc) {
				case GTIDMAC_WINACC_NOACCESSALLOWED:
					winacc = MVXORE_XEXWCR_WINACC_NOAA;
					break;
				case GTIDMAC_WINACC_READONLY:
					winacc = MVXORE_XEXWCR_WINACC_RO;
					break;
				case GTIDMAC_WINACC_FULLACCESS:
				default: /* XXXX: default is full access */
					break;
				}
				break;
			}
		xexwc |= (MVXORE_XEXWCR_WINEN(window) |
		    MVXORE_XEXWCR_WINACC(window, winacc));
		window++;
	}

	for (i = 0; i < sc->sc_mvxore_nchan; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXWCR(sc, i),
		    xexwc);

		/* XXXXX: reset... */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXAOCR(sc, 0),
		    0);
	}
}

static int
gtidmac_buffer_setup(struct gtidmac_softc *sc)
{
	bus_dma_segment_t segs;
	struct gtidmac_dma_desc *dd;
	uint32_t mask;
	int nchan, nsegs, i;

	nchan = sc->sc_gtidmac_nchan;

	if (bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC * nchan,
	    PAGE_SIZE, 0, &segs, 1, &nsegs, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamem_alloc failed: descriptor buffer\n");
		goto fail0;
	}
	if (bus_dmamem_map(sc->sc_dmat, &segs, 1,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC * nchan,
	    (void **)&sc->sc_dbuf, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamem_map failed: descriptor buffer\n");
		goto fail1;
	}
	if (bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC * nchan, 1,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC * nchan, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dmap)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamap_create failed: descriptor buffer\n");
		goto fail2;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmap, sc->sc_dbuf,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC * nchan,
	    NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamap_load failed: descriptor buffer\n");
		goto fail3;
	}
	SLIST_INIT(&sc->sc_dlist);
	for (i = 0; i < GTIDMAC_NDESC * nchan; i++) {
		dd = &sc->sc_dd_buffer[i];
		dd->dd_index = i;
		dd->dd_idmac_vaddr = &sc->sc_dbuf[i];
		dd->dd_paddr = sc->sc_dmap->dm_segs[0].ds_addr +
		    (sizeof(struct gtidmac_desc) * i);
			SLIST_INSERT_HEAD(&sc->sc_dlist, dd, dd_next);
	}

	/* Initialize IDMAC DMA channels */
	mask = 0;
	for (i = 0; i < nchan; i++) {
		if (i > 0 && ((i * GTIDMAC_I_BITS) & 31 /*bit*/) == 0) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    GTIDMAC_IMR(i - 1), mask);
			mask = 0;
		}

		if (bus_dmamap_create(sc->sc_dmat, GTIDMAC_MAXXFER,
		    GTIDMAC_NSEGS, GTIDMAC_MAXXFER, 0, BUS_DMA_NOWAIT,
		    &sc->sc_cdesc[i].chan_in)) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_create failed: chan%d in\n", i);
			goto fail4;
		}
		if (bus_dmamap_create(sc->sc_dmat, GTIDMAC_MAXXFER,
		    GTIDMAC_NSEGS, GTIDMAC_MAXXFER, 0, BUS_DMA_NOWAIT,
		    &sc->sc_cdesc[i].chan_out)) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_create failed: chan%d out\n", i);
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_cdesc[i].chan_in);
			goto fail4;
		}
		sc->sc_cdesc[i].chan_totalcnt = 0;
		sc->sc_cdesc[i].chan_running = NULL;

		/* Ignore bits overflow.  The mask is 32bit. */
		mask |= GTIDMAC_I(i,
		    GTIDMAC_I_COMP	|
		    GTIDMAC_I_ADDRMISS	|
		    GTIDMAC_I_ACCPROT	|
		    GTIDMAC_I_WRPROT	|
		    GTIDMAC_I_OWN);

		/* 8bits/channel * 4channels => 32bit */
		if ((i & 0x3) == 0x3) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    GTIDMAC_IMR(i), mask);
			mask = 0;
		}
	}

	return 0;

fail4:
	for (; i-- > 0;) {
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdesc[i].chan_in);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdesc[i].chan_out);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap);
fail3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap);
fail2:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_dbuf,
	    sizeof(struct gtidmac_desc) * GTIDMAC_NDESC);
fail1:
	bus_dmamem_free(sc->sc_dmat, &segs, 1);
fail0:
	return -1;
}

static int
mvxore_buffer_setup(struct gtidmac_softc *sc)
{
	bus_dma_segment_t segs;
	struct gtidmac_dma_desc *dd;
	uint32_t mask;
	int nchan, nsegs, i, j;

	nchan = sc->sc_mvxore_nchan;

	if (bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC * nchan,
	    PAGE_SIZE, 0, &segs, 1, &nsegs, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamem_alloc failed: xore descriptor buffer\n");
		goto fail0;
	}
	if (bus_dmamem_map(sc->sc_dmat, &segs, 1,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC * nchan,
	    (void **)&sc->sc_dbuf_xore, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamem_map failed: xore descriptor buffer\n");
		goto fail1;
	}
	if (bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC * nchan, 1,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC * nchan, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dmap_xore)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamap_create failed: xore descriptor buffer\n");
		goto fail2;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmap_xore, sc->sc_dbuf_xore,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC * nchan,
	    NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "bus_dmamap_load failed: xore descriptor buffer\n");
		goto fail3;
	}
	SLIST_INIT(&sc->sc_dlist_xore);
	for (i = 0; i < MVXORE_NDESC * nchan; i++) {
		dd =
		    &sc->sc_dd_buffer[i + GTIDMAC_NDESC * sc->sc_gtidmac_nchan];
		dd->dd_index = i;
		dd->dd_xore_vaddr = &sc->sc_dbuf_xore[i];
		dd->dd_paddr = sc->sc_dmap_xore->dm_segs[0].ds_addr +
		    (sizeof(struct mvxore_desc) * i);
		SLIST_INSERT_HEAD(&sc->sc_dlist_xore, dd, dd_next);
	}

	/* Initialize XORE DMA channels */
	mask = 0;
	for (i = 0; i < nchan; i++) {
		for (j = 0; j < MVXORE_NSRC; j++) {
			if (bus_dmamap_create(sc->sc_dmat,
			    MVXORE_MAXXFER, MVXORE_NSEGS,
			    MVXORE_MAXXFER, 0, BUS_DMA_NOWAIT,
			    &sc->sc_cdesc_xore[i].chan_in[j])) {
				aprint_error_dev(sc->sc_dev,
				    "bus_dmamap_create failed:"
				    " xore chan%d in[%d]\n", i, j);
				goto fail4;
			}
		}
		if (bus_dmamap_create(sc->sc_dmat, MVXORE_MAXXFER,
		    MVXORE_NSEGS, MVXORE_MAXXFER, 0,
		    BUS_DMA_NOWAIT, &sc->sc_cdesc_xore[i].chan_out)) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_create failed: chan%d out\n", i);
			goto fail5;
		}
		sc->sc_cdesc_xore[i].chan_totalcnt = 0;
		sc->sc_cdesc_xore[i].chan_running = NULL;

		mask |= MVXORE_I(i,
		    MVXORE_I_EOC	|
		    MVXORE_I_ADDRDECODE	|
		    MVXORE_I_ACCPROT	|
		    MVXORE_I_WRPROT	|
		    MVXORE_I_OWN	|
		    MVXORE_I_INTPARITY	|
		    MVXORE_I_XBAR);

		/* 16bits/channel * 2channels => 32bit */
		if (i & 0x1) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XEIMR(sc, i >> 1), mask);
			mask = 0;
		}
	}

	return 0;

	for (; i-- > 0;) {
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdesc_xore[i].chan_out);

fail5:
		j = MVXORE_NSRC;
fail4:
		for (; j-- > 0;)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_cdesc_xore[i].chan_in[j]);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap_xore);
fail3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap_xore);
fail2:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_dbuf_xore,
	    sizeof(struct mvxore_desc) * MVXORE_NDESC);
fail1:
	bus_dmamem_free(sc->sc_dmat, &segs, 1);
fail0:
	return -1;
}

#ifdef GTIDMAC_DEBUG
static void
gtidmac_dump_idmacreg(struct gtidmac_softc *sc, int chan)
{
	uint32_t val;
	char buf[256];

	printf("IDMAC Registers\n");

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CIDMABCR(chan));
	snprintb(buf, sizeof(buf), "\177\020b\037Own\0b\036BCLeft\0", val);
	printf("  Byte Count                 : %s\n", buf);
	printf("    ByteCnt                  :   0x%06x\n",
	    val & GTIDMAC_CIDMABCR_BYTECNT_MASK);
	printf("  Source Address             : 0x%08x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CIDMASAR(chan)));
	printf("  Destination Address        : 0x%08x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CIDMADAR(chan)));
	printf("  Next Descriptor Pointer    : 0x%08x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CNDPR(chan)));
	printf("  Current Descriptor Pointer : 0x%08x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCDPR(chan)));

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCLR(chan));
	snprintb(buf, sizeof(buf),
	    "\177\020b\024Abr\0b\021CDEn\0b\016ChanAct\0b\015FetchND\0"
	    "b\014ChanEn\0b\012IntMode\0b\005DestHold\0b\003SrcHold\0",
	    val);
	printf("  Channel Control (Low)      : %s\n", buf);
	printf("    SrcBurstLimit            : %s Bytes\n",
	  (val & GTIDMAC_CCLR_SBL_MASK) == GTIDMAC_CCLR_SBL_128B ? "128" :
	    (val & GTIDMAC_CCLR_SBL_MASK) == GTIDMAC_CCLR_SBL_64B ? "64" :
	    (val & GTIDMAC_CCLR_SBL_MASK) == GTIDMAC_CCLR_SBL_32B ? "32" :
	    (val & GTIDMAC_CCLR_SBL_MASK) == GTIDMAC_CCLR_SBL_16B ? "16" :
	    (val & GTIDMAC_CCLR_SBL_MASK) == GTIDMAC_CCLR_SBL_8B ? "8" :
	    "unknwon");
	printf("    DstBurstLimit            : %s Bytes\n",
	  (val & GTIDMAC_CCLR_DBL_MASK) == GTIDMAC_CCLR_DBL_128B ? "128" :
	    (val & GTIDMAC_CCLR_DBL_MASK) == GTIDMAC_CCLR_DBL_64B ? "64" :
	    (val & GTIDMAC_CCLR_DBL_MASK) == GTIDMAC_CCLR_DBL_32B ? "32" :
	    (val & GTIDMAC_CCLR_DBL_MASK) == GTIDMAC_CCLR_DBL_16B ? "16" :
	    (val & GTIDMAC_CCLR_DBL_MASK) == GTIDMAC_CCLR_DBL_8B ? "8" :
	    "unknwon");
	printf("    ChainMode                : %sChained\n",
	    val & GTIDMAC_CCLR_CHAINMODE_NC ? "Non-" : "");
	printf("    TransferMode             : %s\n",
	    val & GTIDMAC_CCLR_TRANSFERMODE_B ? "Block" : "Demand");
	printf("    DescMode                 : %s\n",
	    val & GTIDMAC_CCLR_DESCMODE_16M ? "16M" : "64k");
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIDMAC_CCHR(chan));
	snprintb(buf, sizeof(buf),
	    "\177\020b\001DescByteSwap\0b\000Endianness\0", val);
	printf("  Channel Control (High)     : %s\n", buf);
}

static void
gtidmac_dump_idmacdesc(struct gtidmac_softc *sc, struct gtidmac_dma_desc *dd,
		       uint32_t mode, int post)
{
	struct gtidmac_desc *desc;
	int i;
	char buf[256];

	printf("IDMAC Descriptor\n");

	i = 0;
	while (1 /*CONSTCOND*/) {
		if (post)
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap,
			    dd->dd_index * sizeof(*desc), sizeof(*desc),
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		desc = dd->dd_idmac_vaddr;

		printf("%d (0x%lx)\n", i, dd->dd_paddr);
		if (mode & GTIDMAC_CCLR_DESCMODE_16M) {
			snprintb(buf, sizeof(buf),
			    "\177\020b\037Own\0b\036BCLeft\0",
			    desc->bc.mode16m.bcnt);
			printf("  Byte Count              : %s\n", buf);
			printf("    ByteCount             :   0x%06x\n",
			    desc->bc.mode16m.bcnt &
			    GTIDMAC_CIDMABCR_BYTECNT_MASK);
		} else {
			printf("  Byte Count              :     0x%04x\n",
			    desc->bc.mode64k.bcnt);
			printf("  Remind Byte Count       :     0x%04x\n",
			    desc->bc.mode64k.rbc);
		}
		printf("  Source Address          : 0x%08x\n", desc->srcaddr);
		printf("  Destination Address     : 0x%08x\n", desc->dstaddr);
		printf("  Next Descriptor Pointer : 0x%08x\n", desc->nextdp);

		if (desc->nextdp == (uint32_t)NULL)
			break;

		if (!post)
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap,
			    dd->dd_index * sizeof(*desc), sizeof(*desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		i++;
		dd = SLIST_NEXT(dd, dd_next);
	}
	if (!post)
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

static void
gtidmac_dump_xorereg(struct gtidmac_softc *sc, int chan)
{
	uint32_t val, opmode;
	char buf[64];

	printf("XORE Registers\n");

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXCR(sc, chan));
	snprintb(buf, sizeof(buf),
	    "\177\020"
	    "b\017RegAccProtect\0b\016DesSwp\0b\015DwrReqSwp\0b\014DrdResSwp\0",
	    val);
	printf(" Configuration    : 0x%s\n", buf);
	opmode = val & MVXORE_XEXCR_OM_MASK;
	printf("    OperationMode : %s operation\n",
	  opmode == MVXORE_XEXCR_OM_XOR ? "XOR calculate" :
	  opmode == MVXORE_XEXCR_OM_CRC32 ? "CRC-32 calculate" :
	  opmode == MVXORE_XEXCR_OM_DMA ? "DMA" :
	  opmode == MVXORE_XEXCR_OM_ECC ? "ECC cleanup" :
	  opmode == MVXORE_XEXCR_OM_MEMINIT ? "Memory Initialization" :
	  "unknown");
	printf("    SrcBurstLimit : %s Bytes\n",
	  (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_128B ? "128" :
	    (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_64B ? "64" :
	    (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_32B ? "32" :
	    "unknwon");
	printf("    DstBurstLimit : %s Bytes\n",
	  (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_128B ? "128" :
	    (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_64B ? "64" :
	    (val & MVXORE_XEXCR_SBL_MASK) == MVXORE_XEXCR_SBL_32B ? "32" :
	    "unknwon");
	val =
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXACTR(sc, chan));
	printf("  Activation      : 0x%08x\n", val);
	val &= MVXORE_XEXACTR_XESTATUS_MASK;
	printf("    XEstatus      : %s\n",
	    val == MVXORE_XEXACTR_XESTATUS_NA ? "Channel not active" :
	    val == MVXORE_XEXACTR_XESTATUS_ACT ? "Channel active" :
	    val == MVXORE_XEXACTR_XESTATUS_P ? "Channel paused" : "???");

	if (opmode == MVXORE_XEXCR_OM_XOR ||
	    opmode == MVXORE_XEXCR_OM_CRC32 ||
	    opmode == MVXORE_XEXCR_OM_DMA) {
		printf("  NextDescPtr     : 0x%08x\n",
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXNDPR(sc, chan)));
		printf("  CurrentDescPtr  : 0x%08x\n",
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXCDPR(chan)));
	}
	printf("  ByteCnt         : 0x%08x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVXORE_XEXBCR(chan)));

	if (opmode == MVXORE_XEXCR_OM_ECC ||
	    opmode == MVXORE_XEXCR_OM_MEMINIT) {
		printf("  DstPtr          : 0x%08x\n",
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXDPR(sc, chan)));
		printf("  BlockSize       : 0x%08x\n",
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MVXORE_XEXBSR(sc, chan)));

		if (opmode == MVXORE_XEXCR_OM_ECC) {
			val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XETMCR);
			if (val & MVXORE_XETMCR_TIMEREN) {
				val >>= MVXORE_XETMCR_SECTIONSIZECTRL_SHIFT;
				val &= MVXORE_XETMCR_SECTIONSIZECTRL_MASK;
				printf("  SectionSizeCtrl : 0x%08x\n", 2 ^ val);
				printf("  TimerInitVal    : 0x%08x\n",
				    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    MVXORE_XETMIVR));
				printf("  TimerCrntVal    : 0x%08x\n",
				    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    MVXORE_XETMCVR));
			}
		} else	/* MVXORE_XEXCR_OM_MEMINIT */
			printf("  InitVal         : 0x%08x%08x\n",
			    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XEIVRH),
			    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVXORE_XEIVRL));
	}
}

static void
gtidmac_dump_xoredesc(struct gtidmac_softc *sc, struct gtidmac_dma_desc *dd,
		      uint32_t mode, int post)
{
	struct mvxore_desc *desc;
	int i, j;
	char buf[256];

	printf("XORE Descriptor\n");

	mode &= MVXORE_XEXCR_OM_MASK;

	i = 0;
	while (1 /*CONSTCOND*/) {
		if (post)
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
			    dd->dd_index * sizeof(*desc), sizeof(*desc),
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		desc = dd->dd_xore_vaddr;

		printf("%d (0x%lx)\n", i, dd->dd_paddr);

		snprintb(buf, sizeof(buf), "\177\020b\037Own\0b\036Success\0",
		    desc->stat);
		printf("  Status                  : 0x%s\n", buf);
		if (desc->cmd & MVXORE_DESC_CMD_CRCLAST && post)
			printf("  CRC-32 Result           : 0x%08x\n",
			    desc->result);
		snprintb(buf, sizeof(buf),
		    "\177\020b\037EODIntEn\0b\036CRCLast\0"
		    "b\007Src7Cmd\0b\006Src6Cmd\0b\005Src5Cmd\0b\004Src4Cmd\0"
		    "b\003Src3Cmd\0b\002Src2Cmd\0b\001Src1Cmd\0b\000Src0Cmd\0",
		    desc->cmd);
		printf("  Command                 : 0x%s\n", buf);
		printf("  Next Descriptor Address : 0x%08x\n", desc->nextda);
		printf("  Byte Count              :   0x%06x\n", desc->bcnt);
		printf("  Destination Address     : 0x%08x\n", desc->dstaddr);
		if (mode == MVXORE_XEXCR_OM_XOR) {
			for (j = 0; j < MVXORE_NSRC; j++)
				if (desc->cmd & MVXORE_DESC_CMD_SRCCMD(j))
					printf("  Source Address#%d        :"
					    " 0x%08x\n", j, desc->srcaddr[j]);
		} else
			printf("  Source Address          : 0x%08x\n",
			    desc->srcaddr[0]);

		if (desc->nextda == (uint32_t)NULL)
			break;

		if (!post)
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
			    dd->dd_index * sizeof(*desc), sizeof(*desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		i++;
		dd = SLIST_NEXT(dd, dd_next);
	}
	if (!post)
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_xore,
		    dd->dd_index * sizeof(*desc), sizeof(*desc),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}
#endif
