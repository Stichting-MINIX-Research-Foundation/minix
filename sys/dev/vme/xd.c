/*	$NetBSD: xd.c,v 1.95 2015/04/26 15:15:20 mlelstv Exp $	*/

/*
 * Copyright (c) 1995 Charles D. Cranor
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

/*
 *
 * x d . c   x y l o g i c s   7 5 3 / 7 0 5 3   v m e / s m d   d r i v e r
 *
 * author: Chuck Cranor <chuck@netbsd>
 * started: 27-Feb-95
 * references: [1] Xylogics Model 753 User's Manual
 *                 part number: 166-753-001, Revision B, May 21, 1988.
 *                 "Your Partner For Performance"
 *             [2] other NetBSD disk device drivers
 *
 * Special thanks go to Scott E. Campbell of Xylogics, Inc. for taking
 * the time to answer some of my questions about the 753/7053.
 *
 * note: the 753 and the 7053 are programmed the same way, but are
 * different sizes.   the 753 is a 6U VME card, while the 7053 is a 9U
 * VME card (found in many VME based suns).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xd.c,v 1.95 2015/04/26 15:15:20 mlelstv Exp $");

#undef XDC_DEBUG		/* full debug */
#define XDC_DIAG		/* extra sanity checks */
#if defined(DIAGNOSTIC) && !defined(XDC_DIAG)
#define XDC_DIAG		/* link in with master DIAG option */
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/dkbad.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <sys/bus.h>
#include <sys/intr.h>

#if defined(__sparc__) || defined(sun3)
#include <dev/sun/disklabel.h>
#endif

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/vme/xdreg.h>
#include <dev/vme/xdvar.h>
#include <dev/vme/xio.h>

#include "locators.h"

/*
 * macros
 */

/*
 * XDC_TWAIT: add iorq "N" to tail of SC's wait queue
 */
#define XDC_TWAIT(SC, N) { \
	(SC)->waitq[(SC)->waitend] = (N); \
	(SC)->waitend = ((SC)->waitend + 1) % XDC_MAXIOPB; \
	(SC)->nwait++; \
}

/*
 * XDC_HWAIT: add iorq "N" to head of SC's wait queue
 */
#define XDC_HWAIT(SC, N) { \
	(SC)->waithead = ((SC)->waithead == 0) ? \
		(XDC_MAXIOPB - 1) : ((SC)->waithead - 1); \
	(SC)->waitq[(SC)->waithead] = (N); \
	(SC)->nwait++; \
}

/*
 * XDC_GET_WAITER: gets the first request waiting on the waitq
 * and removes it (so it can be submitted)
 */
#define XDC_GET_WAITER(XDCSC, RQ) { \
	(RQ) = (XDCSC)->waitq[(XDCSC)->waithead]; \
	(XDCSC)->waithead = ((XDCSC)->waithead + 1) % XDC_MAXIOPB; \
	xdcsc->nwait--; \
}

/*
 * XDC_FREE: add iorq "N" to SC's free list
 */
#define XDC_FREE(SC, N) { \
	(SC)->freereq[(SC)->nfree++] = (N); \
	(SC)->reqs[N].mode = 0; \
	if ((SC)->nfree == 1) wakeup(&(SC)->nfree); \
}


/*
 * XDC_RQALLOC: allocate an iorq off the free list (assume nfree > 0).
 */
#define XDC_RQALLOC(XDCSC) (XDCSC)->freereq[--((XDCSC)->nfree)]

/*
 * XDC_GO: start iopb ADDR (DVMA addr in a u_long) on XDC
 */
#define XDC_GO(XDC, ADDR) { \
	(XDC)->xdc_iopbaddr0 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr1 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr2 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr3 = (ADDR); \
	(XDC)->xdc_iopbamod = XDC_ADDRMOD; \
	(XDC)->xdc_csr = XDC_ADDIOPB; /* go! */ \
}

/*
 * XDC_WAIT: wait for XDC's csr "BITS" to come on in "TIME".
 *   LCV is a counter.  If it goes to zero then we timed out.
 */
#define XDC_WAIT(XDC, LCV, TIME, BITS) { \
	(LCV) = (TIME); \
	while ((LCV) > 0) { \
		if ((XDC)->xdc_csr & (BITS)) break; \
		(LCV) = (LCV) - 1; \
		DELAY(1); \
	} \
}

/*
 * XDC_DONE: don't need IORQ, get error code and free (done after xdc_cmd)
 */
#define XDC_DONE(SC,RQ,ER) { \
	if ((RQ) == XD_ERR_FAIL) { \
		(ER) = (RQ); \
	} else { \
		if ((SC)->ndone-- == XDC_SUBWAITLIM) \
		wakeup(&(SC)->ndone); \
		(ER) = (SC)->reqs[RQ].errnum; \
		XDC_FREE((SC), (RQ)); \
	} \
}

/*
 * XDC_ADVANCE: advance iorq's pointers by a number of sectors
 */
#define XDC_ADVANCE(IORQ, N) { \
	if (N) { \
		(IORQ)->sectcnt -= (N); \
		(IORQ)->blockno += (N); \
		(IORQ)->dbuf += ((N)*XDFM_BPS); \
	} \
}

/*
 * note - addresses you can sleep on:
 *   [1] & of xd_softc's "state" (waiting for a chance to attach a drive)
 *   [2] & of xdc_softc's "nfree" (waiting for a free iorq/iopb)
 *   [3] & of xdc_softc's "ndone" (waiting for number of done iorq/iopb's
 *                                 to drop below XDC_SUBWAITLIM)
 *   [4] & an iorq (waiting for an XD_SUB_WAIT iorq to finish)
 */


/*
 * function prototypes
 * "xdc_*" functions are internal, all others are external interfaces
 */

extern int pil_to_vme[];	/* from obio.c */

/* internals */
int	xdc_cmd(struct xdc_softc *, int, int, int, int, int, char *, int);
const char *xdc_e2str(int);
int	xdc_error(struct xdc_softc *, struct xd_iorq *,
		   struct xd_iopb *, int, int);
int	xdc_ioctlcmd(struct xd_softc *, dev_t dev, struct xd_iocmd *);
void	xdc_perror(struct xd_iorq *, struct xd_iopb *, int);
int	xdc_piodriver(struct xdc_softc *, int, int);
int	xdc_remove_iorq(struct xdc_softc *);
int	xdc_reset(struct xdc_softc *, int, int, int, struct xd_softc *);
inline void xdc_rqinit(struct xd_iorq *, struct xdc_softc *,
			struct xd_softc *, int, u_long, int,
			void *, struct buf *);
void	xdc_rqtopb(struct xd_iorq *, struct xd_iopb *, int, int);
void	xdc_start(struct xdc_softc *, int);
int	xdc_startbuf(struct xdc_softc *, struct xd_softc *, struct buf *);
int	xdc_submit_iorq(struct xdc_softc *, int, int);
void	xdc_tick(void *);
void	xdc_xdreset(struct xdc_softc *, struct xd_softc *);
int	xd_dmamem_alloc(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
			int *, bus_size_t, void **, bus_addr_t *);
void	xd_dmamem_free(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
			int, bus_size_t, void *);


/* machine interrupt hook */
int	xdcintr(void *);

/* autoconf */
int	xdcmatch(device_t, cfdata_t, void *);
void	xdcattach(device_t, device_t, void *);
int	xdmatch(device_t, cfdata_t, void *);
void	xdattach(device_t, device_t, void *);
static	int xdc_probe(void *, bus_space_tag_t, bus_space_handle_t);

static	void xddummystrat(struct buf *);
int	xdgetdisklabel(struct xd_softc *, void *);

/* XXX - think about this more.. xd_machdep? */
void xdc_md_setup(void);
int	XDC_DELAY;

#if defined(__sparc__)
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
void xdc_md_setup(void)
{
	if (CPU_ISSUN4 && cpuinfo.cpu_type == CPUTYP_4_300)
		XDC_DELAY = XDC_DELAY_4_300;
	else
		XDC_DELAY = XDC_DELAY_SPARC;
}
#elif defined(sun3)
void xdc_md_setup(void)
{
	XDC_DELAY = XDC_DELAY_SUN3;
}
#else
void xdc_md_setup(void)
{
	XDC_DELAY = 0;
}
#endif

/*
 * cfattach's: device driver interface to autoconfig
 */

CFATTACH_DECL_NEW(xdc, sizeof(struct xdc_softc),
    xdcmatch, xdcattach, NULL, NULL);

CFATTACH_DECL_NEW(xd, sizeof(struct xd_softc),
    xdmatch, xdattach, NULL, NULL);

extern struct cfdriver xd_cd;

dev_type_open(xdopen);
dev_type_close(xdclose);
dev_type_read(xdread);
dev_type_write(xdwrite);
dev_type_ioctl(xdioctl);
dev_type_strategy(xdstrategy);
dev_type_dump(xddump);
dev_type_size(xdsize);

const struct bdevsw xd_bdevsw = {
	.d_open = xdopen,
	.d_close = xdclose,
	.d_strategy = xdstrategy,
	.d_ioctl = xdioctl,
	.d_dump = xddump,
	.d_psize = xdsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw xd_cdevsw = {
	.d_open = xdopen,
	.d_close = xdclose,
	.d_read = xdread,
	.d_write = xdwrite,
	.d_ioctl = xdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

struct xdc_attach_args {	/* this is the "aux" args to xdattach */
	int	driveno;	/* unit number */
	int	fullmode;	/* submit mode */
	int	booting;	/* are we booting or not? */
};

/*
 * dkdriver
 */

struct dkdriver xddkdriver = {
	.d_strategy = xdstrategy
};

/*
 * start: disk label fix code (XXX)
 */

static void *xd_labeldata;

static void
xddummystrat(struct buf *bp)
{
	if (bp->b_bcount != XDFM_BPS)
		panic("xddummystrat");
	memcpy(bp->b_data, xd_labeldata, XDFM_BPS);
	bp->b_oflags |= BO_DONE;
	bp->b_cflags &= ~BC_BUSY;
}

int
xdgetdisklabel(struct xd_softc *xd, void *b)
{
	const char *err;
#if defined(__sparc__) || defined(sun3)
	struct sun_disklabel *sdl;
#endif

	/* We already have the label data in `b'; setup for dummy strategy */
	xd_labeldata = b;

	/* Required parameter for readdisklabel() */
	xd->sc_dk.dk_label->d_secsize = XDFM_BPS;

	err = readdisklabel(MAKEDISKDEV(0, device_unit(xd->sc_dev), RAW_PART),
			    xddummystrat,
			    xd->sc_dk.dk_label, xd->sc_dk.dk_cpulabel);
	if (err) {
		aprint_error_dev(xd->sc_dev, "%s\n", err);
		return(XD_ERR_FAIL);
	}

#if defined(__sparc__) || defined(sun3)
	/* Ok, we have the label; fill in `pcyl' if there's SunOS magic */
	sdl = (struct sun_disklabel *)xd->sc_dk.dk_cpulabel->cd_block;
	if (sdl->sl_magic == SUN_DKMAGIC) {
		xd->pcyl = sdl->sl_pcylinders;
	} else
#endif
	{
		printf("%s: WARNING: no `pcyl' in disk label.\n",
			device_xname(xd->sc_dev));
		xd->pcyl = xd->sc_dk.dk_label->d_ncylinders +
			xd->sc_dk.dk_label->d_acylinders;
		printf("%s: WARNING: guessing pcyl=%d (ncyl+acyl)\n",
			device_xname(xd->sc_dev), xd->pcyl);
	}

	xd->ncyl = xd->sc_dk.dk_label->d_ncylinders;
	xd->acyl = xd->sc_dk.dk_label->d_acylinders;
	xd->nhead = xd->sc_dk.dk_label->d_ntracks;
	xd->nsect = xd->sc_dk.dk_label->d_nsectors;
	xd->sectpercyl = xd->nhead * xd->nsect;
	xd->sc_dk.dk_label->d_secsize = XDFM_BPS; /* not handled by
						  * sun->bsd */
	return(XD_ERR_AOK);
}

/*
 * end: disk label fix code (XXX)
 */

/*
 * Shorthand for allocating, mapping and loading a DMA buffer
 */
int
xd_dmamem_alloc(bus_dma_tag_t tag, bus_dmamap_t map, bus_dma_segment_t *seg, int *nsegp, bus_size_t len, void * *kvap, bus_addr_t *dmap)
{
	int nseg;
	int error;

	if ((error = bus_dmamem_alloc(tag, len, 0, 0,
				      seg, 1, &nseg, BUS_DMA_NOWAIT)) != 0) {
		return (error);
	}

	if ((error = bus_dmamem_map(tag, seg, nseg,
				    len, kvap,
				    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		bus_dmamem_free(tag, seg, nseg);
		return (error);
	}

	if ((error = bus_dmamap_load(tag, map,
				     *kvap, len, NULL,
				     BUS_DMA_NOWAIT)) != 0) {
		bus_dmamem_unmap(tag, *kvap, len);
		bus_dmamem_free(tag, seg, nseg);
		return (error);
	}

	*dmap = map->dm_segs[0].ds_addr;
	*nsegp = nseg;
	return (0);
}

void
xd_dmamem_free(bus_dma_tag_t tag, bus_dmamap_t map, bus_dma_segment_t *seg, int nseg, bus_size_t len, void * kva)
{

	bus_dmamap_unload(tag, map);
	bus_dmamem_unmap(tag, kva, len);
	bus_dmamem_free(tag, seg, nseg);
}


/*
 * a u t o c o n f i g   f u n c t i o n s
 */

/*
 * xdcmatch: determine if xdc is present or not.   we do a
 * soft reset to detect the xdc.
 */

int
xdc_probe(void *arg, bus_space_tag_t tag, bus_space_handle_t handle)
{
	struct xdc *xdc = (void *)handle; /* XXX */
	int del = 0;

	xdc->xdc_csr = XDC_RESET;
	XDC_WAIT(xdc, del, XDC_RESETUSEC, XDC_RESET);
	return (del > 0 ? 0 : EIO);
}

int
xdcmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct vme_attach_args	*va = aux;
	vme_chipset_tag_t	ct = va->va_vct;
	vme_am_t		mod;
	int error;

	mod = VME_AM_A16 | VME_AM_MBO | VME_AM_SUPER | VME_AM_DATA;
	if (vme_space_alloc(ct, va->r[0].offset, sizeof(struct xdc), mod))
		return (0);

	error = vme_probe(ct, va->r[0].offset, sizeof(struct xdc),
			  mod, VME_D32, xdc_probe, 0);
	vme_space_free(va->va_vct, va->r[0].offset, sizeof(struct xdc), mod);

	return (error == 0);
}

/*
 * xdcattach: attach controller
 */
void
xdcattach(device_t parent, device_t self, void *aux)
{
	struct vme_attach_args	*va = aux;
	vme_chipset_tag_t	ct = va->va_vct;
	bus_space_tag_t		bt;
	bus_space_handle_t	bh;
	vme_intr_handle_t	ih;
	vme_am_t		mod;
	struct xdc_softc	*xdc = device_private(self);
	struct xdc_attach_args	xa;
	int			lcv, rqno, error;
	struct xd_iopb_ctrl	*ctl;
	bus_dma_segment_t	seg;
	int			rseg;
	vme_mapresc_t resc;
	bus_addr_t		busaddr;

	xdc->sc_dev = self;
	xdc_md_setup();

	/* get addressing and intr level stuff from autoconfig and load it
	 * into our xdc_softc. */

	xdc->dmatag = va->va_bdt;
	mod = VME_AM_A16 | VME_AM_MBO | VME_AM_SUPER | VME_AM_DATA;

	if (vme_space_alloc(ct, va->r[0].offset, sizeof(struct xdc), mod))
		panic("xdc: vme alloc");

	if (vme_space_map(ct, va->r[0].offset, sizeof(struct xdc),
			  mod, VME_D32, 0, &bt, &bh, &resc) != 0)
		panic("xdc: vme_map");

	xdc->xdc = (struct xdc *) bh; /* XXX */
	xdc->ipl = va->ilevel;
	xdc->vector = va->ivector;

	for (lcv = 0; lcv < XDC_MAXDEV; lcv++)
		xdc->sc_drives[lcv] = NULL;

	/*
	 * allocate and zero buffers
	 *
	 * note: we simplify the code by allocating the max number of iopbs and
	 * iorq's up front.   thus, we avoid linked lists and the costs
	 * associated with them in exchange for wasting a little memory.
	 */

	/* Get DMA handle for misc. transfers */
	if ((error = vme_dmamap_create(
				ct,		/* VME chip tag */
				MAXPHYS,	/* size */
				VME_AM_A24,	/* address modifier */
				VME_D32,	/* data size */
				0,		/* swap */
				1,		/* nsegments */
				MAXPHYS,	/* maxsegsz */
				0,		/* boundary */
				BUS_DMA_NOWAIT,
				&xdc->auxmap)) != 0) {

		aprint_error_dev(xdc->sc_dev, "DMA buffer map create error %d\n",
			error);
		return;
	}


	/* Get DMA handle for mapping iorq descriptors */
	if ((error = vme_dmamap_create(
				ct,		/* VME chip tag */
				XDC_MAXIOPB * sizeof(struct xd_iopb),
				VME_AM_A24,	/* address modifier */
				VME_D32,	/* data size */
				0,		/* swap */
				1,		/* nsegments */
				XDC_MAXIOPB * sizeof(struct xd_iopb),
				0,		/* boundary */
				BUS_DMA_NOWAIT,
				&xdc->iopmap)) != 0) {

		aprint_error_dev(xdc->sc_dev, "DMA buffer map create error %d\n",
			error);
		return;
	}

	/* Get DMA buffer for iorq descriptors */
	if ((error = xd_dmamem_alloc(xdc->dmatag, xdc->iopmap, &seg, &rseg,
				     XDC_MAXIOPB * sizeof(struct xd_iopb),
				     (void **)&xdc->iopbase,
				     &busaddr)) != 0) {
		aprint_error_dev(xdc->sc_dev, "DMA buffer alloc error %d\n",
			error);
		return;
	}
	xdc->dvmaiopb = (struct xd_iopb *)(u_long)BUS_ADDR_PADDR(busaddr);

	memset(xdc->iopbase, 0, XDC_MAXIOPB * sizeof(struct xd_iopb));

	xdc->reqs = (struct xd_iorq *)
	    malloc(XDC_MAXIOPB * sizeof(struct xd_iorq),
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (xdc->reqs == NULL)
		panic("xdc malloc");

	/* init free list, iorq to iopb pointers, and non-zero fields in the
	 * iopb which never change. */

	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		xdc->reqs[lcv].iopb = &xdc->iopbase[lcv];
		xdc->reqs[lcv].dmaiopb = &xdc->dvmaiopb[lcv];
		xdc->freereq[lcv] = lcv;
		xdc->iopbase[lcv].fixd = 1;	/* always the same */
		xdc->iopbase[lcv].naddrmod = XDC_ADDRMOD; /* always the same */
		xdc->iopbase[lcv].intr_vec = xdc->vector; /* always the same */

		if ((error = vme_dmamap_create(
				ct,		/* VME chip tag */
				MAXPHYS,	/* size */
				VME_AM_A24,	/* address modifier */
				VME_D32,	/* data size */
				0,		/* swap */
				1,		/* nsegments */
				MAXPHYS,	/* maxsegsz */
				0,		/* boundary */
				BUS_DMA_NOWAIT,
				&xdc->reqs[lcv].dmamap)) != 0) {

			aprint_error_dev(xdc->sc_dev, "DMA buffer map create error %d\n",
				error);
			return;
		}
	}
	xdc->nfree = XDC_MAXIOPB;
	xdc->nrun = 0;
	xdc->waithead = xdc->waitend = xdc->nwait = 0;
	xdc->ndone = 0;

	/* init queue of waiting bufs */

	bufq_alloc(&xdc->sc_wq, "fcfs", 0);
	callout_init(&xdc->sc_tick_ch, 0);

	/*
	 * section 7 of the manual tells us how to init the controller:
	 * - read controller parameters (6/0)
	 * - write controller parameters (5/0)
	 */

	/* read controller parameters and insure we have a 753/7053 */

	rqno = xdc_cmd(xdc, XDCMD_RDP, XDFUN_CTL, 0, 0, 0, 0, XD_SUB_POLL);
	if (rqno == XD_ERR_FAIL) {
		printf(": couldn't read controller params\n");
		return;		/* shouldn't ever happen */
	}
	ctl = (struct xd_iopb_ctrl *) &xdc->iopbase[rqno];
	if (ctl->ctype != XDCT_753) {
		if (xdc->reqs[rqno].errnum)
			printf(": %s: ", xdc_e2str(xdc->reqs[rqno].errnum));
		printf(": doesn't identify as a 753/7053\n");
		XDC_DONE(xdc, rqno, error);
		return;
	}
	printf(": Xylogics 753/7053, PROM=0x%x.%02x.%02x\n",
	    ctl->eprom_partno, ctl->eprom_lvl, ctl->eprom_rev);
	XDC_DONE(xdc, rqno, error);

	/* now write controller parameters (xdc_cmd sets all params for us) */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_CTL, 0, 0, 0, 0, XD_SUB_POLL);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xdc->sc_dev, "controller config error: %s\n",
			xdc_e2str(error));
		return;
	}

	/* link in interrupt with higher level software */
	vme_intr_map(ct, va->ilevel, va->ivector, &ih);
	vme_intr_establish(ct, ih, IPL_BIO, xdcintr, xdc);
	evcnt_attach_dynamic(&xdc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(xdc->sc_dev), "intr");


	/* now we must look for disks using autoconfig */
	xa.fullmode = XD_SUB_POLL;
	xa.booting = 1;

	for (xa.driveno = 0; xa.driveno < XDC_MAXDEV; xa.driveno++)
		(void) config_found(self, (void *) &xa, NULL);

	/* start the watchdog clock */
	callout_reset(&xdc->sc_tick_ch, XDC_TICKCNT, xdc_tick, xdc);

}

