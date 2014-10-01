# $NetBSD: Makefile,v 1.1 2011/03/10 09:18:00 jmmv Exp $

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
SRCDIR=		${NETBSDSRCDIR}/external/bsd/tmux/dist
.PATH:		${SRCDIR}/examples

FILESDIR=	/usr/share/examples/tmux
FILESMODE=	444
FILES=		screen-keys.conf
.endif

.include <bsd.prog.mk>
