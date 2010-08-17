/*
 * This file is part of the lowest layer of the MINIX kernel.  (The other part 
 * is "proc.c".)  The lowest layer does process switching and message handling. 
 * Furthermore it contains the assembler startup code for Minix and the 32-bit 
 * interrupt handlers.  It cooperates with the code in "start.c" to set up a  
 * good environment for main(). 
 *
 * Kernel is entered either because of kernel-calls, ipc-calls, interrupts or
 * exceptions. TSS is set so that the kernel stack is loaded. The user context is
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
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "kernel/proc.h"
#include "sconst.h"
#include "multiboot.h"

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

IMPORT(copr_not_available_handler)
IMPORT(params_size)
IMPORT(params_offset)
IMPORT(mon_ds)
IMPORT(switch_to_user)

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
.short	0x03FD	
	nop	/* extra byte to sync up disassembler */
	
/* Multiboot header here*/

.balign 8

multiboot_magic:
	.long MULTIBOOT_HEADER_MAGIC
multiboot_flags:
	.long MULTIBOOT_FLAGS
multiboot_checksum:
	.long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_FLAGS)
multiboot_header_addr:
	.long (MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_ENTRY_OFFSET + multiboot_magic)
multiboot_load_addr:
	.long MULTIBOOT_LOAD_ADDRESS
multiboot_load_end_addr:
	.long 0
multiboot_bss_end_addr:
	.long 0
multiboot_entry_addr:
	.long (MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_ENTRY_OFFSET + multiboot_init)
/* Video mode */
multiboot_mode_type:
	.long MULTIBOOT_VIDEO_MODE_EGA
multiboot_width:
	.long MULTIBOOT_CONSOLE_COLS
multiboot_height:
	.long MULTIBOOT_CONSOLE_LINES
multiboot_depth:
	.long 0

over_flags:
/* Set up a C stack frame on the monitor stack.  (The monitor sets cs and ds */
/* right.  The ss descriptor still references the monitor data segment.) */
	movzwl	%sp, %esp	/* monitor stack is a 16 bit stack */

.globl kernel_init
kernel_init: /* after pre-init*/
	push	%ebp
	mov	%esp, %ebp
	push	%esi
	push	%edi
	cmp	$0, 4(%ebp)	/* monitor return vector is */
	je	noret	/* nonzero if return possible */
	incl	_C_LABEL(mon_return)
noret:
	movl	%esp, _C_LABEL(mon_sp) /* save stack pointer for later return */

/* Copy the monitor global descriptor table to the address space of kernel and */
/* switch over to it.  Prot_init() can then update it with immediate effect. */

	sgdt	_C_LABEL(gdt)+GDT_SELECTOR	/* get the monitor gdtr */
	movl	_C_LABEL(gdt)+GDT_SELECTOR+2, %esi	/* absolute address of GDT */
	mov	$_C_LABEL(gdt), %ebx	/* address of kernel GDT */
	mov	$8*8, %ecx	/* copying eight descriptors */
copygdt:
	movb    %es:(%esi), %al
	movb	%al, (%ebx)
	inc	%esi
	inc	%ebx
	loop	copygdt
	movl	_C_LABEL(gdt)+DS_SELECTOR+2, %eax /* base of kernel data */
	and	$0x00FFFFFF, %eax	/* only 24 bits */
	add	$_C_LABEL(gdt), %eax	/* eax = vir2phys(gdt) */
	movl	%eax, _C_LABEL(gdt)+GDT_SELECTOR+2 /* set base of GDT */
	lgdt	_C_LABEL(gdt)+GDT_SELECTOR	/* switch over to kernel GDT */

/* Locate boot parameters, set up kernel segment registers and stack. */
	mov	8(%ebp), %ebx	/* boot parameters offset */
	mov	12(%ebp), %edx	/* boot parameters length */
	mov	16(%ebp), %eax	/* address of a.out headers */
	movl	%eax, _C_LABEL(aout)
	mov	%ds, %ax	/* kernel data */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	mov	$_C_LABEL(k_boot_stktop), %esp	/* set sp to point to the top of kernel stack */

/* Save boot parameters into these global variables for i386 code */
	movl	%edx, _C_LABEL(params_size)
	movl	%ebx, _C_LABEL(params_offset)
	movl	$SS_SELECTOR, _C_LABEL(mon_ds)

