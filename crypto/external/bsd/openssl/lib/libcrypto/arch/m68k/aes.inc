.if ${MACHINE_ARCH} != "m68000"
.PATH.S: ${.PARSEDIR}
AES_SRCS = aes-m68k.S aes_cbc.c
AESCPPFLAGS = -DAES_ASM
.endif
.include "../../aes.inc"
