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
 * @(#) n8_memory.c 1.47@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_memory.c                                                        *
 *  @brief Memory Services - Cross-Platform                                  *
 *                                                                           *
 * This file implements the kernel only portions of the memory allocation    *
 * and management service for the NSP2000 devices, to accomplish two basic   *
 * objectives :                                                              *
 *                                                                           *
 *     - Size limitations in allocation requests. Linux only allows up to    *
 *       128K to be allocated in a single call. n8_memory enables the api    *
 *       to allocate larger physically contiguous space.                     *
 *                                                                           *
 * n8_memoryInit -                                                           *
 *      Obtains the large allocated space. It allocates the large pool       *
 *      with n8_GetLargeAllocation (platform-specific).                      *
 * n8_memory_Release -                                                       *
 *      Purges the allocation map and deallocates the large allocation space.*
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/17/04 jpw   Change n8_memoryInit() memory allocation failure messages
 * 		  to kernel warning level and make it clear that the driver
 * 		  has not loaded. Suggest reserving space with bigphysarea
 * 05/08/03 brr   Reworked user pools to make better use of multiple driver
 *                banks. Moved user pool setup into the kernel.
 * 05/02/03 brr   Reset global pointer when memory is freed.
 * 04/30/03 brr   Reconcile differences between 2.4 & 3.0 baselines.
 * 04/22/03 brr   Correct semaphore initialization problem.
 * 04/22/03 brr   Removed redundant parameter from n8_FreeLargeAllocation.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 11/25/02 brr   Clean up comments and format to coding standards.
 * 11/01/02 brr   Correct order of resource deallocation in n8_memoryRelease.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 07/02/02 brr   Added N8_QueryMemStatistics & access curAllocated as an
 *                atomic variable.
 * 06/26/02 brr   Remove bank parameter from calls to N8_PhysToVirt.
 * 06/25/02 brr   Rework user pool allocation to only mmap portion used by
 *                the individual process.
 * 06/14/02 hml   Change the type of an allocated buffer to 
 *                N8_MEMORY_KERNEL_BUFFER and store the physical address of the
 *                bank in the memory struct.
 * 06/12/02 hml   Calls to N8_PhysToVirt now take the bank parameter. We 
 *                also set the bank index in N8_BufferAllocate call.
 * 06/11/02 hml   Added include of n8_semaphore.h.
 * 06/11/02 hml   Use the new process sem calls instead of *Atomic*.
 * 06/11/02 hml   Removed some dangling prototypes.
 * 06/10/02 hml   Make n8_memoryRelease check for a NULL pointer.
 * 06/10/02 hml   Moved some functionality into n8_manage_memory.c.
 * 05/22/02 hml   Removed use of global variables and moved allocation map 
 *                into the allocated space.
 * 04/30/02 brr   Do not memset the buffer to zero in N8_AllocateBuffer.
 * 04/06/02 brr   Make print for memory init already complete conditional.
 * 03/27/02 brr   Removed semaphore from free function since they are called
 *                from interrupt context and are not needed. Rework allocation 
 *                so they are done in a round robin fashon.
 * 03/26/02 brr   Modified pfreeSessID to not free buffers on the command queue.
 * 03/22/02 brr   Move clearing of memory out of n8_pmalloc to avoid two calls
 *                to PhysToVirt.
 * 03/22/02 hml   N8_AllocateBuffer now calls N8_GET_SESSION_ID instead of
 *                hardcoding a session id.
 * 03/18/02 brr   Pass sessionID into allocate & free functions.
 * 03/13/02 brr   Make n8_memoryInit reentrant to support BSD initialization.
 * 03/08/02 brr   Removed use of N8_UTIL_MALLOC.
 * 02/26/02 brr   Redo some of the print statements.
 * 02/22/02 spm   Converted printk's to DBG's.  Added include of n8_OS_intf.h.
 * 02/18/02 brr   Renamed management fuctions to match those in n8_driver_api.c
 * 02/14/02 brr   Reconcile memory management differences with 2.0.
 * 01/17/02 brr   Function name cleanup.
 * 01/11/02 bac   Added debug messages to warn if the driver is forced to clean
 *                up after an exiting process.  Also, track the users requested
 *                allocation size in bytes.
 * 12/20/01 brr   Use numBlocks instead of sessionID to determine availability.
 *                This allows driver to do static allocations with session ID 0.
 * 12/19/01 brr   Reset firstFreeIndex in n8_pfreeSessID.
 * 12/18/01 brr   Added n8_pfreeSessID.
 * 12/14/01 brr   Memory management performance improvements.
 * 05/17/01 mmd   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver Memory Allocation Services - Cross-Platform.
 */


#include "n8_OS_intf.h"
#include "helper.h"
#include "n8_driver_parms.h"
#include "n8_driver_api.h"
#include "n8_malloc_common.h" 
#include "n8_memory.h" 
#include "n8_semaphore.h" 
#include "n8_version.h" 
#include "userPool.h" 

/* Local global that indicates whether debugging messages are enabled */
static int DebugFlag_g = 0;

/* The control structures for all memory banks */
MemBankCtl_t  *memBankCtl_gp[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS] = {NULL};

void n8_calculateSize(MemBankCtl_t *memCtl_p, unsigned long size);
void n8_setupAllocatedMemory(MemBankCtl_t *memCtl_p, unsigned long size);
/*****************************************************************************
 * n8_memoryDebug
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Enable/disable debug messages.
 *
 * This routine enables or disables debug messages in the n8 memory module.
 *
 * @par Externals:
 *    DebugFlag_g   RO: Switch to enable/disable debug messages.
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_memoryDebug(int enable)
{
   DebugFlag_g = enable;
}


/*****************************************************************************
 * n8_memoryDisplay
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Display contents of allocation map.
 *
 * This debugging routine simply displays the contents of the allocation map
 * via the standard kernel debugging message facility. It also assumes that
 * the caller has properly locked the allocation map.
 *
 * @param memCtl_p  RW:  The memory bank in question. The result of this
 *                           function is stored in the maxEntries field.
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/
 
void n8_memoryDisplay(N8_MemoryType_t bank)
{
   int          idx  = 0;
   MemCtlList_t* memCtlList_p;
   MemBankCtl_t  *memCtl_p = memBankCtl_gp[bank];
 
   if (memCtl_p->maxEntries == 0)
   {
      return;
   }
   N8_PRINT( "n8_memory: " N8_VERSION_STRING);
   N8_PRINT( "n8_memory: <******* PHYSICAL ALLOCATION MAP STATISTICS ******>\n");
 
   N8_PRINT( "n8_memory: Memory Allocated for bank %d - %d KB\n",
            bank, memCtl_p->allocSize / N8_ONE_KILOBYTE);
   N8_PRINT( "n8_memory: Memory Granularity for this bank - %ld \n",
            memCtl_p->granularity );
   N8_PRINT( "n8_memory: Current Allocation - %d / %d blocks   %d %%\n",
            n8_atomic_read(memCtl_p->curAllocated), memCtl_p->maxEntries,
            ((n8_atomic_read(memCtl_p->curAllocated) * 100)/ memCtl_p->maxEntries));
   N8_PRINT( "n8_memory: Maximum Allocation - %d / %d blocks   %d %%\n",
            memCtl_p->maxAllocated, memCtl_p->maxEntries,
            ((memCtl_p->maxAllocated * 100)/ memCtl_p->maxEntries));
   N8_PRINT( "n8_memory: Failed Allocations - %d, Next Index - %d\n",
            memCtl_p->failedAllocs, memCtl_p->nextIndex);

   N8_PRINT( "n8_memory: <******* PHYSICAL ALLOCATION MAP START at %lx *******>\n",
            memCtl_p->memBaseAddress );
   memCtlList_p = (MemCtlList_t *)
   N8_PhysToVirt(memCtl_p->memBaseAddress + sizeof(MemBankCtl_t));
 
   while (idx < memCtl_p->maxEntries)
   {
      if (memCtlList_p[idx].numBlocks)
      {
         N8_PRINT( "n8_memory: #%04d-Addr=%08lx SessID=%08d  Num Blocks=%d \n",
                    idx,
                    (int)memCtl_p->memBankPtr + (idx * memCtl_p->granularity),
                    memCtlList_p[idx].sessionID,
                    memCtlList_p[idx].numBlocks);
         idx += memCtlList_p[idx].numBlocks;
      }
      else
      {
         idx++;
      }
   }
   N8_PRINT( "n8_memory: <********* PHYSICAL ALLOCATION MAP END ***********>\n");
   return;
}

/*****************************************************************************
 * n8_calculateSize
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Calculate the number of blocks of a given granularity that can
 *        be carved out of a contiguous memory area of a given size, given
 *        the need for the management structure to be part of the memory area.
 *
 * Time to revisit Algebra I! 
 *
 * The diagram below shows the layout of the 
 * allocated space.  First we have the space for the MemBankCtl_t 
 * structure.  This is the constant M. Next we have the 
 * space for the memory control list for the area.  The size of this
 * area = sizeof(MemCtlList_t) (N) * the number of data blocks that can
 * fit in the remainder of the buffer (C). This depends on the total size
 * of the buffer.  The remainder of the used space in the buffer (D) is 
 * composed of C data blocks each of size granularity (G). Of course, we
 * have between 0 and G - 1 bytes wasted at the end of the buffer.
 *
 *      ////////////////////////////////////////////////////////////////
 *      /                /                   /                         /
 *      /<--- M -------->/<--- N * C ------->/<------- D ------------->/
 *      / (MemBankCtl_t) / (MemCtlList_t *   /<----- C * G ----------->/
 *      /                / Count of blocks)  / (Data = C * Granularity)/
 *      /                /                   /                         /
 *      ////////////////////////////////////////////////////////////////
 *      |                                                              |
 *      |<-----              S (Total size of buffer)            ----->|
 *
 * This gives us the following equations:
 *
 * 1) S = M + (N * C) + D
 * 2) C = D / G
 *
 * Substiting D/G for C in 1 gives
 *
 * 3) S = M + (N * (D/G) + D
 * 
 * By combining terms and subtraction we have
 *
 * 4) S - M = (N * D)/ G + D
 * 
 * Multiplying both sides by G yields
 *
 * 5) G * (S - M) = (N * D) + (G * D) = (N + G) * D
 *
 * Now divide both sides by (N + G) to get
 *
 * 6) (G * (S - M))/(N + G) = D
 * 
 * What we actually want is C.  Since C = D/G, we can divide both sides by
 * G and get
 *
 * 7) (S - M)/(N + G) = C
 *
 * Since G, S, M and N are all constant we can now get the number of data
 * blocks (C).
 *                                                                        
 * @param memCtl_p  RW:  The memory bank in question. The result of this 
 *                           function is stored in the maxEntries field.
 * @param size          RO:  The size of the memory bank.
 *
 * @par Externals:
 *    Debug_g          RO: Switch to enable/disable debug messages.   <BR>
 *
 * @return
 *    NULL       pointer if process failed
 *
 * @par Errors:
 *    We will set the maxEntries field to 0 if size is NULL.
 *****************************************************************************/
void n8_calculateSize(MemBankCtl_t *memCtl_p, unsigned long size)
{
   unsigned int dividend;  /* dividend of equation 7 */
   unsigned int divisor;   /* divisor of equation 7 */

   /* Do some minimal error checking */
   if (memCtl_p == NULL)
   {
      return;
   }

   else if (size <= sizeof(MemBankCtl_t)) 
   {
      memCtl_p->maxEntries = 0;
      return;
   }

   /* Equation 7 */
   dividend =  size - sizeof(MemBankCtl_t);
   divisor = sizeof(MemCtlList_t) + memCtl_p->granularity;

   /* Integer divide is good, since we want complete blocks */
   memCtl_p->maxEntries = dividend / divisor;   
}
/*****************************************************************************
 * n8_setupAllocatedMemory
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - n8_memory initialization after the large allocation.
 *
 * This routine should be called after a large memory block such as a
 * bank or a pool is allocated.  The routine sets up the memBankCtl structure
 * and initializes the allocation map. It does not do the initilization of the
 * lock, which can be done before or after this routine is called.
 *                                                                        
 * @param memCtl_p   RW:  Virtual pointer to the memBankCtl_t to initialize.
 * @param size       RO:  The size of the allocated block.
 *
 * @par Externals:
 *    Debug_g          RO: Switch to enable/disable debug messages.   <BR>
 *
 * @return
 *    NULL       pointer if process failed
 *    non-NULL   void physical pointer to the base of the big allocation
 *
 * @par Errors:
 *    See return section for error information.
 *
 * @par Assumptions:
 *    We assume that the granularity field of the MemBankCtl_t structure is set
 *    before this routine is called and that memCtl_p is a virtual pointer
 *****************************************************************************/
void n8_setupAllocatedMemory(MemBankCtl_t *memCtl_p, unsigned long size)
{
   int            ctlListSize;

   /* Save the total size of the allocated block */
   memCtl_p->allocSize = size;

   /* Calculate the maximum number of entries */
   n8_calculateSize(memCtl_p, size);

     /* INITIALIZE ALLOCATION LIST AND MARK BANK BOUNDARIES */
   memCtl_p->nextIndex = 0;
   n8_atomic_set(memCtl_p->curAllocated, 0);
   memCtl_p->maxAllocated = 0;
   memCtl_p->failedAllocs = 0;

   /* Set the mem bank ptr to the address after the bank control structure
      and the allocation map rounded up to next granularity. */
   memCtl_p->memBankPtr = (char *)(memCtl_p->memBaseAddress +
       (((size/memCtl_p->granularity) - memCtl_p->maxEntries) *memCtl_p->granularity));

   /* Clear the allocation map */
   ctlListSize = sizeof(MemCtlList_t) * memCtl_p->maxEntries;
   memset(&memCtl_p->memCtlList, 0, ctlListSize);


}

/*****************************************************************************
 * n8_memoryInit
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - n8_memory initialization and large allocation.
 *
 * This routine should be called when a driver is loaded and initialized,
 * before making any other n8_memory calls. It first performs some OS-specific
 * tasks to allocate the requested large memory size, from physically
 * contiguous memory. If it fails, we immediately free everything and return
 * a NULL pointer. If successful, we return a pointer to the base of the
 * entire big allocation. which the caller may use or discard as it sees fit -
 * it is not needed again.
 *                                                                        
 * @param bank          RO:  Index which specifies the enumerated bank type
 * @param size          RO:  Specifies the desired allocation size per bank
 * @param granularity   RO:  The size of the smallest allocation.
 *
 * @par Externals:
 *    memBankCtl_gp     RW: Global pointers to memory control structures<BR>
 *
 * @return
 *    NULL       pointer if process failed
 *    non-NULL   void physical pointer to the base of the allocation
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void *n8_memoryInit(N8_MemoryType_t   bank,
                    unsigned long     size,
                    unsigned long     granularity)
{
   MemBankCtl_t   tmpBankCtl;
   MemBankCtl_t  *memBankCtl_p = memBankCtl_gp[bank];

   if (bank >= N8_MEMBANK_MAX+DEF_USER_POOL_BANKS)
   {
      return NULL;
   }

   if (memBankCtl_p != NULL)
   {
      if (DebugFlag_g)
      {
          N8_PRINT(KERN_CRIT "n8_memoryInit: Initialize already complete exiting...\n");
      }
      return (void *)((memBankCtl_p)->memBaseAddress);
   }

   if (DebugFlag_g)
   {
      N8_PRINT(KERN_CRIT "n8_memoryInit: size = %ld.\n", size);
   }

   /* We are using the temporary bank pointer because on at least one
      OS we support (linux) we need some of the space in the memBankCtl
      to complete the large allocation. Because we want to end up with
      the memBankCtl in the allocated space, we use a temporary that
      is allocated automatically, then copy the temp structure into the
      allocated space after the large allocation is complete. */

   /* Insure everything starts nice and clean */
   memset(&tmpBankCtl, 0x0, sizeof(tmpBankCtl));

   /* Set the desired granularity  and bank */
   tmpBankCtl.granularity = granularity; 
   tmpBankCtl.bankIndex = bank;

   /* Perform OS-specific large allocation */
   tmpBankCtl.memBaseAddress = n8_GetLargeAllocation(bank, size, DebugFlag_g);
   if (!tmpBankCtl.memBaseAddress)
   {
      N8_PRINT(KERN_WARNING "NSP2000: n8_memoryInit: Failed to allocate %ld bytes, bank %d .\n", size, bank);
      N8_PRINT(KERN_WARNING "NSP2000: Driver NOT loaded. \n");
      N8_PRINT(KERN_WARNING "NSP2000: Reserve space with bigphysarea patch \n");
      N8_PRINT(KERN_WARNING "NSP2000: or decrease EA or PK Pool request.\n");
      return NULL;
   }
     
   /* Set the memBankCtl pointer to the virtual address of the beginning
      of the block */
   memBankCtl_p = (MemBankCtl_t *) N8_PhysToVirt(tmpBankCtl.memBaseAddress);

   /* Copy the tmp structure to inside the allocated block */
   memcpy(memBankCtl_p, &tmpBankCtl, sizeof(MemBankCtl_t));

   /* Set up the atomic lock */
   N8_initProcessSem(&(memBankCtl_p->allocationMapLock));

   /* Setup the memBankCtl_p and the allocation map */
   n8_setupAllocatedMemory(memBankCtl_p, size);

   memBankCtl_gp[bank] = memBankCtl_p;

   if (DebugFlag_g)
   {
      N8_PRINT(KERN_CRIT "n8_memoryInit: returning %lx.\n", memBankCtl_p->memBaseAddress);
   }

   /* Return the physical (non-zero) address */
   return (void *)((memBankCtl_p)->memBaseAddress);
}



