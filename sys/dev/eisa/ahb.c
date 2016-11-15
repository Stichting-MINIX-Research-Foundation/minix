/*	$NetBSD: ahb.c,v 1.62 2014/10/18 08:33:27 snj Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ahb.c,v 1.62 2014/10/18 08:33:27 snj Exp $");

#include "opt_ddb.h"

#undef	AHBDEBUG

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

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>
#include <dev/eisa/ahbreg.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1742.c)")
#endif /* ! DDB */

#define AHB_ECB_MAX	32	/* store up to 32 ECBs at one time */
#define	ECB_HASH_SIZE	32	/* hash table size for phystokv */
#define	ECB_HASH_SHIFT	9
#define ECB_HASH(x)	((((long)(x))>>ECB_HASH_SHIFT) & (ECB_HASH_SIZE - 1))

#define AHB_MAXXFER	((AHB_NSEG - 1) << PGSHIFT)

struct ahb_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	void *sc_ih;

	bus_dmamap_t sc_dmamap_ecb;	/* maps the ecbs */
	struct ahb_ecb *sc_ecbs;	/* all our ecbs */

	struct ahb_ecb *sc_ecbhash[ECB_HASH_SIZE];
	TAILQ_HEAD(, ahb_ecb) sc_free_ecb;
	struct ahb_ecb *sc_immed_ecb;	/* an outstanding immediete command */
	int sc_numecbs;

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;
};

/*
 * Offset of an ECB from the beginning of the ECB DMA mapping.
 */
#define	AHB_ECB_OFF(e)	(((u_long)(e)) - ((u_long)&sc->sc_ecbs[0]))

struct ahb_probe_data {
	int sc_irq;
	int sc_scsi_dev;
};

static void	ahb_send_mbox(struct ahb_softc *, int, struct ahb_ecb *);
static void	ahb_send_immed(struct ahb_softc *, u_int32_t, struct ahb_ecb *);
static int	ahbintr(void *);
static void	ahb_free_ecb(struct ahb_softc *, struct ahb_ecb *);
static struct	ahb_ecb *ahb_get_ecb(struct ahb_softc *);
static struct	ahb_ecb *ahb_ecb_phys_kv(struct ahb_softc *, physaddr);
static void	ahb_done(struct ahb_softc *, struct ahb_ecb *);
static int	ahb_find(bus_space_tag_t, bus_space_handle_t,
		    struct ahb_probe_data *);
static int	ahb_init(struct ahb_softc *);
static void	ahbminphys(struct buf *);
static void	ahb_scsipi_request(struct scsipi_channel *,
		    scsipi_adapter_req_t, void *);
static int	ahb_poll(struct ahb_softc *, struct scsipi_xfer *, int);
static void	ahb_timeout(void *);
static int	ahb_create_ecbs(struct ahb_softc *, struct ahb_ecb *, int);

static int	ahb_init_ecb(struct ahb_softc *, struct ahb_ecb *);

static int	ahbmatch(device_t, cfdata_t, void *);
static void	ahbattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ahb, sizeof(struct ahb_softc),
    ahbmatch, ahbattach, NULL, NULL);

#define	AHB_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
static int
ahbmatch(device_t parent, cfdata_t match,
    void *aux)
{
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	int rv;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "ADP0000") &&
	    strcmp(ea->ea_idstring, "ADP0001") &&
	    strcmp(ea->ea_idstring, "ADP0002") &&
	    strcmp(ea->ea_idstring, "ADP0400"))
		return (0);

	if (bus_space_map(iot,
	    EISA_SLOT_ADDR(ea->ea_slot) + AHB_EISA_SLOT_OFFSET, AHB_EISA_IOSIZE,
	    0, &ioh))
		return (0);

	rv = !ahb_find(iot, ioh, NULL);

	bus_space_unmap(iot, ioh, AHB_EISA_IOSIZE);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
