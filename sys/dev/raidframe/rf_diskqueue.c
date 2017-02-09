/*	$NetBSD: rf_diskqueue.c,v 1.53 2011/05/05 06:04:09 mrg Exp $	*/
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

/****************************************************************************
 *
 * rf_diskqueue.c -- higher-level disk queue code
 *
 * the routines here are a generic wrapper around the actual queueing
 * routines.  The code here implements thread scheduling, synchronization,
 * and locking ops (see below) on top of the lower-level queueing code.
 *
 * to support atomic RMW, we implement "locking operations".  When a
 * locking op is dispatched to the lower levels of the driver, the
 * queue is locked, and no further I/Os are dispatched until the queue
 * receives & completes a corresponding "unlocking operation".  This
 * code relies on the higher layers to guarantee that a locking op
 * will always be eventually followed by an unlocking op.  The model
 * is that the higher layers are structured so locking and unlocking
 * ops occur in pairs, i.e.  an unlocking op cannot be generated until
 * after a locking op reports completion.  There is no good way to
 * check to see that an unlocking op "corresponds" to the op that
 * currently has the queue locked, so we make no such attempt.  Since
 * by definition there can be only one locking op outstanding on a
 * disk, this should not be a problem.
 *
 * In the kernel, we allow multiple I/Os to be concurrently dispatched
 * to the disk driver.  In order to support locking ops in this
 * environment, when we decide to do a locking op, we stop dispatching
 * new I/Os and wait until all dispatched I/Os have completed before
 * dispatching the locking op.
 *
 * Unfortunately, the code is different in the 3 different operating
 * states (user level, kernel, simulator).  In the kernel, I/O is
 * non-blocking, and we have no disk threads to dispatch for us.
 * Therefore, we have to dispatch new I/Os to the scsi driver at the
 * time of enqueue, and also at the time of completion.  At user
 * level, I/O is blocking, and so only the disk threads may dispatch
 * I/Os.  Thus at user level, all we can do at enqueue time is enqueue
 * and wake up the disk thread to do the dispatch.
 *
 ****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_diskqueue.c,v 1.53 2011/05/05 06:04:09 mrg Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_raid.h"
#include "rf_diskqueue.h"
#include "rf_alloclist.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_debugprint.h"
#include "rf_shutdown.h"
#include "rf_cvscan.h"
#include "rf_sstf.h"
#include "rf_fifo.h"
#include "rf_kintf.h"

static void rf_ShutdownDiskQueueSystem(void *);

#ifndef RF_DEBUG_DISKQUEUE
#define RF_DEBUG_DISKQUEUE 0
#endif

#if RF_DEBUG_DISKQUEUE
#define Dprintf1(s,a)         if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)
#else
#define Dprintf1(s,a)
#define Dprintf2(s,a,b)
#define Dprintf3(s,a,b,c)
#endif

/*****************************************************************************
 *
 * the disk queue switch defines all the functions used in the
 * different queueing disciplines queue ID, init routine, enqueue
 * routine, dequeue routine
 *
 ****************************************************************************/

static const RF_DiskQueueSW_t diskqueuesw[] = {
	{"fifo",		/* FIFO */
		rf_FifoCreate,
		rf_FifoEnqueue,
		rf_FifoDequeue,
		rf_FifoPeek,
	rf_FifoPromote},

	{"cvscan",		/* cvscan */
		rf_CvscanCreate,
		rf_CvscanEnqueue,
		rf_CvscanDequeue,
		rf_CvscanPeek,
	rf_CvscanPromote},

	{"sstf",		/* shortest seek time first */
		rf_SstfCreate,
		rf_SstfEnqueue,
		rf_SstfDequeue,
		rf_SstfPeek,
	rf_SstfPromote},

	{"scan",		/* SCAN (two-way elevator) */
		rf_ScanCreate,
		rf_SstfEnqueue,
		rf_ScanDequeue,
		rf_ScanPeek,
	rf_SstfPromote},

	{"cscan",		/* CSCAN (one-way elevator) */
		rf_CscanCreate,
		rf_SstfEnqueue,
		rf_CscanDequeue,
		rf_CscanPeek,
	rf_SstfPromote},

};
#define NUM_DISK_QUEUE_TYPES (sizeof(diskqueuesw)/sizeof(RF_DiskQueueSW_t))

#define RF_MAX_FREE_DQD 256
#define RF_MIN_FREE_DQD  64

#include <sys/buf.h>

/* configures a single disk queue */

static void
rf_ShutdownDiskQueue(void *arg)
{
	RF_DiskQueue_t *diskqueue = arg;

	rf_destroy_mutex2(diskqueue->mutex);
}

