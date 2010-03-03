.text
#if defined(__ACK__)
.rom
#else
.data
#endif
.data
.bss
#if defined(__ACK__)
.end	/* only for declaration of _end, __end or endbss. */
.globl _end
_end:
#endif
