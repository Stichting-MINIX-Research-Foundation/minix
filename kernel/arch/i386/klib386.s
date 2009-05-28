# 
! sections

.sect .text; .sect .rom; .sect .data; .sect .bss

#include <minix/config.h>
#include <minix/const.h>
#include <ibm/interrupt.h>
#include <archconst.h>
#include "../../const.h"
#include "vm.h"
#include "sconst.h"

! This file contains a number of assembly code utility routines needed by the
! kernel.  They are:

.define	_monitor	! exit Minix and return to the monitor
.define	_int86		! let the monitor make an 8086 interrupt call
!.define	_cp_mess	! copies messages from source to destination
.define	_exit		! dummy for library routines
.define	__exit		! dummy for library routines
.define	___exit		! dummy for library routines
.define	___main		! dummy for GCC
!.define	_phys_insw	! transfer data from (disk controller) port to memory
!.define	_phys_insb	! likewise byte by byte
!.define	_phys_outsw	! transfer data from memory to (disk controller) port
!.define	_phys_outsb	! likewise byte by byte
.define	_intr_unmask	! enable an irq at the 8259 controller
.define	_intr_mask	! disable an irq
.define	_phys_copy	! copy data from anywhere to anywhere in memory
.define	_phys_memset	! write pattern anywhere in memory
.define	_mem_rdw	! copy one word from [segment:offset]
.define	_reset		! reset the system
.define	_idle_task	! task executed when there is no work
.define	_level0		! call a function at level 0
.define	_read_cpu_flags	! read the cpu flags
.define	_read_cr0	! read cr0
.define	_write_cr3	! write cr3
.define	_getcr3val
.define _last_cr3
.define	_write_cr0	! write a value in cr0
.define	_read_cr4
.define	_thecr3
.define	_write_cr4
.define _i386_invlpg_addr
.define _i386_invlpg_level0
.define __memcpy_k
.define __memcpy_k_fault

! The routines only guarantee to preserve the registers the C compiler
! expects to be preserved (ebx, esi, edi, ebp, esp, segment registers, and
! direction bit in the flags).

.sect .text
!*===========================================================================*
!*				monitor					     *
!*===========================================================================*
! PUBLIC void monitor();
! Return to the monitor.

_monitor:
	mov	esp, (_mon_sp)		! restore monitor stack pointer
    o16 mov	dx, SS_SELECTOR		! monitor data segment
	mov	ds, dx
	mov	es, dx
	mov	fs, dx
	mov	gs, dx
	mov	ss, dx
	pop	edi
	pop	esi
	pop	ebp
    o16 retf				! return to the monitor


!*===========================================================================*
!*				int86					     *
!*===========================================================================*
! PUBLIC void int86();
_int86:
	cmpb	(_mon_return), 0	! is the monitor there?
	jnz	0f
	movb	ah, 0x01		! an int 13 error seems appropriate
	movb	(_reg86+ 0), ah		! reg86.w.f = 1 (set carry flag)
	movb	(_reg86+13), ah		! reg86.b.ah = 0x01 = "invalid command"
	ret
0:	push	ebp			! save C registers
	push	esi
	push	edi
	push	ebx
	pushf				! save flags
	cli				! no interruptions

	inb	INT2_CTLMASK
	movb	ah, al
	inb	INT_CTLMASK
	push	eax			! save interrupt masks
	mov	eax, (_irq_use)		! map of in-use IRQ's
	and	eax, ~[1<<CLOCK_IRQ]	! keep the clock ticking
	outb	INT_CTLMASK		! enable all unused IRQ's and vv.
	movb	al, ah
	outb	INT2_CTLMASK

	mov	eax, SS_SELECTOR	! monitor data segment
	mov	ss, ax
	xchg	esp, (_mon_sp)		! switch stacks
	push	(_reg86+36)		! parameters used in INT call
	push	(_reg86+32)
	push	(_reg86+28)
	push	(_reg86+24)
	push	(_reg86+20)
	push	(_reg86+16)
	push	(_reg86+12)
	push	(_reg86+ 8)
	push	(_reg86+ 4)
	push	(_reg86+ 0)
	mov	ds, ax			! remaining data selectors
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	push	cs
	push	return			! kernel return address and selector
    o16	jmpf	20+2*4+10*4+2*4(esp)	! make the call
return:
	pop	(_reg86+ 0)
	pop	(_reg86+ 4)
	pop	(_reg86+ 8)
	pop	(_reg86+12)
	pop	(_reg86+16)
	pop	(_reg86+20)
	pop	(_reg86+24)
	pop	(_reg86+28)
	pop	(_reg86+32)
	pop	(_reg86+36)
	lgdt	(_gdt+GDT_SELECTOR)	! reload global descriptor table
	jmpf	CS_SELECTOR:csinit	! restore everything
csinit:	mov	eax, DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	xchg	esp, (_mon_sp)		! unswitch stacks
	lidt	(_gdt+IDT_SELECTOR)	! reload interrupt descriptor table
	andb	(_gdt+TSS_SELECTOR+DESC_ACCESS), ~0x02  ! clear TSS busy bit
	mov	eax, TSS_SELECTOR
	ltr	ax			! set TSS register

	pop	eax
	outb	INT_CTLMASK		! restore interrupt masks
	movb	al, ah
	outb	INT2_CTLMASK

	add	(_lost_ticks), ecx	! record lost clock ticks

	popf				! restore flags
	pop	ebx			! restore C registers
	pop	edi
	pop	esi
	pop	ebp
	ret


!*===========================================================================*
!*				exit					     *
!*===========================================================================*
! PUBLIC void exit();
! Some library routines use exit, so provide a dummy version.
! Actual calls to exit cannot occur in the kernel.
! GNU CC likes to call ___main from main() for nonobvious reasons.

_exit:
__exit:
___exit:
	sti
	jmp	___exit

___main:
	ret


!*===========================================================================*
!*				phys_insw				     *
!*===========================================================================*
! PUBLIC void phys_insw(Port_t port, phys_bytes buf, size_t count);
! Input an array from an I/O port.  Absolute address version of insw().
!
!_phys_insw:
!	push	ebp
!	mov	ebp, esp
!	cld
!	push	edi
!	push	es
!
!	mov	ecx, FLAT_DS_SELECTOR
!	mov	es, cx
!	mov	edx, 8(ebp)		! port to read from
!	mov	edi, 12(ebp)		! destination addr
!	mov	ecx, 16(ebp)		! byte count
!	shr	ecx, 1			! word count
!rep o16	ins				! input many words
!	pop	es
!	pop	edi
!	pop	ebp
!	ret
!

!*===========================================================================*
!*				phys_insb				     *
!*===========================================================================*
! PUBLIC void phys_insb(Port_t port, phys_bytes buf, size_t count);
! Input an array from an I/O port.  Absolute address version of insb().
!
!_phys_insb:
!	push	ebp
!	mov	ebp, esp
!	cld
!	push	edi
!	push	es
!
!	mov	ecx, FLAT_DS_SELECTOR
!	mov	es, cx
!	mov	edx, 8(ebp)		! port to read from
!	mov	edi, 12(ebp)		! destination addr
!	mov	ecx, 16(ebp)		! byte count
!!	shr	ecx, 1			! word count
!   rep	insb				! input many bytes
!	pop	es
!	pop	edi
!	pop	ebp
!	ret
!

!*===========================================================================*
!*				phys_outsw				     *
!*===========================================================================*
! PUBLIC void phys_outsw(Port_t port, phys_bytes buf, size_t count);
! Output an array to an I/O port.  Absolute address version of outsw().

