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

static char const n8_id[] = "$Id: QMQueue.c,v 1.2 2009/11/22 19:09:16 mbalmer Exp $";
/*****************************************************************************/
/** @file QMQueue.c
 *  @brief Public Key Encryption / Encryption Authentication
 *         Request Queue Handler.
 *
 *    This file handles the queue of command blocks sent by the API
 *    to the Device Driver for the Public Key Encryption hardware, 
 *    or the Encryption Authentication hardware.
 *
 *    NOTE: I have made a general assumption in this module, that if a
 *    queue is not an Encryption/Authentication queue then it is a
 *    Public Key Encryption queue.  The Random Number Generation queue
 *    has its own separate routines.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 07/01/03 brr   Do not set command complete bit on EA errors in QMgrCmdError.
 * 05/08/03 brr   Support reworked user pools.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 04/01/03 brr   Reverted N8_WaitOnRequest to accept timeout parameter.
 * 03/31/03 brr   Do not rely on atomic_inc_and_test.
 * 03/27/03 brr   Fix race condition when waking processes on wait queue.
 * 03/24/03 brr   Fix race condition when conditionally enabling the AMBA
 *                timer, also improve performance by performing fewer atomic
 *                operations in QMgrCheckQueue.
 * 03/21/03 brr   Track requests queued by unit type.
 * 03/19/03 brr   Enable the AMBA timer only when requests are queued to the 
 *                NSP2000. Modified N8_WaitOnRequest to wait on a specific
 *                request and reuse the "synchronous" wait queue.
 * 03/10/03 brr   Added conditional support to perform callbacks in interrupt
 *                in interrupt context.
 * 02/02/03 brr   Updated command completion determination to use the number
 *                of commands completed instead of queue position. This
 *                elimated the need for forceCheck & relying on the request
 *                state to avoid prematurely marking a command complete.
 * 11/02/02 brr   Reinstate forceCheck.
 * 10/23/02 brr   Disable interrupts during queue operation and remove 
 *                forceCheck since it is no longer needed.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 09/18/02 brr   Have QMgrCheckQueue return the number of requests completed.
 * 09/10/02 brr   OR command complete bit with last NOP in QMgrCmdError.
 * 07/08/02 brr   Added support for polling NSP2000 device.
 * 06/26/02 brr   Remove bank from the calls to N8_PhysToVirt.
 * 06/25/02 brr   Rework user pool allocation to only mmap portion used by
 *                the individual process.
 * 06/14/02 hml   Make sure we have physical addresses when we try to
 *                delete a request allocated from a request pool.
 * 06/12/02 hml   Passes the bank from kmem_p to calls to N8_PhysToVirt.
 * 06/10/02 hml   Handle deletion of buffers from deleted pools.
 * 06/11/02 brr   Convert N8_WaitEventInt to an OS dependent function.
 * 06/07/02 brr   Use unsigned arithmetic to compute queue depth.
 * 06/06/02 brr   Move wait queue initializaton to QMgrInit.
 * 06/04/02 brr   Make sure QMgrCheckQueue retrieves an updated write pointer
 *                for each loop iteration to prevent a race condition where
 *                requests queued by the other processor are marked as 
 *                completed as they are being queued.
 * 05/30/02 brr   Reworked error handling such that it is done by the ISR.
 *                Replaced QMgrReadNspStats with QMgrCmdError that the ISR
 *                calls if a command entry needs to be NOP'ed.
 * 05/07/02 msz   Pend on synchronous requests.
 * 05/01/02 brr   Updated with comments from code review including update 
 *                comments, add error code for API request queue full,
 *                eliminate modulo operations, and remove all chained request
 *                references. Also reworked to improve performance.
 * 03/27/02 hml   Changed all N8_QueueReturnCodes_t to N8_Status_t. Needed for
 *                Bug 634.
 * 03/22/02 msz   Don't process a request as done until it has been queued.
 * 03/26/02 brr   Ensure that buffers on the command queue are not returned to 
 *                the buffer pool before the commands using them have executed.
 * 03/20/02 mmd   Incrementing requestsCompleted stat counter in QMgrCheckQueue.
 * 03/19/02 msz   Removed shared memory
 * 03/18/02 msz   We no longer need the shadow queue nor it associated changes
 * 03/06/02 brr   Do not include and SAPI includes & removed old semaphores
 * 03/01/02 brr   Convert the queueControlSem to an ATOMICLOCK. Reworked 
 *                dequeue operation not to require a semaphore.
 * 02/26/02 brr   Keep local copies of read/write pointers & have the ISR write
 *                them to hardware. Also have ISR complete API requests which
 *                are now KMALLOC'ed and no longer need copied to user space.
 * 02/20/02 brr   Streamlined copyInRequestsCommandBlocks & 
 *                copyOutRequestsCommandBlocks to better support copying blocks
 *                to user space. Also eliminated QMgrSetupQueue.
 * 02/19/02 brr   Copy the command block from user space, if the request was
 *                submitted from user space.
 * 02/18/02 brr   Select the queue on entry to QMgrQueue.
 * 02/14/02 brr   Resove 2.0 merge problems.
 * 02/04/02 msz   Clear shadow status on error, changes in QMgrReadNspStatus
 * 01/31/02 brr   Substantial rework to improve performance.
 * 01/25/01 msz   Check for hardware error on Queue Full condition.
 *                Also added a debug routine
 * 01/16/02 brr   Removed obsolete include file.
 * 01/14/02 hml   Replaced N8_VIRT_FREE with N8_FREE.
 * 01/13/02 brr   Removed obsolete include file.
 * 01/09/02 brr   Replaced N8_VIRT_MALLOC with N8_UMALLOC.
 * 12/15/01 msz   Adding shadow copy of command queue.  Fix for BUG 382, 411.
 *                Rewrote QMgrReadNspStatus so it will clean up any hw error.
 * 12/07/01 bac   Fixed an annoying compiler warning.
 * 12/05/01 brr   only perform queue initialization for the behavioral model.
 * 12/05/01 brr   Move initialization of Queue to the driver.
 * 12/05/01 brr   Move initialization of Queue to the driver.
 * 12/03/01 mel   BUG 395 : Added stdio.h include - to eliminate compilation 
 *                errors. 
 * 12/04/01 msz   Changes to allow chaining of requests.
 * 11/27/01 msz   BUG 373 (actually couldn't occur, but cleaned up code)
 *                Fixed copyOutRequestsCommandBlocks so it doesn't return
 *                status, as it always returned ok.
 *                Don't get process lock and setup queue if we don't have
 *                to - BUG 379
 * 11/26/01 msz   Remove the setting of the timeFirstCommandSent field. BUG 335
 * 11/14/01 spm   Gated prom-to-cache copy so that it is only done for PK
 * 11/14/01 msz   Moved callbacks out of locked area. BUG 282
 * 11/13/01 brr   Removed references to shared_resource, fixed warnings.
 * 11/12/01 msz   Some code review fixes.  Fixes to items 19,20,21,24,30,32
 * 11/11/01 mmd   Modified QMgrSetupQueue to attach to existing command queue
 *                if using real hardware.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/10/01 spm   Added SKS prom-to-cache copy to QMgr setup.  Addressed
 *                bug #295 with a comment.
 * 11/08/01 bac   Fixed documenation that referred to the unused return code
 *                N8_QUEUE_REQUEST_ERROR_QUEUE_IN_ERROR
 * 10/29/01 msz   Fixed NOPing of CCH (ea) commands.
 * 10/10/01 brr   Removed warning exposed when optimization turned up.
 * 10/12/01 msz   Protect Behavioral Model with hardwareAccessSem.  Needed
 *                for running in a multithreaded environment.
 * 10/09/01 msz   Initialize hardware if it has not been initialized, not
 *                if we didn't attach to memory.
 * 10/08/01 bac   Correctly set the API Request field
 *                indexOfCommandThatCausedError.
 * 10/03/01 msz   removed extra DBG, fixed when queue is initialized and when
 *                it is set to initialized.
 * 10/02/01 bac   Potential fix to QMgrCheckQueue bug when the hardware locks.
 * 09/24/01 msz   Shared memory changes & some extra DBG.
 * 09/21/01 msz   Fixed a bug in QMgrCheckQueue, where we were dereferencing
 *                a freed structure.
 * 09/14/01 bac   Set the bitfields in the command block before allocation to
 *                the default set for unshared kernel memory.  This will need to
 *                be addressed for the multi-process case.
 * 09/06/01 bac   Added include of <string.h> to silence warning.
 * 08/29/01 bac   Removed a incorrect call to QMgrWaitForNotBusy in the setup
 *                routine. 
 * 08/25/01 msz   Changed QMgrCheckQueue, QMgrReadNspStatus
 * 08/16/01 msz   Code review changes
 * 08/09/01 msz   Added locking.
 * 08/01/01 msz   Created from merging EA_EnQueue.h and PK_EnQueue.h
 *                To see older revision history please see those files.
 *
 ****************************************************************************/
