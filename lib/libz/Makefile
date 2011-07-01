# Minix makefile for libz.

LIB=		z

SRCS= adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c \
      gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c

.if (${NBSD_LIBC} != "no")
INCSDIR= /usr/include
.else
INCSDIR= /usr/include.ack
.endif

INCS=           zconf.h zlib.h

CPPFLAGS += -DNO_VIZ=1

.include <bsd.lib.mk>
