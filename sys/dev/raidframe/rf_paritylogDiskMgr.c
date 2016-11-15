/*	$NetBSD: rf_paritylogDiskMgr.c,v 1.28 2011/05/11 06:20:33 mrg Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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
/* Code for flushing and reintegration operations related to parity logging.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_paritylogDiskMgr.c,v 1.28 2011/05/11 06:20:33 mrg Exp $");

#include "rf_archs.h"

#if RF_INCLUDE_PARITYLOGGING > 0

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_mcpair.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagfuncs.h"
#include "rf_desc.h"
#include "rf_layout.h"
#include "rf_diskqueue.h"
#include "rf_paritylog.h"
#include "rf_general.h"
#include "rf_etimer.h"
#include "rf_paritylogging.h"
#include "rf_engine.h"
#include "rf_dagutils.h"
#include "rf_map.h"
#include "rf_parityscan.h"

#include "rf_paritylogDiskMgr.h"

static void *AcquireReintBuffer(RF_RegionBufferQueue_t *);

static void *
AcquireReintBuffer(RF_RegionBufferQueue_t *pool)
{
	void *bufPtr = NULL;

	/* Return a region buffer from the free list (pool). If the free list
	 * is empty, WAIT. BLOCKING */

	rf_lock_mutex2(pool->mutex);
	if (pool->availableBuffers > 0) {
		bufPtr = pool->buffers[pool->availBuffersIndex];
		pool->availableBuffers--;
		pool->availBuffersIndex++;
		if (pool->availBuffersIndex == pool->totalBuffers)
			pool->availBuffersIndex = 0;
		rf_unlock_mutex2(pool->mutex);
	} else {
		RF_PANIC();	/* should never happen in correct config,
				 * single reint */
		rf_wait_cond2(pool->cond, pool->mutex);
	}
	return (bufPtr);
}

static void
ReleaseReintBuffer(
    RF_RegionBufferQueue_t * pool,
    void *bufPtr)
{
	/* Insert a region buffer (bufPtr) into the free list (pool).
	 * NON-BLOCKING */

	rf_lock_mutex2(pool->mutex);
	pool->availableBuffers++;
	pool->buffers[pool->emptyBuffersIndex] = bufPtr;
	pool->emptyBuffersIndex++;
	if (pool->emptyBuffersIndex == pool->totalBuffers)
		pool->emptyBuffersIndex = 0;
	RF_ASSERT(pool->availableBuffers <= pool->totalBuffers);
	/*
	 * XXXmrg this signal goes with the above "shouldn't happen" wait?
	 */
	rf_signal_cond2(pool->cond);
	rf_unlock_mutex2(pool->mutex);
}



static void
ReadRegionLog(
    RF_RegionId_t regionID,
    RF_MCPair_t * rrd_mcpair,
    void *regionBuffer,
    RF_Raid_t * raidPtr,
    RF_DagHeader_t ** rrd_dag_h,
    RF_AllocListElem_t ** rrd_alloclist,
    RF_PhysDiskAddr_t ** rrd_pda)
{
	/* Initiate the read a region log from disk.  Once initiated, return
	 * to the calling routine.
	 *
	 * NON-BLOCKING */

	RF_AccTraceEntry_t *tracerec;
	RF_DagNode_t *rrd_rdNode;

	/* create DAG to read region log from disk */
	rf_MakeAllocList(*rrd_alloclist);
	*rrd_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, regionBuffer,
				      rf_DiskReadFunc, rf_DiskReadUndoFunc,
				      "Rrl", *rrd_alloclist,
				      RF_DAG_FLAGS_NONE,
				      RF_IO_NORMAL_PRIORITY);

	/* create and initialize PDA for the core log */
	/* RF_Malloc(*rrd_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t
	 * *)); */
	*rrd_pda = rf_AllocPDAList(1);
	rf_MapLogParityLogging(raidPtr, regionID, 0,
			       &((*rrd_pda)->col), &((*rrd_pda)->startSector));
	(*rrd_pda)->numSector = raidPtr->regionInfo[regionID].capacity;

	if ((*rrd_pda)->next) {
		(*rrd_pda)->next = NULL;
		printf("set rrd_pda->next to NULL\n");
	}
	/* initialize DAG parameters */
	RF_Malloc(tracerec,sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
	memset((char *) tracerec, 0, sizeof(RF_AccTraceEntry_t));
	(*rrd_dag_h)->tracerec = tracerec;
	rrd_rdNode = (*rrd_dag_h)->succedents[0]->succedents[0];
	rrd_rdNode->params[0].p = *rrd_pda;
/*  rrd_rdNode->params[1] = regionBuffer; */
	rrd_rdNode->params[2].v = 0;
	rrd_rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0);

	/* launch region log read dag */
	rf_DispatchDAG(*rrd_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) rrd_mcpair);
}



