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

static char const n8_id[] = "$Id: n8_event.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_event.c
 *  @brief Routines for processing asynchronous events.
 *
 * When select API calls are made asynchronously, indicated by passing a
 * non-null pointer to a N8_Event_t structure, the calling routine needs to be
 * able to inquire as to the progress of the call and retrieve results.  This
 * module provides the following functions to that end:<br>
 *    N8_EventCheck
 *    N8_EventWait
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 05/19/03 brr   Clean up include files.
 * 04/01/03 brr   Modified N8_EventWait to pass timeout to N8_WaitOnRequest.
 * 03/19/03 brr   Modified N8_EventWait to pass specific event to 
 *                N8_WaitOnRequest.
 * 03/10/03 brr   Added support for API callbacks.
 * 09/18/02 brr   Replaced InternalEventWait with N8_WaitOnRequest.
 *  7/01/02 arh   Fixed Bug #813: N8_EventCheck returns N8_EVENT_NONE_READY
 *                unless last event complete. Removed extraneous assignment
 *                of N8_EVENT_NONE_READY to *ready_p inside inner for loop.
 * 05/15/02 brr   Removed obsolete chained request functions. Also support RNG
 *                modifications to return random bytes in the queue ioctl.
 * 05/01/02 brr   Removed redundant call to n8_preamble in N8_EventWait. Modified
 *                N8_EventCheck to only call QMgrCheckRequests for RNG events.
 * 04/06/02 brr   Validate events_p and ready_p in N8_EventCheck.
 * 03/29/02 brr   Modified InternalEventWait to use N8_AMBA_TIMER_CHIP.
 * 03/26/02 hml   Modified N8_EventCheck for Bug 635 to again return 
 *                N8_EVENT_NONE_READY, but not spin forever.
 * 03/22/02 brr   Modified N8_EventCheck to return N8_EVENT_NONE_AVAILABLE if
 *                the event pointer is NULL and count > 0. (Bug 635)
 * 03/07/02 brr   Fix race condition in EventCheck.
 * 03/04/02 brr   Set event status when event is not complete.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/26/02 brr   Removed queue references.
 * 02/27/02 msz   Fixes for N8_WaitOnInterrupt support.
 * 02/22/02 spm   Converted printf's to DBG's and n8_udelay's to n8_usleeps.
 * 02/20/02 msz   N8_CheckEvent simplifications.
 * 02/19/02 brr   Perform callback processing in N8_CheckEvent.
 * 01/21/02 msz   Added support for configurable event wait.
 * 01/16/02 bac   Fixed Bug #478:  the state in an event could be NULL if
 *                previously processed.  Set traps to catch this condition.
 * 01/11/02 msz   Use timer of first nsp only.
 * 01/02/02 msz   Don't use usleep or n8_usleep if hardware is present in
 *                one of the events
 * 12/05/01 msz   Simplification of status codes in preparation for chaining
 *                of callbacks.
 * 11/28/01 mel   Fixed bug #376: n8_event.c uses usleep.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 10/19/01 hml   Fixed compiler warnings.
 * 10/02/01 bac   Added a comment to clear up the use of the return code from
 *                EA_CheckRequest. 
 * 09/10/01 msz   Minor change to correct a return type.
 * 08/07/01 mel   Deleted unnecessary includes.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/09/01 hml   Check for rnreq_p == NULL before we try to free the request
 *                in CheckEvent (BUG #117?)
 * 07/09/01 hml   Set pointer to NULL after a freeRequest and do a break
 *                when the request is null in CheckEvent.
 * 07/06/01 hml   Don't call CheckRequest in N8_EventCheck if the request
 *                is already completed or caused an error (BUG #113).
 * 07/11/01 mel   Porting to VxWorks.
 * 07/02/01 bac   Fixed comments.
 * 06/28/01 bac   Added correct request freeing for RNG events.
 * 06/25/01 bac   More changes for QMgr v.1.0.1
 * 06/19/01 bac   Fleshed out N8_EventCheck and N8_EventWait
 * 05/31/01 bac   Original version.
 ****************************************************************************/
/** @defgroup n8_event Event processing
 */
 
#include "n8_util.h"
#include "n8_common.h"
#include "n8_enqueue_common.h"
#include "n8_API_Initialize.h"
#include "n8_OS_intf.h"
#include "nsp_ioctl.h"
#include "n8_device_info.h"
#include "n8_driver_api.h"

/*****************************************************************************
 * N8_EventCheck
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Check a list of events for completion status.
 *
 * Given a list of events, check each one for its completion status.  Each event
 * will be updated to reflect its current status.
 *
 *  @param events_p            RW:  Array of events to be checked.
 *  @param count               RO:  Number of events.
 *  @param ready_p             RW:  Status of event.
 *
 * @par Externals
 *    None
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *    N8_INVALID_OBJECT - one or more of the events is not valid.<br>
 *    N8_INVALID_VALUE - count < 0 or count > max.
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
N8_Status_t N8_EventCheck(N8_Event_t *events_p, const int count, int *ready_p)
{
   int i;
   N8_Status_t ret = N8_STATUS_OK;
   N8_Boolean_t checkCalled;
   QMgrRequest_t *qreq_p = NULL;
   N8_QueueStatusCodes_t status;
   N8_Status_t usrStatus;

   do
   {
      CHECK_OBJECT(events_p, ret);
      CHECK_OBJECT(ready_p, ret);

      if (count < 1 || count > N8_EVENT_MAX)
      {
         ret = N8_INVALID_VALUE;
      }
      CHECK_RETURN(ret);

      ret = N8_preamble();
      CHECK_RETURN(ret);

      *ready_p = N8_EVENT_NONE_READY;

      /* Per invocation of N8_EventCheck, the _CheckQueue method will only be
       * called once per execution unit.  We set up an array of booleans to see
       * if the execution unit has been called yet or not. */
      checkCalled = N8_FALSE;
      for (i = 0; i < count; i++)
      {
         qreq_p = (QMgrRequest_t *) events_p[i].state;

         /* If the request was previously checked and found
            to be completed or to have caused an error, then
            the request struct would have been freed.  So we
            want to skip the processing. */
         if (qreq_p != NULL)
         {
            /* Note we don't call the CheckRequest for any case
               if the request is already finished or caused an error */

            status = qreq_p->requestStatus;

            if ( status == N8_QUEUE_REQUEST_FINISHED )
            {
#ifdef SUPPORT_DEVICE_POLL
	       N8_QMgrDequeue();
#endif
               if ( qreq_p->requestError != N8_QUEUE_REQUEST_ERROR )
               {
                  /* Do the callback if needed. */
                  if ( qreq_p->callback != NULL )
                  {
                     qreq_p->callback( qreq_p );
                  }
                  events_p[i].status = N8_QUEUE_REQUEST_FINISHED;
		  usrStatus = N8_STATUS_OK;
               }
               else
               {
                  events_p[i].status = N8_QUEUE_REQUEST_COMMAND_ERROR;
		  usrStatus = N8_HARDWARE_ERROR;
               }

               /* free the request. */
               freeRequest((API_Request_t *)qreq_p);
               events_p[i].state = NULL;

#ifdef SUPPORT_CALLBACKS
               /* Perform the user's callback. */
	       if (events_p[i].usrCallback)
               {
                  events_p[i].usrCallback( events_p[i].usrData, usrStatus );
               }
#endif

               if (*ready_p == N8_EVENT_NONE_READY)
               {
                  *ready_p = i;
               }
            }

         } /* qreq_p != NULL */
         else
         {
            /* qreq_p is NULL, but if the request status is 
               N8_QUEUE_REQUEST_FINISHED, we still want to consider
               setting ready_p.  This means the USER would be well served
               to remove finished requests from the list they send in. */
            if (events_p[i].status == N8_QUEUE_REQUEST_FINISHED)
            {
               /* The request is finished */ 
               if (*ready_p == N8_EVENT_NONE_READY)
               {
                  *ready_p = i;
               }
            }
         }

      }
   } while (FALSE);
   return ret;
} /* N8_EventCheck */


