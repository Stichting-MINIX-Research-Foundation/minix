/*	$NetBSD: ld_iop.c,v 1.35 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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
 * I2O front-end for ld(4) driver, supporting random block storage class
 * devices.  Currently, this doesn't handle anything more complex than
 * fixed direct-access devices.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_iop.c,v 1.35 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/ldvar.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>

#define	LD_IOP_TIMEOUT		30*1000

#define	LD_IOP_CLAIMED		0x01
#define	LD_IOP_NEW_EVTMASK	0x02

struct ld_iop_softc {
	struct	ld_softc sc_ld;
	struct	iop_initiator sc_ii;
	struct	iop_initiator sc_eventii;
	int	sc_flags;
};

static void	ld_iop_adjqparam(device_t, int);
static void	ld_iop_attach(device_t, device_t, void *);
static int	ld_iop_detach(device_t, int);
static int	ld_iop_dump(struct ld_softc *, void *, int, int);
static int	ld_iop_flush(struct ld_softc *, int);
static void	ld_iop_intr(device_t, struct iop_msg *, void *);
static void	ld_iop_intr_event(device_t, struct iop_msg *, void *);
static int	ld_iop_match(device_t, cfdata_t, void *);
static int	ld_iop_start(struct ld_softc *, struct buf *);
static void	ld_iop_unconfig(struct ld_iop_softc *, int);

CFATTACH_DECL_NEW(ld_iop, sizeof(struct ld_iop_softc),
    ld_iop_match, ld_iop_attach, ld_iop_detach, NULL);

static const char * const ld_iop_errors[] = {
	"success",
	"media error",
	"access error",
	"device failure",
	"device not ready",
	"media not present",
	"media locked",
	"media failure",
	"protocol failure",
	"bus failure",
	"access violation",
	"media write protected",
	"device reset",
	"volume changed, waiting for acknowledgement",
	"timeout",
};

static int
ld_iop_match(device_t parent, cfdata_t match, void *aux)
{
	struct iop_attach_args *ia;

	ia = aux;

	return (ia->ia_class == I2O_CLASS_RANDOM_BLOCK_STORAGE);
}

static void
ld_iop_attach(device_t parent, device_t self, void *aux)
{
	struct iop_attach_args *ia = aux;
	struct ld_iop_softc *sc = device_private(self);
	struct iop_softc *iop = device_private(parent);
	struct ld_softc *ld = &sc->sc_ld;
	int rv, evreg, enable;
	const char *typestr, *fixedstr;
	u_int cachesz;
	u_int32_t timeoutbase, rwvtimeoutbase, rwvtimeout;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		union {
			struct	i2o_param_rbs_cache_control cc;
			struct	i2o_param_rbs_device_info bdi;
		} p;
	} __packed param;

	ld->sc_dv = self;
	evreg = 0;

	/* Register us as an initiator. */
	sc->sc_ii.ii_dv = self;
	sc->sc_ii.ii_intr = ld_iop_intr;
	sc->sc_ii.ii_adjqparam = ld_iop_adjqparam;
	sc->sc_ii.ii_flags = 0;
	sc->sc_ii.ii_tid = ia->ia_tid;
	iop_initiator_register(iop, &sc->sc_ii);

	/* Register another initiator to handle events from the device. */
	sc->sc_eventii.ii_dv = self;
	sc->sc_eventii.ii_intr = ld_iop_intr_event;
	sc->sc_eventii.ii_flags = II_NOTCTX | II_UTILITY;
	sc->sc_eventii.ii_tid = ia->ia_tid;
	iop_initiator_register(iop, &sc->sc_eventii);

	rv = iop_util_eventreg(iop, &sc->sc_eventii,
	    I2O_EVENT_GEN_EVENT_MASK_MODIFIED |
	    I2O_EVENT_GEN_DEVICE_RESET |
	    I2O_EVENT_GEN_STATE_CHANGE |
	    I2O_EVENT_GEN_GENERAL_WARNING);
	if (rv != 0) {
		aprint_error_dev(self, "unable to register for events");
		goto bad;
	}
	evreg = 1;

	/*
	 * Start out with one queued command.  The `iop' driver will adjust
	 * the queue parameters once we're up and running.
	 */
	ld->sc_maxqueuecnt = 1;

	ld->sc_maxxfer = IOP_MAX_XFER;
	ld->sc_dump = ld_iop_dump;
	ld->sc_flush = ld_iop_flush;
	ld->sc_start = ld_iop_start;

	/* Say what the device is. */
	printf(":");
	iop_print_ident(iop, ia->ia_tid);

	/*
	 * Claim the device so that we don't get any nasty surprises.  Allow
	 * failure.
	 */
	rv = iop_util_claim(iop, &sc->sc_ii, 0,
	    I2O_UTIL_CLAIM_CAPACITY_SENSITIVE |
	    I2O_UTIL_CLAIM_NO_PEER_SERVICE |
	    I2O_UTIL_CLAIM_NO_MANAGEMENT_SERVICE |
	    I2O_UTIL_CLAIM_PRIMARY_USER);
	sc->sc_flags = rv ? 0 : LD_IOP_CLAIMED;

	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_RBS_DEVICE_INFO,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

	ld->sc_secsize = le32toh(param.p.bdi.blocksize);
	ld->sc_secperunit = (int)
	    (le64toh(param.p.bdi.capacity) / ld->sc_secsize);

	switch (param.p.bdi.type) {
	case I2O_RBS_TYPE_DIRECT:
		typestr = "direct access";
		enable = 1;
		break;
	case I2O_RBS_TYPE_WORM:
		typestr = "WORM";
		enable = 0;
		break;
	case I2O_RBS_TYPE_CDROM:
		typestr = "CD-ROM";
		enable = 0;
		break;
	case I2O_RBS_TYPE_OPTICAL:
		typestr = "optical";
		enable = 0;
		break;
	default:
		typestr = "unknown";
		enable = 0;
		break;
	}

	if ((le32toh(param.p.bdi.capabilities) & I2O_RBS_CAP_REMOVABLE_MEDIA)
	    != 0) {
		/* ld->sc_flags = LDF_REMOVABLE; */
		fixedstr = "removable";
		enable = 0;
	} else
		fixedstr = "fixed";

	printf(" %s, %s", typestr, fixedstr);

	/*
	 * Determine if the device has an private cache.  If so, print the
	 * cache size.  Even if the device doesn't appear to have a cache,
	 * we perform a flush at shutdown.
	 */
	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_RBS_CACHE_CONTROL,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

	if ((cachesz = le32toh(param.p.cc.totalcachesize)) != 0)
		printf(", %dkB cache", cachesz >> 10);

	printf("\n");

	/*
	 * Configure the DDM's timeout functions to time out all commands
	 * after 30 seconds.
	 */
	timeoutbase = htole32(LD_IOP_TIMEOUT * 1000);
	rwvtimeoutbase = htole32(LD_IOP_TIMEOUT * 1000);
	rwvtimeout = 0;

	iop_field_set(iop, ia->ia_tid, I2O_PARAM_RBS_OPERATION,
	    &timeoutbase, sizeof(timeoutbase),
	    I2O_PARAM_RBS_OPERATION_timeoutbase);
	iop_field_set(iop, ia->ia_tid, I2O_PARAM_RBS_OPERATION,
	    &rwvtimeoutbase, sizeof(rwvtimeoutbase),
	    I2O_PARAM_RBS_OPERATION_rwvtimeoutbase);
	iop_field_set(iop, ia->ia_tid, I2O_PARAM_RBS_OPERATION,
	    &rwvtimeout, sizeof(rwvtimeout),
	    I2O_PARAM_RBS_OPERATION_rwvtimeoutbase);

	if (enable)
		ld->sc_flags |= LDF_ENABLED;
	else
		aprint_error_dev(self, "device not yet supported\n");

	ldattach(ld);
	return;

 bad:
	ld_iop_unconfig(sc, evreg);
}

