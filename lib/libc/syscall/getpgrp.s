.sect .text
.extern	__getpgrp
.define	_getpgrp
.align 2

_getpgrp:
	jmp	__getpgrp
