/*	$NetBSD: start_dos.S,v 1.10 2010/12/20 01:12:44 jakllsch Exp $	*/
	
/*
 * startup for DOS .COM programs
 * with input from:
 * netbsd:sys/arch/i386/boot/start.S
 * Tor Egge's patches for NetBSD boot (pr port-i386/1002)
 * freebsd:sys/i386/boot/netboot/start2.S
 * XMS support by Martin Husemann
 */

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>


CR0_PE		=	0x1

	.data
	.globl _C_LABEL(ourseg)
_C_LABEL(ourseg):
	.long	0

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
#ifdef __ELF__
	.align	16
#else
	.align	4
#endif
gdt:
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

#ifdef SUPPORT_LINUX	/* additional dummy */
	.word	0, 0
	.byte	0, 0x00, 0x00, 0
#endif

	/* kernel code segment */
	.globl flatcodeseg
flatcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* kernel data segment */
	.globl flatdataseg
flatdataseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* boot code segment, base will be patched */
bootcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x40, 0

	/* boot data segment, base will be patched */
bootdataseg = . - gdt
#ifdef HEAP_BELOW_64K
	.word	0xffff, 0
	.byte	0, 0x92, 0x00, 0
#else
	.word	0xffff, 0
	.byte	0, 0x92, 0x4f, 0
#endif

	/* 16 bit real mode, base will be patched */
bootrealseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x00, 0

	/* limits (etc) for data segment in real mode */
bootrealdata = . - gdt
	.word	0xffff, 0
	.byte	0, 0x92, 0x00, 0
gdtlen = . - gdt

#ifdef __ELF__
	.align	16
#else
	.align	4
#endif
gdtarg:
	.word	gdtlen-1		/* limit */
	.long	0			/* addr, will be inserted */

	.text
ENTRY(start)
	.code16

	# Check we are in real mode
	movl	%cr0, %eax
	testl	$CR0_PE, %eax
	jz	2f
	mov	$1f, %si
	call	message
	ret
1:	.asciz	"must be in real mode\r\n"
2:

	xorl	%eax, %eax
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	movl	%eax, _C_LABEL(ourseg)
#ifdef STACK_START
	add	$STACK_START / 16, %ax
	mov	%ax, %ss
	mov	$0xfffc, %sp
#endif

	/* fix up GDT entries for bootstrap */
#define FIXUP(gdt_index) \
	movw	%ax, gdt+gdt_index+2;	\
	movb	%bl, gdt+gdt_index+4

	mov	%cs, %ax
	shll	$4, %eax
	shldl	$16, %eax, %ebx
	FIXUP(bootcodeseg)
	FIXUP(bootrealseg)
	FIXUP(bootdataseg)

	/* fix up GDT pointer */
	addl	$gdt, %eax
	movl	%eax, gdtarg+2

	/* change to protected mode */
	calll	_C_LABEL(real_to_prot)
	.code32

	/* clear the bss */
	movl	$_C_LABEL(edata), %edi
	movl	$_C_LABEL(end), %ecx
	subl	%edi, %ecx
	xorb	%al, %al
	rep
	stosb

	call	_C_LABEL(doscommain)
ENTRY(_rtt)
	call	_C_LABEL(prot_to_real)
	.code16
ENTRY(exit16)
	sti
	movb	$0x4c, %ah		/* return */
	int	$0x21

/*
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */
ENTRY(real_to_prot)
	.code16
	pushl	%eax
	# guarantee that interrupt is disabled when in prot mode
	cli

	# load the gdtr
	lgdtl	%cs:gdtarg

	# set the PE bit of CR0
	movl	%cr0, %eax
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	# make intrasegment jump to flush the processor pipeline and
	# reload CS register
	ljmp	$bootcodeseg, $xprot

xprot:
	.code32
	# we are in USE32 mode now
	# set up the protected mode segment registers : DS, SS, ES
	movl	$bootdataseg, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
#ifdef STACK_START
	addl	$STACK_START, %esp
#endif

	popl	%eax
	ret

/*
 * prot_to_real()
 * 	transfer from protected mode to real mode
 */
ENTRY(prot_to_real)
	.code32
	pushl	%eax
	# set up a dummy stack frame for the second seg change.
	# Adjust the intersegment jump instruction following 
	# the clearing of protected mode bit.
	# This is self-modifying code, but we need a writable
	# code segment, and an intersegment return does not give us that.

	movl	_C_LABEL(ourseg), %eax
	movw	%ax, xreal-2

	/*
	 * Load the segment registers while still in protected mode.
	 * Otherwise the control bits don't get changed.
	 * The correct values are loaded later.
	 */
	movw	$bootrealdata, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	# Change to use16 mode.
	ljmp	$bootrealseg, $x16

x16:
	.code16
	# clear the PE bit of CR0
	movl	%cr0, %eax
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0

	# Here we have an 16 bits intersegment jump.
	ljmp	$0, $xreal		/* segment patched above */

xreal:
	# we are in real mode now
	# set up the real mode segment registers : DS, SS, ES
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
#ifdef STACK_START
	add	$STACK_START / 16, %ax
	mov	%ax, %ss
	subl	$STACK_START, %esp
#else
	mov	%ax, %ss
#endif
	push	%bp
	movw	%sp, %bp
	/* check we are returning to an address below 64k */
	movw	2/*bp*/ + 4/*eax*/ + 2(%bp), %ax	/* high bits ret addr */
	test	%ax, %ax
	jne	1f
	pop	%bp

	sti
	popl	%eax
	retl

