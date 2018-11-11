.PATH.S: ${.PARSEDIR}
MODES_SRCS += ghash-x86.o
MODESCPPFLAGS = -DGHASH_ASM -DOPENSSL_IA32_SSE2

.include "../../modes.inc"
