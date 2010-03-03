.text
#if defined(__ACK__)
.rom
#else
.data
#endif
.data
.balign _EM_WSIZE
.globl _edata
_edata:
