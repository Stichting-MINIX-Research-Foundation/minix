/*	$NetBSD: ld_virtio.c,v 1.8 2015/05/05 10:56:13 ozaki-r Exp $	*/

/*
 * Copyright (c) 2010 Minoura Makoto.
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
__KERNEL_RCSID(0, "$NetBSD: ld_virtio.c,v 1.8 2015/05/05 10:56:13 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/mutex.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ldvar.h>
#include <dev/pci/virtioreg.h>
#include <dev/pci/virtiovar.h>

/*
 * ld_virtioreg:
 */
/* Configuration registers */
#define VIRTIO_BLK_CONFIG_CAPACITY	0 /* 64bit */
#define VIRTIO_BLK_CONFIG_SIZE_MAX	8 /* 32bit */
#define VIRTIO_BLK_CONFIG_SEG_MAX	12 /* 32bit */
#define VIRTIO_BLK_CONFIG_GEOMETRY_C	16 /* 16bit */
#define VIRTIO_BLK_CONFIG_GEOMETRY_H	18 /* 8bit */
#define VIRTIO_BLK_CONFIG_GEOMETRY_S	19 /* 8bit */
#define VIRTIO_BLK_CONFIG_BLK_SIZE	20 /* 32bit */

/* Feature bits */
#define VIRTIO_BLK_F_BARRIER	(1<<0)
#define VIRTIO_BLK_F_SIZE_MAX	(1<<1)
#define VIRTIO_BLK_F_SEG_MAX	(1<<2)
#define VIRTIO_BLK_F_GEOMETRY	(1<<4)
#define VIRTIO_BLK_F_RO		(1<<5)
#define VIRTIO_BLK_F_BLK_SIZE	(1<<6)
#define VIRTIO_BLK_F_SCSI	(1<<7)
#define VIRTIO_BLK_F_FLUSH	(1<<9)

/* Command */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1
#define VIRTIO_BLK_T_BARRIER	0x80000000

/* Status */
#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1

/* Request header structure */
struct virtio_blk_req_hdr {
	uint32_t	type;	/* VIRTIO_BLK_T_* */
	uint32_t	ioprio;
	uint64_t	sector;
} __packed;
/* 512*virtio_blk_req_hdr.sector byte payload and 1 byte status follows */


/*
 * ld_virtiovar:
 */
struct virtio_blk_req {
	struct virtio_blk_req_hdr	vr_hdr;
	uint8_t				vr_status;
	struct buf			*vr_bp;
	bus_dmamap_t			vr_cmdsts;
	bus_dmamap_t			vr_payload;
};

struct ld_virtio_softc {
	struct ld_softc		sc_ld;
	device_t		sc_dev;

	struct virtio_softc	*sc_virtio;
	struct virtqueue	sc_vq[1];

	struct virtio_blk_req	*sc_reqs;
	bus_dma_segment_t	sc_reqs_segs[1];

	kmutex_t		sc_lock;

	int			sc_readonly;
};

static int	ld_virtio_match(device_t, cfdata_t, void *);
static void	ld_virtio_attach(device_t, device_t, void *);
static int	ld_virtio_detach(device_t, int);

CFATTACH_DECL_NEW(ld_virtio, sizeof(struct ld_virtio_softc),
    ld_virtio_match, ld_virtio_attach, ld_virtio_detach, NULL);

static int
ld_virtio_match(device_t parent, cfdata_t match, void *aux)
{
	struct virtio_softc *va = aux;

	if (va->sc_childdevid == PCI_PRODUCT_VIRTIO_BLOCK)
		return 1;

	return 0;
}

static int ld_virtio_vq_done(struct virtqueue *);
static int ld_virtio_dump(struct ld_softc *, void *, int, int);
static int ld_virtio_start(struct ld_softc *, struct buf *);

static int
ld_virtio_alloc_reqs(struct ld_virtio_softc *sc, int qsize)
{
	int allocsize, r, rsegs, i;
	struct ld_softc *ld = &sc->sc_ld;
	void *vaddr;

	allocsize = sizeof(struct virtio_blk_req) * qsize;
	r = bus_dmamem_alloc(sc->sc_virtio->sc_dmat, allocsize, 0, 0,
			     &sc->sc_reqs_segs[0], 1, &rsegs, BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(sc->sc_dev,
				 "DMA memory allocation failed, size %d, "
				 "error code %d\n", allocsize, r);
		goto err_none;
	}
	r = bus_dmamem_map(sc->sc_virtio->sc_dmat,
			   &sc->sc_reqs_segs[0], 1, allocsize,
			   &vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(sc->sc_dev,
				 "DMA memory map failed, "
				 "error code %d\n", r);
		goto err_dmamem_alloc;
	}
	sc->sc_reqs = vaddr;
	memset(vaddr, 0, allocsize);
	for (i = 0; i < qsize; i++) {
		struct virtio_blk_req *vr = &sc->sc_reqs[i];
		r = bus_dmamap_create(sc->sc_virtio->sc_dmat,
				      offsetof(struct virtio_blk_req, vr_bp),
				      1,
				      offsetof(struct virtio_blk_req, vr_bp),
				      0,
				      BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
				      &vr->vr_cmdsts);
		if (r != 0) {
			aprint_error_dev(sc->sc_dev,
					 "command dmamap creation failed, "
					 "error code %d\n", r);
			goto err_reqs;
		}
		r = bus_dmamap_load(sc->sc_virtio->sc_dmat, vr->vr_cmdsts,
				    &vr->vr_hdr,
				    offsetof(struct virtio_blk_req, vr_bp),
				    NULL, BUS_DMA_NOWAIT);
		if (r != 0) {
			aprint_error_dev(sc->sc_dev,
					 "command dmamap load failed, "
					 "error code %d\n", r);
			goto err_reqs;
		}
		r = bus_dmamap_create(sc->sc_virtio->sc_dmat,
				      ld->sc_maxxfer,
				      (ld->sc_maxxfer / NBPG) + 2,
				      ld->sc_maxxfer,
				      0,
				      BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
				      &vr->vr_payload);
		if (r != 0) {
			aprint_error_dev(sc->sc_dev,
					 "payload dmamap creation failed, "
					 "error code %d\n", r);
			goto err_reqs;
		}
	}
	return 0;

err_reqs:
	for (i = 0; i < qsize; i++) {
		struct virtio_blk_req *vr = &sc->sc_reqs[i];
		if (vr->vr_cmdsts) {
			bus_dmamap_destroy(sc->sc_virtio->sc_dmat,
					   vr->vr_cmdsts);
			vr->vr_cmdsts = 0;
		}
		if (vr->vr_payload) {
			bus_dmamap_destroy(sc->sc_virtio->sc_dmat,
					   vr->vr_payload);
			vr->vr_payload = 0;
		}
	}
	bus_dmamem_unmap(sc->sc_virtio->sc_dmat, sc->sc_reqs, allocsize);
err_dmamem_alloc:
	bus_dmamem_free(sc->sc_virtio->sc_dmat, &sc->sc_reqs_segs[0], 1);
err_none:
	return -1;
}

static void
ld_virtio_attach(device_t parent, device_t self, void *aux)
{
	struct ld_virtio_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	struct virtio_softc *vsc = device_private(parent);
	uint32_t features;
	int qsize, maxxfersize;

	if (vsc->sc_child != NULL) {
		aprint_normal(": child already attached for %s; "
			      "something wrong...\n",
			      device_xname(parent));
		return;
	}
	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_dev = self;
	sc->sc_virtio = vsc;

	vsc->sc_child = self;
	vsc->sc_ipl = IPL_BIO;
	vsc->sc_vqs = &sc->sc_vq[0];
	vsc->sc_nvqs = 1;
	vsc->sc_config_change = NULL;
	vsc->sc_intrhand = virtio_vq_intr;
	vsc->sc_flags = 0;

	features = virtio_negotiate_features(vsc,
					     (VIRTIO_BLK_F_SIZE_MAX |
					      VIRTIO_BLK_F_SEG_MAX |
					      VIRTIO_BLK_F_GEOMETRY |
					      VIRTIO_BLK_F_RO |
					      VIRTIO_BLK_F_BLK_SIZE));
	if (features & VIRTIO_BLK_F_RO)
		sc->sc_readonly = 1;
	else
		sc->sc_readonly = 0;

	ld->sc_secsize = 512;
	if (features & VIRTIO_BLK_F_BLK_SIZE) {
		ld->sc_secsize = virtio_read_device_config_4(vsc,
					VIRTIO_BLK_CONFIG_BLK_SIZE);
	}
	maxxfersize = MAXPHYS;
#if 0	/* At least genfs_io assumes maxxfer == MAXPHYS. */
	if (features & VIRTIO_BLK_F_SEG_MAX) {
		maxxfersize = virtio_read_device_config_4(vsc,
					VIRTIO_BLK_CONFIG_SEG_MAX)
				* ld->sc_secsize;
		if (maxxfersize > MAXPHYS)
			maxxfersize = MAXPHYS;
	}
#endif

	if (virtio_alloc_vq(vsc, &sc->sc_vq[0], 0,
			    maxxfersize, maxxfersize / NBPG + 2,
			    "I/O request") != 0) {
		goto err;
	}
	qsize = sc->sc_vq[0].vq_num;
	sc->sc_vq[0].vq_done = ld_virtio_vq_done;

	ld->sc_dv = self;
	ld->sc_secperunit = virtio_read_device_config_8(vsc,
				VIRTIO_BLK_CONFIG_CAPACITY);
	ld->sc_maxxfer = maxxfersize;
	if (features & VIRTIO_BLK_F_GEOMETRY) {
		ld->sc_ncylinders = virtio_read_device_config_2(vsc,
					VIRTIO_BLK_CONFIG_GEOMETRY_C);
		ld->sc_nheads     = virtio_read_device_config_1(vsc,
					VIRTIO_BLK_CONFIG_GEOMETRY_H);
		ld->sc_nsectors   = virtio_read_device_config_1(vsc,
					VIRTIO_BLK_CONFIG_GEOMETRY_S);
	}
	ld->sc_maxqueuecnt = qsize;

	if (ld_virtio_alloc_reqs(sc, qsize) < 0)
		goto err;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_BIO);

	ld->sc_dump = ld_virtio_dump;
	ld->sc_flush = NULL;
	ld->sc_start = ld_virtio_start;

	ld->sc_flags = LDF_ENABLED;
	ldattach(ld);

	return;

