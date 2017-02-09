/*	$NetBSD: ata_wdc.c,v 1.105 2015/01/02 19:42:06 christos Exp $	*/

/*
 * Copyright (c) 1998, 2001, 2003 Manuel Bouyer.
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

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_wdc.c,v 1.105 2015/01/02 19:42:06 christos Exp $");

#include "opt_ata.h"
#include "opt_wdc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <sys/intr.h>
#include <sys/bus.h>
#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define    bus_space_write_multi_stream_2    bus_space_write_multi_2
#define    bus_space_write_multi_stream_4    bus_space_write_multi_4
#define    bus_space_read_multi_stream_2    bus_space_read_multi_2
#define    bus_space_read_multi_stream_4    bus_space_read_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#include <dev/ata/ataconf.h>
#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef ATADEBUG
extern int wdcdebug_wd_mask; /* inited in wd.c */
#define ATADEBUG_PRINT(args, level) \
	if (wdcdebug_wd_mask & (level)) \
		printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

#define ATA_DELAY 10000 /* 10s for a drive I/O */

static int	wdc_ata_bio(struct ata_drive_datas*, struct ata_bio*);
static void	wdc_ata_bio_start(struct ata_channel *,struct ata_xfer *);
static void	_wdc_ata_bio_start(struct ata_channel *,struct ata_xfer *);
static int	wdc_ata_bio_intr(struct ata_channel *, struct ata_xfer *,
				 int);
static void	wdc_ata_bio_kill_xfer(struct ata_channel *,
				      struct ata_xfer *, int);
static void	wdc_ata_bio_done(struct ata_channel *, struct ata_xfer *);
static int	wdc_ata_err(struct ata_drive_datas *, struct ata_bio *);
#define WDC_ATA_NOERR 0x00 /* Drive doesn't report an error */
#define WDC_ATA_RECOV 0x01 /* There was a recovered error */
#define WDC_ATA_ERR   0x02 /* Drive reports an error */
static int	wdc_ata_addref(struct ata_drive_datas *);
static void	wdc_ata_delref(struct ata_drive_datas *);

const struct ata_bustype wdc_ata_bustype = {
	SCSIPI_BUSTYPE_ATA,
	wdc_ata_bio,
	wdc_reset_drive,
	wdc_reset_channel,
	wdc_exec_command,
	ata_get_params,
	wdc_ata_addref,
	wdc_ata_delref,
	ata_kill_pending,
};

/*
 * Handle block I/O operation. Return ATACMD_COMPLETE, ATACMD_QUEUED, or
 * ATACMD_TRY_AGAIN. Must be called at splbio().
 */
static int
wdc_ata_bio(struct ata_drive_datas *drvp, struct ata_bio *ata_bio)
{
	struct ata_xfer *xfer;
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;

	xfer = ata_get_xfer(ATAXF_NOSLEEP);
	if (xfer == NULL)
		return ATACMD_TRY_AGAIN;
	if (atac->atac_cap & ATAC_CAP_NOIRQ)
		ata_bio->flags |= ATA_POLL;
	if (ata_bio->flags & ATA_POLL)
		xfer->c_flags |= C_POLL;
#if NATA_DMA
	if ((drvp->drive_flags & (ATA_DRIVE_DMA | ATA_DRIVE_UDMA)) &&
	    (ata_bio->flags & ATA_SINGLE) == 0)
		xfer->c_flags |= C_DMA;
#endif
#if NATA_DMA && NATA_PIOBM
	else
#endif
#if NATA_PIOBM
	if (atac->atac_cap & ATAC_CAP_PIOBM)
		xfer->c_flags |= C_PIOBM;
#endif
	xfer->c_drive = drvp->drive;
	xfer->c_cmd = ata_bio;
	xfer->c_databuf = ata_bio->databuf;
	xfer->c_bcount = ata_bio->bcount;
	xfer->c_start = wdc_ata_bio_start;
	xfer->c_intr = wdc_ata_bio_intr;
	xfer->c_kill_xfer = wdc_ata_bio_kill_xfer;
	ata_exec_xfer(chp, xfer);
	return (ata_bio->flags & ATA_ITSDONE) ? ATACMD_COMPLETE : ATACMD_QUEUED;
}

