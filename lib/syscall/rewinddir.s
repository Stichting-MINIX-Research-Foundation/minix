.sect .text
.extern	__rewinddir
.define	_rewinddir
.align 2

_rewinddir:
	jmp	__rewinddir
