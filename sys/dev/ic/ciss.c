/*	$NetBSD: ciss.c,v 1.35 2015/03/12 19:56:51 christos Exp $	*/
/*	$OpenBSD: ciss.c,v 1.68 2013/05/30 16:15:02 deraadt Exp $	*/

/*
 * Copyright (c) 2005,2006 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ciss.c,v 1.35 2015/03/12 19:56:51 christos Exp $");

#include "bio.h"

/* #define CISS_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsipi_all.h>

#include <dev/ic/cissreg.h>
#include <dev/ic/cissvar.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif /* NBIO > 0 */

#ifdef CISS_DEBUG
#define	CISS_DPRINTF(m,a)	if (ciss_debug & (m)) printf a
#define	CISS_D_CMD	0x0001
#define	CISS_D_INTR	0x0002
#define	CISS_D_MISC	0x0004
#define	CISS_D_DMA	0x0008
#define	CISS_D_IOCTL	0x0010
#define	CISS_D_ERR	0x0020
int ciss_debug = 0
	| CISS_D_CMD
	| CISS_D_INTR
	| CISS_D_MISC
	| CISS_D_DMA
	| CISS_D_IOCTL
	| CISS_D_ERR
	;
#else
#define	CISS_DPRINTF(m,a)	/* m, a */
#endif

static void	ciss_scsi_cmd(struct scsipi_channel *chan,
			scsipi_adapter_req_t req, void *arg);
static int	ciss_scsi_ioctl(struct scsipi_channel *chan, u_long cmd,
	    void *addr, int flag, struct proc *p);
static void	cissminphys(struct buf *bp);

#if 0
static void	ciss_scsi_raw_cmd(struct scsipi_channel *chan,
			scsipi_adapter_req_t req, void *arg);
#endif

static int	ciss_sync(struct ciss_softc *sc);
static void	ciss_heartbeat(void *v);
static void	ciss_shutdown(void *v);

static struct ciss_ccb *ciss_get_ccb(struct ciss_softc *sc);
static void	ciss_put_ccb(struct ciss_ccb *ccb);
static int	ciss_cmd(struct ciss_ccb *ccb, int flags, int wait);
static int	ciss_done(struct ciss_ccb *ccb);
static int	ciss_error(struct ciss_ccb *ccb);
struct ciss_ld *ciss_pdscan(struct ciss_softc *sc, int ld);
static int	ciss_inq(struct ciss_softc *sc, struct ciss_inquiry *inq);
int	ciss_ldid(struct ciss_softc *, int, struct ciss_ldid *);
int	ciss_ldstat(struct ciss_softc *, int, struct ciss_ldstat *);
static int	ciss_ldmap(struct ciss_softc *sc);
int	ciss_pdid(struct ciss_softc *, u_int8_t, struct ciss_pdid *, int);

#if NBIO > 0
int		ciss_ioctl(device_t, u_long, void *);
int		ciss_ioctl_vol(struct ciss_softc *, struct bioc_vol *);
int		ciss_blink(struct ciss_softc *, int, int, int, struct ciss_blink *);
int		ciss_create_sensors(struct ciss_softc *);
void		ciss_sensor_refresh(struct sysmon_envsys *, envsys_data_t *);
#endif /* NBIO > 0 */

static struct ciss_ccb *
ciss_get_ccb(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;

	mutex_enter(&sc->sc_mutex);
	if ((ccb = TAILQ_LAST(&sc->sc_free_ccb, ciss_queue_head))) {
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ccb_link);
		ccb->ccb_state = CISS_CCB_READY;
	}
	mutex_exit(&sc->sc_mutex);
	return ccb;
}

static void
ciss_put_ccb(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;

	ccb->ccb_state = CISS_CCB_FREE;
	mutex_enter(&sc->sc_mutex);
	TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
	mutex_exit(&sc->sc_mutex);
}

