#	$NetBSD: Makefile,v 1.19 2012/02/14 19:39:37 joerg Exp $

.include <bsd.own.mk>

.if ${MKGROFF} != "no"
SUBDIR=	notes .WAIT
.endif

.if !defined(__MINIX)
SUBDIR+= utils
.endif

.if exists(${MACHINE}/miniroot/Makefile.inc)
SUBDIR+= miniroot .WAIT
.endif

.if exists(${RELEASEMACHINE}/Makefile)
SUBDIR+= ${RELEASEMACHINE}
.endif

TARGETS+=release
TARGETS+=iso_image
TARGETS+=live_image
TARGETS+=install_image

.include <bsd.subdir.mk>
