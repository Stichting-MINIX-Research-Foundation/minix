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

static char const n8_id[] = "$Id: contextMem.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file contextMem.c
 *  @brief NSP2000 Device Driver Context Memory Manager.
 *
 * This file manages the context memory resource for the NSP2000 device driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/16/04 jpw   Add N8_NO_64_BURST to prevent 64bit bursts to 32 bit regs
 *                during sequential slave io accesses.
 * 05/08/03 brr   Modified kernel functions to use N8_GET_KERNEL_ID instead
 *                of N8_GET_SESSION_ID.
 * 04/18/03 brr   Allocate Context memory map using vmalloc instead of
 *                requesting it from the continuous driver pool. (Bug 718)
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/26/02 brr   Remove bank parameter from calls to N8_PhysToVirt.                         
 * 06/12/02 hml   Calls to N8_PhysToVirt now take the bank parameter.
 * 06/10/02 hml   Pass memBankCtl as a parameter when needed.
 * 04/29/02 brr   Modified N8_ContextMemInit to correctly check allocation of 
 *                contextMemMap & also initialize the semaphore even when 
 *                there is no context memory installed.
 * 04/02/02 bac   Changed n8_contextalloc to look across all chips if
 *                N8_ANY_UNIT is specified and no more resources are
 *                encountered.  (BUG 515)
 * 03/29/02 hml   Added N8_ContextMemValidate. (BUGS 657 658).
 * 03/27/02 hml   Delete of unused variable.
 * 03/26/02 hml   Converted the context memory free functions to match the
 *                paradigm used by the memory implementation.  Also added
 *                status returns. (Bug 637).
 * 03/22/02 hml   Converted the context memory allocation functions to match
 *                the paradigm used by the memory implementation.
 * 03/19/02 brr   Correctly limit Allocation index to contextMemEntries.
 * 03/18/02 brr   Pass sessionID into allocation function.
 * 03/08/02 brr   Memset context memory map allocation to zero since KMALLOC
 *                is no longer doing it.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 02/25/02 brr   Removed references to qmgrData.
 * 02/18/02 brr   Support chip selection.
 * 02/15/02 brr   Integrate with VxWorks, added mutual exclusion semaphore.
 * 02/05/02 brr   File created.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver Context Memory Manager.
 */

#include "helper.h"
#include "contextMem.h"
#include "n8_pub_errors.h"
#include "n8_memory.h"
#include "n8_common.h"
#include "n8_driver_main.h"
#include "nsp2000_regs.h"
#include "QMUtil.h"
#include "n8_driver_api.h"

extern NspInstance_t NSPDeviceTable_g [DEF_MAX_SIMON_INSTANCES];
extern int NSPcount_g;

/*****************************************************************************
 * N8_ReadContextMemory
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Reads a value from a specified address in context memory.
 *
 * This routine reads a 32-bit value from the specified location
 * in context memory.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param address         RO: Specifies the context memory address.
 * @param value           RW: Returns the read value.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

static unsigned char N8_ReadContextMemory(NspInstance_t *NSPinstance_p,
                                    unsigned long  address,
                                    unsigned long *value )
{
     volatile NSP2000REGS_t *nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
     unsigned long           reg;

     /* FIRST CHECK FOR PENDING OPERATIONS */
     if (nsp->cch_context_addr & 0xc0000000)
     {
          return 0;
     }

     /* SPECIFY ADDRESS TO READ, ENABLE OPERATION, AND AWAIT COMPLETION */
     nsp->cch_context_addr = address | 0x80000000;
     reg = nsp->cch_context_addr;
     if (nsp->cch_context_addr & 0xc0000000)
     {
          /* ANY ACCESS SHOULD COMPLETE WITHIN THE SPACE OF 2 ACCESSES */
          return 0;
     }

     /* RETURN READ VALUE */
     *value = (unsigned long)(nsp->cch_context_data0);
     return 1;
} /* N8_ReadContextMemory */



