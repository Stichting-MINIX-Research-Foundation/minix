/*	$NetBSD: wds.c,v 1.76 2012/10/27 17:18:25 chs Exp $	*/

/*
 * XXX
 * aborts
 * resets
 */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1994, 1995 Julian Highfield.  All rights reserved.
 * Portions copyright (c) 1994, 1996, 1997
 *	Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Julian Highfield.
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

/*
 * This driver is for the WD7000 family of SCSI controllers:
 *   the WD7000-ASC, a bus-mastering DMA controller,
 *   the WD7000-FASST2, an -ASC with new firmware and scatter-gather,
 *   and the WD7000-ASE, which was custom manufactured for Apollo
 *      workstations and seems to include an -ASC as well as floppy
 *      and ESDI interfaces.
 *
 * Loosely based on Theo Deraadt's unfinished attempt.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wds.c,v 1.76 2012/10/27 17:18:25 chs Exp $");

#include "opt_ddb.h"

#undef WDSDIAG
#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/wdsreg.h>

#define	WDS_ISA_IOSIZE	8

#ifndef DDB
#define Debugger() panic("should call debugger here (wds.c)")
#endif /* ! DDB */

#define	WDS_MAXXFER	((WDS_NSEG - 1) << PGSHIFT)

#define WDS_MBX_SIZE	16

#define WDS_SCB_MAX	32
#define	SCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	SCB_HASH_SHIFT	9
#define	SCB_HASH(x)	((((long)(x))>>SCB_HASH_SHIFT) & (SCB_HASH_SIZE - 1))

#define	wds_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[WDS_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct wds_mbx {
	struct wds_mbx_out mbo[WDS_MBX_SIZE];
	struct wds_mbx_in mbi[WDS_MBX_SIZE];
	struct wds_mbx_out *cmbo;	/* Collection Mail Box out */
	struct wds_mbx_out *tmbo;	/* Target Mail Box out */
	struct wds_mbx_in *tmbi;	/* Target Mail Box in */
};

struct wds_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap_mbox;	/* maps the mailbox */
	void *sc_ih;

	struct wds_mbx *sc_mbx;
#define	wmbx	(sc->sc_mbx)
	struct wds_scb *sc_scbhash[SCB_HASH_SIZE];
	TAILQ_HEAD(, wds_scb) sc_free_scb, sc_waiting_scb;
	int sc_numscbs, sc_mbofull;

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;

	int sc_revision;
	int sc_maxsegs;
};

struct wds_probe_data {
#ifdef notyet
	int sc_irq, sc_drq;
#endif
	int sc_scsi_dev;
};

integrate void
	wds_wait(bus_space_tag_t, bus_space_handle_t, int, int, int);
int     wds_cmd(bus_space_tag_t, bus_space_handle_t, u_char *, int);
integrate void wds_finish_scbs(struct wds_softc *);
int     wdsintr(void *);
integrate void wds_reset_scb(struct wds_softc *, struct wds_scb *);
void    wds_free_scb(struct wds_softc *, struct wds_scb *);
integrate int wds_init_scb(struct wds_softc *, struct wds_scb *);
struct	wds_scb *wds_get_scb(struct wds_softc *);
struct	wds_scb *wds_scb_phys_kv(struct wds_softc *, u_long);
void	wds_queue_scb(struct wds_softc *, struct wds_scb *);
void	wds_collect_mbo(struct wds_softc *);
void	wds_start_scbs(struct wds_softc *);
void    wds_done(struct wds_softc *, struct wds_scb *, u_char);
int	wds_find(bus_space_tag_t, bus_space_handle_t, struct wds_probe_data *);
void	wds_attach(struct wds_softc *, struct wds_probe_data *);
void	wds_init(struct wds_softc *, int);
void	wds_inquire_setup_information(struct wds_softc *);
void    wdsminphys(struct buf *);
void	wds_scsipi_request(struct scsipi_channel *,
	    scsipi_adapter_req_t, void *);