1:	movw	$2f, %si
	call	message
	movl	2/*bp*/ + 4/*eax*/(%bp), %eax		/*  return address */
	call	dump_eax
	jmp	exit16
2:	.asciz  "prot_to_real can't return to "


/**************************************************************************
___MAIN - Dummy to keep GCC happy
**************************************************************************/
ENTRY(__main)
	ret

/*
 * pbzero(dst, cnt)
 *	where dst is a physical address and cnt is the length
 */
ENTRY(pbzero)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	mov	%ax, %es

	movl	8(%ebp), %edi		# destination
	movl	12(%ebp), %ecx		# count
	xorl	%eax, %eax		# value

	rep
	stosb

	popl	%edi
	popl	%es
	popl	%ebp
	ret

/*
 * vpbcopy(src, dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */
ENTRY(vpbcopy)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	mov	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

/*
 * pvbcopy(src, dst, cnt)
 *	where src is a physical address and dst is a virtual address
 */
ENTRY(pvbcopy)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ds
	pushl	%esi
	pushl	%edi

	cld

	# set %ds to point at the flat segment
	movl	$flatdataseg, %eax
	mov	%ax, %ds

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%ds
	popl	%ebp
	ret

ENTRY(vtophys)
	.code32
	movl	_C_LABEL(ourseg), %eax
	shll	$4, %eax
	addl	4(%esp), %eax
	ret

message:
	.code16
	pushal
message_1:
	cld
1:	lodsb
	testb	%al, %al
	jz	2f
	movb	$0xe, %ah
	movw	$1, %bx
	int	$0x10
	jmp	1b

2:	movb	$0x86, %ah
	mov	$16, %cx
	int	$0x15			/* delay about a second */
	popal
	ret

/* These are useful for debugging
 */
	.data
eax_buf:
	.long	0, 0, 0, 0
	.text
ENTRY(dump_eax)
	.code16
	pushal
	movw	$eax_buf, %si
	mov	%si, %di
	movw	$8, %cx
1:	roll	$4, %eax
	mov	%ax, %bx
	andb	$0x0f, %al
	addb	$0x30, %al			/* 30..3f - clear AF */
#if 1 /* 5 bytes to generate real hex... */
	daa					/* 30..39, 40..45 */
	addb	$0xc0, %al			/* f0..f9, 00..05 */
	adcb	$0x40, %al			/* 30..39, 41..45 */
#endif
	movb	%al, (%di)			/* %es != %ds, so can't ... */
	inc	%di				/* ... use stosb */
	mov	%bx, %ax
	loop	1b
	movw	$0x20, %ax			/* space + null */
	movw	%ax, (%di)
	jmp	message_1

	.globl	_C_LABEL(trace_word)
_C_LABEL(trace_word):
	.code32
	movl	4(%esp), %edx

	call	_C_LABEL(prot_to_real)
	.code16
	movl	%edx, %eax
	call	dump_eax
	calll	_C_LABEL(real_to_prot)
	.code32
	ret

	.globl	_C_LABEL(trace_str)
_C_LABEL(trace_str):
	.code32
	pushl	%esi

	call	_C_LABEL(prot_to_real)
	.code16
	mov	%sp, %si
	mov	8(%si), %si
	call	message
	calll	_C_LABEL(real_to_prot)
	.code32
	popl	%esi
	ret

#ifdef XMS

/* pointer to XMS driver, 0 if no XMS used */

	.data
_C_LABEL(xmsdrv):
	.long	0

	.text
ENTRY(checkxms)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edx
	pushl	%es
	pushl	%esi
	pushl	%edi

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movw	$0x4300, %ax
	int	$0x2f			/* check if XMS installed */
	cmpb	$0x80, %al
	jnz	noxms

	movw	$0x4310, %ax
	int	$0x2f			/* get driver address */

	movw	%bx, _C_LABEL(xmsdrv)	/* save es:bx to _xmsdrv */
	movw	%es, _C_LABEL(xmsdrv) + 2

	movb	$0x08, %ah		/* XMS: query free extended memory */
#if 0
	movb	$0x00, %bl
#endif
	lcall	*_C_LABEL(xmsdrv)
	jmp	xdone

noxms:		/* no XMS manager found */
	mov	$0, %dx

xdone:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	xorl	%eax, %eax
	movw	%dx, %ax

	popl	%edi
	popl	%esi
	popl	%es
	popl	%edx
	popl	%ebx
	popl	%ebp
	ret

/*
	Allocate a block of XMS memory with the requested size
		void *xmsalloc(long int kBytes);

	Depends on _xmsdrv being set by getextmem() before first call
	to this function.

	Return value: a physical address.
*/
ENTRY(xmsalloc)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edx
	pushl	%esi
	pushl	%edi

	movl	0x8(%ebp), %edx # Kbytes needed

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x09, %ah		# XMS allocate block
	lcall	*_C_LABEL(xmsdrv)	# result: handle in %dx
	movb	$0x0c, %ah		# XMS lock block
	lcall	*_C_LABEL(xmsdrv)	# result: 32 bit physical addr in DX:BX

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%edx, %eax
	shl	$16, %eax
	movw	%bx, %ax	# result in %eax

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ebx
	popl	%ebp
	ret

/*
 * ppbcopy(src, dst, cnt)
 *	where src and dst are physical addresses
 */
ENTRY(ppbcopy)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	mov	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	es
	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

#endif
