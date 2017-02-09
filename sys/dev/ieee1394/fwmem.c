/*	$NetBSD: fwmem.c,v 1.18 2014/02/25 18:30:09 pooka Exp $	*/
/*-
 * Copyright (c) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwmem.c,v 1.18 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwmem.h>

#include "ioconf.h"

static int fwmem_speed=2, fwmem_debug=0;
static struct fw_eui64 fwmem_eui64;

static int sysctl_fwmem_verify(SYSCTLFN_PROTO, int, int);
static int sysctl_fwmem_verify_speed(SYSCTLFN_PROTO);

static struct fw_xfer *fwmem_xfer_req(struct fw_device *, void *, int, int,
				      int, void *);
static void fwmem_biodone(struct fw_xfer *);

/*
 * Setup sysctl(3) MIB, hw.fwmem.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_fwmem, "sysctl fwmem subtree setup")
{
	int rc, fwmem_node_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "fwmem",
	    SYSCTL_DESCR("IEEE1394 Memory Access"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	fwmem_node_num = node->sysctl_num;

	/* fwmem target EUI64 high/low */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "eui64_hi", SYSCTL_DESCR("Fwmem target EUI64 high"),
	    NULL, 0, &fwmem_eui64.hi,
	    0, CTL_HW, fwmem_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "eui64_lo", SYSCTL_DESCR("Fwmem target EUI64 low"),
	    NULL, 0, &fwmem_eui64.lo,
	    0, CTL_HW, fwmem_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* fwmem link speed */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "speed", SYSCTL_DESCR("Fwmem link speed"),
	    sysctl_fwmem_verify_speed, 0, &fwmem_speed,
	    0, CTL_HW, fwmem_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* fwmem driver debug flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "fwmem_debug", SYSCTL_DESCR("Fwmem driver debug flag"),
	    NULL, 0, &fwmem_debug,
	    0, CTL_HW, fwmem_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	return;

err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
sysctl_fwmem_verify(SYSCTLFN_ARGS, int lower, int upper)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < lower || t > upper)
		return EINVAL;

	*(int*)rnode->sysctl_data = t;

	return 0;
}

static int
sysctl_fwmem_verify_speed(SYSCTLFN_ARGS)
{

	return sysctl_fwmem_verify(SYSCTLFN_CALL(rnode), 0, FWSPD_S400);
}

#define MAXLEN (512 << fwmem_speed)

struct fwmem_softc {
	struct fw_eui64 eui;
	struct firewire_softc *sc;
	int refcount;
	STAILQ_HEAD(, fw_xfer) xferlist;
};


int      
fwmem_open(dev_t dev, int flags, int fmt, struct lwp *td)   
{
	struct firewire_softc *sc;
	struct fwmem_softc *fms;
	struct fw_xfer *xfer;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (sc->si_drv1 != NULL) {
		if ((flags & FWRITE) != 0) {
			return EBUSY;
		}
		fms = (struct fwmem_softc *)sc->si_drv1;
		fms->refcount++;
	} else {
		sc->si_drv1 = (void *)-1;
		sc->si_drv1 = malloc(sizeof(struct fwmem_softc),
		    M_FW, M_WAITOK);
		if (sc->si_drv1 == NULL)
			return ENOMEM;
		fms = (struct fwmem_softc *)sc->si_drv1;
		memcpy(&fms->eui, &fwmem_eui64, sizeof(struct fw_eui64));
		fms->sc = sc;
		fms->refcount = 1;
		STAILQ_INIT(&fms->xferlist);
		xfer = fw_xfer_alloc(M_FW);
		STAILQ_INSERT_TAIL(&fms->xferlist, xfer, link);
	}
	if (fwmem_debug)
		printf("%s: refcount=%d\n", __func__, fms->refcount);

	return 0;
}

int
fwmem_close(dev_t dev, int flags, int fmt, struct lwp *td)
{
	struct firewire_softc *sc;
	struct fwmem_softc *fms;
	struct fw_xfer *xfer;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	fms = (struct fwmem_softc *)sc->si_drv1;

	fms->refcount--;
	if (fwmem_debug)
		printf("%s: refcount=%d\n", __func__, fms->refcount);
	if (fms->refcount < 1) {
		while ((xfer = STAILQ_FIRST(&fms->xferlist)) != NULL) {
			STAILQ_REMOVE_HEAD(&fms->xferlist, link);
			fw_xfer_free(xfer);
		}
		free(sc->si_drv1, M_FW);
		sc->si_drv1 = NULL;
	}

	return 0;
}

void
fwmem_strategy(struct bio *bp)
{
	struct firewire_softc *sc;
	struct fwmem_softc *fms;
	struct fw_device *fwdev;
	struct fw_xfer *xfer;
	dev_t dev = bp->bio_dev;
	int iolen, err = 0, s;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return;

	/* XXX check request length */

	s = splvm();
	fms = (struct fwmem_softc *)sc->si_drv1;
	fwdev = fw_noderesolve_eui64(fms->sc->fc, &fms->eui);
	if (fwdev == NULL) {
		if (fwmem_debug)
			printf("fwmem: no such device ID:%08x%08x\n",
					fms->eui.hi, fms->eui.lo);
		err = EINVAL;
		goto error;
	}

	iolen = MIN(bp->bio_bcount, MAXLEN);
	if ((bp->bio_cmd & BIO_READ) == BIO_READ) {
		if (iolen == 4 && (bp->bio_offset & 3) == 0)
			xfer = fwmem_read_quad(fwdev, (void *) bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    bp->bio_data, fwmem_biodone);
		else
			xfer = fwmem_read_block(fwdev, (void *) bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    iolen, bp->bio_data, fwmem_biodone);
	} else {
		if (iolen == 4 && (bp->bio_offset & 3) == 0)
			xfer = fwmem_write_quad(fwdev, (void *)bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    bp->bio_data, fwmem_biodone);
		else
			xfer = fwmem_write_block(fwdev, (void *)bp, fwmem_speed,
			    bp->bio_offset >> 32, bp->bio_offset & 0xffffffff,
			    iolen, bp->bio_data, fwmem_biodone);
	}
	if (xfer == NULL) {
		err = EIO;
		goto error;
	}
	/* XXX */
	bp->bio_resid = bp->bio_bcount - iolen;
error:
	splx(s);
	if (err != 0) {
		if (fwmem_debug)
			printf("%s: err=%d\n", __func__, err);
		bp->bio_error = err;
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
	}
}