int	wds_poll(struct wds_softc *, struct scsipi_xfer *, int);
int	wds_ipoll(struct wds_softc *, struct wds_scb *, int);
void	wds_timeout(void *);
int	wds_create_scbs(struct wds_softc *, void *, size_t);

int	wdsprobe(device_t, cfdata_t, void *);
void	wdsattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(wds, sizeof(struct wds_softc),
    wdsprobe, wdsattach, NULL, NULL);

#ifdef WDSDEBUG
int wds_debug = 0;
#endif

#define	WDS_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

integrate void
wds_wait(bus_space_tag_t iot, bus_space_handle_t ioh, int port, int mask, int val)
{

	while ((bus_space_read_1(iot, ioh, port) & mask) != val)
		;
}

/*
 * Write a command to the board's I/O ports.
 */
int
wds_cmd(bus_space_tag_t iot, bus_space_handle_t ioh, u_char *ibuf, int icnt)
{
	u_char c;

	wds_wait(iot, ioh, WDS_STAT, WDSS_RDY, WDSS_RDY);

	while (icnt--) {
		bus_space_write_1(iot, ioh, WDS_CMD, *ibuf++);
		wds_wait(iot, ioh, WDS_STAT, WDSS_RDY, WDSS_RDY);
		c = bus_space_read_1(iot, ioh, WDS_STAT);
		if (c & WDSS_REJ)
			return 1;
	}

	return 0;
}

/*
 * Check for the presence of a WD7000 SCSI controller.
 */
int
wdsprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct wds_probe_data wpd;
	int rv;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, WDS_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = wds_find(iot, ioh, &wpd);

	bus_space_unmap(iot, ioh, WDS_ISA_IOSIZE);

	if (rv) {
#ifdef notyet
		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != wpd.sc_irq)
			return (0);
		if (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ &&
		    ia->ia_drq[0].ir_drq != wpd.sc_drq)
			return (0);

		ia->ia_nirq = 1;
		ia->ia_irq[0].ir_irq = wpd.sc_irq;

		ia->ia_ndrq = 1;
		ia->ia_drq[0].ir_drq = wpd.sc_drq;
#else
		if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
			return (0);
		if (ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
			return (0);

		ia->ia_nirq = 1;
		ia->ia_ndrq = 1;
#endif
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = WDS_ISA_IOSIZE;

		ia->ia_niomem = 0;
	}
	return (rv);
}

/*
 * Attach all available units.
 */
void
wdsattach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct wds_softc *sc = device_private(self);
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct wds_probe_data wpd;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	sc->sc_dev = self;

	printf("\n");

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, WDS_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;
	if (!wds_find(iot, ioh, &wpd)) {
		aprint_error_dev(sc->sc_dev, "wds_find failed\n");
		return;
	}

	bus_space_write_1(iot, ioh, WDS_HCR, WDSH_DRQEN);
#ifdef notyet
	if (wpd.sc_drq != -1) {
		if ((error = isa_dmacascade(ic, wpd.sc_drq)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to cascade DRQ, error = %d\n", error);
			return;
		}
	}

	sc->sc_ih = isa_intr_establish(ic, wpd.sc_irq, IST_EDGE, IPL_BIO,
	    wdsintr, sc);
#else
	if ((error = isa_dmacascade(ic, ia->ia_drq[0].ir_drq)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to cascade DRQ, error = %d\n", error);
		return;
	}

	sc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_BIO, wdsintr, sc);
#endif
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return;
	}

	wds_attach(sc, &wpd);
}

void
wds_attach(struct wds_softc *sc, struct wds_probe_data *wpd)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;

	TAILQ_INIT(&sc->sc_free_scb);
	TAILQ_INIT(&sc->sc_waiting_scb);

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* adapt_openings initialized below */
	adapt->adapt_max_periph = 1;
	adapt->adapt_request = wds_scsipi_request;
	adapt->adapt_minphys = minphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = wpd->sc_scsi_dev;

	wds_init(sc, 0);
	wds_inquire_setup_information(sc);

	/* XXX add support for GROW */
	adapt->adapt_openings = sc->sc_numscbs;

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