static void
WriteCoreLog(
    RF_ParityLog_t * log,
    RF_MCPair_t * fwr_mcpair,
    RF_Raid_t * raidPtr,
    RF_DagHeader_t ** fwr_dag_h,
    RF_AllocListElem_t ** fwr_alloclist,
    RF_PhysDiskAddr_t ** fwr_pda)
{
	RF_RegionId_t regionID = log->regionID;
	RF_AccTraceEntry_t *tracerec;
	RF_SectorNum_t regionOffset;
	RF_DagNode_t *fwr_wrNode;

	/* Initiate the write of a core log to a region log disk. Once
	 * initiated, return to the calling routine.
	 *
	 * NON-BLOCKING */

	/* create DAG to write a core log to a region log disk */
	rf_MakeAllocList(*fwr_alloclist);
	*fwr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, log->bufPtr,
				      rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
	    "Wcl", *fwr_alloclist, RF_DAG_FLAGS_NONE, RF_IO_NORMAL_PRIORITY);

	/* create and initialize PDA for the region log */
	/* RF_Malloc(*fwr_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t
	 * *)); */
	*fwr_pda = rf_AllocPDAList(1);
	regionOffset = log->diskOffset;
	rf_MapLogParityLogging(raidPtr, regionID, regionOffset,
			       &((*fwr_pda)->col),
			       &((*fwr_pda)->startSector));
	(*fwr_pda)->numSector = raidPtr->numSectorsPerLog;

	/* initialize DAG parameters */
	RF_Malloc(tracerec,sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
	memset((char *) tracerec, 0, sizeof(RF_AccTraceEntry_t));
	(*fwr_dag_h)->tracerec = tracerec;
	fwr_wrNode = (*fwr_dag_h)->succedents[0]->succedents[0];
	fwr_wrNode->params[0].p = *fwr_pda;
/*  fwr_wrNode->params[1] = log->bufPtr; */
	fwr_wrNode->params[2].v = 0;
	fwr_wrNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0);

	/* launch the dag to write the core log to disk */
	rf_DispatchDAG(*fwr_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) fwr_mcpair);
}