/* Call C startup code to set up a proper environment to run main(). */
	push	%edx
	push	%ebx
	push	$SS_SELECTOR
	push	$DS_SELECTOR
	push	$CS_SELECTOR
	call	_C_LABEL(cstart) /* cstart(cs, ds, mds, parmoff, parmlen) */
	add	$5*4, %esp

/* Reload gdtr, idtr and the segment registers to global descriptor table set */
/* up by prot_init(). */

	lgdt	_C_LABEL(gdt)+GDT_SELECTOR
	lidt	_C_LABEL(gdt)+IDT_SELECTOR

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
	jmp	_C_LABEL(main)	/* main() */


/*===========================================================================*/
/*				interrupt handlers			     */
/*		interrupt handlers for 386 32-bit protected mode	     */
/*===========================================================================*/

#define PIC_IRQ_HANDLER(irq)	\
	push	$irq							    	;\
	call	_C_LABEL(irq_handle)	/* intr_handle(irq_handlers[irq]) */	;\
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
	call	_C_LABEL(context_stop)					;\
	add	$4, %esp						;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	jmp	_C_LABEL(switch_to_user)				;\
									\
0:									\
	pusha								;\
	call	_C_LABEL(context_stop_idle)				;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	CLEAR_IF(10*4(%esp))						;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_master macro */
ENTRY(hwint00)
/* Interrupt routine for irq 0 (the clock). */
	hwint_master(0)

ENTRY(hwint01)
/* Interrupt routine for irq 1 (keyboard) */
	hwint_master(1)

ENTRY(hwint02)
/* Interrupt routine for irq 2 (cascade!) */
	hwint_master(2)

ENTRY(hwint03)
/* Interrupt routine for irq 3 (second serial) */
	hwint_master(3)

ENTRY(hwint04)
/* Interrupt routine for irq 4 (first serial) */
	hwint_master(4)

ENTRY(hwint05)
/* Interrupt routine for irq 5 (XT winchester) */
	hwint_master(5)

ENTRY(hwint06)
/* Interrupt routine for irq 6 (floppy) */
	hwint_master(6)

ENTRY(hwint07)
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
	call	_C_LABEL(context_stop)					;\
	add	$4, %esp						;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	jmp	_C_LABEL(switch_to_user)				;\
									\
0:									\
	pusha								;\
	call	_C_LABEL(context_stop_idle)				;\
	PIC_IRQ_HANDLER(irq)						;\
	movb	$END_OF_INT, %al					;\
	outb	$INT_CTL	/* reenable interrupts in master pic */	;\
	outb	$INT2_CTL	/* reenable slave 8259		  */	;\
	CLEAR_IF(10*4(%esp))						;\
	popa								;\
	iret								;

/* Each of these entry points is an expansion of the hwint_slave macro */
ENTRY(hwint08)
/* Interrupt routine for irq 8 (realtime clock) */
	hwint_slave(8)

ENTRY(hwint09)
/* Interrupt routine for irq 9 (irq 2 redirected) */
	hwint_slave(9)

ENTRY(hwint10)
/* Interrupt routine for irq 10 */
	hwint_slave(10)

ENTRY(hwint11)
/* Interrupt routine for irq 11 */
	hwint_slave(11)

ENTRY(hwint12)
/* Interrupt routine for irq 12 */
	hwint_slave(12)

ENTRY(hwint13)
/* Interrupt routine for irq 13 (FPU exception) */
	hwint_slave(13)

ENTRY(hwint14)
/* Interrupt routine for irq 14 (AT winchester) */
	hwint_slave(14)

ENTRY(hwint15)
/* Interrupt routine for irq 15 */
	hwint_slave(15)

/*
 * IPC is only from a process to kernel
 */
ENTRY(ipc_entry)

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
	call	_C_LABEL(context_stop)
	add	$4, %esp

	call	_C_LABEL(do_ipc)

	/* restore the current process pointer and save the return value */
	add	$3 * 4, %esp
	pop	%esi
	mov     %eax, AXREG(%esi)

	jmp	_C_LABEL(switch_to_user)


/*
 * kernel call is only from a process to kernel
 */