static void
ld_iop_unconfig(struct ld_iop_softc *sc, int evreg)
{
	struct iop_softc *iop;

	iop = device_private(device_parent(sc->sc_ld.sc_dv));

	if ((sc->sc_flags & LD_IOP_CLAIMED) != 0)
		iop_util_claim(iop, &sc->sc_ii, 1,
		    I2O_UTIL_CLAIM_PRIMARY_USER);

	if (evreg) {
		/*
		 * Mask off events, and wait up to 5 seconds for a reply.
		 * Note that some adapters won't reply to this (XXX We
		 * should check the event capabilities).
		 */
		mutex_spin_enter(&iop->sc_intrlock);
		sc->sc_flags &= ~LD_IOP_NEW_EVTMASK;
		mutex_spin_exit(&iop->sc_intrlock);

		iop_util_eventreg(iop, &sc->sc_eventii,
		    I2O_EVENT_GEN_EVENT_MASK_MODIFIED);

		mutex_spin_enter(&iop->sc_intrlock);
		if ((sc->sc_flags & LD_IOP_NEW_EVTMASK) == 0)
			cv_timedwait(&sc->sc_eventii.ii_cv,
			    &iop->sc_intrlock, hz * 5);
		mutex_spin_exit(&iop->sc_intrlock);
	}

	iop_initiator_unregister(iop, &sc->sc_eventii);
	iop_initiator_unregister(iop, &sc->sc_ii);
}

