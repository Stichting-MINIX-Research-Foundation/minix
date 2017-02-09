/*	$NetBSD: rf_states.c,v 1.49 2011/05/11 18:13:12 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Robby Findler
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_states.c,v 1.49 2011/05/11 18:13:12 mrg Exp $");

#include <sys/errno.h>

#include "rf_archs.h"
#include "rf_threadstuff.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_desc.h"
#include "rf_aselect.h"
#include "rf_general.h"
#include "rf_states.h"
#include "rf_dagutils.h"
#include "rf_driver.h"
#include "rf_engine.h"
#include "rf_map.h"
#include "rf_etimer.h"
#include "rf_kintf.h"
#include "rf_paritymap.h"

#ifndef RF_DEBUG_STATES
#define RF_DEBUG_STATES 0
#endif

/* prototypes for some of the available states.

   States must:

     - not block.

     - either schedule rf_ContinueRaidAccess as a callback and return
       RF_TRUE, or complete all of their work and return RF_FALSE.

     - increment desc->state when they have finished their work.
*/

#if RF_DEBUG_STATES
static char *
StateName(RF_AccessState_t state)
{
	switch (state) {
		case rf_QuiesceState:return "QuiesceState";
	case rf_MapState:
		return "MapState";
	case rf_LockState:
		return "LockState";
	case rf_CreateDAGState:
		return "CreateDAGState";
	case rf_ExecuteDAGState:
		return "ExecuteDAGState";
	case rf_ProcessDAGState:
		return "ProcessDAGState";
	case rf_CleanupState:
		return "CleanupState";
	case rf_LastState:
		return "LastState";
	case rf_IncrAccessesCountState:
		return "IncrAccessesCountState";
	case rf_DecrAccessesCountState:
		return "DecrAccessesCountState";
	default:
		return "!!! UnnamedState !!!";
	}
}
#endif

void
rf_ContinueRaidAccess(RF_RaidAccessDesc_t *desc)
{
	int     suspended = RF_FALSE;
	int     current_state_index = desc->state;
	RF_AccessState_t current_state = desc->states[current_state_index];
#if RF_DEBUG_STATES
	int     unit = desc->raidPtr->raidid;
#endif

	do {

		current_state_index = desc->state;
		current_state = desc->states[current_state_index];

		switch (current_state) {

		case rf_QuiesceState:
			suspended = rf_State_Quiesce(desc);
			break;
		case rf_IncrAccessesCountState:
			suspended = rf_State_IncrAccessCount(desc);
			break;
		case rf_MapState:
			suspended = rf_State_Map(desc);
			break;
		case rf_LockState:
			suspended = rf_State_Lock(desc);
			break;
		case rf_CreateDAGState:
			suspended = rf_State_CreateDAG(desc);
			break;
		case rf_ExecuteDAGState:
			suspended = rf_State_ExecuteDAG(desc);
			break;
		case rf_ProcessDAGState:
			suspended = rf_State_ProcessDAG(desc);
			break;
		case rf_CleanupState:
			suspended = rf_State_Cleanup(desc);
			break;
		case rf_DecrAccessesCountState:
			suspended = rf_State_DecrAccessCount(desc);
			break;
		case rf_LastState:
			suspended = rf_State_LastState(desc);
			break;
		}

		/* after this point, we cannot dereference desc since
		 * desc may have been freed. desc is only freed in
		 * LastState, so if we renter this function or loop
		 * back up, desc should be valid. */

#if RF_DEBUG_STATES
		if (rf_printStatesDebug) {
			printf("raid%d: State: %-24s StateIndex: %3i desc: 0x%ld %s\n",
			       unit, StateName(current_state),
			       current_state_index, (long) desc,
			       suspended ? "callback scheduled" : "looping");
		}
#endif
	} while (!suspended && current_state != rf_LastState);

	return;
}


void
rf_ContinueDagAccess(RF_DagList_t *dagList)
{
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &(dagList->desc->tracerec);
	RF_Etimer_t timer;
#endif
	RF_RaidAccessDesc_t *desc;
	RF_DagHeader_t *dag_h;
	int     i;

	desc = dagList->desc;

#if RF_ACC_TRACE > 0
	timer = tracerec->timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.exec_us = RF_ETIMER_VAL_US(timer);
	RF_ETIMER_START(tracerec->timer);
#endif

	/* skip to dag which just finished */
	dag_h = dagList->dags;
	for (i = 0; i < dagList->numDagsDone; i++) {
		dag_h = dag_h->next;
	}

	/* check to see if retry is required */
	if (dag_h->status == rf_rollBackward) {
		/* when a dag fails, mark desc status as bad and allow
		 * all other dags in the desc to execute to
		 * completion.  then, free all dags and start over */
		desc->status = 1;	/* bad status */
#if 0
		printf("raid%d: DAG failure: %c addr 0x%lx "
		       "(%ld) nblk 0x%x (%d) buf 0x%lx state %d\n",
		       desc->raidPtr->raidid, desc->type,
		       (long) desc->raidAddress,
		       (long) desc->raidAddress, (int) desc->numBlocks,
		       (int) desc->numBlocks,
		       (unsigned long) (desc->bufPtr), desc->state);
#endif
	}
	dagList->numDagsDone++;
	rf_ContinueRaidAccess(desc);
}

int
rf_State_LastState(RF_RaidAccessDesc_t *desc)
{
	void    (*callbackFunc) (RF_CBParam_t) = desc->callbackFunc;
	RF_CBParam_t callbackArg;

	callbackArg.p = desc->callbackArg;

	/*
	 * We don't support non-async IO.
	 */
	KASSERT(desc->async_flag);

	/*
	 * That's all the IO for this one... unbusy the 'disk'.
	 */

	rf_disk_unbusy(desc);

	/*
	 * Wakeup any requests waiting to go.
	 */

	rf_lock_mutex2(desc->raidPtr->mutex);
	desc->raidPtr->openings++;
	rf_unlock_mutex2(desc->raidPtr->mutex);

	rf_lock_mutex2(desc->raidPtr->iodone_lock);
	rf_signal_cond2(desc->raidPtr->iodone_cv);
	rf_unlock_mutex2(desc->raidPtr->iodone_lock);

	/*
	 * The parity_map hook has to go here, because the iodone
	 * callback goes straight into the kintf layer.
	 */
	if (desc->raidPtr->parity_map != NULL &&
	    desc->type == RF_IO_TYPE_WRITE)
		rf_paritymap_end(desc->raidPtr->parity_map, 
		    desc->raidAddress, desc->numBlocks);

	/* printf("Calling biodone on 0x%x\n",desc->bp); */
	biodone(desc->bp);	/* access came through ioctl */

	if (callbackFunc)
		callbackFunc(callbackArg);
	rf_FreeRaidAccDesc(desc);

	return RF_FALSE;
}

int
rf_State_IncrAccessCount(RF_RaidAccessDesc_t *desc)
{
	RF_Raid_t *raidPtr;

	raidPtr = desc->raidPtr;
	/* Bummer. We have to do this to be 100% safe w.r.t. the increment
	 * below */
	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	raidPtr->accs_in_flight++;	/* used to detect quiescence */
	rf_unlock_mutex2(raidPtr->access_suspend_mutex);

	desc->state++;
	return RF_FALSE;
}

int
rf_State_DecrAccessCount(RF_RaidAccessDesc_t *desc)
{
	RF_Raid_t *raidPtr;

	raidPtr = desc->raidPtr;

	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	raidPtr->accs_in_flight--;
	if (raidPtr->accesses_suspended && raidPtr->accs_in_flight == 0) {
		rf_SignalQuiescenceLock(raidPtr);
	}
	rf_unlock_mutex2(raidPtr->access_suspend_mutex);

	desc->state++;
	return RF_FALSE;
}

int
rf_State_Quiesce(RF_RaidAccessDesc_t *desc)
{
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
#endif
	RF_CallbackDesc_t *cb;
	RF_Raid_t *raidPtr;
	int     suspended = RF_FALSE;
	int need_cb, used_cb;

	raidPtr = desc->raidPtr;

#if RF_ACC_TRACE > 0
	RF_ETIMER_START(timer);
	RF_ETIMER_START(desc->timer);
#endif

	need_cb = 0;
	used_cb = 0;
	cb = NULL;

	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	/* Do an initial check to see if we might need a callback structure */
	if (raidPtr->accesses_suspended) {
		need_cb = 1;
	}
	rf_unlock_mutex2(raidPtr->access_suspend_mutex);

	if (need_cb) {
		/* create a callback if we might need it...
		   and we likely do. */
		cb = rf_AllocCallbackDesc();
	}

	rf_lock_mutex2(raidPtr->access_suspend_mutex);
	if (raidPtr->accesses_suspended) {
		cb->callbackFunc = (void (*) (RF_CBParam_t)) rf_ContinueRaidAccess;
		cb->callbackArg.p = (void *) desc;
		cb->next = raidPtr->quiesce_wait_list;
		raidPtr->quiesce_wait_list = cb;
		suspended = RF_TRUE;
		used_cb = 1;
	}
	rf_unlock_mutex2(raidPtr->access_suspend_mutex);

	if ((need_cb == 1) && (used_cb == 0)) {
		rf_FreeCallbackDesc(cb);
	}

#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.suspend_ovhd_us += RF_ETIMER_VAL_US(timer);
#endif

#if RF_DEBUG_QUIESCE
	if (suspended && rf_quiesceDebug)
		printf("Stalling access due to quiescence lock\n");
#endif
	desc->state++;
	return suspended;
}

int
rf_State_Map(RF_RaidAccessDesc_t *desc)
{
	RF_Raid_t *raidPtr = desc->raidPtr;
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;

	RF_ETIMER_START(timer);
#endif

	if (!(desc->asmap = rf_MapAccess(raidPtr, desc->raidAddress, desc->numBlocks,
		    desc->bufPtr, RF_DONT_REMAP)))
		RF_PANIC();

#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.map_us = RF_ETIMER_VAL_US(timer);
#endif

	desc->state++;
	return RF_FALSE;
}

int
rf_State_Lock(RF_RaidAccessDesc_t *desc)
{
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
#endif
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_AccessStripeMap_t *asm_p;
	RF_StripeNum_t lastStripeID = -1;
	int     suspended = RF_FALSE;

#if RF_ACC_TRACE > 0
	RF_ETIMER_START(timer);
#endif

	/* acquire each lock that we don't already hold */
	for (asm_p = asmh->stripeMap; asm_p; asm_p = asm_p->next) {
		RF_ASSERT(RF_IO_IS_R_OR_W(desc->type));
		if (!rf_suppressLocksAndLargeWrites &&
		    asm_p->parityInfo &&
		    !(desc->flags & RF_DAG_SUPPRESS_LOCKS) &&
		    !(asm_p->flags & RF_ASM_FLAGS_LOCK_TRIED)) {
			asm_p->flags |= RF_ASM_FLAGS_LOCK_TRIED;
				/* locks must be acquired hierarchically */
			RF_ASSERT(asm_p->stripeID > lastStripeID);
			lastStripeID = asm_p->stripeID;

			RF_INIT_LOCK_REQ_DESC(asm_p->lockReqDesc, desc->type,
					      (void (*) (struct buf *)) rf_ContinueRaidAccess, desc, asm_p,
					      raidPtr->Layout.dataSectorsPerStripe);
			if (rf_AcquireStripeLock(raidPtr->lockTable, asm_p->stripeID,
						 &asm_p->lockReqDesc)) {
				suspended = RF_TRUE;
				break;
			}
		}
		if (desc->type == RF_IO_TYPE_WRITE &&
		    raidPtr->status == rf_rs_reconstructing) {
			if (!(asm_p->flags & RF_ASM_FLAGS_FORCE_TRIED)) {
				int     val;

				asm_p->flags |= RF_ASM_FLAGS_FORCE_TRIED;
				val = rf_ForceOrBlockRecon(raidPtr, asm_p,
							   (void (*) (RF_Raid_t *, void *)) rf_ContinueRaidAccess, desc);
				if (val == 0) {
					asm_p->flags |= RF_ASM_FLAGS_RECON_BLOCKED;
				} else {
					suspended = RF_TRUE;
					break;
				}
			} else {
#if RF_DEBUG_PSS > 0
				if (rf_pssDebug) {
					printf("raid%d: skipping force/block because already done, psid %ld\n",
					       desc->raidPtr->raidid,
					       (long) asm_p->stripeID);
				}
#endif
			}
		} else {
#if RF_DEBUG_PSS > 0
			if (rf_pssDebug) {
				printf("raid%d: skipping force/block because not write or not under recon, psid %ld\n",
				       desc->raidPtr->raidid,
				       (long) asm_p->stripeID);
			}
#endif
		}
	}
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.lock_us += RF_ETIMER_VAL_US(timer);
#endif
	if (suspended)
		return (RF_TRUE);

	desc->state++;
	return (RF_FALSE);
}
/*
 * the following three states create, execute, and post-process dags
 * the error recovery unit is a single dag.
 * by default, SelectAlgorithm creates an array of dags, one per parity stripe
 * in some tricky cases, multiple dags per stripe are created
 *   - dags within a parity stripe are executed sequentially (arbitrary order)
 *   - dags for distinct parity stripes are executed concurrently
 *
 * repeat until all dags complete successfully -or- dag selection fails
 *
 * while !done
 *   create dag(s) (SelectAlgorithm)
 *   if dag
 *     execute dag (DispatchDAG)
 *     if dag successful
 *       done (SUCCESS)
 *     else
 *       !done (RETRY - start over with new dags)
 *   else
 *     done (FAIL)
 */