int
fwmem_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *td)
{
	struct firewire_softc *sc;
	struct fwmem_softc *fms;
	int err = 0;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	fms = (struct fwmem_softc *)sc->si_drv1;
	switch (cmd) {
	case FW_SDEUI64:
		memcpy(&fms->eui, data, sizeof(struct fw_eui64));
		break;

	case FW_GDEUI64:
		memcpy(data, &fms->eui, sizeof(struct fw_eui64));
		break;

	default:
		err = EINVAL;
	}
	return err;
}


struct fw_xfer *
fwmem_read_quad(struct fw_device *fwdev, void *	sc, uint8_t spd,
		uint16_t dst_hi, uint32_t dst_lo, void *data,
		void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, (void *)sc, spd, 0, 4, hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.dest_hi = dst_hi;
	fp->mode.rreqq.dest_lo = dst_lo;

	xfer->send.payload = NULL;
	xfer->recv.payload = (uint32_t *)data;

	if (fwmem_debug)
		aprint_error("fwmem_read_quad: %d %04x:%08x\n",
		    fwdev->dst, dst_hi, dst_lo);

	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_write_quad(struct fw_device *fwdev, void *sc, uint8_t spd,
		 uint16_t dst_hi, uint32_t dst_lo, void *data,
		 void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 0, 0, hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.wreqq.tcode = FWTCODE_WREQQ;
	fp->mode.wreqq.dest_hi = dst_hi;
	fp->mode.wreqq.dest_lo = dst_lo;
	fp->mode.wreqq.data = *(uint32_t *)data;

	xfer->send.payload = xfer->recv.payload = NULL;

	if (fwmem_debug)
		aprint_error("fwmem_write_quad: %d %04x:%08x %08x\n",
		    fwdev->dst, dst_hi, dst_lo, *(uint32_t *)data);

	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_read_block(struct fw_device *fwdev, void *sc, uint8_t spd,
		 uint16_t dst_hi, uint32_t dst_lo, int len, void *data,
		 void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, 0, roundup2(len, 4), hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.rreqb.tcode = FWTCODE_RREQB;
	fp->mode.rreqb.dest_hi = dst_hi;
	fp->mode.rreqb.dest_lo = dst_lo;
	fp->mode.rreqb.len = len;
	fp->mode.rreqb.extcode = 0;

	xfer->send.payload = NULL;
	xfer->recv.payload = data;

	if (fwmem_debug)
		aprint_error("fwmem_read_block: %d %04x:%08x %d\n",
		    fwdev->dst, dst_hi, dst_lo, len);
	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}

struct fw_xfer *
fwmem_write_block(struct fw_device *fwdev, void *sc, uint8_t spd,
		  uint16_t dst_hi, uint32_t dst_lo, int len, void *data,
		  void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fwmem_xfer_req(fwdev, sc, spd, len, 0, hand);
	if (xfer == NULL)
		return NULL;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.tcode = FWTCODE_WREQB;
	fp->mode.wreqb.dest_hi = dst_hi;
	fp->mode.wreqb.dest_lo = dst_lo;
	fp->mode.wreqb.len = len;
	fp->mode.wreqb.extcode = 0;

	xfer->send.payload = data;
	xfer->recv.payload = NULL;

	if (fwmem_debug)
		aprint_error("fwmem_write_block: %d %04x:%08x %d\n",
		    fwdev->dst, dst_hi, dst_lo, len);
	if (fw_asyreq(xfer->fc, -1, xfer) == 0)
		return xfer;

	fw_xfer_free(xfer);
	return NULL;
}


static struct fw_xfer *
fwmem_xfer_req(struct fw_device *fwdev, void *sc, int spd, int slen, int rlen,
	       void *hand)
{
	struct fw_xfer *xfer;

	xfer = fw_xfer_alloc(M_FW);
	if (xfer == NULL)
		return NULL;

	xfer->fc = fwdev->fc;
	xfer->send.hdr.mode.hdr.dst = FWLOCALBUS | fwdev->dst;
	if (spd < 0)
		xfer->send.spd = fwdev->speed;
	else
		xfer->send.spd = min(spd, fwdev->speed);
	xfer->hand = hand;
	xfer->sc = sc;
	xfer->send.pay_len = slen;
	xfer->recv.pay_len = rlen;

	return xfer;
}

static void
fwmem_biodone(struct fw_xfer *xfer)
{
	struct bio *bp;

	bp = (struct bio *)xfer->sc;
	bp->bio_error = xfer->resp;

	if (bp->bio_error != 0) {
		if (fwmem_debug)
			printf("%s: err=%d\n", __func__, bp->bio_error);
		bp->bio_resid = bp->bio_bcount;
	}

	fw_xfer_free(xfer);
	biodone(bp);
}
