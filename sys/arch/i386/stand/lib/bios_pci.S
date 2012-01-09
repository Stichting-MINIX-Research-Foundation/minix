/*	$NetBSD: bios_pci.S,v 1.6 2005/12/11 12:17:48 christos Exp $	*/

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* minimal calls to PCI BIOS */

#include <machine/asm.h>

#define	addr32	.byte 0x67
#define	data32	.byte 0x66

#define PCI_FUNCTION_ID 0xb1
#define PCI_BIOS_PRESENT 0x01
#define FIND_PCI_DEVICE 0x02
#define READ_CONFIG_DWORD 0x0a
#define WRITE_CONFIG_DWORD 0x0d

/* int pcibios_present(int *signature)
    return: AX from BIOS call, -1 on error
    var param: EDX from BIOS call, must be signature "PCI "
*/
ENTRY(pcibios_present)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$PCI_FUNCTION_ID, %ah
	movb	$PCI_BIOS_PRESENT, %al
	int	$0x1a

	jnc	ok1
	movl	$-1, %ebx
	jmp err1

ok1:
	xorl	%ebx, %ebx
	mov	%ax, %bx
err1:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	8(%ebp), %eax
	movl	%edx, (%eax)

	movl	%ebx, %eax		# return value in %eax

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/* int pcibios_finddev(int vendor, int device, int index, int *busdevfcn)
    return: AH from BIOS call, -1 on error
    var param: BX from BIOS call, contains bus/device/function
*/
ENTRY(pcibios_finddev)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi

	movl	8(%ebp), %edx
	movl	12(%ebp), %ecx
	movl	16(%ebp), %esi

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$PCI_FUNCTION_ID, %ah
	movb	$FIND_PCI_DEVICE, %al
	int	$0x1a

	jnc	ok2
	movl	$-1, %edx
	jmp err2

ok2:
	movl	$0,%edx
	movb	%ah, %dl
err2:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	20(%ebp), %eax
	mov	%bx, (%eax)

	movl	%edx, %eax		# return value in %eax

	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/* int pcibios_cfgread(int busdevfcn, int offset, int *value)
    return: AH from BIOS call, -1 on error
    var param: ECX from BIOS call, contains value read
*/
ENTRY(pcibios_cfgread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%edi

	movl	8(%ebp), %ebx
	movl	12(%ebp), %edi

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$PCI_FUNCTION_ID, %ah
	movb	$READ_CONFIG_DWORD, %al
	int	$0x1a

	jnc	ok3
	movl	$-1, %edx
	jmp err3

ok3:
	movl	$0,%edx
	movb	%ah, %dl
err3:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	16(%ebp), %eax
	movl	%ecx, (%eax)

	movl	%edx, %eax		# return value in %eax

	popl	%edi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/* int pcibios_cfgwrite(int busdevfcn, int offset, int value)
    return: AH from BIOS call, -1 on error
    var param: ECX from BIOS call, contains value read
*/
ENTRY(pcibios_cfgwrite)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%edi

	movl	8(%ebp), %ebx
	movl	12(%ebp), %edi
	movl	16(%ebp), %ecx

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$PCI_FUNCTION_ID, %ah
	movb	$WRITE_CONFIG_DWORD, %al
	int	$0x1a

	jnc	ok4
	movl	$-1, %edx
	jmp err4

ok4:
	movl	$0,%edx
	movb	%ah, %dl
err4:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%edx, %eax		# return value in %eax

	popl	%edi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret
