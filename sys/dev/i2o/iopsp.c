/*	$NetBSD: iopsp.c,v 1.37 2015/08/16 19:22:33 msaitoh Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Raw SCSI device support for I2O.  IOPs present SCSI devices individually;
 * we group them by controlling port.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iopsp.c,v 1.37 2015/08/16 19:22:33 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/scsiio.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>
#include <dev/i2o/iopspvar.h>

static void	iopsp_adjqparam(device_t, int);
static void	iopsp_attach(device_t, device_t, void *);
static void	iopsp_intr(device_t, struct iop_msg *, void *);
static int	iopsp_ioctl(struct scsipi_channel *, u_long,
			    void *, int, struct proc *);
static int	iopsp_match(device_t, cfdata_t, void *);
static int	iopsp_rescan(struct iopsp_softc *);
static int	iopsp_reconfig(device_t);
static void	iopsp_scsipi_request(struct scsipi_channel *,
				     scsipi_adapter_req_t, void *);

CFATTACH_DECL_NEW(iopsp, sizeof(struct iopsp_softc),
    iopsp_match, iopsp_attach, NULL, NULL);

/*
 * Match a supported device.
 */
static int
iopsp_match(device_t parent, cfdata_t match, void *aux)
{
	struct iop_attach_args *ia;
	struct iop_softc *iop;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_hba_ctlr_info ci;
	} __packed param;

	ia = aux;
	iop = device_private(parent);

	if (ia->ia_class != I2O_CLASS_BUS_ADAPTER_PORT)
		return (0);

	if (iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_HBA_CTLR_INFO, &param,
	    sizeof(param), NULL) != 0)
		return (0);

	return (param.ci.bustype == I2O_HBA_BUS_SCSI ||
	    param.ci.bustype == I2O_HBA_BUS_FCA);
}

/*
 * Attach a supported device.
 */
static void
iopsp_attach(device_t parent, device_t self, void *aux)
{
	struct iop_attach_args *ia;
	struct iopsp_softc *sc;
	struct iop_softc *iop;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		union {
			struct	i2o_param_hba_ctlr_info ci;
			struct	i2o_param_hba_scsi_ctlr_info sci;
			struct	i2o_param_hba_scsi_port_info spi;
		} p;
	} __packed param;
	int fc, rv;
	int size;

	ia = (struct iop_attach_args *)aux;
	sc = device_private(self);
	iop = device_private(parent);

	/* Register us as an initiator. */
	sc->sc_ii.ii_dv = self;
	sc->sc_ii.ii_intr = iopsp_intr;
	sc->sc_ii.ii_flags = 0;
	sc->sc_ii.ii_tid = ia->ia_tid;
	sc->sc_ii.ii_reconfig = iopsp_reconfig;
	sc->sc_ii.ii_adjqparam = iopsp_adjqparam;
	iop_initiator_register(iop, &sc->sc_ii);

	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_HBA_CTLR_INFO,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

	fc = (param.p.ci.bustype == I2O_HBA_BUS_FCA);

	/*
	 * Say what the device is.  If we can find out what the controling
	 * device is, say what that is too.
	 */
	aprint_normal(": SCSI port");
	iop_print_ident(iop, ia->ia_tid);
	aprint_normal("\n");

	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_HBA_SCSI_CTLR_INFO,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

	aprint_normal_dev(sc->sc_dev, "");
	if (fc)
		aprint_normal("FC");
	else
		aprint_normal("%d-bit", param.p.sci.maxdatawidth);
	aprint_normal(", max sync rate %dMHz, initiator ID %d\n",
	    (u_int32_t)le64toh(param.p.sci.maxsyncrate) / 1000,
	    le32toh(param.p.sci.initiatorid));

	sc->sc_openings = 1;

	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = 1;
	sc->sc_adapter.adapt_max_periph = 1;
	sc->sc_adapter.adapt_ioctl = iopsp_ioctl;
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = iopsp_scsipi_request;

	memset(&sc->sc_channel, 0, sizeof(sc->sc_channel));
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = fc ?
	    IOPSP_MAX_FC_TARGET : param.p.sci.maxdatawidth;
	sc->sc_channel.chan_nluns = IOPSP_MAX_LUN;
	sc->sc_channel.chan_id = le32toh(param.p.sci.initiatorid);
	sc->sc_channel.chan_flags = SCSIPI_CHAN_NOSETTLE;

	/*
	 * Allocate the target map.  Currently used for informational
	 * purposes only.
	 */
	size = sc->sc_channel.chan_ntargets * sizeof(struct iopsp_target);
	sc->sc_targetmap = malloc(size, M_DEVBUF, M_NOWAIT|M_ZERO);

 	/* Build the two maps, and attach to scsipi. */
	if (iopsp_reconfig(self) != 0) {
		aprint_error_dev(sc->sc_dev, "configure failed\n");
		goto bad;
	}
	config_found(self, &sc->sc_channel, scsiprint);
	return;

 bad:
	iop_initiator_unregister(iop, &sc->sc_ii);
}

