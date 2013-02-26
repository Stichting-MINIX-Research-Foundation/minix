# $NetBSD: Makefile,v 1.1 2013/02/16 21:29:48 jmmv Exp $

.include <bsd.own.mk>

.if ${MKSHARE} != "no" || \
	make(clean) || make(cleandir) || make(distclean) || make(obj)
SUBDIR= examples
.endif

.include <bsd.subdir.mk>
