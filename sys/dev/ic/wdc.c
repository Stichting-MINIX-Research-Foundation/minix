/*	$NetBSD: wdc.c,v 1.279 2013/09/15 16:08:28 martin Exp $ */

/*
 * Copyright (c) 1998, 2001, 2003 Manuel Bouyer.  All rights reserved.
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
 * Copyright (c) 1998, 2003, 2004 The NetBSD Foundation, Inc.
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

/*
 * CODE UNTESTED IN THE CURRENT REVISION:
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wdc.c,v 1.279 2013/09/15 16:08:28 martin Exp $");

#include "opt_ata.h"
#include "opt_wdc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <sys/intr.h>
#include <sys/bus.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_write_multi_stream_4	bus_space_write_multi_4
#define bus_space_read_multi_stream_2	bus_space_read_multi_2
#define bus_space_read_multi_stream_4	bus_space_read_multi_4
#define bus_space_read_stream_2	bus_space_read_2
#define bus_space_read_stream_4	bus_space_read_4
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_stream_4	bus_space_write_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#include <dev/ata/atavar.h>
#include <dev/ata/atareg.h>
#include <dev/ata/satareg.h>
#include <dev/ata/satavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include "locators.h"

#include "atapibus.h"
#include "wd.h"
#include "sata.h"

#define WDCDELAY  100 /* 100 microseconds */
#define WDCNDELAY_RST (WDC_RESET_WAIT * 1000 / WDCDELAY)
#if 0
/* If you enable this, it will report any delays more than WDCDELAY * N long. */
#define WDCNDELAY_DEBUG	50
#endif

/* When polling wait that much and then tsleep for 1/hz seconds */
#define WDCDELAY_POLL 1 /* ms */

/* timeout for the control commands */
#define WDC_CTRL_DELAY 10000 /* 10s, for the recall command */

/*
 * timeout when waiting for BSY to deassert when probing.
 * set to 5s. From the standards this could be up to 31, but we can't
 * wait that much at boot time, and 5s seems to be enough.
 */
#define WDC_PROBE_WAIT 5


#if NWD > 0
extern const struct ata_bustype wdc_ata_bustype; /* in ata_wdc.c */
#else
/* A fake one, the autoconfig will print "wd at foo ... not configured */
const struct ata_bustype wdc_ata_bustype = {
	SCSIPI_BUSTYPE_ATA,
	NULL,				/* wdc_ata_bio */
	NULL,				/* wdc_reset_drive */
	wdc_reset_channel,
	wdc_exec_command,
	NULL,				/* ata_get_params */
	NULL,				/* wdc_ata_addref */
	NULL,				/* wdc_ata_delref */
	NULL				/* ata_kill_pending */
};
#endif

/* Flags to wdcreset(). */
#define	RESET_POLL	1
#define	RESET_SLEEP	0	/* wdcreset() will use tsleep() */

static int	wdcprobe1(struct ata_channel *, int);
static int	wdcreset(struct ata_channel *, int);
static void	__wdcerror(struct ata_channel *, const char *);
static int	__wdcwait_reset(struct ata_channel *, int, int);
static void	__wdccommand_done(struct ata_channel *, struct ata_xfer *);
static void	__wdccommand_done_end(struct ata_channel *, struct ata_xfer *);
static void	__wdccommand_kill_xfer(struct ata_channel *,
			               struct ata_xfer *, int);
static void	__wdccommand_start(struct ata_channel *, struct ata_xfer *);
static int	__wdccommand_intr(struct ata_channel *, struct ata_xfer *, int);
static int	__wdcwait(struct ata_channel *, int, int, int);

static void	wdc_datain_pio(struct ata_channel *, int, void *, size_t);
static void	wdc_dataout_pio(struct ata_channel *, int, void *, size_t);
#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_DETACH 0x20
#define DEBUG_DELAY  0x40
#ifdef ATADEBUG
extern int atadebug_mask; /* init'ed in ata.c */
int wdc_nxfer = 0;
#define ATADEBUG_PRINT(args, level)  if (atadebug_mask & (level)) printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

/*
 * Initialize the "shadow register" handles for a standard wdc controller.
 */
void
wdc_init_shadow_regs(struct ata_channel *chp)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

	wdr->cmd_iohs[wd_status] = wdr->cmd_iohs[wd_command];
	wdr->cmd_iohs[wd_features] = wdr->cmd_iohs[wd_error];
}

/*
 * Allocate a wdc_regs array, based on the number of channels.
 */
void
wdc_allocate_regs(struct wdc_softc *wdc)
{

	wdc->regs = malloc(wdc->sc_atac.atac_nchannels *
			   sizeof(struct wdc_regs), M_DEVBUF, M_WAITOK);
}

#if NSATA > 0
/*
 * probe drives on SATA controllers with standard SATA registers:
 * bring the PHYs online, read the drive signature and set drive flags
 * appropriately.
 */
void
wdc_sataprobe(struct ata_channel *chp)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);
	uint8_t st = 0, sc __unused, sn __unused, cl, ch;
	int i, s;

	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);

	/* reset the PHY and bring online */
	switch (sata_reset_interface(chp, wdr->sata_iot, wdr->sata_control,
	    wdr->sata_status, AT_WAIT)) {
	case SStatus_DET_DEV:
		/* wait 5s for BSY to clear */
		for (i = 0; i < WDC_PROBE_WAIT * hz; i++) {
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sdh], 0, WDSD_IBM);
			delay(10);      /* 400ns delay */
			st = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
			if ((st & WDCS_BSY) == 0)
				break;
			tsleep(&chp, PRIBIO, "sataprb", 1);
		}
		if (i == WDC_PROBE_WAIT * hz)
			aprint_error_dev(chp->ch_atac->atac_dev,
			    "BSY never cleared, status 0x%02x\n", st);
		sc = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_seccnt], 0);
		sn = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_sector], 0);
		cl = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_cyl_lo], 0);
		ch = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_cyl_hi], 0);
		ATADEBUG_PRINT(("%s: port %d: sc=0x%x sn=0x%x "
		    "cl=0x%x ch=0x%x\n",
		    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
		    sc, sn, cl, ch), DEBUG_PROBE);
		if (atabus_alloc_drives(chp, 1) != 0)
			return;
		/*
		 * sc and sn are supposed to be 0x1 for ATAPI, but in some
		 * cases we get wrong values here, so ignore it.
		 */
		s = splbio();
		if (cl == 0x14 && ch == 0xeb)
			chp->ch_drive[0].drive_type = ATA_DRIVET_ATAPI;
		else
			chp->ch_drive[0].drive_type = ATA_DRIVET_ATA;
		splx(s);

		/*
		 * issue a reset in case only the interface part of the drive
		 * is up
		 */
		if (wdcreset(chp, RESET_SLEEP) != 0)
			chp->ch_drive[0].drive_type = ATA_DRIVET_NONE;
		break;

	default:
		break;
	}
}
#endif /* NSATA > 0 */


/* Test to see controller with at last one attached drive is there.
 * Returns a bit for each possible drive found (0x01 for drive 0,
 * 0x02 for drive 1).
 * Logic:
 * - If a status register is at 0xff, assume there is no drive here
 *   (ISA has pull-up resistors).  Similarly if the status register has
 *   the value we last wrote to the bus (for IDE interfaces without pullups).
 *   If no drive at all -> return.
 * - reset the controller, wait for it to complete (may take up to 31s !).
 *   If timeout -> return.
 * - test ATA/ATAPI signatures. If at last one drive found -> return.
 * - try an ATA command on the master.
 */

