.PATH.S: ${.PARSEDIR}
MD5_SRCS = md5-x86_64.S
MD5CPPFLAGS = -DMD5_ASM
.include "../../md5.inc"
