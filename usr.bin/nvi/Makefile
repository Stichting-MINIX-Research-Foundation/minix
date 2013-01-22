#	$NetBSD: Makefile,v 1.3 2008/10/13 18:11:54 dyoung Exp $

.include "Makefile.inc"

.include <bsd.own.mk>

.if defined(__MINIX)
SUBDIR+= build
.else
.if ${MKSHARE} != "no"
SUBDIR+= catalog
.endif

SUBDIR+= build recover

.if ${MKDOC} != "no"
SUBDIR+= docs
.endif
.endif # defined(__MINIX)

.include <bsd.subdir.mk>
