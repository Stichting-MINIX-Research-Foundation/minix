/*	$NetBSD: atapi_wdc.c,v 1.119 2012/07/31 15:59:57 bouyer Exp $	*/

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atapi_wdc.c,v 1.119 2012/07/31 15:59:57 bouyer Exp $");

#ifndef ATADEBUG
#define ATADEBUG
#endif /* ATADEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/dvdio.h>

#include <sys/intr.h>
#include <sys/bus.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_write_multi_stream_4	bus_space_write_multi_4
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#define	bus_space_read_multi_stream_4	bus_space_read_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#include <dev/ata/ataconf.h>
#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include <dev/scsipi/scsi_all.h> /* for SCSI status */

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef ATADEBUG
int wdcdebug_atapi_mask = 0;
#define ATADEBUG_PRINT(args, level) \
	if (wdcdebug_atapi_mask & (level)) \
		printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

#define ATAPI_DELAY 10	/* 10 ms, this is used only before sending a cmd */
#define ATAPI_MODE_DELAY 1000	/* 1s, timeout for SET_FEATYRE cmds */

static int	wdc_atapi_get_params(struct scsipi_channel *, int,
				     struct ataparams *);
static void	wdc_atapi_probe_device(struct atapibus_softc *, int);
static void	wdc_atapi_minphys (struct buf *bp);
static void	wdc_atapi_start(struct ata_channel *,struct ata_xfer *);
static int	wdc_atapi_intr(struct ata_channel *, struct ata_xfer *, int);
static void	wdc_atapi_kill_xfer(struct ata_channel *,
				    struct ata_xfer *, int);
static void	wdc_atapi_phase_complete(struct ata_xfer *);
static void	wdc_atapi_done(struct ata_channel *, struct ata_xfer *);
static void	wdc_atapi_reset(struct ata_channel *, struct ata_xfer *);
static void	wdc_atapi_scsipi_request(struct scsipi_channel *,
					 scsipi_adapter_req_t, void *);
static void	wdc_atapi_kill_pending(struct scsipi_periph *);
static void	wdc_atapi_polldsc(void *arg);

#define MAX_SIZE MAXPHYS

static const struct scsipi_bustype wdc_atapi_bustype = {
	SCSIPI_BUSTYPE_ATAPI,
	atapi_scsipi_cmd,
	atapi_interpret_sense,
	atapi_print_addr,
	wdc_atapi_kill_pending,
	NULL,
};

void
wdc_atapibus_attach(struct atabus_softc *ata_sc)
{
	struct ata_channel *chp = ata_sc->sc_chan;
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_adapter *adapt = &atac->atac_atapi_adapter._generic;
	struct scsipi_channel *chan = &chp->ch_atapi_channel;

	/*
	 * Fill in the scsipi_adapter.
	 */
	adapt->adapt_dev = atac->atac_dev;
	adapt->adapt_nchannels = atac->atac_nchannels;
	adapt->adapt_request = wdc_atapi_scsipi_request;
	adapt->adapt_minphys = wdc_atapi_minphys;
	if (atac->atac_cap & ATAC_CAP_NOIRQ)
		adapt->adapt_flags |= SCSIPI_ADAPT_POLL_ONLY;
	atac->atac_atapi_adapter.atapi_probe_device = wdc_atapi_probe_device;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &wdc_atapi_bustype;
	chan->chan_channel = chp->ch_channel;
	chan->chan_flags = SCSIPI_CHAN_OPENINGS;
	chan->chan_openings = 1;
	chan->chan_max_periph = 1;
	chan->chan_ntargets = 2;
	chan->chan_nluns = 1;

	chp->atapibus = config_found_ia(ata_sc->sc_dev, "atapi", chan,
		atapiprint);
}

static void
wdc_atapi_minphys(struct buf *bp)
{

	if (bp->b_bcount > MAX_SIZE)
		bp->b_bcount = MAX_SIZE;
	minphys(bp);
}

/*
 * Kill off all pending xfers for a periph.
 *
 * Must be called at splbio().
 */
static void
wdc_atapi_kill_pending(struct scsipi_periph *periph)
{
	struct atac_softc *atac =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	struct ata_channel *chp =
	    atac->atac_channels[periph->periph_channel->chan_channel];

	ata_kill_pending(&chp->ch_drive[periph->periph_target]);
}

static void
wdc_atapi_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer, int reason)
{
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;

	/* remove this command from xfer queue */
	switch (reason) {
	case KILL_GONE:
		sc_xfer->error = XS_DRIVER_STUFFUP;
		break;
	case KILL_RESET:
		sc_xfer->error = XS_RESET;
		break;
	default:
		printf("wdc_ata_bio_kill_xfer: unknown reason %d\n",
		    reason);
		panic("wdc_ata_bio_kill_xfer");
	}
	ata_free_xfer(chp, xfer);
	scsipi_done(sc_xfer);
}

