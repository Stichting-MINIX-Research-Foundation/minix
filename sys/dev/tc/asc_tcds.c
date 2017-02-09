/* $NetBSD: asc_tcds.c,v 1.25 2010/11/13 13:52:11 uebayasi Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: asc_tcds.c,v 1.25 2010/11/13 13:52:11 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <sys/bus.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/tcdsreg.h>
#include <dev/tc/tcdsvar.h>

struct asc_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */
	bus_space_tag_t sc_bst;			/* bus space tag */
	bus_space_handle_t sc_scsi_bsh;		/* ASC register handle */
	bus_dma_tag_t sc_dmat;			/* bus DMA tag */
	bus_dmamap_t sc_dmamap;			/* bus dmamap */
	uint8_t **sc_dmaaddr;
	size_t *sc_dmalen;
	size_t sc_dmasize;
	unsigned sc_flags;
#define	ASC_ISPULLUP		0x01
#define	ASC_DMAACTIVE		0x02
#define	ASC_MAPLOADED		0x04
	struct tcds_slotconfig *sc_tcds;	/* DMA/slot info lives here */
};

static int  asc_tcds_match(device_t, cfdata_t, void *);
static void asc_tcds_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(asc_tcds, sizeof(struct asc_softc),
    asc_tcds_match, asc_tcds_attach, NULL, NULL);

/*
 * Functions and the switch for the MI code.
 */
static uint8_t	asc_read_reg(struct ncr53c9x_softc *, int);
static void	asc_write_reg(struct ncr53c9x_softc *, int, uint8_t);
static int	tcds_dma_isintr(struct ncr53c9x_softc *);
static void	tcds_dma_reset(struct ncr53c9x_softc *);
static int	tcds_dma_intr(struct ncr53c9x_softc *);
static int	tcds_dma_setup(struct ncr53c9x_softc *, uint8_t **,
		    size_t *, int, size_t *);
static void	tcds_dma_go(struct ncr53c9x_softc *);
static void	tcds_dma_stop(struct ncr53c9x_softc *);
static int	tcds_dma_isactive(struct ncr53c9x_softc *);
static void	tcds_clear_latched_intr(struct ncr53c9x_softc *);

static struct ncr53c9x_glue asc_tcds_glue = {
	asc_read_reg,
	asc_write_reg,
	tcds_dma_isintr,
	tcds_dma_reset,
	tcds_dma_intr,
	tcds_dma_setup,
	tcds_dma_go,
	tcds_dma_stop,
	tcds_dma_isactive,
	tcds_clear_latched_intr,
};

static int
asc_tcds_match(device_t parent, cfdata_t cf, void *aux)
{

	/* We always exist. */
	return 1;
}

#define DMAMAX(a)	(PAGE_SIZE - ((a) & (PAGE_SIZE - 1)))

/*
 * Attach this instance, and then all the sub-devices
 */
static void
asc_tcds_attach(device_t parent, device_t self, void *aux)
{
	struct asc_softc *asc = device_private(self);
	struct ncr53c9x_softc *sc = &asc->sc_ncr53c9x;
	struct tcdsdev_attach_args *tcdsdev = aux;
	int error;

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_dev = self;
	sc->sc_glue = &asc_tcds_glue;

	asc->sc_bst = tcdsdev->tcdsda_bst;
	asc->sc_scsi_bsh = tcdsdev->tcdsda_bsh;
	asc->sc_tcds = tcdsdev->tcdsda_sc;

	/*
	 * The TCDS ASIC cannot DMA across 8k boundaries, and this
	 * driver is written such that each DMA segment gets a new
	 * call to tcds_dma_setup().  Thus, the DMA map only needs
	 * to support 8k transfers.
	 */
	asc->sc_dmat = tcdsdev->tcdsda_dmat;
	if ((error = bus_dmamap_create(asc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE,
	    PAGE_SIZE, BUS_DMA_NOWAIT, &asc->sc_dmamap)) < 0) {
		aprint_error(": failed to create DMA map, error = %d\n", error);
		return;
	}

	sc->sc_id = tcdsdev->tcdsda_id;
	sc->sc_freq = tcdsdev->tcdsda_freq;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	tcds_intr_establish(parent, tcdsdev->tcdsda_chip, ncr53c9x_intr, sc);

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * Set up static configuration info.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	sc->sc_cfg3 = NCRCFG3_CDB;
	if (sc->sc_freq > 25)
		sc->sc_cfg3 |= NCRF9XCFG3_FCLK;
	sc->sc_rev = tcdsdev->tcdsda_variant;
	if (tcdsdev->tcdsda_fast) {
		sc->sc_features |= NCR_F_FASTSCSI;
		sc->sc_cfg3_fscsi = NCRF9XCFG3_FSCSI;
	}

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = (1000 / sc->sc_freq) * tcdsdev->tcdsda_period / 4;

	sc->sc_maxxfer = 64 * 1024;

	/* Do the common parts of attachment. */
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = ncr53c9x_scsipi_request;
	ncr53c9x_attach(sc);
}

