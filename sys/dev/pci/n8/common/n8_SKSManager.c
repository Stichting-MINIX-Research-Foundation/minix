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

static char const n8_id[] = "$Id: n8_SKSManager.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_SKSManager.c
 *  @brief NSP2000 SKS Manager
 *
 * This file is the portion of the SKS Management Interface that is always
 * kernel resident.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/21/04 jpw   Change N8_NO_64_BURST macro to come from nsp2000_regs.h
 * 01/15/04 bac   Bug #990:  Added N8_NO_64_BURST definition and use to break up
 *                writes to consecutive registers that are then optimized and
 *                confusing to the NSP2000.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 08/23/02 bac   Fixed incorrect DBG messages that were generating compiler
 *                warnings. 
 * 05/02/02 brr   Removed all references to queue structures.
 * 04/02/02 spm   Changed %i escape sequence to %d, because at least BSD
 *                kernel print doesn't understand %i.
 * 04/01/02 spm   Moved deletion of key handle files from n8_SKSResetUnit
 *                ioctl to N8_SKSReset API call.
 * 03/27/02 spm   Changed all N8_HARDWARE_ERROR returns to N8_INVALID_KEY
 *                (Bug 505) in n8_SKSWrite.  Fixed return values for
 *                n8_SKSResetUnit, so that N8_HARDWARE_ERROR is never used
 *                (Bug 646).
 * 03/20/02 bac   In n8_SKSWrite added a second delay loop to ensure the SKS
 *                Go/Busy bit is really low.  It has been observed to bounce
 *                once after initially going low, which can then cause an
 *                access error upon performing a write.
 * 03/14/02 bac   Fixed n8_SKSResetUnit and n8_SKSWrite.  A reset no longer
 *                calls write with a single word when trying to zero the entire
 *                SKS contents.  Write grabs the lock once and does all of the
 *                writing necessary.  If the SKS is busy, we wait a few times
 *                rather than just returning an error or waiting blindly whether
 *                it is busy or not.
 * 03/12/02 brr   Updated to use AtomicLocks.
 * 02/25/02 msz   File created by moving functions from n8_sks.c
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver Context Memory Manager.
 */

#include "helper.h"
#include "n8_driver_main.h"
#include "n8_enqueue_common.h"
#include "n8_sks.h"
#include "n8_daemon_sks.h"
#include "n8_sks_util.h"
#include "n8_SKSManager.h"
#include "nsp2000_regs.h"
#include "n8_time.h"

#define MAX_FAILURES 6


extern int NSPcount_g;
extern NspInstance_t NSPDeviceTable_g [];

/*****************************************************************************
 * n8_SKSWrite
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Write data to the SKS PROM.
 *
 * More detailed description of the function including any unusual algorithms
 * or suprising details.
 *
 * @param targetSKS     RO: A integer, the SKS PROM to write to.
 * @param data_p        RO: A uint32_t pointer to the data to write.  If data_p
 *                          is NULL, then the data_length of 0x0 will be written.
 * @param data_length   RO: A int, the data_p buffer length in 32 bit words.
 * @param offset        RO: A int, the SKS offset to begin the write.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the write(s) successfully completed.
 *    N8_UNEXPECTED_ERROR indicates an error writing to the SKS or that the
 *      API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the target SKS exists and that the queue control struct has a valid
 *    pointer to the target SKS registers.
 *****************************************************************************/

