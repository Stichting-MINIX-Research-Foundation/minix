.sect .text; .sect .rom; .sect .data; .sect .bss
.define .error
.define .Xtrp

	! eax is trap number
	! all registers must be saved
	! because return is possible
	! May only be called with error no's <16
.sect .text
.error:
	mov  ecx,eax
	mov  ebx,1
	sal  ebx,cl
.extern .ignmask
.extern .trp
	test ebx,(.ignmask)
	jne  2f
	call    .trp
2:
	ret

.Xtrp:
	pusha
	cmp	eax,16
	jge	1f
	call	.error
	popa
	ret
1:
	call	.trp
	popa
	ret