/*****************************************************************************
 * n8_memoryRelease
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - Memory deactivation and release.
 *
 * This routine deallocates and releases the large space allocated with
 * n8_memoryInit.
 *
 * @param bank          RO:  Index which specifies the enumerated bank type
 *
 * @par Externals:
 *    memBankCtl_gp  RO: Global pointers to memory control structures<BR>
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_memoryRelease(N8_MemoryType_t bank)
{
   MemBankCtl_t  *memCtl_p = memBankCtl_gp[bank];

   if (DebugFlag_g)
   {
      N8_PRINT(KERN_CRIT "n8_memoryRelease: bank = %d, memCtl_p %p.\n", bank, memCtl_p);
   }

   /* Check for null in case this is called because the second memory
      segment was not created. */
   if (memCtl_p == NULL)
   {
      return;
   }

   N8_acquireProcessSem(&memCtl_p->allocationMapLock);

   memBankCtl_gp[bank] = NULL;

   N8_releaseProcessSem(&memCtl_p->allocationMapLock);

   memCtl_p->memBaseAddress = 0;

   N8_deleteProcessSem(&memCtl_p->allocationMapLock);

   n8_FreeLargeAllocation(bank, DebugFlag_g);

}

/*****************************************************************************
 * n8_pfreeSessID
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - Releases a all allocations of a given session ID.
 *
 *
 * @param bank       RO:  Index which specifies the enumerated bank type
 * @param sessionID  R0: Session ID of the exiting process.
 *
 * @par Externals:
 *    memBankCtl_gp  RO: Global pointers to memory control structures<BR>
 *    Debug_g        RO: Global flag that indicates whether debugging<BR>
 *                             messages are enabled.                       <BR>
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_pfreeSessID(N8_MemoryType_t bank, unsigned long sessionID)
{

   MemCtlList_t      *memCtlList_p;
   int                index;
   int                numBlocks;
   N8_MemoryHandle_t *kmem_p;
   MemBankCtl_t  *memCtl_p = memBankCtl_gp[bank];

   if (memCtl_p == NULL)
   {
      return;
   }
   if (DebugFlag_g)
   {
      N8_PRINT(KERN_CRIT "n8_pfreeSessID: sessionID %ld, for bank %d\n",
		                          sessionID, bank);
   }

   /* Retrieve the memCltList_p in the bank structure. */ 
   memCtlList_p = &memCtl_p->memCtlList[0];
   index = 0;

   while (index < memCtl_p->maxEntries)
   {
     /* If this entry been allocated, see if it matches our session ID  */
     if (memCtlList_p[index].numBlocks) 
     {

        numBlocks = memCtlList_p[index].numBlocks; 

        /* Determine whether this entry matches the sessionID passed int */
        if (memCtlList_p[index].sessionID == sessionID)
        {
           char *ptr;
           ptr = index * memCtl_p->granularity + memCtl_p->memBankPtr;

           /* If we have a match on the session ID, convert the pointer to  */
           /* a virtual address to determine whether it is currently in use */
           /* by one of the command queues.                                 */
           kmem_p = (N8_MemoryHandle_t *)N8_PhysToVirt((int)ptr);

           if (kmem_p->bufferState != N8_BUFFER_QUEUED)
           {

                /* The Buffer is not on a command queue, free it. */
                if (DebugFlag_g)
                {
                   N8_PRINT("Kernel mem leak on exit %d bytes at %lx\n",
                    memCtlList_p[index].reqSize, (unsigned long) ptr);
		}

                n8_pfree(memCtl_p->bankIndex, ptr);
             }
             else
             {
                /* Mark the buffer so QMgr frees it 
                   when the command has completed */
                if (DebugFlag_g)
                {
                   N8_PRINT(KERN_CRIT "Kernel mem leak on exit"
                       "skipped queued buffer %d bytes at %lx\n",
                       memCtlList_p[index].reqSize, (unsigned long) ptr);
		}
                kmem_p->bufferState = N8_BUFFER_SESS_DELETED;
             }
          }
          index += numBlocks;
    
      }
      else
      {
         index++;
      }
   }

   return;
}



