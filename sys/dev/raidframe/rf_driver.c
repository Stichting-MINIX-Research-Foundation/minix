/*	$NetBSD: rf_driver.c,v 1.131 2012/12/10 08:36:03 msaitoh Exp $	*/
/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster
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
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Khalil Amiri, Claudson Bornstein, William V. Courtright II,
 *         Robby Findler, Daniel Stodolsky, Rachad Youssef, Jim Zelenka
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

/******************************************************************************
 *
 * rf_driver.c -- main setup, teardown, and access routines for the RAID driver
 *
 * all routines are prefixed with rf_ (raidframe), to avoid conficts.
 *
 ******************************************************************************/


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_driver.c,v 1.131 2012/12/10 08:36:03 msaitoh Exp $");

#ifdef _KERNEL_OPT
#include "opt_raid_diagnostic.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>


#include "rf_archs.h"
#include "rf_threadstuff.h"

#include <sys/errno.h>

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_aselect.h"
#include "rf_diskqueue.h"
#include "rf_parityscan.h"
#include "rf_alloclist.h"
#include "rf_dagutils.h"
#include "rf_utils.h"
#include "rf_etimer.h"
#include "rf_acctrace.h"
#include "rf_general.h"
#include "rf_desc.h"
#include "rf_states.h"
#include "rf_decluster.h"
#include "rf_map.h"
#include "rf_revent.h"
#include "rf_callback.h"
#include "rf_engine.h"
#include "rf_mcpair.h"
#include "rf_nwayxor.h"
#include "rf_copyback.h"
#include "rf_driver.h"
#include "rf_options.h"
#include "rf_shutdown.h"
#include "rf_kintf.h"
#include "rf_paritymap.h"

#include <sys/buf.h>

#ifndef RF_ACCESS_DEBUG
#define RF_ACCESS_DEBUG 0
#endif

/* rad == RF_RaidAccessDesc_t */
#define RF_MAX_FREE_RAD 128
#define RF_MIN_FREE_RAD  32

/* debug variables */
char    rf_panicbuf[2048];	/* a buffer to hold an error msg when we panic */

/* main configuration routines */
static int raidframe_booted = 0;

static void rf_ConfigureDebug(RF_Config_t * cfgPtr);
static void set_debug_option(char *name, long val);
static void rf_UnconfigureArray(void);
static void rf_ShutdownRDFreeList(void *);
static int rf_ConfigureRDFreeList(RF_ShutdownList_t **);

rf_declare_mutex2(rf_printf_mutex);	/* debug only:  avoids interleaved
					 * printfs by different stripes */

#define SIGNAL_QUIESCENT_COND(_raid_) \
	rf_broadcast_cond2((_raid_)->access_suspend_cv)
#define WAIT_FOR_QUIESCENCE(_raid_) \
	rf_wait_cond2((_raid_)->access_suspend_cv, \
		      (_raid_)->access_suspend_mutex)

static int configureCount = 0;	/* number of active configurations */
static int isconfigged = 0;	/* is basic raidframe (non per-array)
				 * stuff configured */
static rf_declare_mutex2(configureMutex); /* used to lock the configuration
					   * stuff */
static RF_ShutdownList_t *globalShutdown;	/* non array-specific
						 * stuff */

static int rf_ConfigureRDFreeList(RF_ShutdownList_t ** listp);
static int rf_AllocEmergBuffers(RF_Raid_t *);
static void rf_FreeEmergBuffers(RF_Raid_t *);
static void rf_destroy_mutex_cond(RF_Raid_t *);
static void rf_alloc_mutex_cond(RF_Raid_t *);

/* called at system boot time */
int
rf_BootRaidframe(void)
{

	if (raidframe_booted)
		return (EBUSY);
	raidframe_booted = 1;
	rf_init_mutex2(configureMutex, IPL_NONE);
 	configureCount = 0;
	isconfigged = 0;
	globalShutdown = NULL;
	return (0);
}

/*
 * Called whenever an array is shutdown
 */
static void
rf_UnconfigureArray(void)
{

	rf_lock_mutex2(configureMutex);
	if (--configureCount == 0) {	/* if no active configurations, shut
					 * everything down */
		rf_destroy_mutex2(rf_printf_mutex);
		isconfigged = 0;
		rf_ShutdownList(&globalShutdown);

		/*
	         * We must wait until now, because the AllocList module
	         * uses the DebugMem module.
	         */
#if RF_DEBUG_MEM
		if (rf_memDebug)
			rf_print_unfreed();
#endif
	}
	rf_unlock_mutex2(configureMutex);
}

