/*	$NetBSD: rf_reconutil.c,v 1.35 2013/09/15 12:48:58 martin Exp $	*/
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

/********************************************
 * rf_reconutil.c -- reconstruction utilities
 ********************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_reconutil.c,v 1.35 2013/09/15 12:48:58 martin Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_raid.h"
#include "rf_desc.h"
#include "rf_reconutil.h"
#include "rf_reconbuffer.h"
#include "rf_general.h"
#include "rf_decluster.h"
#include "rf_raid5_rotatedspare.h"
#include "rf_interdecluster.h"
#include "rf_chaindecluster.h"

/*******************************************************************
 * allocates/frees the reconstruction control information structures
 *******************************************************************/

/* fcol - failed column
 * scol - identifies which spare we are using
 */

RF_ReconCtrl_t *
rf_MakeReconControl(RF_RaidReconDesc_t *reconDesc,
		    RF_RowCol_t fcol, RF_RowCol_t scol)
{
	RF_Raid_t *raidPtr = reconDesc->raidPtr;
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconUnitCount_t RUsPerPU = layoutPtr->SUsPerPU / layoutPtr->SUsPerRU;
	RF_ReconUnitCount_t numSpareRUs;
	RF_ReconCtrl_t *reconCtrlPtr;
	RF_ReconBuffer_t *rbuf;
	const RF_LayoutSW_t *lp;
#if (RF_INCLUDE_PARITY_DECLUSTERING_DS > 0)
	int     retcode;
#endif
	RF_RowCol_t i;

	lp = raidPtr->Layout.map;

	/* make and zero the global reconstruction structure and the per-disk
	 * structure */
	RF_Malloc(reconCtrlPtr, sizeof(RF_ReconCtrl_t), (RF_ReconCtrl_t *));

	/* note: this zeros the perDiskInfo */
	RF_Malloc(reconCtrlPtr->perDiskInfo, raidPtr->numCol *
		  sizeof(RF_PerDiskReconCtrl_t), (RF_PerDiskReconCtrl_t *));
	reconCtrlPtr->reconDesc = reconDesc;
	reconCtrlPtr->fcol = fcol;
	reconCtrlPtr->spareCol = scol;
	reconCtrlPtr->lastPSID = layoutPtr->numStripe / layoutPtr->SUsPerPU;
	reconCtrlPtr->percentComplete = 0;
	reconCtrlPtr->error = 0;
	reconCtrlPtr->pending_writes = 0;

	/* initialize each per-disk recon information structure */
	for (i = 0; i < raidPtr->numCol; i++) {
		reconCtrlPtr->perDiskInfo[i].reconCtrl = reconCtrlPtr;
		reconCtrlPtr->perDiskInfo[i].col = i;
		/* make it appear as if we just finished an RU */
		reconCtrlPtr->perDiskInfo[i].curPSID = -1;
		reconCtrlPtr->perDiskInfo[i].ru_count = RUsPerPU - 1;
	}

	/* Get the number of spare units per disk and the sparemap in case
	 * spare is distributed  */

	if (lp->GetNumSpareRUs) {
		numSpareRUs = lp->GetNumSpareRUs(raidPtr);
	} else {
		numSpareRUs = 0;
	}

#if (RF_INCLUDE_PARITY_DECLUSTERING_DS > 0)
	/*
         * Not all distributed sparing archs need dynamic mappings
         */
	if (lp->InstallSpareTable) {
		retcode = rf_InstallSpareTable(raidPtr, 0, fcol);
		if (retcode) {
			RF_PANIC();	/* XXX fix this */
		}
	}
#endif
	/* make the reconstruction map */
	reconCtrlPtr->reconMap = rf_MakeReconMap(raidPtr, (int) (layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit),
	    raidPtr->sectorsPerDisk, numSpareRUs);

	/* make the per-disk reconstruction buffers */
	for (i = 0; i < raidPtr->numCol; i++) {
		reconCtrlPtr->perDiskInfo[i].rbuf = (i == fcol) ? NULL : rf_MakeReconBuffer(raidPtr, i, RF_RBUF_TYPE_EXCLUSIVE);
	}

	/* initialize the event queue */
	rf_init_mutex2(reconCtrlPtr->eq_mutex, IPL_VM);
	rf_init_cond2(reconCtrlPtr->eq_cv, "rfevq");

	reconCtrlPtr->eventQueue = NULL;
	reconCtrlPtr->eq_count = 0;

	/* make the floating recon buffers and append them to the free list */
	rf_init_mutex2(reconCtrlPtr->rb_mutex, IPL_VM);
	rf_init_cond2(reconCtrlPtr->rb_cv, "rfrcw");

	reconCtrlPtr->fullBufferList = NULL;
	reconCtrlPtr->floatingRbufs = NULL;
	reconCtrlPtr->committedRbufs = NULL;
	for (i = 0; i < raidPtr->numFloatingReconBufs; i++) {
		rbuf = rf_MakeReconBuffer(raidPtr, fcol,
					  RF_RBUF_TYPE_FLOATING);
		rbuf->next = reconCtrlPtr->floatingRbufs;
		reconCtrlPtr->floatingRbufs = rbuf;
	}

	/* create the parity stripe status table */
	reconCtrlPtr->pssTable = rf_MakeParityStripeStatusTable(raidPtr);

	/* set the initial min head sep counter val */
	reconCtrlPtr->minHeadSepCounter = 0;

	return (reconCtrlPtr);
}

