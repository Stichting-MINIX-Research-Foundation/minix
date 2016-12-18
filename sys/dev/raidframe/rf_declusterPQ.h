/*	$NetBSD: rf_declusterPQ.h,v 1.7 2006/04/26 17:08:48 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
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

#ifndef _RF__RF_DECLUSTERPQ_H_
#define _RF__RF_DECLUSTERPQ_H_

#include <dev/raidframe/raidframevar.h>

int
rf_ConfigureDeclusteredPQ(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int     rf_GetDefaultNumFloatingReconBuffersPQ(RF_Raid_t * raidPtr);
void
rf_MapSectorDeclusteredPQ(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void
rf_MapParityDeclusteredPQ(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void
rf_MapQDeclusteredPQ(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void
rf_IdentifyStripeDeclusteredPQ(RF_Raid_t * raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids);

#endif				/* !_RF__RF_DECLUSTERPQ_H_ */