/*
 * Called to shut down an array.
 */
int
rf_Shutdown(RF_Raid_t *raidPtr)
{

	if (!raidPtr->valid) {
		RF_ERRORMSG("Attempt to shut down unconfigured RAIDframe driver.  Aborting shutdown\n");
		return (EINVAL);
	}
	/*
         * wait for outstanding IOs to land
         * As described in rf_raid.h, we use the rad_freelist lock
         * to protect the per-array info about outstanding descs
         * since we need to do freelist locking anyway, and this
         * cuts down on the amount of serialization we've got going
         * on.
         */
	rf_lock_mutex2(raidPtr->rad_lock);
	if (raidPtr->waitShutdown) {
		rf_unlock_mutex2(raidPtr->rad_lock);
		return (EBUSY);
	}
	raidPtr->waitShutdown = 1;
	while (raidPtr->nAccOutstanding) {
		rf_wait_cond2(raidPtr->outstandingCond, raidPtr->rad_lock);
	}
	rf_unlock_mutex2(raidPtr->rad_lock);

	/* Wait for any parity re-writes to stop... */
	while (raidPtr->parity_rewrite_in_progress) {
		printf("raid%d: Waiting for parity re-write to exit...\n",
		       raidPtr->raidid);
		tsleep(&raidPtr->parity_rewrite_in_progress, PRIBIO,
		       "rfprwshutdown", 0);
	}

	/* Wait for any reconstruction to stop... */
	rf_lock_mutex2(raidPtr->mutex);
	while (raidPtr->reconInProgress) {
		printf("raid%d: Waiting for reconstruction to stop...\n",
		       raidPtr->raidid);
		rf_wait_cond2(raidPtr->waitForReconCond, raidPtr->mutex);
	}
	rf_unlock_mutex2(raidPtr->mutex);

	raidPtr->valid = 0;

	if (raidPtr->parity_map != NULL)
		rf_paritymap_detach(raidPtr);

	rf_update_component_labels(raidPtr, RF_FINAL_COMPONENT_UPDATE);

	rf_UnconfigureVnodes(raidPtr);

	rf_FreeEmergBuffers(raidPtr);

	rf_ShutdownList(&raidPtr->shutdownList);

	rf_destroy_mutex_cond(raidPtr);

	rf_UnconfigureArray();

	return (0);
}


#define DO_INIT_CONFIGURE(f) { \
	rc = f (&globalShutdown); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		rf_ShutdownList(&globalShutdown); \
		configureCount--; \
		rf_unlock_mutex2(configureMutex); \
		rf_destroy_mutex2(rf_printf_mutex); \
		return(rc); \
	} \
}

#define DO_RAID_FAIL() { \
	rf_UnconfigureVnodes(raidPtr); \
	rf_FreeEmergBuffers(raidPtr); \
	rf_ShutdownList(&raidPtr->shutdownList); \
	rf_UnconfigureArray(); \
	rf_destroy_mutex_cond(raidPtr); \
}

#define DO_RAID_INIT_CONFIGURE(f) { \
	rc = f (&raidPtr->shutdownList, raidPtr, cfgPtr); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