integrate void
wds_finish_scbs(struct wds_softc *sc)
{
	struct wds_mbx_in *wmbi;
	struct wds_scb *scb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == WDS_MBI_FREE) {
		for (i = 0; i < WDS_MBX_SIZE; i++) {
			if (wmbi->stat != WDS_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    device_xname(sc->sc_dev));
				goto AGAIN;
			}
			wds_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef WDSDIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    device_xname(sc->sc_dev));
#endif
		return;
	}

AGAIN:
	do {
		scb = wds_scb_phys_kv(sc, phystol(wmbi->scb_addr));
		if (!scb) {
			printf("%s: bad mbi scb pointer; skipping\n",
			    device_xname(sc->sc_dev));
			goto next;
		}

#ifdef WDSDEBUG
		if (wds_debug) {
			u_char *cp = scb->cmd.xx;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = %p, ",
			    wmbi->stat, wmbi);
			printf("scb addr = %p\n", scb);
		}
#endif /* WDSDEBUG */

		callout_stop(&scb->xs->xs_callout);
		wds_done(sc, scb, wmbi->stat);

	next:
		wmbi->stat = WDS_MBI_FREE;
		wds_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != WDS_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Process an interrupt.
 */
int
wdsintr(void *arg)
{
	struct wds_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char c;

	/* Was it really an interrupt from the board? */
	if ((bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_IRQ) == 0)
		return 0;

	/* Get the interrupt status byte. */
	c = bus_space_read_1(iot, ioh, WDS_IRQSTAT) & WDSI_MASK;

	/* Acknowledge (which resets) the interrupt. */
	bus_space_write_1(iot, ioh, WDS_IRQACK, 0x00);

	switch (c) {
	case WDSI_MSVC:
		wds_finish_scbs(sc);
		break;

	case WDSI_MFREE:
		wds_start_scbs(sc);
		break;

	default:
		aprint_error_dev(sc->sc_dev, "unrecognized interrupt type %02x", c);
		break;
	}

	return 1;
}

integrate void
wds_reset_scb(struct wds_softc *sc, struct wds_scb *scb)
{

	scb->flags = 0;
}

/*
 * Free the command structure, the outgoing mailbox and the data buffer.
 */
void
wds_free_scb(struct wds_softc *sc, struct wds_scb *scb)
{
	int s;

	s = splbio();
	wds_reset_scb(sc, scb);
	TAILQ_INSERT_HEAD(&sc->sc_free_scb, scb, chain);
	splx(s);
}

integrate int
wds_init_scb(struct wds_softc *sc, struct wds_scb *scb)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	int hashnum, error;

	/*
	 * XXX Should we put a DIAGNOSTIC check for multiple
	 * XXX SCB inits here?
	 */

	memset(scb, 0, sizeof(struct wds_scb));

	/*
	 * Create DMA maps for this SCB.
	 */
	error = bus_dmamap_create(dmat, sizeof(struct wds_scb), 1,
	    sizeof(struct wds_scb), 0, BUS_DMA_NOWAIT, &scb->dmamap_self);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't create scb dmamap_self\n");
		return (error);
	}

	error = bus_dmamap_create(dmat, WDS_MAXXFER, WDS_NSEG, WDS_MAXXFER,
	    0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &scb->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't create scb dmamap_xfer\n");
		bus_dmamap_destroy(dmat, scb->dmamap_self);
		return (error);
	}

	/*
	 * Load the permanent DMA maps.
	 */
	error = bus_dmamap_load(dmat, scb->dmamap_self, scb,
	    sizeof(struct wds_scb), NULL, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't load scb dmamap_self\n");
		bus_dmamap_destroy(dmat, scb->dmamap_self);
		bus_dmamap_destroy(dmat, scb->dmamap_xfer);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	scb->hashkey = scb->dmamap_self->dm_segs[0].ds_addr;
	hashnum = SCB_HASH(scb->hashkey);
	scb->nexthash = sc->sc_scbhash[hashnum];
	sc->sc_scbhash[hashnum] = scb;
	wds_reset_scb(sc, scb);
	return (0);
}

/*
 * Create a set of scbs and add them to the free list.
 */
int
wds_create_scbs(struct wds_softc *sc, void *mem, size_t size)
{
	bus_dma_segment_t seg;
	struct wds_scb *scb;
	int rseg, error;

	if (sc->sc_numscbs >= WDS_SCB_MAX)
		return (0);

	if ((scb = mem) != NULL)
		goto have_mem;

	size = PAGE_SIZE;
	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &seg,
	    1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't allocate memory for scbs\n");
		return (error);
	}

	error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, size,
	    (void *)&scb, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't map memory for scbs\n");
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return (error);
	}

 have_mem:
	memset(scb, 0, size);
	while (size > sizeof(struct wds_scb) && sc->sc_numscbs < WDS_SCB_MAX) {
		error = wds_init_scb(sc, scb);
		if (error) {
			aprint_error_dev(sc->sc_dev, "can't initialize scb\n");
			return (error);
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_scb, scb, chain);
		scb = (struct wds_scb *)((char *)scb +
			ALIGN(sizeof(struct wds_scb)));
		size -= ALIGN(sizeof(struct wds_scb));
		sc->sc_numscbs++;
	}

	return (0);
}

/*
 * Get a free scb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct wds_scb *
wds_get_scb(struct wds_softc *sc)
{
	struct wds_scb *scb;
	int s;

	s = splbio();
	scb = TAILQ_FIRST(&sc->sc_free_scb);
	if (scb != NULL) {
		TAILQ_REMOVE(&sc->sc_free_scb, scb, chain);
		scb->flags |= SCB_ALLOC;
	}
	splx(s);
	return (scb);
}

struct wds_scb *
wds_scb_phys_kv(struct wds_softc *sc, u_long scb_phys)
{
	int hashnum = SCB_HASH(scb_phys);
	struct wds_scb *scb = sc->sc_scbhash[hashnum];

	while (scb) {
		if (scb->hashkey == scb_phys)
			break;
		/* XXX Check to see if it matches the sense command block. */
		if (scb->hashkey == (scb_phys - sizeof(struct wds_cmd)))
			break;
		scb = scb->nexthash;
	}
	return (scb);
}

/*
 * Queue a SCB to be sent to the controller, and send it if possible.
 */
void
wds_queue_scb(struct wds_softc *sc, struct wds_scb *scb)
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_scb, scb, chain);
	wds_start_scbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
wds_collect_mbo(struct wds_softc *sc)
{
	struct wds_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef WDSDIAG
	struct wds_scb *scb;
#endif

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != WDS_MBO_FREE)
			break;

#ifdef WDSDIAG
		scb = wds_scb_phys_kv(sc, phystol(wmbo->scb_addr));
		scb->flags &= ~SCB_SENDING;
#endif

		--sc->sc_mbofull;
		wds_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many SCBs as we have empty mailboxes for.
 */
