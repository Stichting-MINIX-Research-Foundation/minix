/*	$NetBSD: rf_reconstruct.c,v 1.121 2014/11/14 14:29:16 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/************************************************************
 *
 * rf_reconstruct.c -- code to perform on-line reconstruction
 *
 ************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_reconstruct.c,v 1.121 2014/11/14 14:29:16 oster Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/namei.h> /* for pathbuf */
#include <dev/raidframe/raidframevar.h>

#include <miscfs/specfs/specdev.h> /* for v_rdev */

#include "rf_raid.h"
#include "rf_reconutil.h"
#include "rf_revent.h"
#include "rf_reconbuffer.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_dag.h"
#include "rf_desc.h"
#include "rf_debugprint.h"
#include "rf_general.h"
#include "rf_driver.h"
#include "rf_utils.h"
#include "rf_shutdown.h"

#include "rf_kintf.h"

/* setting these to -1 causes them to be set to their default values if not set by debug options */

#if RF_DEBUG_RECON
#define Dprintf(s)         if (rf_reconDebug) rf_debug_printf(s,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf1(s,a)         if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)
#define Dprintf4(s,a,b,c,d)   if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),NULL,NULL,NULL,NULL)
#define Dprintf5(s,a,b,c,d,e) if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),NULL,NULL,NULL)
#define Dprintf6(s,a,b,c,d,e,f) if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),(void *)((unsigned long)f),NULL,NULL)
#define Dprintf7(s,a,b,c,d,e,f,g) if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),(void *)((unsigned long)f),(void *)((unsigned long)g),NULL)

#define DDprintf1(s,a)         if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define DDprintf2(s,a,b)       if (rf_reconDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)

#else /* RF_DEBUG_RECON */

#define Dprintf(s) {}
#define Dprintf1(s,a) {}
#define Dprintf2(s,a,b) {}
#define Dprintf3(s,a,b,c) {}
#define Dprintf4(s,a,b,c,d) {}
#define Dprintf5(s,a,b,c,d,e) {}
#define Dprintf6(s,a,b,c,d,e,f) {}
#define Dprintf7(s,a,b,c,d,e,f,g) {}

#define DDprintf1(s,a) {}
#define DDprintf2(s,a,b) {}

#endif /* RF_DEBUG_RECON */

#define RF_RECON_DONE_READS   1
#define RF_RECON_READ_ERROR   2
#define RF_RECON_WRITE_ERROR  3
#define RF_RECON_READ_STOPPED 4
#define RF_RECON_WRITE_DONE   5

#define RF_MAX_FREE_RECONBUFFER 32
#define RF_MIN_FREE_RECONBUFFER 16

static RF_RaidReconDesc_t *AllocRaidReconDesc(RF_Raid_t *, RF_RowCol_t,
					      RF_RaidDisk_t *, int, RF_RowCol_t);
static void FreeReconDesc(RF_RaidReconDesc_t *);
static int ProcessReconEvent(RF_Raid_t *, RF_ReconEvent_t *);
static int IssueNextReadRequest(RF_Raid_t *, RF_RowCol_t);
static int TryToRead(RF_Raid_t *, RF_RowCol_t);
static int ComputePSDiskOffsets(RF_Raid_t *, RF_StripeNum_t, RF_RowCol_t,
				RF_SectorNum_t *, RF_SectorNum_t *, RF_RowCol_t *,
				RF_SectorNum_t *);
static int IssueNextWriteRequest(RF_Raid_t *);
static int ReconReadDoneProc(void *, int);
static int ReconWriteDoneProc(void *, int);
static void CheckForNewMinHeadSep(RF_Raid_t *, RF_HeadSepLimit_t);
static int CheckHeadSeparation(RF_Raid_t *, RF_PerDiskReconCtrl_t *,
			       RF_RowCol_t, RF_HeadSepLimit_t,
			       RF_ReconUnitNum_t);
static int CheckForcedOrBlockedReconstruction(RF_Raid_t *,
					      RF_ReconParityStripeStatus_t *,
					      RF_PerDiskReconCtrl_t *,
					      RF_RowCol_t, RF_StripeNum_t,
					      RF_ReconUnitNum_t);
static void ForceReconReadDoneProc(void *, int);
static void rf_ShutdownReconstruction(void *);

struct RF_ReconDoneProc_s {
	void    (*proc) (RF_Raid_t *, void *);
	void   *arg;
	RF_ReconDoneProc_t *next;
};

/**************************************************************************
 *
 * sets up the parameters that will be used by the reconstruction process
 * currently there are none, except for those that the layout-specific
 * configuration (e.g. rf_ConfigureDeclustered) routine sets up.
 *
 * in the kernel, we fire off the recon thread.
 *
 **************************************************************************/
static void
rf_ShutdownReconstruction(void *ignored)
{
	pool_destroy(&rf_pools.reconbuffer);
}

int
rf_ConfigureReconstruction(RF_ShutdownList_t **listp)
{

	rf_pool_init(&rf_pools.reconbuffer, sizeof(RF_ReconBuffer_t),
		     "rf_reconbuffer_pl", RF_MIN_FREE_RECONBUFFER, RF_MAX_FREE_RECONBUFFER);
	rf_ShutdownCreate(listp, rf_ShutdownReconstruction, NULL);

	return (0);
}

static RF_RaidReconDesc_t *
AllocRaidReconDesc(RF_Raid_t *raidPtr, RF_RowCol_t col,
		   RF_RaidDisk_t *spareDiskPtr, int numDisksDone,
		   RF_RowCol_t scol)
{

	RF_RaidReconDesc_t *reconDesc;

	RF_Malloc(reconDesc, sizeof(RF_RaidReconDesc_t),
		  (RF_RaidReconDesc_t *));
	reconDesc->raidPtr = raidPtr;
	reconDesc->col = col;
	reconDesc->spareDiskPtr = spareDiskPtr;
	reconDesc->numDisksDone = numDisksDone;
	reconDesc->scol = scol;
	reconDesc->next = NULL;

	return (reconDesc);
}

static void
FreeReconDesc(RF_RaidReconDesc_t *reconDesc)
{
#if RF_RECON_STATS > 0
	printf("raid%d: %lu recon event waits, %lu recon delays\n",
	       reconDesc->raidPtr->raidid,
	       (long) reconDesc->numReconEventWaits,
	       (long) reconDesc->numReconExecDelays);
#endif				/* RF_RECON_STATS > 0 */
	printf("raid%d: %lu max exec ticks\n",
	       reconDesc->raidPtr->raidid,
	       (long) reconDesc->maxReconExecTicks);
	RF_Free(reconDesc, sizeof(RF_RaidReconDesc_t));
}


/*****************************************************************************
 *
 * primary routine to reconstruct a failed disk.  This should be called from
 * within its own thread.  It won't return until reconstruction completes,
 * fails, or is aborted.
 *****************************************************************************/
int
rf_ReconstructFailedDisk(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	const RF_LayoutSW_t *lp;
	int     rc;

	lp = raidPtr->Layout.map;
	if (lp->SubmitReconBuffer) {
		/*
	         * The current infrastructure only supports reconstructing one
	         * disk at a time for each array.
	         */
		rf_lock_mutex2(raidPtr->mutex);
		while (raidPtr->reconInProgress) {
			rf_wait_cond2(raidPtr->waitForReconCond, raidPtr->mutex);
		}
		raidPtr->reconInProgress++;
		rf_unlock_mutex2(raidPtr->mutex);
		rc = rf_ReconstructFailedDiskBasic(raidPtr, col);
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->reconInProgress--;
	} else {
		RF_ERRORMSG1("RECON: no way to reconstruct failed disk for arch %c\n",
		    lp->parityConfig);
		rc = EIO;
		rf_lock_mutex2(raidPtr->mutex);
	}
	rf_signal_cond2(raidPtr->waitForReconCond);
	rf_unlock_mutex2(raidPtr->mutex);
	return (rc);
}

