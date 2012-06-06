#	$NetBSD: Makefile,v 1.5 2011/08/09 12:56:41 joerg Exp $

PROG=		cc1
SRCS=		${G_C_OBJS:Nlibcpp.a:.o=.c} main.c ${PROG}-checksum.c
CPPFLAGS+=	-DPREFIX=\"/usr\"

.include "../Makefile.backend"
.include "../Makefile.libcpp"
.include "../Makefile.libdecnumber"

# VER_CPPFLAGS from Makefile.inc
CPPFLAGS.c-cppbuiltin.c=	${VER_CPPFLAGS}

COPTS+=	-Wno-stack-protector

LDADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} -lintl -lz -lm
DPADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} ${LIBINTL} ${LIBZ} ${LIBM}

.PATH: ${DIST}/gcc ${G_out_file:H}
