/*	$NetBSD: ld_ataraid.c,v 1.40 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support for ATA RAID logical disks.
 *
 * Note that all the RAID happens in software here; the ATA RAID
 * controllers we're dealing with (Promise, etc.) only support
 * configuration data on the component disks, with the BIOS supporting
 * booting from the RAID volumes.
 *            
 * bio(4) support was written by Juan Romero Pardines <xtraeme@gmail.com>.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_ataraid.c,v 1.40 2015/04/13 16:33:24 riastradh Exp $");

#include "bio.h"


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#if NBIO > 0
#include <dev/ata/atavar.h>
#include <dev/ata/atareg.h>
#include <dev/ata/wdvar.h>
#include <dev/biovar.h>
#include <dev/scsipi/scsipiconf.h> /* for scsipi_strvis() */
#endif

#include <miscfs/specfs/specdev.h>

#include <dev/ldvar.h>

#include <dev/ata/ata_raidvar.h>

struct ld_ataraid_softc {
	struct ld_softc sc_ld;

	struct ataraid_array_info *sc_aai;
	struct vnode *sc_vnodes[ATA_RAID_MAX_DISKS];

	void	(*sc_iodone)(struct buf *);

       pool_cache_t sc_cbufpool;

       SIMPLEQ_HEAD(, cbuf) sc_cbufq;

       void    *sc_sih_cookie;
};

static int	ld_ataraid_match(device_t, cfdata_t, void *);
static void	ld_ataraid_attach(device_t, device_t, void *);

static int	ld_ataraid_dump(struct ld_softc *, void *, int, int);

static int     cbufpool_ctor(void *, void *, int);
static void    cbufpool_dtor(void *, void *);

static void    ld_ataraid_start_vstrategy(void *);
static int	ld_ataraid_start_span(struct ld_softc *, struct buf *);

static int	ld_ataraid_start_raid0(struct ld_softc *, struct buf *);
static void	ld_ataraid_iodone_raid0(struct buf *);

#if NBIO > 0
static int	ld_ataraid_bioctl(device_t, u_long, void *);
static int	ld_ataraid_bioinq(struct ld_ataraid_softc *, struct bioc_inq *);
static int	ld_ataraid_biovol(struct ld_ataraid_softc *, struct bioc_vol *);
static int	ld_ataraid_biodisk(struct ld_ataraid_softc *,
				   struct bioc_disk *);
#endif

CFATTACH_DECL_NEW(ld_ataraid, sizeof(struct ld_ataraid_softc),
    ld_ataraid_match, ld_ataraid_attach, NULL, NULL);

struct cbuf {
	struct buf	cb_buf;		/* new I/O buf */
	struct buf	*cb_obp;	/* ptr. to original I/O buf */
	struct ld_ataraid_softc *cb_sc;	/* pointer to ld softc */
	u_int		cb_comp;	/* target component */
	SIMPLEQ_ENTRY(cbuf) cb_q;	/* fifo of component buffers */
	struct cbuf	*cb_other;	/* other cbuf in case of mirror */
	int		cb_flags;
#define	CBUF_IODONE	0x00000001	/* I/O is already successfully done */
};

#define        CBUF_GET()      pool_cache_get(sc->sc_cbufpool, PR_NOWAIT);
#define        CBUF_PUT(cbp)   pool_cache_put(sc->sc_cbufpool, (cbp))

