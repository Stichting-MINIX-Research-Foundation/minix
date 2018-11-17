# $NetBSD: Makefile.inc,v 1.5 2017/04/21 12:43:22 joerg Exp $

.include <bsd.own.mk>

.include "${.PARSEDIR}/../Makefile.inc"

LIBINSTALL != cd ${.PARSEDIR}/../lib && ${PRINTOBJDIR}

BINDIR?=	/usr/sbin
CPPFLAGS+=	-DBINDIR='"${BINDIR}"'

DPADD+=	${LIBINSTALL}/libinstall.a
LDADD+=	-L${LIBINSTALL} -linstall -ltermcap

DPADD+=		${LIBNETPGPVERIFY} ${LIBFETCH} ${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lnetpgpverify -lfetch -lssl -lcrypto

DPADD+=		${LIBARCHIVE}
LDADD+=		-larchive

DPADD+=		${LIBZ} ${LIBBZ2}
LDADD+=		-lz -lbz2

.if !defined(__MINIX)
DPADD+=		${LIBLZMA} ${LIBPTHREAD}
LDADD+=		-llzma -lpthread
.else
DPADD+=		${LIBLZMA}
LDADD+=		-llzma
.endif # !defined(__MINIX)
