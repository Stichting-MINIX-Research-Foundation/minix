/*	$NetBSD: lock_stubs.S,v 1.38 2022/09/13 05:36:29 knakahara Exp $	*/

/*
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

#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"

#include <machine/asm.h>
#include <machine/frameasm.h>

#include "assym.h"

#define LOCK	\
	HOTPATCH(HP_NAME_NOLOCK, 1)	; \
	lock
#define RET	\
	HOTPATCH(HP_NAME_RETFENCE, 3)	; \
	ret; nop; nop			; \
	ret

#ifndef LOCKDEBUG

	.align	64

/*
 * void mutex_enter(kmutex_t *mtx);
 *
 * Acquire a mutex and post a load fence.
 */
ENTRY(mutex_enter)
	movq	CPUVAR(CURLWP), %rcx
	xorq	%rax, %rax
	LOCK
	cmpxchgq %rcx, (%rdi)
	jnz	1f
	RET
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
	movq	CPUVAR(CURLWP), %rax
	xorq	%rdx, %rdx
	cmpxchgq %rdx, (%rdi)
	jnz	1f
	ret
1:
	jmp	_C_LABEL(mutex_vector_exit)
END(mutex_exit)

/*
 * void mutex_spin_enter(kmutex_t *mtx);
 *
 * Acquire a spin mutex and post a load fence.
 */
ENTRY(mutex_spin_enter)
	movl	$1, %eax
	movzbl	CPUVAR(ILEVEL), %esi
	movzbl	MTX_IPL(%rdi), %ecx		/* new SPL */
	cmpl	%ecx, %esi			/* higher? */
	cmovgl	%esi, %ecx
	movb	%cl, CPUVAR(ILEVEL)		/* splraiseipl() */
	subl	%eax, CPUVAR(MTX_COUNT)		/* decl doesnt set CF */
	cmovncl	CPUVAR(MTX_OLDSPL), %esi
	movl	%esi, CPUVAR(MTX_OLDSPL)
	xchgb	%al, MTX_LOCK(%rdi)		/* lock */
#ifdef MULTIPROCESSOR	/* XXX for xen */
	testb	%al, %al
	jnz	1f
#endif
	RET
1:
	jmp	_C_LABEL(mutex_spin_retry)	/* failed; hard case */
END(mutex_spin_enter)

/*
 * void mutex_spin_exit(kmutex_t *mtx);
 *
 * Release a spin mutex and post a load fence.
 */
ENTRY(mutex_spin_exit)
#ifdef DIAGNOSTIC

	movl	$0x0001, %eax			/* new + expected value */
	movq	CPUVAR(SELF), %r8
	cmpxchgb %ah, MTX_LOCK(%rdi)		/* unlock */
	jnz	_C_LABEL(mutex_vector_exit)	/* hard case if problems */
	movl	CPU_INFO_MTX_OLDSPL(%r8), %edi
	incl	CPU_INFO_MTX_COUNT(%r8)
	jnz	1f
	cmpb	CPU_INFO_ILEVEL(%r8), %dil
	jae	1f
	movq	CPU_INFO_IUNMASK(%r8,%rdi,8), %rsi
	CLI(ax)
	testq	CPU_INFO_IPENDING(%r8), %rsi
	jnz	_C_LABEL(Xspllower)
	movb	%dil, CPU_INFO_ILEVEL(%r8)
	STI(ax)
1:	rep					/* double byte ret as branch */
	ret					/* target: see AMD docs */

#else	/* DIAGNOSTIC */

	movq	CPUVAR(SELF), %rsi
	movb	$0x00, MTX_LOCK(%rdi)
	movl	CPU_INFO_MTX_OLDSPL(%rsi), %ecx
	incl	CPU_INFO_MTX_COUNT(%rsi)
	movzbl	CPU_INFO_ILEVEL(%rsi),%edx
	cmovnzl	%edx,%ecx
	cmpl	%edx,%ecx			/* new level is lower? */
	jae	2f
	xorq	%rdi,%rdi			/* rdi: ci_ipending mask */
	notq	%rdi
	shrq	$8,%rdi
	movq	%rcx,%r9			/* r9: shifted new level */
	shlq	$56,%r9
1:
	movq	CPU_INFO_IPENDING(%rsi),%rax
	testq	%rax,CPU_INFO_IUNMASK(%rsi,%rcx,8)/* deferred interrupts? */
	jnz	3f
	movq	%rax,%r8
	andq	%rdi,%r8
	orq	%r9,%r8
	cmpxchgq %r8,CPU_INFO_ISTATE(%rsi)	/* swap in new ilevel */
	jnz	4f
