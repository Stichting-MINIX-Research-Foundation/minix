/*	$NetBSD: i386_trap.S,v 1.5 2014/02/12 23:24:09 dsl Exp $	*/

/*
 * Copyright 2002 (c) Wasabi Systems, Inc.
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

/*-
 * Copyright (c) 1998, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Andrew Doran.
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

#if 0
#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: i386_trap.S,v 1.5 2014/02/12 23:24:09 dsl Exp $");
#endif

/*
 * Trap and fault vector routines
 *
 * On exit from the kernel to user mode, we always need to check for ASTs.  In
 * addition, we need to do this atomically; otherwise an interrupt may occur
 * which causes an AST, but it won't get processed until the next kernel entry
 * (possibly the next clock tick).  Thus, we disable interrupt before checking,
 * and only enable them again on the final `iret' or before calling the AST
 * handler.
 */ 

#define TRAP(a)			pushl $(a) ; jmp _C_LABEL(alltraps)
#define ZTRAP(a)		pushl $0 ; TRAP(a)

#ifdef IPKDB
#define BPTTRAP(a)	pushl $0; pushl $(a); jmp _C_LABEL(bpttraps)
#else
#define BPTTRAP(a)	ZTRAP(a)
#endif


	.text
IDTVEC(trap00)
	ZTRAP(T_DIVIDE)
IDTVEC_END(trap00)
IDTVEC(trap01)
	BPTTRAP(T_TRCTRAP)
IDTVEC_END(trap01)
IDTVEC(trap02)
	pushl $0
	pushl $(T_NMI)
	INTRENTRY
	jmp _C_LABEL(calltrap)
IDTVEC_END(trap02)
IDTVEC(trap03)
	BPTTRAP(T_BPTFLT)
IDTVEC_END(trap03)
IDTVEC(trap04)
	ZTRAP(T_OFLOW)
IDTVEC_END(trap04)
IDTVEC(trap05)
	ZTRAP(T_BOUND)
IDTVEC_END(trap05)
/*
 * Privileged instruction fault.
 */
#ifdef KDTRACE_HOOKS
	SUPERALIGN_TEXT
IDTVEC(trap06)
	/* Check if there is no DTrace hook registered. */
	cmpl	$0,dtrace_invop_jump_addr
	je	norm_ill

	/* Check if this is a user fault. */
	/* XXX this was 0x0020 in FreeBSD */
	cmpl	$GSEL(GCODE_SEL, SEL_KPL), 4(%esp)   /* Check code segment. */

	/* If so, just handle it as a normal trap. */
	jne	norm_ill
              
	/*
	 * This is a kernel instruction fault that might have been caused
	 * by a DTrace provider.
	 */
	pushal				/* Push all registers onto the stack. */

	/*
	 * Set our jump address for the jump back in the event that
	 * the exception wasn't caused by DTrace at all.
	 */
	movl	$norm_ill, dtrace_invop_calltrap_addr

	/* Jump to the code hooked in by DTrace. */
	jmpl	*dtrace_invop_jump_addr

	/*
	 * Process the instruction fault in the normal way.
	 */
norm_ill:
	ZTRAP(T_PRIVINFLT)
IDTVEC_END(trap06)
#else
IDTVEC(trap06)
	ZTRAP(T_PRIVINFLT)
IDTVEC_END(trap06)
#endif
IDTVEC(trap07)
	pushl	$0			# dummy error code
	pushl	$T_DNA
	INTRENTRY
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
#endif
	pushl	%esp
	call	_C_LABEL(fpudna)
	addl	$4,%esp
	jmp	_C_LABEL(trapreturn)
IDTVEC_END(trap07)
IDTVEC(trap08)
	TRAP(T_DOUBLEFLT)
IDTVEC_END(trap08)
IDTVEC(trap09)
	ZTRAP(T_FPOPFLT)
IDTVEC_END(trap09)
IDTVEC(trap0a)
	TRAP(T_TSSFLT)
IDTVEC_END(trap0a)
IDTVEC(trap0b)
	TRAP(T_SEGNPFLT)
IDTVEC_END(trap0b)
IDTVEC(trap0c)
	TRAP(T_STKFLT)
IDTVEC_END(trap0c)
IDTVEC(trap0d)
	TRAP(T_PROTFLT)