int
rf_ReconstructFailedDiskBasic(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_ComponentLabel_t *c_label;
	RF_RaidDisk_t *spareDiskPtr = NULL;
	RF_RaidReconDesc_t *reconDesc;
	RF_RowCol_t scol;
	int     numDisksDone = 0, rc;

	/* first look for a spare drive onto which to reconstruct the data */
	/* spare disk descriptors are stored in row 0.  This may have to
	 * change eventually */

	rf_lock_mutex2(raidPtr->mutex);
	RF_ASSERT(raidPtr->Disks[col].status == rf_ds_failed);
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		if (raidPtr->status != rf_rs_degraded) {
			RF_ERRORMSG1("Unable to reconstruct disk at col %d because status not degraded\n", col);
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		scol = (-1);
	} else {
#endif
		for (scol = raidPtr->numCol; scol < raidPtr->numCol + raidPtr->numSpare; scol++) {
			if (raidPtr->Disks[scol].status == rf_ds_spare) {
				spareDiskPtr = &raidPtr->Disks[scol];
				spareDiskPtr->status = rf_ds_rebuilding_spare;
				break;
			}
		}
		if (!spareDiskPtr) {
			RF_ERRORMSG1("Unable to reconstruct disk at col %d because no spares are available\n", col);
			rf_unlock_mutex2(raidPtr->mutex);
			return (ENOSPC);
		}
		printf("RECON: initiating reconstruction on col %d -> spare at col %d\n", col, scol);
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	}
#endif
	rf_unlock_mutex2(raidPtr->mutex);

	reconDesc = AllocRaidReconDesc((void *) raidPtr, col, spareDiskPtr, numDisksDone, scol);
	raidPtr->reconDesc = (void *) reconDesc;
#if RF_RECON_STATS > 0
	reconDesc->hsStallCount = 0;
	reconDesc->numReconExecDelays = 0;
	reconDesc->numReconEventWaits = 0;
#endif				/* RF_RECON_STATS > 0 */
	reconDesc->reconExecTimerRunning = 0;
	reconDesc->reconExecTicks = 0;
	reconDesc->maxReconExecTicks = 0;
	rc = rf_ContinueReconstructFailedDisk(reconDesc);

	if (!rc) {
		/* fix up the component label */
		/* Don't actually need the read here.. */
		c_label = raidget_component_label(raidPtr, scol);

		raid_init_component_label(raidPtr, c_label);
		c_label->row = 0;
		c_label->column = col;
		c_label->clean = RF_RAID_DIRTY;
		c_label->status = rf_ds_optimal;
		rf_component_label_set_partitionsize(c_label,
		    raidPtr->Disks[scol].partitionSize);

		/* We've just done a rebuild based on all the other
		   disks, so at this point the parity is known to be
		   clean, even if it wasn't before. */

		/* XXX doesn't hold for RAID 6!!*/

		rf_lock_mutex2(raidPtr->mutex);
		/* The failed disk has already been marked as rf_ds_spared 
		   (or rf_ds_dist_spared) in
		   rf_ContinueReconstructFailedDisk() 
		   so we just update the spare disk as being a used spare
		*/

		spareDiskPtr->status = rf_ds_used_spare;
		raidPtr->parity_good = RF_RAID_CLEAN;
		rf_unlock_mutex2(raidPtr->mutex);

		/* XXXX MORE NEEDED HERE */

		raidflush_component_label(raidPtr, scol);
	} else {
		/* Reconstruct failed. */

		rf_lock_mutex2(raidPtr->mutex);
		/* Failed disk goes back to "failed" status */
		raidPtr->Disks[col].status = rf_ds_failed;

		/* Spare disk goes back to "spare" status. */
		spareDiskPtr->status = rf_ds_spare;
		rf_unlock_mutex2(raidPtr->mutex);

	}
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	return (rc);
}

/*

   Allow reconstructing a disk in-place -- i.e. component /dev/sd2e goes AWOL,
   and you don't get a spare until the next Monday.  With this function
   (and hot-swappable drives) you can now put your new disk containing
   /dev/sd2e on the bus, scsictl it alive, and then use raidctl(8) to
   rebuild the data "on the spot".

*/

int
rf_ReconstructInPlace(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_RaidDisk_t *spareDiskPtr = NULL;
	RF_RaidReconDesc_t *reconDesc;
	const RF_LayoutSW_t *lp;
	RF_ComponentLabel_t *c_label;
	int     numDisksDone = 0, rc;
	uint64_t numsec;
	unsigned int secsize;
	struct pathbuf *pb;
	struct vnode *vp;
	int retcode;
	int ac;

	rf_lock_mutex2(raidPtr->mutex);
	lp = raidPtr->Layout.map;
	if (!lp->SubmitReconBuffer) {
		RF_ERRORMSG1("RECON: no way to reconstruct failed disk for arch %c\n",
			     lp->parityConfig);
		/* wakeup anyone who might be waiting to do a reconstruct */
		rf_signal_cond2(raidPtr->waitForReconCond);
		rf_unlock_mutex2(raidPtr->mutex);
		return(EIO);
	}

	/*
	 * The current infrastructure only supports reconstructing one
	 * disk at a time for each array.
	 */

	if (raidPtr->Disks[col].status != rf_ds_failed) {
		/* "It's gone..." */
		raidPtr->numFailures++;
		raidPtr->Disks[col].status = rf_ds_failed;
		raidPtr->status = rf_rs_degraded;
		rf_unlock_mutex2(raidPtr->mutex);
		rf_update_component_labels(raidPtr,
					   RF_NORMAL_COMPONENT_UPDATE);
		rf_lock_mutex2(raidPtr->mutex);
	}

	while (raidPtr->reconInProgress) {
		rf_wait_cond2(raidPtr->waitForReconCond, raidPtr->mutex);
	}

	raidPtr->reconInProgress++;

	/* first look for a spare drive onto which to reconstruct the
	   data.  spare disk descriptors are stored in row 0.  This
	   may have to change eventually */

	/* Actually, we don't care if it's failed or not...  On a RAID
	   set with correct parity, this function should be callable
	   on any component without ill effects. */
	/* RF_ASSERT(raidPtr->Disks[col].status == rf_ds_failed); */

#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		RF_ERRORMSG1("Unable to reconstruct to disk at col %d: operation not supported for RF_DISTRIBUTE_SPARE\n", col);

		raidPtr->reconInProgress--;
		rf_signal_cond2(raidPtr->waitForReconCond);
		rf_unlock_mutex2(raidPtr->mutex);
		return (EINVAL);
	}
#endif

	/* This device may have been opened successfully the
	   first time. Close it before trying to open it again.. */

	if (raidPtr->raid_cinfo[col].ci_vp != NULL) {
#if 0
		printf("Closed the open device: %s\n",
		       raidPtr->Disks[col].devname);
#endif
		vp = raidPtr->raid_cinfo[col].ci_vp;
		ac = raidPtr->Disks[col].auto_configured;
		rf_unlock_mutex2(raidPtr->mutex);
		rf_close_component(raidPtr, vp, ac);
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->raid_cinfo[col].ci_vp = NULL;
	}
	/* note that this disk was *not* auto_configured (any longer)*/
	raidPtr->Disks[col].auto_configured = 0;

#if 0
	printf("About to (re-)open the device for rebuilding: %s\n",
	       raidPtr->Disks[col].devname);
#endif
	rf_unlock_mutex2(raidPtr->mutex);
	pb = pathbuf_create(raidPtr->Disks[col].devname);
	if (pb == NULL) {
		retcode = ENOMEM;
	} else {
		retcode = dk_lookup(pb, curlwp, &vp);
		pathbuf_destroy(pb);
	}

	if (retcode) {
		printf("raid%d: rebuilding: dk_lookup on device: %s failed: %d!\n",raidPtr->raidid,
		       raidPtr->Disks[col].devname, retcode);

		/* the component isn't responding properly...
		   must be still dead :-( */
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->reconInProgress--;
		rf_signal_cond2(raidPtr->waitForReconCond);
		rf_unlock_mutex2(raidPtr->mutex);
		return(retcode);
	}

	/* Ok, so we can at least do a lookup...
	   How about actually getting a vp for it? */

	retcode = getdisksize(vp, &numsec, &secsize);
	if (retcode) {
		vn_close(vp, FREAD | FWRITE, kauth_cred_get());
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->reconInProgress--;
		rf_signal_cond2(raidPtr->waitForReconCond);
		rf_unlock_mutex2(raidPtr->mutex);
		return(retcode);
	}
	rf_lock_mutex2(raidPtr->mutex);
	raidPtr->Disks[col].blockSize =	secsize;
	raidPtr->Disks[col].numBlocks = numsec - rf_protectedSectors;

	raidPtr->raid_cinfo[col].ci_vp = vp;
	raidPtr->raid_cinfo[col].ci_dev = vp->v_rdev;

	raidPtr->Disks[col].dev = vp->v_rdev;

	/* we allow the user to specify that only a fraction
	   of the disks should be used this is just for debug:
	   it speeds up * the parity scan */
	raidPtr->Disks[col].numBlocks = raidPtr->Disks[col].numBlocks *
		rf_sizePercentage / 100;
	rf_unlock_mutex2(raidPtr->mutex);

	spareDiskPtr = &raidPtr->Disks[col];
	spareDiskPtr->status = rf_ds_rebuilding_spare;

	printf("raid%d: initiating in-place reconstruction on column %d\n",
	       raidPtr->raidid, col);

	reconDesc = AllocRaidReconDesc((void *) raidPtr, col, spareDiskPtr,
				       numDisksDone, col);
	raidPtr->reconDesc = (void *) reconDesc;
#if RF_RECON_STATS > 0
	reconDesc->hsStallCount = 0;
	reconDesc->numReconExecDelays = 0;
	reconDesc->numReconEventWaits = 0;
#endif				/* RF_RECON_STATS > 0 */
	reconDesc->reconExecTimerRunning = 0;
	reconDesc->reconExecTicks = 0;
	reconDesc->maxReconExecTicks = 0;
	rc = rf_ContinueReconstructFailedDisk(reconDesc);

	if (!rc) {
		rf_lock_mutex2(raidPtr->mutex);
		/* Need to set these here, as at this point it'll be claiming
		   that the disk is in rf_ds_spared!  But we know better :-) */

		raidPtr->Disks[col].status = rf_ds_optimal;
		raidPtr->status = rf_rs_optimal;
		rf_unlock_mutex2(raidPtr->mutex);

		/* fix up the component label */
		/* Don't actually need the read here.. */
		c_label = raidget_component_label(raidPtr, col);

		rf_lock_mutex2(raidPtr->mutex);
		raid_init_component_label(raidPtr, c_label);

		c_label->row = 0;
		c_label->column = col;

		/* We've just done a rebuild based on all the other
		   disks, so at this point the parity is known to be
		   clean, even if it wasn't before. */

		/* XXX doesn't hold for RAID 6!!*/

		raidPtr->parity_good = RF_RAID_CLEAN;
		rf_unlock_mutex2(raidPtr->mutex);

		raidflush_component_label(raidPtr, col);
	} else {
		/* Reconstruct-in-place failed.  Disk goes back to
		   "failed" status, regardless of what it was before.  */
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->Disks[col].status = rf_ds_failed;
		rf_unlock_mutex2(raidPtr->mutex);
	}

	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);

	rf_lock_mutex2(raidPtr->mutex);
	raidPtr->reconInProgress--;
	rf_signal_cond2(raidPtr->waitForReconCond);
	rf_unlock_mutex2(raidPtr->mutex);

	return (rc);
}


