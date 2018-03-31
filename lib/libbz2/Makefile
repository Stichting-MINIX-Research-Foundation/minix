#	$NetBSD: Makefile,v 1.18 2014/07/05 19:22:42 dholland Exp $

USE_FORT?= yes	# data driven bugs?

NOMAN=		# defined

.include <bsd.own.mk>

LIB=		bz2
DIST=		${NETBSDSRCDIR}/external/bsd/bzip2/dist
.PATH:		${DIST}

SRCS=		blocksort.c huffman.c crctable.c randtable.c compress.c \
		decompress.c bzlib.c
INCS=		bzlib.h
INCSDIR=	/usr/include

.if defined(__MINIX)
.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif
.endif # defined(__MINIX)

# XXX huffman.c gets mis-compiled with 2.95.3
.if ${MACHINE_ARCH} == "vax"
COPTS+=		-O0
.endif

# XXX blocksort.c gets mis-compiled with 4.1
.if (${MACHINE_ARCH} == "sh3el" || ${MACHINE_ARCH} == "sh3eb") && \
    defined(HAVE_GCC)
COPTS.blocksort.c+=	-fno-loop-optimize
.endif

.if ${MKSHARE} != "no"
FILESDIR=	/usr/share/doc/reference/ref1/bzip2
FILES=		manual.html
.endif

.include <bsd.info.mk>
.include <bsd.lib.mk>