IDTVEC_END(trap0d)
IDTVEC(trap0e)
#ifndef XEN
	pushl	$T_PAGEFLT
	INTRENTRY
	STI(%eax)
	testb	$PGEX_U,TF_ERR(%esp)
	jnz	calltrap
	movl	%cr2,%eax
	subl	_C_LABEL(pentium_idt),%eax
	cmpl	$(6*8),%eax
	jne	calltrap
	movb	$T_PRIVINFLT,TF_TRAPNO(%esp)
	jmp	calltrap
#else /* !XEN */
	TRAP(T_PAGEFLT)
#endif /* !XEN */
IDTVEC_END(trap0e)

IDTVEC(intrspurious)
IDTVEC(trap0f)
	/*
	 * The Pentium Pro local APIC may erroneously call this vector for a
	 * default IR7.  Just ignore it.
	 *
	 * (The local APIC does this when CPL is raised while it's on the 
	 * way to delivering an interrupt.. presumably enough has been set 
	 * up that it's inconvenient to abort delivery completely..)
	 */
	pushl	$0			# dummy error code
	pushl	$T_ASTFLT
	INTRENTRY
	STI(%eax)
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
#endif
	jmp	_C_LABEL(trapreturn)
IDTVEC_END(trap0f)
IDTVEC_END(intrspurious)

IDTVEC(trap10)
	/*
	 * Handle like an interrupt so that we can call npxintr to clear the
	 * error.  It would be better to handle npx interrupts as traps but
	 * this is difficult for nested interrupts.
	 */
	pushl	$0			# dummy error code
	pushl	$T_ARITHTRAP
.Ldo_fputrap:
	INTRENTRY
	movl	CPUVAR(ILEVEL),%ebx
	pushl	%esp
	addl	$1,CPUVAR(NTRAP)	# statistical info
	adcl	$0,CPUVAR(NTRAP)+4
	call	_C_LABEL(fputrap)
	addl	$4,%esp
	jmp	_C_LABEL(trapreturn)
IDTVEC_END(trap10)
IDTVEC(trap11)
	TRAP(T_ALIGNFLT)
IDTVEC_END(trap11)

IDTVEC(trap12)
	ZTRAP(T_MCA)
IDTVEC_END(trap12)
IDTVEC(trap13)
	pushl	$0			# dummy error code
	pushl	$T_XMM
	jmp	.Ldo_fputrap
IDTVEC_END(trap13)

IDTVEC(trap14)
IDTVEC(trap15)
IDTVEC(trap16)
IDTVEC(trap17)
IDTVEC(trap18)
IDTVEC(trap19)
IDTVEC(trap1a)
IDTVEC(trap1b)
IDTVEC(trap1c)
IDTVEC(trap1d)
IDTVEC(trap1e)
IDTVEC(trap1f)
	/* 20 - 31 reserved for future exp */
	ZTRAP(T_RESERVED)
IDTVEC_END(trap1f)
IDTVEC_END(trap1e)
IDTVEC_END(trap1d)
IDTVEC_END(trap1c)
IDTVEC_END(trap1b)
IDTVEC_END(trap1a)
IDTVEC_END(trap19)
IDTVEC_END(trap18)
IDTVEC_END(trap17)
IDTVEC_END(trap16)
IDTVEC_END(trap15)
IDTVEC_END(trap14)
#ifndef XEN
IDTVEC_END(trap13)
IDTVEC_END(trap12)
#else
IDTVEC_END(trap13)
IDTVEC_END(trap12)
#endif
IDTVEC_END(trap11)

IDTVEC(exceptions)
	.long	_C_LABEL(Xtrap00), _C_LABEL(Xtrap01)
	.long	_C_LABEL(Xtrap02), _C_LABEL(Xtrap03)
	.long	_C_LABEL(Xtrap04), _C_LABEL(Xtrap05)
	.long	_C_LABEL(Xtrap06), _C_LABEL(Xtrap07)
	.long	_C_LABEL(Xtrap08), _C_LABEL(Xtrap09)
	.long	_C_LABEL(Xtrap0a), _C_LABEL(Xtrap0b)
	.long	_C_LABEL(Xtrap0c), _C_LABEL(Xtrap0d)
	.long	_C_LABEL(Xtrap0e), _C_LABEL(Xtrap0f)
	.long	_C_LABEL(Xtrap10), _C_LABEL(Xtrap11)
	.long	_C_LABEL(Xtrap12), _C_LABEL(Xtrap13)
	.long	_C_LABEL(Xtrap14), _C_LABEL(Xtrap15)
	.long	_C_LABEL(Xtrap16), _C_LABEL(Xtrap17)
	.long	_C_LABEL(Xtrap18), _C_LABEL(Xtrap19)
	.long	_C_LABEL(Xtrap1a), _C_LABEL(Xtrap1b)
	.long	_C_LABEL(Xtrap1c), _C_LABEL(Xtrap1d)
	.long	_C_LABEL(Xtrap1e), _C_LABEL(Xtrap1f)
