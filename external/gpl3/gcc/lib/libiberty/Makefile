#	$NetBSD: Makefile,v 1.3 2011/08/09 12:58:55 joerg Exp $

LIBISPRIVATE=	yes

.include <bsd.own.mk>

LIB=		iberty

.include "${.CURDIR}/defs.mk"

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
GNUHOSTDIST=	${DIST}

SRCS=		${G_REQUIRED_OFILES:.o=.c} ${G_EXTRA_OFILES:.o=.c} \
		${G_LIBOBJS:.o=.c} ${G_ALLOCA:.o=.c}

CPPFLAGS+=	-DHAVE_CONFIG_H -I${.CURDIR}/arch/${MACHINE_ARCH} \
		-I${DIST}/include

COPTS.argv.c = -Wno-stack-protector
COPTS.cp-demangle.c = -Wno-stack-protector
COPTS.make-relative-prefix.c = -Wno-stack-protector
COPTS.regex.c = -Wno-stack-protector

CWARNFLAGS.clang+=	-Wno-unused-value

.PATH: ${DIST}/libiberty

.include <bsd.lib.mk>
