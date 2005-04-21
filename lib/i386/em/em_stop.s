.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .stop
.stop:
	jmp	___exit
