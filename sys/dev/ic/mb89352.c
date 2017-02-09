/*	$NetBSD: mb89352.c,v 1.54 2013/11/04 16:54:56 christos Exp $	*/
/*	NecBSD: mb89352.c,v 1.4 1998/03/14 07:31:20 kmatsuda Exp	*/

/*-
 * Copyright (c) 1996-1999,2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, Masaru Oki and Kouichi Matsuda.
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
 *
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
 */

/*
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/* TODO list:
 * 1) Get the DMA stuff working.
 * 2) Get the iov/uio stuff working. Is this a good thing ???
 * 3) Get the synch stuff working.
 * 4) Rewrite it to use malloc for the acb structs instead of static alloc.?
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mb89352.c,v 1.54 2013/11/04 16:54:56 christos Exp $");

#ifdef DDB
#define	integrate
#else
#define	integrate	inline static
#endif

/*
 * A few customizable items:
 */

/* Synchronous data transfers? */
#define SPC_USE_SYNCHRONOUS	0
#define SPC_SYNC_REQ_ACK_OFS 	8

/* Wide data transfers? */
#define	SPC_USE_WIDE		0
#define	SPC_MAX_WIDTH		0

/* Max attempts made to transmit a message */
#define SPC_MSG_MAX_ATTEMPT	3 /* Not used now XXX */

/*
 * Some spin loop parameters (essentially how long to wait some places)
 * The problem(?) is that sometimes we expect either to be able to transmit a
 * byte or to get a new one from the SCSI bus pretty soon.  In order to avoid
 * returning from the interrupt just to get yanked back for the next byte we
 * may spin in the interrupt routine waiting for this byte to come.  How long?
 * This is really (SCSI) device and processor dependent.  Tuneable, I guess.
 */
#define SPC_MSGIN_SPIN	1 	/* Will spinwait upto ?ms for a new msg byte */
#define SPC_MSGOUT_SPIN	1

/*
 * Include debug functions?  At the end of this file there are a bunch of
 * functions that will print out various information regarding queued SCSI
 * commands, driver state and chip contents.  You can call them from the
 * kernel debugger.  If you set SPC_DEBUG to 0 they are not included (the
 * kernel uses less memory) but you lose the debugging facilities.
 */
#if 0
#define SPC_DEBUG		1
#endif

#define	SPC_ABORT_TIMEOUT	2000	/* time to wait for abort */

/* threshold length for DMA transfer */
#define SPC_MIN_DMA_LEN	32

#ifdef luna68k	/* XXX old drives like DK312C in LUNAs require this */
#define NO_MANUAL_XFER
#endif
#ifdef x68k	/* XXX it seems x68k SPC SCSI hardware has some quirks */
#define NEED_DREQ_ON_HARDWARE_XFER
#define NO_MANUAL_XFER
#endif

/* End of customizable parameters */

/*
 * MB89352 SCSI Protocol Controller (SPC) routines.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/mb89352reg.h>
#include <dev/ic/mb89352var.h>

#ifndef DDB
#define	Debugger() panic("should call debugger here (mb89352.c)")
#endif /* ! DDB */

#if SPC_DEBUG
int spc_debug = 0x00; /* SPC_SHOWSTART|SPC_SHOWMISC|SPC_SHOWTRACE; */
#endif

void	spc_done(struct spc_softc *, struct spc_acb *);
void	spc_dequeue(struct spc_softc *, struct spc_acb *);
void	spc_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
    void *);
int	spc_poll(struct spc_softc *, struct scsipi_xfer *, int);
integrate void	spc_sched_msgout(struct spc_softc *, uint8_t);
integrate void	spc_setsync(struct spc_softc *, struct spc_tinfo *);
void	spc_select(struct spc_softc *, struct spc_acb *);
void	spc_timeout(void *);
void	spc_scsi_reset(struct spc_softc *);
void	spc_reset(struct spc_softc *);
void	spc_free_acb(struct spc_softc *, struct spc_acb *, int);
struct spc_acb* spc_get_acb(struct spc_softc *);
int	spc_reselect(struct spc_softc *, int);
void	spc_msgin(struct spc_softc *);
void	spc_abort(struct spc_softc *, struct spc_acb *);
void	spc_msgout(struct spc_softc *);
int	spc_dataout_pio(struct spc_softc *, uint8_t *, int);
int	spc_datain_pio(struct spc_softc *, uint8_t *, int);
#if SPC_DEBUG
void	spc_print_acb(struct spc_acb *);
void	spc_dump_driver(struct spc_softc *);
void	spc_dump89352(struct spc_softc *);
void	spc_show_scsi_cmd(struct spc_acb *);
void	spc_print_active_acb(void);
#endif

extern struct cfdriver spc_cd;

/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * Do the real search-for-device.
 * Prerequisite: sc->sc_iobase should be set to the proper value
 */
int
spc_find(bus_space_tag_t iot, bus_space_handle_t ioh, int bdid)
{
	long timeout = SPC_ABORT_TIMEOUT;

	SPC_TRACE(("spc: probing for spc-chip\n"));
	/*
	 * Disable interrupts then reset the FUJITSU chip.
	 */
	bus_space_write_1(iot, ioh, SCTL, SCTL_DISABLE | SCTL_CTRLRST);
	bus_space_write_1(iot, ioh, SCMD, 0);
	bus_space_write_1(iot, ioh, PCTL, 0);
	bus_space_write_1(iot, ioh, TEMP, 0);
	bus_space_write_1(iot, ioh, TCH, 0);
	bus_space_write_1(iot, ioh, TCM, 0);
	bus_space_write_1(iot, ioh, TCL, 0);
	bus_space_write_1(iot, ioh, INTS, 0);
	bus_space_write_1(iot, ioh, SCTL,
	    SCTL_DISABLE | SCTL_ABRT_ENAB | SCTL_PARITY_ENAB | SCTL_RESEL_ENAB);
	bus_space_write_1(iot, ioh, BDID, bdid);
	delay(400);
	bus_space_write_1(iot, ioh, SCTL,
	    bus_space_read_1(iot, ioh, SCTL) & ~SCTL_DISABLE);

	/* The following detection is derived from spc.c
	 * (by Takahide Matsutsuka) in FreeBSD/pccard-test.
	 */
	while (bus_space_read_1(iot, ioh, PSNS) && timeout) {
		timeout--;
		DELAY(1);
	}
	if (timeout == 0) {
		printf("spc: find failed\n");
		return 0;
	}

	SPC_START(("SPC found"));
	return 1;
}

void
spc_attach(struct spc_softc *sc)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;

	SPC_TRACE(("spc_attach  "));
	sc->sc_state = SPC_INIT;

	sc->sc_freq = 20;	/* XXXX Assume 20 MHz. */

#if SPC_USE_SYNCHRONOUS
	/*
	 * These are the bounds of the sync period, based on the frequency of
	 * the chip's clock input and the size and offset of the sync period
	 * register.
	 *
	 * For a 20MHz clock, this gives us 25, or 100ns, or 10MB/s, as a
	 * maximum transfer rate, and 112.5, or 450ns, or 2.22MB/s, as a
	 * minimum transfer rate.
	 */
	sc->sc_minsync = (2 * 250) / sc->sc_freq;
	sc->sc_maxsync = (9 * 250) / sc->sc_freq;
#endif

	/*
	 * Fill in the adapter.
	 */
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = 7;
	adapt->adapt_max_periph = 1;
	adapt->adapt_request = spc_scsipi_request;
	adapt->adapt_minphys = minphys;

	chan->chan_adapter = &sc->sc_adapter;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = sc->sc_initiator;

	/*
	 * Add reference to adapter so that we drop the reference after
	 * config_found() to make sure the adatper is disabled.
	 */
	if (scsipi_adapter_addref(adapt) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to enable controller\n");
		return;
	}

	spc_init(sc, 1);	/* Init chip and driver */

	/*
	 * ask the adapter what subunits are present
	 */
	sc->sc_child = config_found(sc->sc_dev, chan, scsiprint);
	scsipi_adapter_delref(adapt);
}

