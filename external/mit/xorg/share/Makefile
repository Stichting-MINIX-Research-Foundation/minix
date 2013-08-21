#	$NetBSD: Makefile,v 1.2 2010/11/18 18:47:35 tron Exp $

.include <bsd.own.mk>

SUBDIR=	cf
.if ${MKSHARE} != "no" || \
	make(clean) || make(cleandir) || make(distclean) || make(obj)
SUBDIR+=fonts man nls
.endif

.include <bsd.subdir.mk>
