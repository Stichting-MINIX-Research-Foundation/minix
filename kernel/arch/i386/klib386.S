/* sections */

.text; .data; .data; .bss

#include <minix/config.h>
#include <minix/const.h>
#include <ibm/interrupt.h>
#include <archconst.h>
#include "../../const.h"
#include "sconst.h"

/*
 * This file contains a number of assembly code utility routines needed by the
 * kernel.  They are:
 */

.globl	monitor	/* exit Minix and return to the monitor */
.globl	int86	/* let the monitor make an 8086 interrupt call */
.globl	exit	/* dummy for library routines */
.globl	_exit	/* dummy for library routines */
.globl	__exit	/* dummy for library routines */
.globl	__main	/* dummy for GCC */
.globl	phys_insw	/* transfer data from (disk controller) port to memory */
.globl	phys_insb	/* likewise byte by byte */
.globl	phys_outsw	/* transfer data from memory to (disk controller) port */
.globl	phys_outsb	/* likewise byte by byte */
.globl	phys_copy	/* copy data from anywhere to anywhere in memory */
.globl	phys_copy_fault	/* phys_copy pagefault */
.globl	phys_copy_fault_in_kernel /* phys_copy pagefault in kernel */
.globl	phys_memset	/* write pattern anywhere in memory */
.globl	mem_rdw	/* copy one word from [segment:offset] */
.globl	reset	/* reset the system */
.globl	halt_cpu/* halts the current cpu when idle */
.globl	level0	/* call a function at level 0 */
.globl	read_cpu_flags	/* read the cpu flags */
.globl	read_cr0	/* read cr0 */
.globl	read_cr2	/* read cr2 */
.globl	getcr3val
.globl	write_cr0	/* write a value in cr0 */
.globl	read_cr4
.globl  thecr3
.globl	write_cr4

.globl	catch_pagefaults
.globl	read_ds
.globl  read_cs
.globl	read_ss
.globl	idt_reload	/* reload idt when returning to monitor. */

.globl	fninit			/* non-waiting FPU initialization */
.globl	fnstsw			/* store status word (non-waiting) */
.globl	fnstcw			/* store control word (non-waiting) */

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

monitor:
	movl	mon_sp, %esp	/* restore monitor stack pointer */
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
int86:
	cmpb	$0, mon_return	/* is the monitor there? */
	jne	0f
	movb	$0x01, %ah	/* an int 13 error seems appropriate */
	movb	%ah, reg86+0	/* reg86.w.f = 1 (set carry flag) */
	movb	%ah, reg86+13	/* reg86.b.ah = 0x01 = "invalid command" */
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
	movl	irq_use, %eax	/* map of in-use IRQ's */
	and	$~(1<<CLOCK_IRQ), %eax	/* keep the clock ticking */
	outb	$INT_CTLMASK	/* enable all unused IRQ's and vv. */
	movb	%ah, %al
	outb	$INT2_CTLMASK

	mov	$SS_SELECTOR, %eax	/* monitor data segment */
	mov	%ax, %ss
	xchgl	mon_sp, %esp	/* switch stacks */
	push	reg86+36	/* parameters used in INT call */
	push	reg86+32
	push	reg86+28
	push	reg86+24
	push	reg86+20
	push	reg86+16
	push	reg86+12
	push	reg86+8
	push	reg86+4
	push	reg86+0
	mov	%ax, %ds	/* remaining data selectors */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	push	%cs
	push	$return	/* kernel return address and selector */
        ljmpw    *20+2*4+10*4+2*4(%esp)
return:
	pop	reg86+0
	pop	reg86+4
	pop	reg86+8
	pop	reg86+12
	pop	reg86+16
	pop	reg86+20
	pop	reg86+24
	pop	reg86+28
	pop	reg86+32
	pop	reg86+36
	lgdt	gdt+GDT_SELECTOR	/* reload global descriptor table */
        ljmp    $CS_SELECTOR, $csinit
csinit:
	mov	$DS_SELECTOR, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	xchgl	mon_sp, %esp	/* unswitch stacks */
	lidt	gdt+IDT_SELECTOR	/* reload interrupt descriptor table */

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
	addl	gdt+DESC_ACCESS, %eax
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
	addl	%ecx, lost_ticks	/* record lost clock ticks */

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

