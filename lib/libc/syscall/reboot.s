.sect .text
.extern	__reboot
.define	_reboot
.align 2

_reboot:
	jmp	__reboot
