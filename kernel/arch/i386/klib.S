/* sections */


#include <minix/config.h>
#include <minix/const.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "sconst.h"

/*
 * This file contains a number of assembly code utility routines needed by the
 * kernel.  They are:
 */

.globl	_monitor/* exit Minix and return to the monitor */
.globl	_int86	/* let the monitor make an 8086 interrupt call */
#ifdef __ACK__
.globl	_exit	/* dummy for library routines */
.globl	__exit	/* dummy for library routines */
.globl	___exit	/* dummy for library routines */
#endif
.globl	___main	/* dummy for GCC */
.globl	_phys_insw	/* transfer data from (disk controller) port to memory */
.globl	_phys_insb	/* likewise byte by byte */
.globl	_phys_outsw	/* transfer data from memory to (disk controller) port */
.globl	_phys_outsb	/* likewise byte by byte */
.globl	_phys_copy	/* copy data from anywhere to anywhere in memory */
.globl	_phys_copy_fault	/* phys_copy pagefault */
.globl	_phys_copy_fault_in_kernel /* phys_copy pagefault in kernel */
.globl	_phys_memset	/* write pattern anywhere in memory */
.globl	_mem_rdw	/* copy one word from [segment:offset] */
.globl	_reset	/* reset the system */
.globl	_halt_cpu/* halts the current cpu when idle */
.globl	_read_cpu_flags	/* read the cpu flags */
.globl	_read_cr0	/* read cr0 */
.globl	_read_cr2	/* read cr2 */
.globl	_getcr3val
.globl	_write_cr0	/* write a value in cr0 */
.globl	_read_cr3
.globl	_write_cr3
.globl	_read_cr4
.globl	_write_cr4

.globl	_catch_pagefaults
.globl	_read_ds
.globl  _read_cs
.globl	_read_ss
.globl	_idt_reload	/* reload idt when returning to monitor. */

.globl	_fninit			/* non-waiting FPU initialization */
.globl	_fnstsw			/* store status word (non-waiting) */
.globl	_fnstcw			/* store control word (non-waiting) */
.globl	_fxsave	
.globl	_fnsave	
.globl	_fxrstor	
.globl	_frstor	
.globl	_clts	

/*
 * The routines only guarantee to preserve the registers the C compiler 
 * expects to be preserved (ebx, esi, edi, ebp, esp, segment registers, and 
 * direction bit in the flags). 
 */

.text
/*===========================================================================*/
/*				monitor					     */
/*===========================================================================*/
/* PUBLIC void monitor(); */
/* Return to the monitor. */

_monitor:
	movl	_mon_sp, %esp	/* restore monitor stack pointer */
	movw	$SS_SELECTOR, %dx	/* monitor data segment */
	mov	%dx, %ds
	mov	%dx, %es
	mov	%dx, %fs
	mov	%dx, %gs
	mov	%dx, %ss
	pop	%edi
	pop	%esi
	pop	%ebp
	lretw	/* return to the monitor */


/*===========================================================================*/
/*				int86					     */
/*===========================================================================*/
/* PUBLIC void int86(); */
_int86:
	cmpb	$0, _mon_return	/* is the monitor there? */
	jne	0f
	movb	$0x01, %ah	/* an int 13 error seems appropriate */
	movb	%ah, _reg86+0	/* reg86.w.f = 1 (set carry flag) */
	movb	%ah, _reg86+13	/* reg86.b.ah = 0x01 = "invalid command" */
	ret
0:
	push	%ebp	/* save C registers */
	push	%esi
	push	%edi
	push	%ebx
	pushf	/* save flags */
	cli	/* no interruptions */

	inb	$INT2_CTLMASK
	movb	%al, %ah
	inb	$INT_CTLMASK
	push	%eax	/* save interrupt masks */
	movl	_irq_use, %eax	/* map of in-use IRQ's */
	and	$~(1<<CLOCK_IRQ), %eax	/* keep the clock ticking */
	outb	$INT_CTLMASK	/* enable all unused IRQ's and vv. */
	movb	%ah, %al
	outb	$INT2_CTLMASK

	mov	$SS_SELECTOR, %eax	/* monitor data segment */
	mov	%ax, %ss
	xchgl	_mon_sp, %esp	/* switch stacks */
	push	_reg86+36	/* parameters used in INT call */
	push	_reg86+32
	push	_reg86+28
	push	_reg86+24
	push	_reg86+20
	push	_reg86+16
	push	_reg86+12
	push	_reg86+8
	push	_reg86+4
	push	_reg86+0
	mov	%ax, %ds	/* remaining data selectors */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	push	%cs
	push	$return	/* kernel return address and selector */
        ljmpw    *20+2*4+10*4+2*4(%esp)
