.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .mon

.mon:
.extern .stop
	call    .stop
