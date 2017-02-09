/*	$NetBSD: lock_stubs.S,v 1.26 2014/01/12 20:31:50 joerg Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Where possible we make each routine fit into an assumed 64-byte cache
 * line.  Please check alignment with 'objdump -d' after making changes. 
 */

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: lock_stubs.S,v 1.26 2014/01/12 20:31:50 joerg Exp $");

#include "opt_lockdebug.h"

#include <machine/cputypes.h>

#include "assym.h"

#define	ALIGN64		.align	64
#define	ALIGN32		.align	32
#define	LOCK(num)	.Lpatch ## num:	lock
#define	RET(num)	.Lret ## num: ret; nop; nop; ret
#define	ENDLABEL(name,a) .align	a; LABEL(name)

#if !defined(LOCKDEBUG)

/*
 * void mutex_enter(kmutex_t *mtx);
 *
 * Acquire a mutex and post a load fence.
 */
	ALIGN64

ENTRY(mutex_enter)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	movl	%fs:CPU_INFO_CURLWP(%eax), %ecx
	LOCK(1)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	RET(1)
1:
	jmp	_C_LABEL(mutex_vector_enter)
END(mutex_enter)

/*
 * void mutex_exit(kmutex_t *mtx);
 *
 * Release a mutex and post a load fence.
 *
 * See comments in mutex_vector_enter() about doing this operation unlocked
 * on multiprocessor systems, and comments in arch/x86/include/lock.h about
 * memory ordering on Intel x86 systems.
 */
ENTRY(mutex_exit)
	movl	4(%esp), %edx
	xorl	%ecx, %ecx
	movl	%fs:CPU_INFO_CURLWP(%ecx), %eax
	cmpxchgl %ecx, (%edx)
	jnz	1f
	ret
1:
	jmp	_C_LABEL(mutex_vector_exit)
END(mutex_exit)

/*
 * void rw_enter(krwlock_t *rwl, krw_t op);
 *
 * Acquire one hold on a RW lock.
 */
ENTRY(rw_enter)
	movl	4(%esp), %edx
	cmpl	$RW_READER, 8(%esp)
	jne	2f

	/*
	 * Reader
	 */
	movl	(%edx), %eax
0:
	testb	$(RW_WRITE_LOCKED|RW_WRITE_WANTED), %al
	jnz	3f
	leal	RW_READ_INCR(%eax), %ecx
	LOCK(2)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	RET(2)
1:
	jmp	0b

	/*
	 * Writer
	 */
2:	xorl	%eax, %eax
	movl	%fs:CPU_INFO_CURLWP(%eax), %ecx
	orl	$RW_WRITE_LOCKED, %ecx
	LOCK(3)
	cmpxchgl %ecx, (%edx)
	jnz	3f
	RET(3)
3:
	jmp	_C_LABEL(rw_vector_enter)
END(rw_enter)

/*
 * void rw_exit(krwlock_t *rwl);
 *
 * Release one hold on a RW lock.
 */
ENTRY(rw_exit)
	movl	4(%esp), %edx
	movl	(%edx), %eax
	testb	$RW_WRITE_LOCKED, %al
	jnz	2f

	/*
	 * Reader
	 */
0:	testb	$RW_HAS_WAITERS, %al
	jnz	3f
	cmpl	$RW_READ_INCR, %eax
	jb	3f
	leal	-RW_READ_INCR(%eax), %ecx
	LOCK(4)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	ret
1:
	jmp	0b

	/*
	 * Writer
	 */
2:	leal	-RW_WRITE_LOCKED(%eax), %ecx
	subl	CPUVAR(CURLWP), %ecx
	jnz	3f
	LOCK(5)
	cmpxchgl %ecx, (%edx)
	jnz	3f
	ret

	/*
	 * Slow path.
	 */
3:	jmp	_C_LABEL(rw_vector_exit)
END(rw_exit)

/*
 * int rw_tryenter(krwlock_t *rwl, krw_t op);
 *
 * Try to acquire one hold on a RW lock.
 */
ENTRY(rw_tryenter)
	movl	4(%esp), %edx
	cmpl	$RW_READER, 8(%esp)
	jne	2f

	/*
	 * Reader
	 */
	movl	(%edx), %eax
0:
	testb	$(RW_WRITE_LOCKED|RW_WRITE_WANTED), %al
	jnz	4f
	leal	RW_READ_INCR(%eax), %ecx
	LOCK(12)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	movl	%edx, %eax			/* nonzero */
	RET(4)
1:
	jmp	0b

	/*
	 * Writer
	 */
2:
	xorl	%eax, %eax
	movl	%fs:CPU_INFO_CURLWP(%eax), %ecx
	orl	$RW_WRITE_LOCKED, %ecx
	LOCK(13)
	cmpxchgl %ecx, (%edx)
	movl	$0, %eax
	setz	%al
3:
	RET(5)
4:
	xorl	%eax, %eax
	jmp	3b
END(rw_tryenter)

#ifndef __XEN__

