#	$NetBSD: Makefile,v 1.8 2013/10/31 17:28:10 christos Exp $

.include <bsd.own.mk>

.if ${MKBINUTILS} != "no"
SUBDIR+=	binutils
.endif

.if ${MKGCC} != "no"
.if ${HAVE_GCC} >= 45
.if ${MKGCCCMDS} != "no"
SUBDIR+=	gcc
.endif
.endif
.endif

.if ${MKGDB} != "no"
.if ${HAVE_GDB} == "7"
SUBDIR+=        gdb
.endif
.endif

.include <bsd.subdir.mk>
