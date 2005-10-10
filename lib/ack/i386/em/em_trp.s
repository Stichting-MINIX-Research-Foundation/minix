.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .trp
.extern .trppc, .stop

		! eax is trap number
.trp:
	xor     ebx,ebx
	xchg    ebx,(.trppc)
	test    ebx,ebx
	jz      2f
	push    eax
	call    ebx
	pop     eax
	ret
2:
	push	eax
	call    .stop
