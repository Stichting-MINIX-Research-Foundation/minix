#	$NetBSD: Makefile.inc,v 1.1 2013/04/27 23:02:21 joerg Exp $

.include <bsd.own.mk>

LIBCXXRT_SRCDIR:=	${.PARSEDIR}/dist/libcxxrt
LIBCXX_SRCDIR:=	${.PARSEDIR}/dist/libcxx

.if defined(__MINIX)
# Should I try -D_LIBCPP_HAS_NO_THREADS ?
#LDADD+= -lmthread
#DPADD+= ${LIBMTHREAD}
.endif # defined(__MINIX)