int
ciss_attach(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_inquiry *inq;
	bus_dma_segment_t seg[1];
	int error, i, total, rseg, maxfer;
	paddr_t pa;

	bus_space_read_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);

	if (sc->cfg.signature != CISS_SIGNATURE) {
		printf(": bad sign 0x%08x\n", sc->cfg.signature);
		return -1;
	}

	if (!(sc->cfg.methods & CISS_METH_SIMPL)) {
		printf(": not simple 0x%08x\n", sc->cfg.methods);
		return -1;
	}

	sc->cfg.rmethod = CISS_METH_SIMPL;
	sc->cfg.paddr_lim = 0;			/* 32bit addrs */
	sc->cfg.int_delay = 0;			/* disable coalescing */
	sc->cfg.int_count = 0;
	strlcpy(sc->cfg.hostname, "HUMPPA", sizeof(sc->cfg.hostname));
	sc->cfg.driverf |= CISS_DRV_PRF;	/* enable prefetch */
	if (!sc->cfg.maxsg)
		sc->cfg.maxsg = MAXPHYS / PAGE_SIZE + 1;

	bus_space_write_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);
	bus_space_barrier(sc->sc_iot, sc->cfg_ioh, sc->cfgoff, sizeof(sc->cfg),
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IDB, CISS_IDB_CFG);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, CISS_IDB, 4,
	    BUS_SPACE_BARRIER_WRITE);
	for (i = 1000; i--; DELAY(1000)) {
		/* XXX maybe IDB is really 64bit? - hp dl380 needs this */
		(void)bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB + 4);
		if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB) & CISS_IDB_CFG))
			break;
		bus_space_barrier(sc->sc_iot, sc->sc_ioh, CISS_IDB, 4,
		    BUS_SPACE_BARRIER_READ);
	}

	if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB) & CISS_IDB_CFG) {
		printf(": cannot set config\n");
		return -1;
	}

	bus_space_read_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);

	if (!(sc->cfg.amethod & CISS_METH_SIMPL)) {
		printf(": cannot simplify 0x%08x\n", sc->cfg.amethod);
		return -1;
	}

	/* i'm ready for you and i hope you're ready for me */
	for (i = 30000; i--; DELAY(1000)) {
		if (bus_space_read_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
		    offsetof(struct ciss_config, amethod)) & CISS_METH_READY)
			break;
		bus_space_barrier(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
		    offsetof(struct ciss_config, amethod), 4,
		    BUS_SPACE_BARRIER_READ);
	}

	if (!(bus_space_read_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
	    offsetof(struct ciss_config, amethod)) & CISS_METH_READY)) {
		printf(": she never came ready for me 0x%08x\n",
		    sc->cfg.amethod);
		return -1;
	}

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&sc->sc_mutex_scratch, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_condvar, "ciss_cmd");
	sc->maxcmd = sc->cfg.maxcmd;
	sc->maxsg = sc->cfg.maxsg;
	if (sc->maxsg > MAXPHYS / PAGE_SIZE + 1)
		sc->maxsg = MAXPHYS / PAGE_SIZE + 1;
	i = sizeof(struct ciss_ccb) +
	    sizeof(ccb->ccb_cmd.sgl[0]) * (sc->maxsg - 1);
	for (sc->ccblen = 0x10; sc->ccblen < i; sc->ccblen <<= 1);

	total = sc->ccblen * sc->maxcmd;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, total, PAGE_SIZE, 0,
	    sc->cmdseg, 1, &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate CCBs (%d)\n", error);
		return -1;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, sc->cmdseg, rseg, total,
	    (void **)&sc->ccbs, BUS_DMA_NOWAIT))) {
		printf(": cannot map CCBs (%d)\n", error);
		return -1;
	}
	memset(sc->ccbs, 0, total);

	if ((error = bus_dmamap_create(sc->sc_dmat, total, 1,
	    total, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->cmdmap))) {
		printf(": cannot create CCBs dmamap (%d)\n", error);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		return -1;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->cmdmap, sc->ccbs, total,
	    NULL, BUS_DMA_NOWAIT))) {
		printf(": cannot load CCBs dmamap (%d)\n", error);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_ccbdone);
	TAILQ_INIT(&sc->sc_free_ccb);

	maxfer = sc->maxsg * PAGE_SIZE;
	for (i = 0; total > 0 && i < sc->maxcmd; i++, total -= sc->ccblen) {
		ccb = (struct ciss_ccb *) ((char *)sc->ccbs + i * sc->ccblen);
		cmd = &ccb->ccb_cmd;
		pa = sc->cmdseg[0].ds_addr + i * sc->ccblen;

		ccb->ccb_sc = sc;
		ccb->ccb_cmdpa = pa + offsetof(struct ciss_ccb, ccb_cmd);
		ccb->ccb_state = CISS_CCB_FREE;

		cmd->id = htole32(i << 2);
		cmd->id_hi = htole32(0);
		cmd->sgin = sc->maxsg;
		cmd->sglen = htole16((u_int16_t)cmd->sgin);
		cmd->err_len = htole32(sizeof(ccb->ccb_err));
		pa += offsetof(struct ciss_ccb, ccb_err);
		cmd->err_pa = htole64((u_int64_t)pa);

		if ((error = bus_dmamap_create(sc->sc_dmat, maxfer, sc->maxsg,
		    maxfer, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap)))
			break;

		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
	}

	if (i < sc->maxcmd) {
		printf(": cannot create ccb#%d dmamap (%d)\n", i, error);
		if (i == 0) {
			/* TODO leaking cmd's dmamaps and shitz */
			bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
			bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
			return -1;
		}
	}

	if ((error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    seg, 1, &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate scratch buffer (%d)\n", error);
		return -1;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, seg, rseg, PAGE_SIZE,
	    (void **)&sc->scratch, BUS_DMA_NOWAIT))) {
		printf(": cannot map scratch buffer (%d)\n", error);
		return -1;
	}
	memset(sc->scratch, 0, PAGE_SIZE);
	sc->sc_waitflag = XS_CTL_NOSLEEP;		/* can't sleep yet */

	mutex_enter(&sc->sc_mutex_scratch);		/* is this really needed? */
	inq = sc->scratch;
	if (ciss_inq(sc, inq)) {
		printf(": adapter inquiry failed\n");
		mutex_exit(&sc->sc_mutex_scratch);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	if (!(inq->flags & CISS_INQ_BIGMAP)) {
		printf(": big map is not supported, flags=0x%x\n",
		    inq->flags);
		mutex_exit(&sc->sc_mutex_scratch);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	sc->maxunits = inq->numld;
	sc->nbus = inq->nscsi_bus;
	sc->ndrives = inq->buswidth ? inq->buswidth : 256;
	printf(": %d LD%s, HW rev %d, FW %4.4s/%4.4s",
	    inq->numld, inq->numld == 1? "" : "s",
	    inq->hw_rev, inq->fw_running, inq->fw_stored);

	if (sc->cfg.methods & CISS_METH_FIFO64)
		printf(", 64bit fifo");
	else if (sc->cfg.methods & CISS_METH_FIFO64_RRO)
		printf(", 64bit fifo rro");
	printf("\n");

	mutex_exit(&sc->sc_mutex_scratch);

	callout_init(&sc->sc_hb, 0);
	callout_setfunc(&sc->sc_hb, ciss_heartbeat, sc);
	callout_schedule(&sc->sc_hb, hz * 3);

	/* map LDs */
	if (ciss_ldmap(sc)) {
		aprint_error_dev(sc->sc_dev, "adapter LD map failed\n");
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	if (!(sc->sc_lds = malloc(sc->maxunits * sizeof(*sc->sc_lds),
	    M_DEVBUF, M_NOWAIT))) {
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}
	memset(sc->sc_lds, 0, sc->maxunits * sizeof(*sc->sc_lds));

	sc->sc_flush = CISS_FLUSH_ENABLE;
	if (!(sc->sc_sh = shutdownhook_establish(ciss_shutdown, sc))) {
		printf(": unable to establish shutdown hook\n");
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = sc->maxunits;
	sc->sc_channel.chan_nluns = 1;	/* ciss doesn't really have SCSI luns */
	sc->sc_channel.chan_openings = sc->maxcmd;
#if NBIO > 0
	/* XXX Reserve some ccb's for sensor and bioctl. */
	if (sc->sc_channel.chan_openings > 2)
		sc->sc_channel.chan_openings -= 2;
#endif
	sc->sc_channel.chan_flags = 0;
	sc->sc_channel.chan_id = sc->maxunits;

	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_openings = sc->sc_channel.chan_openings;
	sc->sc_adapter.adapt_max_periph = min(sc->sc_adapter.adapt_openings, 256);
	sc->sc_adapter.adapt_request = ciss_scsi_cmd;
	sc->sc_adapter.adapt_minphys = cissminphys;
	sc->sc_adapter.adapt_ioctl = ciss_scsi_ioctl;
	sc->sc_adapter.adapt_nchannels = 1;
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);

#if 0
	sc->sc_link_raw.adapter_softc = sc;
	sc->sc_link.openings = sc->sc_channel.chan_openings;
	sc->sc_link_raw.adapter = &ciss_raw_switch;
	sc->sc_link_raw.adapter_target = sc->ndrives;
	sc->sc_link_raw.adapter_buswidth = sc->ndrives;
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
#endif

#if NBIO > 0
	/* now map all the physdevs into their lds */
	/* XXX currently we assign all of them into ld0 */
	for (i = 0; i < sc->maxunits && i < 1; i++)
		if (!(sc->sc_lds[i] = ciss_pdscan(sc, i))) {
			sc->sc_waitflag = 0;	/* we can sleep now */
			return 0;
		}

	if (bio_register(sc->sc_dev, ciss_ioctl) != 0)
		aprint_error_dev(sc->sc_dev, "controller registration failed");
	else
		sc->sc_ioctl = ciss_ioctl;
	if (ciss_create_sensors(sc) != 0)
		aprint_error_dev(sc->sc_dev, "unable to create sensors");
#endif
	sc->sc_waitflag = 0;			/* we can sleep now */

	return 0;
}

static void
ciss_shutdown(void *v)
{
	struct ciss_softc *sc = v;

	sc->sc_flush = CISS_FLUSH_DISABLE;
	/* timeout_del(&sc->sc_hb); */
	ciss_sync(sc);
}

static void
cissminphys(struct buf *bp)
{
#if 0	/* TODO */
#define	CISS_MAXFER	(PAGE_SIZE * (sc->maxsg + 1))
	if (bp->b_bcount > CISS_MAXFER)
		bp->b_bcount = CISS_MAXFER;
#endif
	minphys(bp);
}

static struct ciss_ccb *
ciss_poll1(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	uint32_t id;

	if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_ISR) & sc->iem)) {
		CISS_DPRINTF(CISS_D_CMD, ("N"));
		return NULL;
	}

	if (sc->cfg.methods & CISS_METH_FIFO64) {
		if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ64_HI) ==
		    0xffffffff) {
			CISS_DPRINTF(CISS_D_CMD, ("Q"));
			return NULL;
		}
		id = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ64_LO);
	} else if (sc->cfg.methods & CISS_METH_FIFO64_RRO) {
		id = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ64_LO);
		if (id == 0xffffffff) {
			CISS_DPRINTF(CISS_D_CMD, ("Q"));
			return NULL;
		}
		(void)bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ64_HI);
	} else {
		id = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ);
		if (id == 0xffffffff) {
			CISS_DPRINTF(CISS_D_CMD, ("Q"));
			return NULL;
		}
	}

	CISS_DPRINTF(CISS_D_CMD, ("got=0x%x ", id));
	ccb = (struct ciss_ccb *) ((char *)sc->ccbs + (id >> 2) * sc->ccblen);
	ccb->ccb_cmd.id = htole32(id);
	ccb->ccb_cmd.id_hi = htole32(0);
	return ccb;
}