static void
ReadRegionParity(
    RF_RegionId_t regionID,
    RF_MCPair_t * prd_mcpair,
    void *parityBuffer,
    RF_Raid_t * raidPtr,
    RF_DagHeader_t ** prd_dag_h,
    RF_AllocListElem_t ** prd_alloclist,
    RF_PhysDiskAddr_t ** prd_pda)
{
	/* Initiate the read region parity from disk. Once initiated, return
	 * to the calling routine.
	 *
	 * NON-BLOCKING */

	RF_AccTraceEntry_t *tracerec;
	RF_DagNode_t *prd_rdNode;

	/* create DAG to read region parity from disk */
	rf_MakeAllocList(*prd_alloclist);
	*prd_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, NULL, rf_DiskReadFunc,
				      rf_DiskReadUndoFunc, "Rrp",
				      *prd_alloclist, RF_DAG_FLAGS_NONE,
				      RF_IO_NORMAL_PRIORITY);

	/* create and initialize PDA for region parity */
	/* RF_Malloc(*prd_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t
	 * *)); */
	*prd_pda = rf_AllocPDAList(1);
	rf_MapRegionParity(raidPtr, regionID,
			   &((*prd_pda)->col), &((*prd_pda)->startSector),
			   &((*prd_pda)->numSector));
	if (rf_parityLogDebug)
		printf("[reading %d sectors of parity from region %d]\n",
		    (int) (*prd_pda)->numSector, regionID);
	if ((*prd_pda)->next) {
		(*prd_pda)->next = NULL;
		printf("set prd_pda->next to NULL\n");
	}
	/* initialize DAG parameters */
	RF_Malloc(tracerec,sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
	memset((char *) tracerec, 0, sizeof(RF_AccTraceEntry_t));
	(*prd_dag_h)->tracerec = tracerec;
	prd_rdNode = (*prd_dag_h)->succedents[0]->succedents[0];
	prd_rdNode->params[0].p = *prd_pda;
	prd_rdNode->params[1].p = parityBuffer;
	prd_rdNode->params[2].v = 0;
	prd_rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0);
#if RF_DEBUG_VALIDATE_DAG
	if (rf_validateDAGDebug)
		rf_ValidateDAG(*prd_dag_h);
#endif
	/* launch region parity read dag */
	rf_DispatchDAG(*prd_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) prd_mcpair);
}

static void
WriteRegionParity(
    RF_RegionId_t regionID,
    RF_MCPair_t * pwr_mcpair,
    void *parityBuffer,
    RF_Raid_t * raidPtr,
    RF_DagHeader_t ** pwr_dag_h,
    RF_AllocListElem_t ** pwr_alloclist,
    RF_PhysDiskAddr_t ** pwr_pda)
{
	/* Initiate the write of region parity to disk. Once initiated, return
	 * to the calling routine.
	 *
	 * NON-BLOCKING */

	RF_AccTraceEntry_t *tracerec;
	RF_DagNode_t *pwr_wrNode;

	/* create DAG to write region log from disk */
	rf_MakeAllocList(*pwr_alloclist);
	*pwr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, parityBuffer,
				      rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
				      "Wrp", *pwr_alloclist,
				      RF_DAG_FLAGS_NONE,
				      RF_IO_NORMAL_PRIORITY);

	/* create and initialize PDA for region parity */
	/* RF_Malloc(*pwr_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t
	 * *)); */
	*pwr_pda = rf_AllocPDAList(1);
	rf_MapRegionParity(raidPtr, regionID,
			   &((*pwr_pda)->col), &((*pwr_pda)->startSector),
			   &((*pwr_pda)->numSector));

	/* initialize DAG parameters */
	RF_Malloc(tracerec,sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
	memset((char *) tracerec, 0, sizeof(RF_AccTraceEntry_t));
	(*pwr_dag_h)->tracerec = tracerec;
	pwr_wrNode = (*pwr_dag_h)->succedents[0]->succedents[0];
	pwr_wrNode->params[0].p = *pwr_pda;
/*  pwr_wrNode->params[1] = parityBuffer; */
	pwr_wrNode->params[2].v = 0;
	pwr_wrNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0);

	/* launch the dag to write region parity to disk */
	rf_DispatchDAG(*pwr_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) pwr_mcpair);
}

