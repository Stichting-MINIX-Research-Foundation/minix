/*	$NetBSD: rf_raid5.c,v 1.19 2006/11/16 01:33:23 christos Exp $	*/
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
 *
 * rf_raid5.c -- implements RAID Level 5
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_raid5.c,v 1.19 2006/11/16 01:33:23 christos Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_raid.h"
#include "rf_raid5.h"
#include "rf_dag.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagdegwr.h"
#include "rf_dagutils.h"
#include "rf_general.h"
#include "rf_map.h"
#include "rf_utils.h"

typedef struct RF_Raid5ConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;	/* filled in at config time and used
					 * by IdentifyStripe */
}       RF_Raid5ConfigInfo_t;

int
rf_ConfigureRAID5(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
		  RF_Config_t *cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid5ConfigInfo_t *info;
	RF_RowCol_t i, j, startdisk;

	/* create a RAID level 5 configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_Raid5ConfigInfo_t), (RF_Raid5ConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	/* the stripe identifier must identify the disks in each stripe, IN
	 * THE ORDER THAT THEY APPEAR IN THE STRIPE. */
	info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, raidPtr->numCol, raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	startdisk = 0;
	for (i = 0; i < raidPtr->numCol; i++) {
		for (j = 0; j < raidPtr->numCol; j++) {
			info->stripeIdentifier[i][j] = (startdisk + j) % raidPtr->numCol;
		}
		if ((--startdisk) < 0)
			startdisk = raidPtr->numCol - 1;
	}

	/* fill in the remaining layout parameters */
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
	layoutPtr->numDataCol = raidPtr->numCol - 1;
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 1;
	layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;

	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

	return (0);
}

int
rf_GetDefaultNumFloatingReconBuffersRAID5(RF_Raid_t *raidPtr)
{
	return (20);
}

RF_HeadSepLimit_t
rf_GetDefaultHeadSepLimitRAID5(RF_Raid_t *raidPtr)
{
	return (10);
}
#if !defined(__NetBSD__) && !defined(_KERNEL)
/* not currently used */
int
rf_ShutdownRAID5(RF_Raid_t *raidPtr)
{
	return (0);
}
#endif