static void
ahbattach(device_t parent, device_t self, void *aux)
{
	struct eisa_attach_args *ea = aux;
	struct ahb_softc *sc = device_private(self);
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;
	struct ahb_probe_data apd;
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;
	char intrbuf[EISA_INTRSTR_LEN];

	sc->sc_dev = self;

	if (!strcmp(ea->ea_idstring, "ADP0000"))
		model = EISA_PRODUCT_ADP0000;
	else if (!strcmp(ea->ea_idstring, "ADP0001"))
		model = EISA_PRODUCT_ADP0001;
	else if (!strcmp(ea->ea_idstring, "ADP0002"))
		model = EISA_PRODUCT_ADP0002;
	else if (!strcmp(ea->ea_idstring, "ADP0400"))
		model = EISA_PRODUCT_ADP0400;
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (bus_space_map(iot,
	    EISA_SLOT_ADDR(ea->ea_slot) + AHB_EISA_SLOT_OFFSET, AHB_EISA_IOSIZE,
	    0, &ioh))
		panic("ahbattach: could not map I/O addresses");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ea->ea_dmat;
	if (ahb_find(iot, ioh, &apd))
		panic("ahbattach: ahb_find failed!");

	TAILQ_INIT(&sc->sc_free_ecb);

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* adapt_openings initialized below */
	adapt->adapt_max_periph = 4;		/* XXX arbitrary? */
	adapt->adapt_request = ahb_scsipi_request;
	adapt->adapt_minphys = ahbminphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = apd.sc_scsi_dev;

	if (ahb_init(sc) != 0) {
		/* Error during initialization! */
		return;
	}

	if (eisa_intr_map(ec, apd.sc_irq, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt (%d)\n",
		    apd.sc_irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    ahbintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	if (intrstr != NULL)
		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n",
		    intrstr);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_channel, scsiprint);
}

/*
 * Function to send a command out through a mailbox
 */
static void
ahb_send_mbox(struct ahb_softc *sc, int opcode, struct ahb_ecb *ecb)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wait = 300;	/* 1ms should be enough */

	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", device_xname(sc->sc_dev));
		Debugger();
	}

	/*
	 * don't know if this will work.
	 * XXX WHAT DOES THIS COMMENT MEAN?!  --thorpej
	 */
	bus_space_write_4(iot, ioh, MBOXOUT0,
	    sc->sc_dmamap_ecb->dm_segs[0].ds_addr + AHB_ECB_OFF(ecb));
	bus_space_write_1(iot, ioh, ATTN, opcode |
		ecb->xs->xs_periph->periph_target);

	if ((ecb->xs->xs_control & XS_CTL_POLL) == 0)
		callout_reset(&ecb->xs->xs_callout,
		    mstohz(ecb->timeout), ahb_timeout, ecb);
}

/*
 * Function to  send an immediate type command to the adapter
 */
static void
ahb_send_immed(struct ahb_softc *sc, u_int32_t cmd, struct ahb_ecb *ecb)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wait = 100;	/* 1 ms enough? */

	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", device_xname(sc->sc_dev));
		Debugger();
	}

	bus_space_write_4(iot, ioh, MBOXOUT0, cmd);	/* don't know this will work */
	bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_SET_HOST_READY);
	bus_space_write_1(iot, ioh, ATTN, OP_IMMED |
		ecb->xs->xs_periph->periph_target);

	if ((ecb->xs->xs_control & XS_CTL_POLL) == 0)
		callout_reset(&ecb->xs->xs_callout,
		    mstohz(ecb->timeout), ahb_timeout, ecb);
}

/*
 * Catch an interrupt from the adaptor
 */
static int
ahbintr(void *arg)
{
	struct ahb_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ahb_ecb *ecb;
	u_char ahbstat;
	u_int32_t mboxval;

#ifdef	AHBDEBUG
	printf("%s: ahbintr ", device_xname(sc->sc_dev));
#endif /* AHBDEBUG */

	if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) == 0)
		return 0;

	for (;;) {
		/*
		 * First get all the information and then
		 * acknowledge the interrupt
		 */
		ahbstat = bus_space_read_1(iot, ioh, G2INTST);
		mboxval = bus_space_read_4(iot, ioh, MBOXIN0);
		bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);

