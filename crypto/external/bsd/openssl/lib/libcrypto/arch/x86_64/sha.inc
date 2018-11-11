.PATH.S: ${.PARSEDIR}
SHA_SRCS = sha1-x86_64.S sha1-mb-x86_64.S
SHACPPFLAGS = -DSHA1_ASM
.if 0
SHA_SRCS += sha512-x86_64.S sha256-mb-x86_64.S
SHACPPFLAGS+= -DSHA256_ASM -DSHA512_ASM
.endif
.include "../../sha.inc"