void
rf_FreeReconControl(RF_Raid_t *raidPtr)
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl;
	RF_ReconBuffer_t *t;
	RF_ReconUnitNum_t i;

	RF_ASSERT(reconCtrlPtr);
	for (i = 0; i < raidPtr->numCol; i++)
		if (reconCtrlPtr->perDiskInfo[i].rbuf)
			rf_FreeReconBuffer(reconCtrlPtr->perDiskInfo[i].rbuf);

	t = reconCtrlPtr->floatingRbufs;
	while (t) {
		reconCtrlPtr->floatingRbufs = t->next;
		rf_FreeReconBuffer(t);
		t = reconCtrlPtr->floatingRbufs;
	}

	rf_destroy_mutex2(reconCtrlPtr->eq_mutex);
	rf_destroy_cond2(reconCtrlPtr->eq_cv);

	rf_destroy_mutex2(reconCtrlPtr->rb_mutex);
	rf_destroy_cond2(reconCtrlPtr->rb_cv);

	rf_FreeReconMap(reconCtrlPtr->reconMap);
	rf_FreeParityStripeStatusTable(raidPtr, reconCtrlPtr->pssTable);
	RF_Free(reconCtrlPtr->perDiskInfo,
		raidPtr->numCol * sizeof(RF_PerDiskReconCtrl_t));
	RF_Free(reconCtrlPtr, sizeof(*reconCtrlPtr));
}


/******************************************************************************
 * computes the default head separation limit
 *****************************************************************************/
RF_HeadSepLimit_t
rf_GetDefaultHeadSepLimit(RF_Raid_t *raidPtr)
{
	RF_HeadSepLimit_t hsl;
	const RF_LayoutSW_t *lp;

	lp = raidPtr->Layout.map;
	if (lp->GetDefaultHeadSepLimit == NULL)
		return (-1);
	hsl = lp->GetDefaultHeadSepLimit(raidPtr);
	return (hsl);
}


/******************************************************************************
 * computes the default number of floating recon buffers
 *****************************************************************************/
int
rf_GetDefaultNumFloatingReconBuffers(RF_Raid_t *raidPtr)
{
	const RF_LayoutSW_t *lp;
	int     nrb;

	lp = raidPtr->Layout.map;
	if (lp->GetDefaultNumFloatingReconBuffers == NULL)
		return (3 * raidPtr->numCol);
	nrb = lp->GetDefaultNumFloatingReconBuffers(raidPtr);
	return (nrb);
}


/******************************************************************************
 * creates and initializes a reconstruction buffer
 *****************************************************************************/
RF_ReconBuffer_t *
rf_MakeReconBuffer(RF_Raid_t *raidPtr, RF_RowCol_t col, RF_RbufType_t type)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconBuffer_t *t;
	u_int   recon_buffer_size = rf_RaidAddressToByte(raidPtr, layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit);

	t = pool_get(&rf_pools.reconbuffer, PR_WAITOK);
	RF_Malloc(t->buffer, recon_buffer_size, (void *));
	t->raidPtr = raidPtr;
	t->col = col;
	t->priority = RF_IO_RECON_PRIORITY;
	t->type = type;
	t->pssPtr = NULL;
	t->next = NULL;
	return (t);
}
/******************************************************************************
 * frees a reconstruction buffer
 *****************************************************************************/
void
rf_FreeReconBuffer(RF_ReconBuffer_t *rbuf)
{
	RF_Raid_t *raidPtr = rbuf->raidPtr;
	u_int   recon_buffer_size __unused;

	recon_buffer_size = rf_RaidAddressToByte(raidPtr, raidPtr->Layout.SUsPerRU * raidPtr->Layout.sectorsPerStripeUnit);

	RF_Free(rbuf->buffer, recon_buffer_size);
	pool_put(&rf_pools.reconbuffer, rbuf);
}

#if RF_DEBUG_RECON
XXXX IF you use this, you really want to fix the locking in here.
/******************************************************************************
 * debug only:  sanity check the number of floating recon bufs in use
 *****************************************************************************/
void
rf_CheckFloatingRbufCount(RF_Raid_t *raidPtr, int dolock)
{
	RF_ReconParityStripeStatus_t *p;
	RF_PSStatusHeader_t *pssTable;
	RF_ReconBuffer_t *rbuf;
	int     i, j, sum = 0;

	if (dolock)
		rf_lock_mutex2(raidPtr->reconControl->rb_mutex);
	pssTable = raidPtr->reconControl->pssTable;

	for (i = 0; i < raidPtr->pssTableSize; i++) {
		rf_lock_mutex2(pssTable[i].mutex);
		for (p = pssTable[i].chain; p; p = p->next) {
			rbuf = (RF_ReconBuffer_t *) p->rbuf;
			if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
				sum++;

			rbuf = (RF_ReconBuffer_t *) p->writeRbuf;
			if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
				sum++;

			for (j = 0; j < p->xorBufCount; j++) {
				rbuf = (RF_ReconBuffer_t *) p->rbufsForXor[j];
				RF_ASSERT(rbuf);
				if (rbuf->type == RF_RBUF_TYPE_FLOATING)
					sum++;
			}
		}
		rf_unlock_mutex2(pssTable[i].mutex);
	}

	for (rbuf = raidPtr->reconControl->floatingRbufs; rbuf;
	     rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	for (rbuf = raidPtr->reconControl->committedRbufs; rbuf;
	     rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	for (rbuf = raidPtr->reconControl->fullBufferList; rbuf;
	     rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	RF_ASSERT(sum == raidPtr->numFloatingReconBufs);

	if (dolock)
		rf_unlock_mutex2(raidPtr->reconControl->rb_mutex);
}
#endif