/*****************************************************************************
 * N8_AllocateBuffer
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Allocates a buffer and maps it between user and kernel space.
 *
 * This routine first requests that the driver allocate a physically
 * contiguous memory space of a specified size. With the returned physical
 * base address, it then maps it to user-space.
 *
 * The N8_MemoryHandle_t should be treated as read-only upon return from this
 * call, for subsequent calls to N8_TestBuffer and N8_FreeBuffer.
 *
 * @param size   RO: Amount of memory to allocate.
 *
 * @par Externals:
 *    MemBankCtl_p  RW: Bank control structure to allocate from.
 *                                  
 * @return 
 *    Address of the new memory handle or NULL if the allocation could not be
 *    completed.
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_MemoryHandle_t *N8_AllocateBuffer(const unsigned int size)

{
    
   N8_MemoryHandle_t *buffPtr;
   unsigned long physaddr;
   unsigned long sessionID = N8_GET_KERNEL_ID;

   /* Do not attempt allocations of zero size */
   if (!size)
   {
      return NULL;
   }

     
   /* Allocate a buffer from the physically contigious pool */
   if ((physaddr = n8_pmalloc(N8_MEMBANK_EA,size + N8_BUFFER_HEADER_SIZE, sessionID)) == 0)
   {
      return NULL;
   }

   /* Set up a N8_MemoryHandle in the head of the buffer */
   buffPtr = (N8_MemoryHandle_t *)N8_PhysToVirt(physaddr);

   /* Ensure the buffer is cleared */
   /* memset(buffPtr, 0x0, size + N8_BUFFER_HEADER_SIZE); */
   buffPtr->Size            = size;
   buffPtr->PhysicalAddress = physaddr + N8_BUFFER_HEADER_SIZE;
   buffPtr->VirtualAddress  = (unsigned long *)((int)buffPtr + N8_BUFFER_HEADER_SIZE);
   buffPtr->bankPhysAddress = memBankCtl_gp[N8_MEMBANK_EA]->memBaseAddress;
   buffPtr->bankAddress = (unsigned long) memBankCtl_gp[N8_MEMBANK_EA];
   buffPtr->bankIndex = N8_MEMBANK_EA;

   return buffPtr;
}


