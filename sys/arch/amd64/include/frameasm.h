/*	$NetBSD: frameasm.h,v 1.55.4.1 2026/01/25 16:38:52 martin Exp $	*/

#ifndef _AMD64_MACHINE_FRAMEASM_H
#define _AMD64_MACHINE_FRAMEASM_H

#ifdef _KERNEL_OPT
#include "opt_xen.h"
#include "opt_svs.h"
#include "opt_kcov.h"
#include "opt_kmsan.h"
#endif

/*
 * Macros to define pushing/popping frames for interrupts, traps
 * and system calls. Currently all the same; will diverge later.
 */

#ifdef XENPV
#define HYPERVISOR_iret hypercall_page + (__HYPERVISOR_iret * 32)
/* Xen do not need swapgs, done by hypervisor */
#define swapgs
#define iretq	pushq $0 ; jmp HYPERVISOR_iret
#define	XEN_ONLY2(x,y)	x,y
#define	NOT_XEN(x)

#ifdef DIAGNOSTIC
/*
 * acessing EVTCHN_UPCALL_MASK is safe only if preemption is disabled, i.e.:
 * l_nopreempt is not 0, or
 * ci_ilevel is not 0, or
 * EVTCHN_UPCALL_MASK is not 0
 * ci_idepth is not negative
 */
