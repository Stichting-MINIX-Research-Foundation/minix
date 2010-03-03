.text
.extern	__getsysinfo
.globl	_getsysinfo
.extern	__getsysinfo_up
.globl	_getsysinfo_up
.balign	2

_getsysinfo:
	jmp	__getsysinfo
_getsysinfo_up:
	jmp	__getsysinfo_up
