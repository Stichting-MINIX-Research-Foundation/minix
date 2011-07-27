# Makefile for libminixutil

LIB=	minixutil

.if (${NBSD_LIBC} != "no")
# SHA and MD5 functions already in libc
SRCS=	openpty.c efun.c
.else
SRCS=	openpty.c sha2.c efun.c \
		sha1.c sha1hl.c sha2.c sha2hl.c md5c.c md5hl.c
.endif
.include <bsd.lib.mk>