static int
ciss_poll(struct ciss_softc *sc, struct ciss_ccb *ccb, int ms)
{
	struct ciss_ccb *ccb1;

	ms /= 10;

	while (ms-- > 0) {
		DELAY(10);
		ccb1 = ciss_poll1(sc);
		if (ccb1 == NULL)
			continue;
		ciss_done(ccb1);
		if (ccb1 == ccb)
			return 0;
	}

	return ETIMEDOUT;
}

static int
ciss_wait(struct ciss_softc *sc, struct ciss_ccb *ccb, int ms)
{
	int tohz, etick;

	tohz = mstohz(ms);
	if (tohz == 0)
		tohz = 1;
	etick = hardclock_ticks + tohz;

	for (;;) {
		ccb->ccb_state = CISS_CCB_POLL;
		CISS_DPRINTF(CISS_D_CMD, ("cv_timedwait(%d) ", tohz));
		mutex_enter(&sc->sc_mutex);
		if (cv_timedwait(&sc->sc_condvar, &sc->sc_mutex, tohz)
		    == EWOULDBLOCK) {
			mutex_exit(&sc->sc_mutex);
			return EWOULDBLOCK;
		}
		mutex_exit(&sc->sc_mutex);
		if (ccb->ccb_state == CISS_CCB_ONQ) {
			ciss_done(ccb);
			return 0;
		}
		tohz = etick - hardclock_ticks;
		if (tohz <= 0)
			return EWOULDBLOCK;
		CISS_DPRINTF(CISS_D_CMD, ("T"));
	}
}

/*
 * submit a command and optionally wait for completition.
 * wait arg abuses XS_CTL_POLL|XS_CTL_NOSLEEP flags to request
 * to wait (XS_CTL_POLL) and to allow tsleep() (!XS_CTL_NOSLEEP)
 * instead of busy loop waiting
 */
