# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:54 jmmv Exp $

.include <bsd.init.mk>

.if ${MKSHARE} != "no"
.PATH:		${SRCDIR}/store

FILESDIR=	${KYUA_STOREDIR}
FILESMODE=	444
FILES=		migrate_v1_v2.sql \
		schema_v2.sql
.endif

.include <bsd.files.mk>
