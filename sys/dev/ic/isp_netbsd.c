/* $NetBSD: isp_netbsd.c,v 1.88 2014/12/31 17:10:45 christos Exp $ */
/*
 * Platform (NetBSD) dependent common attachment code for Qlogic adapters.
 */
/*
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isp_netbsd.c,v 1.88 2014/12/31 17:10:45 christos Exp $");

#include <dev/ic/isp_netbsd.h>
#include <dev/ic/isp_ioctl.h>
#include <sys/scsiio.h>

#include <sys/timevar.h>

/*
 * Set a timeout for the watchdogging of a command.
 *
 * The dimensional analysis is
 *
 *	milliseconds * (seconds/millisecond) * (ticks/second) = ticks
 *
 *			=
 *
 *	(milliseconds / 1000) * hz = ticks
 *
 *
 * For timeouts less than 1 second, we'll get zero. Because of this, and
 * because we want to establish *our* timeout to be longer than what the
 * firmware might do, we just add 3 seconds at the back end.
 */
#define	_XT(xs)	((((xs)->timeout/1000) * hz) + (3 * hz))

static void isp_config_interrupts(device_t);
static void ispminphys_1020(struct buf *);
static void ispminphys(struct buf *);
static void ispcmd(struct ispsoftc *, XS_T *);
static void isprequest(struct scsipi_channel *, scsipi_adapter_req_t, void *);
static int
ispioctl(struct scsipi_channel *, u_long, void *, int, struct proc *);

static void isp_polled_cmd_wait(struct ispsoftc *, XS_T *);
static void isp_dog(void *);
static void isp_gdt(void *);
static void isp_ldt(void *);
static void isp_make_here(ispsoftc_t *, int);
static void isp_make_gone(ispsoftc_t *, int);
static void isp_fc_worker(void *);

static const char *roles[4] = {
    "(none)", "Target", "Initiator", "Target/Initiator"
};
static const char prom3[] =
    "PortID 0x%06x Departed from Target %u because of %s";
int isp_change_is_bad = 0;	/* "changed" devices are bad */
int isp_quickboot_time = 15;	/* don't wait more than N secs for loop up */
static int isp_fabric_hysteresis = 5;
#define	isp_change_is_bad	0

/*
 * Complete attachment of hardware, include subdevices.
 */

void
isp_attach(struct ispsoftc *isp)
{
	device_t self = isp->isp_osinfo.dev;
	int i;

	isp->isp_state = ISP_RUNSTATE;

	isp->isp_osinfo.adapter.adapt_dev = self;
	isp->isp_osinfo.adapter.adapt_openings = isp->isp_maxcmds;
	isp->isp_osinfo.loop_down_limit = 300;

	/*
	 * It's not stated whether max_periph is limited by SPI
	 * tag uage, but let's assume that it is.
	 */
	isp->isp_osinfo.adapter.adapt_max_periph = min(isp->isp_maxcmds, 255);
	isp->isp_osinfo.adapter.adapt_ioctl = ispioctl;
	isp->isp_osinfo.adapter.adapt_request = isprequest;
	if (isp->isp_type <= ISP_HA_SCSI_1020A) {
		isp->isp_osinfo.adapter.adapt_minphys = ispminphys_1020;
	} else {
		isp->isp_osinfo.adapter.adapt_minphys = ispminphys;
	}

	callout_init(&isp->isp_osinfo.gdt, 0);
	callout_setfunc(&isp->isp_osinfo.gdt, isp_gdt, isp);
	callout_init(&isp->isp_osinfo.ldt, 0);
	callout_setfunc(&isp->isp_osinfo.ldt, isp_ldt, isp);
	if (IS_FC(isp)) {
		if (kthread_create(PRI_NONE, 0, NULL, isp_fc_worker, isp,
		    &isp->isp_osinfo.thread, "%s:fc_thrd",
		    device_xname(self))) {
			isp_prt(isp, ISP_LOGERR,
			    "unable to create FC worker thread");
			return;
		}
	}

	for (i = 0; i != isp->isp_osinfo.adapter.adapt_nchannels; i++) {
		isp->isp_osinfo.chan[i].chan_adapter =
		    &isp->isp_osinfo.adapter;
		isp->isp_osinfo.chan[i].chan_bustype = &scsi_bustype;
		isp->isp_osinfo.chan[i].chan_channel = i;
		/*
		 * Until the midlayer is fixed to use REPORT LUNS,
		 * limit to 8 luns.
		 */
		isp->isp_osinfo.chan[i].chan_nluns = min(isp->isp_maxluns, 8);
		if (IS_FC(isp)) {
			isp->isp_osinfo.chan[i].chan_ntargets = MAX_FC_TARG;
			if (ISP_CAP_2KLOGIN(isp) == 0 && MAX_FC_TARG > 256) {
				isp->isp_osinfo.chan[i].chan_ntargets = 256;
			}
			isp->isp_osinfo.chan[i].chan_id = MAX_FC_TARG;
		} else {
			isp->isp_osinfo.chan[i].chan_ntargets = MAX_TARGETS;
			isp->isp_osinfo.chan[i].chan_id =
			    SDPARAM(isp, i)->isp_initiator_id;
			ISP_LOCK(isp);
			(void) isp_control(isp, ISPCTL_RESET_BUS, i);
			ISP_UNLOCK(isp);
		}
	}

	/*
         * Defer enabling mailbox interrupts until later.
         */
        config_interrupts(self, isp_config_interrupts);
}

static void
isp_config_interrupts(device_t self)
{
	int i;
        struct ispsoftc *isp = device_private(self);

        isp->isp_osinfo.mbox_sleep_ok = 1;

	if (IS_FC(isp) && (FCPARAM(isp, 0)->isp_fwstate != FW_READY ||
	    FCPARAM(isp, 0)->isp_loopstate != LOOP_READY)) {
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		   "Starting Initial Loop Down Timer");
		callout_schedule(&isp->isp_osinfo.ldt, isp_quickboot_time * hz);
	}

	/*
	 * And attach children (if any).
	 */
	for (i = 0; i < isp->isp_osinfo.adapter.adapt_nchannels; i++) {
		config_found(self, &isp->isp_osinfo.chan[i], scsiprint);
	}
}

/*
 * minphys our xfers
 */
