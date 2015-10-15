.PATH.S: ${.PARSEDIR}
AES_SRCS = aes-armv4.S aesv8-armx.S bsaes-armv7.S aes_cbc.c
AESCPPFLAGS = -DAES_ASM -DBSAES_ASM
.include "../../aes.inc"
