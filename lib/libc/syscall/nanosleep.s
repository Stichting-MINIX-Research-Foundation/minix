.sect .text
.extern	__nanosleep
.define	_nanosleep

_nanosleep:
	jmp	__nanosleep
