/*	$NetBSD: fwdev.c,v 1.30 2014/07/25 08:10:37 dholland Exp $	*/
/*-
 * Copyright (c) 2003 Hidetoshi Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 * $FreeBSD: src/sys/dev/firewire/fwdev.c,v 1.52 2007/06/06 14:31:36 simokawa Exp $
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwdev.c,v 1.30 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/fwmem.h>
#include <dev/ieee1394/iec68113.h>

#include "ioconf.h"

#define	FWNODE_INVAL 0xffff

dev_type_open(fw_open);
dev_type_close(fw_close);
dev_type_read(fw_read);
dev_type_write(fw_write);
dev_type_ioctl(fw_ioctl);
dev_type_poll(fw_poll);
dev_type_mmap(fw_mmap);
dev_type_strategy(fw_strategy);

const struct bdevsw fw_bdevsw = {
	.d_open = fw_open,
	.d_close = fw_close,
	.d_strategy = fw_strategy,
	.d_ioctl = fw_ioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

const struct cdevsw fw_cdevsw = {
	.d_open = fw_open,
	.d_close = fw_close,
	.d_read = fw_read,
	.d_write = fw_write,
	.d_ioctl = fw_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = fw_poll,
	.d_mmap = fw_mmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

struct fw_drv1 {
	struct firewire_comm *fc;
	struct fw_xferq *ir;
	struct fw_xferq *it;
	struct fw_isobufreq bufreq;
	STAILQ_HEAD(, fw_bind) binds;
	STAILQ_HEAD(, fw_xfer) rq;
};

static int fwdev_allocbuf(struct firewire_comm *, struct fw_xferq *,
			  struct fw_bufspec *);
static int fwdev_freebuf(struct fw_xferq *);
static int fw_read_async(struct fw_drv1 *, struct uio *, int);
static int fw_write_async(struct fw_drv1 *, struct uio *, int);
static void fw_hand(struct fw_xfer *);


int
fw_open(dev_t dev, int flags, int fmt, struct lwp *td)
{
	struct firewire_softc *sc;
	struct fw_drv1 *d;
	int err = 0;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (DEV_FWMEM(dev))
		return fwmem_open(dev, flags, fmt, td);

	mutex_enter(&sc->fc->fc_mtx);
	if (sc->si_drv1 != NULL) {
		mutex_exit(&sc->fc->fc_mtx);
		return EBUSY;
	}
	/* set dummy value for allocation */
	sc->si_drv1 = (void *)-1;
	mutex_exit(&sc->fc->fc_mtx);

	sc->si_drv1 = malloc(sizeof(struct fw_drv1), M_FW, M_WAITOK | M_ZERO);
	if (sc->si_drv1 == NULL)
		return ENOMEM;

	d = (struct fw_drv1 *)sc->si_drv1;
	d->fc = sc->fc;
	STAILQ_INIT(&d->binds);
	STAILQ_INIT(&d->rq);

	return err;
}

