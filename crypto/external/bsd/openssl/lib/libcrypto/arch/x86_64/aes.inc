.PATH.S: ${.PARSEDIR}
AES_SRCS = aes-x86_64.S aesni-x86_64.S bsaes-x86_64.S vpaes-x86_64.S
AESCPPFLAGS = -DAES_ASM -DVPAES_ASM -DBSAES_ASM
AESNI = yes
.include "../../aes.inc"
