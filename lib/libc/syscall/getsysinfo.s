.sect .text
.extern	__getsysinfo
.define	_getsysinfo
.extern	__getsysinfo_up
.define	_getsysinfo_up
.align 2

_getsysinfo:
	jmp	__getsysinfo
_getsysinfo_up:
	jmp	__getsysinfo_up