int
fw_close(dev_t dev, int flags, int fmt, struct lwp *td)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	struct fw_xfer *xfer;
	struct fw_bind *fwb;
        int err = 0;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (DEV_FWMEM(dev))
		return fwmem_close(dev, flags, fmt, td);

	d = (struct fw_drv1 *)sc->si_drv1;
	fc = d->fc;

	/* remove binding */
	for (fwb = STAILQ_FIRST(&d->binds); fwb != NULL;
	    fwb = STAILQ_FIRST(&d->binds)) {
		fw_bindremove(fc, fwb);
		STAILQ_REMOVE_HEAD(&d->binds, chlist);
		fw_xferlist_remove(&fwb->xferlist);
		free(fwb, M_FW);
	}
	if (d->ir != NULL) {
		struct fw_xferq *ir = d->ir;

		if ((ir->flag & FWXFERQ_OPEN) == 0)
			return EINVAL;
		if (ir->flag & FWXFERQ_RUNNING) {
			ir->flag &= ~FWXFERQ_RUNNING;
			fc->irx_disable(fc, ir->dmach);
		}
		/* free extbuf */
		fwdev_freebuf(ir);
		/* drain receiving buffer */
		for (xfer = STAILQ_FIRST(&ir->q); xfer != NULL;
		    xfer = STAILQ_FIRST(&ir->q)) {
			ir->queued--;
			STAILQ_REMOVE_HEAD(&ir->q, link);

			xfer->resp = 0;
			fw_xfer_done(xfer);
		}
		ir->flag &=
		    ~(FWXFERQ_OPEN | FWXFERQ_MODEMASK | FWXFERQ_CHTAGMASK);
		d->ir = NULL;

	}
	if (d->it != NULL) {
		struct fw_xferq *it = d->it;

		if ((it->flag & FWXFERQ_OPEN) == 0)
			return EINVAL;
		if (it->flag & FWXFERQ_RUNNING) {
			it->flag &= ~FWXFERQ_RUNNING;
			fc->itx_disable(fc, it->dmach);
		}
		/* free extbuf */
		fwdev_freebuf(it);
		it->flag &=
		    ~(FWXFERQ_OPEN | FWXFERQ_MODEMASK | FWXFERQ_CHTAGMASK);
		d->it = NULL;
	}
	free(sc->si_drv1, M_FW);
	sc->si_drv1 = NULL;

	return err;
}

int
fw_read(dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	struct fw_xferq *ir;
	struct fw_pkt *fp;
	int err = 0, slept = 0;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (DEV_FWMEM(dev))
		return physio(fw_strategy, NULL, dev, ioflag, minphys, uio);

	d = (struct fw_drv1 *)sc->si_drv1;
	fc = d->fc;
	ir = d->ir;

	if (ir == NULL)
		return fw_read_async(d, uio, ioflag);

	if (ir->buf == NULL)
		return EIO;

	mutex_enter(&fc->fc_mtx);
readloop:
	if (ir->stproc == NULL) {
		/* iso bulkxfer */
		ir->stproc = STAILQ_FIRST(&ir->stvalid);
		if (ir->stproc != NULL) {
			STAILQ_REMOVE_HEAD(&ir->stvalid, link);
			ir->queued = 0;
		}
	}
	if (ir->stproc == NULL) {
		/* no data avaliable */
		if (slept == 0) {
			slept = 1;
			ir->flag |= FWXFERQ_WAKEUP;
			mutex_exit(&fc->fc_mtx);
			err = tsleep(ir, FWPRI, "fw_read", hz);
			mutex_enter(&fc->fc_mtx);
			ir->flag &= ~FWXFERQ_WAKEUP;
			if (err == 0)
				goto readloop;
		} else if (slept == 1)
			err = EIO;
		mutex_exit(&fc->fc_mtx);
		return err;
	} else if (ir->stproc != NULL) {
		/* iso bulkxfer */
		mutex_exit(&fc->fc_mtx);
		fp = (struct fw_pkt *)fwdma_v_addr(ir->buf,
		    ir->stproc->poffset + ir->queued);
		if (fc->irx_post != NULL)
			fc->irx_post(fc, fp->mode.ld);
		if (fp->mode.stream.len == 0)
			return EIO;
		err = uiomove((void *)fp,
		    fp->mode.stream.len + sizeof(uint32_t), uio);
		ir->queued++;
		if (ir->queued >= ir->bnpacket) {
			STAILQ_INSERT_TAIL(&ir->stfree, ir->stproc, link);
			fc->irx_enable(fc, ir->dmach);
			ir->stproc = NULL;
		}
		if (uio->uio_resid >= ir->psize) {
			slept = -1;
			mutex_enter(&fc->fc_mtx);
			goto readloop;
		}
	} else
		mutex_exit(&fc->fc_mtx);
	return err;
}