/** @defgroup QMgr QMQueue
 */

#include "n8_pub_types.h"
#include "n8_common.h"
#include "n8_OS_intf.h"
#include "n8_malloc_common.h"
#include "QMQueue.h"
#include "n8_enqueue_common.h"
#include "QMUtil.h"
#include "n8_semaphore.h"
#include "n8_time.h"
#include "RN_Queue.h"
#include "n8_manage_memory.h"
#include "userPool.h"
#include "n8_driver_api.h"
#include "irq.h"
#include "n8_memory.h"
#include <sys/proc.h>

n8_atomic_t  requestsComplete;
n8_atomic_t  requestsQueued[N8_NUM_COMPONENTS];
#if defined(__linux) && defined(SUPPORT_DEVICE_POLL)
extern wait_queue_head_t nsp2000_wait;
#endif
/*****************************************************************************
 * copyOutRequestsCommandBlocks
 *****************************************************************************/
/** @ingroup QMgr
 * @brief copy command blocks to the API back after hardware completes a request
 *
 * @param queue_p        RO: Pointer to the Queue Control Structure for
 *                           this EA or PK execution unit.
 * @param request_p      RO: Pointer to the request to be copied from
 *                           this EA or PK execution unit.
 *
 * @par Externals:
 *  None.
 *
 * @return
 *   None
 *
 * @par Errors:  none checked for
 *
 * @par Locks:
 *    This routine is called from interrupt context and assumes exclusive
 *    access to the command blocks pointed to by the API request.
 *
 * @par Assumptions:
 *  this function is called only after it is verified that the API requires the
 *  command blocks to be copied back
 *
 ******************************************************************************/

