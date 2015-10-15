#	$NetBSD: Makefile,v 1.7 2014/03/13 16:19:24 gdt Exp $

.include "../Makefile.inc"

.if exists(${.CURDIR}/arch/${GCC_MACHINE_ARCH}.mk)
.include "${.CURDIR}/arch/${GCC_MACHINE_ARCH}.mk"
.endif

INCS= ${G_EXTRA_HEADERS:T}

# XXX missed some how
.if ${GCC_MACHINE_ARCH} == "i386" || ${GCC_MACHINE_ARCH} == "x86_64"
INCS+=	mm_malloc.h

# XXX we should put this in gcc/config.gcc and regenerate the i386/x86
# mknative files
mm_malloc.h: ${DIST}/gcc/config/i386/pmm_malloc.h
	cp -f $> ${.TARGET}

CLEANFILES+=	mm_malloc.h
.endif

INCSDIR=	/usr/include/gcc-4.8

.include <bsd.prog.mk>

.PATH: ${G_EXTRA_HEADERS:H:O:u}