/*****************************************************************************
 * N8_EventWait
 *****************************************************************************/
/** @ingroup n8_event
 * @brief Wait on an event to become ready.
 *
 * A <b>blocking</b> call to wait for at least one event to complete.
 *
 *  @param events_p            RW:  Array of events to complete.
 *  @param count               RO:  Number of events.
 *  @param ready_p             RW:  Status of event.
 *
 * @par Externals
 *    None
 *
 * @return
 *    N8_STATUS_OK - all's well.<br>
 *    N8_INVALID_OBJECT - one or more of the events is not valid.<br>
 *    N8_INVALID_VALUE - count < 0 or count > max.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    Multiple events may become ready.  An arbitrary one of those events will
 * be marked as completed.  The others may be complete but their statuses will
 * not be updated to reflect this.
 *****************************************************************************/
N8_Status_t N8_EventWait(N8_Event_t *events_p, const int count, int *ready_p)
{
   N8_Status_t ret;

   do
   {
      /* This is guarenteed to be redundant since N8_EventCheck */
      /* will perform this same check.                          */
      /* ret = N8_preamble(); */
      /* CHECK_RETURN(ret);   */

      ret = N8_EventCheck(events_p, count, ready_p);
      while ((ret == N8_STATUS_OK) &&
             (*ready_p == N8_EVENT_NONE_READY))
      {
         
         N8_WaitOnRequest(1);
         ret = N8_EventCheck(events_p, count, ready_p);
      }
   } while (FALSE);
   return ret;
} /* N8_EventWait */