int
fw_write(dev_t dev, struct uio *uio, int ioflag)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	struct fw_pkt *fp;
	struct fw_xferq *it;
        int slept = 0, err = 0;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (DEV_FWMEM(dev))
		return physio(fw_strategy, NULL, dev, ioflag, minphys, uio);

	d = (struct fw_drv1 *)sc->si_drv1;
	fc = d->fc;
	it = d->it;

	if (it == NULL)
		return fw_write_async(d, uio, ioflag);

	if (it->buf == NULL)
		return EIO;

	mutex_enter(&fc->fc_mtx);
isoloop:
	if (it->stproc == NULL) {
		it->stproc = STAILQ_FIRST(&it->stfree);
		if (it->stproc != NULL) {
			STAILQ_REMOVE_HEAD(&it->stfree, link);
			it->queued = 0;
		} else if (slept == 0) {
			slept = 1;
#if 0   /* XXX to avoid lock recursion */
			err = fc->itx_enable(fc, it->dmach);
			if (err)
				goto out;
#endif
			mutex_exit(&fc->fc_mtx);
			err = tsleep(it, FWPRI, "fw_write", hz);
			mutex_enter(&fc->fc_mtx);
			if (err)
				goto out;
			goto isoloop;
		} else {
			err = EIO;
			goto out;
		}
	}
	mutex_exit(&fc->fc_mtx);
	fp = (struct fw_pkt *)fwdma_v_addr(it->buf,
	    it->stproc->poffset + it->queued);
	err = uiomove((void *)fp, sizeof(struct fw_isohdr), uio);
	if (err != 0)
		return err;
	err =
	    uiomove((void *)fp->mode.stream.payload, fp->mode.stream.len, uio);
	it->queued++;
	if (it->queued >= it->bnpacket) {
		STAILQ_INSERT_TAIL(&it->stvalid, it->stproc, link);
		it->stproc = NULL;
		err = fc->itx_enable(fc, it->dmach);
	}
	if (uio->uio_resid >= sizeof(struct fw_isohdr)) {
		slept = 0;
		mutex_enter(&fc->fc_mtx);
		goto isoloop;
	}
	return err;

out:
	mutex_exit(&fc->fc_mtx);
	return err;
}