!	.align	16
!_phys_outsw:
!	push	ebp
!	mov	ebp, esp
!	cld
!	push	esi
!	push	ds
!
!	mov	ecx, FLAT_DS_SELECTOR
!	mov	ds, cx
!	mov	edx, 8(ebp)		! port to write to
!	mov	esi, 12(ebp)		! source addr
!	mov	ecx, 16(ebp)		! byte count
!	shr	ecx, 1			! word count
!rep o16	outs				! output many words
!	pop	ds
!	pop	esi
!	pop	ebp
!	ret
!

!*===========================================================================*
!*				phys_outsb				     *
!*===========================================================================*
! PUBLIC void phys_outsb(Port_t port, phys_bytes buf, size_t count);
! Output an array to an I/O port.  Absolute address version of outsb().
!
!	.align	16
!_phys_outsb:
!	push	ebp
!	mov	ebp, esp
!	cld
!	push	esi
!	push	ds
!
!	mov	ecx, FLAT_DS_SELECTOR
!	mov	ds, cx
!	mov	edx, 8(ebp)		! port to write to
!	mov	esi, 12(ebp)		! source addr
!	mov	ecx, 16(ebp)		! byte count
!   rep	outsb				! output many bytes
!	pop	ds
!	pop	esi
!	pop	ebp
!	ret


!*==========================================================================*
!*				intr_unmask				    *
!*==========================================================================*/
! PUBLIC void intr_unmask(irq_hook_t *hook)
! Enable an interrupt request line by clearing an 8259 bit.
! Equivalent C code for hook->irq < 8:
!   if ((irq_actids[hook->irq] &= ~hook->id) == 0)
!	outb(INT_CTLMASK, inb(INT_CTLMASK) & ~(1 << irq));

	.align	16
_intr_unmask:
	push	ebp
	mov	ebp, esp
	pushf
	cli
	mov	eax, 8(ebp)		! hook
	mov	ecx, 8(eax)		! irq
	mov	eax, 12(eax)		! id bit
	not	eax
	and	_irq_actids(ecx*4), eax	! clear this id bit
	jnz	en_done			! still masked by other handlers?
	movb	ah, ~1
	rolb	ah, cl			! ah = ~(1 << (irq % 8))
	mov	edx, INT_CTLMASK	! enable irq < 8 at the master 8259
	cmpb	cl, 8
	jb	0f
	mov	edx, INT2_CTLMASK	! enable irq >= 8 at the slave 8259
0:	inb	dx
	andb	al, ah
	outb	dx			! clear bit at the 8259
en_done:popf
	leave
	ret


!*==========================================================================*
!*				intr_mask				    *
!*==========================================================================*/
! PUBLIC int intr_mask(irq_hook_t *hook)
! Disable an interrupt request line by setting an 8259 bit.
! Equivalent C code for irq < 8:
!   irq_actids[hook->irq] |= hook->id;
!   outb(INT_CTLMASK, inb(INT_CTLMASK) | (1 << irq));
! Returns true iff the interrupt was not already disabled.

	.align	16
_intr_mask:
	push	ebp
	mov	ebp, esp
	pushf
	cli
	mov	eax, 8(ebp)		! hook
	mov	ecx, 8(eax)		! irq
	mov	eax, 12(eax)		! id bit
	or	_irq_actids(ecx*4), eax	! set this id bit
	movb	ah, 1
	rolb	ah, cl			! ah = (1 << (irq % 8))
	mov	edx, INT_CTLMASK	! disable irq < 8 at the master 8259
	cmpb	cl, 8
	jb	0f
	mov	edx, INT2_CTLMASK	! disable irq >= 8 at the slave 8259
0:	inb	dx
	testb	al, ah
	jnz	dis_already		! already disabled?
	orb	al, ah
	outb	dx			! set bit at the 8259
	mov	eax, 1			! disabled by this function
	popf
	leave
	ret
dis_already:
	xor	eax, eax		! already disabled
	popf
	leave
	ret


!*===========================================================================*
!*				phys_copy				     *
!*===========================================================================*
! PUBLIC void phys_copy(phys_bytes source, phys_bytes destination,
!			phys_bytes bytecount);
! Copy a block of physical memory.

PC_ARGS	=	4 + 4 + 4 + 4	! 4 + 4 + 4
!		es edi esi eip	 src dst len

	.align	16
_phys_copy:
	cld
	push	esi
	push	edi
	push	es

	mov	eax, FLAT_DS_SELECTOR
	mov	es, ax

	mov	esi, PC_ARGS(esp)
	mov	edi, PC_ARGS+4(esp)
	mov	eax, PC_ARGS+4+4(esp)

	cmp	eax, 10			! avoid align overhead for small counts
	jb	pc_small
	mov	ecx, esi		! align source, hope target is too
	neg	ecx
	and	ecx, 3			! count for alignment
	sub	eax, ecx
	rep
   eseg	movsb
	mov	ecx, eax
	shr	ecx, 2			! count of dwords
	rep
   eseg	movs
	and	eax, 3
pc_small:
	xchg	ecx, eax		! remainder
	rep
   eseg	movsb

	pop	es
	pop	edi
	pop	esi
	ret

!*===========================================================================*
!*				phys_memset				     *
!*===========================================================================*
! PUBLIC void phys_memset(phys_bytes source, unsigned long pattern,
!	phys_bytes bytecount);
! Fill a block of physical memory with pattern.

	.align	16
_phys_memset:
	push	ebp
	mov	ebp, esp
	push	esi
	push	ebx
	push	ds

	mov	esi, 8(ebp)
	mov	eax, 16(ebp)
	mov	ebx, FLAT_DS_SELECTOR
	mov	ds, bx
	mov	ebx, 12(ebp)
	shr	eax, 2
fill_start:
   	mov     (esi), ebx
	add	esi, 4
	dec	eax
	jnz	fill_start
	! Any remaining bytes?
	mov	eax, 16(ebp)
!	and	eax, 3
remain_fill:
	cmp	eax, 0
	jz	fill_done
	movb	bl, 12(ebp)
   	movb    (esi), bl
	add	esi, 1
	inc	ebp
	dec	eax
	jmp	remain_fill
fill_done:
	pop	ds
	pop	ebx
	pop	esi
	pop	ebp
	ret
	

!*===========================================================================*
!*				mem_rdw					     *
!*===========================================================================*
! PUBLIC u16_t mem_rdw(U16_t segment, u16_t *offset);
! Load and return word at far pointer segment:offset.

	.align	16
_mem_rdw:
	mov	cx, ds
	mov	ds, 4(esp)		! segment
	mov	eax, 4+4(esp)		! offset
	movzx	eax, (eax)		! word to return
	mov	ds, cx
	ret


!*===========================================================================*
!*				reset					     *
!*===========================================================================*
! PUBLIC void reset();
! Reset the system by loading IDT with offset 0 and interrupting.

_reset:
	lidt	(idt_zero)
	int	3		! anything goes, the 386 will not like it
.sect .data
idt_zero:	.data4	0, 0
.sect .text


!*===========================================================================*
!*				idle_task				     *
!*===========================================================================*
_idle_task:
! This task is called when the system has nothing else to do.  The HLT
! instruction puts the processor in a state where it draws minimum power.
	push	halt
	call	_level0		! level0(halt)
	pop	eax
	jmp	_idle_task
halt:
	sti
	hlt
	cli
	ret

!*===========================================================================*
!*			      level0					     *
!*===========================================================================*
! PUBLIC void level0(void (*func)(void))
! Call a function at permission level 0.  This allows kernel tasks to do
! things that are only possible at the most privileged CPU level.
!
_level0:
	mov	eax, 4(esp)
	mov	(_level0_func), eax
	int	LEVEL0_VECTOR
	ret