int
rf_ConfigureDiskQueue(RF_Raid_t *raidPtr, RF_DiskQueue_t *diskqueue,
		      RF_RowCol_t c, const RF_DiskQueueSW_t *p,
		      RF_SectorCount_t sectPerDisk, dev_t dev,
		      int maxOutstanding, RF_ShutdownList_t **listp,
		      RF_AllocListElem_t *clList)
{
	diskqueue->col = c;
	diskqueue->qPtr = p;
	diskqueue->qHdr = (p->Create) (sectPerDisk, clList, listp);
	diskqueue->dev = dev;
	diskqueue->numOutstanding = 0;
	diskqueue->queueLength = 0;
	diskqueue->maxOutstanding = maxOutstanding;
	diskqueue->curPriority = RF_IO_NORMAL_PRIORITY;
	diskqueue->flags = 0;
	diskqueue->raidPtr = raidPtr;
	diskqueue->rf_cinfo = &raidPtr->raid_cinfo[c];
	rf_init_mutex2(diskqueue->mutex, IPL_VM);
	rf_ShutdownCreate(listp, rf_ShutdownDiskQueue, diskqueue);
	return (0);
}

static void
rf_ShutdownDiskQueueSystem(void *ignored)
{
	pool_destroy(&rf_pools.dqd);
}

int
rf_ConfigureDiskQueueSystem(RF_ShutdownList_t **listp)
{

	rf_pool_init(&rf_pools.dqd, sizeof(RF_DiskQueueData_t),
		     "rf_dqd_pl", RF_MIN_FREE_DQD, RF_MAX_FREE_DQD);
	rf_ShutdownCreate(listp, rf_ShutdownDiskQueueSystem, NULL);

	return (0);
}

int
rf_ConfigureDiskQueues(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
		       RF_Config_t *cfgPtr)
{
	RF_DiskQueue_t *diskQueues, *spareQueues;
	const RF_DiskQueueSW_t *p;
	RF_RowCol_t r,c;
	int     rc, i;

	raidPtr->maxQueueDepth = cfgPtr->maxOutstandingDiskReqs;

	for (p = NULL, i = 0; i < NUM_DISK_QUEUE_TYPES; i++) {
		if (!strcmp(diskqueuesw[i].queueType, cfgPtr->diskQueueType)) {
			p = &diskqueuesw[i];
			break;
		}
	}
	if (p == NULL) {
		RF_ERRORMSG2("Unknown queue type \"%s\".  Using %s\n", cfgPtr->diskQueueType, diskqueuesw[0].queueType);
		p = &diskqueuesw[0];
	}
	raidPtr->qType = p;

	RF_MallocAndAdd(diskQueues,
			(raidPtr->numCol + RF_MAXSPARE) *
			sizeof(RF_DiskQueue_t), (RF_DiskQueue_t *),
			raidPtr->cleanupList);
	if (diskQueues == NULL)
		return (ENOMEM);
	raidPtr->Queues = diskQueues;

	for (c = 0; c < raidPtr->numCol; c++) {
		rc = rf_ConfigureDiskQueue(raidPtr, &diskQueues[c],
					   c, p,
					   raidPtr->sectorsPerDisk,
					   raidPtr->Disks[c].dev,
					   cfgPtr->maxOutstandingDiskReqs,
					   listp, raidPtr->cleanupList);
		if (rc)
			return (rc);
	}

	spareQueues = &raidPtr->Queues[raidPtr->numCol];
	for (r = 0; r < raidPtr->numSpare; r++) {
		rc = rf_ConfigureDiskQueue(raidPtr, &spareQueues[r],
					   raidPtr->numCol + r, p,
					   raidPtr->sectorsPerDisk,
					   raidPtr->Disks[raidPtr->numCol + r].dev,
					   cfgPtr->maxOutstandingDiskReqs, listp,
					   raidPtr->cleanupList);
		if (rc)
			return (rc);
	}
	return (0);
}
/* Enqueue a disk I/O
 *
 * In the kernel, I/O is non-blocking and so we'd like to have multiple
 * I/Os outstanding on the physical disks when possible.
 *
 * when any request arrives at a queue, we have two choices:
 *    dispatch it to the lower levels
 *    queue it up
 *
 * kernel rules for when to do what:
 *    unlocking req  :  always dispatch it
 *    normal req     :  queue empty => dispatch it & set priority
 *                      queue not full & priority is ok => dispatch it
 *                      else queue it
 */