void
wds_start_scbs(struct wds_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct wds_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct wds_scb *scb;
	u_char c;

	wmbo = wmbx->tmbo;

	while ((scb = sc->sc_waiting_scb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= WDS_MBX_SIZE) {
			wds_collect_mbo(sc);
			if (sc->sc_mbofull >= WDS_MBX_SIZE) {
				c = WDSC_IRQMFREE;
				wds_cmd(iot, ioh, &c, sizeof c);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_scb, scb, chain);
#ifdef WDSDIAG
		scb->flags |= SCB_SENDING;
#endif

		/* Link scb to mbo. */
		ltophys(scb->dmamap_self->dm_segs[0].ds_addr +
		    offsetof(struct wds_scb, cmd), wmbo->scb_addr);
		/* XXX What about aborts? */
		wmbo->cmd = WDS_MBO_START;

		/* Tell the card to poll immediately. */
		c = WDSC_MSTART(wmbo - wmbx->mbo);
		wds_cmd(sc->sc_iot, sc->sc_ioh, &c, sizeof c);

		if ((scb->flags & SCB_POLLED) == 0)
			callout_reset(&scb->xs->xs_callout,
			    mstohz(scb->timeout), wds_timeout, scb);

		++sc->sc_mbofull;
		wds_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * Process the result of a SCSI command.
 */
void
wds_done(struct wds_softc *sc, struct wds_scb *scb, u_char stat)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct scsipi_xfer *xs = scb->xs;

	/* XXXXX */

	/* Don't release the SCB if it was an internal command. */
	if (xs == 0) {
		scb->flags |= SCB_DONE;
		return;
	}

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, scb->dmamap_xfer, 0,
		    scb->dmamap_xfer->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, scb->dmamap_xfer);
	}
	if (xs->error == XS_NOERROR) {
		/* If all went well, or an error is acceptable. */
		if (stat == WDS_MBI_OK) {
			/* OK, set the result */
			xs->resid = 0;
		} else {
			/* Check the mailbox status. */
			switch (stat) {
			case WDS_MBI_OKERR:
				/*
				 * SCSI error recorded in scb,
				 * counts as WDS_MBI_OK
				 */
				switch (scb->cmd.venderr) {
				case 0x00:
					aprint_error_dev(sc->sc_dev, "Is this "
					    "an error?\n");
					/* Experiment. */
					xs->error = XS_DRIVER_STUFFUP;
					break;
				case 0x01:
#if 0
					aprint_error_dev(sc->sc_dev, "OK, see SCSI "
					    "error field.\n");
#endif
					if (scb->cmd.stat == SCSI_CHECK ||
					    scb->cmd.stat == SCSI_BUSY) {
						xs->status = scb->cmd.stat;
						xs->error = XS_BUSY;
					}
					break;
				case 0x40:
#if 0
					printf("%s: DMA underrun!\n",
					    device_xname(sc->sc_dev));
#endif
					/*
					 * Hits this if the target
					 * returns fewer that datalen
					 * bytes (eg my CD-ROM, which
					 * returns a short version
					 * string, or if DMA is
					 * turned off etc.
					 */
					xs->resid = 0;
					break;
				default:
					printf("%s: VENDOR ERROR "
					    "%02x, scsi %02x\n",
					    device_xname(sc->sc_dev),
					    scb->cmd.venderr,
					    scb->cmd.stat);
					/* Experiment. */
					xs->error = XS_DRIVER_STUFFUP;
					break;
				}
					break;
			case WDS_MBI_ETIME:
				/*
				 * The documentation isn't clear on
				 * what conditions might generate this,
				 * but selection timeouts are the only
				 * one I can think of.
				 */
				xs->error = XS_SELTIMEOUT;
				break;
			case WDS_MBI_ERESET:
			case WDS_MBI_ETARCMD:
			case WDS_MBI_ERESEL:
			case WDS_MBI_ESEL:
			case WDS_MBI_EABORT:
			case WDS_MBI_ESRESET:
			case WDS_MBI_EHRESET:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		}
	} /* XS_NOERROR */

	wds_free_scb(sc, scb);
	scsipi_done(xs);
}

int
wds_find(bus_space_tag_t iot, bus_space_handle_t ioh, struct wds_probe_data *sc)
{
	int i;

	/* XXXXX */

	/*
	 * Sending a command causes the CMDRDY bit to clear.
 	 */
	for (i = 5; i; i--) {
		if ((bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_RDY) != 0)
			break;
		delay(100);
	}
	if (!i)
		return 0;

	bus_space_write_1(iot, ioh, WDS_CMD, WDSC_NOOP);
	if ((bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_RDY) != 0)
		return 0;

	bus_space_write_1(iot, ioh, WDS_HCR, WDSH_SCSIRESET|WDSH_ASCRESET);
	delay(10000);
	bus_space_write_1(iot, ioh, WDS_HCR, 0x00);
	delay(500000);
	wds_wait(iot, ioh, WDS_STAT, WDSS_RDY, WDSS_RDY);
	if (bus_space_read_1(iot, ioh, WDS_IRQSTAT) != 1)
		if (bus_space_read_1(iot, ioh, WDS_IRQSTAT) != 7)
			return 0;

	for (i = 2000; i; i--) {
		if ((bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_RDY) != 0)
			break;
		delay(100);
	}
	if (!i)
		return 0;

	if (sc) {
#ifdef notyet
		sc->sc_irq = ...;
		sc->sc_drq = ...;
#endif
		/* XXX Can we do this better? */
		sc->sc_scsi_dev = 7;
	}

	return 1;
}

/*
 * Initialise the board and driver.
 */
void
wds_init(struct wds_softc *sc, int isreset)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_dma_segment_t seg;
	struct wds_setup init;
	u_char c;
	int i, rseg;

	if (isreset)
		goto doinit;

	/*
	 * Allocate the mailbox.
	 */
	if (bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(sc->sc_dmat, &seg, rseg, PAGE_SIZE,
	    (void **)&wmbx, BUS_DMA_NOWAIT|BUS_DMA_COHERENT))
		panic("wds_init: can't create or map mailbox");

	/*
	 * Since DMA memory allocation is always rounded up to a
	 * page size, create some scbs from the leftovers.
	 */
	if (wds_create_scbs(sc, ((char *)wmbx) +
	    ALIGN(sizeof(struct wds_mbx)),
	    PAGE_SIZE - ALIGN(sizeof(struct wds_mbx))))
		panic("wds_init: can't create scbs");

	/*
	 * Create and load the mailbox DMA map.
	 */
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct wds_mbx), 1,
	    sizeof(struct wds_mbx), 0, BUS_DMA_NOWAIT, &sc->sc_dmamap_mbox) ||
	    bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_mbox, wmbx,
	    sizeof(struct wds_mbx), NULL, BUS_DMA_NOWAIT))
		panic("wds_ionit: can't create or load mailbox DMA map");

 doinit:
	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < WDS_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = WDS_MBO_FREE;
		wmbx->mbi[i].stat = WDS_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	init.opcode = WDSC_INIT;
	init.scsi_id = sc->sc_channel.chan_id;
	init.buson_t = 48;
	init.busoff_t = 24;
	init.xx = 0;
	ltophys(sc->sc_dmamap_mbox->dm_segs[0].ds_addr, init.mbaddr);
	init.nomb = init.nimb = WDS_MBX_SIZE;
	wds_cmd(iot, ioh, (u_char *)&init, sizeof init);

	wds_wait(iot, ioh, WDS_STAT, WDSS_INIT, WDSS_INIT);

	c = WDSC_DISUNSOL;
	wds_cmd(iot, ioh, &c, sizeof c);
}