N8_Status_t n8_SKSWrite(const unsigned int targetSKS, 
                        const uint32_t *data_p, 
                        const int data_length,
                        const uint32_t offset_input,
                        const int fromUser)
{
   int i = 0;
   unsigned int failures;
   uint32_t word;
   N8_Status_t ret = N8_STATUS_OK;
   uint32_t offset = offset_input;
   uint32_t sks_status;

   NspInstance_t *NSPinstance_p;
   volatile NSP2000REGS_t *nsp;
 
   if ((targetSKS < 0) || (targetSKS >= NSPcount_g))
   {
      DBG(("Failed to get control structure: %d\n", ret));
      return N8_UNEXPECTED_ERROR;
   }

   /* assign the right control struct for the target HW */
   NSPinstance_p = &NSPDeviceTable_g[targetSKS];

/* Create a nsp pointer so the N8_NO_64_BURST macro will work */
/* N8_NO_64_BURST is used to interleave a dummy register access between
 * successive real 32 bit accesses that could be incorrectly "optimized" into a
 * 64 bit burst.
 */
   nsp = ((NSP2000REGS_t *)(NSPinstance_p->NSPregs_p));

   /* Entering critical section. */
   N8_AtomicLock(NSPinstance_p->SKSSem);

   for (i = 0; i < data_length; i++)
   {
      /* Get the data.  It either needs to be copied into kernel space  */
      /* or does not need the copy and can be read directly.            */
      if (data_p == NULL)
      {
         word = 0x0;
      }
      else if (fromUser == TRUE)
      {
         N8_FROM_USER(&word, &data_p[i], sizeof(word));
      }
      else
      {
         word = data_p[i];
      }

      /*
       * Cannot access data register while
       * PK_SKS_Go_Busy is on.
       */
      failures = 0;
      if (SKS_READ_CONTROL(NSPinstance_p) & PK_SKS_Go_Busy)
      {
         if (++failures > MAX_FAILURES)
         {
            DBG(("Multiple failures waiting for SKS busy.\n"));
            ret = N8_INVALID_KEY;
            goto n8_SKSWrite_0;
         }
         /* go to sleep briefly */
         n8_usleep(N8_MINIMUM_YIELD_USECS);
      }
      DBG(("Main wait for busy -- Iteration %d: Continuing "
           "after %d failures.\n", i, failures));

      /* This second wait block is here due to occasional spiking behavior in
       * the SKS busy bit.  It has been observed that the busy bit will go low,
       * and then briefly spike again before settling low.  This secondary wait
       * look will ensure a single spike is detected and avoided. */
      if (SKS_READ_CONTROL(NSPinstance_p) & PK_SKS_Go_Busy)
      {
         DBG(("Busy bit spike detected on iteration %d\n", i));
      }
      failures = 0;
      if (SKS_READ_CONTROL(NSPinstance_p) & PK_SKS_Go_Busy)
      {
         if (++failures > MAX_FAILURES)
         {
            DBG(("Multiple failures waiting for SKS busy.\n"));
            ret = N8_INVALID_KEY;
            goto n8_SKSWrite_0;
         }
         /* go to sleep briefly */
         n8_usleep(N8_MINIMUM_YIELD_USECS);
      }
      DBG(("2nd wait for busy -- Iteration %d: Continuing after %d failures.\n",
           i, failures)); 

      /* Clear any residual errors */
      SKS_WRITE_CONTROL(NSPinstance_p, PK_SKS_Access_Error | PK_SKS_PROM_Error);
      SKS_WRITE_DATA(NSPinstance_p, BE_to_uint32(&word));
      /* Perform a dummy operation to thwart optimization that would lead to a
       * 64-bit burst output which confuses the NSP2000.
       * DO NOT REMOVE THIS CALL WITHOUT UNDERSTANDING THE IMPLICATIONS.
       */
      N8_NO_64_BURST; 

      /* Enable the SKS write. */
      SKS_WRITE_CONTROL(NSPinstance_p, PK_SKS_Go_Busy | 
                        (offset++ & PK_Cmd_SKS_Offset_Mask));
      /* Check for errors. */
      sks_status = SKS_READ_CONTROL(NSPinstance_p);
      if ((sks_status & PK_SKS_Access_Error) |
          (sks_status & PK_SKS_PROM_Error))
      {
         DBG(("Error writing to SKS PROM. SKS Control Register = %08x\n", 
              sks_status));
         /* Clear the error */
         SKS_WRITE_CONTROL(NSPinstance_p,
                           PK_SKS_Access_Error | PK_SKS_PROM_Error);
         ret = N8_INVALID_KEY;
         goto n8_SKSWrite_0;
      }
   } /* for loop */

   /*
    * wait again so that no one tries to access 
    * SKS before it is completely written.
    */
      
   failures = 0;
   if (SKS_READ_CONTROL(NSPinstance_p) & PK_SKS_Go_Busy)
   {
      failures++;
      if (failures >= MAX_FAILURES)
      {
         ret = N8_INVALID_KEY;
         DBG(("Multiple failures waiting for SKS busy.\n"));
         goto n8_SKSWrite_0;
      }
      /* go to sleep briefly */
      n8_usleep(N8_MINIMUM_YIELD_USECS);
   }

n8_SKSWrite_0:   
   /* Leaving critical section. */
   N8_AtomicUnlock(NSPinstance_p->SKSSem);
   return ret;

} /* n8_SKSWrite */


/*****************************************************************************
 * n8_SKSResetUnit
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Perform SKS reset for a specific unit.
 *
 *  @param targetSKS           RO:  Unit number
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_STATUS_OK on success.<br>
 *    N8_FILE_ERROR if errors occur while reading/writing files or
 *                  directories.<br>
 *    
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/

N8_Status_t n8_SKSResetUnit(const N8_Unit_t targetSKS)
{
   int i;
   N8_Status_t ret = N8_STATUS_OK;
   NspInstance_t *NSPinstance_p;

   DBG(("Reset :\n"));

   /* Find out which, if any, key entries exist. Then blast 'em. */
   DBG(("Resetting SKS %d.\n", targetSKS));

 
   if ((targetSKS < 0) || (targetSKS >= NSPcount_g))
   {
      DBG(("Failed to get control structure: %d\n", ret));
      return N8_INVALID_VALUE;
   }

   /* assign the right control struct for the target HW */
   NSPinstance_p = &NSPDeviceTable_g[targetSKS];


