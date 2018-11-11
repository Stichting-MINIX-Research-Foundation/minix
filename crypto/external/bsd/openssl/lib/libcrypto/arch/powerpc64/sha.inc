.PATH.S: ${.PARSEDIR}
SHA_SRCS = sha1-ppc.S
SHACPPFLAGS = -DSHA1_ASM
.if NOT_USING_SHAXXX_FROM_LIBC
SHA_SRCS += sha512-ppc.S sha512p8-ppc.S
SHA_SRCS += sha256-ppc.S sha256p8-ppc.S
SHACPPFLAGS+= -DSHA256_ASM -DSHA512_ASM
.endif
.include "../../sha.inc"
