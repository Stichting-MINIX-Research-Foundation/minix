#	$NetBSD: Makefile,v 1.1 2011/06/21 06:02:25 mrg Exp $

SUBDIR+=	libgcc

.include <bsd.own.mk>

.if ${MKPIC} != "no"
SUBDIR+=	libgcc_eh libgcc_s
.endif

.if !defined(__MINIX)
SUBDIR+=	libgcov
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