static int
wdc_atapi_get_params(struct scsipi_channel *chan, int drive,
    struct ataparams *id)
{
	struct wdc_softc *wdc = device_private(chan->chan_adapter->adapt_dev);
	struct atac_softc *atac = &wdc->sc_atac;
	struct wdc_regs *wdr = &wdc->regs[chan->chan_channel];
	struct ata_channel *chp = atac->atac_channels[chan->chan_channel];
	struct ata_command ata_c;

	memset(&ata_c, 0, sizeof(struct ata_command));
	ata_c.r_command = ATAPI_SOFT_RESET;
	ata_c.r_st_bmask = 0;
	ata_c.r_st_pmask = 0;
	ata_c.flags = AT_WAIT | AT_POLL;
	ata_c.timeout = WDC_RESET_WAIT;
	if (wdc_exec_command(&chp->ch_drive[drive], &ata_c) != ATACMD_COMPLETE) {
		printf("wdc_atapi_get_params: ATAPI_SOFT_RESET failed for"
		    " drive %s:%d:%d: driver failed\n",
		    device_xname(atac->atac_dev), chp->ch_channel, drive);
		panic("wdc_atapi_get_params");
	}
	if (ata_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		ATADEBUG_PRINT(("wdc_atapi_get_params: ATAPI_SOFT_RESET "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    device_xname(atac->atac_dev), chp->ch_channel, drive,
		    ata_c.r_error), DEBUG_PROBE);
		return -1;
	}
	chp->ch_drive[drive].state = 0;

	(void)bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_status], 0);

	/* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);
	if (ata_get_params(&chp->ch_drive[drive], AT_WAIT, id) != 0) {
		ATADEBUG_PRINT(("wdc_atapi_get_params: ATAPI_IDENTIFY_DEVICE "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    device_xname(atac->atac_dev), chp->ch_channel, drive,
		    ata_c.r_error), DEBUG_PROBE);
		return -1;
	}
	return 0;
}

static void
wdc_atapi_probe_device(struct atapibus_softc *sc, int target)
{
	struct scsipi_channel *chan = sc->sc_channel;
	struct scsipi_periph *periph;
	struct ataparams ids;
	struct ataparams *id = &ids;
	struct wdc_softc *wdc = device_private(chan->chan_adapter->adapt_dev);
	struct atac_softc *atac = &wdc->sc_atac;
	struct ata_channel *chp = atac->atac_channels[chan->chan_channel];
	struct ata_drive_datas *drvp = &chp->ch_drive[target];
	struct scsipibus_attach_args sa;
	char serial_number[21], model[41], firmware_revision[9];
	int s;

	/* skip if already attached */
	if (scsipi_lookup_periph(chan, target, 0) != NULL)
		return;

	/* if no ATAPI device detected at wdc attach time, skip */
	if (drvp->drive_type != ATA_DRIVET_ATAPI) {
		ATADEBUG_PRINT(("wdc_atapi_probe_device: "
		    "drive %d not present\n", target), DEBUG_PROBE);
		return;
	}

	if (wdc_atapi_get_params(chan, target, id) == 0) {
#ifdef ATAPI_DEBUG_PROBE
		printf("%s drive %d: cmdsz 0x%x drqtype 0x%x\n",
		    device_xname(sc->sc_dev), target,
		    id->atap_config & ATAPI_CFG_CMD_MASK,
		    id->atap_config & ATAPI_CFG_DRQ_MASK);
#endif
		periph = scsipi_alloc_periph(M_NOWAIT);
		if (periph == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate periph for drive %d\n",
			    target);
			return;
		}
		periph->periph_dev = NULL;
		periph->periph_channel = chan;
		periph->periph_switch = &atapi_probe_periphsw;
		periph->periph_target = target;
		periph->periph_lun = 0;
		periph->periph_quirks = PQUIRK_ONLYBIG;

#ifdef SCSIPI_DEBUG
		if (SCSIPI_DEBUG_TYPE == SCSIPI_BUSTYPE_ATAPI &&
		    SCSIPI_DEBUG_TARGET == target)
			periph->periph_dbflags |= SCSIPI_DEBUG_FLAGS;