#ifdef	AHBDEBUG
		printf("status = 0x%x ", ahbstat);
#endif /* AHBDEBUG */

		/*
		 * Process the completed operation
		 */
		switch (ahbstat & G2INTST_INT_STAT) {
		case AHB_ECB_OK:
		case AHB_ECB_RECOVERED:
		case AHB_ECB_ERR:
			ecb = ahb_ecb_phys_kv(sc, mboxval);
			if (!ecb) {
				aprint_error_dev(sc->sc_dev, "BAD ECB RETURNED!\n");
				goto next;	/* whatever it was, it'll timeout */
			}
			break;

		case AHB_IMMED_ERR:
			ecb = sc->sc_immed_ecb;
			sc->sc_immed_ecb = 0;
			ecb->flags |= ECB_IMMED_FAIL;
			break;

		case AHB_IMMED_OK:
			ecb = sc->sc_immed_ecb;
			sc->sc_immed_ecb = 0;
			break;

		default:
			aprint_error_dev(sc->sc_dev, "unexpected interrupt %x\n",
			    ahbstat);
			goto next;
		}

		callout_stop(&ecb->xs->xs_callout);
		ahb_done(sc, ecb);

	next:
		if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) == 0)
			return 1;
	}
}

static inline void
ahb_reset_ecb(struct ahb_softc *sc, struct ahb_ecb *ecb)
{

	ecb->flags = 0;
}

/*
 * A ecb (and hence a mbx-out is put onto the
 * free list.
 */
static void
ahb_free_ecb(struct ahb_softc *sc, struct ahb_ecb *ecb)
{
	int s;

	s = splbio();
	ahb_reset_ecb(sc, ecb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ecb, ecb, chain);
	splx(s);
}

/*
 * Create a set of ecbs and add them to the free list.
 */
static int
ahb_init_ecb(struct ahb_softc *sc, struct ahb_ecb *ecb)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	int hashnum, error;

	/*
	 * Create the DMA map for this ECB.
	 */
	error = bus_dmamap_create(dmat, AHB_MAXXFER, AHB_NSEG, AHB_MAXXFER,
	    0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &ecb->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't create ecb dmamap_xfer\n");
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ecb->hashkey = sc->sc_dmamap_ecb->dm_segs[0].ds_addr +
	    AHB_ECB_OFF(ecb);
	hashnum = ECB_HASH(ecb->hashkey);
	ecb->nexthash = sc->sc_ecbhash[hashnum];
	sc->sc_ecbhash[hashnum] = ecb;
	ahb_reset_ecb(sc, ecb);
	return (0);
}

static int
ahb_create_ecbs(struct ahb_softc *sc, struct ahb_ecb *ecbstore, int count)
{
	struct ahb_ecb *ecb;
	int i, error;

	memset(ecbstore, 0, sizeof(struct ahb_ecb) * count);
	for (i = 0; i < count; i++) {
		ecb = &ecbstore[i];
		if ((error = ahb_init_ecb(sc, ecb)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to initialize ecb, error = %d\n",
			    error);
			goto out;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_ecb, ecb, chain);
	}
 out:
	return (i);
}

/*
 * Get a free ecb
 *
 * If there are none, see if we can allocate a new one. If so, put it in the
 * hash table too otherwise either return an error or sleep.
 */
static struct ahb_ecb *
ahb_get_ecb(struct ahb_softc *sc)
{
	struct ahb_ecb *ecb;
	int s;

	s = splbio();
	ecb = TAILQ_FIRST(&sc->sc_free_ecb);
	if (ecb != NULL) {
		TAILQ_REMOVE(&sc->sc_free_ecb, ecb, chain);
		ecb->flags |= ECB_ALLOC;
	}
	splx(s);
	return (ecb);
}

