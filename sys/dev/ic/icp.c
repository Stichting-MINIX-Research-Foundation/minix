/*	$NetBSD: icp.c,v 1.31 2012/10/27 17:18:20 chs Exp $	*/

/*-
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, and by Jason R. Thorpe of Wasabi Systems, Inc.
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
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * from OpenBSD: gdt_common.c,v 1.12 2001/07/04 06:43:18 niklas Exp
 */

/*
 * This driver would not have written if it was not for the hardware donations
 * from both ICP-Vortex and Öko.neT.  I want to thank them for their support.
 *
 * Re-worked for NetBSD by Andrew Doran.  Test hardware kindly supplied by
 * Intel.
 *
 * Support for the ICP-Vortex management tools added by
 * Jason R. Thorpe of Wasabi Systems, Inc., based on code
 * provided by Achim Leubner <achim.leubner@intel.com>.
 *
 * Additional support for dynamic rescan of cacheservice drives by
 * Jason R. Thorpe of Wasabi Systems, Inc.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: icp.c,v 1.31 2012/10/27 17:18:20 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/disk.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/icpreg.h>
#include <dev/ic/icpvar.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include "locators.h"

int	icp_async_event(struct icp_softc *, int);
void	icp_ccb_submit(struct icp_softc *icp, struct icp_ccb *ic);
void	icp_chain(struct icp_softc *);
int	icp_print(void *, const char *);
void	icp_watchdog(void *);
void	icp_ucmd_intr(struct icp_ccb *);
void	icp_recompute_openings(struct icp_softc *);

int	icp_count;	/* total # of controllers, for ioctl interface */

/*
 * Statistics for the ioctl interface to query.
 *
 * XXX Global.  They should probably be made per-controller
 * XXX at some point.
 */
gdt_statist_t icp_stats;

