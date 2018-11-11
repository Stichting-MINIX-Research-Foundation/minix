.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
MODES_SRCS = ghash-sparcv9.S
MODESCPPFLAGS = -DGHASH_ASM
AFLAGS.ghash-sparcv9.S+= -Wa,-Av9
.endif
.include "../../modes.inc"
