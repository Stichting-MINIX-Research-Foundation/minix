!	getprocessor() - determine processor type	Author: Kees J. Bot
!								26 Jan 1994

.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text

! int getprocessor(void);
!	Return 386, 486, 586, ...

.define	_getprocessor

_getprocessor:
	push	ebp
	mov	ebp, esp
	and	esp, 0xFFFFFFFC	! Align stack to avoid AC fault
	mov	ecx, 0x00040000	! Try to flip the AC bit introduced on the 486
	call	flip
	mov	eax, 386	! 386 if it didn't react to "flipping"
	jz	gotprocessor
	mov	ecx, 0x00200000	! Try to flip the ID bit introduced on the 586
	call	flip
	mov	eax, 486	! 486 if it didn't react
	jz	gotprocessor
	pushf
	pusha			! Save the world
	mov	eax, 1
	.data1	0x0F, 0xA2	! CPUID instruction tells the processor type
	andb	ah, 0x0F	! Extract the family (5, 6, ...)
	movzxb	eax, ah
	imul	eax, 100	! 500, 600, ...
	add	eax, 86		! 586, 686, ...
	mov	7*4(esp), eax	! Pass eax through
	popa
	popf
gotprocessor:
	leave
	ret

flip:
	pushf			! Push eflags
	pop	eax		! eax = eflags
	mov	edx, eax	! Save original eflags
	xor	eax, ecx	! Flip the bit to test
	push	eax		! Push modified eflags value
	popf			! Load modified eflags register
	pushf
	pop	eax		! Get it again
	push	edx
	popf			! Restore original eflags register
	xor	eax, edx	! See if the bit changed
	test	eax, ecx
	ret