static void
ispminphys_1020(struct buf *bp)
{
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
	minphys(bp);
}

static void
ispminphys(struct buf *bp)
{
	if (bp->b_bcount >= (1 << 30)) {
		bp->b_bcount = (1 << 30);
	}
	minphys(bp);
}

static int
ispioctl(struct scsipi_channel *chan, u_long cmd, void *addr, int flag,
	struct proc *p)
{
	struct ispsoftc *isp = device_private(chan->chan_adapter->adapt_dev);
	int nr, bus, retval = ENOTTY;

	switch (cmd) {
	case ISP_SDBLEV:
	{
		int olddblev = isp->isp_dblev;
		isp->isp_dblev = *(int *)addr;
		*(int *)addr = olddblev;
		retval = 0;
		break;
	}
	case ISP_GETROLE:
		bus = *(int *)addr;
		if (bus < 0 || bus >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}
		if (IS_FC(isp)) {
			*(int *)addr = FCPARAM(isp, bus)->role;
		} else {
			*(int *)addr = SDPARAM(isp, bus)->role;
		}
		retval = 0;
		break;
	case ISP_SETROLE:

		nr = *(int *)addr;
		bus = nr >> 8;
		if (bus < 0 || bus >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}
		nr &= 0xff;
		if (nr & ~(ISP_ROLE_INITIATOR|ISP_ROLE_TARGET)) {
			retval = EINVAL;
			break;
		}
		if (IS_FC(isp)) {
			*(int *)addr = FCPARAM(isp, bus)->role;
			FCPARAM(isp, bus)->role = nr;
		} else {
			*(int *)addr = SDPARAM(isp, bus)->role;
			SDPARAM(isp, bus)->role = nr;
		}
		retval = 0;
		break;

	case ISP_RESETHBA:
		ISP_LOCK(isp);
		isp_reinit(isp, 0);
		ISP_UNLOCK(isp);
		retval = 0;
		break;

	case ISP_RESCAN:
		if (IS_FC(isp)) {
			bus = *(int *)addr;
			if (bus < 0 || bus >= isp->isp_nchan) {
				retval = -ENXIO;
				break;
			}
			ISP_LOCK(isp);
			if (isp_fc_runstate(isp, bus, 5 * 1000000)) {
				retval = EIO;
			} else {
				retval = 0;
			}
			ISP_UNLOCK(isp);
		}
		break;

	case ISP_FC_LIP:
		if (IS_FC(isp)) {
			bus = *(int *)addr;
			if (bus < 0 || bus >= isp->isp_nchan) {
				retval = -ENXIO;
				break;
			}
			ISP_LOCK(isp);
			if (isp_control(isp, ISPCTL_SEND_LIP, bus)) {
				retval = EIO;
			} else {
				retval = 0;
			}
			ISP_UNLOCK(isp);
		}
		break;
	case ISP_FC_GETDINFO:
	{
		struct isp_fc_device *ifc = (struct isp_fc_device *) addr;
		fcportdb_t *lp;

		if (IS_SCSI(isp)) {
			break;
		}
		if (ifc->loopid >= MAX_FC_TARG) {
			retval = EINVAL;
			break;
		}
		lp = &FCPARAM(isp, ifc->chan)->portdb[ifc->loopid];
		if (lp->state == FC_PORTDB_STATE_VALID) {
			ifc->role = lp->roles;
			ifc->loopid = lp->handle;
			ifc->portid = lp->portid;
			ifc->node_wwn = lp->node_wwn;
			ifc->port_wwn = lp->port_wwn;
			retval = 0;
		} else {
			retval = ENODEV;
		}
		break;
	}
	case ISP_GET_STATS:
	{
		isp_stats_t *sp = (isp_stats_t *) addr;

		ISP_MEMZERO(sp, sizeof (*sp));
		sp->isp_stat_version = ISP_STATS_VERSION;
		sp->isp_type = isp->isp_type;
		sp->isp_revision = isp->isp_revision;
		ISP_LOCK(isp);
		sp->isp_stats[ISP_INTCNT] = isp->isp_intcnt;
		sp->isp_stats[ISP_INTBOGUS] = isp->isp_intbogus;
		sp->isp_stats[ISP_INTMBOXC] = isp->isp_intmboxc;
		sp->isp_stats[ISP_INGOASYNC] = isp->isp_intoasync;
		sp->isp_stats[ISP_RSLTCCMPLT] = isp->isp_rsltccmplt;
		sp->isp_stats[ISP_FPHCCMCPLT] = isp->isp_fphccmplt;
		sp->isp_stats[ISP_RSCCHIWAT] = isp->isp_rscchiwater;
		sp->isp_stats[ISP_FPCCHIWAT] = isp->isp_fpcchiwater;
		ISP_UNLOCK(isp);
		retval = 0;
		break;
	}
	case ISP_CLR_STATS:
		ISP_LOCK(isp);
		isp->isp_intcnt = 0;
		isp->isp_intbogus = 0;
		isp->isp_intmboxc = 0;
		isp->isp_intoasync = 0;
		isp->isp_rsltccmplt = 0;
		isp->isp_fphccmplt = 0;
		isp->isp_rscchiwater = 0;
		isp->isp_fpcchiwater = 0;
		ISP_UNLOCK(isp);
		retval = 0;
		break;
	case ISP_FC_GETHINFO:
	{
		struct isp_hba_device *hba = (struct isp_hba_device *) addr;
		bus = hba->fc_channel;

		if (bus < 0 || bus >= isp->isp_nchan) {
			retval = ENXIO;
			break;
		}
		hba->fc_fw_major = ISP_FW_MAJORX(isp->isp_fwrev);
		hba->fc_fw_minor = ISP_FW_MINORX(isp->isp_fwrev);
		hba->fc_fw_micro = ISP_FW_MICROX(isp->isp_fwrev);
		hba->fc_nchannels = isp->isp_nchan;
		hba->fc_nports = isp->isp_nchan;/* XXXX 24XX STUFF? XXX */
		if (IS_FC(isp)) {
			hba->fc_speed = FCPARAM(isp, bus)->isp_gbspeed;
			hba->fc_topology = FCPARAM(isp, bus)->isp_topo + 1;
			hba->fc_loopid = FCPARAM(isp, bus)->isp_loopid;
			hba->nvram_node_wwn = FCPARAM(isp, bus)->isp_wwnn_nvram;
			hba->nvram_port_wwn = FCPARAM(isp, bus)->isp_wwpn_nvram;
			hba->active_node_wwn = FCPARAM(isp, bus)->isp_wwnn;
			hba->active_port_wwn = FCPARAM(isp, bus)->isp_wwpn;
		} else {
			hba->fc_speed = 0;
			hba->fc_topology = 0;
			hba->nvram_node_wwn = 0ull;
			hba->nvram_port_wwn = 0ull;
			hba->active_node_wwn = 0ull;
			hba->active_port_wwn = 0ull;
		}
		retval = 0;
		break;
	}
	case ISP_TSK_MGMT:
	{
		int needmarker;
		struct isp_fc_tsk_mgmt *fct = (struct isp_fc_tsk_mgmt *) addr;
		uint16_t loopid;
		mbreg_t mbs;

		if (IS_SCSI(isp)) {
			break;
		}

		bus = fct->chan;
		if (bus < 0 || bus >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}

		memset(&mbs, 0, sizeof (mbs));
		needmarker = retval = 0;
		loopid = fct->loopid;
		if (ISP_CAP_2KLOGIN(isp) == 0) {
			loopid <<= 8;
		}
		switch (fct->action) {
		case IPT_CLEAR_ACA:
			mbs.param[0] = MBOX_CLEAR_ACA;
			mbs.param[1] = loopid;
			mbs.param[2] = fct->lun;
			break;
		case IPT_TARGET_RESET:
			mbs.param[0] = MBOX_TARGET_RESET;
			mbs.param[1] = loopid;
			needmarker = 1;
			break;
		case IPT_LUN_RESET:
			mbs.param[0] = MBOX_LUN_RESET;
			mbs.param[1] = loopid;
			mbs.param[2] = fct->lun;
			needmarker = 1;
			break;
		case IPT_CLEAR_TASK_SET:
			mbs.param[0] = MBOX_CLEAR_TASK_SET;
			mbs.param[1] = loopid;
			mbs.param[2] = fct->lun;
			needmarker = 1;
			break;
		case IPT_ABORT_TASK_SET:
			mbs.param[0] = MBOX_ABORT_TASK_SET;
			mbs.param[1] = loopid;
			mbs.param[2] = fct->lun;
			needmarker = 1;
			break;
		default:
			retval = EINVAL;
			break;
		}
		if (retval == 0) {
			if (needmarker) {
				FCPARAM(isp, bus)->sendmarker = 1;
			}
			ISP_LOCK(isp);
			retval = isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
			ISP_UNLOCK(isp);
			if (retval) {
				retval = EIO;
			}
		}
		break;
	}
	case ISP_FC_GETDLIST:
	{
		isp_dlist_t local, *ua;
		uint16_t nph, nphe, count, channel, lim;
		struct wwnpair pair, *uptr;

		if (IS_SCSI(isp)) {
			retval = EINVAL;
			break;
		}

		ua = *(isp_dlist_t **)addr;
		if (copyin(ua, &local, sizeof (isp_dlist_t))) {
			retval = EFAULT;
			break;
		}
		lim = local.count;
		channel = local.channel;

		ua = *(isp_dlist_t **)addr;
		uptr = &ua->wwns[0];

		if (ISP_CAP_2KLOGIN(isp)) {
			nphe = NPH_MAX_2K;
		} else {
			nphe = NPH_MAX;
		}
		for (count = 0, nph = 0; count < lim && nph != nphe; nph++) {
			ISP_LOCK(isp);
			retval = isp_control(isp, ISPCTL_GET_NAMES, channel,
			    nph, &pair.wwnn, &pair.wwpn);
			ISP_UNLOCK(isp);
			if (retval || (pair.wwpn == INI_NONE &&
			    pair.wwnn == INI_NONE)) {
				retval = 0;
				continue;
			}
			if (copyout(&pair, (void *)uptr++, sizeof (pair))) {
				retval = EFAULT;
				break;
			}
			count++;
		}
		if (retval == 0) {
			if (copyout(&count, (void *)&ua->count,
			    sizeof (count))) {
				retval = EFAULT;
			}
		}
		break;
	}
	case SCBUSIORESET:
		ISP_LOCK(isp);
		if (isp_control(isp, ISPCTL_RESET_BUS, &chan->chan_channel)) {
			retval = EIO;
		} else {
			retval = 0;
		}
		ISP_UNLOCK(isp);
		break;
	default:
		break;
	}
	return (retval);
}

