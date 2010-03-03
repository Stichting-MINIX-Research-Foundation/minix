.text
.extern	__alarm
.globl	_alarm
.balign	2

_alarm:
	jmp	__alarm
