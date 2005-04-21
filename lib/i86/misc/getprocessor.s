!	getprocessor() - determine processor type	Author: Kees J. Bot
!								26 Jan 1994

.text

	o32 = 0x66		! 32 bit operand size prefix

! int getprocessor(void);
!	Return 86, 186, 286, 386, 486, 586, ...

.define	_getprocessor

_getprocessor:
	push	bp
	mov	bp, sp
	push	sp		! see if pushed sp == sp
	pop	ax
	cmp	ax, sp
	jz	new_processor
	mov	cx, #0x0120	! see if shifts are mod 32
	shlb	ch, cl		! zero tells if 86
	mov	ax, #86
	jz	got_processor
	mov	ax, #186
	jmp	got_processor

new_processor:			! see if high bits are set in saved IDT
	sub	sp, #6		! space for IDT ptr
	sidt	-6(bp)		! save 3 word IDT ptr
	cmpb	-1(bp), #0xFF	! top byte of IDT ptr is always FF on 286
	mov	ax, #286
	je	got_processor

! 386, 486, 586
	and	sp, #0xFFFC	! Align stack to avoid AC fault (needed?)
	mov	cx, #0x0004	! Try to flip the AC bit introduced on the 486
	call	flip
	mov	ax, #386	! 386 if it didn't react to "flipping"
	jz	got_processor
	mov	cx, #0x0020	! Try to flip the ID bit introduced on the 586
	call	flip
	mov	ax, #486	! 486 if it didn't react
	jz	got_processor
	.data1	o32
	pushf
	.data1	o32
	pusha			! Save the world
	.data1	o32
	xor	ax, ax
	inc	ax		! eax = 1
	.data1	0x0F, 0xA2	! CPUID instruction tells the processor type
	andb	ah, #0x0F	! Extract the family (5, 6, ...)
	movb	al, ah
	movb	ah, #100
	mulb	ah		! 500, 600, ...
	add	ax, #86		! 586, 686, ...
	mov	bx, sp
	mov	7*4(bx), ax	! Pass ax through
	.data1	o32
	popa
	.data1	o32
	popf

got_processor:
	mov	sp, bp
	pop	bp
	ret

flip:
	push	bx		! Save bx and realign stack to multiple of 4
	.data1	o32		! About to operate on a 32 bit object
	pushf			! Push eflags
	pop	ax
	pop	dx		! dx:ax = eflags
	mov	bx, dx		! Save original eflags (high word only)
	xor	dx, cx		! Flip the bit to test
	push	dx
	push	ax		! Push modified eflags value
	.data1	o32
	popf			! Load modified eflags register
	.data1	o32
	pushf
	pop	ax
	pop	dx		! Get it again
	push	bx
	push	ax
	.data1	o32
	popf			! Restore original eflags register
	xor	dx, bx		! See if the bit changed
	test	dx, cx
	pop	bx		! Restore bx
	ret
