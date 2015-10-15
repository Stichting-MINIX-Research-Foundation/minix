.PATH.S: ${.PARSEDIR}
AES_SRCS = aes_core.c aes_cbc.c aes-sparcv9.S
AESCPPFLAGS = -DAES_ASM
.include "../../aes.inc"
