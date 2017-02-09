/*	$NetBSD: copy.S,v 1.23 2014/01/10 16:47:07 pedro Exp $	*/
/*	NetBSD: locore.S,v 1.34 2005/04/01 11:59:31 yamt Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)locore.s	7.3 (Berkeley) 5/13/91
 */

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: copy.S,v 1.23 2014/01/10 16:47:07 pedro Exp $");

#include "assym.h"

#include <sys/errno.h>

#include <machine/frameasm.h>
#include <machine/cputypes.h>

#define GET_CURPCB(reg)	\
	movl	CPUVAR(CURLWP), reg; \
	movl	L_PCB(reg), reg

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
LABEL(x86_copyfunc_start)

/*
 * Handle deferred pmap switch.  We must re-enable preemption without
 * making a function call, so that the program counter is visible to
 * cpu_kpreempt_exit().  It can then know if it needs to restore the
 * pmap on returning, because a preemption occurred within one of the
 * copy functions.
 */
NENTRY(do_pmap_load)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	movl	CPUVAR(CURLWP), %ebx
1:
	incl	L_NOPREEMPT(%ebx)
	call	_C_LABEL(pmap_load)
	decl	L_NOPREEMPT(%ebx)
	jnz	2f
	cmpl	$0, L_DOPREEMPT(%ebx)
	jz	2f
	pushl	$0
	call	_C_LABEL(kpreempt)
	addl	$4, %esp
2:
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)
	jnz	1b
	popl	%ebx
	leave
	ret

/*
 * void *return_address(unsigned int level);
 *
 * The return address if level == 0, the return address of the caller
 * `level' levels down the stack if level > 0.
 */
ENTRY(return_address)
	movl	%ebp,%eax	/* frame pointer -> %eax */ 
	movl	4(%esp),%ecx	/* level -> %ecx */
	movl	CPUVAR(CURLWP),%edx
	movl	L_PCB(%edx),%edx
	movl	$_C_LABEL(return_address_fault),PCB_ONFAULT(%edx)
	cmpl	$0,%ecx
	je	2f
1:
	movl	(%eax),%eax	/* next frame pointer */
	decl	%ecx
	jnz	1b
2:
	movl	0x4(%eax),%eax
	movl	$0,PCB_ONFAULT(%edx)
	ret

/*
 * int kcopy(const void *from, void *to, size_t len);
 * Copy len bytes, abort on fault.
 */
/* LINTSTUB: Func: int kcopy(const void *from, void *to, size_t len) */
ENTRY(kcopy)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
.Lkcopy_start:
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax		# overlapping?
	movl	%ecx,%edx
	jb	1f
	# nope, copy forward
	shrl	$2,%ecx			# copy by 32-bit words
	rep
	movsl
	movl	%edx,%ecx
	andl	$3,%ecx			# any bytes left?
	jz	0f
	rep
	movsb
0:
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret

	ALIGN_TEXT
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	andl	$3,%ecx			# any fractional bytes?
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	%edx,%ecx		# copy remainder by 32-bit words
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	cld

.Lkcopy_end:
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret

/*****************************************************************************/

/*
 * The following primitives are used to copy data in and out of the user's
 * address space.
 */

/*
 * int copyout(const void *from, void *to, size_t len);
 * Copy len bytes into the user's address space.
 * see copyout(9)
 */

/* LINTSTUB: Func: int copyout(const void *kaddr, void *uaddr, size_t len) */
ENTRY(copyout)
	DEFERRED_SWITCH_CHECK
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%eax
.Lcopyout_start:
	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.
	 */
	movl	%edi,%edx
	addl	%eax,%edx
	jc	_C_LABEL(copy_efault)
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_C_LABEL(copy_efault)
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	andl	$3,%eax
	jz	1f
	movl	%eax,%ecx
	rep
	movsb
1:
.Lcopyout_end:
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL

/*
 * int copyin(const void *from, void *to, size_t len);
 * Copy len bytes from the user's address space.
 * see copyin(9)
 */

