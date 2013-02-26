# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:53 jmmv Exp $

.include <bsd.init.mk>

.if ${MKSHARE} != "no"
.PATH:		${SRCDIR}/examples

FILESDIR=	${KYUA_EXAMPLESDIR}
FILESMODE=	444
FILES=		Kyuafile.top kyua.conf
.endif

.include <bsd.files.mk>