static int
ld_ataraid_match(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

static void
ld_ataraid_attach(device_t parent, device_t self, void *aux)
{
	struct ld_ataraid_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	struct ataraid_array_info *aai = aux;
	struct ataraid_disk_info *adi = NULL;
	const char *level;
	struct vnode *vp;
	char unklev[32];
	u_int i;

	ld->sc_dv = self;

       sc->sc_cbufpool = pool_cache_init(sizeof(struct cbuf), 0,
           0, 0, "ldcbuf", NULL, IPL_BIO, cbufpool_ctor, cbufpool_dtor, sc);
       sc->sc_sih_cookie = softint_establish(SOFTINT_BIO,
           ld_ataraid_start_vstrategy, sc);

	sc->sc_aai = aai;	/* this data persists */

	ld->sc_maxxfer = MAXPHYS * aai->aai_width;	/* XXX */
	ld->sc_secperunit = aai->aai_capacity;
	ld->sc_secsize = 512;				/* XXX */
	ld->sc_maxqueuecnt = 128;			/* XXX */
	ld->sc_dump = ld_ataraid_dump;

	switch (aai->aai_level) {
	case AAI_L_SPAN:
		level = "SPAN";
		ld->sc_start = ld_ataraid_start_span;
		sc->sc_iodone = ld_ataraid_iodone_raid0;
		break;

	case AAI_L_RAID0:
		level = "RAID-0";
		ld->sc_start = ld_ataraid_start_raid0;
		sc->sc_iodone = ld_ataraid_iodone_raid0;
		break;

	case AAI_L_RAID1:
		level = "RAID-1";
		ld->sc_start = ld_ataraid_start_raid0;
		sc->sc_iodone = ld_ataraid_iodone_raid0;
		break;

	case AAI_L_RAID0 | AAI_L_RAID1:
		level = "RAID-10";
		ld->sc_start = ld_ataraid_start_raid0;
		sc->sc_iodone = ld_ataraid_iodone_raid0;
		break;

	default:
		snprintf(unklev, sizeof(unklev), "<unknown level 0x%x>",
		    aai->aai_level);
		level = unklev;
	}

	aprint_naive(": ATA %s array\n", level);
	aprint_normal(": %s ATA %s array\n",
	    ata_raid_type_name(aai->aai_type), level);

	if (ld->sc_start == NULL) {
		aprint_error_dev(ld->sc_dv, "unsupported array type\n");
		return;
	}

	/*
	 * We get a geometry from the device; use it.
	 */
	ld->sc_nheads = aai->aai_heads;
	ld->sc_nsectors = aai->aai_sectors;
	ld->sc_ncylinders = aai->aai_cylinders;

	/*
	 * Configure all the component disks.
	 */
	for (i = 0; i < aai->aai_ndisks; i++) {
		adi = &aai->aai_disks[i];
		vp = ata_raid_disk_vnode_find(adi);
		if (vp == NULL) {
			/*
			 * XXX This is bogus.  We should just mark the
			 * XXX component as FAILED, and write-back new
			 * XXX config blocks.
			 */
			break;
		}
		sc->sc_vnodes[i] = vp;
	}
	if (i == aai->aai_ndisks) {
		ld->sc_flags = LDF_ENABLED;
		goto finish;
	}

	for (i = 0; i < aai->aai_ndisks; i++) {
		vp = sc->sc_vnodes[i];
		sc->sc_vnodes[i] = NULL;
		if (vp != NULL)
			(void) vn_close(vp, FREAD|FWRITE, NOCRED);
	}

 finish:
#if NBIO > 0
	if (bio_register(self, ld_ataraid_bioctl) != 0)
		panic("%s: bioctl registration failed\n",
		    device_xname(ld->sc_dv));
#endif
       SIMPLEQ_INIT(&sc->sc_cbufq);
	ldattach(ld);
}

static int
cbufpool_ctor(void *arg, void *obj, int flags)
{
       struct ld_ataraid_softc *sc = arg;
       struct ld_softc *ld = &sc->sc_ld;
       struct cbuf *cbp = obj;

       /* We release/reacquire the spinlock before calling buf_init() */
       mutex_exit(&ld->sc_mutex);
       buf_init(&cbp->cb_buf);
       mutex_enter(&ld->sc_mutex);

       return 0;
}

static void
cbufpool_dtor(void *arg, void *obj)
{
       struct cbuf *cbp = obj;

       buf_destroy(&cbp->cb_buf);
}

static struct cbuf *
ld_ataraid_make_cbuf(struct ld_ataraid_softc *sc, struct buf *bp,
    u_int comp, daddr_t bn, void *addr, long bcount)
{
	struct cbuf *cbp;

	cbp = CBUF_GET();
	if (cbp == NULL)
               return NULL;
	cbp->cb_buf.b_flags = bp->b_flags;
	cbp->cb_buf.b_oflags = bp->b_oflags;
	cbp->cb_buf.b_cflags = bp->b_cflags;
	cbp->cb_buf.b_iodone = sc->sc_iodone;
	cbp->cb_buf.b_proc = bp->b_proc;
	cbp->cb_buf.b_vp = sc->sc_vnodes[comp];
	cbp->cb_buf.b_objlock = sc->sc_vnodes[comp]->v_interlock;
	cbp->cb_buf.b_blkno = bn + sc->sc_aai->aai_offset;
	cbp->cb_buf.b_data = addr;
	cbp->cb_buf.b_bcount = bcount;

	/* Context for iodone */
	cbp->cb_obp = bp;
	cbp->cb_sc = sc;
	cbp->cb_comp = comp;
	cbp->cb_other = NULL;
	cbp->cb_flags = 0;

       return cbp;
}

static void
ld_ataraid_start_vstrategy(void *arg)
{
       struct ld_ataraid_softc *sc = arg;
       struct cbuf *cbp;

       while ((cbp = SIMPLEQ_FIRST(&sc->sc_cbufq)) != NULL) {
               SIMPLEQ_REMOVE_HEAD(&sc->sc_cbufq, cb_q);
               if ((cbp->cb_buf.b_flags & B_READ) == 0) {
                       mutex_enter(cbp->cb_buf.b_vp->v_interlock);
                       cbp->cb_buf.b_vp->v_numoutput++;
                       mutex_exit(cbp->cb_buf.b_vp->v_interlock);
               }
               VOP_STRATEGY(cbp->cb_buf.b_vp, &cbp->cb_buf);
       }
}

static int
ld_ataraid_start_span(struct ld_softc *ld, struct buf *bp)
{
	struct ld_ataraid_softc *sc = (void *) ld;
	struct ataraid_array_info *aai = sc->sc_aai;
	struct ataraid_disk_info *adi;
	struct cbuf *cbp;
	char *addr;
	daddr_t bn;
	long bcount, rcount;
	u_int comp;

	/* Allocate component buffers. */
	addr = bp->b_data;

	/* Find the first component. */
	comp = 0;
	adi = &aai->aai_disks[comp];
	bn = bp->b_rawblkno;
	while (bn >= adi->adi_compsize) {
		bn -= adi->adi_compsize;
		adi = &aai->aai_disks[++comp];
	}

	bp->b_resid = bp->b_bcount;

	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		rcount = bp->b_bcount;
		if ((adi->adi_compsize - bn) < btodb(rcount))
			rcount = dbtob(adi->adi_compsize - bn);

		cbp = ld_ataraid_make_cbuf(sc, bp, comp, bn, addr, rcount);
		if (cbp == NULL) {
			/* Free the already allocated component buffers. */
                       while ((cbp = SIMPLEQ_FIRST(&sc->sc_cbufq)) != NULL) {
                               SIMPLEQ_REMOVE_HEAD(&sc->sc_cbufq, cb_q);
				CBUF_PUT(cbp);
			}
                       return EAGAIN;
		}

		/*
		 * For a span, we always know we advance to the next disk,
		 * and always start at offset 0 on that disk.
		 */
		adi = &aai->aai_disks[++comp];
		bn = 0;

               SIMPLEQ_INSERT_TAIL(&sc->sc_cbufq, cbp, cb_q);
		addr += rcount;
	}

	/* Now fire off the requests. */
       softint_schedule(sc->sc_sih_cookie);

       return 0;
}

