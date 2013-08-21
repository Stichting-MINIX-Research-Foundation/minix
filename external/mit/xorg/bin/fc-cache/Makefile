#	$NetBSD: Makefile,v 1.3 2013/06/03 20:23:39 mrg Exp $

.include <bsd.own.mk>

PROG=	fc-cache

CPPFLAGS+=	-I${X11SRCDIR.fontconfig} -I.
CPPFLAGS+=	-DHAVE_CONFIG_H -I${X11SRCDIR.fontconfig}/../include

LDADD+=	-lfontconfig -lfreetype -lexpat
DPADD+=	${LIBFONTCONFIG} ${LIBFREETYPE} ${LIBEXPAT}

.PATH:	${X11SRCDIR.fontconfig}/${PROG}

.include "../../lib/fontconfig/src/Makefile.fcarch"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
