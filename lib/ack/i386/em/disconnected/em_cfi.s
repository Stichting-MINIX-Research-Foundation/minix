.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cfi

	.sect .text
.cfi:
	mov	bx,sp
	fstcw	4(bx)
	wait
	mov	dx,4(bx)
	or	4(bx),0xc00	! truncating mode
	wait
	fldcw	4(bx)
	cmp	8(bx),4
	jne	2f
				! loc 4 loc ? cfi
	flds	12(bx)
	fistpl	12(bx)
1:
	mov	4(bx),dx
	wait
	fldcw	4(bx)
	ret
2:
				! loc 8 loc ? cfi
	fldd	12(bx)
	fistpl	16(bx)
	jmp	1b