return:
	pop	_reg86+0
	pop	_reg86+4
	pop	_reg86+8
	pop	_reg86+12
	pop	_reg86+16
	pop	_reg86+20
	pop	_reg86+24
	pop	_reg86+28
	pop	_reg86+32
	pop	_reg86+36
	lgdt	_gdt+GDT_SELECTOR	/* reload global descriptor table */
        ljmp    $CS_SELECTOR, $csinit
csinit:
	mov	$DS_SELECTOR, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	xchgl	_mon_sp, %esp	/* unswitch stacks */
	lidt	_gdt+IDT_SELECTOR	/* reload interrupt descriptor table */

#ifdef CONFIG_APIC
	cmpl	$0x0, lapic_addr
	jne	3f
	mov	$0, %ebx
	jmp	4f

3:
	mov	$FLAT_DS_SELECTOR, %ebx
	mov	%bx, %fs
	movl	lapic_addr, %eax
	add	$0x20, %eax
	.byte 0x64; mov	(%eax), %ebx
	and	$0xFF000000, %ebx
	shr	$24, %ebx
	movzb	%bl, %ebx

4:
	add	$apicid2cpuid, %ebx
	movzb	(%ebx), %eax
	shl	$3, %eax
	mov	%eax, %ebx
	add	$TSS_SELECTOR, %eax
	addl	_gdt+DESC_ACCESS, %eax
	and	$~0x02, %eax
	ltr	%bx	/* set TSS register */

	mov	$DS_SELECTOR, %eax
	mov	%ax, %fs

#endif /* CONFIG_APIC */

	pop	%eax
	outb	$INT_CTLMASK	/* restore interrupt masks */
	movb	%ah, %al
	outb	$INT2_CTLMASK

6:
	addl	%ecx, _lost_ticks	/* record lost clock ticks */

	popf	/* restore flags */
	pop	%ebx	/* restore C registers */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret

/*===========================================================================*/
/*				exit					     */
/*===========================================================================*/
/*
 * PUBLIC void exit(); 
 * Some library routines use exit, so provide a dummy version. 
 * Actual calls to exit cannot occur in the kernel. 
 * GNU CC likes to call ___main from main() for nonobvious reasons. 
 */
#ifdef __ACK__
_exit:
__exit:
___exit:
	sti
	jmp	___exit
#endif

___main:
	ret


/*===========================================================================*/
/*				phys_insw				     */
/*===========================================================================*/
/*
 * PUBLIC void phys_insw(Port_t port, phys_bytes buf, size_t count); 
 * Input an array from an I/O port.  Absolute address version of insw(). 
 */

_phys_insw:
	push	%ebp
	mov	%esp, %ebp
	cld
	push	%edi
	push	%es

	mov	$FLAT_DS_SELECTOR, %ecx
	mov	%cx, %es
	mov	8(%ebp), %edx	/* port to read from */
	mov	12(%ebp), %edi	/* destination addr */
	mov	16(%ebp), %ecx	/* byte count */
	shr	$1, %ecx	/* word count */
	rep insw	/* input many words */
	pop	%es
	pop	%edi
	pop	%ebp
	ret


/*===========================================================================*/
/*				phys_insb				     */
/*===========================================================================*/
/*
 * PUBLIC void phys_insb(Port_t port, phys_bytes buf, size_t count); 
 * Input an array from an I/O port.  Absolute address version of insb(). 
 */

_phys_insb:
	push	%ebp
	mov	%esp, %ebp
	cld
	push	%edi
	push	%es

	mov	$FLAT_DS_SELECTOR, %ecx
	mov	%cx, %es
	mov	8(%ebp), %edx	/* port to read from */
	mov	12(%ebp), %edi	/* destination addr */
	mov	16(%ebp), %ecx	/* byte count */
	rep insb	/* input many bytes */
	pop	%es
	pop	%edi
	pop	%ebp
	ret


