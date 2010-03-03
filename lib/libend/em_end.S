.text
.balign _EM_WSIZE
.globl __etext, endtext
__etext:
endtext:
#if defined(__ACK__)
.rom
#else
.data
#endif
.balign _EM_WSIZE
.globl endrom
endrom:
.data
.balign _EM_WSIZE
.globl __edata, enddata
__edata:
enddata:
.bss
.balign _EM_WSIZE
#if defined(__ACK__)
.end	/* only for declaration of _end, __end or endbss. */
.globl __end, endbss
__end:
endbss:
#endif