err:
	vsc->sc_child = (void*)1;
	return;
}

static int
ld_virtio_start(struct ld_softc *ld, struct buf *bp)
{
	/* splbio */
	struct ld_virtio_softc *sc = device_private(ld->sc_dv);
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[0];
	struct virtio_blk_req *vr;
	int r;
	int isread = (bp->b_flags & B_READ);
	int slot;

	if (sc->sc_readonly && !isread)
		return EIO;

	r = virtio_enqueue_prep(vsc, vq, &slot);
	if (r != 0)
		return r;
	vr = &sc->sc_reqs[slot];
	r = bus_dmamap_load(vsc->sc_dmat, vr->vr_payload,
			    bp->b_data, bp->b_bcount, NULL,
			    ((isread?BUS_DMA_READ:BUS_DMA_WRITE)
			     |BUS_DMA_NOWAIT));
	if (r != 0)
		return r;

	r = virtio_enqueue_reserve(vsc, vq, slot, vr->vr_payload->dm_nsegs + 2);
	if (r != 0) {
		bus_dmamap_unload(vsc->sc_dmat, vr->vr_payload);
		return r;
	}

	vr->vr_bp = bp;
	vr->vr_hdr.type = isread?VIRTIO_BLK_T_IN:VIRTIO_BLK_T_OUT;
	vr->vr_hdr.ioprio = 0;
	vr->vr_hdr.sector = bp->b_rawblkno * sc->sc_ld.sc_secsize / 512;

	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			0, sizeof(struct virtio_blk_req_hdr),
			BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload,
			0, bp->b_bcount,
			isread?BUS_DMASYNC_PREREAD:BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			offsetof(struct virtio_blk_req, vr_status),
			sizeof(uint8_t),
			BUS_DMASYNC_PREREAD);

	virtio_enqueue_p(vsc, vq, slot, vr->vr_cmdsts,
			 0, sizeof(struct virtio_blk_req_hdr),
			 true);
	virtio_enqueue(vsc, vq, slot, vr->vr_payload, !isread);
	virtio_enqueue_p(vsc, vq, slot, vr->vr_cmdsts,
			 offsetof(struct virtio_blk_req, vr_status),
			 sizeof(uint8_t),
			 false);
	virtio_enqueue_commit(vsc, vq, slot, true);

	return 0;
}

static void
ld_virtio_vq_done1(struct ld_virtio_softc *sc, struct virtio_softc *vsc,
		   struct virtqueue *vq, int slot)
{
	struct virtio_blk_req *vr = &sc->sc_reqs[slot];
	struct buf *bp = vr->vr_bp;

	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			0, sizeof(struct virtio_blk_req_hdr),
			BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload,
			0, bp->b_bcount,
			(bp->b_flags & B_READ)?BUS_DMASYNC_POSTREAD
					      :BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			sizeof(struct virtio_blk_req_hdr), sizeof(uint8_t),
			BUS_DMASYNC_POSTREAD);

	if (vr->vr_status != VIRTIO_BLK_S_OK) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
	} else {
		bp->b_error = 0;
		bp->b_resid = 0;
	}

	virtio_dequeue_commit(vsc, vq, slot);

	lddone(&sc->sc_ld, bp);
}

