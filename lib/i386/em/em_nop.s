.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .nop
.extern printd, printc, hol0

.nop:
	mov     eax,(hol0)
	call    printd
	movb    al,'\n'
	jmp     printc