static int
ciss_cmd(struct ciss_ccb *ccb, int flags, int wait)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct ciss_cmd *cmd = &ccb->ccb_cmd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	u_int64_t addr;
	int i, error = 0;

	if (ccb->ccb_state != CISS_CCB_READY) {
		printf("%s: ccb %d not ready state=0x%x\n", device_xname(sc->sc_dev),
		    cmd->id, ccb->ccb_state);
		return (EINVAL);
	}

	if (ccb->ccb_data) {
		bus_dma_segment_t *sgd;

		if ((error = bus_dmamap_load(sc->sc_dmat, dmap, ccb->ccb_data,
		    ccb->ccb_len, NULL, flags))) {
			if (error == EFBIG)
				printf("more than %d dma segs\n", sc->maxsg);
			else
				printf("error %d loading dma map\n", error);
			ciss_put_ccb(ccb);
			return (error);
		}
		cmd->sgin = dmap->dm_nsegs;

		sgd = dmap->dm_segs;
		CISS_DPRINTF(CISS_D_DMA, ("data=%p/%zu<%#" PRIxPADDR "/%zu",
		    ccb->ccb_data, ccb->ccb_len, sgd->ds_addr, sgd->ds_len));

		for (i = 0; i < dmap->dm_nsegs; sgd++, i++) {
			cmd->sgl[i].addr_lo = htole32(sgd->ds_addr);
			cmd->sgl[i].addr_hi =
			    htole32((u_int64_t)sgd->ds_addr >> 32);
			cmd->sgl[i].len = htole32(sgd->ds_len);
			cmd->sgl[i].flags = htole32(0);
			if (i) {
				CISS_DPRINTF(CISS_D_DMA,
				    (",%#" PRIxPADDR "/%zu", sgd->ds_addr,
				    sgd->ds_len));
			}
		}

		CISS_DPRINTF(CISS_D_DMA, ("> "));

		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	} else
		cmd->sgin = 0;
	cmd->sglen = htole16((u_int16_t)cmd->sgin);
	memset(&ccb->ccb_err, 0, sizeof(ccb->ccb_err));

	bus_dmamap_sync(sc->sc_dmat, sc->cmdmap, 0, sc->cmdmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if ((wait & (XS_CTL_POLL|XS_CTL_NOSLEEP)) == (XS_CTL_POLL|XS_CTL_NOSLEEP))
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) | sc->iem);

	mutex_enter(&sc->sc_mutex);
	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ccb_link);
	mutex_exit(&sc->sc_mutex);
	ccb->ccb_state = CISS_CCB_ONQ;
	CISS_DPRINTF(CISS_D_CMD, ("submit=0x%x ", cmd->id));
	if (sc->cfg.methods & (CISS_METH_FIFO64|CISS_METH_FIFO64_RRO)) {
		/*
		 * Write the upper 32bits immediately before the lower
		 * 32bits and set bit 63 to indicate 64bit FIFO mode.
		 */
		addr = (u_int64_t)ccb->ccb_cmdpa;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_INQ64_HI,
		    (addr >> 32) | 0x80000000);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_INQ64_LO,
		    addr & 0x00000000ffffffffULL);
	} else
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_INQ,
		    ccb->ccb_cmdpa);

	if (wait & XS_CTL_POLL) {
		int ms;
		CISS_DPRINTF(CISS_D_CMD, ("waiting "));

		ms = ccb->ccb_xs ? ccb->ccb_xs->timeout : 60000;
		if (wait & XS_CTL_NOSLEEP)
			error = ciss_poll(sc, ccb, ms);
		else
			error = ciss_wait(sc, ccb, ms);

		/* if never got a chance to be done above... */
		if (ccb->ccb_state != CISS_CCB_FREE) {
			KASSERT(error);
			ccb->ccb_err.cmd_stat = CISS_ERR_TMO;
			error = ciss_done(ccb);
		}

		CISS_DPRINTF(CISS_D_CMD, ("done %d:%d",
		    ccb->ccb_err.cmd_stat, ccb->ccb_err.scsi_stat));
	}

	if ((wait & (XS_CTL_POLL|XS_CTL_NOSLEEP)) == (XS_CTL_POLL|XS_CTL_NOSLEEP))
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) & ~sc->iem);

	return (error);
}

static int
ciss_done(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct scsipi_xfer *xs = ccb->ccb_xs;
	struct ciss_cmd *cmd;
	int error = 0;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_done(%p) ", ccb));

	if (ccb->ccb_state != CISS_CCB_ONQ) {
		printf("%s: unqueued ccb %p ready, state=0x%x\n",
		    device_xname(sc->sc_dev), ccb, ccb->ccb_state);
		return 1;
	}

	ccb->ccb_state = CISS_CCB_READY;
	mutex_enter(&sc->sc_mutex);
	TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);
	mutex_exit(&sc->sc_mutex);

	if (ccb->ccb_cmd.id & CISS_CMD_ERR)
		error = ciss_error(ccb);

	cmd = &ccb->ccb_cmd;
	if (ccb->ccb_data) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, (cmd->flags & CISS_CDB_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
		ccb->ccb_xs = NULL;
		ccb->ccb_data = NULL;
	}

	ciss_put_ccb(ccb);

	if (xs) {
		xs->resid = 0;
		CISS_DPRINTF(CISS_D_CMD, ("scsipi_done(%p) ", xs));
		if (xs->cmd->opcode == INQUIRY) {
			struct scsipi_inquiry_data *inq;
			inq = (struct scsipi_inquiry_data *)xs->data;
			if ((inq->version & SID_ANSII) == 0 &&
			    (inq->flags3 & SID_CmdQue) != 0) {
				inq->version |= 2;
			}
		}
		scsipi_done(xs);
	}

	return error;
}

static int
ciss_error(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct ciss_error *err = &ccb->ccb_err;
	struct scsipi_xfer *xs = ccb->ccb_xs;
	int rv;

	switch ((rv = le16toh(err->cmd_stat))) {
	case CISS_ERR_OK:
		rv = 0;
		break;

	case CISS_ERR_INVCMD:
		if (xs == NULL ||
		    xs->cmd->opcode != SCSI_SYNCHRONIZE_CACHE_10)
			printf("%s: invalid cmd 0x%x: 0x%x is not valid @ 0x%x[%d]\n",
			    device_xname(sc->sc_dev), ccb->ccb_cmd.id,
			    err->err_info, err->err_type[3], err->err_type[2]);
		if (xs) {
			memset(&xs->sense, 0, sizeof(xs->sense));
			xs->sense.scsi_sense.response_code =
				SSD_RCODE_CURRENT | SSD_RCODE_VALID;
			xs->sense.scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
			xs->sense.scsi_sense.asc = 0x24; /* ill field */
			xs->sense.scsi_sense.ascq = 0x0;
			xs->error = XS_SENSE;
		}
		rv = EIO;
		break;

	case CISS_ERR_TMO:
		xs->error = XS_TIMEOUT;
		rv = ETIMEDOUT;
		break;

	case CISS_ERR_UNRUN:
		/* Underrun */
		xs->resid = le32toh(err->resid);
		CISS_DPRINTF(CISS_D_CMD, (" underrun resid=0x%x ",
					  xs->resid));
		rv = EIO;
		break;
	default:
		if (xs) {
			CISS_DPRINTF(CISS_D_CMD, ("scsi_stat=%x ", err->scsi_stat));
			switch (err->scsi_stat) {
			case SCSI_CHECK:
				xs->error = XS_SENSE;
				memcpy(&xs->sense, &err->sense[0],
				    sizeof(xs->sense));
				CISS_DPRINTF(CISS_D_CMD, (" sense=%02x %02x %02x %02x ",
					     err->sense[0], err->sense[1], err->sense[2], err->sense[3]));
				rv = EIO;
				break;

			case XS_BUSY:
				xs->error = XS_BUSY;
				rv = EBUSY;
				break;

			default:
				CISS_DPRINTF(CISS_D_ERR, ("%s: "
				    "cmd_stat=%x scsi_stat=0x%x resid=0x%x\n",
				    device_xname(sc->sc_dev), rv, err->scsi_stat,
				    le32toh(err->resid)));
				printf("ciss driver stuffup in %s:%d: %s()\n",
				       __FILE__, __LINE__, __func__);
				xs->error = XS_DRIVER_STUFFUP;
				rv = EIO;
				break;
			}
			xs->resid = le32toh(err->resid);
		} else
			rv = EIO;
	}
	ccb->ccb_cmd.id &= htole32(~3);

	return rv;
}

