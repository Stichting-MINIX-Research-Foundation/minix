#	$NetBSD: Makefile,v 1.2 2011/05/30 14:41:26 joerg Exp $

NOLINT=		# defined
NOPROFILE=	# defined
NOPIC=		# defined
NOLINKLIB=	# defined
NOCLANGERROR=	# defined

.include <bsd.own.mk>

LIB=		iberty

.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"

COPTS.argv.c = -Wno-stack-protector
COPTS.cp-demangle.c = -Wno-stack-protector
COPTS.make-relative-prefix.c = -Wno-stack-protector
COPTS.regex.c = -Wno-stack-protector

DIST=		${NETBSDSRCDIR}/external/gpl3/binutils/dist

SRCS=		${G_REQUIRED_OFILES:.o=.c} ${G_EXTRA_OFILES:.o=.c} \
		${G_LIBOBJS:.o=.c} ${G_ALLOCA:.o=.c}

CPPFLAGS+=	-DHAVE_CONFIG_H -I${.CURDIR}/arch/${MACHINE_ARCH} \
		-I${DIST}/include

.PATH: ${DIST}/libiberty

.include <bsd.lib.mk>
