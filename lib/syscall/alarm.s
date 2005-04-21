.sect .text
.extern	__alarm
.define	_alarm
.align 2

_alarm:
	jmp	__alarm
