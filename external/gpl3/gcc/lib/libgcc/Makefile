#	$NetBSD: Makefile,v 1.2 2013/08/22 00:16:54 matt Exp $

.include <bsd.own.mk>

SUBDIR+=	libgcc

.if defined(__MINIX) && ${MKGCCCMDS} == "yes"

.if ${MKPIC} != "no" || ${MACHINE_ARCH} == "earm"
SUBDIR+=	libgcc_eh
.endif

SUBDIR+=	libgcc_s

.else
.if ${MKPIC} != "no"
SUBDIR+=	libgcc_eh libgcc_s
.endif

SUBDIR+=	libgcov
.endif # defined(__MINIX) && ${MKGCCCMDS} == "yes"

.include <bsd.subdir.mk>
