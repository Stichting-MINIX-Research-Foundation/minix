.PATH.S: ${.PARSEDIR}
SHA_SRCS = sha1-sparcv9.S
SHACPPFLAGS = -DSHA1_ASM
.include "../../sha.inc"
