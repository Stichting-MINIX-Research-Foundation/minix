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

static char const n8_id[] = "$Id: n8_callback.c,v 1.2 2014/03/25 16:19:14 christos Exp $";
/*****************************************************************************/
/** @file n8_callback.c
 *  @brief Routines for implementing the API callbacks.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 06/26/03 brr   Send signal to kernel callback thread when terminated.
 * 06/25/03 brr   Moved all thread data into the data structure allocated by 
 *                callbackInit so that a new callback thread can be started
 *                immediately after terminating one.
 * 06/23/03 brr   Verify previous callback thread has terminated completely
 *                before attempting to start another. (Bug 934)
 * 04/24/03 brr   Deallocate resources in callbackShutdown when no thread was
 *                started. Modified N8_EventPoll for efficiency and to return
 *                the number of completed events.
 * 04/08/03 brr   Fixed warnings.
 * 04/03/03 brr   Removed overhead of calling N8_EventCheck from the callback
 *                thread to improve performance.
 * 04/01/03 brr   Include n8_sdk_config instead of n8_driver_parms.
 * 04/01/03 brr   Updated for changes to N8_WaitOnRequest.
 * 03/24/03 brr   Modified thread to wait on AMBA interrupt when no requests 
 *                are queued.
 * 03/20/03 brr   Updated for changes to N8_WaitOnRequest.
 * 02/25/03 brr   Original version.
 ****************************************************************************/
/** @defgroup n8_event Callback processing
 */
 
#include "n8_common.h"
#include "n8_pub_service.h"
#include "n8_OS_intf.h"
#include "n8_malloc_common.h"
#include "n8_semaphore.h"
#include "n8_sdk_config.h"
#include "n8_driver_api.h"
#include "n8_util.h"

#ifdef SUPPORT_CALLBACK_THREAD

typedef struct
{
   int          n8_thread;
   int          timeout;
   volatile int eventReadIndex;
   volatile int eventWriteIndex;
   int          eventMax;
   n8_Lock_t    eventLock;
   N8_Event_t   n8_Events[0];
} N8_CallbackData_t;

static N8_CallbackData_t *n8_callbackData_gp;
static N8_Thread_t        n8_callbackThread_g;

#ifdef __KERNEL__
   struct task_struct *threadStruct;
#endif
/*****************************************************************************
 * queueEvent
 *****************************************************************************/
/** @ingroup n8_event
 * @brief This function inserts an event into the SDK's event queue.
 *
 * This function inserts an event into the SDK's event queue. This function
 * takes the eventLock_g to ensure mutual exclusion.
 *
 * @par Externals
 *    n8_callback_gp    - Pointer to the callback threads data structure.
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *    N8_EVENT_QUEUE_FULL - No entries are available in the event queue.<br>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *    This is the only function that should increment eventWriteIndex,
 *    the write index in the SDK's list of Events.
 *****************************************************************************/
N8_Status_t queueEvent(N8_Event_t *event_p)
{
   int nextIndex;
   N8_Status_t ret = N8_EVENT_QUEUE_FULL;

   if (n8_callbackData_gp->eventMax != 0)
   {
      /* Acquire eventLock_g to update eventWriteIndex_g */
      N8_acquireProcessSem(&n8_callbackData_gp->eventLock);

      /* Calculate the next index */
      nextIndex = n8_callbackData_gp->eventWriteIndex + 1;
      if (nextIndex == n8_callbackData_gp->eventMax)
      {
         nextIndex = 0;
      }

      if (nextIndex != n8_callbackData_gp->eventReadIndex)
      {
         /* The queue is not full, queue the event */
         n8_callbackData_gp->n8_Events[n8_callbackData_gp->eventWriteIndex] = *event_p;
         n8_callbackData_gp->eventWriteIndex = nextIndex;
         ret = N8_STATUS_OK;
      }

      /* Release the eventLock_g */
      N8_releaseProcessSem(&n8_callbackData_gp->eventLock);
   }

   return ret;
}