/*
 * xdmatch: probe for disk.
 *
 * note: we almost always say disk is present.   this allows us to
 * spin up and configure a disk after the system is booted (we can
 * call xdattach!).
 */
int
xdmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct xdc_attach_args *xa = aux;

	/* looking for autoconf wildcard or exact match */

	if (cf->cf_loc[XDCCF_DRIVE] != XDCCF_DRIVE_DEFAULT &&
	    cf->cf_loc[XDCCF_DRIVE] != xa->driveno)
		return 0;

	return 1;

}

/*
 * xdattach: attach a disk.   this can be called from autoconf and also
 * from xdopen/xdstrategy.
 */
void
xdattach(device_t parent, device_t self, void *aux)
{
	struct xd_softc *xd = device_private(self);
	struct xdc_softc *xdc = device_private(parent);
	struct xdc_attach_args *xa = aux;
	int     rqno, spt = 0, mb, blk, lcv, fmode, s = 0, newstate;
	struct xd_iopb_drive *driopb;
	struct dkbad *dkb;
	int			rseg, error;
	bus_dma_segment_t	seg;
	bus_addr_t		busaddr;
	void *			dmaddr;
	char *			buf;

	xd->sc_dev = self;

	/*
	 * Always re-initialize the disk structure.  We want statistics
	 * to start with a clean slate.
	 */
	memset(&xd->sc_dk, 0, sizeof(xd->sc_dk));

	/* if booting, init the xd_softc */

	if (xa->booting) {
		xd->state = XD_DRIVE_UNKNOWN;	/* to start */
		xd->flags = 0;
		xd->parent = xdc;
	}
	xd->xd_drive = xa->driveno;
	fmode = xa->fullmode;
	xdc->sc_drives[xa->driveno] = xd;

	/* if not booting, make sure we are the only process in the attach for
	 * this drive.   if locked out, sleep on it. */

	if (!xa->booting) {
		s = splbio();
		while (xd->state == XD_DRIVE_ATTACHING) {
			if (tsleep(&xd->state, PRIBIO, "xdattach", 0)) {
				splx(s);
				return;
			}
		}
		printf("%s at %s",
			device_xname(xd->sc_dev), device_xname(xd->parent->sc_dev));
	}

	/* we now have control */
	xd->state = XD_DRIVE_ATTACHING;
	newstate = XD_DRIVE_UNKNOWN;

	buf = NULL;
	if ((error = xd_dmamem_alloc(xdc->dmatag, xdc->auxmap, &seg, &rseg,
				     XDFM_BPS,
				     (void **)&buf,
				     &busaddr)) != 0) {
		aprint_error_dev(xdc->sc_dev, "DMA buffer alloc error %d\n",
			error);
		return;
	}
	dmaddr = (void *)(u_long)BUS_ADDR_PADDR(busaddr);

	/* first try and reset the drive */

	rqno = xdc_cmd(xdc, XDCMD_RST, 0, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error == XD_ERR_NRDY) {
		printf(" drive %d: off-line\n", xa->driveno);
		goto done;
	}
	if (error) {
		printf(": ERROR 0x%02x (%s)\n", error, xdc_e2str(error));
		goto done;
	}
	printf(" drive %d: ready\n", xa->driveno);

	/* now set format parameters */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_FMT, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "write format parameters failed: %s\n",
			xdc_e2str(error));
		goto done;
	}

	/* get drive parameters */
	rqno = xdc_cmd(xdc, XDCMD_RDP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	if (rqno != XD_ERR_FAIL) {
		driopb = (struct xd_iopb_drive *) &xdc->iopbase[rqno];
		spt = driopb->sectpertrk;
	}
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "read drive parameters failed: %s\n",
			xdc_e2str(error));
		goto done;
	}

	/*
	 * now set drive parameters (to semi-bogus values) so we can read the
	 * disk label.
	 */
	xd->pcyl = xd->ncyl = 1;
	xd->acyl = 0;
	xd->nhead = 1;
	xd->nsect = 1;
	xd->sectpercyl = 1;
	for (lcv = 0; lcv < 126; lcv++)	/* init empty bad144 table */
		xd->dkb.bt_bad[lcv].bt_cyl = xd->dkb.bt_bad[lcv].bt_trksec = 0xffff;
	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "write drive parameters failed: %s\n",
			xdc_e2str(error));
		goto done;
	}

	/* read disk label */
	rqno = xdc_cmd(xdc, XDCMD_RD, 0, xd->xd_drive, 0, 1, dmaddr, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "reading disk label failed: %s\n",
			xdc_e2str(error));
		goto done;
	}
	newstate = XD_DRIVE_NOLABEL;

	xd->hw_spt = spt;
	/* Attach the disk: must be before getdisklabel to malloc label */
	disk_init(&xd->sc_dk, device_xname(xd->sc_dev), &xddkdriver);
	disk_attach(&xd->sc_dk);

	if (xdgetdisklabel(xd, buf) != XD_ERR_AOK)
		goto done;

	/* inform the user of what is up */
	printf("%s: <%s>, pcyl %d, hw_spt %d\n", device_xname(xd->sc_dev),
		buf, xd->pcyl, spt);
	mb = xd->ncyl * (xd->nhead * xd->nsect) / (1048576 / XDFM_BPS);
	printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec\n",
		device_xname(xd->sc_dev), mb, xd->ncyl, xd->nhead, xd->nsect,
		XDFM_BPS);

	/* now set the real drive parameters! */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "write real drive parameters failed: %s\n",
			xdc_e2str(error));
		goto done;
	}
	newstate = XD_DRIVE_ONLINE;

	/*
	 * read bad144 table. this table resides on the first sector of the
	 * last track of the disk (i.e. second cyl of "acyl" area).
	 */

	blk = (xd->ncyl + xd->acyl - 1) * (xd->nhead * xd->nsect) + /* last cyl */
	    (xd->nhead - 1) * xd->nsect;	/* last head */
	rqno = xdc_cmd(xdc, XDCMD_RD, 0, xd->xd_drive, blk, 1, dmaddr, fmode);
	XDC_DONE(xdc, rqno, error);
	if (error) {
		aprint_error_dev(xd->sc_dev, "reading bad144 failed: %s\n",
			xdc_e2str(error));
		goto done;
	}

	/* check dkbad for sanity */
	dkb = (struct dkbad *) buf;
	for (lcv = 0; lcv < 126; lcv++) {
		if ((dkb->bt_bad[lcv].bt_cyl == 0xffff ||
				dkb->bt_bad[lcv].bt_cyl == 0) &&
		     dkb->bt_bad[lcv].bt_trksec == 0xffff)
			continue;	/* blank */
		if (dkb->bt_bad[lcv].bt_cyl >= xd->ncyl)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec >> 8) >= xd->nhead)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec & 0xff) >= xd->nsect)
			break;
	}
	if (lcv != 126) {
		aprint_error_dev(xd->sc_dev, "warning: invalid bad144 sector!\n");
	} else {
		memcpy(&xd->dkb, buf, XDFM_BPS);
	}

done:
	if (buf != NULL) {
		xd_dmamem_free(xdc->dmatag, xdc->auxmap,
				&seg, rseg, XDFM_BPS, buf);
	}

	xd->state = newstate;
	if (!xa->booting) {
		wakeup(&xd->state);
		splx(s);
	}
}

/*
 * end of autoconfig functions
 */

/*
 * { b , c } d e v s w   f u n c t i o n s
 */

/*
 * xdclose: close device
 */
int
xdclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct xd_softc *xd = device_lookup_private(&xd_cd, DISKUNIT(dev));
	int     part = DISKPART(dev);

	/* clear mask bits */

	switch (fmt) {
	case S_IFCHR:
		xd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		xd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	xd->sc_dk.dk_openmask = xd->sc_dk.dk_copenmask | xd->sc_dk.dk_bopenmask;

	return 0;
}

/*
 * xddump: crash dump system
 */
int
xddump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	int     unit, part;
	struct xd_softc *xd;

	unit = DISKUNIT(dev);
	part = DISKPART(dev);

	xd = device_lookup_private(&xd_cd, unit);
	if (!xd)
		return ENXIO;

	printf("%s%c: crash dump not supported (yet)\n", device_xname(xd->sc_dev),
	    'a' + part);

	return ENXIO;

	/* outline: globals: "dumplo" == sector number of partition to start
	 * dump at (convert to physical sector with partition table)
	 * "dumpsize" == size of dump in clicks "physmem" == size of physical
	 * memory (clicks, ctob() to get bytes) (normal case: dumpsize ==
	 * physmem)
	 *
	 * dump a copy of physical memory to the dump device starting at sector
	 * "dumplo" in the swap partition (make sure > 0).   map in pages as
	 * we go.   use polled I/O.
	 *
	 * XXX how to handle NON_CONTIG? */

}

static enum kauth_device_req
xd_getkauthreq(u_char cmd)
{
	enum kauth_device_req req;

	switch (cmd) {
	case XDCMD_WR:
	case XDCMD_XWR:
		req = KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITE;
		break;

	case XDCMD_RD:
		req = KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READ;
		break;

	case XDCMD_RDP:
	case XDCMD_XRD:
		req = KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READCONF;
		break;

	case XDCMD_WRP:
	case XDCMD_RST:
		req = KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITECONF;
		break;

	case XDCMD_NOP:
	case XDCMD_SK:
	case XDCMD_TST:
	default:
		req = 0;
		break;
	}

	return (req);
}

/*
 * xdioctl: ioctls on XD drives.   based on ioctl's of other netbsd disks.
 */
int
xdioctl(dev_t dev, u_long command, void *addr, int flag, struct lwp *l)

{
	struct xd_softc *xd;
	struct xd_iocmd *xio;
	int     error, s, unit;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif
	struct disklabel *lp;

	unit = DISKUNIT(dev);

	if ((xd = device_lookup_private(&xd_cd, unit)) == NULL)
		return (ENXIO);

	error = disk_ioctl(&xd->sc_dk, dev, command, addr, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	/* switch on ioctl type */

	switch (command) {
	case DIOCSBAD:		/* set bad144 info */
		if ((flag & FWRITE) == 0)
			return EBADF;
		s = splbio();
		memcpy(&xd->dkb, addr, sizeof(xd->dkb));
		splx(s);
		return 0;

	case DIOCSDINFO:	/* set disk label */
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCSDINFO:
		if (command == ODIOCSDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, addr, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)addr;

		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(xd->sc_dk.dk_label,
		    lp, /* xd->sc_dk.dk_openmask : */ 0,
		    xd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (xd->state == XD_DRIVE_NOLABEL)
				xd->state = XD_DRIVE_ONLINE;
		}
		return error;

	case DIOCWLABEL:	/* change write status of disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *) addr)
			xd->flags |= XD_WLABEL;
		else
			xd->flags &= ~XD_WLABEL;
		return 0;

	case DIOCWDINFO:	/* write disk label */
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
		if (command == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, addr, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)addr;

		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(xd->sc_dk.dk_label,
		    lp, /* xd->sc_dk.dk_openmask : */ 0,
		    xd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (xd->state == XD_DRIVE_NOLABEL)
				xd->state = XD_DRIVE_ONLINE;

			/* Simulate opening partition 0 so write succeeds. */
			xd->sc_dk.dk_openmask |= (1 << 0);
			error = writedisklabel(MAKEDISKDEV(major(dev),
			    DISKUNIT(dev), RAW_PART),
			    xdstrategy, xd->sc_dk.dk_label,
			    xd->sc_dk.dk_cpulabel);
			xd->sc_dk.dk_openmask =
			    xd->sc_dk.dk_copenmask | xd->sc_dk.dk_bopenmask;
		}
		return error;

	case DIOSXDCMD: {
		enum kauth_device_req req;

		xio = (struct xd_iocmd *) addr;
		req = xd_getkauthreq(xio->cmd);
		if ((error = kauth_authorize_device_passthru(l->l_cred,
		    dev, req, xio)) != 0)
			return (error);
		return (xdc_ioctlcmd(xd, dev, xio));
		}

	default:
		return ENOTTY;
	}
}
/*
 * xdopen: open drive
 */

int
xdopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int     unit, part;
	struct xd_softc *xd;
	struct xdc_attach_args xa;

	/* first, could it be a valid target? */

	unit = DISKUNIT(dev);
	if ((xd = device_lookup_private(&xd_cd, unit)) == NULL)
		return (ENXIO);
	part = DISKPART(dev);

	/* do we need to attach the drive? */

	if (xd->state == XD_DRIVE_UNKNOWN) {
		xa.driveno = xd->xd_drive;
		xa.fullmode = XD_SUB_WAIT;
		xa.booting = 0;
		xdattach(xd->parent->sc_dev, xd->sc_dev, &xa);
		if (xd->state == XD_DRIVE_UNKNOWN) {
			return (EIO);
		}
	}
	/* check for partition */

	if (part != RAW_PART &&
	    (part >= xd->sc_dk.dk_label->d_npartitions ||
		xd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		return (ENXIO);
	}
	/* set open masks */

	switch (fmt) {
	case S_IFCHR:
		xd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		xd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	xd->sc_dk.dk_openmask = xd->sc_dk.dk_copenmask | xd->sc_dk.dk_bopenmask;

	return 0;
}

int
xdread(dev_t dev, struct uio *uio, int flags)
{

	return (physio(xdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
xdwrite(dev_t dev, struct uio *uio, int flags)
{

	return (physio(xdstrategy, NULL, dev, B_WRITE, minphys, uio));
}


/*
 * xdsize: return size of a partition for a dump
 */

int
xdsize(dev_t dev)
{
	struct xd_softc *xdsc;
	int     unit, part, size, omask;

	/* valid unit? */
	unit = DISKUNIT(dev);
	if ((xdsc = device_lookup_private(&xd_cd, unit)) == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = xdsc->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && xdopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);

	/* do it */
	if (xdsc->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;	/* only give valid size for swap partitions */
	else
		size = xdsc->sc_dk.dk_label->d_partitions[part].p_size *
		    (xdsc->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && xdclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}
/*
 * xdstrategy: buffering system interface to xd.
 */

void
xdstrategy(struct buf *bp)
{
	struct xd_softc *xd;
	struct xdc_softc *parent;
	int     s, unit;
	struct xdc_attach_args xa;

	unit = DISKUNIT(bp->b_dev);

	/* check for live device */

	if (!(xd = device_lookup_private(&xd_cd, unit)) ||
	    bp->b_blkno < 0 ||
	    (bp->b_bcount % xd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto done;
	}
	/* do we need to attach the drive? */

	if (xd->state == XD_DRIVE_UNKNOWN) {
		xa.driveno = xd->xd_drive;
		xa.fullmode = XD_SUB_WAIT;
		xa.booting = 0;
		xdattach(xd->parent->sc_dev, xd->sc_dev, &xa);
		if (xd->state == XD_DRIVE_UNKNOWN) {
			bp->b_error = EIO;
			goto done;
		}
	}
	if (xd->state != XD_DRIVE_ONLINE && DISKPART(bp->b_dev) != RAW_PART) {
		/* no I/O to unlabeled disks, unless raw partition */
		bp->b_error = EIO;
		goto done;
	}
	/* short circuit zero length request */

	if (bp->b_bcount == 0)
		goto done;

	/* check bounds with label (disksubr.c).  Determine the size of the
	 * transfer, and make sure it is within the boundaries of the
	 * partition. Adjust transfer if needed, and signal errors or early
	 * completion. */

	if (bounds_check_with_label(&xd->sc_dk, bp,
		(xd->flags & XD_WLABEL) != 0) <= 0)
		goto done;

	/*
	 * now we know we have a valid buf structure that we need to do I/O
	 * on.
	 *
	 * note that we don't disksort because the controller has a sorting
	 * algorithm built into the hardware.
	 */

	s = splbio();		/* protect the queues */

	/* first, give jobs in front of us a chance */
	parent = xd->parent;
	while (parent->nfree > 0 && bufq_peek(parent->sc_wq) != NULL)
		if (xdc_startbuf(parent, NULL, NULL) != XD_ERR_AOK)
			break;

	/* if there are no free iorq's, then we just queue and return. the
	 * buffs will get picked up later by xdcintr().
	 */

	if (parent->nfree == 0) {
		bufq_put(parent->sc_wq, bp);
		splx(s);
		return;
	}

	/* now we have free iopb's and we are at splbio... start 'em up */
	if (xdc_startbuf(parent, xd, bp) != XD_ERR_AOK) {
		return;
	}

	/* done! */

	splx(s);
	return;

done:				/* tells upper layers we are done with this
				 * buf */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}
/*
 * end of {b,c}devsw functions
 */

/*
 * i n t e r r u p t   f u n c t i o n
 *
 * xdcintr: hardware interrupt.
 */
int
xdcintr(void *v)
{
	struct xdc_softc *xdcsc = v;

	/* kick the event counter */

	xdcsc->sc_intrcnt.ev_count++;

	/* remove as many done IOPBs as possible */

	xdc_remove_iorq(xdcsc);

	/* start any iorq's already waiting */

	xdc_start(xdcsc, XDC_MAXIOPB);

	/* fill up any remaining iorq's with queue'd buffers */

	while (xdcsc->nfree > 0 && bufq_peek(xdcsc->sc_wq) != NULL)
		if (xdc_startbuf(xdcsc, NULL, NULL) != XD_ERR_AOK)
			break;

	return (1);
}
/*
 * end of interrupt function
 */

/*
 * i n t e r n a l   f u n c t i o n s
 */

/*
 * xdc_rqinit: fill out the fields of an I/O request
 */

inline void
xdc_rqinit(struct xd_iorq *rq, struct xdc_softc *xdc, struct xd_softc *xd, int md, u_long blk, int cnt, void *db, struct buf *bp)
{
	rq->xdc = xdc;
	rq->xd = xd;
	rq->ttl = XDC_MAXTTL + 10;
	rq->mode = md;
	rq->tries = rq->errnum = rq->lasterror = 0;
	rq->blockno = blk;
	rq->sectcnt = cnt;
	rq->dbuf = db;
	rq->buf = bp;
}
/*
 * xdc_rqtopb: load up an IOPB based on an iorq
 */

void
xdc_rqtopb(struct xd_iorq *iorq, struct xd_iopb *iopb, int cmd, int subfun)
{
	u_long  block, dp;

	/* standard stuff */

	iopb->errs = iopb->done = 0;
	iopb->comm = cmd;
	iopb->errnum = iopb->status = 0;
	iopb->subfun = subfun;
	if (iorq->xd)
		iopb->unit = iorq->xd->xd_drive;
	else
		iopb->unit = 0;

	/* check for alternate IOPB format */

	if (cmd == XDCMD_WRP) {
		switch (subfun) {
		case XDFUN_CTL:{
			struct xd_iopb_ctrl *ctrl =
				(struct xd_iopb_ctrl *) iopb;
			iopb->lll = 0;
			iopb->intl = (XD_STATE(iorq->mode) == XD_SUB_POLL)
					? 0
					: iorq->xdc->ipl;
			ctrl->param_a = XDPA_TMOD | XDPA_DACF;
			ctrl->param_b = XDPB_ROR | XDPB_TDT_3_2USEC;
			ctrl->param_c = XDPC_OVS | XDPC_COP | XDPC_ASR |
					XDPC_RBC | XDPC_ECC2;
			ctrl->throttle = XDC_THROTTLE;
			ctrl->delay = XDC_DELAY;
			break;
			}
		case XDFUN_DRV:{
			struct xd_iopb_drive *drv =
				(struct xd_iopb_drive *)iopb;
			/* we assume that the disk label has the right
			 * info */
			if (XD_STATE(iorq->mode) == XD_SUB_POLL)
				drv->dparam_ipl = (XDC_DPARAM << 3);
			else
				drv->dparam_ipl = (XDC_DPARAM << 3) |
						  iorq->xdc->ipl;
			drv->maxsect = iorq->xd->nsect - 1;
			drv->maxsector = drv->maxsect;
			/* note: maxsector != maxsect only if you are
			 * doing cyl sparing */
			drv->headoff = 0;
			drv->maxcyl = iorq->xd->pcyl - 1;
			drv->maxhead = iorq->xd->nhead - 1;
			break;
			}
		case XDFUN_FMT:{
			struct xd_iopb_format *form =
					(struct xd_iopb_format *) iopb;
			if (XD_STATE(iorq->mode) == XD_SUB_POLL)
				form->interleave_ipl = (XDC_INTERLEAVE << 3);
			else
				form->interleave_ipl = (XDC_INTERLEAVE << 3) |
						       iorq->xdc->ipl;
			form->field1 = XDFM_FIELD1;
			form->field2 = XDFM_FIELD2;
			form->field3 = XDFM_FIELD3;
			form->field4 = XDFM_FIELD4;
			form->bytespersec = XDFM_BPS;
			form->field6 = XDFM_FIELD6;
			form->field7 = XDFM_FIELD7;
			break;
			}
		}
	} else {

		/* normal IOPB case (harmless to RDP command) */

		iopb->lll = 0;
		iopb->intl = (XD_STATE(iorq->mode) == XD_SUB_POLL)
				? 0
				: iorq->xdc->ipl;
		iopb->sectcnt = iorq->sectcnt;
		block = iorq->blockno;
		if (iorq->xd == NULL || block == 0) {
			iopb->sectno = iopb->headno = iopb->cylno = 0;
		} else {
			iopb->sectno = block % iorq->xd->nsect;
			block = block / iorq->xd->nsect;
			iopb->headno = block % iorq->xd->nhead;
			block = block / iorq->xd->nhead;
			iopb->cylno = block;
		}
		dp = (u_long) iorq->dbuf;
		dp = iopb->daddr = (iorq->dbuf == NULL) ? 0 : dp;
		iopb->addrmod = ((dp + (XDFM_BPS * iorq->sectcnt)) > 0x1000000)
					? XDC_ADDRMOD32
					: XDC_ADDRMOD;
	}
}

/*
 * xdc_cmd: front end for POLL'd and WAIT'd commands.  Returns rqno.
 * If you've already got an IORQ, you can call submit directly (currently
 * there is no need to do this).    NORM requests are handled separately.
 */
int
xdc_cmd(struct xdc_softc *xdcsc, int cmd, int subfn, int unit, int block,
	int scnt, char *dptr, int fullmode)
{
	int     rqno, submode = XD_STATE(fullmode), retry;
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;

	/* get iorq/iopb */
	switch (submode) {
	case XD_SUB_POLL:
		while (xdcsc->nfree == 0) {
			if (xdc_piodriver(xdcsc, 0, 1) != XD_ERR_AOK)
				return (XD_ERR_FAIL);
		}
		break;
	case XD_SUB_WAIT:
		retry = 1;
		while (retry) {
			while (xdcsc->nfree == 0) {
			    if (tsleep(&xdcsc->nfree, PRIBIO, "xdnfree", 0))
				return (XD_ERR_FAIL);
			}
			while (xdcsc->ndone > XDC_SUBWAITLIM) {
			    if (tsleep(&xdcsc->ndone, PRIBIO, "xdsubwait", 0))
				return (XD_ERR_FAIL);
			}
			if (xdcsc->nfree)
				retry = 0;	/* got it */
		}
		break;
	default:
		return (XD_ERR_FAIL);	/* illegal */
	}
	if (xdcsc->nfree == 0)
		panic("xdcmd nfree");
	rqno = XDC_RQALLOC(xdcsc);
	iorq = &xdcsc->reqs[rqno];
	iopb = iorq->iopb;


	/* init iorq/iopb */

	xdc_rqinit(iorq, xdcsc,
	    (unit == XDC_NOUNIT) ? NULL : xdcsc->sc_drives[unit],
	    fullmode, block, scnt, dptr, NULL);

	/* load IOPB from iorq */

	xdc_rqtopb(iorq, iopb, cmd, subfn);

	/* submit it for processing */

	xdc_submit_iorq(xdcsc, rqno, fullmode);	/* error code will be in iorq */

	return (rqno);
}
/*
 * xdc_startbuf
 * start a buffer running, assumes nfree > 0
 */

int
xdc_startbuf(struct xdc_softc *xdcsc, struct xd_softc *xdsc, struct buf *bp)
{
	int     rqno, partno;
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;
	u_long  block;
/*	void *dbuf;*/
	int error;

	if (!xdcsc->nfree)
		panic("xdc_startbuf free");
	rqno = XDC_RQALLOC(xdcsc);
	iorq = &xdcsc->reqs[rqno];
	iopb = iorq->iopb;

	/* get buf */

	if (bp == NULL) {
		bp = bufq_get(xdcsc->sc_wq);
		if (bp == NULL)
			panic("xdc_startbuf bp");
		xdsc = xdcsc->sc_drives[DISKUNIT(bp->b_dev)];
	}
	partno = DISKPART(bp->b_dev);
#ifdef XDC_DEBUG
	printf("xdc_startbuf: %s%c: %s block %d\n", device_xname(xdsc->sc_dev),
	    'a' + partno, (bp->b_flags & B_READ) ? "read" : "write", bp->b_blkno);
	printf("xdc_startbuf: b_bcount %d, b_data 0x%x\n",
	    bp->b_bcount, bp->b_data);
#endif

	/*
	 * load request.  we have to calculate the correct block number based
	 * on partition info.
	 *
	 * note that iorq points to the buffer as mapped into DVMA space,
	 * where as the bp->b_data points to its non-DVMA mapping.
	 */

	block = bp->b_blkno + ((partno == RAW_PART) ? 0 :
	    xdsc->sc_dk.dk_label->d_partitions[partno].p_offset);

	error = bus_dmamap_load(xdcsc->dmatag, iorq->dmamap,
			 bp->b_data, bp->b_bcount, 0, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(xdcsc->sc_dev, "warning: cannot load DMA map\n");
		XDC_FREE(xdcsc, rqno);
		bufq_put(xdcsc->sc_wq, bp);
		return (XD_ERR_FAIL);	/* XXX: need some sort of
					 * call-back scheme here? */
	}
	bus_dmamap_sync(xdcsc->dmatag, iorq->dmamap, 0,
			iorq->dmamap->dm_mapsize, (bp->b_flags & B_READ)
				? BUS_DMASYNC_PREREAD
				: BUS_DMASYNC_PREWRITE);

	/* init iorq and load iopb from it */
	xdc_rqinit(iorq, xdcsc, xdsc, XD_SUB_NORM | XD_MODE_VERBO, block,
		   bp->b_bcount / XDFM_BPS,
		   (void *)(u_long)iorq->dmamap->dm_segs[0].ds_addr,
		   bp);

	xdc_rqtopb(iorq, iopb, (bp->b_flags & B_READ) ? XDCMD_RD : XDCMD_WR, 0);

	/* Instrumentation. */
	disk_busy(&xdsc->sc_dk);

	/* now submit [note that xdc_submit_iorq can never fail on NORM reqs] */

	xdc_submit_iorq(xdcsc, rqno, XD_SUB_NORM);
	return (XD_ERR_AOK);
}


/*
 * xdc_submit_iorq: submit an iorq for processing.  returns XD_ERR_AOK
 * if ok.  if it fail returns an error code.  type is XD_SUB_*.
 *
 * note: caller frees iorq in all cases except NORM
 *
 * return value:
 *   NORM: XD_AOK (req pending), XD_FAIL (couldn't submit request)
 *   WAIT: XD_AOK (success), <error-code> (failed)
 *   POLL: <same as WAIT>
 *   NOQ : <same as NORM>
 *
 * there are three sources for i/o requests:
 * [1] xdstrategy: normal block I/O, using "struct buf" system.
 * [2] autoconfig/crash dump: these are polled I/O requests, no interrupts.
 * [3] open/ioctl: these are I/O requests done in the context of a process,
 *                 and the process should block until they are done.
 *
 * software state is stored in the iorq structure.  each iorq has an
 * iopb structure.  the hardware understands the iopb structure.
 * every command must go through an iopb.  a 7053 can only handle
 * XDC_MAXIOPB (31) active iopbs at one time.  iopbs are allocated in
 * DVMA space at boot up time.  what happens if we run out of iopb's?
 * for i/o type [1], the buffers are queued at the "buff" layer and
 * picked up later by the interrupt routine.  for case [2] the
 * programmed i/o driver is called with a special flag that says
 * return when one iopb is free.  for case [3] the process can sleep
 * on the iorq free list until some iopbs are available.
 */


int
xdc_submit_iorq(struct xdc_softc *xdcsc, int iorqno, int type)
{
	u_long  iopbaddr;
	struct xd_iorq *iorq = &xdcsc->reqs[iorqno];

#ifdef XDC_DEBUG
	printf("xdc_submit_iorq(%s, no=%d, type=%d)\n", device_xname(xdcsc->sc_dev),
	    iorqno, type);
#endif

	/* first check and see if controller is busy */
	if (xdcsc->xdc->xdc_csr & XDC_ADDING) {
#ifdef XDC_DEBUG
		printf("xdc_submit_iorq: XDC not ready (ADDING)\n");
#endif
		if (type == XD_SUB_NOQ)
			return (XD_ERR_FAIL);	/* failed */
		XDC_TWAIT(xdcsc, iorqno);	/* put at end of waitq */
		switch (type) {
		case XD_SUB_NORM:
			return XD_ERR_AOK;	/* success */
		case XD_SUB_WAIT:
			while (iorq->iopb->done == 0) {
				(void) tsleep(iorq, PRIBIO, "xdciorq", 0);
			}
			return (iorq->errnum);
		case XD_SUB_POLL:
			return (xdc_piodriver(xdcsc, iorqno, 0));
		default:
			panic("xdc_submit_iorq adding");
		}
	}
#ifdef XDC_DEBUG
	{
		u_char *rio = (u_char *) iorq->iopb;
		int     sz = sizeof(struct xd_iopb), lcv;
		printf("%s: aio #%d [",
			device_xname(xdcsc->sc_dev), iorq - xdcsc->reqs);
		for (lcv = 0; lcv < sz; lcv++)
			printf(" %02x", rio[lcv]);
		printf("]\n");
	}
#endif				/* XDC_DEBUG */

	/* controller not busy, start command */
	iopbaddr = (u_long) iorq->dmaiopb;
	XDC_GO(xdcsc->xdc, iopbaddr);	/* go! */
	xdcsc->nrun++;
	/* command now running, wrap it up */
	switch (type) {
	case XD_SUB_NORM:
	case XD_SUB_NOQ:
		return (XD_ERR_AOK);	/* success */
	case XD_SUB_WAIT:
		while (iorq->iopb->done == 0) {
			(void) tsleep(iorq, PRIBIO, "xdciorq", 0);
		}
		return (iorq->errnum);
	case XD_SUB_POLL:
		return (xdc_piodriver(xdcsc, iorqno, 0));
	default:
		panic("xdc_submit_iorq wrap up");
	}
	panic("xdc_submit_iorq");
	return 0;	/* not reached */
}


/*
 * xdc_piodriver
 *
 * programmed i/o driver.   this function takes over the computer
 * and drains off all i/o requests.   it returns the status of the iorq
 * the caller is interesting in.   if freeone is true, then it returns
 * when there is a free iorq.
 */
int
xdc_piodriver(struct xdc_softc *xdcsc, int iorqno, int freeone)
{
	int	nreset = 0;
	int	retval = 0;
	u_long	count;
	struct	xdc *xdc = xdcsc->xdc;
#ifdef XDC_DEBUG
	printf("xdc_piodriver(%s, %d, freeone=%d)\n", device_xname(xdcsc->sc_dev),
	    iorqno, freeone);
#endif

	while (xdcsc->nwait || xdcsc->nrun) {
#ifdef XDC_DEBUG
		printf("xdc_piodriver: wait=%d, run=%d\n",
			xdcsc->nwait, xdcsc->nrun);
#endif
		XDC_WAIT(xdc, count, XDC_MAXTIME, (XDC_REMIOPB | XDC_F_ERROR));
#ifdef XDC_DEBUG
		printf("xdc_piodriver: done wait with count = %d\n", count);
#endif
		/* we expect some progress soon */
		if (count == 0 && nreset >= 2) {
			xdc_reset(xdcsc, 0, XD_RSET_ALL, XD_ERR_FAIL, 0);
#ifdef XDC_DEBUG
			printf("xdc_piodriver: timeout\n");
#endif
			return (XD_ERR_FAIL);
		}
		if (count == 0) {
			if (xdc_reset(xdcsc, 0,
				      (nreset++ == 0) ? XD_RSET_NONE : iorqno,
				      XD_ERR_FAIL,
				      0) == XD_ERR_FAIL)
				return (XD_ERR_FAIL);	/* flushes all but POLL
							 * requests, resets */
			continue;
		}
		xdc_remove_iorq(xdcsc);	/* could resubmit request */
		if (freeone) {
			if (xdcsc->nrun < XDC_MAXIOPB) {
#ifdef XDC_DEBUG
				printf("xdc_piodriver: done: one free\n");
#endif
				return (XD_ERR_AOK);
			}
			continue;	/* don't xdc_start */
		}
		xdc_start(xdcsc, XDC_MAXIOPB);
	}

	/* get return value */

	retval = xdcsc->reqs[iorqno].errnum;

#ifdef XDC_DEBUG
	printf("xdc_piodriver: done, retval = 0x%x (%s)\n",
	    xdcsc->reqs[iorqno].errnum, xdc_e2str(xdcsc->reqs[iorqno].errnum));
#endif

	/* now that we've drained everything, start up any bufs that have
	 * queued */

	while (xdcsc->nfree > 0 && bufq_peek(xdcsc->sc_wq) != NULL)
		if (xdc_startbuf(xdcsc, NULL, NULL) != XD_ERR_AOK)
			break;

	return (retval);
}

/*
 * xdc_reset: reset one drive.   NOTE: assumes xdc was just reset.
 * we steal iopb[0] for this, but we put it back when we are done.
 */
void
xdc_xdreset(struct xdc_softc *xdcsc, struct xd_softc *xdsc)
{
	struct xd_iopb tmpiopb;
	u_long  addr;
	int     del;
	memcpy(&tmpiopb, xdcsc->iopbase, sizeof(tmpiopb));
	memset(xdcsc->iopbase, 0, sizeof(tmpiopb));
	xdcsc->iopbase->comm = XDCMD_RST;
	xdcsc->iopbase->unit = xdsc->xd_drive;
	addr = (u_long) xdcsc->dvmaiopb;
	XDC_GO(xdcsc->xdc, addr);	/* go! */
	XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_REMIOPB);
	if (del <= 0 || xdcsc->iopbase->errs) {
		printf("%s: off-line: %s\n", device_xname(xdcsc->sc_dev),
		    xdc_e2str(xdcsc->iopbase->errnum));
		xdcsc->xdc->xdc_csr = XDC_RESET;
		XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_RESET);
		if (del <= 0)
			panic("xdc_reset");
	} else {
		xdcsc->xdc->xdc_csr = XDC_CLRRIO;	/* clear RIO */
	}
	memcpy(xdcsc->iopbase, &tmpiopb, sizeof(tmpiopb));
}


/*
 * xdc_reset: reset everything: requests are marked as errors except
 * a polled request (which is resubmitted)
 */
int
xdc_reset(struct xdc_softc *xdcsc, int quiet, int blastmode, int error,
	struct xd_softc *xdsc)

{
	int     del = 0, lcv, retval = XD_ERR_AOK;
	int     oldfree = xdcsc->nfree;

	/* soft reset hardware */

	if (!quiet)
		printf("%s: soft reset\n", device_xname(xdcsc->sc_dev));
	xdcsc->xdc->xdc_csr = XDC_RESET;
	XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_RESET);
	if (del <= 0) {
		blastmode = XD_RSET_ALL;	/* dead, flush all requests */
		retval = XD_ERR_FAIL;
	}
	if (xdsc)
		xdc_xdreset(xdcsc, xdsc);

	/* fix queues based on "blast-mode" */

	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		register struct xd_iorq *iorq = &xdcsc->reqs[lcv];

		if (XD_STATE(iorq->mode) != XD_SUB_POLL &&
		    XD_STATE(iorq->mode) != XD_SUB_WAIT &&
		    XD_STATE(iorq->mode) != XD_SUB_NORM)
			/* is it active? */
			continue;

		xdcsc->nrun--;	/* it isn't running any more */
		if (blastmode == XD_RSET_ALL || blastmode != lcv) {
			/* failed */
			iorq->errnum = error;
			xdcsc->iopbase[lcv].done = xdcsc->iopbase[lcv].errs = 1;
			switch (XD_STATE(xdcsc->reqs[lcv].mode)) {
			case XD_SUB_NORM:
			    iorq->buf->b_error = EIO;
			    iorq->buf->b_resid =
			       iorq->sectcnt * XDFM_BPS;

			    bus_dmamap_sync(xdcsc->dmatag, iorq->dmamap, 0,
					    iorq->dmamap->dm_mapsize,
					    (iorq->buf->b_flags & B_READ)
						? BUS_DMASYNC_POSTREAD
						: BUS_DMASYNC_POSTWRITE);

			    bus_dmamap_unload(xdcsc->dmatag, iorq->dmamap);

			    disk_unbusy(&xdcsc->reqs[lcv].xd->sc_dk,
				(xdcsc->reqs[lcv].buf->b_bcount -
				xdcsc->reqs[lcv].buf->b_resid),
				(iorq->buf->b_flags & B_READ));
			    biodone(iorq->buf);
			    XDC_FREE(xdcsc, lcv);	/* add to free list */
			    break;
			case XD_SUB_WAIT:
			    wakeup(iorq);
			case XD_SUB_POLL:
			    xdcsc->ndone++;
			    iorq->mode =
				XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			    break;
			}

		} else {

			/* resubmit, put at front of wait queue */
			XDC_HWAIT(xdcsc, lcv);
		}
	}

	/*
	 * now, if stuff is waiting, start it.
	 * since we just reset it should go
	 */
	xdc_start(xdcsc, XDC_MAXIOPB);

	/* ok, we did it */
	if (oldfree == 0 && xdcsc->nfree)
		wakeup(&xdcsc->nfree);

#ifdef XDC_DIAG
	del = xdcsc->nwait + xdcsc->nrun + xdcsc->nfree + xdcsc->ndone;
	if (del != XDC_MAXIOPB)
		printf("%s: diag: xdc_reset miscount (%d should be %d)!\n",
		    device_xname(xdcsc->sc_dev), del, XDC_MAXIOPB);
	else
		if (xdcsc->ndone > XDC_MAXIOPB - XDC_SUBWAITLIM)
			printf("%s: diag: lots of done jobs (%d)\n",
			    device_xname(xdcsc->sc_dev), xdcsc->ndone);
#endif
	printf("RESET DONE\n");
	return (retval);
}
/*
 * xdc_start: start all waiting buffers
 */

void
xdc_start(struct xdc_softc *xdcsc, int maxio)

{
	int     rqno;
	while (maxio && xdcsc->nwait &&
		(xdcsc->xdc->xdc_csr & XDC_ADDING) == 0) {
		XDC_GET_WAITER(xdcsc, rqno);	/* note: rqno is an "out"
						 * param */
		if (xdc_submit_iorq(xdcsc, rqno, XD_SUB_NOQ) != XD_ERR_AOK)
			panic("xdc_start");	/* should never happen */
		maxio--;
	}
}
/*
 * xdc_remove_iorq: remove "done" IOPB's.
 */

int
xdc_remove_iorq(struct xdc_softc *xdcsc)
{
	int     errnum, rqno, comm, errs;
	struct xdc *xdc = xdcsc->xdc;
	struct xd_iopb *iopb;
	struct xd_iorq *iorq;
	struct buf *bp;

	if (xdc->xdc_csr & XDC_F_ERROR) {
		/*
		 * FATAL ERROR: should never happen under normal use. This
		 * error is so bad, you can't even tell which IOPB is bad, so
		 * we dump them all.
		 */
		errnum = xdc->xdc_f_err;
		aprint_error_dev(xdcsc->sc_dev, "fatal error 0x%02x: %s\n",
		    errnum, xdc_e2str(errnum));
		if (xdc_reset(xdcsc, 0, XD_RSET_ALL, errnum, 0) != XD_ERR_AOK) {
			aprint_error_dev(xdcsc->sc_dev, "soft reset failed!\n");
			panic("xdc_remove_iorq: controller DEAD");
		}
		return (XD_ERR_AOK);
	}

	/*
	 * get iopb that is done
	 *
	 * hmm... I used to read the address of the done IOPB off the VME
	 * registers and calculate the rqno directly from that.   that worked
	 * until I started putting a load on the controller.   when loaded, i
	 * would get interrupts but neither the REMIOPB or F_ERROR bits would
	 * be set, even after DELAY'ing a while!   later on the timeout
	 * routine would detect IOPBs that were marked "running" but their
	 * "done" bit was set.   rather than dealing directly with this
	 * problem, it is just easier to look at all running IOPB's for the
	 * done bit.
	 */
	if (xdc->xdc_csr & XDC_REMIOPB) {
		xdc->xdc_csr = XDC_CLRRIO;
	}

	for (rqno = 0; rqno < XDC_MAXIOPB; rqno++) {
		iorq = &xdcsc->reqs[rqno];
		if (iorq->mode == 0 || XD_STATE(iorq->mode) == XD_SUB_DONE)
			continue;	/* free, or done */
		iopb = &xdcsc->iopbase[rqno];
		if (iopb->done == 0)
			continue;	/* not done yet */

#ifdef XDC_DEBUG
		{
			u_char *rio = (u_char *) iopb;
			int     sz = sizeof(struct xd_iopb), lcv;
			printf("%s: rio #%d [", device_xname(xdcsc->sc_dev), rqno);
			for (lcv = 0; lcv < sz; lcv++)
				printf(" %02x", rio[lcv]);
			printf("]\n");
		}
#endif				/* XDC_DEBUG */

		xdcsc->nrun--;

		comm = iopb->comm;
		errs = iopb->errs;

		if (errs)
			iorq->errnum = iopb->errnum;
		else
			iorq->errnum = 0;

		/* handle non-fatal errors */

		if (errs &&
		    xdc_error(xdcsc, iorq, iopb, rqno, comm) == XD_ERR_AOK)
			continue;	/* AOK: we resubmitted it */


		/* this iorq is now done (hasn't been restarted or anything) */

		if ((iorq->mode & XD_MODE_VERBO) && iorq->lasterror)
			xdc_perror(iorq, iopb, 0);

		/* now, if read/write check to make sure we got all the data
		 * we needed. (this may not be the case if we got an error in
		 * the middle of a multisector request).   */

		if ((iorq->mode & XD_MODE_B144) != 0 && errs == 0 &&
		    (comm == XDCMD_RD || comm == XDCMD_WR)) {
			/* we just successfully processed a bad144 sector
			 * note: if we are in bad 144 mode, the pointers have
			 * been advanced already (see above) and are pointing
			 * at the bad144 sector.   to exit bad144 mode, we
			 * must advance the pointers 1 sector and issue a new
			 * request if there are still sectors left to process
			 *
			 */
			XDC_ADVANCE(iorq, 1);	/* advance 1 sector */

			/* exit b144 mode */
			iorq->mode = iorq->mode & (~XD_MODE_B144);

			if (iorq->sectcnt) {	/* more to go! */
				iorq->lasterror = iorq->errnum = iopb->errnum = 0;
				iopb->errs = iopb->done = 0;
				iorq->tries = 0;
				iopb->sectcnt = iorq->sectcnt;
				iopb->cylno = iorq->blockno /
						iorq->xd->sectpercyl;
				iopb->headno =
					(iorq->blockno / iorq->xd->nhead) %
						iorq->xd->nhead;
				iopb->sectno = iorq->blockno % XDFM_BPS;
				iopb->daddr = (u_long) iorq->dbuf;
				XDC_HWAIT(xdcsc, rqno);
				xdc_start(xdcsc, 1);	/* resubmit */
				continue;
			}
		}
		/* final cleanup, totally done with this request */

		switch (XD_STATE(iorq->mode)) {
		case XD_SUB_NORM:
			bp = iorq->buf;
			if (errs) {
				bp->b_error = EIO;
				bp->b_resid = iorq->sectcnt * XDFM_BPS;
			} else {
				bp->b_resid = 0;	/* done */
			}
			bus_dmamap_sync(xdcsc->dmatag, iorq->dmamap, 0,
					iorq->dmamap->dm_mapsize,
					(bp->b_flags & B_READ)
						? BUS_DMASYNC_POSTREAD
						: BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(xdcsc->dmatag, iorq->dmamap);

			disk_unbusy(&iorq->xd->sc_dk,
			    (bp->b_bcount - bp->b_resid),
			    (bp->b_flags & B_READ));
			XDC_FREE(xdcsc, rqno);
			biodone(bp);
			break;
		case XD_SUB_WAIT:
			iorq->mode = XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			xdcsc->ndone++;
			wakeup(iorq);
			break;
		case XD_SUB_POLL:
			iorq->mode = XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			xdcsc->ndone++;
			break;
		}
	}

	return (XD_ERR_AOK);
}

/*
 * xdc_perror: print error.
 * - if still_trying is true: we got an error, retried and got a
 *   different error.  in that case lasterror is the old error,
 *   and errnum is the new one.
 * - if still_trying is not true, then if we ever had an error it
 *   is in lasterror. also, if iorq->errnum == 0, then we recovered
 *   from that error (otherwise iorq->errnum == iorq->lasterror).
 */
void
xdc_perror(struct xd_iorq *iorq, struct xd_iopb *iopb, int still_trying)

{

	int     error = iorq->lasterror;

	printf("%s", (iorq->xd) ? device_xname(iorq->xd->sc_dev)
	    : device_xname(iorq->xdc->sc_dev));
	if (iorq->buf)
		printf("%c: ", 'a' + (char)DISKPART(iorq->buf->b_dev));
	if (iopb->comm == XDCMD_RD || iopb->comm == XDCMD_WR)
		printf("%s %d/%d/%d: ",
			(iopb->comm == XDCMD_RD) ? "read" : "write",
			iopb->cylno, iopb->headno, iopb->sectno);
	printf("%s", xdc_e2str(error));

	if (still_trying)
		printf(" [still trying, new error=%s]", xdc_e2str(iorq->errnum));
	else
		if (iorq->errnum == 0)
			printf(" [recovered in %d tries]", iorq->tries);

	printf("\n");
}

/*
 * xdc_error: non-fatal error encountered... recover.
 * return AOK if resubmitted, return FAIL if this iopb is done
 */
int
xdc_error(struct xdc_softc *xdcsc, struct xd_iorq *iorq, struct xd_iopb *iopb,
	int rqno, int comm)

{
	int     errnum = iorq->errnum;
	int     erract = errnum & XD_ERA_MASK;
	int     oldmode, advance;
#ifdef __sparc__
	int i;
#endif

	if (erract == XD_ERA_RSET) {	/* some errors require a reset */
		oldmode = iorq->mode;
		iorq->mode = XD_SUB_DONE | (~XD_SUB_MASK & oldmode);
		xdcsc->ndone++;
		/* make xdc_start ignore us */
		xdc_reset(xdcsc, 1, XD_RSET_NONE, errnum, iorq->xd);
		iorq->mode = oldmode;
		xdcsc->ndone--;
	}
	/* check for read/write to a sector in bad144 table if bad: redirect
	 * request to bad144 area */

	if ((comm == XDCMD_RD || comm == XDCMD_WR) &&
	    (iorq->mode & XD_MODE_B144) == 0) {
		advance = iorq->sectcnt - iopb->sectcnt;
		XDC_ADVANCE(iorq, advance);
#ifdef __sparc__
		if ((i = isbad(&iorq->xd->dkb, iorq->blockno / iorq->xd->sectpercyl,
			    (iorq->blockno / iorq->xd->nsect) % iorq->xd->nhead,
			    iorq->blockno % iorq->xd->nsect)) != -1) {
			iorq->mode |= XD_MODE_B144;	/* enter bad144 mode &
							 * redirect */
			iopb->errnum = iopb->done = iopb->errs = 0;
			iopb->sectcnt = 1;
			iopb->cylno = (iorq->xd->ncyl + iorq->xd->acyl) - 2;
			/* second to last acyl */
			i = iorq->xd->sectpercyl - 1 - i;	/* follow bad144
								 * standard */
			iopb->headno = i / iorq->xd->nhead;
			iopb->sectno = i % iorq->xd->nhead;
			XDC_HWAIT(xdcsc, rqno);
			xdc_start(xdcsc, 1);	/* resubmit */
			return (XD_ERR_AOK);	/* recovered! */
		}
#endif
	}

	/*
	 * it isn't a bad144 sector, must be real error! see if we can retry
	 * it?
	 */
	if ((iorq->mode & XD_MODE_VERBO) && iorq->lasterror)
		xdc_perror(iorq, iopb, 1);	/* inform of error state
						 * change */
	iorq->lasterror = errnum;

	if ((erract == XD_ERA_RSET || erract == XD_ERA_HARD)
	    && iorq->tries < XDC_MAXTRIES) {	/* retry? */
		iorq->tries++;
		iorq->errnum = iopb->errnum = iopb->done = iopb->errs = 0;
		XDC_HWAIT(xdcsc, rqno);
		xdc_start(xdcsc, 1);	/* restart */
		return (XD_ERR_AOK);	/* recovered! */
	}

	/* failed to recover from this error */
	return (XD_ERR_FAIL);
}

/*
 * xdc_tick: make sure xd is still alive and ticking (err, kicking).
 */
void
xdc_tick(void *arg)

{
	struct xdc_softc *xdcsc = arg;
	int     lcv, s, reset = 0;
#ifdef XDC_DIAG
	int     nwait, nrun, nfree, ndone, whd = 0;
	u_char  fqc[XDC_MAXIOPB], wqc[XDC_MAXIOPB], mark[XDC_MAXIOPB];
	s = splbio();
	nwait = xdcsc->nwait;
	nrun = xdcsc->nrun;
	nfree = xdcsc->nfree;
	ndone = xdcsc->ndone;
	memcpy(wqc, xdcsc->waitq, sizeof(wqc));
	memcpy(fqc, xdcsc->freereq, sizeof(fqc));
	splx(s);
	if (nwait + nrun + nfree + ndone != XDC_MAXIOPB) {
		printf("%s: diag: IOPB miscount (got w/f/r/d %d/%d/%d/%d, wanted %d)\n",
		    device_xname(xdcsc->sc_dev), nwait, nfree, nrun, ndone,
		    XDC_MAXIOPB);
		memset(mark, 0, sizeof(mark));
		printf("FREE: ");
		for (lcv = nfree; lcv > 0; lcv--) {
			printf("%d ", fqc[lcv - 1]);
			mark[fqc[lcv - 1]] = 1;
		}
		printf("\nWAIT: ");
		lcv = nwait;
		while (lcv > 0) {
			printf("%d ", wqc[whd]);
			mark[wqc[whd]] = 1;
			whd = (whd + 1) % XDC_MAXIOPB;
			lcv--;
		}
		printf("\n");
		for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
			if (mark[lcv] == 0)
				printf("MARK: running %d: mode %d done %d errs %d errnum 0x%x ttl %d buf %p\n",
				lcv, xdcsc->reqs[lcv].mode,
				xdcsc->iopbase[lcv].done,
				xdcsc->iopbase[lcv].errs,
				xdcsc->iopbase[lcv].errnum,
				xdcsc->reqs[lcv].ttl, xdcsc->reqs[lcv].buf);
		}
	} else
		if (ndone > XDC_MAXIOPB - XDC_SUBWAITLIM)
			printf("%s: diag: lots of done jobs (%d)\n",
				device_xname(xdcsc->sc_dev), ndone);

#endif
#ifdef XDC_DEBUG
	printf("%s: tick: csr 0x%x, w/f/r/d %d/%d/%d/%d\n",
		device_xname(xdcsc->sc_dev),
		xdcsc->xdc->xdc_csr, xdcsc->nwait, xdcsc->nfree, xdcsc->nrun,
		xdcsc->ndone);
	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		if (xdcsc->reqs[lcv].mode)
		  printf("running %d: mode %d done %d errs %d errnum 0x%x\n",
			 lcv,
			 xdcsc->reqs[lcv].mode, xdcsc->iopbase[lcv].done,
			 xdcsc->iopbase[lcv].errs, xdcsc->iopbase[lcv].errnum);
	}
#endif

	/* reduce ttl for each request if one goes to zero, reset xdc */
	s = splbio();
	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		if (xdcsc->reqs[lcv].mode == 0 ||
		    XD_STATE(xdcsc->reqs[lcv].mode) == XD_SUB_DONE)
			continue;
		xdcsc->reqs[lcv].ttl--;
		if (xdcsc->reqs[lcv].ttl == 0)
			reset = 1;
	}
	if (reset) {
		printf("%s: watchdog timeout\n", device_xname(xdcsc->sc_dev));
		xdc_reset(xdcsc, 0, XD_RSET_NONE, XD_ERR_FAIL, NULL);
	}
	splx(s);

	/* until next time */

	callout_reset(&xdcsc->sc_tick_ch, XDC_TICKCNT, xdc_tick, xdcsc);
}

/*
 * xdc_ioctlcmd: this function provides a user level interface to the
 * controller via ioctl.   this allows "format" programs to be written
 * in user code, and is also useful for some debugging.   we return
 * an error code.   called at user priority.
 */
int
xdc_ioctlcmd(struct xd_softc *xd, dev_t dev, struct xd_iocmd *xio)

{
	int     s, rqno, dummy;
	char *dvmabuf = NULL, *buf = NULL;
	struct xdc_softc *xdcsc;
	int			rseg, error;
	bus_dma_segment_t	seg;

	/* check sanity of requested command */

	switch (xio->cmd) {

	case XDCMD_NOP:	/* no op: everything should be zero */
		if (xio->subfn || xio->dptr || xio->dlen ||
		    xio->block || xio->sectcnt)
			return (EINVAL);
		break;

	case XDCMD_RD:		/* read / write sectors (up to XD_IOCMD_MAXS) */
	case XDCMD_WR:
		if (xio->subfn || xio->sectcnt > XD_IOCMD_MAXS ||
		    xio->sectcnt * XDFM_BPS != xio->dlen || xio->dptr == NULL)
			return (EINVAL);
		break;

	case XDCMD_SK:		/* seek: doesn't seem useful to export this */
		return (EINVAL);

	case XDCMD_WRP:	/* write parameters */
		return (EINVAL);/* not useful, except maybe drive
				 * parameters... but drive parameters should
				 * go via disklabel changes */

	case XDCMD_RDP:	/* read parameters */
		if (xio->subfn != XDFUN_DRV ||
		    xio->dlen || xio->block || xio->dptr)
			return (EINVAL);	/* allow read drive params to
						 * get hw_spt */
		xio->sectcnt = xd->hw_spt;	/* we already know the answer */
		return (0);
		break;

	case XDCMD_XRD:	/* extended read/write */
	case XDCMD_XWR:

		switch (xio->subfn) {

		case XDFUN_THD:/* track headers */
			if (xio->sectcnt != xd->hw_spt ||
			    (xio->block % xd->nsect) != 0 ||
			    xio->dlen != XD_IOCMD_HSZ * xd->hw_spt ||
			    xio->dptr == NULL)
				return (EINVAL);
			xio->sectcnt = 0;
			break;

		case XDFUN_FMT:/* NOTE: also XDFUN_VFY */
			if (xio->cmd == XDCMD_XRD)
				return (EINVAL);	/* no XDFUN_VFY */
			if (xio->sectcnt || xio->dlen ||
			    (xio->block % xd->nsect) != 0 || xio->dptr)
				return (EINVAL);
			break;

		case XDFUN_HDR:/* header, header verify, data, data ECC */
			return (EINVAL);	/* not yet */

		case XDFUN_DM:	/* defect map */
		case XDFUN_DMX:/* defect map (alternate location) */
			if (xio->sectcnt || xio->dlen != XD_IOCMD_DMSZ ||
			    (xio->block % xd->nsect) != 0 || xio->dptr == NULL)
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
		break;

	case XDCMD_TST:	/* diagnostics */
		return (EINVAL);

	default:
		return (EINVAL);/* ??? */
	}

	xdcsc = xd->parent;

	/* create DVMA buffer for request if needed */
	if (xio->dlen) {
		bus_addr_t busbuf;

		if ((error = xd_dmamem_alloc(xdcsc->dmatag, xdcsc->auxmap,
					     &seg, &rseg,
					     xio->dlen, (void **)&buf,
					     &busbuf)) != 0) {
			return (error);
		}
		dvmabuf = (void *)(u_long)BUS_ADDR_PADDR(busbuf);

		if (xio->cmd == XDCMD_WR || xio->cmd == XDCMD_XWR) {
			if ((error = copyin(xio->dptr, buf, xio->dlen)) != 0) {
				bus_dmamem_unmap(xdcsc->dmatag, buf, xio->dlen);
				bus_dmamem_free(xdcsc->dmatag, &seg, rseg);
				return (error);
			}
		}
	}

	/* do it! */

	error = 0;
	s = splbio();
	rqno = xdc_cmd(xdcsc, xio->cmd, xio->subfn, xd->xd_drive, xio->block,
	    xio->sectcnt, dvmabuf, XD_SUB_WAIT);
	if (rqno == XD_ERR_FAIL) {
		error = EIO;
		goto done;
	}
	xio->errnum = xdcsc->reqs[rqno].errnum;
	xio->tries = xdcsc->reqs[rqno].tries;
	XDC_DONE(xdcsc, rqno, dummy);
	__USE(dummy);

	if (xio->cmd == XDCMD_RD || xio->cmd == XDCMD_XRD)
		error = copyout(buf, xio->dptr, xio->dlen);

done:
	splx(s);
	if (dvmabuf) {
		xd_dmamem_free(xdcsc->dmatag, xdcsc->auxmap, &seg, rseg,
				xio->dlen, buf);
	}
	return (error);
}

/*
 * xdc_e2str: convert error code number into an error string
 */
const char *
xdc_e2str(int no)
{
	switch (no) {
	case XD_ERR_FAIL:
		return ("Software fatal error");
	case XD_ERR_AOK:
		return ("Successful completion");
	case XD_ERR_ICYL:
		return ("Illegal cylinder address");
	case XD_ERR_IHD:
		return ("Illegal head address");
	case XD_ERR_ISEC:
		return ("Illgal sector address");
	case XD_ERR_CZER:
		return ("Count zero");
	case XD_ERR_UIMP:
		return ("Unimplemented command");
	case XD_ERR_IF1:
		return ("Illegal field length 1");
	case XD_ERR_IF2:
		return ("Illegal field length 2");
	case XD_ERR_IF3:
		return ("Illegal field length 3");
	case XD_ERR_IF4:
		return ("Illegal field length 4");
	case XD_ERR_IF5:
		return ("Illegal field length 5");
	case XD_ERR_IF6:
		return ("Illegal field length 6");
	case XD_ERR_IF7:
		return ("Illegal field length 7");
	case XD_ERR_ISG:
		return ("Illegal scatter/gather length");
	case XD_ERR_ISPT:
		return ("Not enough sectors per track");
	case XD_ERR_ALGN:
		return ("Next IOPB address alignment error");
	case XD_ERR_SGAL:
		return ("Scatter/gather address alignment error");
	case XD_ERR_SGEC:
		return ("Scatter/gather with auto-ECC");
	case XD_ERR_SECC:
		return ("Soft ECC corrected");
	case XD_ERR_SIGN:
		return ("ECC ignored");
	case XD_ERR_ASEK:
		return ("Auto-seek retry recovered");
	case XD_ERR_RTRY:
		return ("Soft retry recovered");
	case XD_ERR_HECC:
		return ("Hard data ECC");
	case XD_ERR_NHDR:
		return ("Header not found");
	case XD_ERR_NRDY:
		return ("Drive not ready");
	case XD_ERR_TOUT:
		return ("Operation timeout");
	case XD_ERR_VTIM:
		return ("VMEDMA timeout");
	case XD_ERR_DSEQ:
		return ("Disk sequencer error");
	case XD_ERR_HDEC:
		return ("Header ECC error");
	case XD_ERR_RVFY:
		return ("Read verify");
	case XD_ERR_VFER:
		return ("Fatail VMEDMA error");
	case XD_ERR_VBUS:
		return ("VMEbus error");
	case XD_ERR_DFLT:
		return ("Drive faulted");
	case XD_ERR_HECY:
		return ("Header error/cyliner");
	case XD_ERR_HEHD:
		return ("Header error/head");
	case XD_ERR_NOCY:
		return ("Drive not on-cylinder");
	case XD_ERR_SEEK:
		return ("Seek error");
	case XD_ERR_ILSS:
		return ("Illegal sector size");
	case XD_ERR_SEC:
		return ("Soft ECC");
	case XD_ERR_WPER:
		return ("Write-protect error");
	case XD_ERR_IRAM:
		return ("IRAM self test failure");
	case XD_ERR_MT3:
		return ("Maintenance test 3 failure (DSKCEL RAM)");
	case XD_ERR_MT4:
		return ("Maintenance test 4 failure (header shift reg)");
	case XD_ERR_MT5:
		return ("Maintenance test 5 failure (VMEDMA regs)");
	case XD_ERR_MT6:
		return ("Maintenance test 6 failure (REGCEL chip)");
	case XD_ERR_MT7:
		return ("Maintenance test 7 failure (buffer parity)");
	case XD_ERR_MT8:
		return ("Maintenance test 8 failure (disk FIFO)");
	case XD_ERR_IOCK:
		return ("IOPB checksum miscompare");
	case XD_ERR_IODM:
		return ("IOPB DMA fatal");
	case XD_ERR_IOAL:
		return ("IOPB address alignment error");
	case XD_ERR_FIRM:
		return ("Firmware error");
	case XD_ERR_MMOD:
		return ("Illegal maintenance mode test number");
	case XD_ERR_ACFL:
		return ("ACFAIL asserted");
	default:
		return ("Unknown error");
	}
}
