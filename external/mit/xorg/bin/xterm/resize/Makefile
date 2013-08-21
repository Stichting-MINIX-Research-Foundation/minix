#	$NetBSD: Makefile,v 1.4 2013/05/31 21:40:29 mrg Exp $

.include <bsd.own.mk>

PROG=	resize
SRCS=	resize.c xstrings.c version.c

CPPFLAGS+=-I${X11SRCDIR.xterm}
CPPFLAGS+=-DHAVE_TERMCAP_H

.PATH:	${X11SRCDIR.xterm}

LDADD+=	-lterminfo
DPADD+=	${LIBTERMINFO} 

.include <bsd.x11.mk>
.include <bsd.prog.mk>