#define CLI(temp_reg) \
 	movq CPUVAR(CURLWP),%r ## temp_reg ;			\
	cmpl $0, L_NOPREEMPT(%r ## temp_reg);			\
	jne 199f;						\
	cmpb $0, CPUVAR(ILEVEL);				\
	jne 199f;						\
	movl CPUVAR(IDEPTH), %e ## temp_reg;			\
	test %e ## temp_reg, %e ## temp_reg;			\
	jns 199f;						\
 	movq $_C_LABEL(panicstr),%r ## temp_reg ;			 	\
	cmpq $0, 0(%r ## temp_reg);				\
	jne 199f;						\
	movq _C_LABEL(cli_panic), %rdi;				\
	call _C_LABEL(panic);					\
199:	movq CPUVAR(VCPU),%r ## temp_reg ;			\
	movb $1,EVTCHN_UPCALL_MASK(%r ## temp_reg);

#define STI(temp_reg) \
 	movq CPUVAR(VCPU),%r ## temp_reg ;			\
	cmpb $0, EVTCHN_UPCALL_MASK(%r ## temp_reg);		\
	jne 198f;						\
 	movq $_C_LABEL(panicstr),%r ## temp_reg ;		\
	cmpq $0, 0(%r ## temp_reg);				\
	jne 197f;						\
	movq _C_LABEL(sti_panic), %rdi;				\
	call _C_LABEL(panic);					\
197:	movq CPUVAR(VCPU),%r ## temp_reg ;			\
198:	movb $0,EVTCHN_UPCALL_MASK(%r ## temp_reg);
#else

#define CLI(temp_reg) \
 	movq CPUVAR(VCPU),%r ## temp_reg ;			\
	movb $1,EVTCHN_UPCALL_MASK(%r ## temp_reg);

#define STI(temp_reg) \
 	movq CPUVAR(VCPU),%r ## temp_reg ;			\
	movb $0,EVTCHN_UPCALL_MASK(%r ## temp_reg);		\

#endif /* DIAGNOSTIC */

/* CLI() with preemtion disabled */
#define CLI2(temp_reg, temp_reg2) \
 	movq CPUVAR(CURLWP),% ## temp_reg2 ;			\
	incl L_NOPREEMPT(% ## temp_reg2);			\
 	movq CPUVAR(VCPU),%r ## temp_reg ;			\
	movb $1,EVTCHN_UPCALL_MASK(%r ## temp_reg);		\
	decl L_NOPREEMPT(% ## temp_reg2);

#else /* XENPV */
#define	XEN_ONLY2(x,y)
#define	NOT_XEN(x)	x
#define CLI(temp_reg) cli
#define CLI2(temp_reg, temp_reg2) cli
#define STI(temp_reg) sti
#endif	/* XENPV */

#define HP_NAME_CLAC		1
#define HP_NAME_STAC		2
#define HP_NAME_NOLOCK		3
#define HP_NAME_RETFENCE	4
#define HP_NAME_SVS_ENTER	5
#define HP_NAME_SVS_LEAVE	6
#define HP_NAME_SVS_ENTER_ALT	7
#define HP_NAME_SVS_LEAVE_ALT	8
#define HP_NAME_IBRS_ENTER	9
#define HP_NAME_IBRS_LEAVE	10
#define HP_NAME_SVS_ENTER_NMI	11
#define HP_NAME_SVS_LEAVE_NMI	12
#define HP_NAME_MDS_LEAVE	13

#define HOTPATCH(name, size) \
123:						; \
	.pushsection	.rodata.hotpatch, "a"	; \
	.byte		name			; \
	.byte		size			; \
	.quad		123b			; \
	.popsection

#define SMAP_ENABLE \
	HOTPATCH(HP_NAME_CLAC, 3)		; \
	.byte 0x0F, 0x1F, 0x00			; \

#define SMAP_DISABLE \
	HOTPATCH(HP_NAME_STAC, 3)		; \
	.byte 0x0F, 0x1F, 0x00			; \

/*
 * IBRS
 */

#define IBRS_ENTER_BYTES	12
#define IBRS_ENTER \
	HOTPATCH(HP_NAME_IBRS_ENTER, IBRS_ENTER_BYTES)		; \
	NOIBRS_ENTER
#define NOIBRS_ENTER \
	.byte 0xEB, (IBRS_ENTER_BYTES-2)	/* jmp */	; \
	.fill	(IBRS_ENTER_BYTES-2),1,0xCC

#define IBRS_LEAVE_BYTES	12
#define IBRS_LEAVE \
	HOTPATCH(HP_NAME_IBRS_LEAVE, IBRS_LEAVE_BYTES)		; \
	NOIBRS_LEAVE
#define NOIBRS_LEAVE \
	.byte 0xEB, (IBRS_LEAVE_BYTES-2)	/* jmp */	; \
	.fill	(IBRS_LEAVE_BYTES-2),1,0xCC

/*
 * MDS
 */

#define MDS_LEAVE_BYTES	10
#define MDS_LEAVE \
	HOTPATCH(HP_NAME_MDS_LEAVE, MDS_LEAVE_BYTES)		; \
	NOMDS_LEAVE
#define NOMDS_LEAVE \
	.byte 0xEB, (MDS_LEAVE_BYTES-2)	/* jmp */		; \
	.fill	(MDS_LEAVE_BYTES-2),1,0xCC

#define	SWAPGS	NOT_XEN(swapgs)

/*
 * These are used on interrupt or trap entry or exit.
 */
#define INTR_SAVE_GPRS \
	movq	%rdi,TF_RDI(%rsp)	; \
	movq	%rsi,TF_RSI(%rsp)	; \
	movq	%rdx,TF_RDX(%rsp)	; \
	movq	%rcx,TF_RCX(%rsp)	; \
	movq	%r8,TF_R8(%rsp)		; \
	movq	%r9,TF_R9(%rsp)		; \
	movq	%r10,TF_R10(%rsp)	; \
	movq	%r11,TF_R11(%rsp)	; \
	movq	%r12,TF_R12(%rsp)	; \
	movq	%r13,TF_R13(%rsp)	; \
	movq	%r14,TF_R14(%rsp)	; \
	movq	%r15,TF_R15(%rsp)	; \
	movq	%rbp,TF_RBP(%rsp)	; \
	movq	%rbx,TF_RBX(%rsp)	; \
	movq	%rax,TF_RAX(%rsp)

#define	INTR_RESTORE_GPRS \
	movq	TF_RDI(%rsp),%rdi	; \
	movq	TF_RSI(%rsp),%rsi	; \
	movq	TF_RDX(%rsp),%rdx	; \
	movq	TF_RCX(%rsp),%rcx	; \
	movq	TF_R8(%rsp),%r8		; \
	movq	TF_R9(%rsp),%r9		; \
	movq	TF_R10(%rsp),%r10	; \
	movq	TF_R11(%rsp),%r11	; \
	movq	TF_R12(%rsp),%r12	; \
	movq	TF_R13(%rsp),%r13	; \
	movq	TF_R14(%rsp),%r14	; \
	movq	TF_R15(%rsp),%r15	; \
	movq	TF_RBP(%rsp),%rbp	; \
	movq	TF_RBX(%rsp),%rbx	; \
	movq	TF_RAX(%rsp),%rax

#define TEXT_USER_BEGIN	.pushsection	.text.user, "ax"
#define TEXT_USER_END	.popsection

#ifdef SVS

/* XXX: put this somewhere else */
#define SVS_UTLS		0xffffff0000000000 /* PMAP_PCPU_BASE */
#define UTLS_KPDIRPA		0
#define UTLS_SCRATCH		8
#define UTLS_RSP0		16

#define SVS_ENTER_BYTES	22
#define NOSVS_ENTER \
	.byte 0xEB, (SVS_ENTER_BYTES-2)	/* jmp */	; \
	.fill	(SVS_ENTER_BYTES-2),1,0xCC
#define SVS_ENTER \
	HOTPATCH(HP_NAME_SVS_ENTER, SVS_ENTER_BYTES)	; \
	NOSVS_ENTER

#define SVS_LEAVE_BYTES	21
#define NOSVS_LEAVE \
	.byte 0xEB, (SVS_LEAVE_BYTES-2)	/* jmp */	; \
	.fill	(SVS_LEAVE_BYTES-2),1,0xCC
#define SVS_LEAVE \
	HOTPATCH(HP_NAME_SVS_LEAVE, SVS_LEAVE_BYTES)	; \
	NOSVS_LEAVE

#define SVS_ENTER_ALT_BYTES	23
#define NOSVS_ENTER_ALTSTACK \
	.byte 0xEB, (SVS_ENTER_ALT_BYTES-2)	/* jmp */	; \
	.fill	(SVS_ENTER_ALT_BYTES-2),1,0xCC
#define SVS_ENTER_ALTSTACK \
	HOTPATCH(HP_NAME_SVS_ENTER_ALT, SVS_ENTER_ALT_BYTES)	; \
	NOSVS_ENTER_ALTSTACK

#define SVS_LEAVE_ALT_BYTES	22
#define NOSVS_LEAVE_ALTSTACK \
	.byte 0xEB, (SVS_LEAVE_ALT_BYTES-2)	/* jmp */	; \
	.fill	(SVS_LEAVE_ALT_BYTES-2),1,0xCC
#define SVS_LEAVE_ALTSTACK \
	HOTPATCH(HP_NAME_SVS_LEAVE_ALT, SVS_LEAVE_ALT_BYTES)	; \
	NOSVS_LEAVE_ALTSTACK

#define SVS_ENTER_NMI_BYTES	22
#define NOSVS_ENTER_NMI \
	.byte 0xEB, (SVS_ENTER_NMI_BYTES-2)	/* jmp */	; \
	.fill	(SVS_ENTER_NMI_BYTES-2),1,0xCC
#define SVS_ENTER_NMI \
	HOTPATCH(HP_NAME_SVS_ENTER_NMI, SVS_ENTER_NMI_BYTES)	; \
	NOSVS_ENTER_NMI

#define SVS_LEAVE_NMI_BYTES	11
#define NOSVS_LEAVE_NMI \
	.byte 0xEB, (SVS_LEAVE_NMI_BYTES-2)	/* jmp */	; \
	.fill	(SVS_LEAVE_NMI_BYTES-2),1,0xCC
#define SVS_LEAVE_NMI \
	HOTPATCH(HP_NAME_SVS_LEAVE_NMI, SVS_LEAVE_NMI_BYTES)	; \
	NOSVS_LEAVE_NMI

#else
#define SVS_ENTER	/* nothing */
#define SVS_ENTER_NMI	/* nothing */
#define SVS_LEAVE	/* nothing */
#define SVS_LEAVE_NMI	/* nothing */
#define SVS_ENTER_ALTSTACK	/* nothing */
#define SVS_LEAVE_ALTSTACK	/* nothing */
#endif

#ifdef KMSAN
/* XXX this belongs somewhere else. */
#define KMSAN_ENTER	\
	movq	%rsp,%rdi		; \
	movq	$TF_REGSIZE+16+40,%rsi	; \
	xorq	%rdx,%rdx		; \
	callq	kmsan_mark		; \
	callq	kmsan_intr_enter
#define KMSAN_LEAVE	\
	pushq	%rbp			; \
	movq	%rsp,%rbp		; \
	callq	kmsan_intr_leave	; \
	popq	%rbp
#define KMSAN_INIT_ARG(sz)	\
	pushq	%rax			; \
	pushq	%rcx			; \
	pushq	%rdx			; \
	pushq	%rsi			; \
	pushq	%rdi			; \
	pushq	%r8			; \
	pushq	%r9			; \
	pushq	%r10			; \
	pushq	%r11			; \
	movq	$sz,%rdi		; \
	callq	_C_LABEL(kmsan_init_arg); \
	popq	%r11			; \
	popq	%r10			; \
	popq	%r9			; \
	popq	%r8			; \
	popq	%rdi			; \
	popq	%rsi			; \
	popq	%rdx			; \
	popq	%rcx			; \
	popq	%rax
#define KMSAN_INIT_RET(sz)	\
	pushq	%rax			; \
	pushq	%rcx			; \
	pushq	%rdx			; \
	pushq	%rsi			; \
	pushq	%rdi			; \
	pushq	%r8			; \
	pushq	%r9			; \
	pushq	%r10			; \
	pushq	%r11			; \
	movq	$sz,%rdi		; \
	callq	_C_LABEL(kmsan_init_ret); \
	popq	%r11			; \
	popq	%r10			; \
	popq	%r9			; \
	popq	%r8			; \
	popq	%rdi			; \
	popq	%rsi			; \
	popq	%rdx			; \
	popq	%rcx			; \
	popq	%rax
#else
#define KMSAN_ENTER		/* nothing */
#define KMSAN_LEAVE		/* nothing */
#define KMSAN_INIT_ARG(sz)	/* nothing */
#define KMSAN_INIT_RET(sz)	/* nothing */
#endif

#ifdef KCOV
#define KCOV_DISABLE			\
	incl	CPUVAR(IDEPTH)
#define KCOV_ENABLE			\
	decl	CPUVAR(IDEPTH)
#else
#define KCOV_DISABLE		/* nothing */
#define KCOV_ENABLE		/* nothing */
#endif

#define	INTRENTRY \
	subq	$TF_REGSIZE,%rsp	; \
	INTR_SAVE_GPRS			; \
	cld				; \
	SMAP_ENABLE			; \
	testb	$SEL_UPL,TF_CS(%rsp)	; \
	je	98f			; \
	SWAPGS				; \
	IBRS_ENTER			; \
	SVS_ENTER			; \
	movw	%gs,TF_GS(%rsp)		; \
	movw	%fs,TF_FS(%rsp)		; \
	movw	%es,TF_ES(%rsp)		; \
	movw	%ds,TF_DS(%rsp)		; \
98:	KMSAN_ENTER

#define INTRFASTEXIT \
	jmp	intrfastexit

#define INTR_RECURSE_HWFRAME \
	movq	%rsp,%r10		; \
	movl	%ss,%r11d		; \
	pushq	%r11			; \
	pushq	%r10			; \
	pushfq				; \
	pushq	$GSEL(GCODE_SEL,SEL_KPL); \
/* XEN: We must fixup CS, as even kernel mode runs at CPL 3 */ \
 	XEN_ONLY2(andb	$0xfc,(%rsp);)	  \
	pushq	%r13			;

#define INTR_RECURSE_ENTRY \
	subq	$TF_REGSIZE,%rsp	; \
	INTR_SAVE_GPRS			; \
	cld				; \
	KMSAN_ENTER

#define	CHECK_DEFERRED_SWITCH \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)

#define CHECK_ASTPENDING(reg)	cmpl	$0, L_MD_ASTPENDING(reg)
#define CLEAR_ASTPENDING(reg)	movl	$0, L_MD_ASTPENDING(reg)

/*
 * If the FPU state is not in the CPU, restore it. Executed with interrupts
 * disabled.
 *
 *     %r14 is curlwp, must not be modified
 *     %rbx must not be modified
 */
#define HANDLE_DEFERRED_FPU	\
	testl	$MDL_FPU_IN_CPU,L_MD_FLAGS(%r14)	; \
	jnz	1f					; \
	call	_C_LABEL(fpu_handle_deferred)		; \
	orl	$MDL_FPU_IN_CPU,L_MD_FLAGS(%r14)	; \
1:

#endif /* _AMD64_MACHINE_FRAMEASM_H */
