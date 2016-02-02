.PATH.S: ${.PARSEDIR}
MODES_SRCS += ghash-x86.o
MODESCPPFLAGS = -DGHASH_ASM
.include "../../modes.inc"