static void
wdc_ata_bio_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct ata_bio *ata_bio = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int wait_flags;
	const char *errstring;
#ifdef WDC_NO_IDS
	wait_flags = AT_POLL;
#else
	wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
#endif

	ATADEBUG_PRINT(("wdc_ata_bio_start %s:%d:%d state %d drive_flags 0x%x "
	    "c_flags 0x%x ch_flags 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    drvp->state, drvp->drive_flags, xfer->c_flags, chp->ch_flags),
	    DEBUG_XFERS);

	/* Do control operations specially. */
	if (__predict_false(drvp->state < READY)) {
		/*
		 * Actually, we want to be careful not to mess with the control
		 * state if the device is currently busy, but we can assume
		 * that we never get to this point if that's the case.
		 */
		/* If it's not a polled command, we need the kernel thread */
		if ((xfer->c_flags & C_POLL) == 0 &&
		    (chp->ch_flags & ATACH_TH_RUN) == 0) {
			chp->ch_queue->queue_freeze++;
			wakeup(&chp->ch_thread);
			return;
		}
		/*
		 * disable interrupts, all commands here should be quick
		 * enough to be able to poll, and we don't go here that often
		 */
		if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL))
			bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, 
			    wd_aux_ctlr, WDCTL_4BIT | WDCTL_IDS);
		if (wdc->select)
			wdc->select(chp, xfer->c_drive);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM | (xfer->c_drive << 4));
		DELAY(10);
		errstring = "wait";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		wdccommandshort(chp, xfer->c_drive, WDCC_RECAL);
		/* Wait for at last 400ns for status bit to be valid */
		DELAY(1);
		errstring = "recal";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			goto ctrlerror;
		/* Don't try to set modes if controller can't be adjusted */
		if (atac->atac_set_modes == NULL)
			goto geometry;
		/* Also don't try if the drive didn't report its mode */
		if ((drvp->drive_flags & ATA_DRIVE_MODE) == 0)
			goto geometry;
		wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
		    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
		errstring = "piomode";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			goto ctrlerror;
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
			goto geometry;
		}
		errstring = "dmamode";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			goto ctrlerror;
#endif	/* NATA_DMA */
geometry:
		if (ata_bio->flags & ATA_LBA)
			goto multimode;
		wdccommand(chp, xfer->c_drive, WDCC_IDP,
		    ata_bio->lp->d_ncylinders,
		    ata_bio->lp->d_ntracks - 1, 0, ata_bio->lp->d_nsectors,
		    (ata_bio->lp->d_type == DKTYPE_ST506) ?
			ata_bio->lp->d_precompcyl / 4 : 0);
		errstring = "geometry";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			goto ctrlerror;
multimode:
		if (ata_bio->multi == 1)
			goto ready;
		wdccommand(chp, xfer->c_drive, WDCC_SETMULTI, 0, 0, 0,
		    ata_bio->multi, 0);
		errstring = "setmulti";
		if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, wait_flags))
			goto ctrltimeout;
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			goto ctrlerror;
ready:
		drvp->state = READY;
		/*
		 * The drive is usable now
		 */
		if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL))
			bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, 
			    wd_aux_ctlr, WDCTL_4BIT);
		delay(10); /* some drives need a little delay here */
	}

	_wdc_ata_bio_start(chp, xfer);
	return;
ctrltimeout:
	printf("%s:%d:%d: %s timed out\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    errstring);
	ata_bio->error = TIMEOUT;
	goto ctrldone;
ctrlerror:
	printf("%s:%d:%d: %s ",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    errstring);
	if (chp->ch_status & WDCS_DWF) {
		printf("drive fault\n");
		ata_bio->error = ERR_DF;
	} else {
		printf("error (%x)\n", chp->ch_error);
		ata_bio->r_error = chp->ch_error;
		ata_bio->error = ERROR;
	}
ctrldone:
	drvp->state = 0;
	wdc_ata_bio_done(chp, xfer);

	if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL))
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr, 
		    WDCTL_4BIT);
	return;
}