N8_MemoryHandle_t *N8_AllocateBufferPK(const unsigned int size)

{
    
   N8_MemoryHandle_t *buffPtr;
   unsigned long physaddr;
   unsigned long sessionID = N8_GET_KERNEL_ID;

   /* Do not attempt allocations of zero size */
   if (!size)
   {
      return NULL;
   }

     
   /* Allocate a buffer from the physically contigious pool */
   if ((physaddr = n8_pmalloc(N8_MEMBANK_PK,size + N8_BUFFER_HEADER_SIZE, sessionID)) == 0)
   {
      return NULL;
   }

   /* Set up a N8_MemoryHandle in the head of the buffer */
   buffPtr = 
      (N8_MemoryHandle_t *)N8_PhysToVirt(physaddr);

   /* Ensure the buffer is cleared */
   /* memset(buffPtr, 0x0, size + N8_BUFFER_HEADER_SIZE); */
   buffPtr->Size            = size;
   buffPtr->PhysicalAddress = physaddr + N8_BUFFER_HEADER_SIZE;
   buffPtr->VirtualAddress  = (unsigned long *)((int)buffPtr + N8_BUFFER_HEADER_SIZE);
   buffPtr->bankPhysAddress = memBankCtl_gp[N8_MEMBANK_PK]->memBaseAddress;
   buffPtr->bankAddress = (unsigned long) memBankCtl_gp[N8_MEMBANK_PK];
   buffPtr->bankIndex = N8_MEMBANK_PK;

   return buffPtr;
}