/*
 * Scan the LCT to determine which devices we control, and enter them into
 * the maps.
 */
static int
iopsp_reconfig(device_t dv)
{
	struct iopsp_softc *sc;
	struct iop_softc *iop;
	struct i2o_lct_entry *le;
	struct scsipi_channel *sc_chan;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_scsi_device_info sdi;
	} __packed param;
	u_int tid, nent, i, targ, lun, size, rv, bptid;
	u_short *tidmap;
	void *tofree;
	struct iopsp_target *it;
	int syncrate;

	sc = device_private(dv);
	iop = device_private(device_parent(sc->sc_dev));
	sc_chan = &sc->sc_channel;

	KASSERT(mutex_owned(&iop->sc_conflock));

	/* Anything to do? */
	if (iop->sc_chgind == sc->sc_chgind)
		return (0);

	/*
	 * Allocate memory for the target/LUN -> TID map.  Use zero to
	 * denote absent targets (zero is the TID of the I2O executive,
	 * and we never address that here).
	 */
	size = sc_chan->chan_ntargets * (IOPSP_MAX_LUN) * sizeof(u_short);
	if ((tidmap = malloc(size, M_DEVBUF, M_WAITOK|M_ZERO)) == NULL)
		return (ENOMEM);

	for (i = 0; i < sc_chan->chan_ntargets; i++)
		sc->sc_targetmap[i].it_flags &= ~IT_PRESENT;

	/*
	 * A quick hack to handle Intel's stacked bus port arrangement.
	 */
	bptid = sc->sc_ii.ii_tid;
	nent = iop->sc_nlctent;
	for (le = iop->sc_lct->entry; nent != 0; nent--, le++)
		if ((le16toh(le->classid) & 4095) ==
		    I2O_CLASS_BUS_ADAPTER_PORT &&
		    (le32toh(le->usertid) & 4095) == bptid) {
			bptid = le16toh(le->localtid) & 4095;
			break;
		}

	nent = iop->sc_nlctent;
	for (i = 0, le = iop->sc_lct->entry; i < nent; i++, le++) {
		if ((le16toh(le->classid) & 4095) != I2O_CLASS_SCSI_PERIPHERAL)
			continue;
		if (((le32toh(le->usertid) >> 12) & 4095) != bptid)
			continue;
		tid = le16toh(le->localtid) & 4095;

		rv = iop_field_get_all(iop, tid, I2O_PARAM_SCSI_DEVICE_INFO,
		    &param, sizeof(param), NULL);
		if (rv != 0)
			continue;
		targ = le32toh(param.sdi.identifier);
		lun = param.sdi.luninfo[1];
#if defined(DIAGNOSTIC) || defined(I2ODEBUG)
		if (targ >= sc_chan->chan_ntargets ||
		    lun >= sc_chan->chan_nluns) {
			aprint_error_dev(sc->sc_dev, "target %d,%d (tid %d): "
			    "bad target/LUN\n", targ, lun, tid);
			continue;
		}
#endif

		/*
		 * If we've already described this target, and nothing has
		 * changed, then don't describe it again.
		 */
		it = &sc->sc_targetmap[targ];
		it->it_flags |= IT_PRESENT;
		syncrate = ((int)le64toh(param.sdi.negsyncrate) + 500) / 1000;
		if (it->it_width != param.sdi.negdatawidth ||
		    it->it_offset != param.sdi.negoffset ||
		    it->it_syncrate != syncrate) {
			it->it_width = param.sdi.negdatawidth;
			it->it_offset = param.sdi.negoffset;
			it->it_syncrate = syncrate;

			aprint_verbose_dev(sc->sc_dev, "target %d (tid %d): %d-bit, ",
			    targ, tid, it->it_width);
			if (it->it_syncrate == 0)
				aprint_verbose("asynchronous\n");
			else
				aprint_verbose("synchronous at %dMHz, "
				    "offset 0x%x\n", it->it_syncrate,
				    it->it_offset);
		}

		/* Ignore the device if it's in use by somebody else. */
		if ((le32toh(le->usertid) & 4095) != I2O_TID_NONE) {
			if (sc->sc_tidmap == NULL ||
			    IOPSP_TIDMAP(sc->sc_tidmap, targ, lun) !=
			    IOPSP_TID_INUSE) {
				aprint_verbose_dev(sc->sc_dev, "target %d,%d (tid %d): "
				    "in use by tid %d\n",
				    targ, lun, tid,
				    le32toh(le->usertid) & 4095);
			}
			IOPSP_TIDMAP(tidmap, targ, lun) = IOPSP_TID_INUSE;
		} else
			IOPSP_TIDMAP(tidmap, targ, lun) = (u_short)tid;
	}

	for (i = 0; i < sc_chan->chan_ntargets; i++)
		if ((sc->sc_targetmap[i].it_flags & IT_PRESENT) == 0)
			sc->sc_targetmap[i].it_width = 0;

	/* Swap in the new map and return. */
	mutex_spin_enter(&iop->sc_intrlock);
	tofree = sc->sc_tidmap;
	sc->sc_tidmap = tidmap;
	mutex_spin_exit(&iop->sc_intrlock);

	if (tofree != NULL)
		free(tofree, M_DEVBUF);
	sc->sc_chgind = iop->sc_chgind;
	return (0);
}

