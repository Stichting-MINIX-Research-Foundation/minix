.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .rck

	! descriptor address in ebx
	! value in eax, must be left there
.rck:
	cmp     eax,(ebx)
	jl      2f
	cmp     eax,4(ebx)
	jg      2f
	ret
2:
	push    eax
.extern ERANGE
.extern .error
	mov     eax,ERANGE
	call    .error
	pop     eax
	ret
