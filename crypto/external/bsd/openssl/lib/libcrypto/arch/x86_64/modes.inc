.PATH.S: ${.PARSEDIR}
MODES_SRCS += ghash-x86_64.o
MODESCPPFLAGS = -DGHASH_ASM
.include "../../modes.inc"