static void
FlushLogsToDisk(
    RF_Raid_t * raidPtr,
    RF_ParityLog_t * logList)
{
	/* Flush a linked list of core logs to the log disk. Logs contain the
	 * disk location where they should be written.  Logs were written in
	 * FIFO order and that order must be preserved.
	 *
	 * Recommended optimizations: 1) allow multiple flushes to occur
	 * simultaneously 2) coalesce contiguous flush operations
	 *
	 * BLOCKING */

	RF_ParityLog_t *log;
	RF_RegionId_t regionID;
	RF_MCPair_t *fwr_mcpair;
	RF_DagHeader_t *fwr_dag_h;
	RF_AllocListElem_t *fwr_alloclist;
	RF_PhysDiskAddr_t *fwr_pda;

	fwr_mcpair = rf_AllocMCPair();
	RF_LOCK_MCPAIR(fwr_mcpair);

	RF_ASSERT(logList);
	log = logList;
	while (log) {
		regionID = log->regionID;

		/* create and launch a DAG to write the core log */
		if (rf_parityLogDebug)
			printf("[initiating write of core log for region %d]\n", regionID);
		fwr_mcpair->flag = RF_FALSE;
		WriteCoreLog(log, fwr_mcpair, raidPtr, &fwr_dag_h,
			     &fwr_alloclist, &fwr_pda);

		/* wait for the DAG to complete */
		while (!fwr_mcpair->flag)
			RF_WAIT_MCPAIR(fwr_mcpair);
		if (fwr_dag_h->status != rf_enable) {
			RF_ERRORMSG1("Unable to write core log to disk (region %d)\n", regionID);
			RF_ASSERT(0);
		}
		/* RF_Free(fwr_pda, sizeof(RF_PhysDiskAddr_t)); */
		rf_FreePhysDiskAddr(fwr_pda);
		rf_FreeDAG(fwr_dag_h);
		rf_FreeAllocList(fwr_alloclist);

		log = log->next;
	}
	RF_UNLOCK_MCPAIR(fwr_mcpair);
	rf_FreeMCPair(fwr_mcpair);
	rf_ReleaseParityLogs(raidPtr, logList);
}