int
icp_init(struct icp_softc *icp, const char *intrstr)
{
	struct icp_attach_args icpa;
	struct icp_binfo binfo;
	struct icp_ccb *ic;
	u_int16_t cdev_cnt;
	int i, j, state, feat, nsegs, rv;
	int locs[ICPCF_NLOCS];

	state = 0;

	if (intrstr != NULL)
		aprint_normal_dev(icp->icp_dv, "interrupting at %s\n",
		    intrstr);

	SIMPLEQ_INIT(&icp->icp_ccb_queue);
	SIMPLEQ_INIT(&icp->icp_ccb_freelist);
	SIMPLEQ_INIT(&icp->icp_ucmd_queue);
	callout_init(&icp->icp_wdog_callout, 0);

	/*
	 * Allocate a scratch area.
	 */
	if (bus_dmamap_create(icp->icp_dmat, ICP_SCRATCH_SIZE, 1,
	    ICP_SCRATCH_SIZE, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &icp->icp_scr_dmamap) != 0) {
		aprint_error_dev(icp->icp_dv, "cannot create scratch dmamap\n");
		return (1);
	}
	state++;

	if (bus_dmamem_alloc(icp->icp_dmat, ICP_SCRATCH_SIZE, PAGE_SIZE, 0,
	    icp->icp_scr_seg, 1, &nsegs, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(icp->icp_dv, "cannot alloc scratch dmamem\n");
		goto bail_out;
	}
	state++;

	if (bus_dmamem_map(icp->icp_dmat, icp->icp_scr_seg, nsegs,
	    ICP_SCRATCH_SIZE, &icp->icp_scr, 0)) {
		aprint_error_dev(icp->icp_dv, "cannot map scratch dmamem\n");
		goto bail_out;
	}
	state++;

	if (bus_dmamap_load(icp->icp_dmat, icp->icp_scr_dmamap, icp->icp_scr,
	    ICP_SCRATCH_SIZE, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(icp->icp_dv, "cannot load scratch dmamap\n");
		goto bail_out;
	}
	state++;

	/*
	 * Allocate and initialize the command control blocks.
	 */
	ic = malloc(sizeof(*ic) * ICP_NCCBS, M_DEVBUF, M_NOWAIT | M_ZERO);
	if ((icp->icp_ccbs = ic) == NULL) {
		aprint_error_dev(icp->icp_dv, "malloc() failed\n");
		goto bail_out;
	}
	state++;

	for (i = 0; i < ICP_NCCBS; i++, ic++) {
		/*
		 * The first two command indexes have special meanings, so
		 * we can't use them.
		 */
		ic->ic_ident = i + 2;
		rv = bus_dmamap_create(icp->icp_dmat, ICP_MAX_XFER,
		    ICP_MAXSG, ICP_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ic->ic_xfer_map);
		if (rv != 0)
			break;
		icp->icp_nccbs++;
		icp_ccb_free(icp, ic);
	}
#ifdef DIAGNOSTIC
	if (icp->icp_nccbs != ICP_NCCBS)
		aprint_error_dev(icp->icp_dv, "%d/%d CCBs usable\n",
		    icp->icp_nccbs, ICP_NCCBS);
#endif

	/*
	 * Initalize the controller.
	 */
	if (!icp_cmd(icp, ICP_SCREENSERVICE, ICP_INIT, 0, 0, 0)) {
		aprint_error_dev(icp->icp_dv, "screen service init error %d\n",
		    icp->icp_status);
		goto bail_out;
	}

	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_INIT, ICP_LINUX_OS, 0, 0)) {
		aprint_error_dev(icp->icp_dv, "cache service init error %d\n",
		    icp->icp_status);
		goto bail_out;
	}

	icp_cmd(icp, ICP_CACHESERVICE, ICP_UNFREEZE_IO, 0, 0, 0);

	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_MOUNT, 0xffff, 1, 0)) {
		aprint_error_dev(icp->icp_dv, "cache service mount error %d\n",
		    icp->icp_status);
		goto bail_out;
	}

	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_INIT, ICP_LINUX_OS, 0, 0)) {
		aprint_error_dev(icp->icp_dv, "cache service post-mount init error %d\n",
		    icp->icp_status);
		goto bail_out;
	}
	cdev_cnt = (u_int16_t)icp->icp_info;
	icp->icp_fw_vers = icp->icp_service;

	if (!icp_cmd(icp, ICP_SCSIRAWSERVICE, ICP_INIT, 0, 0, 0)) {
		aprint_error_dev(icp->icp_dv, "raw service init error %d\n",
		    icp->icp_status);
		goto bail_out;
	}

	/*
	 * Set/get raw service features (scatter/gather).
	 */
	feat = 0;
	if (icp_cmd(icp, ICP_SCSIRAWSERVICE, ICP_SET_FEAT, ICP_SCATTER_GATHER,
	    0, 0))
		if (icp_cmd(icp, ICP_SCSIRAWSERVICE, ICP_GET_FEAT, 0, 0, 0))
			feat = icp->icp_info;

	if ((feat & ICP_SCATTER_GATHER) == 0) {
#ifdef DIAGNOSTIC
		aprint_normal_dev(icp->icp_dv, 
		    "scatter/gather not supported (raw service)\n");
#endif
	} else
		icp->icp_features |= ICP_FEAT_RAWSERVICE;

	/*
	 * Set/get cache service features (scatter/gather).
	 */
	feat = 0;
	if (icp_cmd(icp, ICP_CACHESERVICE, ICP_SET_FEAT, 0,
	    ICP_SCATTER_GATHER, 0))
		if (icp_cmd(icp, ICP_CACHESERVICE, ICP_GET_FEAT, 0, 0, 0))
			feat = icp->icp_info;

	if ((feat & ICP_SCATTER_GATHER) == 0) {
#ifdef DIAGNOSTIC
		aprint_normal_dev(icp->icp_dv, 
		    "scatter/gather not supported (cache service)\n");
#endif
	} else
		icp->icp_features |= ICP_FEAT_CACHESERVICE;

	/*
	 * Pull some information from the board and dump.
	 */
	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_IOCTL, ICP_BOARD_INFO,
	    ICP_INVALID_CHANNEL, sizeof(struct icp_binfo))) {
		aprint_error_dev(icp->icp_dv, "unable to retrive board info\n");
		goto bail_out;
	}
	memcpy(&binfo, icp->icp_scr, sizeof(binfo));

	aprint_normal_dev(icp->icp_dv,
	    "model <%s>, firmware <%s>, %d channel(s), %dMB memory\n",
	    binfo.bi_type_string, binfo.bi_raid_string,
	    binfo.bi_chan_count, le32toh(binfo.bi_memsize) >> 20);

	/*
	 * Determine the number of devices, and number of openings per
	 * device.
	 */
	if (icp->icp_features & ICP_FEAT_CACHESERVICE) {
		for (j = 0; j < cdev_cnt && j < ICP_MAX_HDRIVES; j++) {
			if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_INFO, j, 0,
			    0))
				continue;

			icp->icp_cdr[j].cd_size = icp->icp_info;
			if (icp->icp_cdr[j].cd_size != 0)
				icp->icp_ndevs++;

			if (icp_cmd(icp, ICP_CACHESERVICE, ICP_DEVTYPE, j, 0,
			    0))
				icp->icp_cdr[j].cd_type = icp->icp_info;
		}
	}

	if (icp->icp_features & ICP_FEAT_RAWSERVICE) {
		icp->icp_nchan = binfo.bi_chan_count;
		icp->icp_ndevs += icp->icp_nchan;
	}

	icp_recompute_openings(icp);

	/*
	 * Attach SCSI channels.
	 */
	if (icp->icp_features & ICP_FEAT_RAWSERVICE) {
		struct icp_ioc_version *iv;
		struct icp_rawioc *ri;
		struct icp_getch *gc;

		iv = (struct icp_ioc_version *)icp->icp_scr;
		iv->iv_version = htole32(ICP_IOC_NEWEST);
		iv->iv_listents = ICP_MAXBUS;
		iv->iv_firstchan = 0;
		iv->iv_lastchan = ICP_MAXBUS - 1;
		iv->iv_listoffset = htole32(sizeof(*iv));

		if (icp_cmd(icp, ICP_CACHESERVICE, ICP_IOCTL,
		    ICP_IOCHAN_RAW_DESC, ICP_INVALID_CHANNEL,
		    sizeof(*iv) + ICP_MAXBUS * sizeof(*ri))) {
			ri = (struct icp_rawioc *)(iv + 1);
			for (j = 0; j < binfo.bi_chan_count; j++, ri++)
				icp->icp_bus_id[j] = ri->ri_procid;
		} else {
			/*
			 * Fall back to the old method.
			 */
			gc = (struct icp_getch *)icp->icp_scr;

			for (j = 0; j < binfo.bi_chan_count; j++) {
				if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_IOCTL,
				    ICP_SCSI_CHAN_CNT | ICP_L_CTRL_PATTERN,
				    ICP_IO_CHANNEL | ICP_INVALID_CHANNEL,
				    sizeof(*gc))) {
				    	aprint_error_dev(icp->icp_dv,
					    "unable to get chan info");
					goto bail_out;
				}
				icp->icp_bus_id[j] = gc->gc_scsiid;
			}
		}

		for (j = 0; j < binfo.bi_chan_count; j++) {
			if (icp->icp_bus_id[j] > ICP_MAXID_FC)
				icp->icp_bus_id[j] = ICP_MAXID_FC;

			icpa.icpa_unit = j + ICPA_UNIT_SCSI;

			locs[ICPCF_UNIT] = j + ICPA_UNIT_SCSI;

			icp->icp_children[icpa.icpa_unit] =
				config_found_sm_loc(icp->icp_dv, "icp", locs,
					&icpa, icp_print, config_stdsubmatch);
		}
	}

	/*
	 * Attach cache devices.
	 */
	if (icp->icp_features & ICP_FEAT_CACHESERVICE) {
		for (j = 0; j < cdev_cnt && j < ICP_MAX_HDRIVES; j++) {
			if (icp->icp_cdr[j].cd_size == 0)
				continue;

			icpa.icpa_unit = j;

			locs[ICPCF_UNIT] = j;

			icp->icp_children[icpa.icpa_unit] =
			    config_found_sm_loc(icp->icp_dv, "icp", locs,
				&icpa, icp_print, config_stdsubmatch);
		}
	}

	/*
	 * Start the watchdog.
	 */
	icp_watchdog(icp);

	/*
	 * Count the controller, and we're done!
	 */
	if (icp_count++ == 0)
		mutex_init(&icp_ioctl_mutex, MUTEX_DEFAULT, IPL_NONE);

	return (0);

 bail_out:
	if (state > 4)
		for (j = 0; j < i; j++)
			bus_dmamap_destroy(icp->icp_dmat,
			    icp->icp_ccbs[j].ic_xfer_map);
 	if (state > 3)
		free(icp->icp_ccbs, M_DEVBUF);
	if (state > 2)
		bus_dmamap_unload(icp->icp_dmat, icp->icp_scr_dmamap);
	if (state > 1)
		bus_dmamem_unmap(icp->icp_dmat, icp->icp_scr,
		    ICP_SCRATCH_SIZE);
	if (state > 0)
		bus_dmamem_free(icp->icp_dmat, icp->icp_scr_seg, nsegs);
	bus_dmamap_destroy(icp->icp_dmat, icp->icp_scr_dmamap);

	return (1);
}

void
icp_register_servicecb(struct icp_softc *icp, int unit,
    const struct icp_servicecb *cb)
{

	icp->icp_servicecb[unit] = cb;
}

void
icp_rescan(struct icp_softc *icp, int unit)
{
	struct icp_attach_args icpa;
	u_int newsize, newtype;
	int locs[ICPCF_NLOCS];

	/*
	 * NOTE: It is very important that the queue be frozen and not
	 * commands running when this is called.  The ioctl mutex must
	 * also be held.
	 */

	KASSERT(icp->icp_qfreeze != 0);
	KASSERT(icp->icp_running == 0);
	KASSERT(unit < ICP_MAX_HDRIVES);

	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_INFO, unit, 0, 0)) {
#ifdef ICP_DEBUG
		printf("%s: rescan: unit %d ICP_INFO failed -> 0x%04x\n",
		    device_xname(icp->icp_dv), unit, icp->icp_status);
#endif
		goto gone;
	}
	if ((newsize = icp->icp_info) == 0) {
#ifdef ICP_DEBUG
		printf("%s: rescan: unit %d has zero size\n",
		    device_xname(icp->icp_dv), unit);
#endif
 gone:
		/*
		 * Host drive is no longer present; detach if a child
		 * is currently there.
		 */
		if (icp->icp_cdr[unit].cd_size != 0)
			icp->icp_ndevs--;
		icp->icp_cdr[unit].cd_size = 0;
		if (icp->icp_children[unit] != NULL) {
			(void) config_detach(icp->icp_children[unit],
			    DETACH_FORCE);
			icp->icp_children[unit] = NULL;
		}
		return;
	}

	if (icp_cmd(icp, ICP_CACHESERVICE, ICP_DEVTYPE, unit, 0, 0))
		newtype = icp->icp_info;
	else {
#ifdef ICP_DEBUG
		printf("%s: rescan: unit %d ICP_DEVTYPE failed\n",
		    device_xname(icp->icp_dv), unit);
#endif
		newtype = 0;	/* XXX? */
	}

#ifdef ICP_DEBUG
	printf("%s: rescan: unit %d old %u/%u, new %u/%u\n",
	    device_xname(icp->icp_dv), unit, icp->icp_cdr[unit].cd_size,
	    icp->icp_cdr[unit].cd_type, newsize, newtype);
#endif

	/*
	 * If the type or size changed, detach any old child (if it exists)
	 * and attach a new one.
	 */
	if (icp->icp_children[unit] == NULL ||
	    newsize != icp->icp_cdr[unit].cd_size ||
	    newtype != icp->icp_cdr[unit].cd_type) {
		if (icp->icp_cdr[unit].cd_size == 0)
			icp->icp_ndevs++;
		icp->icp_cdr[unit].cd_size = newsize;
		icp->icp_cdr[unit].cd_type = newtype;
		if (icp->icp_children[unit] != NULL)
			(void) config_detach(icp->icp_children[unit],
			    DETACH_FORCE);

		icpa.icpa_unit = unit;

		locs[ICPCF_UNIT] = unit;

		icp->icp_children[unit] = config_found_sm_loc(icp->icp_dv,
			"icp", locs, &icpa, icp_print, config_stdsubmatch);
	}

	icp_recompute_openings(icp);
}

void
icp_rescan_all(struct icp_softc *icp)
{
	int unit;
	u_int16_t cdev_cnt;

	/*
	 * This is the old method of rescanning the host drives.  We
	 * start by reinitializing the cache service.
	 */
	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_INIT, ICP_LINUX_OS, 0, 0)) {
		printf("%s: unable to re-initialize cache service for rescan\n",
		    device_xname(icp->icp_dv));
		return;
	}
	cdev_cnt = (u_int16_t) icp->icp_info;

	/* For each host drive, do the new-style rescan. */
	for (unit = 0; unit < cdev_cnt && unit < ICP_MAX_HDRIVES; unit++)
		icp_rescan(icp, unit);

	/* Now detach anything in the slots after cdev_cnt. */
	for (; unit < ICP_MAX_HDRIVES; unit++) {
		if (icp->icp_cdr[unit].cd_size != 0) {
#ifdef ICP_DEBUG
			printf("%s: rescan all: unit %d < new cdev_cnt (%d)\n",
			    device_xname(icp->icp_dv), unit, cdev_cnt);
#endif
			icp->icp_ndevs--;
			icp->icp_cdr[unit].cd_size = 0;
			if (icp->icp_children[unit] != NULL) {
				(void) config_detach(icp->icp_children[unit],
				    DETACH_FORCE);
				icp->icp_children[unit] = NULL;
			}
		}
	}

	icp_recompute_openings(icp);
}