/*
 * void mutex_spin_enter(kmutex_t *mtx);
 *
 * Acquire a spin mutex and post a load fence.
 */
ENTRY(mutex_spin_enter)
	movl	4(%esp), %edx
	movb	CPUVAR(ILEVEL), %cl
	movb	MTX_IPL(%edx), %ch
	movl	$0x01, %eax
	cmpb	%ch, %cl
	jg	1f
	movb	%ch, CPUVAR(ILEVEL)		/* splraiseipl() */
1:
	subl	%eax, CPUVAR(MTX_COUNT)		/* decl does not set CF */
	jnc	2f
	movb	%cl, CPUVAR(MTX_OLDSPL)
2:
	xchgb	%al, MTX_LOCK(%edx)		/* lock it */
	testb	%al, %al
	jnz	3f
	RET(6)
3:
	jmp	_C_LABEL(mutex_spin_retry)

	ALIGN64
LABEL(mutex_spin_enter_end)
END(mutex_spin_enter)

/*
 * Release a spin mutex and post a store fence.
 */
ENTRY(mutex_spin_exit)
	movl	4(%esp), %edx
	movl	CPUVAR(MTX_OLDSPL), %ecx
	incl	CPUVAR(MTX_COUNT)
	movb	$0, MTX_LOCK(%edx)		/* zero */
	jnz	1f
	movl	%fs:CPU_INFO_IUNMASK(,%ecx,4), %edx
	cli
	testl	CPUVAR(IPENDING), %edx
	movl    %ecx, 4(%esp)
	jnz	_C_LABEL(Xspllower)		/* does sti */
	movl	%ecx, CPUVAR(ILEVEL)
	sti
1:	ret
	.space 32
	.align	32
LABEL(mutex_spin_exit_end)
END(mutex_spin_exit)

/*
 * Patch for i686 CPUs where cli/sti is prohibitively expensive.
 * Must be the same size as mutex_spin_exit().
 */
ENTRY(i686_mutex_spin_exit)
	mov	4(%esp),%edx
	movl	CPUVAR(MTX_OLDSPL), %ecx
	incl	CPUVAR(MTX_COUNT)
	movb	%ch, MTX_LOCK(%edx)		/* zero */
	jnz	1f
	pushl	%ebx
0:
	movl	CPUVAR(IPENDING), %eax
	testl	%eax, %fs:CPU_INFO_IUNMASK(,%ecx,4)
	jnz	2f
	movl	%eax, %ebx
	cmpxchg8b CPUVAR(ISTATE)		/* swap in new ilevel */
	jnz	0b
	popl	%ebx
1:
	ret
2:
	popl	%ebx
	movl	%ecx,4(%esp)
LABEL(i686_mutex_spin_exit_patch)
	jmp	_C_LABEL(Xspllower)
	.space 16
	.align	32
LABEL(i686_mutex_spin_exit_end)
END(i686_mutex_spin_exit)

#else	/* !__XEN__ */

/* For now; strong alias not working for some reason. */
ENTRY(mutex_spin_enter)
	jmp	_C_LABEL(mutex_vector_enter)

ENTRY(mutex_spin_exit)
	jmp	_C_LABEL(mutex_vector_exit)

#endif	/* !__XEN__ */

#endif	/* !LOCKDEBUG */

/*
 * Spinlocks.
 */
ENTRY(__cpu_simple_lock_init)
	movl	4(%esp), %edx
	movb	$0, (%edx)
	ret
END(__cpu_simple_lock_init)

NENTRY(__cpu_simple_lock)
	movl	4(%esp), %edx
	movl	$0x0100, %eax
1:
	LOCK(6)
	cmpxchgb %ah, (%edx)
	jnz	2f
	RET(7)
2:
	movl	$0x0100, %eax
	pause
	nop
	nop
	cmpb	$0, (%edx)
	je	1b
	jmp	2b
END(__cpu_simple_lock)

ENTRY(__cpu_simple_unlock)
	movl	4(%esp), %edx
	movb	$0, (%edx)
	ret
END(__cpu_simple_unlock)

ENTRY(__cpu_simple_lock_try)
	movl	4(%esp), %edx
	movl	$0x0100, %eax
	LOCK(7)
	cmpxchgb %ah, (%edx)
	movl	$0, %eax
	setz	%al	
	RET(8)
END(__cpu_simple_lock_try)

/*
 * Patchpoints to replace with NOP when ncpu == 1.
 */
	.data

#ifndef LOCKDEBUG
LABEL(x86_lockpatch)
	.long	.Lpatch1, .Lpatch2, .Lpatch3, .Lpatch4
	.long	.Lpatch5, .Lpatch6, .Lpatch7, .Lpatch12
	.long	.Lpatch13
	.long	0
#endif

LABEL(x86_retpatch)
#ifndef LOCKDEBUG
	.long	.Lret1, .Lret2, .Lret3, .Lret4, .Lret5
#ifndef __XEN__
	.long	.Lret6
#endif
#endif
	.long	.Lret7, .Lret8
	.long	0