static int
ld_ataraid_start_raid0(struct ld_softc *ld, struct buf *bp)
{
       struct ld_ataraid_softc *sc = (void *)ld;
	struct ataraid_array_info *aai = sc->sc_aai;
	struct ataraid_disk_info *adi;
	struct cbuf *cbp, *other_cbp;
	char *addr;
	daddr_t bn, cbn, tbn, off;
	long bcount, rcount;
	u_int comp;
	const int read = bp->b_flags & B_READ;
	const int mirror = aai->aai_level & AAI_L_RAID1;
       int error = 0;

	/* Allocate component buffers. */
	addr = bp->b_data;
	bn = bp->b_rawblkno;

	bp->b_resid = bp->b_bcount;

	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		tbn = bn / aai->aai_interleave;
		off = bn % aai->aai_interleave;

		if (__predict_false(tbn == aai->aai_capacity /
					   aai->aai_interleave)) {
			/* Last stripe. */
			daddr_t sz = (aai->aai_capacity -
				      (tbn * aai->aai_interleave)) /
				     aai->aai_width;
			comp = off / sz;
			cbn = ((tbn / aai->aai_width) * aai->aai_interleave) +
			    (off % sz);
			rcount = min(bcount, dbtob(sz));
		} else {
			comp = tbn % aai->aai_width;
			cbn = ((tbn / aai->aai_width) * aai->aai_interleave) +
			    off;
			rcount = min(bcount, dbtob(aai->aai_interleave - off));
		}

		/*
		 * See if a component is valid.
		 */
try_mirror:
		adi = &aai->aai_disks[comp];
		if ((adi->adi_status & ADI_S_ONLINE) == 0) {
			if (mirror && comp < aai->aai_width) {
				comp += aai->aai_width;
				goto try_mirror;
			}

			/*
			 * No component available.
			 */
			error = EIO;
			goto free_and_exit;
		}

		cbp = ld_ataraid_make_cbuf(sc, bp, comp, cbn, addr, rcount);
		if (cbp == NULL) {
resource_shortage:
			error = EAGAIN;
free_and_exit:
			/* Free the already allocated component buffers. */
                       while ((cbp = SIMPLEQ_FIRST(&sc->sc_cbufq)) != NULL) {
                               SIMPLEQ_REMOVE_HEAD(&sc->sc_cbufq, cb_q);
				CBUF_PUT(cbp);
			}
                       return error;
		}
               SIMPLEQ_INSERT_TAIL(&sc->sc_cbufq, cbp, cb_q);
		if (mirror && !read && comp < aai->aai_width) {
			comp += aai->aai_width;
			adi = &aai->aai_disks[comp];
			if (adi->adi_status & ADI_S_ONLINE) {
				other_cbp = ld_ataraid_make_cbuf(sc, bp,
				    comp, cbn, addr, rcount);
				if (other_cbp == NULL)
					goto resource_shortage;
                               SIMPLEQ_INSERT_TAIL(&sc->sc_cbufq,
                                   other_cbp, cb_q);
				other_cbp->cb_other = cbp;
				cbp->cb_other = other_cbp;
			}
		}
		bn += btodb(rcount);
		addr += rcount;
	}

	/* Now fire off the requests. */
       softint_schedule(sc->sc_sih_cookie);

       return error;
}

/*
 * Called at interrupt time.  Mark the component as done and if all
 * components are done, take an "interrupt".
 */
static void
ld_ataraid_iodone_raid0(struct buf *vbp)
{
	struct cbuf *cbp = (struct cbuf *) vbp, *other_cbp;
	struct buf *bp = cbp->cb_obp;
	struct ld_ataraid_softc *sc = cbp->cb_sc;
	struct ataraid_array_info *aai = sc->sc_aai;
	struct ataraid_disk_info *adi;
	long count;
	int s, iodone;

	s = splbio();

	iodone = cbp->cb_flags & CBUF_IODONE;
	other_cbp = cbp->cb_other;
	if (other_cbp != NULL)
		/* You are alone */
		other_cbp->cb_other = NULL;

	if (cbp->cb_buf.b_error != 0) {
		/*
		 * Mark this component broken.
		 */
		adi = &aai->aai_disks[cbp->cb_comp];
		adi->adi_status &= ~ADI_S_ONLINE;

		printf("%s: error %d on component %d (%s)\n",
		    device_xname(sc->sc_ld.sc_dv), bp->b_error, cbp->cb_comp,
		    device_xname(adi->adi_dev));

		/*
		 * If we didn't see an error yet and we are reading
		 * RAID1 disk, try another component.
		 */
		if (bp->b_error == 0 &&
		    (cbp->cb_buf.b_flags & B_READ) != 0 &&
		    (aai->aai_level & AAI_L_RAID1) != 0 &&
		    cbp->cb_comp < aai->aai_width) {
			cbp->cb_comp += aai->aai_width;
			adi = &aai->aai_disks[cbp->cb_comp];
			if (adi->adi_status & ADI_S_ONLINE) {
				cbp->cb_buf.b_error = 0;
				VOP_STRATEGY(cbp->cb_buf.b_vp, &cbp->cb_buf);
				goto out;
			}
		}

		if (iodone || other_cbp != NULL)
			/*
			 * If I/O on other component successfully done
			 * or the I/O is still in progress, no need
			 * to tell an error to upper layer.
			 */
			;
		else {
			bp->b_error = cbp->cb_buf.b_error ?
			    cbp->cb_buf.b_error : EIO;
		}

		/* XXX Update component config blocks. */

	} else {
		/*
		 * If other I/O is still in progress, tell it that
		 * our I/O is successfully done.
		 */
		if (other_cbp != NULL)
			other_cbp->cb_flags |= CBUF_IODONE;
	}
	count = cbp->cb_buf.b_bcount;
	CBUF_PUT(cbp);

	if (other_cbp != NULL)
		goto out;

	/* If all done, "interrupt". */
	bp->b_resid -= count;
	if (bp->b_resid < 0)
		panic("ld_ataraid_iodone_raid0: count");
	if (bp->b_resid == 0)
		lddone(&sc->sc_ld, bp);

out:
	splx(s);
}

static int
ld_ataraid_dump(struct ld_softc *sc, void *data,
    int blkno, int blkcnt)
{

	return (EIO);
}

#if NBIO > 0
static int
ld_ataraid_bioctl(device_t self, u_long cmd, void *addr)
{
	struct ld_ataraid_softc *sc = device_private(self);
	int error = 0;

	switch (cmd) {
	case BIOCINQ:
		error = ld_ataraid_bioinq(sc, (struct bioc_inq *)addr);
		break;
	case BIOCVOL:
		error = ld_ataraid_biovol(sc, (struct bioc_vol *)addr);
		break;
	case BIOCDISK:
		error = ld_ataraid_biodisk(sc, (struct bioc_disk *)addr);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return error;
}

static int
ld_ataraid_bioinq(struct ld_ataraid_softc *sc, struct bioc_inq *bi)
{
	struct ataraid_array_info *aai = sc->sc_aai;

	/* there's always one volume per ld device */
	bi->bi_novol = 1;
	bi->bi_nodisk = aai->aai_ndisks;

	return 0;
}

static int
ld_ataraid_biovol(struct ld_ataraid_softc *sc, struct bioc_vol *bv)
{
	struct ataraid_array_info *aai = sc->sc_aai;
	struct ld_softc *ld = &sc->sc_ld;
#define	to_kibytes(ld,s)	(ld->sc_secsize*(s)/1024)

	/* Fill in data for _this_ volume */
	bv->bv_percent = -1;
	bv->bv_seconds = 0;

	switch (aai->aai_status) {
	case AAI_S_READY:
		bv->bv_status = BIOC_SVONLINE;
		break;
	case AAI_S_DEGRADED:
		bv->bv_status = BIOC_SVDEGRADED;
		break;
	}

	bv->bv_size = ld->sc_secsize * ld->sc_secperunit;

	switch (aai->aai_level) {
	case AAI_L_SPAN:
	case AAI_L_RAID0:
		bv->bv_stripe_size = to_kibytes(ld, aai->aai_interleave);
		bv->bv_level = 0;
		break;
	case AAI_L_RAID1:
		bv->bv_stripe_size = 0;
		bv->bv_level = 1;
		break;
	case AAI_L_RAID5:
		bv->bv_stripe_size = to_kibytes(ld, aai->aai_interleave);
		bv->bv_level = 5;
		break;
	}

	bv->bv_nodisk = aai->aai_ndisks;
	strlcpy(bv->bv_dev, device_xname(ld->sc_dv), sizeof(bv->bv_dev));
	if (aai->aai_name[0] != '\0')
		strlcpy(bv->bv_vendor, aai->aai_name, sizeof(bv->bv_vendor));

	return 0;
}

static int
ld_ataraid_biodisk(struct ld_ataraid_softc *sc, struct bioc_disk *bd)
{
	struct ataraid_array_info *aai = sc->sc_aai;
	struct ataraid_disk_info *adi;
	struct ld_softc *ld = &sc->sc_ld;
	struct atabus_softc *atabus;
	struct wd_softc *wd;
	char model[81], serial[41], rev[17];

	/* sanity check */
	if (bd->bd_diskid > aai->aai_ndisks)
		return EINVAL;

	adi = &aai->aai_disks[bd->bd_diskid];
	atabus = device_private(device_parent(adi->adi_dev));
	wd = device_private(adi->adi_dev);

	/* fill in data for _this_ disk */
	switch (adi->adi_status) {
	case ADI_S_ONLINE | ADI_S_ASSIGNED:
		bd->bd_status = BIOC_SDONLINE;
		break;
	case ADI_S_SPARE:
		bd->bd_status = BIOC_SDHOTSPARE;
		break;
	default:
		bd->bd_status = BIOC_SDOFFLINE;
		break;
	}

	bd->bd_channel = 0;
	bd->bd_target = atabus->sc_chan->ch_channel;
	bd->bd_lun = 0;
	bd->bd_size = (wd->sc_capacity * ld->sc_secsize) - aai->aai_reserved;

	strlcpy(bd->bd_procdev, device_xname(adi->adi_dev),
	    sizeof(bd->bd_procdev));

	scsipi_strvis(serial, sizeof(serial), wd->sc_params.atap_serial,
	    sizeof(wd->sc_params.atap_serial));
	scsipi_strvis(model, sizeof(model), wd->sc_params.atap_model,
	    sizeof(wd->sc_params.atap_model));
	scsipi_strvis(rev, sizeof(rev), wd->sc_params.atap_revision,
	    sizeof(wd->sc_params.atap_revision));

	snprintf(bd->bd_vendor, sizeof(bd->bd_vendor), "%s %s", model, rev);
	strlcpy(bd->bd_serial, serial, sizeof(bd->bd_serial));

	return 0;
}
#endif /* NBIO > 0 */
