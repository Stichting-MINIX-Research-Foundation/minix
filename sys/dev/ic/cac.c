/*	$NetBSD: cac.c,v 1.55 2015/03/12 15:33:10 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2006, 2007 The NetBSD Foundation, Inc.
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
 * Driver for Compaq array controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cac.c,v 1.55 2015/03/12 15:33:10 christos Exp $");

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/pool.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#include <dev/ic/cacreg.h>
#include <dev/ic/cacvar.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif /* NBIO > 0 */

#include "locators.h"

static struct	cac_ccb *cac_ccb_alloc(struct cac_softc *, int);
static void	cac_ccb_done(struct cac_softc *, struct cac_ccb *);
static void	cac_ccb_free(struct cac_softc *, struct cac_ccb *);
static int	cac_ccb_poll(struct cac_softc *, struct cac_ccb *, int);
static int	cac_ccb_start(struct cac_softc *, struct cac_ccb *);
static int	cac_print(void *, const char *);
static void	cac_shutdown(void *);

static struct	cac_ccb *cac_l0_completed(struct cac_softc *);
static int	cac_l0_fifo_full(struct cac_softc *);
static void	cac_l0_intr_enable(struct cac_softc *, int);
static int	cac_l0_intr_pending(struct cac_softc *);
static void	cac_l0_submit(struct cac_softc *, struct cac_ccb *);

static void	*cac_sdh;	/* shutdown hook */

#if NBIO > 0
int		cac_ioctl(device_t, u_long, void *);
int		cac_ioctl_vol(struct cac_softc *, struct bioc_vol *);
int		cac_create_sensors(struct cac_softc *);
void		cac_sensor_refresh(struct sysmon_envsys *, envsys_data_t *);
#endif /* NBIO > 0 */

const struct cac_linkage cac_l0 = {
	cac_l0_completed,
	cac_l0_fifo_full,
	cac_l0_intr_enable,
	cac_l0_intr_pending,
	cac_l0_submit
};

/*
 * Initialise our interface to the controller.
 */
int
cac_init(struct cac_softc *sc, const char *intrstr, int startfw)
{
	struct cac_controller_info cinfo;
	struct cac_attach_args caca;
	int error, rseg, size, i;
	bus_dma_segment_t seg;
	struct cac_ccb *ccb;
	int locs[CACCF_NLOCS];
	char firm[8];

	if (intrstr != NULL)
		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n",
		    intrstr);

	SIMPLEQ_INIT(&sc->sc_ccb_free);
	SIMPLEQ_INIT(&sc->sc_ccb_queue);
	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_ccb_cv, "cacccb");

        size = sizeof(struct cac_ccb) * CAC_MAX_CCBS;

	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate CCBs, error = %d\n",
		    error);
		return (-1);
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, size,
	    (void **)&sc->sc_ccbs,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map CCBs, error = %d\n",
		    error);
		return (-1);
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create CCB DMA map, error = %d\n",
		    error);
		return (-1);
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, sc->sc_ccbs,
	    size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load CCB DMA map, error = %d\n",
		    error);
		return (-1);
	}

	sc->sc_ccbs_paddr = sc->sc_dmamap->dm_segs[0].ds_addr;
	memset(sc->sc_ccbs, 0, size);
	ccb = (struct cac_ccb *)sc->sc_ccbs;

	for (i = 0; i < CAC_MAX_CCBS; i++, ccb++) {
		/* Create the DMA map for this CCB's data */
		error = bus_dmamap_create(sc->sc_dmat, CAC_MAX_XFER,
		    CAC_SG_SIZE, CAC_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap_xfer);

		if (error) {
			aprint_error_dev(sc->sc_dev, "can't create ccb dmamap (%d)\n",
			    error);
			break;
		}

		ccb->ccb_flags = 0;
		ccb->ccb_paddr = sc->sc_ccbs_paddr + i * sizeof(struct cac_ccb);
		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_free, ccb, ccb_chain);
	}

	/* Start firmware background tasks, if needed. */
	if (startfw) {
		if (cac_cmd(sc, CAC_CMD_START_FIRMWARE, &cinfo, sizeof(cinfo),
		    0, 0, CAC_CCB_DATA_IN, NULL)) {
			aprint_error_dev(sc->sc_dev, "CAC_CMD_START_FIRMWARE failed\n");
			return (-1);
		}
	}

	if (cac_cmd(sc, CAC_CMD_GET_CTRL_INFO, &cinfo, sizeof(cinfo), 0, 0,
	    CAC_CCB_DATA_IN, NULL)) {
		aprint_error_dev(sc->sc_dev, "CAC_CMD_GET_CTRL_INFO failed\n");
		return (-1);
	}

	strlcpy(firm, cinfo.firm_rev, 4+1);
	printf("%s: %d channels, firmware <%s>\n", device_xname(sc->sc_dev),
	    cinfo.scsi_chips, firm);

	sc->sc_nunits = cinfo.num_drvs;
	for (i = 0; i < cinfo.num_drvs; i++) {
		caca.caca_unit = i;

		locs[CACCF_UNIT] = i;

		config_found_sm_loc(sc->sc_dev, "cac", locs, &caca,
		    cac_print, config_stdsubmatch);
	}

	/* Set our `shutdownhook' before we start any device activity. */
	if (cac_sdh == NULL)
		cac_sdh = shutdownhook_establish(cac_shutdown, NULL);

	mutex_enter(&sc->sc_mutex);
	(*sc->sc_cl.cl_intr_enable)(sc, CAC_INTR_ENABLE);
	mutex_exit(&sc->sc_mutex);

#if NBIO > 0
	if (bio_register(sc->sc_dev, cac_ioctl) != 0)
		aprint_error_dev(sc->sc_dev, "controller registration failed");
	else
		sc->sc_ioctl = cac_ioctl;
	if (cac_create_sensors(sc) != 0)
		aprint_error_dev(sc->sc_dev, "unable to create sensors\n");
#endif

	return (0);
}

/*
 * Shut down all `cac' controllers.
 */
static void
cac_shutdown(void *cookie)
{
	extern struct cfdriver cac_cd;
	struct cac_softc *sc;
	u_int8_t tbuf[512];
	int i;

	for (i = 0; i < cac_cd.cd_ndevs; i++) {
		if ((sc = device_lookup_private(&cac_cd, i)) == NULL)
			continue;
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = 1;
		cac_cmd(sc, CAC_CMD_FLUSH_CACHE, tbuf, sizeof(tbuf), 0, 0,
		    CAC_CCB_DATA_OUT, NULL);
	}
}

/*
 * Print autoconfiguration message for a sub-device.
 */
static int
cac_print(void *aux, const char *pnp)
{
	struct cac_attach_args *caca;

	caca = (struct cac_attach_args *)aux;

	if (pnp != NULL)
		aprint_normal("block device at %s", pnp);
	aprint_normal(" unit %d", caca->caca_unit);
	return (UNCONF);
}

/*
 * Handle an interrupt from the controller: process finished CCBs and
 * dequeue any waiting CCBs.
 */
int
cac_intr(void *cookie)
{
	struct cac_softc *sc;
	struct cac_ccb *ccb;
	int rv;

	sc = cookie;

	mutex_enter(&sc->sc_mutex);

	if ((*sc->sc_cl.cl_intr_pending)(sc)) {
		while ((ccb = (*sc->sc_cl.cl_completed)(sc)) != NULL) {
			cac_ccb_done(sc, ccb);
			cac_ccb_start(sc, NULL);
		}
		rv = 1;
	} else
		rv = 0;

	mutex_exit(&sc->sc_mutex);

	return (rv);
}

/*
 * Execute a [polled] command.
 */