static void
copyOutRequestsCommandBlocks(QueueControl_t *queue_p, API_Request_t *request_p)
{

   void            *CommandBlock_p = NULL;
   int firstCmd  =  request_p->index_of_first_command;
   int numCmds   =  request_p->numNewCmds;
   int remainCmds;
   void            *copy_p;

   CommandBlock_p = (void *)((int)request_p + sizeof(API_Request_t));

   /* Compute the address in the queue base on the index and the size of a */
   /* command for this queue.                                              */
   copy_p = (void *)(queue_p->cmdQueVirtPtr + (firstCmd * queue_p->cmdEntrySize));

   /* If the Queue did not wrap with this API request, copy the entire */
   /* block back into the API Request in one operation.                */
   if (firstCmd <= request_p->index_of_last_command)
   {
      memcpy(CommandBlock_p, copy_p, queue_p->cmdEntrySize * numCmds);
   }
   else
   /* This request wrapped to the beginning of the command queue, */
   /* perform the copy back in two operations.                    */
   {
      remainCmds = queue_p->sizeOfQueue - firstCmd;

      memcpy(CommandBlock_p, copy_p, (queue_p->cmdEntrySize * remainCmds));
      memcpy((void *)((int)CommandBlock_p + (remainCmds * queue_p->cmdEntrySize)),
             (void *)queue_p->cmdQueVirtPtr,
             queue_p->cmdEntrySize * (numCmds - remainCmds));
   }

   return;

} /* copyOutRequestsCommandBlocks */

/*****************************************************************************
 * copyInRequestsCommandBlocks
 *****************************************************************************/
/** @ingroup QMgr
 * @brief copy command blocks from the API request into the command queue.
 *
 * @param API_req_p             RO: Request from API, used to return status
 *                                  back to the API in the request.
 * @param writeQueueIndex_p     RW: The write index pointer from hardware
 *
 * @par Externals:
 *  None.
 *
 * @return
 *  None.
 *
 * @par Errors:
 *     See return values.
 *
 * @par Locks:
 *    This routine requires that the queueControlSem has been previously
 *    acquired.  (It is called from a section that requires that this lock
 *    be held, so the routine requires the lock be held rather than acquiring
 *    the lock internally.)
 *
 * @par Assumptions:
 *
 ******************************************************************************/