static void
ispcmd(struct ispsoftc *isp, XS_T *xs)
{
	volatile uint8_t ombi;
	int lim, chan;

	ISP_LOCK(isp);
	if (isp->isp_state < ISP_RUNSTATE) {
		ISP_DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			ISP_ENABLE_INTS(isp);
			ISP_UNLOCK(isp);
			isp_prt(isp, ISP_LOGERR, "isp not at init state");
			XS_SETERR(xs, HBA_BOTCH);
			scsipi_done(xs);
			return;
		}
		isp->isp_state = ISP_RUNSTATE;
		ISP_ENABLE_INTS(isp);
	}
	chan = XS_CHANNEL(xs);

	/*
	 * Handle the case of a FC card where the FC thread hasn't
	 * fired up yet and we don't yet have a known loop state.
	 */
	if (IS_FC(isp) && (FCPARAM(isp, chan)->isp_fwstate != FW_READY ||
	    FCPARAM(isp, chan)->isp_loopstate != LOOP_READY) &&
	    isp->isp_osinfo.thread == NULL) {
		ombi = isp->isp_osinfo.mbox_sleep_ok != 0;
		int delay_time;

		if (xs->xs_control & XS_CTL_POLL) {
			isp->isp_osinfo.mbox_sleep_ok = 0;
		}

		if (isp->isp_osinfo.loop_checked == 0) {
			delay_time = 10 * 1000000;
			isp->isp_osinfo.loop_checked = 1;
		} else {
			delay_time = 250000;
		}

		if (isp_fc_runstate(isp,  XS_CHANNEL(xs), delay_time) != 0) {
			if (xs->xs_control & XS_CTL_POLL) {
				isp->isp_osinfo.mbox_sleep_ok = ombi;
			}
			if (FCPARAM(isp, XS_CHANNEL(xs))->loop_seen_once == 0) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				scsipi_done(xs);
				ISP_UNLOCK(isp);
				return;
			}
			/*
			 * Otherwise, fall thru to be queued up for later.
			 */
		} else {
			int wasblocked =
			    (isp->isp_osinfo.blocked || isp->isp_osinfo.paused);
			isp->isp_osinfo.blocked = isp->isp_osinfo.paused = 0;
			if (wasblocked) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "THAW QUEUES @ LINE %d", __LINE__);
				scsipi_channel_thaw(&isp->isp_osinfo.chan[chan],
				    1);
			}
		}
		if (xs->xs_control & XS_CTL_POLL) {
			isp->isp_osinfo.mbox_sleep_ok = ombi;
		}
	}

	if (isp->isp_osinfo.paused) {
		isp_prt(isp, ISP_LOGWARN, "I/O while paused");
		xs->error = XS_RESOURCE_SHORTAGE;
		scsipi_done(xs);
		ISP_UNLOCK(isp);
		return;
	}
	if (isp->isp_osinfo.blocked) {
		isp_prt(isp, ISP_LOGWARN,
		    "I/O while blocked with retries %d", xs, xs->xs_retries);
		if (xs->xs_retries) {
			xs->error = XS_REQUEUE;
			xs->xs_retries--;
		} else {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		scsipi_done(xs);
		ISP_UNLOCK(isp);
		return;
	}

	if (xs->xs_control & XS_CTL_POLL) {
		ombi = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 0;
	}

	switch (isp_start(xs)) {
	case CMD_QUEUED:
		if (IS_FC(isp) && isp->isp_osinfo.wwns[XS_TGT(xs)] == 0) {
			fcparam *fcp = FCPARAM(isp, XS_CHANNEL(xs));
			int dbidx = fcp->isp_dev_map[XS_TGT(xs)] - 1;
			device_t dev = xs->xs_periph->periph_dev;
			
			if (dbidx >= 0 && dev &&
			    prop_dictionary_set_uint64(device_properties(dev),
			    "port-wwn", fcp->portdb[dbidx].port_wwn) == TRUE) {
				isp->isp_osinfo.wwns[XS_TGT(xs)] =
				    fcp->portdb[dbidx].port_wwn;
			}
                }
		if (xs->xs_control & XS_CTL_POLL) {
			isp_polled_cmd_wait(isp, xs);
			isp->isp_osinfo.mbox_sleep_ok = ombi;
		} else if (xs->timeout) {
			callout_reset(&xs->xs_callout, _XT(xs), isp_dog, xs);
		}
		break;
	case CMD_EAGAIN:
		isp->isp_osinfo.paused = 1;
		xs->error = XS_RESOURCE_SHORTAGE;
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "FREEZE QUEUES @ LINE %d", __LINE__);
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			scsipi_channel_freeze(&isp->isp_osinfo.chan[chan], 1);
		}
		scsipi_done(xs);
		break;
	case CMD_RQLATER:
		/*
		 * We can only get RQLATER from FC devices (1 channel only)
		 *
		 * If we've never seen loop up see if if we've been down
		 * quickboot time, otherwise wait loop down limit time.
		 * If so, then we start giving up on commands.
		 */
		if (FCPARAM(isp, XS_CHANNEL(xs))->loop_seen_once == 0) {
			lim = isp_quickboot_time;
		} else {
			lim = isp->isp_osinfo.loop_down_limit;
		}
		if (isp->isp_osinfo.loop_down_time >= lim) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "RQLATER->SELTIMEOUT for %d (%d >= %d)", XS_TGT(xs),
			    isp->isp_osinfo.loop_down_time, lim);
			XS_SETERR(xs, HBA_SELTIMEOUT);
			scsipi_done(xs);
			break;
		}
		if (isp->isp_osinfo.blocked == 0) {
			isp->isp_osinfo.blocked = 1;
			scsipi_channel_freeze(&isp->isp_osinfo.chan[chan], 1);
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "FREEZE QUEUES @ LINE %d", __LINE__);
		} else {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "RQLATER WITH FROZEN QUEUES @ LINE %d", __LINE__);
		}
		xs->error = XS_REQUEUE;
		scsipi_done(xs);
		break;
	case CMD_COMPLETE:
		scsipi_done(xs);
		break;
	}
	ISP_UNLOCK(isp);
}

