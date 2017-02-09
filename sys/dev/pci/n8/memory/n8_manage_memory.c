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
 * @(#) n8_memory.c 1.1@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_manage_memory.c                                                 *
 *  @brief Memory Services - Cross-Platform                                  *
 *                                                                           *
 * This file implements the portions of the memory allocation and management *
 * service for the NSP2000 devices which are available from both user and    *
 * kernel space. The most important of these are                             *
 *                                                                           *
 * n8_pmalloc -                                                              *
 *      Replicates standard malloc() functionality with a similar interface. *
 *      Allocates a block of memory from a pool that is safe to perform DMA  *
 *      operations to/from since is has been allocated and locked down by    *
 *      the device driver.                                                   *
 * n8_pfree -                                                                *
 *      Replicates standard free() functionality with the same interface. It *
 *      free the block of memory allocated by n8_pmalloc.                    *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/07/03 brr   Moved n8_calculateSize & n8_setupAllocateMemory into the
 *                kernel. Updated comments.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 07/29/02 jpw   n8_pmalloc - SearchCount must be changed before  searchIndex
 *		  when scanning for free blocks - prevents false pmalloc failures  
 * 07/11/02 brr   Round memBankPtr up to next granularity.
 * 07/01/02 brr   Ensure all operations on curAllocated are atomic.
 * 06/27/02 brr   Removed bank parameter from call to N8_PhysToVirt.
 * 06/25/02 brr   Moved n8_displayMemory to n8_memory.c.
 * 06/14/02 hml   Release semaphore at the bottom of n8_pmalloc.
 * 06/12/02 hml   Calls to N8_PhysToVirt now take the bank parameter.
 * 06/11/02 hml   Use the processSem ops instead of the *ATOMIC* ops.
 * 06/10/02 hml   Updated comments.
 * 06/10/02 hml   New file with functionality taken from n8_memory.c.
 ****************************************************************************/
/** @defgroup NSP2000Driver Memory Allocation Services - Cross-Platform.
 */


#include "n8_OS_intf.h"                                 
#if (defined __KERNEL__) || (defined KERNEL)
#include "helper.h"
#endif
#include "n8_malloc_common.h" 
#include "n8_semaphore.h" 

/* LOCAL GLOBAL THAT INDICATES WHETHER DEBUGGING MESSAGES ARE ENABLED */
static int DebugFlag_g = 0;

/* The memory bank controller for the request pool */
extern MemBankCtl_t *requestPoolCtl_gp;
extern MemBankCtl_t  *memBankCtl_gp[];


/* MACROS FOR READABILITY */
#define PRINTK          if (DebugFlag_g) N8_PRINT

/*****************************************************************************
 * n8_pmalloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - Allocates a portion of the large space.
 *
 * This routine allocates a portion of the large space, from within the   
 * specified bank, and returns a pointer to the physical address of
 * the portion. This routine is meant to be equivalent to a standard
 * malloc() sort of system call.
 *                                                                          
 * @param bank       RW:  Specifies the bank from which to allocate. The 
 *                        allocation map inside the bank is updated.
 * @param bytesReq   RO:  Specifies the number of bytes to be allocated
 * @param sessionID  RO:  The sessionID doing the allocation.
 * 
 *
 * @par Externals:
 *    Debug_g              RO: Switch to enable/disable debug messages.   <BR>
 *
 * @return
 *    NULL       pointer if there's insufficient free space in the big alloc.
 *    non-NULL   void virtual pointer to the base virtual address of the
 *               desired chunk of the big allocation.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

unsigned long 
n8_pmalloc(int bank, int bytesReq, unsigned long sessionID)
{
     int      searchIndex;
     int      searchCount;
     MemCtlList_t* memCtlPtr;
     int      freeBlocks;
     int      indx;
     int      blocksReq;
     unsigned long physAddr;
#if (defined __KERNEL__) || (defined KERNEL) || (defined VX_BUILD)
     MemBankCtl_t *memCtl_p = memBankCtl_gp[bank];
#else
     MemBankCtl_t *memCtl_p = requestPoolCtl_gp;
#endif

     if (memCtl_p == NULL)
     {
        return 0;
     }
     PRINTK("n8_pmalloc: bytes requested = %d, sessionID = %lx \n", 
                                                  bytesReq, sessionID);
     blocksReq = bytesReq / memCtl_p->granularity;
     if (bytesReq % memCtl_p->granularity)
     {
        blocksReq++;
     }

     /* ENSURE VALID SIZE AND BANK PARAMETERS AND A NON-NULL ALLOCATION LIST */
     if (!blocksReq)
     {
          if (!blocksReq)
          {
               PRINTK("n8_pmalloc: Invalid allocation size: %d\n",blocksReq);
          }
          return 0;
     }

     /* LOCK ALLOCATION LIST */
     N8_acquireProcessSem(&memCtl_p->allocationMapLock);

     /* LOCATE THE BEGINNING OF THIS BANK */
     memCtlPtr  = &memCtl_p->memCtlList[0];
     searchIndex = memCtl_p->nextIndex;
     searchCount = 0;

     /* Memory blocks are free if the numBlocks field is zero, The     */
     /* nextIndex is our best guess at the next freeBlock but is       */
     /* not guarenteed.                                                */
     while (searchCount < memCtl_p->maxEntries)
     {

       if (memCtlPtr[searchIndex].numBlocks == 0)
       {

          /* Handle the simple case of a single block first in order to */
          /* optimize the majority of the requests.                     */
          if (blocksReq == 1)
          {
              memCtlPtr[searchIndex].sessionID = sessionID;
              memCtlPtr[searchIndex].numBlocks = blocksReq;
              memCtlPtr[searchIndex].reqSize = bytesReq;

              /* Update the next search index with our best guess */
              memCtl_p->nextIndex = (searchIndex + 1) % memCtl_p->maxEntries;
              n8_atomic_add(memCtl_p->curAllocated, 1);

              /* Check high water mark to see if it needs updated. */
              if (n8_atomic_read(memCtl_p->curAllocated) > memCtl_p->maxAllocated)
              {
                   memCtl_p->maxAllocated = n8_atomic_read(memCtl_p->curAllocated);
              }

              N8_releaseProcessSem(&memCtl_p->allocationMapLock);
              physAddr = (unsigned long)(memCtl_p->memBankPtr + 
                                        (searchIndex * memCtl_p->granularity));
              return (physAddr);
          }

          /* We have a free block, check for enough continuous blocks to honor */
          /* this request.                                                     */
          freeBlocks = 1;

          while (((searchIndex + freeBlocks) < memCtl_p->maxEntries) &&
                  (!memCtlPtr[searchIndex + freeBlocks].numBlocks))
          {
             freeBlocks++;

             /* We have found a sufficient number of free blocks, return them */
             if (blocksReq == freeBlocks)
             {
                 /* Mark the first block with the sessionID & requested bytes */
                 memCtlPtr[searchIndex].sessionID = sessionID;
                 memCtlPtr[searchIndex].reqSize = bytesReq;

                 /* Mark the each block with allocated blocks */
                 for (indx = 0; indx < blocksReq; indx++)
                 {
                      memCtlPtr[searchIndex + indx].numBlocks = blocksReq;
                 }

                 /* Update the next search index with our best guess */
                 memCtl_p->nextIndex = (searchIndex + blocksReq) % memCtl_p->maxEntries;

                 /* Check high water mark to see if it needs updated. */
                 n8_atomic_add(memCtl_p->curAllocated, blocksReq);
                 if (n8_atomic_read(memCtl_p->curAllocated) > memCtl_p->maxAllocated)
                 {
                      memCtl_p->maxAllocated = n8_atomic_read(memCtl_p->curAllocated);
                 }
                 N8_releaseProcessSem(&memCtl_p->allocationMapLock);
                 physAddr = (unsigned long)(memCtl_p->memBankPtr + 
                                           (searchIndex * memCtl_p->granularity));
                 return (physAddr);
             }
          }
          searchIndex = (searchIndex + freeBlocks) % memCtl_p->maxEntries;
          searchCount += freeBlocks;
       }
       else
       {
	  /* Note - SearchCount must be changed BEFORE searchIndex is changed */
          searchCount += memCtlPtr[searchIndex].numBlocks;
          searchIndex = (searchIndex + memCtlPtr[searchIndex].numBlocks) % memCtl_p->maxEntries;
       }
     }
     N8_releaseProcessSem(&memCtl_p->allocationMapLock);

     /* Only log failure message on the first occurance */
     if (!memCtl_p->failedAllocs)
     {
        N8_PRINT(KERN_CRIT "NSP2000: kmalloc FAILURE unable to allocate %d blocks.\n", 
                                        blocksReq);
     }
     memCtl_p->failedAllocs++;
     return 0;

}