static int
ld_iop_detach(device_t self, int flags)
{
	struct ld_iop_softc *sc;
	struct iop_softc *iop;
	int rv;

	sc = device_private(self);
	iop = device_private(device_parent(self));

	if ((rv = ldbegindetach(&sc->sc_ld, flags)) != 0)
		return (rv);

	/*
	 * Abort any requests queued with the IOP, but allow requests that
	 * are already in progress to complete.
	 */
	if ((sc->sc_ld.sc_flags & LDF_ENABLED) != 0)
		iop_util_abort(iop, &sc->sc_ii, 0, 0,
		    I2O_UTIL_ABORT_WILD | I2O_UTIL_ABORT_CLEAN);

	ldenddetach(&sc->sc_ld);

	/* Un-claim the target, and un-register our initiators. */
	if ((sc->sc_ld.sc_flags & LDF_ENABLED) != 0)
		ld_iop_unconfig(sc, 1);

	return (0);
}

static int
ld_iop_start(struct ld_softc *ld, struct buf *bp)
{
	struct iop_msg *im;
	struct iop_softc *iop;
	struct ld_iop_softc *sc;
	struct i2o_rbs_block_read *mf;
	u_int rv, flags, write;
	u_int64_t ba;
	u_int32_t mb[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];

	sc = device_private(ld->sc_dv);
	iop = device_private(device_parent(ld->sc_dv));

	im = iop_msg_alloc(iop, 0);
	im->im_dvcontext = bp;

	write = ((bp->b_flags & B_READ) == 0);
	ba = (u_int64_t)bp->b_rawblkno * ld->sc_secsize;

	/*
	 * Write through the cache when performing synchronous writes.  When
	 * performing a read, we don't request that the DDM cache the data,
	 * as there's little advantage to it.
	 */
	if (write) {
		if ((bp->b_flags & B_ASYNC) == 0)
			flags = I2O_RBS_BLOCK_WRITE_CACHE_WT;
		else
			flags = I2O_RBS_BLOCK_WRITE_CACHE_WB;
	} else
		flags = 0;

	/*
	 * Fill the message frame.  We can use the block_read structure for
	 * both reads and writes, as it's almost identical to the
	 * block_write structure.
	 */
	mf = (struct i2o_rbs_block_read *)mb;
	mf->msgflags = I2O_MSGFLAGS(i2o_rbs_block_read);
	mf->msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid,
	    write ? I2O_RBS_BLOCK_WRITE : I2O_RBS_BLOCK_READ);
	mf->msgictx = sc->sc_ii.ii_ictx;
	mf->msgtctx = im->im_tctx;
	mf->flags = flags | (1 << 16);		/* flags & time multiplier */
	mf->datasize = bp->b_bcount;
	mf->lowoffset = (u_int32_t)ba;
	mf->highoffset = (u_int32_t)(ba >> 32);

	/* Map the data transfer and enqueue the command. */
	rv = iop_msg_map_bio(iop, im, mb, bp->b_data, bp->b_bcount, write);
	if (rv == 0) {
		if ((rv = iop_post(iop, mb)) != 0) {
			iop_msg_unmap(iop, im);
			iop_msg_free(iop, im);
		}
	}
	return (rv);
}