static void
copyInRequestsCommandBlocks( QueueControl_t *queue_p,
                             API_Request_t  *API_req_p,
                             uint32_t       *writeQueueIndex_p )
{
   void                     *CommandBlock_p;
   int                       remainCmds;
   void                     *copy_p;

   /* copy this requests' command blocks to the queue */
   CommandBlock_p = (void *)((int)API_req_p + sizeof(API_Request_t));

   /* Compute the address in the queue base on the index and the size of a */
   /* command for this queue.                                              */
   copy_p = (void *)
      (queue_p->cmdQueVirtPtr + (*writeQueueIndex_p * queue_p->cmdEntrySize));

   /* If the Queue does not wrap with API request, copy the entire block */
   /* into the command queue in one operation.                           */
   if (*writeQueueIndex_p + API_req_p->numNewCmds < queue_p->sizeOfQueue)
   {
      memcpy(copy_p, CommandBlock_p,
             queue_p->cmdEntrySize * API_req_p->numNewCmds);
      *writeQueueIndex_p += API_req_p->numNewCmds;
   }
   else
   /* This request will wrap to the beginning of the command queue, */
   /* perform the copy in two operations.                           */
   {
      remainCmds = queue_p->sizeOfQueue - *writeQueueIndex_p;
      memcpy(copy_p, CommandBlock_p,
             queue_p->cmdEntrySize * remainCmds);
      *writeQueueIndex_p = 
         (*writeQueueIndex_p + API_req_p->numNewCmds) & queue_p->sizeMask;
      memcpy((void *)queue_p->cmdQueVirtPtr,
         (void *)((int)CommandBlock_p + (remainCmds * queue_p->cmdEntrySize)),
         queue_p->cmdEntrySize * *writeQueueIndex_p);
   }

} /* copyInRequestsCommandBlocks */


/*****************************************************************************
 * QMgrCheckQueue
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Given the number of completed command blocks, traverse the list of i
 *        API Requests and mark them finished until the number of command blocks
 *        has been exhausted.
 *
 * @param unit         RO: Indicates EA or PK execution unit.
 *        chip         RO: Indicates which chip.
 *        cmdsComplete RO: Number of command blocks completed since
 *                         the last call.
 *
 * @par Externals:
 *
 * @return
 *      int - number of completed requests on this queue.
 *
 * @par Errors:
 *   Error conditions are handled in the ISR. This function only processes
 *   completed requests.
 *
 * @par Locks:
 *    This routine function is called in interrupt context and assumes 
 *    excluse access to dequeue API requests. IT MUST NOT TAKE A SEMAPHORE
 *    NOR CAN ANY PROCESS UPDATE THE apiReadIdx.
 *
 *    If this routine is called from a non interrupt context, it assumes
 *    exclusive access to dequeue API requests.  Therefore it MUST
 *    DISABLE INTERRUPTS BEFORE CALLING THIS FUNCTION.
 *
 * @par Assumptions:
 *   It is assumed that the user is not modifying requests that are on the
 *   queue.
 *
 ******************************************************************************/

