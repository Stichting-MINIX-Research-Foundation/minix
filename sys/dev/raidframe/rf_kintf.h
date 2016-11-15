/*	$NetBSD: rf_kintf.h,v 1.23 2011/08/03 14:44:38 oster Exp $	*/
/*
 * rf_kintf.h
 *
 * RAIDframe exported kernel interface
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef _RF__RF_KINTF_H_
#define _RF__RF_KINTF_H_

#include <dev/raidframe/raidframevar.h>

int     rf_GetSpareTableFromDaemon(RF_SparetWait_t * req);
int rf_reasonable_label(RF_ComponentLabel_t *, uint64_t);


void    raidstart(RF_Raid_t * raidPtr);
int     rf_DispatchKernelIO(RF_DiskQueue_t * queue, RF_DiskQueueData_t * req);

int raidfetch_component_label(RF_Raid_t *, RF_RowCol_t);
RF_ComponentLabel_t *raidget_component_label(RF_Raid_t *, RF_RowCol_t);
int raidflush_component_label(RF_Raid_t *, RF_RowCol_t);

void rf_paritymap_kern_write(RF_Raid_t *, struct rf_paritymap_ondisk *);
void rf_paritymap_kern_read(RF_Raid_t *, struct rf_paritymap_ondisk *);

#define RF_NORMAL_COMPONENT_UPDATE 0
#define RF_FINAL_COMPONENT_UPDATE 1
void rf_update_component_labels(RF_Raid_t *, int);
int raidmarkclean(RF_Raid_t *, RF_RowCol_t);
int raidmarkdirty(RF_Raid_t *, RF_RowCol_t);
void raid_init_component_label(RF_Raid_t *, RF_ComponentLabel_t *);
void rf_print_component_label(RF_ComponentLabel_t *);
void rf_UnconfigureVnodes( RF_Raid_t * );
void rf_close_component( RF_Raid_t *, struct vnode *, int);
void rf_disk_unbusy(RF_RaidAccessDesc_t *);
int rf_getdisksize(struct vnode *, RF_RaidDisk_t *);
int rf_sync_component_caches(RF_Raid_t *raidPtr);
#endif				/* _RF__RF_KINTF_H_ */