/*===========================================================================*/
/*				phys_outsw				     */
/*===========================================================================*/
/*
 * PUBLIC void phys_outsw(Port_t port, phys_bytes buf, size_t count); 
 * Output an array to an I/O port.  Absolute address version of outsw(). 
 */

.balign	16
_phys_outsw:
	push	%ebp
	mov	%esp, %ebp
	cld
	push	%esi
	push	%ds

	mov	$FLAT_DS_SELECTOR, %ecx
	mov	%cx, %ds
	mov	8(%ebp), %edx	/* port to write to */
	mov	12(%ebp), %esi	/* source addr */
	mov	16(%ebp), %ecx	/* byte count */
	shr	$1, %ecx	/* word count */
	rep outsw	/* output many words */
	pop	%ds
	pop	%esi
	pop	%ebp
	ret


/*===========================================================================*/
/*				phys_outsb				     */
/*===========================================================================*/
/* 
 * PUBLIC void phys_outsb(Port_t port, phys_bytes buf, size_t count);
 * Output an array to an I/O port.  Absolute address version of outsb().
 */

.balign	16
_phys_outsb:
	push	%ebp
	mov	%esp, %ebp
	cld
	push	%esi
	push	%ds

	mov	$FLAT_DS_SELECTOR, %ecx
	mov	%cx, %ds
	mov	8(%ebp), %edx	/* port to write to */
	mov	12(%ebp), %esi	/* source addr */
	mov	16(%ebp), %ecx	/* byte count */
	rep outsb	/* output many bytes */
	pop	%ds
	pop	%esi
	pop	%ebp
	ret


/*===========================================================================*/
/*				phys_copy				     */
/*===========================================================================*/
/*
 * PUBLIC phys_bytes phys_copy(phys_bytes source, phys_bytes destination,
 *			phys_bytes bytecount); 
 * Copy a block of physical memory. 
 */

	PC_ARGS = 4+4+4+4	/* 4 + 4 + 4 */
/*		es edi esi eip	 src dst len */

.balign	16
_phys_copy:
	cld
	push	%esi
	push	%edi
	push	%es

	mov	$FLAT_DS_SELECTOR, %eax
	mov	%ax, %es

	mov	PC_ARGS(%esp), %esi
	mov	PC_ARGS+4(%esp), %edi
	mov	PC_ARGS+4+4(%esp), %eax

	cmp	$10, %eax	/* avoid align overhead for small counts */
	jb	pc_small
	mov	%esi, %ecx	/* align source, hope target is too */
	neg	%ecx
	and	$3, %ecx	/* count for alignment */
	sub	%ecx, %eax

	rep 	movsb %es:(%esi), %es:(%edi)
	mov	%eax, %ecx
	shr	$2, %ecx	/* count of dwords */

	rep 	movsl %es:(%esi), %es:(%edi)
	and	$3, %eax
pc_small:
	xchg	%eax, %ecx	/* remainder */

	rep 	movsb %es:(%esi), %es:(%edi)

	mov	$0, %eax		/* 0 means: no fault */
_phys_copy_fault:			/* kernel can send us here */
	pop	%es
	pop	%edi
	pop	%esi
	ret

_phys_copy_fault_in_kernel:			/* kernel can send us here */
	pop	%es
	pop	%edi
	pop	%esi
	mov	%cr2, %eax
	ret

/*===========================================================================*/
/*				copy_msg_from_user			     */
/*===========================================================================*/
/*
 * int copy_msg_from_user(struct proc * p, message * user_mbuf, message * dst);
 *
 * Copies a message of 36 bytes from user process space to a kernel buffer. This
 * function assumes that the process address space is installed (cr3 loaded) and
 * the local descriptor table of this process is loaded too.
 *
 * The %gs segment register is used to access the userspace memory. We load the
 * process' data segment in this register.
 *
 * This function from the callers point of view either succeeds or returns an
 * error which gives the caller a chance to respond accordingly. In fact it
 * either succeeds or if it generates a pagefault, general protection or other
 * exception, the trap handler has to redirect the execution to
 * __user_copy_msg_pointer_failure where the error is reported to the caller
 * without resolving the pagefault. It is not kernel's problem to deal with
 * wrong pointers from userspace and the caller should return an error to
 * userspace as if wrong values or request were passed to the kernel
 */

