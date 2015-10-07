.PATH.S: ${.PARSEDIR}
AES_SRCS = aes-586.S aesni-x86.S
AESNI = yes
AESCPPFLAGS = -DAES_ASM -DOPENSSL_IA32_SSE2
.include "../../aes.inc"