int
cac_cmd(struct cac_softc *sc, int command, void *data, int datasize,
	int drive, int blkno, int flags, struct cac_context *context)
{
	struct cac_ccb *ccb;
	struct cac_sgb *sgb;
	int i, rv, size, nsegs;

	size = 0;

	if ((ccb = cac_ccb_alloc(sc, 1)) == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to alloc CCB");
		return (EAGAIN);
	}

	if ((flags & (CAC_CCB_DATA_IN | CAC_CCB_DATA_OUT)) != 0) {
		bus_dmamap_load(sc->sc_dmat, ccb->ccb_dmamap_xfer,
		    (void *)data, datasize, NULL, BUS_DMA_NOWAIT |
		    BUS_DMA_STREAMING | ((flags & CAC_CCB_DATA_IN) ?
		    BUS_DMA_READ : BUS_DMA_WRITE));

		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap_xfer, 0, datasize,
		    (flags & CAC_CCB_DATA_IN) != 0 ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

		sgb = ccb->ccb_seg;
		nsegs = min(ccb->ccb_dmamap_xfer->dm_nsegs, CAC_SG_SIZE);

		for (i = 0; i < nsegs; i++, sgb++) {
			size += ccb->ccb_dmamap_xfer->dm_segs[i].ds_len;
			sgb->length =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_len);
			sgb->addr =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_addr);
		}
	} else {
		size = datasize;
		nsegs = 0;
	}

	ccb->ccb_hdr.drive = drive;
	ccb->ccb_hdr.priority = 0;
	ccb->ccb_hdr.size = htole16((sizeof(struct cac_req) +
	    sizeof(struct cac_sgb) * CAC_SG_SIZE) >> 2);

	ccb->ccb_req.next = 0;
	ccb->ccb_req.error = 0;
	ccb->ccb_req.reserved = 0;
	ccb->ccb_req.bcount = htole16(howmany(size, DEV_BSIZE));
	ccb->ccb_req.command = command;
	ccb->ccb_req.sgcount = nsegs;
	ccb->ccb_req.blkno = htole32(blkno);

	ccb->ccb_flags = flags;
	ccb->ccb_datasize = size;

	mutex_enter(&sc->sc_mutex);

	if (context == NULL) {
		memset(&ccb->ccb_context, 0, sizeof(struct cac_context));

		/* Synchronous commands musn't wait. */
		if ((*sc->sc_cl.cl_fifo_full)(sc)) {
			cac_ccb_free(sc, ccb);
			rv = EAGAIN;
		} else {
#ifdef DIAGNOSTIC
			ccb->ccb_flags |= CAC_CCB_ACTIVE;
#endif
			(*sc->sc_cl.cl_submit)(sc, ccb);
			rv = cac_ccb_poll(sc, ccb, 2000);
			cac_ccb_free(sc, ccb);
		}
	} else {
		memcpy(&ccb->ccb_context, context, sizeof(struct cac_context));
		(void)cac_ccb_start(sc, ccb);
		rv = 0;
	}

	mutex_exit(&sc->sc_mutex);
	return (rv);
}

/*
 * Wait for the specified CCB to complete.
 */
static int
cac_ccb_poll(struct cac_softc *sc, struct cac_ccb *wantccb, int timo)
{
	struct cac_ccb *ccb;

	KASSERT(mutex_owned(&sc->sc_mutex));

	timo *= 1000;

	do {
		for (; timo != 0; timo--) {
			ccb = (*sc->sc_cl.cl_completed)(sc);
			if (ccb != NULL)
				break;
			DELAY(1);
		}

		if (timo == 0) {
			printf("%s: timeout\n", device_xname(sc->sc_dev));
			return (EBUSY);
		}
		cac_ccb_done(sc, ccb);
	} while (ccb != wantccb);

	return (0);
}

/*
 * Enqueue the specified command (if any) and attempt to start all enqueued
 * commands.
 */
static int
cac_ccb_start(struct cac_softc *sc, struct cac_ccb *ccb)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	if (ccb != NULL)
		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_queue, ccb, ccb_chain);

	while ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_queue)) != NULL) {
		if ((*sc->sc_cl.cl_fifo_full)(sc))
			return (EAGAIN);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_queue, ccb_chain);
#ifdef DIAGNOSTIC
		ccb->ccb_flags |= CAC_CCB_ACTIVE;
#endif
		(*sc->sc_cl.cl_submit)(sc, ccb);
	}

	return (0);
}

/*
 * Process a finished CCB.
 */
