/*	$NetBSD: rf_raid4.c,v 1.12 2006/11/16 01:33:23 christos Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Rachad Youssef
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

/***************************************
 *
 * rf_raid4.c -- implements RAID Level 4
 *
 ***************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_raid4.c,v 1.12 2006/11/16 01:33:23 christos Exp $");

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagdegwr.h"
#include "rf_raid4.h"
#include "rf_general.h"

typedef struct RF_Raid4ConfigInfo_s {
	RF_RowCol_t *stripeIdentifier;	/* filled in at config time & used by
					 * IdentifyStripe */
}       RF_Raid4ConfigInfo_t;



int
rf_ConfigureRAID4(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
		  RF_Config_t *cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid4ConfigInfo_t *info;
	int     i;

	/* create a RAID level 4 configuration structure ... */
	RF_MallocAndAdd(info, sizeof(RF_Raid4ConfigInfo_t), (RF_Raid4ConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	/* ... and fill it in. */
	RF_MallocAndAdd(info->stripeIdentifier, raidPtr->numCol * sizeof(RF_RowCol_t), (RF_RowCol_t *), raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	for (i = 0; i < raidPtr->numCol; i++)
		info->stripeIdentifier[i] = i;

	/* fill in the remaining layout parameters */
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
	layoutPtr->numDataCol = raidPtr->numCol - 1;
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 1;
	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

	return (0);
}

int
rf_GetDefaultNumFloatingReconBuffersRAID4(RF_Raid_t *raidPtr)
{
	return (20);
}

RF_HeadSepLimit_t
rf_GetDefaultHeadSepLimitRAID4(RF_Raid_t *raidPtr)
{
	return (20);
}

void
rf_MapSectorRAID4(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	*col = SUID % raidPtr->Layout.numDataCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void
rf_MapParityRAID4(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
		  RF_RowCol_t *col, RF_SectorNum_t *diskSector,
		  int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

	*col = raidPtr->Layout.numDataCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void
rf_IdentifyStripeRAID4(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
		       RF_RowCol_t **diskids)
{
	RF_Raid4ConfigInfo_t *info = raidPtr->Layout.layoutSpecificInfo;

	*diskids = info->stripeIdentifier;
}

void
rf_MapSIDToPSIDRAID4(RF_RaidLayout_t *layoutPtr,
		     RF_StripeNum_t stripeID,
		     RF_StripeNum_t *psID, RF_ReconUnitNum_t *which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}