#endif
		periph->periph_type = ATAPI_CFG_TYPE(id->atap_config);
		if (id->atap_config & ATAPI_CFG_REMOV)
			periph->periph_flags |= PERIPH_REMOVABLE;
		if (periph->periph_type == T_SEQUENTIAL) {
			s = splbio();
			drvp->drive_flags |= ATA_DRIVE_ATAPIDSCW;
			splx(s);
		}

		sa.sa_periph = periph;
		sa.sa_inqbuf.type =  ATAPI_CFG_TYPE(id->atap_config);
		sa.sa_inqbuf.removable = id->atap_config & ATAPI_CFG_REMOV ?
		    T_REMOV : T_FIXED;
		scsipi_strvis((u_char *)model, 40, id->atap_model, 40);
		scsipi_strvis((u_char *)serial_number, 20, id->atap_serial,
		    20);
		scsipi_strvis((u_char *)firmware_revision, 8,
		    id->atap_revision, 8);
		sa.sa_inqbuf.vendor = model;
		sa.sa_inqbuf.product = serial_number;
		sa.sa_inqbuf.revision = firmware_revision;

		/*
		 * Determine the operating mode capabilities of the device.
		 */
		if ((id->atap_config & ATAPI_CFG_CMD_MASK) == ATAPI_CFG_CMD_16)
			periph->periph_cap |= PERIPH_CAP_CMD16;
		/* XXX This is gross. */
		periph->periph_cap |= (id->atap_config & ATAPI_CFG_DRQ_MASK);

		drvp->drv_softc = atapi_probe_device(sc, target, periph, &sa);

		if (drvp->drv_softc)
			ata_probe_caps(drvp);
		else {
			s = splbio();
			drvp->drive_type = ATA_DRIVET_NONE;
			splx(s);
		}
	} else {
		s = splbio();
		drvp->drive_type = ATA_DRIVET_NONE;
		splx(s);
	}
}

static void
wdc_atapi_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_adapter *adapt = chan->chan_adapter;
	struct scsipi_periph *periph;
	struct scsipi_xfer *sc_xfer;
	struct wdc_softc *wdc = device_private(adapt->adapt_dev);
	struct atac_softc *atac = &wdc->sc_atac;
	struct ata_xfer *xfer;
	int channel = chan->chan_channel;
	int drive, s;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		sc_xfer = arg;
		periph = sc_xfer->xs_periph;
		drive = periph->periph_target;

		ATADEBUG_PRINT(("wdc_atapi_scsipi_request %s:%d:%d\n",
		    device_xname(atac->atac_dev), channel, drive),
		    DEBUG_XFERS);
		if (!device_is_active(atac->atac_dev)) {
			sc_xfer->error = XS_DRIVER_STUFFUP;
			scsipi_done(sc_xfer);
			return;
		}

		xfer = ata_get_xfer(ATAXF_NOSLEEP);
		if (xfer == NULL) {
			sc_xfer->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(sc_xfer);
			return;
		}

		if (sc_xfer->xs_control & XS_CTL_POLL)
			xfer->c_flags |= C_POLL;
#if NATA_DMA
		if ((atac->atac_channels[channel]->ch_drive[drive].drive_flags &
		    (ATA_DRIVE_DMA | ATA_DRIVE_UDMA)) && sc_xfer->datalen > 0)
			xfer->c_flags |= C_DMA;
#endif
#if NATA_DMA && NATA_PIOBM
		else
#endif
#if NATA_PIOBM
		if ((atac->atac_cap & ATAC_CAP_PIOBM) &&
		    sc_xfer->datalen > 0)
			xfer->c_flags |= C_PIOBM;
#endif
		xfer->c_drive = drive;
		xfer->c_flags |= C_ATAPI;
#if NATA_DMA
		if (sc_xfer->cmd->opcode == GPCMD_REPORT_KEY ||
		    sc_xfer->cmd->opcode == GPCMD_SEND_KEY ||
		    sc_xfer->cmd->opcode == GPCMD_READ_DVD_STRUCTURE) {
			/*
			 * DVD authentication commands must always be done in
			 * PIO mode.
			 */
			xfer->c_flags &= ~C_DMA;
		}

		/*
		 * DMA normally can't deal with transfers which are not a
		 * multiple of its databus width. It's a bug to request odd
		 * length transfers for ATAPI.
		 *
		 * Some devices also can't cope with unaligned DMA xfers
		 * either. Also some devices seem to not handle DMA xfers of
		 * less than 4 bytes.
		 *
		 * By enforcing at least 4 byte aligned offset and length for
		 * DMA, we might use PIO where DMA could be allowed but better
		 * safe than sorry as recent problems proved.
		 *
		 * Offending structures that are thus done by PIO instead of
		 * DMA are normally small structures since all bulkdata is
		 * aligned. But as the request may come from userland, we have
		 * to protect against it anyway.
		 *
		 * XXX check for the 32 bit wide flag?
		 */

		if (((uintptr_t) sc_xfer->data) & 0x03)
			xfer->c_flags &= ~C_DMA;
		if ((sc_xfer->datalen < 4) || (sc_xfer->datalen & 0x03))
			xfer->c_flags &= ~C_DMA;
