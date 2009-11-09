#
.sect .text; .sect .rom; .sect .data; .sect .bss
.define __fptrp
.sect .text
__fptrp:
#if __i386
	push	ebp
	mov	ebp, esp
	mov	eax, 8(bp)
	call	.Xtrp
	leave
	ret
#else /* i86 */
	push	bp
	mov	bp, sp
	mov	ax, 4(bp)
	call	.Xtrp
	jmp	.cret
#endif