int
rf_ContinueReconstructFailedDisk(RF_RaidReconDesc_t *reconDesc)
{
	RF_Raid_t *raidPtr = reconDesc->raidPtr;
	RF_RowCol_t col = reconDesc->col;
	RF_RowCol_t scol = reconDesc->scol;
	RF_ReconMap_t *mapPtr;
	RF_ReconCtrl_t *tmp_reconctrl;
	RF_ReconEvent_t *event;
	RF_StripeCount_t incPSID,lastPSID,num_writes,pending_writes,prev;
#if RF_INCLUDE_RAID5_RS > 0
	RF_StripeCount_t startPSID,endPSID,aPSID,bPSID,offPSID;
#endif
	RF_ReconUnitCount_t RUsPerPU;
	struct timeval etime, elpsd;
	unsigned long xor_s, xor_resid_us;
	int     i, ds;
	int status, done;
	int recon_error, write_error;

	raidPtr->accumXorTimeUs = 0;
#if RF_ACC_TRACE > 0
	/* create one trace record per physical disk */
	RF_Malloc(raidPtr->recon_tracerecs, raidPtr->numCol * sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
#endif

	/* quiesce the array prior to starting recon.  this is needed
	 * to assure no nasty interactions with pending user writes.
	 * We need to do this before we change the disk or row status. */

	Dprintf("RECON: begin request suspend\n");
	rf_SuspendNewRequestsAndWait(raidPtr);
	Dprintf("RECON: end request suspend\n");

	/* allocate our RF_ReconCTRL_t before we protect raidPtr->reconControl[row] */
	tmp_reconctrl = rf_MakeReconControl(reconDesc, col, scol);

	rf_lock_mutex2(raidPtr->mutex);

	/* create the reconstruction control pointer and install it in
	 * the right slot */
	raidPtr->reconControl = tmp_reconctrl;
	mapPtr = raidPtr->reconControl->reconMap;
	raidPtr->reconControl->numRUsTotal = mapPtr->totalRUs;
	raidPtr->reconControl->numRUsComplete =	0;
	raidPtr->status = rf_rs_reconstructing;
	raidPtr->Disks[col].status = rf_ds_reconstructing;
	raidPtr->Disks[col].spareCol = scol;

	rf_unlock_mutex2(raidPtr->mutex);

	RF_GETTIME(raidPtr->reconControl->starttime);

	Dprintf("RECON: resume requests\n");
	rf_ResumeNewRequests(raidPtr);


	mapPtr = raidPtr->reconControl->reconMap;

	incPSID = RF_RECONMAP_SIZE;
	lastPSID = raidPtr->Layout.numStripe / raidPtr->Layout.SUsPerPU;
	RUsPerPU = raidPtr->Layout.SUsPerPU / raidPtr->Layout.SUsPerRU;
	recon_error = 0;
	write_error = 0;
	pending_writes = incPSID;
	raidPtr->reconControl->lastPSID = incPSID - 1;

	/* bounds check raidPtr->reconControl->lastPSID and
	   pending_writes so that we don't attempt to wait for more IO
	   than can possibly happen */

	if (raidPtr->reconControl->lastPSID > lastPSID)
		raidPtr->reconControl->lastPSID = lastPSID;

	if (pending_writes > lastPSID)
		pending_writes = lastPSID;

	/* start the actual reconstruction */

	done = 0;
	while (!done) {
		
		if (raidPtr->waitShutdown) {
			/* someone is unconfiguring this array... bail on the reconstruct.. */
			recon_error = 1;
			break;
		}

		num_writes = 0;

#if RF_INCLUDE_RAID5_RS > 0
		/* For RAID5 with Rotated Spares we will be 'short'
		   some number of writes since no writes will get
		   issued for stripes where the spare is on the
		   component being rebuilt.  Account for the shortage
		   here so that we don't hang indefinitely below
		   waiting for writes to complete that were never
		   scheduled.

		   XXX: Should be fixed for PARITY_DECLUSTERING and
		   others too! 

		*/

		if (raidPtr->Layout.numDataCol < 
		    raidPtr->numCol - raidPtr->Layout.numParityCol) {
			/* numDataCol is at least 2 less than numCol, so
			   should be RAID 5 with Rotated Spares */

			/* XXX need to update for RAID 6 */
			
			startPSID = raidPtr->reconControl->lastPSID - pending_writes + 1;
			endPSID = raidPtr->reconControl->lastPSID;
			
			offPSID = raidPtr->numCol - col - 1;
			
			aPSID = startPSID - startPSID % raidPtr->numCol + offPSID;
			if (aPSID < startPSID) {
				aPSID += raidPtr->numCol;
			}
			
			bPSID = endPSID - ((endPSID - offPSID) % raidPtr->numCol);
			
			if (aPSID < endPSID) {
				num_writes = ((bPSID - aPSID) / raidPtr->numCol) + 1;
			}
			
			if ((aPSID == endPSID) && (bPSID == endPSID)) {
				num_writes++;
			}
		}
#endif
		
		/* issue a read for each surviving disk */
		
		reconDesc->numDisksDone = 0;
		for (i = 0; i < raidPtr->numCol; i++) {
			if (i != col) {
				/* find and issue the next I/O on the
				 * indicated disk */
				if (IssueNextReadRequest(raidPtr, i)) {
					Dprintf1("RECON: done issuing for c%d\n", i);
					reconDesc->numDisksDone++;
				}
			}
		}

		/* process reconstruction events until all disks report that
		 * they've completed all work */

		while (reconDesc->numDisksDone < raidPtr->numCol - 1) {

			event = rf_GetNextReconEvent(reconDesc);
			status = ProcessReconEvent(raidPtr, event);
			
			/* the normal case is that a read completes, and all is well. */
			if (status == RF_RECON_DONE_READS) {
				reconDesc->numDisksDone++;
			} else if ((status == RF_RECON_READ_ERROR) ||
				   (status == RF_RECON_WRITE_ERROR)) {
				/* an error was encountered while reconstructing...
				   Pretend we've finished this disk.
				*/
				recon_error = 1;
				raidPtr->reconControl->error = 1;
				
				/* bump the numDisksDone count for reads,
				   but not for writes */
				if (status == RF_RECON_READ_ERROR)
					reconDesc->numDisksDone++;
				
				/* write errors are special -- when we are
				   done dealing with the reads that are
				   finished, we don't want to wait for any
				   writes */
				if (status == RF_RECON_WRITE_ERROR) {
					write_error = 1;
					num_writes++;
				}
				
			} else if (status == RF_RECON_READ_STOPPED) {
				/* count this component as being "done" */
				reconDesc->numDisksDone++;
			} else if (status == RF_RECON_WRITE_DONE) {
				num_writes++;
			} 
			
			if (recon_error) {
				/* make sure any stragglers are woken up so that
				   their theads will complete, and we can get out
				   of here with all IO processed */

				rf_WakeupHeadSepCBWaiters(raidPtr);
			}

			raidPtr->reconControl->numRUsTotal =
				mapPtr->totalRUs;
			raidPtr->reconControl->numRUsComplete =
				mapPtr->totalRUs -
				rf_UnitsLeftToReconstruct(mapPtr);

#if RF_DEBUG_RECON
			raidPtr->reconControl->percentComplete =
				(raidPtr->reconControl->numRUsComplete * 100 / raidPtr->reconControl->numRUsTotal);
			if (rf_prReconSched) {
				rf_PrintReconSchedule(raidPtr->reconControl->reconMap, &(raidPtr->reconControl->starttime));
			}
#endif
		}

		/* reads done, wakeup any waiters, and then wait for writes */

		rf_WakeupHeadSepCBWaiters(raidPtr);

		while (!recon_error && (num_writes < pending_writes)) {
			event = rf_GetNextReconEvent(reconDesc);
			status = ProcessReconEvent(raidPtr, event);
			
			if (status == RF_RECON_WRITE_ERROR) {
				num_writes++;
				recon_error = 1;
				raidPtr->reconControl->error = 1;
				/* an error was encountered at the very end... bail */
			} else if (status == RF_RECON_WRITE_DONE) {
				num_writes++;
			} /* else it's something else, and we don't care */
		}
		if (recon_error || 
		    (raidPtr->reconControl->lastPSID == lastPSID)) {
			done = 1;
			break;
		}

		prev = raidPtr->reconControl->lastPSID;
		raidPtr->reconControl->lastPSID += incPSID;

		if (raidPtr->reconControl->lastPSID > lastPSID) {
			pending_writes = lastPSID - prev;
			raidPtr->reconControl->lastPSID = lastPSID;
		}
		
		/* back down curPSID to get ready for the next round... */
		for (i = 0; i < raidPtr->numCol; i++) {
			if (i != col) {
				raidPtr->reconControl->perDiskInfo[i].curPSID--;
				raidPtr->reconControl->perDiskInfo[i].ru_count = RUsPerPU - 1;
			}
		}
	}

	mapPtr = raidPtr->reconControl->reconMap;
	if (rf_reconDebug) {
		printf("RECON: all reads completed\n");
	}
	/* at this point all the reads have completed.  We now wait
	 * for any pending writes to complete, and then we're done */

	while (!recon_error && rf_UnitsLeftToReconstruct(raidPtr->reconControl->reconMap) > 0) {

		event = rf_GetNextReconEvent(reconDesc);
		status = ProcessReconEvent(raidPtr, event);

		if (status == RF_RECON_WRITE_ERROR) {
			recon_error = 1;
			raidPtr->reconControl->error = 1;
			/* an error was encountered at the very end... bail */
		} else {
#if RF_DEBUG_RECON
			raidPtr->reconControl->percentComplete = 100 - (rf_UnitsLeftToReconstruct(mapPtr) * 100 / mapPtr->totalRUs);
			if (rf_prReconSched) {
				rf_PrintReconSchedule(raidPtr->reconControl->reconMap, &(raidPtr->reconControl->starttime));
			}
#endif
		}
	}

	if (recon_error) {
		/* we've encountered an error in reconstructing. */
		printf("raid%d: reconstruction failed.\n", raidPtr->raidid);

		/* we start by blocking IO to the RAID set. */
		rf_SuspendNewRequestsAndWait(raidPtr);

		rf_lock_mutex2(raidPtr->mutex);
		/* mark set as being degraded, rather than
		   rf_rs_reconstructing as we were before the problem.
		   After this is done we can update status of the
		   component disks without worrying about someone
		   trying to read from a failed component.
		*/
		raidPtr->status = rf_rs_degraded;
		rf_unlock_mutex2(raidPtr->mutex);

		/* resume IO */
		rf_ResumeNewRequests(raidPtr);

		/* At this point there are two cases:
		   1) If we've experienced a read error, then we've
		   already waited for all the reads we're going to get,
		   and we just need to wait for the writes.

		   2) If we've experienced a write error, we've also
		   already waited for all the reads to complete,
		   but there is little point in waiting for the writes --
		   when they do complete, they will just be ignored.

		   So we just wait for writes to complete if we didn't have a
		   write error.
		*/

		if (!write_error) {
			/* wait for writes to complete */
			while (raidPtr->reconControl->pending_writes > 0) {

				event = rf_GetNextReconEvent(reconDesc);
				status = ProcessReconEvent(raidPtr, event);

				if (status == RF_RECON_WRITE_ERROR) {
					raidPtr->reconControl->error = 1;
					/* an error was encountered at the very end... bail.
					   This will be very bad news for the user, since
					   at this point there will have been a read error
					   on one component, and a write error on another!
					*/
					break;
				}
			}
		}


		/* cleanup */

		/* drain the event queue - after waiting for the writes above,
		   there shouldn't be much (if anything!) left in the queue. */

		rf_DrainReconEventQueue(reconDesc);

		/* XXX  As much as we'd like to free the recon control structure
		   and the reconDesc, we have no way of knowing if/when those will
		   be touched by IO that has yet to occur.  It is rather poor to be
		   basically causing a 'memory leak' here, but there doesn't seem to be
		   a cleaner alternative at this time.  Perhaps when the reconstruct code
		   gets a makeover this problem will go away.
		*/
#if 0
		rf_FreeReconControl(raidPtr);
#endif

#if RF_ACC_TRACE > 0
		RF_Free(raidPtr->recon_tracerecs, raidPtr->numCol * sizeof(RF_AccTraceEntry_t));
#endif
		/* XXX see comment above */
#if 0
		FreeReconDesc(reconDesc);
#endif

		return (1);
	}

	/* Success:  mark the dead disk as reconstructed.  We quiesce
	 * the array here to assure no nasty interactions with pending
	 * user accesses when we free up the psstatus structure as
	 * part of FreeReconControl() */

	rf_SuspendNewRequestsAndWait(raidPtr);

	rf_lock_mutex2(raidPtr->mutex);
	raidPtr->numFailures--;
	ds = (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE);
	raidPtr->Disks[col].status = (ds) ? rf_ds_dist_spared : rf_ds_spared;
	raidPtr->status = (ds) ? rf_rs_reconfigured : rf_rs_optimal;
	rf_unlock_mutex2(raidPtr->mutex);
	RF_GETTIME(etime);
	RF_TIMEVAL_DIFF(&(raidPtr->reconControl->starttime), &etime, &elpsd);

	rf_ResumeNewRequests(raidPtr);

	printf("raid%d: Reconstruction of disk at col %d completed\n",
	       raidPtr->raidid, col);
	xor_s = raidPtr->accumXorTimeUs / 1000000;
	xor_resid_us = raidPtr->accumXorTimeUs % 1000000;
	printf("raid%d: Recon time was %d.%06d seconds, accumulated XOR time was %ld us (%ld.%06ld)\n",
	       raidPtr->raidid,
	       (int) elpsd.tv_sec, (int) elpsd.tv_usec,
	       raidPtr->accumXorTimeUs, xor_s, xor_resid_us);
	printf("raid%d:  (start time %d sec %d usec, end time %d sec %d usec)\n",
	       raidPtr->raidid,
	       (int) raidPtr->reconControl->starttime.tv_sec,
	       (int) raidPtr->reconControl->starttime.tv_usec,
	       (int) etime.tv_sec, (int) etime.tv_usec);
#if RF_RECON_STATS > 0
	printf("raid%d: Total head-sep stall count was %d\n",
	       raidPtr->raidid, (int) reconDesc->hsStallCount);
#endif				/* RF_RECON_STATS > 0 */
	rf_FreeReconControl(raidPtr);
#if RF_ACC_TRACE > 0
	RF_Free(raidPtr->recon_tracerecs, raidPtr->numCol * sizeof(RF_AccTraceEntry_t));
#endif
	FreeReconDesc(reconDesc);

	return (0);

}
/*****************************************************************************
 * do the right thing upon each reconstruction event.
 *****************************************************************************/
static int
ProcessReconEvent(RF_Raid_t *raidPtr, RF_ReconEvent_t *event)
{
	int     retcode = 0, submitblocked;
	RF_ReconBuffer_t *rbuf;
	RF_SectorCount_t sectorsPerRU;

	retcode = RF_RECON_READ_STOPPED;

	Dprintf1("RECON: ProcessReconEvent type %d\n", event->type);

	switch (event->type) {

		/* a read I/O has completed */
	case RF_REVENT_READDONE:
		rbuf = raidPtr->reconControl->perDiskInfo[event->col].rbuf;
		Dprintf2("RECON: READDONE EVENT: col %d psid %ld\n",
		    event->col, rbuf->parityStripeID);
		Dprintf7("RECON: done read  psid %ld buf %lx  %02x %02x %02x %02x %02x\n",
		    rbuf->parityStripeID, rbuf->buffer, rbuf->buffer[0] & 0xff, rbuf->buffer[1] & 0xff,
		    rbuf->buffer[2] & 0xff, rbuf->buffer[3] & 0xff, rbuf->buffer[4] & 0xff);
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		if (!raidPtr->reconControl->error) {
			submitblocked = rf_SubmitReconBuffer(rbuf, 0, 0);
			Dprintf1("RECON: submitblocked=%d\n", submitblocked);
			if (!submitblocked)
				retcode = IssueNextReadRequest(raidPtr, event->col);
			else
				retcode = 0;
		}
		break;

		/* a write I/O has completed */
	case RF_REVENT_WRITEDONE:
#if RF_DEBUG_RECON
		if (rf_floatingRbufDebug) {
			rf_CheckFloatingRbufCount(raidPtr, 1);
		}
#endif
		sectorsPerRU = raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.SUsPerRU;
		rbuf = (RF_ReconBuffer_t *) event->arg;
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		Dprintf3("RECON: WRITEDONE EVENT: psid %d ru %d (%d %% complete)\n",
		    rbuf->parityStripeID, rbuf->which_ru, raidPtr->reconControl->percentComplete);
		rf_ReconMapUpdate(raidPtr, raidPtr->reconControl->reconMap,
		    rbuf->failedDiskSectorOffset, rbuf->failedDiskSectorOffset + sectorsPerRU - 1);
		rf_RemoveFromActiveReconTable(raidPtr, rbuf->parityStripeID, rbuf->which_ru);

		rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
		raidPtr->reconControl->pending_writes--;
		rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);

		if (rbuf->type == RF_RBUF_TYPE_FLOATING) {
			rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
			while(raidPtr->reconControl->rb_lock) {
				rf_wait_cond2(raidPtr->reconControl->rb_cv,
					      raidPtr->reconControl->rb_mutex);
			}
			raidPtr->reconControl->rb_lock = 1;
			rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);

			raidPtr->numFullReconBuffers--;
			rf_ReleaseFloatingReconBuffer(raidPtr, rbuf);

			rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
			raidPtr->reconControl->rb_lock = 0;
			rf_broadcast_cond2(raidPtr->reconControl->rb_cv);
			rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);
		} else
			if (rbuf->type == RF_RBUF_TYPE_FORCED)
				rf_FreeReconBuffer(rbuf);
			else
				RF_ASSERT(0);
		retcode = RF_RECON_WRITE_DONE;
		break;

	case RF_REVENT_BUFCLEAR:	/* A buffer-stall condition has been
					 * cleared */
		Dprintf1("RECON: BUFCLEAR EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			submitblocked = rf_SubmitReconBuffer(raidPtr->reconControl->perDiskInfo[event->col].rbuf,
							     0, (int) (long) event->arg);
			RF_ASSERT(!submitblocked);	/* we wouldn't have gotten the
							 * BUFCLEAR event if we
							 * couldn't submit */
			retcode = IssueNextReadRequest(raidPtr, event->col);
		}
		break;

	case RF_REVENT_BLOCKCLEAR:	/* A user-write reconstruction
					 * blockage has been cleared */
		DDprintf1("RECON: BLOCKCLEAR EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			retcode = TryToRead(raidPtr, event->col);
		}
		break;

	case RF_REVENT_HEADSEPCLEAR:	/* A max-head-separation
					 * reconstruction blockage has been
					 * cleared */
		Dprintf1("RECON: HEADSEPCLEAR EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			retcode = TryToRead(raidPtr, event->col);
		}
		break;

		/* a buffer has become ready to write */
	case RF_REVENT_BUFREADY:
		Dprintf1("RECON: BUFREADY EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			retcode = IssueNextWriteRequest(raidPtr);
#if RF_DEBUG_RECON
			if (rf_floatingRbufDebug) {
				rf_CheckFloatingRbufCount(raidPtr, 1);
			}
#endif
		}
		break;

		/* we need to skip the current RU entirely because it got
		 * recon'd while we were waiting for something else to happen */
	case RF_REVENT_SKIP:
		DDprintf1("RECON: SKIP EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			retcode = IssueNextReadRequest(raidPtr, event->col);
		}
		break;

		/* a forced-reconstruction read access has completed.  Just
		 * submit the buffer */
	case RF_REVENT_FORCEDREADDONE:
		rbuf = (RF_ReconBuffer_t *) event->arg;
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		DDprintf1("RECON: FORCEDREADDONE EVENT: col %d\n", event->col);
		if (!raidPtr->reconControl->error) {
			submitblocked = rf_SubmitReconBuffer(rbuf, 1, 0);
			RF_ASSERT(!submitblocked);
			retcode = 0;
		}
		break;

		/* A read I/O failed to complete */
	case RF_REVENT_READ_FAILED:
		retcode = RF_RECON_READ_ERROR;
		break;

		/* A write I/O failed to complete */
	case RF_REVENT_WRITE_FAILED:
		retcode = RF_RECON_WRITE_ERROR;

		/* This is an error, but it was a pending write.
		   Account for it. */
		rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
		raidPtr->reconControl->pending_writes--;
		rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);

		rbuf = (RF_ReconBuffer_t *) event->arg;

		/* cleanup the disk queue data */
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);

		/* At this point we're erroring out, badly, and floatingRbufs
		   may not even be valid.  Rather than putting this back onto
		   the floatingRbufs list, just arrange for its immediate
		   destruction.
		*/
		rf_FreeReconBuffer(rbuf);
		break;

		/* a forced read I/O failed to complete */
	case RF_REVENT_FORCEDREAD_FAILED:
		retcode = RF_RECON_READ_ERROR;
		break;

	default:
		RF_PANIC();
	}
	rf_FreeReconEventDesc(event);
	return (retcode);
}
/*****************************************************************************
 *
 * find the next thing that's needed on the indicated disk, and issue
 * a read request for it.  We assume that the reconstruction buffer
 * associated with this process is free to receive the data.  If
 * reconstruction is blocked on the indicated RU, we issue a
 * blockage-release request instead of a physical disk read request.
 * If the current disk gets too far ahead of the others, we issue a
 * head-separation wait request and return.
 *
 * ctrl->{ru_count, curPSID, diskOffset} and
 * rbuf->failedDiskSectorOffset are maintained to point to the unit
 * we're currently accessing.  Note that this deviates from the
 * standard C idiom of having counters point to the next thing to be
 * accessed.  This allows us to easily retry when we're blocked by
 * head separation or reconstruction-blockage events.
 *
 *****************************************************************************/