static int
ld_iop_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{
	struct iop_msg *im;
	struct iop_softc *iop;
	struct ld_iop_softc *sc;
	struct i2o_rbs_block_write *mf;
	int rv, bcount;
	u_int64_t ba;
	u_int32_t mb[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];

	sc = device_private(ld->sc_dv);
	iop = device_private(device_parent(ld->sc_dv));
	bcount = blkcnt * ld->sc_secsize;
	ba = (u_int64_t)blkno * ld->sc_secsize;
	im = iop_msg_alloc(iop, IM_POLL);

	mf = (struct i2o_rbs_block_write *)mb;
	mf->msgflags = I2O_MSGFLAGS(i2o_rbs_block_write);
	mf->msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid, I2O_RBS_BLOCK_WRITE);
	mf->msgictx = sc->sc_ii.ii_ictx;
	mf->msgtctx = im->im_tctx;
	mf->flags = I2O_RBS_BLOCK_WRITE_CACHE_WT | (1 << 16);
	mf->datasize = bcount;
	mf->lowoffset = (u_int32_t)ba;
	mf->highoffset = (u_int32_t)(ba >> 32);

	if ((rv = iop_msg_map(iop, im, mb, data, bcount, 1, NULL)) != 0) {
		iop_msg_free(iop, im);
		return (rv);
	}

	rv = iop_msg_post(iop, im, mb, LD_IOP_TIMEOUT * 2);
	iop_msg_unmap(iop, im);
	iop_msg_free(iop, im);
 	return (rv);
}

static int
ld_iop_flush(struct ld_softc *ld, int flags)
{
	struct iop_msg *im;
	struct iop_softc *iop;
	struct ld_iop_softc *sc;
	struct i2o_rbs_cache_flush mf;
	int rv;

	sc = device_private(ld->sc_dv);
	iop = device_private(device_parent(ld->sc_dv));
	im = iop_msg_alloc(iop, IM_WAIT);

	mf.msgflags = I2O_MSGFLAGS(i2o_rbs_cache_flush);
	mf.msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid, I2O_RBS_CACHE_FLUSH);
	mf.msgictx = sc->sc_ii.ii_ictx;
	mf.msgtctx = im->im_tctx;
	mf.flags = 1 << 16;			/* time multiplier */

	/* Ancient disks will return an error here. */
	rv = iop_msg_post(iop, im, &mf, LD_IOP_TIMEOUT * 2);
	iop_msg_free(iop, im);
	return (rv);
}

void
ld_iop_intr(device_t dv, struct iop_msg *im, void *reply)
{
	struct i2o_rbs_reply *rb;
	struct buf *bp;
	struct ld_iop_softc *sc;
	struct iop_softc *iop;
	int err, detail;
	const char *errstr;

	rb = reply;
	bp = im->im_dvcontext;
	sc = device_private(dv);
	iop = device_private(device_parent(dv));

	err = ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0);

	if (!err && rb->reqstatus != I2O_STATUS_SUCCESS) {
		detail = le16toh(rb->detail);
		if (detail >= __arraycount(ld_iop_errors))
			errstr = "<unknown>";
		else
			errstr = ld_iop_errors[detail];
		aprint_error_dev(dv, "error 0x%04x: %s\n", detail, errstr);
		err = 1;
	}

	if (err) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = bp->b_bcount - le32toh(rb->transfercount);

	iop_msg_unmap(iop, im);
	iop_msg_free(iop, im);
	lddone(&sc->sc_ld, bp);
}

static void
ld_iop_intr_event(device_t dv, struct iop_msg *im, void *reply)
{
	struct i2o_util_event_register_reply *rb;
	struct ld_iop_softc *sc;
	struct iop_softc *iop;
	u_int event;

	rb = reply;

	if ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0)
		return;

	event = le32toh(rb->event);
	sc = device_private(dv);

	if (event == I2O_EVENT_GEN_EVENT_MASK_MODIFIED) {
		iop = device_private(device_parent(dv));
		mutex_spin_enter(&iop->sc_intrlock);
		sc->sc_flags |= LD_IOP_NEW_EVTMASK;
		cv_broadcast(&sc->sc_eventii.ii_cv);
		mutex_spin_exit(&iop->sc_intrlock);
		return;
	}

	printf("%s: event 0x%08x received\n", device_xname(dv), event);
}

static void
ld_iop_adjqparam(device_t dv, int mpi)
{
	struct ld_iop_softc *sc = device_private(dv);
	struct iop_softc *iop = device_private(device_parent(dv));
	struct ld_softc *ld = &sc->sc_ld;

	/*
	 * AMI controllers seem to loose the plot if you hand off lots of
	 * queued commands.
	 */
	if (le16toh(I2O_ORG_AMI) == iop->sc_status.orgid && mpi > 64)
		mpi = 64;

	ldadjqparam(ld, mpi);
}