/* LINTSTUB: Func: int copyin(const void *uaddr, void *kaddr, size_t len) */
ENTRY(copyin)
	DEFERRED_SWITCH_CHECK
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%eax
	/*
	 * We check that the end of the source buffer is not past the end of
	 * the user's address space.  If it's not, then we only need to check
	 * that each page is readable, and the CPU will do that for us.
	 */
.Lcopyin_start:
	movl	%esi,%edx
	addl	%eax,%edx
	jc	_C_LABEL(copy_efault)
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_C_LABEL(copy_efault)
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	andl	$3,%eax
	jz	1f
	movl	%eax,%ecx
	rep
	movsb
1:
.Lcopyin_end:
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL

/* LINTSTUB: Ignore */
NENTRY(copy_efault)
	movl	$EFAULT,%eax

/*
 * kcopy_fault is used by kcopy and copy_fault is used by copyin/out.
 *
 * they're distinguished for lazy pmap switching.  see trap().
 */
/* LINTSTUB: Ignore */
NENTRY(kcopy_fault)
	popl	%edi
	popl	%esi
	ret

/* LINTSTUB: Ignore */
NENTRY(copy_fault)
	popl	%edi
	popl	%esi
	ret

/* LINTSTUB: Ignore */
NENTRY(return_address_fault)
	movl $0,PCB_ONFAULT(%edx)
	movl $0,%eax
	ret

/*
 * int copyoutstr(const void *from, void *to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 * see copyoutstr(9)
 */
/* LINTSTUB: Func: int copyoutstr(const void *kaddr, void *uaddr, size_t len, size_t *done) */
ENTRY(copyoutstr)
	DEFERRED_SWITCH_CHECK
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi		# esi = from
	movl	16(%esp),%edi		# edi = to
	movl	20(%esp),%edx		# edx = maxlen
.Lcopyoutstr_start:
5:	
	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%edi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%edi,%eax
	jc	_C_LABEL(copystr_efault)
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20(%esp)

1:	incl	%edx

1:	decl	%edx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
.Lcopyoutstr_end:
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	copystr_return

2:	/* edx is zero -- return EFAULT or ENAMETOOLONG. */
	cmpl	$VM_MAXUSER_ADDRESS,%edi
	jae	_C_LABEL(copystr_efault)
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL

/*
 * int copyinstr(const void *from, void *to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 * see copyinstr(9)
 */
/* LINTSTUB: Func: int copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done) */
ENTRY(copyinstr)
	DEFERRED_SWITCH_CHECK
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi		# %esi = from
	movl	16(%esp),%edi		# %edi = to
	movl	20(%esp),%edx		# %edx = maxlen

	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%esi).
	 */
.Lcopyinstr_start:
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%esi,%eax
	jc	_C_LABEL(copystr_efault)
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20(%esp)

1:	incl	%edx

1:	decl	%edx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
.Lcopyinstr_end:
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	copystr_return

2:	/* edx is zero -- return EFAULT or ENAMETOOLONG. */
	cmpl	$VM_MAXUSER_ADDRESS,%esi
	jae	_C_LABEL(copystr_efault)
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL

/* LINTSTUB: Ignore */
NENTRY(copystr_efault)
	movl	$EFAULT,%eax

/* LINTSTUB: Ignore */
NENTRY(copystr_fault)
copystr_return:
	/* Set *lencopied and return %eax. */
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	8f
	movl	%ecx,(%edx)

8:	popl	%edi
	popl	%esi
	ret

/*
 * int copystr(const void *from, void *to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NUL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 * see copystr(9)
 */
/* LINTSTUB: Func: int copystr(const void *kfaddr, void *kdaddr, size_t len, size_t *done) */
ENTRY(copystr)
	pushl	%esi
	pushl	%edi

	movl	12(%esp),%esi		# esi = from
	movl	16(%esp),%edi		# edi = to
	movl	20(%esp),%edx		# edx = maxlen
	incl	%edx

1:	decl	%edx
	jz	4f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	6f

4:	/* edx is zero -- return ENAMETOOLONG. */
	movl	$ENAMETOOLONG,%eax

