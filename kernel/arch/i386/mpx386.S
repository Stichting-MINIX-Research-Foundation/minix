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
.globl	save
.globl reload_cr3
.globl write_cr3

.globl	errexception
.globl	exception1
.globl	exception

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
.globl	params_size
.globl	params_offset
.globl	mon_ds
.globl  schedcheck
.globl  dirtypde

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

.globl	s_call
.globl	p_s_call
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
	mov	$k_stktop, %esp	/* set sp to point to the top of kernel stack */

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

/*===========================================================================*/
/*				hwint00 - 07				     */
/*===========================================================================*/
/* Note this is a macro, it just looks like a subroutine. */
#define hwint_master(irq)	\
	call	save			/* save interrupted process state */;\
	push	$irq							    ;\
	call	irq_handle		/* irq_handle(irq) 		  */;\
	pop	%ecx							    ;\
	movb	$END_OF_INT, %al						    ;\
	outb	$INT_CTL			/* reenable master 8259		  */;\
	ret				/* restart (another) process      */

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
	call	save			/* save interrupted process state */;\
	push	$irq							    ;\
	call	irq_handle		/* irq_handle(irq) 		  */;\
	pop	%ecx							    ;\
	movb	$END_OF_INT, %al						    ;\
	outb	$INT_CTL			/* reenable master 8259		  */;\
	outb	$INT2_CTL		/* reenable slave 8259		  */;\
	ret				/* restart (another) process      */

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

/*===========================================================================*/
/*				save					     */
/*===========================================================================*/
/*
 * Save for protected mode. 
 * This is much simpler than for 8086 mode, because the stack already points 
 * into the process table, or has already been switched to the kernel stack. 
 */

.balign	16
save:
	cld	/* set direction flag to a known value */
	pushal	/* save "general" registers */
	pushw	%ds	/* save ds */
	pushw	%es	/* save es */
	pushw	%fs	/* save fs */
	pushw	%gs	/* save gs */
	mov	%ss, %dx	/* ss is kernel data segment */
	mov	%dx, %ds	/* load rest of kernel segments */
	mov	%dx, %es	/* kernel does not use fs, gs */
	mov	%esp, %eax	/* prepare to return */
	incb	k_reenter	/* from -1 if not reentering */
	jne	set_restart1	/* stack is already kernel stack */
	mov	$k_stktop, %esp
	push	$restart	/* build return address for int handler */
	xor	%ebp, %ebp	/* for stacktrace */
	jmp	*RETADR-P_STACKBASE(%eax)

.balign	4
set_restart1:
	push	$restart1
	jmp	*RETADR-P_STACKBASE(%eax)

/*===========================================================================*/
/*				_s_call					     */
/*===========================================================================*/
.balign	16
s_call:
p_s_call:
	cld	/* set direction flag to a known value */
	sub	$4, %esp	/* skip RETADR */
	pusha			/* save "general" registers */
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs

	mov	%ss, %si	/* ss is kernel data segment */
	mov	%si, %ds	/* load rest of kernel segments */
	mov	%si, %es	/* kernel does not use fs, gs */
	incb	k_reenter	/* increment kernel entry count */
	mov	%esp, %esi	/* assumes P_STACKBASE == 0 */
	mov	$k_stktop, %esp
	xor	%ebp, %ebp	/* for stacktrace */
/* end of inline save */
/* now set up parameters for sys_call() */
	push	%edx	/* event set or flags bit map  */
	push	%ebx	/* pointer to user message */
	push	%eax	/* source / destination */
	push	%ecx	/* call number (ipc primitive to use) */

	call	sys_call	/* sys_call(call_nr, src_dst, m_ptr, bit_map) */
/* caller is now explicitly in proc_ptr */
	mov	%eax, AXREG(%esi)

/* Fall into code to restart proc/task running. */

/*===========================================================================*/
/*				restart					     */
/*===========================================================================*/
restart:

/* Restart the current process or the next process if it is set.  */

	cli
	call	schedcheck
	movl	proc_ptr, %esp	/* will assume P_STACKBASE == 0 */
	lldt	P_LDT_SEL(%esp)	/* enable process' segment descriptors  */
	cmpl	$0, P_CR3(%esp)
	jz	0f
	mov	P_CR3(%esp), %eax
	cmpl	loadedcr3, %eax
	jz	0f
	mov	%eax, %cr3
	mov	%eax, loadedcr3
	mov	proc_ptr, %eax
	mov	%eax, ptproc
	movl	$0, dirtypde
0:
	lea	P_STACKTOP(%esp), %eax	/* arrange for next interrupt */
	movl	%eax, tss+TSS3_S_SP0	/* to save state in process table */
