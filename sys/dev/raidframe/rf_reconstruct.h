/*	$NetBSD: rf_reconstruct.h,v 1.28 2011/05/02 07:29:18 mrg Exp $	*/
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

/*********************************************************
 * rf_reconstruct.h -- header file for reconstruction code
 *********************************************************/

#ifndef _RF__RF_RECONSTRUCT_H_
#define _RF__RF_RECONSTRUCT_H_

#include <dev/raidframe/raidframevar.h>
#include <sys/time.h>
#include "rf_reconmap.h"
#include "rf_psstatus.h"

/* reconstruction configuration information */
struct RF_ReconConfig_s {
	unsigned numFloatingReconBufs;	/* number of floating recon bufs to
					 * use */
	RF_HeadSepLimit_t headSepLimit;	/* how far apart the heads are allow
					 * to become, in parity stripes */
};
/* a reconstruction buffer */
struct RF_ReconBuffer_s {
	RF_Raid_t *raidPtr;	/* void *to avoid recursive includes */
	void *buffer;		/* points to the data */
	RF_StripeNum_t parityStripeID;	/* the parity stripe that this data
					 * relates to */
	int     which_ru;	/* which reconstruction unit within the PSS */
	RF_SectorNum_t failedDiskSectorOffset;	/* the offset into the failed
						 * disk */
	RF_RowCol_t col;	/* which disk this buffer belongs to or is
				 * targeted at */
	RF_StripeCount_t count;	/* counts the # of SUs installed so far */
	int     priority;	/* used to force hi priority recon */
	RF_RbufType_t type;	/* FORCED or FLOATING */
	RF_ReconBuffer_t *next;	/* used for buffer management */
	void   *arg;		/* generic field for general use */
	RF_RowCol_t spRow, spCol;	/* spare disk to which this buf should
					 * be written */
	/* if dist sparing off, always identifies the replacement disk */
	RF_SectorNum_t spOffset;/* offset into the spare disk */
	/* if dist sparing off, identical to failedDiskSectorOffset */
	RF_ReconParityStripeStatus_t *pssPtr;	/* debug- pss associated with
						 * issue-pending write */
};
/* a reconstruction event descriptor.  The event types currently are:
 *    RF_REVENT_READDONE    -- a read operation has completed
 *    RF_REVENT_WRITEDONE   -- a write operation has completed
 *    RF_REVENT_BUFREADY    -- the buffer manager has produced a full buffer
 *    RF_REVENT_BLOCKCLEAR  -- a reconstruction blockage has been cleared
 *    RF_REVENT_BUFCLEAR    -- the buffer manager has released a process blocked on submission
 *    RF_REVENT_SKIP        -- we need to skip the current RU and go on to the next one, typ. b/c we found recon forced
 *    RF_REVENT_FORCEDREADONE- a forced-reconstructoin read operation has completed
 */
typedef enum RF_Revent_e {
	RF_REVENT_READDONE,
	RF_REVENT_WRITEDONE,
	RF_REVENT_BUFREADY,
	RF_REVENT_BLOCKCLEAR,
	RF_REVENT_BUFCLEAR,
	RF_REVENT_HEADSEPCLEAR,
	RF_REVENT_SKIP,
	RF_REVENT_FORCEDREADDONE,
	RF_REVENT_READ_FAILED,
	RF_REVENT_WRITE_FAILED,
	RF_REVENT_FORCEDREAD_FAILED
}       RF_Revent_t;

struct RF_ReconEvent_s {
	RF_Revent_t type;	/* what kind of event has occurred */
	RF_RowCol_t col;	/* row ID is implicit in the queue in which
				 * the event is placed */
	void   *arg;		/* a generic argument */
	RF_ReconEvent_t *next;
};
/*
 * Reconstruction control information maintained per-disk
 * (for surviving disks)
 */
