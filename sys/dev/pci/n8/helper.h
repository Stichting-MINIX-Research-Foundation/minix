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
 * @(#) helper.h 1.6@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file helper.h                                                           *
 *  @brief FreeBSD System Call Abstraction - Header.                         *
 *                                                                           *
 * This file provides the FreeBSD implementations of various abstract system *
 * calls and defined constants.                                              *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/12/03 brr   Added N8_GET_KERNEL_ID. (Bug 863)
 * 05/05/03 brr   Removed unused #defines.
 * 04/21/03 brr   Added vmalloc/vfree macros for context memory allocation.
 * 01/06/03 brr/jpw Define N8_GET_SESSION_ID to use process ID, not group.
 * 10/25/02 brr   File created.
 ****************************************************************************/
/** @defgroup NSP2000Driver FreeBSD System Call Abstraction - Header.
 */


#ifndef _HELPER_H
#define _HELPER_H

#ifndef HZ
#define HZ hz
#endif

#if 0
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/ioccom.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include "n8_enqueue_common.h"
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include "n8_enqueue_common.h"

/* Macros for copying data to and from user space */
#define N8_TO_USER(_user,_kernel,_size)   copyout((_kernel), (_user), (_size))
#define N8_FROM_USER(_kernel,_user,_size) copyin((_user), (_kernel), (_size))

#define vmalloc(size)  malloc(size, M_DEVBUF, M_WAITOK)
#define vfree(ptr)     free(ptr, M_DEVBUF)


/* ABSTRACT ATOMIC RESOURCE LOCK */
/* typedef int ATOMICLOCK_t; */
typedef __cpu_simple_lock_t ATOMICLOCK_t;
#define N8_AtomicLock(x)     __cpu_simple_lock(&x)
#define N8_AtomicUnlock(x)   __cpu_simple_unlock(&x)
#define N8_AtomicLockInit(x) __cpu_simple_lock_init(&x)
#define N8_AtomicLockDel(x)

/* ABSTRACT BLOCKING MECHANISM */
#define wait_queue_head_t	atomic_t
#define init_waitqueue_head(A)
typedef int n8_WaitQueue_t;
typedef unsigned char    wait_queue_head_t;
#define WakeUp(A)            wakeup(A);  
#define N8_InitWaitQueue(q_p)

extern int N8_WaitEventInt(n8_WaitQueue_t *, API_Request_t *);
#define N8_DelWaitQueue(q_p)


/* DEBUG MESSAGE DEFINES */
#define KERN_CRIT

#define N8_GET_SESSION_ID  curproc->p_pid
#define N8_GET_KERNEL_ID  0xdead0000

/*****************************************************************************
 * N8_PhysToVirt
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Convert a physical address to a virtual address.
 *
 * This routine abstracts the BSDi system call to convert a physical address
 * to a virtual address.
 *
 * @param physaddr   RO:  Specifies the physical address.
 *
 * @par Externals:
 *    N/A
 *
 * @return 
 *    Returns the corresponding virtual address.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern void *N8_PhysToVirt(unsigned long physaddr);



/*****************************************************************************
 * N8_VirtToPhys
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Convert a virtual address to a physical address.
 *
 * This routine abstracts the BSDi system call to convert a virtual address
 * to a physical address.
 *
 * @param virtaddr   RO:  Specifies the physical address.
 *
 * @par Externals:
 *    N/A
 *
 * @return 
 *    Returns the corresponding physical address.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

extern unsigned long N8_VirtToPhys(void *virtaddr);

int n8_bounds_check(unsigned long phys);


#endif  /* HELPER_H */


