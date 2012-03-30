#	$NetBSD: Makefile.inc,v 1.19 2010/07/11 07:43:12 mrg Exp $

SRCS+=		rtld_start.S mips_reloc.c

COPTS+=		-G0

ABI64?= ${CFLAGS:M-mabi=64} ${CPPFLAGS:M-mabi=64} ${COPTS:M-mabi=64}
ABIO64?= ${CFLAGS:M-mabi=o64} ${CPPFLAGS:M-mabi=o64} ${COPTS:M-mabi=o64}
.if !empty(ABI64) || !empty(ABIO64)
CPPFLAGS+=	-DELFSIZE=64
.else
CPPFLAGS+=	-DELFSIZE=32
.endif
CPPFLAGS+=	-DRTLD_INHIBIT_COPY_RELOCS
AFLAGS+=	-Wa,--fatal-warnings

LDFLAGS+=	-Wl,-e,rtld_start