/*
 * Re-scan the bus; to be called from a higher level (e.g. scsipi).
 */
static int
iopsp_rescan(struct iopsp_softc *sc)
{
	struct iop_softc *iop;
	struct iop_msg *im;
	struct i2o_hba_bus_scan mf;
	int rv;

	iop = device_private(device_parent(sc->sc_dev));

	mutex_enter(&iop->sc_conflock);
	im = iop_msg_alloc(iop, IM_WAIT);

	mf.msgflags = I2O_MSGFLAGS(i2o_hba_bus_scan);
	mf.msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid, I2O_HBA_BUS_SCAN);
	mf.msgictx = sc->sc_ii.ii_ictx;
	mf.msgtctx = im->im_tctx;

	rv = iop_msg_post(iop, im, &mf, 5*60*1000);
	iop_msg_free(iop, im);
	if (rv != 0)
		aprint_error_dev(sc->sc_dev, "bus rescan failed (error %d)\n",
		    rv);

	if ((rv = iop_lct_get(iop)) == 0)
		rv = iopsp_reconfig(sc->sc_dev);

	mutex_exit(&iop->sc_conflock);
	return (rv);
}

/*
 * Start a SCSI command.
 */
static void
iopsp_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
		     void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct iopsp_softc *sc;
	struct iop_msg *im;
	struct iop_softc *iop;
	struct i2o_scsi_scb_exec *mf;
	int error, flags, tid;
	u_int32_t mb[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];

	sc = device_private(chan->chan_adapter->adapt_dev);
	iop = device_private(device_parent(sc->sc_dev));

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		SC_DEBUG(periph, SCSIPI_DB2, ("iopsp_scsi_request run_xfer\n"));

		tid = IOPSP_TIDMAP(sc->sc_tidmap, periph->periph_target,
		    periph->periph_lun);
		if (tid == IOPSP_TID_ABSENT || tid == IOPSP_TID_INUSE) {
			xs->error = XS_SELTIMEOUT;
			scsipi_done(xs);
			return;
		}

		/* Need to reset the target? */
		if ((flags & XS_CTL_RESET) != 0) {
			if (iop_simple_cmd(iop, tid, I2O_SCSI_DEVICE_RESET,
			    sc->sc_ii.ii_ictx, 1, 30*1000) != 0) {
				aprint_error_dev(sc->sc_dev, "reset failed\n");
				xs->error = XS_DRIVER_STUFFUP;
			} else
				xs->error = XS_NOERROR;

			scsipi_done(xs);
			return;
		}