#endif	/* NATA_DMA */

		xfer->c_cmd = sc_xfer;
		xfer->c_databuf = sc_xfer->data;
		xfer->c_bcount = sc_xfer->datalen;
		xfer->c_start = wdc_atapi_start;
		xfer->c_intr = wdc_atapi_intr;
		xfer->c_kill_xfer = wdc_atapi_kill_xfer;
		xfer->c_dscpoll = 0;
		s = splbio();
		ata_exec_xfer(atac->atac_channels[channel], xfer);
#ifdef DIAGNOSTIC
		if ((sc_xfer->xs_control & XS_CTL_POLL) != 0 &&
		    (sc_xfer->xs_status & XS_STS_DONE) == 0)
			panic("wdc_atapi_scsipi_request: polled command "
			    "not done");
#endif
		splx(s);
		return;

	default:
		/* Not supported, nothing to do. */
		;
	}
}

static void
wdc_atapi_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int wait_flags = (sc_xfer->xs_control & XS_CTL_POLL) ? AT_POLL : 0;
	const char *errstring;

	ATADEBUG_PRINT(("wdc_atapi_start %s:%d:%d, scsi flags 0x%x \n",
	    device_xname(atac->atac_dev), chp->ch_channel, drvp->drive,
	    sc_xfer->xs_control), DEBUG_XFERS);
#if NATA_DMA
	if ((xfer->c_flags & C_DMA) && (drvp->n_xfers <= NXFER))
		drvp->n_xfers++;
#endif
	/* Do control operations specially. */
	if (__predict_false(drvp->state < READY)) {
		/* If it's not a polled command, we need the kernel thread */
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0 &&
		    (chp->ch_flags & ATACH_TH_RUN) == 0) {
			chp->ch_queue->queue_freeze++;
			wakeup(&chp->ch_thread);
			return;
		}
		/*
		 * disable interrupts, all commands here should be quick
		 * enough to be able to poll, and we don't go here that often
		 */
		 bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr,
		     WDCTL_4BIT | WDCTL_IDS);
		if (wdc->select)
			wdc->select(chp, xfer->c_drive);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM | (xfer->c_drive << 4));
		/* Don't try to set mode if controller can't be adjusted */
		if (atac->atac_set_modes == NULL)
			goto ready;
		/* Also don't try if the drive didn't report its mode */
		if ((drvp->drive_flags & ATA_DRIVE_MODE) == 0)
			goto ready;
		errstring = "unbusy";
		if (wdc_wait_for_unbusy(chp, ATAPI_DELAY, wait_flags))
			goto timeout;
		wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
		    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
		errstring = "piomode";
		if (wdc_wait_for_unbusy(chp, ATAPI_MODE_DELAY, wait_flags))
			goto timeout;
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error == WDCE_ABRT) {
				/*
				 * Some ATAPI drives reject PIO settings.
				 * Fall back to PIO mode 3 since that's the
				 * minimum for ATAPI.
				 */
				printf("%s:%d:%d: PIO mode %d rejected, "
				    "falling back to PIO mode 3\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, xfer->c_drive,
				    drvp->PIO_mode);
				if (drvp->PIO_mode > 3)
					drvp->PIO_mode = 3;
			} else
				goto error;
		}
#if NATA_DMA
#if NATA_UDMA
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x40 | drvp->UDMA_mode, WDSF_SET_MODE);
		} else
#endif
		if (drvp->drive_flags & ATA_DRIVE_DMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x20 | drvp->DMA_mode, WDSF_SET_MODE);
		} else {
			goto ready;
		}
		errstring = "dmamode";
		if (wdc_wait_for_unbusy(chp, ATAPI_MODE_DELAY, wait_flags))
			goto timeout;
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error == WDCE_ABRT) {
#if NATA_UDMA
				if (drvp->drive_flags & ATA_DRIVE_UDMA)
					goto error;
				else
#endif
				{
					/*
					 * The drive rejected our DMA setting.
					 * Fall back to mode 1.
					 */
					printf("%s:%d:%d: DMA mode %d rejected, "
					    "falling back to DMA mode 0\n",
					    device_xname(atac->atac_dev),
					    chp->ch_channel, xfer->c_drive,
					    drvp->DMA_mode);
					if (drvp->DMA_mode > 0)
						drvp->DMA_mode = 0;
				}
			} else
				goto error;
		}
#endif	/* NATA_DMA */
ready:
		drvp->state = READY;
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr,
		    WDCTL_4BIT);
		delay(10); /* some drives need a little delay here */
	}
	/* start timeout machinery */
	if ((sc_xfer->xs_control & XS_CTL_POLL) == 0)
		callout_reset(&chp->ch_callout, mstohz(sc_xfer->timeout),
		    wdctimeout, chp);

	if (wdc->select)
		wdc->select(chp, xfer->c_drive);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    WDSD_IBM | (xfer->c_drive << 4));
	switch (wdc_wait_for_unbusy(chp, ATAPI_DELAY, wait_flags)) {
	case WDCWAIT_OK:
		break;
	case WDCWAIT_TOUT:
		printf("wdc_atapi_start: not ready, st = %02x\n",
		    chp->ch_status);
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return;
	case WDCWAIT_THR:
		return;
	}

	/*
	 * Even with WDCS_ERR, the device should accept a command packet
	 * Limit length to what can be stuffed into the cylinder register
	 * (16 bits).  Some CD-ROMs seem to interpret '0' as 65536,
	 * but not all devices do that and it's not obvious from the
	 * ATAPI spec that that behaviour should be expected.  If more
	 * data is necessary, multiple data transfer phases will be done.
	 */

	wdccommand(chp, xfer->c_drive, ATAPI_PKT_CMD,
	    xfer->c_bcount <= 0xffff ? xfer->c_bcount : 0xffff,
	    0, 0, 0,
#if NATA_DMA
	    (xfer->c_flags & C_DMA) ? ATAPI_PKT_CMD_FTRE_DMA :
#endif
	    0
	    );

#if NATA_PIOBM
	if (xfer->c_flags & C_PIOBM) {
		int error;
		int dma_flags = (sc_xfer->xs_control & XS_CTL_DATA_IN)
		    ?  WDC_DMA_READ : 0;
		if (xfer->c_flags & C_POLL) {
			/* XXX not supported yet --- fall back to PIO */
			xfer->c_flags &= ~C_PIOBM;
		} else {
			/* Init the DMA channel. */
			error = (*wdc->dma_init)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    (char *)xfer->c_databuf,
			    xfer->c_bcount,
			    dma_flags | WDC_DMA_PIOBM_ATAPI);
			if (error) {
				if (error == EINVAL) {
					/*
					 * We can't do DMA on this transfer
					 * for some reason.  Fall back to
					 * PIO.
					 */
					xfer->c_flags &= ~C_PIOBM;
					error = 0;
				} else {
					sc_xfer->error = XS_DRIVER_STUFFUP;
					errstring = "piobm";
					goto error;
				}
			}
		}
	}
#endif
	/*
	 * If there is no interrupt for CMD input, busy-wait for it (done in
	 * the interrupt routine. If it is a polled command, call the interrupt
	 * routine until command is done.
	 */
	if ((sc_xfer->xs_periph->periph_cap & ATAPI_CFG_DRQ_MASK) !=
	    ATAPI_CFG_IRQ_DRQ || (sc_xfer->xs_control & XS_CTL_POLL)) {
		/* Wait for at last 400ns for status bit to be valid */
		DELAY(1);
		wdc_atapi_intr(chp, xfer, 0);
	} else {
		chp->ch_flags |= ATACH_IRQ_WAIT;
	}
	if (sc_xfer->xs_control & XS_CTL_POLL) {
#if NATA_DMA
		if (chp->ch_flags & ATACH_DMA_WAIT) {
			wdc_dmawait(chp, xfer, sc_xfer->timeout);
			chp->ch_flags &= ~ATACH_DMA_WAIT;
		}
#endif
		while ((sc_xfer->xs_status & XS_STS_DONE) == 0) {
			/* Wait for at last 400ns for status bit to be valid */
			DELAY(1);
			wdc_atapi_intr(chp, xfer, 0);
		}
	}
	return;
timeout:
	printf("%s:%d:%d: %s timed out\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    errstring);
	sc_xfer->error = XS_TIMEOUT;
	bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr, WDCTL_4BIT);
	delay(10); /* some drives need a little delay here */
	wdc_atapi_reset(chp, xfer);
	return;
error:
	printf("%s:%d:%d: %s ",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    errstring);
	printf("error (0x%x)\n", chp->ch_error);
	sc_xfer->error = XS_SHORTSENSE;
	sc_xfer->sense.atapi_sense = chp->ch_error;
	bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr, WDCTL_4BIT);
	delay(10); /* some drives need a little delay here */
	wdc_atapi_reset(chp, xfer);
	return;
}

static int
wdc_atapi_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int len, phase, i, retries=0;
	int ire;
#if NATA_DMA
	int error;
#endif
#if NATA_DMA || NATA_PIOBM
	int dma_flags = 0;
#endif
	void *cmd;

	ATADEBUG_PRINT(("wdc_atapi_intr %s:%d:%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, drvp->drive),
	    DEBUG_INTR);

	/* Is it not a transfer, but a control operation? */
	if (drvp->state < READY) {
		printf("%s:%d:%d: bad state %d in wdc_atapi_intr\n",
		    device_xname(atac->atac_dev), chp->ch_channel,
		    xfer->c_drive, drvp->state);
		panic("wdc_atapi_intr: bad state");
	}
	/*
	 * If we missed an interrupt in a PIO transfer, reset and restart.
	 * Don't try to continue transfer, we may have missed cycles.
	 */
	if ((xfer->c_flags & (C_TIMEOU | C_DMA)) == C_TIMEOU) {
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return 1;
	}

#if NATA_PIOBM
	/* Transfer-done interrupt for busmastering PIO operation */
	if ((xfer->c_flags & C_PIOBM) && (chp->ch_flags & ATACH_PIOBM_WAIT)) {
		chp->ch_flags &= ~ATACH_PIOBM_WAIT;

		/* restore transfer length */
		len = xfer->c_bcount;
		if (xfer->c_lenoff < 0)
			len += xfer->c_lenoff;

		if (sc_xfer->xs_control & XS_CTL_DATA_IN)
			goto end_piobm_datain;
		else
			goto end_piobm_dataout;
	}
#endif

	/* Ack interrupt done in wdc_wait_for_unbusy */
	if (wdc->select)
		wdc->select(chp, xfer->c_drive);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    WDSD_IBM | (xfer->c_drive << 4));
	if (wdc_wait_for_unbusy(chp,
	    (irq == 0) ? sc_xfer->timeout : 0, AT_POLL) == WDCWAIT_TOUT) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0; /* IRQ was not for us */
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip=%d\n",
		    device_xname(atac->atac_dev), chp->ch_channel,
		    xfer->c_drive, xfer->c_bcount, xfer->c_skip);
#if NATA_DMA
		if (xfer->c_flags & C_DMA) {
			ata_dmaerr(drvp,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
		}
#endif
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return 1;
	}
	if (wdc->irqack)
		wdc->irqack(chp);

#if NATA_DMA
	/*
	 * If we missed an IRQ and were using DMA, flag it as a DMA error
	 * and reset device.
	 */
	if ((xfer->c_flags & C_TIMEOU) && (xfer->c_flags & C_DMA)) {
		ata_dmaerr(drvp, (xfer->c_flags & C_POLL) ? AT_POLL : 0);
		sc_xfer->error = XS_RESET;
		wdc_atapi_reset(chp, xfer);
		return (1);
	}
#endif
	/*
	 * if the request sense command was aborted, report the short sense
	 * previously recorded, else continue normal processing
	 */

#if NATA_DMA || NATA_PIOBM
	if (xfer->c_flags & (C_DMA | C_PIOBM))
		dma_flags = (sc_xfer->xs_control & XS_CTL_DATA_IN)
		    ?  WDC_DMA_READ : 0;
#endif
again:
	len = bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_cyl_lo], 0) +
	    256 * bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_cyl_hi], 0);
	ire = bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_ireason], 0);
	phase = (ire & (WDCI_CMD | WDCI_IN)) | (chp->ch_status & WDCS_DRQ);
	ATADEBUG_PRINT(("wdc_atapi_intr: c_bcount %d len %d st 0x%x err 0x%x "
	    "ire 0x%x :", xfer->c_bcount,
	    len, chp->ch_status, chp->ch_error, ire), DEBUG_INTR);

	switch (phase) {
	case PHASE_CMDOUT:
		cmd = sc_xfer->cmd;
		ATADEBUG_PRINT(("PHASE_CMDOUT\n"), DEBUG_INTR);
#if NATA_DMA
		/* Init the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			error = (*wdc->dma_init)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    xfer->c_databuf, xfer->c_bcount, dma_flags);
			if (error) {
				if (error == EINVAL) {
					/*
					 * We can't do DMA on this transfer
					 * for some reason.  Fall back to
					 * PIO.
					 */
					xfer->c_flags &= ~C_DMA;
					error = 0;
				} else {
					sc_xfer->error = XS_DRIVER_STUFFUP;
					break;
				}
			}
		}
#endif

		/* send packet command */
		/* Commands are 12 or 16 bytes long. It's 32-bit aligned */
		wdc->dataout_pio(chp, drvp->drive_flags, cmd, sc_xfer->cmdlen);

#if NATA_DMA
		/* Start the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			(*wdc->dma_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive);
			chp->ch_flags |= ATACH_DMA_WAIT;
		}
#endif

		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0) {
			chp->ch_flags |= ATACH_IRQ_WAIT;
		}
		return 1;

	 case PHASE_DATAOUT:
		/* write data */
		ATADEBUG_PRINT(("PHASE_DATAOUT\n"), DEBUG_INTR);
