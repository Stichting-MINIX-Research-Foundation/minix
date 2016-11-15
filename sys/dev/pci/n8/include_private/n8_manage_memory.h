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
 * @(#) n8_manage_memory.h 1.7@(#)
 *****************************************************************************/
        
/*****************************************************************************/
/** @file n8_manage_memory.h
 *  @brief Common definitions for NSP2000 Interface
 *
 * Common header file for memory management definitions for NSP2000 project
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/07/03 brr   Reduced functions exposed to user space.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 07/02/02 brr   Added structure for memory statistics.
 * 06/25/02 brr   Moved n8_memoryDisplay back to n8_memory.h.
 * 06/03/02 hml   initial version adapted from n8_memory.h
 ****************************************************************************/

#ifndef N8_MANAGE_MEMORY_H
#define N8_MANAGE_MEMORY_H

#if 0
#include "n8_pub_common.h"
#include "n8_pub_types.h"
#endif
#include "n8_OS_intf.h"

typedef struct 
{
     int sessionID; /* 0 if free/non-zero process ID of allocater */
     int numBlocks; /* Number of contiguous blocks in this allocation */
     int reqSize;   /* requested size (as opposed to allocated size ) */

} MemCtlList_t;

/**********************************************************
  The MemBankCtl_t structure describes everything about a 
  contiguous memory area that has been allocated by the 
  driver.

  memBaseAddress-> ////////////////////////////
                   /                          /
                   /  MemBankCtl_t structure  /
                   /                          /
       memCtlPtr-> ////////////////////////////
                   /                          /
                   /  Allocation Map          /
                   /                          /
      memBankPtr-> ////////////////////////////
                   /                          /
                   /                          /
                   / Allocatable memory       /
                   /                          /
                   /                          /
                   /                          /
                   ////////////////////////////

 **********************************************************/
typedef struct
{
     unsigned long memBaseAddress;     /* base phys address of entire block */
     char         *memBankPtr;         /* phys pointer to allocatable memory */
     int           allocSize;          /* Size of entire block */
     int           nextIndex;          /* next index to check */
     n8_atomic_t   curAllocated;       /* current total blocks allocated */
     int           maxAllocated;       /* max allocated since creation */
     int           failedAllocs;       /* failed alloc since creation */
     int           maxEntries;         /* number of blocks created */
     unsigned long granularity;        /* size of each block in bytes */
     unsigned int  bankIndex;          /* The index of this bank */
     n8_Lock_t     allocationMapLock;  /* lock for this structure */
     MemCtlList_t  memCtlList[0];       /* allocation map */
} MemBankCtl_t;

typedef struct 
{
     int     sessionID;          /* Process ID of the allocater */
     int     curAllocated;       /* current number of blocks allocated */
     int     maxAllocated;       /* max allocated since creation */
     int     failedAllocs;       /* failed alloc since creation */
     int     maxEntries;         /* number of blocks in this pool */

} MemStats_t;

unsigned long n8_pmalloc(int           bank, 
                         int           bytesReq, 
                         unsigned long sessionID);
void  n8_pfree(int  bank, void *ptr);

#endif /* N8_MANAGE_MEMORY_H */

