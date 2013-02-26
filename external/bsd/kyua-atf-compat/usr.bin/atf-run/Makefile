# $NetBSD: Makefile,v 1.1 2013/02/25 00:20:10 jmmv Exp $

.include <bsd.init.mk>

FILES=		atf-run
FILESDIR=	/usr/bin
FILESMODE=	${BINMODE}

realall: atf-run
atf-run: atf-run.sh
	src=atf-run.sh dst=atf-run; ${BUILD_SCRIPT}
CLEANFILES+=	atf-run atf-run.new

MAN=		atf-run.1

.include <bsd.prog.mk>
.include <bsd.files.mk>
