.sect .text
.extern	__ioctl
.define	_ioctl
.align 2

_ioctl:
	jmp	__ioctl
