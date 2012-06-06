#	$NetBSD: Makefile,v 1.6 2011/10/31 08:14:44 mrg Exp $

.include <bsd.own.mk>

.if ${MKBINUTILS} != "no"
SUBDIR+=	binutils
.endif

.if ${HAVE_GCC} == 45
.if ${MKGCCCMDS} != "no"
SUBDIR+=	gcc
.endif
.endif

.if ${MKGDB} != "no"
.if ${HAVE_GDB} == "7"
SUBDIR+=        gdb
.endif
.endif

.include <bsd.subdir.mk>
