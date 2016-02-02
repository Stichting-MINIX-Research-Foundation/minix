.PATH.S: ${.PARSEDIR}
MODES_SRCS = ghash-armv4.S ghashv8-armx.S
MODESCPPFLAGS = -DGHASH_ASM
.include "../../modes.inc"
