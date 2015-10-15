#	$NetBSD: Makefile,v 1.3 2014/01/03 20:18:45 christos Exp $

.include <bsd.own.mk>

PROG=	fc-match

CPPFLAGS+=	-I${X11SRCDIR.fontconfig} -I.

LDADD+=	-lfontconfig -lfreetype -lexpat -lbz2 -lz
DPADD+=	${LIBFONTCONFIG} ${LIBFREETYPE} ${LIBEXPAT} ${LIBBZ2} ${LIBZ}

.PATH:	${X11SRCDIR.fontconfig}/${PROG}

.include "../../lib/fontconfig/src/Makefile.fcarch"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
