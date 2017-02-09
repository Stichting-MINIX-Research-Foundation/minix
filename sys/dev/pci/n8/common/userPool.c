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
 * @(#) userPool.c 1.10@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file userPool.c                                                         *
 *  @brief Memory Services - Cross-Platform                                  *
 *                                                                           *
 * This file implements the kernel onlu portions of the user pool allocation *
 * and management service for the NSP2000 devices.                           *
 *                                                                           *
 *                                                                           *
 * userPoolInit -                                                            *
 *      Obtains the large allocated space. It allocates the large pool       *
 *      with n8_GetLargeAllocation (platform-specific).                      *
 * userPoolRelease -                                                       *
 *      Purges the allocation map and deallocates the large allocation space.*
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/07/03 brr   Modified user pools to take advantage of the support for
 *                multiple memory banks.
 * 04/23/03 brr   Removed redundant parameter from n8_FreeLargeAllocation.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 07/15/02 brr   Do not take userPoolSem in userPoolAlloc if not initialzed.
 * 07/11/02 brr   Do not fail userPoolInit if banks is 0.
 * 07/02/02 brr   Added userPoolStats.
 * 06/26/02 brr   Remove bank parameter from calls to N8_PhysToVirt.
 * 06/24/02 brr   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver Memory Allocation Services - Cross-Platform.
 */


#include "n8_OS_intf.h"
#include "helper.h"
#include "n8_driver_parms.h"
#include "n8_malloc_common.h" 
#include "n8_memory.h" 
#include "userPool.h" 

static int                userPoolBanks = 0;
static ATOMICLOCK_t       userPoolSem;

static unsigned long      userPoolBase[DEF_USER_POOL_BANKS];
static n8_UserPoolState_t userPoolState[DEF_USER_POOL_BANKS];
static int                userPoolID[DEF_USER_POOL_BANKS];

/*****************************************************************************
 * userPoolDisplay
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Displays the current state of the user pool.
 *
 * This function displays the current state of the user pool.
 *                                                                        
 * @par Externals:
 *
 * @return
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void userPoolDisplay(void)
{
     int ctr;

     if (userPoolBanks == 0)
     {
        N8_PRINT( "userPool: <************* USER POOL UNINITIALIZED ***********>\n");
        return;
     }
 
     N8_PRINT( "userPool: <***************** USER POOL MAP *****************>\n");
 
     N8_PRINT( "userPool: banks = %d \n", userPoolBanks);

     for (ctr = 0; ctr < userPoolBanks; ctr++)
     {
        N8_PRINT( "userPool: <*************** USER POOL BANK #%d ***************>\n", ctr);
 
        N8_PRINT( "userPool: base address = %x, bank state =  %d, session ID = %d, \n",
                   (int)(userPoolBase[ctr]), 
                   userPoolState[ctr], userPoolID[ctr]);

        if (userPoolState[ctr] != POOL_FREE)
        {
           n8_memoryDisplay(N8_MEMBANK_USERPOOL + ctr);
        }
     }
 
     N8_PRINT( "userPool: <********* USER POOL ALLOCATION MAP END **********>\n");

}

/*****************************************************************************
 * userPoolCount
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Returns the number of allocated user pools.
 *
 * This function returns the number of allocated user pools.
 *                                                                        
 * @par Externals:
 *
 * @return
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int userPoolCount(void)
{
   return(userPoolBanks);
}

/*****************************************************************************
 * userPoolInit
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocate and setup management for user pool allocations.
 *
 * This routine should be called when a driver is loaded and initialized.
 *                                                                        
 * @param bank          RO:  The number of user pool banks to allocate
 * @param size          RO:  Specifies the desired allocation size(MB) per bank
 * @param granularity   RO:  The size of the smallest allocation.
 *
 * @par Externals:
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void userPoolInit(unsigned int  banks,
                  unsigned long size,
                  unsigned long granularity)
{
   int ctr;

   /* Initialize each bank */
   for (ctr = 0; ctr < banks; ctr++)
   {
      userPoolBase[ctr] = (unsigned long)n8_memoryInit(N8_MEMBANK_USERPOOL + ctr,
                                                       size * N8_ONE_MEGABYTE, 
                                                       granularity);
      if (!userPoolBase[ctr])
      {
         break;
      }
   }

   /* Setup user pool globals */
   userPoolBanks = ctr;
   N8_AtomicLockInit(userPoolSem);

}