void
spc_childdet(device_t self, device_t child)
{
	struct spc_softc *sc = device_private(self);

	if (sc->sc_child == child)
		sc->sc_child = NULL;
}

int
spc_detach(device_t self, int flags)
{
	struct spc_softc *sc = device_private(self);
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	return (rv);
}

/*
 * Initialize MB89352 chip itself
 * The following conditions should hold:
 * spc_isa_probe should have succeeded, i.e. the iobase address in spc_softc
 * must be valid.
 */
void
spc_reset(struct spc_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_reset  "));
	/*
	 * Disable interrupts then reset the FUJITSU chip.
	 */
	bus_space_write_1(iot, ioh, SCTL, SCTL_DISABLE | SCTL_CTRLRST);
	bus_space_write_1(iot, ioh, SCMD, 0);
	bus_space_write_1(iot, ioh, TMOD, 0);
	bus_space_write_1(iot, ioh, PCTL, 0);
	bus_space_write_1(iot, ioh, TEMP, 0);
	bus_space_write_1(iot, ioh, TCH, 0);
	bus_space_write_1(iot, ioh, TCM, 0);
	bus_space_write_1(iot, ioh, TCL, 0);
	bus_space_write_1(iot, ioh, INTS, 0);
	bus_space_write_1(iot, ioh, SCTL,
	    SCTL_DISABLE | SCTL_ABRT_ENAB | SCTL_PARITY_ENAB | SCTL_RESEL_ENAB);
	bus_space_write_1(iot, ioh, BDID, sc->sc_initiator);
	delay(400);
	bus_space_write_1(iot, ioh, SCTL,
	    bus_space_read_1(iot, ioh, SCTL) & ~SCTL_DISABLE);
}


/*
 * Pull the SCSI RST line for 500us.
 */
void
spc_scsi_reset(struct spc_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_scsi_reset  "));
	bus_space_write_1(iot, ioh, SCMD,
	    bus_space_read_1(iot, ioh, SCMD) | SCMD_RST);
	delay(500);
	bus_space_write_1(iot, ioh, SCMD,
	    bus_space_read_1(iot, ioh, SCMD) & ~SCMD_RST);
	delay(50);
}

/*
 * Initialize spc SCSI driver.
 */
void
spc_init(struct spc_softc *sc, int bus_reset)
{
	struct spc_acb *acb;
	int r;

	SPC_TRACE(("spc_init  "));
	if (bus_reset) {
		spc_reset(sc);
		spc_scsi_reset(sc);
	}
	spc_reset(sc);

	if (sc->sc_state == SPC_INIT) {
		/* First time through; initialize. */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		acb = sc->sc_acb;
		memset(acb, 0, sizeof(sc->sc_acb));
		for (r = 0; r < sizeof(sc->sc_acb) / sizeof(*acb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
			acb++;
		}
		memset(&sc->sc_tinfo, 0, sizeof(sc->sc_tinfo));
	} else {
		/* Cancel any active commands. */
		sc->sc_state = SPC_CLEANING;
		if ((acb = sc->sc_nexus) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			callout_stop(&acb->xs->xs_callout);
			spc_done(sc, acb);
		}
		while ((acb = TAILQ_FIRST(&sc->nexus_list)) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			callout_stop(&acb->xs->xs_callout);
			spc_done(sc, acb);
		}
	}

	sc->sc_prevphase = PH_INVALID;
	for (r = 0; r < 8; r++) {
		struct spc_tinfo *ti = &sc->sc_tinfo[r];

		ti->flags = 0;
#if SPC_USE_SYNCHRONOUS
		ti->flags |= DO_SYNC;
		ti->period = sc->sc_minsync;
		ti->offset = SPC_SYNC_REQ_ACK_OFS;
#else
		ti->period = ti->offset = 0;
#endif
#if SPC_USE_WIDE
		ti->flags |= DO_WIDE;
		ti->width = SPC_MAX_WIDTH;
#else
		ti->width = 0;
#endif
	}

	sc->sc_state = SPC_IDLE;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCTL,
	    bus_space_read_1(sc->sc_iot, sc->sc_ioh, SCTL) | SCTL_INTR_ENAB);
}

void
spc_free_acb(struct spc_softc *sc, struct spc_acb *acb, int flags)
{
	int s;

	SPC_TRACE(("spc_free_acb  "));
	s = splbio();

	acb->flags = 0;
	TAILQ_INSERT_HEAD(&sc->free_list, acb, chain);
	splx(s);
}

struct spc_acb *
spc_get_acb(struct spc_softc *sc)
{
	struct spc_acb *acb;
	int s;

	SPC_TRACE(("spc_get_acb  "));
	s = splbio();
	acb = TAILQ_FIRST(&sc->free_list);
	if (acb != NULL) {
		TAILQ_REMOVE(&sc->free_list, acb, chain);
		acb->flags |= ACB_ALLOC;
	}
	splx(s);
	return acb;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Expected sequence:
 * 1) Command inserted into ready list
 * 2) Command selected for execution
 * 3) Command won arbitration and has selected target device
 * 4) Send message out (identify message, eventually also sync.negotiations)
 * 5) Send command
 * 5a) Receive disconnect message, disconnect.
 * 5b) Reselected by target
 * 5c) Receive identify message from target.
 * 6) Send or receive data
 * 7) Receive status
 * 8) Receive message (command complete etc.)
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
void
spc_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph __diagused;
	struct spc_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	struct spc_acb *acb;
	int s, flags;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		SPC_TRACE(("spc_scsipi_request  "));
		SPC_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
		    periph->periph_target));

		flags = xs->xs_control;
		acb = spc_get_acb(sc);
#ifdef DIAGNOSTIC
		/*
		 * This should nerver happen as we track the resources
		 * in the mid-layer.
		 */
		if (acb == NULL) {
			scsipi_printaddr(periph);
			printf("unable to allocate acb\n");
			panic("spc_scsipi_request");
		}
#endif

		/* Initialize acb */
		acb->xs = xs;
		acb->timeout = xs->timeout;

		if (xs->xs_control & XS_CTL_RESET) {
			acb->flags |= ACB_RESET;
			acb->scsipi_cmd_length = 0;
			acb->data_length = 0;
		} else {
			memcpy(&acb->scsipi_cmd, xs->cmd, xs->cmdlen);
			acb->scsipi_cmd_length = xs->cmdlen;
			acb->data_addr = xs->data;
			acb->data_length = xs->datalen;
		}
		acb->target_stat = 0;

		s = splbio();

		TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);
		/*
		 * Start scheduling unless a queue process is in progress.
		 */
		if (sc->sc_state == SPC_IDLE)
			spc_sched(sc);
		/*
		 * After successful sending, check if we should return just now.
		 * If so, return SUCCESSFULLY_QUEUED.
		 */

		splx(s);

		if ((flags & XS_CTL_POLL) == 0)
			return;

		/* Not allowed to use interrupts, use polling instead */
		s = splbio();
		if (spc_poll(sc, xs, acb->timeout)) {
			spc_timeout(acb);
			if (spc_poll(sc, xs, acb->timeout))
				spc_timeout(acb);
		}
		splx(s);
		return;
	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;
	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		/*
		 * We don't support Sync, Wide, or Tagged Command Queuing.
		 * Just callback now, to report this.
		 */
		struct scsipi_xfer_mode *xm = arg;

		xm->xm_mode = 0;
		xm->xm_period = 0;
		xm->xm_offset = 0;
		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, xm);
		return;
	    }
	}
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
spc_poll(struct spc_softc *sc, struct scsipi_xfer *xs, int count)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_poll  "));
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, INTS) != 0)
			spc_intr(sc);
		if ((xs->xs_status & XS_STS_DONE) != 0)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

/*
 * LOW LEVEL SCSI UTILITIES
 */

integrate void
spc_sched_msgout(struct spc_softc *sc, uint8_t m)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_sched_msgout  "));
	if (sc->sc_msgpriq == 0)
		bus_space_write_1(iot, ioh, SCMD, SCMD_SET_ATN);
	sc->sc_msgpriq |= m;
}

