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

static char const n8_id[] = "$Id: helper.c,v 1.2 2008/11/03 04:31:01 tls Exp $";
/*****************************************************************************/
/** @file helper.c                                                           *
 *  @brief BSDi System Call Abstraction.                                     *
 *                                                                           *
 * This file implements the BSDi versions of some standard system calls      *
 * that drivers commonly make, in a variety of platforms. It also ensures    *
 * that the necessary system includes are included as well.                  *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/12/04 bac   Fixed n8_usleep to work when the tick interval changes.
 * 05/05/03 brr   Moved memory functions to n8_memory_bsd.c.
 * 04/23/03 brr   Remove warnings from n8_delay_ms.
 * 03/12/03 jpw   Added function n8_delay_ms - keep this spinlock safe!
 * 12/10/02 brr   Added function n8_gettime.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/12/02 brr   Added N8_WaitEventInt function.
 * 04/02/02 spm   Changed usec normalizer in n8_usleep from 1000 to 1000000.
 *                this assures that units of jiffies are passed to tsleep.
 *                Added case to sleep for minimum yield if usecs is less
 *                than minimum yield.
 * 03/11/02 brr   Update for 2.1.
 * 10/12/01 mmd   Renamed Atomic*() routines to N8_Atomic*().
 * 10/04/01 mmd   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver BSDi System Call Abstraction. 
 */

#include "helper.h"
#include "irq.h"
#include "n8_pub_errors.h"
#include "n8_driver_main.h"

#include <sys/proc.h>

/* Since BSD does not give us a mechanism to update the write pointer */
/* and ensure this process is asleep before the interrupt fires, this */
/* fuction attempts to solve the problem. It goes to sleep for an     */
/* arbitrary long wait. If the ISR wakes this process up, we check    */
/* the request immediately and return on realizing the event has      */
/* completed. If the ISR fails to wake us because we are not          */
/* completely suspended, the timeout guarentees we wake up eventually */
/* and check to see if the ISR has marked our request as complete. In */
/* which case we return succes. Otherwise we exceed out timeout count */
/* and return FAILURE. The count value may need adjustment based on   */
/* traffic load on the system.                                        */

int N8_WaitEventInt(n8_WaitQueue_t *waitQueue_p, API_Request_t *API_req_p)
{
   int count = 0;

   do
   {
      if (API_req_p->qr.requestStatus == N8_QUEUE_REQUEST_FINISHED)
      {
         /* The event has completed, return success. */
         return 0;
      }
      tsleep(waitQueue_p, PWAIT, "QMQueue", (1*HZ));
      count++;
   }
   while (count < 5);

   return (1);
}

/*****************************************************************************
 * N8_BlockWithTimeout
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Block for notification or specified timeout.
 *
 * This routine abstracts the Linux system call to block until release or
 * timeout.
 *
 * @param *WaitQueue RO:  Specifies the block element
 * @param  timeout   RO:  Specifies the timeout
 * @param  debug     RO:  Specifies whether to display debug messages
 *
 * @par Externals:
 *    N/A
 *
 * @return 
 *    1 = success - was woken up by corresponding call to wakeup.
 *    0 = failure - timed out.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

unsigned char N8_BlockWithTimeout( wait_queue_head_t *WaitQueue,
                                   unsigned long      timeout,
                                   unsigned char      debug )
{
     int rc;

     /* BLOCK UNTIL RELEASE, TIMEOUT, OR SIGNAL */
     rc = tsleep(WaitQueue, (PWAIT | PCATCH), "N8_BlockWithTimeout", (timeout * hz));
     if (!rc)
     {
          if (debug)
          {
               printf("NSP2000: Received desired interrupt.\n");
          }
          return 1;
     }
     if (debug && (rc == EWOULDBLOCK))
     {
          printf("NSP2000: Timed out while awaiting interrupt.\n");
     }
     else if (debug)
     {
          printf("NSP2000: An unexpected signal has interrupted N8_BlockWithTimeout.\n");
     }
     return 0;
}


#if 0
/*****************************************************************************
 * n8_usleep
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Sleep usec seconds.
 *
 * The minimum delay is N8_MINIMUM_YIELD_USECS, which is usually a system
 * clock tick.
 *
 *
 * @param usecs         RO: Microseconds to sleep
 *
 * @par Externals:
 *    HZ is the CPU clock speed in Hertz (defined in Linux
 *    kernel headers; usually 100 Hz).
 *
 * @return
 *    N8_STATUS_OK              Success
 *
 * @par Errors:
 *    See return section.
 *
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *
 *****************************************************************************/

N8_Status_t
n8_usleep(unsigned int usecs)
{
   char tmp;

   /* the minimum amount to sleep is 1 tick.  if requested sleep time is less
    * than 1 tick, sleep for 1 tick. */
   if (usecs < N8_USECS_PER_TICK)
   {
      tsleep(&tmp,PZERO,"NSP2000 udelay", 1 /* tick */);
   }
   else
   {
      tsleep(&tmp,PZERO,"NSP2000 udelay",((usecs)*hz/N8_USECS_PER_SEC));
   }

   return N8_STATUS_OK;

}

/*****************************************************************************
 * n8_delay_ms
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Sleep for given milliseconds.
 *
 *
 * @param usecs         RO: Microseconds to sleep
 *
 * @par Externals:
 *    HZ is the CPU clock speed in Hertz (defined in Linux
 *    kernel headers; usually 100 Hz).
 *
 * @return
 *    N8_STATUS_OK              Success
 *
 * @par Errors:
 *    See return section.
 *
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *
 *****************************************************************************/

N8_Status_t
n8_delay_ms(unsigned int milliseconds)
{

   DELAY((milliseconds*1000));

   return N8_STATUS_OK;

}
/*****************************************************************************
 * n8_gettime
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Get current timeval                                                  
 *
 * @param n8_timeResults_p      WO: Returned time value in n8_timeval_t,
 *                                  sec and usec
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK              Success
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *
 *****************************************************************************/

N8_Status_t
n8_gettime( n8_timeval_t *n8_timeResults_p )
{

   struct timespec  ts;
   N8_Status_t returnResults = N8_STATUS_OK;

   getnanotime(&ts);

   /* Timespec has a seconds portion and a nano seconds portion.        */
   /* Thus we need to divide to convert nanoseconds to microseconds.    */
   n8_timeResults_p->tv_sec = ts.tv_sec;
   n8_timeResults_p->tv_usec = ts.tv_nsec / 1000;

   return returnResults;

} /* n8_gettime */

#endif