#if NATA_DMA
		if ((sc_xfer->xs_control & XS_CTL_DATA_OUT) == 0 ||
		    (xfer->c_flags & C_DMA) != 0) {
			printf("wdc_atapi_intr: bad data phase DATAOUT\n");
			if (xfer->c_flags & C_DMA) {
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			}
			sc_xfer->error = XS_TIMEOUT;
			wdc_atapi_reset(chp, xfer);
			return 1;
		}
#endif
		xfer->c_lenoff = len - xfer->c_bcount;
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: write only "
			    "%d of %d requested bytes\n", xfer->c_bcount, len);
			len = xfer->c_bcount;
		}

#if NATA_PIOBM
		if (xfer->c_flags & C_PIOBM) {
			/* start the busmastering PIO */
			(*wdc->piobm_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    xfer->c_skip, len, WDC_PIOBM_XFER_IRQ);
			chp->ch_flags |= ATACH_DMA_WAIT | ATACH_IRQ_WAIT |
			    ATACH_PIOBM_WAIT;
			return 1;
		}
#endif
		wdc->dataout_pio(chp, drvp->drive_flags,
	    	    (char *)xfer->c_databuf + xfer->c_skip, len);

#if NATA_PIOBM
	end_piobm_dataout:
#endif
		for (i = xfer->c_lenoff; i > 0; i -= 2)
			bus_space_write_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, 0);

		xfer->c_skip += len;
		xfer->c_bcount -= len;
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0) {
			chp->ch_flags |= ATACH_IRQ_WAIT;
		}
		return 1;

	case PHASE_DATAIN:
		/* Read data */
		ATADEBUG_PRINT(("PHASE_DATAIN\n"), DEBUG_INTR);
#if NATA_DMA
		if ((sc_xfer->xs_control & XS_CTL_DATA_IN) == 0 ||
		    (xfer->c_flags & C_DMA) != 0) {
			printf("wdc_atapi_intr: bad data phase DATAIN\n");
			if (xfer->c_flags & C_DMA) {
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			}
			sc_xfer->error = XS_TIMEOUT;
			wdc_atapi_reset(chp, xfer);
			return 1;
		}
#endif
		xfer->c_lenoff = len - xfer->c_bcount;
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: reading only "
			    "%d of %d bytes\n", xfer->c_bcount, len);
			len = xfer->c_bcount;
		}

#if NATA_PIOBM
		if (xfer->c_flags & C_PIOBM) {
			/* start the busmastering PIO */
			(*wdc->piobm_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    xfer->c_skip, len, WDC_PIOBM_XFER_IRQ);
			chp->ch_flags |= ATACH_DMA_WAIT | ATACH_IRQ_WAIT |
			    ATACH_PIOBM_WAIT;
			return 1;
		}
#endif
		wdc->datain_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, len);

#if NATA_PIOBM
	end_piobm_datain:
#endif
		if (xfer->c_lenoff > 0)
			wdcbit_bucket(chp, len - xfer->c_bcount);

		xfer->c_skip += len;
		xfer->c_bcount -= len;
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0) {
			chp->ch_flags |= ATACH_IRQ_WAIT;
		}
		return 1;

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
		ATADEBUG_PRINT(("PHASE_COMPLETED\n"), DEBUG_INTR);
#if NATA_DMA
		if (xfer->c_flags & C_DMA) {
			xfer->c_bcount -= sc_xfer->datalen;
		}
#endif
		sc_xfer->resid = xfer->c_bcount;
		wdc_atapi_phase_complete(xfer);
		return(1);

	default:
		if (++retries<500) {
			DELAY(100);
			chp->ch_status = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
			chp->ch_error = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_error], 0);
			goto again;
		}
		printf("wdc_atapi_intr: unknown phase 0x%x\n", phase);
		if (chp->ch_status & WDCS_ERR) {
			sc_xfer->error = XS_SHORTSENSE;
			sc_xfer->sense.atapi_sense = chp->ch_error;
		} else {
#if NATA_DMA
			if (xfer->c_flags & C_DMA) {
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			}
#endif
			sc_xfer->error = XS_RESET;
			wdc_atapi_reset(chp, xfer);
			return (1);
		}
	}
	ATADEBUG_PRINT(("wdc_atapi_intr: wdc_atapi_done() (end), error 0x%x "
	    "sense 0x%x\n", sc_xfer->error, sc_xfer->sense.atapi_sense),
	    DEBUG_INTR);
	wdc_atapi_done(chp, xfer);
	return (1);
}

