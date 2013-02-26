# $NetBSD: Makefile,v 1.1 2013/02/24 03:12:59 jmmv Exp $

.include <bsd.init.mk>

SRCDIR=		${NETBSDSRCDIR}/external/bsd/kyua-testers/dist
.PATH:		${SRCDIR}

.if ${MKSHARE} != "no"
.PATH:		${SRCDIR}

FILESDIR=	/usr/share/doc/kyua-testers
FILESMODE=	444
FILES=		AUTHORS COPYING NEWS README
.endif

.include <bsd.files.mk>
