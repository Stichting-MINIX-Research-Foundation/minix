/*	getprocessor() - determine processor type	Author: Kees J. Bot */
/*								26 Jan 1994 */
#include <machine/asm.h>

/* int getprocessor(void); */
/*	Return 386, 486, 586, ... */
ENTRY(getprocessor)
	push	%ebp
	movl	%esp, %ebp
	andl	$0xFFFFFFFC, %esp	/* Align stack to avoid AC fault */
	movl	$0x00040000, %ecx	/* Try to flip the AC bit introduced on the 486 */
	call	flip
	movl	$386, %eax	/* 386 if it didn't react to "flipping" */
	je	gotprocessor
	movl	$0x00200000, %ecx	/* Try to flip the ID bit introduced on the 586 */
	call	flip
	movl	$486, %eax	/* 486 if it didn't react */
	je	gotprocessor
	pushf
	pusha	/* Save the world */
	movl	$1, %eax
.byte	0x0F, 0xA2	/* CPUID instruction tells the processor type */
	andb	$0x0F, %ah	/* Extract the family (5, 6, ...) */
	movzbl	%ah, %eax
	cmpl	$15, %eax	/* 15: extended family */
	jne	direct
	movl	$6, %eax	/* Make it 686 */
direct:
	imull	$100, %eax	/* 500, 600, ... */
	addl	$86, %eax	/* 586, 686, ... */
	movl	%eax, 7*4(%esp)	/* Pass eax through */
	popa
	popf
gotprocessor:
	leave
	ret

flip:
	pushf	/* Push eflags */
	pop	%eax	/* eax = eflags */
	movl	%eax, %edx	/* Save original eflags */
	xorl	%ecx, %eax	/* Flip the bit to test */
	push	%eax	/* Push modified eflags value */
	popf	/* Load modified eflags register */
	pushf
	pop	%eax	/* Get it again */
	push	%edx
	popf	/* Restore original eflags register */
	xorl	%edx, %eax	/* See if the bit changed */
	testl	%ecx, %eax
	ret
