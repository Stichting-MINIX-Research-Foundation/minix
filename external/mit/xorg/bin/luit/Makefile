#	$NetBSD: Makefile,v 1.4 2011/10/10 09:41:47 njoly Exp $

.include <bsd.own.mk>

PROG=	luit
SRCS=	luit.c iso2022.c charset.c parser.c sys.c other.c

CPPFLAGS+=-DLOCALE_ALIAS_FILE=\"${X11LIBDIR}/locale/locale.alias\"
CPPFLAGS+=-DHAVE_CONFIG_H -I${X11SRCDIR.${PROG}}/../include

X11EXTRAMANDEFS+= -e 's,__locale_alias__,${X11LIBDIR}/locale/locale.alias,g'

LDADD+=	-lfontenc -lz
DPADD+=	${LIBFONTENC} ${LIBZ}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