int
fw_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *td)
{
	struct firewire_softc *sc;
	struct firewire_comm *fc;
	struct fw_drv1 *d;
	struct fw_device *fwdev;
	struct fw_bind *fwb;
	struct fw_xferq *ir, *it;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct fw_devinfo *devinfo;
	struct fw_devlstreq *fwdevlst = (struct fw_devlstreq *)data;
	struct fw_asyreq *asyreq = (struct fw_asyreq *)data;
	struct fw_isochreq *ichreq = (struct fw_isochreq *)data;
	struct fw_isobufreq *ibufreq = (struct fw_isobufreq *)data;
	struct fw_asybindreq *bindreq = (struct fw_asybindreq *)data;
	struct fw_crom_buf *crom_buf = (struct fw_crom_buf *)data;
	int i, len, err = 0;
	void *ptr;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (DEV_FWMEM(dev))
		return fwmem_ioctl(dev, cmd, data, flag, td);

	if (!data)
		return EINVAL;

	d = (struct fw_drv1 *)sc->si_drv1;
	fc = d->fc;
	ir = d->ir;
	it = d->it;

	switch (cmd) {
	case FW_STSTREAM:
		if (it == NULL) {
			i = fw_open_isodma(fc, /* tx */1);
			if (i < 0) {
				err = EBUSY;
				break;
			}
			it = fc->it[i];
			err = fwdev_allocbuf(fc, it, &d->bufreq.tx);
			if (err) {
				it->flag &= ~FWXFERQ_OPEN;
				break;
			}
		}
		it->flag &= ~0xff;
		it->flag |= (0x3f & ichreq->ch);
		it->flag |= ((0x3 & ichreq->tag) << 6);
		d->it = it;
		break;

	case FW_GTSTREAM:
		if (it != NULL) {
			ichreq->ch = it->flag & 0x3f;
			ichreq->tag = it->flag >> 2 & 0x3;
		} else
			err = EINVAL;
		break;

	case FW_SRSTREAM:
		if (ir == NULL) {
			i = fw_open_isodma(fc, /* tx */0);
			if (i < 0) {
				err = EBUSY;
				break;
			}
			ir = fc->ir[i];
			err = fwdev_allocbuf(fc, ir, &d->bufreq.rx);
			if (err) {
				ir->flag &= ~FWXFERQ_OPEN;
				break;
			}
		}
		ir->flag &= ~0xff;
		ir->flag |= (0x3f & ichreq->ch);
		ir->flag |= ((0x3 & ichreq->tag) << 6);
		d->ir = ir;
		err = fc->irx_enable(fc, ir->dmach);
		break;

	case FW_GRSTREAM:
		if (d->ir != NULL) {
			ichreq->ch = ir->flag & 0x3f;
			ichreq->tag = ir->flag >> 2 & 0x3;
		} else
			err = EINVAL;
		break;

	case FW_SSTBUF:
		memcpy(&d->bufreq, ibufreq, sizeof(d->bufreq));
		break;

	case FW_GSTBUF:
		memset(&ibufreq->rx, 0, sizeof(ibufreq->rx));
		if (ir != NULL) {
			ibufreq->rx.nchunk = ir->bnchunk;
			ibufreq->rx.npacket = ir->bnpacket;
			ibufreq->rx.psize = ir->psize;
		}
		memset(&ibufreq->tx, 0, sizeof(ibufreq->tx));
		if (it != NULL) {
			ibufreq->tx.nchunk = it->bnchunk;
			ibufreq->tx.npacket = it->bnpacket;
			ibufreq->tx.psize = it->psize;
		}
		break;

	case FW_ASYREQ:
	{
		const struct tcode_info *tinfo;
		int pay_len = 0;

		fp = &asyreq->pkt;
		tinfo = &fc->tcode[fp->mode.hdr.tcode];

		if ((tinfo->flag & FWTI_BLOCK_ASY) != 0)
			pay_len = MAX(0, asyreq->req.len - tinfo->hdr_len);

		xfer = fw_xfer_alloc_buf(M_FW, pay_len, PAGE_SIZE/*XXX*/);
		if (xfer == NULL)
			return ENOMEM;

		switch (asyreq->req.type) {
		case FWASREQNODE:
			break;

		case FWASREQEUI:
			fwdev = fw_noderesolve_eui64(fc, &asyreq->req.dst.eui);
			if (fwdev == NULL) {
				aprint_error_dev(fc->bdev,
				    "cannot find node\n");
				err = EINVAL;
				goto out;
			}
			fp->mode.hdr.dst = FWLOCALBUS | fwdev->dst;
			break;

		case FWASRESTL:
			/* XXX what's this? */
			break;

		case FWASREQSTREAM:
			/* nothing to do */
			break;
		}

		memcpy(&xfer->send.hdr, fp, tinfo->hdr_len);
		if (pay_len > 0)
			memcpy(xfer->send.payload, (char *)fp + tinfo->hdr_len,
			    pay_len);
		xfer->send.spd = asyreq->req.sped;
		xfer->hand = fw_xferwake;

		if ((err = fw_asyreq(fc, -1, xfer)) != 0)
			goto out;
		if ((err = fw_xferwait(xfer)) != 0)
			goto out;
		if (xfer->resp != 0) {
			err = EIO;
			goto out;
		}
		if ((tinfo->flag & FWTI_TLABEL) == 0)
			goto out;

		/* copy response */
		tinfo = &fc->tcode[xfer->recv.hdr.mode.hdr.tcode];
		if (xfer->recv.hdr.mode.hdr.tcode == FWTCODE_RRESB ||
		    xfer->recv.hdr.mode.hdr.tcode == FWTCODE_LRES) {
			pay_len = xfer->recv.pay_len;
			if (asyreq->req.len >=
			    xfer->recv.pay_len + tinfo->hdr_len)
				asyreq->req.len =
				    xfer->recv.pay_len + tinfo->hdr_len;
			else {
				err = EINVAL;
				pay_len = 0;
			}
		} else
			pay_len = 0;
		memcpy(fp, &xfer->recv.hdr, tinfo->hdr_len);
		memcpy((char *)fp + tinfo->hdr_len, xfer->recv.payload,
		    pay_len);
out:
		fw_xfer_free_buf(xfer);
		break;
	}

	case FW_IBUSRST:
		fc->ibr(fc);
		break;

	case FW_CBINDADDR:
		fwb = fw_bindlookup(fc, bindreq->start.hi, bindreq->start.lo);
		if (fwb == NULL) {
			err = EINVAL;
			break;
		}
		fw_bindremove(fc, fwb);
		STAILQ_REMOVE(&d->binds, fwb, fw_bind, chlist);
		fw_xferlist_remove(&fwb->xferlist);
		free(fwb, M_FW);
		break;

	case FW_SBINDADDR:
		if (bindreq->len <= 0 ) {
			err = EINVAL;
			break;
		}
		if (bindreq->start.hi > 0xffff ) {
			err = EINVAL;
			break;
		}
		fwb = (struct fw_bind *)malloc(sizeof(struct fw_bind),
		    M_FW, M_WAITOK);
		if (fwb == NULL) {
			err = ENOMEM;
			break;
		}
		fwb->start = ((u_int64_t)bindreq->start.hi << 32) |
		    bindreq->start.lo;
		fwb->end = fwb->start +  bindreq->len;
		fwb->sc = (void *)d;
		STAILQ_INIT(&fwb->xferlist);
		err = fw_bindadd(fc, fwb);
		if (err == 0) {
			fw_xferlist_add(&fwb->xferlist, M_FW,
			    /* XXX */
			    PAGE_SIZE, PAGE_SIZE, 5, fc, (void *)fwb, fw_hand);
			STAILQ_INSERT_TAIL(&d->binds, fwb, chlist);
		}
		break;

	case FW_GDEVLST:
		i = len = 1;
		/* myself */
		devinfo = fwdevlst->dev;
		devinfo->dst = fc->nodeid;
		devinfo->status = 0;	/* XXX */
		devinfo->eui.hi = fc->eui.hi;
		devinfo->eui.lo = fc->eui.lo;
		STAILQ_FOREACH(fwdev, &fc->devices, link) {
			if (len < FW_MAX_DEVLST) {
				devinfo = &fwdevlst->dev[len++];
				devinfo->dst = fwdev->dst;
				devinfo->status =
				    (fwdev->status == FWDEVINVAL) ? 0 : 1;
				devinfo->eui.hi = fwdev->eui.hi;
				devinfo->eui.lo = fwdev->eui.lo;
			}
			i++;
		}
		fwdevlst->n = i;
		fwdevlst->info_len = len;
		break;

	case FW_GTPMAP:
		memcpy(data, fc->topology_map,
		    (fc->topology_map->crc_len + 1) * 4);
		break;

	case FW_GCROM:
		STAILQ_FOREACH(fwdev, &fc->devices, link)
			if (FW_EUI64_EQUAL(fwdev->eui, crom_buf->eui))
				break;
		if (fwdev == NULL) {
			if (!FW_EUI64_EQUAL(fc->eui, crom_buf->eui)) {
				err = FWNODE_INVAL;
				break;
			}
			/* myself */
			ptr = malloc(CROMSIZE, M_FW, M_WAITOK);
			len = CROMSIZE;
			for (i = 0; i < CROMSIZE/4; i++)
				((uint32_t *)ptr)[i] = ntohl(fc->config_rom[i]);
		} else {
			/* found */
			ptr = (void *)fwdev->csrrom;
			if (fwdev->rommax < CSRROMOFF)
				len = 0;
			else
				len = fwdev->rommax - CSRROMOFF + 4;
		}
		if (crom_buf->len < len)
			len = crom_buf->len;
		else
			crom_buf->len = len;
		err = copyout(ptr, crom_buf->ptr, len);
		if (fwdev == NULL)
			/* myself */
			free(ptr, M_FW);
		break;

	default:
		fc->ioctl(dev, cmd, data, flag, td);
		break;
	}
	return err;
}

