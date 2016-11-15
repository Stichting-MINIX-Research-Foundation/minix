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
 * @(#) n8_semaphore.h 1.10@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_semaphore.h
 *  @brief The include file for the n8 semaphore 
 *         implementation.
 *
 *  This file contains the prototypes for the semaphore implementation.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 11/25/02 brr   Added prototype for n8_delete_process_init_sem.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/11/02 hml   Deleted unused protos and added the processSem operations.
 * 12/20/01 bac   Added N8_SKS_SEM_BASE.  Bug #436.
 * 10/29/01 msz   Added N8_BNC_SEM_BASE and N8_BM_SEM_BASE
 * 10/08/01 msz   Added N8_UPDATE_ALL_SEM_BASE.
 * 08/16/01 bac   Fixed a non-terminated comment.
 * 08/15/01 brr   Added VxWorks support.
 * 08/08/01 msz   Moved semun to n8_OS_intf.h
 * 08/08/01 hml   Completed the set of prototypes for version 1.1 and added 
 *                some defines to be the base keys for semaphore creation.
 * 08/02/01 hml   Added prototypes for the process level, system level and PCI
 *                level semaphores.
 * 06/13/01 hml   Original version.
 ****************************************************************************/
#ifndef _N8_SEMAPHORE_H
#define _N8_SEMAPHORE_H

#include "n8_pub_errors.h"
#include "n8_common.h"
#include "n8_OS_intf.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/ 
#define N8_CONTEXT_SEM_BASE     0xbabe1000
#define N8_QUEUE_SEM_BASE       0xbabe1200
#define N8_HARDWARE_SEM_BASE    0xbabe1300
#define N8_UPDATE_ALL_SEM_BASE  0xbabe1400
#define N8_BNC_SEM_BASE         0xbabe1500
#define N8_BM_SEM_BASE          0xbabe1600
#define N8_SKS_SEM_BASE         0xbabe1700

/* Note: the N8_QUEUE_SEM_BASE and N8_HARDWARE_SEM_BASE values make an  */
/* assumption that there are at most 85 control sets (because           */
/* N8_NUM_COMPONENTS = 3, and 256/3 = 85).  And it is assumed that the  */
/* each value will have x100 between them.                              */

/*****************************************************************************/

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
void
n8_create_process_init_sem(void);

void
n8_delete_process_init_sem(void);

void
n8_acquire_process_init_sem(void);

void
n8_release_process_init_sem(void);

N8_Status_t
n8_get_system_sem(N8_SemKey_t semKey, int initValue, N8_SystemSem_t *handle_p);

N8_Status_t
n8_acquire_system_sem(N8_SystemSem_t semID);

N8_Status_t
n8_release_system_sem(N8_SystemSem_t semID);

/* These are used in both kernel and user space for locking the allocation map.
   In user space these will be pthread calls.  In kernel space they resolve to
   the Atomic lock calls. */
void
N8_initProcessSem(n8_Lock_t *lock_p);

void
N8_deleteProcessSem(n8_Lock_t *lock_p);

void
N8_acquireProcessSem(n8_Lock_t *lock_p);

void
N8_releaseProcessSem(n8_Lock_t *lock_p);
#endif
