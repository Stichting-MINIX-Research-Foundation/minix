/*	$NetBSD: atapi_base.c,v 1.29 2014/10/18 08:33:28 snj Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by Jason R. Thorpe of the Numerical Aerospace
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atapi_base.c,v 1.29 2014/10/18 08:33:28 snj Exp $");

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
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/atapiconf.h>
#include <dev/scsipi/scsipi_base.h>

/*
 * Look at the returned sense and act on the error, determining
 * the unix error number to pass back.  (0 = report no error)
 *
 * THIS IS THE DEFAULT ERROR HANDLER
 */
int
atapi_interpret_sense(struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;
	int key, error;
	const char *msg = NULL;

	/*
	 * If the device has its own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (periph->periph_switch->psw_error != NULL) {
		SC_DEBUG(periph, SCSIPI_DB2,
		    ("calling private err_handler()\n"));
		error = (*periph->periph_switch->psw_error)(xs);
		if (error != EJUSTRETURN)
			return (error);
	}
	/*
	 * otherwise use the default, call the generic sense handler if we have
	 * more than the sense key
	 */
	if (xs->error == XS_SENSE)
		return (scsipi_interpret_sense(xs));

	key = (xs->sense.atapi_sense & 0xf0) >> 4;
	switch (key) {
		case SKEY_RECOVERED_ERROR:
			msg = "soft error (corrected)";
		case SKEY_NO_SENSE:
			if (xs->resid == xs->datalen)
				xs->resid = 0;  /* not short read */
			error = 0;
			break;
		case SKEY_NOT_READY:
			if ((periph->periph_flags & PERIPH_REMOVABLE) != 0)
				periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
			if ((xs->xs_control & XS_CTL_IGNORE_NOT_READY) != 0)
				return (0);
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (EIO);
			msg = "not ready";
			error = EIO;
			break;
		case SKEY_MEDIUM_ERROR: /* MEDIUM ERROR */
			msg = "medium error";
			error = EIO;
			break;
		case SKEY_HARDWARE_ERROR:
			msg = "non-media hardware failure";
			error = EIO;
			break;
		case SKEY_ILLEGAL_REQUEST:
			if ((xs->xs_control &
			     XS_CTL_IGNORE_ILLEGAL_REQUEST) != 0)
				return (0);
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (EIO);
			msg = "illegal request";
			error = EINVAL;
			break;
		case SKEY_UNIT_ATTENTION:
			if ((periph->periph_flags & PERIPH_REMOVABLE) != 0)
				periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
			if ((xs->xs_control &
			     XS_CTL_IGNORE_MEDIA_CHANGE) != 0 ||
			    /* XXX Should reupload any transient state. */
			    (periph->periph_flags & PERIPH_REMOVABLE) == 0)
				return (ERESTART);
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (EIO);
			msg = "unit attention";
			error = EIO;
			break;
		case SKEY_DATA_PROTECT:
			msg = "readonly device";
			error = EROFS;
			break;
		case SKEY_ABORTED_COMMAND:
			msg = "command aborted";
			if (xs->xs_retries != 0) {
				xs->xs_retries--;
				error = ERESTART;
			} else
				error = EIO;
			break;
		default:
			error = EIO;
			break;
	}

	if (!key) {
		if (xs->sense.atapi_sense & 0x01) {
			/* Illegal length indication */
			msg = "ATA illegal length indication";
			error = EIO;
		}
		if (xs->sense.atapi_sense & 0x02) { /* vol overflow */
			msg = "ATA volume overflow";
			error = ENOSPC;
		}
		if (xs->sense.atapi_sense & 0x04) { /* Aborted command */
			msg = "ATA command aborted";
			if (xs->xs_retries != 0) {
				xs->xs_retries--;
				error = ERESTART;
			} else
				error = EIO;
		}
	}
	if (msg) {
		scsipi_printaddr(periph);
		printf("%s\n", msg);
	} else {
		if (error) {
			scsipi_printaddr(periph);
			printf("unknown error code %d\n",
			    xs->sense.atapi_sense);
		}
	}

	return (error);
}

/*
 * Utility routines often used in SCSI stuff
 */


/*
 * Print out the scsi_link structure's address info.
 */
void
atapi_print_addr(struct scsipi_periph *periph)
{
	struct scsipi_channel *chan = periph->periph_channel;
	struct scsipi_adapter *adapt = chan->chan_adapter;

	printf("%s(%s:%d:%d): ", periph->periph_dev != NULL ?
	    device_xname(periph->periph_dev) : "probe",
	    device_xname(adapt->adapt_dev),
	    chan->chan_channel, periph->periph_target);
}

/*
 * ask the atapi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
void
atapi_scsipi_cmd(struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;

	SC_DEBUG(periph, SCSIPI_DB2, ("atapi_cmd\n"));

	xs->cmdlen = (periph->periph_cap & PERIPH_CAP_CMD16) ? 16 : 12;
}