int
QMgrCheckQueue(N8_Component_t    unit,
               int               chip,
               uint16_t          cmdsComplete)
{
   API_Request_t               *API_Request_p;
   N8_MemoryHandle_t           *kmem_p;
   QueueControl_t              *queue_p;
   int                          bankIndex;
   int                          reqsComplete = 0;
   uint32_t                     totalCmds;

   queue_p = &(queueTable_g.controlSets_p[chip][unit]);

   /* Get the first API request on the queue.                  */
   API_Request_p = (API_Request_t *)queue_p->requestQueue[queue_p->apiReadIdx];
   totalCmds = cmdsComplete + queue_p->remainingCmds;

   while ((API_Request_p != NULL) &&
          (totalCmds >= API_Request_p->numNewCmds))
   {

      totalCmds -= API_Request_p->numNewCmds;

      /* Request is done */
      /* Check to see if the process that queued this request has  */
      /* terminated. If so, simply free the buffer and move on.    */
      kmem_p = (N8_MemoryHandle_t *)((int)API_Request_p - N8_BUFFER_HEADER_SIZE);

      if (kmem_p->bufferState == N8_BUFFER_SESS_DELETED)
      {
         /* Kernel buffers can be handled in the good old simple way */
         bankIndex = kmem_p->bankIndex;
         N8_FreeBuffer(kmem_p);


         /* If this buffer was a member of a pool userPoolFreePool will 
            determine if the pool can now be freed. */
         userPoolFreePool(bankIndex);
      }
      else 
      {
         kmem_p->bufferState = N8_BUFFER_NOT_QUEUED;

         /* If this command is to be copied back, and had no    */
         /* errors, then copy the contents of the command block */
         /* that hardware has completed back into the command   */
         /* block sent by the API                               */
         if ( ( API_Request_p->copyBackCommandBlock == N8_TRUE ) &&
              ( API_Request_p->err_rpt_bfr.errorReturnedFromSimon == 0 ) )
         {
            copyOutRequestsCommandBlocks(queue_p, API_Request_p);
         }
   
         API_Request_p->qr.requestStatus = N8_QUEUE_REQUEST_FINISHED;

         /* If this is a synchronous request, wake up the       */
         /* process waiting on it.                              */
         if ( API_Request_p->qr.synchronous == N8_TRUE )
         {
            WakeUp( (&queue_p->waitQueue[queue_p->apiReadIdx] ) );
         }
#ifdef SUPPORT_INT_CONTEXT_CALLBACKS
	 else
         {
            /* This request was submitted by a kernel operation, perform */
            /* the callback processing immediately.                      */
            if (API_Request_p->userSpace == N8_FALSE)
            {
               /* Perform the SDK callback if needed */
               if (API_Request_p->qr.callback)
               {
                  API_Request_p->qr.callback ((void *)API_Request_p);
               }

               /* Perform the user's callback if needed */
               if (API_Request_p->usrCallback)
               {
                  API_Request_p->usrCallback (API_Request_p->usrData, 
                                              N8_STATUS_OK);
               }
	       /* Free the API request */
               N8_FreeBuffer(kmem_p);
            }
         }
#endif

      }

      /* Increment our local count of completed requests. */
      reqsComplete++;

      /* Remove the API request from the requestQueue since     */
      /* QMgr has completed the processing of this API request. */
      queue_p->requestQueue[queue_p->apiReadIdx] = NULL;
      queue_p->apiReadIdx = (queue_p->apiReadIdx + 1) & (API_REQ_MASK);
      API_Request_p = (API_Request_t *)
         queue_p->requestQueue[queue_p->apiReadIdx];


   }  /* end -- while (API_Request_p != NULL)                       */

   queue_p->remainingCmds = totalCmds;

   /* Increment our global counters for completed requests. */
   queue_p->stats.requestsCompleted += reqsComplete;
   n8_atomic_sub(requestsQueued[unit], reqsComplete);

#if defined(__linux) && defined(SUPPORT_DEVICE_POLL)
   n8_atomic_add(requestsComplete, reqsComplete);
   if (n8_atomic_read(requestsComplete))
   {
      WakeUp(&nsp2000_wait);
   }
#endif

   /* Increment our count of how many times the check queue was called. */
   queue_p->stats.requestCheckCalled++;

   /* return the number of requests completed on this queue. */
   return (reqsComplete);

} /* QMgrCheckQueue */

/*****************************************************************************
 * N8_QMgrQueue
 *****************************************************************************/
/** @ingroup QMgr
 * @brief  API Interface to the EA/PK Queue:  accept new requests
 *           from the upper layers (ie, the API) and writes the command
 *           blocks into the command queue.
 *
 *           Requests consist of (>=1) commands.  Requests are kept
 *           in a circular queue while the commands persist in the
 *           command queue. Once the commands have been executed, the
 *           request is marked as complete and is removed from the circular
 *           queue.
 *
 * @param API_req_p             RO: Pointer to the request from the API
 *
 * @par Externals:
 *  None.
 *
 * @return
 *  typedef-enum-ed type see N8_QueueReturnCodes_t
 *      values from PK_ValidateRequest if it failed,
 *      N8_QUEUE_FULL if the new request contains more commands than the
 *                                      queue has capacity for
 *
 * @par Errors:
 *  N8_QUEUE_FULL if the new request contains more commands than the
 *  queue has capacity for
 *
 *  On an error API_req_p->requestStatus will also be set.
 *
 * @par Locks:
 *    This routine acquires the queueControlSem to assure the queue operation
 *    is atomic.
 *
 * @par Assumptions:
 *
 ******************************************************************************/
