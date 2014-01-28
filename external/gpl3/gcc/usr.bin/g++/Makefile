#	$NetBSD: Makefile,v 1.2 2011/06/29 02:00:09 mrg Exp $

PROG=		gplusplus
PROGNAME=	g++
SRCS=		g++spec.c

NOMAN=		# defined

.include <bsd.own.mk>

CPPFLAGS+=	-I${BACKENDOBJ}

.if defined(__MINIX)
.if ${HAVE_LLVM:Uno} == "no"
SYMLINKS+=	g++ ${BINDIR}/c++
.endif # ${HAVE_LLVM:Uno} == "no"
.else
LINKS+=		${BINDIR}/g++ ${BINDIR}/c++
.endif # defined(__MINIX)

.include "../Makefile.frontend"

.PATH: ${DIST}/gcc ${DIST}/gcc/cp

.if !defined(__MINIX)
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # !defined(__MINIX)