static int
IssueNextReadRequest(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_PerDiskReconCtrl_t *ctrl = &raidPtr->reconControl->perDiskInfo[col];
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconBuffer_t *rbuf = ctrl->rbuf;
	RF_ReconUnitCount_t RUsPerPU = layoutPtr->SUsPerPU / layoutPtr->SUsPerRU;
	RF_SectorCount_t sectorsPerRU = layoutPtr->sectorsPerStripeUnit * layoutPtr->SUsPerRU;
	int     do_new_check = 0, retcode = 0, status;

	/* if we are currently the slowest disk, mark that we have to do a new
	 * check */
	if (ctrl->headSepCounter <= raidPtr->reconControl->minHeadSepCounter)
		do_new_check = 1;

	while (1) {

		ctrl->ru_count++;
		if (ctrl->ru_count < RUsPerPU) {
			ctrl->diskOffset += sectorsPerRU;
			rbuf->failedDiskSectorOffset += sectorsPerRU;
		} else {
			ctrl->curPSID++;
			ctrl->ru_count = 0;
			/* code left over from when head-sep was based on
			 * parity stripe id */
			if (ctrl->curPSID > raidPtr->reconControl->lastPSID) {
				CheckForNewMinHeadSep(raidPtr, ++(ctrl->headSepCounter));
				return (RF_RECON_DONE_READS);	/* finito! */
			}
			/* find the disk offsets of the start of the parity
			 * stripe on both the current disk and the failed
			 * disk. skip this entire parity stripe if either disk
			 * does not appear in the indicated PS */
			status = ComputePSDiskOffsets(raidPtr, ctrl->curPSID, col, &ctrl->diskOffset, &rbuf->failedDiskSectorOffset,
			    &rbuf->spCol, &rbuf->spOffset);
			if (status) {
				ctrl->ru_count = RUsPerPU - 1;
				continue;
			}
		}
		rbuf->which_ru = ctrl->ru_count;

		/* skip this RU if it's already been reconstructed */
		if (rf_CheckRUReconstructed(raidPtr->reconControl->reconMap, rbuf->failedDiskSectorOffset)) {
			Dprintf2("Skipping psid %ld ru %d: already reconstructed\n", ctrl->curPSID, ctrl->ru_count);
			continue;
		}
		break;
	}
	ctrl->headSepCounter++;
	if (do_new_check)
		CheckForNewMinHeadSep(raidPtr, ctrl->headSepCounter);	/* update min if needed */


	/* at this point, we have definitely decided what to do, and we have
	 * only to see if we can actually do it now */
	rbuf->parityStripeID = ctrl->curPSID;
	rbuf->which_ru = ctrl->ru_count;
#if RF_ACC_TRACE > 0
	memset((char *) &raidPtr->recon_tracerecs[col], 0,
	    sizeof(raidPtr->recon_tracerecs[col]));
	raidPtr->recon_tracerecs[col].reconacc = 1;
	RF_ETIMER_START(raidPtr->recon_tracerecs[col].recon_timer);
#endif
	retcode = TryToRead(raidPtr, col);
	return (retcode);
}

