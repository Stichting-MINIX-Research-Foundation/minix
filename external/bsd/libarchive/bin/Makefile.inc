# $NetBSD: Makefile.inc,v 1.4 2017/04/20 13:11:04 joerg Exp $

.include "../Makefile.inc"

BINDIR=		/bin

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?=	-static
.endif

DPADD=	${LIBARCHIVE_FE} ${LIBARCHIVE} ${LIBEXPAT} ${LIBBZ2} ${LIBLZMA} ${LIBZ} ${LIBCRYPTO}
LDADD=	-L${LIBARCHIVE_FE_DIR} -larchive_fe -larchive -lexpat -lbz2 -llzma -lz -lcrypto
CPPFLAGS+=	-I${LIBARCHIVEDIR}/libarchive_fe
