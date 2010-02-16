#
.sect .text
.align _EM_WSIZE
.define __etext, endtext
__etext:
endtext:
.sect .rom
.align _EM_WSIZE
.define endrom
endrom:
.sect .data
.align _EM_WSIZE
.define __edata, enddata
__edata:
enddata:
.sect .bss
.align _EM_WSIZE
.sect .end	! only for declaration of _end, __end or endbss.
.define __end, endbss
__end:
endbss:
