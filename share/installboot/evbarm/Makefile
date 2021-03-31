#	$NetBSD: Makefile,v 1.1 2019/05/07 05:02:42 thorpej Exp $

NOOBJ=		# defined

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
FILES=		boards.plist
FILESDIR=	/usr/share/installboot/evbarm
.endif

.include <bsd.prog.mk>