/*****************************************************************************
 * userPoolRelease
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Release memory resources allocated for user pools.
 *
 * This routine deallocates and releases the large space allocated for the
 * user request pool.
 *
 *
 * @par Externals:
 *    N/A
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void userPoolRelease(void)
{
   int ctr;

   N8_AtomicLockDel(userPoolSem);
   for (ctr = 0; ctr < userPoolBanks; ctr++)
   {
      n8_memoryRelease(N8_MEMBANK_USERPOOL+ctr);
      userPoolBase[ctr] = 0;
   }
   userPoolBanks = 0;

   return;
}

/*****************************************************************************
 * userPoolAlloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocate a user pool for a process.
 *
 * This routine should be called when a process opens the device
 *                                                                        
 * @par Externals:
 *
 * @return
 *    NULL       pointer if process failed
 *    non-NULL   void physical pointer to the base of the user pool
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

unsigned long userPoolAlloc(int sessID)
{
   int   ctr;
   unsigned long addr = 0;
   MemBankCtl_t *memBankCtl_p;

   N8_AtomicLock(userPoolSem);

   for (ctr = 0; ctr < userPoolBanks; ctr++)
   {
      if (userPoolState[ctr] == POOL_FREE)
      {
         userPoolState[ctr] = POOL_ALLOCATED;
         userPoolID[ctr] = sessID;
         addr = userPoolBase[ctr];

	 /* Reset memory control variables for new allocation */
	 memBankCtl_p =  memBankCtl_gp[N8_MEMBANK_USERPOOL+ctr];
	 memBankCtl_p->maxAllocated = 0;
	 memBankCtl_p->failedAllocs = 0;
         break;
      }
   }
   N8_AtomicUnlock(userPoolSem);

   return (addr);
}


/*****************************************************************************
 * userPoolFree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Deallocate a user pool for a process.
 *
 * This routine should be called when a process exits
 *                                                                        
 * @param sessID        RO:  The session ID of the exiting process.
 * 
 * @par Externals:
 *
 * @return
 *
 * @par Errors:
 *    See return section for error information.
 *
 * @par Assumptions:
 *   It is assume the caller has disabled interrupts to ensure there is no
 *   conflict with the ISR accessing the currAllocated value.
 *****************************************************************************/

void userPoolFree(int sessID)
{
   int           ctr;
   MemBankCtl_t *memBankCtl_p;

   for (ctr = 0; ctr < userPoolBanks; ctr++)
   {
      if (userPoolID[ctr] == sessID)
      {

         /* If there is memory still allocated, the command blocks on the */
         /* queue may still be pointing to memory in this pool. Thus it   */
         /* cannot be marked as free until everything has been returned.  */
         n8_pfreeSessID(N8_MEMBANK_USERPOOL+ctr, sessID);

	 memBankCtl_p =  memBankCtl_gp[N8_MEMBANK_USERPOOL+ctr];
         if (n8_atomic_read(memBankCtl_p->curAllocated))
         {
            userPoolState[ctr] = POOL_DELETED;
         }
         else
         {
            userPoolState[ctr] = POOL_FREE;
         }
      }
   }

}


/*****************************************************************************
 * userPoolFreePool
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Mark a user pool as free
 *
 * This routine should be called after the last buffer has been 
 * freed from a pool
 *                                                                        
 * @param addr          RO:  The physical address of the user pool.
 * 
 * @par Externals:
 *
 * @return
 *
 * @par Errors:
 *    See return section for error information.
 *
 *****************************************************************************/

void userPoolFreePool(int bankIndex)
{

   /* Validate bank index */
   if ((bankIndex >= N8_MEMBANK_USERPOOL) &&
       (bankIndex <=N8_MEMBANK_USERPOOL+userPoolBanks))
   {
      /* Only mark the bank as free once all the buffers have been freed */
      if (n8_atomic_read(memBankCtl_gp[bankIndex]->curAllocated) == 0)
      {
         userPoolState[bankIndex - N8_MEMBANK_USERPOOL] = POOL_FREE;
      }
   }

}

/*****************************************************************************
 * userPoolStats
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Returns the current state of the user pool.
 *
 * This function return the current state of the user pool.
 *                                                                        
 * @par Externals:
 *
 * @return
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void userPoolStats(MemStats_t *memStatsPtr)
{
   int ctr;
   MemBankCtl_t *memBankCtl_p;

   for (ctr = 0; ctr < userPoolBanks; ctr++)
   {

      if (userPoolState[ctr] != POOL_FREE)
      {
         memBankCtl_p =  memBankCtl_gp[N8_MEMBANK_USERPOOL+ctr];
         memStatsPtr->sessionID    = userPoolID[ctr];
         memStatsPtr->curAllocated = n8_atomic_read(memBankCtl_p->curAllocated);
         memStatsPtr->maxAllocated = memBankCtl_p->maxAllocated;
         memStatsPtr->maxEntries   = memBankCtl_p->maxEntries;
         memStatsPtr->failedAllocs = memBankCtl_p->failedAllocs;

      } 
      else
      {
         memStatsPtr->sessionID = 0;
      }
      memStatsPtr++;
   }
 
}


