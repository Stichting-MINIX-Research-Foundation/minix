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
 * @(#) n8_API_Initialize.c 1.2@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_API_Initialize.c
 *  @brief user space icing layer file.
 *
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/01/03 brr   In N8_TerminateAPI, only shutdown the callback if it has
 *                been initialized. (Bug 949)
 * 06/23/03 brr   Check & set API_init_g in N8_InitializeAPI to avoid
 *                duplicate initialization.
 * 06/06/03 brr   Fix Bug 928, Reset initialization parameters when NULL
 *                pointer is passed.
 * 03/11/03 brr   Modified N8_TerminateAPI to call N8_CloseDevice.
 * 02/25/03 brr   Added callback initialization & minor modifications to 
 *                support both user & driver init in the same file.
 * 10/22/02 brr   Added openMode parameter to N8_OpenDevice call.
 * 05/07/02 brr   Pass bncAddress to allocateBNCConstants.
 * 04/12/02 hml   N8_InitializeAPI now returns N8_HARDWARE_UNAVAILABLE
 *                if the call to open the device driver returns 
 *                N8_INVALID_OBJECT or the number of chips is 0.
 * 03/18/02 brr   Reinstate process_init_sem.
 * 02/25/02 brr   Retrieve driver info at initialization.
 * 02/21/02 brr   Perform BNC constant initialization once upon startup.
 * 01/10/01 hml   Include file changes and ifdef out of sem ops.
 * 11/30/01 hml   Original version
 ****************************************************************************/
/** @defgroup icing Icing Layer
 */

#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_pub_errors.h"
#include "n8_driver_api.h"
#include "nsp_ioctl.h"
#include "n8_semaphore.h"
#include "n8_cb_rsa.h"


/* The initialization global */
static N8_Boolean_t     API_init_g = N8_FALSE;
static N8_Boolean_t     deviceOpen_g = N8_FALSE;
static N8_ConfigAPI_t   apiConfig_g = {0, 0, 0};

NSPdriverInfo_t  nspDriverInfo;
/*****************************************************************************
 * N8_preamble
 *****************************************************************************/
/** @ingroup Init
 * @brief Performs initialization status check.
 * 
 *
 * @return
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      Errors returned by N8_Initialize()
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_preamble()
{
   N8_Status_t ret = N8_STATUS_OK;

   /* Simply check the initialization flag and return if the API has already */
   /* been initialized. This avoids the overhead of the semaphore once the   */
   /* API has been initialized.                                              */
   if (API_init_g == N8_FALSE)
   {
      ret = N8_InitializeAPI(NULL);
   }
   return ret;

} /* N8_preamble */

/*****************************************************************************
 * N8_InitializeAPI
 *****************************************************************************/
/** @ingroup Init
 * @brief Performs required hardware and software initialization of the Simon
 * hardware.
 *
 * This call must be made before any other API call is made. Until
 * N8_API_Initialize has been called successfully, all other API calls will
 * fail. This call checks that the hardware is present, addressable, and
 * functioning at a minimal level.  It establishes the queues in host memory
 * used to communicate with the Simon hardware and all other software
 * initialization.  Parameters may be null, if which case all of the built-in
 * system default configuration values and parameters are used.  If non-null,
 * Parameters specifies a file name of a file containing initialization and
 * configuration values used to override the default values.  Any parameter
 * not explicitly overriden in this file retains its default value.
 * 
 *
 *  @param parameters_p     RO:  If null, then no initialization/configuration
 *                               parameters are specified. In this case,
 *                               built in default values will be used. 
 *                               If non-null, this indicates a pointer to a
 *                               configuration structure containing
 *                               initialization/configuration parameters to be used.
 *
 * @return
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      N8_INVALID_OBJECT -     Parameters does not specify a valid pathname to a valid parameters file.. 
 *      N8_INVALID_VALUE -      One or more of the initialization parameters in the Parameters structure is invalid.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_InitializeAPI(N8_ConfigAPI_t *parameters_p)
{
   N8_Status_t    ret = N8_STATUS_OK;


   n8_acquire_process_init_sem();

   /* Check this flag again, after the process init semphore has been taken */
   /* to avoid a race condition with two concurrent initializers.           */
   if (API_init_g == N8_FALSE)
   {
      if (parameters_p)
      {
         if (parameters_p->structure_version != N8_INITIALIZATION_VERSION)
         {
            ret = N8_INVALID_VALUE;
         }
         else
         {
            apiConfig_g = *parameters_p;
         }
      } 
      else
      {
         memset(&apiConfig_g, 0, sizeof(N8_ConfigAPI_t));
      }
      if ((deviceOpen_g == N8_FALSE) && (ret == N8_STATUS_OK))
      {
         /* Open the driver */
         nspDriverInfo.numChips = 0;

         ret = N8_OpenDevice(&nspDriverInfo, N8_TRUE, N8_OPEN_APP);

         if ((ret == N8_INVALID_OBJECT) || (nspDriverInfo.numChips == 0))
         {
            ret = N8_HARDWARE_UNAVAILABLE;
         }
         else if (ret == N8_STATUS_OK)
         {
            ret = allocateBNCConstants(nspDriverInfo.bncAddress);
            if (ret == N8_STATUS_OK)
            {
               deviceOpen_g = N8_TRUE;
            }
#ifdef SUPPORT_CALLBACK_THREAD
            if (apiConfig_g.callbackEvents != 0)
            {
               callbackInit(apiConfig_g.callbackEvents, apiConfig_g.callbackTimeout);
            }
#endif
         }
      }

      /* Initialization has completed successfully */
      if (ret == N8_STATUS_OK)
      {
         API_init_g = N8_TRUE;
      }
   }

   n8_release_process_init_sem();
    
   return ret;
}


/***************************************************************************n
 * N8_TerminateAPI
 *****************************************************************************/
/** @ingroup Init
 * @brief Shutdowns and releases resources set up by N8_InitializeAPI.
 *
 * This call should be the last API call made. It closes the driver and performs
 * any necessary cleanup to ensure an orderly shutdown.
 *
 * @return
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_TerminateAPI(void)
{
   N8_Status_t ret = N8_STATUS_OK;

#ifdef SUPPORT_CALLBACK_THREAD
   if (apiConfig_g.callbackEvents != 0)
   {
     ret = callbackShutdown();
   }
#endif

   N8_CloseDevice();
   deviceOpen_g = N8_FALSE;
   API_init_g = N8_FALSE;

   return ret;
}

/***************************************************************************n
 * n8_getConfigInfo
 *****************************************************************************/
/** @ingroup Init
 * @brief Retrieves the data structure passed to N8_InitializeAPI.
 *
 * This function retrieves the parameters that were used to configure the API.
 *
 * @return
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t n8_getConfigInfo(N8_ConfigAPI_t *config_p)
{
   *config_p = apiConfig_g;
   return (N8_STATUS_OK);
}