/*
 * given a physical address, find the ecb that it corresponds to.
 */
static struct ahb_ecb *
ahb_ecb_phys_kv(struct ahb_softc *sc, physaddr ecb_phys)
{
	int hashnum = ECB_HASH(ecb_phys);
	struct ahb_ecb *ecb = sc->sc_ecbhash[hashnum];

	while (ecb) {
		if (ecb->hashkey == ecb_phys)
			break;
		ecb = ecb->nexthash;
	}
	return ecb;
}

/*
 * We have a ecb which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
static void
ahb_done(struct ahb_softc *sc, struct ahb_ecb *ecb)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct scsi_sense_data *s1, *s2;
	struct scsipi_xfer *xs = ecb->xs;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("ahb_done\n"));

	bus_dmamap_sync(dmat, sc->sc_dmamap_ecb,
	    AHB_ECB_OFF(ecb), sizeof(struct ahb_ecb),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ecb->dmamap_xfer, 0,
		    ecb->dmamap_xfer->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ecb->dmamap_xfer);
	}

	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((ecb->flags & ECB_ALLOC) == 0) {
		aprint_error_dev(sc->sc_dev, "exiting ecb not allocated!\n");
		Debugger();
	}
	if (ecb->flags & ECB_IMMED) {
		if (ecb->flags & ECB_IMMED_FAIL)
			xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
	if (xs->error == XS_NOERROR) {
		if (ecb->ecb_status.host_stat != HS_OK) {
			switch (ecb->ecb_status.host_stat) {
			case HS_TIMED_OUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    device_xname(sc->sc_dev), ecb->ecb_status.host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (ecb->ecb_status.target_stat != SCSI_OK) {
			switch (ecb->ecb_status.target_stat) {
			case SCSI_CHECK:
				s1 = &ecb->ecb_sense;
				s2 = &xs->sense.scsi_sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    device_xname(sc->sc_dev), ecb->ecb_status.target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
done:
	ahb_free_ecb(sc, ecb);
	scsipi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
static int
ahb_find(bus_space_tag_t iot, bus_space_handle_t ioh, struct ahb_probe_data *sc)
{
	u_char intdef;
	int i, irq, busid;
	int wait = 1000;	/* 1 sec enough? */

	bus_space_write_1(iot, ioh, PORTADDR, PORTADDR_ENHANCED);

#define	NO_NO 1
#ifdef NO_NO
	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */
	bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_HARD_RESET);
	delay(1000);
	bus_space_write_1(iot, ioh, G2CNTRL, 0);
	delay(10000);
	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_BUSY) == 0)
			break;
		delay(1000);
	}
	if (!wait) {
#ifdef	AHBDEBUG
		printf("ahb_find: No answer from aha1742 board\n");
#endif /* AHBDEBUG */
		return ENXIO;
	}
	i = bus_space_read_1(iot, ioh, MBOXIN0);
	if (i) {
		printf("self test failed, val = 0x%x\n", i);
		return EIO;
	}

	/* Set it again, just to be sure. */
	bus_space_write_1(iot, ioh, PORTADDR, PORTADDR_ENHANCED);
#endif

	while (bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) {
		printf(".");
		bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		delay(10000);
	}

	intdef = bus_space_read_1(iot, ioh, INTDEF);
	switch (intdef & 0x07) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("illegal int setting %x\n", intdef);
		return EIO;
	}

	bus_space_write_1(iot, ioh, INTDEF, (intdef | INTEN));	/* make sure we can interrupt */

	/* who are we on the scsi bus? */
	busid = (bus_space_read_1(iot, ioh, SCSIDEF) & HSCSIID);

	/* if we want to return data, do so now */
	if (sc) {
		sc->sc_irq = irq;
		sc->sc_scsi_dev = busid;
	}

	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