2:
	ret
3:
	movl	%ecx, %edi
	jmp	_C_LABEL(Xspllower)
4:
	jmp	1b

#endif	/* DIAGNOSTIC */

END(mutex_spin_exit)

/*
 * void	rw_enter(krwlock_t *rwl, krw_t op);
 *
 * Acquire one hold on a RW lock.
 */
ENTRY(rw_enter)
	cmpl	$RW_READER, %esi
	jne	2f

	/*
	 * Reader: this is the most common case.
	 */
	movq	(%rdi), %rax
0:
	testb	$(RW_WRITE_LOCKED|RW_WRITE_WANTED), %al
	jnz	3f
	leaq	RW_READ_INCR(%rax), %rdx
	LOCK
	cmpxchgq %rdx, (%rdi)
	jnz	1f
	RET
1:
	jmp	0b

	/*
	 * Writer: if the compare-and-set fails, don't bother retrying.
	 */
2:	movq	CPUVAR(CURLWP), %rcx
	xorq	%rax, %rax
	orq	$RW_WRITE_LOCKED, %rcx
	LOCK
	cmpxchgq %rcx, (%rdi)
	jnz	3f
	RET
3:
	jmp	_C_LABEL(rw_vector_enter)
END(rw_enter)

/*
 * void	rw_exit(krwlock_t *rwl);
 *
 * Release one hold on a RW lock.
 */
ENTRY(rw_exit)
	movq	(%rdi), %rax
	testb	$RW_WRITE_LOCKED, %al
	jnz	2f

	/*
	 * Reader
	 */
0:	testb	$RW_HAS_WAITERS, %al
	jnz	3f
	cmpq	$RW_READ_INCR, %rax
	jb	3f
	leaq	-RW_READ_INCR(%rax), %rdx
	LOCK
	cmpxchgq %rdx, (%rdi)
	jnz	1f
	ret
1:
	jmp	0b

	/*
	 * Writer
	 */
2:	leaq	-RW_WRITE_LOCKED(%rax), %rdx
	subq	CPUVAR(CURLWP), %rdx
	jnz	3f
	LOCK
	cmpxchgq %rdx, (%rdi)
	jnz	3f
	ret

3:	jmp	_C_LABEL(rw_vector_exit)
END(rw_exit)

/*
 * int	rw_tryenter(krwlock_t *rwl, krw_t op);
 *
 * Try to acquire one hold on a RW lock.
 */
ENTRY(rw_tryenter)
	cmpl	$RW_READER, %esi
	jne	2f

	/*
	 * Reader: this is the most common case.
	 */
	movq	(%rdi), %rax
0:
	testb	$(RW_WRITE_LOCKED|RW_WRITE_WANTED), %al
	jnz	4f
	leaq	RW_READ_INCR(%rax), %rdx
	LOCK
	cmpxchgq %rdx, (%rdi)
	jnz	1f
	movl	%edx, %eax			/* nonzero */
	RET
1:
	jmp	0b

	/*
	 * Writer: if the compare-and-set fails, don't bother retrying.
	 */
2:	movq	CPUVAR(CURLWP), %rcx
	xorq	%rax, %rax
	orq	$RW_WRITE_LOCKED, %rcx
	LOCK
	cmpxchgq %rcx, (%rdi)
	movl	$0, %eax
	setz	%al
3:
	RET
	ret
4:
	xorl	%eax, %eax
	jmp	3b
END(rw_tryenter)

#endif	/* LOCKDEBUG */

/*
 * Spinlocks.
 */
ENTRY(__cpu_simple_lock_init)
	movb	$0, (%rdi)
	ret
END(__cpu_simple_lock_init)

ENTRY(__cpu_simple_lock)
	movl	$0x0100, %eax
1:
	LOCK
	cmpxchgb %ah, (%rdi)
	jnz	2f
	RET
2:
	movl	$0x0100, %eax
	pause
	nop
	nop
	cmpb	$0, (%rdi)
	je	1b
	jmp	2b
END(__cpu_simple_lock)

ENTRY(__cpu_simple_unlock)
	movb	$0, (%rdi)
	ret
END(__cpu_simple_unlock)

ENTRY(__cpu_simple_lock_try)
	movl	$0x0100, %eax
	LOCK
	cmpxchgb %ah, (%rdi)
	movl	$0, %eax
	setz	%al
	KMSAN_INIT_RET(4)
	RET
END(__cpu_simple_lock_try)

