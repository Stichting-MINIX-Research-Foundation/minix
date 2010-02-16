.sect .text
.extern	__sysuname
.define	_sysuname
.align 2

_sysuname:
	jmp	__sysuname