/*
 * Set synchronous transfer offset and period.
 */
integrate void
spc_setsync(struct spc_softc *sc, struct spc_tinfo *ti)
{
#if SPC_USE_SYNCHRONOUS
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_setsync  "));
	if (ti->offset != 0)
		bus_space_write_1(iot, ioh, TMOD,
		    ((ti->period * sc->sc_freq) / 250 - 2) << 4 | ti->offset);
	else
		bus_space_write_1(iot, ioh, TMOD, 0);
#endif
}

/*
 * Start a selection.  This is used by spc_sched() to select an idle target.
 */
void
spc_select(struct spc_softc *sc, struct spc_acb *acb)
{
	struct scsipi_periph *periph = acb->xs->xs_periph;
	int target = periph->periph_target;
	struct spc_tinfo *ti = &sc->sc_tinfo[target];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	SPC_TRACE(("spc_select  "));
	spc_setsync(sc, ti);

#if 0
	bus_space_write_1(iot, ioh, SCMD, SCMD_SET_ATN);
#endif

	bus_space_write_1(iot, ioh, PCTL, 0);
	bus_space_write_1(iot, ioh, TEMP,
	    (1 << sc->sc_initiator) | (1 << target));
	/*
	 * Setup BSY timeout (selection timeout).
	 * 250ms according to the SCSI specification.
	 * T = (X * 256 + 15) * Tclf * 2  (Tclf = 200ns on x68k)
	 * To setup 256ms timeout,
	 * 128000ns/200ns = X * 256 + 15
	 * 640 - 15 = X * 256
	 * X = 625 / 256
	 * X = 2 + 113 / 256
	 *  ==> tch = 2, tcm = 113 (correct?)
	 */
	/* Time to the information transfer phase start. */
	/* XXX These values should be calculated from sc_freq */
	bus_space_write_1(iot, ioh, TCH, 2);
	bus_space_write_1(iot, ioh, TCM, 113);
	bus_space_write_1(iot, ioh, TCL, 3);
	bus_space_write_1(iot, ioh, SCMD, SCMD_SELECT);

	sc->sc_state = SPC_SELECTING;
}

int
spc_reselect(struct spc_softc *sc, int message)
{
	uint8_t selid, target, lun;
	struct spc_acb *acb;
	struct scsipi_periph *periph;
	struct spc_tinfo *ti;

	SPC_TRACE(("spc_reselect  "));
	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_initiator);
	if (selid & (selid - 1)) {
		printf("%s: reselect with invalid selid %02x; "
		    "sending DEVICE RESET\n", device_xname(sc->sc_dev), selid);
		SPC_BREAK();
		goto reset;
	}

	/*
	 * Search wait queue for disconnected cmd
	 * The list should be short, so I haven't bothered with
	 * any more sophisticated structures than a simple
	 * singly linked list.
	 */
	target = ffs(selid) - 1;
	lun = message & 0x07;
	TAILQ_FOREACH(acb, &sc->nexus_list, chain) {
		periph = acb->xs->xs_periph;
		if (periph->periph_target == target &&
		    periph->periph_lun == lun)
			break;
	}
	if (acb == NULL) {
		printf("%s: reselect from target %d lun %d with no nexus; "
		    "sending ABORT\n", device_xname(sc->sc_dev), target, lun);
		SPC_BREAK();
		goto abort;
	}

	/* Make this nexus active again. */
	TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	sc->sc_state = SPC_CONNECTED;
	sc->sc_nexus = acb;
	ti = &sc->sc_tinfo[target];
	ti->lubusy |= (1 << lun);
	spc_setsync(sc, ti);

	if (acb->flags & ACB_RESET)
		spc_sched_msgout(sc, SEND_DEV_RESET);
	else if (acb->flags & ACB_ABORT)
		spc_sched_msgout(sc, SEND_ABORT);

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = acb->data_addr;
	sc->sc_dleft = acb->data_length;
	sc->sc_cp = (uint8_t *)&acb->scsipi_cmd;
	sc->sc_cleft = acb->scsipi_cmd_length;

	return (0);

reset:
	spc_sched_msgout(sc, SEND_DEV_RESET);
	return (1);

abort:
	spc_sched_msgout(sc, SEND_ABORT);
	return (1);
}

/*
 * Schedule a SCSI operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from spc_scsi_cmd and spc_done.  This may
 * save us an unnecessary interrupt just to get things going.  Should only be
 * called when state == SPC_IDLE and at bio pl.
 */