static void
isprequest(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct ispsoftc *isp = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		ispcmd(isp, (XS_T *) arg);
		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* Not supported. */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
	if (IS_SCSI(isp)) {
		struct scsipi_xfer_mode *xm = arg;
		int dflags = 0;
		sdparam *sdp = SDPARAM(isp, chan->chan_channel);

		if (xm->xm_mode & PERIPH_CAP_TQING)
			dflags |= DPARM_TQING;
		if (xm->xm_mode & PERIPH_CAP_WIDE16)
			dflags |= DPARM_WIDE;
		if (xm->xm_mode & PERIPH_CAP_SYNC)
			dflags |= DPARM_SYNC;
		ISP_LOCK(isp);
		sdp->isp_devparam[xm->xm_target].goal_flags |= dflags;
		dflags = sdp->isp_devparam[xm->xm_target].goal_flags;
		sdp->isp_devparam[xm->xm_target].dev_update = 1;
		sdp->update = 1;
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "isprequest: device flags 0x%x for %d.%d.X",
		    dflags, chan->chan_channel, xm->xm_target);
		break;
	}
	default:
		break;
	}
}

static void
isp_polled_cmd_wait(struct ispsoftc *isp, XS_T *xs)
{
	int infinite = 0, mswait;

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if ((mswait = XS_TIME(xs)) == 0) {
		infinite = 1;
	}

	while (mswait || infinite) {
		uint32_t isr;
		uint16_t sema, mbox;
		if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
			isp_intr(isp, isr, sema, mbox);
			if (XS_CMD_DONE_P(xs)) {
				break;
			}
		}
		ISP_DELAY(1000);
		mswait -= 1;
	}

	/*
	 * If no other error occurred but we didn't finish
	 * something bad happened, so abort the command.
	 */
	if (XS_CMD_DONE_P(xs) == 0) {
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			isp_reinit(isp, 0);
		}
		if (XS_NOERR(xs)) {
			isp_prt(isp, ISP_LOGERR, "polled command timed out");
			XS_SETERR(xs, HBA_BOTCH);
		}
	}
	scsipi_done(xs);
}

void
isp_done(XS_T *xs)
{
	if (XS_CMD_WDOG_P(xs) == 0) {
		struct ispsoftc *isp = XS_ISP(xs);
		callout_stop(&xs->xs_callout);
		if (XS_CMD_GRACE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "finished command on borrowed time");
		}
		XS_CMD_S_CLEAR(xs);
		/*
		 * Fixup- if we get a QFULL, we need
		 * to set XS_BUSY as the error.
		 */
		if (xs->status == SCSI_QUEUE_FULL) {
			xs->error = XS_BUSY;
		}
		if (isp->isp_osinfo.paused) {
			int i;
			isp->isp_osinfo.paused = 0;
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "THAW QUEUES @ LINE %d", __LINE__);
			for (i = 0; i < isp->isp_nchan; i++) {
				scsipi_channel_timed_thaw(&isp->isp_osinfo.chan[i]);
			}
		}
		if (xs->error == XS_DRIVER_STUFFUP) {
			isp_prt(isp, ISP_LOGERR,
			    "BOTCHED cmd for %d.%d.%d cmd 0x%x datalen %ld",
			    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs),
			    XS_CDBP(xs)[0], (long) XS_XFRLEN(xs));
		}
		scsipi_done(xs);
	}
}

static void
isp_dog(void *arg)
{
	XS_T *xs = arg;
	struct ispsoftc *isp = XS_ISP(xs);
	uint32_t handle;
	int sok;


	ISP_ILOCK(isp);
	sok = isp->isp_osinfo.mbox_sleep_ok;
	isp->isp_osinfo.mbox_sleep_ok = 0;
	/*
	 * We've decided this command is dead. Make sure we're not trying
	 * to kill a command that's already dead by getting its handle and
	 * and seeing whether it's still alive.
	 */
	handle = isp_find_handle(isp, xs);
	if (handle) {
		uint32_t isr;
		uint16_t mbox, sema;

		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "watchdog found done cmd (handle 0x%x)", handle);
			goto out;
		}

		if (XS_CMD_WDOG_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "recursive watchdog (handle 0x%x)", handle);
			goto out;
		}

		XS_CMD_S_WDOG(xs);

		if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
			isp_intr(isp, isr, sema, mbox);

		}
		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "watchdog cleanup for handle 0x%x", handle);
			XS_CMD_C_WDOG(xs);
			isp_done(xs);
		} else if (XS_CMD_GRACE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "watchdog timeout for handle 0x%x", handle);
			/*
			 * Make sure the command is *really* dead before we
			 * release the handle (and DMA resources) for reuse.
			 */
			(void) isp_control(isp, ISPCTL_ABORT_CMD, arg);

			/*
			 * After this point, the command is really dead.
			 */
			if (XS_XFRLEN(xs)) {
				ISP_DMAFREE(isp, xs, handle);
			}
			isp_destroy_handle(isp, handle);
			XS_SETERR(xs, XS_TIMEOUT);
			XS_CMD_S_CLEAR(xs);
			isp_done(xs);
		} else {
			void *qe;
			isp_marker_t local, *mp = &local;
			isp_prt(isp, ISP_LOGDEBUG2,
			    "possible command timeout on handle %x", handle);
			XS_CMD_C_WDOG(xs);
			callout_reset(&xs->xs_callout, hz, isp_dog, xs);
			qe = isp_getrqentry(isp);
			if (qe == NULL)
				goto out;
			XS_CMD_S_GRACE(xs);
			ISP_MEMZERO((void *) mp, sizeof (*mp));
			mp->mrk_header.rqs_entry_count = 1;
			mp->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
			mp->mrk_modifier = SYNC_ALL;
			mp->mrk_target = XS_CHANNEL(xs) << 7;
			isp_put_marker(isp, mp, qe);
			ISP_SYNC_REQUEST(isp);
		}
	} else {
		isp_prt(isp, ISP_LOGDEBUG0, "watchdog with no command");
	}
out:
	isp->isp_osinfo.mbox_sleep_ok = sok;
	ISP_IUNLOCK(isp);
}

/*
 * Gone Device Timer Function- when we have decided that a device has gone
 * away, we wait a specific period of time prior to telling the OS it has
 * gone away.
 *
 * This timer function fires once a second and then scans the port database
 * for devices that are marked dead but still have a virtual target assigned.
 * We decrement a counter for that port database entry, and when it hits zero,
 * we tell the OS the device has gone away.
 */
static void
isp_gdt(void *arg)
{
	ispsoftc_t *isp = arg;
	fcportdb_t *lp;
	int dbidx, tgt, more_to_do = 0;

	isp_prt(isp, ISP_LOGDEBUG0, "GDT timer expired");
	ISP_LOCK(isp);
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp, 0)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_ZOMBIE) {
			continue;
		}
		if (lp->dev_map_idx == 0) {
			continue;
		}
		if (lp->new_reserved == 0) {
			continue;
		}
		lp->new_reserved -= 1;
		if (lp->new_reserved != 0) {
			more_to_do++;
			continue;
		}
		tgt = lp->dev_map_idx - 1;
		FCPARAM(isp, 0)->isp_dev_map[tgt] = 0;
		lp->dev_map_idx = 0;
		lp->state = FC_PORTDB_STATE_NIL;
		isp_prt(isp, ISP_LOGCONFIG, prom3, lp->portid, tgt,
		    "Gone Device Timeout");
		isp_make_gone(isp, tgt);
	}
	if (more_to_do) {
		callout_schedule(&isp->isp_osinfo.gdt, hz);
	} else {
		isp->isp_osinfo.gdt_running = 0;
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "stopping Gone Device Timer");
	}
	ISP_UNLOCK(isp);
}

/*
 * Loop Down Timer Function- when loop goes down, a timer is started and
 * and after it expires we come here and take all probational devices that
 * the OS knows about and the tell the OS that they've gone away.
 * 
 * We don't clear the devices out of our port database because, when loop
 * come back up, we have to do some actual cleanup with the chip at that
 * point (implicit PLOGO, e.g., to get the chip's port database state right).
 */
