#	$NetBSD: Makefile,v 1.7 2009/12/13 08:50:56 mrg Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

CPPFLAGS+=	-DELFSIZE=32
LIB=	ldd_elf32

# XXX Force one member
SRCS=	dummy.c

LIBISPRIVATE=	yes
.PATH: ${.CURDIR}/..

.ifdef MLIBDIR
CPPFLAGS+=	-DRTLD_ARCH_SUBDIR=\"${MLIBDIR}\"
.endif

.if ${MACHINE_ARCH} != "alpha"
.include "../Makefile.elf"
.endif

.include <bsd.lib.mk>