int
fw_poll(dev_t dev, int events, struct lwp *td)
{
	struct firewire_softc *sc;
	struct fw_xferq *ir;
	int revents, tmp;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	ir = ((struct fw_drv1 *)sc->si_drv1)->ir;
	revents = 0;
	tmp = POLLIN | POLLRDNORM;
	if (events & tmp) {
		if (STAILQ_FIRST(&ir->q) != NULL)
			revents |= tmp;
		else
			selrecord(td, &ir->rsel);
	}
	tmp = POLLOUT | POLLWRNORM;
	if (events & tmp)
		/* XXX should be fixed */
		revents |= tmp;

	return revents;
}

paddr_t
fw_mmap(dev_t dev, off_t offset, int nproto)
{
	struct firewire_softc *sc;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	return EINVAL;
}

void
fw_strategy(struct bio *bp)
{
	struct firewire_softc *sc;
	dev_t dev = bp->bio_dev;

	sc = device_lookup_private(&ieee1394if_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return;

	if (DEV_FWMEM(dev)) {
		fwmem_strategy(bp);
		return;
	}

	bp->bio_error = EOPNOTSUPP;
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
}


static int
fwdev_allocbuf(struct firewire_comm *fc, struct fw_xferq *q,
	       struct fw_bufspec *b)
{
	int i;

	if (q->flag & (FWXFERQ_RUNNING | FWXFERQ_EXTBUF))
		return EBUSY;

	q->bulkxfer =
	    (struct fw_bulkxfer *)malloc(sizeof(struct fw_bulkxfer) * b->nchunk,
								M_FW, M_WAITOK);
	if (q->bulkxfer == NULL)
		return ENOMEM;

	b->psize = roundup2(b->psize, sizeof(uint32_t));
	q->buf = fwdma_malloc_multiseg(fc, sizeof(uint32_t), b->psize,
	    b->nchunk * b->npacket, BUS_DMA_WAITOK);

	if (q->buf == NULL) {
		free(q->bulkxfer, M_FW);
		q->bulkxfer = NULL;
		return ENOMEM;
	}
	q->bnchunk = b->nchunk;
	q->bnpacket = b->npacket;
	q->psize = (b->psize + 3) & ~3;
	q->queued = 0;

	STAILQ_INIT(&q->stvalid);
	STAILQ_INIT(&q->stfree);
	STAILQ_INIT(&q->stdma);
	q->stproc = NULL;

	for (i = 0 ; i < q->bnchunk; i++) {
		q->bulkxfer[i].poffset = i * q->bnpacket;
		q->bulkxfer[i].mbuf = NULL;
		STAILQ_INSERT_TAIL(&q->stfree, &q->bulkxfer[i], link);
	}

	q->flag &= ~FWXFERQ_MODEMASK;
	q->flag |= FWXFERQ_STREAM;
	q->flag |= FWXFERQ_EXTBUF;

	return 0;
}

static int
fwdev_freebuf(struct fw_xferq *q)
{

	if (q->flag & FWXFERQ_EXTBUF) {
		if (q->buf != NULL)
			fwdma_free_multiseg(q->buf);
		q->buf = NULL;
		free(q->bulkxfer, M_FW);
		q->bulkxfer = NULL;
		q->flag &= ~FWXFERQ_EXTBUF;
		q->psize = 0;
		q->maxq = FWMAXQUEUE;
	}
	return 0;
}

static int
fw_read_async(struct fw_drv1 *d, struct uio *uio, int ioflag)
{
	struct fw_xfer *xfer;
	struct fw_bind *fwb;
	struct fw_pkt *fp;
	const struct tcode_info *tinfo;
	int err = 0;

	mutex_enter(&d->fc->fc_mtx);

	for (;;) {
		xfer = STAILQ_FIRST(&d->rq);
		if (xfer == NULL && err == 0) {
			mutex_exit(&d->fc->fc_mtx);
			err = tsleep(&d->rq, FWPRI, "fwra", 0);
			if (err != 0)
				return err;
			mutex_enter(&d->fc->fc_mtx);
			continue;
		}
		break;
	}

	STAILQ_REMOVE_HEAD(&d->rq, link);
	mutex_exit(&d->fc->fc_mtx);
	fp = &xfer->recv.hdr;
#if 0 /* for GASP ?? */
	if (fc->irx_post != NULL)
		fc->irx_post(fc, fp->mode.ld);
#endif
	tinfo = &xfer->fc->tcode[fp->mode.hdr.tcode];
	err = uiomove((void *)fp, tinfo->hdr_len, uio);
	if (err)
		goto out;
	err = uiomove((void *)xfer->recv.payload, xfer->recv.pay_len, uio);

out:
	/* recycle this xfer */
	fwb = (struct fw_bind *)xfer->sc;
	fw_xfer_unload(xfer);
	xfer->recv.pay_len = PAGE_SIZE;
	mutex_enter(&d->fc->fc_mtx);
	STAILQ_INSERT_TAIL(&fwb->xferlist, xfer, link);
	mutex_exit(&d->fc->fc_mtx);
	return err;
}

static int
fw_write_async(struct fw_drv1 *d, struct uio *uio, int ioflag)
{
	struct fw_xfer *xfer;
	struct fw_pkt pkt;
	const struct tcode_info *tinfo;
	int err;

	memset(&pkt, 0, sizeof(struct fw_pkt));
	if ((err = uiomove((void *)&pkt, sizeof(uint32_t), uio)))
		return err;
	tinfo = &d->fc->tcode[pkt.mode.hdr.tcode];
	if ((err = uiomove((char *)&pkt + sizeof(uint32_t),
	    tinfo->hdr_len - sizeof(uint32_t), uio)))
		return err;

	if ((xfer = fw_xfer_alloc_buf(M_FW, uio->uio_resid,
	    PAGE_SIZE/*XXX*/)) == NULL)
		return ENOMEM;

	memcpy(&xfer->send.hdr, &pkt, sizeof(struct fw_pkt));
	xfer->send.pay_len = uio->uio_resid;
	if (uio->uio_resid > 0) {
		if ((err =
		    uiomove((void *)xfer->send.payload, uio->uio_resid, uio)))
			goto out;
	}

	xfer->fc = d->fc;
	xfer->sc = NULL;
	xfer->hand = fw_xferwake;
	xfer->send.spd = 2 /* XXX */;

	if ((err = fw_asyreq(xfer->fc, -1, xfer)))
		goto out;

	if ((err = fw_xferwait(xfer)))
		goto out;

	if (xfer->resp != 0) {
		err = xfer->resp;
		goto out;
	}

	if (xfer->flag == FWXF_RCVD) {
		mutex_enter(&xfer->fc->fc_mtx);
		STAILQ_INSERT_TAIL(&d->rq, xfer, link);
		mutex_exit(&xfer->fc->fc_mtx);
		return 0;
	}

out:
	fw_xfer_free(xfer);
	return err;
}

static void
fw_hand(struct fw_xfer *xfer)
{
	struct fw_bind *fwb;
	struct fw_drv1 *d;

	fwb = (struct fw_bind *)xfer->sc;
	d = (struct fw_drv1 *)fwb->sc;
	mutex_enter(&xfer->fc->fc_mtx);
	STAILQ_INSERT_TAIL(&d->rq, xfer, link);
	mutex_exit(&xfer->fc->fc_mtx);
	wakeup(&d->rq);
}
