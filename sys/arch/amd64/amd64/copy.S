/*	$NetBSD: copy.S,v 1.34 2020/06/30 16:20:00 maxv Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/asm.h>
#include <machine/frameasm.h>

#define GET_CURPCB(reg)	\
	movq	CPUVAR(CURLWP),reg; \
	movq	L_PCB(reg),reg

/*
 * These are arranged so that the abnormal case is a forwards
 * conditional branch - which will be predicted not-taken by
 * both Intel and AMD processors.
 */
#define DEFERRED_SWITCH_CHECK \
	CHECK_DEFERRED_SWITCH			; \
	jnz	99f				; \
98:

#define DEFERRED_SWITCH_CALL \
99:						; \
	call	_C_LABEL(do_pmap_load)		; \
	jmp	98b

/*
 * The following primitives are to copy regions of memory.
 * Label must be before all copy functions.
 */
	.text

x86_copyfunc_start:	.globl	x86_copyfunc_start

/*
 * Handle deferred pmap switch.  We must re-enable preemption without
 * making a function call, so that the program counter is visible to
 * cpu_kpreempt_exit().  It can then know if it needs to restore the
 * pmap on returning, because a preemption occurred within one of the
 * copy functions.
 */
ENTRY(do_pmap_load)
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rdi
	pushq	%rsi
	pushq	%rdx
	pushq	%rcx
	pushq	%rbx
	movq	CPUVAR(CURLWP),%rbx
1:
	incl	L_NOPREEMPT(%rbx)
	call	_C_LABEL(pmap_load)
	decl	L_NOPREEMPT(%rbx)
	jnz	2f
	cmpl	$0,L_DOPREEMPT(%rbx)
	jz	2f
	xorq	%rdi,%rdi
	call	_C_LABEL(kpreempt)
2:
	cmpl	$0,CPUVAR(WANT_PMAPLOAD)
	jnz	1b
	popq	%rbx
	popq	%rcx
	popq	%rdx
	popq	%rsi
	popq	%rdi
	leaveq
	ret
END(do_pmap_load)

/*
 * Copy routines from and to userland, plus a few more. See the
 * section 9 manpages for info. Some cases can be optimized more.
 *
 * I wonder if it's worthwhile to make these use SSE2 registers?
 * (dsl) Not from info I've read from the AMD guides.
 *
 * Also note that the setup time for 'rep movs' is horrid - especially on P4
 * netburst - but on my AMD X2 it manages one copy (read+write) per clock
 * which can be achieved with a code loop, but is probably impossible to beat.
 * However the use of 'rep movsb' for the final bytes should be killed.
 *
 * Newer Intel cpus have a much lower setup time, and may (someday)
 * be ably to do cache-line size copies....
 */

/*
 * int kcopy(const void *from, void *to, size_t len);
 * Copy len bytes from and to kernel memory, and abort on fault.
 */
ENTRY(kcopy)
	xchgq	%rdi,%rsi
	movq	%rdx,%rcx
.Lkcopy_start:
	movq	%rdi,%rax
	subq	%rsi,%rax
	cmpq	%rcx,%rax		/* overlapping? */
	jb	1f
	/* nope, copy forward */
	shrq	$3,%rcx			/* copy by 64-bit words */
	rep
	movsq

	movq	%rdx,%rcx
	andl	$7,%ecx			/* any bytes left? */
	rep
	movsb

	xorq	%rax,%rax
	ret

/*
 * Using 'rep movs' to copy backwards is not as fast as for forwards copies
 * and ought not be done when the copy doesn't acually overlap.
 * However kcopy() isn't used any that looks even vaguely used often.
 * I'm also not sure it is ever asked to do overlapping copies!
 */

1:	addq	%rcx,%rdi		/* copy backward */
	addq	%rcx,%rsi
	std
	andq	$7,%rcx			/* any fractional bytes? */
	decq	%rdi
	decq	%rsi
	rep
	movsb
	movq	%rdx,%rcx		/* copy remainder by 64-bit words */
	shrq	$3,%rcx
	subq	$7,%rsi
	subq	$7,%rdi
	rep
	movsq
	cld
.Lkcopy_end:
	xorq	%rax,%rax
	ret
END(kcopy)

ENTRY(copyout)
	DEFERRED_SWITCH_CHECK

	xchgq	%rdi,%rsi		/* kernel address to %rsi, user to %rdi */
	movq	%rdx,%rax		/* save transfer length (bytes) */

	addq	%rdi,%rdx		/* end address to %rdx */
	jc	_C_LABEL(copy_efault)	/* jump if wraps */
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)	/* jump if end in kernel space */

	SMAP_DISABLE
