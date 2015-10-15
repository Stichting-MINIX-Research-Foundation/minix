.if ${MACHINE_ARCH:M*armv4*} == ""
.PATH.S: ${.PARSEDIR}
SHA_SRCS = sha1-armv4-large.S sha256-armv4.S sha512-armv4.S
SHACPPFLAGS = -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM
.endif
.include "../../sha.inc"

