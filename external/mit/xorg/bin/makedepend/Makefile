#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=		makedepend
SRCS=		include.c main.c parse.c pr.c cppsetup.c ifparser.c

CPPFLAGS+=	-I${X11SRCDIR.xc}/config/imake \
		-I${DESTDIR}${X11INCDIR}/X11

CPPFLAGS.main.c=-DINCLUDEDIR=\"/usr/include\" -DOBJSUFFIX=\".o\"

.PATH:	${X11SRCDIR.${PROG}}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