#if defined(I2ODEBUG) || defined(SCSIDEBUG)
		if (xs->cmdlen > sizeof(mf->cdb))
			panic("%s: CDB too large", device_xname(sc->sc_dev));
#endif

		im = iop_msg_alloc(iop, IM_POLL_INTR |
		    IM_NOSTATUS | ((flags & XS_CTL_POLL) != 0 ? IM_POLL : 0));
		im->im_dvcontext = xs;

		mf = (struct i2o_scsi_scb_exec *)mb;
		mf->msgflags = I2O_MSGFLAGS(i2o_scsi_scb_exec);
		mf->msgfunc = I2O_MSGFUNC(tid, I2O_SCSI_SCB_EXEC);
		mf->msgictx = sc->sc_ii.ii_ictx;
		mf->msgtctx = im->im_tctx;
		mf->flags = xs->cmdlen | I2O_SCB_FLAG_ENABLE_DISCONNECT |
		    I2O_SCB_FLAG_SENSE_DATA_IN_MESSAGE;
		mf->datalen = xs->datalen;
		memcpy(mf->cdb, xs->cmd, xs->cmdlen);

		switch (xs->xs_tag_type) {
		case MSG_ORDERED_Q_TAG:
			mf->flags |= I2O_SCB_FLAG_ORDERED_QUEUE_TAG;
			break;
		case MSG_SIMPLE_Q_TAG:
			mf->flags |= I2O_SCB_FLAG_SIMPLE_QUEUE_TAG;
			break;
		case MSG_HEAD_OF_Q_TAG:
			mf->flags |= I2O_SCB_FLAG_HEAD_QUEUE_TAG;
			break;
		default:
			break;
		}

		if (xs->datalen != 0) {
			error = iop_msg_map_bio(iop, im, mb, xs->data,
			    xs->datalen, (flags & XS_CTL_DATA_OUT) == 0);
			if (error) {
				xs->error = XS_DRIVER_STUFFUP;
				iop_msg_free(iop, im);
				scsipi_done(xs);
				return;
			}
			if ((flags & XS_CTL_DATA_IN) == 0)
				mf->flags |= I2O_SCB_FLAG_XFER_TO_DEVICE;
			else
				mf->flags |= I2O_SCB_FLAG_XFER_FROM_DEVICE;
		}

		if (iop_msg_post(iop, im, mb, xs->timeout)) {
			if (xs->datalen != 0)
				iop_msg_unmap(iop, im);
			iop_msg_free(iop, im);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
		}
		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * The DDM takes care of this, and we can't modify its
		 * behaviour.
		 */
		break;
	}
}

#ifdef notyet
/*
 * Abort the specified I2O_SCSI_SCB_EXEC message and its associated SCB.
 */
static int
iopsp_scsi_abort(struct iopsp_softc *sc, int atid, struct iop_msg *aim)
{
	struct iop_msg *im;
	struct i2o_scsi_scb_abort mf;
	struct iop_softc *iop;
	int rv, s;

	iop = device_private(device_parent(sc->sc_dev));
	im = iop_msg_alloc(iop, IM_POLL);

	mf.msgflags = I2O_MSGFLAGS(i2o_scsi_scb_abort);
	mf.msgfunc = I2O_MSGFUNC(atid, I2O_SCSI_SCB_ABORT);
	mf.msgictx = sc->sc_ii.ii_ictx;
	mf.msgtctx = im->im_tctx;
	mf.tctxabort = aim->im_tctx;

	rv = iop_msg_post(iop, im, &mf, 30000);
	iop_msg_free(iop, im);

	return (rv);
}
#endif

