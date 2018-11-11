.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
AES_SRCS = aes_core.c aes_cbc.c aes-sparcv9.S aest4-sparcv9.S
AESCPPFLAGS = -DAES_ASM
AFLAGS.aes-sparcv9.S+= -Wa,-Av9
AFLAGS.aest4-sparcv9.S+= -Wa,-Av9
.endif
.include "../../aes.inc"
