/*	$NetBSD: rf_parityscan.h,v 1.8 2009/11/17 18:54:26 jld Exp $	*/
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

#ifndef _RF__RF_PARITYSCAN_H_
#define _RF__RF_PARITYSCAN_H_

#include <dev/raidframe/raidframevar.h>

#include "rf_alloclist.h"

int     rf_RewriteParity(RF_Raid_t *);
int	rf_RewriteParityRange(RF_Raid_t *, RF_SectorNum_t, RF_SectorNum_t);
int 	rf_VerifyParityBasic(RF_Raid_t *, RF_RaidAddr_t, RF_PhysDiskAddr_t *,
    int, RF_RaidAccessFlags_t);
int	rf_VerifyParity(RF_Raid_t *, RF_AccessStripeMap_t *, int,
    RF_RaidAccessFlags_t);
int     rf_TryToRedirectPDA(RF_Raid_t *, RF_PhysDiskAddr_t *, int);
int     rf_VerifyDegrModeWrite(RF_Raid_t *, RF_AccessStripeMapHeader_t *);
RF_DagHeader_t *rf_MakeSimpleDAG(RF_Raid_t *, int, int, char *,
    int (*)(RF_DagNode_t *), int (*) (RF_DagNode_t *), const char *,
    RF_AllocListElem_t *, RF_RaidAccessFlags_t, int);

#define RF_DO_CORRECT_PARITY   1
#define RF_DONT_CORRECT_PARITY 0

/*
 * Return vals for VerifyParity operation
 *
 * Ordering is important here.
 */
#define RF_PARITY_OKAY               0	/* or no parity information */
#define RF_PARITY_CORRECTED          1
#define RF_PARITY_BAD                2
#define RF_PARITY_COULD_NOT_CORRECT  3
#define RF_PARITY_COULD_NOT_VERIFY   4

#endif				/* !_RF__RF_PARITYSCAN_H_ */
