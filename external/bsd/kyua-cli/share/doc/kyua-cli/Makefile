# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:52 jmmv Exp $

.include <bsd.init.mk>

.if ${MKSHARE} != "no"
.PATH:		${SRCDIR}

FILESDIR=	${KYUA_DOCDIR}
FILESMODE=	444
FILES=		AUTHORS COPYING NEWS README
.endif

.include <bsd.files.mk>