static void
isp_ldt(void *arg)
{
	ispsoftc_t *isp = arg;
	fcportdb_t *lp;
	int dbidx, tgt;

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "Loop Down Timer expired");
	ISP_LOCK(isp);

	/*
	 * Notify to the OS all targets who we now consider have departed.
	 */
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp, 0)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_PROBATIONAL) {
			continue;
		}
		if (lp->dev_map_idx == 0) {
			continue;
		}

		/*
		 * XXX: CLEAN UP AND COMPLETE ANY PENDING COMMANDS FIRST!
		 */

		/*
		 * Mark that we've announced that this device is gone....
		 */
		lp->reserved = 1;

		/*
		 * but *don't* change the state of the entry. Just clear
		 * any target id stuff and announce to CAM that the
		 * device is gone. This way any necessary PLOGO stuff
		 * will happen when loop comes back up.
		 */

		tgt = lp->dev_map_idx - 1;
		FCPARAM(isp, 0)->isp_dev_map[tgt] = 0;
		lp->dev_map_idx = 0;
		isp_prt(isp, ISP_LOGCONFIG, prom3, lp->portid, tgt,
		    "Loop Down Timeout");
		isp_make_gone(isp, tgt);
	}

	/*
	 * The loop down timer has expired. Wake up the kthread
	 * to notice that fact (or make it false).
	 */
	isp->isp_osinfo.loop_down_time = isp->isp_osinfo.loop_down_limit+1;
	wakeup(&isp->isp_osinfo.thread);
	ISP_UNLOCK(isp);
}

static void
isp_make_here(ispsoftc_t *isp, int tgt)
{
	isp_prt(isp, ISP_LOGINFO, "target %d has arrived", tgt);
}

static void
isp_make_gone(ispsoftc_t *isp, int tgt)
{
	isp_prt(isp, ISP_LOGINFO, "target %d has departed", tgt);
}

static void
isp_fc_worker(void *arg)
{
	void scsipi_run_queue(struct scsipi_channel *);
	ispsoftc_t *isp = arg;
	int slp = 0;
	int chan = 0;

	int s = splbio();
	/*
	 * The first loop is for our usage where we have yet to have
	 * gotten good fibre channel state.
	 */
	while (isp->isp_osinfo.thread != NULL) {
		int sok, lb, lim;

		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "checking FC state");
		sok = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 1;
		lb = isp_fc_runstate(isp, chan, 250000);
		isp->isp_osinfo.mbox_sleep_ok = sok;
		if (lb) {
			/*
			 * Increment loop down time by the last sleep interval
			 */
			isp->isp_osinfo.loop_down_time += slp;

			if (lb < 0) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "FC loop not up (down count %d)",
				    isp->isp_osinfo.loop_down_time);
			} else {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "FC got to %d (down count %d)",
				    lb, isp->isp_osinfo.loop_down_time);
			}


			/*
			 * If we've never seen loop up and we've waited longer
			 * than quickboot time, or we've seen loop up but we've
			 * waited longer than loop_down_limit, give up and go
			 * to sleep until loop comes up.
			 */
			if (FCPARAM(isp, 0)->loop_seen_once == 0) {
				lim = isp_quickboot_time;
			} else {
				lim = isp->isp_osinfo.loop_down_limit;
			}
			if (isp->isp_osinfo.loop_down_time >= lim) {
				/*
				 * If we're now past our limit, release
				 * the queues and let them come in and
				 * either get HBA_SELTIMOUT or cause
				 * another freeze.
				 */
				isp->isp_osinfo.blocked = 1;
				slp = 0;
			} else if (isp->isp_osinfo.loop_down_time < 10) {
				slp = 1;
			} else if (isp->isp_osinfo.loop_down_time < 30) {
				slp = 5;
			} else if (isp->isp_osinfo.loop_down_time < 60) {
				slp = 10;
			} else if (isp->isp_osinfo.loop_down_time < 120) {
				slp = 20;
			} else {
				slp = 30;
			}

		} else {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "FC state OK");
			isp->isp_osinfo.loop_down_time = 0;
			slp = 0;
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "THAW QUEUES @ LINE %d", __LINE__);
			scsipi_channel_thaw(&isp->isp_osinfo.chan[chan], 1);
		}

		/*
		 * If we'd frozen the queues, unfreeze them now so that
		 * we can start getting commands. If the FC state isn't
		 * okay yet, they'll hit that in isp_start which will
		 * freeze the queues again.
		 */
		if (isp->isp_osinfo.blocked) {
			isp->isp_osinfo.blocked = 0;
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "THAW QUEUES @ LINE %d", __LINE__);
			scsipi_channel_thaw(&isp->isp_osinfo.chan[chan], 1);
		}
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "sleep time %d", slp);
		tsleep(&isp->isp_osinfo.thread, PRIBIO, "ispf", slp * hz);

		/*
		 * If slp is zero, we're waking up for the first time after
		 * things have been okay. In this case, we set a deferral state
		 * for all commands and delay hysteresis seconds before starting
		 * the FC state evaluation. This gives the loop/fabric a chance
		 * to settle.
		 */
		if (slp == 0 && isp_fabric_hysteresis) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "sleep hysteresis tick time %d",
			    isp_fabric_hysteresis * hz);
			(void) tsleep(&isp_fabric_hysteresis, PRIBIO, "ispT",
			    (isp_fabric_hysteresis * hz));
		}
	}
	splx(s);

	/* In case parent is waiting for us to exit. */
	wakeup(&isp->isp_osinfo.thread);
	kthread_exit(0);
}

/*
 * Free any associated resources prior to decommissioning and
 * set the card to a known state (so it doesn't wake up and kick
 * us when we aren't expecting it to).
 *
 * Locks are held before coming here.
 */
void
isp_uninit(struct ispsoftc *isp)
{
	isp_lock(isp);
	/*
	 * Leave with interrupts disabled.
	 */
	ISP_DISABLE_INTS(isp);
	isp_unlock(isp);
}

