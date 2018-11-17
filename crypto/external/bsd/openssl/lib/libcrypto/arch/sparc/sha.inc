.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
SHA_SRCS = sha1-sparcv9.S
SHACPPFLAGS = -DSHA1_ASM
AFLAGS.sha1-sparcv9.S+= -Wa,-Av9
.endif
.include "../../sha.inc"