static int
ciss_inq(struct ciss_softc *sc, struct ciss_inquiry *inq)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = sizeof(*inq);
	ccb->ccb_data = inq;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(0);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_GET;
	cmd->cdb[6] = CISS_CMS_CTRL_CTRL;
	cmd->cdb[7] = sizeof(*inq) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*inq) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);
}

static int
ciss_ldmap(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_ldmap *lmap;
	int total, rv;

	mutex_enter(&sc->sc_mutex_scratch);
	lmap = sc->scratch;
	lmap->size = htobe32(sc->maxunits * sizeof(lmap->map));
	total = sizeof(*lmap) + (sc->maxunits - 1) * sizeof(lmap->map);

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = total;
	ccb->ccb_data = lmap;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = CISS_CMD_MODE_PERIPH;
	cmd->tgt2 = 0;
	cmd->cdblen = 12;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(30);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_LDMAP;
	cmd->cdb[8] = total >> 8;	/* biiiig endian */
	cmd->cdb[9] = total & 0xff;

	rv = ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);

	if (rv) {
		mutex_exit(&sc->sc_mutex_scratch);
		return rv;
	}

	CISS_DPRINTF(CISS_D_MISC, ("lmap %x:%x\n",
	    lmap->map[0].tgt, lmap->map[0].tgt2));

	mutex_exit(&sc->sc_mutex_scratch);
	return 0;
}

static int
ciss_sync(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_flush *flush;
	int rv;

	mutex_enter(&sc->sc_mutex_scratch);
	flush = sc->scratch;
	memset(flush, 0, sizeof(*flush));
	flush->flush = sc->sc_flush;

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = sizeof(*flush);
	ccb->ccb_data = flush;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = CISS_CMD_MODE_PERIPH;
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_OUT;
	cmd->tmo = 0;
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_SET;
	cmd->cdb[6] = CISS_CMS_CTRL_FLUSH;
	cmd->cdb[7] = sizeof(*flush) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*flush) & 0xff;

	rv = ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);
	mutex_exit(&sc->sc_mutex_scratch);

	return rv;
}

int
ciss_ldid(struct ciss_softc *sc, int target, struct ciss_ldid *id)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	ccb = ciss_get_ccb(sc);
	if (ccb == NULL)
		return ENOMEM;
	ccb->ccb_len = sizeof(*id);
	ccb->ccb_data = id;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(0);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_GET;
	cmd->cdb[1] = target;
	cmd->cdb[6] = CISS_CMS_CTRL_LDIDEXT;
	cmd->cdb[7] = sizeof(*id) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*id) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL | sc->sc_waitflag);
}

int
ciss_ldstat(struct ciss_softc *sc, int target, struct ciss_ldstat *stat)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	ccb = ciss_get_ccb(sc);
	if (ccb == NULL)
		return ENOMEM;
	ccb->ccb_len = sizeof(*stat);
	ccb->ccb_data = stat;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(0);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_GET;
	cmd->cdb[1] = target;
	cmd->cdb[6] = CISS_CMS_CTRL_LDSTAT;
	cmd->cdb[7] = sizeof(*stat) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*stat) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL | sc->sc_waitflag);
}

int
ciss_pdid(struct ciss_softc *sc, u_int8_t drv, struct ciss_pdid *id, int wait)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	ccb = ciss_get_ccb(sc);
	if (ccb == NULL)
		return ENOMEM;
	ccb->ccb_len = sizeof(*id);
	ccb->ccb_data = id;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(0);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_GET;
	cmd->cdb[2] = drv;
	cmd->cdb[6] = CISS_CMS_CTRL_PDID;
	cmd->cdb[7] = sizeof(*id) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*id) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, wait);
}


struct ciss_ld *
ciss_pdscan(struct ciss_softc *sc, int ld)
{
	struct ciss_pdid *pdid;
	struct ciss_ld *ldp;
	u_int8_t drv, buf[128];
	int i, j, k = 0;

	mutex_enter(&sc->sc_mutex_scratch);
	pdid = sc->scratch;
	if (sc->ndrives == 256) {
		for (i = 0; i < CISS_BIGBIT; i++)
			if (!ciss_pdid(sc, i, pdid, 
					XS_CTL_POLL|XS_CTL_NOSLEEP) &&
			    (pdid->present & CISS_PD_PRESENT))
				buf[k++] = i;
	} else
		for (i = 0; i < sc->nbus; i++)
			for (j = 0; j < sc->ndrives; j++) {
				drv = CISS_BIGBIT + i * sc->ndrives + j;
				if (!ciss_pdid(sc, drv, pdid,
						XS_CTL_POLL|XS_CTL_NOSLEEP))
					buf[k++] = drv;
			}
	mutex_exit(&sc->sc_mutex_scratch);

	if (!k)
		return NULL;

	ldp = malloc(sizeof(*ldp) + (k-1), M_DEVBUF, M_NOWAIT);
	if (!ldp)
		return NULL;

	memset(&ldp->bling, 0, sizeof(ldp->bling));
	ldp->ndrives = k;
	ldp->xname[0] = 0;
	memcpy(ldp->tgts, buf, k);
	return ldp;
}

