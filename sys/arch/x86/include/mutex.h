/*	$NetBSD: mutex.h,v 1.6 2009/04/24 17:49:51 ad Exp $	*/

/*-
 * Copyright (c) 2002, 2006, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _X86_MUTEX_H_
#define	_X86_MUTEX_H_

struct kmutex {
	union {
		volatile uintptr_t	mtxa_owner;
#ifdef __MUTEX_PRIVATE
		struct {
			volatile uint8_t	mtxs_dummy;
			ipl_cookie_t		mtxs_ipl;
                        __cpu_simple_lock_t	mtxs_lock;
			volatile uint8_t	mtxs_unused;
		} s;
#endif
	} u;
};

#ifdef __MUTEX_PRIVATE

#define	mtx_owner 			u.mtxa_owner
#define	mtx_ipl 			u.s.mtxs_ipl
#define	mtx_lock			u.s.mtxs_lock

#define __HAVE_MUTEX_STUBS		1
#define __HAVE_SPIN_MUTEX_STUBS		1
#define	__HAVE_SIMPLE_MUTEXES		1

/*
 * MUTEX_RECEIVE: technically, no memory barrier is required
 * as 'ret' implies a load fence.  However we need this to
 * handle a bug with some Opteron revisions.  See patch.c,
 * lock_stubs.S.
 */
#define	MUTEX_RECEIVE(mtx)		membar_consumer()

/*
 * MUTEX_GIVE: no memory barrier required, as _lock_cas() will take care of it.
 */
#define	MUTEX_GIVE(mtx)			/* nothing */

#define	MUTEX_CAS(p, o, n)		\
    (_atomic_cas_ulong((volatile unsigned long *)(p), (o), (n)) == (o))

unsigned long	_atomic_cas_ulong(volatile unsigned long *,
    unsigned long, unsigned long);

#endif	/* __MUTEX_PRIVATE */

#endif /* _X86_MUTEX_H_ */
