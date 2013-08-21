#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

CPPSCRIPTS=	gccmakedep
CPPSCRIPTFLAGS=	-DCCCMD='"gcc"' -DRMCMD='"rm -f"' -DLNCMD='"ln -s"' \
		-DMVCMD='"mv -f"'
SCRIPTS=	${CPPSCRIPTS}
MAN=		gccmakedep.1

BUILDSYMLINKS=	gccmdep.cpp gccmakedep.cpp

.PATH:	${X11SRCDIR.xc}/config/util

.include <bsd.x11.mk>
.include <bsd.prog.mk>