void
spc_sched(struct spc_softc *sc)
{
	struct spc_acb *acb;
	struct scsipi_periph *periph;
	struct spc_tinfo *ti;

	/* missing the hw, just return and wait for our hw */
	if (sc->sc_flags & SPC_INACTIVE)
		return;
	SPC_TRACE(("spc_sched  "));
	/*
	 * Find first acb in ready queue that is for a target/lunit pair that
	 * is not busy.
	 */
	TAILQ_FOREACH(acb, &sc->ready_list, chain) {
		periph = acb->xs->xs_periph;
		ti = &sc->sc_tinfo[periph->periph_target];
		if ((ti->lubusy & (1 << periph->periph_lun)) == 0) {
			SPC_MISC(("selecting %d:%d  ",
			    periph->periph_target, periph->periph_lun));
			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			sc->sc_nexus = acb;
			spc_select(sc, acb);
			return;
		} else {
			SPC_MISC(("%d:%d busy\n",
			    periph->periph_target, periph->periph_lun));
		}
	}
	SPC_MISC(("idle  "));
	/* Nothing to start; just enable reselections and wait. */
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
spc_done(struct spc_softc *sc, struct spc_acb *acb)
{
	struct scsipi_xfer *xs = acb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct spc_tinfo *ti = &sc->sc_tinfo[periph->periph_target];

	SPC_TRACE(("spc_done  "));

	if (xs->error == XS_NOERROR) {
		if (acb->flags & ACB_ABORT) {
			xs->error = XS_DRIVER_STUFFUP;
		} else {
			switch (acb->target_stat) {
			case SCSI_CHECK:
				/* First, save the return values */
				xs->resid = acb->data_length;
				/* FALLTHROUGH */
			case SCSI_BUSY:
				xs->status = acb->target_stat;
				xs->error = XS_BUSY;
				break;
			case SCSI_OK:
				xs->resid = acb->data_length;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
#if SPC_DEBUG
				printf("%s: spc_done: bad stat 0x%x\n",
				    device_xname(sc->sc_dev), acb->target_stat);
#endif
				break;
			}
		}
	}

#if SPC_DEBUG
	if ((spc_debug & SPC_SHOWMISC) != 0) {
		if (xs->resid != 0)
			printf("resid=%d ", xs->resid);
		else
			printf("error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ACB from whatever queue it happens to be on.
	 */
	if (acb->flags & ACB_NEXUS)
		ti->lubusy &= ~(1 << periph->periph_lun);
	if (acb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_state = SPC_IDLE;
		spc_sched(sc);
	} else
		spc_dequeue(sc, acb);

	spc_free_acb(sc, acb, xs->xs_control);
	ti->cmds++;
	scsipi_done(xs);
}

void
spc_dequeue(struct spc_softc *sc, struct spc_acb *acb)
{

	SPC_TRACE(("spc_dequeue  "));
	if (acb->flags & ACB_NEXUS)
		TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	else
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/*
 * Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 */
void
spc_msgin(struct spc_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int n;
	uint8_t msg;

	SPC_TRACE(("spc_msgin  "));

	if (sc->sc_prevphase == PH_MSGIN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_flags &= ~SPC_DROP_MSGIN;

nextmsg:
	n = 0;
	sc->sc_imp = &sc->sc_imess[n];

nextbyte:
	/*
	 * Read a whole message, but don't ack the last byte.  If we reject the
	 * message, we have to assert ATN during the message transfer phase
	 * itself.
	 */
	for (;;) {
#ifdef NO_MANUAL_XFER /* XXX */
		if (bus_space_read_1(iot, ioh, INTS) != 0) {
			/*
			 * Target left MESSAGE IN, probably because it
			 * a) noticed our ATN signal, or
			 * b) ran out of messages.
			 */
			goto out;
		}
#endif
		/* If parity error, just dump everything on the floor. */
		if ((bus_space_read_1(iot, ioh, SERR) &
		     (SERR_SCSI_PAR|SERR_SPC_PAR)) != 0) {
			sc->sc_flags |= SPC_DROP_MSGIN;
			spc_sched_msgout(sc, SEND_PARITY_ERROR);
		}

#ifdef NO_MANUAL_XFER /* XXX */
		/* send TRANSFER command. */
		bus_space_write_1(iot, ioh, TCH, 0);
		bus_space_write_1(iot, ioh, TCM, 0);
		bus_space_write_1(iot, ioh, TCL, 1);
		bus_space_write_1(iot, ioh, PCTL,
		    sc->sc_phase | PCTL_BFINT_ENAB);
#ifdef NEED_DREQ_ON_HARDWARE_XFER
		bus_space_write_1(iot, ioh, SCMD, SCMD_XFR);
#else
		bus_space_write_1(iot, ioh, SCMD, SCMD_XFR | SCMD_PROG_XFR);
#endif
		for (;;) {
			if ((bus_space_read_1(iot, ioh, SSTS) &
			    SSTS_DREG_EMPTY) == 0)
				break;
			if (bus_space_read_1(iot, ioh, INTS) != 0)
				goto out;
		}
		msg = bus_space_read_1(iot, ioh, DREG);
#else
		if ((bus_space_read_1(iot, ioh, PSNS) & PSNS_ATN) != 0)
			bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ATN);
		bus_space_write_1(iot, ioh, PCTL, PCTL_BFINT_ENAB | PH_MSGIN);

		while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) == 0) {
			if ((bus_space_read_1(iot, ioh, PSNS) & PH_MASK)
			    != PH_MSGIN ||
			    bus_space_read_1(iot, ioh, INTS) != 0)
				/*
				 * Target left MESSAGE IN, probably because it
				 * a) noticed our ATN signal, or
				 * b) ran out of messages.
				 */
				goto out;
			DELAY(1);	/* XXX needs timeout */
		}

		msg = bus_space_read_1(iot, ioh, TEMP);
#endif

		/* Gather incoming message bytes if needed. */
		if ((sc->sc_flags & SPC_DROP_MSGIN) == 0) {
			if (n >= SPC_MAX_MSG_LEN) {
				sc->sc_flags |= SPC_DROP_MSGIN;
				spc_sched_msgout(sc, SEND_REJECT);
			} else {
				*sc->sc_imp++ = msg;
				n++;
				/*
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not affect performance
				 * significantly.
				 */
				if (n == 1 && MSG_IS1BYTE(sc->sc_imess[0]))
					break;
				if (n == 2 && MSG_IS2BYTE(sc->sc_imess[0]))
					break;
				if (n >= 3 && MSG_ISEXTENDED(sc->sc_imess[0]) &&
				    n == sc->sc_imess[1] + 2)
					break;
			}
		}
		/*
		 * If we reach this spot we're either:
		 * a) in the middle of a multi-byte message, or
		 * b) dropping bytes.
		 */

#ifndef NO_MANUAL_XFER /* XXX */
		/* Ack the last byte read. */
		bus_space_write_1(iot, ioh, SCMD, SCMD_SET_ACK);
		while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) != 0)
			DELAY(1);	/* XXX needs timeout */
		bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ACK);
#endif
	}

	SPC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));

	/* We now have a complete message.  Parse it. */
	switch (sc->sc_state) {
		struct spc_acb *acb;
		struct spc_tinfo *ti;

	case SPC_CONNECTED:
		SPC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;
		ti = &sc->sc_tinfo[acb->xs->xs_periph->periph_target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
#if 0
			if (sc->sc_dleft < 0) {
				periph = acb->xs->xs_periph;
				printf("%s: %ld extra bytes from %d:%d\n",
				    device_xname(sc->sc_dev),
				    (long)-sc->sc_dleft,
				    periph->periph_target, periph->periph_lun);
				sc->sc_dleft = 0;
			}
#endif
			acb->xs->resid = acb->data_length = sc->sc_dleft;
			sc->sc_state = SPC_CMDCOMPLETE;
			break;

		case MSG_PARITY_ERROR:
			/* Resend the last message. */
			spc_sched_msgout(sc, sc->sc_lastmsg);
			break;

		case MSG_MESSAGE_REJECT:
			SPC_MISC(("message rejected %02x  ", sc->sc_lastmsg));
			switch (sc->sc_lastmsg) {
#if SPC_USE_SYNCHRONOUS + SPC_USE_WIDE
			case SEND_IDENTIFY:
				ti->flags &= ~(DO_SYNC | DO_WIDE);
				ti->period = ti->offset = 0;
				spc_setsync(sc, ti);
				ti->width = 0;
				break;
#endif
#if SPC_USE_SYNCHRONOUS
			case SEND_SDTR:
				ti->flags &= ~DO_SYNC;
				ti->period = ti->offset = 0;
				spc_setsync(sc, ti);
				break;
#endif
#if SPC_USE_WIDE
			case SEND_WDTR:
				ti->flags &= ~DO_WIDE;
				ti->width = 0;
				break;
#endif
			case SEND_INIT_DET_ERR:
				spc_sched_msgout(sc, SEND_ABORT);
				break;
			}
			break;

		case MSG_NOOP:
			break;

		case MSG_DISCONNECT:
			ti->dconns++;
			sc->sc_state = SPC_DISCONNECT;
			break;

		case MSG_SAVEDATAPOINTER:
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)&acb->scsipi_cmd;
			sc->sc_cleft = acb->scsipi_cmd_length;
			break;

		case MSG_EXTENDED:
			switch (sc->sc_imess[2]) {
#if SPC_USE_SYNCHRONOUS
			case MSG_EXT_SDTR:
				if (sc->sc_imess[1] != 3)
					goto reject;
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				ti->flags &= ~DO_SYNC;
				if (ti->offset == 0) {
				} else if (ti->period < sc->sc_minsync ||
				    ti->period > sc->sc_maxsync ||
				    ti->offset > 8) {
					ti->period = ti->offset = 0;
					spc_sched_msgout(sc, SEND_SDTR);
				} else {
					scsipi_printaddr(acb->xs->xs_periph);
					printf("sync, offset %d, "
					    "period %dnsec\n",
					    ti->offset, ti->period * 4);
				}
				spc_setsync(sc, ti);
				break;
#endif

#if SPC_USE_WIDE
			case MSG_EXT_WDTR:
				if (sc->sc_imess[1] != 2)
					goto reject;
				ti->width = sc->sc_imess[3];
				ti->flags &= ~DO_WIDE;
				if (ti->width == 0) {
				} else if (ti->width > SPC_MAX_WIDTH) {
					ti->width = 0;
					spc_sched_msgout(sc, SEND_WDTR);
				} else {
					scsipi_printaddr(acb->xs->xs_periph);
					printf("wide, width %d\n",
					    1 << (3 + ti->width));
				}
				break;
#endif

			default:
				printf("%s: unrecognized MESSAGE EXTENDED; "
				    "sending REJECT\n",
				    device_xname(sc->sc_dev));
				SPC_BREAK();
				goto reject;
			}
			break;

		default:
			printf("%s: unrecognized MESSAGE; sending REJECT\n",
			    device_xname(sc->sc_dev));
			SPC_BREAK();
		reject:
			spc_sched_msgout(sc, SEND_REJECT);
			break;
		}
		break;

	case SPC_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("%s: reselect without IDENTIFY; "
			    "sending DEVICE RESET\n", device_xname(sc->sc_dev));
			SPC_BREAK();
			goto reset;
		}

		(void) spc_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
		    device_xname(sc->sc_dev));
		SPC_BREAK();
	reset:
		spc_sched_msgout(sc, SEND_DEV_RESET);
		break;