int
rf_State_CreateDAG(RF_RaidAccessDesc_t *desc)
{
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
#endif
	RF_DagHeader_t *dag_h;
	RF_DagList_t *dagList;
	struct buf *bp;
	int     i, selectStatus;

	/* generate a dag for the access, and fire it off.  When the dag
	 * completes, we'll get re-invoked in the next state. */
#if RF_ACC_TRACE > 0
	RF_ETIMER_START(timer);
#endif
	/* SelectAlgorithm returns one or more dags */
	selectStatus = rf_SelectAlgorithm(desc, desc->flags | RF_DAG_SUPPRESS_LOCKS);
#if RF_DEBUG_VALIDATE_DAG
	if (rf_printDAGsDebug) {
		dagList = desc->dagList;
		for (i = 0; i < desc->numStripes; i++) {
			rf_PrintDAGList(dagList->dags);
			dagList = dagList->next;
		}
	}
#endif /* RF_DEBUG_VALIDATE_DAG */
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	/* update time to create all dags */
	tracerec->specific.user.dag_create_us = RF_ETIMER_VAL_US(timer);
#endif

	desc->status = 0;	/* good status */

	if (selectStatus || (desc->numRetries > RF_RETRY_THRESHOLD)) {
		/* failed to create a dag */
		/* this happens when there are too many faults or incomplete
		 * dag libraries */
		if (selectStatus) {
			printf("raid%d: failed to create a dag. "
			       "Too many component failures.\n",
			       desc->raidPtr->raidid);
		} else {
			printf("raid%d: IO failed after %d retries.\n",
			       desc->raidPtr->raidid, RF_RETRY_THRESHOLD);
		}

		desc->status = 1; /* bad status */
		/* skip straight to rf_State_Cleanup() */
		desc->state = rf_CleanupState;
		bp = (struct buf *)desc->bp;
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
	} else {
		/* bind dags to desc */
		dagList = desc->dagList;
		for (i = 0; i < desc->numStripes; i++) {
			dag_h = dagList->dags;
			while (dag_h) {
				dag_h->bp = (struct buf *) desc->bp;
#if RF_ACC_TRACE > 0
				dag_h->tracerec = tracerec;
#endif
				dag_h = dag_h->next;
			}
			dagList = dagList->next;
		}
		desc->flags |= RF_DAG_DISPATCH_RETURNED;
		desc->state++;	/* next state should be rf_State_ExecuteDAG */
	}
	return RF_FALSE;
}



/* the access has an list of dagLists, one dagList per parity stripe.
 * fire the first dag in each parity stripe (dagList).
 * dags within a stripe (dagList) must be executed sequentially
 *  - this preserves atomic parity update
 * dags for independents parity groups (stripes) are fired concurrently */

int
rf_State_ExecuteDAG(RF_RaidAccessDesc_t *desc)
{
	int     i;
	RF_DagHeader_t *dag_h;
	RF_DagList_t *dagList;

	/* next state is always rf_State_ProcessDAG important to do
	 * this before firing the first dag (it may finish before we
	 * leave this routine) */
	desc->state++;

	/* sweep dag array, a stripe at a time, firing the first dag
	 * in each stripe */
	dagList = desc->dagList;
	for (i = 0; i < desc->numStripes; i++) {
		RF_ASSERT(dagList->numDags > 0);
		RF_ASSERT(dagList->numDagsDone == 0);
		RF_ASSERT(dagList->numDagsFired == 0);
#if RF_ACC_TRACE > 0
		RF_ETIMER_START(dagList->tracerec.timer);
#endif
		/* fire first dag in this stripe */
		dag_h = dagList->dags;
		RF_ASSERT(dag_h);
		dagList->numDagsFired++;
		rf_DispatchDAG(dag_h, (void (*) (void *)) rf_ContinueDagAccess, dagList);
		dagList = dagList->next;
	}

	/* the DAG will always call the callback, even if there was no
	 * blocking, so we are always suspended in this state */
	return RF_TRUE;
}



/* rf_State_ProcessDAG is entered when a dag completes.
 * first, check to all dags in the access have completed
 * if not, fire as many dags as possible */

int
rf_State_ProcessDAG(RF_RaidAccessDesc_t *desc)
{
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_DagHeader_t *dag_h;
	int     i, j, done = RF_TRUE;
	RF_DagList_t *dagList, *temp;

	/* check to see if this is the last dag */
	dagList = desc->dagList;
	for (i = 0; i < desc->numStripes; i++) {
		if (dagList->numDags != dagList->numDagsDone)
			done = RF_FALSE;
		dagList = dagList->next;
	}

	if (done) {
		if (desc->status) {
			/* a dag failed, retry */
			/* free all dags */
			dagList = desc->dagList;
			for (i = 0; i < desc->numStripes; i++) {
				rf_FreeDAG(dagList->dags);
				temp = dagList;
				dagList = dagList->next;
				rf_FreeDAGList(temp);
			}
			desc->dagList = NULL;

			rf_MarkFailuresInASMList(raidPtr, asmh);

			/* note the retry so that we'll bail in
			   rf_State_CreateDAG() once we've retired
			   the IO RF_RETRY_THRESHOLD times */

			desc->numRetries++;

			/* back up to rf_State_CreateDAG */
			desc->state = desc->state - 2;
			return RF_FALSE;
		} else {
			/* move on to rf_State_Cleanup */
			desc->state++;
		}
		return RF_FALSE;
	} else {
		/* more dags to execute */
		/* see if any are ready to be fired.  if so, fire them */
		/* don't fire the initial dag in a list, it's fired in
		 * rf_State_ExecuteDAG */
		dagList = desc->dagList;
		for (i = 0; i < desc->numStripes; i++) {
			if ((dagList->numDagsDone < dagList->numDags)
			    && (dagList->numDagsDone == dagList->numDagsFired)
			    && (dagList->numDagsFired > 0)) {
#if RF_ACC_TRACE > 0
				RF_ETIMER_START(dagList->tracerec.timer);
#endif
				/* fire next dag in this stripe */
				/* first, skip to next dag awaiting execution */
				dag_h = dagList->dags;
				for (j = 0; j < dagList->numDagsDone; j++)
					dag_h = dag_h->next;
				dagList->numDagsFired++;
				rf_DispatchDAG(dag_h, (void (*) (void *)) rf_ContinueDagAccess,
				    dagList);
			}
			dagList = dagList->next;
		}
		return RF_TRUE;
	}
}
/* only make it this far if all dags complete successfully */
int
rf_State_Cleanup(RF_RaidAccessDesc_t *desc)
{
#if RF_ACC_TRACE > 0
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
#endif
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_AccessStripeMap_t *asm_p;
	RF_DagList_t *dagList;
	int i;

	desc->state++;

#if RF_ACC_TRACE > 0
	timer = tracerec->timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.dag_retry_us = RF_ETIMER_VAL_US(timer);

	/* the RAID I/O is complete.  Clean up. */
	tracerec->specific.user.dag_retry_us = 0;

	RF_ETIMER_START(timer);
#endif
	/* free all dags */
	dagList = desc->dagList;
	for (i = 0; i < desc->numStripes; i++) {
		rf_FreeDAG(dagList->dags);
		dagList = dagList->next;
	}
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.cleanup_us = RF_ETIMER_VAL_US(timer);

	RF_ETIMER_START(timer);
#endif
	for (asm_p = asmh->stripeMap; asm_p; asm_p = asm_p->next) {
		if (!rf_suppressLocksAndLargeWrites &&
		    asm_p->parityInfo &&
		    !(desc->flags & RF_DAG_SUPPRESS_LOCKS)) {
			RF_ASSERT_VALID_LOCKREQ(&asm_p->lockReqDesc);
			rf_ReleaseStripeLock(raidPtr->lockTable,
					     asm_p->stripeID,
					     &asm_p->lockReqDesc);
		}
		if (asm_p->flags & RF_ASM_FLAGS_RECON_BLOCKED) {
			rf_UnblockRecon(raidPtr, asm_p);
		}
	}
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.lock_us += RF_ETIMER_VAL_US(timer);

	RF_ETIMER_START(timer);
#endif
	rf_FreeAccessStripeMap(asmh);
#if RF_ACC_TRACE > 0
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.cleanup_us += RF_ETIMER_VAL_US(timer);

	RF_ETIMER_STOP(desc->timer);
	RF_ETIMER_EVAL(desc->timer);

	timer = desc->tracerec.tot_timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	desc->tracerec.total_us = RF_ETIMER_VAL_US(timer);

	rf_LogTraceRec(raidPtr, tracerec);
#endif
	desc->flags |= RF_DAG_ACCESS_COMPLETE;

	return RF_FALSE;
}