void
wdc_drvprobe(struct ata_channel *chp)
{
	struct ataparams params; /* XXX: large struct */
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	u_int8_t st0 = 0, st1 = 0;
	int i, j, error, s;

	if (atabus_alloc_drives(chp, wdc->wdc_maxdrives) != 0)
		return;
	if (wdcprobe1(chp, 0) == 0) {
		/* No drives, abort the attach here. */
		atabus_free_drives(chp);
		return;
	}

	s = splbio();
	/* for ATA/OLD drives, wait for DRDY, 3s timeout */
	for (i = 0; i < mstohz(3000); i++) {
		/*
		 * select drive 1 first, so that master is selected on
		 * exit from the loop
		 */
		if (chp->ch_ndrives > 1 &&
		    chp->ch_drive[1].drive_type == ATA_DRIVET_ATA) {
			if (wdc->select)
				wdc->select(chp,1);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM | 0x10);
			delay(10);	/* 400ns delay */
			st1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
		}
		if (chp->ch_drive[0].drive_type == ATA_DRIVET_ATA) {
			if (wdc->select)
				wdc->select(chp,0);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM);
			delay(10);	/* 400ns delay */
			st0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
		}


		if ((chp->ch_drive[0].drive_type != ATA_DRIVET_ATA ||
		     (st0 & WDCS_DRDY)) &&
		    (chp->ch_ndrives < 2 ||
		     chp->ch_drive[1].drive_type != ATA_DRIVET_ATA ||
		     (st1 & WDCS_DRDY)))
			break;
#ifdef WDC_NO_IDS
		/* cannot tsleep here (can't enable IPL_BIO interrups),
		 * delay instead
		 */
		delay(1000000 / hz);
#else
		tsleep(&params, PRIBIO, "atadrdy", 1);
#endif
	}
	if ((st0 & WDCS_DRDY) == 0 &&
	    chp->ch_drive[0].drive_type != ATA_DRIVET_ATAPI)
		chp->ch_drive[0].drive_type = ATA_DRIVET_NONE;
	if (chp->ch_ndrives > 1 && (st1 & WDCS_DRDY) == 0 &&
	    chp->ch_drive[1].drive_type != ATA_DRIVET_ATAPI)
		chp->ch_drive[1].drive_type = ATA_DRIVET_NONE;
	splx(s);

	ATADEBUG_PRINT(("%s:%d: wait DRDY st0 0x%x st1 0x%x\n",
	    device_xname(atac->atac_dev),
	    chp->ch_channel, st0, st1), DEBUG_PROBE);

	/* Wait a bit, some devices are weird just after a reset. */
	delay(5000);

	for (i = 0; i < chp->ch_ndrives; i++) {
#if NATA_DMA
		/*
		 * Init error counter so that an error withing the first xfers
		 * will trigger a downgrade
		 */
		chp->ch_drive[i].n_dmaerrs = NERRS_MAX-1;
#endif

		/* If controller can't do 16bit flag the drives as 32bit */
		if ((atac->atac_cap &
		    (ATAC_CAP_DATA16 | ATAC_CAP_DATA32)) == ATAC_CAP_DATA32) {
			s = splbio();
			chp->ch_drive[i].drive_flags |= ATA_DRIVE_CAP32;
			splx(s);
		}
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_NONE)
			continue;

		/* Shortcut in case we've been shutdown */
		if (chp->ch_flags & ATACH_SHUTDOWN)
			return;

		/*
		 * Issue an identify, to try to detect ghosts.
		 * Note that we can't use interrupts here, because if there
		 * is no devices, we will get a command aborted without
		 * interrupts.
		 */
		error = ata_get_params(&chp->ch_drive[i],
		    AT_WAIT | AT_POLL, &params);
		if (error != CMD_OK) {
			tsleep(&params, PRIBIO, "atacnf", mstohz(1000));

			/* Shortcut in case we've been shutdown */
			if (chp->ch_flags & ATACH_SHUTDOWN)
				return;

			error = ata_get_params(&chp->ch_drive[i],
			    AT_WAIT | AT_POLL, &params);
		}
		if (error != CMD_OK) {
			ATADEBUG_PRINT(("%s:%d:%d: IDENTIFY failed (%d)\n",
			    device_xname(atac->atac_dev),
			    chp->ch_channel, i, error), DEBUG_PROBE);
			s = splbio();
			if (chp->ch_drive[i].drive_type != ATA_DRIVET_ATA ||
			    (wdc->cap & WDC_CAPABILITY_PREATA) == 0) {
				chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
				splx(s);
				continue;
			}
			splx(s);
			/*
			 * Pre-ATA drive ?
			 * Test registers writability (Error register not
			 * writable, but cyllo is), then try an ATA command.
			 */
			if (wdc->select)
				wdc->select(chp,i);
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sdh], 0, WDSD_IBM | (i << 4));
			delay(10);	/* 400ns delay */
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_error],
			    0, 0x58);
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0, 0xa5);
			if (bus_space_read_1(wdr->cmd_iot,
				wdr->cmd_iohs[wd_error], 0) == 0x58 ||
			    bus_space_read_1(wdr->cmd_iot,
				wdr->cmd_iohs[wd_cyl_lo], 0) != 0xa5) {
				ATADEBUG_PRINT(("%s:%d:%d: register "
				    "writability failed\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, i), DEBUG_PROBE);
				    s = splbio();
				    chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
				    splx(s);
				    continue;
			}
			if (wdc_wait_for_ready(chp, 10000, 0) == WDCWAIT_TOUT) {
				ATADEBUG_PRINT(("%s:%d:%d: not ready\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, i), DEBUG_PROBE);
				s = splbio();
				chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
				splx(s);
				continue;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_command], 0, WDCC_RECAL);
			delay(10);	/* 400ns delay */
			if (wdc_wait_for_ready(chp, 10000, 0) == WDCWAIT_TOUT) {
				ATADEBUG_PRINT(("%s:%d:%d: WDCC_RECAL failed\n",
				    device_xname(atac->atac_dev),
				    chp->ch_channel, i), DEBUG_PROBE);
				s = splbio();
				chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
				splx(s);
			} else {
				s = splbio();
				for (j = 0; j < chp->ch_ndrives; j++) {
					if (chp->ch_drive[i].drive_type !=
					    ATA_DRIVET_NONE) {
						chp->ch_drive[j].drive_type =
						    ATA_DRIVET_OLD;
					}
				}
				splx(s);
			}
		}
	}
}

int
wdcprobe(struct ata_channel *chp)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	/* default reset method */
	if (wdc->reset == NULL)
		wdc->reset = wdc_do_reset;

	return (wdcprobe1(chp, 1));
}