static void
cac_ccb_done(struct cac_softc *sc, struct cac_ccb *ccb)
{
	device_t dv;
	void *context;
	int error;

	error = 0;

	KASSERT(mutex_owned(&sc->sc_mutex));

#ifdef DIAGNOSTIC
	if ((ccb->ccb_flags & CAC_CCB_ACTIVE) == 0)
		panic("cac_ccb_done: CCB not active");
	ccb->ccb_flags &= ~CAC_CCB_ACTIVE;
#endif

	if ((ccb->ccb_flags & (CAC_CCB_DATA_IN | CAC_CCB_DATA_OUT)) != 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap_xfer, 0,
		    ccb->ccb_datasize, ccb->ccb_flags & CAC_CCB_DATA_IN ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap_xfer);
	}

	error = ccb->ccb_req.error;
	if (ccb->ccb_context.cc_handler != NULL) {
		dv = ccb->ccb_context.cc_dv;
		context = ccb->ccb_context.cc_context;
		cac_ccb_free(sc, ccb);
		(*ccb->ccb_context.cc_handler)(dv, context, error);
	} else {
		if ((error & CAC_RET_SOFT_ERROR) != 0)
			aprint_error_dev(sc->sc_dev, "soft error; array may be degraded\n");
		if ((error & CAC_RET_HARD_ERROR) != 0)
			aprint_error_dev(sc->sc_dev, "hard error\n");
		if ((error & CAC_RET_CMD_REJECTED) != 0) {
			error = 1;
			aprint_error_dev(sc->sc_dev, "invalid request\n");
		}
	}
}

/*
 * Allocate a CCB.
 */
static struct cac_ccb *
cac_ccb_alloc(struct cac_softc *sc, int nosleep)
{
	struct cac_ccb *ccb;

	mutex_enter(&sc->sc_mutex);

	for (;;) {
		if ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_free)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_free, ccb_chain);
			break;
		}
		if (nosleep) {
			ccb = NULL;
			break;
		}
		cv_wait(&sc->sc_ccb_cv, &sc->sc_mutex);
	}

	mutex_exit(&sc->sc_mutex);
	return (ccb);
}

/*
 * Put a CCB onto the freelist.
 */
static void
cac_ccb_free(struct cac_softc *sc, struct cac_ccb *ccb)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	ccb->ccb_flags = 0;
	if (SIMPLEQ_EMPTY(&sc->sc_ccb_free))
		cv_signal(&sc->sc_ccb_cv);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_free, ccb, ccb_chain);
}

/*
 * Board specific linkage shared between multiple bus types.
 */

static int
cac_l0_fifo_full(struct cac_softc *sc)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	return (cac_inl(sc, CAC_REG_CMD_FIFO) == 0);
}

static void
cac_l0_submit(struct cac_softc *sc, struct cac_ccb *ccb)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    (char *)ccb - (char *)sc->sc_ccbs,
	    sizeof(struct cac_ccb), BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	cac_outl(sc, CAC_REG_CMD_FIFO, ccb->ccb_paddr);
}

static struct cac_ccb *
cac_l0_completed(struct cac_softc *sc)
{
	struct cac_ccb *ccb;
	paddr_t off;

	KASSERT(mutex_owned(&sc->sc_mutex));

	if ((off = cac_inl(sc, CAC_REG_DONE_FIFO)) == 0)
		return (NULL);

	if ((off & 3) != 0)
		aprint_error_dev(sc->sc_dev, "failed command list returned: %lx\n",
		    (long)off);

	off = (off & ~3) - sc->sc_ccbs_paddr;
	ccb = (struct cac_ccb *)((char *)sc->sc_ccbs + off);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, off, sizeof(struct cac_ccb),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	if ((off & 3) != 0 && ccb->ccb_req.error == 0)
		ccb->ccb_req.error = CAC_RET_CMD_REJECTED;

	return (ccb);
}

static int
cac_l0_intr_pending(struct cac_softc *sc)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	return (cac_inl(sc, CAC_REG_INTR_PENDING) & CAC_INTR_ENABLE);
}

static void
cac_l0_intr_enable(struct cac_softc *sc, int state)
{

	KASSERT(mutex_owned(&sc->sc_mutex));

	cac_outl(sc, CAC_REG_INTR_MASK,
	    state ? CAC_INTR_ENABLE : CAC_INTR_DISABLE);
}

#if NBIO > 0
const int cac_level[] = { 0, 4, 1, 5, 51, 7 };
const int cac_stat[] = { BIOC_SVONLINE, BIOC_SVOFFLINE, BIOC_SVOFFLINE,
    BIOC_SVDEGRADED, BIOC_SVREBUILD, BIOC_SVREBUILD, BIOC_SVDEGRADED,
    BIOC_SVDEGRADED, BIOC_SVINVALID, BIOC_SVINVALID, BIOC_SVBUILDING,
    BIOC_SVOFFLINE, BIOC_SVBUILDING };

