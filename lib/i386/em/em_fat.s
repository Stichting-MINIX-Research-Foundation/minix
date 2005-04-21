.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .fat

.fat:
.extern .trp
.extern .stop
	call    .trp
	call    .stop
	! no return