static int
wdcprobe1(struct ata_channel *chp, int poll)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	u_int8_t st0 = 0, st1 = 0, sc __unused, sn __unused, cl, ch;
	u_int8_t ret_value = 0x03;
	u_int8_t drive;
	int s;
	/* XXX if poll, wdc_probe_count is 0. */
	int wdc_probe_count =
	    poll ? (WDC_PROBE_WAIT / WDCDELAY)
	         : (WDC_PROBE_WAIT * hz);

	/*
	 * Sanity check to see if the wdc channel responds at all.
	 */

	s = splbio();
	if ((wdc->cap & WDC_CAPABILITY_NO_EXTRA_RESETS) == 0) {
		while (wdc_probe_count-- > 0) {
			if (wdc->select)
				wdc->select(chp,0);

			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM);
			delay(10);	/* 400ns delay */
			st0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);

			if (wdc->select)
				wdc->select(chp,1);

			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM | 0x10);
			delay(10);	/* 400ns delay */
			st1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
			if ((st0 & WDCS_BSY) == 0)
				break;
		}

		ATADEBUG_PRINT(("%s:%d: before reset, st0=0x%x, st1=0x%x\n",
		    device_xname(chp->ch_atac->atac_dev),
		    chp->ch_channel, st0, st1), DEBUG_PROBE);

		if (st0 == 0xff || st0 == WDSD_IBM)
			ret_value &= ~0x01;
		if (st1 == 0xff || st1 == (WDSD_IBM | 0x10))
			ret_value &= ~0x02;
		/* Register writability test, drive 0. */
		if (ret_value & 0x01) {
			if (wdc->select)
				wdc->select(chp,0);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM);
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0, 0x02);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x02) {
				ATADEBUG_PRINT(("%s:%d drive 0 wd_cyl_lo: "
				    "got 0x%x != 0x02\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x01;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0, 0x01);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 0 wd_cyl_lo: "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x01;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0, 0x01);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 0 wd_sector: "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x01;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0, 0x02);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			if (cl != 0x02) {
				ATADEBUG_PRINT(("%s:%d drive 0 wd_sector: "
				    "got 0x%x != 0x02\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x01;
			}
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 0 wd_cyl_lo(2): "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x01;
			}
		}
		/* Register writability test, drive 1. */
		if (ret_value & 0x02) {
			if (wdc->select)
			     wdc->select(chp,1);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			     0, WDSD_IBM | 0x10);
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0, 0x02);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x02) {
				ATADEBUG_PRINT(("%s:%d drive 1 wd_cyl_lo: "
				    "got 0x%x != 0x02\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x02;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0, 0x01);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 1 wd_cyl_lo: "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x02;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0, 0x01);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 1 wd_sector: "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x02;
			}
			bus_space_write_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0, 0x02);
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			if (cl != 0x02) {
				ATADEBUG_PRINT(("%s:%d drive 1 wd_sector: "
				    "got 0x%x != 0x02\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x02;
			}
			cl = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			if (cl != 0x01) {
				ATADEBUG_PRINT(("%s:%d drive 1 wd_cyl_lo(2): "
				    "got 0x%x != 0x01\n",
				    device_xname(chp->ch_atac->atac_dev),
				    chp->ch_channel, cl),
				    DEBUG_PROBE);
				ret_value &= ~0x02;
			}
		}

		if (ret_value == 0) {
			splx(s);
			return 0;
		}
	}


#if 0 /* XXX this break some ATA or ATAPI devices */
	/*
	 * reset bus. Also send an ATAPI_RESET to devices, in case there are
	 * ATAPI device out there which don't react to the bus reset
	 */
	if (ret_value & 0x01) {
		if (wdc->select)
			wdc->select(chp,0);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
		     0, WDSD_IBM);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_command], 0,
		    ATAPI_SOFT_RESET);
	}
	if (ret_value & 0x02) {
		if (wdc->select)
			wdc->select(chp,0);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
		     0, WDSD_IBM | 0x10);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_command], 0,
		    ATAPI_SOFT_RESET);
	}

	delay(5000);
#endif

	wdc->reset(chp, RESET_POLL);
	DELAY(2000);
	(void) bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_error], 0);

	if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) 
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr, 
		    WDCTL_4BIT);

#ifdef WDC_NO_IDS
	ret_value = __wdcwait_reset(chp, ret_value, RESET_POLL);
#else
	splx(s);
	ret_value = __wdcwait_reset(chp, ret_value, poll);
	s = splbio();
#endif
	ATADEBUG_PRINT(("%s:%d: after reset, ret_value=0x%d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    ret_value), DEBUG_PROBE);

	/* if reset failed, there's nothing here */
	if (ret_value == 0) {
		splx(s);
		return 0;
	}

	/*
	 * Test presence of drives. First test register signatures looking
	 * for ATAPI devices. If it's not an ATAPI and reset said there may
	 * be something here assume it's ATA or OLD.  Ghost will be killed
	 * later in attach routine.
	 */
	for (drive = 0; drive < wdc->wdc_maxdrives; drive++) {
		if ((ret_value & (0x01 << drive)) == 0)
			continue;
		if (wdc->select)
			wdc->select(chp,drive);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM | (drive << 4));
		delay(10);	/* 400ns delay */
		/* Save registers contents */
		sc = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_seccnt], 0);
		sn = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_sector], 0);
		cl = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_cyl_lo], 0);
		ch = bus_space_read_1(wdr->cmd_iot,
		     wdr->cmd_iohs[wd_cyl_hi], 0);

		ATADEBUG_PRINT(("%s:%d:%d: after reset, sc=0x%x sn=0x%x "
		    "cl=0x%x ch=0x%x\n",
		    device_xname(chp->ch_atac->atac_dev),
		    chp->ch_channel, drive, sc, sn, cl, ch), DEBUG_PROBE);
		/*
		 * sc & sn are supposed to be 0x1 for ATAPI but in some cases
		 * we get wrong values here, so ignore it.
		 */
		if (chp->ch_drive != NULL) {
			if (cl == 0x14 && ch == 0xeb) {
				chp->ch_drive[drive].drive_type = ATA_DRIVET_ATAPI;
			} else {
				chp->ch_drive[drive].drive_type = ATA_DRIVET_ATA;
			}
		}
	}
	/*
	 * Select an existing drive before lowering spl, some WDC_NO_IDS
	 * devices incorrectly assert IRQ on nonexistent slave
	 */
	if (ret_value & 0x01) {
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM);
		(void)bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_status], 0);
	}
	splx(s);
	return (ret_value);
}

void
wdcattach(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);

	KASSERT(wdc->wdc_maxdrives > 0 && wdc->wdc_maxdrives <= WDC_MAXDRIVES);

	/* default data transfer methods */
	if (wdc->datain_pio == NULL)
		wdc->datain_pio = wdc_datain_pio;
	if (wdc->dataout_pio == NULL)
		wdc->dataout_pio = wdc_dataout_pio;
	/* default reset method */
	if (wdc->reset == NULL)
		wdc->reset = wdc_do_reset;

	/* initialise global data */
	if (atac->atac_bustype_ata == NULL)
		atac->atac_bustype_ata = &wdc_ata_bustype;
	if (atac->atac_probe == NULL)
		atac->atac_probe = wdc_drvprobe;
#if NATAPIBUS > 0
	if (atac->atac_atapibus_attach == NULL)
		atac->atac_atapibus_attach = wdc_atapibus_attach;
#endif

	ata_channel_attach(chp);
}

void
wdc_childdetached(device_t self, device_t child)
{
	struct atac_softc *atac = device_private(self);
	struct ata_channel *chp;
	int i;

	for (i = 0; i < atac->atac_nchannels; i++) {
		chp = atac->atac_channels[i];
		if (child == chp->atabus) {
			chp->atabus = NULL;
			return;
		}
	}
}

int
wdcdetach(device_t self, int flags)
{
	struct atac_softc *atac = device_private(self);
	struct ata_channel *chp;
	struct scsipi_adapter *adapt = &atac->atac_atapi_adapter._generic;
	int i, error = 0;

	for (i = 0; i < atac->atac_nchannels; i++) {
		chp = atac->atac_channels[i];
		if (chp->atabus == NULL)
			continue;
		ATADEBUG_PRINT(("wdcdetach: %s: detaching %s\n",
		    device_xname(atac->atac_dev), device_xname(chp->atabus)),
		    DEBUG_DETACH);
		if ((error = config_detach(chp->atabus, flags)) != 0)
			return error;
	}
	if (adapt->adapt_refcnt != 0)
		return EBUSY;
	return 0;
}

/* restart an interrupted I/O */
void
wdcrestart(void *v)
{
	struct ata_channel *chp = v;
	int s;

	s = splbio();
	atastart(chp);
	splx(s);
}


/*
 * Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start the
 * next request.  Also check for a partially done transfer, and continue with
 * the next chunk if so.
 */
int
wdcintr(void *arg)
{
	struct ata_channel *chp = arg;
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct ata_xfer *xfer;
	int ret;

	if (!device_is_active(atac->atac_dev)) {
		ATADEBUG_PRINT(("wdcintr: deactivated controller\n"),
		    DEBUG_INTR);
		return (0);
	}
	if ((chp->ch_flags & ATACH_IRQ_WAIT) == 0) {
		ATADEBUG_PRINT(("wdcintr: inactive controller\n"), DEBUG_INTR);
		/* try to clear the pending interrupt anyway */
		(void)bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_status], 0);
		return (0);
	}

	ATADEBUG_PRINT(("wdcintr\n"), DEBUG_INTR);
	xfer = chp->ch_queue->active_xfer;
