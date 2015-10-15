#	$NetBSD: Makefile,v 1.5 2014/05/31 22:55:17 mrg Exp $

PROG=		gplusplus
PROGNAME=	g++
SRCS=		${G_GXX_OBJS:.o=.c}

NOMAN=		# Linked to gcc.1
.include <bsd.own.mk>

CPPFLAGS+=	-I${BACKENDOBJ}
# XXX
CPPFLAGS+=	-DCONFIGURE_SPECS="\"\"" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\"

.if defined(__MINIX) && ${HAVE_LLVM:Uno} == "no"
LINKS+=		${BINDIR}/g++ ${BINDIR}/c++
.endif # defined(__MINIX) && ${HAVE_LLVM:Uno} == "no"

.if !defined(__MINIX)
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # !defined(__MINIX)

.include "../Makefile.frontend"

${SRCS}: ${GCCARCH}/defs.mk

COPTS.gcc.c=	-Wno-stack-protector

.PATH: ${DIST}/gcc/cp ${DIST}/gcc ${DIST}/gcc/cp