/*
 * Read the board's firmware revision information.
 */
void
wds_inquire_setup_information(struct wds_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct wds_scb *scb;
	u_char *j;
	int s;

	sc->sc_maxsegs = 1;

	scb = wds_get_scb(sc);
	if (scb == 0)
		panic("wds_inquire_setup_information: no scb available");

	scb->xs = NULL;
	scb->timeout = 40;

	memset(&scb->cmd, 0, sizeof scb->cmd);
	scb->cmd.write = 0x80;
	scb->cmd.opcode = WDSX_GETFIRMREV;

	/* Will poll card, await result. */
	bus_space_write_1(iot, ioh, WDS_HCR, WDSH_DRQEN);
	scb->flags |= SCB_POLLED;

	s = splbio();
	wds_queue_scb(sc, scb);
	splx(s);

	if (wds_ipoll(sc, scb, scb->timeout))
		goto out;

	/* Print the version number. */
	printf("%s: version %x.%02x ", device_xname(sc->sc_dev),
	    scb->cmd.targ, scb->cmd.scb[0]);
	sc->sc_revision = (scb->cmd.targ << 8) | scb->cmd.scb[0];
	/* Print out the version string. */
	j = 2 + &(scb->cmd.targ);
	while ((*j >= 32) && (*j < 128)) {
		printf("%c", *j);
		j++;
	}

	/*
	 * Determine if we can use scatter/gather.
	 */
	if (sc->sc_revision >= 0x800)
		sc->sc_maxsegs = WDS_NSEG;

out:
	printf("\n");

	/*
	 * Free up the resources used by this scb.
	 */
	wds_free_scb(sc, scb);
}

