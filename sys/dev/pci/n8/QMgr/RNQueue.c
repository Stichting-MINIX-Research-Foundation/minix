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

static char const n8_id[] = "$Id: RNQueue.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file RNQueue.c
 *  @brief Random Number Queue Manager
 *
 *    This file handles the queue of command blocks sent by the API 
 *    to the DeviceDriver/BehavioralModel for the Random Number Generator
 *    hardware (or the model thereof)
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Enable RNH interrupts.
 * 05/13/03 brr   Correctly compute number of elements in RN Queue to determine
 *                when to update the RN write index. (Bug 914)
 * 03/26/03 brr   Modified RNG not to perform PCI accesses with each reqeust
 *                for random bytes.
 * 03/21/03 jpw   Use correct delay time for RNG_WaitForHardwareStart n8_delay
 * 03/12/03 jpw   N8_usleep may use a wait queue which is not safe when holding
 *		  a spin lock. Use N8_delay_ms instead.
 * 12/12/02 jpw   Make sure TOD counter Flag is enabled - Make sure n8_usleep()
 u*	 	  value is specified in microseconds not milliseconds
 * 12/10/02 brr   Removed obsolete function RNH_WaitWhileBusy.
 * 12/10/02 jpw   Change Queue_RN_Request to properly initialize RNG one time
 *		  by using the RNG only first in seed mode to extract seeds
 * 		  to be used as DES keys for the X9.17 expander. Also change
 *		  TOD prescale to correct chip freq and properly set up the TOD
 * 12/09/02 brr   Modified RN_GetParameters to not shut down the RNH.
 * 11/25/02 brr   Removed use of the updateAll semaphore. Use the define from
 *                n8_driver_parms.h to determine which RNG core to use.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 05/30/02 brr   Use common struct for statistics.
 * 05/15/02 brr   Reworked RNG requests such that the random bytes are returned 
 *                by the ioctl. Removed KMALLOC requirements.
 * 05/01/02 brr   Removed references to qr.queue_p.
 * 04/02/02 msz   Need to index into queue by sample size, not straight index.
 *                (Or else random numbers aren't so random)
 * 03/27/02 brr   Update bufferState when requests are queued/dequeued.
 * 03/20/02 msz   Don't do consecutive 32 bit writes.  Workaround for queue
 *                full and wraparound on disable problem.  Replaced most
 *                macros. (BUG 650)
 * 03/20/02 mmd   Incrementing requestsCompleted stat counter in RN_CheckQueue.
 * 03/19/02 msz   Added a couple statistics counters.
 * 03/18/02 msz   Don't loop forever on a hardware error.
 * 03/08/02 msz   If we have a direct physical address, we don't need the
 *                extra copy of random bytes to a temporary area.
 * 03/06/02 brr   Removed SAPI include files.
 * 02/22/02 spm   Converted n8_udelay to n8_usleep.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 02/18/02 msz   More general QMgrRequest, RNG parameter initialization now
 *                done in this code (with a call out to SAPI), no longer need
 *                some routines (RN_InitParameters, RNG_ValidateRequest)
 * 01/16/02 brr   Removed queue intialization now performed by driver.
 * 12/06/01 msz   Disable/enable rnh around updating read_pointer.
 *                Fix for NSP2000 BUG 2.
 * 12/05/01 brr   Removed obsoleted queue allocation.
 * 12/05/01 brr   Move queue initialization to the driver.
 * 12/04/01 msz   Changes to allow chaining of requests.
 * 11/27/01 msz   Don't get process lock and setup queue if we don't have
 *                to - BUG 379
 * 11/14/01 msz   Moved callbacks out of locked area.
 * 11/13/01 brr   Removed all refernces to shared_resource.
 * 11/12/01 msz   Some small fixes, code review items 43,44,45,46,48,49,50,
 *                51,52.
 * 11/11/01 mmd   Modified RNSetParameters to attach to existing command queue
 *                if using real hardware.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/30/01 hml   Changed NSP_2000 to NSP_2000_HW.
 * 10/23/01 dkm   Mod to use enum for seed source from set params.
 * 10/15/01 brr   Removed warnings exposed when optimization turned up.
 * 10/15/01 msz   Protect Behavioral Model with hardwareAccessSem.  Needed
 *                for running in a multithreaded environment.
 * 10/05/01 msz   Added support for mulitple RNG execution units.
 * 10/04/01 msz   Added RN_InitParameters
 * 09/24/01 msz   Shared memory changes.
 * 09/17/01 msz   Wait a bit to let some random numbers be generated.  This
 *                prevents a hardware hang in waiting for lack of busy bit.
 * 09/14/01 bac   Set the bitfields in the command block before allocation to
 *                the default set for unshared kernel memory.  This will need to
 *                be addressed for the multi-process case.
 * 09/14/01 bac   Use new value of seed_source which requires no shifting.
 * 09/06/01 bac   Added include of <string.h> to silence warning.
 * 09/06/01 msz   Set queue to inititialized after it is first initialized.
 *                Changes for host seed.
 * 08/31/01 msz   Some minor changes in init_rng_q found in investigation of
 *                hang waiting for RNH not busy.
 * 08/21/01 msz   Replaced COPY_OUT with QMCopy
 * 08/15/01 brr   Release process semaphore upon exit of init_rng_q.
 * 08/09/01 msz   Added locking.
 * 08/06/01 msz   All macros now take queue_p rather than simon_p, and are
 *                capitalized, and combined sizeOfRNG_Q with sizeOfQueue
 * 07/27/01 bac   Fixed a bug computing the number of elements between the
 *                read pointer and the end of the queue.
 * 07/27/01 bac   Changed use of COPY_OUT to hard-code NSP2000 as the hardware
 *                type to avoid byte swapping.
 * 07/19/01 bac   Retrieve queue_p from request rather than making a call to
 *                QMgr_get_control_struct. 
 * 07/06/01 msz   Check request pointer against being null before using it
 *                to return error.  BUG #114
 * 07/03/01 msz   Moved n8_common.h first as a workaround for not being
 *                able to compile because of multiple uint8_t's in BSDi
 * 06/28/01 hml   Use the rnh_wait_while_busy macro instead of a usleep.
 * 06/28/01 hml   Added some debugging statements.  Make sure to disable the
 *                rnh before writing the rng.
 * 06/27/01 hml   Used the physical address instead of virtual address and
 *                write the seed values immediately before re-enabling the
 *                rnh.
 * 06/25/01 bac   Fixed a bug in GetRandomBytes so that it acquires the queue
 *                pointer. 
 * 06/21/01 hml   Removed chip parameter from init_rng_q routine.
 * 06/21/01 msz   Added some more documentation, re-arranged some checking
 *                of requests.
 * 06/19/01 hml   Converted to Queue Control Architecture.
 * 06/08/01 msz   Added call to open driver
 * 05/07/01 bac   Shifted the seed source to the correct location for the
 *                control status register.
 * 05/03/01 jke   fixed a bug with the allocation and free-ing of requests
 * 04/27/01 dkm   Fixed return bug in Queue_RN_request and changed
 *                seed use to External. 
 * 04/11/01 bac   Standardization changes to compile with changes to
 *                header files.  This file has not been brought up to
 *                standard otherwise.
 * 04/10/01 jke   Added request queue preliminary to making multithreaded.
 * 03/19/01 jke   Original version.
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */

#include "n8_pub_types.h"
#include "n8_common.h"
#include "RN_Queue.h"
#include "n8_enqueue_common.h"
#include "n8_rn_common.h"
#include "QMUtil.h"
#include "n8_semaphore.h"
#include "n8_malloc_common.h"
#include "n8_OS_intf.h"
#include "helper.h"
#include "QMQueue.h"
#include "n8_driver_api.h"
#include "n8_key_works.h" 


/* Globals */
N8_RNG_Parameter_t      RngParametersShadow;

N8_Status_t
RN_SetChipParameters(N8_RNG_Parameter_t *parms_p, QueueControl_t *queue_p);
/*****************************************************************************
 * function RNG_WaitForHardwareStart
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Waits a short bit for the hardware to generate some random numbers.
 *
 * Waits for random numbers to start.  Called after any enable.
 * The main reason for this wait is to prevent problems from back to back
 * set parameters, which shouldn't occur in a normal case anyhow.
 *
 * Note:
 *    Not done for host seed or external seed mode, as we don't want to
 *    loop forever if we don't have seed.  We don't provide random numbers
 *    if they aren't available anyhow.
 *
 * @param queue_p               RO: Pointer to the control structure
 *                                  for this queue.
 *
 * @par Externals:
 *    none.
 *
 * @return 
 *    None
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine requires the queueControlSem be already held.
 *
 * @par Assumptions:
 *    
 *****************************************************************************/

static void
RNG_WaitForHardwareStart(QueueControl_t *queue_p)
{

   uint32_t read;
   uint32_t write;
   uint32_t counter;

   /* Don't use the wait below for host seed, because it could be we */
   /* don't have a valid host seed.  The code elsewhere will not     */
   /* provide any random numbers if none are available.  The main    */
   /* reason for this wait is to prevent problems from back to back  */
   /* set parameters, which shouldn't occur in a normal case anyhow. */
   if (RngParametersShadow.seed_source != N8_RNG_SEED_INTERNAL)
   {
      DBG(("RNH_WaitForHardwareStart - seed not internal.\n"));
      return;
   }

   counter = 0;
   RNG_CMD_Q_GET_READ_WRITE_PTR(queue_p,&read,&write);
   while ( (read == write) && ( queue_p->hardwareFailed == 0 ) )
   {
      /* If we don't have random numbers in the queue, wait 1ms */
      /* This should be plenty of time to generate X9.17 samples */
      /* If after the START_COUNT is exceeded, the RNG may be */
      /* offline - Print a console Warning - This may NOT be a */
      /* fatal error - but it should be investigated. */

      n8_delay_ms(1); 
      RNG_CMD_Q_GET_READ_WRITE_PTR(queue_p,&read,&write);
      counter++;
      if (counter > RNH_BUSY_WAIT_HARDWARE_START_COUNT)
      {
         N8_PRINT("NSP2000: Warning: RNG_WaitForHardwareStart > 100ms \n");
         counter = 0;
         /* Let's not wait forever for the RNG hardware.             */
         queue_p->hardwareFailed++;
         break;
      }
   }

} /* RNG_WaitForHardwareStart */



/*****************************************************************************
 * function getContentsOfRNGQ
 *****************************************************************************/
/** @ingroup QMgr
 *
 * @brief reads data from the queue of random bytes maintained by NSP.
 *
 * @param queue_p               RO: Pointer to the control structure
 *                                  for this queue.
 * @param numBytesRequested     RO: The number of bytes being requested
 *                                  to be read from the RNG queue.
 * @param buf_p                 RO: A pointer into where we are copying the
 *                                  elements read.
 * @param numBytesReturned_p    WO: The number of bytes being returned
 *                                  (actually read from the queue.)
 *
 * @par Externals:
 *    none.
 *
 * @return 
 *    The number bytes elements actually read from the queue in
 *    numBytesReturned_p, and see Errors.
 *
 * @par Errors:
 *    enum-ed error types.
 *    N8_STATUS_OK                  Bytes are being returned.
 *    N8_RNG_QUEUE_EMPTY            No bytes returned, queue is empty.
 *
 * @par Locks:
 *    This routine requires the caller to obtain the queueControlSem.
 *
 * @par Assumptions:
 *****************************************************************************/

static N8_Status_t
getContentsOfRNGQ(QueueControl_t *queue_p,
                  int             numBytesRequested, 
                  unsigned char  *buf_p,
                  uint32_t       *numBytesReturned_p,
                  int             userReq)
{
   int               numFromReadIndex;
   int               numFromHead;
   int               numElementsInQ;
   int               numBytesInQ;
   int               samplesRead;
   N8_Status_t       ret = N8_STATUS_OK;
   const int         sampleSize = sizeof( RNG_Sample_t );

   /* Calculate the number of bytes available in the queue given the  */
   /* number of samples available in the queue using our soft copy of */
   /* the queue pointers.                                             */
   numElementsInQ = (queue_p->writeIndex - queue_p->readIndex) & queue_p->sizeMask;
   numBytesInQ = numElementsInQ * sampleSize;


   /* are there enough elements in the queue */
   if (numBytesInQ < numBytesRequested)      
   {
      /* Our soft copies of the read and write pointers indicate there are  */
      /* not enough bytes. Read the queue pointers from the NSP2000, update */
      /* our soft copy of the pointers, and retry.                          */
      RNG_CMD_Q_GET_READ_WRITE_PTR(queue_p,&queue_p->readIndex,&queue_p->writeIndex);

      numElementsInQ = (queue_p->writeIndex - queue_p->readIndex) & queue_p->sizeMask;

      /* Calculate the number of bytes available in the queue given the    */
      /* number of samples available in the queue.                         */
      numBytesInQ = numElementsInQ * sampleSize;


      /* are there enough elements in the queue */
      if (numBytesInQ < numBytesRequested)      
      {
         /* if not, return the error */
	 queue_p->stats.hardwareErrorCount++;
         return (N8_RNG_QUEUE_EMPTY);
      }
   }

   /* tell calling fcn how many elements were copied */
   *numBytesReturned_p = numBytesRequested;

   /* are there enough bytes before queue wrap? */
   /* note:  this calc works even if the write_index */
   /* is between the read_index and the end of queue */
   /* because the *numBytesReturned_p has already been */
   /* size limited to the min of numElementsInQ and */
   /* numBytesRequested */
   numFromReadIndex = (queue_p->sizeOfQueue - queue_p->readIndex) * sampleSize; 

   if (numFromReadIndex >= *numBytesReturned_p)
   {                                           
      /* this next calc obtains the num elements returned */
      /* from between the read_index and the eoq */
      numFromReadIndex = *numBytesReturned_p;

      /* return only elements from tail of queue */
      numFromHead = 0;  
   }
   else                                        
   {
      /* else return some elements from head */
      numFromHead = *numBytesReturned_p - numFromReadIndex;
   }

   if (numFromHead != 0)
   {
      /* copy source will be the head of the Q */
      /* copy dest will be target bfr plus elements from Q tail */

      /* Copy into temporary buffer at the end of the        */
      /* request.  The callback routine will copy the bytes  */
      /* back out to the proper place.                       */
      if (userReq == N8_TRUE)
      {
         N8_TO_USER((char *)buf_p + numFromReadIndex,
                  (char *)queue_p->cmdQueVirtPtr,
                  numFromHead);
      }
      else
      {
         memcpy((char *)buf_p + numFromReadIndex,
                  (char *)queue_p->cmdQueVirtPtr,
                  numFromHead);
      }
   }

   /* copy source elements from read_index to tail of queue */
   /* copy dest the beginning of the target bfr */
   if (userReq == N8_TRUE)
   {
      N8_TO_USER((char*)buf_p,
               (char*)(queue_p->cmdQueVirtPtr + queue_p->readIndex * sampleSize), 
               numFromReadIndex);
   }
   else
   {
      memcpy((char*)buf_p,
               (char*)(queue_p->cmdQueVirtPtr + queue_p->readIndex * sampleSize), 
               numFromReadIndex);
   }


   /* Increment the read queue pointer by the number of samples read */
   samplesRead = (*numBytesReturned_p/sampleSize);
   if ( ( *numBytesReturned_p % sampleSize ) != 0 )
   {
      ++samplesRead;
   }
   queue_p->readIndex = (queue_p->readIndex + samplesRead) & (queue_p->sizeMask);

   /* Determine if the read pointer should be written to the NSP2000 */
   numElementsInQ -= samplesRead;

   /* If we have read half the number of samples out of the queue, */
   /* update the read pointer.                                     */
   if (numElementsInQ < (queue_p->sizeOfQueue>>1))
   {
      queue_p->rngReg_p->rnh_q_ptr = queue_p->readIndex;
   }

   return ret;

} /* getContentsOfRNGQ */


/*****************************************************************************
 * function Queue_RN_request
 *****************************************************************************/
/** @ingroup QMgr
 * @brief add new request for entropy to linked-list of such requests.
 *
 * add new request for entropy to linked-list of such requests.
 *                                              
 * @param rn_req_p                RW: request to be enqueued.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    returns an enum-ed error type
 *    return RNG_REQUEST_ERROR          There was a problem with the request
 *    values from RNG_GetRandomBytes
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine gets the queueControlSem.
 *
 * @par Assumptions:
 *   the structure handed in by the pointer argument must not be
 *   freed by the calling function until after the request has been
 *   satisfied.  A request has been satisfied only after a RN_CheckQueue
 *   says that it is finished - (N8_QUEUE_REQUEST_FINISHED.)
 *
 *   It is assumed that the user is not modifying requests that are on the
 *   queue.
 *    
 *****************************************************************************/

N8_Status_t
Queue_RN_request(RN_Request_t *rn_req_p )
{
   N8_Status_t       ret = N8_STATUS_OK;
   QueueControl_t   *queue_p;
   uint32_t          numBytesReturned;
   uint32_t          rng_control_status;  
   n8_timeval_t      n8currenttime;

   /* Verify that the rn_req_p is valid.                                */
   if ((rn_req_p == NULL) || (rn_req_p->userBuffer_p == NULL))
   {
      return N8_INVALID_OBJECT;
   }

   /* Always use chip 0 */
   queue_p = &(queueTable_g.controlSets_p[N8_RNG_UNIT][N8_RNG]);          

   /* Grab the queueControlSem to ensure exclusive access */
   N8_AtomicLock(queue_p->queueControlSem);

   /* Make sure queue initialization has already been done.  If it has  */
   /* not been done, then do it.                                        */
   if (queue_p->rngInitialized != N8_TRUE)
   {
      N8_RNG_Parameter_t n8_rng_param;                 /* random number N */

      /* The RNG has not been called before so we must properly initialize
       * the RNG core and X9.17 seed expander. First we will use the 
       * internal seed generator to generate random seeds to be used as 
       * DES keys for the X9.17 PRBS. The RNG will be reset from seed
       * mode to X9.17 mode once the keys have been generated and the
       * RNH will be enabled to automatically transfer X9.17 RNG output
       * to the RNG output queue.
       * Note that the host system must have asserted PCI Reset 5 seconds
       * before this code runs for the LFSRs to have the appropriate 
       * level of uncertainty.
       */


#ifdef N8_RNG_TEST_MODE
      /* The N8_RNG_TEST_MODE code was the default init prior to 2.3 */
      /* Normally want this running. */
      n8_rng_param.todEnable = N8_TRUE;
      /* Use internal clock */
      n8_rng_param.use_external_clock = N8_FALSE; 
      /* Use internal seed */
      n8_rng_param.seed_source = N8_RNG_SEED_INTERNAL;
      /* Number or randoms before reseed, 255 max */
      n8_rng_param.iteration_count = 255;
      /* Clock frequency of N8 HW */
      n8_rng_param.TOD_prescale = 0x0bebc200;
      n8_rng_param.set_TOD_counter = N8_TRUE;

      /* TODO: Need to randomize this a bit with a call to get time  */
      /* of day or something like that.  n8_gettime                  */
      n8_rng_param.key1[0] = 0x2a; n8_rng_param.key1[1] = 0xd3;
      n8_rng_param.key1[2] = 0x38; n8_rng_param.key1[3] = 0x4f;
      n8_rng_param.key1[4] = 0x2c; n8_rng_param.key1[5] = 0xda;
      n8_rng_param.key1[6] = 0x67; n8_rng_param.key1[7] = 0xab;

      n8_rng_param.key2[0] = 0xf2; n8_rng_param.key2[1] = 0x15;
      n8_rng_param.key2[2] = 0x7a; n8_rng_param.key2[3] = 0x97;
      n8_rng_param.key2[4] = 0x54; n8_rng_param.key2[5] = 0x8c;
      n8_rng_param.key2[6] = 0x46; n8_rng_param.key2[7] = 0xc7;

      n8_rng_param.hostSeed[0] = 0x34; n8_rng_param.hostSeed[1] = 0x56;
      n8_rng_param.hostSeed[2] = 0xab; n8_rng_param.hostSeed[3] = 0xcd;
      n8_rng_param.hostSeed[4] = 0x09; n8_rng_param.hostSeed[5] = 0x87;
      n8_rng_param.hostSeed[6] = 0xf2; n8_rng_param.hostSeed[7] = 0x5a;

      n8_rng_param.initial_TOD_seconds = 0x001029c5;
      n8_rng_param.externalClockScaler = 0; /* Ignored from values above */

      /* Perform initial set-up for RNG */
      ret = RN_SetChipParameters(&n8_rng_param, queue_p);
      /* TODO: Ultimately we should check the return status.         */
      /* The check of return status is more critical once we put in  */
      /* code above to randomize the keys, etc. above.               */
#else
      /* Enable the RNG to get SEEDs for DES keys - we need 2 64 bit 
	 samples to use as the 64bit DES keys.
       */

  do 
   {
      /* clear any errors and set up to enable the RNG */     
      rng_control_status = RNG_Status_RNG_Enable |                
	 RNG_Status_Buffer_Use_Seed_Generator |
         RNG_Status_Seed_Use_Internal |                                        
         RNG_Status_Any_Condition_Mask |                          
         (RNG_Status_Iteration_Count_Mask &                       
          255);                              

      /* Start generating Internally Seeded Samples to the RNG Buffer */
      queue_p->rngReg_p->rng_control_status = rng_control_status;

      /* spin while waiting for for RNG SEEDs to be created */
      n8_delay_ms(30); /* Wait 30 milliseconds for seed data to be valid */

      /* Stop the RNG Core */
      queue_p->rngReg_p->rng_control_status = 0;

	{
	/* Extract the seeds from the RNG buffer and copy them 
	 * into the n8_rng_param structure. 
         */
	unsigned char key[16]; 
	int i;
	for ( i = 0; i < 4; i++ )   
 	   {                                           
             key[i*4]   = ((volatile char *)&queue_p->rngReg_p->rng_buffer[i])[0];
             key[i*4+1] = ((volatile char *)&queue_p->rngReg_p->rng_buffer[i])[1];
             key[i*4+2] = ((volatile char *)&queue_p->rngReg_p->rng_buffer[i])[2];
             key[i*4+3] = ((volatile char *)&queue_p->rngReg_p->rng_buffer[i])[3];
	   }
	for ( i = 0; i < N8_DES_KEY_LENGTH ; i++) {
		 n8_rng_param.key1[i] = key[i];      
		 n8_rng_param.key2[i] = key[i+8];    
		}
 	}

      /* check key1 and key2 for parity and force parity if needed*/    
      if (checkKeyParity(&n8_rng_param.key1) == N8_FALSE)                            
      {                                                                 
         forceParity(&n8_rng_param.key1);                                            
      }
                                                                        
      if(checkKeyParity(&n8_rng_param.key2) == N8_FALSE)                             
      {                                                                 
         forceParity(&n8_rng_param.key2);                                            
      }                                                                     
                                                                            
      /* check key1 and key2 for weakness */                                
      if (checkKeyForWeakness(&n8_rng_param.key1) == N8_TRUE ||                          
          checkKeyForWeakness(&n8_rng_param.key2) == N8_TRUE)                            
      {                                                                     
         DBG(("Weak key\nRNG Startup INIT - return Error\n"));           
         ret = N8_WEAK_KEY;                                                 
         /* get another seed to generate another key ! */
      }                                                                     
    /* If the seed passed all the tests - break out of the do/while loop */
    if (ret != N8_WEAK_KEY)  {
	break;
	}
  } while (TRUE);

      /* Make sure Time of Day counter is running */
      n8_rng_param.todEnable = N8_TRUE;
      /* Use internal clock */
      n8_rng_param.use_external_clock = N8_FALSE; 
      /* Use internal seed */
      n8_rng_param.seed_source = N8_RNG_SEED_INTERNAL;
      /* Number or randoms before reseed, 255 max */
      n8_rng_param.iteration_count = 255;
      /* Clock frequency of N8 HW - Set here to ~167Mhz */
      n8_rng_param.TOD_prescale = 0x09f437C0;
      n8_rng_param.set_TOD_counter = N8_TRUE;

      /* Get the time of day */
      n8_gettime(&n8currenttime); 
      n8_rng_param.initial_TOD_seconds = n8currenttime.tv_sec;
      n8_rng_param.externalClockScaler = 0; /* Ignored from values above */

      /* Perform initial set-up for RNG */
      ret = RN_SetChipParameters(&n8_rng_param, queue_p);


#endif
   }


   /* Copy the bytes out of the RNG queue */
   ret = getContentsOfRNGQ( queue_p,
                            rn_req_p->numBytesRequested,
                            rn_req_p->userBuffer_p,
                            &numBytesReturned,
                            rn_req_p->userRequest );

   /* Increment our count of requests queued. */
   queue_p->stats.requestsQueued++;

   N8_AtomicUnlock(queue_p->queueControlSem);

   return ret;

} /* Queue_RN_request */


/*****************************************************************************
 * function RN_GetParameters
 *****************************************************************************/
/** @ingroup QMgr
 * @brief Fill in values in a N8_RNG_Parameter_t structure that require
 *        looking at hardware.
 *
 * A note on the strategy for RNG_ALL_UNITS:
 *    The use of the RNG core has been simplified to use only one RNG 
 *    execution unit. The obsoletes the system level updateAllSem lock.
 *    All references to RNG_ALL_UNITS have been modified to simply use
 *    the first execution unit.
 *
 * @param parms_p       WO: a pointer to a parameter-holding structure 
 * @param chip          RO: The chipset which is being read.
 *
 * @par Externals:
 *    queueTable_g      RO: The pointer to the global control set table.
 *
 * @return 
 *    enum-ed error types.
 *    N8_STATUS_OK                      Success
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine requires no locks.
 *
 * @par Assumptions:
 *    Only the RN execution unit for chip 0 is used. Given the implementation
 *    of the RNG, using more than one just complicates the implementation and
 *    requires additional resources with no performance gain.
 *    
  *****************************************************************************/

N8_Status_t
RN_GetParameters(N8_RNG_Parameter_t *parms_p, int chip)
{
   QueueControl_t              *queue_p;
   N8_Status_t                  ret = N8_STATUS_OK;

   if (parms_p == NULL )
   {
      return(N8_INVALID_OBJECT);
   }

   ret = QMgr_get_control_struct(&queue_p, N8_RNG, chip);
   if (ret != N8_STATUS_OK)
   {
      return(ret);
   }

   /* Make sure the hardware has been initialized before we can return  */
   /* initialization parameters.                                        */
   if ( queue_p->rngInitialized != N8_TRUE )
   {
      ret = N8_NOT_INITIALIZED;
   }
   else 
   {
      /* Copy in the current parameters.                                */
      memcpy(parms_p, &RngParametersShadow, sizeof(N8_RNG_Parameter_t));
   }

   return ret;

} /* RN_GetParameters */


/*****************************************************************************
 * function RN_SetChipParameters
 *****************************************************************************/
/** @ingroup QMgr
 * @brief initialize the RN hardware
 *
 * This is called only when we want to actually initialize hardware.
 *
 *
 * IMPORTANT note on register access:
 *    The north bridge is optimizing consecutive 32-bit writes into burst
 *    transfers.  SIMON handles the 32-bit bursts correctly but has problems
 *    with the 64-bit bursts.  Therefore we do NOT want to do any consecutive
 *    32-bit writes.  See the structure N8RNGRegs_t for order of the registers.
 *
 * @param parms_p       RO: a pointer to a parameter-holding structure <BR>
 * @param queue_p       RO: Pointer to the queue control structure
 *                          associated with hardware we are configuring.
 *
 * @return 
 *    enum-ed error types.
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine requires that the caller has obtained the queueControlSem.
 *
  *****************************************************************************/

N8_Status_t
RN_SetChipParameters(N8_RNG_Parameter_t *parms_p, QueueControl_t *queue_p)
{
   N8_Status_t                  ret = N8_STATUS_OK;
   uint32_t                     rng_control_status;
   uint32_t                     key1_ms;
   uint32_t                     key1_ls;
   uint32_t                     key2_ms;
   uint32_t                     key2_ls;
   uint32_t                     hostSeed_ms;
   uint32_t                     hostSeed_ls;
   uint32_t                     rng_seed_source;


   /* Only set the chip parameters if it has not already been initialized */
   if (queue_p->rngInitialized != N8_TRUE)
   {

      /* Set up the keys and seed and time of day */
      key1_ms = BE_to_uint32(&parms_p->key1[N8_MS_BYTE]);
      key1_ls = BE_to_uint32(&parms_p->key1[N8_LS_BYTE]);
      key2_ms = BE_to_uint32(&parms_p->key2[N8_MS_BYTE]);
      key2_ls = BE_to_uint32(&parms_p->key2[N8_LS_BYTE]);

      queue_p->rngReg_p->rng_key1_msw = key1_ms;
      queue_p->rngReg_p->rng_key2_msw = key2_ms;

      queue_p->rngReg_p->rng_tod_prescale = parms_p->TOD_prescale;

      queue_p->rngReg_p->rng_key2_lsw = key2_ls;
      queue_p->rngReg_p->rng_key1_lsw = key1_ls;

      /* Set up the time of day */
      if (parms_p->set_TOD_counter == N8_TRUE)
      {
         queue_p->rngReg_p->rng_tod_seconds = parms_p->initial_TOD_seconds;
      }
      DBG(("rnh config: %x\n", queue_p->rngReg_p->rnh_control_status));

      /*
       * Set seed source values.
       * These values are keyed to the expected values set by the hardware
       * spec.  See [RNG] Control Status Register for the Seed Source
       * specification. 
       */
      switch (parms_p->seed_source)
      {
         case N8_RNG_SEED_EXTERNAL:
            rng_seed_source = RNG_Status_Seed_Use_External;
            break;

         case N8_RNG_SEED_HOST:
            rng_seed_source = RNG_Status_Seed_Use_Host3;
            break;
   
         case N8_RNG_SEED_INTERNAL:
         default:
            rng_seed_source = RNG_Status_Seed_Use_Internal;
            break;
      }
   
      /* clear any errors and set up to enable the RNH/RNG */
      rng_control_status = RNG_Status_RNG_Enable |
         RNG_Status_Buffer_Use_X917 |
         rng_seed_source |
         RNG_Status_Any_Condition_Mask |
         (RNG_Status_Iteration_Count_Mask & 
          parms_p->iteration_count);

      /* Set TOD enable if needed. */
      if ( parms_p->todEnable == N8_TRUE )
      {
         rng_control_status |= RNG_Status_TOD_Enable;
      }
      else
      {
         rng_control_status &= ~RNG_Status_TOD_Enable;
      }

      /* Set External clock and external clock scaler register if needed.  */
      if (parms_p->use_external_clock == N8_TRUE)
      {
         rng_control_status |= RNG_Status_Ext_Clock_Enable;
         queue_p->rngReg_p->rng_external_clock_scalar =
            parms_p->externalClockScaler;
      }
      else
      {
         rng_control_status &= ~RNG_Status_Ext_Clock_Enable;
      }

      hostSeed_ms = BE_to_uint32(&parms_p->hostSeed[N8_MS_BYTE]);
      hostSeed_ls = BE_to_uint32(&parms_p->hostSeed[N8_LS_BYTE]);

      queue_p->rngReg_p->rng_hostseed_msw = hostSeed_ms;
      queue_p->rngReg_p->rng_control_status = rng_control_status;
      queue_p->rngReg_p->rng_hostseed_lsw = hostSeed_ls;

      DBG(("rnh config: %x\n", queue_p->rngReg_p->rnh_control_status));

      queue_p->rngReg_p->rnh_control_status =
         RNH_Status_Transfer_Enable |
         RNH_Status_All_Enable_Mask;


      /* Save off the parameters.                                          */
      /* This assumes the parameters are the same on all RNG units.        */
      memcpy(&RngParametersShadow, parms_p, sizeof(N8_RNG_Parameter_t));

      /* Wait for a bit to let some random numbers be generated.           */
      RNG_WaitForHardwareStart(queue_p);

      /* Set that the queue has been initialized. */
      queue_p->rngInitialized = N8_TRUE;
   }

   return ret;

} /* RN_SetChipParameters */

/*****************************************************************************
 * function RN_SetParameters
 *****************************************************************************/
/** @ingroup QMgr
 * @brief initialize the RN Hardware
 *
 * This is called only when we want to actually initialize hardware.
 *
 * A note on the strategy for RNG_ALL_UNITS:
 *    The use of the RNG core has been simplified to use only one RNG 
 *    execution unit. The obsoletes the system level updateAllSem lock.
 *    All references to RNG_ALL_UNITS have been modified to simply use
 *    the first execution unit.
 *
 * @param parms_p       RO: a pointer to a parameter-holding structure <BR>
 * @param chip          RO: The chipset which is being initialized.
 *
 * @par Externals:
 *    queueTable_g      RO: The pointer to the global control set table.
 *
 * @return 
 *    enum-ed error types.
 *
 * @par Errors:
 *
 * @par Locks:
 *    This routine gets the queueControlSem for each execution unit as it
 *    set its individual parameters.
 *
 * @par Assumptions:
 *    Only the RN execution unit for chip 0 is used. Given the implementation
 *    of the RNG, using more than one just complicates the implementation and
 *    requires additional resources with no performance gain.
 *    
 *****************************************************************************/

N8_Status_t
RN_SetParameters(N8_RNG_Parameter_t *parms_p, int chip)
{
   QueueControl_t              *queue_p;
   N8_Status_t                  ret = N8_STATUS_OK;

   if (parms_p == NULL )
   {
      ret = N8_INVALID_OBJECT;
      goto RN_SetParametersReturn;
   }

   ret = QMgr_get_control_struct(&queue_p, N8_RNG, chip);
   if (ret != N8_STATUS_OK)
   {
      goto RN_SetParametersReturn;
   }

   /* Get the queue control semaphore to protect queue initialization   */
   N8_AtomicLock(queue_p->queueControlSem);

   ret = RN_SetChipParameters( parms_p, queue_p );

   /* Release aquired semaphore */
   N8_AtomicUnlock(queue_p->queueControlSem);

RN_SetParametersReturn:
   return ret;

} /* RN_SetParameters */

