/*	$NetBSD: dos_file.S,v 1.6 2009/11/21 11:52:57 dsl Exp $	*/
	
/* extracted from Tor Egge's patches for NetBSD boot */

#include <machine/asm.h>

/*
# MSDOS call "INT 0x21 Function 0x3d" to open a file.
# Call with	%ah = 0x3d
# 		%al = 0x0  (access and sharing modes)
#		%ds:%dx = ASCIZ filename
#		%cl = attribute mask of files to look for
*/

	.globl _C_LABEL(doserrno)
_C_LABEL(doserrno):	.long 1

ENTRY(dosopen)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	
	movl	0x8(%ebp), %edx		# File name.

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%ds
	movl	%edx, %eax
	shrl	$4, %eax
	mov	%ds, %si
	add	%si, %ax
	mov	%ax, %ds
	and	$0xf, %dx

	movb	$0x3d, %ah		# Open existing file.
	movb	$0x0 , %al		# ro

	sti
	int	$0x21
	cli
	pop	%ds

	jnc	ok1
	mov	%ax, _C_LABEL(doserrno)
	movl	$-1, %edx
	jmp err1
ok1:
	movl	$0,%edx
	mov	%ax, %dx
err1:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%edx, %eax		# return value in %eax

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%edx
	popl	%ebp
	ret

ENTRY(dosread)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	
	movl	0x8(%ebp), %ebx		# File handle
	movl	0xc(%ebp), %edx		# Buffer.
	movl	0x10(%ebp), %ecx	# Bytes to read

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%ds
	movl	%edx, %eax
	shrl	$4, %eax
	mov	%ds, %si
	add	%si, %ax
	mov	%ax, %ds
	and	$0xf, %dx

	movb	$0x3f, %ah		# Read from file or device

	sti
	int	$0x21
	cli
	pop	%ds

	jnc	ok2
	mov	%ax, _C_LABEL(doserrno)
	movl	$-1, %edx
	jmp	err2
ok2:
	movl	$0,%edx
	mov	%ax, %dx
err2:
	calll	_C_LABEL(real_to_prot)	# back to protected mode
	.code32

	movl	%edx, %eax		# return value in %eax

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

ENTRY(dosclose)
	.code32
	pushl %ebp
	movl  %esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	
	movl	0x8(%ebp), %ebx		# File handle

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x3e, %ah		# Close file.

	sti
	int	$0x21
	cli

	jnc	ok3
	mov	%ax, _C_LABEL(doserrno)
	movl	$-1, %ebx
	jmp	err3
ok3:
	movl	$0, %ebx
err3:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%ebx, %eax		# return value in %eax

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

ENTRY(dosseek)
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	
	movl	0x8(%ebp), %ebx		# File handle
	movl	0xc(%ebp), %ecx		# Offset
	movl	0x10(%ebp) , %edx	# whence

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x42, %ah		# Seek
	movb	%dl, %al		# whence
	mov	%cx, %dx		#offs lo
	shrl	$0x10, %ecx		#offs hi

	sti
	int	$0x21
	cli

	jnc	ok4
	mov	%ax, _C_LABEL(doserrno)
	movl	$-1, %edx
	jmp	err4
ok4:
	shll	$0x10, %edx	#new ofs hi
	mov	%ax, %dx	#new ofs lo
err4:
	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%edx, %eax		# return value in %eax

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret
