/*	$NetBSD: vector.S,v 1.64 2014/01/26 19:16:17 dsl Exp $	*/

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

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: vector.S,v 1.64 2014/01/26 19:16:17 dsl Exp $");

#include "opt_ddb.h"
#include "opt_multiprocessor.h"
#include "opt_ipkdb.h"
#include "opt_vm86.h"
#include "opt_xen.h"
#include "opt_dtrace.h"

#include <machine/i8259.h>
#include <machine/i82093reg.h>
#include <machine/i82489reg.h>
#include <machine/frameasm.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#ifdef XEN
#include <xen/xen.h>
#endif

#include "ioapic.h"
#include "lapic.h"

#include "assym.h"

/*
 * Macros for interrupt entry, call to handler, and exit.
 *
 * XXX
 * The interrupt frame is set up to look like a trap frame.  This may be a
 * waste.  The only handler which needs a frame is the clock handler, and it
 * only needs a few bits.  Xdoreti() needs a trap frame for handling ASTs, but
 * it could easily convert the frame on demand.
 *
 * The direct costs of setting up a trap frame are two pushl's (error code and
 * trap number), an addl to get rid of these, and pushing and popping the
 * callee-saved registers %esi, %edi, %ebx, and %ebp twice.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first and
 * decide the ipending case with less overhead, e.g., by avoiding loading the
 * segment registers.
 *
 */

/*
 * Store address of TSS in %eax, given a selector in %eax.
 * Clobbers %eax, %ecx, %edx, but that's ok for its usage.
 * This is a bit complicated, but it's done to make as few
 * assumptions as possible about the validity of the environment.
 * The GDT and the current and previous TSS are known to be OK,
 * otherwise we would not be here. The only other thing that needs
 * to be OK is the cpu_info structure for the current CPU.
 */
#define GET_TSS \
	andl	$0xfff8,%eax				;\
	addl	CPUVAR(GDT),%eax			;\
	movl	2(%eax),%edx				;\
	andl	$0xffffff,%edx				;\
	movzbl	7(%eax),%eax				;\
	shl	$24,%eax				;\
	orl	%edx,%eax

#ifdef KDTRACE_HOOKS
	.bss
	.globl	dtrace_invop_jump_addr
	.align	4
	.type	dtrace_invop_jump_addr, @object
        .size	dtrace_invop_jump_addr, 4
dtrace_invop_jump_addr:
	.zero	4
	.globl	dtrace_invop_calltrap_addr
	.align	4
	.type	dtrace_invop_calltrap_addr, @object
        .size	dtrace_invop_calltrap_addr, 4
dtrace_invop_calltrap_addr:
	.zero	8
	.text
#endif

#ifndef XEN
#if NLAPIC > 0
#ifdef MULTIPROCESSOR
/*
 * General purpose IPI handler.
 */
IDTVEC(recurse_lapic_ipi)
	pushfl
	pushl	%cs
	pushl	%esi
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	jmp	1f
IDTVEC_END(recurse_lapic_ipi)
IDTVEC(intr_lapic_ipi)
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_HIGH,%ebx
	jae	2f
IDTVEC_END(intr_lapic_ipi)
IDTVEC(resume_lapic_ipi)
1:
	pushl	%ebx
	IDEPTH_INCR
	movl	$IPL_HIGH,CPUVAR(ILEVEL)
	sti
	call	_C_LABEL(x86_ipi_handler)
	cli
	jmp	_C_LABEL(Xdoreti)
2:
	orl	$(1 << LIR_IPI),CPUVAR(IPENDING)
	INTRFASTEXIT
IDTVEC_END(resume_lapic_ipi)

/*
 * TLB shootdown handler.
 */
IDTVEC(intr_lapic_tlb)
	pushl	$0
	pushl	$T_ASTFLT
	INTRENTRY
	movl	$0, _C_LABEL(local_apic)+LAPIC_EOI
	call	_C_LABEL(pmap_tlb_intr)
	INTRFASTEXIT
IDTVEC_END(intr_lapic_tlb)

#if defined(DDB)
IDTVEC(intrddbipi)
1:
	str	%ax
	GET_TSS
	movzwl	(%eax),%eax
	GET_TSS
	pushl	%eax
	movl	$0xff,_C_LABEL(lapic_tpr)
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	sti
	call	_C_LABEL(ddb_ipi_tss)
	addl	$4,%esp
	movl	$0,_C_LABEL(lapic_tpr)
	iret
	jmp	1b
IDTVEC_END(intrddbipi)
#endif /* DDB */
#endif /* MULTIPROCESSOR */
	
	/*
	 * Interrupt from the local APIC timer.
	 */
IDTVEC(recurse_lapic_ltimer)
	pushfl
	pushl	%cs
	pushl	%esi
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	jmp	1f
IDTVEC_END(recurse_lapic_ltimer)
IDTVEC(intr_lapic_ltimer)
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_CLOCK,%ebx
	jae	2f
IDTVEC(resume_lapic_ltimer)
1:
	pushl	%ebx
	IDEPTH_INCR
	movl	$IPL_CLOCK,CPUVAR(ILEVEL)
	sti
	pushl	$0
	call	_C_LABEL(lapic_clockintr)
	addl	$4,%esp		
	cli
	jmp	_C_LABEL(Xdoreti)
2:
	orl	$(1 << LIR_TIMER),CPUVAR(IPENDING)
	INTRFASTEXIT
IDTVEC_END(intr_lapic_ltimer)
#endif /* NLAPIC > 0 */


#define voidop(num)


/*
 * This macro defines the generic stub code. Its arguments modifiy it
 * for specific PICs.
 */

#define	INTRSTUB(name, num, early_ack, late_ack, mask, unmask, level_mask) \
IDTVEC(recurse_ ## name ## num)						;\
	pushfl								;\
	pushl	%cs							;\
	pushl	%esi							;\
	subl	$4,%esp							;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	INTRENTRY							;\
IDTVEC_END(recurse_ ## name ## num)					;\
IDTVEC(resume_ ## name ## num)						\
	movl	$IREENT_MAGIC,TF_ERR(%esp)				;\
	movl	%ebx,%esi						;\
	movl	CPUVAR(ISOURCES) + (num) * 4, %ebp			;\
	movl	IS_MAXLEVEL(%ebp),%ebx					;\
	jmp	1f							;\
IDTVEC_END(resume_ ## name ## num)					;\
IDTVEC(intr_ ## name ## num)						;\
	pushl	$0			/* dummy error code */		;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	INTRENTRY							;\
	movl	CPUVAR(ISOURCES) + (num) * 4, %ebp			;\
	mask(num)			/* mask it in hardware */	;\
	early_ack(num)			/* and allow other intrs */	;\
	testl	%ebp,%ebp						;\
	jz	9f			/* stray */			;\
	movl	IS_MAXLEVEL(%ebp),%ebx					;\
	movl	CPUVAR(ILEVEL),%esi					;\
	cmpl	%ebx,%esi						;\
	jae	10f			/* currently masked; hold it */	;\
	addl	$1,CPUVAR(NINTR)	/* statistical info */		;\
	adcl	$0,CPUVAR(NINTR)+4					;\
	addl	$1,IS_EVCNTLO(%ebp)	/* inc event counter */		;\
	adcl	$0,IS_EVCNTHI(%ebp)					;\
1:									\
	pushl	%esi			/* if_ppi */			;\
	movl	%ebx,CPUVAR(ILEVEL)					;\
	/* switch stack if necessary, and push a ptr to our intrframe */ \
	IDEPTH_INCR							;\
	sti								;\
	movl	IS_HANDLERS(%ebp),%ebx					;\
6:									\
	movl	IH_LEVEL(%ebx),%edi					;\
	cmpl	%esi,%edi						;\
	jle	7f							;\
	pushl	IH_ARG(%ebx)						;\
	movl	IH_FUN(%ebx),%eax					;\
	movl	%edi,CPUVAR(ILEVEL)					;\
	movl	IH_NEXT(%ebx),%ebx	/* next handler in chain */	;\
	call	*%eax			/* call it */			;\
	addl	$4,%esp			/* toss the arg */		;\
	testl	%ebx,%ebx						;\
	jnz	6b							;\
	cli								;\
	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
7:									\
	cli								;\
	orl     $(1 << num),CPUVAR(IPENDING)				;\
	level_mask(num)							;\
	late_ack(num)							;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
10:									\
	orl     $(1 << num),CPUVAR(IPENDING)				;\
	level_mask(num)							;\
	late_ack(num)							;\
	INTRFASTEXIT							;\
9:									\
	pushl	%esp			/* for unmask */		;\
	unmask(num)							;\
	late_ack(num)							;\
	addl	$4,%esp							;\
	INTRFASTEXIT							;\
IDTVEC_END(intr_ ## name ## num)

#define ICUADDR IO_ICU1

INTRSTUB(legacy,0,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,1,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,2,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,3,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,4,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,5,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,6,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,7,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
#undef ICUADDR
#define ICUADDR IO_ICU2

INTRSTUB(legacy,8,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,9,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,10,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,11,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,12,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,13,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,14,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,15,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)

#if NIOAPIC > 0

INTRSTUB(ioapic_edge,0,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,1,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,2,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,3,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,4,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,5,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,6,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,7,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,8,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,9,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,10,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,11,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,12,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,13,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,14,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,15,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,16,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,17,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,18,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,19,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,20,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,21,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,22,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,23,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,24,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,25,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,26,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,27,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,28,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,29,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,30,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,31,voidop,ioapic_asm_ack,voidop,voidop,voidop)

INTRSTUB(ioapic_level,0,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,1,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,2,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,3,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,4,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,5,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,6,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,7,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,8,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,9,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,10,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,11,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,12,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,13,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,14,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,15,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,16,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,17,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,18,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,19,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,20,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,21,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,22,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,23,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,24,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,25,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,26,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,27,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,28,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,29,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,30,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,31,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)

#endif

	.type	_C_LABEL(i8259_stubs), @object
LABEL(i8259_stubs)
	.long _C_LABEL(Xintr_legacy0), _C_LABEL(Xrecurse_legacy0)
	.long _C_LABEL(Xresume_legacy0)
	.long _C_LABEL(Xintr_legacy1), _C_LABEL(Xrecurse_legacy1)
	.long _C_LABEL(Xresume_legacy1)
	.long _C_LABEL(Xintr_legacy2), _C_LABEL(Xrecurse_legacy2)
	.long _C_LABEL(Xresume_legacy2)
	.long _C_LABEL(Xintr_legacy3), _C_LABEL(Xrecurse_legacy3)
	.long _C_LABEL(Xresume_legacy3)
	.long _C_LABEL(Xintr_legacy4), _C_LABEL(Xrecurse_legacy4)
	.long _C_LABEL(Xresume_legacy4)
	.long _C_LABEL(Xintr_legacy5), _C_LABEL(Xrecurse_legacy5)
	.long _C_LABEL(Xresume_legacy5)
	.long _C_LABEL(Xintr_legacy6), _C_LABEL(Xrecurse_legacy6)
	.long _C_LABEL(Xresume_legacy6)
	.long _C_LABEL(Xintr_legacy7), _C_LABEL(Xrecurse_legacy7)
	.long _C_LABEL(Xresume_legacy7)
	.long _C_LABEL(Xintr_legacy8), _C_LABEL(Xrecurse_legacy8)
	.long _C_LABEL(Xresume_legacy8)
	.long _C_LABEL(Xintr_legacy9), _C_LABEL(Xrecurse_legacy9)
	.long _C_LABEL(Xresume_legacy9)
	.long _C_LABEL(Xintr_legacy10), _C_LABEL(Xrecurse_legacy10)
	.long _C_LABEL(Xresume_legacy10)
	.long _C_LABEL(Xintr_legacy11), _C_LABEL(Xrecurse_legacy11)
	.long _C_LABEL(Xresume_legacy11)
	.long _C_LABEL(Xintr_legacy12), _C_LABEL(Xrecurse_legacy12)
	.long _C_LABEL(Xresume_legacy12)
	.long _C_LABEL(Xintr_legacy13), _C_LABEL(Xrecurse_legacy13)
	.long _C_LABEL(Xresume_legacy13)
	.long _C_LABEL(Xintr_legacy14), _C_LABEL(Xrecurse_legacy14)
	.long _C_LABEL(Xresume_legacy14)
	.long _C_LABEL(Xintr_legacy15), _C_LABEL(Xrecurse_legacy15)
	.long _C_LABEL(Xresume_legacy15)
END(i8259_stubs)

#if NIOAPIC > 0
	.type	_C_LABEL(ioapic_edge_stubs), @object
LABEL(ioapic_edge_stubs)
	.long _C_LABEL(Xintr_ioapic_edge0), _C_LABEL(Xrecurse_ioapic_edge0)
	.long _C_LABEL(Xresume_ioapic_edge0)
	.long _C_LABEL(Xintr_ioapic_edge1), _C_LABEL(Xrecurse_ioapic_edge1)
	.long _C_LABEL(Xresume_ioapic_edge1)
	.long _C_LABEL(Xintr_ioapic_edge2), _C_LABEL(Xrecurse_ioapic_edge2)
	.long _C_LABEL(Xresume_ioapic_edge2)
	.long _C_LABEL(Xintr_ioapic_edge3), _C_LABEL(Xrecurse_ioapic_edge3)
	.long _C_LABEL(Xresume_ioapic_edge3)
	.long _C_LABEL(Xintr_ioapic_edge4), _C_LABEL(Xrecurse_ioapic_edge4)
	.long _C_LABEL(Xresume_ioapic_edge4)
	.long _C_LABEL(Xintr_ioapic_edge5), _C_LABEL(Xrecurse_ioapic_edge5)
	.long _C_LABEL(Xresume_ioapic_edge5)
	.long _C_LABEL(Xintr_ioapic_edge6), _C_LABEL(Xrecurse_ioapic_edge6)
	.long _C_LABEL(Xresume_ioapic_edge6)
	.long _C_LABEL(Xintr_ioapic_edge7), _C_LABEL(Xrecurse_ioapic_edge7)
	.long _C_LABEL(Xresume_ioapic_edge7)
	.long _C_LABEL(Xintr_ioapic_edge8), _C_LABEL(Xrecurse_ioapic_edge8)
	.long _C_LABEL(Xresume_ioapic_edge8)
	.long _C_LABEL(Xintr_ioapic_edge9), _C_LABEL(Xrecurse_ioapic_edge9)
	.long _C_LABEL(Xresume_ioapic_edge9)
	.long _C_LABEL(Xintr_ioapic_edge10), _C_LABEL(Xrecurse_ioapic_edge10)
	.long _C_LABEL(Xresume_ioapic_edge10)
	.long _C_LABEL(Xintr_ioapic_edge11), _C_LABEL(Xrecurse_ioapic_edge11)
	.long _C_LABEL(Xresume_ioapic_edge11)
	.long _C_LABEL(Xintr_ioapic_edge12), _C_LABEL(Xrecurse_ioapic_edge12)
	.long _C_LABEL(Xresume_ioapic_edge12)
	.long _C_LABEL(Xintr_ioapic_edge13), _C_LABEL(Xrecurse_ioapic_edge13)
	.long _C_LABEL(Xresume_ioapic_edge13)
	.long _C_LABEL(Xintr_ioapic_edge14), _C_LABEL(Xrecurse_ioapic_edge14)
	.long _C_LABEL(Xresume_ioapic_edge14)
	.long _C_LABEL(Xintr_ioapic_edge15), _C_LABEL(Xrecurse_ioapic_edge15)
	.long _C_LABEL(Xresume_ioapic_edge15)
	.long _C_LABEL(Xintr_ioapic_edge16), _C_LABEL(Xrecurse_ioapic_edge16)
	.long _C_LABEL(Xresume_ioapic_edge16)
	.long _C_LABEL(Xintr_ioapic_edge17), _C_LABEL(Xrecurse_ioapic_edge17)
	.long _C_LABEL(Xresume_ioapic_edge17)
	.long _C_LABEL(Xintr_ioapic_edge18), _C_LABEL(Xrecurse_ioapic_edge18)
	.long _C_LABEL(Xresume_ioapic_edge18)
	.long _C_LABEL(Xintr_ioapic_edge19), _C_LABEL(Xrecurse_ioapic_edge19)
	.long _C_LABEL(Xresume_ioapic_edge19)
	.long _C_LABEL(Xintr_ioapic_edge20), _C_LABEL(Xrecurse_ioapic_edge20)
	.long _C_LABEL(Xresume_ioapic_edge20)
	.long _C_LABEL(Xintr_ioapic_edge21), _C_LABEL(Xrecurse_ioapic_edge21)
	.long _C_LABEL(Xresume_ioapic_edge21)
	.long _C_LABEL(Xintr_ioapic_edge22), _C_LABEL(Xrecurse_ioapic_edge22)
	.long _C_LABEL(Xresume_ioapic_edge22)
	.long _C_LABEL(Xintr_ioapic_edge23), _C_LABEL(Xrecurse_ioapic_edge23)
	.long _C_LABEL(Xresume_ioapic_edge23)
	.long _C_LABEL(Xintr_ioapic_edge24), _C_LABEL(Xrecurse_ioapic_edge24)
	.long _C_LABEL(Xresume_ioapic_edge24)
	.long _C_LABEL(Xintr_ioapic_edge25), _C_LABEL(Xrecurse_ioapic_edge25)
	.long _C_LABEL(Xresume_ioapic_edge25)
	.long _C_LABEL(Xintr_ioapic_edge26), _C_LABEL(Xrecurse_ioapic_edge26)
	.long _C_LABEL(Xresume_ioapic_edge26)
	.long _C_LABEL(Xintr_ioapic_edge27), _C_LABEL(Xrecurse_ioapic_edge27)
	.long _C_LABEL(Xresume_ioapic_edge27)
	.long _C_LABEL(Xintr_ioapic_edge28), _C_LABEL(Xrecurse_ioapic_edge28)
	.long _C_LABEL(Xresume_ioapic_edge28)
	.long _C_LABEL(Xintr_ioapic_edge29), _C_LABEL(Xrecurse_ioapic_edge29)
	.long _C_LABEL(Xresume_ioapic_edge29)
	.long _C_LABEL(Xintr_ioapic_edge30), _C_LABEL(Xrecurse_ioapic_edge30)
	.long _C_LABEL(Xresume_ioapic_edge30)
	.long _C_LABEL(Xintr_ioapic_edge31), _C_LABEL(Xrecurse_ioapic_edge31)
	.long _C_LABEL(Xresume_ioapic_edge31)
END(ioapic_edge_stubs)

	.type	_C_LABEL(ioapic_level_stubs), @object
LABEL(ioapic_level_stubs)
	.long _C_LABEL(Xintr_ioapic_level0), _C_LABEL(Xrecurse_ioapic_level0)
	.long _C_LABEL(Xresume_ioapic_level0)
	.long _C_LABEL(Xintr_ioapic_level1), _C_LABEL(Xrecurse_ioapic_level1)
	.long _C_LABEL(Xresume_ioapic_level1)
	.long _C_LABEL(Xintr_ioapic_level2), _C_LABEL(Xrecurse_ioapic_level2)
	.long _C_LABEL(Xresume_ioapic_level2)
	.long _C_LABEL(Xintr_ioapic_level3), _C_LABEL(Xrecurse_ioapic_level3)
	.long _C_LABEL(Xresume_ioapic_level3)
	.long _C_LABEL(Xintr_ioapic_level4), _C_LABEL(Xrecurse_ioapic_level4)
	.long _C_LABEL(Xresume_ioapic_level4)
	.long _C_LABEL(Xintr_ioapic_level5), _C_LABEL(Xrecurse_ioapic_level5)
	.long _C_LABEL(Xresume_ioapic_level5)
	.long _C_LABEL(Xintr_ioapic_level6), _C_LABEL(Xrecurse_ioapic_level6)
	.long _C_LABEL(Xresume_ioapic_level6)
	.long _C_LABEL(Xintr_ioapic_level7), _C_LABEL(Xrecurse_ioapic_level7)
	.long _C_LABEL(Xresume_ioapic_level7)
	.long _C_LABEL(Xintr_ioapic_level8), _C_LABEL(Xrecurse_ioapic_level8)
	.long _C_LABEL(Xresume_ioapic_level8)
	.long _C_LABEL(Xintr_ioapic_level9), _C_LABEL(Xrecurse_ioapic_level9)
	.long _C_LABEL(Xresume_ioapic_level9)
	.long _C_LABEL(Xintr_ioapic_level10), _C_LABEL(Xrecurse_ioapic_level10)
	.long _C_LABEL(Xresume_ioapic_level10)
	.long _C_LABEL(Xintr_ioapic_level11), _C_LABEL(Xrecurse_ioapic_level11)
	.long _C_LABEL(Xresume_ioapic_level11)
	.long _C_LABEL(Xintr_ioapic_level12), _C_LABEL(Xrecurse_ioapic_level12)
	.long _C_LABEL(Xresume_ioapic_level12)
	.long _C_LABEL(Xintr_ioapic_level13), _C_LABEL(Xrecurse_ioapic_level13)
	.long _C_LABEL(Xresume_ioapic_level13)
	.long _C_LABEL(Xintr_ioapic_level14), _C_LABEL(Xrecurse_ioapic_level14)
	.long _C_LABEL(Xresume_ioapic_level14)
	.long _C_LABEL(Xintr_ioapic_level15), _C_LABEL(Xrecurse_ioapic_level15)
	.long _C_LABEL(Xresume_ioapic_level15)
	.long _C_LABEL(Xintr_ioapic_level16), _C_LABEL(Xrecurse_ioapic_level16)
	.long _C_LABEL(Xresume_ioapic_level16)
	.long _C_LABEL(Xintr_ioapic_level17), _C_LABEL(Xrecurse_ioapic_level17)
	.long _C_LABEL(Xresume_ioapic_level17)
	.long _C_LABEL(Xintr_ioapic_level18), _C_LABEL(Xrecurse_ioapic_level18)
	.long _C_LABEL(Xresume_ioapic_level18)
	.long _C_LABEL(Xintr_ioapic_level19), _C_LABEL(Xrecurse_ioapic_level19)
	.long _C_LABEL(Xresume_ioapic_level19)
	.long _C_LABEL(Xintr_ioapic_level20), _C_LABEL(Xrecurse_ioapic_level20)
	.long _C_LABEL(Xresume_ioapic_level20)
	.long _C_LABEL(Xintr_ioapic_level21), _C_LABEL(Xrecurse_ioapic_level21)
	.long _C_LABEL(Xresume_ioapic_level21)
	.long _C_LABEL(Xintr_ioapic_level22), _C_LABEL(Xrecurse_ioapic_level22)
	.long _C_LABEL(Xresume_ioapic_level22)
	.long _C_LABEL(Xintr_ioapic_level23), _C_LABEL(Xrecurse_ioapic_level23)
	.long _C_LABEL(Xresume_ioapic_level23)
	.long _C_LABEL(Xintr_ioapic_level24), _C_LABEL(Xrecurse_ioapic_level24)
	.long _C_LABEL(Xresume_ioapic_level24)
	.long _C_LABEL(Xintr_ioapic_level25), _C_LABEL(Xrecurse_ioapic_level25)
	.long _C_LABEL(Xresume_ioapic_level25)
	.long _C_LABEL(Xintr_ioapic_level26), _C_LABEL(Xrecurse_ioapic_level26)
	.long _C_LABEL(Xresume_ioapic_level26)
	.long _C_LABEL(Xintr_ioapic_level27), _C_LABEL(Xrecurse_ioapic_level27)
	.long _C_LABEL(Xresume_ioapic_level27)
	.long _C_LABEL(Xintr_ioapic_level28), _C_LABEL(Xrecurse_ioapic_level28)
	.long _C_LABEL(Xresume_ioapic_level28)
	.long _C_LABEL(Xintr_ioapic_level29), _C_LABEL(Xrecurse_ioapic_level29)
	.long _C_LABEL(Xresume_ioapic_level29)
	.long _C_LABEL(Xintr_ioapic_level30), _C_LABEL(Xrecurse_ioapic_level30)
	.long _C_LABEL(Xresume_ioapic_level30)
	.long _C_LABEL(Xintr_ioapic_level31), _C_LABEL(Xrecurse_ioapic_level31)
	.long _C_LABEL(Xresume_ioapic_level31)
END(ioapic_level_stubs)
#endif
#else /* XEN */
#define voidop(num)

#define	XENINTRSTUB(name, num, early_ack, late_ack, mask, unmask, level_mask) \
IDTVEC(recurse_ ## name ## num)						;\
	pushfl								;\
	pushl	%cs							;\
	pushl	%esi							;\
	subl	$4,%esp							;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	INTRENTRY							;\
	movl	$_C_LABEL(Xdoreti), %esi; /* we now have a trap frame, so loop using doreti instead */ ;\
IDTVEC(resume_ ## name ## num)						\
	movl	$IREENT_MAGIC,TF_ERR(%esp)				;\
	pushl	%ebx							;\
	movl	CPUVAR(ISOURCES) + (num) * 4, %ebp			;\
	movl	$num,CPUVAR(ILEVEL)					;\
	IDEPTH_INCR /* leaves old %esp on stack	*/			;\
	STI(%eax)							;\
	movl	IS_HANDLERS(%ebp),%ebx					;\
6:									\
	pushl	IH_ARG(%ebx)						;\
	call	*IH_FUN(%ebx)		/* call it */			;\
	addl	$4,%esp			/* toss the arg */		;\
	movl	IH_IPL_NEXT(%ebx),%ebx	/* next handler in chain */	;\
	testl	%ebx,%ebx						;\
	jnz	6b							;\
									\
	CLI(%eax)							;\
	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	IDEPTH_DECR							;\
	popl	%ebx							;\
	jmp	*%esi			/* lower spl and do ASTs */	;\

/*
 * Just unmasking the event isn't enough, we also need to
 * reassert the event pending bit if needed. For now just call
 * the C function doing it, maybe rewrite in inline assembly ?
 */
#define hypervisor_asm_unmask(num)			\
	pushl $num					;\
	call _C_LABEL(hypervisor_enable_ipl)		;\
	addl	$4,%esp

XENINTRSTUB(xenev,0,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,1,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,2,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,3,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,4,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,5,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,6,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,7,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,8,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,9,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,10,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,11,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,12,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,13,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,14,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,15,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,16,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,17,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,18,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,19,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,20,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,21,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,22,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,23,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,24,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,25,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,26,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,27,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,28,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,29,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,30,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)
XENINTRSTUB(xenev,31,voidop,voidop,voidop,hypervisor_asm_unmask,voidop)

	.type	_C_LABEL(xenev_stubs), @object
LABEL(xenev_stubs)
	.long _C_LABEL(Xrecurse_xenev0), _C_LABEL(Xresume_xenev0)
	.long _C_LABEL(Xrecurse_xenev1) ,_C_LABEL(Xresume_xenev1)
	.long _C_LABEL(Xrecurse_xenev2) ,_C_LABEL(Xresume_xenev2)
	.long _C_LABEL(Xrecurse_xenev3) ,_C_LABEL(Xresume_xenev3)
	.long _C_LABEL(Xrecurse_xenev4) ,_C_LABEL(Xresume_xenev4)
	.long _C_LABEL(Xrecurse_xenev5) ,_C_LABEL(Xresume_xenev5)
	.long _C_LABEL(Xrecurse_xenev6) ,_C_LABEL(Xresume_xenev6)
	.long _C_LABEL(Xrecurse_xenev7) ,_C_LABEL(Xresume_xenev7)
	.long _C_LABEL(Xrecurse_xenev8) ,_C_LABEL(Xresume_xenev8)
	.long _C_LABEL(Xrecurse_xenev9) ,_C_LABEL(Xresume_xenev9)
	.long _C_LABEL(Xrecurse_xenev10), _C_LABEL(Xresume_xenev10)
	.long _C_LABEL(Xrecurse_xenev11), _C_LABEL(Xresume_xenev11)
	.long _C_LABEL(Xrecurse_xenev12), _C_LABEL(Xresume_xenev12)
	.long _C_LABEL(Xrecurse_xenev13), _C_LABEL(Xresume_xenev13)
	.long _C_LABEL(Xrecurse_xenev14), _C_LABEL(Xresume_xenev14)
	.long _C_LABEL(Xrecurse_xenev15), _C_LABEL(Xresume_xenev15)
	.long _C_LABEL(Xrecurse_xenev16), _C_LABEL(Xresume_xenev16)
	.long _C_LABEL(Xrecurse_xenev17), _C_LABEL(Xresume_xenev17)
	.long _C_LABEL(Xrecurse_xenev18), _C_LABEL(Xresume_xenev18)
	.long _C_LABEL(Xrecurse_xenev19), _C_LABEL(Xresume_xenev19)
	.long _C_LABEL(Xrecurse_xenev20), _C_LABEL(Xresume_xenev20)
	.long _C_LABEL(Xrecurse_xenev21), _C_LABEL(Xresume_xenev21)
	.long _C_LABEL(Xrecurse_xenev22), _C_LABEL(Xresume_xenev22)
	.long _C_LABEL(Xrecurse_xenev23), _C_LABEL(Xresume_xenev23)
	.long _C_LABEL(Xrecurse_xenev24), _C_LABEL(Xresume_xenev24)
	.long _C_LABEL(Xrecurse_xenev25), _C_LABEL(Xresume_xenev25)
	.long _C_LABEL(Xrecurse_xenev26), _C_LABEL(Xresume_xenev26)
	.long _C_LABEL(Xrecurse_xenev27), _C_LABEL(Xresume_xenev27)
	.long _C_LABEL(Xrecurse_xenev28), _C_LABEL(Xresume_xenev28)
	.long _C_LABEL(Xrecurse_xenev29), _C_LABEL(Xresume_xenev29)
	.long _C_LABEL(Xrecurse_xenev30), _C_LABEL(Xresume_xenev30)
	.long _C_LABEL(Xrecurse_xenev31), _C_LABEL(Xresume_xenev31)
END(xenev_stubs)

#endif /* XEN */

#include "i386_trap.S"
#ifdef IPKDB
#include "i386_trap_ipkdb.S"
#endif

#ifdef XEN

/*
 * A note on the "critical region" in our callback handler.
 * We want to avoid stacking callback handlers due to events occurring
 * during handling of the last event. To do this, we keep events disabled
 * until weve done all processing. HOWEVER, we must enable events before
 * popping the stack frame (cant be done atomically) and so it would still
 * be possible to get enough handler activations to overflow the stack.
 * Although unlikely, bugs of that kind are hard to track down, so wed
 * like to avoid the possibility.
 * So, on entry to the handler we detect whether we interrupted an
 * existing activation in its critical region -- if so, we pop the current
 * activation and restart the handler using the previous one.
 */
NENTRY(hypervisor_callback)
	pushl	$0			# dummy error code
	pushl	$T_ASTFLT
	INTRENTRY
        movl TF_EIP(%esp),%eax
        cmpl $scrit,%eax
        jb   11f
        cmpl $ecrit,%eax
        jb   critical_region_fixup
11:     pushl CPUVAR(ILEVEL)
        push %esp
        call do_hypervisor_callback
        add  $8,%esp
        xorl %eax,%eax
        movb TF_CS(%esp),%cl
        test $CHK_UPL,%cl		# slow return to ring 2 or 3
        je   safesti
        movl CPUVAR(ILEVEL),%ebx
        jmp  doreti_checkast
safesti:
	movl CPUVAR(VCPU),%esi
	XEN_UNBLOCK_EVENTS(%esi)	# reenable event callbacks
scrit:  /**** START OF CRITICAL REGION ****/
        XEN_TEST_PENDING(%esi)
        jnz  14f			# process more events if necessary...
        INTRFASTEXIT
critiret:
14:     XEN_BLOCK_EVENTS(%esi)
        jmp  11b
ecrit:  /**** END OF CRITICAL REGION ****/
/*
 * [How we do the fixup]. We want to merge the current stack frame with the
 * just-interrupted frame. How we do this depends on where in the critical
 * region the interrupted handler was executing, and so how many saved
 * registers are in each frame. We do this quickly using the lookup table
 * 'critical_fixup_table'. For each byte offset in the critical region, it
 * provides the number of bytes which have already been popped from the
 * interrupted stack frame.
 */
critical_region_fixup:
        cmpl	$(critiret-1),%eax	    # eip points to iret?
	jne	1f
	movl	$(TF_PUSHSIZE+0x8),%eax
	jmp	2f
1:	xorl	%eax,%eax
2:
	/* %eax contains num bytes popped */
        mov  %esp,%esi
        add  %eax,%esi        # %esi points at end of src region
        mov  %esp,%edi
        add  $(TF_PUSHSIZE+0x8+0xC),%edi # %edi points at end of dst region
        mov  %eax,%ecx
        shr  $2,%ecx          # convert words to bytes
        je   16f              # skip loop if nothing to copy
15:     subl $4,%esi          # pre-decrementing copy loop
        subl $4,%edi
        movl (%esi),%eax
        movl %eax,(%edi)
        loop 15b
16:     movl %edi,%esp        # final %edi is top of merged stack
        jmp  11b
END(hypervisor_callback)


/*
 * Hypervisor uses this for application faults while it executes.
 */
NENTRY(failsafe_callback)
	pop	%ds
	pop	%es
	pop	%fs
	pop	%gs
	call	_C_LABEL(xen_failsafe_handler)
	iret
END(failsafe_callback)

#endif /* XEN */
