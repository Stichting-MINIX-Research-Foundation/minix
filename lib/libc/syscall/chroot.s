.sect .text
.extern	__chroot
.define	_chroot
.align 2

_chroot:
	jmp	__chroot
