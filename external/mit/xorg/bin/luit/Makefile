#	$NetBSD: Makefile,v 1.5 2015/07/22 07:58:00 mrg Exp $

.include <bsd.own.mk>

PROG=	luit
SRCS=	luit.c iso2022.c charset.c parser.c sys.c other.c

CPPFLAGS+=-DLOCALE_ALIAS_FILE=\"${X11LIBDIR}/locale/locale.alias\"
#CPPFLAGS+=-DHAVE_CONFIG_H -I${X11SRCDIR.${PROG}}/../include
CPPFLAGS+=-DHAVE_PUTENV -DHAVE_WORKING_POLL -DHAVE_POLL_H
CPPFLAGS+=-DHAVE_SYS_PARAM -DHAVE_OPENPTY -DHAVE_GRANTPT 
CPPFLAGS+=-DHAVE_POSIX_OPENPT -DHAVE_STRDUP

X11EXTRAMANDEFS+= -e 's,__locale_alias__,${X11LIBDIR}/locale/locale.alias,g'

LDADD+=	-lfontenc -lz
DPADD+=	${LIBFONTENC} ${LIBZ}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
