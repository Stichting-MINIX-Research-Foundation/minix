#	$NetBSD: Makefile,v 1.1 2020/06/22 03:15:48 rin Exp $

NOOBJ=		# defined

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
FILES=		boards.plist
FILESDIR=	/usr/share/installboot/evbmips
.endif

.include <bsd.prog.mk>
