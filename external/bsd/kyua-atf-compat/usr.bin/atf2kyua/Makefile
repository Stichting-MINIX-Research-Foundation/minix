# $NetBSD: Makefile,v 1.1 2013/02/25 00:20:10 jmmv Exp $

.include <bsd.init.mk>

FILES=		atf2kyua
FILESDIR=	/usr/bin
FILESMODE=	${BINMODE}

realall: atf2kyua
atf2kyua: atf2kyua.sh
	src=atf2kyua.sh dst=atf2kyua; ${BUILD_SCRIPT}
CLEANFILES+=	atf2kyua atf2kyua.new

MAN=		atf2kyua.1

.include <bsd.prog.mk>
.include <bsd.files.mk>
