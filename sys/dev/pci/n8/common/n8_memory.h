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
 * @(#) n8_memory.h 1.10@(#)
 *****************************************************************************/
/*****************************************************************************/
/** @file n8_memory.h
 *  @brief This file contains prototypes for kernel space memory allocation.
 *
 * This file provides a prototypes for function used to allocate and free
 * memory blocks kernel space.  The memory blocks are zero filled on allocation. 
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history: 
 * 05/08/03 brr   Modified user pools to take advantage of the support for
 *                multiple memory banks.
 * 04/22/03 brr   Removed redundant parameter from n8_FreeLargeAllocation.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 11/26/02 brr   Updated prototypes for n8_memoryInit & n8_memoryRelease.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 07/02/02 brr   Added prototype for N8_QueryMemStatistics.
 * 06/25/02 brr   Rework user pool allocation to only mmap portion used by
 *                the individual process.
 * 06/11/02 hml   Added protos for N8_get/FreeLargeAllocation.
 * 05/22/02 hml   Added MemCtlList_t and MemBankCtl_t structures and 
 *                updated prototypes.
 * 03/18/02 brr   Add sessionID to allocation & free prototypes.
 * 02/14/02 brr   File created.
 ****************************************************************************/
#ifndef N8_MEMORY_H
#define N8_MEMORY_H

#include "n8_OS_intf.h"
#include "n8_manage_memory.h"

#define N8_ONE_MEGABYTE         1048576
#define N8_ONE_KILOBYTE            1024

typedef enum
{
   POOL_FREE                     = 0,
   POOL_ALLOCATED                = 1,
   POOL_DELETED                  = 2
} n8_UserPoolState_t;

/* The memory control structure for the basic pool */
extern MemBankCtl_t  *memBankCtl_gp[];

void  n8_memoryDebug(int enable);
void  n8_memoryDisplay(N8_MemoryType_t bank);
void *n8_memoryInit   (N8_MemoryType_t bank,
                       unsigned long   size,
                       unsigned long   granularity);
void  n8_memoryRelease(N8_MemoryType_t bank);

void  n8_pfreeSessID  (N8_MemoryType_t bank,
                       unsigned long   SessionID);

unsigned long n8_GetLargeAllocation     ( N8_MemoryType_t bankIndex,
                                          unsigned long   size,
                                          unsigned char   debug );
void          n8_FreeLargeAllocation    ( N8_MemoryType_t bankIndex,
                                          unsigned char   debug );

#endif /* N8_MEMORY_H */
