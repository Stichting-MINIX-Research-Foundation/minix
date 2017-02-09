/*	$NetBSD: rf_reconmap.h,v 1.12 2011/05/10 07:04:17 mrg Exp $	*/
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

/******************************************************************************
 * rf_reconMap.h -- Header file describing reconstruction status data structure
 ******************************************************************************/

#ifndef _RF__RF_RECONMAP_H_
#define _RF__RF_RECONMAP_H_

#include <dev/raidframe/raidframevar.h>
#include <sys/pool.h>

#include "rf_threadstuff.h"

/* the number of recon units in the status table. */
#define RF_RECONMAP_SIZE 32

/*
 * Main reconstruction status descriptor.
 */
struct RF_ReconMap_s {
	RF_SectorCount_t sectorsPerReconUnit;	/* sectors per reconstruct
						 * unit */
	RF_SectorCount_t sectorsInDisk;	/* total sectors in disk */
	RF_SectorCount_t unitsLeft;	/* recon units left to recon */
	RF_ReconUnitCount_t totalRUs;	/* total recon units on disk */
	RF_ReconUnitCount_t spareRUs;	/* total number of spare RUs on failed
					 * disk */
	RF_ReconUnitCount_t low_ru;     /* lowest reconstruction unit number in
					   the status array */
	RF_ReconUnitCount_t high_ru;    /* highest reconstruction unit number
					   in the status array */
	RF_ReconUnitCount_t head;       /* the position in the array where
					   low_ru is found */
	RF_ReconUnitCount_t status_size; /* number of recon units in status */
	RF_StripeCount_t totalParityStripes;	/* total number of parity
						 * stripes in array */
	RF_ReconMapListElem_t **status;	/* array of ptrs to list elements */
	struct pool elem_pool;          /* pool of RF_ReconMapListElem_t's */
	rf_declare_mutex2(mutex);
	rf_declare_cond2(cv);
	int lock;                       /* 1 if someone has the recon map
					   locked, 0 otherwise */
};
/* a list element */
struct RF_ReconMapListElem_s {
	RF_SectorNum_t startSector;	/* bounding sect nums on this block */
	RF_SectorNum_t stopSector;
	RF_ReconMapListElem_t *next;	/* next element in list */
};

RF_ReconMap_t *rf_MakeReconMap(RF_Raid_t *, RF_SectorCount_t,
			       RF_SectorCount_t, RF_ReconUnitCount_t);
void rf_ReconMapUpdate(RF_Raid_t *, RF_ReconMap_t *, RF_SectorNum_t, RF_SectorNum_t);
void rf_FreeReconMap(RF_ReconMap_t *);
int rf_CheckRUReconstructed(RF_ReconMap_t *, RF_SectorNum_t);
RF_ReconUnitCount_t rf_UnitsLeftToReconstruct(RF_ReconMap_t *);
void rf_PrintReconMap(RF_Raid_t *, RF_ReconMap_t *, RF_RowCol_t);
void rf_PrintReconSchedule(RF_ReconMap_t *, struct timeval *);

#endif				/* !_RF__RF_RECONMAP_H_ */
