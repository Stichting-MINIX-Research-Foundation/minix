/*	$NetBSD: rf_archs.h,v 1.26 2005/12/11 12:23:37 christos Exp $	*/
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

/* rf_archs.h -- defines for which architectures you want to
 * include is some particular build of raidframe.  Unfortunately,
 * it's difficult to exclude declustering, P+Q, and distributed
 * sparing because the code is intermixed with RAID5 code.  This
 * should be fixed.
 *
 * this is really intended only for use in the kernel, where I
 * am worried about the size of the object module.  At user level and
 * in the simulator, I don't really care that much, so all the
 * architectures can be compiled together.  Note that by itself, turning
 * off these defines does not affect the size of the executable; you
 * have to edit the makefile for that.
 *
 * comment out any line below to eliminate that architecture.
 * the list below includes all the modules that can be compiled
 * out.
 *
 */

#ifndef _RF__RF_ARCHS_H_
#define _RF__RF_ARCHS_H_

#ifndef RF_INCLUDE_EVENODD
#define RF_INCLUDE_EVENODD       0
#endif

#ifndef RF_INCLUDE_RAID5_RS
#define RF_INCLUDE_RAID5_RS      0
#endif

#ifndef RF_INCLUDE_PARITYLOGGING
#define RF_INCLUDE_PARITYLOGGING 0
#endif

#ifndef RF_INCLUDE_CHAINDECLUSTER
#define RF_INCLUDE_CHAINDECLUSTER 0
#endif

#ifndef RF_INCLUDE_INTERDECLUSTER
#define RF_INCLUDE_INTERDECLUSTER 0
#endif

#ifndef RF_INCLUDE_PARITY_DECLUSTERING
#define RF_INCLUDE_PARITY_DECLUSTERING 0
#endif

#ifndef RF_INCLUDE_PARITY_DECLUSTERING_DS
#define RF_INCLUDE_PARITY_DECLUSTERING_DS 0
#endif

#ifndef RF_INCLUDE_RAID0
#define RF_INCLUDE_RAID0   1
#endif

#ifndef RF_INCLUDE_RAID1
#define RF_INCLUDE_RAID1   1
#endif

#ifndef RF_INCLUDE_RAID4
#define RF_INCLUDE_RAID4   1
#endif

#ifndef RF_INCLUDE_RAID5
#define RF_INCLUDE_RAID5   1
#endif

#ifndef RF_INCLUDE_RAID6
#define RF_INCLUDE_RAID6   0
#endif

#ifndef RF_INCLUDE_DECL_PQ
#define RF_INCLUDE_DECL_PQ 0
#endif

#ifndef RF_MEMORY_REDZONES
#define RF_MEMORY_REDZONES 0
#endif

#ifndef RF_RECON_STATS
#define RF_RECON_STATS     1
#endif

#ifndef RF_DEBUG_DAG
#define RF_DEBUG_DAG 0
#endif

#ifndef RF_DEBUG_ENGINE
#define RF_DEBUG_ENGINE 0
#endif

#ifndef RF_DEBUG_VALIDATE_DAG
#define RF_DEBUG_VALIDATE_DAG 0
#endif

#ifndef RF_DEBUG_STRIPELOCK
#define RF_DEBUG_STRIPELOCK 0
#endif

#ifndef RF_DEBUG_RECON
#define RF_DEBUG_RECON 0
#endif

#ifndef RF_DEBUG_VERIFYPARITY
#define RF_DEBUG_VERIFYPARITY 0
#endif

#ifndef RF_DEBUG_QUEUE
#define RF_DEBUG_QUEUE 0
#endif

#ifndef RF_DEBUG_QUIESCE
#define RF_DEBUG_QUIESCE 0
#endif

#ifndef RF_DEBUG_PSS
#define RF_DEBUG_PSS 0
#endif

#ifndef RF_DEBUG_MAP
#define RF_DEBUG_MAP 0
#endif

#ifndef RF_ACC_TRACE
#define RF_ACC_TRACE 1
#endif

#include "rf_options.h"

#endif				/* !_RF__RF_ARCHS_H_ */