/*****************************************************************************
 * N8_WriteContextMemory
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Writes a value to a specified address in context memory.
 *
 * This routine writes the specified 32-bit value to the specified location
 * in context memory.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param address         RO: Specifies the context memory address.
 * @param value           RO: Specifies the data to write.
 *
 * @return 
 *    0  Failure - the write failed.
 *    1  Success
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

static unsigned char N8_WriteContextMemory( NspInstance_t *NSPinstance_p,
                                            unsigned long  address,
                                            unsigned long  value )
{
     volatile NSP2000REGS_t *nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
     unsigned long           reg;

     /* FIRST CHECK FOR PENDING OPERATIONS */
     if (nsp->cch_context_addr & 0xc0000000)
     {
          return 0;
     }

     /* SPECIFY ADDRESS TO BE WRITTEN */
     nsp->cch_context_addr = address;

     /* SPECIFY VALUE TO WRITE, INITIATE WRITE OPERATION, AND AWAIT COMPLETION */
     nsp->cch_context_data1 = 0;
     N8_NO_64_BURST;
     nsp->cch_context_data0 = value;
     reg = nsp->cch_context_addr;
     if (nsp->cch_context_addr & 0xc0000000)
     {
        /* ANY ACCESS SHOULD COMPLETE WITHIN THE SPACE OF 2 ACCESSES */
        return 0;
     }
     return 1;
} /* N8_WriteContextMemory */



/*****************************************************************************
 * N8_GetContextMemSize
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Determine the size of context memory.
 *
 * This routine probes the context memory to determine how large it is.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 *
 * @return 
 *    Returns the amount of detected memory. If any accesses fail, we return
 *    0 bytes (no memory installed).
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

static unsigned long N8_GetContextMemSize(NspInstance_t *NSPinstance_p)
{
     unsigned long  tmp = 32, size = 0, readvalue1, readvalue2;

     /* FIRST TEST FOR THE EXISTENCE OF CONTEXT MEMORY */
     printf("N8_GetContextMemSize: testing for context memory\n");
     if ( !N8_WriteContextMemory(NSPinstance_p, 0,  0x12345678)  ||
          !N8_WriteContextMemory(NSPinstance_p, 10, 0x87654321)  ||
          !N8_ReadContextMemory (NSPinstance_p, 0,  &readvalue1) ||
          !N8_ReadContextMemory (NSPinstance_p, 10, &readvalue2) )
     {
	 printf("N8_GetContextMemSize: no context memory\n");
          return 0;
     }
     if ((0x12345678 != readvalue1) || (0x87654321 != readvalue2))
     {
          /* NO CONTEXT MEMORY APPEARS TO BE PRESENT               */
          /* CANNOT TEST THIS WITH FPGA, BECAUSE THE CTX REGISTERS */
          /* ARE NOT IMPLEMENTED, AND READS/WRITES WILL FAIL.      */
	  printf("N8_GetContextMemSize: no context memory again\n");
          return 0;
     }

     /* USING SIZING ALGORITHM AS IN NSP2000 DATASHEET P. 4-5 */
     printf("N8_GetContextMemSize: initializing\n");
     if ( !N8_WriteContextMemory(NSPinstance_p, 0,        64)   ||
          !N8_WriteContextMemory(NSPinstance_p, 0x400000, tmp)  ||
          !N8_ReadContextMemory (NSPinstance_p, 0,        &tmp) ||
          !N8_WriteContextMemory(NSPinstance_p, 0,        128)  ||
          !N8_WriteContextMemory(NSPinstance_p, 0x800000, tmp)  ||
          !N8_ReadContextMemory (NSPinstance_p, 0,        &size) )
     {
          return 0;
     }

#if 0
     /* DO ADDITIONAL CONFIGURATION */
     if (size < 128)
     {
          /* SET ref_8k_i = 0 */
     }
     else
     {
          /* SET ref_8k_i = 1 */
     }
#endif
     /* TRANSLATE Mb VALUE INTO BYTES */
     return (size * 1024 * 1024);
} /* N8_GetContextMemSize */

/*****************************************************************************
 * N8_ContextMemInit
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Initialize all data necessary to manage the context memory.
 *
 * This routine initializes all the data structures necessary to manage the 
 * the context memory on an NPS2000.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance.
 *
 * @return 
 *
 *****************************************************************************/