extern int ambaTimerActive;

N8_Status_t
N8_QMgrQueue( API_Request_t *API_req_p )
{
   uint32_t                  writeQueueIndex;
   uint32_t                  readQueueIndex;
   uint32_t                  firstCommand;
   uint32_t                  lastCommand;
   uint32_t                  cmd_blks_available;
   QueueControl_t           *queue_p;
   N8_Status_t               returnCode = N8_STATUS_OK;
   N8_MemoryHandle_t        *kmem_p;
   int                       waitReturn;
   int                       requestIndex;
   N8_Component_t            unit;

   /* Determine which queue to use, and set the queue_p in the API request */
   unit = API_req_p->qr.unit;
   returnCode = QMgr_get_valid_unit_num(unit,
                                        API_req_p->qr.chip,
                                        &API_req_p->qr.chip);
   if (returnCode != N8_STATUS_OK)
   {
      /* unit is invalid, we're out of here */ 
      goto QMgrQueueReturn;
   }

   queue_p = &(queueTable_g.controlSets_p[API_req_p->qr.chip][unit]);

   /* Acquire the lock to insure that this is the only process or       */
   /* thread that is adding to the queue at a time.  We want to hold    */
   /* the lock from the moment we get the write index until we are done */
   /* copying in the data and have incremented the write index.         */
   N8_AtomicLock(queue_p->queueControlSem);


   /* Get the current read and write index.                             */
   readQueueIndex = *queue_p->readIndex_p;
   writeQueueIndex = queue_p->writeIndex;

   /* Calculate the available space in the queue.  Note that the        */
   /* capacity of the queue is one less than the size of the queue      */
   /* so we can distinguish between the full and empty conditions.      */
   /* NOTE: the unsigned arithemetic handles negative numbers           */
   cmd_blks_available = (readQueueIndex - writeQueueIndex - 1) & 
                        queue_p->sizeMask;

   /* if there are more new commands than room */
   if ( API_req_p->numNewCmds > cmd_blks_available )
   {
      /* this request cannot be filled */
      returnCode = N8_QUEUE_FULL;
      goto QMgrQueueReleaseReturn;
   }

   requestIndex = queue_p->apiWriteIdx;
   if (queue_p->requestQueue[requestIndex] == NULL)
   { 
      queue_p->requestQueue[requestIndex] = &API_req_p->qr;
      queue_p->apiWriteIdx = (requestIndex + 1) & (API_REQ_MASK);
   }
   else 
   {
      /* then this request cannot be filled */
      returnCode = N8_API_QUEUE_FULL;
      goto QMgrQueueReleaseReturn;
   }

   /* denote where this requests' commands begin */
   firstCommand = writeQueueIndex;
   API_req_p->index_of_first_command = firstCommand;


   /* copy this requests' command blocks to the queue   */
   /* On a failure, do not expect writeQueueIndex to    */
   /* be valid.                                         */
   copyInRequestsCommandBlocks(queue_p, API_req_p, &writeQueueIndex );

   /* denote where this requests' commands end */
   lastCommand =
      (writeQueueIndex - 1 + queue_p->sizeOfQueue) & queue_p->sizeMask;
   API_req_p->index_of_last_command = lastCommand;

   /* Mark this buffer as QUEUED so a close of the driver will not free */
   /* this buffer until the command has been processed.                 */
   kmem_p = (N8_MemoryHandle_t *)((int)API_req_p - N8_BUFFER_HEADER_SIZE);
   kmem_p->bufferState = N8_BUFFER_QUEUED;

   /* The queue operation is complete.          */
   /* set the status flag so API knows msg rcvd */
   API_req_p->qr.requestStatus = N8_QUEUE_REQUEST_QUEUED;

   /* Add this to the current number of requests queued */
   n8_atomic_add(requestsQueued[unit], 1);

   /* notify the hardware that new commands are pending */
   /* by setting the write pointer to the next empty    */
   queue_p->writeIndex = writeQueueIndex;
   queue_p->n8reg_p->q_ptr = writeQueueIndex;

   /* If this is the only outstanding EA request, the   */
   /* AMBA timer must be reloaded                       */
   if ((unit == N8_EA) && (ambaTimerActive == FALSE))
   {
      reload_AMBA_timer();
   }

   /* Increment our count of requests queued.           */
   queue_p->stats.requestsQueued++;

   /* As we notified hardware, we can now release the           */
   /* semaphore.  We can also release the queue semaphore       */
   N8_AtomicUnlock(queue_p->queueControlSem);

   /* If the request is a synchronous request, then wait for it */
   /* to be done before we return.  We can look at the request  */
   /* status field to see that it is finished, and this is safe */
   /* because the SAPI code is waiting/sleeping on this request */
   /* therefore can't be manipulating it.                       */
   if ( API_req_p->qr.synchronous == N8_TRUE )
   {
      waitReturn = N8_WaitEventInt(&queue_p->waitQueue[requestIndex], API_req_p);
      if (waitReturn != 0)
      {
         /* The wait was preempted by a signal or timeout, increment stat and requeue */
         queue_p->stats.requestsPreempted++;
         returnCode = N8_EVENT_INCOMPLETE;
      }
   }

QMgrQueueReturn:
   return returnCode;

/* These is an error return case */

QMgrQueueReleaseReturn:
   API_req_p->qr.requestError = N8_QUEUE_REQUEST_ERROR;
   API_req_p->qr.requestStatus = N8_QUEUE_REQUEST_FINISHED;
   N8_AtomicUnlock(queue_p->queueControlSem);
   return returnCode;

} /* N8_QMgrQueue */

