.sect .text
.extern	__umount
.define	_umount
.align 2

_umount:
	jmp	__umount
