#	$NetBSD: Makefile,v 1.1 2011/06/21 06:02:25 mrg Exp $

SUBDIR+=	libgcc

.include <bsd.own.mk>

.if !defined(__MINIX)
.if ${MKPIC} != "no"
SUBDIR+=	libgcc_eh libgcc_s
.endif

SUBDIR+=	libgcov
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
