# $NetBSD: Makefile.inc,v 1.3 2010/11/02 19:14:53 joerg Exp $

.include "../Makefile.inc"

BINDIR=		/bin

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?=	-static
.endif

DPADD=	${LIBARCHIVE_FE} ${LIBARCHIVE} ${LIBBZ2} ${LIBLZMA} ${LIBZ}
LDADD=	-L${LIBARCHIVE_FE_DIR} -larchive_fe -larchive -lbz2 -llzma -lz
CPPFLAGS+=	-I${LIBARCHIVEDIR}/libarchive_fe