void N8_ContextMemInit(int chip)
{
   int ctxEntries;
   int size;
   NspInstance_t *NSPinstance_p = &NSPDeviceTable_g[chip];

   
   printf("N8_ContextMemInit(chip=%d)\n", chip);
   NSPinstance_p->contextMemSize = N8_GetContextMemSize(NSPinstance_p);
   printf("N8_ContextMemInit(chip=%d): still ok\n", chip);
   if (NSPinstance_p->contextMemSize)
   {
      ctxEntries = NSPinstance_p->contextMemSize/CONTEXT_ENTRY_SIZE;
      size = ctxEntries * sizeof(ContextMemoryMap_t);
      NSPinstance_p->contextMemMap_p = vmalloc(size);

      if (NSPinstance_p->contextMemMap_p)
      {
         memset(NSPinstance_p->contextMemMap_p, 0, size);
         NSPinstance_p->contextMemEntries = ctxEntries;
      }
   }

   /* Initialize the semaphore even though there is no context memory. This  */
   /* simplifies the allocation in case there is a mixture of NSP2000's with */
   /* and without context memory.                                            */
   printf("N8_ContextMemInit(chip=%d): N8_AtomicLockInit\n", chip);
   N8_AtomicLockInit(NSPinstance_p->contextMemSem);

} /* N8_ContextMemInit */

/*****************************************************************************
 * N8_ContextMemRemove
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Deallocates all resources allocated to manage the context memory.
 *
 * This routine deallocates all the resources allocated to manage the 
 * the context memory on an NPS2000.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance.
 *
 * @return 
 *
 *****************************************************************************/

void N8_ContextMemRemove(int chip)
{
   
   NspInstance_t *NSPinstance_p = &NSPDeviceTable_g[chip];

   if (NSPinstance_p->contextMemMap_p)
   {
      vfree(NSPinstance_p->contextMemMap_p);
      NSPinstance_p->contextMemMap_p = NULL;
   }
   N8_AtomicLockDel(NSPinstance_p->contextMemSem);
   NSPinstance_p->contextMemSize = 0;
   NSPinstance_p->contextMemNext = 0;
} /* N8_ContextMemRemove */

/*****************************************************************************
 * N8_ContextMemAlloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocate an entry from the context memory.
 *
 * This routine is called from the kernel space version of N8_AllocateContext.
 * Since we aren't coming in through the ioctl, we have to calculate the
 * session ID.  Then we call the common lower level function to do the work.
 * 
 * @param chip            RO: User specified chip, which must be validated.
 *
 * @return 
 *    Returns the index of the context memory allocation.
 *    -1 - The allocation has failed.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t N8_ContextMemAlloc(N8_Unit_t *chip, unsigned int *index_p)
{
   unsigned long  sessionID;

   sessionID = N8_GET_KERNEL_ID;

   return (n8_contextalloc(chip, sessionID, index_p));
} /* N8_ContextMemAlloc */


/*****************************************************************************
 * n8_contextalloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocate an entry from the context memory.
 *
 * This routine allocates and entry from the context memory on an NPS2000.
 * 
 * @param chip            RO: User specified chip, which must be validated.
 * @param sessionID       RO: ID of the allocating session.
 *
 * @return 
 *    Returns the index of the context memory allocation.
 *    -1 - The allocation has failed.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t n8_contextalloc(N8_Unit_t *chip, 
                            unsigned long sessionID,
                            unsigned int *index_p)
{
   N8_Unit_t      selectedChip;
   int            ctr;
   NspInstance_t *NSPinstance_p;
   unsigned long  index;
   int            numberOfUnitsToCheck = 1;
   int            i;
   N8_Status_t    ret = N8_STATUS_OK;

   /* Determine which queue to use, and set the queue_p in the API request */
   ret = QMgr_get_valid_unit_num(N8_EA, *chip, &selectedChip);
   if (ret == N8_STATUS_OK)
   {
      /* if the user requests any unit, we must check them all and give up only
       * if there is no context available on any of the chips.  if a specific
       * unit is specified, then the loop only checks the specfied unit.
       */
      if (*chip == N8_ANY_UNIT)
      {
         numberOfUnitsToCheck = NSPcount_g;
      }

      for (i = 0; i < numberOfUnitsToCheck; i++)
      {
         static int active = 0;
         NSPinstance_p = &NSPDeviceTable_g[selectedChip];
         index = NSPinstance_p->contextMemNext;

         N8_AtomicLock(NSPinstance_p->contextMemSem);
	 active++;
	 if (active > 1) {
		 printf("%s.%d: context unprotected - active=%d\n", __FILE__, __LINE__, active);
	 }
         for (ctr = 0; ctr < NSPinstance_p->contextMemEntries; ctr++)
         {
            if (NSPinstance_p->contextMemMap_p[index].userID == 0)
            {
               NSPinstance_p->contextMemMap_p[index].userID = sessionID;
               NSPinstance_p->contextMemNext = (index + 1) %  NSPinstance_p->contextMemEntries;
               *index_p = index;
               break;
            }
            index = (index + 1) %  NSPinstance_p->contextMemEntries;
         } /* for ctr */
	 active--;
         N8_AtomicUnlock(NSPinstance_p->contextMemSem);
   
         if (ctr != NSPinstance_p->contextMemEntries)
         {
            /* a free context was found -- stop looking */
            DBG(("Found context %d on chip %d\n", *index_p, selectedChip));
            break;
         }
         /* no context was found on this unit.  try another. */
         DBG(("Context not found.  Rolling to a new chip.\n"));
         selectedChip = (selectedChip + 1) % NSPcount_g;
      } /* for i */
      if (i == numberOfUnitsToCheck)
      {
         /* No more context memory */
         ret = N8_NO_MORE_RESOURCE;
      }
      else
      {
         *chip = selectedChip;
      }
   }
   return ret;
} /* n8_contextalloc */