/*****************************************************************************
 * n8_callbackThread
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Runs as a seperate thread polling the list of events and completing 
 *        them.
 *
 * This function is started as a seperate thread. It continually runs through
 * the SDK's internal list of events, checking each one for its 
 * completion status. If the event is completed, the user's callback is 
 * invoked and the event freed.
 *
 * @par Externals
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *    This is the only function that should increment eventReadIndex,
 *    the read index in the SDK's list of Events.
 *    N8_EventPoll should not be called if this thread is running.
 *****************************************************************************/
N8_Status_t n8_callbackThread(N8_CallbackData_t *callbackData_p)
{
   int         nextRead;
   N8_Event_t *events_p;
   QMgrRequest_t *qreq_p = NULL;
   N8_Status_t usrStatus;

   /* Perform Linux Kernal specific initialization */
#ifdef __KERNEL__
   threadStruct = current;
   /* Mask out the unwanted signals */
   current->blocked.sig[0] |= sigmask(SIGINT) | sigmask(SIGTERM);
   recalc_sigpending(current);

   /* set name of this process (max 15 chars + 0 !) */
   strcpy(current->comm, "NSP2000 Thread");
#endif
        
   while (callbackData_p->n8_thread == TRUE)
   {
      if (callbackData_p->eventReadIndex != callbackData_p->eventWriteIndex)
      {
         events_p = &callbackData_p->n8_Events[callbackData_p->eventReadIndex];
         qreq_p = (QMgrRequest_t *) events_p->state;

         if ( qreq_p->requestStatus == N8_QUEUE_REQUEST_FINISHED )
         {
            if ( qreq_p->requestError != N8_QUEUE_REQUEST_ERROR )
            {
               /* Do the callback if needed. */
               if ( qreq_p->callback != NULL )
               {
                  qreq_p->callback( qreq_p );
               }
               /* events_p->status = N8_QUEUE_REQUEST_FINISHED; */
	       usrStatus = N8_STATUS_OK;
            }
            else
            {
               /* events_p->status = N8_QUEUE_REQUEST_COMMAND_ERROR; */
	       usrStatus = N8_HARDWARE_ERROR;
            }

            /* free the request. */
            freeRequest((API_Request_t *)qreq_p);

            /* Perform the user's callback. */
            if (events_p->usrCallback)
            {
               events_p->usrCallback( events_p->usrData, usrStatus );
            }

            /* Free the position in the Event queue */
            nextRead = callbackData_p->eventReadIndex + 1;
            if (nextRead == callbackData_p->eventMax)
            {
               nextRead = 0;
            }
            callbackData_p->eventReadIndex = nextRead;
	 }
         else
         {
            N8_WaitOnRequest(callbackData_p->timeout);
         }
      }
      else
      {
         N8_WaitOnRequest(callbackData_p->timeout);
      }
   }

   /* Free the resources allocated by callback initialization.          */
   /* This is done here instead of in callbackShutdown to avoid freeing */
   /* resources while the callback thread continues to reference them.  */
   callbackData_p->eventMax = 0;
   N8_deleteProcessSem(&callbackData_p->eventLock);
   N8_UFREE(callbackData_p);

   return N8_STATUS_OK;
}

/*****************************************************************************
 * n8_callbackInit
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Allocates resources for the event queue and starts callback thread.
 *
 * This function is called by N8_InitializeAPI. It allocates resources used
 * by callback functions and starts a seperate thread if the timeout value is
 * non-zero. If the timeout is zero, the resources are allocated, but it is 
 * assume the user will be calling N8_EventPoll to complete the events.
 *
 * @par Externals
 *    n8_callback_gp    - Pointer to the callback threads data structure.
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *    N8_EVENT_ALLOC_FAILED - insufficient resources to allocate event array.<br>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *****************************************************************************/
N8_Status_t callbackInit(uint32_t numEvents, uint32_t timeout)
{
   int dataSize = sizeof(N8_CallbackData_t) + 
	           ((numEvents + 1) * sizeof(N8_Event_t));

   n8_callbackData_gp = N8_UMALLOC(dataSize);
		                   
   if (n8_callbackData_gp == NULL)
   {
      return (N8_EVENT_ALLOC_FAILED);
   }

   memset(n8_callbackData_gp, 0, dataSize);
   N8_initProcessSem(&n8_callbackData_gp->eventLock);
   n8_callbackData_gp->eventMax = numEvents;
   if (timeout)
   {
      n8_callbackData_gp->n8_thread = TRUE;
      n8_callbackData_gp->timeout = timeout;
      N8_THREAD_INIT(n8_callbackThread, n8_callbackData_gp, n8_callbackThread_g);
   }

   return(N8_STATUS_OK);
}

