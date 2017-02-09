/*	$NetBSD: rf_diskqueue.h,v 1.24 2011/05/05 06:04:09 mrg Exp $	*/
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

/*****************************************************************************************
 *
 * rf_diskqueue.h -- header file for disk queues
 *
 * see comments in rf_diskqueue.c
 *
 ****************************************************************************************/


#ifndef _RF__RF_DISKQUEUE_H_
#define _RF__RF_DISKQUEUE_H_

#include <sys/queue.h>

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_acctrace.h"
#include "rf_alloclist.h"
#include "rf_etimer.h"
#include "rf_netbsd.h"


#define RF_IO_NORMAL_PRIORITY 1
#define RF_IO_LOW_PRIORITY    0

/* the data held by a disk queue entry */
struct RF_DiskQueueData_s {
	RF_SectorNum_t sectorOffset;	/* sector offset into the disk */
	RF_SectorCount_t numSector;	/* number of sectors to read/write */
	RF_IoType_t type;	/* read/write/nop */
	void *buf;		/* buffer pointer */
	RF_StripeNum_t parityStripeID;	/* the RAID parity stripe ID this
					 * access is for */
	RF_ReconUnitNum_t which_ru;	/* which RU within this parity stripe */
	int     priority;	/* the priority of this request */
	int     (*CompleteFunc) (void *, int);	/* function to be called upon
						 * completion */
	void   *argument;	/* argument to be passed to CompleteFunc */
	RF_Raid_t *raidPtr;	/* needed for simulation */
	RF_AccTraceEntry_t *tracerec;	/* perf mon only */
	RF_Etimer_t qtime;	/* perf mon only - time request is in queue */
	RF_DiskQueueData_t *next;
	RF_DiskQueueData_t *prev;
	RF_DiskQueue_t *queue;	/* the disk queue to which this req is
				 * targeted */
	RF_DiskQueueDataFlags_t flags;	/* flags controlling operation */

	struct proc *b_proc;	/* the b_proc from the original bp passed into
				 * the driver for this I/O */
	struct buf *bp;		/* a bp to use to get this I/O done */
	/* TAILQ bits for a queue for completed I/O requests */
	TAILQ_ENTRY(RF_DiskQueueData_s) iodone_entries;
	int  error;             /* Indicate if an error occurred
				   on this I/O (1=yes, 0=no) */
};

/* note: "Create" returns type-specific queue header pointer cast to (void *) */
struct RF_DiskQueueSW_s {
	RF_DiskQueueType_t queueType;
	void   *(*Create) (RF_SectorCount_t, RF_AllocListElem_t *, RF_ShutdownList_t **);	/* creation routine --
												 * one call per queue in
												 * system */
	void    (*Enqueue) (void *, RF_DiskQueueData_t *, int);	/* enqueue routine */
	RF_DiskQueueData_t *(*Dequeue) (void *);	/* dequeue routine */
	RF_DiskQueueData_t *(*Peek) (void *);	/* peek at head of queue */

	/* the rest are optional:  they improve performance, but the driver
	 * will deal with it if they don't exist */
	int     (*Promote) (void *, RF_StripeNum_t, RF_ReconUnitNum_t);	/* promotes priority of
									 * tagged accesses */
};

struct RF_DiskQueue_s {
	const RF_DiskQueueSW_t *qPtr;	/* access point to queue functions */
	void   *qHdr;		/* queue header, of whatever type */
	rf_declare_mutex2(mutex);/* mutex locking data structures */
	long    numOutstanding;	/* number of I/Os currently outstanding on
				 * disk */
	long    maxOutstanding;	/* max # of I/Os that can be outstanding on a
				 * disk (in-kernel only) */
	int     curPriority;	/* the priority of accs all that are currently
				 * outstanding */
	long    queueLength;	/* number of requests in queue */
	RF_DiskQueueFlags_t flags;	/* terminate, locked */
	RF_Raid_t *raidPtr;	/* associated array */
	dev_t   dev;		/* device number for kernel version */
	RF_SectorNum_t last_deq_sector;	/* last sector number dequeued or
					 * dispatched */
	int     col;	/* debug only */
	struct raidcinfo *rf_cinfo;	/* disks component info.. */
};
#define RF_DQ_LOCKED  0x02	/* no new accs allowed until queue is
				 * explicitly unlocked */

/* macros setting & returning information about queues and requests */
#define RF_QUEUE_EMPTY(_q)                  ((_q)->numOutstanding == 0)
#define RF_QUEUE_FULL(_q)                   ((_q)->numOutstanding == (_q)->maxOutstanding)

#define RF_LOCK_QUEUE_MUTEX(_q_,_wh_)   rf_lock_mutex2((_q_)->mutex)
#define RF_UNLOCK_QUEUE_MUTEX(_q_,_wh_) rf_unlock_mutex2((_q_)->mutex)

/* whether it is ok to dispatch a regular request */
#define RF_OK_TO_DISPATCH(_q_,_r_) \
  (RF_QUEUE_EMPTY(_q_) || \
    (!RF_QUEUE_FULL(_q_) && ((_r_)->priority >= (_q_)->curPriority)))

int rf_ConfigureDiskQueueSystem(RF_ShutdownList_t **);
int rf_ConfigureDiskQueues(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);
void rf_DiskIOEnqueue(RF_DiskQueue_t *, RF_DiskQueueData_t *, int);
void rf_DiskIOComplete(RF_DiskQueue_t *, RF_DiskQueueData_t *, int);
int rf_DiskIOPromote(RF_DiskQueue_t *, RF_StripeNum_t,  RF_ReconUnitNum_t);
RF_DiskQueueData_t *rf_CreateDiskQueueData(RF_IoType_t, RF_SectorNum_t,
					   RF_SectorCount_t , void *,
					   RF_StripeNum_t, RF_ReconUnitNum_t,
					   int (*wakeF) (void *, int),
					   void *,
					   RF_AccTraceEntry_t *, RF_Raid_t *,
					   RF_DiskQueueDataFlags_t,
					   void *, int);
void rf_FreeDiskQueueData(RF_DiskQueueData_t *);
int rf_ConfigureDiskQueue(RF_Raid_t *, RF_DiskQueue_t *,
			  RF_RowCol_t, const RF_DiskQueueSW_t *,
			  RF_SectorCount_t, dev_t, int,
			  RF_ShutdownList_t **,
			  RF_AllocListElem_t *);

#endif				/* !_RF__RF_DISKQUEUE_H_ */