/*****************************************************************************
 * QMgrCmdError
 *****************************************************************************/
/** @ingroup QMgr
 * @brief a problems has been detected with the current command block, 
 *        write NOPs all command block for this request so the hardware
 *        can be re-enabled
 *
 *    -----------------
 *  0 |               | base (0)        first < last
 *    -----------------                 1, 2, 3 have commands in an errored
 *  1 | Error Request | first (1)       request, read pointer points to (2)
 *    -----------------                 which is command that halted hardware.
 *  2 | Error Request | read (2)
 *    -----------------
 *  3 | Error Request | last (3)        read >= first && read <= last
 *    -----------------                 then its halting command
 *  4 |               | write (4)
 *    -----------------                 read < first || read > last
 *  5 |               |                 then its not halting command
 *    -----------------                 (but if its halted clean up anyhow)
 *  6 |               |
 *    -----------------
 *  7 |               |
 *    -----------------
 *
 *
 *    -----------------
 *  0 | Error Request | base (0)        last < first
 *    -----------------                 7, 1, 2 have commands in an errored
 *  1 | Error Request | last (1)        request, read pointer points to (7)
 *    -----------------                 which is command that halted hardware.
 *  2 |               |
 *    -----------------
 *  3 |               |                 read >= first || read <= last
 *    -----------------                 then its halting command
 *  4 |               |
 *    -----------------                 read < first && read > last
 *  5 |               |                 then its not halting command
 *    -----------------                 (but if its halted clean up anyhow)
 *  6 |               |
 *    -----------------
 *  7 | Error Request | first (7) read (7)
 *    -----------------
 *
 *
 *
 *
 * @param unit            RO: this is an EA or PK execution unit.
 *        chip            RO: the chip number for the failed command
 *        readQueueIndex  RO: the index of the command that has failed
 *        configStatus    RO: the value of the status word for the
 *                            failed command
 *
 * @par Externals:
 *
 * @return
 *   None.
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine is called from interrupt context and assumes exclusive
 *    access to the command blocks pointed to by the error condition.
 *
 * @par Assumptions:
 *   This function assumes that QMgrCheckQueue has been called prior to
 *   this call to ensure apiReadIdx is pointing to the failed command.
 *   This command has halted hardware and we need to write NOPs to this 
 *   command and to all subsequent commands in this request.
 *
 ******************************************************************************/

