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

static char const n8_id[] = "$Id: n8_rng.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_rng.c
 *  @brief Contains Random Number Generator interface functions
 *
 * Functions:
 *          N8_SetRNGParameters - Sets the operating parameters for the 
 *                                 Random Number Generator. 
 *          N8_GetRNGParameters - Gets the operating parameters for the 
 *                                 Random Number Generator. 
 *          N8_GetRandomBytes - Gets requested number of random bytes.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 07/28/03 brr   Removed obsolete #ifdefs. (Bug 918)
 * 05/19/03 brr   Clean up include files.
 * 05/15/03 brr   Eliminated obsolete n8_rng.h & updated constants.
 * 11/25/02 brr   Use new define N8_RNG_UNIT to set/get RNG parameters.
 * 05/15/02 brr   Rework RNG queue such that ioctl returns the bytes requested.
 * 04/04/02 msz   Fix for BUG 685 (INCONSISTANT not INVALID OBJECT)
 * 04/02/02 msz   Fix for BUG 503 (not catching weak key of all zeros - check
 *                key after setting parity.)
 * 03/26/02 msz   Fix for BUG 506 (external clock w/o seed source external)
 *                Fix for BUG 507 (check seed source)
 *                Fix for BUG 509 (null pointer on get random bytes)
 * 03/20/02 msz   Fix for BUG 508 (check for request of 0 bytes.)
 * 03/08/02 msz   Request Handler callback is optional.
 * 02/28/02 msz   Request Handler callback is back.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/18/02 msz   No longer need Request Handler callback.  No longer need
 *                references to the queue control structure.  No longer need
 *                current_RNGparms_gp.  No longer need initRNGParameters
 * 02/15/02 brr   Correct DBG print for requestStatus.
 * 02/06/02 bac   Added error checking to functions even in the temporary
 *                USE_OS_RNG path so that QA tests behave as expected.
 * 01/29/02 bac   Changed Get/Set parameters to do nothing if using
 *                OS rand function.
 * 01/29/02 bac   Changed N8_GetRandomBytes to use rand() instead of the
 *                behavioral model.  This is a temporary measure until the
 *                NSP2000 hardware fix is effected.
 * 11/13/01 brr   Removed references to shared_resource.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/07/01 mel   Fixed Bugs #187 , #188 , #189 - cosmetic changes
 * 10/05/01 msz   Added slightly more protection around current_RNGparms_gp.
 * 10/05/01 msz   Added support for mulitple RNG execution units.
 * 10/04/01 msz   Added internal initRNGParameters.
 * 10/02/01 msz   Continued merging, use n8_get_shared_resource
 * 09/25/01 msz   Removed bufferLen_log2 from N8_RNG_Parameter_t.
 *                Ask QMgr if hardware has been already initialized before
 *                initializing it.  Put current parameters in shared memory.
 * 09/20/01 bac   Changed Key_cblock to key_cblock_t to follow coding stds.
 * 09/17/01 bac   Removed occurence of buffer_source in a DBG statement.
 * 09/14/01 bac   Removed buffer_source from RNG parameters.  It will always be
 *                X917.  Also removed reference to enums for seed source and use
 *                the new bit-pattern macros instead.
 * 09/07/01 bac   Changed the return code to N8_INVALID_OBJECT if
 *                N8_SetRNGParameters includes a buffer size that is too large
 *                -- as listed in the API Programmer Reference.  (BUG #157)
 * 09/07/01 bac   Added a return code check after calling set parameters.
 *                (BUG #156). 
 * 09/11/01 msz   Adjusted N8_RNG_Parameter_t parameters
 * 09/04/01 bac   Fixed BUG #56 by editting comments.
 * 08/27/01 msz   Renamed fcnToCallbackWhenRequestIsFilled to callback
 * 08/08/01 msz   Use internal seed, external does not work on FPGA.
 * 08/08/01 msz   Include n8_semaphore.h directly.
 * 08/07/01 hml   Added Locks section to appropriate comment block.
 * 08/07/01 hml   Added protection of globals with a process level semaphore.
 * 08/03/01 msz   Added a test for request_p not zero before de-referencing
 *                it in N8_GetRandomBytes.
 * 08/02/01 bac   Changed the default to set seed source to internal.  Also
 *                fixed a bug when freeing a request.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/31/01 bac   Check to ensure a request to get random bytes is not for more
 *                than RNG_MAX_REQUEST as required by the specification.
 * 07/30/01 bac   Set the queue pointer in the request.
 * 07/12/01 bac   Deleted unused variables.
 * 06/28/01 bac   Changes to hook-up callback function and get event handling to
 *                work properly.
 * 06/25/01 bac   Substantial changes to support QMgr v 1.0.1 and new kernel
 *                memory management.  Added event to the GetRandomBytes call.
 * 06/20/01 mel   Corrected use of kernel memory.
 * 05/30/01 mel   Deleted checkKeyForWeakness and checkKeyParity.
 *                Added forcing parity on key.
 * 05/21/01 bac   Converted to use N8_ContextHandle_t.
 * 05/18/01 bac   Memory management macros.
 * 05/03/01 bac   Replaced integer use of NULL with 0.
 * 04/30/01 bac   Fixed problem with resultHandler to eliminate compiler
 *                warnings.  Warning that resultHandler is defined but not
 *                used still exists as this problem needs to be resolved.
 * 04/12/01 mel   Original version.
 ****************************************************************************/
/** @defgroup RNG Random Number Methods
 */
#include "n8_common.h"          /* common definitions */
#include "n8_pub_errors.h"      /* Errors definition */
#include "n8_pub_types.h"       /* Some type definitions. */
#include "n8_enqueue_common.h"  /* common definitions for enqueue */
#include "n8_util.h"            /* definitions for CHECK_RETURN and other macros */
#include "n8_key_works.h"       /* definitions for functions that work with key */
#include "n8_API_Initialize.h"
#include "n8_semaphore.h"
#include "n8_rand.h"

/*****************************************************************************
 * N8_SetRNGParameters
 *****************************************************************************/
/** @ingroup RNG
 * @brief Sets the operating parameters for Random Number Generator.
 *
 * Sets the operating parameters for the Random Number Generator to the
 * values contained in the N8_RNG_Parameter_t object. These parameters
 * include the base address of the host memory buffer into which the RNG
 * will write random values, the size of this buffer (in terms of entries,
 * where each entry is a 64-bit random value), the seed source to use, etc.
 * as defined in the RNG specification.  Once this call returns, all values
 * returned by N8_GetRandomBytes will be values generated using the parameters
 * specified in this call (up until a subsequent call to N8_SetRNGParameters).
 * A call to N8_SetRNGParameters must be made before N8_GetRandomBytes can
 * return any random values. 
 *
 * Currently this code will set parameters on all RNG units the same way.
 * Thus, when we go to set parameters, we will tell RN code to set parameters
 * on all units.
 *
 * @param p RO: Pointer to N8_RNG_Parameter_t
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *           N8_INVALID_OBJECT      -   one or more values specified in
 *                                      N8_RNG_Parameter_t are outside its
 *                                      permitted range <BR>
 *           N8_INCONSISTENT        -   the values specified in
 *                                      N8_RNG_Parameter_t are not
 *                                      consistent with one another <BR>
 *           N8_WEAK_KEY            -   key1 or key2 or both - weak key(s) <BR>
 *           N8_INVALID_ENUM        -   buffer/seed type is invalid <BR>
 *           N8_INVALID_INPUT_SIZE  -   invalid size of the kernel-space
 *                                      buffer <BR>
 *           N8_INVALID_KEY         -   key's parity check failed
 *
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *
 *****************************************************************************/

#define TOD_MIN                  100

N8_Status_t N8_SetRNGParameters(N8_RNG_Parameter_t *p)
{
   N8_Status_t          ret = N8_STATUS_OK;
   key_cblock_t         key1, key2;     /* Keys to be checked */
   uint32_t i;

   DBG(("N8_SetRNGParameters(N8_RNG_Parameter_t *p)\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* verify RNG parameter object */
      CHECK_OBJECT(p, ret);
    
      /*
       * 1. verify parameters for range and consistency
       */
    
    
      /* verify the consistency of the Time of the Day parameters */
      if ((p->set_TOD_counter == N8_TRUE) && (p->TOD_prescale < TOD_MIN))
      {
         DBG(("TOD_prescale is out of range: %d\n", p->TOD_prescale));
         DBG(("N8_SetRNGParameters - return Error\n"));
         ret = N8_INCONSISTENT;
         break;
      }
    
      /* build keys for weakness and parity verification */
      for ( i = 0; i < N8_DES_KEY_LENGTH; i++ )
      {
         key1[i] = p->key1[i];   
         key2[i] = p->key2[i];   
      }


      /* check key1 and key2 for parity and force parity if needed*/
      if (checkKeyParity(&key1) == N8_FALSE)
      {
         forceParity(&key1);
         for ( i = 0; i < N8_DES_KEY_LENGTH; i++ )
         {
            p->key1[i] = key1[i];
         }
      }

      if(checkKeyParity(&key2) == N8_FALSE)
      {
         forceParity(&key2);
         for ( i = 0; i < N8_DES_KEY_LENGTH; i++ )
         {
            p->key2[i] = key2[i];
         }
      }
    
      /* check key1 and key2 for weakness */
      if (checkKeyForWeakness(&key1) == N8_TRUE ||
          checkKeyForWeakness(&key2) == N8_TRUE)
      {
         DBG(("Weak key\nN8_SetRNGParameters - return Error\n"));
         ret = N8_WEAK_KEY;
         break;
      }


      CHECK_RETURN(ret);

      /* verify iteration_count. */
      if ((p->iteration_count < 1) || (p->iteration_count > 256))
      {
         DBG(("Iteration count is out of range\n"));
         DBG(("N8_SetRNGParameters - return Error\n"));
         /* iteration count is out of range */
         ret = N8_INVALID_OBJECT;
         break;
      }


      /* Check seed souce */
      if ( (p->seed_source != N8_RNG_SEED_INTERNAL) /* &&
           (p->seed_source != N8_RNG_SEED_EXTERNAL) && 
           (p->seed_source != N8_RNG_SEED_HOST) */ )
      {
         DBG(("Invalid seed source\n"));
         DBG(("N8_SetRNGParameters - return Error\n"));
         ret = N8_INVALID_VALUE;
         break;
      }

      /* Check for use of external clock without seed being seed external */
      if ((p->use_external_clock == N8_TRUE) &&
          (p->seed_source != N8_RNG_SEED_EXTERNAL))
      {
         DBG(("External clock set without seed source external\n"));
         DBG(("N8_SetRNGParameters - return Error\n"));
         ret = N8_INCONSISTENT;
         break;
      }


      /*
       * 2. set parameters in queue manager
       */

      /* Initialize parameters in queue manager.  All RNG units are     */
      /* initialized the same way.                                      */
      /* if queue was not able to set parameters return invalid object error */
      ret = RN_SetParameters(p, N8_RNG_UNIT);
      if (ret != N8_STATUS_OK)
      {
         DBG(("Queue was not able to set parameters\n"));
         DBG(("N8_SetRNGParameters - return Error\n"));
         N8_PRINT("N8_SetRNGParameters - return %d\n", ret);
         ret = N8_INVALID_OBJECT;
         break;
      }

      
      DBG(("N8_SetRNGParameters - FINISHED\n"));

   } while (FALSE);


   return ret;
} /* N8_SetRNGParameters */

/*****************************************************************************
 * N8_GetRNGParameters
 *****************************************************************************/
/** @ingroup RNG
 * @brief Gets the current operating parameters for RNG.
 *
 * Gets the current operating parameters for the Random Number Generator and 
 * returns their values in the N8_RNG_Parameter_t object. These parameters 
 * include the size of this buffer (in terms of entries, where each entry is a
 * 64-bit random value), the seed source to use, etc. as defined in the RNG 
 * specification. This call has no effect on the operation of the RNG. A call 
 * to N8_GetRNGParameters made before any call to N8_SetRNGParameters will 
 * return whatever values are in the RNG's registers at the time of the call.
 *
 * We currently set all RNG units up the same way.  So for the most part
 * all parameters are the same on each unit.  However, when we look at
 * hardware specific parameters, we need to tell RN code to get parameters
 * from all units.  The RN code will handle any specific manipulation of
 * the individual units to present one overall view.
 *
 * @param p WO: Pointer to N8_RNG_Parameter_t
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *           N8_INVALID_OBJECT  -   Pointer to N8_RNG_Parameter_t is NULL<BR>
 *           N8_NOT_INITIALIZED -   RNG wasn't initialized<BR>
 *
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    Assumes hardware is or will be initialized.
 *****************************************************************************/
N8_Status_t N8_GetRNGParameters(N8_RNG_Parameter_t *p)
{

   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   DBG(("N8_GetRNGParameters(N8_RNG_Parameter_t *p)\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* verify RNG parameter object */
      CHECK_OBJECT(p, ret);

      /* QMgr keeps the copy of what the RNG Parameters look like.  It  */
      /* will fill in the parameters                                    */
      ret = RN_GetParameters(p, N8_RNG_UNIT);
      if (ret != N8_STATUS_OK)
      {
         DBG(("Queue was not able to get parameters\n"));
         DBG(("N8_GetRNGParameters - return Error\n"));
         ret = N8_INVALID_OBJECT;
         break;
      }
    
      /* Set set_TOD_counter to N8_FALSE, as that is what is claimed to  */
      /* be done in n8_rn_common.h's documentation.                     */
      p->set_TOD_counter = N8_FALSE;

      DBG(("todEnable = %s\n",
           (p->todEnable == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));
      DBG(("use_external_clock = %s\n",
           (p->use_external_clock == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));
      DBG(("seed_source = %d\n", p->seed_source));
      DBG(("iteration_count = %d\n", p->iteration_count));
      DBG(("TOD_prescale = %x\n", p->TOD_prescale));
      DBG(("set_TOD_counter = %s\n",
           (p->set_TOD_counter == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));

      /* Note - These DBG lines assume N8_DES_KEY_LENGTH is 8 */
      DBG(("key1 = %02x%02x%02x%02x%02x%02x%02x%02x\n",
           p->key1[0],p->key1[1],p->key1[2],p->key1[3],
           p->key1[4],p->key1[5],p->key1[6],p->key1[7]));
      DBG(("key1 = %02x%02x%02x%02x%02x%02x%02x%02x\n",
           p->key2[0],p->key2[1],p->key2[2],p->key2[3],
           p->key2[4],p->key2[5],p->key2[6],p->key2[7]));
      DBG(("hostSeed = %02x%02x%02x%02x%02x%02x%02x%02x\n",
           p->hostSeed[0],p->hostSeed[1],p->hostSeed[2],p->hostSeed[3],
           p->hostSeed[4],p->hostSeed[5],p->hostSeed[6],p->hostSeed[7]));

      DBG(("initial_TOD_seconds = %x\n", p->initial_TOD_seconds));
      DBG(("externalClockScaler = %x\n", p->externalClockScaler));
      DBG(("hostSeedValid = %s\n",
           (p->hostSeedValid == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));
      DBG(("seedErrorFlag = %s\n",
           (p->seedErrorFlag == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));
      DBG(("x9_17_errorFlag = %s\n",
           (p->x9_17_errorFlag == N8_TRUE) ? "N8_TRUE" : "N8_FALSE"));
      DBG(("seedValue_ms = %08x\n", p->seedValue_ms));
      DBG(("seedValue_ls = %08x\n", p->seedValue_ls));

      DBG(("N8_GetRNGParameters - FINISHED\n"));

   } while(FALSE);

   return ret;
} /* N8_GetRNGParameters */

/*****************************************************************************
 * N8_GetRandomBytes
 *****************************************************************************/
/** @ingroup RNG
 * @brief Gets random bytes generated by the RNG.
 *
 * Gets reqvested number of random bytes generated by the RNG and returns 
 * them in the buffer, which must be at least as big as number of random bytes
 * reqvested. Up to 8 K random bytes can be returned in a single call. A call 
 * to N8_SetRNGParameters must be made before N8_GetRandomBytes can return
 * any values.
 *
 * We currently pick a unit (next available) to get the parameters.
 * This is ok, as all RNG units are set up the same way.
 *
 * @param num_bytes      RO: The number of random bytes desired
 * @param buf            WO: The buffer for returned random bytes
 * @param event_p        RW:    On input, if null the call is synchronous 
 *                              and no event is returned. The operation 
 *                              is complete when the call returns. If 
 *                              non-null, then the call is asynchronous; 
 *                              an event is returned that can be used to 
 *                              determine when the operation completes.
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *    buf          - the returned random bytes<BR>
 *
 * @par Errors:
 *           N8_NOT_INITIALIZED    -    the RNG has not been initialized<BR>
 *           N8_INVALID_INPUT_SIZE -    num_bytes is less than 0
 *                                      or more than 8 KBytes<BR>
 *
 * @par Assumptions:
 *    Buffer is pre-allocated and is large enough.
 *****************************************************************************/

N8_Status_t N8_GetRandomBytes(int num_bytes, char *buf_p, N8_Event_t *event_p)
{
   RN_Request_t        rn_request;   /* RNG request structure */
   N8_Status_t          ret;
   DBG(("N8_GetRandomBytes\n"));

   do
   {
      /* check the buffer */
      CHECK_OBJECT(buf_p, ret);
      
      /* check the number of bytes requested
         it can't be less or equal to 0 or more than N8_RNG_MAX_REQUEST. */
      if ((num_bytes <= 0)  || (num_bytes > N8_RNG_MAX_REQUEST))
      {
         DBG(("Number of bytes requested is out of range : %d\n", num_bytes));
         DBG(("N8_GetRandomBytes - return Error\n"));
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
    
      ret = N8_preamble();
      CHECK_RETURN(ret);

      rn_request.userRequest  = N8_FALSE;
      rn_request.userBuffer_p = buf_p;
      rn_request.numBytesRequested = num_bytes;
                    
      /* we have a valid request.  queue it up. */
      ret = Queue_RN_request(&rn_request);
      CHECK_RETURN(ret);

      if (event_p != NULL)
      {
         N8_SET_EVENT_FINISHED(event_p, N8_RNG);
      }
   } while (FALSE);

   return ret; 

} /* N8_GetRandomBytes */


