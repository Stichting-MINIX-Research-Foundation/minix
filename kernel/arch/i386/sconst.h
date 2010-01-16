#ifndef __SCONST_H__
#define __SCONST_H__

#include "../../const.h"

/* Miscellaneous constants used in assembler code. */
	W = _WORD_SIZE	/* Machine word size. */

/* Offsets in struct proc. They MUST match proc.h. */
	P_STACKBASE = 0
	GSREG = P_STACKBASE
	FSREG = GSREG+2	/* 386 introduces FS and GS segments*/
	ESREG = FSREG+2
	DSREG = ESREG+2
	DIREG = DSREG+2
	SIREG = DIREG+W
	BPREG = SIREG+W
	STREG = BPREG+W	/* hole for another SP*/
	BXREG = STREG+W
	DXREG = BXREG+W
	CXREG = DXREG+W
	AXREG = CXREG+W
	RETADR = AXREG+W	/* return address for save() call*/
	PCREG = RETADR+W
	CSREG = PCREG+W
	PSWREG = CSREG+W
	SPREG = PSWREG+W
	SSREG = SPREG+W
	P_STACKTOP = SSREG+W
	FP_SAVE_AREA_P = P_STACKTOP
	P_LDT_SEL = FP_SAVE_AREA_P + 532
	P_CR3 = P_LDT_SEL+W
	P_LDT = P_CR3+W
	P_MISC_FLAGS = P_LDT + 50
	Msize = 9	/* size of a message in 32-bit words*/


/*
 * offset to current process pointer right after trap, we assume we always have
 * error code on the stack
 */
#define CURR_PROC_PTR		20

/*
 * tests whether the interrupt was triggered in kernel. If so, jump to the
 * label. Displacement tell the macro ha far is the CS value saved by the trap
 * from the current %esp. The kernel code segment selector has the lower 3 bits
 * zeroed
 */
#define TEST_INT_IN_KERNEL(displ, label)	\
	cmpl	$CS_SELECTOR, displ(%esp)	;\
	je	label				;

/*
 * saves the basic interrupt context (no error code) to the process structure
 *
 * displ is the displacement of %esp from the original stack after trap
 * pptr is the process structure pointer
 * tmp is an available temporary register
 */
#define SAVE_TRAP_CTX(displ, pptr, tmp)			\
	movl	(0 + displ)(%esp), tmp			;\
	movl	tmp, PCREG(pptr)			;\
	movl	(4 + displ)(%esp), tmp			;\
	movl	tmp, CSREG(pptr)			;\
	movl	(8 + displ)(%esp), tmp			;\
	movl	tmp, PSWREG(pptr)			;\
	movl	(12 + displ)(%esp), tmp			;\
	movl	tmp, SPREG(pptr)			;\
	movl	tmp, STREG(pptr)			;\
	movl	(16 + displ)(%esp), tmp			;\
	movl	tmp, SSREG(pptr)			;

#define SAVE_SEGS(pptr)		\
	mov	%ds, %ss:DSREG(pptr)	;\
	mov	%es, %ss:ESREG(pptr)	;\
	mov	%fs, %ss:FSREG(pptr)	;\
	mov	%gs, %ss:GSREG(pptr)	;

#define RESTORE_SEGS(pptr)		\
	movw	%ss:DSREG(pptr), %ds	;\
	movw	%ss:ESREG(pptr), %es	;\
	movw	%ss:FSREG(pptr), %fs	;\
	movw	%ss:GSREG(pptr), %gs	;

/*
 * restore kernel segments, %ss is kernnel data segment, %cs is aready set and
 * %fs, %gs are not used
 */
#define RESTORE_KERNEL_SEGS	\
	mov	%ss, %si	;\
	mov	%si, %ds	;\
	mov	%si, %es	;\
	movw	$0, %si		;\
	mov	%si, %gs	;\
	mov	%si, %fs	;

#define SAVE_GP_REGS(pptr)	\
	mov	%eax, %ss:AXREG(pptr)		;\
	mov	%ecx, %ss:CXREG(pptr)		;\
	mov	%edx, %ss:DXREG(pptr)		;\
	mov	%ebx, %ss:BXREG(pptr)		;\
	mov	%esi, %ss:SIREG(pptr)		;\
	mov	%edi, %ss:DIREG(pptr)		;

#define RESTORE_GP_REGS(pptr)	\
	movl	%ss:AXREG(pptr), %eax		;\
	movl	%ss:CXREG(pptr), %ecx		;\
	movl	%ss:DXREG(pptr), %edx		;\
	movl	%ss:BXREG(pptr), %ebx		;\
	movl	%ss:SIREG(pptr), %esi		;\
	movl	%ss:DIREG(pptr), %edi		;

/*
 * save the context of the interrupted process to the structure in the process
 * table. It pushses the %ebp to stack to get a scratch register. After %esi is
 * saved, we can use it to get the saved %ebp from stack and save it to the
 * final location
 *
 * displ is the stack displacement. In case of an exception, there are two extra
 * value on the stack - error code and the exception number
 */
#define SAVE_PROCESS_CTX_NON_LAZY(displ) \
	push	%ebp					;\
							;\
	movl	(CURR_PROC_PTR + 4 + displ)(%esp), %ebp	;\
							;\
	/* save the segment registers */		\
	SAVE_SEGS(%ebp)					;\
							\
	SAVE_GP_REGS(%ebp)				;\
	pop	%esi			/* get the orig %ebp and save it */ ;\
	mov	%esi, %ss:BPREG(%ebp)			;\
							\
	RESTORE_KERNEL_SEGS				;\
	SAVE_TRAP_CTX(displ, %ebp, %esi)		;

#define SAVE_PROCESS_CTX(displ)				\
	SAVE_PROCESS_CTX_NON_LAZY(displ)			;\
	push	%ebp					;\
	call	lazy_fpu					;\
	add	$4, %esp					;

#endif /* __SCONST_H__ */
