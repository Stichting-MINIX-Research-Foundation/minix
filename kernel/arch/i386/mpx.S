/*
 * This file is part of the lowest layer of the MINIX kernel.  (The other part 
 * is "proc.c".)  The lowest layer does process switching and message handling. 
 * Furthermore it contains the assembler startup code for Minix and the 32-bit 
 * interrupt handlers.  It cooperates with the code in "start.c" to set up a  
 * good environment for main(). 
 *
 * Kernel is entered either because of kernel-calls, ipc-calls, interrupts or
 * exceptions. TSS is set so that the kernel stack is loaded. The user cotext is
 * saved to the proc table and the handler of the event is called. Once the
 * handler is done, switch_to_user() function is called to pick a new process,
 * finish what needs to be done for the next process to run, sets its context
 * and switch to userspace.
 *
 * For communication with the boot monitor at startup time some constant 
 * data are compiled into the beginning of the text segment. This facilitates  
 * reading the data at the start of the boot process, since only the first 
 * sector of the file needs to be read. 
 *
 * Some data storage is also allocated at the end of this file. This data  
 * will be at the start of the data segment of the kernel and will be read 
 * and modified by the boot monitor before the kernel starts.
 */

#include "kernel/kernel.h" /* configures the kernel */

/* sections */

#include <machine/vm.h>

#ifdef __ACK__
.text
begtext:
#ifdef __ACK__
.rom
#else
.data
#endif
begrom:
.data
begdata:
.bss
begbss:
#endif


#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "kernel/proc.h"
#include "sconst.h"

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

/*
 * Exported functions 
 * Note: in assembly language the .define statement applied to a function name  
 * is loosely equivalent to a prototype in C code -- it makes it possible to 
 * link to an entity declared in the assembly code but does not create 
 * the entity. 
 */

.globl	_restore_user_context
.globl	_reload_cr3

.globl	_divide_error
.globl	_single_step_exception
.globl	_nmi
.globl	_breakpoint_exception
.globl	_overflow
.globl	_bounds_check
.globl	_inval_opcode
.globl	_copr_not_available
.globl	_double_fault
.globl	_copr_seg_overrun
.globl	_inval_tss
.globl	_segment_not_present
.globl	_stack_exception
.globl	_general_protection
.globl	_page_fault
.globl	_copr_error
.globl	_alignment_check
.globl	_machine_check
.globl	_simd_exception
.globl	_params_size
.globl	_params_offset
.globl	_mon_ds
.globl	_switch_to_user
.globl	_save_fpu

.globl	_hwint00	/* handlers for hardware interrupts */
.globl	_hwint01
.globl	_hwint02
.globl	_hwint03
.globl	_hwint04
.globl	_hwint05
.globl	_hwint06
.globl	_hwint07
.globl	_hwint08
.globl	_hwint09
.globl	_hwint10
.globl	_hwint11
.globl	_hwint12
.globl	_hwint13
.globl	_hwint14
.globl	_hwint15

/* Exported variables. */
.globl	begbss
.globl	begdata

.text
/*===========================================================================*/
/*				MINIX					     */
/*===========================================================================*/
.globl MINIX
MINIX:
/* this is the entry point for the MINIX kernel */
	jmp	over_flags	/* skip over the next few bytes */
.short	CLICK_SHIFT	/* for the monitor: memory granularity */

flags:
/* boot monitor flags:
 *	call in 386 mode, make bss, make stack, 
 *	load high, don't patch, will return, 
 *	uses generic INT, memory vector, 
 *	new boot code return 
 */
.short	0x01FD	
	nop	/* extra byte to sync up disassembler */
over_flags:

/* Set up a C stack frame on the monitor stack.  (The monitor sets cs and ds */
/* right.  The ss descriptor still references the monitor data segment.) */
	movzwl	%sp, %esp	/* monitor stack is a 16 bit stack */
	push	%ebp
	mov	%esp, %ebp
	push	%esi
	push	%edi
	cmp	$0, 4(%ebp)	/* monitor return vector is */
	je	noret	/* nonzero if return possible */
	incl	_mon_return
noret:
	movl	%esp, _mon_sp	/* save stack pointer for later return */

/* Copy the monitor global descriptor table to the address space of kernel and */
/* switch over to it.  Prot_init() can then update it with immediate effect. */

	sgdt	_gdt+GDT_SELECTOR	/* get the monitor gdtr */
	movl	_gdt+GDT_SELECTOR+2, %esi	/* absolute address of GDT */
	mov	$_gdt, %ebx	/* address of kernel GDT */
	mov	$8*8, %ecx	/* copying eight descriptors */
copygdt:
	movb    %es:(%esi), %al
	movb	%al, (%ebx)
	inc	%esi
	inc	%ebx
	loop	copygdt
	movl	_gdt+DS_SELECTOR+2, %eax	/* base of kernel data */
	and	$0x00FFFFFF, %eax	/* only 24 bits */
	add	$_gdt, %eax	/* eax = vir2phys(gdt) */
	movl	%eax, _gdt+GDT_SELECTOR+2	/* set base of GDT */
	lgdt	_gdt+GDT_SELECTOR	/* switch over to kernel GDT */

/* Locate boot parameters, set up kernel segment registers and stack. */
	mov	8(%ebp), %ebx	/* boot parameters offset */
	mov	12(%ebp), %edx	/* boot parameters length */
	mov	16(%ebp), %eax	/* address of a.out headers */
	movl	%eax, _aout
	mov	%ds, %ax	/* kernel data */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	mov	$_k_boot_stktop, %esp	/* set sp to point to the top of kernel stack */

/* Save boot parameters into these global variables for i386 code */
	movl	%edx, _params_size
	movl	%ebx, _params_offset
	movl	$SS_SELECTOR, _mon_ds

/* Call C startup code to set up a proper environment to run main(). */
	push	%edx
	push	%ebx
	push	$SS_SELECTOR
	push	$DS_SELECTOR
	push	$CS_SELECTOR
	call	_cstart	/* cstart(cs, ds, mds, parmoff, parmlen) */
	add	$5*4, %esp

/* Reload gdtr, idtr and the segment registers to global descriptor table set */
/* up by prot_init(). */

	lgdt	_gdt+GDT_SELECTOR
	lidt	_gdt+IDT_SELECTOR

	ljmp    $CS_SELECTOR, $csinit
csinit:
	movw	$DS_SELECTOR, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	movw	$TSS_SELECTOR, %ax	/* no other TSS is used */
	ltr	%ax
	push	$0	/* set flags to known good state */
	popf	/* esp, clear nested task and int enable */
	jmp	_main	/* main() */


/*===========================================================================*/
/*				interrupt handlers			     */
/*		interrupt handlers for 386 32-bit protected mode	     */
/*===========================================================================*/

#define PIC_IRQ_HANDLER(irq)	\
	push	$irq							    	;\
	call	_irq_handle		/* intr_handle(irq_handlers[irq]) */	;\
	add	$4, %esp						    	;

/*===========================================================================*/
/*				hwint00 - 07				     */
/*===========================================================================*/
/* Note this is a macro, it just looks like a subroutine. */

#define hwint_master(irq) \
	TEST_INT_IN_KERNEL(4, 0f)					;\
									\
	SAVE_PROCESS_CTX(0)						;\
	push	%ebp							;\
	movl	$0, %ebp	/* for stack trace */			;\
	call	_context_stop						;\
	add	$4, %esp						;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	jmp	_switch_to_user						;\
									\
0:									\
	pusha								;\
	call	_context_stop_idle					;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	CLEAR_IF(10*4(%esp))						;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_master macro */
.balign	16
_hwint00:
/* Interrupt routine for irq 0 (the clock). */
	hwint_master(0)

.balign	16
_hwint01:
/* Interrupt routine for irq 1 (keyboard) */
	hwint_master(1)

.balign	16
_hwint02:
/* Interrupt routine for irq 2 (cascade!) */
	hwint_master(2)

.balign	16
_hwint03:
/* Interrupt routine for irq 3 (second serial) */
	hwint_master(3)

.balign	16
_hwint04:
/* Interrupt routine for irq 4 (first serial) */
	hwint_master(4)

.balign	16
_hwint05:
/* Interrupt routine for irq 5 (XT winchester) */
	hwint_master(5)

.balign	16
_hwint06:
/* Interrupt routine for irq 6 (floppy) */
	hwint_master(6)

.balign	16
_hwint07:
/* Interrupt routine for irq 7 (printer) */
	hwint_master(7)

/*===========================================================================*/
/*				hwint08 - 15				     */
/*===========================================================================*/
/* Note this is a macro, it just looks like a subroutine. */
#define hwint_slave(irq)	\
	TEST_INT_IN_KERNEL(4, 0f)					;\
									\
	SAVE_PROCESS_CTX(0)						;\
	push	%ebp							;\
	movl	$0, %ebp	/* for stack trace */			;\
	call	_context_stop						;\
	add	$4, %esp						;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	jmp	_switch_to_user						;\
									\
0:									\
	pusha								;\
	call	_context_stop_idle					;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	CLEAR_IF(10*4(%esp))						;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_slave macro */
.balign	16
_hwint08:
/* Interrupt routine for irq 8 (realtime clock) */
	hwint_slave(8)

.balign	16
_hwint09:
/* Interrupt routine for irq 9 (irq 2 redirected) */
	hwint_slave(9)

.balign	16
_hwint10:
/* Interrupt routine for irq 10 */
	hwint_slave(10)

.balign	16
_hwint11:
/* Interrupt routine for irq 11 */
	hwint_slave(11)

.balign	16
_hwint12:
/* Interrupt routine for irq 12 */
	hwint_slave(12)

.balign	16
_hwint13:
/* Interrupt routine for irq 13 (FPU exception) */
	hwint_slave(13)

.balign	16
_hwint14:
/* Interrupt routine for irq 14 (AT winchester) */
	hwint_slave(14)

.balign	16
_hwint15:
/* Interrupt routine for irq 15 */
	hwint_slave(15)

/*
 * IPC is only from a process to kernel
 */
.balign 16
.globl _ipc_entry
_ipc_entry:

	SAVE_PROCESS_CTX(0)

	/* save the pointer to the current process */
	push	%ebp

	/*
	 * pass the syscall arguments from userspace to the handler.
	 * SAVE_PROCESS_CTX() does not clobber these registers, they are still
	 * set as the userspace have set them
	 */
	push	%ebx
	push	%eax
	push	%ecx

	/* stop user process cycles */
	push	%ebp
	/* for stack trace */
	movl	$0, %ebp
	call	_context_stop
	add	$4, %esp

	call	_do_ipc

	/* restore the current process pointer and save the return value */
	add	$3 * 4, %esp
	pop	%esi
	mov     %eax, AXREG(%esi)

	jmp	_switch_to_user


/*
 * kernel call is only from a process to kernel
 */
.balign 16
.globl _kernel_call_entry
_kernel_call_entry:

	SAVE_PROCESS_CTX(0)

	/* save the pointer to the current process */
	push	%ebp

	/*
	 * pass the syscall arguments from userspace to the handler.
	 * SAVE_PROCESS_CTX() does not clobber these registers, they are still
	 * set as the userspace have set them
	 */
	push	%eax

	/* stop user process cycles */
	push	%ebp
	/* for stack trace */
	movl	$0, %ebp
	call	_context_stop
	add	$4, %esp

	call	_kernel_call

	/* restore the current process pointer and save the return value */
	add	$8, %esp

	jmp	_switch_to_user


.balign 16
/*
 * called by the exception interrupt vectors. If the exception does not push
 * errorcode, we assume that the vector handler pushed 0 instead. Next pushed
 * thing is the vector number. From this point on we can continue as if every
 * exception pushes an error code
 */
exception_entry:
	/*
	 * check if it is a nested trap by comparing the saved code segment
	 * descriptor with the kernel CS first
	 */
	TEST_INT_IN_KERNEL(12, exception_entry_nested)

exception_entry_from_user:
	SAVE_PROCESS_CTX(8)

	/* stop user process cycles */
	push	%ebp
	/* for stack trace clear %ebp */
	movl	$0, %ebp
	call	_context_stop
	add	$4, %esp

	/*
	 * push a pointer to the interrupt state pushed by the cpu and the
	 * vector number pushed by the vector handler just before calling
	 * exception_entry and call the exception handler.
	 */
	push	%esp
	push	$0	/* it's not a nested exception */
	call 	_exception_handler

	jmp	_switch_to_user

exception_entry_nested:

	pusha
	mov	%esp, %eax
	add	$(8 * 4), %eax
	push	%eax
	pushl	$1			/* it's a nested exception */
	call	_exception_handler
	add	$8, %esp
	popa

	/* clear the error code and the exception number */
	add	$8, %esp
	/* resume execution at the point of exception */
	iret

/*===========================================================================*/
/*				restart					     */
/*===========================================================================*/
_restore_user_context:
	mov	4(%esp), %ebp	/* will assume P_STACKBASE == 0 */

	/* reconstruct the stack for iret */
	movl	SSREG(%ebp), %eax
	push	%eax
	movl	SPREG(%ebp), %eax
	push	%eax
	movl	PSWREG(%ebp), %eax
	push	%eax
	movl	CSREG(%ebp), %eax
	push	%eax
	movl	PCREG(%ebp), %eax
	push	%eax

	RESTORE_GP_REGS(%ebp)

	RESTORE_SEGS(%ebp)

	movl	%ss:BPREG(%ebp), %ebp

	iret	/* continue process */

/*===========================================================================*/
/*				exception handlers			     */
/*===========================================================================*/

#define EXCEPTION_ERR_CODE(vector)	\
	push	$vector			;\
	jmp	exception_entry

#define EXCEPTION_NO_ERR_CODE(vector)	\
	pushl	$0		;\
	EXCEPTION_ERR_CODE(vector)

_divide_error:
	EXCEPTION_NO_ERR_CODE(DIVIDE_VECTOR)

_single_step_exception:
	EXCEPTION_NO_ERR_CODE(DEBUG_VECTOR)

_nmi:
#ifndef CONFIG_WATCHDOG
	EXCEPTION_NO_ERR_CODE(NMI_VECTOR)
#else
	/*
	 * We have to be very careful as this interrupt can occur anytime. On
	 * the other hand, if it interrupts a user process, we will resume the
	 * same process which makes things a little simpler. We know that we are
	 * already on kernel stack whenever it happened and we can be
	 * conservative and save everything as we don't need to be extremely
	 * efficient as the interrupt is infrequent and some overhead is already
	 * expected.
	 */

	/*
	 * save the important registers. We don't save %cs and %ss and they are
	 * saved and restored by CPU
	 */
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	pusha

	/*
	 * We cannot be sure about the state of the kernel segment register,
	 * however, we always set %ds and %es to the same as %ss
	 */
	mov	%ss, %si
	mov	%si, %ds
	mov	%si, %es

	push	%esp
	call	_nmi_watchdog_handler
	add	$4, %esp

	/* restore all the important registers as they were before the trap */
	popa
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds

	iret
#endif

_breakpoint_exception:
	EXCEPTION_NO_ERR_CODE(BREAKPOINT_VECTOR)

_overflow:
	EXCEPTION_NO_ERR_CODE(OVERFLOW_VECTOR)

_bounds_check:
	EXCEPTION_NO_ERR_CODE(BOUNDS_VECTOR)

_inval_opcode:
	EXCEPTION_NO_ERR_CODE(INVAL_OP_VECTOR)

_copr_not_available:
	TEST_INT_IN_KERNEL(4, copr_not_available_in_kernel)
	cld			/* set direction flag to a known value */
	SAVE_PROCESS_CTX_NON_LAZY(0)
	/* stop user process cycles */
	push	%ebp
	mov	$0, %ebp
	call	_context_stop
	pop	%ebp
	lea	P_MISC_FLAGS(%ebp), %ebx
	orw	$MF_USED_FPU, (%ebx)
	mov	$0, %ebp
	jmp	_switch_to_user

copr_not_available_in_kernel:
	pushl	$0
	pushl	$COPROC_NOT_VECTOR
	jmp	exception_entry_nested

_double_fault:
	EXCEPTION_ERR_CODE(DOUBLE_FAULT_VECTOR)

_copr_seg_overrun:
	EXCEPTION_NO_ERR_CODE(COPROC_SEG_VECTOR)

_inval_tss:
	EXCEPTION_ERR_CODE(INVAL_TSS_VECTOR)

_segment_not_present:
	EXCEPTION_ERR_CODE(SEG_NOT_VECTOR)

_stack_exception:
	EXCEPTION_ERR_CODE(STACK_FAULT_VECTOR)

_general_protection:
	EXCEPTION_ERR_CODE(PROTECTION_VECTOR)

_page_fault:
	EXCEPTION_ERR_CODE(PAGE_FAULT_VECTOR)

_copr_error:
	EXCEPTION_NO_ERR_CODE(COPROC_ERR_VECTOR)

_alignment_check:
	EXCEPTION_NO_ERR_CODE(ALIGNMENT_CHECK_VECTOR)

_machine_check:
	EXCEPTION_NO_ERR_CODE(MACHINE_CHECK_VECTOR)

_simd_exception:
	EXCEPTION_NO_ERR_CODE(SIMD_EXCEPTION_VECTOR)

/*===========================================================================*/
/*				reload_cr3				     */
/*===========================================================================*/
/* PUBLIC void reload_cr3(void); */
_reload_cr3:
	push    %ebp
	mov     %esp, %ebp
	mov	%cr3, %eax
	mov	%eax, %cr3
	pop     %ebp
	ret

/*===========================================================================*/
/*				data					     */
/*===========================================================================*/

#ifdef __ACK__
.rom	/* Before the string table please */
#else
.data
#endif
.short	0x526F	/* this must be the first data entry (magic #) */

.bss
/*
 * the kernel stack
 */
.globl _k_boot_stktop
k_boot_stack:
.space	4096		/* kernel stack */ /* FIXME use macro here */
_k_boot_stktop:		/* top of kernel stack */
