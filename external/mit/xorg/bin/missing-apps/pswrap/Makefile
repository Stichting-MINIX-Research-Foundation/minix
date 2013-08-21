#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=	pswrap
SRCS=	main.c pswparser.y lexer.l pswdict.c pswfile.c \
	systemnames.c psw.c pswstring.c pswsemantics.c

YHEADER=1

CPPFLAGS+=	-DXENVIRONMENT \
		-DFRIENDSFILE='"<DPS/dpsfriends.h>"' \
		-I. \
		-I${X11SRCDIR.xc}/config/pswrap

pswpriv.h: pswparser.h

.PATH:	${X11SRCDIR.xc}/config/pswrap

.include <bsd.x11.mk>
.include <bsd.prog.mk>
