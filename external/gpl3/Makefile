#	$NetBSD: Makefile,v 1.15 2015/09/17 09:34:20 skrll Exp $

.include <bsd.own.mk>

.if ${MKBINUTILS} != "no"
SUBDIR+=	binutils
.endif

.if ${MKGCC} != "no"
SUBDIR+=	${EXTERNAL_GCC_SUBDIR}
.endif

.if ${MKGDB} != "no"
SUBDIR+=        gdb
.endif

.include <bsd.subdir.mk>