void
QMgrCmdError(N8_Component_t    unit,
             int               chip,
             int               readQueueIndex,
             int               configStatus)
{
   uint32_t                     requestCommandIndex;
   uint32_t                     requestLastCommand;
   uint32_t                     requestNextCommand;
   API_Request_t               *API_Request_p;
   QueueControl_t              *queue_p;

   queue_p = &(queueTable_g.controlSets_p[chip][unit]);

   /* An error has occurred, the failing command and any subsequent     */
   /* commands in the request must be changed to NOPs and restarted     */

   /* Increment our count of hardware errors encountered.            */
   queue_p->stats.hardwareErrorCount++;

   /* The apiReadIdx pointing into the requestQueue of the queue     */
   /* will be the last request that had a command that was           */
   /* executing, therefore it is the request with the errored        */
   /* command.                                                       */
   API_Request_p = 
        (API_Request_t *)queue_p->requestQueue[queue_p->apiReadIdx];

   /* Mark the request as errored.                                   */
   API_Request_p->qr.requestError = N8_QUEUE_REQUEST_ERROR;
   API_Request_p->err_rpt_bfr.errorReturnedFromSimon = configStatus;
   API_Request_p->err_rpt_bfr.indexOfCommandThatCausedError =
      ((readQueueIndex - API_Request_p->index_of_first_command) %
       queue_p->sizeOfQueue) + 1;

   /* NOP out the request starting at the errored command to the     */
   /* end of the request that caused the hardware to halt.           */
   requestCommandIndex = readQueueIndex;
   requestLastCommand = API_Request_p->index_of_last_command;
   requestNextCommand = (requestLastCommand+1) & queue_p->sizeMask;

   do
   {

      if ( QMGR_IS_EA(queue_p) )
      {
         (queue_p->EAq_head_p + requestCommandIndex)->opcode_iter_length =
            EA_Cmd_NOP;
      }
      else
      {
         (queue_p->PKq_head_p + requestCommandIndex)->opcode_si = PK_Cmd_NOP;
	 if (requestCommandIndex == requestLastCommand)
	 {
            (queue_p->PKq_head_p + requestCommandIndex)->opcode_si |= 
		    PK_Cmd_SI_Mask;
	 }
      }

      requestCommandIndex = (requestCommandIndex+1) & queue_p->sizeMask;

   } while (requestCommandIndex != requestNextCommand);

#ifdef SUPPORT_INT_CONTEXT_CALLBACKS
   /* This request was submitted by a kernel operation, perform */
   /* the callback processing immediately.                      */
   if (API_Request_p->userSpace == N8_FALSE)
   {
      N8_MemoryHandle_t           *kmem_p;

      /* Perform the user's callback if needed */
      if (API_Request_p->usrCallback)
      {
         API_Request_p->usrCallback (API_Request_p->usrData, 
         N8_HARDWARE_ERROR);
      }
      /* Free the API request */
      kmem_p = (N8_MemoryHandle_t *)((int)API_Request_p - N8_BUFFER_HEADER_SIZE);
      N8_FreeBuffer(kmem_p);
   }
#endif

}

/*****************************************************************************
 * N8_QMgrDequeue
 *****************************************************************************/
/** @ingroup QMgr
 * @brief  API read Interface to the EA/PK Queue:  
 *         This call is optional and used to support device polling. It
 *         simply decrements the count of completed requests available.
 *
 * @param
 *
 * @par Externals:
 *  requestsComplete.
 *
 * @return
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine use the atomic library to ensure operation is atomic.
 *
 * @par Assumptions:
 *
 ******************************************************************************/

N8_Status_t N8_QMgrDequeue(void)
{
   n8_atomic_sub(requestsComplete, 1);
   return N8_STATUS_OK;
}

/*****************************************************************************
 * QMgrPoll
 *****************************************************************************/
/** @ingroup QMgr
 * @brief  API poll Interface to the EA/PK Queue:  
 *         This call simply returns the count of completed requests 
 *         available.
 *
 * @param
 *
 * @par Externals:
 *  requestsComplete.
 *
 * @return
 *  number of complete events
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine use the atomic library to ensure operation is atomic.
 *
 * @par Assumptions:
 *  This call assumes the user is calling N8_QMgrDequeue to maintain the
 *  requestsComplete count correctly.
 *
 ******************************************************************************/

int QMgrPoll(void)
{
   return(n8_atomic_read(requestsComplete));
}
/*****************************************************************************
 * QMgrCount
 *****************************************************************************/
/** @ingroup QMgr
 * @brief  This call simply returns the count of requests currently queued to
 *         the NSP2000
 *
 * @param
 *
 * @par Externals:
 *  requestsQueued.
 *
 * @return
 *  number of queued requests
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine use the atomic library to ensure operation is atomic.
 *
 * @par Assumptions:
 *
 ******************************************************************************/

int QMgrCount(N8_Component_t unit)
{
   return(n8_atomic_read(requestsQueued[unit]));
}