.balign 16
.globl _copy_msg_from_user
_copy_msg_from_user:
	push	%gs

	mov	8(%esp), %eax
	movw	DSREG(%eax), %gs

	/* load the source pointer */
	mov	12(%esp), %ecx
	/* load the destination pointer */
	mov	16(%esp), %edx

	mov	%gs:0*4(%ecx), %eax
	mov	%eax, 0*4(%edx)
	mov	%gs:1*4(%ecx), %eax
	mov	%eax, 1*4(%edx)
	mov	%gs:2*4(%ecx), %eax
	mov	%eax, 2*4(%edx)
	mov	%gs:3*4(%ecx), %eax
	mov	%eax, 3*4(%edx)
	mov	%gs:4*4(%ecx), %eax
	mov	%eax, 4*4(%edx)
	mov	%gs:5*4(%ecx), %eax
	mov	%eax, 5*4(%edx)
	mov	%gs:6*4(%ecx), %eax
	mov	%eax, 6*4(%edx)
	mov	%gs:7*4(%ecx), %eax
	mov	%eax, 7*4(%edx)
	mov	%gs:8*4(%ecx), %eax
	mov	%eax, 8*4(%edx)

.globl ___copy_msg_from_user_end
___copy_msg_from_user_end:

	pop	%gs

	movl	$0, %eax
	ret

/*===========================================================================*/
/*				copy_msg_to_user			     */
/*===========================================================================*/
/*
 * void copy_msg_to_user(struct proc * p, message * src, message * user_mbuf);
 *
 * Copies a message of 36 bytes to user process space from a kernel buffer. This
 * function assumes that the process address space is installed (cr3 loaded) and
 * the local descriptor table of this process is loaded too.
 *
 * All the other copy_msg_from_user() comments apply here as well!
 */

.balign 16
.globl _copy_msg_to_user
_copy_msg_to_user:
	push	%gs

	mov	8(%esp), %eax
	movw	DSREG(%eax), %gs

	/* load the source pointer */
	mov	12(%esp), %ecx
	/* load the destination pointer */
	mov	16(%esp), %edx

	mov	0*4(%ecx), %eax
	mov	%eax, %gs:0*4(%edx)
	mov	1*4(%ecx), %eax
	mov	%eax, %gs:1*4(%edx)
	mov	2*4(%ecx), %eax
	mov	%eax, %gs:2*4(%edx)
	mov	3*4(%ecx), %eax
	mov	%eax, %gs:3*4(%edx)
	mov	4*4(%ecx), %eax
	mov	%eax, %gs:4*4(%edx)
	mov	5*4(%ecx), %eax
	mov	%eax, %gs:5*4(%edx)
	mov	6*4(%ecx), %eax
	mov	%eax, %gs:6*4(%edx)
	mov	7*4(%ecx), %eax
	mov	%eax, %gs:7*4(%edx)
	mov	8*4(%ecx), %eax
	mov	%eax, %gs:8*4(%edx)

.globl ___copy_msg_to_user_end
___copy_msg_to_user_end:

	pop	%gs

	movl	$0, %eax
	ret

/*
 * if a function from a selected set of copies from or to userspace fails, it is
 * because of a wrong pointer supplied by the userspace. We have to clean up and
 * and return -1 to indicated that something wrong has happend. The place it was
 * called from has to handle this situation. The exception handler redirect us
 * here to continue, clean up and report the error
 */
.balign 16
.globl ___user_copy_msg_pointer_failure
___user_copy_msg_pointer_failure:
	pop	%gs

	movl	$-1, %eax
	ret

/*===========================================================================*/
/*				phys_memset				     */
/*===========================================================================*/
/*
 * PUBLIC void phys_memset(phys_bytes source, unsigned long pattern, 
 *	phys_bytes bytecount); 
 * Fill a block of physical memory with pattern. 
 */

.balign	16
_phys_memset:
	push	%ebp
	mov	%esp, %ebp
	push	%esi
	push	%ebx
	push	%ds

	mov	8(%ebp), %esi
	mov	16(%ebp), %eax
	mov	$FLAT_DS_SELECTOR, %ebx
	mov	%bx, %ds
	mov	12(%ebp), %ebx
	shr	$2, %eax
fill_start:
	mov	%ebx, (%esi)
	add	$4, %esi
	dec	%eax
	jne	fill_start
/* Any remaining bytes? */
	mov	16(%ebp), %eax
	and	$3, %eax
remain_fill:
	cmp	$0, %eax
	je	fill_done
	movb	12(%ebp), %bl
	movb	%bl, (%esi)
	add	$1, %esi
	inc	%ebp
	dec	%eax
	jmp	remain_fill
fill_done:
	pop	%ds
	pop	%ebx
	pop	%esi
	pop	%ebp
	ret


/*===========================================================================*/
/*				mem_rdw					     */
/*===========================================================================*/
/* 
 * PUBLIC u16_t mem_rdw(U16_t segment, u16_t *offset); 
 * Load and return word at far pointer segment:offset. 
 */

.balign	16
_mem_rdw:
	mov	%ds, %cx
	mov	4(%esp), %ds
	mov	4+4(%esp), %eax	/* offset */
	movzwl	(%eax), %eax	/* word to return */
	mov	%cx, %ds
	ret


/*===========================================================================*/
/*				reset					     */
/*===========================================================================*/
/*
 * PUBLIC void reset(); 
 * Reset the system by loading IDT with offset 0 and interrupting. 
 */

_reset:
	lidt	idt_zero
	int	$3	/* anything goes, the 386 will not like it */
.data
idt_zero:
.long	0, 0
.text


/*===========================================================================*/
/*			      	halt_cpu				     */
/*===========================================================================*/
/*
 * PUBLIC void halt_cpu(void);
 * reanables interrupts and puts the cpu in the halts state. Once an interrupt
 * is handled the execution resumes by disabling interrupts and continues
 */
_halt_cpu:
	sti
	hlt /* interrupts enabled only after this instruction is executed! */
	/*
	 * interrupt handlers make sure that the interrupts are disabled when we
	 * get here so we take only _one_ interrupt after halting the CPU
	 */
	ret

/*===========================================================================*/
/*			      read_flags				     */
/*===========================================================================*/
/*
 * PUBLIC unsigned long read_cpu_flags(void);
 * Read CPU status flags from C.
 */
.balign	16
_read_cpu_flags:
	pushf
	mov	(%esp), %eax
	add	$4, %esp
	ret

_read_ds:
	mov	$0, %eax
	mov	%ds, %ax
	ret

_read_cs:
	mov	$0, %eax
	mov	%cs, %ax
	ret

_read_ss:
	mov	$0, %eax
	mov	%ss, %ax
	ret


/*===========================================================================*/
/*                            fpu_routines                                   */
/*===========================================================================*/
_fninit:
	fninit
	ret

_clts:
	clts
	ret

_fnstsw:
	xor     %eax, %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fnstsw	%ax
	ret

_fnstcw:
	push    %eax
	mov     8(%esp), %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fnstcw	(%eax)
	pop     %eax
	ret

/*===========================================================================*/
/*			      fxsave			     		*/
/*===========================================================================*/
_fxsave:
	mov	4(%esp), %eax
	fxsave	(%eax)		/* Do not change the operand! (gas2ack) */
	ret

/*===========================================================================*/
/*			      fnsave			     		*/
/*===========================================================================*/
_fnsave:
	mov	4(%esp), %eax
	fnsave	(%eax)		/* Do not change the operand! (gas2ack) */
	fwait	/* required for compatibility with processors prior pentium */
	ret

/*===========================================================================*/
/*			      fxrstor			     		*/
/*===========================================================================*/
_fxrstor:
	mov	4(%esp), %eax
	fxrstor	(%eax)		/* Do not change the operand! (gas2ack) */
	ret

/*===========================================================================*/
/*			      frstor			     		*/
/*===========================================================================*/
_frstor:
	mov	4(%esp), %eax
	frstor	(%eax)		/* Do not change the operand! (gas2ack) */
	ret


/*===========================================================================*/
/*			      read_cr0					     */
/*===========================================================================*/
/* PUBLIC unsigned long read_cr0(void); */
_read_cr0:
	push	%ebp
	mov	%esp, %ebp
	mov	%cr0, %eax
	pop	%ebp
	ret

/*===========================================================================*/
/*			      write_cr0					     */
/*===========================================================================*/
/* PUBLIC void write_cr0(unsigned long value); */
_write_cr0:
	push	%ebp
	mov	%esp, %ebp
	mov	8(%ebp), %eax
	mov	%eax, %cr0
	jmp	0f	/* A jump is required for some flags */
