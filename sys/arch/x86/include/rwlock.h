/*	$NetBSD: rwlock.h,v 1.5 2008/04/28 20:23:40 martin Exp $	*/

/*-
 * Copyright (c) 2002, 2006 The NetBSD Foundation, Inc.
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

#ifndef _X86_RWLOCK_H_
#define	_X86_RWLOCK_H_

struct krwlock {
	volatile uintptr_t	rw_owner;
};

#ifdef __RWLOCK_PRIVATE

#define	__HAVE_SIMPLE_RW_LOCKS		1
#define	__HAVE_RW_STUBS			1

/*
 * RW_RECEIVE: no memory barrier required, as 'ret' implies a load fence. 
 */
#define	RW_RECEIVE(rw)			/* nothing */

/*
 * RW_GIVE: no memory barrier required, as _lock_cas() will take care of it.
 */
#define	RW_GIVE(rw)			/* nothing */

#define	RW_CAS(p, o, n)			\
    (_atomic_cas_ulong((volatile unsigned long *)(p), (o), (n)) == (o))

unsigned long	_atomic_cas_ulong(volatile unsigned long *,
    unsigned long, unsigned long);

#endif	/* __RWLOCK_PRIVATE */

#endif /* _X86_RWLOCK_H_ */