/*****************************************************************************
 * N8_ContextMemFree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Free a context memory entry.
 *
 * This routine frees a context memory entry on an NPS2000.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance.
 *        entry           R0: The index of the entry to be freed.
 *
 * @return 
 *
 *****************************************************************************/

N8_Status_t N8_ContextMemFree(int chip, unsigned long entry)
{
   unsigned long  sessionID;

   sessionID = N8_GET_KERNEL_ID;

   return (n8_contextfree(chip, sessionID, entry));
} /* N8_ContextMemFree */

 /*****************************************************************************
 * n8_contextmemfree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Free a context memory entry.
 *
 * This routine frees a context memory entry on an NPS2000.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance.
 *        entry           R0: The index of the entry to be freed.
 *
 * @return 
 *
 *****************************************************************************/

N8_Status_t 
n8_contextfree(int chip, unsigned long sessionID, unsigned long entry)
{
   NspInstance_t *NSPinstance_p = &NSPDeviceTable_g[chip];
   N8_Status_t    retCode = N8_STATUS_OK;

   /* Check for a valid entry */
   if (entry < NSPinstance_p->contextMemEntries)
   {
      if (NSPinstance_p->contextMemMap_p[entry].userID == 0)
      {
         /* Attempting to free a context that is already free */
         retCode = N8_UNALLOCATED_CONTEXT;
      }
      else if (sessionID == NSPinstance_p->contextMemMap_p[entry].userID)
      {
         /* Session ID matches the session ID that allocated this
            memory. */      
         NSPinstance_p->contextMemMap_p[entry].userID = 0;
      }
      else
      {
         /* Wrong session ID */
         retCode = N8_INVALID_VALUE;
      }
   }
   else
   {
      /* entry specified is invalid */
      retCode = N8_INVALID_PARAMETER;
   }

   return (retCode);
} /* n8_contextfree */

/*****************************************************************************
 * N8_ContextMemValidate
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Validate a context memory entry.
 *
 * This routine frees a context memory entry on an NPS2000.
 * 
 * @param chip            RO: The chip.
 *        entry           R0: The index of the entry to be freed.
 *
 * @return 
 *
 *****************************************************************************/

N8_Status_t N8_ContextMemValidate(N8_Unit_t chip, unsigned int entry)
{
   unsigned long  sessionID;

   sessionID = N8_GET_KERNEL_ID;

   return (n8_contextvalidate(chip, sessionID, entry));
} /* N8_ContextMemValidate */

 /*****************************************************************************
 * n8_contextvalidate
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Validate a context memory entry.
 *
 * This routine validates a context memory entry on an NPS2000. This in
 * intended only to be called from the ContextRead and ContextWrite functions.
 * 
 * @param chip            RO: The chip.
 *        sessionID       RO: The sessionID for the entry.
 *        entry           RO: The index of the entry to be freed.
 *
 * @return 
 *
 *****************************************************************************/