#ifdef notdef
	abort:
		spc_sched_msgout(sc, SEND_ABORT);
		break;
#endif
	}

#ifndef NO_MANUAL_XFER /* XXX */
	/* Ack the last message byte. */
	bus_space_write_1(iot, ioh, SCMD, SCMD_SET_ACK);
	while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) != 0)
		DELAY(1);	/* XXX needs timeout */
	bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ACK);
#endif

	/* Go get the next message, if any. */
	goto nextmsg;

out:
#ifdef NO_MANUAL_XFER /* XXX */
	/* Ack the last message byte. */
	bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ACK);
#endif
	SPC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));
}

/*
 * Send the highest priority, scheduled message.
 */
void
spc_msgout(struct spc_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
#if SPC_USE_SYNCHRONOUS
	struct spc_tinfo *ti;
#endif
	int n;

	SPC_TRACE(("spc_msgout  "));

	if (sc->sc_prevphase == PH_MSGOUT) {
		if (sc->sc_omp == sc->sc_omess) {
			/*
			 * This is a retransmission.
			 *
			 * We get here if the target stayed in MESSAGE OUT
			 * phase.  Section 5.1.9.2 of the SCSI 2 spec indicates
			 * that all of the previously transmitted messages must
			 * be sent again, in the same order.  Therefore, we
			 * requeue all the previously transmitted messages, and
			 * start again from the top.  Our simple priority
			 * scheme keeps the messages in the right order.
			 */
			SPC_MISC(("retransmitting  "));
			sc->sc_msgpriq |= sc->sc_msgoutq;
			/*
			 * Set ATN.  If we're just sending a trivial 1-byte
			 * message, we'll clear ATN later on anyway.
			 */
			bus_space_write_1(iot, ioh, SCMD,
			    SCMD_SET_ATN);	/* XXX? */
		} else {
			/* This is a continuation of the previous message. */
			n = sc->sc_omp - sc->sc_omess;
			goto nextbyte;
		}
	}

	/* No messages transmitted so far. */
	sc->sc_msgoutq = 0;
	sc->sc_lastmsg = 0;

nextmsg:
	/* Pick up highest priority message. */
	sc->sc_currmsg = sc->sc_msgpriq & -sc->sc_msgpriq;
	sc->sc_msgpriq &= ~sc->sc_currmsg;
	sc->sc_msgoutq |= sc->sc_currmsg;

	/* Build the outgoing message data. */
	switch (sc->sc_currmsg) {
	case SEND_IDENTIFY:
		SPC_ASSERT(sc->sc_nexus != NULL);
		sc->sc_omess[0] =
		    MSG_IDENTIFY(sc->sc_nexus->xs->xs_periph->periph_lun, 1);
		n = 1;
		break;

#if SPC_USE_SYNCHRONOUS
	case SEND_SDTR:
		SPC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->xs_periph->periph_target];
		sc->sc_omess[4] = MSG_EXTENDED;
		sc->sc_omess[3] = MSG_EXT_SDTR_LEN;
		sc->sc_omess[2] = MSG_EXT_SDTR;
		sc->sc_omess[1] = ti->period >> 2;
		sc->sc_omess[0] = ti->offset;
		n = 5;
		break;
#endif

#if SPC_USE_WIDE
	case SEND_WDTR:
		SPC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->xs_periph->periph_target];
		sc->sc_omess[3] = MSG_EXTENDED;
		sc->sc_omess[2] = MSG_EXT_WDTR_LEN;
		sc->sc_omess[1] = MSG_EXT_WDTR;
		sc->sc_omess[0] = ti->width;
		n = 4;
		break;
#endif

	case SEND_DEV_RESET:
		sc->sc_flags |= SPC_ABORTING;
		sc->sc_omess[0] = MSG_BUS_DEV_RESET;
		n = 1;
		break;

	case SEND_REJECT:
		sc->sc_omess[0] = MSG_MESSAGE_REJECT;
		n = 1;
		break;

	case SEND_PARITY_ERROR:
		sc->sc_omess[0] = MSG_PARITY_ERROR;
		n = 1;
		break;

	case SEND_INIT_DET_ERR:
		sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
		n = 1;
		break;

	case SEND_ABORT:
		sc->sc_flags |= SPC_ABORTING;
		sc->sc_omess[0] = MSG_ABORT;
		n = 1;
		break;

	default:
		printf("%s: unexpected MESSAGE OUT; sending NOOP\n",
		    device_xname(sc->sc_dev));
		SPC_BREAK();
		sc->sc_omess[0] = MSG_NOOP;
		n = 1;
		break;
	}
	sc->sc_omp = &sc->sc_omess[n];

nextbyte:
	/* Send message bytes. */
	/* send TRANSFER command. */
	bus_space_write_1(iot, ioh, TCH, n >> 16);
	bus_space_write_1(iot, ioh, TCM, n >> 8);
	bus_space_write_1(iot, ioh, TCL, n);
	bus_space_write_1(iot, ioh, PCTL, sc->sc_phase | PCTL_BFINT_ENAB);
#ifdef NEED_DREQ_ON_HARDWARE_XFER
	bus_space_write_1(iot, ioh, SCMD, SCMD_XFR);	/* XXX */
#else
	bus_space_write_1(iot, ioh, SCMD,
	    SCMD_XFR | SCMD_PROG_XFR);
#endif
	for (;;) {
		if ((bus_space_read_1(iot, ioh, SSTS) & SSTS_BUSY) != 0)
			break;
		if (bus_space_read_1(iot, ioh, INTS) != 0)
			goto out;
	}
	for (;;) {
#if 0
		for (;;) {
			if ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) != 0)
				break;
			/* Wait for REQINIT.  XXX Need timeout. */
		}
#endif
		if (bus_space_read_1(iot, ioh, INTS) != 0) {
			/*
			 * Target left MESSAGE OUT, possibly to reject
			 * our message.
			 *
			 * If this is the last message being sent, then we
			 * deassert ATN, since either the target is going to
			 * ignore this message, or it's going to ask for a
			 * retransmission via MESSAGE PARITY ERROR (in which
			 * case we reassert ATN anyway).
			 */
#if 0
			if (sc->sc_msgpriq == 0)
				bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ATN);
#endif
			goto out;
		}

#if 0
		/* Clear ATN before last byte if this is the last message. */
		if (n == 1 && sc->sc_msgpriq == 0)
			bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ATN);
#endif

		while ((bus_space_read_1(iot, ioh, SSTS) & SSTS_DREG_FULL) != 0)
			DELAY(1);
		/* Send message byte. */
		bus_space_write_1(iot, ioh, DREG, *--sc->sc_omp);
		--n;
		/* Keep track of the last message we've sent any bytes of. */
		sc->sc_lastmsg = sc->sc_currmsg;
#if 0
		/* Wait for ACK to be negated.  XXX Need timeout. */
		while ((bus_space_read_1(iot, ioh, PSNS) & ACKI) != 0)
			;
#endif

		if (n == 0)
			break;
	}

	/* We get here only if the entire message has been transmitted. */
	if (sc->sc_msgpriq != 0) {
		/* There are more outgoing messages. */
		goto nextmsg;
	}

	/*
	 * The last message has been transmitted.  We need to remember the last
	 * message transmitted (in case the target switches to MESSAGE IN phase
	 * and sends a MESSAGE REJECT), and the list of messages transmitted
	 * this time around (in case the target stays in MESSAGE OUT phase to
	 * request a retransmit).
	 */

out:
	/* Disable REQ/ACK protocol. */
	return;
}

/*
 * spc_dataout_pio: perform a data transfer using the FIFO datapath in the spc
 * Precondition: The SCSI bus should be in the DOUT phase, with REQ asserted
 * and ACK deasserted (i.e. waiting for a data byte)
 *
 * This new revision has been optimized (I tried) to make the common case fast,
 * and the rarer cases (as a result) somewhat more comlex
 */
int
spc_dataout_pio(struct spc_softc *sc, uint8_t *p, int n)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t intstat = 0;
	int out = 0;
