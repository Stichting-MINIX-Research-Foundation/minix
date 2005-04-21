#
! This file, mpx88.s, is included by mpx.s when Minix is compiled for
! 16-bit Intel CPUs. The alternative mpx386.s is compiled for 32-bit CPUs.
!
! This file contains the assembler startup code for Minix and the 16-bit
! interrupt handlers.  It cooperates with cstart.c to set up a good
! environment for main().

! This file is part of the lowest layer of the MINIX kernel.  The other part
! is "proc.c".  The lowest layer does process switching and message handling.

! Every transition to the kernel goes through this file.  Transitions are
! caused by sending/receiving messages and by most interrupts.  (RS232
! interrupts may be handled in the file "rs2.s" and then they rarely enter
! the kernel.)

! Transitions to the kernel may be nested.  The initial entry may be with a
! system call, exception or hardware interrupt; reentries may only be made
! by hardware interrupts.  The count of reentries is kept in "k_reenter".
! It is important for deciding whether to switch to the kernel stack and
! for protecting the message passing code in "proc.c".

! For the message passing trap, most of the machine state is saved in the
! proc table.  (Some of the registers need not be saved.)  Then the stack is
! switched to "k_stack", and interrupts are reenabled.  Finally, the system
! call handler (in C) is called.  When it returns, interrupts are disabled
! again and the code falls into the restart routine, to finish off held-up
! interrupts and run the process or task whose pointer is in "proc_ptr".

! Hardware interrupt handlers do the same, except  (1) The entire state must
! be saved.  (2) There are too many handlers to do this inline, so the save
! routine is called.  A few cycles are saved by pushing the address of the
! appropiate restart routine for a return later.  (3) A stack switch is
! avoided when the stack is already switched.  (4) The (master) 8259 interrupt
! controller is reenabled centrally in save().  (5) Each interrupt handler
! masks its interrupt line using the 8259 before enabling (other unmasked)
! interrupts, and unmasks it after servicing the interrupt.  This limits the
! nest level to the number of lines and protects the handler from itself.

! For communication with the boot monitor at startup time some constant
! data are compiled into the beginning of the text segment. This facilitates 
! reading the data at the start of the boot process, since only the first
! sector of the file needs to be read.

! Some data storage is also allocated at the end of this file. This data 
! will be at the start of the data segment of the kernel and will be read
! and modified by the boot monitor before the kernel starts.

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include "const.h"
#include "sconst.h"
#include "protect.h"

! The external entry points into this file are:
! Note: in assembly language the .define statement applied to a function name 
! is loosely equivalent to a prototype in C code -- it makes it possible to
! link to an entity declared in the assembly code but does not create
! the entity.

.define	_int00		! handlers for traps and exceptions
.define	_int01
.define	_int02
.define	_int03
.define	_int04
.define	_int05
.define	_int06
.define	_int07
.define _hwint00	! handlers for hardware interrupts
.define _hwint01
.define _hwint02
.define _hwint03
.define _hwint04
.define _hwint05
.define _hwint06
.define _hwint07
.define _hwint08
.define _hwint09
.define _hwint10
.define _hwint11
.define _hwint12
.define _hwint13
.define _hwint14
.define _hwint15
.define	_restart	! start running a task or process
.define	save		! save the machine state in the proc table
.define	_s_call		! process or task wants to send or receive a message

! Exported variables.
.define	kernel_ds
.define	begbss
.define	begdata

	.text
!*===========================================================================*
!*				MINIX					     *
!*===========================================================================*
MINIX:				! this is the entry point for the MINIX kernel
	jmp	over_kernel_ds	! skip over the next few bytes
	.data2	CLICK_SHIFT	! for the monitor: memory granularity