0:
	pop	%ebp
	ret

/*===========================================================================*/
/*			      read_cr2					     */
/*===========================================================================*/
/* PUBLIC reg_t read_cr2(void); */
_read_cr2:
	mov	%cr2, %eax
	ret

/*===========================================================================*/
/*			      read_cr3					     */
/*===========================================================================*/
/* PUBLIC unsigned long read_cr3(void); */
_read_cr3:
	push	%ebp
	mov	%esp, %ebp

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	mov	%cr3, %eax
	pop	%ebp
	ret

/*===========================================================================*/
/*			      read_cr4					     */
/*===========================================================================*/
/* PUBLIC unsigned long read_cr4(void); */
_read_cr4:
	push	%ebp
	mov	%esp, %ebp

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	mov	%cr4, %eax
	pop	%ebp
	ret

/*===========================================================================*/
/*			      write_cr4					     */
/*===========================================================================*/
/* PUBLIC void write_cr4(unsigned long value); */
_write_cr4:
	push	%ebp
	mov	%esp, %ebp
	mov	8(%ebp), %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	mov	%eax, %cr4
	jmp	0f
0:
	pop	%ebp
	ret

/*===========================================================================*/
/*			      write_cr3					     */
/*===========================================================================*/
/* PUBLIC void write_cr3(unsigned long value); */
_write_cr3:
	push	%ebp
	mov	%esp, %ebp
	mov	8(%ebp), %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	mov	%eax, %cr3

	pop	%ebp
	ret

/*===========================================================================*/
/*				getcr3val				     */
/*===========================================================================*/
/* PUBLIC unsigned long getcr3val(void); */
_getcr3val:
	mov	%cr3, %eax
	ret

/*
 * Read the Model Specific Register (MSR) of IA32 architecture
 *
 * void ia32_msr_read(u32_t reg, u32_t * hi, u32_t * lo)
 */
.globl _ia32_msr_read
_ia32_msr_read:
	push	%ebp
	mov	%esp, %ebp

	mov	8(%ebp), %ecx
	rdmsr
	mov	12(%ebp), %ecx
	mov	%edx, (%ecx)
	mov	16(%ebp), %ecx
	mov	%eax, (%ecx)

	pop	%ebp
	ret

/*
 * Write the Model Specific Register (MSR) of IA32 architecture
 *
 * void ia32_msr_write(u32_t reg, u32_t hi, u32_t lo)
 */
.globl _ia32_msr_write
_ia32_msr_write:
	push	%ebp
	mov	%esp, %ebp

	mov	12(%ebp), %edx
	mov	16(%ebp), %eax
	mov	8(%ebp), %ecx
	wrmsr

	pop	%ebp
	ret

/*===========================================================================*/
/*			      idt_reload				     */
/*===========================================================================*/
/*  PUBLIC void idt_reload (void); */
.balign	16
_idt_reload:
	lidt	_gdt+IDT_SELECTOR	/*  reload interrupt descriptor table */
	ret

/*
 * void reload_segment_regs(void)
 */

#define RELOAD_SEG_REG(reg)	\
	mov	reg, %ax	;\
	mov	%ax, reg	;

.globl _reload_ds
_reload_ds:
	RELOAD_SEG_REG(%ds)
	ret

/*===========================================================================*/
/*			      switch_address_space			     */
/*===========================================================================*/
/* PUBLIC void switch_address_space(struct proc *p)
 *
 * sets the %cr3 register to the supplied value if it is not already set to the
 * same value in which case it would only result in an extra TLB flush which is
 * not desirable
 */
 .balign 16
.globl _switch_address_space
_switch_address_space:

	/* read the process pointer */
	mov	4(%esp), %edx
	/* enable process' segment descriptors  */
	lldt	P_LDT_SEL(%edx)
	/* get the new cr3 value */
	movl	P_CR3(%edx), %eax
	/* test if the new cr3 != NULL */
	cmpl	$0, %eax
	je	0f

	/*
	 * test if the cr3 is loaded with the current value to avoid unnecessary
	 * TLB flushes
	 */
	mov	%cr3, %ecx
	cmp	%ecx, %eax
	je	0f
	mov	%eax, %cr3
	mov	%edx, _ptproc
0:
	ret