exit:
_exit:
__exit:
	sti
	jmp	__exit

__main:
	ret


/*===========================================================================*/
/*				phys_insw				     */
/*===========================================================================*/
/*
 * PUBLIC void phys_insw(Port_t port, phys_bytes buf, size_t count); 
 * Input an array from an I/O port.  Absolute address version of insw(). 
 */

phys_insw:
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

phys_insb:
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
phys_outsw:
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
phys_outsb:
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
phys_copy:
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
phys_copy_fault:			/* kernel can send us here */
	pop	%es
	pop	%edi
	pop	%esi
	ret

phys_copy_fault_in_kernel:			/* kernel can send us here */
	pop	%es
	pop	%edi
	pop	%esi
	mov	%cr2, %eax
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
phys_memset:
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
mem_rdw:
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

reset:
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
halt_cpu:
	sti
	hlt
	cli
	ret

/*===========================================================================*/
/*			      level0					     */
/*===========================================================================*/
/*
 * PUBLIC void level0(void (*func)(void)) 
 * Call a function at permission level 0.  This allows kernel tasks to do 
 * things that are only possible at the most privileged CPU level. 
 */ 
level0:
	/* check whether we are already running in kernel, the kernel cs
	 * selector has 3 lower bits zeroed */
	mov	%cs, %ax
	cmpw	$CS_SELECTOR, %ax
	jne	0f

	/* call the function directly as if it was a normal function call */
	mov	4(%esp), %eax
	call	*%eax
	ret


	/* if not runnig in the kernel yet, trap to kernel */
0:
	mov	4(%esp), %eax
	int	$LEVEL0_VECTOR
	ret

/*===========================================================================*/
/*			      read_flags				     */
/*===========================================================================*/
/*
 * PUBLIC unsigned long read_cpu_flags(void);
 * Read CPU status flags from C.
 */
.balign	16
read_cpu_flags:
	pushf
	mov	(%esp), %eax
	add	$4, %esp
	ret

read_ds:
	mov	$0, %eax
	mov	%ds, %ax
	ret

read_cs:
	mov	$0, %eax
	mov	%cs, %ax
	ret

read_ss:
	mov	$0, %eax
	mov	%ss, %ax
	ret


/*===========================================================================*/
/*                            fpu_routines                                   */
/*===========================================================================*/
fninit:
	fninit
	ret

fnstsw:
	xor     %eax, %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fnstsw	%ax
	ret

fnstcw:
	push    %eax
	mov     8(%esp), %eax

	/* DO NOT CHANGE THE OPERAND!!! gas2ack does not handle it yet */
	fnstcw	(%eax)
	pop     %eax
	ret

/*===========================================================================*/
/*			      read_cr0					     */
/*===========================================================================*/
/* PUBLIC unsigned long read_cr0(void); */
read_cr0:
	push	%ebp
	mov	%esp, %ebp
	mov	%cr0, %eax
	pop	%ebp
	ret

/*===========================================================================*/
/*			      write_cr0					     */
/*===========================================================================*/
/* PUBLIC void write_cr0(unsigned long value); */
write_cr0:
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
read_cr2:
	mov	%cr2, %eax
	ret

/*===========================================================================*/
/*			      read_cr4					     */
/*===========================================================================*/
/* PUBLIC unsigned long read_cr4(void); */
read_cr4:
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
write_cr4:
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
/*				getcr3val				     */
/*===========================================================================*/
/* PUBLIC unsigned long getcr3val(void); */
getcr3val:
	mov	%cr3, %eax
	mov	%eax, thecr3
	ret

/*
 * Read the Model Specific Register (MSR) of IA32 architecture
 *
 * void ia32_msr_read(u32_t reg, u32_t * hi, u32_t * lo)
 */
.globl ia32_msr_read
ia32_msr_read:
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
.globl ia32_msr_write
ia32_msr_write:
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
idt_reload:
	lidt	gdt+IDT_SELECTOR	/*  reload interrupt descriptor table */
	ret

/*
 * void reload_segment_regs(void)
 */

#define RELOAD_SEG_REG(reg)	\
	mov	reg, %ax	;\
	mov	%ax, reg	;

.globl reload_ds
reload_ds:
	RELOAD_SEG_REG(%ds)
	ret