static void
ReintegrateRegion(
    RF_Raid_t * raidPtr,
    RF_RegionId_t regionID,
    RF_ParityLog_t * coreLog)
{
	RF_MCPair_t *rrd_mcpair = NULL, *prd_mcpair, *pwr_mcpair;
	RF_DagHeader_t *rrd_dag_h = NULL, *prd_dag_h, *pwr_dag_h;
	RF_AllocListElem_t *rrd_alloclist = NULL, *prd_alloclist, *pwr_alloclist;
	RF_PhysDiskAddr_t *rrd_pda = NULL, *prd_pda, *pwr_pda;
	void *parityBuffer, *regionBuffer = NULL;

	/* Reintegrate a region (regionID).
	 *
	 * 1. acquire region and parity buffers
	 * 2. read log from disk
	 * 3. read parity from disk
	 * 4. apply log to parity
	 * 5. apply core log to parity
	 * 6. write new parity to disk
	 *
	 * BLOCKING */

	if (rf_parityLogDebug)
		printf("[reintegrating region %d]\n", regionID);

	/* initiate read of region parity */
	if (rf_parityLogDebug)
		printf("[initiating read of parity for region %d]\n",regionID);
	parityBuffer = AcquireReintBuffer(&raidPtr->parityBufferPool);
	prd_mcpair = rf_AllocMCPair();
	RF_LOCK_MCPAIR(prd_mcpair);
	prd_mcpair->flag = RF_FALSE;
	ReadRegionParity(regionID, prd_mcpair, parityBuffer, raidPtr,
			 &prd_dag_h, &prd_alloclist, &prd_pda);

	/* if region log nonempty, initiate read */
	if (raidPtr->regionInfo[regionID].diskCount > 0) {
		if (rf_parityLogDebug)
			printf("[initiating read of disk log for region %d]\n",
			       regionID);
		regionBuffer = AcquireReintBuffer(&raidPtr->regionBufferPool);
		rrd_mcpair = rf_AllocMCPair();
		RF_LOCK_MCPAIR(rrd_mcpair);
		rrd_mcpair->flag = RF_FALSE;
		ReadRegionLog(regionID, rrd_mcpair, regionBuffer, raidPtr,
			      &rrd_dag_h, &rrd_alloclist, &rrd_pda);
	}
	/* wait on read of region parity to complete */
	while (!prd_mcpair->flag) {
		RF_WAIT_MCPAIR(prd_mcpair);
	}
	RF_UNLOCK_MCPAIR(prd_mcpair);
	if (prd_dag_h->status != rf_enable) {
		RF_ERRORMSG("Unable to read parity from disk\n");
		/* add code to fail the parity disk */
		RF_ASSERT(0);
	}
	/* apply core log to parity */
	/* if (coreLog) ApplyLogsToParity(coreLog, parityBuffer); */

	if (raidPtr->regionInfo[regionID].diskCount > 0) {
		/* wait on read of region log to complete */
		while (!rrd_mcpair->flag)
			RF_WAIT_MCPAIR(rrd_mcpair);
		RF_UNLOCK_MCPAIR(rrd_mcpair);
		if (rrd_dag_h->status != rf_enable) {
			RF_ERRORMSG("Unable to read region log from disk\n");
			/* add code to fail the log disk */
			RF_ASSERT(0);
		}
		/* apply region log to parity */
		/* ApplyRegionToParity(regionID, regionBuffer, parityBuffer); */
		/* release resources associated with region log */
		/* RF_Free(rrd_pda, sizeof(RF_PhysDiskAddr_t)); */
		rf_FreePhysDiskAddr(rrd_pda);
		rf_FreeDAG(rrd_dag_h);
		rf_FreeAllocList(rrd_alloclist);
		rf_FreeMCPair(rrd_mcpair);
		ReleaseReintBuffer(&raidPtr->regionBufferPool, regionBuffer);
	}
	/* write reintegrated parity to disk */
	if (rf_parityLogDebug)
		printf("[initiating write of parity for region %d]\n",
		       regionID);
	pwr_mcpair = rf_AllocMCPair();
	RF_LOCK_MCPAIR(pwr_mcpair);
	pwr_mcpair->flag = RF_FALSE;
	WriteRegionParity(regionID, pwr_mcpair, parityBuffer, raidPtr,
			  &pwr_dag_h, &pwr_alloclist, &pwr_pda);
	while (!pwr_mcpair->flag)
		RF_WAIT_MCPAIR(pwr_mcpair);
	RF_UNLOCK_MCPAIR(pwr_mcpair);
	if (pwr_dag_h->status != rf_enable) {
		RF_ERRORMSG("Unable to write parity to disk\n");
		/* add code to fail the parity disk */
		RF_ASSERT(0);
	}
	/* release resources associated with read of old parity */
	/* RF_Free(prd_pda, sizeof(RF_PhysDiskAddr_t)); */
	rf_FreePhysDiskAddr(prd_pda);
	rf_FreeDAG(prd_dag_h);
	rf_FreeAllocList(prd_alloclist);
	rf_FreeMCPair(prd_mcpair);

	/* release resources associated with write of new parity */
	ReleaseReintBuffer(&raidPtr->parityBufferPool, parityBuffer);
	/* RF_Free(pwr_pda, sizeof(RF_PhysDiskAddr_t)); */
	rf_FreePhysDiskAddr(pwr_pda);
	rf_FreeDAG(pwr_dag_h);
	rf_FreeAllocList(pwr_alloclist);
	rf_FreeMCPair(pwr_mcpair);

	if (rf_parityLogDebug)
		printf("[finished reintegrating region %d]\n", regionID);
}