IDTVEC_END(exceptions)

 
IDTVEC(tss_trap08)
1:
	str	%ax
	GET_TSS
	movzwl	(%eax),%eax
	GET_TSS
	pushl	$T_DOUBLEFLT
	pushl	%eax
	call	_C_LABEL(trap_tss)
	addl	$12,%esp
	iret
	jmp	1b
IDTVEC_END(tss_trap08)

/*
 * trap() calls here when it detects a fault in INTRFASTEXIT (loading the
 * segment registers or during the iret itself).
 * The address of the (possibly reconstructed) user trap frame is
 * passed as an argument.
 * Typically the code will have raised a SIGSEGV which will be actioned
 * by the code below.
 */
	.type	_C_LABEL(trap_return_fault_return), @function
LABEL(trap_return_fault_return)
	mov	4(%esp),%esp	/* frame for user return */
	jmp	_C_LABEL(trapreturn)
END(trap_return_fault_return)

/* LINTSTUB: Ignore */
NENTRY(alltraps)
	INTRENTRY
	STI(%eax)
calltrap:
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
#endif /* DIAGNOSTIC */
	addl	$1,CPUVAR(NTRAP)	# statistical info
	adcl	$0,CPUVAR(NTRAP)+4
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
_C_LABEL(trapreturn):	.globl	trapreturn
	testb	$CHK_UPL,TF_CS(%esp)
	jnz	.Lalltraps_checkast
#ifdef VM86
	testl	$PSL_VM,TF_EFLAGS(%esp)
	jz	6f
#else
	jmp	6f
#endif
.Lalltraps_checkast:
	/* Check for ASTs on exit to user mode. */
	CLI(%eax)
	CHECK_ASTPENDING(%eax)
	jz	3f
5:	CLEAR_ASTPENDING(%eax)
	STI(%eax)
	movl	$T_ASTFLT,TF_TRAPNO(%esp)
	addl	$1,CPUVAR(NTRAP)	# statistical info
	adcl	$0,CPUVAR(NTRAP)+4
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
	jmp	.Lalltraps_checkast	/* re-check ASTs */
3:	CHECK_DEFERRED_SWITCH
	jnz	9f
#ifdef XEN
	STIC(%eax)
	jz      6f
	call    _C_LABEL(stipending)
	testl   %eax,%eax
	jz      6f
	/* process pending interrupts */
	CLI(%eax)
	movl    CPUVAR(ILEVEL), %ebx
	movl    $.Lalltraps_resume, %esi # address to resume loop at
.Lalltraps_resume:
	movl    %ebx,%eax               # get cpl
	movl    CPUVAR(IUNMASK)(,%eax,4),%eax
	andl    CPUVAR(IPENDING),%eax   # any non-masked bits left?
	jz	7f
	bsrl    %eax,%eax
	btrl    %eax,CPUVAR(IPENDING)
	movl    CPUVAR(ISOURCES)(,%eax,4),%eax
	jmp     *IS_RESUME(%eax)
7:      movl    %ebx, CPUVAR(ILEVEL) #restore cpl
	jmp     _C_LABEL(trapreturn)
#endif /* XEN */
#ifndef DIAGNOSTIC
6:	INTRFASTEXIT
#else
6:	cmpl	CPUVAR(ILEVEL),%ebx
	jne	3f
	INTRFASTEXIT
3:	STI(%eax)
	pushl	$4f
	call	_C_LABEL(panic)
	addl	$4,%esp
	pushl	%ebx
	call	_C_LABEL(spllower)
	addl	$4,%esp
	jmp	.Lalltraps_checkast	/* re-check ASTs */
4:	.asciz	"SPL NOT LOWERED ON TRAP EXIT\n"
#endif /* DIAGNOSTIC */
9:	STI(%eax)
	call	_C_LABEL(pmap_load)
	jmp	.Lalltraps_checkast	/* re-check ASTs */
END(alltraps)