.Lcopyout_start:
	movq	%rax,%rcx		/* length */
	shrq	$3,%rcx			/* count of 8-byte words */
	rep
	movsq				/* copy from %rsi to %rdi */
	movb	%al,%cl
	andb	$7,%cl			/* remaining number of bytes */
	rep
	movsb				/* copy remaining bytes */
.Lcopyout_end:
	SMAP_ENABLE

	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL
END(copyout)

ENTRY(copyin)
	DEFERRED_SWITCH_CHECK

	xchgq	%rdi,%rsi
	movq	%rdx,%rax

	addq	%rsi,%rdx		/* check source address not wrapped */
	jc	_C_LABEL(copy_efault)
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)	/* j if end in kernel space */

	SMAP_DISABLE
.Lcopyin_start:
3:	/* bcopy(%rsi, %rdi, %rax); */
	movq	%rax,%rcx
	shrq	$3,%rcx
	rep
	movsq
	movb	%al,%cl
	andb	$7,%cl
	rep
	movsb
.Lcopyin_end:
	SMAP_ENABLE

	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL
END(copyin)

ENTRY(copy_efault)
	movq	$EFAULT,%rax
	ret
END(copy_efault)

ENTRY(kcopy_fault)
	cld
	ret
END(kcopy_fault)

ENTRY(copy_fault)
	SMAP_ENABLE
	ret
END(copy_fault)

ENTRY(copyoutstr)
	DEFERRED_SWITCH_CHECK
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rdi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rdi,%rax
	jc	_C_LABEL(copystr_efault)
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8
1:	incq	%rdx

	SMAP_DISABLE
.Lcopyoutstr_start:
1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b
.Lcopyoutstr_end:
	SMAP_ENABLE

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* rdx is zero -- return EFAULT or ENAMETOOLONG. */
	SMAP_ENABLE
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rdi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL
END(copyoutstr)

ENTRY(copyinstr)
	DEFERRED_SWITCH_CHECK
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rsi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rsi,%rax
	jc	_C_LABEL(copystr_efault)
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8
1:	incq	%rdx

	SMAP_DISABLE
.Lcopyinstr_start:
1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b
.Lcopyinstr_end:
	SMAP_ENABLE

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* rdx is zero -- return EFAULT or ENAMETOOLONG. */
	SMAP_ENABLE
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rsi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL
END(copyinstr)

ENTRY(copystr_efault)
	movl	$EFAULT,%eax
	jmp	copystr_return
END(copystr_efault)

ENTRY(copystr_fault)
	SMAP_ENABLE
copystr_return:
	/* Set *lencopied and return %eax. */
	testq	%r9,%r9
	jz	8f
	subq	%rdx,%r8
	movq	%r8,(%r9)
8:	ret
END(copystr_fault)

/**************************************************************************/

#define	UFETCHSTORE_PROLOGUE(x)						\
	movq	$VM_MAXUSER_ADDRESS-x,%r11			;	\
	cmpq	%r11,%rdi					;	\
	ja	_C_LABEL(copy_efault)

/* LINTSTUB: int _ufetch_8(const uint8_t *uaddr, uint8_t *valp); */
ENTRY(_ufetch_8)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(1)

	SMAP_DISABLE
.L_ufetch_8_start:
	movb	(%rdi),%al
.L_ufetch_8_end:
	SMAP_ENABLE

	movb	%al,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ufetch_8)

/* LINTSTUB: int _ufetch_16(const uint16_t *uaddr, uint16_t *valp); */
ENTRY(_ufetch_16)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(2)

	SMAP_DISABLE
.L_ufetch_16_start:
	movw	(%rdi),%ax
.L_ufetch_16_end:
	SMAP_ENABLE

	movw	%ax,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ufetch_16)

/* LINTSTUB: int _ufetch_32(const uint32_t *uaddr, uint32_t *valp); */
ENTRY(_ufetch_32)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(4)

	SMAP_DISABLE
.L_ufetch_32_start:
	movl	(%rdi),%eax
.L_ufetch_32_end:
	SMAP_ENABLE

	movl	%eax,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ufetch_32)

/* LINTSTUB: int _ufetch_64(const uint64_t *uaddr, uint64_t *valp); */
ENTRY(_ufetch_64)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(8)

	SMAP_DISABLE
.L_ufetch_64_start:
	movq	(%rdi),%rax
.L_ufetch_64_end:
	SMAP_ENABLE

	movq	%rax,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ufetch_64)

/* LINTSTUB: int _ustore_8(uint8_t *uaddr, uint8_t val); */
ENTRY(_ustore_8)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(1)

	SMAP_DISABLE
.L_ustore_8_start:
	movb	%sil,(%rdi)
.L_ustore_8_end:
	SMAP_ENABLE

	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ustore_8)

/* LINTSTUB: int _ustore_16(uint16_t *uaddr, uint16_t val); */
ENTRY(_ustore_16)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(2)

	SMAP_DISABLE
.L_ustore_16_start:
	movw	%si,(%rdi)
.L_ustore_16_end:
	SMAP_ENABLE

	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ustore_16)

/* LINTSTUB: int _ustore_32(uint32_t *uaddr, uint32_t val); */
ENTRY(_ustore_32)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(4)

	SMAP_DISABLE
.L_ustore_32_start:
	movl	%esi,(%rdi)
.L_ustore_32_end:
	SMAP_ENABLE

	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ustore_32)

/* LINTSTUB: int _ustore_64(uint64_t *uaddr, uint64_t val); */
ENTRY(_ustore_64)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE(8)

	SMAP_DISABLE
.L_ustore_64_start:
	movq	%rsi,(%rdi)
.L_ustore_64_end:
	SMAP_ENABLE

	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ustore_64)

/**************************************************************************/

/*
 * Compare-and-swap the 64-bit integer in the user-space.
 *
 * int	_ucas_64(volatile uint64_t *uptr, uint64_t old, uint64_t new,
 *		 uint64_t *ret);
 */
ENTRY(_ucas_64)
	DEFERRED_SWITCH_CHECK
	/* Fail if kernel-space */
	movq	$VM_MAXUSER_ADDRESS-8,%r8
	cmpq	%r8,%rdi
	ja	_C_LABEL(ucas_efault)
	movq	%rsi,%rax

	SMAP_DISABLE
.Lucas64_start:
	/* Perform the CAS */
	lock
	cmpxchgq %rdx,(%rdi)
.Lucas64_end:
	SMAP_ENABLE

	/*
	 * Note: %rax is "old" value.
	 * Set the return values.
	 */
	movq	%rax,(%rcx)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ucas_64)

/*
 * int	_ucas_32(volatile uint32_t *uptr, uint32_t old, uint32_t new,
 *		 uint32_t *ret);
 */
ENTRY(_ucas_32)
	DEFERRED_SWITCH_CHECK
	/* Fail if kernel-space */
	movq	$VM_MAXUSER_ADDRESS-4,%r8
	cmpq	%r8,%rdi
	ja	_C_LABEL(ucas_efault)
	movl	%esi,%eax

	SMAP_DISABLE
.Lucas32_start:
	/* Perform the CAS */
	lock
	cmpxchgl %edx,(%rdi)
.Lucas32_end:
	SMAP_ENABLE

	/*
	 * Note: %eax is "old" value.
	 * Set the return values.
	 */
	movl	%eax,(%rcx)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(_ucas_32)

ENTRY(ucas_efault)
	movq	$EFAULT,%rax
	ret
END(ucas_efault)

ENTRY(ucas_fault)
	SMAP_ENABLE
	ret
END(ucas_fault)

/*
 * Label must be after all copy functions.
 */
x86_copyfunc_end:	.globl	x86_copyfunc_end

/*
 * Fault table of copy functions for trap().
 */
	.section ".rodata"
	.globl _C_LABEL(onfault_table)

_C_LABEL(onfault_table):
	.quad .Lcopyin_start
	.quad .Lcopyin_end
	.quad _C_LABEL(copy_fault)

	.quad .Lcopyout_start
	.quad .Lcopyout_end
	.quad _C_LABEL(copy_fault)

	.quad .Lkcopy_start
	.quad .Lkcopy_end
	.quad _C_LABEL(kcopy_fault)

	.quad .Lcopyoutstr_start
	.quad .Lcopyoutstr_end
	.quad _C_LABEL(copystr_fault)

	.quad .Lcopyinstr_start
	.quad .Lcopyinstr_end
	.quad _C_LABEL(copystr_fault)

	.quad .Lucas64_start
	.quad .Lucas64_end
	.quad _C_LABEL(ucas_fault)

	.quad .Lucas32_start
	.quad .Lucas32_end
	.quad _C_LABEL(ucas_fault)

	.quad .L_ufetch_8_start
	.quad .L_ufetch_8_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ufetch_16_start
	.quad .L_ufetch_16_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ufetch_32_start
	.quad .L_ufetch_32_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ufetch_64_start
	.quad .L_ufetch_64_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ustore_8_start
	.quad .L_ustore_8_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ustore_16_start
	.quad .L_ustore_16_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ustore_32_start
	.quad .L_ustore_32_end
	.quad _C_LABEL(copy_fault)

	.quad .L_ustore_64_start
	.quad .L_ustore_64_end
	.quad _C_LABEL(copy_fault)

	.quad 0	/* terminate */

	.text