static int
ahb_init(struct ahb_softc *sc)
{
	bus_dma_segment_t seg;
	int i, error, rseg;

#define	ECBSIZE		(AHB_ECB_MAX * sizeof(struct ahb_ecb))

	/*
	 * Allocate the ECBs.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, ECBSIZE,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate ecbs, error = %d\n",
		    error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    ECBSIZE, (void **)&sc->sc_ecbs,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map ecbs, error = %d\n",
		    error);
		return (error);
	}

	/*
	 * Create and load the DMA map used for the ecbs.
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat, ECBSIZE,
	    1, ECBSIZE, 0, BUS_DMA_NOWAIT, &sc->sc_dmamap_ecb)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create ecb DMA map, error = %d\n",
		    error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_ecb,
	    sc->sc_ecbs, ECBSIZE, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load ecb DMA map, error = %d\n",
		    error);
		return (error);
	}

#undef ECBSIZE

	/*
	 * Initialize the ecbs.
	 */
	i = ahb_create_ecbs(sc, sc->sc_ecbs, AHB_ECB_MAX);
	if (i == 0) {
		aprint_error_dev(sc->sc_dev, "unable to create ecbs\n");
		return (ENOMEM);
	} else if (i != AHB_ECB_MAX) {
		printf("%s: WARNING: only %d of %d ecbs created\n",
		    device_xname(sc->sc_dev), i, AHB_ECB_MAX);
	}

	sc->sc_adapter.adapt_openings = i;

	return (0);
}

