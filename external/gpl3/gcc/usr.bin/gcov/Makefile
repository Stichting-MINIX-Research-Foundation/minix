#	$NetBSD: Makefile,v 1.5 2014/03/05 21:37:02 tron Exp $

.include <bsd.own.mk>

PROG=           gcov
SRCS=		${G_GCOV_OBJS:.o=.c}

CPPFLAGS+=	-I${GCCARCH} -I${BACKENDOBJ} ${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*}

BINDIR=		/usr/bin

MAN=		gcov.1

HOSTPROG_CXX=	1

.include <bsd.prog.mk>

# Force using C++ for this
HOST_CC:=	${HOST_CXX}
CC:=		${CXX}
CFLAGS:=	${CXXFLAGS} -Wno-stack-protector

.include "../Makefile.common"
.include "../Makefile.libcpp"
.include "../Makefile.backtrace"
.include "../Makefile.libiberty"
.include "../Makefile.libdecnumber"

.if !defined(__MINIX)
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # !defined(__MINIX)

.PATH: ${DIST}/gcc
