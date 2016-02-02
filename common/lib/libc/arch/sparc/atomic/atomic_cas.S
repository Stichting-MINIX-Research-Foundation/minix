/*	$NetBSD: atomic_cas.S,v 1.13 2014/02/21 16:21:02 martin Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran and Jason R. Thorpe.
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

#include <sys/param.h>
#include "atomic_op_asm.h"

#if defined(_HARDKERNEL)

#include <machine/psl.h>

#include "opt_multiprocessor.h"

#define	DISABLE_INTERRUPTS						 \
	rd	%psr, %o4			/* disable interrupts */;\
	or	%o4, PSR_PIL, %o5					;\
	wr	%o5, 0, %psr						;\
	nop								;\
	nop								;\
	nop

#define	RESTORE_INTERRUPTS						 \
	wr	%o4, 0, %psr			/* enable interrupts */	;\
	nop								;\
	nop								;\
	nop

#else	/* _HARDKERNEL */

#define	MULTIPROCESSOR	1
#define	DISABLE_INTERRUPTS		/* nothing */
#define	RESTORE_INTERRUPTS		/* nothing */

#endif	/* _HARDKERNEL */

#if defined(MULTIPROCESSOR)
	.section .bss
	.align	1024
#ifdef __PIC__
	.globl _C_LABEL(_atomic_cas_locktab)
#endif
OTYPE(_C_LABEL(_atomic_cas_locktab))
_C_LABEL(_atomic_cas_locktab):
	.space	1024

#ifdef __PIC__
/* o4 is not used for PSR in PIC cases, so we can use it here */
#define GETLOCKTAB							\
	PIC_PROLOGUE(%o3,%o4)						;\
	set _C_LABEL(_atomic_cas_locktab), %o4				;\
	ld [%o3 + %o4], %o3
#else
#define GETLOCKTAB							\
	sethi	%hi(_C_LABEL(_atomic_cas_locktab)), %o3
#endif

#define	ACQUIRE_INTERLOCK						 \
	DISABLE_INTERRUPTS						;\
	srl	%o0, 3, %o5			/* get lock address */	;\
	and	%o5, 1023, %o5						;\
	GETLOCKTAB							;\
	add	%o5, %o3, %o5						;\
									;\
	/* %o5 has interlock address */					;\
									;\
1:	ldstub	[%o5], %o3			/* acquire lock */	;\
	tst	%o3							;\
	bz,a	2f							;\
	 nop								;\
	nop								;\
	nop								;\
	b,a	1b				/* spin	*/		;\
	 nop								;\
	/* We now hold the interlock */					;\
2:

#define	RELEASE_INTERLOCK						 \
	stb	%g0, [%o5]			/* release interlock */	;\
	RESTORE_INTERRUPTS

#else /* ! MULTIPROCESSOR */

#define	ACQUIRE_INTERLOCK	DISABLE_INTERRUPTS

#define	RELEASE_INTERLOCK	RESTORE_INTERRUPTS

#endif /* MULTIPROCESSOR */

	.text

/*
 * The v7 and v8 SPARC doesn't have compare-and-swap, so we block interrupts
 * and use an interlock.
 *
 * XXX On single CPU systems, this should use a restartable sequence:
 * XXX there we don't need the overhead of interlocking.
 *
 * XXX NOTE!  The interlock trick only works if EVERYTHING writes to
 * XXX the memory cell through this code path!
 */
ENTRY(_atomic_cas_32)
	ACQUIRE_INTERLOCK
	! %o4 has saved PSR value
	! %o5 has interlock address

	ld	[%o0], %o3			! get old value
	cmp	%o1, %o3			! old == new?
	beq,a	3f				! yes, do the store
	 st	%o2, [%o0]			! (in the delay slot)

3:	RELEASE_INTERLOCK

	retl
	 mov	%o3, %o0			! return old value

ATOMIC_OP_ALIAS(atomic_cas_32,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_32)
STRONG_ALIAS(__sync_val_compare_and_swap_4,_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_32_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_32)