void
isp_async(struct ispsoftc *isp, ispasync_t cmd, ...)
{
	int bus, tgt;
	const char *msg = NULL;
	static const char prom[] =
	    "PortID 0x%06x handle 0x%x role %s %s\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	static const char prom2[] =
	    "PortID 0x%06x handle 0x%x role %s %s tgt %u\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	fcportdb_t *lp;
	va_list ap;

	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	if (IS_SCSI(isp)) {
		sdparam *sdp;
		int flags;
		struct scsipi_xfer_mode xm;

		va_start(ap, cmd);
		bus = va_arg(ap, int);
		tgt = va_arg(ap, int);
		va_end(ap);
		sdp = SDPARAM(isp, bus);
		flags = sdp->isp_devparam[tgt].actv_flags;

		xm.xm_mode = 0;
		xm.xm_period = sdp->isp_devparam[tgt].actv_period;
		xm.xm_offset = sdp->isp_devparam[tgt].actv_offset;
		xm.xm_target = tgt;

		if ((flags & DPARM_SYNC) && xm.xm_period && xm.xm_offset)
			xm.xm_mode |= PERIPH_CAP_SYNC;
		if (flags & DPARM_WIDE)
			xm.xm_mode |= PERIPH_CAP_WIDE16;
		if (flags & DPARM_TQING)
			xm.xm_mode |= PERIPH_CAP_TQING;
		scsipi_async_event(&isp->isp_osinfo.chan[bus],
		    ASYNC_EVENT_XFER_MODE, &xm);
		break;
	}
	case ISPASYNC_BUS_RESET:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);
		isp_prt(isp, ISP_LOGINFO, "SCSI bus %d reset detected", bus);
		scsipi_async_event(&isp->isp_osinfo.chan[bus],
		    ASYNC_EVENT_RESET, NULL);
		break;
	case ISPASYNC_LIP:
		if (msg == NULL) {
			msg = "LIP Received";
		}
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_RESET:
		if (msg == NULL) {
			msg = "LOOP Reset Received";
		}
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_DOWN:
		if (msg == NULL) {
			msg = "Loop DOWN";
		}
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);

		/*
		 * Don't do queue freezes or blockage until we have the
		 * thread running and interrupts that can unfreeze/unblock us.
		 */
		if (isp->isp_osinfo.mbox_sleep_ok &&
		    isp->isp_osinfo.blocked == 0 &&
		    isp->isp_osinfo.thread) {
			isp->isp_osinfo.blocked = 1;
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "FREEZE QUEUES @ LINE %d", __LINE__);
			scsipi_channel_freeze(&isp->isp_osinfo.chan[bus], 1);
			if (callout_pending(&isp->isp_osinfo.ldt) == 0) {
				callout_schedule(&isp->isp_osinfo.ldt,
				    isp->isp_osinfo.loop_down_limit * hz);
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				   "Starting Loop Down Timer");
			}
		}
		isp_prt(isp, ISP_LOGINFO, msg);
		break;
        case ISPASYNC_LOOP_UP:
		/*
		 * Let the subsequent ISPASYNC_CHANGE_NOTIFY invoke
		 * the FC worker thread. When the FC worker thread
		 * is done, let *it* call scsipi_channel_thaw...
		 */
		isp_prt(isp, ISP_LOGINFO, "Loop UP");
		break;
	case ISPASYNC_DEV_ARRIVED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		lp->reserved = 0;
		if ((FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) &&
		    (lp->roles & (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT))) {
			int dbidx = lp - FCPARAM(isp, bus)->portdb;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				if (i >= FL_ID && i <= SNS_ID) {
					continue;
				}
				if (FCPARAM(isp, bus)->isp_dev_map[i] == 0) {
					break;
				}
			}
			if (i < MAX_FC_TARG) {
				FCPARAM(isp, bus)->isp_dev_map[i] = dbidx + 1;
				lp->dev_map_idx = i + 1;
			} else {
				isp_prt(isp, ISP_LOGWARN, "out of target ids");
				isp_dump_portdb(isp, bus);
			}
		}
		if (lp->dev_map_idx) {
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
			isp_make_here(isp, tgt);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived",
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_CHANGED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		if (isp_change_is_bad) {
			lp->state = FC_PORTDB_STATE_NIL;
			if (lp->dev_map_idx) {
				tgt = lp->dev_map_idx - 1;
				FCPARAM(isp, bus)->isp_dev_map[tgt] = 0;
				lp->dev_map_idx = 0;
				isp_prt(isp, ISP_LOGCONFIG, prom3,
				    lp->portid, tgt, "change is bad");
				isp_make_gone(isp, tgt);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles],
				    "changed and departed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		} else {
			lp->portid = lp->new_portid;
			lp->roles = lp->new_roles;
			if (lp->dev_map_idx) {
				int t = lp->dev_map_idx - 1;
				FCPARAM(isp, bus)->isp_dev_map[t] =
				    (lp - FCPARAM(isp, bus)->portdb) + 1;
				tgt = lp->dev_map_idx - 1;
				isp_prt(isp, ISP_LOGCONFIG, prom2,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed at", tgt,
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		}
		break;
	case ISPASYNC_DEV_STAYED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		if (lp->dev_map_idx) {
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed at", tgt,
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_GONE:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		/*
		 * If this has a virtual target and we haven't marked it
		 * that we're going to have isp_gdt tell the OS it's gone,
		 * set the isp_gdt timer running on it.
		 *
		 * If it isn't marked that isp_gdt is going to get rid of it,
		 * announce that it's gone.
		 */
		if (lp->dev_map_idx && lp->reserved == 0) {
			lp->reserved = 1;
			lp->new_reserved = isp->isp_osinfo.gone_device_time;
			lp->state = FC_PORTDB_STATE_ZOMBIE;
			if (isp->isp_osinfo.gdt_running == 0) {
				isp->isp_osinfo.gdt_running = 1;
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "starting Gone Device Timer");
				callout_schedule(&isp->isp_osinfo.gdt, hz);
			}
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "gone zombie at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else if (lp->reserved == 0) {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
			    roles[lp->roles], "departed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
			    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_CHANGE_NOTIFY:
	{
		int opt;

		va_start(ap, cmd);
		bus = va_arg(ap, int);
		opt = va_arg(ap, int);
		va_end(ap);

		if (opt == ISPASYNC_CHANGE_PDB) {
			msg = "Port Database Changed";
		} else if (opt == ISPASYNC_CHANGE_SNS) {
			msg = "Name Server Database Changed";
		} else {
			msg = "Other Change Notify";
		}
		/*
		 * If the loop down timer is running, cancel it.
		 */
		if (callout_pending(&isp->isp_osinfo.ldt)) {
			callout_stop(&isp->isp_osinfo.ldt);
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			   "Stopping Loop Down Timer");
		}
		isp_prt(isp, ISP_LOGINFO, msg);
		/*
		 * We can set blocked here because we know it's now okay
		 * to try and run isp_fc_runstate (in order to build loop
		 * state). But we don't try and freeze the midlayer's queue
		 * if we have no thread that we can wake to later unfreeze
		 * it.
		 */
		if (isp->isp_osinfo.blocked == 0) {
			isp->isp_osinfo.blocked = 1;
			if (isp->isp_osinfo.thread) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "FREEZE QUEUES @ LINE %d", __LINE__);
				scsipi_channel_freeze(&isp->isp_osinfo.chan[bus], 1);
			}
		}
		/*
		 * Note that we have work for the thread to do, and
		 * if the thread is here already, wake it up.
		 */
		if (isp->isp_osinfo.thread) {
			wakeup(&isp->isp_osinfo.thread);
		} else {
			isp_prt(isp, ISP_LOGDEBUG1, "no FC thread yet");
		}
		break;
	}
	case ISPASYNC_FW_CRASH:
	{
		uint16_t mbox1;
		mbox1 = ISP_READ(isp, OUTMAILBOX1);
		if (IS_DUALBUS(isp)) {
			bus = ISP_READ(isp, OUTMAILBOX6);
		} else {
			bus = 0;
		}
                isp_prt(isp, ISP_LOGERR,
                    "Internal Firmware Error on bus %d @ RISC Address 0x%x",
                    bus, mbox1);
		if (IS_FC(isp)) {
			if (isp->isp_osinfo.blocked == 0) {
				isp->isp_osinfo.blocked = 1;
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "FREEZE QUEUES @ LINE %d", __LINE__);
				scsipi_channel_freeze(&isp->isp_osinfo.chan[bus], 1);
			}
		}
		mbox1 = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp_reinit(isp, 0);
		isp->isp_osinfo.mbox_sleep_ok = mbox1;
		isp_async(isp, ISPASYNC_FW_RESTARTED, NULL);
		break;
	}
	default:
		break;
	}
}

void
isp_prt(struct ispsoftc *isp, int level, const char *fmt, ...)
{
	va_list ap;
	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	printf("%s: ", device_xname(isp->isp_osinfo.dev));
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void
isp_xs_prt(struct ispsoftc *isp, XS_T *xs, int level, const char *fmt, ...)
{
	va_list ap;
	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	scsipi_printaddr(xs->xs_periph);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void
isp_lock(struct ispsoftc *isp)
{
	int s = splbio();
	if (isp->isp_osinfo.islocked++ == 0) {
		isp->isp_osinfo.splsaved = s;
	} else {
		splx(s);
	}
}

void
isp_unlock(struct ispsoftc *isp)
{
	if (isp->isp_osinfo.islocked-- <= 1) {
		isp->isp_osinfo.islocked = 0;
		splx(isp->isp_osinfo.splsaved);
	}
}

uint64_t
isp_microtime_sub(struct timeval *b, struct timeval *a)
{
	struct timeval x;
	uint64_t elapsed;
	timersub(b, a, &x);
	elapsed = GET_NANOSEC(&x);
	if (elapsed == 0)
		elapsed++;
	return (elapsed);
}

int
isp_mbox_acquire(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mboxbsy) {
		return (1);
	} else {
		isp->isp_osinfo.mboxcmd_done = 0;
		isp->isp_osinfo.mboxbsy = 1;
		return (0);
	}
}

void
isp_mbox_wait_complete(struct ispsoftc *isp, mbreg_t *mbp)
{
	unsigned int usecs = mbp->timeout;
	unsigned int maxc, olim, ilim;
	struct timeval start;

	if (usecs == 0) {
		usecs = MBCMD_DEFAULT_TIMEOUT;
	}
	maxc = isp->isp_mbxwrk0 + 1;

	microtime(&start);
	if (isp->isp_osinfo.mbox_sleep_ok) {
		int to;
		struct timeval tv, utv;

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		for (olim = 0; olim < maxc; olim++) {
			utv.tv_sec = usecs / 1000000;
			utv.tv_usec = usecs % 1000000;
			timeradd(&tv, &utv, &tv);
		}
		to = tvtohz(&tv);
		if (to == 0)
			to = 1;
		timeradd(&tv, &start, &tv);

		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp->isp_osinfo.mbox_sleeping = 1;
		tsleep(&isp->isp_mbxworkp, PRIBIO, "ispmbx_sleep", to);
		isp->isp_osinfo.mbox_sleeping = 0;
		isp->isp_osinfo.mbox_sleep_ok = 1;
	} else {
		for (olim = 0; olim < maxc; olim++) {
			for (ilim = 0; ilim < usecs; ilim += 100) {
				uint32_t isr;
				uint16_t sema, mbox;
				if (isp->isp_osinfo.mboxcmd_done) {
					break;
				}
				if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
					isp_intr(isp, isr, sema, mbox);
					if (isp->isp_osinfo.mboxcmd_done) {
						break;
					}
				}
				ISP_DELAY(100);
			}
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
	}
	if (isp->isp_osinfo.mboxcmd_done == 0) {
		struct timeval finish, elapsed;

		microtime(&finish);
		timersub(&finish, &start, &elapsed);
		isp_prt(isp, ISP_LOGWARN,
		    "%s Mailbox Command (0x%x) Timeout (%uus actual)",
		    isp->isp_osinfo.mbox_sleep_ok? "Interrupting" : "Polled",
		    isp->isp_lastmbxcmd, (elapsed.tv_sec * 1000000) +
		    elapsed.tv_usec);
		mbp->param[0] = MBOX_TIMEOUT;
		isp->isp_osinfo.mboxcmd_done = 1;
	}
}

void
isp_mbox_notify_done(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mbox_sleeping) {
		wakeup(&isp->isp_mbxworkp);
	}
	isp->isp_osinfo.mboxcmd_done = 1;
}

void
isp_mbox_release(ispsoftc_t *isp)
{
	isp->isp_osinfo.mboxbsy = 0;
}
