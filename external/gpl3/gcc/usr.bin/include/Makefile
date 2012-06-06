#	$NetBSD: Makefile,v 1.4 2011/07/07 08:46:20 mrg Exp $

.include "../Makefile.inc"

# XXX for G_EXTRA_HEADERS.  make mknative-gcc put that somewhere
# inside usr.bin/ not lib/
.if exists(${.CURDIR}/../../lib/libgcc/arch/${MACHINE_ARCH}.mk)
.include "${.CURDIR}/../../lib/libgcc/arch/${MACHINE_ARCH}.mk"
.endif

INCS= ${G_EXTRA_HEADERS:T}

# XXX missed some how
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
INCS+=	mm_malloc.h

# XXX we should put this in gcc/config.gcc and regenerate the i386/x86
# mknative files
mm_malloc.h: ${DIST}/gcc/config/i386/pmm_malloc.h
	cp $> ${.TARGET}

CLEANFILES+=	mm_malloc.h
.endif

INCSDIR=	/usr/include/gcc-4.5

.include <bsd.prog.mk>

# XXX probably don't need the first of these
.PATH: ${G_out_file:H}
.PATH: ${G_EXTRA_HEADERS:H:O:u}