int
rf_Configure(RF_Raid_t *raidPtr, RF_Config_t *cfgPtr, RF_AutoConfig_t *ac)
{
	RF_RowCol_t col;
	int rc;

	rf_lock_mutex2(configureMutex);
	configureCount++;
	if (isconfigged == 0) {
		rf_init_mutex2(rf_printf_mutex, IPL_VM);

		/* initialize globals */

		DO_INIT_CONFIGURE(rf_ConfigureAllocList);

		/*
	         * Yes, this does make debugging general to the whole
	         * system instead of being array specific. Bummer, drag.
		 */
		rf_ConfigureDebug(cfgPtr);
		DO_INIT_CONFIGURE(rf_ConfigureDebugMem);
#if RF_ACC_TRACE > 0
		DO_INIT_CONFIGURE(rf_ConfigureAccessTrace);
#endif
		DO_INIT_CONFIGURE(rf_ConfigureMapModule);
		DO_INIT_CONFIGURE(rf_ConfigureReconEvent);
		DO_INIT_CONFIGURE(rf_ConfigureCallback);
		DO_INIT_CONFIGURE(rf_ConfigureRDFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureNWayXor);
		DO_INIT_CONFIGURE(rf_ConfigureStripeLockFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureMCPair);
		DO_INIT_CONFIGURE(rf_ConfigureDAGs);
		DO_INIT_CONFIGURE(rf_ConfigureDAGFuncs);
		DO_INIT_CONFIGURE(rf_ConfigureReconstruction);
		DO_INIT_CONFIGURE(rf_ConfigureCopyback);
		DO_INIT_CONFIGURE(rf_ConfigureDiskQueueSystem);
		DO_INIT_CONFIGURE(rf_ConfigurePSStatus);
		isconfigged = 1;
	}
	rf_unlock_mutex2(configureMutex);

	rf_alloc_mutex_cond(raidPtr);

	/* set up the cleanup list.  Do this after ConfigureDebug so that
	 * value of memDebug will be set */

	rf_MakeAllocList(raidPtr->cleanupList);
	if (raidPtr->cleanupList == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	rf_ShutdownCreate(&raidPtr->shutdownList,
			  (void (*) (void *)) rf_FreeAllocList,
			  raidPtr->cleanupList);

	raidPtr->numCol = cfgPtr->numCol;
	raidPtr->numSpare = cfgPtr->numSpare;

	raidPtr->status = rf_rs_optimal;
	raidPtr->reconControl = NULL;

	DO_RAID_INIT_CONFIGURE(rf_ConfigureEngine);
	DO_RAID_INIT_CONFIGURE(rf_ConfigureStripeLocks);

	raidPtr->nAccOutstanding = 0;
	raidPtr->waitShutdown = 0;

	if (ac!=NULL) {
		/* We have an AutoConfig structure..  Don't do the
		   normal disk configuration... call the auto config
		   stuff */
		rf_AutoConfigureDisks(raidPtr, cfgPtr, ac);
	} else {
		DO_RAID_INIT_CONFIGURE(rf_ConfigureDisks);
		DO_RAID_INIT_CONFIGURE(rf_ConfigureSpareDisks);
	}
	/* do this after ConfigureDisks & ConfigureSpareDisks to be sure dev
	 * no. is set */
	DO_RAID_INIT_CONFIGURE(rf_ConfigureDiskQueues);

	DO_RAID_INIT_CONFIGURE(rf_ConfigureLayout);

	/* Initialize per-RAID PSS bits */
	rf_InitPSStatus(raidPtr);

#if RF_INCLUDE_CHAINDECLUSTER > 0
	for (col = 0; col < raidPtr->numCol; col++) {
		/*
		 * XXX better distribution
		 */
		raidPtr->hist_diskreq[col] = 0;
	}
#endif
	raidPtr->numNewFailures = 0;
	raidPtr->copyback_in_progress = 0;
	raidPtr->parity_rewrite_in_progress = 0;
	raidPtr->adding_hot_spare = 0;
	raidPtr->recon_in_progress = 0;

	raidPtr->maxOutstanding = cfgPtr->maxOutstandingDiskReqs;

	/* autoconfigure and root_partition will actually get filled in
	   after the config is done */
	raidPtr->autoconfigure = 0;
	raidPtr->root_partition = 0;
	raidPtr->last_unit = raidPtr->raidid;
	raidPtr->config_order = 0;

	if (rf_keepAccTotals) {
		raidPtr->keep_acc_totals = 1;
	}

	/* Allocate a bunch of buffers to be used in low-memory conditions */
	raidPtr->iobuf = NULL;

	rc = rf_AllocEmergBuffers(raidPtr);
	if (rc) {
		printf("raid%d: Unable to allocate emergency buffers.\n",
		       raidPtr->raidid);
		DO_RAID_FAIL();
		return(rc);
	}

	/* Set up parity map stuff, if applicable. */
#ifndef RF_NO_PARITY_MAP
	rf_paritymap_attach(raidPtr, cfgPtr->force);
#endif

	raidPtr->valid = 1;

	printf("raid%d: %s\n", raidPtr->raidid,
	       raidPtr->Layout.map->configName);
	printf("raid%d: Components:", raidPtr->raidid);

	for (col = 0; col < raidPtr->numCol; col++) {
		printf(" %s", raidPtr->Disks[col].devname);
		if (RF_DEAD_DISK(raidPtr->Disks[col].status)) {
			printf("[**FAILED**]");
		}
	}
	printf("\n");
	printf("raid%d: Total Sectors: %" PRIu64 " (%" PRIu64 " MB)\n",
	       raidPtr->raidid,
	       raidPtr->totalSectors,
	       (raidPtr->totalSectors / 1024 *
				(1 << raidPtr->logBytesPerSector) / 1024));

	return (0);
}


/*

  Routines to allocate and free the "emergency buffers" for a given
  RAID set.  These emergency buffers will be used when the kernel runs
  out of kernel memory.

 */

static int
rf_AllocEmergBuffers(RF_Raid_t *raidPtr)
{
	void *tmpbuf;
	RF_VoidPointerListElem_t *vple;
	int i;

	/* XXX next line needs tuning... */
	raidPtr->numEmergencyBuffers = 10 * raidPtr->numCol;
#if DEBUG
	printf("raid%d: allocating %d buffers of %d bytes.\n",
	       raidPtr->raidid,
	       raidPtr->numEmergencyBuffers,
	       (int)(raidPtr->Layout.sectorsPerStripeUnit <<
	       raidPtr->logBytesPerSector));
#endif
	for (i = 0; i < raidPtr->numEmergencyBuffers; i++) {
		tmpbuf = malloc( raidPtr->Layout.sectorsPerStripeUnit <<
				 raidPtr->logBytesPerSector,
				 M_RAIDFRAME, M_WAITOK);
		if (tmpbuf) {
			vple = rf_AllocVPListElem();
			vple->p= tmpbuf;
			vple->next = raidPtr->iobuf;
			raidPtr->iobuf = vple;
			raidPtr->iobuf_count++;
		} else {
			printf("raid%d: failed to allocate emergency buffer!\n",
			       raidPtr->raidid);
			return 1;
		}
	}

	/* XXX next line needs tuning too... */
	raidPtr->numEmergencyStripeBuffers = 10;
        for (i = 0; i < raidPtr->numEmergencyStripeBuffers; i++) {
                tmpbuf = malloc( raidPtr->numCol * (raidPtr->Layout.sectorsPerStripeUnit <<
                                 raidPtr->logBytesPerSector),
                                 M_RAIDFRAME, M_WAITOK);
                if (tmpbuf) {
                        vple = rf_AllocVPListElem();
                        vple->p= tmpbuf;
                        vple->next = raidPtr->stripebuf;
                        raidPtr->stripebuf = vple;
                        raidPtr->stripebuf_count++;
                } else {
                        printf("raid%d: failed to allocate emergency stripe buffer!\n",
                               raidPtr->raidid);
			return 1;
                }
        }

	return (0);
}

static void
rf_FreeEmergBuffers(RF_Raid_t *raidPtr)
{
	RF_VoidPointerListElem_t *tmp;

	/* Free the emergency IO buffers */
	while (raidPtr->iobuf != NULL) {
		tmp = raidPtr->iobuf;
		raidPtr->iobuf = raidPtr->iobuf->next;
		free(tmp->p, M_RAIDFRAME);
		rf_FreeVPListElem(tmp);
	}

	/* Free the emergency stripe buffers */
	while (raidPtr->stripebuf != NULL) {
		tmp = raidPtr->stripebuf;
		raidPtr->stripebuf = raidPtr->stripebuf->next;
		free(tmp->p, M_RAIDFRAME);
		rf_FreeVPListElem(tmp);
	}
}


static void
rf_ShutdownRDFreeList(void *ignored)
{
	pool_destroy(&rf_pools.rad);
}

static int
rf_ConfigureRDFreeList(RF_ShutdownList_t **listp)
{

	rf_pool_init(&rf_pools.rad, sizeof(RF_RaidAccessDesc_t),
		     "rf_rad_pl", RF_MIN_FREE_RAD, RF_MAX_FREE_RAD);
	rf_ShutdownCreate(listp, rf_ShutdownRDFreeList, NULL);
	return (0);
}

RF_RaidAccessDesc_t *
rf_AllocRaidAccDesc(RF_Raid_t *raidPtr, RF_IoType_t type,
		    RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks,
		    void *bufPtr, void *bp, RF_RaidAccessFlags_t flags,
		    const RF_AccessState_t *states)
{
	RF_RaidAccessDesc_t *desc;

	desc = pool_get(&rf_pools.rad, PR_WAITOK);

	rf_lock_mutex2(raidPtr->rad_lock);
	if (raidPtr->waitShutdown) {
		/*
	         * Actually, we're shutting the array down. Free the desc
	         * and return NULL.
	         */

		rf_unlock_mutex2(raidPtr->rad_lock);
		pool_put(&rf_pools.rad, desc);
		return (NULL);
	}
	raidPtr->nAccOutstanding++;

	rf_unlock_mutex2(raidPtr->rad_lock);

	desc->raidPtr = (void *) raidPtr;
	desc->type = type;
	desc->raidAddress = raidAddress;
	desc->numBlocks = numBlocks;
	desc->bufPtr = bufPtr;
	desc->bp = bp;
	desc->flags = flags;
	desc->states = states;
	desc->state = 0;
	desc->dagList = NULL;

	desc->status = 0;
	desc->numRetries = 0;
#if RF_ACC_TRACE > 0
	memset((char *) &desc->tracerec, 0, sizeof(RF_AccTraceEntry_t));
#endif
	desc->callbackFunc = NULL;
	desc->callbackArg = NULL;
	desc->next = NULL;
	desc->iobufs = NULL;
	desc->stripebufs = NULL;

	return (desc);
}

void
rf_FreeRaidAccDesc(RF_RaidAccessDesc_t *desc)
{
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_DagList_t *dagList, *temp;
	RF_VoidPointerListElem_t *tmp;

	RF_ASSERT(desc);

	/* Cleanup the dagList(s) */
	dagList = desc->dagList;
	while(dagList != NULL) {
		temp = dagList;
		dagList = dagList->next;
		rf_FreeDAGList(temp);
	}

	while (desc->iobufs) {
		tmp = desc->iobufs;
		desc->iobufs = desc->iobufs->next;
		rf_FreeIOBuffer(raidPtr, tmp);
	}

	while (desc->stripebufs) {
		tmp = desc->stripebufs;
		desc->stripebufs = desc->stripebufs->next;
		rf_FreeStripeBuffer(raidPtr, tmp);
	}

	pool_put(&rf_pools.rad, desc);
	rf_lock_mutex2(raidPtr->rad_lock);
	raidPtr->nAccOutstanding--;
	if (raidPtr->waitShutdown) {
		rf_signal_cond2(raidPtr->outstandingCond);
	}
	rf_unlock_mutex2(raidPtr->rad_lock);
}
/*********************************************************************
 * Main routine for performing an access.
 * Accesses are retried until a DAG can not be selected.  This occurs
 * when either the DAG library is incomplete or there are too many
 * failures in a parity group.
 *
 * type should be read or write async_flag should be RF_TRUE or
 * RF_FALSE bp_in is a buf pointer.  void *to facilitate ignoring it
 * outside the kernel
 ********************************************************************/
int
rf_DoAccess(RF_Raid_t * raidPtr, RF_IoType_t type, int async_flag,
	    RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks,
	    void *bufPtr, struct buf *bp, RF_RaidAccessFlags_t flags)
{
	RF_RaidAccessDesc_t *desc;
	void *lbufPtr = bufPtr;

	raidAddress += rf_raidSectorOffset;

#if RF_ACCESS_DEBUG
	if (rf_accessDebug) {

		printf("logBytes is: %d %d %d\n", raidPtr->raidid,
		    raidPtr->logBytesPerSector,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks));
		printf("raid%d: %s raidAddr %d (stripeid %d-%d) numBlocks %d (%d bytes) buf 0x%lx\n", raidPtr->raidid,
		    (type == RF_IO_TYPE_READ) ? "READ" : "WRITE", (int) raidAddress,
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress),
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress + numBlocks - 1),
		    (int) numBlocks,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks),
		    (long) bufPtr);
	}
#endif

	desc = rf_AllocRaidAccDesc(raidPtr, type, raidAddress,
	    numBlocks, lbufPtr, bp, flags, raidPtr->Layout.map->states);

	if (desc == NULL) {
		return (ENOMEM);
	}
#if RF_ACC_TRACE > 0
	RF_ETIMER_START(desc->tracerec.tot_timer);
#endif
	desc->async_flag = async_flag;

	if (raidPtr->parity_map != NULL && 
	    type == RF_IO_TYPE_WRITE)
		rf_paritymap_begin(raidPtr->parity_map, raidAddress, 
		    numBlocks);

	rf_ContinueRaidAccess(desc);

	return (0);
}
#if 0
/* force the array into reconfigured mode without doing reconstruction */
int
rf_SetReconfiguredMode(RF_Raid_t *raidPtr, int col)
{
	if (!(raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		printf("Can't set reconfigured mode in dedicated-spare array\n");
		RF_PANIC();
	}
	rf_lock_mutex2(raidPtr->mutex);
	raidPtr->numFailures++;
	raidPtr->Disks[col].status = rf_ds_dist_spared;
	raidPtr->status = rf_rs_reconfigured;
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	/* install spare table only if declustering + distributed sparing
	 * architecture. */
	if (raidPtr->Layout.map->flags & RF_BD_DECLUSTERED)
		rf_InstallSpareTable(raidPtr, col);
	rf_unlock_mutex2(raidPtr->mutex);
	return (0);
}
#endif

int
rf_FailDisk(RF_Raid_t *raidPtr, int fcol, int initRecon)
{

	/* need to suspend IO's here -- if there are DAGs in flight
	   and we pull the rug out from under ci_vp, Bad Things
	   can happen.  */

	rf_SuspendNewRequestsAndWait(raidPtr);

	rf_lock_mutex2(raidPtr->mutex);
	if (raidPtr->Disks[fcol].status != rf_ds_failed) {
		/* must be failing something that is valid, or else it's
		   already marked as failed (in which case we don't
		   want to mark it failed again!) */
		raidPtr->numFailures++;
		raidPtr->Disks[fcol].status = rf_ds_failed;
		raidPtr->status = rf_rs_degraded;
	}
	rf_unlock_mutex2(raidPtr->mutex);

	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);

	/* Close the component, so that it's not "locked" if someone
	   else want's to use it! */

	rf_close_component(raidPtr, raidPtr->raid_cinfo[fcol].ci_vp,
			   raidPtr->Disks[fcol].auto_configured);

	rf_lock_mutex2(raidPtr->mutex);
	raidPtr->raid_cinfo[fcol].ci_vp = NULL;

	/* Need to mark the component as not being auto_configured
	   (in case it was previously). */

	raidPtr->Disks[fcol].auto_configured = 0;
	rf_unlock_mutex2(raidPtr->mutex);
	/* now we can allow IO to continue -- we'll be suspending it
	   again in rf_ReconstructFailedDisk() if we have to.. */

	rf_ResumeNewRequests(raidPtr);

	if (initRecon)
		rf_ReconstructFailedDisk(raidPtr, fcol);
	return (0);
}
/* releases a thread that is waiting for the array to become quiesced.
 * access_suspend_mutex should be locked upon calling this
 */
