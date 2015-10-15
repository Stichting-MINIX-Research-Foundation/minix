#	$NetBSD: Makefile,v 1.6 2015/04/17 12:41:46 martin Exp $

PROG=		gcc
SRCS=		gccspec.c ${G_GCC_OBJS:.o=.c}

CPPFLAGS+=	-I${BACKENDOBJ}
# XXX
CPPFLAGS+=	-DCONFIGURE_SPECS="\"\"" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\"

COPTS.builtins.c = -Wno-stack-protector

.if defined(__MINIX) && ${HAVE_LLVM:Uno} == "no"
LINKS+=		${BINDIR}/gcc ${BINDIR}/cc 
.endif # defined(__MINIX) && ${HAVE_LLVM:Uno} == "no"

MAN=		gcc.1
MLINKS=		gcc.1 cc.1 gcc.1 g++.1 gcc.1 c++.1

TEXINFO=	gcc.texi
INFOFLAGS=	-I${DIST}/gcc/doc -I${DIST}/gcc/doc/include

.if !defined(__MINIX)
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # !defined(__MINIX)

.include "../Makefile.frontend"

${SRCS}: ${GCCARCH}/defs.mk

.include <bsd.info.mk>

COPTS.gcc.c=	-Wno-stack-protector
.if ${MACHINE_ARCH} == "vax"
COPTS.dse.c=	-O1
.endif

.PATH: ${DIST}/gcc ${DIST}/gcc/doc ${DIST}/gcc/c

gcc.info: gcc-vers.texi
