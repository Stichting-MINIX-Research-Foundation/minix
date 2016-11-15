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
 * @(#) n8_malloc_common.h 1.24@(#)
 *****************************************************************************/
/*****************************************************************************/
/** @file n8_malloc_common.h
 *  @brief This file contains portability macros for user space and kernel 
 *  space memory allocation.
 *
 * This file provides a portable set of macros for allocating and freeing
 * memory blocks in both user and kernel space.  The memory blocks are 
 * zero filled on allocation.  The pointers to the memory blocks are set
 * to NULL on free.  A free of a NULL pointer generates a lowest priority
 * message using the N8_Debug function.  Platforms currently supported are
 *
 *    Behavioral Model
 *    Linux
 *    Solaris
 *
 * To support a new platform, execute the following command in a directory
 * where you have write permission:
 *
 *    touch foo.h; cpp -dM foo.h
 *
 * This will show all of the preprocesser defined macros (such as __linux__). 
 * Pick one of these macros to use as the new platform ifdef.
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/11/02 hml   Added externs for the memroy bank control structs.
 * 03/26/02 brr   Added Buffer states, dropped unused parameters from N8_KMALLOC.
 * 03/08/02 brr   Added size to N8_UmallocHdr_t.
 * 02/18/02 brr   Removed all kernel specific references, reconciled function
 *                name discrepancies with n8_driver_api.c.
 * 02/14/02 brr   Reconcile 2.0 memory management changes.
 * 01/30/02 brr   Removed old definition of KMALLOC.
 * 01/23/02 brr   Modifed N8_UMALLOC/FREE macros to call QM_calloc/free. 
 * 01/21/02 bac   Changes to support new kernel allocation scheme.
 * 01/12/02 bac   Changed the call to QM_AllocateBuffer to match new signature,
 *                which now includes the __FILE__ and __LINE__ values.
 * 01/16/02 brr   Changed N8_UMALLOC/FREE macros to call n8 functions.
 * 01/14/02 brr   Check size before performing vmalloc.
 * 01/14/02 hml   Added a memset to the N8_UMALLOC macro in kernel build.
 * 01/10/01 hml   Updated for kernel/user space environment.
 * 01/08/01 brr   Define N8_UMALLOC conditionally base on OS.
 * 12/18/01 brr   Removed obsolete bitflags from KMALLOC.
 * 09/14/01 bac   Added macro definition for N8_KMEM_DEFAULT_BITFIELDS which
 *                specifies the default flags for allocating unshared kernel
 *                memory.  Changed the macro N8_KMALLOC to use these defaults
 *                when calling QM_AllocateBuffer.
 * 07/30/01 bac   Removed dead code for U/KMALLOC definitions.
 * 06/22/01 bac   Added include of QMMemory.h
 * 06/15/01 bac   Changed N8_KMALLOC/N8_KFREE
 * 05/09/01 bac   Fixed some compilation bugs.
 * 05/03/01 HML   Original version.
 ****************************************************************************/
#ifndef N8_MALLOC_COMMON_H
#define N8_MALLOC_COMMON_H

#include "n8_pub_common.h"
#include "n8_pub_errors.h"
#include "n8_OS_intf.h"
#include "n8_manage_memory.h"

typedef struct {
   void *nextPtr;
   int   size;
} N8_UmallocHdr_t;

#if defined(__KERNEL__)
     #include <asm/io.h>
#endif

extern void *n8_vmalloc(unsigned long size);
extern void  n8_vfree(void *memPtr);

#define N8_BUFFER_HEADER_SIZE  32

/* The following defines are the possible states of a KMALLOC'ed buffer */
#define N8_BUFFER_NOT_QUEUED   0  /* Buffer is not on a command Queue */
#define N8_BUFFER_QUEUED       1  /* Buffer has been placed on a command Queue */
#define N8_BUFFER_SESS_DELETED 2  /* Buffer has been placed on a command Queue */
                                  /* but the process has terminated.           */

/* User space malloc and free macros. Note these are identical regardless 
   of platform. */

#define N8_UMALLOC(SIZE)   n8_vmalloc((unsigned long)SIZE)
#define N8_UFREE(BUF)      n8_vfree((void *)BUF)

#define N8_KMALLOC(SIZE) N8_AllocateBuffer(SIZE)
#define N8_KMALLOC_PK(SIZE) N8_AllocateBufferPK(SIZE)
#define N8_KFREE(MEM) N8_FreeBuffer(MEM)

#endif /* N8_MALLOC_COMMON_H */