static void
_wdc_ata_bio_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct ata_bio *ata_bio = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
	uint16_t cyl;
	uint8_t head, sect, cmd = 0;
	int nblks;
#if NATA_DMA || NATA_PIOBM
	int error, dma_flags = 0;
#endif

	ATADEBUG_PRINT(("_wdc_ata_bio_start %s:%d:%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive),
	    DEBUG_INTR | DEBUG_XFERS);

#if NATA_DMA || NATA_PIOBM
	if (xfer->c_flags & (C_DMA | C_PIOBM)) {
#if NATA_DMA
		if (drvp->n_xfers <= NXFER)
			drvp->n_xfers++;
#endif
		dma_flags = (ata_bio->flags & ATA_READ) ?  WDC_DMA_READ : 0;
		if (ata_bio->flags & ATA_LBA48)
			dma_flags |= WDC_DMA_LBA48;
	}
#endif
again:
	/*
	 *
	 * When starting a multi-sector transfer, or doing single-sector
	 * transfers...
	 */
	if (xfer->c_skip == 0 || (ata_bio->flags & ATA_SINGLE) != 0) {
		if (ata_bio->flags & ATA_SINGLE)
			nblks = 1;
		else
			nblks = xfer->c_bcount / ata_bio->lp->d_secsize;
		/* Check for bad sectors and adjust transfer, if necessary. */
		if ((ata_bio->lp->d_flags & D_BADSECT) != 0) {
			long blkdiff;
			int i;
			for (i = 0; (blkdiff = ata_bio->badsect[i]) != -1;
			    i++) {
				blkdiff -= ata_bio->blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of transfer. */
					ata_bio->blkno =
					    ata_bio->lp->d_secperunit -
					    ata_bio->lp->d_nsectors - i - 1;
				}
				if (blkdiff < nblks) {
					/* Bad block inside transfer. */
					ata_bio->flags |= ATA_SINGLE;
					nblks = 1;
				}
				break;
			}
		/* Transfer is okay now. */
		}
		if (ata_bio->flags & ATA_LBA48) {
			sect = 0;
			cyl =  0;
			head = 0;
		} else if (ata_bio->flags & ATA_LBA) {
			sect = (ata_bio->blkno >> 0) & 0xff;
			cyl = (ata_bio->blkno >> 8) & 0xffff;
			head = (ata_bio->blkno >> 24) & 0x0f;
			head |= WDSD_LBA;
		} else {
			int blkno = ata_bio->blkno;
			sect = blkno % ata_bio->lp->d_nsectors;
			sect++;    /* Sectors begin with 1, not 0. */
			blkno /= ata_bio->lp->d_nsectors;
			head = blkno % ata_bio->lp->d_ntracks;
			blkno /= ata_bio->lp->d_ntracks;
			cyl = blkno;
			head |= WDSD_CHS;
		}
#if NATA_DMA
		if (xfer->c_flags & C_DMA) {
			ata_bio->nblks = nblks;
			ata_bio->nbytes = xfer->c_bcount;
			cmd = (ata_bio->flags & ATA_READ) ?
			    WDCC_READDMA : WDCC_WRITEDMA;
	    		/* Init the DMA channel. */
			error = (*wdc->dma_init)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    (char *)xfer->c_databuf + xfer->c_skip,
			    ata_bio->nbytes, dma_flags);
			if (error) {
				if (error == EINVAL) {
					/*
					 * We can't do DMA on this transfer
					 * for some reason.  Fall back to
					 * PIO.
					 */
					xfer->c_flags &= ~C_DMA;
					error = 0;
					goto do_pio;
				}
				ata_bio->error = ERR_DMA;
				ata_bio->r_error = 0;
				wdc_ata_bio_done(chp, xfer);
				return;
			}
			/* Initiate command */
			if (wdc->select)
				wdc->select(chp, xfer->c_drive);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM | (xfer->c_drive << 4));
			switch(wdc_wait_for_ready(chp, ATA_DELAY, wait_flags)) {
			case WDCWAIT_OK:
				break;
			case WDCWAIT_TOUT:
				goto timeout;
			case WDCWAIT_THR:
				return;
			}
			if (ata_bio->flags & ATA_LBA48) {
			    wdccommandext(chp, xfer->c_drive, atacmd_to48(cmd),
				ata_bio->blkno, nblks, 0, WDSD_LBA);
			} else {
			    wdccommand(chp, xfer->c_drive, cmd, cyl,
				head, sect, nblks, 0);
			}
			/* start the DMA channel */
			(*wdc->dma_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive);
			chp->ch_flags |= ATACH_DMA_WAIT;
			/* start timeout machinery */
			if ((xfer->c_flags & C_POLL) == 0)
				callout_reset(&chp->ch_callout,
				    ATA_DELAY / 1000 * hz, wdctimeout, chp);
			/* wait for irq */
			goto intr;
		} /* else not DMA */
 do_pio:
#endif	/* NATA_DMA */
#if NATA_PIOBM
		if ((xfer->c_flags & C_PIOBM) && xfer->c_skip == 0) {
			if (ata_bio->flags & ATA_POLL) {
				/* XXX not supported yet --- fall back to PIO */
				xfer->c_flags &= ~C_PIOBM;
			} else {
				/* Init the DMA channel. */
				error = (*wdc->dma_init)(wdc->dma_arg,
				    chp->ch_channel, xfer->c_drive,
				    (char *)xfer->c_databuf + xfer->c_skip,
				    xfer->c_bcount,
				    dma_flags | WDC_DMA_PIOBM_ATA);
				if (error) {
					if (error == EINVAL) {
						/*
						 * We can't do DMA on this
						 * transfer for some reason.
						 * Fall back to PIO.
						 */
						xfer->c_flags &= ~C_PIOBM;
						error = 0;
					} else {
						ata_bio->error = ERR_DMA;
						ata_bio->r_error = 0;
						wdc_ata_bio_done(chp, xfer);
						return;
					}
				}
			}
		}
#endif
		ata_bio->nblks = min(nblks, ata_bio->multi);
		ata_bio->nbytes = ata_bio->nblks * ata_bio->lp->d_secsize;
		KASSERT(nblks == 1 || (ata_bio->flags & ATA_SINGLE) == 0);
		if (ata_bio->nblks > 1) {
			cmd = (ata_bio->flags & ATA_READ) ?
			    WDCC_READMULTI : WDCC_WRITEMULTI;
		} else {
			cmd = (ata_bio->flags & ATA_READ) ?
			    WDCC_READ : WDCC_WRITE;
		}
		/* Initiate command! */
		if (wdc->select)
			wdc->select(chp, xfer->c_drive);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM | (xfer->c_drive << 4));
		switch(wdc_wait_for_ready(chp, ATA_DELAY, wait_flags)) {
		case WDCWAIT_OK:
			break;
		case WDCWAIT_TOUT:
			goto timeout;
		case WDCWAIT_THR:
			return;
		}
		if (ata_bio->flags & ATA_LBA48) {
		    wdccommandext(chp, xfer->c_drive, atacmd_to48(cmd),
			ata_bio->blkno, nblks, 0, WDSD_LBA);
		} else {
		    wdccommand(chp, xfer->c_drive, cmd, cyl,
			head, sect, nblks,
			(ata_bio->lp->d_type == DKTYPE_ST506) ?
			ata_bio->lp->d_precompcyl / 4 : 0);
		}
		/* start timeout machinery */
		if ((xfer->c_flags & C_POLL) == 0)
			callout_reset(&chp->ch_callout,
			    ATA_DELAY / 1000 * hz, wdctimeout, chp);
	} else if (ata_bio->nblks > 1) {
		/* The number of blocks in the last stretch may be smaller. */
		nblks = xfer->c_bcount / ata_bio->lp->d_secsize;
		if (ata_bio->nblks > nblks) {
		ata_bio->nblks = nblks;
		ata_bio->nbytes = xfer->c_bcount;
		}
	}
	/* If this was a write and not using DMA, push the data. */
	if ((ata_bio->flags & ATA_READ) == 0) {
		/*
		 * we have to busy-wait here, we can't rely on running in
		 * thread context.
		 */
		if (wdc_wait_for_drq(chp, ATA_DELAY, AT_POLL) != 0) {
			printf("%s:%d:%d: timeout waiting for DRQ, "
			    "st=0x%02x, err=0x%02x\n",
			    device_xname(atac->atac_dev), chp->ch_channel,
			    xfer->c_drive, chp->ch_status, chp->ch_error);
			if (wdc_ata_err(drvp, ata_bio) != WDC_ATA_ERR)
				ata_bio->error = TIMEOUT;
			wdc_ata_bio_done(chp, xfer);
			return;
		}
		if (wdc_ata_err(drvp, ata_bio) == WDC_ATA_ERR) {
			wdc_ata_bio_done(chp, xfer);
			return;
		}
#if NATA_PIOBM
		if (xfer->c_flags & C_PIOBM) {
			/* start the busmastering PIO */
			(*wdc->piobm_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    xfer->c_skip, ata_bio->nbytes, 0);
			chp->ch_flags |= ATACH_DMA_WAIT;
		} else
#endif

		wdc->dataout_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, ata_bio->nbytes);
	}

#if NATA_DMA
intr:
#endif
	/* Wait for IRQ (either real or polled) */
	if ((ata_bio->flags & ATA_POLL) == 0) {
		chp->ch_flags |= ATACH_IRQ_WAIT;
	} else {
		/* Wait for at last 400ns for status bit to be valid */
		delay(1);
#if NATA_DMA
		if (chp->ch_flags & ATACH_DMA_WAIT) {
			wdc_dmawait(chp, xfer, ATA_DELAY);
			chp->ch_flags &= ~ATACH_DMA_WAIT;
		}
#endif
		wdc_ata_bio_intr(chp, xfer, 0);
		if ((ata_bio->flags & ATA_ITSDONE) == 0)
			goto again;
	}
	return;
timeout:
	printf("%s:%d:%d: not ready, st=0x%02x, err=0x%02x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    chp->ch_status, chp->ch_error);
	if (wdc_ata_err(drvp, ata_bio) != WDC_ATA_ERR)
		ata_bio->error = TIMEOUT;
	wdc_ata_bio_done(chp, xfer);
	return;
}

static int
wdc_ata_bio_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct ata_bio *ata_bio = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int drv_err;

	ATADEBUG_PRINT(("wdc_ata_bio_intr %s:%d:%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive),
	    DEBUG_INTR | DEBUG_XFERS);


	/* Is it not a transfer, but a control operation? */
	if (drvp->state < READY) {
		printf("%s:%d:%d: bad state %d in wdc_ata_bio_intr\n",
		    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
		    drvp->state);
		panic("wdc_ata_bio_intr: bad state");
	}

	/*
	 * if we missed an interrupt in a PIO transfer, reset and restart.
	 * Don't try to continue transfer, we may have missed cycles.
	 */
	if ((xfer->c_flags & (C_TIMEOU | C_DMA)) == C_TIMEOU) {
		ata_bio->error = TIMEOUT;
		wdc_ata_bio_done(chp, xfer);
		return 1;
	}

#if NATA_PIOBM
	/* Transfer-done interrupt for busmastering PIO read */
	if ((xfer->c_flags & C_PIOBM) && (chp->ch_flags & ATACH_PIOBM_WAIT)) {
		chp->ch_flags &= ~ATACH_PIOBM_WAIT;
		goto end;
	}
#endif

	/* Ack interrupt done by wdc_wait_for_unbusy */
	if (wdc_wait_for_unbusy(chp, (irq == 0) ? ATA_DELAY : 0, AT_POLL) < 0) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0; /* IRQ was not for us */
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip%d\n",
		    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
		    xfer->c_bcount, xfer->c_skip);
		ata_bio->error = TIMEOUT;
		wdc_ata_bio_done(chp, xfer);
		return 1;
	}
	if (wdc->irqack)
		wdc->irqack(chp);

	drv_err = wdc_ata_err(drvp, ata_bio);

#if NATA_DMA
	/* If we were using DMA, Turn off the DMA channel and check for error */
	if (xfer->c_flags & C_DMA) {
		if (ata_bio->flags & ATA_POLL) {
			/*
			 * IDE drives deassert WDCS_BSY before transfer is
			 * complete when using DMA. Polling for DRQ to deassert
			 * is not enough DRQ is not required to be
			 * asserted for DMA transfers, so poll for DRDY.
			 */
			if (wdcwait(chp, WDCS_DRDY | WDCS_DRQ, WDCS_DRDY,
			    ATA_DELAY, ATA_POLL) == WDCWAIT_TOUT) {
				printf("%s:%d:%d: polled transfer timed out "
				    "(st=0x%x)\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, xfer->c_drive,
				    chp->ch_status);
				ata_bio->error = TIMEOUT;
				drv_err = WDC_ATA_ERR;
			}
		}
		if (wdc->dma_status != 0) {
			if (drv_err != WDC_ATA_ERR) {
				ata_bio->error = ERR_DMA;
				drv_err = WDC_ATA_ERR;
			}
		}
		if (chp->ch_status & WDCS_DRQ) {
			if (drv_err != WDC_ATA_ERR) {
				printf("%s:%d:%d: intr with DRQ (st=0x%x)\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel,
				    xfer->c_drive, chp->ch_status);
				ata_bio->error = TIMEOUT;
				drv_err = WDC_ATA_ERR;
			}
		}
		if (drv_err != WDC_ATA_ERR)
			goto end;
		if (ata_bio->r_error & WDCE_CRC || ata_bio->error == ERR_DMA)
			ata_dmaerr(drvp, (xfer->c_flags & C_POLL) ? AT_POLL : 0);
	}
#endif	/* NATA_DMA */

	/* if we had an error, end */
	if (drv_err == WDC_ATA_ERR) {
		wdc_ata_bio_done(chp, xfer);
		return 1;
	}

	/* If this was a read and not using DMA, fetch the data. */
	if ((ata_bio->flags & ATA_READ) != 0) {
		if ((chp->ch_status & WDCS_DRQ) != WDCS_DRQ) {
			printf("%s:%d:%d: read intr before drq\n",
			    device_xname(atac->atac_dev), chp->ch_channel,
			    xfer->c_drive);
			ata_bio->error = TIMEOUT;
			wdc_ata_bio_done(chp, xfer);
			return 1;
		}
#if NATA_PIOBM
		if (xfer->c_flags & C_PIOBM) {
			/* start the busmastering PIO */
			(*wdc->piobm_start)(wdc->dma_arg,
			    chp->ch_channel, xfer->c_drive,
			    xfer->c_skip, ata_bio->nbytes,
			    WDC_PIOBM_XFER_IRQ);
			chp->ch_flags |= ATACH_DMA_WAIT | ATACH_PIOBM_WAIT | ATACH_IRQ_WAIT;
			return 1;
		} else
#endif
		wdc->datain_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, ata_bio->nbytes);
	}