/*****************************************************************************
 * n8_pfree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief API - Releases a suballocation of the large allocated space.
 *
 * This routine returns the memory allocated by the physical address to the 
 * memory pool. The length is calculated so only the bank need be specified.
 *                                                                         
 *
 * @param bank  RO:  Specifies the bank from which the memory was allocated
 *                   from.
 * @param ptr   RO:  Void pointer to the physical base address of the chunk to
 *                   be returned to the memory bank.
 *
 * @par Externals:
 *    Debug_g  RO: Global flag that indicates whether debugging<BR>
 *                             messages are enabled.                       <BR>
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *
 * NOTE: THIS FUNCTION MAY BE CALLED FROM INTERRUPT CONTEXT and cannot take
 *       semaphores or print.
 *****************************************************************************/

void n8_pfree(int bank, void *ptr)
{

     MemCtlList_t* memCtlPtr;
     int           index;
     int           count;
     int           numBlocks;
#if (defined __KERNEL__) || (defined KERNEL) || (defined VX_BUILD)
     MemBankCtl_t *memCtl_p = memBankCtl_gp[bank];
#else
     MemBankCtl_t *memCtl_p = requestPoolCtl_gp;
#endif


     if (memCtl_p == NULL)
     {
        return;
     }

     memCtlPtr  = &memCtl_p->memCtlList[0];
     index = ((unsigned long)ptr - (int)memCtl_p->memBankPtr)/memCtl_p->granularity;

     /* Verify pointer is within the address range of our preallocated pool */
     if (((char *)ptr >= memCtl_p->memBankPtr) && (index < memCtl_p->maxEntries))
     {
          numBlocks = memCtlPtr[index].numBlocks;
          for (count = 0; count<numBlocks; count++)
          {
              memCtlPtr[index+count].sessionID = 0;
              memCtlPtr[index+count].numBlocks = 0;
          }
#if (defined __KERNEL__) || (defined KERNEL)
          n8_atomic_sub(memCtl_p->curAllocated, numBlocks);
#else
	  /* In user space there is no atomic operation available, so it  */
	  /* is necessary to take the semaphore to ensure the operation   */
	  /* is atomic. The n8_atomic_sub is an abstraction that merely   */
	  /* equates to integer subtraction in user space.                */
          N8_acquireProcessSem(&memCtl_p->allocationMapLock);
          n8_atomic_sub(memCtl_p->curAllocated, numBlocks);
          N8_releaseProcessSem(&memCtl_p->allocationMapLock);
#endif
     }

     return;
}
