# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:53 jmmv Exp $

.include <bsd.init.mk>

.if ${MKSHARE} != "no"
.PATH:		${SRCDIR}/misc

FILESDIR=	${KYUA_MISCDIR}
FILESMODE=	444
FILES=		context.html \
		index.html \
		report.css \
		test_result.html
.endif

.include <bsd.files.mk>