static void
wdc_atapi_phase_complete(struct ata_xfer *xfer)
{
	struct ata_channel *chp = xfer->c_chp;
	struct atac_softc *atac = chp->ch_atac;
#if NATA_DMA || NATA_PIOBM
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
#endif
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];

	/* wait for DSC if needed */
	if (drvp->drive_flags & ATA_DRIVE_ATAPIDSCW) {
		ATADEBUG_PRINT(("wdc_atapi_phase_complete(%s:%d:%d) "
		    "polldsc %d\n", device_xname(atac->atac_dev),
		    chp->ch_channel,
		    xfer->c_drive, xfer->c_dscpoll), DEBUG_XFERS);
#if 1
		if (cold)
			panic("wdc_atapi_phase_complete: cold");
#endif
		if (wdcwait(chp, WDCS_DSC, WDCS_DSC, 10,
		    AT_POLL) == WDCWAIT_TOUT) {
			/* 10ms not enough, try again in 1 tick */
			if (xfer->c_dscpoll++ >
			    mstohz(sc_xfer->timeout)) {
				printf("%s:%d:%d: wait_for_dsc "
				    "failed\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, xfer->c_drive);
				sc_xfer->error = XS_TIMEOUT;
				wdc_atapi_reset(chp, xfer);
				return;
			} else
				callout_reset(&chp->ch_callout, 1,
				    wdc_atapi_polldsc, xfer);
			return;
		}
	}

	/*
	 * Some drive occasionally set WDCS_ERR with
	 * "ATA illegal length indication" in the error
	 * register. If we read some data the sense is valid
	 * anyway, so don't report the error.
	 */
	if (chp->ch_status & WDCS_ERR &&
	    ((sc_xfer->xs_control & XS_CTL_REQSENSE) == 0 ||
	    sc_xfer->resid == sc_xfer->datalen)) {
		/* save the short sense */
		sc_xfer->error = XS_SHORTSENSE;
		sc_xfer->sense.atapi_sense = chp->ch_error;
		if ((sc_xfer->xs_periph->periph_quirks &
		    PQUIRK_NOSENSE) == 0) {
			/* ask scsipi to send a REQUEST_SENSE */
			sc_xfer->error = XS_BUSY;
			sc_xfer->status = SCSI_CHECK;
		}
#if NATA_DMA || NATA_PIOBM
		else if (wdc->dma_status &
		    (WDC_DMAST_NOIRQ | WDC_DMAST_ERR)) {
#if NATA_DMA
			ata_dmaerr(drvp,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
#endif
			sc_xfer->error = XS_RESET;
			wdc_atapi_reset(chp, xfer);
			return;
		}
#endif
	}
	if (xfer->c_bcount != 0) {
		ATADEBUG_PRINT(("wdc_atapi_intr: bcount value is "
		    "%d after io\n", xfer->c_bcount), DEBUG_XFERS);
	}
#ifdef DIAGNOSTIC
	if (xfer->c_bcount < 0) {
		printf("wdc_atapi_intr warning: bcount value "
		    "is %d after io\n", xfer->c_bcount);
	}
#endif
	ATADEBUG_PRINT(("wdc_atapi_phase_complete: wdc_atapi_done(), "
	    "error 0x%x sense 0x%x\n", sc_xfer->error,
	    sc_xfer->sense.atapi_sense), DEBUG_INTR);
	wdc_atapi_done(chp, xfer);
}

static void
wdc_atapi_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	int drive = xfer->c_drive;

	ATADEBUG_PRINT(("wdc_atapi_done %s:%d:%d: flags 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    (u_int)xfer->c_flags), DEBUG_XFERS);
	callout_stop(&chp->ch_callout);
	/* mark controller inactive and free the command */
	chp->ch_queue->active_xfer = NULL;
	ata_free_xfer(chp, xfer);

	if (chp->ch_drive[drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		sc_xfer->error = XS_DRIVER_STUFFUP;
		chp->ch_drive[drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	}

	ATADEBUG_PRINT(("wdc_atapi_done: scsipi_done\n"), DEBUG_XFERS);
	scsipi_done(sc_xfer);
	ATADEBUG_PRINT(("atastart from wdc_atapi_done, flags 0x%x\n",
	    chp->ch_flags), DEBUG_XFERS);
	atastart(chp);
}

static void
wdc_atapi_reset(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;

	wdccommandshort(chp, xfer->c_drive, ATAPI_SOFT_RESET);
	drvp->state = 0;
	if (wdc_wait_for_unbusy(chp, WDC_RESET_WAIT, AT_POLL) != 0) {
		printf("%s:%d:%d: reset failed\n",
		    device_xname(atac->atac_dev), chp->ch_channel,
		    xfer->c_drive);
		sc_xfer->error = XS_SELTIMEOUT;
	}
	wdc_atapi_done(chp, xfer);
	return;
}

static void
wdc_atapi_polldsc(void *arg)
{

	wdc_atapi_phase_complete(arg);
}