static int
ld_virtio_vq_done(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct ld_virtio_softc *sc = device_private(vsc->sc_child);
	int r = 0;
	int slot;

again:
	if (virtio_dequeue(vsc, vq, &slot, NULL))
		return r;
	r = 1;

	ld_virtio_vq_done1(sc, vsc, vq, slot);
	goto again;
}

static int
ld_virtio_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{
	struct ld_virtio_softc *sc = device_private(ld->sc_dv);
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[0];
	struct virtio_blk_req *vr;
	int slot, r;

	if (sc->sc_readonly)
		return EIO;

	r = virtio_enqueue_prep(vsc, vq, &slot);
	if (r != 0) {
		if (r == EAGAIN) { /* no free slot; dequeue first */
			delay(100);
			ld_virtio_vq_done(vq);
			r = virtio_enqueue_prep(vsc, vq, &slot);
			if (r != 0)
				return r;
		}
		return r;
	}
	vr = &sc->sc_reqs[slot];
	r = bus_dmamap_load(vsc->sc_dmat, vr->vr_payload,
			    data, blkcnt*ld->sc_secsize, NULL,
			    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
	if (r != 0)
		return r;

	r = virtio_enqueue_reserve(vsc, vq, slot, vr->vr_payload->dm_nsegs + 2);
	if (r != 0) {
		bus_dmamap_unload(vsc->sc_dmat, vr->vr_payload);
		return r;
	}

	vr->vr_bp = (void*)0xdeadbeef;
	vr->vr_hdr.type = VIRTIO_BLK_T_OUT;
	vr->vr_hdr.ioprio = 0;
	vr->vr_hdr.sector = (daddr_t) blkno * ld->sc_secsize / 512;

	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			0, sizeof(struct virtio_blk_req_hdr),
			BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload,
			0, blkcnt*ld->sc_secsize,
			BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			offsetof(struct virtio_blk_req, vr_status),
			sizeof(uint8_t),
			BUS_DMASYNC_PREREAD);

	virtio_enqueue_p(vsc, vq, slot, vr->vr_cmdsts,
			 0, sizeof(struct virtio_blk_req_hdr),
			 true);
	virtio_enqueue(vsc, vq, slot, vr->vr_payload, true);
	virtio_enqueue_p(vsc, vq, slot, vr->vr_cmdsts,
			 offsetof(struct virtio_blk_req, vr_status),
			 sizeof(uint8_t),
			 false);
	virtio_enqueue_commit(vsc, vq, slot, true);

	for ( ; ; ) {
		int dslot;

		r = virtio_dequeue(vsc, vq, &dslot, NULL);
		if (r != 0)
			continue;
		if (dslot != slot) {
			ld_virtio_vq_done1(sc, vsc, vq, dslot);
			continue;
		} else
			break;
	}
		
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			0, sizeof(struct virtio_blk_req_hdr),
			BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_payload,
			0, blkcnt*ld->sc_secsize,
			BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(vsc->sc_dmat, vr->vr_cmdsts,
			offsetof(struct virtio_blk_req, vr_status),
			sizeof(uint8_t),
			BUS_DMASYNC_POSTREAD);
	if (vr->vr_status == VIRTIO_BLK_S_OK)
		r = 0;
	else
		r = EIO;
	virtio_dequeue_commit(vsc, vq, slot);

	return r;
}

static int
ld_virtio_detach(device_t self, int flags)
{
	struct ld_virtio_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	bus_dma_tag_t dmat = sc->sc_virtio->sc_dmat;
	int r, i, qsize;

	qsize = sc->sc_vq[0].vq_num;
	r = ldbegindetach(ld, flags);
	if (r != 0)
		return r;
	virtio_reset(sc->sc_virtio);
	virtio_free_vq(sc->sc_virtio, &sc->sc_vq[0]);

	for (i = 0; i < qsize; i++) {
		bus_dmamap_destroy(dmat,
				   sc->sc_reqs[i].vr_cmdsts);
		bus_dmamap_destroy(dmat,
				   sc->sc_reqs[i].vr_payload);
	}
	bus_dmamem_unmap(dmat, sc->sc_reqs,
			 sizeof(struct virtio_blk_req) * qsize);
	bus_dmamem_free(dmat, &sc->sc_reqs_segs[0], 1);

	ldenddetach(ld);

	return 0;
}
