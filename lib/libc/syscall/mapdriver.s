.sect .text
.extern	__mapdriver
.define	_mapdriver
.align 2

_mapdriver:
	jmp	__mapdriver
