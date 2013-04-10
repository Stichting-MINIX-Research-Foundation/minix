#	$NetBSD: Makefile,v 1.6 2010/02/03 15:34:39 roy Exp $

.include <bsd.own.mk>

PROG=		info
SRCS=		dir.c display.c dribble.c echo-area.c filesys.c \
		footnotes.c gc.c indices.c info-utils.c info.c infodoc.c \
		infomap.c m-x.c man.c nodemenu.c nodes.c search.c session.c \
		signals.c terminal.c tilde.c variables.c window.c

COMMONDIR!=cd ${.CURDIR}/../common && ${PRINTOBJDIR}

CPPFLAGS+=	-I${IDIST}/info -I${COMMONDIR} \
		-DDEFAULT_INFOPATH=\"/usr/pkg/info:/usr/X11R6/info:/usr/X11/info:/usr/local/info:/usr/share/info:/usr/info\"
LDADD+=		-lterminfo
DPADD+=		${LIBTERMINFO}

.include <bsd.prog.mk>

.PATH: ${IDIST}/info ${IDIST}/doc
