/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*****************************************************************************
 * @(#) QMQueue.h 1.55@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file QMQueue.h 
 *  @brief Header for Encryption/Authentication and Public Key Handler Queues
 * 
 *
 *  Header file for the Encryption/Authentication and Public Key Handler
 *  Queues, and their interface definitions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 05/13/03 brr   Correctly compute number of elements in RN Queue to determine
 *                when to update the RN write index. (Bug 914)
 * 04/21/03 brr   Added API_REQ_MASK.
 * 03/26/03 brr   Modified RNG not to perform PCI accesses with each reqeust 
 *                for random bytes.
 * 03/19/03 brr   Added prototype for QMgrCount.
 * 02/20/03 brr   Elimated all references to shared memory.
 * 11/25/02 brr   Removed the updateAllSem.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 09/18/02 brr   Have QMgrCheckQueue return the number of requests completed.
 * 07/08/02 brr   Added support for polling NSP2000 device.
 * 06/06/02 brr   Moved waitQueue into the queue structure.
 * 05/30/02 brr   Use common struct for statistics & removed obsolete fields.
 * 05/02/02 brr   Moved all SKS fields to device struture.
 * 05/01/02 brr   Removed obsolete fields and added haltingMask & sizeMask.
 * 03/27/02 hml   Replaced N8_QueueReturnCodes_t with N8_Status_t.
 * 03/27/02 brr   Removed QMGR* macros and driverHandle from QueueControl_t.
 * 03/20/02 mmd   Added requestsCompleted stat counter to QueueControl_t.
 * 03/19/02 msz   Shared memory is now part of the queue control structure.
 * 03/18/02 msz   Don't loop forever on hardware error - Added hardwareFailed
 * 03/01/02 brr   Moved definition of QueueControl_t to this file since its
 *                internal to QMgr.
 * 02/26/02 brr   Modified read/write macros to update shared memory instead
 *                of accessing the hardware.
 * 02/14/02 brr   Resolve 2.0 merge problems.
 * 01/17/02 msz   Changes to support mirror registers - BUG  413
 * 12/15/01 msz   Added QMGR_CMD_Q_GET_READ_WRITE_PTR.
 * 12/04/01 mel   Fixed bug #395 : Added BM_includes.h to avoid crash for build
 *                when SUPPORTING_BM is not defined 
 * 11/19/01 spm   Added proto for n8_SKS_Prom2Cache which is used
 *                QMgr setup to copy from SKS PROM to cache
 * 09/06/01 bac   Fixed the return type of QMgrCheckRequest.
 * 08/24/01 msz   Simplified macros a bit.
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon.h.
 * 08/16/01 msz   Code review changes
 * 08/15/01 brr   Fixed complier error when SUPPORTING_BM is not defined.
 * 08/01/01 msz   Created from merging EA_EnQueue.h and PK_EnQueue.h
 *                To see older revision history please see those files.
 *
 ****************************************************************************/
/** @defgroup QMgr  
 */

#ifndef QMQUEUE_H
#define QMQUEUE_H

#include "n8_pub_types.h"
#include "n8_pub_common.h"
#include "n8_enqueue_common.h"
#include "n8_rn_common.h"
#include "nsp2000_regs.h"
#include "n8_device_info.h"
#include "n8_driver_parms.h"
#include "helper.h"


/*****************************************************************************
 * #defines 
 *****************************************************************************/

#define API_REQ_MASK    (MAX_API_REQS-1)

/*****************************************************************************
 * Macros
 *****************************************************************************/
/*
 * @brief Macros for queue manager operations.
 *
 * In general the macros take _queue_p as a pointer to the queue control
 * structure (QueueControl_t *).
 *
 * @par Assumptions:
 *    If the queue type is not EA (Encryption/Authentication) then it
 *    is PK (Public Key).  This can be done because there are separate
 *    macros for the RNG (random number generator) hardware.
 *****************************************************************************/


/********** QMGR_IS_QUEUE_EA **********/
#define QMGR_IS_EA(_queue_p)                                        \
   ( (_queue_p)->type == N8_EA )


/*****************************************************************************
 * external data
 *****************************************************************************/
 
/* The one and only queue table! */
extern struct QueueControlTable_s  queueTable_g;

/*****************************************************************************
 * QueueControl_t 
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct QueueControl_t
 *  @brief This structure contains everything we need to know about a single
 *         queue.
 *
 *  The QueueControl_t structure contains all of the data needed for the use
 *  of a single queue. There is one queue for each execution unit on each 
 *  chip.  
 *
 *  Locks:
 *    The queueControlSem lock is used by the QMgr code. It ensures exclusive
 *    access to the QueueControl_t structure.
 *
 *    The process level concurrency semaphore queueControlSem will protect:
 *    · Data fields within the QueueControl_t structure, as these are on a
 *      per process basis.
 *
 *****************************************************************************/

typedef struct QueueControl_s
{
   N8CommonRegs_t   *n8reg_p;           /**< The hardware pointer.  This
                                             value is set by the driver, so
                                             it may be common across different
                                             queue control structures. */

   /* These are used in management of the queue. */
   QMgrRequest_t    *requestQueue[MAX_API_REQS];  /* Circular queue of API Requests */
   n8_WaitQueue_t    waitQueue[MAX_API_REQS];     /* Wait queue for the API Requests */
   int               apiWriteIdx;
   int               apiReadIdx;


   /* This section is for statistics on the queue */
   QueueStats_t      stats;

   /* This section is for entries in the structure which are invariant across
      every process in the system.  In theory these variables could be moved to
      a shared memory segment, initialized once and attached to by all 
      subsequent processes */
   int               chip;              /**< Chip number for this queue.
                                             Begins at 0. */
   N8_Component_t    type;              /**< Type of the execution unit this
                                             queue manages. */
   N8_Hardware_t     hardwareType;      /**< Type of hardware for this execution
                                             unit.  Note the hardware type
                                             may be "Behavioral Model". */
   int               sizeOfQueue;       /**< number of elements (not bytes) */
   uint32_t          sizeMask;          /**< Mask to apply to queue indexes */

   unsigned long     cmdQueVirtPtr;     /**< Virtual Ptr to the command queue.  */
   unsigned long     cmdEntrySize;      /**< Length of command entry in bytes.  */

   /* QMgr Locks */
   ATOMICLOCK_t      queueControlSem;   /**< Process sem for queue control */

   /* EA_PK_data: */

   EA_CMD_BLOCK_t   *EAq_head_p;        /**< head of the current linked list
                                             of EA requests.  Points into
                                             data allocated by the
                                             commandBlock structure above.
                                             This is set to a non zero
                                             value on a EA queue only. */
   PK_CMD_BLOCK_t   *PKq_head_p;        /**< head of the current linked list
                                             of PK requests.  Points into
                                             data allocated by the
                                             commandBlock structure above.
                                             This is set to a non zero
                                             value on a PK queue only. */
   uint16_t          writeIndex;        /* Current write index          */
   uint16_t         *readIndex_p;       /* Pointer to the read index    */
   uint32_t          remainingCmds;     /* Number of commands unable to */
                                        /* be processed because they do */
                                        /* not complete a request.      */

   /* RNG_data: */
   uint16_t          readIndex;         /* Current read index            */
   N8RNGRegs_t      *rngReg_p;          /* Pointer to the RNG registers  */ 
   int               rngInitialized;    /* Flag indicating state of RNG  */

   unsigned char     hardwareFailed;    /* Boolean set to non 0 if the   */
                                        /* hardware has a serious error. */
                                        /* Currently only needed in RNG. */
} QueueControl_t;


 
/*****************************************************************************
 * QueueControlSet_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @typedef QueueControlSet_t
 *  @brief This type represents the queue information for a complete nsp2000
 *         implementation.
 *
 *  The QueueControlSet_t type contains all of the data needed for the use
 *  of a single nsp2000 chip.
 *
 *****************************************************************************/
typedef QueueControl_t  QueueControlSet_t[N8_NUM_COMPONENTS];
 
 
/*****************************************************************************
 * QueueControlTable_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct QueueControlTable_t
 *  @brief This structure contains the data needed for the use of all of the
 *  nsp2000 based queues on the machine.
 *
 *  This data structure contains an array of control sets and the number of
 *  control sets.
 *
 *****************************************************************************/
 
typedef struct QueueControlTable_s
{
   QueueControlSet_t *controlSets_p;  /**< The control sets */
   uint32_t           nControlSets;   /**< The number of control sets */
   uint32_t           currentSet;     /**< The current control set to assign */
} QueueControlTable_t;



/* prototypes */
int  QMgrCheckQueue(N8_Component_t unit, int chip, uint16_t cmdsComplete);
void QMgrCmdError(N8_Component_t    unit,
                  int               chip,
                  int               readQueueIndex,
                  int               configStatus);

int QMgrPoll(void);
int QMgrCount(N8_Component_t unit);

#endif
