/* $NetBSD: isp_sbus.c,v 1.81 2012/09/07 22:37:27 macallan Exp $ */
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
 *
 * Copyright (C) 1997, 1998, 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * Additional Copyright (C) 2000-2007 by Matthew Jacob
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isp_sbus.c,v 1.81 2012/09/07 22:37:27 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <dev/ic/isp_netbsd.h>
#include <sys/intr.h>
#include <machine/autoconf.h>
#include <dev/sbus/sbusvar.h>
#include <sys/reboot.h>
#include "opt_isp.h"

static void isp_sbus_reset0(ispsoftc_t *);
static void isp_sbus_reset1(ispsoftc_t *);
static int isp_sbus_intr(void *);
static int
isp_sbus_rd_isr(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
static uint32_t isp_sbus_rd_reg(ispsoftc_t *, int);
static void isp_sbus_wr_reg (ispsoftc_t *, int, uint32_t);
static int isp_sbus_mbxdma(ispsoftc_t *);
static int isp_sbus_dmasetup(ispsoftc_t *, XS_T *, void *);
static void isp_sbus_dmateardown(ispsoftc_t *, XS_T *, uint32_t);

#ifndef	ISP_DISABLE_FW
#include <dev/microcode/isp/asm_sbus.h>
#else
#define	ISP_1000_RISC_CODE	NULL
#endif

static const struct ispmdvec mdvec = {
	isp_sbus_rd_isr,
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	isp_sbus_reset0,
	isp_sbus_reset1,
	NULL,
	ISP_1000_RISC_CODE,
	0,
	0
};

struct isp_sbussoftc {
	ispsoftc_t	sbus_isp;
	sdparam		sbus_dev;
	struct scsipi_channel sbus_chan;
	bus_space_tag_t	sbus_bustag;
	bus_space_handle_t sbus_reg;
	int		sbus_node;
	int		sbus_pri;
	struct ispmdvec	sbus_mdvec;
	bus_dmamap_t	*sbus_dmamap;
	int16_t		sbus_poff[_NREG_BLKS];
};


static int isp_match(device_t, cfdata_t, void *);
static void isp_sbus_attach(device_t, device_t, void *);
CFATTACH_DECL_NEW(isp_sbus, sizeof (struct isp_sbussoftc),
    isp_match, isp_sbus_attach, NULL, NULL);

static int
isp_match(device_t parent, cfdata_t cf, void *aux)
{
	int rv;
	struct sbus_attach_args *sa = aux;

	rv = (strcmp(cf->cf_name, sa->sa_name) == 0 ||
		strcmp("PTI,ptisp", sa->sa_name) == 0 ||
		strcmp("ptisp", sa->sa_name) == 0 ||
		strcmp("SUNW,isp", sa->sa_name) == 0 ||
		strcmp("QLGC,isp", sa->sa_name) == 0);

	return (rv);
}


static void
isp_sbus_attach(device_t parent, device_t self, void *aux)
{
	int freq, ispburst, sbusburst;
	struct sbus_attach_args *sa = aux;
	struct isp_sbussoftc *sbc = device_private(self);
	struct sbus_softc *sbsc = device_private(parent);
	ispsoftc_t *isp = &sbc->sbus_isp;

	isp->isp_osinfo.dev = self;

	printf(" for %s\n", sa->sa_name);

	isp->isp_nchan = isp->isp_osinfo.adapter.adapt_nchannels = 1;

	sbc->sbus_bustag = sa->sa_bustag;
	if (sa->sa_nintr != 0)
		sbc->sbus_pri = sa->sa_pri;
	sbc->sbus_mdvec = mdvec;

	if (sa->sa_npromvaddrs) {
		sbus_promaddr_to_handle(sa->sa_bustag,
			sa->sa_promvaddrs[0], &sbc->sbus_reg);
	} else {
		if (sbus_bus_map(sa->sa_bustag,	sa->sa_slot, sa->sa_offset,
			sa->sa_size, 0, &sbc->sbus_reg) != 0) {
			aprint_error_dev(self, "cannot map registers\n");
			return;
		}
	}
	sbc->sbus_node = sa->sa_node;

	freq = prom_getpropint(sa->sa_node, "clock-frequency", 0);
	if (freq) {
		/*
		 * Convert from HZ to MHz, rounding up.
		 */
		freq = (freq + 500000)/1000000;
	}
	sbc->sbus_mdvec.dv_clock = freq;

	/*
	 * Now figure out what the proper burst sizes, etc., to use.
	 * Unfortunately, there is no ddi_dma_burstsizes here which
	 * walks up the tree finding the limiting burst size node (if
	 * any).
	 */
	sbusburst = sbsc->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1;
	ispburst = prom_getpropint(sa->sa_node, "burst-sizes", -1);
	if (ispburst == -1) {
		ispburst = sbusburst;
	}
	ispburst &= sbusburst;
	ispburst &= ~(1 << 7);
	ispburst &= ~(1 << 6);
	sbc->sbus_mdvec.dv_conf1 =  0;
	if (ispburst & (1 << 5)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_32;
	} else if (ispburst & (1 << 4)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_16;
	} else if (ispburst & (1 << 3)) {
		sbc->sbus_mdvec.dv_conf1 =
		    BIU_SBUS_CONF1_BURST8 | BIU_SBUS_CONF1_FIFO_8;
	}
	if (sbc->sbus_mdvec.dv_conf1) {
		sbc->sbus_mdvec.dv_conf1 |= BIU_BURST_ENABLE;
	}

	isp->isp_mdvec = &sbc->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbc->sbus_dev;
	isp->isp_dmatag = sa->sa_dmatag;
	ISP_MEMZERO(isp->isp_param, sizeof (sdparam));
	isp->isp_osinfo.chan = &sbc->sbus_chan;

	sbc->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbc->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbc->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbc->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbc->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;

	/* Establish interrupt channel */
	bus_intr_establish(sbc->sbus_bustag, sbc->sbus_pri, IPL_BIO,
	    isp_sbus_intr, sbc);

	/*
	 * Set up logging levels.
	 */
#ifdef	ISP_LOGDEFAULT
	isp->isp_dblev = ISP_LOGDEFAULT;
#else
	isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	if (bootverbose)
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
#ifdef	SCSIDEBUG
	isp->isp_dblev |= ISP_LOGDEBUG1|ISP_LOGDEBUG2;
#endif
#ifdef	DEBUG
	isp->isp_dblev |= ISP_LOGDEBUG0;
#endif
#endif

	isp->isp_confopts = device_cfdata(self)->cf_flags;
	SDPARAM(isp, 0)->role = ISP_DEFAULT_ROLES;

	/*
	 * There's no tool on sparc to set NVRAM for ISPs, so ignore it.
	 */
	isp->isp_confopts |= ISP_CFG_NONVRAM;

	/*
	 * Mark things if we're a PTI SBus adapter.
	 */
	if (strcmp("PTI,ptisp", sa->sa_name) == 0 ||
	    strcmp("ptisp", sa->sa_name) == 0) {
		SDPARAM(isp, 0)->isp_ptisp = 1;
	}
	ISP_LOCK(isp);
	isp_reset(isp, 1);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		return;
	}
	ISP_ENABLE_INTS(isp);
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		return;
	}

	/*
	 * do generic attach.
	 */
	ISP_UNLOCK(isp);
	isp_attach(isp);
}


static void
isp_sbus_reset0(ispsoftc_t *isp)
{
	ISP_DISABLE_INTS(isp);
}

static void
isp_sbus_reset1(ispsoftc_t *isp)
{
	ISP_ENABLE_INTS(isp);
}

static int
isp_sbus_intr(void *arg)
{
	uint32_t isr;
	uint16_t sema, mbox;
	ispsoftc_t *isp = arg;

	if (ISP_READ_ISR(isp, &isr, &sema, &mbox) == 0) {
		isp->isp_intbogus++;
		return (0);
	} else {
		struct isp_sbussoftc *sbc = arg;
		sbc->sbus_isp.isp_osinfo.onintstack = 1;
		isp_intr(isp, isr, sema, mbox);
		sbc->sbus_isp.isp_osinfo.onintstack = 0;
		return (1);
	}
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_sbussoftc *)a)->sbus_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(sbc, off)		\
	bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, off)

static int
isp_sbus_rd_isr(ispsoftc_t *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	uint32_t isr;
	uint16_t sema;

	isr = BXR2(sbc, IspVirt2Off(isp, BIU_ISR));
	sema = BXR2(sbc, IspVirt2Off(isp, BIU_SEMA));
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		*mbp = BXR2(sbc, IspVirt2Off(isp, OUTMAILBOX0));
	}
	return (1);
}

static uint32_t
isp_sbus_rd_reg(ispsoftc_t *isp, int regoff)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, offset));
}

static void
isp_sbus_wr_reg(ispsoftc_t *isp, int regoff, uint32_t val)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(sbc->sbus_bustag, sbc->sbus_reg, offset, val);
}

static int
isp_sbus_mbxdma(ispsoftc_t *isp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dma_segment_t reqseg, rspseg;
	int reqrs, rsprs, i, progress;
	size_t n;
	bus_size_t len;

	if (isp->isp_rquest_dma)
		return (0);

	n = isp->isp_maxcmds * sizeof (isp_hdl_t);
	isp->isp_xflist = (isp_hdl_t *) malloc(n, M_DEVBUF, M_WAITOK);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		return (1);
	}
	ISP_MEMZERO(isp->isp_xflist, n);
	for (n = 0; n < isp->isp_maxcmds - 1; n++) {
		isp->isp_xflist[n].cmd = &isp->isp_xflist[n+1];
	}
	isp->isp_xffree = isp->isp_xflist;
	n = sizeof (bus_dmamap_t) * isp->isp_maxcmds;
	sbc->sbus_dmamap = (bus_dmamap_t *) malloc(n, M_DEVBUF, M_WAITOK);
	if (sbc->sbus_dmamap == NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot alloc dmamap array");
		return (1);
	}
	for (i = 0; i < isp->isp_maxcmds; i++) {
		/* Allocate a DMA handle */
		if (bus_dmamap_create(isp->isp_dmatag, MAXPHYS, 1, MAXPHYS,
		    1 << 24, BUS_DMA_NOWAIT, &sbc->sbus_dmamap[i]) != 0) {
			isp_prt(isp, ISP_LOGERR, "cmd DMA maps create error");
			break;
		}
	}
	if (i < isp->isp_maxcmds) {
		while (--i >= 0) {
			bus_dmamap_destroy(isp->isp_dmatag,
			    sbc->sbus_dmamap[i]);
		}
		free(isp->isp_xflist, M_DEVBUF);
		free(sbc->sbus_dmamap, M_DEVBUF);
		isp->isp_xflist = NULL;
		sbc->sbus_dmamap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request and response queues
	 */
	progress = 0;
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(isp->isp_dmatag, len, 0, 0, &reqseg, 1, &reqrs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamem_map(isp->isp_dmatag, &reqseg, reqrs, len,
	    (void *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_create(isp->isp_dmatag, len, 1, len, 1 << 24,
	    BUS_DMA_NOWAIT, &isp->isp_rqdmap) != 0) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_load(isp->isp_dmatag, isp->isp_rqdmap,
	    isp->isp_rquest, len, NULL, BUS_DMA_NOWAIT) != 0) {
		goto dmafail;
	}
	progress++;
	isp->isp_rquest_dma = isp->isp_rqdmap->dm_segs[0].ds_addr;

	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(isp->isp_dmatag, len, 0, 0, &rspseg, 1, &rsprs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamem_map(isp->isp_dmatag, &rspseg, rsprs, len,
	    (void *)&isp->isp_result, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_create(isp->isp_dmatag, len, 1, len, 1 << 24,
	    BUS_DMA_NOWAIT, &isp->isp_rsdmap) != 0) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_load(isp->isp_dmatag, isp->isp_rsdmap,
	    isp->isp_result, len, NULL, BUS_DMA_NOWAIT) != 0) {
		goto dmafail;
	}
	isp->isp_result_dma = isp->isp_rsdmap->dm_segs[0].ds_addr;

	return (0);

dmafail:
	isp_prt(isp, ISP_LOGERR, "Mailbox DMA Setup Failure");

	if (progress >= 8) {
		bus_dmamap_unload(isp->isp_dmatag, isp->isp_rsdmap);
	}
	if (progress >= 7) {
		bus_dmamap_destroy(isp->isp_dmatag, isp->isp_rsdmap);
	}
	if (progress >= 6) {
		bus_dmamem_unmap(isp->isp_dmatag,
		    isp->isp_result, ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp)));
	}
	if (progress >= 5) {
		bus_dmamem_free(isp->isp_dmatag, &rspseg, rsprs);
	}

	if (progress >= 4) {
		bus_dmamap_unload(isp->isp_dmatag, isp->isp_rqdmap);
	}
	if (progress >= 3) {
		bus_dmamap_destroy(isp->isp_dmatag, isp->isp_rqdmap);
	}
	if (progress >= 2) {
		bus_dmamem_unmap(isp->isp_dmatag,
		    isp->isp_rquest, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)));
	}
	if (progress >= 1) {
		bus_dmamem_free(isp->isp_dmatag, &reqseg, reqrs);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(isp->isp_dmatag, sbc->sbus_dmamap[i]);
	}
	free(sbc->sbus_dmamap, M_DEVBUF);
	free(isp->isp_xflist, M_DEVBUF);
	isp->isp_xflist = NULL;
	sbc->sbus_dmamap = NULL;
	return (1);
}

/*
 * Map a DMA request.
 * We're guaranteed that rq->req_handle is a value from 1 to isp->isp_maxcmds.
 */

static int
isp_sbus_dmasetup(struct ispsoftc *isp, struct scsipi_xfer *xs, void *arg)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *)isp;
	ispreq_t *rq = arg;
	bus_dmamap_t dmap;
	bus_dma_segment_t *dm_segs;
	uint32_t nsegs, hidx;
	isp_ddir_t ddir;

	hidx = isp_handle_index(isp, rq->req_handle);
	if (hidx == ISP_BAD_HANDLE_INDEX) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}
	dmap = sbc->sbus_dmamap[hidx];
	if (xs->datalen == 0) {
		ddir = ISP_NOXFR;
		nsegs = 0;
		dm_segs = NULL;
	 } else {
		int error;
		uint32_t flag, flg2;

		if (xs->xs_control & XS_CTL_DATA_IN) {
			flg2 = BUS_DMASYNC_PREREAD;
			flag = BUS_DMA_READ;
			ddir = ISP_FROM_DEVICE;
		} else {
			flg2 = BUS_DMASYNC_PREWRITE;
			flag = BUS_DMA_WRITE;
			ddir = ISP_TO_DEVICE;
		}
		error = bus_dmamap_load(isp->isp_dmatag, dmap, xs->data, xs->datalen,
		    NULL, ((xs->xs_control & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK) | BUS_DMA_STREAMING | flag);
		if (error) {
			isp_prt(isp, ISP_LOGWARN, "unable to load DMA (%d)", error);
			XS_SETERR(xs, HBA_BOTCH);
			if (error == EAGAIN || error == ENOMEM) {
				return (CMD_EAGAIN);
			} else {
				return (CMD_COMPLETE);
			}
		}
		dm_segs = dmap->dm_segs;
		nsegs = dmap->dm_nsegs;
		bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize, flg2);
	}

	if (isp_send_cmd(isp, rq, dm_segs, nsegs, xs->datalen, ddir) != CMD_QUEUED) {
		return (CMD_EAGAIN);
	} else {
		return (CMD_QUEUED);
	}
}

static void
isp_sbus_dmateardown(ispsoftc_t *isp, XS_T *xs, uint32_t handle)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;
	uint32_t hidx;

	hidx = isp_handle_index(isp, handle);
	if (hidx == ISP_BAD_HANDLE_INDEX) {
		isp_xs_prt(isp, xs, ISP_LOGERR, "bad handle on teardown");
		return;
	}
	dmap = sbc->sbus_dmamap[hidx];
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0,
	    xs->datalen, (xs->xs_control & XS_CTL_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}
