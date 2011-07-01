.include <bsd.own.mk>

LIB=		bz2
BZ2DIR=		${MINIXSRCDIR}/commands/bzip2
.PATH:		${BZ2DIR}

SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
CPPFLAGS+=	-I ${BZ2DIR}
INCS=		bzlib.h

.if (${NBSD_LIBC} != "no")
INCSDIR=       /usr/include
.else
INCSDIR=       /usr/include.ack
.endif

.include <bsd.lib.mk>