void
icp_recompute_openings(struct icp_softc *icp)
{
	int unit, openings;

	if (icp->icp_ndevs != 0)
		openings =
		    (icp->icp_nccbs - ICP_NCCB_RESERVE) / icp->icp_ndevs;
	else
		openings = 0;
	if (openings == icp->icp_openings)
		return;
	icp->icp_openings = openings;

#ifdef ICP_DEBUG
	printf("%s: %d device%s, %d openings per device\n",
	    device_xname(icp->icp_dv), icp->icp_ndevs,
	    icp->icp_ndevs == 1 ? "" : "s", icp->icp_openings);
#endif

	for (unit = 0; unit < ICP_MAX_HDRIVES + ICP_MAXBUS; unit++) {
		if (icp->icp_children[unit] != NULL)
			(*icp->icp_servicecb[unit]->iscb_openings)(
			    icp->icp_children[unit], icp->icp_openings);
	}
}

void
icp_watchdog(void *cookie)
{
	struct icp_softc *icp;
	int s;

	icp = cookie;

	s = splbio();
	icp_intr(icp);
	if (ICP_HAS_WORK(icp))
		icp_ccb_enqueue(icp, NULL);
	splx(s);

	callout_reset(&icp->icp_wdog_callout, hz * ICP_WATCHDOG_FREQ,
	    icp_watchdog, icp);
}

int
icp_print(void *aux, const char *pnp)
{
	struct icp_attach_args *icpa;
	const char *str;

	icpa = (struct icp_attach_args *)aux;

	if (pnp != NULL) {
		if (icpa->icpa_unit < ICPA_UNIT_SCSI)
			str = "block device";
		else
			str = "SCSI channel";
		aprint_normal("%s at %s", str, pnp);
	}
	aprint_normal(" unit %d", icpa->icpa_unit);

	return (UNCONF);
}

int
icp_async_event(struct icp_softc *icp, int service)
{

	if (service == ICP_SCREENSERVICE) {
		if (icp->icp_status == ICP_S_MSG_REQUEST) {
			/* XXX */
		}
	} else {
		if ((icp->icp_fw_vers & 0xff) >= 0x1a) {
			icp->icp_evt.size = 0;
			icp->icp_evt.eu.async.ionode =
			    device_unit(icp->icp_dv);
			icp->icp_evt.eu.async.status = icp->icp_status;
			/*
			 * Severity and event string are filled in by the
			 * hardware interface interrupt handler.
			 */
			printf("%s: %s\n", device_xname(icp->icp_dv),
			    icp->icp_evt.event_string);
		} else {
			icp->icp_evt.size = sizeof(icp->icp_evt.eu.async);
			icp->icp_evt.eu.async.ionode =
			    device_unit(icp->icp_dv);
			icp->icp_evt.eu.async.service = service;
			icp->icp_evt.eu.async.status = icp->icp_status;
			icp->icp_evt.eu.async.info = icp->icp_info;
			/* XXXJRT FIX THIS */
			*(u_int32_t *) icp->icp_evt.eu.async.scsi_coord =
			    icp->icp_info2;
		}
		icp_store_event(icp, GDT_ES_ASYNC, service, &icp->icp_evt);
	}

	return (0);
}

int
icp_intr(void *cookie)
{
	struct icp_softc *icp;
	struct icp_intr_ctx ctx;
	struct icp_ccb *ic;

	icp = cookie;

	ctx.istatus = (*icp->icp_get_status)(icp);
	if (!ctx.istatus) {
		icp->icp_status = ICP_S_NO_STATUS;
		return (0);
	}

	(*icp->icp_intr)(icp, &ctx);

	icp->icp_status = ctx.cmd_status;
	icp->icp_service = ctx.service;
	icp->icp_info = ctx.info;
	icp->icp_info2 = ctx.info2;

	switch (ctx.istatus) {
	case ICP_ASYNCINDEX:
		icp_async_event(icp, ctx.service);
		return (1);

	case ICP_SPEZINDEX:
		aprint_error_dev(icp->icp_dv, "uninitialized or unknown service (%d/%d)\n",
		    ctx.info, ctx.info2);
		icp->icp_evt.size = sizeof(icp->icp_evt.eu.driver);
		icp->icp_evt.eu.driver.ionode = device_unit(icp->icp_dv);
		icp_store_event(icp, GDT_ES_DRIVER, 4, &icp->icp_evt);
		return (1);
	}

	if ((ctx.istatus - 2) > icp->icp_nccbs)
		panic("icp_intr: bad command index returned");

	ic = &icp->icp_ccbs[ctx.istatus - 2];
	ic->ic_status = icp->icp_status;

	if ((ic->ic_flags & IC_ALLOCED) == 0) {
		/* XXX ICP's "iir" driver just sends an event here. */
		panic("icp_intr: inactive CCB identified");
	}

	/*
	 * Try to protect ourselves from the running command count already
	 * being 0 (e.g. if a polled command times out).
	 */
	KDASSERT(icp->icp_running != 0);
	if (--icp->icp_running == 0 &&
	    (icp->icp_flags & ICP_F_WAIT_FREEZE) != 0) {
		icp->icp_flags &= ~ICP_F_WAIT_FREEZE;
		wakeup(&icp->icp_qfreeze);
	}

	switch (icp->icp_status) {
	case ICP_S_BSY:
#ifdef ICP_DEBUG
		printf("%s: ICP_S_BSY received\n", device_xname(icp->icp_dv));
#endif
		if (__predict_false((ic->ic_flags & IC_UCMD) != 0))
			SIMPLEQ_INSERT_HEAD(&icp->icp_ucmd_queue, ic, ic_chain);
		else
			SIMPLEQ_INSERT_HEAD(&icp->icp_ccb_queue, ic, ic_chain);
		break;

	default:
		ic->ic_flags |= IC_COMPLETE;

		if ((ic->ic_flags & IC_WAITING) != 0)
			wakeup(ic);
		else if (ic->ic_intr != NULL)
			(*ic->ic_intr)(ic);

		if (ICP_HAS_WORK(icp))
			icp_ccb_enqueue(icp, NULL);

		break;
	}

	return (1);
}