void
wdsminphys(struct buf *bp)
{

	if (bp->b_bcount > WDS_MAXXFER)
		bp->b_bcount = WDS_MAXXFER;
	minphys(bp);
}

/*
 * Send a SCSI command.
 */
void
wds_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct wds_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct wds_scb *scb;
	int error, seg, flags, s;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;

		if (xs->xs_control & XS_CTL_RESET) {
			/* XXX Fix me! */
			printf("%s: reset!\n", device_xname(sc->sc_dev));
			wds_init(sc, 1);
			scsipi_done(xs);
			return;
		}

		if (xs->xs_control & XS_CTL_DATA_UIO) {
			/* XXX Fix me! */
			/*
			 * Let's not worry about UIO. There isn't any code
			 * for the non-SG boards anyway!
			 */
			aprint_error_dev(sc->sc_dev, "UIO is untested and disabled!\n");
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			return;
		}

		flags = xs->xs_control;

		/* Get an SCB to use. */
		scb = wds_get_scb(sc);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen as we track the resources
		 * in the mid-layer.
		 */
		if (scb == NULL) {
			scsipi_printaddr(periph);
			printf("unable to allocate scb\n");
			panic("wds_scsipi_request");
		}
#endif

		scb->xs = xs;
		scb->timeout = xs->timeout;

		/* Zero out the command structure. */
		if (xs->cmdlen > sizeof(scb->cmd.scb)) {
			aprint_error_dev(sc->sc_dev, "cmdlen %d too large for SCB\n",
			    xs->cmdlen);
			xs->error = XS_DRIVER_STUFFUP;
			goto out_bad;
		}
		memset(&scb->cmd, 0, sizeof scb->cmd);
		memcpy(&scb->cmd.scb, xs->cmd, xs->cmdlen);

		/* Set up some of the command fields. */
		scb->cmd.targ = (periph->periph_target << 5) |
		    periph->periph_lun;

		/*
		 * NOTE: cmd.write may be OK as 0x40 (disable direction
		 * checking) on boards other than the WD-7000V-ASE. Need
		 * this for the ASE:
 		 */
		scb->cmd.write = (xs->xs_control & XS_CTL_DATA_IN) ?
		    0x80 : 0x00;

		if (xs->datalen) {
			seg = 0;
#ifdef TFS
			if (flags & XS_CTL_DATA_UIO) {
				error = bus_dmamap_load_uio(dmat,
				    scb->dmamap_xfer, (struct uio *)xs->data,
				    BUS_DMA_NOWAIT |
				    ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				     BUS_DMA_WRITE));
			} else