/*
 * tries to issue the next read on the indicated disk.  We may be
 * blocked by (a) the heads being too far apart, or (b) recon on the
 * indicated RU being blocked due to a write by a user thread.  In
 * this case, we issue a head-sep or blockage wait request, which will
 * cause this same routine to be invoked again later when the blockage
 * has cleared.
 */

static int
TryToRead(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_PerDiskReconCtrl_t *ctrl = &raidPtr->reconControl->perDiskInfo[col];
	RF_SectorCount_t sectorsPerRU = raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.SUsPerRU;
	RF_StripeNum_t psid = ctrl->curPSID;
	RF_ReconUnitNum_t which_ru = ctrl->ru_count;
	RF_DiskQueueData_t *req;
	int     status;
	RF_ReconParityStripeStatus_t *pssPtr, *newpssPtr;

	/* if the current disk is too far ahead of the others, issue a
	 * head-separation wait and return */
	if (CheckHeadSeparation(raidPtr, ctrl, col, ctrl->headSepCounter, which_ru))
		return (0);

	/* allocate a new PSS in case we need it */
	newpssPtr = rf_AllocPSStatus(raidPtr);

	RF_LOCK_PSS_MUTEX(raidPtr, psid);
	pssPtr = rf_LookupRUStatus(raidPtr, raidPtr->reconControl->pssTable, psid, which_ru, RF_PSS_CREATE, newpssPtr);

	if (pssPtr != newpssPtr) {
		rf_FreePSStatus(raidPtr, newpssPtr);
	}

	/* if recon is blocked on the indicated parity stripe, issue a
	 * block-wait request and return. this also must mark the indicated RU
	 * in the stripe as under reconstruction if not blocked. */
	status = CheckForcedOrBlockedReconstruction(raidPtr, pssPtr, ctrl, col, psid, which_ru);
	if (status == RF_PSS_RECON_BLOCKED) {
		Dprintf2("RECON: Stalling psid %ld ru %d: recon blocked\n", psid, which_ru);
		goto out;
	} else
		if (status == RF_PSS_FORCED_ON_WRITE) {
			rf_CauseReconEvent(raidPtr, col, NULL, RF_REVENT_SKIP);
			goto out;
		}
	/* make one last check to be sure that the indicated RU didn't get
	 * reconstructed while we were waiting for something else to happen.
	 * This is unfortunate in that it causes us to make this check twice
	 * in the normal case.  Might want to make some attempt to re-work
	 * this so that we only do this check if we've definitely blocked on
	 * one of the above checks.  When this condition is detected, we may
	 * have just created a bogus status entry, which we need to delete. */
	if (rf_CheckRUReconstructed(raidPtr->reconControl->reconMap, ctrl->rbuf->failedDiskSectorOffset)) {
		Dprintf2("RECON: Skipping psid %ld ru %d: prior recon after stall\n", psid, which_ru);
		if (pssPtr == newpssPtr)
			rf_PSStatusDelete(raidPtr, raidPtr->reconControl->pssTable, pssPtr);
		rf_CauseReconEvent(raidPtr, col, NULL, RF_REVENT_SKIP);
		goto out;
	}
	/* found something to read.  issue the I/O */
	Dprintf4("RECON: Read for psid %ld on col %d offset %ld buf %lx\n",
	    psid, col, ctrl->diskOffset, ctrl->rbuf->buffer);
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(raidPtr->recon_tracerecs[col].recon_timer);
	RF_ETIMER_EVAL(raidPtr->recon_tracerecs[col].recon_timer);
	raidPtr->recon_tracerecs[col].specific.recon.recon_start_to_fetch_us =
	    RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[col].recon_timer);
	RF_ETIMER_START(raidPtr->recon_tracerecs[col].recon_timer);
#endif
	/* should be ok to use a NULL proc pointer here, all the bufs we use
	 * should be in kernel space */
	req = rf_CreateDiskQueueData(RF_IO_TYPE_READ, ctrl->diskOffset, sectorsPerRU, ctrl->rbuf->buffer, psid, which_ru,
	    ReconReadDoneProc, (void *) ctrl,
#if RF_ACC_TRACE > 0
				     &raidPtr->recon_tracerecs[col],
#else
				     NULL,
#endif
				     (void *) raidPtr, 0, NULL, PR_WAITOK);

	ctrl->rbuf->arg = (void *) req;
	rf_DiskIOEnqueue(&raidPtr->Queues[col], req, RF_IO_RECON_PRIORITY);
	pssPtr->issued[col] = 1;

out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, psid);
	return (0);
}


/*
 * given a parity stripe ID, we want to find out whether both the
 * current disk and the failed disk exist in that parity stripe.  If
 * not, we want to skip this whole PS.  If so, we want to find the
 * disk offset of the start of the PS on both the current disk and the
 * failed disk.
 *
 * this works by getting a list of disks comprising the indicated
 * parity stripe, and searching the list for the current and failed
 * disks.  Once we've decided they both exist in the parity stripe, we
 * need to decide whether each is data or parity, so that we'll know
 * which mapping function to call to get the corresponding disk
 * offsets.
 *
 * this is kind of unpleasant, but doing it this way allows the
 * reconstruction code to use parity stripe IDs rather than physical
 * disks address to march through the failed disk, which greatly
 * simplifies a lot of code, as well as eliminating the need for a
 * reverse-mapping function.  I also think it will execute faster,
 * since the calls to the mapping module are kept to a minimum.
 *
 * ASSUMES THAT THE STRIPE IDENTIFIER IDENTIFIES THE DISKS COMPRISING
 * THE STRIPE IN THE CORRECT ORDER
 *
 * raidPtr          - raid descriptor
 * psid             - parity stripe identifier
 * col              - column of disk to find the offsets for
 * spCol            - out: col of spare unit for failed unit
 * spOffset         - out: offset into disk containing spare unit
 *
 */


static int
ComputePSDiskOffsets(RF_Raid_t *raidPtr, RF_StripeNum_t psid,
		     RF_RowCol_t col, RF_SectorNum_t *outDiskOffset,
		     RF_SectorNum_t *outFailedDiskSectorOffset,
		     RF_RowCol_t *spCol, RF_SectorNum_t *spOffset)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_RowCol_t fcol = raidPtr->reconControl->fcol;
	RF_RaidAddr_t sosRaidAddress;	/* start-of-stripe */
	RF_RowCol_t *diskids;
	u_int   i, j, k, i_offset, j_offset;
	RF_RowCol_t pcol;
	int     testcol;
	RF_SectorNum_t poffset;
	char    i_is_parity = 0, j_is_parity = 0;
	RF_RowCol_t stripeWidth = layoutPtr->numDataCol + layoutPtr->numParityCol;

	/* get a listing of the disks comprising that stripe */
	sosRaidAddress = rf_ParityStripeIDToRaidAddress(layoutPtr, psid);
	(layoutPtr->map->IdentifyStripe) (raidPtr, sosRaidAddress, &diskids);
	RF_ASSERT(diskids);

	/* reject this entire parity stripe if it does not contain the
	 * indicated disk or it does not contain the failed disk */

	for (i = 0; i < stripeWidth; i++) {
		if (col == diskids[i])
			break;
	}
	if (i == stripeWidth)
		goto skipit;
	for (j = 0; j < stripeWidth; j++) {
		if (fcol == diskids[j])
			break;
	}
	if (j == stripeWidth) {
		goto skipit;
	}
	/* find out which disk the parity is on */
	(layoutPtr->map->MapParity) (raidPtr, sosRaidAddress, &pcol, &poffset, RF_DONT_REMAP);

	/* find out if either the current RU or the failed RU is parity */
	/* also, if the parity occurs in this stripe prior to the data and/or
	 * failed col, we need to decrement i and/or j */
	for (k = 0; k < stripeWidth; k++)
		if (diskids[k] == pcol)
			break;
	RF_ASSERT(k < stripeWidth);
	i_offset = i;
	j_offset = j;
	if (k < i)
		i_offset--;
	else
		if (k == i) {
			i_is_parity = 1;
			i_offset = 0;
		}		/* set offsets to zero to disable multiply
				 * below */
	if (k < j)
		j_offset--;
	else
		if (k == j) {
			j_is_parity = 1;
			j_offset = 0;
		}
	/* at this point, [ij]_is_parity tells us whether the [current,failed]
	 * disk is parity at the start of this RU, and, if data, "[ij]_offset"
	 * tells us how far into the stripe the [current,failed] disk is. */

	/* call the mapping routine to get the offset into the current disk,
	 * repeat for failed disk. */
	if (i_is_parity)
		layoutPtr->map->MapParity(raidPtr, sosRaidAddress + i_offset * layoutPtr->sectorsPerStripeUnit, &testcol, outDiskOffset, RF_DONT_REMAP);
	else
		layoutPtr->map->MapSector(raidPtr, sosRaidAddress + i_offset * layoutPtr->sectorsPerStripeUnit, &testcol, outDiskOffset, RF_DONT_REMAP);

	RF_ASSERT(col == testcol);

	if (j_is_parity)
		layoutPtr->map->MapParity(raidPtr, sosRaidAddress + j_offset * layoutPtr->sectorsPerStripeUnit, &testcol, outFailedDiskSectorOffset, RF_DONT_REMAP);
	else
		layoutPtr->map->MapSector(raidPtr, sosRaidAddress + j_offset * layoutPtr->sectorsPerStripeUnit, &testcol, outFailedDiskSectorOffset, RF_DONT_REMAP);
	RF_ASSERT(fcol == testcol);

	/* now locate the spare unit for the failed unit */
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {
		if (j_is_parity)
			layoutPtr->map->MapParity(raidPtr, sosRaidAddress + j_offset * layoutPtr->sectorsPerStripeUnit, spCol, spOffset, RF_REMAP);
		else
			layoutPtr->map->MapSector(raidPtr, sosRaidAddress + j_offset * layoutPtr->sectorsPerStripeUnit, spCol, spOffset, RF_REMAP);
	} else {
#endif
		*spCol = raidPtr->reconControl->spareCol;
		*spOffset = *outFailedDiskSectorOffset;
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	}
#endif
	return (0);

skipit:
	Dprintf2("RECON: Skipping psid %ld: nothing needed from c%d\n",
	    psid, col);
	return (1);
}
/* this is called when a buffer has become ready to write to the replacement disk */
static int
IssueNextWriteRequest(RF_Raid_t *raidPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_SectorCount_t sectorsPerRU = layoutPtr->sectorsPerStripeUnit * layoutPtr->SUsPerRU;
#if RF_ACC_TRACE > 0
	RF_RowCol_t fcol = raidPtr->reconControl->fcol;
#endif
	RF_ReconBuffer_t *rbuf;
	RF_DiskQueueData_t *req;

	rbuf = rf_GetFullReconBuffer(raidPtr->reconControl);
	RF_ASSERT(rbuf);	/* there must be one available, or we wouldn't
				 * have gotten the event that sent us here */
	RF_ASSERT(rbuf->pssPtr);

	rbuf->pssPtr->writeRbuf = rbuf;
	rbuf->pssPtr = NULL;

	Dprintf6("RECON: New write (c %d offs %d) for psid %ld ru %d (failed disk offset %ld) buf %lx\n",
	    rbuf->spCol, rbuf->spOffset, rbuf->parityStripeID,
	    rbuf->which_ru, rbuf->failedDiskSectorOffset, rbuf->buffer);
	Dprintf6("RECON: new write psid %ld   %02x %02x %02x %02x %02x\n",
	    rbuf->parityStripeID, rbuf->buffer[0] & 0xff, rbuf->buffer[1] & 0xff,
	    rbuf->buffer[2] & 0xff, rbuf->buffer[3] & 0xff, rbuf->buffer[4] & 0xff);

	/* should be ok to use a NULL b_proc here b/c all addrs should be in
	 * kernel space */
	req = rf_CreateDiskQueueData(RF_IO_TYPE_WRITE, rbuf->spOffset,
	    sectorsPerRU, rbuf->buffer,
	    rbuf->parityStripeID, rbuf->which_ru,
	    ReconWriteDoneProc, (void *) rbuf,
#if RF_ACC_TRACE > 0
	    &raidPtr->recon_tracerecs[fcol],
#else
				     NULL,
#endif
	    (void *) raidPtr, 0, NULL, PR_WAITOK);

	rbuf->arg = (void *) req;
	rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
	raidPtr->reconControl->pending_writes++;
	rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);
	rf_DiskIOEnqueue(&raidPtr->Queues[rbuf->spCol], req, RF_IO_RECON_PRIORITY);

	return (0);
}

/*
 * this gets called upon the completion of a reconstruction read
 * operation the arg is a pointer to the per-disk reconstruction
 * control structure for the process that just finished a read.
 *
 * called at interrupt context in the kernel, so don't do anything
 * illegal here.
 */
static int
ReconReadDoneProc(void *arg, int status)
{
	RF_PerDiskReconCtrl_t *ctrl = (RF_PerDiskReconCtrl_t *) arg;
	RF_Raid_t *raidPtr;

	/* Detect that reconCtrl is no longer valid, and if that
	   is the case, bail without calling rf_CauseReconEvent().
	   There won't be anyone listening for this event anyway */

	if (ctrl->reconCtrl == NULL)
		return(0);

	raidPtr = ctrl->reconCtrl->reconDesc->raidPtr;

	if (status) {
		printf("raid%d: Recon read failed: %d\n", raidPtr->raidid, status);
		rf_CauseReconEvent(raidPtr, ctrl->col, NULL, RF_REVENT_READ_FAILED);
		return(0);
	}
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	RF_ETIMER_EVAL(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	raidPtr->recon_tracerecs[ctrl->col].specific.recon.recon_fetch_to_return_us =
	    RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	RF_ETIMER_START(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
#endif
	rf_CauseReconEvent(raidPtr, ctrl->col, NULL, RF_REVENT_READDONE);
	return (0);
}
/* this gets called upon the completion of a reconstruction write operation.
 * the arg is a pointer to the rbuf that was just written
 *
 * called at interrupt context in the kernel, so don't do anything illegal here.
 */
static int
ReconWriteDoneProc(void *arg, int status)
{
	RF_ReconBuffer_t *rbuf = (RF_ReconBuffer_t *) arg;

	/* Detect that reconControl is no longer valid, and if that
	   is the case, bail without calling rf_CauseReconEvent().
	   There won't be anyone listening for this event anyway */

	if (rbuf->raidPtr->reconControl == NULL)
		return(0);

	Dprintf2("Reconstruction completed on psid %ld ru %d\n", rbuf->parityStripeID, rbuf->which_ru);
	if (status) {
		printf("raid%d: Recon write failed (status %d(0x%x))!\n", rbuf->raidPtr->raidid,status,status);
		rf_CauseReconEvent(rbuf->raidPtr, rbuf->col, arg, RF_REVENT_WRITE_FAILED);
		return(0);
	}
	rf_CauseReconEvent(rbuf->raidPtr, rbuf->col, arg, RF_REVENT_WRITEDONE);
	return (0);
}


/*
 * computes a new minimum head sep, and wakes up anyone who needs to
 * be woken as a result
 */
static void
CheckForNewMinHeadSep(RF_Raid_t *raidPtr, RF_HeadSepLimit_t hsCtr)
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl;
	RF_HeadSepLimit_t new_min;
	RF_RowCol_t i;
	RF_CallbackDesc_t *p;
	RF_ASSERT(hsCtr >= reconCtrlPtr->minHeadSepCounter);	/* from the definition
								 * of a minimum */


	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	while(reconCtrlPtr->rb_lock) {
		rf_wait_cond2(reconCtrlPtr->rb_cv, reconCtrlPtr->rb_mutex);
	}
	reconCtrlPtr->rb_lock = 1;
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);

	new_min = ~(1L << (8 * sizeof(long) - 1));	/* 0x7FFF....FFF */
	for (i = 0; i < raidPtr->numCol; i++)
		if (i != reconCtrlPtr->fcol) {
			if (reconCtrlPtr->perDiskInfo[i].headSepCounter < new_min)
				new_min = reconCtrlPtr->perDiskInfo[i].headSepCounter;
		}
	/* set the new minimum and wake up anyone who can now run again */
	if (new_min != reconCtrlPtr->minHeadSepCounter) {
		reconCtrlPtr->minHeadSepCounter = new_min;
		Dprintf1("RECON:  new min head pos counter val is %ld\n", new_min);
		while (reconCtrlPtr->headSepCBList) {
			if (reconCtrlPtr->headSepCBList->callbackArg.v > new_min)
				break;
			p = reconCtrlPtr->headSepCBList;
			reconCtrlPtr->headSepCBList = p->next;
			p->next = NULL;
			rf_CauseReconEvent(raidPtr, p->col, NULL, RF_REVENT_HEADSEPCLEAR);
			rf_FreeCallbackDesc(p);
		}

	}
	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	reconCtrlPtr->rb_lock = 0;
	rf_broadcast_cond2(reconCtrlPtr->rb_cv);
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);
}

/*
 * checks to see that the maximum head separation will not be violated
 * if we initiate a reconstruction I/O on the indicated disk.
 * Limiting the maximum head separation between two disks eliminates
 * the nasty buffer-stall conditions that occur when one disk races
 * ahead of the others and consumes all of the floating recon buffers.
 * This code is complex and unpleasant but it's necessary to avoid
 * some very nasty, albeit fairly rare, reconstruction behavior.
 *
 * returns non-zero if and only if we have to stop working on the
 * indicated disk due to a head-separation delay.
 */
static int
CheckHeadSeparation(RF_Raid_t *raidPtr, RF_PerDiskReconCtrl_t *ctrl,
		    RF_RowCol_t col, RF_HeadSepLimit_t hsCtr,
		    RF_ReconUnitNum_t which_ru)
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl;
	RF_CallbackDesc_t *cb, *p, *pt;
	int     retval = 0;

	/* if we're too far ahead of the slowest disk, stop working on this
	 * disk until the slower ones catch up.  We do this by scheduling a
	 * wakeup callback for the time when the slowest disk has caught up.
	 * We define "caught up" with 20% hysteresis, i.e. the head separation
	 * must have fallen to at most 80% of the max allowable head
	 * separation before we'll wake up.
	 *
	 */
	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	while(reconCtrlPtr->rb_lock) {
		rf_wait_cond2(reconCtrlPtr->rb_cv, reconCtrlPtr->rb_mutex);
	}
	reconCtrlPtr->rb_lock = 1;
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);
	if ((raidPtr->headSepLimit >= 0) &&
	    ((ctrl->headSepCounter - reconCtrlPtr->minHeadSepCounter) > raidPtr->headSepLimit)) {
		Dprintf5("raid%d: RECON: head sep stall: col %d hsCtr %ld minHSCtr %ld limit %ld\n",
			 raidPtr->raidid, col, ctrl->headSepCounter,
			 reconCtrlPtr->minHeadSepCounter,
			 raidPtr->headSepLimit);
		cb = rf_AllocCallbackDesc();
		/* the minHeadSepCounter value we have to get to before we'll
		 * wake up.  build in 20% hysteresis. */
		cb->callbackArg.v = (ctrl->headSepCounter - raidPtr->headSepLimit + raidPtr->headSepLimit / 5);
		cb->col = col;
		cb->next = NULL;

		/* insert this callback descriptor into the sorted list of
		 * pending head-sep callbacks */
		p = reconCtrlPtr->headSepCBList;
		if (!p)
			reconCtrlPtr->headSepCBList = cb;
		else
			if (cb->callbackArg.v < p->callbackArg.v) {
				cb->next = reconCtrlPtr->headSepCBList;
				reconCtrlPtr->headSepCBList = cb;
			} else {
				for (pt = p, p = p->next; p && (p->callbackArg.v < cb->callbackArg.v); pt = p, p = p->next);
				cb->next = p;
				pt->next = cb;
			}
		retval = 1;
#if RF_RECON_STATS > 0
		ctrl->reconCtrl->reconDesc->hsStallCount++;
#endif				/* RF_RECON_STATS > 0 */
	}
	rf_lock_mutex2(reconCtrlPtr->rb_mutex);
	reconCtrlPtr->rb_lock = 0;
	rf_broadcast_cond2(reconCtrlPtr->rb_cv);
	rf_unlock_mutex2(reconCtrlPtr->rb_mutex);

	return (retval);
}
/*
 * checks to see if reconstruction has been either forced or blocked
 * by a user operation.  if forced, we skip this RU entirely.  else if
 * blocked, put ourselves on the wait list.  else return 0.
 *
 * ASSUMES THE PSS MUTEX IS LOCKED UPON ENTRY
 */
static int
CheckForcedOrBlockedReconstruction(RF_Raid_t *raidPtr,
				   RF_ReconParityStripeStatus_t *pssPtr,
				   RF_PerDiskReconCtrl_t *ctrl,
				   RF_RowCol_t col,
				   RF_StripeNum_t psid,
				   RF_ReconUnitNum_t which_ru)
{
	RF_CallbackDesc_t *cb;
	int     retcode = 0;

	if ((pssPtr->flags & RF_PSS_FORCED_ON_READ) || (pssPtr->flags & RF_PSS_FORCED_ON_WRITE))
		retcode = RF_PSS_FORCED_ON_WRITE;
	else
		if (pssPtr->flags & RF_PSS_RECON_BLOCKED) {
			Dprintf3("RECON: col %d blocked at psid %ld ru %d\n", col, psid, which_ru);
			cb = rf_AllocCallbackDesc();	/* append ourselves to
							 * the blockage-wait
							 * list */
			cb->col = col;
			cb->next = pssPtr->blockWaitList;
			pssPtr->blockWaitList = cb;
			retcode = RF_PSS_RECON_BLOCKED;
		}
	if (!retcode)
		pssPtr->flags |= RF_PSS_UNDER_RECON;	/* mark this RU as under
							 * reconstruction */

	return (retcode);
}
/*
 * if reconstruction is currently ongoing for the indicated stripeID,
 * reconstruction is forced to completion and we return non-zero to
 * indicate that the caller must wait.  If not, then reconstruction is
 * blocked on the indicated stripe and the routine returns zero.  If
 * and only if we return non-zero, we'll cause the cbFunc to get
 * invoked with the cbArg when the reconstruction has completed.
 */
