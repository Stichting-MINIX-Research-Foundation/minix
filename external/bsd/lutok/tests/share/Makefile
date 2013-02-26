# $NetBSD: Makefile,v 1.1 2013/02/16 21:29:50 jmmv Exp $

.include <bsd.own.mk>

.if ${MKSHARE} != "no" || \
	make(clean) || make(cleandir) || make(distclean) || make(obj)
SUBDIR= lutok
.endif

.include <bsd.subdir.mk>