int
cac_ioctl(device_t dev, u_long cmd, void *addr)
{
	struct cac_softc *sc = device_private(dev);
	struct bioc_inq *bi;
	struct bioc_disk *bd;
	cac_lock_t lock;
	int error = 0;

	lock = CAC_LOCK(sc);
	switch (cmd) {
	case BIOCINQ:
		bi = (struct bioc_inq *)addr;
		strlcpy(bi->bi_dev, device_xname(sc->sc_dev), sizeof(bi->bi_dev));
		bi->bi_novol = sc->sc_nunits;
		bi->bi_nodisk = 0;
		break;

	case BIOCVOL:
		error = cac_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
	case BIOCDISK_NOVOL:
		bd = (struct bioc_disk *)addr;
		if (bd->bd_volid > sc->sc_nunits) {
			error = EINVAL;
			break;
		}
		/* No disk information yet */
		break;

	case BIOCBLINK:
	case BIOCALARM:
	case BIOCSETSTATE:
	default:
		error = EINVAL;
	}
	CAC_UNLOCK(sc, lock);

	return (error);
}

int
cac_ioctl_vol(struct cac_softc *sc, struct bioc_vol *bv)
{
	struct cac_drive_info dinfo;
	struct cac_drive_status dstatus;
	u_int32_t blks;

	if (bv->bv_volid > sc->sc_nunits) {
		return EINVAL;
	}
	if (cac_cmd(sc, CAC_CMD_GET_LOG_DRV_INFO, &dinfo, sizeof(dinfo),
	    bv->bv_volid, 0, CAC_CCB_DATA_IN, NULL)) {
		return EIO;
	}
	if (cac_cmd(sc, CAC_CMD_SENSE_DRV_STATUS, &dstatus, sizeof(dstatus),
	    bv->bv_volid, 0, CAC_CCB_DATA_IN, NULL)) {
		return EIO;
	}
	blks = CAC_GET2(dinfo.ncylinders) * CAC_GET1(dinfo.nheads) *
	    CAC_GET1(dinfo.nsectors);
	bv->bv_size = (off_t)blks * CAC_GET2(dinfo.secsize);
	bv->bv_level = cac_level[CAC_GET1(dinfo.mirror)];	/*XXX limit check */
	bv->bv_nodisk = 0;		/* XXX */
	bv->bv_status = 0;		/* XXX */
	bv->bv_percent = -1;
	bv->bv_seconds = 0;
	if (dstatus.stat < sizeof(cac_stat)/sizeof(cac_stat[0]))
		bv->bv_status = cac_stat[dstatus.stat];
	if (bv->bv_status == BIOC_SVREBUILD ||
	    bv->bv_status == BIOC_SVBUILDING)
		bv->bv_percent = ((blks - CAC_GET4(dstatus.prog)) * 1000ULL) /
		    blks;
	return 0;
}

int
cac_create_sensors(struct cac_softc *sc)
{
	int			i;
	int nsensors = sc->sc_nunits;

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sensor = malloc(sizeof(envsys_data_t) * nsensors,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_sensor == NULL) {
		aprint_error_dev(sc->sc_dev, "can't allocate envsys_data_t\n");
		return(ENOMEM);
	}

	for (i = 0; i < nsensors; i++) {
		sc->sc_sensor[i].units = ENVSYS_DRIVE;
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].value_cur = ENVSYS_DRIVE_EMPTY;
		/* Enable monitoring for drive state changes */
		sc->sc_sensor[i].flags |= ENVSYS_FMONSTCHANGED;
		/* logical drives */
		snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc), "%s:%d",
		    device_xname(sc->sc_dev), i);
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
		    &sc->sc_sensor[i]))
			goto out;
	}
	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = cac_sensor_refresh;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(sc->sc_dev, "unable to register with sysmon\n");
		return(1);
	}
	return (0);

out:
	free(sc->sc_sensor, M_DEVBUF);
	sysmon_envsys_destroy(sc->sc_sme);
	return EINVAL;
}

void
cac_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct cac_softc	*sc = sme->sme_cookie;
	struct bioc_vol		bv;
	int s;

	if (edata->sensor >= sc->sc_nunits)
		return;

	memset(&bv, 0, sizeof(bv));
	bv.bv_volid = edata->sensor;
	s = splbio();
	if (cac_ioctl_vol(sc, &bv))
		bv.bv_status = BIOC_SVINVALID;
	splx(s);

	bio_vol_to_envsys(edata, &bv);
}
#endif /* NBIO > 0 */