int
rf_ForceOrBlockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
		     void (*cbFunc)(RF_Raid_t *, void *), void *cbArg)
{
	RF_StripeNum_t stripeID = asmap->stripeID;	/* the stripe ID we're
							 * forcing recon on */
	RF_SectorCount_t sectorsPerRU = raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.SUsPerRU;	/* num sects in one RU */
	RF_ReconParityStripeStatus_t *pssPtr, *newpssPtr;	/* a pointer to the parity
						 * stripe status structure */
	RF_StripeNum_t psid;	/* parity stripe id */
	RF_SectorNum_t offset, fd_offset;	/* disk offset, failed-disk
						 * offset */
	RF_RowCol_t *diskids;
	RF_ReconUnitNum_t which_ru;	/* RU within parity stripe */
	RF_RowCol_t fcol, diskno, i;
	RF_ReconBuffer_t *new_rbuf;	/* ptr to newly allocated rbufs */
	RF_DiskQueueData_t *req;/* disk I/O req to be enqueued */
	RF_CallbackDesc_t *cb;
	int     nPromoted;

	psid = rf_MapStripeIDToParityStripeID(&raidPtr->Layout, stripeID, &which_ru);

	/* allocate a new PSS in case we need it */
        newpssPtr = rf_AllocPSStatus(raidPtr);

	RF_LOCK_PSS_MUTEX(raidPtr, psid);

	pssPtr = rf_LookupRUStatus(raidPtr, raidPtr->reconControl->pssTable, psid, which_ru, RF_PSS_CREATE | RF_PSS_RECON_BLOCKED, newpssPtr);

        if (pssPtr != newpssPtr) {
                rf_FreePSStatus(raidPtr, newpssPtr);
        }

	/* if recon is not ongoing on this PS, just return */
	if (!(pssPtr->flags & RF_PSS_UNDER_RECON)) {
		RF_UNLOCK_PSS_MUTEX(raidPtr, psid);
		return (0);
	}
	/* otherwise, we have to wait for reconstruction to complete on this
	 * RU. */
	/* In order to avoid waiting for a potentially large number of
	 * low-priority accesses to complete, we force a normal-priority (i.e.
	 * not low-priority) reconstruction on this RU. */
	if (!(pssPtr->flags & RF_PSS_FORCED_ON_WRITE) && !(pssPtr->flags & RF_PSS_FORCED_ON_READ)) {
		DDprintf1("Forcing recon on psid %ld\n", psid);
		pssPtr->flags |= RF_PSS_FORCED_ON_WRITE;	/* mark this RU as under
								 * forced recon */
		pssPtr->flags &= ~RF_PSS_RECON_BLOCKED;	/* clear the blockage
							 * that we just set */
		fcol = raidPtr->reconControl->fcol;

		/* get a listing of the disks comprising the indicated stripe */
		(raidPtr->Layout.map->IdentifyStripe) (raidPtr, asmap->raidAddress, &diskids);

		/* For previously issued reads, elevate them to normal
		 * priority.  If the I/O has already completed, it won't be
		 * found in the queue, and hence this will be a no-op. For
		 * unissued reads, allocate buffers and issue new reads.  The
		 * fact that we've set the FORCED bit means that the regular
		 * recon procs will not re-issue these reqs */
		for (i = 0; i < raidPtr->Layout.numDataCol + raidPtr->Layout.numParityCol; i++)
			if ((diskno = diskids[i]) != fcol) {
				if (pssPtr->issued[diskno]) {
					nPromoted = rf_DiskIOPromote(&raidPtr->Queues[diskno], psid, which_ru);
					if (rf_reconDebug && nPromoted)
						printf("raid%d: promoted read from col %d\n", raidPtr->raidid, diskno);
				} else {
					new_rbuf = rf_MakeReconBuffer(raidPtr, diskno, RF_RBUF_TYPE_FORCED);	/* create new buf */
					ComputePSDiskOffsets(raidPtr, psid, diskno, &offset, &fd_offset,
					    &new_rbuf->spCol, &new_rbuf->spOffset);	/* find offsets & spare
													 * location */
					new_rbuf->parityStripeID = psid;	/* fill in the buffer */
					new_rbuf->which_ru = which_ru;
					new_rbuf->failedDiskSectorOffset = fd_offset;
					new_rbuf->priority = RF_IO_NORMAL_PRIORITY;

					/* use NULL b_proc b/c all addrs
					 * should be in kernel space */
					req = rf_CreateDiskQueueData(RF_IO_TYPE_READ, offset + which_ru * sectorsPerRU, sectorsPerRU, new_rbuf->buffer,
					    psid, which_ru, (int (*) (void *, int)) ForceReconReadDoneProc, (void *) new_rbuf,
					    NULL, (void *) raidPtr, 0, NULL, PR_WAITOK);

					new_rbuf->arg = req;
					rf_DiskIOEnqueue(&raidPtr->Queues[diskno], req, RF_IO_NORMAL_PRIORITY);	/* enqueue the I/O */
					Dprintf2("raid%d: Issued new read req on col %d\n", raidPtr->raidid, diskno);
				}
			}
		/* if the write is sitting in the disk queue, elevate its
		 * priority */
		if (rf_DiskIOPromote(&raidPtr->Queues[fcol], psid, which_ru))
			if (rf_reconDebug)
				printf("raid%d: promoted write to col %d\n",
				       raidPtr->raidid, fcol);
	}
	/* install a callback descriptor to be invoked when recon completes on
	 * this parity stripe. */
	cb = rf_AllocCallbackDesc();
	/* XXX the following is bogus.. These functions don't really match!!
	 * GO */
	cb->callbackFunc = (void (*) (RF_CBParam_t)) cbFunc;
	cb->callbackArg.p = (void *) cbArg;
	cb->next = pssPtr->procWaitList;
	pssPtr->procWaitList = cb;
	DDprintf2("raid%d: Waiting for forced recon on psid %ld\n",
		  raidPtr->raidid, psid);

	RF_UNLOCK_PSS_MUTEX(raidPtr, psid);
	return (1);
}
/* called upon the completion of a forced reconstruction read.
 * all we do is schedule the FORCEDREADONE event.
 * called at interrupt context in the kernel, so don't do anything illegal here.
 */
static void
ForceReconReadDoneProc(void *arg, int status)
{
	RF_ReconBuffer_t *rbuf = arg;

	/* Detect that reconControl is no longer valid, and if that
	   is the case, bail without calling rf_CauseReconEvent().
	   There won't be anyone listening for this event anyway */

	if (rbuf->raidPtr->reconControl == NULL)
		return;

	if (status) {
		printf("raid%d: Forced recon read failed!\n", rbuf->raidPtr->raidid);
		rf_CauseReconEvent(rbuf->raidPtr, rbuf->col, (void *) rbuf, RF_REVENT_FORCEDREAD_FAILED);
		return;
	}
	rf_CauseReconEvent(rbuf->raidPtr, rbuf->col, (void *) rbuf, RF_REVENT_FORCEDREADDONE);
}
/* releases a block on the reconstruction of the indicated stripe */
int
rf_UnblockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap)
{
	RF_StripeNum_t stripeID = asmap->stripeID;
	RF_ReconParityStripeStatus_t *pssPtr;
	RF_ReconUnitNum_t which_ru;
	RF_StripeNum_t psid;
	RF_CallbackDesc_t *cb;

	psid = rf_MapStripeIDToParityStripeID(&raidPtr->Layout, stripeID, &which_ru);
	RF_LOCK_PSS_MUTEX(raidPtr, psid);
	pssPtr = rf_LookupRUStatus(raidPtr, raidPtr->reconControl->pssTable, psid, which_ru, RF_PSS_NONE, NULL);

	/* When recon is forced, the pss desc can get deleted before we get
	 * back to unblock recon. But, this can _only_ happen when recon is
	 * forced. It would be good to put some kind of sanity check here, but
	 * how to decide if recon was just forced or not? */
	if (!pssPtr) {
		/* printf("Warning: no pss descriptor upon unblock on psid %ld
		 * RU %d\n",psid,which_ru); */
#if (RF_DEBUG_RECON > 0) || (RF_DEBUG_PSS > 0)
		if (rf_reconDebug || rf_pssDebug)
			printf("Warning: no pss descriptor upon unblock on psid %ld RU %d\n", (long) psid, which_ru);
#endif
		goto out;
	}
	pssPtr->blockCount--;
	Dprintf3("raid%d: unblocking recon on psid %ld: blockcount is %d\n",
		 raidPtr->raidid, psid, pssPtr->blockCount);
	if (pssPtr->blockCount == 0) {	/* if recon blockage has been released */

		/* unblock recon before calling CauseReconEvent in case
		 * CauseReconEvent causes us to try to issue a new read before
		 * returning here. */
		pssPtr->flags &= ~RF_PSS_RECON_BLOCKED;


		while (pssPtr->blockWaitList) {
			/* spin through the block-wait list and
			   release all the waiters */
			cb = pssPtr->blockWaitList;
			pssPtr->blockWaitList = cb->next;
			cb->next = NULL;
			rf_CauseReconEvent(raidPtr, cb->col, NULL, RF_REVENT_BLOCKCLEAR);
			rf_FreeCallbackDesc(cb);
		}
		if (!(pssPtr->flags & RF_PSS_UNDER_RECON)) {
			/* if no recon was requested while recon was blocked */
			rf_PSStatusDelete(raidPtr, raidPtr->reconControl->pssTable, pssPtr);
		}
	}
out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, psid);
	return (0);
}

void
rf_WakeupHeadSepCBWaiters(RF_Raid_t *raidPtr)
{
	RF_CallbackDesc_t *p;

	rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
	while(raidPtr->reconControl->rb_lock) {
		rf_wait_cond2(raidPtr->reconControl->rb_cv,
			      raidPtr->reconControl->rb_mutex);
	}
	
	raidPtr->reconControl->rb_lock = 1;
	rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);
	
	while (raidPtr->reconControl->headSepCBList) {
		p = raidPtr->reconControl->headSepCBList;
		raidPtr->reconControl->headSepCBList = p->next;
		p->next = NULL;
		rf_CauseReconEvent(raidPtr, p->col, NULL, RF_REVENT_HEADSEPCLEAR);
		rf_FreeCallbackDesc(p);
	}
	rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
	raidPtr->reconControl->rb_lock = 0;
	rf_broadcast_cond2(raidPtr->reconControl->rb_cv);
	rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);
	
}

