/*	$NetBSD: rf_acctrace.c,v 1.24 2011/05/01 06:22:54 mrg Exp $	*/
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

/*****************************************************************************
 *
 * acctrace.c -- code to support collecting information about each access
 *
 *****************************************************************************/


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_acctrace.c,v 1.24 2011/05/01 06:22:54 mrg Exp $");

#include <sys/stat.h>
#include <sys/types.h>
#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_debugMem.h"
#include "rf_acctrace.h"
#include "rf_general.h"
#include "rf_raid.h"
#include "rf_etimer.h"
#include "rf_hist.h"
#include "rf_shutdown.h"

#if RF_ACC_TRACE > 0
static long numTracesSoFar;

rf_declare_mutex2(rf_tracing_mutex);

static void
rf_ShutdownAccessTrace(void *unused)
{
	rf_destroy_mutex2(rf_tracing_mutex);
}

int
rf_ConfigureAccessTrace(RF_ShutdownList_t **listp)
{
	numTracesSoFar = 0;
	rf_init_mutex2(rf_tracing_mutex, IPL_VM);
	rf_ShutdownCreate(listp, rf_ShutdownAccessTrace, NULL);
	return (0);
}

/* install a trace record.  cause a flush to disk or to the trace
 * collector daemon if the trace buffer is at least 1/2 full.
 */
void
rf_LogTraceRec(RF_Raid_t *raid, RF_AccTraceEntry_t *rec)
{
	RF_AccTotals_t *acc = &raid->acc_totals;

	if (((rf_maxNumTraces >= 0) && (numTracesSoFar >= rf_maxNumTraces)))
		return;

	/* update AccTotals for this device */
	if (!raid->keep_acc_totals)
		return;
	acc->num_log_ents++;
	if (rec->reconacc) {
		acc->recon_start_to_fetch_us += rec->specific.recon.recon_start_to_fetch_us;
		acc->recon_fetch_to_return_us += rec->specific.recon.recon_fetch_to_return_us;
		acc->recon_return_to_submit_us += rec->specific.recon.recon_return_to_submit_us;
		acc->recon_num_phys_ios += rec->num_phys_ios;
		acc->recon_phys_io_us += rec->phys_io_us;
		acc->recon_diskwait_us += rec->diskwait_us;
		acc->recon_reccount++;
	} else {
		RF_HIST_ADD(acc->tot_hist, rec->total_us);
		RF_HIST_ADD(acc->dw_hist, rec->diskwait_us);
		/* count of physical ios which are too big.  often due to
		 * thermal recalibration */
		/* if bigvals > 0, you should probably ignore this data set */
		if (rec->diskwait_us > 100000)
			acc->bigvals++;
		acc->total_us += rec->total_us;
		acc->suspend_ovhd_us += rec->specific.user.suspend_ovhd_us;
		acc->map_us += rec->specific.user.map_us;
		acc->lock_us += rec->specific.user.lock_us;
		acc->dag_create_us += rec->specific.user.dag_create_us;
		acc->dag_retry_us += rec->specific.user.dag_retry_us;
		acc->exec_us += rec->specific.user.exec_us;
		acc->cleanup_us += rec->specific.user.cleanup_us;
		acc->exec_engine_us += rec->specific.user.exec_engine_us;
		acc->xor_us += rec->xor_us;
		acc->q_us += rec->q_us;
		acc->plog_us += rec->plog_us;
		acc->diskqueue_us += rec->diskqueue_us;
		acc->diskwait_us += rec->diskwait_us;
		acc->num_phys_ios += rec->num_phys_ios;
		acc->phys_io_us = rec->phys_io_us;
		acc->user_reccount++;
	}
}
#endif /* RF_ACC_TRACE > 0 */