/*****************************************************************************
 * N8_FreeBuffer
 *****************************************************************************/
/** @ingroup NSP2000_Driver_API
 * @brief Releases and unmaps buffers allocated with N8_AllocateBuffer.
 *
 * This routine follows two steps. It first unmaps the specified buffer from
 * user-space. It then requests that the driver free the buffer. Because
 * unique keys are used to identify buffers, only if a corresponding buffer is
 * located, is anything freed. If not found, nothing happens.
 *
 * @param MemoryStruct   RO: Pointer to a struct that associates the necessary
 *                           parameters that completely identify an allocated
 *                           buffer.
 *
 * @par Externals:
 *    N/A
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 * 
 * NOTE: THIS FUNCTION MAY BE CALLED FROM INTERRUPT CONTEXT and cannot take
 *       semaphores or print. It also assumes it will only be called to free
 *       buffers allocted from the kernel buffer allocation pool.
 *****************************************************************************/

void N8_FreeBuffer(N8_MemoryHandle_t *MemoryStruct_p)
{

   /* DIRECT DRIVER TO DEALLOCATE THE BUSMASTERED BUFFER */
   n8_pfree(MemoryStruct_p->bankIndex,
             (void *)(MemoryStruct_p->PhysicalAddress - N8_BUFFER_HEADER_SIZE));
}

N8_Status_t N8_QueryMemStatistics(MemStats_t *memStatsPtr)
{
   int           bankIndex;
   MemBankCtl_t *memCtl_p;

   for (bankIndex = 0; bankIndex <= N8_MEMBANK_PK; bankIndex++)
   {

      memCtl_p = memBankCtl_gp[bankIndex];

      /* Retrieve stats for the base kernel buffer pool */
      memStatsPtr->curAllocated = n8_atomic_read(memCtl_p->curAllocated);
      memStatsPtr->maxAllocated = memCtl_p->maxAllocated;
      memStatsPtr->maxEntries   = memCtl_p->maxEntries;
      memStatsPtr->failedAllocs = memCtl_p->failedAllocs;
      memStatsPtr++;
   }

   return (N8_STATUS_OK);
}


