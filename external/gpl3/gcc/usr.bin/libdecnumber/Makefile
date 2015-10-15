#	$NetBSD: Makefile,v 1.6 2014/06/01 19:51:02 mrg Exp $

DIST=		${GCCDIST}
GNUHOSTDIST=	${DIST}

LIBISPRIVATE=   yes

.include <bsd.own.mk>

LIB=		decnumber

SRCS=		decNumber.c decContext.c decimal32.c decimal64.c decimal128.c
CPPFLAGS+=	-I${.CURDIR}/arch/${GCC_MACHINE_ARCH} -I${DIST}/libdecnumber
CPPFLAGS+=	-I${DIST}/libgcc

.if ${MACHINE_ARCH} == "vax"
COPTS.decNumber.c=-O0
.endif

HOSTPROG_CXX=	1

.include <bsd.lib.mk>

# Force using C++ for this
HOST_CC:=	${HOST_CXX}
CC:=		${CXX}
CFLAGS:=	${CXXFLAGS}

.PATH: ${DIST}/libdecnumber ${DIST}/libdecnumber/dpd