struct icp_ucmd_ctx {
	gdt_ucmd_t *iu_ucmd;
	u_int32_t iu_cnt;
};

void
icp_ucmd_intr(struct icp_ccb *ic)
{
	struct icp_softc *icp = device_private(ic->ic_dv);
	struct icp_ucmd_ctx *iu = ic->ic_context;
	gdt_ucmd_t *ucmd = iu->iu_ucmd;

	ucmd->status = icp->icp_status;
	ucmd->info = icp->icp_info;

	if (iu->iu_cnt != 0) {
		bus_dmamap_sync(icp->icp_dmat,
		    icp->icp_scr_dmamap,
		    ICP_SCRATCH_UCMD, iu->iu_cnt,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		memcpy(ucmd->data,
		    (char *)icp->icp_scr + ICP_SCRATCH_UCMD, iu->iu_cnt);
	}

	icp->icp_ucmd_ccb = NULL;

	ic->ic_flags |= IC_COMPLETE;
	wakeup(ic);
}

/*
 * NOTE: We assume that it is safe to sleep here!
 */
int
icp_cmd(struct icp_softc *icp, u_int8_t service, u_int16_t opcode,
	u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
	struct icp_ioctlcmd *icmd;
	struct icp_cachecmd *cc;
	struct icp_rawcmd *rc;
	int retries, rv;
	struct icp_ccb *ic;

	retries = ICP_RETRIES;

	do {
		ic = icp_ccb_alloc_wait(icp);
		memset(&ic->ic_cmd, 0, sizeof(ic->ic_cmd));
		ic->ic_cmd.cmd_opcode = htole16(opcode);

		switch (service) {
		case ICP_CACHESERVICE:
			if (opcode == ICP_IOCTL) {
				icmd = &ic->ic_cmd.cmd_packet.ic;
				icmd->ic_subfunc = htole16(arg1);
				icmd->ic_channel = htole32(arg2);
				icmd->ic_bufsize = htole32(arg3);
				icmd->ic_addr =
				    htole32(icp->icp_scr_seg[0].ds_addr);

				bus_dmamap_sync(icp->icp_dmat,
				    icp->icp_scr_dmamap, 0, arg3,
				    BUS_DMASYNC_PREWRITE |
				    BUS_DMASYNC_PREREAD);
			} else {
				cc = &ic->ic_cmd.cmd_packet.cc;
				cc->cc_deviceno = htole16(arg1);
				cc->cc_blockno = htole32(arg2);
			}
			break;

		case ICP_SCSIRAWSERVICE:
			rc = &ic->ic_cmd.cmd_packet.rc;
			rc->rc_direction = htole32(arg1);
			rc->rc_bus = arg2;
			rc->rc_target = arg3;
			rc->rc_lun = arg3 >> 8;
			break;
		}

		ic->ic_service = service;
		ic->ic_cmdlen = sizeof(ic->ic_cmd);
		rv = icp_ccb_poll(icp, ic, 10000);

		switch (service) {
		case ICP_CACHESERVICE:
			if (opcode == ICP_IOCTL) {
				bus_dmamap_sync(icp->icp_dmat,
				    icp->icp_scr_dmamap, 0, arg3,
				    BUS_DMASYNC_POSTWRITE |
				    BUS_DMASYNC_POSTREAD);
			}
			break;
		}

		icp_ccb_free(icp, ic);
	} while (rv != 0 && --retries > 0);

	return (icp->icp_status == ICP_S_OK);
}

int
icp_ucmd(struct icp_softc *icp, gdt_ucmd_t *ucmd)
{
	struct icp_ccb *ic;
	struct icp_ucmd_ctx iu;
	u_int32_t cnt;
	int error;

	if (ucmd->service == ICP_CACHESERVICE) {
		if (ucmd->command.cmd_opcode == ICP_IOCTL) {
			cnt = ucmd->command.cmd_packet.ic.ic_bufsize;
			if (cnt > GDT_SCRATCH_SZ) {
				aprint_error_dev(icp->icp_dv, "scratch buffer too small (%d/%d)\n",
				    GDT_SCRATCH_SZ, cnt);
				return (EINVAL);
			}
		} else {
			cnt = ucmd->command.cmd_packet.cc.cc_blockcnt *
			    ICP_SECTOR_SIZE;
			if (cnt > GDT_SCRATCH_SZ) {
				aprint_error_dev(icp->icp_dv, "scratch buffer too small (%d/%d)\n",
				    GDT_SCRATCH_SZ, cnt);
				return (EINVAL);
			}
		}
	} else {
		cnt = ucmd->command.cmd_packet.rc.rc_sdlen +
		    ucmd->command.cmd_packet.rc.rc_sense_len;
		if (cnt > GDT_SCRATCH_SZ) {
			aprint_error_dev(icp->icp_dv, "scratch buffer too small (%d/%d)\n",
			    GDT_SCRATCH_SZ, cnt);
			return (EINVAL);
		}
	}

	iu.iu_ucmd = ucmd;
	iu.iu_cnt = cnt;

	ic = icp_ccb_alloc_wait(icp);
	memset(&ic->ic_cmd, 0, sizeof(ic->ic_cmd));
	ic->ic_cmd.cmd_opcode = htole16(ucmd->command.cmd_opcode);

	if (ucmd->service == ICP_CACHESERVICE) {
		if (ucmd->command.cmd_opcode == ICP_IOCTL) {
			struct icp_ioctlcmd *icmd, *uicmd;

			icmd = &ic->ic_cmd.cmd_packet.ic;
			uicmd = &ucmd->command.cmd_packet.ic;

			icmd->ic_subfunc = htole16(uicmd->ic_subfunc);
			icmd->ic_channel = htole32(uicmd->ic_channel);
			icmd->ic_bufsize = htole32(uicmd->ic_bufsize);
			icmd->ic_addr =
			    htole32(icp->icp_scr_seg[0].ds_addr +
				    ICP_SCRATCH_UCMD);
		} else {
			struct icp_cachecmd *cc, *ucc;

			cc = &ic->ic_cmd.cmd_packet.cc;
			ucc = &ucmd->command.cmd_packet.cc;

			cc->cc_deviceno = htole16(ucc->cc_deviceno);
			cc->cc_blockno = htole32(ucc->cc_blockno);
			cc->cc_blockcnt = htole32(ucc->cc_blockcnt);
			cc->cc_addr = htole32(0xffffffffU);
			cc->cc_nsgent = htole32(1);
			cc->cc_sg[0].sg_addr =
			    htole32(icp->icp_scr_seg[0].ds_addr +
				    ICP_SCRATCH_UCMD);
			cc->cc_sg[0].sg_len = htole32(cnt);
		}
	} else {
		struct icp_rawcmd *rc, *urc;

		rc = &ic->ic_cmd.cmd_packet.rc;
		urc = &ucmd->command.cmd_packet.rc;

		rc->rc_direction = htole32(urc->rc_direction);
		rc->rc_sdata = htole32(0xffffffffU);
		rc->rc_sdlen = htole32(urc->rc_sdlen);
		rc->rc_clen = htole32(urc->rc_clen);
		memcpy(rc->rc_cdb, urc->rc_cdb, sizeof(rc->rc_cdb));
		rc->rc_target = urc->rc_target;
		rc->rc_lun = urc->rc_lun;
		rc->rc_bus = urc->rc_bus;
		rc->rc_sense_len = htole32(urc->rc_sense_len);
		rc->rc_sense_addr =
		    htole32(icp->icp_scr_seg[0].ds_addr +
			    ICP_SCRATCH_UCMD + urc->rc_sdlen);
		rc->rc_nsgent = htole32(1);
		rc->rc_sg[0].sg_addr =
		    htole32(icp->icp_scr_seg[0].ds_addr + ICP_SCRATCH_UCMD);
		rc->rc_sg[0].sg_len = htole32(cnt - urc->rc_sense_len);
	}

	ic->ic_service = ucmd->service;
	ic->ic_cmdlen = sizeof(ic->ic_cmd);
	ic->ic_context = &iu;

	/*
	 * XXX What units are ucmd->timeout in?  Until we know, we
	 * XXX just pull a number out of thin air.
	 */
	if (__predict_false((error = icp_ccb_wait_user(icp, ic, 30000)) != 0))
		aprint_error_dev(icp->icp_dv, "error %d waiting for ucmd to complete\n",
		    error);

	/* icp_ucmd_intr() has updated ucmd. */
	icp_ccb_free(icp, ic);

	return (error);
}

struct icp_ccb *
icp_ccb_alloc(struct icp_softc *icp)
{
	struct icp_ccb *ic;
	int s;

	s = splbio();
	if (__predict_false((ic =
			     SIMPLEQ_FIRST(&icp->icp_ccb_freelist)) == NULL)) {
		splx(s);
		return (NULL);
	}
	SIMPLEQ_REMOVE_HEAD(&icp->icp_ccb_freelist, ic_chain);
	splx(s);

	ic->ic_flags = IC_ALLOCED;
	return (ic);
}

struct icp_ccb *
icp_ccb_alloc_wait(struct icp_softc *icp)
{
	struct icp_ccb *ic;
	int s;

	s = splbio();
	while ((ic = SIMPLEQ_FIRST(&icp->icp_ccb_freelist)) == NULL) {
		icp->icp_flags |= ICP_F_WAIT_CCB;
		(void) tsleep(&icp->icp_ccb_freelist, PRIBIO, "icpccb", 0);
	}
	SIMPLEQ_REMOVE_HEAD(&icp->icp_ccb_freelist, ic_chain);
	splx(s);

	ic->ic_flags = IC_ALLOCED;
	return (ic);
}

void
icp_ccb_free(struct icp_softc *icp, struct icp_ccb *ic)
{
	int s;

	s = splbio();
	ic->ic_flags = 0;
	ic->ic_intr = NULL;
	SIMPLEQ_INSERT_HEAD(&icp->icp_ccb_freelist, ic, ic_chain);
	if (__predict_false((icp->icp_flags & ICP_F_WAIT_CCB) != 0)) {
		icp->icp_flags &= ~ICP_F_WAIT_CCB;
		wakeup(&icp->icp_ccb_freelist);
	}
	splx(s);
}

void
icp_ccb_enqueue(struct icp_softc *icp, struct icp_ccb *ic)
{
	int s;

	s = splbio();

	if (ic != NULL) {
		if (__predict_false((ic->ic_flags & IC_UCMD) != 0))
			SIMPLEQ_INSERT_TAIL(&icp->icp_ucmd_queue, ic, ic_chain);
		else
			SIMPLEQ_INSERT_TAIL(&icp->icp_ccb_queue, ic, ic_chain);
	}

	for (; icp->icp_qfreeze == 0;) {
		if (__predict_false((ic =
			    SIMPLEQ_FIRST(&icp->icp_ucmd_queue)) != NULL)) {
			struct icp_ucmd_ctx *iu = ic->ic_context;
			gdt_ucmd_t *ucmd = iu->iu_ucmd;

			/*
			 * All user-generated commands share the same
			 * scratch space, so if one is already running,
			 * we have to stall the command queue.
			 */
			if (icp->icp_ucmd_ccb != NULL)
				break;
			if ((*icp->icp_test_busy)(icp))
				break;
			icp->icp_ucmd_ccb = ic;

			if (iu->iu_cnt != 0) {
				memcpy((char *)icp->icp_scr + ICP_SCRATCH_UCMD,
				    ucmd->data, iu->iu_cnt);
				bus_dmamap_sync(icp->icp_dmat,
				    icp->icp_scr_dmamap,
				    ICP_SCRATCH_UCMD, iu->iu_cnt,
				    BUS_DMASYNC_PREREAD |
				    BUS_DMASYNC_PREWRITE);
			}
		} else if (__predict_true((ic =
				SIMPLEQ_FIRST(&icp->icp_ccb_queue)) != NULL)) {
			if ((*icp->icp_test_busy)(icp))
				break;
		} else {
			/* no command found */
			break;
		}
		icp_ccb_submit(icp, ic);
		if (__predict_false((ic->ic_flags & IC_UCMD) != 0))
			SIMPLEQ_REMOVE_HEAD(&icp->icp_ucmd_queue, ic_chain);
		else
			SIMPLEQ_REMOVE_HEAD(&icp->icp_ccb_queue, ic_chain);
	}

	splx(s);
}

int
icp_ccb_map(struct icp_softc *icp, struct icp_ccb *ic, void *data, int size,
	    int dir)
{
	struct icp_sg *sg;
	int nsegs, i, rv;
	bus_dmamap_t xfer;

	xfer = ic->ic_xfer_map;

	rv = bus_dmamap_load(icp->icp_dmat, xfer, data, size, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((dir & IC_XFER_IN) ? BUS_DMA_READ : BUS_DMA_WRITE));
	if (rv != 0)
		return (rv);

	nsegs = xfer->dm_nsegs;
	ic->ic_xfer_size = size;
	ic->ic_nsgent = nsegs;
	ic->ic_flags |= dir;
	sg = ic->ic_sg;

	if (sg != NULL) {
		for (i = 0; i < nsegs; i++, sg++) {
			sg->sg_addr = htole32(xfer->dm_segs[i].ds_addr);
			sg->sg_len = htole32(xfer->dm_segs[i].ds_len);
		}
	} else if (nsegs > 1)
		panic("icp_ccb_map: no SG list specified, but nsegs > 1");

	if ((dir & IC_XFER_OUT) != 0)
		i = BUS_DMASYNC_PREWRITE;
	else /* if ((dir & IC_XFER_IN) != 0) */
		i = BUS_DMASYNC_PREREAD;

	bus_dmamap_sync(icp->icp_dmat, xfer, 0, ic->ic_xfer_size, i);
	return (0);
}

void
icp_ccb_unmap(struct icp_softc *icp, struct icp_ccb *ic)
{
	int i;

	if ((ic->ic_flags & IC_XFER_OUT) != 0)
		i = BUS_DMASYNC_POSTWRITE;
	else /* if ((ic->ic_flags & IC_XFER_IN) != 0) */
		i = BUS_DMASYNC_POSTREAD;

	bus_dmamap_sync(icp->icp_dmat, ic->ic_xfer_map, 0, ic->ic_xfer_size, i);
	bus_dmamap_unload(icp->icp_dmat, ic->ic_xfer_map);
}

int
icp_ccb_poll(struct icp_softc *icp, struct icp_ccb *ic, int timo)
{
	int s, rv;

	s = splbio();

	for (timo = ICP_BUSY_WAIT_MS * 100; timo != 0; timo--) {
		if (!(*icp->icp_test_busy)(icp))
			break;
		DELAY(10);
	}
	if (timo == 0) {
		printf("%s: submit: busy\n", device_xname(icp->icp_dv));
		return (EAGAIN);
	}

	icp_ccb_submit(icp, ic);

	if (cold) {
		for (timo *= 10; timo != 0; timo--) {
			DELAY(100);
			icp_intr(icp);
			if ((ic->ic_flags & IC_COMPLETE) != 0)
				break;
		}
	} else {
		ic->ic_flags |= IC_WAITING;
		while ((ic->ic_flags & IC_COMPLETE) == 0) {
			if ((rv = tsleep(ic, PRIBIO, "icpwccb",
					 mstohz(timo))) != 0) {
				timo = 0;
				break;
			}
		}
	}

	if (timo != 0) {
		if (ic->ic_status != ICP_S_OK) {
#ifdef ICP_DEBUG
			printf("%s: request failed; status=0x%04x\n",
			    device_xname(icp->icp_dv), ic->ic_status);
#endif
			rv = EIO;
		} else
			rv = 0;
	} else {
		aprint_error_dev(icp->icp_dv, "command timed out\n");
		rv = EIO;
	}

	while ((*icp->icp_test_busy)(icp) != 0)
		DELAY(10);

	splx(s);

	return (rv);
}

int
icp_ccb_wait(struct icp_softc *icp, struct icp_ccb *ic, int timo)
{
	int s, rv;

	ic->ic_flags |= IC_WAITING;

	s = splbio();
	icp_ccb_enqueue(icp, ic);
	while ((ic->ic_flags & IC_COMPLETE) == 0) {
		if ((rv = tsleep(ic, PRIBIO, "icpwccb", mstohz(timo))) != 0) {
			splx(s);
			return (rv);
		}
	}
	splx(s);

	if (ic->ic_status != ICP_S_OK) {
		aprint_error_dev(icp->icp_dv, "command failed; status=%x\n",
		    ic->ic_status);
		return (EIO);
	}

	return (0);
}

int
icp_ccb_wait_user(struct icp_softc *icp, struct icp_ccb *ic, int timo)
{
	int s, rv;

	ic->ic_dv = icp->icp_dv;
	ic->ic_intr = icp_ucmd_intr;
	ic->ic_flags |= IC_UCMD;

	s = splbio();
	icp_ccb_enqueue(icp, ic);
	while ((ic->ic_flags & IC_COMPLETE) == 0) {
		if ((rv = tsleep(ic, PRIBIO, "icpwuccb", mstohz(timo))) != 0) {
			splx(s);
			return (rv);
		}
	}
	splx(s);

	return (0);
}

void
icp_ccb_submit(struct icp_softc *icp, struct icp_ccb *ic)
{

	ic->ic_cmdlen = (ic->ic_cmdlen + 3) & ~3;

	(*icp->icp_set_sema0)(icp);
	DELAY(10);

	ic->ic_cmd.cmd_boardnode = htole32(ICP_LOCALBOARD);
	ic->ic_cmd.cmd_cmdindex = htole32(ic->ic_ident);

	icp->icp_running++;

	(*icp->icp_copy_cmd)(icp, ic);
	(*icp->icp_release_event)(icp, ic);
}

int
icp_freeze(struct icp_softc *icp)
{
	int s, error = 0;

	s = splbio();
	if (icp->icp_qfreeze++ == 0) {
		while (icp->icp_running != 0) {
			icp->icp_flags |= ICP_F_WAIT_FREEZE;
			error = tsleep(&icp->icp_qfreeze, PRIBIO|PCATCH,
			    "icpqfrz", 0);
			if (error != 0 && --icp->icp_qfreeze == 0 &&
			    ICP_HAS_WORK(icp)) {
				icp_ccb_enqueue(icp, NULL);
				break;
			}
		}
	}
	splx(s);

	return (error);
}

void
icp_unfreeze(struct icp_softc *icp)
{
	int s;

	s = splbio();
	KDASSERT(icp->icp_qfreeze != 0);
	if (--icp->icp_qfreeze == 0 && ICP_HAS_WORK(icp))
		icp_ccb_enqueue(icp, NULL);
	splx(s);
}

/* XXX Global - should be per-controller? XXX */
static gdt_evt_str icp_event_buffer[ICP_MAX_EVENTS];
static int icp_event_oldidx;
static int icp_event_lastidx;

gdt_evt_str *
icp_store_event(struct icp_softc *icp, u_int16_t source, u_int16_t idx,
    gdt_evt_data *evt)
{
	gdt_evt_str *e;

	/* no source == no event */
	if (source == 0)
		return (NULL);

	e = &icp_event_buffer[icp_event_lastidx];
	if (e->event_source == source && e->event_idx == idx &&
	    ((evt->size != 0 && e->event_data.size != 0 &&
	      memcmp(&e->event_data.eu, &evt->eu, evt->size) == 0) ||
	     (evt->size == 0 && e->event_data.size == 0 &&
	      strcmp((char *) e->event_data.event_string,
	      	     (char *) evt->event_string) == 0))) {
		e->last_stamp = time_second;
		e->same_count++;
	} else {
		if (icp_event_buffer[icp_event_lastidx].event_source != 0) {
			icp_event_lastidx++;
			if (icp_event_lastidx == ICP_MAX_EVENTS)
				icp_event_lastidx = 0;
			if (icp_event_lastidx == icp_event_oldidx) {
				icp_event_oldidx++;
				if (icp_event_oldidx == ICP_MAX_EVENTS)
					icp_event_oldidx = 0;
			}
		}
		e = &icp_event_buffer[icp_event_lastidx];
		e->event_source = source;
		e->event_idx = idx;
		e->first_stamp = e->last_stamp = time_second;
		e->same_count = 1;
		e->event_data = *evt;
		e->application = 0;
	}
	return (e);
}

int
icp_read_event(struct icp_softc *icp, int handle, gdt_evt_str *estr)
{
	gdt_evt_str *e;
	int eindex, s;

	s = splbio();

	if (handle == -1)
		eindex = icp_event_oldidx;
	else
		eindex = handle;

	estr->event_source = 0;

	if (eindex < 0 || eindex >= ICP_MAX_EVENTS) {
		splx(s);
		return (eindex);
	}

	e = &icp_event_buffer[eindex];
	if (e->event_source != 0) {
		if (eindex != icp_event_lastidx) {
			eindex++;
			if (eindex == ICP_MAX_EVENTS)
				eindex = 0;
		} else
			eindex = -1;
		memcpy(estr, e, sizeof(gdt_evt_str));
	}

	splx(s);

	return (eindex);
}

void
icp_readapp_event(struct icp_softc *icp, u_int8_t application,
    gdt_evt_str *estr)
{
	gdt_evt_str *e;
	int found = 0, eindex, s;

	s = splbio();

	eindex = icp_event_oldidx;
	for (;;) {
		e = &icp_event_buffer[eindex];
		if (e->event_source == 0)
			break;
		if ((e->application & application) == 0) {
			e->application |= application;
			found = 1;
			break;
		}
		if (eindex == icp_event_lastidx)
			break;
		eindex++;
		if (eindex == ICP_MAX_EVENTS)
			eindex = 0;
	}
	if (found)
		memcpy(estr, e, sizeof(gdt_evt_str));
	else
		estr->event_source = 0;

	splx(s);
}

void
icp_clear_events(struct icp_softc *icp)
{
	int s;

	s = splbio();
	icp_event_oldidx = icp_event_lastidx = 0;
	memset(icp_event_buffer, 0, sizeof(icp_event_buffer));
	splx(s);
}
