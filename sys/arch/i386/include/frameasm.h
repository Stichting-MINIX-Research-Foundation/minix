/*	$NetBSD: frameasm.h,v 1.15 2011/07/26 12:57:35 yamt Exp $	*/

#ifndef _I386_FRAMEASM_H_
#define _I386_FRAMEASM_H_

#ifdef _KERNEL_OPT
#include "opt_multiprocessor.h"
#include "opt_xen.h"
#endif

#if !defined(XEN)
#define CLI(reg)        cli
#define STI(reg)        sti
#else
/* XXX assym.h */
#define TRAP_INSTR      int $0x82
#define XEN_BLOCK_EVENTS(reg)   movb $1,EVTCHN_UPCALL_MASK(reg)
#define XEN_UNBLOCK_EVENTS(reg) movb $0,EVTCHN_UPCALL_MASK(reg)
#define XEN_TEST_PENDING(reg)   testb $0xFF,EVTCHN_UPCALL_PENDING(reg)

#define CLI(reg)        movl    CPUVAR(VCPU),reg ;  \
                        XEN_BLOCK_EVENTS(reg)
#define STI(reg)        movl    CPUVAR(VCPU),reg ;  \
			XEN_UNBLOCK_EVENTS(reg)
#define STIC(reg)       movl    CPUVAR(VCPU),reg ;  \
			XEN_UNBLOCK_EVENTS(reg)  ; \
			testb $0xff,EVTCHN_UPCALL_PENDING(reg)
#endif

#ifndef TRAPLOG
#define TLOG		/**/
#else
/*
 * Fill in trap record
 */
#define TLOG						\
9:							\
	movl	%fs:CPU_TLOG_OFFSET, %eax;		\
	movl	%fs:CPU_TLOG_BASE, %ebx;		\
	addl	$SIZEOF_TREC,%eax;			\
	andl	$SIZEOF_TLOG-1,%eax;			\
	addl	%eax,%ebx;				\
	movl	%eax,%fs:CPU_TLOG_OFFSET;		\
	movl	%esp,TREC_SP(%ebx);			\
	movl	$9b,TREC_HPC(%ebx);			\
	movl	TF_EIP(%esp),%eax;			\
	movl	%eax,TREC_IPC(%ebx);			\
	rdtsc			;			\
	movl	%eax,TREC_TSC(%ebx);			\
	movl	$MSR_LASTBRANCHFROMIP,%ecx;		\
	rdmsr			;			\
	movl	%eax,TREC_LBF(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_LBT(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_IBF(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_IBT(%ebx)
#endif
		
/*
 * These are used on interrupt or trap entry or exit.
 */
#define	INTRENTRY \
	subl	$TF_PUSHSIZE,%esp	; \
	movw	%gs,TF_GS(%esp)	; \
	movw	%fs,TF_FS(%esp) ; \
	movl	%eax,TF_EAX(%esp)	; \
	movw	%es,TF_ES(%esp) ; \
	movw	%ds,TF_DS(%esp) ; \
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	; \
	movl	%edi,TF_EDI(%esp)	; \
	movl	%esi,TF_ESI(%esp)	; \
	movw	%ax,%ds	; \
	movl	%ebp,TF_EBP(%esp)	; \
	movw	%ax,%es	; \
	movl	%ebx,TF_EBX(%esp)	; \
	movw	%ax,%gs	; \
	movl	%edx,TF_EDX(%esp)	; \
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax	; \
	movl	%ecx,TF_ECX(%esp)	; \
	movl	%eax,%fs	; \
	cld			; \
	TLOG

/*
 * INTRFASTEXIT should be in sync with trap(), resume_iret and friends.
 */
#define	INTRFASTEXIT \
	movw	TF_GS(%esp),%gs	; \
	movw	TF_FS(%esp),%fs	; \
	movw	TF_ES(%esp),%es	; \
	movw	TF_DS(%esp),%ds	; \
	movl	TF_EDI(%esp),%edi	; \
	movl	TF_ESI(%esp),%esi	; \
	movl	TF_EBP(%esp),%ebp	; \
	movl	TF_EBX(%esp),%ebx	; \
	movl	TF_EDX(%esp),%edx	; \
	movl	TF_ECX(%esp),%ecx	; \
	movl	TF_EAX(%esp),%eax	; \
	addl	$(TF_PUSHSIZE+8),%esp	; \
	iret

#define	DO_DEFERRED_SWITCH \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)		; \
	jz	1f					; \
	call	_C_LABEL(pmap_load)			; \
	1:

#define	DO_DEFERRED_SWITCH_RETRY \
	1:						; \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)		; \
	jz	1f					; \
	call	_C_LABEL(pmap_load)			; \
	jmp	1b					; \
	1:

#define	CHECK_DEFERRED_SWITCH \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)

#define	CHECK_ASTPENDING(reg)	movl	CPUVAR(CURLWP),reg	; \
				cmpl	$0, L_MD_ASTPENDING(reg)
#define	CLEAR_ASTPENDING(reg)	movl	$0, L_MD_ASTPENDING(reg)

/*
 * IDEPTH_INCR:
 * increase ci_idepth and switch to the interrupt stack if necessary.
 * note that the initial value of ci_idepth is -1.
 *
 * => should be called with interrupt disabled.
 * => save the old value of %esp in %eax.
 */

#define	IDEPTH_INCR \
	incl	CPUVAR(IDEPTH); \
	movl	%esp, %eax; \
	jne	999f; \
	movl	CPUVAR(INTRSTACK), %esp; \
999:	pushl	%eax; /* eax == pointer to intrframe */ \

/*
 * IDEPTH_DECR:
 * decrement ci_idepth and switch back to
 * the original stack saved by IDEPTH_INCR.
 *
 * => should be called with interrupt disabled.
 */

#define	IDEPTH_DECR \
	popl	%esp; \
	decl	CPUVAR(IDEPTH)

#endif /* _I386_FRAMEASM_H_ */
