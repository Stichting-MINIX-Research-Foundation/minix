.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
MODES_SRCS = ghash-sparcv9.o
MODESCPPFLAGS = -DGHASH_ASM
.endif
.include "../../modes.inc"
