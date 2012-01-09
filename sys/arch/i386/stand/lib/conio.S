/*	$NetBSD: conio.S,v 1.7 2011/06/16 13:27:59 joerg Exp $	*/

/* PC console handling
  originally from: FreeBSD:sys/i386/boot/netboot/start2.S
 */

#include <machine/asm.h>

	.text

/**************************************************************************
CLR - Clear screen
**************************************************************************/
ENTRY(conclr)
	pusha

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	/* Clear screen. */
	movw	$0x0600, %ax
	movw	$0x0700, %bx
	xorw	%cx, %cx
	movw	$0x184f, %dx	/* 80x25 */
	int	$0x10

	/* Home cursor. */
	movb	$0x02, %ah
	xorw	%bx, %bx
	xorw	%dx, %dx
	int	$0x10

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	popa
	ret

/**************************************************************************
PUTC - Print a character
**************************************************************************/
ENTRY(conputc)
	pusha

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movw	$1,%bx
	movb	$0x0e,%ah
	movb	%al, %cl
	int	$0x10

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	popa
	ret

/**************************************************************************
GETC - Get a character
**************************************************************************/
ENTRY(congetc)
	xorl	%eax, %eax
	pusha

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movb	%bl, 28(%esp)

	popa
	ret

/**************************************************************************
ISSHIFT - Check for keyboard interrupt; via shift key
**************************************************************************/
ENTRY(conisshift)
	xorl	%eax, %eax
	pusha

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	xor	%bx,%bx
	movb	$0x2,%ah
	int	$0x16
	testb	$3,%al
	setnz	%bl

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movb	%bl, 28(%esp)

	popa
	ret

/**************************************************************************
ISKEY - Check for keyboard input
**************************************************************************/
ENTRY(coniskey)
	xorl	%eax, %eax
	pusha

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	xor	%bx,%bx
	movb	$0x1,%ah
	int	$0x16
	setnz	%bl

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movb	%bl, 28(%esp)

	popa
	ret
