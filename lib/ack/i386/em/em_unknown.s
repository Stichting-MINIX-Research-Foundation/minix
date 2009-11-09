.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .unknown
.extern EILLINS, .fat

.unknown:
	mov  eax,EILLINS
	push eax
	jmp  .fat
