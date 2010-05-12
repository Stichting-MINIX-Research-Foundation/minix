.include <minix.own.mk>

LIB=		bz2
BZ2DIR=		${MINIXSRCDIR}/commands/bzip2
.PATH:		${BZ2DIR}

SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
CPPFLAGS+=	-I ${BZ2DIR}
CPPFLAGS+=	-O -D_MINIX -D_POSIX_SOURCE
INCS=		bzlib.h
INCSDIR=	/usr/include

.include <minix.lib.mk>