restart1:
	decb	k_reenter
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	popal
	add	$4, %esp	/* skip return adr */
	iret	/* continue process */

/*===========================================================================*/
/*				exception handlers			     */
/*===========================================================================*/
divide_error:
	push	$DIVIDE_VECTOR
	jmp	handle_exception

single_step_exception:
	push	$DEBUG_VECTOR
	jmp	handle_exception

nmi:
	push	$NMI_VECTOR
	jmp	handle_exception

breakpoint_exception:
	push	$BREAKPOINT_VECTOR
	jmp	handle_exception

overflow:
	push	$OVERFLOW_VECTOR
	jmp	handle_exception

bounds_check:
	push	$BOUNDS_VECTOR
	jmp	handle_exception

inval_opcode:
	push	$INVAL_OP_VECTOR
	jmp	handle_exception

copr_not_available:
	push	$COPROC_NOT_VECTOR
	jmp	handle_exception

double_fault:
	push	$DOUBLE_FAULT_VECTOR
	jmp	errexception

copr_seg_overrun:
	push	$COPROC_SEG_VECTOR
	jmp	handle_exception

inval_tss:
	push	$INVAL_TSS_VECTOR
	jmp	errexception

segment_not_present:
	push	$SEG_NOT_VECTOR
	jmp	errexception

stack_exception:
	push	$STACK_FAULT_VECTOR
	jmp	errexception

general_protection:
	push	$PROTECTION_VECTOR
	jmp	errexception

page_fault:
	push	$PAGE_FAULT_VECTOR
	push	%eax
	mov	%cr2, %eax
	movl	%eax, %ss:pagefaultcr2
	pop	%eax
	jmp	errexception

copr_error:
	push	$COPROC_ERR_VECTOR
	jmp	handle_exception

/*===========================================================================*/
/*				handle_exception				     */
/*===========================================================================*/
/* This is called for all exceptions which do not push an error code. */

.balign	16
handle_exception:
	movl	$0, %ss:trap_errno	/* clear trap_errno */
	pop	%ss:ex_number
	jmp	exception1

/*===========================================================================*/
/*				errexception				     */
/*===========================================================================*/
/* This is called for all exceptions which push an error code. */

.balign	16
errexception:
	pop	%ss:ex_number
	pop	%ss:trap_errno
exception1:
/* Common for all exceptions. */
	movl	%esp, %ss:old_eax_ptr		/* where will eax be saved */
	subl	$PCREG-AXREG, %ss:old_eax_ptr	/* here */

	push	%eax	/* eax is scratch register */

	mov	0+4(%esp), %eax	/* old eip */
	movl	%eax, %ss:old_eip
	mov	%esp, %eax
	add	$4, %eax
	mov	%eax, %ss:old_eip_ptr
	movzwl	4+4(%esp), %eax	/* old cs */
	movl	%eax, %ss:old_cs
	mov	8+4(%esp), %eax	/* old eflags */
	movl	%eax, %ss:old_eflags

	pop	%eax
	call	save
	push	pagefaultcr2
	push	old_eax_ptr
	push	old_eip_ptr
	push	old_eflags
	push	old_cs
	push	old_eip
	push	trap_errno
	push	ex_number
	call	exception	/* (ex_number, trap_errno, old_eip, */
				/* old_cs, old_eflags) */
	add	$8*4, %esp
	ret

/*===========================================================================*/
/*				write_cr3				     */
/*===========================================================================*/
/* PUBLIC void write_cr3(unsigned long value); */
write_cr3:
	push    %ebp
	mov     %esp, %ebp
	mov	8(%ebp), %eax
	cmpl	loadedcr3, %eax
	jz	0f
	mov	%eax, %cr3
	mov	%eax, loadedcr3
	movl	$0, dirtypde
0:
	pop     %ebp
	ret

/*===========================================================================*/
/*				level0_call				     */
/*===========================================================================*/
level0_call:
/*
 * which level0 function to call was passed here by putting it in eax, so
 * we get that from the saved state.
 */
	call	save
	movl	proc_ptr, %eax
	movl	AXREG(%eax), %eax
	jmp	*%eax


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
k_stack:
.space	K_STACK_BYTES	/* kernel stack */
k_stktop:
/* top of kernel stack */
.lcomm	ex_number, 4
.lcomm	trap_errno, 4
.lcomm  old_eip_ptr, 4
.lcomm  old_eax_ptr, 4
.lcomm	old_eip, 4
.lcomm	old_cs, 4
.lcomm	old_eflags, 4
.lcomm  pagefaultcr2, 4
.lcomm  loadedcr3, 4
