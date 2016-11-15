/*	$NetBSD: scsipi_ioctl.c,v 1.68 2015/08/24 22:50:33 pooka Exp $	*/

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

/*
 * Contributed by HD Associates (hd@world.std.com).
 * Copyright (c) 1992, 1993 HD Associates
 *
 * Berkeley style copyright.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: scsipi_ioctl.c,v 1.68 2015/08/24 22:50:33 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_freebsd.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/fcntl.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>
#include <dev/scsipi/scsiconf.h>
#include <sys/scsiio.h>

#include "scsibus.h"
#include "atapibus.h"

struct scsi_ioctl {
	LIST_ENTRY(scsi_ioctl) si_list;
	struct buf si_bp;
	struct uio si_uio;
	struct iovec si_iov;
	scsireq_t si_screq;
	struct scsipi_periph *si_periph;
};

static LIST_HEAD(, scsi_ioctl) si_head;

static struct scsi_ioctl *
si_get(void)
{
	struct scsi_ioctl *si;
	int s;

	si = malloc(sizeof(struct scsi_ioctl), M_TEMP, M_WAITOK|M_ZERO);
	buf_init(&si->si_bp);
	s = splbio();
	LIST_INSERT_HEAD(&si_head, si, si_list);
	splx(s);
	return (si);
}

static void
si_free(struct scsi_ioctl *si)
{
	int s;

	s = splbio();
	LIST_REMOVE(si, si_list);
	splx(s);
	buf_destroy(&si->si_bp);
	free(si, M_TEMP);
}

static struct scsi_ioctl *
si_find(struct buf *bp)
{
	struct scsi_ioctl *si;
	int s;

	s = splbio();
	for (si = si_head.lh_first; si != 0; si = si->si_list.le_next)
		if (bp == &si->si_bp)
			break;
	splx(s);
	return (si);
}

/*
 * We let the user interpret his own sense in the generic scsi world.
 * This routine is called at interrupt time if the XS_CTL_USERCMD bit was set
 * in the flags passed to scsi_scsipi_cmd(). No other completion processing
 * takes place, even if we are running over another device driver.
 * The lower level routines that call us here, will free the xs and restart
 * the device's queue if such exists.
 */
void
scsipi_user_done(struct scsipi_xfer *xs)
{
	struct buf *bp;
	struct scsi_ioctl *si;
	scsireq_t *screq;
	struct scsipi_periph *periph = xs->xs_periph;
	int s;

	bp = xs->bp;
#ifdef DIAGNOSTIC
	if (bp == NULL) {
		scsipi_printaddr(periph);
		printf("user command with no buf\n");
		panic("scsipi_user_done");
	}
#endif
	si = si_find(bp);
#ifdef DIAGNOSTIC
	if (si == NULL) {
		scsipi_printaddr(periph);
		printf("user command with no ioctl\n");
		panic("scsipi_user_done");
	}
#endif

	screq = &si->si_screq;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("user-done\n"));

	screq->retsts = 0;
	screq->status = xs->status;
	switch (xs->error) {
	case XS_NOERROR:
		SC_DEBUG(periph, SCSIPI_DB3, ("no error\n"));
		screq->datalen_used =
		    xs->datalen - xs->resid;	/* probably rubbish */
		screq->retsts = SCCMD_OK;
		break;
	case XS_SENSE:
		SC_DEBUG(periph, SCSIPI_DB3, ("have sense\n"));
		screq->senselen_used = min(sizeof(xs->sense.scsi_sense),
		    SENSEBUFLEN);
		memcpy(screq->sense, &xs->sense.scsi_sense, screq->senselen);
		screq->retsts = SCCMD_SENSE;
		break;
	case XS_SHORTSENSE:
		SC_DEBUG(periph, SCSIPI_DB3, ("have short sense\n"));
		screq->senselen_used = min(sizeof(xs->sense.atapi_sense),
		    SENSEBUFLEN);
		memcpy(screq->sense, &xs->sense.scsi_sense, screq->senselen);
		screq->retsts = SCCMD_UNKNOWN; /* XXX need a shortsense here */
		break;
	case XS_DRIVER_STUFFUP:
		scsipi_printaddr(periph);
		printf("passthrough: adapter inconsistency\n");
		screq->retsts = SCCMD_UNKNOWN;
		break;
	case XS_SELTIMEOUT:
		SC_DEBUG(periph, SCSIPI_DB3, ("seltimeout\n"));
		screq->retsts = SCCMD_TIMEOUT;
		break;
	case XS_TIMEOUT:
		SC_DEBUG(periph, SCSIPI_DB3, ("timeout\n"));
		screq->retsts = SCCMD_TIMEOUT;
		break;
	case XS_BUSY:
		SC_DEBUG(periph, SCSIPI_DB3, ("busy\n"));
		screq->retsts = SCCMD_BUSY;
		break;
	default:
		scsipi_printaddr(periph);
		printf("unknown error category %d from adapter\n",
		    xs->error);
		screq->retsts = SCCMD_UNKNOWN;
		break;
	}

	if (xs->xs_control & XS_CTL_ASYNC) {
		s = splbio();
		scsipi_put_xs(xs);
		splx(s);
	}
}


/* Pseudo strategy function
 * Called by scsipi_do_ioctl() via physio/physstrat if there is to
 * be data transfered, and directly if there is no data transfer.
 *
 * Should I reorganize this so it returns to physio instead
 * of sleeping in scsiio_scsipi_cmd?  Is there any advantage, other
 * than avoiding the probable duplicate wakeup in iodone? [PD]
 *
 * No, seems ok to me... [JRE]
 * (I don't see any duplicate wakeups)
 *
 * Can't be used with block devices or raw_read/raw_write directly
 * from the cdevsw/bdevsw tables because they couldn't have added
 * the screq structure. [JRE]
 */
static void
scsistrategy(struct buf *bp)
{
	struct scsi_ioctl *si;
	scsireq_t *screq;
	struct scsipi_periph *periph;
	int error;
	int flags = 0;

	si = si_find(bp);
	if (si == NULL) {
		printf("scsistrategy: "
		    "No matching ioctl request found in queue\n");
		error = EINVAL;
		goto done;
	}
	screq = &si->si_screq;
	periph = si->si_periph;
	SC_DEBUG(periph, SCSIPI_DB2, ("user_strategy\n"));

	/*
	 * We're in trouble if physio tried to break up the transfer.
	 */
	if (bp->b_bcount != screq->datalen) {
		scsipi_printaddr(periph);
		printf("physio split the request.. cannot proceed\n");
		error = EIO;
		goto done;
	}

	if (screq->timeout == 0) {
		error = EINVAL;
		goto done;
	}

	if (screq->cmdlen > sizeof(struct scsipi_generic)) {
		scsipi_printaddr(periph);
		printf("cmdlen too big\n");
		error = EFAULT;
		goto done;
	}

	if ((screq->flags & SCCMD_READ) && screq->datalen > 0)
		flags |= XS_CTL_DATA_IN;
	if ((screq->flags & SCCMD_WRITE) && screq->datalen > 0)
		flags |= XS_CTL_DATA_OUT;
	if (screq->flags & SCCMD_TARGET)
		flags |= XS_CTL_TARGET;
	if (screq->flags & SCCMD_ESCAPE)
		flags |= XS_CTL_ESCAPE;

	error = scsipi_command(periph, (void *)screq->cmd, screq->cmdlen,
	    (void *)bp->b_data, screq->datalen,
	    0, /* user must do the retries *//* ignored */
	    screq->timeout, bp, flags | XS_CTL_USERCMD);

done:
	if (error)
		bp->b_resid = bp->b_bcount;
	bp->b_error = error;
	biodone(bp);
	return;
}

/*
 * Something (e.g. another driver) has called us
 * with a periph and a scsi-specific ioctl to perform,
 * better try.  If user-level type command, we must
 * still be running in the context of the calling process
 */
int
scsipi_do_ioctl(struct scsipi_periph *periph, dev_t dev, u_long cmd,
    void *addr, int flag, struct lwp *l)
{
	int error;

	SC_DEBUG(periph, SCSIPI_DB2, ("scsipi_do_ioctl(0x%lx)\n", cmd));

	if (addr == NULL)
		return EINVAL;

	/* Check for the safe-ness of this request. */
	switch (cmd) {
	case OSCIOCIDENTIFY:
	case SCIOCIDENTIFY:
		break;
	case SCIOCCOMMAND:
		if ((((scsireq_t *)addr)->flags & SCCMD_READ) == 0 &&
		    (flag & FWRITE) == 0)
			return (EBADF);
		break;
	default:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case SCIOCCOMMAND: {
		scsireq_t *screq = (scsireq_t *)addr;
		struct scsi_ioctl *si;
		int len;

		si = si_get();
		si->si_screq = *screq;
		si->si_periph = periph;
		len = screq->datalen;
		if (len) {
			si->si_iov.iov_base = screq->databuf;
			si->si_iov.iov_len = len;
			si->si_uio.uio_iov = &si->si_iov;
			si->si_uio.uio_iovcnt = 1;
			si->si_uio.uio_resid = len;
			si->si_uio.uio_offset = 0;
			si->si_uio.uio_rw =
			    (screq->flags & SCCMD_READ) ? UIO_READ : UIO_WRITE;
			if ((flag & FKIOCTL) == 0) {
				si->si_uio.uio_vmspace = l->l_proc->p_vmspace;
			} else {
				UIO_SETUP_SYSSPACE(&si->si_uio);
			}
			error = physio(scsistrategy, &si->si_bp, dev,
			    (screq->flags & SCCMD_READ) ? B_READ : B_WRITE,
			    periph->periph_channel->chan_adapter->adapt_minphys,
			    &si->si_uio);
		} else {
			/* if no data, no need to translate it.. */
			si->si_bp.b_flags = 0;
			si->si_bp.b_data = 0;
			si->si_bp.b_bcount = 0;
			si->si_bp.b_dev = dev;
			si->si_bp.b_proc = l->l_proc;
			scsistrategy(&si->si_bp);
			error = si->si_bp.b_error;
		}
		*screq = si->si_screq;
		si_free(si);
		return (error);
	}
	case SCIOCDEBUG: {
		int level = *((int *)addr);

		SC_DEBUG(periph, SCSIPI_DB3, ("debug set to %d\n", level));
		periph->periph_dbflags = 0;
		if (level & 1)
			periph->periph_dbflags |= SCSIPI_DB1;
		if (level & 2)
			periph->periph_dbflags |= SCSIPI_DB2;
		if (level & 4)
			periph->periph_dbflags |= SCSIPI_DB3;
		if (level & 8)
			periph->periph_dbflags |= SCSIPI_DB4;
		return (0);
	}
	case SCIOCRECONFIG:
	case SCIOCDECONFIG:
		return (EINVAL);
	case SCIOCIDENTIFY: {
		struct scsi_addr *sca = (struct scsi_addr *)addr;

		switch (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(periph))) {
		case SCSIPI_BUSTYPE_SCSI:
			sca->type = TYPE_SCSI;
			sca->addr.scsi.scbus =
			    device_unit(device_parent(periph->periph_dev));
			sca->addr.scsi.target = periph->periph_target;
			sca->addr.scsi.lun = periph->periph_lun;
			return (0);
		case SCSIPI_BUSTYPE_ATAPI:
			sca->type = TYPE_ATAPI;
			sca->addr.atapi.atbus =
			    device_unit(device_parent(periph->periph_dev));
			sca->addr.atapi.drive = periph->periph_target;
			return (0);
		}
		return (ENXIO);
	}
#if defined(COMPAT_12) || defined(COMPAT_FREEBSD)
	/* SCIOCIDENTIFY before ATAPI staff merge */
	case OSCIOCIDENTIFY: {
		struct oscsi_addr *sca = (struct oscsi_addr *)addr;

		switch (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(periph))) {
		case SCSIPI_BUSTYPE_SCSI:
			sca->scbus =
			    device_unit(device_parent(periph->periph_dev));
			sca->target = periph->periph_target;
			sca->lun = periph->periph_lun;
			return (0);
		}
		return (ENODEV);
	}
#endif
	default:
		return (ENOTTY);
	}

#ifdef DIAGNOSTIC
	panic("scsipi_do_ioctl: impossible");
#endif
}