ENTRY(kernel_call_entry)

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
	call	_C_LABEL(context_stop)
	add	$4, %esp

	call	_C_LABEL(kernel_call)

	/* restore the current process pointer and save the return value */
	add	$8, %esp

	jmp	_C_LABEL(switch_to_user)


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
	call	_C_LABEL(context_stop)
	add	$4, %esp

	/*
	 * push a pointer to the interrupt state pushed by the cpu and the
	 * vector number pushed by the vector handler just before calling
	 * exception_entry and call the exception handler.
	 */
	push	%esp
	push	$0	/* it's not a nested exception */
	call 	_C_LABEL(exception_handler)

	jmp	_C_LABEL(switch_to_user)

exception_entry_nested:

	pusha
	mov	%esp, %eax
	add	$(8 * 4), %eax
	push	%eax
	pushl	$1			/* it's a nested exception */
	call	_C_LABEL(exception_handler)
	add	$8, %esp
	popa

	/* clear the error code and the exception number */
	add	$8, %esp
	/* resume execution at the point of exception */
	iret

/*===========================================================================*/
/*				restart					     */
/*===========================================================================*/
ENTRY(restore_user_context)
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

LABEL(divide_error)
	EXCEPTION_NO_ERR_CODE(DIVIDE_VECTOR)

LABEL(single_step_exception)
	EXCEPTION_NO_ERR_CODE(DEBUG_VECTOR)

LABEL(nmi)
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
	call	_C_LABEL(nmi_watchdog_handler)
	add	$4, %esp

	/* restore all the important registers as they were before the trap */
	popa
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds

	iret
#endif

LABEL(breakpoint_exception)
	EXCEPTION_NO_ERR_CODE(BREAKPOINT_VECTOR)

LABEL(overflow)
	EXCEPTION_NO_ERR_CODE(OVERFLOW_VECTOR)

LABEL(bounds_check)
	EXCEPTION_NO_ERR_CODE(BOUNDS_VECTOR)

LABEL(inval_opcode)
	EXCEPTION_NO_ERR_CODE(INVAL_OP_VECTOR)

LABEL(copr_not_available)
	TEST_INT_IN_KERNEL(4, copr_not_available_in_kernel)
	cld			/* set direction flag to a known value */
	SAVE_PROCESS_CTX(0)
	/* stop user process cycles */
	push	%ebp
	mov	$0, %ebp
	call	_C_LABEL(context_stop)
	jmp	_C_LABEL(copr_not_available_handler)

copr_not_available_in_kernel:
	pushl	$0
	pushl	$COPROC_NOT_VECTOR
	jmp	exception_entry_nested

LABEL(double_fault)
	EXCEPTION_ERR_CODE(DOUBLE_FAULT_VECTOR)

LABEL(copr_seg_overrun)
	EXCEPTION_NO_ERR_CODE(COPROC_SEG_VECTOR)

LABEL(inval_tss)
	EXCEPTION_ERR_CODE(INVAL_TSS_VECTOR)

LABEL(segment_not_present)
	EXCEPTION_ERR_CODE(SEG_NOT_VECTOR)

LABEL(stack_exception)
	EXCEPTION_ERR_CODE(STACK_FAULT_VECTOR)

LABEL(general_protection)
	EXCEPTION_ERR_CODE(PROTECTION_VECTOR)

LABEL(page_fault)
	EXCEPTION_ERR_CODE(PAGE_FAULT_VECTOR)

LABEL(copr_error)
	EXCEPTION_NO_ERR_CODE(COPROC_ERR_VECTOR)

LABEL(alignment_check)
	EXCEPTION_NO_ERR_CODE(ALIGNMENT_CHECK_VECTOR)

LABEL(machine_check)
	EXCEPTION_NO_ERR_CODE(MACHINE_CHECK_VECTOR)

LABEL(simd_exception)
	EXCEPTION_NO_ERR_CODE(SIMD_EXCEPTION_VECTOR)

/*===========================================================================*/
/*				reload_cr3				     */
/*===========================================================================*/
/* PUBLIC void reload_cr3(void); */
ENTRY(reload_cr3)
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
k_boot_stack:
.space	4096		/* kernel stack */ /* FIXME use macro here */
LABEL(k_boot_stktop)	/* top of kernel stack */
