#	$NetBSD: Makefile,v 1.1 2009/08/18 20:22:19 skrll Exp $

PROG=		objdump

.if defined(__MINIX)
CPPFLAGS.objdump.c+=	-DOBJDUMP_PRIVATE_VECTORS=""
.endif # defined(__MINIX)

.include <bsd.own.mk>

.include "${.CURDIR}/../common/Makefile.prog"
