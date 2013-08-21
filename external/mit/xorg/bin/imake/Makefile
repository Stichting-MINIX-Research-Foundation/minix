#	$NetBSD: Makefile,v 1.4 2013/05/31 05:41:26 mrg Exp $

.include <bsd.own.mk>

PROG=	imake

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=	-I${X11SRCDIR.${PROG}}/../include

CPP_PROGRAM=	cpp
CPPFLAGS+=	-DCPP_PROGRAM=\"$(CPP_PROGRAM)\"
X11EXTRAMANDEFS=-e 's,__cpp__,$(CPP_PROGRAM),g'

.PATH:	${X11SRCDIR.${PROG}}

COPTS.imake.c+=	-Wno-error	# const issues

.include <bsd.x11.mk>
.include <bsd.prog.mk>