!*===========================================================================*
!*			      read_flags					     *
!*===========================================================================*
! PUBLIC unsigned long read_cpu_flags(void);
! Read CPU status flags from C.
.align 16
_read_cpu_flags:
	pushf
	mov eax, (esp)
	popf
	ret


!*===========================================================================*
!*			      read_cr0					     *
!*===========================================================================*
! PUBLIC unsigned long read_cr0(void);
_read_cr0:
	push	ebp
	mov	ebp, esp
	mov	eax, cr0
	pop	ebp
	ret

!*===========================================================================*
!*			      write_cr0					     *
!*===========================================================================*
! PUBLIC void write_cr0(unsigned long value);
_write_cr0:
	push	ebp
	mov	ebp, esp
	mov	eax, 8(ebp)
	mov	cr0, eax
	jmp	0f		! A jump is required for some flags
0:
	pop	ebp
	ret

!*===========================================================================*
!*			      read_cr4					     *
!*===========================================================================*
! PUBLIC unsigned long read_cr4(void);
_read_cr4:
	push	ebp
	mov	ebp, esp
.data1	0x0f, 0x20, 0xe0 ! mov eax, cr4
	pop	ebp
	ret

!*===========================================================================*
!*			      write_cr4					     *
!*===========================================================================*
! PUBLIC void write_cr4(unsigned long value);
_write_cr4:
	push	ebp
	mov	ebp, esp
	mov	eax, 8(ebp)
.data1	0x0f, 0x22, 0xe0 ! mov cr4, eax
	jmp	0f
0:
	pop	ebp
	ret

!*===========================================================================*
!*				write_cr3				*
!*===========================================================================*
! PUBLIC void write_cr3(unsigned long value);
_write_cr3:
	push    ebp
	mov     ebp, esp
	mov	eax, 8(ebp)
	mov	cr3, eax
	pop     ebp
	ret

!*===========================================================================*
!*				getcr3val				*
!*===========================================================================*
! PUBLIC unsigned long getcr3val(void);
_getcr3val:
	mov	eax, cr3
	mov	(_thecr3), eax
	ret

!*===========================================================================*
!*				i386_invlpg				*
!*===========================================================================*
! PUBLIC void i386_invlpg(void);
_i386_invlpg_level0:
	push    ebp
	invlpg	(_i386_invlpg_addr)
	pop	ebp
	ret


!*===========================================================================*
!*				_memcpy_k				     *
!*===========================================================================*
!	_memcpy_k()				Original Author: Kees J. Bot
!								2 Jan 1994
! void *_memcpy_k(void *s1, const void *s2, size_t n)
!	Copy a chunk of memory that the kernel can use to trap pagefaults.
.define __memcpy_k
.define __memcpy_k_fault
	.align	16
__memcpy_k:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	mov	edi, 8(ebp)	! String s1
	mov	esi, 12(ebp)	! String s2
	mov	ecx, 16(ebp)	! Length
	cld			! Clear direction bit: upwards
	cmp	ecx, 16
	jb	upbyte		! Don't bother being smart with short arrays
	mov	eax, esi
	or	eax, edi
	testb	al, 1
	jnz	upbyte		! Bit 0 set, use byte copy
	testb	al, 2
	jnz	upword		! Bit 1 set, use word copy
uplword:shrd	eax, ecx, 2	! Save low 2 bits of ecx in eax
	shr	ecx, 2
	rep
	movs			! Copy longwords.
	shld	ecx, eax, 2	! Restore excess count
upword:	shr	ecx, 1
	rep
    o16	movs			! Copy words
	adc	ecx, ecx	! One more byte?
upbyte:	rep
	movsb			! Copy bytes
done:	mov	eax, 0
__memcpy_k_fault:		! Kernel can send us here with pf cr2 in eax
	pop	edi
	pop	esi
	pop	ebp
	ret
