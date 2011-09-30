#	$NetBSD: Makefile,v 1.15 2007/05/28 12:06:25 tls Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/2/94

.include <bsd.own.mk>

USE_FORT?= yes	# setuid
PROG=	chpass
SRCS=	chpass.c edit.c field.c table.c util.c
BINOWN=	root
BINMODE=4555
.ifdef __MINIX
.PATH:  ${NETBSDSRCDIR}/lib/nbsd_libc/gen
.else
.PATH:	${NETBSDSRCDIR}/lib/libc/gen
.endif
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1

.ifdef __MINIX
CPPFLAGS+=-I${NETBSDSRCDIR}/lib/nbsd_libc/include
.else
CPPFLAGS+=-I${NETBSDSRCDIR}/lib/libc/include
.endif

.if defined(__MINIX)
USE_YP=	no
.endif

.if (${USE_YP} != "no")
SRCS+=	pw_yp.c
CPPFLAGS+=-DYP
DPADD+=	${LIBRPCSVC}
LDADD+=	-lrpcsvc
.else
SRCS+=	getpwent.c
CPPFLAGS.getpwent.c=-UYP
.endif

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
