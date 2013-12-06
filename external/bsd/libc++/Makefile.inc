#	$NetBSD: Makefile.inc,v 1.1 2013/04/27 23:02:21 joerg Exp $

.include <bsd.own.mk>

LIBCXXRT_SRCDIR:=	${.PARSEDIR}/dist/libcxxrt
LIBCXX_SRCDIR:=	${.PARSEDIR}/dist/libcxx

.if defined(__MINIX)
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
.endif # defined(__MINIX)
