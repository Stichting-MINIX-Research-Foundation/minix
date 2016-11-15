/*	$NetBSD: rf_desc.h,v 1.20 2007/03/04 06:02:36 christos Exp $	*/
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

#ifndef _RF__RF_DESC_H_
#define _RF__RF_DESC_H_

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_etimer.h"
#include "rf_dag.h"
#include "rf_layout.h"

struct RF_RaidReconDesc_s {
	RF_Raid_t *raidPtr;	/* raid device descriptor */
	RF_RowCol_t col;	/* col of failed disk */
	RF_RaidDisk_t *spareDiskPtr;	/* describes target disk for recon
					 * (not used in dist sparing) */
	int     numDisksDone;	/* the number of surviving disks that have
				 * completed their work */
	RF_RowCol_t scol;	/* col ID of the spare disk (not used in dist
				 * sparing) */
	/*
         * Prevent recon from hogging CPU
         */
	RF_Etimer_t recon_exec_timer;
	RF_uint64 reconExecTimerRunning;
	RF_uint64 reconExecTicks;
	RF_uint64 maxReconExecTicks;

#if RF_RECON_STATS > 0
	RF_uint64 hsStallCount;	/* head sep stall count */
	RF_uint64 numReconExecDelays;
	RF_uint64 numReconEventWaits;
#endif				/* RF_RECON_STATS > 0 */
	RF_RaidReconDesc_t *next;
};

struct RF_RaidAccessDesc_s {
	RF_Raid_t *raidPtr;	/* raid device descriptor */
	RF_IoType_t type;	/* read or write */
	RF_RaidAddr_t raidAddress;	/* starting address in raid address
					 * space */
	RF_SectorCount_t numBlocks;	/* number of blocks (sectors) to
					 * transfer */
	RF_StripeCount_t numStripes;	/* number of stripes involved in
					 * access */
	void *bufPtr;		/* pointer to data buffer */
	RF_RaidAccessFlags_t flags;	/* flags controlling operation */
	int     state;		/* index into states telling how far along the
				 * RAID operation has gotten */
	const RF_AccessState_t *states;	/* array of states to be run */
	int     status;		/* pass/fail status of the last operation */
	int     numRetries;     /* number of times this IO has been attempted */
	RF_DagList_t *dagList;	/* list of dag lists, one list per stripe */
	RF_VoidPointerListElem_t *iobufs; /* iobufs that need to be cleaned
					     up at the end of this IO */
	RF_VoidPointerListElem_t *stripebufs; /* stripe buffers that need to
						 be cleaned up at the end of
						 this IO */
	RF_AccessStripeMapHeader_t *asmap;	/* the asm for this I/O */
	struct buf *bp;		/* buf pointer for this RAID acc */
	RF_AccTraceEntry_t tracerec;	/* perf monitoring information for a
					 * user access (not for dag stats) */
	void    (*callbackFunc) (RF_CBParam_t);	/* callback function for this
						 * I/O */
	void   *callbackArg;	/* arg to give to callback func */
	RF_RaidAccessDesc_t *next;
	int     async_flag;
	RF_Etimer_t timer;	/* used for timing this access */
};
#endif				/* !_RF__RF_DESC_H_ */