kernel_ds:
	.data2	0x01B4		! boot monitor flags:  (later kernel DS)
				!	call in 8086 mode, make bss, make stack,
				!	load low, don`t patch, will return,
				!	(has own INT calls), memory vector,
				!	new boot code return
over_kernel_ds:

! Set up a C stack frame on the monitor stack.  (The monitor sets cs and ds
! right.  The ss register still references the monitor data segment.)
	push	bp
	mov	bp, sp
	push	si
	push	di
	cmp	4(bp), #0	! monitor code segment is
	jz	noret		! nonzero if return possible
	inc	_mon_return
noret:	mov	_mon_ss, ss	! save stack location for later return
	mov	_mon_sp, sp

! Locate boot parameters, set up kernel segment registers and stack.
	mov	bx, 6(bp)	! boot parameters offset
	mov	dx, 8(bp)	! boot parameters length
	mov	ax, 10(bp)	! address of a.out headers
	mov	_aout+0, ax
	mov	ax, 12(bp)
	mov	_aout+2, ax
	mov	ax, ds		! kernel data
	mov	es, ax
	mov	ss, ax
	mov	sp, #k_stktop	! set sp to point to the top of kernel stack

! Real mode needs to get kernel DS from the code segment.  Protected mode
! needs CS in the jump back to real mode.

  cseg	mov	kernel_cs, cs
  cseg	mov	kernel_ds, ds

! Call C startup code to set up a proper environment to run main().
	push	dx
	push	bx
	push	_mon_ss
	push	ds
	push	cs
	call	_cstart		! cstart(cs, ds, mds, parmoff, parmlen)
	add	sp, #5*2

	cmp	_protected_mode, #0
	jz	nosw		! ok to switch to protected mode?

	call	klib_init_prot	! initialize klib functions for protected mode
	call	real2prot	! switch to protected mode

	push	#0		! set flags to known good state
	popf			! especially, clear nested task and int enable
nosw:
	jmp	_main		! main()


!*===========================================================================*
!*				interrupt handlers			     *
!*===========================================================================*


!*===========================================================================*
!*				hwint00 - 07				     *
!*===========================================================================*
! Note that the first few lines are a macro
#define hwint_master(irq)	\
	call	save			/* save interrupted process state */;\
	mov	si, *[2*irq]		/* load array index offset 	  */;\
	mov	di, *[1<<irq]		/* irq mask bit			  */;\
	jmp	hwint_master		/* continue with common code	  */
hwint_master:
	inb	INT_CTLMASK
	or	ax, di			! al |= (1 << irq)
	outb	INT_CTLMASK		! disable the irq
	movb	al, *ENABLE
	outb	INT_CTL			! reenable master 8259
	mov	cx, _irq_hooks(si)	! irq_hooks[irq]
	push	cx
	sti				! enable interrupts
	call	_intr_handle		! intr_handle(irq_hooks[irq])
	cli				! disable interrupts
	pop	cx
	cmp	_irq_actids(si), *0	! interrupt still active?
	jnz	0f
	inb	INT_CTLMASK
	not	di
	and	ax, di			! al &= ~(1 << irq)
	outb	INT_CTLMASK		! enable the irq
0:	ret				! restart (another) process

! Each of these entry points is an expansion of the hwint_master macro

_hwint00:		! Interrupt routine for irq 0 (the clock).
	hwint_master(0)


_hwint01:		! Interrupt routine for irq 1 (keyboard)
	hwint_master(1)


_hwint02:		! Interrupt routine for irq 2 (cascade!)
	hwint_master(2)


_hwint03:		! Interrupt routine for irq 3 (second serial)
	hwint_master(3)


_hwint04:		! Interrupt routine for irq 4 (first serial)
	hwint_master(4)


_hwint05:		! Interrupt routine for irq 5 (XT winchester)
	hwint_master(5)


_hwint06:		! Interrupt routine for irq 6 (floppy)
	hwint_master(6)


_hwint07:		! Interrupt routine for irq 7 (printer)
	hwint_master(7)


!*===========================================================================*
!*				hwint08 - 15				     *
!*===========================================================================*
! Note that the first few lines are a macro
#define hwint_slave(irq)	\
	call	save			/* save interrupted process state */;\
	mov	si, *[2*irq]		/* load array index offset 	  */;\
	mov	di, *[1<<[irq-8]]	/* irq mask bit			  */;\
	jmp	hwint_slave		/* continue with common code	  */
hwint_slave:
	inb	INT2_CTLMASK
	or	ax, di			! al |= (1 << (irq-8))
	outb	INT2_CTLMASK		! disable the irq
	movb	al, *ENABLE
	outb	INT_CTL			! reenable master 8259
	mov	cx, _irq_hooks(si)	! irq_hooks[irq]
	outb	INT2_CTL		! reenable slave 8259
	push	cx
	sti				! enable interrupts
	call	_intr_handle		! intr_handle(irq_hooks[irq])
	cli				! disable interrupts
	pop	cx
	cmp	_irq_actids(si), *0	! interrupt still active?
	jnz	0f
	inb	INT2_CTLMASK
	not	di
	and	ax, di			! al &= ~(1 << (irq-8))
	outb	INT2_CTLMASK		! enable the irq
0:	ret				! restart (another) process

! Each of these entry points is an expansion of the hwint_slave macro

_hwint08:		! Interrupt routine for irq 8 (realtime clock)
	hwint_slave(8)


_hwint09:		! Interrupt routine for irq 9 (irq 2 redirected)
	hwint_slave(9)


_hwint10:		! Interrupt routine for irq 10
	hwint_slave(10)


_hwint11:		! Interrupt routine for irq 11
	hwint_slave(11)


_hwint12:		! Interrupt routine for irq 12
	hwint_slave(12)


_hwint13:		! Interrupt routine for irq 13 (FPU exception)
	hwint_slave(13)


_hwint14:		! Interrupt routine for irq 14 (AT winchester)
	hwint_slave(14)


_hwint15:		! Interrupt routine for irq 15
	hwint_slave(15)


!*===========================================================================*
!*				save					     *
!*===========================================================================*
save:				! save the machine state in the proc table.

! In protected mode a jump to p_save is patched over the following
! code during initialization.

	cld			! set direction flag to a known value
	push	ds
	push	si
  cseg	mov	ds,kernel_ds
	incb	_k_reenter	! from -1 if not reentering
	jnz	push_current_stack	! stack is already kernel stack

	mov	si,_proc_ptr
	mov	AXREG(si),ax
	mov	BXREG(si),bx
	mov	CXREG(si),cx
	mov	DXREG(si),dx
	pop	SIREG(si)
	mov	DIREG(si),di
	mov	BPREG(si),bp
	mov	ESREG(si),es
	pop	DSREG(si)
	pop	bx		! return adr
	pop	PCREG(si)
	pop	CSREG(si)
	pop	PSWREG(si)
	mov	SPREG(si),sp
	mov	SSREG(si),ss

	mov	dx,ds
	mov	ss,dx
	mov	sp,#k_stktop
	mov	ax,#_restart	! build return address for interrupt handler
	push	ax

stack_switched:
	mov	es,dx
	jmp	(bx)

push_current_stack:
	push	es
	push	bp
	push	di
	push	dx
	push	cx
	push	bx
	push	ax
	mov	bp,sp
	mov	bx,18(bp)	! get the return adr; it becomes junk on stack
	mov	ax,#restart1
	push	ax
	mov	dx,ss
	mov	ds,dx
	jmp	stack_switched


!*===========================================================================*
!*				s_call					     *
!*===========================================================================*
! This is real mode version. Alternate (_p_s_call) will be used in
! protected mode

_s_call:			! System calls are vectored here.
				! Do save routine inline for speed,
				! but do not save ax, bx, cx, dx,
				! since C does not require preservation,
				! and ax returns the result code anyway.
				! Regs bp, si, di get saved by sys_call as
				! well, but it is impractical not to preserve
				! them here, in case context gets switched.
				! Some special-case code in pick_proc()
				! could avoid this.
	cld			! set direction flag to a known value
	push	ds
	push	si
  cseg	mov	ds,kernel_ds
	incb	_k_reenter
	mov	si,_proc_ptr
	pop	SIREG(si)
	mov	DIREG(si),di
	mov	BPREG(si),bp
	mov	ESREG(si),es
	pop	DSREG(si)
	pop	PCREG(si)
	pop	CSREG(si)
	pop	PSWREG(si)
	mov	SPREG(si),sp
	mov	SSREG(si),ss
	mov	dx,ds
	mov	es,dx
	mov	ss,dx		! interrupt handlers may not make system calls
	mov	sp,#k_stktop	! so stack is not already switched
				! end of inline save
				! now set up parameters for C routine sys_call
	push	bx		! pointer to user message
	push	ax		! src/dest
	push	cx		! SEND/RECEIVE/BOTH
	sti			! allow SWITCHER to be interrupted
	call	_sys_call	! sys_call(function, src_dest, m_ptr)
				! caller is now explicitly in proc_ptr
	mov	AXREG(si),ax	! sys_call MUST PRESERVE si
	cli

! Fall into code to restart proc/task running.


!*===========================================================================*
!*				restart					     *
!*===========================================================================*
_restart:

! Flush any held-up interrupts.
! This reenables interrupts, so the current interrupt handler may reenter.
! This does not matter, because the current handler is about to exit and no
! other handlers can reenter since flushing is only done when k_reenter == 0.

! In protected mode a jump to p_restart is patched over the following
! code during initialization.

	cmp	_held_head,#0	! do fast test to usually avoid function call
	jz	over_call_unhold
	call	_unhold		! this is rare so overhead is acceptable
over_call_unhold:

	mov	si,_proc_ptr
	decb	_k_reenter
	mov	ax,AXREG(si)	! start restoring registers from proc table
				! could make AXREG == 0 to use lodw here
	mov	bx,BXREG(si)
	mov	cx,CXREG(si)
	mov	dx,DXREG(si)
	mov	di,DIREG(si)
	mov	bp,BPREG(si)
	mov	es,ESREG(si)
	mov	ss,SSREG(si)
	mov	sp,SPREG(si)
	push	PSWREG(si)	! fake interrupt stack frame
	push	CSREG(si)
	push	PCREG(si)
				! could put si:ds together to use
				! lds si,SIREG(si)
	push	DSREG(si)
	mov	si,SIREG(si)
	pop	ds
	iret

restart1:
	decb	_k_reenter
	pop	ax
	pop	bx
	pop	cx
	pop	dx
	pop	di
	pop	bp
	pop	es
	pop	si
	pop	ds
	add	sp,#2		! skip return adr
	iret


!*===========================================================================*
!*				int00-07				     *
!*===========================================================================*
! These are entry points for exceptions (processor generated interrupts, 
! usually caused by error conditions such as an attempt to divide by zero)

_int00:				! interrupt through vector 0
	push	ax
	movb	al,#0
	jmp	exception

_int01:				! interrupt through vector 1, etc
	push	ax
	movb	al,#1
	jmp	exception

_int02:
	push	ax
	movb	al,#2
	jmp	exception

_int03:
	push	ax
	movb	al,#3
	jmp	exception

_int04:
	push	ax
	movb	al,#4
	jmp	exception

_int05:
	push	ax
	movb	al,#5
	jmp	exception

_int06:
	push	ax
	movb	al,#6
	jmp	exception

_int07:
	push	ax
	movb	al,#7
	!jmp	exception

exception:
  cseg	movb	ex_number,al	! it is cumbersome to get this into dseg
	pop	ax
	call	save
  cseg	push	ex_number	! high byte is constant 0
	call	_exception	! do whatever is necessary (sti only if safe)
	add	sp,#2
	cli
	ret


!*===========================================================================*
!*				level0_call				     *
!*===========================================================================*
_level0_call:
	call	save
	jmp	@_level0_func

!*===========================================================================*
!*				data					     *
!*===========================================================================*
! NB some variables are stored in code segment.

ex_number:			! exception number
	.space	2


!*===========================================================================*
!*			variants for 286 protected mode			     *
!*===========================================================================*

! Most routines are different in 286 protected mode.
! The only essential difference is that an interrupt in protected mode
! (usually) switches the stack, so there is less to do in software.

! These functions are reached along jumps patched in by klib_init_prot():

	.define		p_restart	! replaces _restart
	.define		p_save		! replaces save

! These exception and software-interrupt handlers are enabled by the new
! interrupt vector table set up in protect.c:

	.define		_divide_error		! _int00
	.define		_single_step_exception	! _int01
	.define		_nmi			! _int02
	.define		_breakpoint_exception	! _int03
	.define		_overflow		! _int04
	.define		_bounds_check		! _int05
	.define		_inval_opcode		! _int06
	.define		_copr_not_available	! _int07
	.define		_double_fault		! (286 trap)
	.define		_copr_seg_overrun	! (etc)
	.define		_inval_tss
	.define		_segment_not_present
	.define		_stack_exception
	.define		_general_protection
	.define		_p_s_call		! _s_call
	.define		_level0_call

! The hardware interrupt handlers need not be altered apart from putting
! them in the new table (save() handles the differences).
! Some of the intxx handlers (those for exceptions which do not push an
! error code) need not have been replaced, but the names here are better.

#include "protect.h"

/* Selected 286 tss offsets. */
#define TSS2_S_SP0	2

! imported variables

	.extern		_tss
	.extern		_level0_func

!*===========================================================================*
!*				p_save					     *
!*===========================================================================*
! Save for 286 protected mode.
! This is much simpler than for 8086 mode, because the stack already points
! into process table, or has already been switched to the kernel stack.

p_save:
	cld			! set direction flag to a known value
	pusha			! save "general" registers
	push	ds		! save ds
	push	es		! save es
	mov	dx,ss		! ss is kernel data segment
	mov	ds,dx		! load rest of kernel segments
	mov	es,dx
	mov	bp,sp		! prepare to return
	incb	_k_reenter	! from -1 if not reentering
	jnz	set_p1_restart	! stack is already kernel stack
	mov	sp,#k_stktop
	push	#p_restart	! build return address for interrupt handler
	jmp	@RETADR-P_STACKBASE(bp)

set_p1_restart:
	push	#p1_restart
	jmp	@RETADR-P_STACKBASE(bp)


!*===========================================================================*
!*				p_s_call				     *
!*===========================================================================*
_p_s_call:
	cld			! set direction flag to a known value
	sub	sp,#6*2		! skip RETADR, ax, cx, dx, bx, st
	push	bp		! stack already points into process table
	push	si
	push	di
	push	ds
	push	es
	mov	dx,ss
	mov	ds,dx
	mov	es,dx
	incb	_k_reenter
	mov	si,sp		! assumes P_STACKBASE == 0
	mov	sp,#k_stktop
				! end of inline save
	sti			! allow SWITCHER to be interrupted
				! now set up parameters for C routine sys_call
	push	bx		! pointer to user message
	push	ax		! src/dest
	push	cx		! SEND/RECEIVE/BOTH
	call	_sys_call	! sys_call(function, src_dest, m_ptr)
				! caller is now explicitly in proc_ptr
	mov	AXREG(si),ax	! sys_call MUST PRESERVE si
	cli

! Fall into code to restart proc/task running.

p_restart:

! Flush any held-up interrupts.
! This reenables interrupts, so the current interrupt handler may reenter.
! This does not matter, because the current handler is about to exit and no
! other handlers can reenter since flushing is only done when k_reenter == 0.

	cmp	_held_head,#0	! do fast test to usually avoid function call
	jz	p_over_call_unhold
	call	_unhold		! this is rare so overhead is acceptable
p_over_call_unhold:
	mov	si,_proc_ptr
	lldt	P_LDT_SEL(si)		! enable segment descriptors for task
	lea	ax,P_STACKTOP(si)	! arrange for next interrupt
	mov	_tss+TSS2_S_SP0,ax	! to save state in process table
	mov	sp,si		! assumes P_STACKBASE == 0
p1_restart:
	decb	_k_reenter
	pop	es
	pop	ds
	popa
	add	sp,#2		! skip return adr
	iret			! continue process


!*===========================================================================*
!*				exception handlers			     *
!*===========================================================================*
_divide_error:
	push	#DIVIDE_VECTOR
	jmp	p_exception

_single_step_exception:
	push	#DEBUG_VECTOR
	jmp	p_exception

_nmi:
	push	#NMI_VECTOR
	jmp	p_exception

_breakpoint_exception:
	push	#BREAKPOINT_VECTOR
	jmp	p_exception

_overflow:
	push	#OVERFLOW_VECTOR
	jmp	p_exception

_bounds_check:
	push	#BOUNDS_VECTOR
	jmp	p_exception

_inval_opcode:
	push	#INVAL_OP_VECTOR
	jmp	p_exception

_copr_not_available:
	push	#COPROC_NOT_VECTOR
	jmp	p_exception

_double_fault:
	push	#DOUBLE_FAULT_VECTOR
	jmp	errexception

_copr_seg_overrun:
	push	#COPROC_SEG_VECTOR
	jmp	p_exception

_inval_tss:
	push	#INVAL_TSS_VECTOR
	jmp	errexception

_segment_not_present:
	push	#SEG_NOT_VECTOR
	jmp	errexception

_stack_exception:
	push	#STACK_FAULT_VECTOR
	jmp	errexception

_general_protection:
	push	#PROTECTION_VECTOR
	jmp	errexception


!*===========================================================================*
!*				p_exception				     *
!*===========================================================================*
! This is called for all exceptions which do not push an error code.

p_exception:
  sseg	pop	ds_ex_number
	call	p_save
	jmp	p1_exception


!*===========================================================================*
!*				errexception				     *
!*===========================================================================*
! This is called for all exceptions which push an error code.

errexception:
  sseg	pop	ds_ex_number
  sseg	pop	trap_errno
	call	p_save
p1_exception:			! Common for all exceptions.
	push	ds_ex_number
	call	_exception
	add	sp,#2
	cli
	ret


!*===========================================================================*
!*				data					     *
!*===========================================================================*

	.data
begdata:
	.data2	0x526F		! this must be the first data entry (magic #)

	.bss
begbss:
k_stack:
	.space	K_STACK_BYTES	! kernel stack
k_stktop:			! top of kernel stack
	.comm	ds_ex_number, 2
	.comm	trap_errno, 2