void
rf_SignalQuiescenceLock(RF_Raid_t *raidPtr)
{
#if RF_DEBUG_QUIESCE
	if (rf_quiesceDebug) {
		printf("raid%d: Signalling quiescence lock\n",
		       raidPtr->raidid);
	}
#endif
	raidPtr->access_suspend_release = 1;

	if (raidPtr->waiting_for_quiescence) {
		SIGNAL_QUIESCENT_COND(raidPtr);
	}
}
/* suspends all new requests to the array.  No effect on accesses that are in flight.  */
int
rf_SuspendNewRequestsAndWait(RF_Raid_t *raidPtr)
{
#if RF_DEBUG_QUIESCE
	if (rf_quiesceDebug)
		printf("raid%d: Suspending new reqs\n", raidPtr->raidid);
#endif
	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended++;
	raidPtr->waiting_for_quiescence = (raidPtr->accs_in_flight == 0) ? 0 : 1;

	if (raidPtr->waiting_for_quiescence) {
		raidPtr->access_suspend_release = 0;
		while (!raidPtr->access_suspend_release) {
#if RF_DEBUG_QUIESCE
			printf("raid%d: Suspending: Waiting for Quiescence\n",
			       raidPtr->raidid);
#endif
			WAIT_FOR_QUIESCENCE(raidPtr);
			raidPtr->waiting_for_quiescence = 0;
		}
	}
#if RF_DEBUG_QUIESCE
	printf("raid%d: Quiescence reached..\n", raidPtr->raidid);
#endif

	rf_unlock_mutex2(raidPtr->access_suspend_mutex);
	return (raidPtr->waiting_for_quiescence);
}
/* wake up everyone waiting for quiescence to be released */
void
rf_ResumeNewRequests(RF_Raid_t *raidPtr)
{
	RF_CallbackDesc_t *t, *cb;

#if RF_DEBUG_QUIESCE
	if (rf_quiesceDebug)
		printf("raid%d: Resuming new requests\n", raidPtr->raidid);
#endif

	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended--;
	if (raidPtr->accesses_suspended == 0)
		cb = raidPtr->quiesce_wait_list;
	else
		cb = NULL;
	raidPtr->quiesce_wait_list = NULL;
	rf_unlock_mutex2(raidPtr->access_suspend_mutex);

	while (cb) {
		t = cb;
		cb = cb->next;
		(t->callbackFunc) (t->callbackArg);
		rf_FreeCallbackDesc(t);
	}
}
/*****************************************************************************************
 *
 * debug routines
 *
 ****************************************************************************************/

