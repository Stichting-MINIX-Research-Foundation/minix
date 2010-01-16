/*
 * This file, mpx386.s, is included by mpx.s when Minix is compiled for  
 * 32-bit Intel CPUs. The alternative mpx88.s is compiled for 16-bit CPUs. 
 *
 * This file is part of the lowest layer of the MINIX kernel.  (The other part 
 * is "proc.c".)  The lowest layer does process switching and message handling. 
 * Furthermore it contains the assembler startup code for Minix and the 32-bit 
 * interrupt handlers.  It cooperates with the code in "start.c" to set up a  
 * good environment for main(). 
 *
 * Every transition to the kernel goes through this file.  Transitions to the  
 * kernel may be nested.  The initial entry may be with a system call (i.e.,  
 * send or receive a message), an exception or a hardware interrupt;  kernel  
 * reentries may only be made by hardware interrupts.  The count of reentries  
 * is kept in "k_reenter". It is important for deciding whether to switch to  
 * the kernel stack and for protecting the message passing code in "proc.c". 
 *
 * For the message passing trap, most of the machine state is saved in the 
 * proc table.  (Some of the registers need not be saved.)  Then the stack is 
 * switched to "k_stack", and interrupts are reenabled.  Finally, the system 
 * call handler (in C) is called.  When it returns, interrupts are disabled 
 * again and the code falls into the restart routine, to finish off held-up 
 * interrupts and run the process or task whose pointer is in "proc_ptr". 
 *
 * Hardware interrupt handlers do the same, except  (1) The entire state must 
 * be saved.  (2) There are too many handlers to do this inline, so the save 
 * routine is called.  A few cycles are saved by pushing the address of the 
 * appropiate restart routine for a return later.  (3) A stack switch is 
 * avoided when the stack is already switched.  (4) The (master) 8259 interrupt 
 * controller is reenabled centrally in save().  (5) Each interrupt handler 
 * masks its interrupt line using the 8259 before enabling (other unmasked) 
 * interrupts, and unmasks it after servicing the interrupt.  This limits the 
 * nest level to the number of lines and protects the handler from itself. 
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

#include "../../kernel.h" /* configures the kernel */

/* sections */

#include <sys/vm_i386.h>

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
#include <ibm/interrupt.h>
#include <archconst.h>
#include "../../const.h"
#include "../../proc.h"
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

.globl	restart
.globl reload_cr3
.globl write_cr3

.globl	divide_error
.globl	single_step_exception
.globl	nmi
.globl	breakpoint_exception
.globl	overflow
.globl	bounds_check
.globl	inval_opcode
.globl	copr_not_available
.globl	double_fault
.globl	copr_seg_overrun
.globl	inval_tss
.globl	segment_not_present
.globl	stack_exception
.globl	general_protection
.globl	page_fault
.globl	copr_error
.globl  alignment_check
.globl  machine_check
.globl  simd_exception
.globl	params_size
.globl	params_offset
.globl	mon_ds
.globl  schedcheck
.globl  dirtypde
.globl  lazy_fpu

.globl	hwint00	/* handlers for hardware interrupts */
.globl	hwint01
.globl	hwint02
.globl	hwint03
.globl	hwint04
.globl	hwint05
.globl	hwint06
.globl	hwint07
.globl	hwint08
.globl	hwint09
.globl	hwint10
.globl	hwint11
.globl	hwint12
.globl	hwint13
.globl	hwint14
.globl	hwint15

.globl	level0_call

/* Exported variables. */
.globl	begbss
.globl	begdata

.text
/*===========================================================================*/
/*				MINIX					     */
/*===========================================================================*/
.global MINIX
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
	incl	mon_return
noret:
	movl	%esp, mon_sp	/* save stack pointer for later return */

/* Copy the monitor global descriptor table to the address space of kernel and */
/* switch over to it.  Prot_init() can then update it with immediate effect. */

	sgdt	gdt+GDT_SELECTOR	/* get the monitor gdtr */
	movl	gdt+GDT_SELECTOR+2, %esi	/* absolute address of GDT */
	mov	$gdt, %ebx	/* address of kernel GDT */
	mov	$8*8, %ecx	/* copying eight descriptors */
copygdt:
	movb    %es:(%esi), %al
	movb	%al, (%ebx)
	inc	%esi
	inc	%ebx
	loop	copygdt
	movl	gdt+DS_SELECTOR+2, %eax	/* base of kernel data */
	and	$0x00FFFFFF, %eax	/* only 24 bits */
	add	$gdt, %eax	/* eax = vir2phys(gdt) */
	movl	%eax, gdt+GDT_SELECTOR+2	/* set base of GDT */
	lgdt	gdt+GDT_SELECTOR	/* switch over to kernel GDT */

/* Locate boot parameters, set up kernel segment registers and stack. */
	mov	8(%ebp), %ebx	/* boot parameters offset */
	mov	12(%ebp), %edx	/* boot parameters length */
	mov	16(%ebp), %eax	/* address of a.out headers */
	movl	%eax, aout
	mov	%ds, %ax	/* kernel data */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	mov	$k_boot_stktop, %esp	/* set sp to point to the top of kernel stack */

/* Save boot parameters into these global variables for i386 code */
	movl	%edx, params_size
	movl	%ebx, params_offset
	movl	$SS_SELECTOR, mon_ds

/* Call C startup code to set up a proper environment to run main(). */
	push	%edx
	push	%ebx
	push	$SS_SELECTOR
	push	$DS_SELECTOR
	push	$CS_SELECTOR
	call	cstart	/* cstart(cs, ds, mds, parmoff, parmlen) */
	add	$5*4, %esp

/* Reload gdtr, idtr and the segment registers to global descriptor table set */
/* up by prot_init(). */

	lgdt	gdt+GDT_SELECTOR
	lidt	gdt+IDT_SELECTOR

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
	jmp	main	/* main() */


/*===========================================================================*/
/*				interrupt handlers			     */
/*		interrupt handlers for 386 32-bit protected mode	     */
/*===========================================================================*/

#define PIC_IRQ_HANDLER(irq)	\
	push	$irq							    	;\
	call	irq_handle		/* intr_handle(irq_handlers[irq]) */	;\
	add	$4, %esp						    	;

/*===========================================================================*/
/*				hwint00 - 07				     */
/*===========================================================================*/
/* Note this is a macro, it just looks like a subroutine. */

#define hwint_master(irq) \
	TEST_INT_IN_KERNEL(4, 0f)					;\
									\
	SAVE_PROCESS_CTX(0)						;\
	movl	$0, %ebp	/* for stack trace */			;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	jmp	restart							;\
									\
0:									\
	pusha								;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_master macro */
.balign	16
hwint00:
/* Interrupt routine for irq 0 (the clock). */
	hwint_master(0)

.balign	16
hwint01:
/* Interrupt routine for irq 1 (keyboard) */
	hwint_master(1)

.balign	16
hwint02:
/* Interrupt routine for irq 2 (cascade!) */
	hwint_master(2)

.balign	16
hwint03:
/* Interrupt routine for irq 3 (second serial) */
	hwint_master(3)

.balign	16
hwint04:
/* Interrupt routine for irq 4 (first serial) */
	hwint_master(4)

.balign	16
hwint05:
/* Interrupt routine for irq 5 (XT winchester) */
	hwint_master(5)

.balign	16
hwint06:
/* Interrupt routine for irq 6 (floppy) */
	hwint_master(6)

.balign	16
hwint07:
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
	movl	$0, %ebp	/* for stack trace */			;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	jmp	restart							;\
									\
0:									\
	pusha								;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_slave macro */
.balign	16
hwint08:
/* Interrupt routine for irq 8 (realtime clock) */
	hwint_slave(8)

.balign	16
hwint09:
/* Interrupt routine for irq 9 (irq 2 redirected) */
	hwint_slave(9)

.balign	16
hwint10:
/* Interrupt routine for irq 10 */
	hwint_slave(10)

.balign	16
hwint11:
/* Interrupt routine for irq 11 */
	hwint_slave(11)

.balign	16
hwint12:
/* Interrupt routine for irq 12 */
	hwint_slave(12)

.balign	16
hwint13:
/* Interrupt routine for irq 13 (FPU exception) */
	hwint_slave(13)

.balign	16
hwint14:
/* Interrupt routine for irq 14 (AT winchester) */
	hwint_slave(14)

.balign	16
hwint15:
/* Interrupt routine for irq 15 */
	hwint_slave(15)

/*
 * syscall is only from a process to kernel
 */
.balign 16
.globl syscall_entry
syscall_entry:

	SAVE_PROCESS_CTX(0)

	/* save the pointer to the current process */
	push	%ebp

	/*
	 * pass the syscall arguments from userspace to the handler.
	 * SAVE_PROCESS_CTX() does not clobber these registers, they are still
	 * set as the userspace have set them
	 */
	push	%edx
	push	%ebx
	push	%eax
	push	%ecx

	/* for stack trace */
	movl	$0, %ebp

	call	sys_call

	/* restore the current process pointer and save the return value */
	add	$4 * 4, %esp
	pop	%esi
	mov     %eax, AXREG(%esi)

	jmp	restart


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

	cld

	SAVE_PROCESS_CTX(8)

	/* for stack trace clear %ebp */
	movl	$0, %ebp

	/*
	 * push a pointer to the interrupt state pushed by the cpu and the
	 * vector number pushed by the vector handler just before calling
	 * exception_entry and call the exception handler.
	 */
	push	%esp
	push	$0	/* it's not a nested exception */
	call 	exception_handler

	jmp	restart

exception_entry_nested:

	pusha
	mov	%esp, %eax
	add	$(8 * 4), %eax
	push	%eax
	pushl	$1			/* it's a nested exception */
	call	exception_handler
	add	$8, %esp
	popa

	/* clear the error code and the exception number */
	add	$8, %esp
	/* resume execution at the point of exception */
	iret

/*===========================================================================*/
/*				restart					     */
/*===========================================================================*/
restart:
	call	schedcheck

	/* %eax is set by schedcheck() to the process to run */
	mov	%eax, %ebp	/* will assume P_STACKBASE == 0 */

	cmpl	$0, P_CR3(%ebp)
	jz	0f

	/*
	 * test if the cr3 is loaded with the current value to avoid unnecessary
	 * TLB flushes
	 */
	mov	P_CR3(%ebp), %eax
	mov	%cr3, %ecx
	cmp	%ecx, %eax
	jz	0f
	mov	%eax, %cr3
	mov	%ebp, ptproc
	movl	$0, dirtypde
0:

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

	lldt	P_LDT_SEL(%ebp)	/* enable process' segment descriptors  */
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

divide_error:
	EXCEPTION_NO_ERR_CODE(DIVIDE_VECTOR)

single_step_exception:
	EXCEPTION_NO_ERR_CODE(DEBUG_VECTOR)

nmi:
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
	call	nmi_watchdog_handler
	add	$4, %esp

	/* restore all the important registers as they were before the trap */
	popa
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds

	iret
#endif

breakpoint_exception:
	EXCEPTION_NO_ERR_CODE(BREAKPOINT_VECTOR)

overflow:
	EXCEPTION_NO_ERR_CODE(OVERFLOW_VECTOR)

bounds_check:
	EXCEPTION_NO_ERR_CODE(BOUNDS_VECTOR)

inval_opcode:
	EXCEPTION_NO_ERR_CODE(INVAL_OP_VECTOR)

copr_not_available:
	TEST_INT_IN_KERNEL(4, copr_not_available_in_kernel)
	clts
	cld				/* set direction flag to a known value */
	SAVE_PROCESS_CTX_NON_LAZY(0)
	lea	P_MISC_FLAGS(%ebp), %ebx
	movw	(%ebx), %cx
	and	$MF_FPU_INITIALIZED, %cx
	jnz	0f			/* jump if FPU is already initialized */
	orw	$MF_FPU_INITIALIZED, (%ebx)
	fninit
	jmp	copr_return
0:					/* load FPU context for current process */
	mov	%ss:FP_SAVE_AREA_P(%ebp), %eax
	cmp	$0, osfxsr_feature
	jz	fp_l_no_fxsr		/* FXSR is not avaible. */

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fxrstor	(%eax)
	jmp	copr_return
fp_l_no_fxsr:
	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	frstor	(%eax)
copr_return:
	orw	$MF_USED_FPU, (%ebx)	/* fpu was used during last execution */
	jmp	restart

copr_not_available_in_kernel:
	movl	$NO_NUM, 4(%esp)
	movl	$0, (%esp)
	call	minix_panic

double_fault:
	EXCEPTION_ERR_CODE(DOUBLE_FAULT_VECTOR)

copr_seg_overrun:
	EXCEPTION_NO_ERR_CODE(COPROC_SEG_VECTOR)

inval_tss:
	EXCEPTION_ERR_CODE(INVAL_TSS_VECTOR)

segment_not_present:
	EXCEPTION_ERR_CODE(SEG_NOT_VECTOR)

stack_exception:
	EXCEPTION_ERR_CODE(STACK_FAULT_VECTOR)

general_protection:
	EXCEPTION_ERR_CODE(PROTECTION_VECTOR)

page_fault:
	EXCEPTION_ERR_CODE(PAGE_FAULT_VECTOR)

copr_error:
	EXCEPTION_NO_ERR_CODE(COPROC_ERR_VECTOR)

alignment_check:
	EXCEPTION_NO_ERR_CODE(ALIGNMENT_CHECK_VECTOR)

machine_check:
	EXCEPTION_NO_ERR_CODE(MACHINE_CHECK_VECTOR)

simd_exception:
	EXCEPTION_NO_ERR_CODE(SIMD_EXCEPTION_VECTOR)

/*===========================================================================*/
/*                              lazy_fpu                                     */
/*===========================================================================*/
/* void lazy_fpu(struct proc *pptr)
 * It's called, when we are on kernel stack.
 * Actualy lazy code is just few lines, which check MF_USED_FPU,
 * another part is save_init_fpu().
 */
lazy_fpu:
	push	%ebp
	mov	%esp, %ebp
	push	%eax
	push	%ebx
	push	%ecx
	cmp	$0, fpu_presence		/* Do we have FPU? */
	jz	no_fpu_available
	mov	8(%ebp), %eax			/* Get pptr */
	lea	P_MISC_FLAGS(%eax), %ebx
	movw	(%ebx), %cx
	and	$MF_USED_FPU, %cx
	jz	0f				/* Don't save FPU */
	mov	%ss:FP_SAVE_AREA_P(%eax), %eax
	cmp	$0, osfxsr_feature
	jz	fp_s_no_fxsr			/* FXSR is not avaible. */
	
	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fxsave	(%eax)
	fninit
	jmp	fp_saved
fp_s_no_fxsr:
	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fnsave	(%eax)
	fwait	/* required for compatibility with processors prior pentium */
fp_saved:
	andw	$~MF_USED_FPU, (%ebx)
0:	mov	%cr0, %eax
	or	$0x00000008, %eax		/* Set TS flag */
	mov	%eax, %cr0
no_fpu_available:
	pop	%ecx
	pop	%ebx
	pop	%eax
	pop	%ebp
	ret

/*===========================================================================*/
/*				write_cr3				     */
/*===========================================================================*/
/* PUBLIC void write_cr3(unsigned long value); */
write_cr3:
	push    %ebp
	mov     %esp, %ebp
	mov	8(%ebp), %eax
	mov	%cr3, %ecx
	cmp	%ecx, %eax
	jz	0f
	mov	%eax, %cr3
	movl	$0, dirtypde
0:
	pop     %ebp
	ret

/*===========================================================================*/
/*				level0_call				     */
/*===========================================================================*/
level0_call:
/*
 * which level0 function to call was passed here by putting it in %eax
 */
	SAVE_PROCESS_CTX(0)
	/* for stack trace */
	movl	$0, %ebp
	/*
	 * the function to call is in %eax, set in userspace. SAVE_PROCESS_CTX()
	 * does not clobber this register so we can use it straightaway
	 */
	call	*%eax
	jmp	restart


/*===========================================================================*/
/*				reload_cr3				     */
/*===========================================================================*/
/* PUBLIC void reload_cr3(void); */
reload_cr3:
	push    %ebp
	mov     %esp, %ebp
	movl	$0, dirtypde
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
 * this stack is used temporarily for booting only. We switch to a proper kernel
 * stack after the first trap to kernel
 */
.globl k_boot_stktop
k_boot_stack:
.space	4096		/* kernel stack */ /* FIXME use macro here */
k_boot_stktop:		/* top of kernel stack */