#endif /* TFS */
			{
				error = bus_dmamap_load(dmat,
				    scb->dmamap_xfer, xs->data, xs->datalen,
				    NULL, BUS_DMA_NOWAIT |
				    ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				     BUS_DMA_WRITE));
			}

			switch (error) {
			case 0:
				break;

			case ENOMEM:
			case EAGAIN:
				xs->error = XS_RESOURCE_SHORTAGE;
				goto out_bad;

			default:
				xs->error = XS_DRIVER_STUFFUP;
				aprint_error_dev(sc->sc_dev, "error %d loading DMA map\n", error);
 out_bad:
				wds_free_scb(sc, scb);
				scsipi_done(xs);
				return;
			}

			bus_dmamap_sync(dmat, scb->dmamap_xfer, 0,
			    scb->dmamap_xfer->dm_mapsize,
			    (flags & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);

			if (sc->sc_maxsegs > 1) {
				/*
				 * Load the hardware scatter/gather map with the
				 * contents of the DMA map.
				 */
				for (seg = 0;
				     seg < scb->dmamap_xfer->dm_nsegs; seg++) {
				ltophys(scb->dmamap_xfer->dm_segs[seg].ds_addr,
					    scb->scat_gath[seg].seg_addr);
				ltophys(scb->dmamap_xfer->dm_segs[seg].ds_len,
					    scb->scat_gath[seg].seg_len);
				}

				/*
				 * Set up for scatter/gather transfer.
				 */
				scb->cmd.opcode = WDSX_SCSISG;
				ltophys(scb->dmamap_self->dm_segs[0].ds_addr +
				    offsetof(struct wds_scb, scat_gath),
				    scb->cmd.data);
				ltophys(scb->dmamap_self->dm_nsegs *
				    sizeof(struct wds_scat_gath), scb->cmd.len);
			} else {
				/*
				 * This board is an ASC or an ASE, and the
				 * transfer has been mapped contig for us.
				 */
				scb->cmd.opcode = WDSX_SCSICMD;
				ltophys(scb->dmamap_xfer->dm_segs[0].ds_addr,
				    scb->cmd.data);
				ltophys(scb->dmamap_xfer->dm_segs[0].ds_len,
				    scb->cmd.len);
			}
		} else {
			scb->cmd.opcode = WDSX_SCSICMD;
			ltophys(0, scb->cmd.data);
			ltophys(0, scb->cmd.len);
		}

		scb->cmd.stat = 0x00;
		scb->cmd.venderr = 0x00;
		ltophys(0, scb->cmd.link);

		/* XXX Do we really want to do this? */
		if (flags & XS_CTL_POLL) {
			/* Will poll card, await result. */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    WDS_HCR, WDSH_DRQEN);
			scb->flags |= SCB_POLLED;
		} else {
			/*
			 * Will send command, let interrupt routine
			 * handle result.
			 */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, WDS_HCR,
			    WDSH_IRQEN | WDSH_DRQEN);
		}

		s = splbio();
		wds_queue_scb(sc, scb);
		splx(s);

		if ((flags & XS_CTL_POLL) == 0)
			return;

		if (wds_poll(sc, xs, scb->timeout)) {
			wds_timeout(scb);
			if (wds_poll(sc, xs, scb->timeout))
				wds_timeout(scb);
		}
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		/* XXX How do we do this? */
		return;
	}
}

/*
 * Poll a particular unit, looking for a particular scb
 */
int
wds_poll(struct wds_softc *sc, struct scsipi_xfer *xs, int count)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_IRQ)
			wdsintr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

/*
 * Poll a particular unit, looking for a particular scb
 */
int
wds_ipoll(struct wds_softc *sc, struct wds_scb *scb, int count)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, WDS_STAT) & WDSS_IRQ)
			wdsintr(sc);
		if (scb->flags & SCB_DONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

void
wds_timeout(void *arg)
{
	struct wds_scb *scb = arg;
	struct scsipi_xfer *xs = scb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct wds_softc *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

#ifdef WDSDIAG
	/*
	 * If The scb's mbx is not free, then the board has gone south?
	 */
	wds_collect_mbo(sc);
	if (scb->flags & SCB_SENDING) {
		aprint_error_dev(sc->sc_dev, "not taking commands!\n");
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (scb->flags & SCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		scb->xs->error = XS_TIMEOUT;
		scb->timeout = WDS_ABORT_TIMEOUT;
		scb->flags |= SCB_ABORT;
		wds_queue_scb(sc, scb);
	}

	splx(s);
}