void
rf_DiskIOEnqueue(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req, int pri)
{
	RF_ETIMER_START(req->qtime);
	RF_ASSERT(req->type == RF_IO_TYPE_NOP || req->numSector);
	req->priority = pri;

#if RF_DEBUG_DISKQUEUE
	if (rf_queueDebug && (req->numSector == 0)) {
		printf("Warning: Enqueueing zero-sector access\n");
	}
#endif
	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOEnqueue");
	if (RF_OK_TO_DISPATCH(queue, req)) {
		Dprintf2("Dispatching pri %d regular op to c %d (ok to dispatch)\n", pri, queue->col);
		rf_DispatchKernelIO(queue, req);
	} else {
		queue->queueLength++;	/* increment count of number of requests waiting in this queue */
		Dprintf2("Enqueueing pri %d regular op to c %d (not ok to dispatch)\n", pri, queue->col);
		req->queue = (void *) queue;
		(queue->qPtr->Enqueue) (queue->qHdr, req, pri);
	}
	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOEnqueue");
}


/* get the next set of I/Os started */
void
rf_DiskIOComplete(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req, int status)
{
	int     done = 0;

	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOComplete");
	queue->numOutstanding--;
	RF_ASSERT(queue->numOutstanding >= 0);

	/* dispatch requests to the disk until we find one that we can't. */
	/* no reason to continue once we've filled up the queue */
	/* no reason to even start if the queue is locked */

	while (!done && !RF_QUEUE_FULL(queue)) {
		req = (queue->qPtr->Dequeue) (queue->qHdr);
		if (req) {
			Dprintf2("DiskIOComplete: extracting pri %d req from queue at c %d\n", req->priority, queue->col);
			queue->queueLength--;	/* decrement count of number of requests waiting in this queue */
			RF_ASSERT(queue->queueLength >= 0);
			if (RF_OK_TO_DISPATCH(queue, req)) {
				Dprintf2("DiskIOComplete: dispatching pri %d regular req to c %d (ok to dispatch)\n", req->priority, queue->col);
				rf_DispatchKernelIO(queue, req);
			} else {	
				/* we can't dispatch it, so just re-enqueue it.  
				   potential trouble here if disk queues batch reqs */
				Dprintf2("DiskIOComplete: re-enqueueing pri %d regular req to c %d\n", req->priority, queue->col);
				queue->queueLength++;
				(queue->qPtr->Enqueue) (queue->qHdr, req, req->priority);
				done = 1;
			}
		} else {	
			Dprintf1("DiskIOComplete: no more requests to extract.\n", "");
			done = 1;
		}
	}

	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOComplete");
}
/* promotes accesses tagged with the given parityStripeID from low priority
 * to normal priority.  This promotion is optional, meaning that a queue
 * need not implement it.  If there is no promotion routine associated with
 * a queue, this routine does nothing and returns -1.
 */
int
rf_DiskIOPromote(RF_DiskQueue_t *queue, RF_StripeNum_t parityStripeID,
		 RF_ReconUnitNum_t which_ru)
{
	int     retval;

	if (!queue->qPtr->Promote)
		return (-1);
	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOPromote");
	retval = (queue->qPtr->Promote) (queue->qHdr, parityStripeID, which_ru);
	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOPromote");
	return (retval);
}

RF_DiskQueueData_t *
rf_CreateDiskQueueData(RF_IoType_t typ, RF_SectorNum_t ssect,
		       RF_SectorCount_t nsect, void *bf,
		       RF_StripeNum_t parityStripeID,
		       RF_ReconUnitNum_t which_ru,
		       int (*wakeF) (void *, int), void *arg,
		       RF_AccTraceEntry_t *tracerec, RF_Raid_t *raidPtr,
		       RF_DiskQueueDataFlags_t flags, void *kb_proc,
		       int waitflag)
{
	RF_DiskQueueData_t *p;

	p = pool_get(&rf_pools.dqd, waitflag);
	if (p == NULL)
		return (NULL);

	memset(p, 0, sizeof(RF_DiskQueueData_t));
	if (waitflag == PR_WAITOK) {
		p->bp = getiobuf(NULL, true);
	} else {
		p->bp = getiobuf(NULL, false);
	}
	if (p->bp == NULL) {
		pool_put(&rf_pools.dqd, p);
		return (NULL);
	}
	SET(p->bp->b_cflags, BC_BUSY);	/* mark buffer busy */

	p->sectorOffset = ssect + rf_protectedSectors;
	p->numSector = nsect;
	p->type = typ;
	p->buf = bf;
	p->parityStripeID = parityStripeID;
	p->which_ru = which_ru;
	p->CompleteFunc = wakeF;
	p->argument = arg;
	p->next = NULL;
	p->tracerec = tracerec;
	p->priority = RF_IO_NORMAL_PRIORITY;
	p->raidPtr = raidPtr;
	p->flags = flags;
	p->b_proc = kb_proc;
	return (p);
}

void
rf_FreeDiskQueueData(RF_DiskQueueData_t *p)
{
	int s;
	s = splbio();		/* XXX protect only pool_put, or neither? */
	putiobuf(p->bp);
	pool_put(&rf_pools.dqd, p);
	splx(s);
}