static void
ReintegrateLogs(
    RF_Raid_t * raidPtr,
    RF_ParityLog_t * logList)
{
	RF_ParityLog_t *log, *freeLogList = NULL;
	RF_ParityLogData_t *logData, *logDataList;
	RF_RegionId_t regionID;

	RF_ASSERT(logList);
	while (logList) {
		log = logList;
		logList = logList->next;
		log->next = NULL;
		regionID = log->regionID;
		ReintegrateRegion(raidPtr, regionID, log);
		log->numRecords = 0;

		/* remove all items which are blocked on reintegration of this
		 * region */
		rf_lock_mutex2(raidPtr->parityLogDiskQueue.mutex);
		logData = rf_SearchAndDequeueParityLogData(raidPtr, regionID,
			   &raidPtr->parityLogDiskQueue.reintBlockHead,
			   &raidPtr->parityLogDiskQueue.reintBlockTail,
							   RF_TRUE);
		logDataList = logData;
		while (logData) {
			logData->next = rf_SearchAndDequeueParityLogData(
					 raidPtr, regionID,
					 &raidPtr->parityLogDiskQueue.reintBlockHead,
					 &raidPtr->parityLogDiskQueue.reintBlockTail,
					 RF_TRUE);
			logData = logData->next;
		}
		rf_unlock_mutex2(raidPtr->parityLogDiskQueue.mutex);

		/* process blocked log data and clear reintInProgress flag for
		 * this region */
		if (logDataList)
			rf_ParityLogAppend(logDataList, RF_TRUE, &log, RF_TRUE);
		else {
			/* Enable flushing for this region.  Holding both
			 * locks provides a synchronization barrier with
			 * DumpParityLogToDisk */
			rf_lock_mutex2(raidPtr->regionInfo[regionID].mutex);
			rf_lock_mutex2(raidPtr->regionInfo[regionID].reintMutex);
			/* XXXmrg: don't need this? */
			rf_lock_mutex2(raidPtr->parityLogDiskQueue.mutex);
			raidPtr->regionInfo[regionID].diskCount = 0;
			raidPtr->regionInfo[regionID].reintInProgress = RF_FALSE;
			rf_unlock_mutex2(raidPtr->regionInfo[regionID].mutex);
			rf_unlock_mutex2(raidPtr->regionInfo[regionID].reintMutex);	/* flushing is now
											 * enabled */
			/* XXXmrg: don't need this? */
			rf_unlock_mutex2(raidPtr->parityLogDiskQueue.mutex);
		}
		/* if log wasn't used, attach it to the list of logs to be
		 * returned */
		if (log) {
			log->next = freeLogList;
			freeLogList = log;
		}
	}
	if (freeLogList)
		rf_ReleaseParityLogs(raidPtr, freeLogList);
}

int
rf_ShutdownLogging(RF_Raid_t * raidPtr)
{
	/* shutdown parity logging 1) disable parity logging in all regions 2)
	 * reintegrate all regions */

	RF_SectorCount_t diskCount;
	RF_RegionId_t regionID;
	RF_ParityLog_t *log;

	if (rf_parityLogDebug)
		printf("[shutting down parity logging]\n");
	/* Since parity log maps are volatile, we must reintegrate all
	 * regions. */
	if (rf_forceParityLogReint) {
		for (regionID = 0; regionID < rf_numParityRegions; regionID++) {
			rf_lock_mutex2(raidPtr->regionInfo[regionID].mutex);
			raidPtr->regionInfo[regionID].loggingEnabled =
				RF_FALSE;
			log = raidPtr->regionInfo[regionID].coreLog;
			raidPtr->regionInfo[regionID].coreLog = NULL;
			diskCount = raidPtr->regionInfo[regionID].diskCount;
			rf_unlock_mutex2(raidPtr->regionInfo[regionID].mutex);
			if (diskCount > 0 || log != NULL)
				ReintegrateRegion(raidPtr, regionID, log);
			if (log != NULL)
				rf_ReleaseParityLogs(raidPtr, log);
		}
	}
	if (rf_parityLogDebug) {
		printf("[parity logging disabled]\n");
		printf("[should be done!]\n");
	}
	return (0);
}

