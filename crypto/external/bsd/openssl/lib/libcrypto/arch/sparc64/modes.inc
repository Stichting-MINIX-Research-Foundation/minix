.PATH.S: ${.PARSEDIR}
MODES_SRCS = ghash-sparcv9.S
MODESCPPFLAGS = -DGHASH_ASM
.include "../../modes.inc"