#define DOUTAMOUNT 8		/* Full FIFO */

	SPC_TRACE(("spc_dataout_pio  "));
	/* send TRANSFER command. */
	bus_space_write_1(iot, ioh, TCH, n >> 16);
	bus_space_write_1(iot, ioh, TCM, n >> 8);
	bus_space_write_1(iot, ioh, TCL, n);
	bus_space_write_1(iot, ioh, PCTL, sc->sc_phase | PCTL_BFINT_ENAB);
#ifdef NEED_DREQ_ON_HARDWARE_XFER
	bus_space_write_1(iot, ioh, SCMD, SCMD_XFR);	/* XXX */
#else
	bus_space_write_1(iot, ioh, SCMD,
	    SCMD_XFR | SCMD_PROG_XFR);	/* XXX */
#endif
	for (;;) {
		if ((bus_space_read_1(iot, ioh, SSTS) & SSTS_BUSY) != 0)
			break;
		if (bus_space_read_1(iot, ioh, INTS) != 0)
			break;
	}

	/*
	 * I have tried to make the main loop as tight as possible.  This
	 * means that some of the code following the loop is a bit more
	 * complex than otherwise.
	 */
	while (n > 0) {
		int xfer;

		for (;;) {
			intstat = bus_space_read_1(iot, ioh, INTS);
			/* Wait till buffer is empty. */
			if ((bus_space_read_1(iot, ioh, SSTS) &
			    SSTS_DREG_EMPTY) != 0)
				break;
			/* Break on interrupt. */
			if (intstat != 0)
				goto phasechange;
			DELAY(1);
		}

		xfer = min(DOUTAMOUNT, n);

		SPC_MISC(("%d> ", xfer));

		n -= xfer;
		out += xfer;

		bus_space_write_multi_1(iot, ioh, DREG, p, xfer);
		p += xfer;
	}

	if (out == 0) {
		for (;;) {
			if (bus_space_read_1(iot, ioh, INTS) != 0)
				break;
			DELAY(1);
		}
		SPC_MISC(("extra data  "));
	} else {
		/* See the bytes off chip */
		for (;;) {
			/* Wait till buffer is empty. */
			if ((bus_space_read_1(iot, ioh, SSTS) &
			    SSTS_DREG_EMPTY) != 0)
				break;
			intstat = bus_space_read_1(iot, ioh, INTS);
			/* Break on interrupt. */
			if (intstat != 0)
				goto phasechange;
			DELAY(1);
		}
	}

phasechange:
	/* Stop the FIFO data path. */

	if (intstat != 0) {
		/* Some sort of phase change. */
		int amount;

		amount = (bus_space_read_1(iot, ioh, TCH) << 16) |
		    (bus_space_read_1(iot, ioh, TCM) << 8) |
		    bus_space_read_1(iot, ioh, TCL);
		if (amount > 0) {
			out -= amount;
			SPC_MISC(("+%d ", amount));
		}
	}

	return out;
}

/*
 * spc_datain_pio: perform data transfers using the FIFO datapath in the spc
 * Precondition: The SCSI bus should be in the DIN phase, with REQ asserted
 * and ACK deasserted (i.e. at least one byte is ready).
 *
 * For now, uses a pretty dumb algorithm, hangs around until all data has been
 * transferred.  This, is OK for fast targets, but not so smart for slow
 * targets which don't disconnect or for huge transfers.
 */
int
spc_datain_pio(struct spc_softc *sc, uint8_t *p, int n)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int in = 0;
	uint8_t intstat, sstat;
#define DINAMOUNT 8		/* Full FIFO */

	SPC_TRACE(("spc_datain_pio  "));
	/* send TRANSFER command. */
	bus_space_write_1(iot, ioh, TCH, n >> 16);
	bus_space_write_1(iot, ioh, TCM, n >> 8);
	bus_space_write_1(iot, ioh, TCL, n);
	bus_space_write_1(iot, ioh, PCTL, sc->sc_phase | PCTL_BFINT_ENAB);
#ifdef NEED_DREQ_ON_HARDWARE_XFER
	bus_space_write_1(iot, ioh, SCMD, SCMD_XFR);	/* XXX */
#else
	bus_space_write_1(iot, ioh, SCMD,
	    SCMD_XFR | SCMD_PROG_XFR);	/* XXX */
#endif

	/*
	 * We leave this loop if one or more of the following is true:
	 * a) phase != PH_DATAIN && FIFOs are empty
	 * b) reset has occurred or busfree is detected.
	 */
	intstat = 0;
	while (n > 0) {
		sstat = bus_space_read_1(iot, ioh, SSTS);
		if ((sstat & SSTS_DREG_FULL) != 0) {
			n -= DINAMOUNT;
			in += DINAMOUNT;
			bus_space_read_multi_1(iot, ioh, DREG, p, DINAMOUNT);
			p += DINAMOUNT;
		} else if ((sstat & SSTS_DREG_EMPTY) == 0) {
			n--;
			in++;
			*p++ = bus_space_read_1(iot, ioh, DREG);
		} else {
			if (intstat != 0)
				goto phasechange;
			intstat = bus_space_read_1(iot, ioh, INTS);
		}
	}

	/*
	 * Some SCSI-devices are rude enough to transfer more data than what
	 * was requested, e.g. 2048 bytes from a CD-ROM instead of the
	 * requested 512.  Test for progress, i.e. real transfers.  If no real
	 * transfers have been performed (n is probably already zero) and the
	 * FIFO is not empty, waste some bytes....
	 */
	if (in == 0) {
		for (;;) {
			sstat = bus_space_read_1(iot, ioh, SSTS);
			if ((sstat & SSTS_DREG_EMPTY) == 0) {
				(void) bus_space_read_1(iot, ioh, DREG);
			} else {
				if (intstat != 0)
					goto phasechange;
				intstat = bus_space_read_1(iot, ioh, INTS);
			}
			DELAY(1);
		}
		SPC_MISC(("extra data  "));
	}

phasechange:
	/* Stop the FIFO data path. */

	return in;
}

/*
 * Catch an interrupt from the adaptor
 */
/*
 * This is the workhorse routine of the driver.
 * Deficiencies (for now):
 * 1) always uses programmed I/O
 */
int
spc_intr(void *arg)
{
	struct spc_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t ints;
	struct spc_acb *acb;
	struct scsipi_periph *periph;
	struct spc_tinfo *ti;
	int n;

	SPC_TRACE(("spc_intr  "));

	ints = bus_space_read_1(iot, ioh, INTS);
	if (ints == 0)
		return 0;

	/*
	 * Disable interrupt.
	 */
	bus_space_write_1(iot, ioh, SCTL,
	    bus_space_read_1(iot, ioh, SCTL) & ~SCTL_INTR_ENAB);

	if (sc->sc_dma_done != NULL &&
	    sc->sc_state == SPC_CONNECTED &&
	    (sc->sc_flags & SPC_DOINGDMA) != 0 &&
	    (sc->sc_phase == PH_DATAOUT || sc->sc_phase == PH_DATAIN)) {
		(*sc->sc_dma_done)(sc);
	}

loop:
	/*
	 * Loop until transfer completion.
	 */
	/*
	 * First check for abnormal conditions, such as reset.
	 */
	ints = bus_space_read_1(iot, ioh, INTS);
	SPC_MISC(("ints = 0x%x  ", ints));

	if ((ints & INTS_RST) != 0) {
		printf("%s: SCSI bus reset\n", device_xname(sc->sc_dev));
		goto reset;
	}

	/*
	 * Check for less serious errors.
	 */
	if ((bus_space_read_1(iot, ioh, SERR) & (SERR_SCSI_PAR|SERR_SPC_PAR))
	    != 0) {
		printf("%s: SCSI bus parity error\n", device_xname(sc->sc_dev));
		if (sc->sc_prevphase == PH_MSGIN) {
			sc->sc_flags |= SPC_DROP_MSGIN;
			spc_sched_msgout(sc, SEND_PARITY_ERROR);
		} else
			spc_sched_msgout(sc, SEND_INIT_DET_ERR);
	}

	/*
	 * If we're not already busy doing something test for the following
	 * conditions:
	 * 1) We have been reselected by something
	 * 2) We have selected something successfully
	 * 3) Our selection process has timed out
	 * 4) This is really a bus free interrupt just to get a new command
	 *    going?
	 * 5) Spurious interrupt?
	 */
	switch (sc->sc_state) {
	case SPC_IDLE:
	case SPC_SELECTING:
		SPC_MISC(("ints:0x%02x ", ints));

		if ((ints & INTS_SEL) != 0) {
			/*
			 * We don't currently support target mode.
			 */
			printf("%s: target mode selected; going to BUS FREE\n",
			    device_xname(sc->sc_dev));

			goto sched;
		} else if ((ints & INTS_RESEL) != 0) {
			SPC_MISC(("reselected  "));

			/*
			 * If we're trying to select a target ourselves,
			 * push our command back into the ready list.
			 */
			if (sc->sc_state == SPC_SELECTING) {
				SPC_MISC(("backoff selector  "));
				SPC_ASSERT(sc->sc_nexus != NULL);
				acb = sc->sc_nexus;
				sc->sc_nexus = NULL;
				TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
			}

			/* Save reselection ID. */
			sc->sc_selid = bus_space_read_1(iot, ioh, TEMP);

			sc->sc_state = SPC_RESELECTED;
		} else if ((ints & INTS_CMD_DONE) != 0) {
			SPC_MISC(("selected  "));

			/*
			 * We have selected a target. Things to do:
			 * a) Determine what message(s) to send.
			 * b) Verify that we're still selecting the target.
			 * c) Mark device as busy.
			 */
			if (sc->sc_state != SPC_SELECTING) {
				printf("%s: selection out while idle; "
				    "resetting\n", device_xname(sc->sc_dev));
				SPC_BREAK();
				goto reset;
			}
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			periph = acb->xs->xs_periph;
			ti = &sc->sc_tinfo[periph->periph_target];

			sc->sc_msgpriq = SEND_IDENTIFY;
			if (acb->flags & ACB_RESET)
				sc->sc_msgpriq |= SEND_DEV_RESET;
			else if (acb->flags & ACB_ABORT)
				sc->sc_msgpriq |= SEND_ABORT;
			else {
#if SPC_USE_SYNCHRONOUS
				if ((ti->flags & DO_SYNC) != 0)
					sc->sc_msgpriq |= SEND_SDTR;
#endif
#if SPC_USE_WIDE
				if ((ti->flags & DO_WIDE) != 0)
					sc->sc_msgpriq |= SEND_WDTR;
#endif
			}

			acb->flags |= ACB_NEXUS;
			ti->lubusy |= (1 << periph->periph_lun);

			/* Do an implicit RESTORE POINTERS. */
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)&acb->scsipi_cmd;
			sc->sc_cleft = acb->scsipi_cmd_length;

			/* On our first connection, schedule a timeout. */
			if ((acb->xs->xs_control & XS_CTL_POLL) == 0)
				callout_reset(&acb->xs->xs_callout,
				    mstohz(acb->timeout), spc_timeout, acb);

			sc->sc_state = SPC_CONNECTED;
		} else if ((ints & INTS_TIMEOUT) != 0) {
			SPC_MISC(("selection timeout  "));

			if (sc->sc_state != SPC_SELECTING) {
				printf("%s: selection timeout while idle; "
				    "resetting\n", device_xname(sc->sc_dev));
				SPC_BREAK();
				goto reset;
			}
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;

			delay(250);

			acb->xs->error = XS_SELTIMEOUT;
			goto finish;
		} else {
			if (sc->sc_state != SPC_IDLE) {
				printf("%s: BUS FREE while not idle; "
				    "state=%d\n",
				    device_xname(sc->sc_dev), sc->sc_state);
				SPC_BREAK();
				goto out;
			}

			goto sched;
		}

		/*
		 * Turn off selection stuff, and prepare to catch bus free
		 * interrupts, parity errors, and phase changes.
		 */

		sc->sc_flags = 0;
		sc->sc_prevphase = PH_INVALID;
		goto dophase;
	}

	if ((ints & INTS_DISCON) != 0) {
		/* We've gone to BUS FREE phase. */
		/* disable disconnect interrupt */
		bus_space_write_1(iot, ioh, PCTL,
		    bus_space_read_1(iot, ioh, PCTL) & ~PCTL_BFINT_ENAB);
		/* XXX reset interrput */
		bus_space_write_1(iot, ioh, INTS, ints);

		switch (sc->sc_state) {
		case SPC_RESELECTED:
			goto sched;

		case SPC_CONNECTED:
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;

#if SPC_USE_SYNCHRONOUS + SPC_USE_WIDE
			if (sc->sc_prevphase == PH_MSGOUT) {
				/*
				 * If the target went to BUS FREE phase during
				 * or immediately after sending a SDTR or WDTR
				 * message, disable negotiation.
				 */
				periph = acb->xs->xs_periph;
				ti = &sc->sc_tinfo[periph->periph_target];
				switch (sc->sc_lastmsg) {
#if SPC_USE_SYNCHRONOUS
				case SEND_SDTR:
					ti->flags &= ~DO_SYNC;
					ti->period = ti->offset = 0;
					break;
#endif
#if SPC_USE_WIDE
				case SEND_WDTR:
					ti->flags &= ~DO_WIDE;
					ti->width = 0;
					break;
#endif
				}
			}
#endif

			if ((sc->sc_flags & SPC_ABORTING) == 0) {
				/*
				 * Section 5.1.1 of the SCSI 2 spec suggests
				 * issuing a REQUEST SENSE following an
				 * unexpected disconnect.  Some devices go into
				 * a contingent allegiance condition when
				 * disconnecting, and this is necessary to
				 * clean up their state.
				 */
				printf("%s: unexpected disconnect; "
				    "sending REQUEST SENSE\n",
				    device_xname(sc->sc_dev));
				SPC_BREAK();
				acb->target_stat = SCSI_CHECK;
				acb->xs->error = XS_NOERROR;
				goto finish;
			}

			acb->xs->error = XS_DRIVER_STUFFUP;
			goto finish;

		case SPC_DISCONNECT:
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			TAILQ_INSERT_HEAD(&sc->nexus_list, acb, chain);
			sc->sc_nexus = NULL;
			goto sched;

		case SPC_CMDCOMPLETE:
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			goto finish;
		}
	}
	else if ((ints & INTS_CMD_DONE) != 0 &&
	    sc->sc_prevphase == PH_MSGIN &&
	    sc->sc_state != SPC_CONNECTED)
		goto out;

dophase:
#if 0
	if ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) == 0) {
		/* Wait for REQINIT. */
		goto out;
	}
#else
	bus_space_write_1(iot, ioh, INTS, ints);
	ints = 0;
	while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) == 0)
		delay(1);	/* need timeout XXX */
#endif

	/*
	 * State transition.
	 */
	sc->sc_phase = bus_space_read_1(iot, ioh, PSNS) & PH_MASK;
#if 0
	bus_space_write_1(iot, ioh, PCTL, sc->sc_phase);
#endif

	SPC_MISC(("phase=%d\n", sc->sc_phase));
	switch (sc->sc_phase) {
	case PH_MSGOUT:
		if (sc->sc_state != SPC_CONNECTED &&
		    sc->sc_state != SPC_RESELECTED)
			break;
		spc_msgout(sc);
		sc->sc_prevphase = PH_MSGOUT;
		goto loop;

	case PH_MSGIN:
		if (sc->sc_state != SPC_CONNECTED &&
		    sc->sc_state != SPC_RESELECTED)
			break;
		spc_msgin(sc);
		sc->sc_prevphase = PH_MSGIN;
		goto loop;

	case PH_CMD:
		if (sc->sc_state != SPC_CONNECTED)
			break;
#if SPC_DEBUG
		if ((spc_debug & SPC_SHOWMISC) != 0) {
			SPC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			printf("cmd=0x%02x+%d  ",
			    acb->scsipi_cmd.opcode, acb->scsipi_cmd_length - 1);
		}
#endif
		n = spc_dataout_pio(sc, sc->sc_cp, sc->sc_cleft);
		sc->sc_cp += n;
		sc->sc_cleft -= n;
		sc->sc_prevphase = PH_CMD;
		goto loop;

	case PH_DATAOUT:
		if (sc->sc_state != SPC_CONNECTED)
			break;
		SPC_MISC(("dataout dleft=%zu  ", sc->sc_dleft));
		if (sc->sc_dma_start != NULL &&
		    sc->sc_dleft > SPC_MIN_DMA_LEN) {
			(*sc->sc_dma_start)(sc, sc->sc_dp, sc->sc_dleft, 0);
			sc->sc_prevphase = PH_DATAOUT;
			goto out;
		}
		n = spc_dataout_pio(sc, sc->sc_dp, sc->sc_dleft);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAOUT;
		goto loop;

	case PH_DATAIN:
		if (sc->sc_state != SPC_CONNECTED)
			break;
		SPC_MISC(("datain  "));
		if (sc->sc_dma_start != NULL &&
		    sc->sc_dleft > SPC_MIN_DMA_LEN) {
			(*sc->sc_dma_start)(sc, sc->sc_dp, sc->sc_dleft, 1);
			sc->sc_prevphase = PH_DATAIN;
			goto out;
		}
		n = spc_datain_pio(sc, sc->sc_dp, sc->sc_dleft);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAIN;
		goto loop;

	case PH_STAT:
		if (sc->sc_state != SPC_CONNECTED)
			break;
		SPC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;

		if ((bus_space_read_1(iot, ioh, PSNS) & PSNS_ATN) != 0)
			bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ATN);
		bus_space_write_1(iot, ioh, PCTL, PCTL_BFINT_ENAB | PH_STAT);
		while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) == 0)
			DELAY(1);	/* XXX needs timeout */
		acb->target_stat = bus_space_read_1(iot, ioh, TEMP);
		bus_space_write_1(iot, ioh, SCMD, SCMD_SET_ACK);
		while ((bus_space_read_1(iot, ioh, PSNS) & PSNS_REQ) != 0)
			DELAY(1);	/* XXX needs timeout */
		bus_space_write_1(iot, ioh, SCMD, SCMD_RST_ACK);

		SPC_MISC(("target_stat=0x%02x  ", acb->target_stat));
		sc->sc_prevphase = PH_STAT;
		goto loop;
	}

	printf("%s: unexpected bus phase; resetting\n",
	    device_xname(sc->sc_dev));
	SPC_BREAK();
reset:
	spc_init(sc, 1);
	return 1;

finish:
	callout_stop(&acb->xs->xs_callout);
	bus_space_write_1(iot, ioh, INTS, ints);
	ints = 0;
	spc_done(sc, acb);
	goto out;

sched:
	sc->sc_state = SPC_IDLE;
	spc_sched(sc);
	goto out;

out:
	if (ints)
		bus_space_write_1(iot, ioh, INTS, ints);
	bus_space_write_1(iot, ioh, SCTL,
	    bus_space_read_1(iot, ioh, SCTL) | SCTL_INTR_ENAB);
	return 1;
}

void
spc_abort(struct spc_softc *sc, struct spc_acb *acb)
{

	/* 2 secs for the abort */
	acb->timeout = SPC_ABORT_TIMEOUT;
	acb->flags |= ACB_ABORT;

	if (acb == sc->sc_nexus) {
		/*
		 * If we're still selecting, the message will be scheduled
		 * after selection is complete.
		 */
		if (sc->sc_state == SPC_CONNECTED)
			spc_sched_msgout(sc, SEND_ABORT);
	} else {
		spc_dequeue(sc, acb);
		TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
		if (sc->sc_state == SPC_IDLE)
			spc_sched(sc);
	}
}

void
spc_timeout(void *arg)
{
	struct spc_acb *acb = arg;
	struct scsipi_xfer *xs = acb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct spc_softc *sc;
	int s;

	sc = device_private(periph->periph_channel->chan_adapter->adapt_dev);
	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

	if (acb->flags & ACB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		acb->xs->error = XS_TIMEOUT;
		spc_abort(sc, acb);
	}

	splx(s);
}

#ifdef SPC_DEBUG
/*
 * The following functions are mostly used for debugging purposes, either
 * directly called from the driver or from the kernel debugger.
 */

void
spc_show_scsi_cmd(struct spc_acb *acb)
{
	uint8_t  *b = (uint8_t *)&acb->scsipi_cmd;
	int i;

	scsipi_printaddr(acb->xs->xs_periph);
	if ((acb->xs->xs_control & XS_CTL_RESET) == 0) {
		for (i = 0; i < acb->scsipi_cmd_length; i++) {
			if (i)
				printf(",");
			printf("%x", b[i]);
		}
		printf("\n");
	} else
		printf("RESET\n");
}

void
spc_print_acb(struct spc_acb *acb)
{

	printf("acb@%p xs=%p flags=%x", acb, acb->xs, acb->flags);
	printf(" dp=%p dleft=%d target_stat=%x\n",
	    acb->data_addr, acb->data_length, acb->target_stat);
	spc_show_scsi_cmd(acb);
}

void
spc_print_active_acb(void)
{
	struct spc_acb *acb;
	struct spc_softc *sc = device_lookup_private(&spc_cd, 0); /* XXX */

	printf("ready list:\n");
	TAILQ_FOREACH(acb, &sc->ready_list, chain)
		spc_print_acb(acb);
	printf("nexus:\n");
	if (sc->sc_nexus != NULL)
		spc_print_acb(sc->sc_nexus);
	printf("nexus list:\n");
	TAILQ_FOREACH(acb, &sc->nexus_list, chain)
		spc_print_acb(acb);
}

void
spc_dump89352(struct spc_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	printf("mb89352: BDID=%x SCTL=%x SCMD=%x TMOD=%x\n",
	    bus_space_read_1(iot, ioh, BDID),
	    bus_space_read_1(iot, ioh, SCTL),
	    bus_space_read_1(iot, ioh, SCMD),
	    bus_space_read_1(iot, ioh, TMOD));
	printf("         INTS=%x PSNS=%x SSTS=%x SERR=%x PCTL=%x\n",
	    bus_space_read_1(iot, ioh, INTS),
	    bus_space_read_1(iot, ioh, PSNS),
	    bus_space_read_1(iot, ioh, SSTS),
	    bus_space_read_1(iot, ioh, SERR),
	    bus_space_read_1(iot, ioh, PCTL));
	printf("         MBC=%x DREG=%x TEMP=%x TCH=%x TCM=%x\n",
	    bus_space_read_1(iot, ioh, MBC),
#if 0
	    bus_space_read_1(iot, ioh, DREG),
#else
	    0,
#endif
	    bus_space_read_1(iot, ioh, TEMP),
	    bus_space_read_1(iot, ioh, TCH),
	    bus_space_read_1(iot, ioh, TCM));
	printf("         TCL=%x EXBF=%x\n",
	    bus_space_read_1(iot, ioh, TCL),
	    bus_space_read_1(iot, ioh, EXBF));
}

void
spc_dump_driver(struct spc_softc *sc)
{
	struct spc_tinfo *ti;
	int i;

	printf("nexus=%p prevphase=%x\n", sc->sc_nexus, sc->sc_prevphase);
	printf("state=%x msgin=%x msgpriq=%x msgoutq=%x lastmsg=%x "
	    "currmsg=%x\n", sc->sc_state, sc->sc_imess[0],
	    sc->sc_msgpriq, sc->sc_msgoutq, sc->sc_lastmsg, sc->sc_currmsg);
	for (i = 0; i < 7; i++) {
		ti = &sc->sc_tinfo[i];
		printf("tinfo%d: %d cmds %d disconnects %d timeouts",
		    i, ti->cmds, ti->dconns, ti->touts);
		printf(" %d senses flags=%x\n", ti->senses, ti->flags);
	}
}
#endif