static void
set_debug_option(char *name, long val)
{
	RF_DebugName_t *p;

	for (p = rf_debugNames; p->name; p++) {
		if (!strcmp(p->name, name)) {
			*(p->ptr) = val;
			printf("[Set debug variable %s to %ld]\n", name, val);
			return;
		}
	}
	RF_ERRORMSG1("Unknown debug string \"%s\"\n", name);
}


/* would like to use sscanf here, but apparently not available in kernel */
/*ARGSUSED*/
static void
rf_ConfigureDebug(RF_Config_t *cfgPtr)
{
	char   *val_p, *name_p, *white_p;
	long    val;
	int     i;

	rf_ResetDebugOptions();
	for (i = 0; i < RF_MAXDBGV && cfgPtr->debugVars[i][0]; i++) {
		name_p = rf_find_non_white(&cfgPtr->debugVars[i][0]);
		white_p = rf_find_white(name_p);	/* skip to start of 2nd
							 * word */
		val_p = rf_find_non_white(white_p);
		if (*val_p == '0' && *(val_p + 1) == 'x')
			val = rf_htoi(val_p + 2);
		else
			val = rf_atoi(val_p);
		*white_p = '\0';
		set_debug_option(name_p, val);
	}
}

void
rf_print_panic_message(int line, const char *file)
{
	snprintf(rf_panicbuf, sizeof(rf_panicbuf),
	    "raidframe error at line %d file %s", line, file);
}