int
rf_ParityLoggingDiskManager(RF_Raid_t * raidPtr)
{
	RF_ParityLog_t *reintQueue, *flushQueue;
	int     workNeeded, done = RF_FALSE;
	int s;

	/* Main program for parity logging disk thread.  This routine waits
	 * for work to appear in either the flush or reintegration queues and
	 * is responsible for flushing core logs to the log disk as well as
	 * reintegrating parity regions.
	 *
	 * BLOCKING */

	s = splbio();

	rf_lock_mutex2(raidPtr->parityLogDiskQueue.mutex);

	/*
         * Inform our creator that we're running. Don't bother doing the
         * mutex lock/unlock dance- we locked above, and we'll unlock
         * below with nothing to do, yet.
         */
	raidPtr->parityLogDiskQueue.threadState |= RF_PLOG_RUNNING;
	rf_signal_cond2(raidPtr->parityLogDiskQueue.cond);

	/* empty the work queues */
	flushQueue = raidPtr->parityLogDiskQueue.flushQueue;
	raidPtr->parityLogDiskQueue.flushQueue = NULL;
	reintQueue = raidPtr->parityLogDiskQueue.reintQueue;
	raidPtr->parityLogDiskQueue.reintQueue = NULL;
	workNeeded = (flushQueue || reintQueue);

	while (!done) {
		while (workNeeded) {
			/* First, flush all logs in the flush queue, freeing
			 * buffers Second, reintegrate all regions which are
			 * reported as full. Third, append queued log data
			 * until blocked.
			 *
			 * Note: Incoming appends (ParityLogAppend) can block on
			 * either 1. empty buffer pool 2. region under
			 * reintegration To preserve a global FIFO ordering of
			 * appends, buffers are not released to the world
			 * until those appends blocked on buffers are removed
			 * from the append queue.  Similarly, regions which
			 * are reintegrated are not opened for general use
			 * until the append queue has been emptied. */

			rf_unlock_mutex2(raidPtr->parityLogDiskQueue.mutex);

			/* empty flushQueue, using free'd log buffers to
			 * process bufTail */
			if (flushQueue)
			       FlushLogsToDisk(raidPtr, flushQueue);

			/* empty reintQueue, flushing from reintTail as we go */
			if (reintQueue)
				ReintegrateLogs(raidPtr, reintQueue);

			rf_lock_mutex2(raidPtr->parityLogDiskQueue.mutex);
			flushQueue = raidPtr->parityLogDiskQueue.flushQueue;
			raidPtr->parityLogDiskQueue.flushQueue = NULL;
			reintQueue = raidPtr->parityLogDiskQueue.reintQueue;
			raidPtr->parityLogDiskQueue.reintQueue = NULL;
			workNeeded = (flushQueue || reintQueue);
		}
		/* no work is needed at this point */
		if (raidPtr->parityLogDiskQueue.threadState & RF_PLOG_TERMINATE) {
			/* shutdown parity logging 1. disable parity logging
			 * in all regions 2. reintegrate all regions */
			done = RF_TRUE;	/* thread disabled, no work needed */
			rf_unlock_mutex2(raidPtr->parityLogDiskQueue.mutex);
			rf_ShutdownLogging(raidPtr);
		}
		if (!done) {
			/* thread enabled, no work needed, so sleep */
			if (rf_parityLogDebug)
				printf("[parity logging disk manager sleeping]\n");
			rf_wait_cond2(raidPtr->parityLogDiskQueue.cond,
				      raidPtr->parityLogDiskQueue.mutex);
			if (rf_parityLogDebug)
				printf("[parity logging disk manager just woke up]\n");
			flushQueue = raidPtr->parityLogDiskQueue.flushQueue;
			raidPtr->parityLogDiskQueue.flushQueue = NULL;
			reintQueue = raidPtr->parityLogDiskQueue.reintQueue;
			raidPtr->parityLogDiskQueue.reintQueue = NULL;
			workNeeded = (flushQueue || reintQueue);
		}
	}
	/*
         * Announce that we're done.
         */
	rf_lock_mutex2(raidPtr->parityLogDiskQueue.mutex);
	raidPtr->parityLogDiskQueue.threadState |= RF_PLOG_SHUTDOWN;
	rf_signal_cond2(raidPtr->parityLogDiskQueue.cond);
	rf_unlock_mutex2(raidPtr->parityLogDiskQueue.mutex);

	splx(s);

	/*
         * In the NetBSD kernel, the thread must exit; returning would
         * cause the proc trampoline to attempt to return to userspace.
         */
	kthread_exit(0);	/* does not return */
}
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */
