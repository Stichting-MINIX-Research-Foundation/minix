#	$NetBSD: Makefile,v 1.8 2014/05/31 22:55:17 mrg Exp $

# For ../Makefile.inc and bsd.own.mk
.include <bsd.init.mk>

.if defined(__MINIX)
# In case of installation of both clang and GCC, we want to be able to
# switch which one is the default by adapting the cc, c++ & cpp symlinks
PROG=		gcpp
.else
PROG=		cpp
.endif # defined(__MINIX)
SRCS=		cppspec.c ${G_GCC_OBJS:.o=.c}

CPPFLAGS+=	-I${BACKENDOBJ}
# XXX
CPPFLAGS+=	-DCONFIGURE_SPECS="\"\"" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\"

COPTS+=		-Wno-stack-protector

.if defined(__MINIX)
.if ${HAVE_LLVM:Uno} == "no"
SYMLINKS=	${BINDIR}/gcpp ${BINDIR}/cpp
.endif # ${HAVE_LLVM:Uno} == "no"
.else
LINKS=		${BINDIR}/cpp ${BINDIR}/gcpp
.endif # defined(__MINIX)

MAN=		cpp.1
MLINKS+=	cpp.1 gcpp.1 cpp.1 cccp.1

TEXINFO=	cpp.texi
INFOFLAGS=	-I${DIST}/gcc/doc -I${DIST}/gcc/doc/include -I.

.if !defined(__MINIX)
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # !defined(__MINIX)

.include "../Makefile.frontend"

${SRCS}: ${GCCARCH}/defs.mk

.include <bsd.info.mk>

COPTS.gcc.c=	-Wno-stack-protector

.PATH: ${DIST}/gcc ${DIST}/gcc/c-family ${DIST}/gcc/doc

cpp.info: gcc-vers.texi