static void
tcds_dma_reset(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	/* TCDS SCSI disable/reset/enable. */
	tcds_scsi_reset(asc->sc_tcds);			/* XXX */

	if (asc->sc_flags & ASC_MAPLOADED)
		bus_dmamap_unload(asc->sc_dmat, asc->sc_dmamap);
	asc->sc_flags &= ~(ASC_DMAACTIVE|ASC_MAPLOADED);
}

/*
 * start a DMA transfer or keep it going
 */
int
tcds_dma_setup(struct ncr53c9x_softc *sc, uint8_t **addr, size_t *len,
    int ispullup, size_t *dmasize)
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	struct tcds_slotconfig *tcds = asc->sc_tcds;
	size_t size;
	uint32_t dic;

	NCR_DMA(("tcds_dma %d: start %d@%p,%s\n", tcds->sc_slot,
	    (int)*asc->sc_dmalen, *asc->sc_dmaaddr,
	    (ispullup) ? "IN" : "OUT"));

	/*
	 * the rules say we cannot transfer more than the limit
	 * of this DMA chip (64k) and we cannot cross a 8k boundary.
	 */
	size = min(*dmasize, DMAMAX((size_t)*addr));
	asc->sc_dmaaddr = addr;
	asc->sc_dmalen = len;
	asc->sc_flags = (ispullup) ? ASC_ISPULLUP : 0;
	*dmasize = asc->sc_dmasize = size;

	NCR_DMA(("dma_start: dmasize = %d\n", (int)size));

	if (size == 0)
		return 0;

	if (bus_dmamap_load(asc->sc_dmat, asc->sc_dmamap, *addr, size,
	    NULL, BUS_DMA_NOWAIT | (ispullup ? BUS_DMA_READ : BUS_DMA_WRITE))) {
		/*
		 * XXX Should return an error, here, but the upper-layer
		 * XXX doesn't check the return value!
		 */
		panic("%s: dmamap load failed", __func__);
	}

	/* synchronize dmamap contents with memory image */
	bus_dmamap_sync(asc->sc_dmat, asc->sc_dmamap, 0, size,
	    (ispullup) ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	/* load address, set/clear unaligned transfer and read/write bits. */
	bus_space_write_4(tcds->sc_bst, tcds->sc_bsh, tcds->sc_sda,
	    asc->sc_dmamap->dm_segs[0].ds_addr >> 2);
	dic = bus_space_read_4(tcds->sc_bst, tcds->sc_bsh, tcds->sc_dic);
	dic &= ~TCDS_DIC_ADDRMASK;
	dic |= asc->sc_dmamap->dm_segs[0].ds_addr & TCDS_DIC_ADDRMASK;
	if (ispullup)
		dic |= TCDS_DIC_WRITE;
	else
		dic &= ~TCDS_DIC_WRITE;
	bus_space_write_4(tcds->sc_bst, tcds->sc_bsh, tcds->sc_dic, dic);

	asc->sc_flags |= ASC_MAPLOADED;
	return 0;
}

static void
tcds_dma_go(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	/* mark unit as DMA-active */
	asc->sc_flags |= ASC_DMAACTIVE;

	/* start DMA */
	tcds_dma_enable(asc->sc_tcds, 1);
}

static void
tcds_dma_stop(struct ncr53c9x_softc *sc)
{
#if 0
	struct asc_softc *asc = (struct asc_softc *)sc;
#endif

	/*
	 * XXX STOP DMA HERE!
	 */
}

/*
 * Pseudo (chained) interrupt from the asc driver to kick the
 * current running DMA transfer. Called from ncr53c9x_intr()
 * for now.
 *
 * return 1 if it was a DMA continue.
 */
static int
tcds_dma_intr(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	struct tcds_slotconfig *tcds = asc->sc_tcds;
	int trans, resid;
	uint32_t tcl, tcm;
	uint32_t dud, dudmask, *addr;
	bus_addr_t pa;

	NCR_DMA(("tcds_dma %d: intr", tcds->sc_slot));

	if (tcds_scsi_iserr(tcds))
		return 0;

	/* This is an "assertion" :) */
	if ((asc->sc_flags & ASC_DMAACTIVE) == 0)
		panic("%s: DMA wasn't active", __func__);

	/* DMA has stopped */
	tcds_dma_enable(tcds, 0);
	asc->sc_flags &= ~ASC_DMAACTIVE;

	if (asc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		tcl = NCR_READ_REG(sc, NCR_TCL);
		tcm = NCR_READ_REG(sc, NCR_TCM);
		NCR_DMA(("dma_intr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    tcl | (tcm << 8), tcl, tcm));
		return 0;
	}

	resid = 0;
	if ((asc->sc_flags & ASC_ISPULLUP) == 0 &&
	    (resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("dma_intr: empty esp FIFO of %d ", resid));
		DELAY(1);
	}

	resid += (tcl = NCR_READ_REG(sc, NCR_TCL));
	resid += (tcm = NCR_READ_REG(sc, NCR_TCM)) << 8;

	trans = asc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		printf("tcds_dma %d: xfer (%d) > req (%d)\n",
		    tcds->sc_slot, trans, (int)asc->sc_dmasize);
		trans = asc->sc_dmasize;
	}

	NCR_DMA(("dma_intr: tcl=%d, tcm=%d; trans=%d, resid=%d\n",
	    tcl, tcm, trans, resid));

	*asc->sc_dmalen -= trans;
	*asc->sc_dmaaddr += trans;

	bus_dmamap_sync(asc->sc_dmat, asc->sc_dmamap,
	    0, asc->sc_dmamap->dm_mapsize,
	    (sc->sc_flags & ASC_ISPULLUP) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	/*
	 * Clean up unaligned DMAs into main memory.
	 */
	if (asc->sc_flags & ASC_ISPULLUP) {
		/* Handle unaligned starting address, length. */
		dud = bus_space_read_4(tcds->sc_bst,
		    tcds->sc_bsh, tcds->sc_dud0);
		if ((dud & TCDS_DUD0_VALIDBITS) != 0) {
			addr = (uint32_t *)((paddr_t)*asc->sc_dmaaddr & ~0x3);
			dudmask = 0;
			if (dud & TCDS_DUD0_VALID00)
				panic("%s: dud0 byte 0 valid", __func__);
			if (dud & TCDS_DUD0_VALID01)
				dudmask |= TCDS_DUD_BYTE01;
			if (dud & TCDS_DUD0_VALID10)
				dudmask |= TCDS_DUD_BYTE10;
#ifdef DIAGNOSTIC
			if (dud & TCDS_DUD0_VALID11)
				dudmask |= TCDS_DUD_BYTE11;
#endif
			NCR_DMA(("dud0 at %p dudmask 0x%x\n",
			    addr, dudmask));
			*addr = (*addr & ~dudmask) | (dud & dudmask);
		}
		dud = bus_space_read_4(tcds->sc_bst,
		    tcds->sc_bsh, tcds->sc_dud1);
		if ((dud & TCDS_DUD1_VALIDBITS) != 0) {
			pa = bus_space_read_4(tcds->sc_bst, tcds->sc_bsh,
			    tcds->sc_sda) << 2;
			dudmask = 0;
			if (dud & TCDS_DUD1_VALID00)
				dudmask |= TCDS_DUD_BYTE00;
			if (dud & TCDS_DUD1_VALID01)
				dudmask |= TCDS_DUD_BYTE01;
			if (dud & TCDS_DUD1_VALID10)
				dudmask |= TCDS_DUD_BYTE10;
#ifdef DIAGNOSTIC
			if (dud & TCDS_DUD1_VALID11)
				panic("%s: dud1 byte 3 valid", __func__);
#endif
			NCR_DMA(("dud1 at 0x%lx dudmask 0x%x\n",
			    pa, dudmask));
			/* XXX Fix TC_PHYS_TO_UNCACHED() */
#if defined(__alpha__)
			addr = (uint32_t *)ALPHA_PHYS_TO_K0SEG(pa);
#elif defined(__mips__)
			addr = (uint32_t *)MIPS_PHYS_TO_KSEG1(pa);
#elif defined(__vax__)
			addr = (uint32_t *)VAX_PHYS_TO_S0(pa);
#else
#error TURBOchannel only exists on DECs, folks...
#endif
			*addr = (*addr & ~dudmask) | (dud & dudmask);
		}
		/* XXX deal with saved residual byte? */
	}

	bus_dmamap_unload(asc->sc_dmat, asc->sc_dmamap);
	asc->sc_flags &= ~ASC_MAPLOADED;

	return 0;
}

/*
 * Glue functions.
 */
static uint8_t
asc_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	uint32_t v;

	v = bus_space_read_4(asc->sc_bst, asc->sc_scsi_bsh,
	    reg * sizeof(uint32_t));

	return v & 0xff;
}

static void
asc_write_reg(struct ncr53c9x_softc *sc, int reg, u_char val)
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	bus_space_write_4(asc->sc_bst, asc->sc_scsi_bsh,
	    reg * sizeof(uint32_t), val);
}

static int
tcds_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	int x;

	x = tcds_scsi_isintr(asc->sc_tcds, 1);

	/* XXX */
	return x;
}

static int
tcds_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	return (asc->sc_flags & ASC_DMAACTIVE) != 0;
}

static void
tcds_clear_latched_intr(struct ncr53c9x_softc *sc)
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	/* Clear the TCDS interrupt bit. */
	(void)tcds_scsi_isintr(asc->sc_tcds, 1);
}