6:	/* Set *lencopied and return %eax. */
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)

7:	popl	%edi
	popl	%esi
	ret

/*
 * long fuword(const void *uaddr);
 * Fetch an int from the user's address space.
 * see fuword(9)
 */
/* LINTSTUB: Func: long fuword(const void *base) */
ENTRY(fuword)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-4,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * int fusword(const void *uaddr);
 * Fetch a short from the user's address space.
 * see fusword(9)
 */
/* LINTSTUB: Func: int fusword(const void *base) */
ENTRY(fusword)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * int fuswintr(const void *uaddr);
 * Fetch a short from the user's address space.  Can be called during an
 * interrupt.
 * see fuswintr(9)
 */
/* LINTSTUB: Func: int fuswintr(const void *base) */
ENTRY(fuswintr)
	cmpl	$TLBSTATE_VALID, CPUVAR(TLBSTATE)
	jnz	_C_LABEL(fusuaddrfault)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_C_LABEL(fusuaddrfault)
	movl	CPUVAR(CURLWP),%ecx
	movl	L_PCB(%ecx),%ecx
	movl	$_C_LABEL(fusubail),PCB_ONFAULT(%ecx)
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

/*
 * int fubyte(const void *uaddr);
 * Fetch a byte from the user's address space.
 * see fubyte(9)
 */
/* LINTSTUB: Func: int fubyte(const void *base) */
ENTRY(fubyte)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movzbl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * Handle faults from [fs]u*().  Clean up and return -1.
 */
/* LINTSTUB: Ignore */
NENTRY(fusufault)
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$-1,%eax
	ret

/*
 * Handle faults from [fs]u*().  Clean up and return -1.  This differs from
 * fusufault() in that trap() will recognize it and return immediately rather
 * than trying to page fault.
 */
/* LINTSTUB: Ignore */
NENTRY(fusubail)
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$-1,%eax
	ret

/*
 * Handle earlier faults from [fs]u*(), due to our of range addresses.
 */
/* LINTSTUB: Ignore */
NENTRY(fusuaddrfault)
	movl	$-1,%eax
	ret

/*
 * int suword(void *uaddr, long x);
 * Store an int in the user's address space.
 * see suword(9)
 */
/* LINTSTUB: Func: int suword(void *base, long c) */
ENTRY(suword)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-4,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movl	8(%esp),%eax
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * int susword(void *uaddr, short x);
 * Store a short in the user's address space.
 * see susword(9)
 */
/* LINTSTUB: Func: int susword(void *base, short c) */
ENTRY(susword)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movl	8(%esp),%eax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * int suswintr(void *uaddr, short x);
 * Store a short in the user's address space.  Can be called during an
 * interrupt.
 * see suswintr(9)
 */
/* LINTSTUB: Func: int suswintr(void *base, short c) */
ENTRY(suswintr)
	cmpl	$TLBSTATE_VALID, CPUVAR(TLBSTATE)
	jnz	_C_LABEL(fusuaddrfault)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_C_LABEL(fusuaddrfault)
	movl	CPUVAR(CURLWP),%ecx
	movl	L_PCB(%ecx),%ecx
	movl	$_C_LABEL(fusubail),PCB_ONFAULT(%ecx)
	movl	8(%esp),%eax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

/*
 * int subyte(void *uaddr, char x);
 * Store a byte in the user's address space.
 * see subyte(9)
 */
/* LINTSTUB: Func: int subyte(void *base, int c) */
ENTRY(subyte)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	_C_LABEL(fusuaddrfault)
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(fusufault),PCB_ONFAULT(%ecx)
	movb	8(%esp),%al
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
	DEFERRED_SWITCH_CALL

/*
 * Compare-and-swap the 32-bit integer in the user-space.
 *
 * int	ucas_32(volatile int32_t *uptr, int32_t old, int32_t new, int32_t *ret);
 */
ENTRY(ucas_32)
	DEFERRED_SWITCH_CHECK
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	/* Fail if kernel-space */
	cmpl	$VM_MAXUSER_ADDRESS-4, %edx
	ja	_C_LABEL(ucas_efault)
	/* Label for fault handler */
.Lucas32_start:
	/* Perform the CAS */
	lock
	cmpxchgl %ecx, (%edx)
.Lucas32_end:
	/*
	 * Note: %eax is "old" value.
	 * Set the return values.
	 */
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	xorl	%eax, %eax
	ret
	DEFERRED_SWITCH_CALL

NENTRY(ucas_efault)
	mov	$EFAULT, %eax
NENTRY(ucas_fault)
	ret

/*
 * int	ucas_int(volatile int *uptr, int old, int new, int *ret);
 * int	ucas_ptr(volatile void **uptr, void *old, void *new, void **ret);
 */
STRONG_ALIAS(ucas_ptr, ucas_32)
STRONG_ALIAS(ucas_int, ucas_32)

/*
 * copyin() optimised for bringing in syscall arguments.
 */
ENTRY(x86_copyargs)
	DEFERRED_SWITCH_CHECK
	pushl	%esi
	movl	8(%esp),%esi
	movl	12(%esp),%edx
	movl	16(%esp),%ecx

	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.  If it's not, then we only need to
	 * check that each page is readable, and the CPU will do that for us.
	 */
.Lx86_copyargs_start:
	movl	%esi,%eax
	addl	%ecx,%eax
	jc	_C_LABEL(x86_copyargs_efault)
	cmpl	$VM_MAXUSER_ADDRESS,%eax
	ja	_C_LABEL(x86_copyargs_efault)
	/* There are a maximum of 8 args + 2 for syscall indirect */
	cmp     $16,%ecx
	movl	(%esi),%eax
	movl	4(%esi),%ecx
	movl	%eax,(%edx)
	movl	%ecx,4(%edx)
	movl	8(%esi),%eax
	movl	12(%esi),%ecx
	movl	%eax,8(%edx)
	movl	%ecx,12(%edx)
	ja	2f		/* Optimise since most sycalls have <= 4 args */
1:
	popl	%esi
	xorl	%eax,%eax
	ret
2:
	movl	16(%esi),%eax
	movl	20(%esi),%ecx
	movl	%eax,16(%edx)
	movl	%ecx,20(%edx)
	movl	24(%esi),%eax
	movl	28(%esi),%ecx
	movl	%eax,24(%edx)
	movl	%ecx,28(%edx)
	movl	32(%esi),%eax
	movl	36(%esi),%ecx
	movl	%eax,32(%edx)
	movl	%ecx,36(%edx)
.Lx86_copyargs_end:
	jmp 1b

/* LINTSTUB: Ignore */
NENTRY(x86_copyargs_efault)
	movl	$EFAULT,%eax

/* LINTSTUB: Ignore */
NENTRY(x86_copyargs_fault)
	popl	%esi
	ret
	DEFERRED_SWITCH_CALL

/*
 * Label must be after all copy functions.
 */
LABEL(x86_copyfunc_end)

/*
 * Fault table of copy functions for trap().
 */
	.section ".rodata"
	.globl _C_LABEL(onfault_table)
_C_LABEL(onfault_table):
	.long .Lcopyin_start
	.long .Lcopyin_end
	.long _C_LABEL(copy_fault)

	.long .Lcopyout_start
	.long .Lcopyout_end
	.long _C_LABEL(copy_fault)

	.long .Lkcopy_start
	.long .Lkcopy_end
	.long _C_LABEL(kcopy_fault)

	.long .Lcopyoutstr_start
	.long .Lcopyoutstr_end
	.long _C_LABEL(copystr_fault)

	.long .Lcopyinstr_start
	.long .Lcopyinstr_end
	.long _C_LABEL(copystr_fault)

	.long .Lucas32_start
	.long .Lucas32_end
	.long _C_LABEL(ucas_fault)

	.long .Lx86_copyargs_start
	.long .Lx86_copyargs_end
	.long _C_LABEL(x86_copyargs_fault)

	.long 0	/* terminate */

	.text
