.PATH.S: ${.PARSEDIR}
AES_SRCS = aes_core.c aes_cbc.c aes-ppc.S aesp8-ppc.S vpaes-ppc.S
AESCPPFLAGS = -DAES_ASM -DVPAES_ASM
.include "../../aes.inc"
