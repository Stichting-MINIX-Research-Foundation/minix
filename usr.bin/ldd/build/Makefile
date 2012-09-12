#	$NetBSD: Makefile,v 1.1 2011/04/15 08:47:02 plunky Exp $

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

PROG=	ldd
SRCS=	ldd.c
MAN=	ldd.1

.PATH: ${.CURDIR}/..

.if (${MACHINE_ARCH} != "alpha")
LIB_ELF32DIR!=	cd ${.CURDIR}/../elf32 && ${PRINTOBJDIR}
EXTRA_LIBS+=	${LIB_ELF32DIR}/libldd_elf32.a
.endif

.if (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
LIB_ELF32COMPATDIR!=	cd ${.CURDIR}/../elf32_compat && ${PRINTOBJDIR}
EXTRA_LIBS+=		${LIB_ELF32COMPATDIR}/libldd_elf32_compat.a
.endif

.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "sparc64") || \
    (${MACHINE_ARCH} == "x86_64") || (${MACHINE_ARCH} == "powerpc64") || \
    (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
LIB_ELF64DIR!=	cd ${.CURDIR}/../elf64 && ${PRINTOBJDIR}
EXTRA_LIBS+=	${LIB_ELF64DIR}/libldd_elf64.a
CPPFLAGS.ldd.c= -DELFSIZE=64
.else
CPPFLAGS.ldd.c= -DELFSIZE=32
.endif

LDADD+=	${EXTRA_LIBS}
DPADD+=	${EXTRA_LIBS}

.include "Makefile.common"

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?= -static
.endif

.include <bsd.prog.mk>