N8_Status_t 
n8_contextvalidate(N8_Unit_t chip, unsigned long sessionID, unsigned int entry)
{
   NspInstance_t *NSPinstance_p = &NSPDeviceTable_g[chip];
   N8_Status_t    retCode = N8_STATUS_OK;

   /* Check for a valid entry */
   if (entry < NSPinstance_p->contextMemEntries)
   {
      if (NSPinstance_p->contextMemMap_p[entry].userID == 0)
      {
         /* Attempting to free a context that is already free */
         retCode = N8_UNALLOCATED_CONTEXT;
      }
      else if (sessionID != NSPinstance_p->contextMemMap_p[entry].userID)
      {
         /* Session ID does not match the session ID that allocated this
            memory. */      
         retCode = N8_INVALID_VALUE;
      }
   }
   else
   {
      /* entry specified is invalid */
      retCode = N8_INVALID_VALUE;
   }

   return (retCode);
} /* n8_contextvalidate */

/*****************************************************************************
 * N8_ContextFreeAll
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Free all of the entries for a singles session on a single chip.
 *
 * This routine marks all of the context entries owned by the specified session
 * on the specified chip as free. This routine is called as part of the session
 * cleanup in the exit handler of the driver. 
 *
 * We do not get the lock at any time during this operation.
 *
 * IMPORTANT NOTE: This algorithm currently used in this routine is the most
 *                 brute force (and consequently the slowest) imaginable. In
 *                 order to optimize the speed of this routine we should
 *                 probably move towards a session based management scheme.
 *
 * 
 * @param chip        RO: The chip number for which to clear the context memory.
 * @param sessionID   RO: The session id for which to clear the context memory.
 *
 * @return 
 *    None.  What would the user do if this function failed?
 *   
 * @par Errors:
 *****************************************************************************/

void N8_ContextMemFreeAll(N8_Unit_t chip, unsigned long sessionID)
{
   int ctr;
   NspInstance_t *NSPinstance_p;

   NSPinstance_p = &NSPDeviceTable_g[chip];

   /* Look through the whole table and clear any entries whose
      sessionID matches our target. */
   for (ctr = 0; ctr < NSPinstance_p->contextMemEntries; ctr++)
   {
      if (NSPinstance_p->contextMemMap_p[ctr].userID == sessionID)
      {
         NSPinstance_p->contextMemMap_p[ctr].userID = 0;
      }
   }
} /* N8_ContextMemFreeAll */

/*****************************************************************************
 * n8_contextDisplay
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Display contents of contex allocation map.
 *
 * This debugging routine simply displays the contents of the context map
 * via the standard kernel debugging message facility. It also assumes that
 * the caller has properly locked the allocation map.
 *
 * Note that Debug_g must be set, for this routine to do anything.
 *
 * @par Externals:
 *    NSPDeviceTable_g RO: Global allocation map.              <BR>
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_contextDisplay(void)
{
   int            ctr;
   int            chip; 
   int            nUsed = 0;
   NspInstance_t *NSPinstance_p;

   N8_PRINT( "n8_context: <******* CONTEXT MEMORY STATISTICS ******>\n");
   for (chip = 0; chip < NSPcount_g; chip++)
   {
      N8_PRINT( "n8_context: <******* CHIP %d STATISTICS ******>\n", chip);

      NSPinstance_p = &NSPDeviceTable_g[chip];
      for (ctr = 0; ctr < NSPinstance_p->contextMemEntries; ctr++)
      {
         if (NSPinstance_p->contextMemMap_p[ctr].userID != 0)
         {
            nUsed ++;
            if (nUsed <= N8_CONTEXT_MAX_PRINT)
            {
               N8_PRINT( "n8_context: Slot %d used by Session %d\n",
               ctr, NSPinstance_p->contextMemMap_p[ctr].userID);
            }
         }
      }

      if (nUsed > N8_CONTEXT_MAX_PRINT)
      {
         N8_PRINT( "n8_context: Only printed first %d details\n",
                   N8_CONTEXT_MAX_PRINT) ;
      }
      N8_PRINT(
         "n8_context: <******* CHIP %d TOTAL CONTEXT ALLOCATION %d ******>\n",
         chip, nUsed);
      N8_PRINT("n8_context: <******* END CHIP %d STATISTICS ******>\n", chip);
      nUsed = 0;
   }

   N8_PRINT( "n8_context: <********* CONTEXT MEMORY END ***********>\n");
   return;
} /* n8_contextDisplay */