#ifdef DIAGNOSTIC
	if (xfer == NULL)
		panic("wdcintr: no xfer");
	if (xfer->c_chp != chp) {
		printf("channel %d expected %d\n", xfer->c_chp->ch_channel,
		    chp->ch_channel);
		panic("wdcintr: wrong channel");
	}
#endif
#if NATA_DMA || NATA_PIOBM
	if (chp->ch_flags & ATACH_DMA_WAIT) {
		wdc->dma_status =
		    (*wdc->dma_finish)(wdc->dma_arg, chp->ch_channel,
			xfer->c_drive, WDC_DMAEND_END);
		if (wdc->dma_status & WDC_DMAST_NOIRQ) {
			/* IRQ not for us, not detected by DMA engine */
			return 0;
		}
		chp->ch_flags &= ~ATACH_DMA_WAIT;
	}
#endif
	chp->ch_flags &= ~ATACH_IRQ_WAIT;
	KASSERT(xfer->c_intr != NULL);
	ret = xfer->c_intr(chp, xfer, 1);
	if (ret == 0) /* irq was not for us, still waiting for irq */
		chp->ch_flags |= ATACH_IRQ_WAIT;
	return (ret);
}

/* Put all disk in RESET state */
void
wdc_reset_drive(struct ata_drive_datas *drvp, int flags, uint32_t *sigp)
{
	struct ata_channel *chp = drvp->chnl_softc;

	KASSERT(sigp == NULL);

	ATADEBUG_PRINT(("wdc_reset_drive %s:%d for drive %d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    drvp->drive), DEBUG_FUNCS);

	ata_reset_channel(chp, flags);
}

void
wdc_reset_channel(struct ata_channel *chp, int flags)
{
	TAILQ_HEAD(, ata_xfer) reset_xfer;
	struct ata_xfer *xfer, *next_xfer;
#if NATA_DMA || NATA_PIOBM
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
#endif
	TAILQ_INIT(&reset_xfer);

	chp->ch_flags &= ~ATACH_IRQ_WAIT;

	/*
	 * if the current command if on an ATAPI device, issue a
	 * ATAPI_SOFT_RESET
	 */
	xfer = chp->ch_queue->active_xfer;
	if (xfer && xfer->c_chp == chp && (xfer->c_flags & C_ATAPI)) {
		wdccommandshort(chp, xfer->c_drive, ATAPI_SOFT_RESET);
		if (flags & AT_WAIT)
			tsleep(&flags, PRIBIO, "atardl", mstohz(1) + 1);
		else
			delay(1000);
	}

	/* reset the channel */
	if (flags & AT_WAIT)
		(void) wdcreset(chp, RESET_SLEEP);
	else
		(void) wdcreset(chp, RESET_POLL);

	/*
	 * wait a bit after reset; in case the DMA engines needs some time
	 * to recover.
	 */
	if (flags & AT_WAIT)
		tsleep(&flags, PRIBIO, "atardl", mstohz(1) + 1);
	else
		delay(1000);
	/*
	 * look for pending xfers. If we have a shared queue, we'll also reset
	 * the other channel if the current xfer is running on it.
	 * Then we'll dequeue only the xfers for this channel.
	 */
	if ((flags & AT_RST_NOCMD) == 0) {
		/*
		 * move all xfers queued for this channel to the reset queue,
		 * and then process the current xfer and then the reset queue.
		 * We have to use a temporary queue because c_kill_xfer()
		 * may requeue commands.
		 */
		for (xfer = TAILQ_FIRST(&chp->ch_queue->queue_xfer);
		    xfer != NULL; xfer = next_xfer) {
			next_xfer = TAILQ_NEXT(xfer, c_xferchain);
			if (xfer->c_chp != chp)
				continue;
			TAILQ_REMOVE(&chp->ch_queue->queue_xfer,
			    xfer, c_xferchain);
			TAILQ_INSERT_TAIL(&reset_xfer, xfer, c_xferchain);
		}
		xfer = chp->ch_queue->active_xfer;
		if (xfer) {
			if (xfer->c_chp != chp)
				ata_reset_channel(xfer->c_chp, flags);
			else {
				callout_stop(&chp->ch_callout);
#if NATA_DMA || NATA_PIOBM
				/*
				 * If we're waiting for DMA, stop the
				 * DMA engine
				 */
				if (chp->ch_flags & ATACH_DMA_WAIT) {
					(*wdc->dma_finish)(
					    wdc->dma_arg,
					    chp->ch_channel,
					    xfer->c_drive,
					    WDC_DMAEND_ABRT_QUIET);
					chp->ch_flags &= ~ATACH_DMA_WAIT;
				}
#endif
				chp->ch_queue->active_xfer = NULL;
				if ((flags & AT_RST_EMERG) == 0)
					xfer->c_kill_xfer(
					    chp, xfer, KILL_RESET);
			}
		}

		for (xfer = TAILQ_FIRST(&reset_xfer);
		    xfer != NULL; xfer = next_xfer) {
			next_xfer = TAILQ_NEXT(xfer, c_xferchain);
			TAILQ_REMOVE(&reset_xfer, xfer, c_xferchain);
			if ((flags & AT_RST_EMERG) == 0)
				xfer->c_kill_xfer(chp, xfer, KILL_RESET);
		}
	}
}

static int
wdcreset(struct ata_channel *chp, int poll)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	int drv_mask1, drv_mask2;

#ifdef WDC_NO_IDS
	poll = RESET_POLL;
#endif
	wdc->reset(chp, poll);

	drv_mask1 = (chp->ch_drive[0].drive_type !=  ATA_DRIVET_NONE) ? 0x01:0x00;
	if (chp->ch_ndrives > 1) 
		drv_mask1 |=
		    (chp->ch_drive[1].drive_type != ATA_DRIVET_NONE) ? 0x02:0x00;
	drv_mask2 = __wdcwait_reset(chp, drv_mask1,
	    (poll == RESET_SLEEP) ? 0 : 1);
	if (drv_mask2 != drv_mask1) {
		aprint_error("%s channel %d: reset failed for",
		    device_xname(atac->atac_dev), chp->ch_channel);
		if ((drv_mask1 & 0x01) != 0 && (drv_mask2 & 0x01) == 0)
			aprint_normal(" drive 0");
		if ((drv_mask1 & 0x02) != 0 && (drv_mask2 & 0x02) == 0)
			aprint_normal(" drive 1");
		aprint_normal("\n");
	}
	if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) 
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr, 
		    WDCTL_4BIT);

	return  (drv_mask1 != drv_mask2) ? 1 : 0;
}

void
wdc_do_reset(struct ata_channel *chp, int poll)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	int s = 0;

	if (poll != RESET_SLEEP)
		s = splbio();
	if (wdc->select)
		wdc->select(chp,0);
	/* master */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0, WDSD_IBM);
	delay(10);	/* 400ns delay */
	/* assert SRST, wait for reset to complete */
	if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) {
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr,
		    WDCTL_RST | WDCTL_IDS | WDCTL_4BIT);
		delay(2000);
	}
	(void) bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_error], 0);
	if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) 
		bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, wd_aux_ctlr,
		    WDCTL_4BIT | WDCTL_IDS);
	delay(10);	/* 400ns delay */
	if (poll != RESET_SLEEP) {
		/* ACK interrupt in case there is one pending left */
		if (wdc->irqack)
			wdc->irqack(chp);
		splx(s);
	}
}

static int
__wdcwait_reset(struct ata_channel *chp, int drv_mask, int poll)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	int timeout, nloop;
	u_int8_t st0 = 0, st1 = 0;
#ifdef ATADEBUG
	u_int8_t sc0 = 0, sn0 = 0, cl0 = 0, ch0 = 0;
	u_int8_t sc1 = 0, sn1 = 0, cl1 = 0, ch1 = 0;
#endif
	if (poll)
		nloop = WDCNDELAY_RST;
	else
		nloop = WDC_RESET_WAIT * hz / 1000;
	/* wait for BSY to deassert */
	for (timeout = 0; timeout < nloop; timeout++) {
		if ((drv_mask & 0x01) != 0) {
			if (wdc->select)
				wdc->select(chp,0);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM); /* master */
			delay(10);
			st0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
#ifdef ATADEBUG
			sc0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_seccnt], 0);
			sn0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			cl0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			ch0 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_hi], 0);
#endif
		}
		if ((drv_mask & 0x02) != 0) {
			if (wdc->select)
				wdc->select(chp,1);
			bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh],
			    0, WDSD_IBM | 0x10); /* slave */
			delay(10);
			st1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_status], 0);
#ifdef ATADEBUG
			sc1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_seccnt], 0);
			sn1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0);
			cl1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0);
			ch1 = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_hi], 0);
#endif
		}

		if ((drv_mask & 0x01) == 0) {
			/* no master */
			if ((drv_mask & 0x02) != 0 && (st1 & WDCS_BSY) == 0) {
				/* No master, slave is ready, it's done */
				goto end;
			}
			if ((drv_mask & 0x02) == 0) {
				/* No master, no slave: it's done */
				goto end;
			}
		} else if ((drv_mask & 0x02) == 0) {
			/* no slave */
			if ((drv_mask & 0x01) != 0 && (st0 & WDCS_BSY) == 0) {
				/* No slave, master is ready, it's done */
				goto end;
			}
		} else {
			/* Wait for both master and slave to be ready */
			if ((st0 & WDCS_BSY) == 0 && (st1 & WDCS_BSY) == 0) {
				goto end;
			}
		}
		if (poll)
			delay(WDCDELAY);
		else
			tsleep(&nloop, PRIBIO, "atarst", 1);
	}
	/* Reset timed out. Maybe it's because drv_mask was not right */
	if (st0 & WDCS_BSY)
		drv_mask &= ~0x01;
	if (st1 & WDCS_BSY)
		drv_mask &= ~0x02;
end:
	ATADEBUG_PRINT(("%s:%d:0: after reset, sc=0x%x sn=0x%x "
	    "cl=0x%x ch=0x%x\n",
	     device_xname(chp->ch_atac->atac_dev),
	     chp->ch_channel, sc0, sn0, cl0, ch0), DEBUG_PROBE);
	ATADEBUG_PRINT(("%s:%d:1: after reset, sc=0x%x sn=0x%x "
	    "cl=0x%x ch=0x%x\n",
	     device_xname(chp->ch_atac->atac_dev),
	     chp->ch_channel, sc1, sn1, cl1, ch1), DEBUG_PROBE);

	ATADEBUG_PRINT(("%s:%d: wdcwait_reset() end, st0=0x%x st1=0x%x\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    st0, st1), DEBUG_PROBE);

	return drv_mask;
}

/*
 * Wait for a drive to be !BSY, and have mask in its status register.
 * return -1 for a timeout after "timeout" ms.
 */
static int
__wdcwait(struct ata_channel *chp, int mask, int bits, int timeout)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	u_char status;
	int xtime = 0;

	ATADEBUG_PRINT(("__wdcwait %s:%d\n",
			device_xname(chp->ch_atac->atac_dev),
			chp->ch_channel), DEBUG_STATUS);
	chp->ch_error = 0;

	timeout = timeout * 1000 / WDCDELAY; /* delay uses microseconds */

	for (;;) {
		chp->ch_status = status =
		    bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_status], 0);
		if ((status & (WDCS_BSY | mask)) == bits)
			break;
		if (++xtime > timeout) {
			ATADEBUG_PRINT(("__wdcwait: timeout (time=%d), "
			    "status %x error %x (mask 0x%x bits 0x%x)\n",
			    xtime, status,
			    bus_space_read_1(wdr->cmd_iot,
				wdr->cmd_iohs[wd_error], 0), mask, bits),
			    DEBUG_STATUS | DEBUG_PROBE | DEBUG_DELAY);
			return(WDCWAIT_TOUT);
		}
		delay(WDCDELAY);
	}
#ifdef ATADEBUG
	if (xtime > 0 && (atadebug_mask & DEBUG_DELAY))
		printf("__wdcwait: did busy-wait, time=%d\n", xtime);
#endif
	if (status & WDCS_ERR)
		chp->ch_error = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_error], 0);
#ifdef WDCNDELAY_DEBUG
	/* After autoconfig, there should be no long delays. */
	if (!cold && xtime > WDCNDELAY_DEBUG) {
		struct ata_xfer *xfer = chp->ch_queue->active_xfer;
		if (xfer == NULL)
			printf("%s channel %d: warning: busy-wait took %dus\n",
			    device_xname(chp->ch_atac->atac_dev),
			    chp->ch_channel, WDCDELAY * xtime);
		else
			printf("%s:%d:%d: warning: busy-wait took %dus\n",
			    device_xname(chp->ch_atac->atac_dev),
			    chp->ch_channel, xfer->c_drive,
			    WDCDELAY * xtime);
	}
#endif
	return(WDCWAIT_OK);
}

/*
 * Call __wdcwait(), polling using tsleep() or waking up the kernel
 * thread if possible
 */
int
wdcwait(struct ata_channel *chp, int mask, int bits, int timeout, int flags)
{
	int error, i, timeout_hz = mstohz(timeout);

	if (timeout_hz == 0 ||
	    (flags & (AT_WAIT | AT_POLL)) == AT_POLL)
		error = __wdcwait(chp, mask, bits, timeout);
	else {
		error = __wdcwait(chp, mask, bits, WDCDELAY_POLL);
		if (error != 0) {
			if ((chp->ch_flags & ATACH_TH_RUN) ||
			    (flags & AT_WAIT)) {
				/*
				 * we're running in the channel thread
				 * or some userland thread context
				 */
				for (i = 0; i < timeout_hz; i++) {
					if (__wdcwait(chp, mask, bits,
					    WDCDELAY_POLL) == 0) {
						error = 0;
						break;
					}
					tsleep(&chp, PRIBIO, "atapoll", 1);
				}
			} else {
				/*
				 * we're probably in interrupt context,
				 * ask the thread to come back here
				 */
#ifdef DIAGNOSTIC
				if (chp->ch_queue->queue_freeze > 0)
					panic("wdcwait: queue_freeze");
#endif
				chp->ch_queue->queue_freeze++;
				wakeup(&chp->ch_thread);
				return(WDCWAIT_THR);
			}
		}
	}
	return (error);
}


#if NATA_DMA
/*
 * Busy-wait for DMA to complete
 */
int
wdc_dmawait(struct ata_channel *chp, struct ata_xfer *xfer, int timeout)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	int xtime;

	for (xtime = 0;  xtime < timeout * 1000 / WDCDELAY; xtime++) {
		wdc->dma_status =
		    (*wdc->dma_finish)(wdc->dma_arg,
			chp->ch_channel, xfer->c_drive, WDC_DMAEND_END);
		if ((wdc->dma_status & WDC_DMAST_NOIRQ) == 0)
			return 0;
		delay(WDCDELAY);
	}
	/* timeout, force a DMA halt */
	wdc->dma_status = (*wdc->dma_finish)(wdc->dma_arg,
	    chp->ch_channel, xfer->c_drive, WDC_DMAEND_ABRT);
	return 1;
}
#endif

void
wdctimeout(void *arg)
{
	struct ata_channel *chp = (struct ata_channel *)arg;
#if NATA_DMA || NATA_PIOBM
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
#endif
	struct ata_xfer *xfer = chp->ch_queue->active_xfer;
	int s;

	ATADEBUG_PRINT(("wdctimeout\n"), DEBUG_FUNCS);

	s = splbio();
	if ((chp->ch_flags & ATACH_IRQ_WAIT) != 0) {
		__wdcerror(chp, "lost interrupt");
		printf("\ttype: %s tc_bcount: %d tc_skip: %d\n",
		    (xfer->c_flags & C_ATAPI) ?  "atapi" : "ata",
		    xfer->c_bcount,
		    xfer->c_skip);
#if NATA_DMA || NATA_PIOBM
		if (chp->ch_flags & ATACH_DMA_WAIT) {
			wdc->dma_status =
			    (*wdc->dma_finish)(wdc->dma_arg,
				chp->ch_channel, xfer->c_drive,
				WDC_DMAEND_ABRT);
			chp->ch_flags &= ~ATACH_DMA_WAIT;
		}
#endif
		/*
		 * Call the interrupt routine. If we just missed an interrupt,
		 * it will do what's needed. Else, it will take the needed
		 * action (reset the device).
		 * Before that we need to reinstall the timeout callback,
		 * in case it will miss another irq while in this transfer
		 * We arbitray chose it to be 1s
		 */
		callout_reset(&chp->ch_callout, hz, wdctimeout, chp);
		xfer->c_flags |= C_TIMEOU;
		chp->ch_flags &= ~ATACH_IRQ_WAIT;
		KASSERT(xfer->c_intr != NULL);
		xfer->c_intr(chp, xfer, 1);
	} else
		__wdcerror(chp, "missing untimeout");
	splx(s);
}

int
wdc_exec_command(struct ata_drive_datas *drvp, struct ata_command *ata_c)
{
	struct ata_channel *chp = drvp->chnl_softc;
	struct ata_xfer *xfer;
	int s, ret;

	ATADEBUG_PRINT(("wdc_exec_command %s:%d:%d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    drvp->drive), DEBUG_FUNCS);

	/* set up an xfer and queue. Wait for completion */
	xfer = ata_get_xfer(ata_c->flags & AT_WAIT ? ATAXF_CANSLEEP :
	    ATAXF_NOSLEEP);
	if (xfer == NULL) {
		return ATACMD_TRY_AGAIN;
	 }

	if (chp->ch_atac->atac_cap & ATAC_CAP_NOIRQ)
		ata_c->flags |= AT_POLL;
	if (ata_c->flags & AT_POLL)
		xfer->c_flags |= C_POLL;
	if (ata_c->flags & AT_WAIT)
		xfer->c_flags |= C_WAIT;
	xfer->c_drive = drvp->drive;
	xfer->c_databuf = ata_c->data;
	xfer->c_bcount = ata_c->bcount;
	xfer->c_cmd = ata_c;
	xfer->c_start = __wdccommand_start;
	xfer->c_intr = __wdccommand_intr;
	xfer->c_kill_xfer = __wdccommand_kill_xfer;

	s = splbio();
	ata_exec_xfer(chp, xfer);
#ifdef DIAGNOSTIC
	if ((ata_c->flags & AT_POLL) != 0 &&
	    (ata_c->flags & AT_DONE) == 0)
		panic("wdc_exec_command: polled command not done");
#endif
	if (ata_c->flags & AT_DONE) {
		ret = ATACMD_COMPLETE;
	} else {
		if (ata_c->flags & AT_WAIT) {
			while ((ata_c->flags & AT_DONE) == 0) {
				tsleep(ata_c, PRIBIO, "wdccmd", 0);
			}
			ret = ATACMD_COMPLETE;
		} else {
			ret = ATACMD_QUEUED;
		}
	}
	splx(s);
	return ret;
}

static void
__wdccommand_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	int drive = xfer->c_drive;
	int wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
	struct ata_command *ata_c = xfer->c_cmd;

	ATADEBUG_PRINT(("__wdccommand_start %s:%d:%d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    xfer->c_drive),
	    DEBUG_FUNCS);

	if (wdc->select)
		wdc->select(chp,drive);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    WDSD_IBM | (drive << 4));
	switch(wdcwait(chp, ata_c->r_st_bmask | WDCS_DRQ,
	    ata_c->r_st_bmask, ata_c->timeout, wait_flags)) {
	case WDCWAIT_OK:
		break;
	case WDCWAIT_TOUT:
		ata_c->flags |= AT_TIMEOU;
		__wdccommand_done(chp, xfer);
		return;
	case WDCWAIT_THR:
		return;
	}
	if (ata_c->flags & AT_POLL) {
		/* polled command, disable interrupts */
		if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) 
			bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, 
			    wd_aux_ctlr, WDCTL_4BIT | WDCTL_IDS);
	}
	if ((ata_c->flags & AT_LBA48) != 0) {
		wdccommandext(chp, drive, ata_c->r_command,
		    ata_c->r_lba, ata_c->r_count, ata_c->r_features,
		    ata_c->r_device & ~0x10);
	} else {
		wdccommand(chp, drive, ata_c->r_command,
		    (ata_c->r_lba >> 8) & 0xffff,
		    WDSD_IBM | (drive << 4) |
		    (((ata_c->flags & AT_LBA) != 0) ? WDSD_LBA : 0) |
		    ((ata_c->r_lba >> 24) & 0x0f),
		    ata_c->r_lba & 0xff,
		    ata_c->r_count & 0xff,
		    ata_c->r_features & 0xff);
	}

	if ((ata_c->flags & AT_POLL) == 0) {
		chp->ch_flags |= ATACH_IRQ_WAIT; /* wait for interrupt */
		callout_reset(&chp->ch_callout, ata_c->timeout / 1000 * hz,
		    wdctimeout, chp);
		return;
	}
	/*
	 * Polled command. Wait for drive ready or drq. Done in intr().
	 * Wait for at last 400ns for status bit to be valid.
	 */
	delay(10);	/* 400ns delay */
	__wdccommand_intr(chp, xfer, 0);
}

static int
__wdccommand_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct ata_command *ata_c = xfer->c_cmd;
	int bcount = ata_c->bcount;
	char *data = ata_c->data;
	int wflags;
	int drive_flags;

	if (ata_c->r_command == WDCC_IDENTIFY ||
	    ata_c->r_command == ATAPI_IDENTIFY_DEVICE) {
		/*
		 * The IDENTIFY data has been designed as an array of
		 * u_int16_t, so we can byteswap it on the fly.
		 * Historically it's what we have always done so keeping it
		 * here ensure binary backward compatibility.
		 */
		 drive_flags = ATA_DRIVE_NOSTREAM |
				chp->ch_drive[xfer->c_drive].drive_flags;
	} else {
		/*
		 * Other data structure are opaque and should be transfered
		 * as is.
		 */
		drive_flags = chp->ch_drive[xfer->c_drive].drive_flags;
	}

#ifdef WDC_NO_IDS
	wflags = AT_POLL;
#else
	if ((ata_c->flags & (AT_WAIT | AT_POLL)) == (AT_WAIT | AT_POLL)) {
		/* both wait and poll, we can tsleep here */
		wflags = AT_WAIT | AT_POLL;
	} else {
		wflags = AT_POLL;
	}
#endif

 again:
	ATADEBUG_PRINT(("__wdccommand_intr %s:%d:%d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    xfer->c_drive), DEBUG_INTR);
	/*
	 * after a ATAPI_SOFT_RESET, the device will have released the bus.
	 * Reselect again, it doesn't hurt for others commands, and the time
	 * penalty for the extra register write is acceptable,
	 * wdc_exec_command() isn't called often (mostly for autoconfig)
	 */
	if ((xfer->c_flags & C_ATAPI) != 0) {
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
		    WDSD_IBM | (xfer->c_drive << 4));
	}
	if ((ata_c->flags & AT_XFDONE) != 0) {
		/*
		 * We have completed a data xfer. The drive should now be
		 * in its initial state
		 */
		if (wdcwait(chp, ata_c->r_st_bmask | WDCS_DRQ,
		    ata_c->r_st_bmask, (irq == 0)  ? ata_c->timeout : 0,
		    wflags) ==  WDCWAIT_TOUT) {
			if (irq && (xfer->c_flags & C_TIMEOU) == 0)
				return 0; /* IRQ was not for us */
			ata_c->flags |= AT_TIMEOU;
		}
		goto out;
	}
	if (wdcwait(chp, ata_c->r_st_pmask, ata_c->r_st_pmask,
	     (irq == 0)  ? ata_c->timeout : 0, wflags) == WDCWAIT_TOUT) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0; /* IRQ was not for us */
		ata_c->flags |= AT_TIMEOU;
		goto out;
	}
	if (wdc->irqack)
		wdc->irqack(chp);
	if (ata_c->flags & AT_READ) {
		if ((chp->ch_status & WDCS_DRQ) == 0) {
			ata_c->flags |= AT_TIMEOU;
			goto out;
		}
		wdc->datain_pio(chp, drive_flags, data, bcount);
		/* at this point the drive should be in its initial state */
		ata_c->flags |= AT_XFDONE;
		/*
		 * XXX checking the status register again here cause some
		 * hardware to timeout.
		 */
	} else if (ata_c->flags & AT_WRITE) {
		if ((chp->ch_status & WDCS_DRQ) == 0) {
			ata_c->flags |= AT_TIMEOU;
			goto out;
		}
		wdc->dataout_pio(chp, drive_flags, data, bcount);
		ata_c->flags |= AT_XFDONE;
		if ((ata_c->flags & AT_POLL) == 0) {
			chp->ch_flags |= ATACH_IRQ_WAIT; /* wait for interrupt */
			callout_reset(&chp->ch_callout,
			    mstohz(ata_c->timeout), wdctimeout, chp);
			return 1;
		} else {
			goto again;
		}
	}
 out:
	__wdccommand_done(chp, xfer);
	return 1;
}

static void
__wdccommand_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];
	struct ata_command *ata_c = xfer->c_cmd;

	ATADEBUG_PRINT(("__wdccommand_done %s:%d:%d flags 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    ata_c->flags), DEBUG_FUNCS);


	if (chp->ch_status & WDCS_DWF)
		ata_c->flags |= AT_DF;
	if (chp->ch_status & WDCS_ERR) {
		ata_c->flags |= AT_ERROR;
		ata_c->r_error = chp->ch_error;
	}
	if ((ata_c->flags & AT_READREG) != 0 &&
	    device_is_active(atac->atac_dev) &&
	    (ata_c->flags & (AT_ERROR | AT_DF)) == 0) {
		ata_c->r_status = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_status], 0);
		ata_c->r_error = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_error], 0);
		ata_c->r_count = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_seccnt], 0);
		ata_c->r_lba = (uint64_t)bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_sector], 0) << 0;
		ata_c->r_lba |= (uint64_t)bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_cyl_lo], 0) << 8;
		ata_c->r_lba |= (uint64_t)bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_cyl_hi], 0) << 16;
		ata_c->r_device = bus_space_read_1(wdr->cmd_iot,
		    wdr->cmd_iohs[wd_sdh], 0);

		if ((ata_c->flags & AT_LBA48) != 0) {
			if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) {
				if ((ata_c->flags & AT_POLL) != 0)
					bus_space_write_1(wdr->ctl_iot, 
					    wdr->ctl_ioh, wd_aux_ctlr,
					    WDCTL_HOB|WDCTL_4BIT|WDCTL_IDS);
				else
					bus_space_write_1(wdr->ctl_iot, 
					    wdr->ctl_ioh, wd_aux_ctlr, 
					    WDCTL_HOB|WDCTL_4BIT);
			}
			ata_c->r_count |= bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_seccnt], 0) << 8;
			ata_c->r_lba |= (uint64_t)bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_sector], 0) << 24;
			ata_c->r_lba |= (uint64_t)bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_lo], 0) << 32;
			ata_c->r_lba |= (uint64_t)bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_cyl_hi], 0) << 40;
			if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) {
				if ((ata_c->flags & AT_POLL) != 0)
					bus_space_write_1(wdr->ctl_iot, 
					    wdr->ctl_ioh, wd_aux_ctlr, 
					    WDCTL_4BIT|WDCTL_IDS);
				else
					bus_space_write_1(wdr->ctl_iot, 
					    wdr->ctl_ioh, wd_aux_ctlr, 
					    WDCTL_4BIT);
			}
		} else {
			ata_c->r_lba |=
			    (uint64_t)(ata_c->r_device & 0x0f) << 24;
		}
		ata_c->r_device &= 0xf0;
	}
	callout_stop(&chp->ch_callout);
	chp->ch_queue->active_xfer = NULL;
	if (ata_c->flags & AT_POLL) {
		/* enable interrupts */
		if (! (wdc->cap & WDC_CAPABILITY_NO_AUXCTL)) 
			bus_space_write_1(wdr->ctl_iot, wdr->ctl_ioh, 
			    wd_aux_ctlr, WDCTL_4BIT);
		delay(10); /* some drives need a little delay here */
	}
	if (chp->ch_drive[xfer->c_drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		__wdccommand_kill_xfer(chp, xfer, KILL_GONE);
		chp->ch_drive[xfer->c_drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	} else
		__wdccommand_done_end(chp, xfer);
}

static void
__wdccommand_done_end(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct ata_command *ata_c = xfer->c_cmd;

	ata_c->flags |= AT_DONE;
	ata_free_xfer(chp, xfer);
	if (ata_c->flags & AT_WAIT)
		wakeup(ata_c);
	else if (ata_c->callback)
		ata_c->callback(ata_c->callback_arg);
	atastart(chp);
	return;
}

static void
__wdccommand_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer,
    int reason)
{
	struct ata_command *ata_c = xfer->c_cmd;

	switch (reason) {
	case KILL_GONE:
		ata_c->flags |= AT_GONE;
		break;
	case KILL_RESET:
		ata_c->flags |= AT_RESET;
		break;
	default:
		printf("__wdccommand_kill_xfer: unknown reason %d\n",
		    reason);
		panic("__wdccommand_kill_xfer");
	}
	__wdccommand_done_end(chp, xfer);
}

/*
 * Send a command. The drive should be ready.
 * Assumes interrupts are blocked.
 */
void
wdccommand(struct ata_channel *chp, u_int8_t drive, u_int8_t command,
    u_int16_t cylin, u_int8_t head, u_int8_t sector, u_int8_t count,
    u_int8_t features)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];

	ATADEBUG_PRINT(("wdccommand %s:%d:%d: command=0x%x cylin=%d head=%d "
	    "sector=%d count=%d features=%d\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel, drive,
	    command, cylin, head, sector, count, features), DEBUG_FUNCS);

	if (wdc->select)
		wdc->select(chp,drive);

	/* Select drive, head, and addressing mode. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    WDSD_IBM | (drive << 4) | head);
	/* Load parameters into the wd_features register. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_features], 0,
	    features);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_seccnt], 0, count);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sector], 0, sector);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_cyl_lo], 0, cylin);
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_cyl_hi],
	    0, cylin >> 8);

	/* Send command. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_command], 0, command);
	return;
}

/*
 * Send a 48-bit addressing command. The drive should be ready.
 * Assumes interrupts are blocked.
 */
