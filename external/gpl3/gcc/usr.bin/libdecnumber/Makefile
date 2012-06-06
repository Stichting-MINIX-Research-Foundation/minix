#	$NetBSD

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
GNUHOSTDIST=	${DIST}

LIBISPRIVATE=   yes

.include <bsd.own.mk>

LIB=		decnumber

SRCS=		decNumber.c decContext.c decimal32.c decimal64.c decimal128.c
CPPFLAGS+=	-I${.CURDIR}/arch/${MACHINE_ARCH} -I${DIST}/libdecnumber
CPPFLAGS+=	-I${DIST}/libgcc

.include <bsd.lib.mk>

.PATH: ${DIST}/libdecnumber ${DIST}/libdecnumber/dpd