/*****************************************************************************
 * n8_callbackShutdown
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Sets the global flag to alert the callback thread to shutdown.
 *
 * This function is called by N8_TerminateAPI. It Sets the flag n8_thread
 * flag to alert the callback thread to shutdown or frees resources if
 * polling was configured.
 *
 * @par Externals
 *    n8_callback_gp    - Pointer to the callback threads data structure.
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *****************************************************************************/
N8_Status_t callbackShutdown(void)
{

   if (n8_callbackData_gp->n8_thread == FALSE)
   {
      /* There was no callback thread started, so it is safe to deallocate */
      /* resources immediately.                                            */
      n8_callbackData_gp->eventMax = 0;
      N8_deleteProcessSem(&n8_callbackData_gp->eventLock);
      N8_UFREE(n8_callbackData_gp);
   }
   else
   {
      /* If a callback thread was started, just set the n8_thread   */
      /* flag so the thread terminates, then deallocate resources.  */
      n8_callbackData_gp->n8_thread = FALSE;

#ifdef __KERNEL__
      send_sig(SIGKILL, threadStruct, 0);
#endif
   }


   return(N8_STATUS_OK);
}


/*****************************************************************************
 * N8_EventPoll
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Run through the list of events and complete them.
 *
 * Run through the SDK's internal list of events, checking each one for its 
 * completion status. If the event is completed, the user's callback is 
 * invoked and the event freed.
 *
 * @par Externals
 *    n8_callback_gp    - Pointer to the callback threads data structure.
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *    This is the only function that should increment eventReadIndex,
 *    the read index in the SDK's list of Events.
 *    This function should not be called if the callback thread is running.
 *****************************************************************************/
int N8_EventPoll(void)
{
   int         nextRead;
   N8_Event_t *events_p;
   int         eventsComplete = 0;
   QMgrRequest_t *qreq_p = NULL;
   N8_Status_t usrStatus;

   while (n8_callbackData_gp->eventReadIndex != n8_callbackData_gp->eventWriteIndex)
   {
      events_p = &n8_callbackData_gp->n8_Events[n8_callbackData_gp->eventReadIndex];
      qreq_p = (QMgrRequest_t *) events_p->state;

      if ( qreq_p->requestStatus == N8_QUEUE_REQUEST_FINISHED )
      {
         if ( qreq_p->requestError != N8_QUEUE_REQUEST_ERROR )
         {
            /* Do the callback if needed. */
            if ( qreq_p->callback != NULL )
            {
               qreq_p->callback( qreq_p );
            }
            /* events_p->status = N8_QUEUE_REQUEST_FINISHED; */
            usrStatus = N8_STATUS_OK;
         }
         else
         {
            /* events_p->status = N8_QUEUE_REQUEST_COMMAND_ERROR; */
            usrStatus = N8_HARDWARE_ERROR;
         }

         /* free the request. */
         freeRequest((API_Request_t *)qreq_p);

         /* Perform the user's callback. */
         if (events_p->usrCallback)
         {
            events_p->usrCallback( events_p->usrData, usrStatus );
         }

         /* Free the position in the Event queue */
         nextRead = n8_callbackData_gp->eventReadIndex + 1;
         if (nextRead == n8_callbackData_gp->eventMax)
         {
            nextRead = 0;
         }
         n8_callbackData_gp->eventReadIndex = nextRead;
	 eventsComplete++;
	 }
   }

   return eventsComplete;
}
#endif
