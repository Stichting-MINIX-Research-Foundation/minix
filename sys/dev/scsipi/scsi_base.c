/*	$NetBSD: scsi_base.c,v 1.90 2012/04/20 20:23:21 bouyer Exp $	*/

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
__KERNEL_RCSID(0, "$NetBSD: scsi_base.c,v 1.90 2012/04/20 20:23:21 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsipi_base.h>

static void scsi_print_xfer_mode(struct scsipi_periph *);
/*
 * Do a scsi operation, asking a device to run as SCSI-II if it can.
 */
int
scsi_change_def(struct scsipi_periph *periph, int flags)
{
	struct scsi_changedef cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = SCSI_CHANGE_DEFINITION;
	cmd.how = SC_SCSI_2;

	return (scsipi_command(periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    SCSIPIRETRIES, 100000, NULL, flags));
}

/*
 * ask the scsi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
void
scsi_scsipi_cmd(struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;

	SC_DEBUG(periph, SCSIPI_DB2, ("scsi_scsipi_cmd\n"));

	/*
	 * Set the LUN in the CDB if we have an older device.  We also
	 * set it for more modern SCSI-2 devices "just in case".
	 */
	if (periph->periph_version <= 2)
		xs->cmd->bytes[0] |=
		    ((periph->periph_lun << SCSI_CMD_LUN_SHIFT) &
			SCSI_CMD_LUN_MASK);
}

/*
 * Utility routines often used in SCSI stuff
 */

/*
 * Print out the periph's address info.
 */
void
scsi_print_addr(struct scsipi_periph *periph)
{
	struct scsipi_channel *chan = periph->periph_channel;
	struct scsipi_adapter *adapt = chan->chan_adapter;

	printf("%s(%s:%d:%d:%d): ", periph->periph_dev != NULL ?
	    device_xname(periph->periph_dev) : "probe",
	    device_xname(adapt->adapt_dev),
	    chan->chan_channel, periph->periph_target,
	    periph->periph_lun);
}

/*
 * Kill off all pending xfers for a periph.
 *
 * Must be called at splbio().
 */
void
scsi_kill_pending(struct scsipi_periph *periph)
{
}

/*
 * scsi_print_xfer_mode:
 *
 *	Print a parallel SCSI periph's capabilities.
 */
static void
scsi_print_xfer_mode(struct scsipi_periph *periph)
{
	int period, freq, speed, mbs;

	aprint_normal_dev(periph->periph_dev, "");
	if (periph->periph_mode & (PERIPH_CAP_SYNC | PERIPH_CAP_DT)) {
		period = scsipi_sync_factor_to_period(periph->periph_period);
		aprint_normal("sync (%d.%02dns offset %d)",
		    period / 100, period % 100, periph->periph_offset);
	} else
		aprint_normal("async");

	if (periph->periph_mode & PERIPH_CAP_WIDE32)
		aprint_normal(", 32-bit");
	else if (periph->periph_mode & (PERIPH_CAP_WIDE16 | PERIPH_CAP_DT))
		aprint_normal(", 16-bit");
	else
		aprint_normal(", 8-bit");

	if (periph->periph_mode & (PERIPH_CAP_SYNC | PERIPH_CAP_DT)) {
		freq = scsipi_sync_factor_to_freq(periph->periph_period);
		speed = freq;
		if (periph->periph_mode & PERIPH_CAP_WIDE32)
			speed *= 4;
		else if (periph->periph_mode &
		    (PERIPH_CAP_WIDE16 | PERIPH_CAP_DT))
			speed *= 2;
		mbs = speed / 1000;
		if (mbs > 0) {
			aprint_normal(" (%d.%03dMB/s)", mbs,
			    speed % 1000);
		} else
			aprint_normal(" (%dKB/s)", speed % 1000);
	}

	aprint_normal(" transfers");

	if (periph->periph_mode & PERIPH_CAP_TQING)
		aprint_normal(", tagged queueing");

	aprint_normal("\n");
}

/*
 * scsi_async_event_xfer_mode:
 *
 *	Update the xfer mode for all parallel SCSI periphs sharing the
 *	specified I_T Nexus.
 */
void
scsi_async_event_xfer_mode(struct scsipi_channel *chan, void *arg)
{
	struct scsipi_xfer_mode *xm = arg;
	struct scsipi_periph *periph;
	int lun, announce, mode, period, offset;

	for (lun = 0; lun < chan->chan_nluns; lun++) {
		periph = scsipi_lookup_periph(chan, xm->xm_target, lun);
		if (periph == NULL)
			continue;
		announce = 0;

		/*
		 * Clamp the xfer mode down to this periph's capabilities.
		 */
		mode = xm->xm_mode & periph->periph_cap;
		if (mode & PERIPH_CAP_SYNC) {
			period = xm->xm_period;
			offset = xm->xm_offset;
		} else {
			period = 0;
			offset = 0;
		}

		/*
		 * If we do not have a valid xfer mode yet, or the parameters
		 * are different, announce them.
		 */
		if ((periph->periph_flags & PERIPH_MODE_VALID) == 0 ||
		    periph->periph_mode != mode ||
		    periph->periph_period != period ||
		    periph->periph_offset != offset)
			announce = 1;

		periph->periph_mode = mode;
		periph->periph_period = period;
		periph->periph_offset = offset;
		periph->periph_flags |= PERIPH_MODE_VALID;

		if (announce)
			scsi_print_xfer_mode(periph);
	}
}

/*
 * scsipi_async_event_xfer_mode:
 *
 *	Update the xfer mode for all SAS/FC periphs sharing the
 *	specified I_T Nexus.
 */
void
scsi_fc_sas_async_event_xfer_mode(struct scsipi_channel *chan, void *arg)
{
	struct scsipi_xfer_mode *xm = arg;
	struct scsipi_periph *periph;
	int lun, announce, mode;

	for (lun = 0; lun < chan->chan_nluns; lun++) {
		periph = scsipi_lookup_periph(chan, xm->xm_target, lun);
		if (periph == NULL)
			continue;
		announce = 0;

		/*
		 * Clamp the xfer mode down to this periph's capabilities.
		 */
		mode = xm->xm_mode & periph->periph_cap;
		/*
		 * If we do not have a valid xfer mode yet, or the parameters
		 * are different, announce them.
		 */
		if ((periph->periph_flags & PERIPH_MODE_VALID) == 0 ||
		    periph->periph_mode != mode)
			announce = 1;

		periph->periph_mode = mode;
		periph->periph_flags |= PERIPH_MODE_VALID;

		if (announce &&
		    (periph->periph_mode & PERIPH_CAP_TQING) != 0) {
			aprint_normal_dev(periph->periph_dev,
			    "tagged queueing\n");
		}
	}
}