struct RF_PerDiskReconCtrl_s {
	RF_ReconCtrl_t *reconCtrl;
	RF_RowCol_t col;	/* to make this structure self-identifying */
	RF_StripeNum_t curPSID;	/* the next parity stripe ID to check on this
				 * disk */
	RF_HeadSepLimit_t headSepCounter;	/* counter used to control
						 * maximum head separation */
	RF_SectorNum_t diskOffset;	/* the offset into the indicated disk
					 * of the current PU */
	RF_ReconUnitNum_t ru_count;	/* this counts off the recon units
					 * within each parity unit */
	RF_ReconBuffer_t *rbuf;	/* the recon buffer assigned to this disk */
};
/* main reconstruction control structure */
struct RF_ReconCtrl_s {
	RF_RaidReconDesc_t *reconDesc;
	RF_RowCol_t fcol;	/* which column has failed */
	RF_PerDiskReconCtrl_t *perDiskInfo;	/* information maintained
						 * per-disk */
	RF_ReconMap_t *reconMap;/* map of what has/has not been reconstructed */
	RF_RowCol_t spareCol;   /* which of the spare disks we're using */
	RF_StripeNum_t lastPSID;/* the ID of the last parity stripe we want
				 * reconstructed */
	int     percentComplete;/* percentage completion of reconstruction */
	RF_ReconUnitCount_t  numRUsComplete; /* number of Reconstruction Units done */
	RF_ReconUnitCount_t  numRUsTotal;    /* total number of Reconstruction Units */
	int error;              /* non-0 indicates that an error has
				   occured during reconstruction, and
				   the reconstruction is in the process of
				   bailing out. */

	/* reconstruction event queue */
	RF_ReconEvent_t *eventQueue;	/* queue of pending reconstruction
					 * events */
	rf_declare_mutex2(eq_mutex);	/* mutex for locking event */
	rf_declare_cond2(eq_cv);	/* queue */
	int     eq_count;	/* debug only */

	/* reconstruction buffer management */
	rf_declare_mutex2(rb_mutex);	        /* mutex/cv for messing */
	rf_declare_cond2(rb_cv);		/* around with recon buffers */
	int rb_lock;                            /* 1 if someone is mucking
						   with recon buffers,
						   0 otherwise */
	int pending_writes;			/* number of writes which
						   have not completed */
	RF_ReconBuffer_t *floatingRbufs;	/* available floating
						 * reconstruction buffers */
	RF_ReconBuffer_t *committedRbufs;	/* recon buffers that have
						 * been committed to some
						 * waiting disk */
	RF_ReconBuffer_t *fullBufferList;	/* full buffers waiting to be
						 * written out */
	RF_CallbackDesc_t *bufferWaitList;	/* disks that are currently
						 * blocked waiting for buffers */

	/* parity stripe status table */
	RF_PSStatusHeader_t *pssTable;	/* stores the reconstruction status of
					 * active parity stripes */

	/* maximum-head separation control */
	RF_HeadSepLimit_t minHeadSepCounter;	/* the minimum hs counter over
						 * all disks */
	RF_CallbackDesc_t *headSepCBList;	/* list of callbacks to be
						 * done as minPSID advances */

	/* performance monitoring */
	struct timeval starttime;	/* recon start time */
};
/* the default priority for reconstruction accesses */
#define RF_IO_RECON_PRIORITY RF_IO_LOW_PRIORITY

int rf_ConfigureReconstruction(RF_ShutdownList_t **);
int rf_ReconstructFailedDisk(RF_Raid_t *, RF_RowCol_t);
int rf_ReconstructFailedDiskBasic(RF_Raid_t *, RF_RowCol_t);
int rf_ReconstructInPlace(RF_Raid_t *, RF_RowCol_t);
int rf_ContinueReconstructFailedDisk(RF_RaidReconDesc_t *);
int rf_ForceOrBlockRecon(RF_Raid_t *, RF_AccessStripeMap_t *,
			 void (*cbFunc) (RF_Raid_t *, void *),
			 void *);
int rf_UnblockRecon(RF_Raid_t *, RF_AccessStripeMap_t *);
void rf_WakeupHeadSepCBWaiters(RF_Raid_t *);

extern struct pool rf_reconbuffer_pool;

#endif				/* !_RF__RF_RECONSTRUCT_H_ */
