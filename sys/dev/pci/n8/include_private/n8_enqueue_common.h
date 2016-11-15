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
 * @(#) n8_enqueue_common.h 1.32@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_enqueue_common.h
 *  @brief Common queueing definitions.
 *
 * Common infrastructure for queueing requests.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/19/03 brr   Eliminate freeList_p from API request.
 * 05/15/03 brr   Eliminated obsolete N8_freeList_t & include n8_rn_common.h.
 * 04/01/03 brr   Eliminated APIReqIndex.
 * 03/19/03 brr   Added APIReqIndex to API_Request_t to support waiting on a
 *                specifice request.
 * 03/10/03 brr   Added support for API callbacks.
 * 06/10/02 hml   Added the userRequest boolean to API_Request_t.
 * 06/06/02 brr   Moved waitQueue to the queue structure.
 * 05/30/02 brr   Added common struct for queue statistics.
 * 05/14/02 brr   Removed all fields associated with chained requests.
 * 05/07/02 msz   Added field to QMgr Request to mark as synchronous.
 * 04/30/02 brr   Removed obsolete fields in API requests.
 * 04/10/02 mmd   Removed obsolete NSP_PSEUDO_DEVICE_NAME.
 * 03/27/02 hml   Removed obsolete include file.
 * 03/26/02 brr   Added dataoffset to API_Request_t.
 * 03/06/02 msz   Added physical address to QMgrRequest
 *                (Chained subsequent messages in kernel support)
 * 02/28/02 brr   Move internal QMgr structure to QMQueue.h
 * 02/20/02 brr   Removed obsolete "size" variables since FPGA's are not
 *                supported.
 * 02/18/02 brr   Support user space API, removed obsolete Context memory
 *                references.
 * 02/19/02 msz   No more randomParmSem.
 * 01/31/02 brr   Added postProcessBuffer to API_Request_t.
 * 01/25/02 bac   Added ccmFreeIndex_p to QueueControl_t.
 * 01/28/02 msz   No more nextRequestToBeReturnedToAPI_index
 * 01/21/02 msz   Added event wait type information.
 * 01/23/02 brr   Modified API_Request_t to support reduced memory allocations.
 * 01/17/02 msz   Changes to support mirror registers - BUG  413
 * 01/16/01 brr   Moved several definition here from QMgrInit.h.
 * 12/15/01 msz   Adding shadow copy of command queue.  Fix for BUG 382, 411.
 * 12/04/01 msz   Changes to allow chaining of requests.
 *                Changed N8_QueueStatusCodes_t, added N8_QueueErrorCodes_t
 * 11/15/01 msz   Added nextAPIRequest_t to API_Request_t so they can be
 *                chained together.
 * 11/11/01 mmd   Added nspData field to QueueControl_t.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/30/01 hml   Added EA_ContextSize and SKS_MemorySize to QueueControl_t.
 * 10/29/01 msz   Added bncConstantsSem.
 * 10/02/01 bac   API_Request_t copyBackCommandBlock changed name and is now a
 *                N8_Boolean_t. 
 * 10/02/01 msz   Changed sharedPerUnit as it now accessed differently.
 * 09/26/01 msz   Merged with hml 9/21/01 and 09/13/01 changes below, note
 *                however that there is no longer any seed_source to add.
 * 09/21/01 hml   Reordered and augmented the Queue control structure.
 * 09/13/01 hml   Added seed_source.
 * 09/26/01 msz   No longer need seed_source, started shared per execution
 *                unit data.
 * 09/24/01 msz   Added some comments.
 * 09/13/01 msz   Added seed_source.
 * 09/06/01 bac   Renumbered enums to start with non-zero (BUG #190).
 * 08/29/01 msz   Moved elapsedTimeInQueueAndSimon so it is per request rather
 *                than per command.  Eliminated numCmdBlksProcessed from
 *                API_Request_t, eliminated nextCmdToBeReturnedToAPI_index,
 *                index_of_next_command_to_evaluate.
 * 08/27/01 msz   Renamed fcnToCallbackWhenRequestIsFilled to callback
 *                and FcnCalledOnRequestCompletionOrError to callback
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon.h and
 *                simon_driver_api.h.
 * 08/20/01 msz   Use n8_time and n8_timeval_t
 * 08/08/01 msz   Removed include of n8_semaphore.h
 *                Merged other Revision history into this one.
 * 08/08/01 hml   Added contextListSem and randomParmSem to queue control struct.
 * 08/06/01 msz   Combined sizeOfRNG_Q with sizeOfQueue, some moving around
 *                of offsets of values in QueueControl_t.
 * 08/01/01 bac   Removed N8_QueueStatusCodes_t and N8_QueueReturnCodes_t
 *                as they are now defined elsewhere.
 * 07/31/01 bac   Documented simon_error_report_t to clarify the index.
 * 07/30/01 bac   Added BNC_multiplier and PK_BytesPerBignumDigit to
 *                queue control.
 * 07/23/01 bac   Forced to add ENQUEUE enums back in for integration.  They
 *                will be removed later.
 * 07/17/01 bac   Consolidated all success enums to be N8_QUEUE_SUCCESS.  Added
 *                values to enum definitions for easier lookup.
 * 07/20/01 hml   Added chip to the queue control.
 * 07/20/01 bac   Added hardware type and BNC bits per digit to the queue
 *                control.
 * 07/19/01 bac   Moved LLReqCtrl_t, API_Request_t, and RN_Request_t to here
 *                from other header files.
 * 07/10/01 bac   Changed DEFAULT_QUEUE_LEN_LOG_2 to be 10 (1024).
 * 06/15/01 hml   added unit and chip to the API_Request_t structure.
 * 05/01/01 bac   Added to API_Reqeust_t the freeList_p and postProcessingData_p
 * 04/24/01 bac   fixed merge error.
 * 04/16/01 jke   moved enqueue retvals from n8_pk_common.h, genericized names
 * 04/16/01 jke   moved status and return types from PK*.h and genericized for 
 *                use in EA and PK
 * 04/12/01 jke   added header per spec
 * 04/11/01 bac   Standardization.  Removed non-ANSI-compliant code.
 * 03/28/01 jke   copied from EnQueue.h 
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */
/*****************************************************************************
 * n8_enqueue_common.h
 *
 * #defs and typedefs used in implementing the request queues.  
 *
 *****************************************************************************/



#ifndef N8_ENQUEUE_COMMON_H
#define N8_ENQUEUE_COMMON_H

#include "n8_time.h"                     /* for the n8_timeval_t */
#include "n8_common.h"
#include "n8_OS_intf.h"
#include "n8_pub_common.h"
#include "n8_pk_common.h"                /* for typedef of PK_CMD_BLOCK */
#include "n8_ea_common.h"                /* for typedef of EA_CMD_BLOCK */
#include "n8_malloc_common.h"


typedef struct simon_error_report_t
{
  int errorReturnedFromSimon;            /* error returned from the hardware.
                                          * look at the hardware specification
                                          * error flags for deciphering this
                                          * number */ 
  int indexOfCommandThatCausedError;     /* index of command that caused the
                                          * error.  (1...number of commands) */
} simon_error_report_t;

  
/*****************************************************************************
 * N8_QueueErrorCodes_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @brief This enum contains the error status of a request from SAPI to QMGR.
 *
 *  N8_QUEUE_REQUEST_OK                 The request has no error
 *  N8_QUEUE_REQUEST_ERROR              There was an error in or with the
 *                                      request.
 *
 *****************************************************************************/

/* Error status of request/event that is in the queue */
typedef enum 
{
   N8_QUEUE_REQUEST_OK                    = 1,
   N8_QUEUE_REQUEST_ERROR                 = 2,
} N8_QueueErrorCodes_t;


/*****************************************************************************
 * QMgrRequest_t 
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct QMgrRequest_t
 *  @brief This structure contains the common part of a RN_request_ and
 *         API_Request_t (and any other request types that might be added).
 *
 *  requestStatus   tells if the request is in API or QMgr, or if its FINISHED
 *  requestError    tells if the request is in error state
 *  unit            tells the component the request goes to (eg. PK, EA, RNG)
 *  chip            tells what chipset the request is for
 *  callback        is a pointer to a callback routine, or a NULL if
 *                  there is no callback routine.
 *  physicalAddress The Physical Address of this request (used in user address
 *                  to kernel address conversions)
 *  synchronous     This request has been generated by a synchronous API call.
 *
 *****************************************************************************/

typedef struct QMgrRequest_t
{
   N8_QueueStatusCodes_t  requestStatus;
   N8_QueueErrorCodes_t   requestError;
   N8_Component_t         unit;
   int                    chip;
   void (* callback)     (struct QMgrRequest_t *);
   unsigned long          physicalAddress;
   N8_Boolean_t           synchronous;

} QMgrRequest_t;


/*****************************************************************************
 * RN_Request_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct RN_Request_t
 *  @brief This structure contains the a request to the RNG queue.
 *
 *  userBuffer_p        Where to copy the random numbers to.
 *  userRequest         Flag to indicate request came from user space.
 *  numBytesRequested   Has the number of random bytes the user requested
 *  numBytesProvided    Used to store how many bytes have been copied out.
 *
 *****************************************************************************/

typedef struct RN_Request_t 
{
   char                  *userBuffer_p;
   int                    userRequest;    /* Flag to indicate request    */
                                          /* received from user space.   */
   int                    numBytesRequested;
   int                    numBytesProvided;
} RN_Request_t;


/*****************************************************************************
 * API_request_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct API_request_t
 *  @brief This structure contains the a request to the EA or PK queue.  The
 *         first section of this structure must be a QMgrRequest_t type.
 *  qr     Has common QMgrRequest data.
 *
 *****************************************************************************/

typedef struct 
{
   QMgrRequest_t          qr;   /* NOTE:  the QMgrRequest_t contains the common
                                 * elements between an API_Request_t and an
                                 * RN_Request_t.  IT MUST BE THE FIRST ENTRY IN
                                 * THIS STRUCTURE. */

   /* QMgr Only members */
   int                   index_of_first_command;
   int                   index_of_last_command;

   /* Shared members */
   EA_CMD_BLOCK_t        *EA_CommandBlock_ptr;   
   PK_CMD_BLOCK_t        *PK_CommandBlock_ptr;  
   int                    numNewCmds;
   N8_Boolean_t           copyBackCommandBlock;
   simon_error_report_t   err_rpt_bfr;

   N8_Callback_t          usrCallback;     /* User's callback function */
   void                  *usrData;         /* Pointer to user's data   */

   /* SAPI Only members */
   uint32_t              dataoffset;
   N8_Boolean_t          userRequest; /* N8_TRUE: This request was alloced by
                                         N8_AllocateRequest */
   N8_Boolean_t          userSpace;   /* N8_TRUE: This request was generated
                                         in user space */

   /* set this pointer to any data that needs to be associated with
    * this request for processing after the completion of the request
    */ 
   void                  *postProcessingData_p;
   int                    copyBackSize;
   N8_Buffer_t           *copyBackFrom_p;
   N8_Buffer_t           *copyBackTo_p;
   struct uio            *copyBackTo_uio;
   uint32_t              postProcessBuffer[12];
} API_Request_t;


/*****************************************************************************
 * QueueStats_t
 *****************************************************************************/
/** @ingroup QMgr
 *  @struct QueueStats_t
 *  @brief This structure contains the statistics for each queue.
 *
 *****************************************************************************/

typedef struct
{

     uint32_t      hardwareErrorCount; /**< Counter of hardware errors
                                            detected.  A non zero value does
                                            not mean there is a hardware
                                            problem. */
     uint32_t      requestsQueued;     /**< Counter of number of requests
                                            that have been queued (not the
                                            number of current requests). */
     uint32_t      requestsCompleted;  /**< Counter of number of queued requests
                                            that have been completed, regardless
                                            of success or failure. */
     uint32_t      requestCheckCalled; /**< Counter of number of times we
                                            have called check request on
                                            this queue.  */
     uint32_t      requestsPreempted;  /**< Counter of number of times a
                                            request has been preempted by
                                            a signal or timeout.  */
} QueueStats_t;


typedef struct
{
     N8_Unit_t        chip;

     QueueStats_t     PKstats;
     QueueStats_t     EAstats;
     QueueStats_t     RNstats;

} N8_QueueStatistics_t;


#endif               /* ifdef N8_ENQUEUE_COMMON_H */