/*
 * We have a message which has been processed and replied to by the IOP -
 * deal with it.
 */
static void
iopsp_intr(device_t dv, struct iop_msg *im, void *reply)
{
	struct scsipi_xfer *xs;
	struct iopsp_softc *sc;
	struct i2o_scsi_reply *rb;
 	struct iop_softc *iop;
	u_int sl;

	sc = device_private(dv);
	xs = im->im_dvcontext;
	iop = device_private(device_parent(dv));
	rb = reply;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("iopsp_intr\n"));

	if ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		xs->resid = xs->datalen;
	} else {
		if (rb->hbastatus != I2O_SCSI_DSC_SUCCESS) {
			switch (rb->hbastatus) {
			case I2O_SCSI_DSC_ADAPTER_BUSY:
			case I2O_SCSI_DSC_SCSI_BUS_RESET:
			case I2O_SCSI_DSC_BUS_BUSY:
				xs->error = XS_BUSY;
				break;
			case I2O_SCSI_DSC_SELECTION_TIMEOUT:
				xs->error = XS_SELTIMEOUT;
				break;
			case I2O_SCSI_DSC_COMMAND_TIMEOUT:
			case I2O_SCSI_DSC_DEVICE_NOT_PRESENT:
			case I2O_SCSI_DSC_LUN_INVALID:
			case I2O_SCSI_DSC_SCSI_TID_INVALID:
				xs->error = XS_TIMEOUT;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			aprint_error_dev(sc->sc_dev, "HBA status 0x%02x\n",
			    rb->hbastatus);
		} else if (rb->scsistatus != SCSI_OK) {
			switch (rb->scsistatus) {
			case SCSI_CHECK:
				xs->error = XS_SENSE;
				sl = le32toh(rb->senselen);
				if (sl > sizeof(xs->sense.scsi_sense))
					sl = sizeof(xs->sense.scsi_sense);
				memcpy(&xs->sense.scsi_sense, rb->sense, sl);
				break;
			case SCSI_QUEUE_FULL:
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->error = XS_NOERROR;

		xs->resid = xs->datalen - le32toh(rb->datalen);
		xs->status = rb->scsistatus;
	}

	/* Free the message wrapper and pass the news to scsipi. */
	if (xs->datalen != 0)
		iop_msg_unmap(iop, im);
	iop_msg_free(iop, im);

	scsipi_done(xs);
}

/*
 * ioctl hook; used here only to initiate low-level rescans.
 */
static int
iopsp_ioctl(struct scsipi_channel *chan, u_long cmd, void *data,
    int flag, struct proc *p)
{
	int rv;

	switch (cmd) {
	case SCBUSIOLLSCAN:
		/*
		 * If it's boot time, the bus will have been scanned and the
		 * maps built.  Locking would stop re-configuration, but we
		 * want to fake success.
		 */
		if (curlwp != &lwp0)
			rv = iopsp_rescan(
			   device_private(chan->chan_adapter->adapt_dev));
		else
			rv = 0;
		break;

	default:
		rv = ENOTTY;
		break;
	}

	return (rv);
}

/*
 * The number of openings available to us has changed, so inform scsipi.
 */
static void
iopsp_adjqparam(device_t dv, int mpi)
{
	struct iopsp_softc *sc;
	struct iop_softc *iop;

	sc = device_private(dv);
	iop = device_private(device_parent(dv));

	mutex_spin_enter(&iop->sc_intrlock);
	sc->sc_adapter.adapt_openings += mpi - sc->sc_openings;
	sc->sc_openings = mpi;
	mutex_spin_exit(&iop->sc_intrlock);
}