#ifndef SKIP_SKS_ZERO   
   /* '0' out SKS. Wipe them out. All of them. Passing NULL as the data pointer
    * indicates to write 0x0 to entries. */
   ret = n8_SKSWrite(targetSKS, NULL, SKS_PROM_MAX_OFFSET, 0, FALSE);
   if (ret != N8_STATUS_OK)
   {
      DBG(("Error zeroing SKS in N8_SKSReset: %d\n", ret));
      return N8_INVALID_VALUE;
   }
#endif   
   /* Entering critical section. */
   N8_AtomicLock(NSPinstance_p->SKSSem);

   for (i=0; i < SKS_ALLOC_UNITS_PER_PROM; i++)
   {
      /* Clear the SKS descriptor table. */
      NSPinstance_p->SKS_map[i] = SKS_FREE;
   }

   /* Leaving critical section. */
   N8_AtomicUnlock(NSPinstance_p->SKSSem);

   return N8_STATUS_OK;
} /* n8_SKSResetUnit */


/*****************************************************************************
 * n8_SKSAllocate
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Allocate an entry for an SKS PROM.
 *
 * Attempts to find a best fit space in unallocated space, according to the
 * descriptor tables, for the given key handle.
 *
 * @param keyHandle_p       RW: A N8_SKSKeyHandle_t.
 *
 * @par Externals:
 *    None
 * @return 
 *    N8_STATUS_OK indicates the write(s) successfully completed.
 *    N8_UNEXPECTED_ERROR indicates an error allocating the key handle or that 
 *      the API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the key handle pointer is valid.
 *****************************************************************************/ 

N8_Status_t n8_SKSAllocate(N8_SKSKeyHandle_t* keyHandle_p)
{

   unsigned int i;
   unsigned int free_blocks_start_index = 0; 
   unsigned int free_blocks = 0;
   unsigned int best_fit_index = 0; 
   unsigned int best_fit_blocks = 0; 
   unsigned int SKS_words_needed = 0; 
   unsigned int SKS_allocation_units_needed = 0;
   NspInstance_t *NSPinstance_p;

   N8_Boolean_t sks_hole_found = N8_FALSE;

   uint32_t keyLength = keyHandle_p->key_length;
   N8_SKSKeyType_t keyType = keyHandle_p->key_type;
   int targetSKS = (int) keyHandle_p->unitID;
   N8_Status_t ret = N8_STATUS_OK;

   DBG(("N8_Allocate: \n"));

   if ((targetSKS < 0) || (targetSKS >= NSPcount_g))
   {
      DBG(("Invalid unit: %d\n", targetSKS));
      return N8_INVALID_VALUE;
   }
   /* assign the right control struct for the target HW */
   NSPinstance_p = &NSPDeviceTable_g[targetSKS];

   DBG(("SKS WORDS %d\n", SKS_RSA_DATA_LENGTH(keyLength)));

   if ((ret = n8_ComputeKeyLength(keyType, keyLength,
                                 &SKS_words_needed)) != N8_STATUS_OK)
   {
      DBG(("Could not compute SKS key length: %d\n", ret));
      return ret;
   }

   DBG((
      "Total of %d words needed in the SKS (%d) PROM for key length %d.\n", 
      SKS_words_needed, targetSKS, keyLength));

   SKS_allocation_units_needed =
      CEIL(SKS_words_needed, SKS_WORDS_PER_ALLOC_UNIT); 

   DBG((
      "Total of %d allocation units needed in the SKS (%d) PROM.\n",
      SKS_allocation_units_needed, targetSKS));
   DBG((
      "Looking for free blocks in descriptor %d.\n", targetSKS));

   best_fit_blocks = SKS_ALLOC_UNITS_PER_PROM;
   best_fit_index = 0;

   /* Entering critical section */
   N8_AtomicLock(NSPinstance_p->SKSSem);

   /* Find the best fit for this block of words. */
   sks_hole_found = N8_FALSE;
   i = SKS_PROM_MIN_OFFSET;
   while (i < SKS_ALLOC_UNITS_PER_PROM)
   {
      if (NSPinstance_p->SKS_map[i] == SKS_FREE)
      {
         DBG(("Found a free block at SKS allocation unit offset %d.\n", i));

         free_blocks_start_index = i;
         i++;
         while ((i < SKS_ALLOC_UNITS_PER_PROM) &&
                  ((NSPinstance_p->SKS_map[i]) == SKS_FREE))
         {
            i++;
         }

         free_blocks = i - free_blocks_start_index;

         DBG(("Number of free allocation blocks is %d.\n", free_blocks));

         /* If the number of free blocks to allocate is larger than the
            * needed number of blocks (in groups of SKS_WORDS_PER_ALLOC_UNIT)
            * then we can allocate this block.
            */

         if (free_blocks >= SKS_allocation_units_needed)
         {
            DBG(("Number of free blocks (%d) >= to needed blocks (%d).\n",
                      free_blocks, SKS_allocation_units_needed));

            sks_hole_found = N8_TRUE;

            /* See if this is the smallest fit. */
            if (free_blocks <= best_fit_blocks)
            {
               best_fit_index = free_blocks_start_index;
               best_fit_blocks = free_blocks;
            }
         }
         else
         {
            /* block is too small */
            DBG(("Number of free blocks (%d) < to needed blocks (%d).\n",
                      free_blocks, SKS_allocation_units_needed)); 
         }

      }
      i++;
   } /* while i < SKS_ALLOC_UNITS_PER_PROM */

   if (sks_hole_found == N8_TRUE)
   {
      DBG((
         "Allocating %d blocks out of %d free allocation blocks to key.\n",
         SKS_allocation_units_needed, free_blocks));

      /* Mark the blocks, in alloc unit sizes, as in use. */
      for (i = best_fit_index;
           i < best_fit_index + SKS_allocation_units_needed;
           i++)
      {
         DBG(("Allocating block %d.\n", i));
         NSPinstance_p->SKS_map[i] = SKS_INUSE;
      } /* for */

      /* Set the key offset. */
      keyHandle_p->sks_offset =
         best_fit_index * SKS_WORDS_PER_ALLOC_UNIT;

      DBG(("New key handle offset will be :%d\n", keyHandle_p->sks_offset));
   }
   else
   {
      /* No space found! */
      DBG((
         "Unable to find enough free space in SKS to allocate for key.\n"));
      ret = N8_NO_MORE_RESOURCE;
   }

   /* Leaving critical section. */
   N8_AtomicUnlock(NSPinstance_p->SKSSem);

   return ret;
} /* n8_SKSAllocate */


/*****************************************************************************
 * n8_SKSAllocate
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Set the status of an SKS entry.
 *
 * Sets status of SKS entry pointed to by key handle.
 * descriptor tables, for the given key handle.
 *
 * @param keyHandle_p       RW: A N8_SKSKeyHandle_t.
 * @param status            RO: The status value being set.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    N8_STATUS_OK indicates the write(s) successfully completed.
 *    N8_UNEXPECTED_ERROR indicates an error allocating the key handle or that 
 *      the API was not or could not be initialized.
 *
 * @par Assumptions:
 *    That the key handle pointer is valid.  And that the status is valid.
 *
 *****************************************************************************/ 

N8_Status_t n8_SKSsetStatus(N8_SKSKeyHandle_t *keyHandle_p,
                                   unsigned int       status)
{
   int i;
   unsigned int alloc_units_to_free = 0;
   unsigned int num_sks_words;
   unsigned int sks_alloc_unit_offset = 0;
   NspInstance_t *NSPinstance_p;
   N8_Status_t ret = N8_STATUS_OK;

   if ((keyHandle_p->unitID < 0) || (keyHandle_p->unitID >= NSPcount_g))
   {
      DBG(("Invalid unit: %d\n", keyHandle_p->unitID));
      return N8_INVALID_VALUE;
   }
   /* assign the right control struct for the target HW */
   NSPinstance_p = &NSPDeviceTable_g[keyHandle_p->unitID];

   ret = n8_ComputeKeyLength(keyHandle_p->key_type,
                             keyHandle_p->key_length,
                             &num_sks_words);
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }

   /* given the number SKS words, compute the number of allocation units */
   alloc_units_to_free = CEIL(num_sks_words, SKS_WORDS_PER_ALLOC_UNIT);

   /* given the offset in words, find the first allocation unit */
   sks_alloc_unit_offset = keyHandle_p->sks_offset / SKS_WORDS_PER_ALLOC_UNIT;
   if (ret != N8_STATUS_OK)
   {
      return ret;
   }

   N8_AtomicLock(NSPinstance_p->SKSSem);
   for (i = 0; i < alloc_units_to_free; i++)
   {
      NSPinstance_p->SKS_map[sks_alloc_unit_offset + i] = status;
   }
   N8_AtomicUnlock(NSPinstance_p->SKSSem);

   return ret;

} /* n8_SKSsetStatus */