#ifdef RAID_DIAGNOSTIC
void
rf_print_assert_panic_message(int line,	const char *file, const char *condition)
{
	snprintf(rf_panicbuf, sizeof(rf_panicbuf),
		"raidframe error at line %d file %s (failed asserting %s)\n",
		line, file, condition);
}
#endif

void
rf_print_unable_to_init_mutex(const char *file, int line, int rc)
{
	RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n",
		     file, line, rc);
}

void
rf_print_unable_to_add_shutdown(const char *file, int line, int rc)
{
	RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		     file, line, rc);
}

static void
rf_alloc_mutex_cond(RF_Raid_t *raidPtr)
{

	rf_init_mutex2(raidPtr->mutex, IPL_VM);

	rf_init_cond2(raidPtr->outstandingCond, "rfocond");
	rf_init_mutex2(raidPtr->rad_lock, IPL_VM);

	rf_init_mutex2(raidPtr->access_suspend_mutex, IPL_VM);
	rf_init_cond2(raidPtr->access_suspend_cv, "rfquiesce");

	rf_init_cond2(raidPtr->waitForReconCond, "rfrcnw");

	rf_init_cond2(raidPtr->adding_hot_spare_cv, "raidhs");
}

static void
rf_destroy_mutex_cond(RF_Raid_t *raidPtr)
{

	rf_destroy_cond2(raidPtr->waitForReconCond);
	rf_destroy_cond2(raidPtr->adding_hot_spare_cv);

	rf_destroy_mutex2(raidPtr->access_suspend_mutex);
	rf_destroy_cond2(raidPtr->access_suspend_cv);

	rf_destroy_cond2(raidPtr->outstandingCond);
	rf_destroy_mutex2(raidPtr->rad_lock);

	rf_destroy_mutex2(raidPtr->mutex);
}
