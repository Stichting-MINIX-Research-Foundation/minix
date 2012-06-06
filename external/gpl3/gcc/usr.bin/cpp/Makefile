#	$NetBSD: Makefile,v 1.4 2011/07/19 15:59:02 tron Exp $

# For ../Makefile.inc and bsd.own.mk
.include <bsd.init.mk>

PROG=		cpp
SRCS=		gcc.c cppspec.c prefix.c version.c

# XXX
.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "i386"
SRCS+=		driver-i386.c
.PATH:		${DIST}/gcc/config/i386
.endif
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
SRCS+=		driver-rs6000.c
.PATH:		${DIST}/gcc/config/rs6000
.endif

CPPFLAGS+=	-I${BACKENDOBJ}
CPPFLAGS.prefix.c+=	-DPREFIX=\"/usr\"

CPPFLAGS.gcc.c+=	-I${GCCARCH} -I${BACKENDOBJ} -I. \
		${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*} \
		-DPREFIX=\"/usr\" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\" \
		-DCONFIGURE_SPECS="\"\""

LINKS=		${BINDIR}/cpp ${BINDIR}/gcpp

TEXINFO=	cpp.texi
INFOFLAGS=	-I${DIST}/gcc/doc -I${DIST}/gcc/doc/include -I.

MAN=		cpp.1
MLINKS+=	cpp.1 gcpp.1 cpp.1 cccp.1

.include "../Makefile.frontend"

LDADD+=		-lintl

COPTS+=		-Wno-stack-protector

.include <bsd.info.mk>

.PATH: ${DIST}/gcc ${DIST}/gcc/doc

cpp.info: gcc-vers.texi
