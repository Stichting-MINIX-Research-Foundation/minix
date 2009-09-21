.sect .text
.extern	__sleep
.define	_sleep
.align 2

_sleep:
	jmp	__sleep

.extern	__nanosleep
.define	_nanosleep

_nanosleep:
	jmp	__nanosleep
