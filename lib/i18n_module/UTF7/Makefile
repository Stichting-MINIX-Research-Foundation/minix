#	$NetBSD: Makefile,v 1.4 2011/06/20 06:52:36 mrg Exp $

SRCPRE=citrus_
.include <bsd.lib.mk>

.if ${MACHINE_ARCH} == "vax" && defined(HAVE_GCC)
COPTS.citrus_utf7.c+=	-O0
.endif