static void
ahbminphys(struct buf *bp)
{

	if (bp->b_bcount > AHB_MAXXFER)
		bp->b_bcount = AHB_MAXXFER;
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
static void
ahb_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct ahb_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct ahb_ecb *ecb;
	int error, seg, flags, s;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		SC_DEBUG(periph, SCSIPI_DB2, ("ahb_scsipi_request\n"));

		/* Get an ECB to use. */
		ecb = ahb_get_ecb(sc);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen as we track the resources
		 * in the mid-layer.
		 */
		if (ecb == NULL) {
			scsipi_printaddr(periph);
			printf("unable to allocate ecb\n");
			panic("ahb_scsipi_request");
		}
#endif

		ecb->xs = xs;
		ecb->timeout = xs->timeout;

		/*
		 * If it's a reset, we need to do an 'immediate'
		 * command, and store its ecb for later
		 * if there is already an immediate waiting,
		 * then WE must wait
		 */
		if (flags & XS_CTL_RESET) {
			ecb->flags |= ECB_IMMED;
			if (sc->sc_immed_ecb) {
				ahb_free_ecb(sc, ecb);
				xs->error = XS_BUSY;
				scsipi_done(xs);
				return;
			}
			sc->sc_immed_ecb = ecb;

			s = splbio();
			ahb_send_immed(sc, AHB_TARG_RESET, ecb);
			splx(s);

			if ((flags & XS_CTL_POLL) == 0)
				return;

			/*
			 * If we can't use interrupts, poll on completion
			 */
			if (ahb_poll(sc, xs, ecb->timeout))
				ahb_timeout(ecb);
			return;
		}

		/*
		 * Put all the arguments for the xfer in the ecb
		 */
		if (xs->cmdlen > sizeof(ecb->scsi_cmd)) {
			aprint_error_dev(sc->sc_dev, "cmdlen %d too large for ECB\n",
			    xs->cmdlen);
			xs->error = XS_DRIVER_STUFFUP;
			goto out_bad;
		}
		ecb->opcode = ECB_SCSI_OP;
		ecb->opt1 = ECB_SES /*| ECB_DSB*/ | ECB_ARS;
		ecb->opt2 = periph->periph_lun | ECB_NRB;
		memcpy(&ecb->scsi_cmd, xs->cmd,
		    ecb->scsi_cmd_length = xs->cmdlen);
		ecb->sense_ptr = sc->sc_dmamap_ecb->dm_segs[0].ds_addr +
		    AHB_ECB_OFF(ecb) + offsetof(struct ahb_ecb, ecb_sense);
		ecb->req_sense_length = sizeof(ecb->ecb_sense);
		ecb->status = sc->sc_dmamap_ecb->dm_segs[0].ds_addr +
		    AHB_ECB_OFF(ecb) + offsetof(struct ahb_ecb, ecb_status);
		ecb->ecb_status.host_stat = 0x00;
		ecb->ecb_status.target_stat = 0x00;

		if (xs->datalen) {
			/*
			 * Map the DMA transfer.
			 */
#ifdef TFS
			if (flags & XS_CTL_DATA_UIO) {
				error = bus_dmamap_load_uio(sc->sc_dmat,
				    ecb->dmamap_xfer, (struct uio *)xs->data,
				    BUS_DMA_NOWAIT);
			} else
#endif /* TFS */
			{
				error = bus_dmamap_load(sc->sc_dmat,
				    ecb->dmamap_xfer, xs->data, xs->datalen,
				    NULL, BUS_DMA_NOWAIT);
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
				aprint_error_dev(sc->sc_dev, "error %d loading DMA map\n",
				    error);
 out_bad:
				ahb_free_ecb(sc, ecb);
				scsipi_done(xs);
				return;
			}

			bus_dmamap_sync(dmat, ecb->dmamap_xfer, 0,
			    ecb->dmamap_xfer->dm_mapsize,
			    (flags & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);

			/*
			 * Load the hardware scatter/gather map with the
			 * contents of the DMA map.
			 */
			for (seg = 0; seg < ecb->dmamap_xfer->dm_nsegs; seg++) {
				ecb->ahb_dma[seg].seg_addr =
				    ecb->dmamap_xfer->dm_segs[seg].ds_addr;
				ecb->ahb_dma[seg].seg_len =
				    ecb->dmamap_xfer->dm_segs[seg].ds_len;
			}

			ecb->data_addr = sc->sc_dmamap_ecb->dm_segs[0].ds_addr +
			    AHB_ECB_OFF(ecb) +
			    offsetof(struct ahb_ecb, ahb_dma);
			ecb->data_length = ecb->dmamap_xfer->dm_nsegs *
			    sizeof(struct ahb_dma_seg);
			ecb->opt1 |= ECB_S_G;
		} else {	/* No data xfer, use non S/G values */
			ecb->data_addr = (physaddr)0;
			ecb->data_length = 0;
		}
		ecb->link_addr = (physaddr)0;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ecb,
		    AHB_ECB_OFF(ecb), sizeof(struct ahb_ecb),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		s = splbio();
		ahb_send_mbox(sc, OP_START_ECB, ecb);
		splx(s);

		if ((flags & XS_CTL_POLL) == 0)
			return;

		/*
		 * If we can't use interrupts, poll on completion
		 */
		if (ahb_poll(sc, xs, ecb->timeout)) {
			ahb_timeout(ecb);
			if (ahb_poll(sc, xs, ecb->timeout))
				ahb_timeout(ecb);
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
 * Function to poll for command completion when in poll mode
 */
static int
ahb_poll(struct ahb_softc *sc, struct scsipi_xfer *xs, int count)
{				/* in msec  */
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND)
			ahbintr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

static void
ahb_timeout(void *arg)
{
	struct ahb_ecb *ecb = arg;
	struct scsipi_xfer *xs = ecb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct ahb_softc *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

	if (ecb->flags & ECB_IMMED) {
		printf("\n");
		ecb->flags |= ECB_IMMED_FAIL;
		/* XXX Must reset! */
	} else

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ecb->flags & ECB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ecb->xs->error = XS_TIMEOUT;
		ecb->timeout = AHB_ABORT_TIMEOUT;
		ecb->flags |= ECB_ABORT;
		ahb_send_mbox(sc, OP_ABORT_ECB, ecb);
	}

	splx(s);
}
