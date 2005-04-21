.sect .text
.extern	__getsysinfo
.define	_getsysinfo
.align 2

_getsysinfo:
	jmp	__getsysinfo