#if 0
static void
ciss_scsi_raw_cmd(struct scsipi_channel *chan, scsipi_adapter_req_t req,
	void *arg)				/* TODO */
{
	struct scsipi_xfer *xs = (struct scsipi_xfer *) arg;
	struct ciss_rawsoftc *rsc = device_private(
	    chan->chan_adapter->adapt_dev);
	struct ciss_softc *sc = rsc->sc_softc;
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	int error;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_scsi_raw_cmd "));

	switch (req)
	{
	case ADAPTER_REQ_RUN_XFER:
		if (xs->cmdlen > CISS_MAX_CDB) {
			CISS_DPRINTF(CISS_D_CMD, ("CDB too big %p ", xs));
			memset(&xs->sense, 0, sizeof(xs->sense));
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __func__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}

		error = 0;
		xs->error = XS_NOERROR;

		/* TODO check this target has not yet employed w/ any volume */

		ccb = ciss_get_ccb(sc);
		cmd = &ccb->ccb_cmd;
		ccb->ccb_len = xs->datalen;
		ccb->ccb_data = xs->data;
		ccb->ccb_xs = xs;

		cmd->cdblen = xs->cmdlen;
		cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL;
		if (xs->xs_control & XS_CTL_DATA_IN)
			cmd->flags |= CISS_CDB_IN;
		else if (xs->xs_control & XS_CTL_DATA_OUT)
			cmd->flags |= CISS_CDB_OUT;
		cmd->tmo = xs->timeout < 1000? 1 : xs->timeout / 1000;
		memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
		memcpy(&cmd->cdb[0], xs->cmd, CISS_MAX_CDB);

		if (ciss_cmd(ccb, BUS_DMA_WAITOK,
		    xs->xs_control & (XS_CTL_POLL|XS_CTL_NOSLEEP))) {
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __func__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}

		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't change the transfer mode, but at least let
		 * scsipi know what the adapter has negociated.
		 */
		 /* Get xfer mode and return it */
		break;
	}
}
#endif

static void
ciss_scsi_cmd(struct scsipi_channel *chan, scsipi_adapter_req_t req,
	void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_xfer_mode *xm;
	struct ciss_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	u_int8_t target;
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_scsi_cmd "));

	switch (req)
	{
	case ADAPTER_REQ_RUN_XFER:
		xs = (struct scsipi_xfer *) arg;
		target = xs->xs_periph->periph_target;
		CISS_DPRINTF(CISS_D_CMD, ("targ=%d ", target));
		if (xs->cmdlen > CISS_MAX_CDB) {
			CISS_DPRINTF(CISS_D_CMD, ("CDB too big %p ", xs));
			memset(&xs->sense, 0, sizeof(xs->sense));
			xs->error = XS_SENSE;
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __func__);
			scsipi_done(xs);
			break;
		}

		xs->error = XS_NOERROR;

		/* XXX emulate SYNCHRONIZE_CACHE ??? */

		ccb = ciss_get_ccb(sc);
		cmd = &ccb->ccb_cmd;
		ccb->ccb_len = xs->datalen;
		ccb->ccb_data = xs->data;
		ccb->ccb_xs = xs;
		cmd->tgt = CISS_CMD_MODE_LD | target;
		cmd->tgt2 = 0;
		cmd->cdblen = xs->cmdlen;
		cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL;
		if (xs->xs_control & XS_CTL_DATA_IN)
			cmd->flags |= CISS_CDB_IN;
		else if (xs->xs_control & XS_CTL_DATA_OUT)
			cmd->flags |= CISS_CDB_OUT;
		cmd->tmo = htole16(xs->timeout < 1000? 1 : xs->timeout / 1000);
		memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
		memcpy(&cmd->cdb[0], xs->cmd, CISS_MAX_CDB);
		CISS_DPRINTF(CISS_D_CMD, ("cmd=%02x %02x %02x %02x %02x %02x ",
			     cmd->cdb[0], cmd->cdb[1], cmd->cdb[2],
			     cmd->cdb[3], cmd->cdb[4], cmd->cdb[5]));

		if (ciss_cmd(ccb, BUS_DMA_WAITOK,
		    xs->xs_control & (XS_CTL_POLL|XS_CTL_NOSLEEP))) {
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __func__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			return;
		}

		break;
	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;
	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't change the transfer mode, but at least let
		 * scsipi know what the adapter has negociated.
		 */
		xm = (struct scsipi_xfer_mode *)arg;
		xm->xm_mode |= PERIPH_CAP_TQING;
		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, xm);
		break;
	default:
		printf("%s: %d %d unsupported\n", __func__, __LINE__, req);
	}
}

int
ciss_intr(void *v)
{
	struct ciss_softc *sc = v;
	struct ciss_ccb *ccb;
	u_int32_t id;
	bus_size_t reg;
	int hit = 0;

	CISS_DPRINTF(CISS_D_INTR, ("intr "));

	if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_ISR) & sc->iem))
		return 0;

	if (sc->cfg.methods & CISS_METH_FIFO64)
		reg = CISS_OUTQ64_HI;
	else if (sc->cfg.methods & CISS_METH_FIFO64_RRO)
		reg = CISS_OUTQ64_LO;
	else
		reg = CISS_OUTQ;
	while ((id = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg)) !=
	    0xffffffff) {
		if (reg == CISS_OUTQ64_HI)
			id = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    CISS_OUTQ64_LO);
		else if (reg == CISS_OUTQ64_LO)
			(void)bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    CISS_OUTQ64_HI);

		ccb = (struct ciss_ccb *) ((char *)sc->ccbs + (id >> 2) * sc->ccblen);
		ccb->ccb_cmd.id = htole32(id);
		ccb->ccb_cmd.id_hi = htole32(0); /* ignore the upper 32bits */
		if (ccb->ccb_state == CISS_CCB_POLL) {
			ccb->ccb_state = CISS_CCB_ONQ;
			mutex_enter(&sc->sc_mutex);
			cv_broadcast(&sc->sc_condvar);
			mutex_exit(&sc->sc_mutex);
		} else
			ciss_done(ccb);

		hit = 1;
	}

	CISS_DPRINTF(CISS_D_INTR, ("exit\n"));
	return hit;
}

static void
ciss_heartbeat(void *v)
{
	struct ciss_softc *sc = v;
	u_int32_t hb;

	hb = bus_space_read_4(sc->sc_iot, sc->cfg_ioh,
	    sc->cfgoff + offsetof(struct ciss_config, heartbeat));
	if (hb == sc->heartbeat) {
		sc->fibrillation++;
		CISS_DPRINTF(CISS_D_ERR, ("%s: fibrillation #%d (value=%d)\n",
		    device_xname(sc->sc_dev), sc->fibrillation, hb));
		if (sc->fibrillation >= 11) {
			/* No heartbeat for 33 seconds */
			panic("%s: dead", device_xname(sc->sc_dev));	/* XXX reset! */
		}
	} else {
		sc->heartbeat = hb;
		if (sc->fibrillation) {
			CISS_DPRINTF(CISS_D_ERR, ("%s: "
			    "fibrillation ended (value=%d)\n",
			    device_xname(sc->sc_dev), hb));
		}
		sc->fibrillation = 0;
	}

	callout_schedule(&sc->sc_hb, hz * 3);
}

static int
ciss_scsi_ioctl(struct scsipi_channel *chan, u_long cmd,
    void *addr, int flag, struct proc *p)
{
#if NBIO > 0
	return ciss_ioctl(chan->chan_adapter->adapt_dev, cmd, addr);
#else
	return ENOTTY;
#endif
}

#if NBIO > 0
const int ciss_level[] = { 0, 4, 1, 5, 51, 7 };
const int ciss_stat[] = { BIOC_SVONLINE, BIOC_SVOFFLINE, BIOC_SVOFFLINE,
    BIOC_SVDEGRADED, BIOC_SVREBUILD, BIOC_SVREBUILD, BIOC_SVDEGRADED,
    BIOC_SVDEGRADED, BIOC_SVINVALID, BIOC_SVINVALID, BIOC_SVBUILDING,
    BIOC_SVOFFLINE, BIOC_SVBUILDING };

int
ciss_ioctl(device_t dev, u_long cmd, void *addr)
{
	struct ciss_softc	*sc = device_private(dev);
	struct bioc_inq *bi;
	struct bioc_disk *bd;
	struct bioc_blink *bb;
	struct ciss_ldstat *ldstat;
	struct ciss_pdid *pdid;
	struct ciss_blink *blink;
	struct ciss_ld *ldp;
	u_int8_t drv;
	int ld, pd, error = 0;

	switch (cmd) {
	case BIOCINQ:
		bi = (struct bioc_inq *)addr;
		strlcpy(bi->bi_dev, device_xname(sc->sc_dev), sizeof(bi->bi_dev));
		bi->bi_novol = sc->maxunits;
		bi->bi_nodisk = sc->sc_lds[0]->ndrives;
		break;

	case BIOCVOL:
		error = ciss_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK_NOVOL:
/*
 * XXX since we don't know how to associate physical drives with logical drives
 * yet, BIOCDISK_NOVOL is equivalent to BIOCDISK to the volume that we've
 * associated all physical drives to.
 * Maybe assoicate all physical drives to all logical volumes, but only return
 * physical drives on one logical volume.  Which one?  Either 1st volume that
 * is degraded, rebuilding, or failed?
 */
		bd = (struct bioc_disk *)addr;
		bd->bd_volid = 0;
		bd->bd_disknovol = true;
		/* FALLTHROUGH */
	case BIOCDISK:
		bd = (struct bioc_disk *)addr;
		if (bd->bd_volid > sc->maxunits) {
			error = EINVAL;
			break;
		}
		ldp = sc->sc_lds[0];
		if (!ldp || (pd = bd->bd_diskid) > ldp->ndrives) {
			error = EINVAL;
			break;
		}
		ldstat = sc->scratch;
		if ((error = ciss_ldstat(sc, bd->bd_volid, ldstat))) {
			break;
		}
		bd->bd_status = -1;
		if (ldstat->stat == CISS_LD_REBLD &&
		    ldstat->bigrebuild == ldp->tgts[pd])
			bd->bd_status = BIOC_SDREBUILD;
		if (ciss_bitset(ldp->tgts[pd] & (~CISS_BIGBIT),
		    ldstat->bigfailed)) {
			bd->bd_status = BIOC_SDFAILED;
			bd->bd_size = 0;
			bd->bd_channel = (ldp->tgts[pd] & (~CISS_BIGBIT)) /
			    sc->ndrives;
			bd->bd_target = ldp->tgts[pd] % sc->ndrives;
			bd->bd_lun = 0;
			bd->bd_vendor[0] = '\0';
			bd->bd_serial[0] = '\0';
			bd->bd_procdev[0] = '\0';
		} else {
			pdid = sc->scratch;
			if ((error = ciss_pdid(sc, ldp->tgts[pd], pdid,
			    XS_CTL_POLL))) {
				bd->bd_status = BIOC_SDFAILED;
				bd->bd_size = 0;
				bd->bd_channel = (ldp->tgts[pd] & (~CISS_BIGBIT)) /
				    sc->ndrives;
				bd->bd_target = ldp->tgts[pd] % sc->ndrives;
				bd->bd_lun = 0;
				bd->bd_vendor[0] = '\0';
				bd->bd_serial[0] = '\0';
				bd->bd_procdev[0] = '\0';
				error = 0;
				break;
			}
			if (bd->bd_status < 0) {
				if (pdid->config & CISS_PD_SPARE)
					bd->bd_status = BIOC_SDHOTSPARE;
				else if (pdid->present & CISS_PD_PRESENT)
					bd->bd_status = BIOC_SDONLINE;
				else
					bd->bd_status = BIOC_SDINVALID;
			}
			bd->bd_size = (u_int64_t)le32toh(pdid->nblocks) *
			    le16toh(pdid->blksz);
			bd->bd_channel = pdid->bus;  
			bd->bd_target = pdid->target;
			bd->bd_lun = 0;
			strlcpy(bd->bd_vendor, pdid->model,
			    sizeof(bd->bd_vendor));
			strlcpy(bd->bd_serial, pdid->serial,
			    sizeof(bd->bd_serial));
			bd->bd_procdev[0] = '\0';
		}
		break;

	case BIOCBLINK:
		bb = (struct bioc_blink *)addr;
		blink = sc->scratch;
		error = EINVAL;
		/* XXX workaround completely dumb scsi addressing */
		for (ld = 0; ld < sc->maxunits; ld++) {
			ldp = sc->sc_lds[ld];
			if (!ldp)
				continue;
			if (sc->ndrives == 256)
				drv = bb->bb_target;
			else
				drv = CISS_BIGBIT +
				    bb->bb_channel * sc->ndrives +
				    bb->bb_target;
			for (pd = 0; pd < ldp->ndrives; pd++)
				if (ldp->tgts[pd] == drv)
					error = ciss_blink(sc, ld, pd,
					    bb->bb_status, blink);
		}
		break;

	case BIOCALARM:
	case BIOCSETSTATE:
	default:
		error = EINVAL;
	}

	return (error);
}

int
ciss_ioctl_vol(struct ciss_softc *sc, struct bioc_vol *bv)
{
	struct ciss_ldid *ldid;
	struct ciss_ld *ldp;
	struct ciss_ldstat *ldstat;
	struct ciss_pdid *pdid;
	int error = 0;
	u_int blks;

	if (bv->bv_volid > sc->maxunits) {
		return EINVAL;
	}
	ldp = sc->sc_lds[bv->bv_volid];
	ldid = sc->scratch;
	if ((error = ciss_ldid(sc, bv->bv_volid, ldid))) {
		return error;
	}
	bv->bv_status = BIOC_SVINVALID;
	blks = (u_int)le16toh(ldid->nblocks[1]) << 16 |
	    le16toh(ldid->nblocks[0]);
	bv->bv_size = blks * (u_quad_t)le16toh(ldid->blksize);
	bv->bv_level = ciss_level[ldid->type];
/*
 * XXX Should only return bv_nodisk for logigal volume that we've associated
 * the physical drives to:  either the 1st degraded, rebuilding, or failed
 * volume else volume 0?
 */
	if (ldp) {
		bv->bv_nodisk = ldp->ndrives;
		strlcpy(bv->bv_dev, ldp->xname, sizeof(bv->bv_dev));
	}
	strlcpy(bv->bv_vendor, "CISS", sizeof(bv->bv_vendor));
	ldstat = sc->scratch;
	memset(ldstat, 0, sizeof(*ldstat));
	if ((error = ciss_ldstat(sc, bv->bv_volid, ldstat))) {
		return error;
	}
	bv->bv_percent = -1;
	bv->bv_seconds = 0;
	if (ldstat->stat < sizeof(ciss_stat)/sizeof(ciss_stat[0]))
		bv->bv_status = ciss_stat[ldstat->stat];
	if (bv->bv_status == BIOC_SVREBUILD ||
	    bv->bv_status == BIOC_SVBUILDING) {
	 	u_int64_t prog;

		ldp = sc->sc_lds[0];
		if (ldp) {
			bv->bv_nodisk = ldp->ndrives;
			strlcpy(bv->bv_dev, ldp->xname, sizeof(bv->bv_dev));
		}
/*
 * XXX ldstat->prog is blocks remaining on physical drive being rebuilt
 * blks is only correct for a RAID1 set;  RAID5 needs to determine the
 * size of the physical device - which we don't yet know.
 * ldstat->bigrebuild has physical device target, so could be used with
 * pdid to get size.   Another way is to save pd information in sc so it's
 * easy to reference.
 */
		prog = (u_int64_t)((ldstat->prog[3] << 24) |
		    (ldstat->prog[2] << 16) | (ldstat->prog[1] << 8) |
		    ldstat->prog[0]);
		pdid = sc->scratch;
		if (!ciss_pdid(sc, ldstat->bigrebuild, pdid, XS_CTL_POLL)) {
			blks = le32toh(pdid->nblocks);
			bv->bv_percent = (blks - prog) * 1000ULL / blks;
		 }
	}
	return 0;
}

int
ciss_blink(struct ciss_softc *sc, int ld, int pd, int stat,
    struct ciss_blink *blink)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_ld *ldp;

	if (ld > sc->maxunits)
		return EINVAL;

	ldp = sc->sc_lds[ld];
	if (!ldp || pd > ldp->ndrives)
		return EINVAL;

	ldp->bling.pdtab[ldp->tgts[pd]] = stat == BIOC_SBUNBLINK? 0 :
	    CISS_BLINK_ALL;
	memcpy(blink, &ldp->bling, sizeof(*blink));

	ccb = ciss_get_ccb(sc);
	if (ccb == NULL)
		return ENOMEM;
	ccb->ccb_len = sizeof(*blink);
	ccb->ccb_data = blink;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_OUT;
	cmd->tmo = htole16(0);
	memset(&cmd->cdb[0], 0, sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_SET;
	cmd->cdb[6] = CISS_CMS_CTRL_PDBLINK;
	cmd->cdb[7] = sizeof(*blink) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*blink) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL);
}

int
ciss_create_sensors(struct ciss_softc *sc)
{
	int			i;
	int nsensors = sc->maxunits;

	if (nsensors == 0) {
		return 0;
	}

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sensor = malloc(sizeof(envsys_data_t) * nsensors,
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_sensor == NULL) {
		aprint_error_dev(sc->sc_dev, "can't allocate envsys_data");
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
	sc->sc_sme->sme_refresh = ciss_sensor_refresh;
	if (sysmon_envsys_register(sc->sc_sme)) {
		printf("%s: unable to register with sysmon\n",
		    device_xname(sc->sc_dev));
		return(1);
	}
	return (0);

out:
	free(sc->sc_sensor, M_DEVBUF);
	sysmon_envsys_destroy(sc->sc_sme);
	return EINVAL;
}

void
ciss_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct ciss_softc	*sc = sme->sme_cookie;
	struct bioc_vol		bv;

	if (edata->sensor >= sc->maxunits)
		return;

	memset(&bv, 0, sizeof(bv));
	bv.bv_volid = edata->sensor;
	if (ciss_ioctl_vol(sc, &bv))
		bv.bv_status = BIOC_SVINVALID;

	bio_vol_to_envsys(edata, &bv);
}
#endif /* NBIO > 0 */
