.include <bsd.own.mk>

PROG=	bsdtar
SRCS=	bsdtar.c \
		cmdline.c \
		getdate.c \
		read.c \
		subst.c \
		tree.c \
		util.c \
		write.c
.include "${.CURDIR}/libarchive_fe/Makefile.inc"

DPADD+= ${LIBARCHIVE} ${LIBZ} ${LIBBZ2}
LDADD+=	-larchive -lbz2 -lz
CPPFLAGS+= -DHAVE_CONFIG_H
CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/libarchive_fe

LINKS+= ${BINDIR}/bsdtar ${BINDIR}/tar
MLINKS+= bsdtar.1 tar.1

.include <bsd.prog.mk>