void
wdccommandext(struct ata_channel *chp, u_int8_t drive, u_int8_t command,
    u_int64_t blkno, u_int16_t count, u_int16_t features, u_int8_t device)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];

	ATADEBUG_PRINT(("wdccommandext %s:%d:%d: command=0x%02x "
	    "blkno=0x%012"PRIx64" count=0x%04x features=0x%04x "
	    "device=0x%02x\n", device_xname(chp->ch_atac->atac_dev),
	    chp->ch_channel, drive, command, blkno, count, features, device),
	    DEBUG_FUNCS);

	KASSERT(drive < wdc->wdc_maxdrives);

	if (wdc->select)
		wdc->select(chp,drive);

	/* Select drive, head, and addressing mode. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    (drive << 4) | device);

	if (wdc->cap & WDC_CAPABILITY_WIDEREGS) {
		bus_space_write_2(wdr->cmd_iot, wdr->cmd_iohs[wd_features],
		    0, features);
		bus_space_write_2(wdr->cmd_iot, wdr->cmd_iohs[wd_seccnt],
		    0, count);
		bus_space_write_2(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_lo],
		    0, (((blkno >> 16) & 0xff00) | (blkno & 0x00ff)));
		bus_space_write_2(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_mi],
		    0, (((blkno >> 24) & 0xff00) | ((blkno >> 8) & 0x00ff)));
		bus_space_write_2(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_hi],
		    0, (((blkno >> 32) & 0xff00) | ((blkno >> 16) & 0x00ff)));
	} else {
		/* previous */
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_features],
		    0, features >> 8);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_seccnt],
		    0, count >> 8);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_lo],
		    0, blkno >> 24);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_mi],
		    0, blkno >> 32);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_hi],
		    0, blkno >> 40);

		/* current */
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_features],
		    0, features);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_seccnt],
		    0, count);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_lo],
		    0, blkno);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_mi],
		    0, blkno >> 8);
		bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_lba_hi],
		    0, blkno >> 16);
	}

	/* Send command. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_command], 0, command);
	return;
}

/*
 * Simplified version of wdccommand().  Unbusy/ready/drq must be
 * tested by the caller.
 */
void
wdccommandshort(struct ata_channel *chp, int drive, int command)
{
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct wdc_regs *wdr = &wdc->regs[chp->ch_channel];

	ATADEBUG_PRINT(("wdccommandshort %s:%d:%d command 0x%x\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel, drive,
	    command), DEBUG_FUNCS);

	if (wdc->select)
		wdc->select(chp,drive);

	/* Select drive. */
	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_sdh], 0,
	    WDSD_IBM | (drive << 4));

	bus_space_write_1(wdr->cmd_iot, wdr->cmd_iohs[wd_command], 0, command);
}

static void
__wdcerror(struct ata_channel *chp, const char *msg)
{
	struct atac_softc *atac = chp->ch_atac;
	struct ata_xfer *xfer = chp->ch_queue->active_xfer;

	if (xfer == NULL)
		aprint_error("%s:%d: %s\n", device_xname(atac->atac_dev),
		    chp->ch_channel, msg);
	else
		aprint_error("%s:%d:%d: %s\n", device_xname(atac->atac_dev),
		    chp->ch_channel, xfer->c_drive, msg);
}

/*
 * the bit bucket
 */
void
wdcbit_bucket(struct ata_channel *chp, int size)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

	for (; size >= 2; size -= 2)
		(void)bus_space_read_2(wdr->cmd_iot, wdr->cmd_iohs[wd_data], 0);
	if (size)
		(void)bus_space_read_1(wdr->cmd_iot, wdr->cmd_iohs[wd_data], 0);
}

static void
wdc_datain_pio(struct ata_channel *chp, int flags, void *bf, size_t len)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

#ifndef __NO_STRICT_ALIGNMENT
	if ((uintptr_t)bf & 1)
		goto unaligned;
	if ((flags & ATA_DRIVE_CAP32) && ((uintptr_t)bf & 3))
		goto unaligned;
#endif

	if (flags & ATA_DRIVE_NOSTREAM) {
		if ((flags & ATA_DRIVE_CAP32) && len > 3) {
			bus_space_read_multi_4(wdr->data32iot,
			    wdr->data32ioh, 0, bf, len >> 2);
			bf = (char *)bf + (len & ~3);
			len &= 3;
		}
		if (len > 1) {
			bus_space_read_multi_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, bf, len >> 1);
			bf = (char *)bf + (len & ~1);
			len &= 1;
		}
	} else {
		if ((flags & ATA_DRIVE_CAP32) && len > 3) {
			bus_space_read_multi_stream_4(wdr->data32iot,
			    wdr->data32ioh, 0, bf, len >> 2);
			bf = (char *)bf + (len & ~3);
			len &= 3;
		}
		if (len > 1) {
			bus_space_read_multi_stream_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, bf, len >> 1);
			bf = (char *)bf + (len & ~1);
			len &= 1;
		}
	}
	if (len)
		*((uint8_t *)bf) = bus_space_read_1(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0);
	return;

#ifndef __NO_STRICT_ALIGNMENT
unaligned:
	if (flags & ATA_DRIVE_NOSTREAM) {
		if (flags & ATA_DRIVE_CAP32) {
			while (len > 3) {
				uint32_t val;

				val = bus_space_read_4(wdr->data32iot,
				    wdr->data32ioh, 0);
				memcpy(bf, &val, 4);
				bf = (char *)bf + 4;
				len -= 4;
			}
		}
		while (len > 1) {
			uint16_t val;

			val = bus_space_read_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0);
			memcpy(bf, &val, 2);
			bf = (char *)bf + 2;
			len -= 2;
		}
	} else {
		if (flags & ATA_DRIVE_CAP32) {
			while (len > 3) {
				uint32_t val;

				val = bus_space_read_stream_4(wdr->data32iot,
				    wdr->data32ioh, 0);
				memcpy(bf, &val, 4);
				bf = (char *)bf + 4;
				len -= 4;
			}
		}
		while (len > 1) {
			uint16_t val;

			val = bus_space_read_stream_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0);
			memcpy(bf, &val, 2);
			bf = (char *)bf + 2;
			len -= 2;
		}
	}
#endif
}

static void
wdc_dataout_pio(struct ata_channel *chp, int flags, void *bf, size_t len)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

#ifndef __NO_STRICT_ALIGNMENT
	if ((uintptr_t)bf & 1)
		goto unaligned;
	if ((flags & ATA_DRIVE_CAP32) && ((uintptr_t)bf & 3))
		goto unaligned;
#endif

	if (flags & ATA_DRIVE_NOSTREAM) {
		if (flags & ATA_DRIVE_CAP32) {
			bus_space_write_multi_4(wdr->data32iot,
			    wdr->data32ioh, 0, bf, len >> 2);
			bf = (char *)bf + (len & ~3);
			len &= 3;
		}
		if (len) {
			bus_space_write_multi_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, bf, len >> 1);
		}
	} else {
		if (flags & ATA_DRIVE_CAP32) {
			bus_space_write_multi_stream_4(wdr->data32iot,
			    wdr->data32ioh, 0, bf, len >> 2);
			bf = (char *)bf + (len & ~3);
			len &= 3;
		}
		if (len) {
			bus_space_write_multi_stream_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, bf, len >> 1);
		}
	}
	return;

#ifndef __NO_STRICT_ALIGNMENT
unaligned:
	if (flags & ATA_DRIVE_NOSTREAM) {
		if (flags & ATA_DRIVE_CAP32) {
			while (len > 3) {
				uint32_t val;

				memcpy(&val, bf, 4);
				bus_space_write_4(wdr->data32iot,
				    wdr->data32ioh, 0, val);
				bf = (char *)bf + 4;
				len -= 4;
			}
		}
		while (len > 1) {
			uint16_t val;

			memcpy(&val, bf, 2);
			bus_space_write_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, val);
			bf = (char *)bf + 2;
			len -= 2;
		}
	} else {
		if (flags & ATA_DRIVE_CAP32) {
			while (len > 3) {
				uint32_t val;

				memcpy(&val, bf, 4);
				bus_space_write_stream_4(wdr->data32iot,
				    wdr->data32ioh, 0, val);
				bf = (char *)bf + 4;
				len -= 4;
			}
		}
		while (len > 1) {
			uint16_t val;

			memcpy(&val, bf, 2);
			bus_space_write_stream_2(wdr->cmd_iot,
			    wdr->cmd_iohs[wd_data], 0, val);
			bf = (char *)bf + 2;
			len -= 2;
		}
	}
#endif
}
