.PATH.S: ${.PARSEDIR}
CPPFLAGS+=	-DRC4_CHUNK="unsigned long" -DRC4_INT="unsigned char"
.include "../../rc4.inc"