void
rf_MapSectorRAID5(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	*col = (SUID % raidPtr->numCol);
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void
rf_MapParityRAID5(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

	*col = raidPtr->Layout.numDataCol - (SUID / raidPtr->Layout.numDataCol) % raidPtr->numCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void
rf_IdentifyStripeRAID5(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
		       RF_RowCol_t **diskids)
{
	RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
	RF_Raid5ConfigInfo_t *info = (RF_Raid5ConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

	*diskids = info->stripeIdentifier[stripeID % raidPtr->numCol];
}

void
rf_MapSIDToPSIDRAID5(RF_RaidLayout_t *layoutPtr,
		     RF_StripeNum_t stripeID,
		     RF_StripeNum_t *psID, RF_ReconUnitNum_t *which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}
/* select an algorithm for performing an access.  Returns two pointers,
 * one to a function that will return information about the DAG, and
 * another to a function that will create the dag.
 */
void
rf_RaidFiveDagSelect(RF_Raid_t *raidPtr, RF_IoType_t type,
		     RF_AccessStripeMap_t *asmap,
		     RF_VoidFuncPtr *createFunc)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_PhysDiskAddr_t *failedPDA = NULL;
	RF_RowCol_t fcol;
	RF_RowStatus_t rstat;
	int     prior_recon;

	RF_ASSERT(RF_IO_IS_R_OR_W(type));

	if ((asmap->numDataFailed + asmap->numParityFailed > 1) ||
	    (raidPtr->numFailures > 1)){
#if RF_DEBUG_DAG
		if (rf_dagDebug)
			RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
#endif
		*createFunc = NULL;
		return;
	}

	if (asmap->numDataFailed + asmap->numParityFailed == 1) {

		/* if under recon & already reconstructed, redirect
		 * the access to the spare drive and eliminate the
		 * failure indication */
		failedPDA = asmap->failedPDAs[0];
		fcol = failedPDA->col;
		rstat = raidPtr->status;
		prior_recon = (rstat == rf_rs_reconfigured) || (
			    (rstat == rf_rs_reconstructing) ?
			    rf_CheckRUReconstructed(raidPtr->reconControl->reconMap, failedPDA->startSector) : 0
			    );
		if (prior_recon) {
#if RF_DEBUG_DAG > 0 || RF_DEBUG_MAP > 0
			RF_RowCol_t oc = failedPDA->col;
			RF_SectorNum_t oo = failedPDA->startSector;
#endif
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
			if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {	/* redirect to dist
										 * spare space */

				if (failedPDA == asmap->parityInfo) {

					/* parity has failed */
					(layoutPtr->map->MapParity) (raidPtr, failedPDA->raidAddress,
								     &failedPDA->col, &failedPDA->startSector, RF_REMAP);

					if (asmap->parityInfo->next) {	/* redir 2nd component,
									 * if any */
						RF_PhysDiskAddr_t *p = asmap->parityInfo->next;
						RF_SectorNum_t SUoffs = p->startSector % layoutPtr->sectorsPerStripeUnit;
						p->col = failedPDA->col;
						p->startSector = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, failedPDA->startSector) +
							SUoffs;	/* cheating:
								 * startSector is not
								 * really a RAID address */
					}
				} else
					if (asmap->parityInfo->next && failedPDA == asmap->parityInfo->next) {
						RF_ASSERT(0);	/* should not ever
								 * happen */
					} else {

						/* data has failed */
						(layoutPtr->map->MapSector) (raidPtr, failedPDA->raidAddress,
									     &failedPDA->col, &failedPDA->startSector, RF_REMAP);

					}

			} else {
#endif
				/* redirect to dedicated spare space */

				failedPDA->col = raidPtr->Disks[fcol].spareCol;

				/* the parity may have two distinct
				 * components, both of which may need
				 * to be redirected */
				if (asmap->parityInfo->next) {
					if (failedPDA == asmap->parityInfo) {
						failedPDA->next->col = failedPDA->col;
					} else
						if (failedPDA == asmap->parityInfo->next) {	/* paranoid:  should
												 * never occur */
							asmap->parityInfo->col = failedPDA->col;
						}
				}
#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
			}
#endif
			RF_ASSERT(failedPDA->col != -1);

#if RF_DEBUG_DAG > 0 || RF_DEBUG_MAP > 0
			if (rf_dagDebug || rf_mapDebug) {
				printf("raid%d: Redirected type '%c' c %d o %ld -> c %d o %ld\n",
				       raidPtr->raidid, type, oc,
				       (long) oo, failedPDA->col,
				       (long) failedPDA->startSector);
			}
#endif
			asmap->numDataFailed = asmap->numParityFailed = 0;
		}
	}
	/* all dags begin/end with block/unblock node therefore, hdrSucc &
	 * termAnt counts should always be 1 also, these counts should not be
	 * visible outside dag creation routines - manipulating the counts
	 * here should be removed */
	if (type == RF_IO_TYPE_READ) {
		if (asmap->numDataFailed == 0)
			*createFunc = (RF_VoidFuncPtr) rf_CreateFaultFreeReadDAG;
		else
			*createFunc = (RF_VoidFuncPtr) rf_CreateRaidFiveDegradedReadDAG;
	} else {


		/* if mirroring, always use large writes.  If the access
		 * requires two distinct parity updates, always do a small
		 * write.  If the stripe contains a failure but the access
		 * does not, do a small write. The first conditional
		 * (numStripeUnitsAccessed <= numDataCol/2) uses a
		 * less-than-or-equal rather than just a less-than because
		 * when G is 3 or 4, numDataCol/2 is 1, and I want
		 * single-stripe-unit updates to use just one disk. */
		if ((asmap->numDataFailed + asmap->numParityFailed) == 0) {
			if (rf_suppressLocksAndLargeWrites ||
			    (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) && (layoutPtr->numDataCol != 1)) ||
				(asmap->parityInfo->next != NULL) || rf_CheckStripeForFailures(raidPtr, asmap))) {
				*createFunc = (RF_VoidFuncPtr) rf_CreateSmallWriteDAG;
			} else
				*createFunc = (RF_VoidFuncPtr) rf_CreateLargeWriteDAG;
		} else {
			if (asmap->numParityFailed == 1)
				*createFunc = (RF_VoidFuncPtr) rf_CreateNonRedundantWriteDAG;
			else
				if (asmap->numStripeUnitsAccessed != 1 && (failedPDA == NULL || failedPDA->numSector != layoutPtr->sectorsPerStripeUnit))
					*createFunc = NULL;
				else
					*createFunc = (RF_VoidFuncPtr) rf_CreateDegradedWriteDAG;
		}
	}
}
