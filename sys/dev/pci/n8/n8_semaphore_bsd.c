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
 * @(#) n8_semaphore_bsd.c 1.2@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_semaphore_bsd.c
 *  @brief This file implements the n8 semaphore operations for BSD kernel
 *         environment.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/02/02 brr   Added n8_delete_process_init_sem function.
 * 06/12/02 hml   Implemented the processSem ops.  They are all empty for BSD.
 * 03/18/02 brr   Original version.
 ****************************************************************************/
/** @defgroup n8_semaphore NetOctave Semaphore Operations
 */

#include "helper.h"
#include "n8_pub_errors.h"
#include "n8_semaphore.h"                             

static int          initComplete = FALSE;
static ATOMICLOCK_t initProcessMutex_g = SIMPLELOCK_INITIALIZER;

/*****************************************************************************
 * n8_acquire_process_init_sem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Acquire the statically defined initilization semaphore.
 *
 *  This function is the POSIX implementation of a function that grabs
 *  the statically allocated initialization semaphore.  It turns out this
 *  is needed in order to prevent any race conditions on initialization.
 *
 * @par Externals:
 *    initProcessMutex_g  RW: The global process initialization mutex.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
n8_acquire_process_init_sem()
{
   N8_AtomicLock(initProcessMutex_g);
}

/*****************************************************************************
 * n8_release_process_init_sem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Release the statically defined initilization semaphore.
 *
 *  This function is the POSIX implementation of a function that releases
 *  the statically allocated initialization semaphore.  It turns out this
 *  is needed in order to prevent any race conditions on initialization.
 *
 * @par Externals:
 *    initProcessMutex_g  RW: The global process initialization mutex.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
n8_release_process_init_sem()
{
   N8_AtomicUnlock(initProcessMutex_g);
}

/*****************************************************************************
 * n8_create_process_init_sem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Creates the process initialization semaphore
 *
 *        This function allocates a kernel semaphore,
 *
 * @return 
 *
 * @par Errors:
 *
 * @par Locks:
 *    None.
 *    A text description of locks required by this routine and
 *    locks aquired by this routine.  Optional if there are no locks.
 *
 * @par Assumptions:
 *    initVal is a sane number
 *****************************************************************************/

void
n8_create_process_init_sem(void)
{
   if (initComplete == FALSE)
   {
       N8_AtomicLockInit(initProcessMutex_g);
       initComplete = TRUE;
   }
}

/*****************************************************************************
 * n8_delete_process_init_sem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Deletes the process initialization semaphore
 *
 *        This function deletes a kernel semaphore,
 *
 * @return
 *
 * @par Errors:
 *
 * @par Locks:
 *    None.
 *    A text description of locks required by this routine and
 *    locks aquired by this routine.  Optional if there are no locks.
 *
 * @par Assumptions:
 *    initVal is a sane number
 *****************************************************************************/

void
n8_delete_process_init_sem(void)
{
   N8_AtomicLockDel(initProcessMutex_g);
   initComplete = FALSE;
}

/*****************************************************************************
 * N8_acquireProcessSem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Acquire a lock of type N8_Lock_t.
 *
 *  This function is the kernel space implementation of a function that grabs
 *  a lock of type N8_Lock_t. Does nothing in BSD.
 * 
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
N8_acquireProcessSem(n8_Lock_t *lock_p)
{
}

/*****************************************************************************
 * N8_releaseProcessSem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Release a lock of type N8_Lock_t.
 *
 *  This function is the kernel space implementation of a function that releases
 *  a lock of type N8_Lock_t. Does nothing in BSD.
 * 
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
N8_releaseProcessSem(n8_Lock_t *lock_p)
{
}

/*****************************************************************************
 * N8_initProcessSem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Initialize a lock of type N8_Lock_t.
 *
 *  This function is the kernel space implementation of a function that 
 *  initializes a lock of type N8_Lock_t. Does nothing in BSD.
 * 
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
N8_initProcessSem(n8_Lock_t *lock_p)
{
}

/*****************************************************************************
 * N8_deleteProcessSem
 *****************************************************************************/
/** @ingroup n8_semaphore
 * @brief Initialize a lock of type N8_Lock_t.
 *
 *  This function is the kernel space implementation of a function that 
 *  deletes a lock of type N8_Lock_t. Does nothing in BSD.
 * 
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
void
N8_deleteProcessSem(n8_Lock_t *lock_p)
{
}



