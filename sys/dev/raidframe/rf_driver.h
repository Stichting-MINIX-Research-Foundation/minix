/*	$NetBSD: rf_driver.h,v 1.19 2011/04/30 01:44:36 mrg Exp $	*/
/*
 * rf_driver.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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

#ifndef _RF__RF_DRIVER_H_
#define _RF__RF_DRIVER_H_

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_netbsd.h"

#ifndef RF_RETRY_THRESHOLD
#define RF_RETRY_THRESHOLD 5
#endif

extern rf_declare_mutex2(rf_printf_mutex);
int rf_BootRaidframe(void);
int rf_UnbootRaidframe(void);
int rf_Shutdown(RF_Raid_t *);
int rf_Configure(RF_Raid_t *, RF_Config_t *, RF_AutoConfig_t *);
RF_RaidAccessDesc_t *rf_AllocRaidAccDesc(RF_Raid_t *, RF_IoType_t,
					 RF_RaidAddr_t, RF_SectorCount_t,
					 void *, void *,
					 RF_RaidAccessFlags_t,
					 const RF_AccessState_t *);
void rf_FreeRaidAccDesc(RF_RaidAccessDesc_t *);
int rf_DoAccess(RF_Raid_t *, RF_IoType_t, int, RF_RaidAddr_t,
		RF_SectorCount_t, void *, struct buf *,
		RF_RaidAccessFlags_t);
#if 0
int rf_SetReconfiguredMode(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
#endif
int rf_FailDisk(RF_Raid_t *, RF_RowCol_t, int);
void rf_SignalQuiescenceLock(RF_Raid_t *);
int rf_SuspendNewRequestsAndWait(RF_Raid_t *);
void rf_ResumeNewRequests(RF_Raid_t *);

#endif				/* !_RF__RF_DRIVER_H_ */
