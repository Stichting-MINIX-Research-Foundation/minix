#	$NetBSD: Makefile,v 1.3 2011/07/19 15:59:02 tron Exp $

.include <bsd.own.mk>

PROG=           gcov
SRCS=		${G_GCOV_OBJS:.o=.c}

CPPFLAGS+=	-I${GCCARCH} -I${BACKENDOBJ} ${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*}

BINDIR=		/usr/bin

MAN=		gcov.1

.include <bsd.prog.mk>

.if defined(__MINIX)
LDADD=		-L${LIBIBERTYOBJ} -liberty
.else
LDADD=		-L${LIBIBERTYOBJ} -liberty -lintl
.endif # defined(__MINIX)

COPTS+=		-Wno-stack-protector

.PATH: ${DIST}/gcc