#if NATA_DMA || NATA_PIOBM
end:
#endif
	ata_bio->blkno += ata_bio->nblks;
	ata_bio->blkdone += ata_bio->nblks;
	xfer->c_skip += ata_bio->nbytes;
	xfer->c_bcount -= ata_bio->nbytes;
	/* See if this transfer is complete. */
	if (xfer->c_bcount > 0) {
		if ((ata_bio->flags & ATA_POLL) == 0) {
			/* Start the next operation */
			_wdc_ata_bio_start(chp, xfer);
		} else {
			/* Let _wdc_ata_bio_start do the loop */
			return 1;
		}
	} else { /* Done with this transfer */
		ata_bio->error = NOERROR;
		wdc_ata_bio_done(chp, xfer);
	}
	return 1;
}

static void
wdc_ata_bio_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer,
    int reason)
{
	struct ata_bio *ata_bio = xfer->c_cmd;
	int drive = xfer->c_drive;

	ata_free_xfer(chp, xfer);

	ata_bio->flags |= ATA_ITSDONE;
	switch (reason) {
	case KILL_GONE:
		ata_bio->error = ERR_NODEV;
		break;
	case KILL_RESET:
		ata_bio->error = ERR_RESET;
		break;
	default:
		printf("wdc_ata_bio_kill_xfer: unknown reason %d\n",
		    reason);
		panic("wdc_ata_bio_kill_xfer");
	}
	ata_bio->r_error = WDCE_ABRT;
	ATADEBUG_PRINT(("wdc_ata_bio_kill_xfer: drv_done\n"), DEBUG_XFERS);
	(*chp->ch_drive[drive].drv_done)(chp->ch_drive[drive].drv_softc);
}

static void
wdc_ata_bio_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct ata_bio *ata_bio = xfer->c_cmd;
	int drive = xfer->c_drive;

	ATADEBUG_PRINT(("wdc_ata_bio_done %s:%d:%d: flags 0x%x\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    xfer->c_drive, (u_int)xfer->c_flags),
	    DEBUG_XFERS);

	callout_stop(&chp->ch_callout);

	/* feed back residual bcount to our caller */
	ata_bio->bcount = xfer->c_bcount;

	/* mark controller inactive and free xfer */
	chp->ch_queue->active_xfer = NULL;
	ata_free_xfer(chp, xfer);

	if (chp->ch_drive[drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		ata_bio->error = ERR_NODEV;
		chp->ch_drive[drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	}
	ata_bio->flags |= ATA_ITSDONE;
	ATADEBUG_PRINT(("wdc_ata_done: drv_done\n"), DEBUG_XFERS);
	(*chp->ch_drive[drive].drv_done)(chp->ch_drive[drive].drv_softc);
	ATADEBUG_PRINT(("atastart from wdc_ata_done, flags 0x%x\n",
	    chp->ch_flags), DEBUG_XFERS);
	atastart(chp);
}

static int
wdc_ata_err(struct ata_drive_datas *drvp, struct ata_bio *ata_bio)
{
	struct ata_channel *chp = drvp->chnl_softc;
	ata_bio->error = 0;
	if (chp->ch_status & WDCS_BSY) {
		ata_bio->error = TIMEOUT;
		return WDC_ATA_ERR;
	}

	if (chp->ch_status & WDCS_DWF) {
		ata_bio->error = ERR_DF;
		return WDC_ATA_ERR;
	}

	if (chp->ch_status & WDCS_ERR) {
		ata_bio->error = ERROR;
		ata_bio->r_error = chp->ch_error;
		if (ata_bio->r_error & (WDCE_BBK | WDCE_UNC | WDCE_IDNF |
		    WDCE_ABRT | WDCE_TK0NF | WDCE_AMNF))
			return WDC_ATA_ERR;
		return WDC_ATA_NOERR;
	}

	if (chp->ch_status & WDCS_CORR)
		ata_bio->flags |= ATA_CORR;
	return WDC_ATA_NOERR;
}

static int
wdc_ata_addref(struct ata_drive_datas *drvp)
{
	struct ata_channel *chp = drvp->chnl_softc;

	return (ata_addref(chp));
}

static void
wdc_ata_delref(struct ata_drive_datas *drvp)
{
	struct ata_channel *chp = drvp->chnl_softc;

	ata_delref(chp);
}
