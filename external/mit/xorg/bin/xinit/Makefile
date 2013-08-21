#	$NetBSD: Makefile,v 1.6 2011/08/03 02:56:29 mrg Exp $

.include <bsd.own.mk>

PROG=	xinit
MAN=	xinit.1 startx.1

XINITDIR=	${X11ETCDIR}/xinit

CPPFLAGS+=	${X11FLAGS.CONNECTION} -DBINDIR=\"${X11BINDIR}\" \
		-DRETSIGTYPE=void

CPPSCRIPTS=	startx xinitrc
CPPSCRIPTFLAGS_xinitrc=	-DXINITDIR=${XINITDIR} -DBINDIR=${X11BINDIR} \
		-DSHELL_CMD=/bin/sh -DXRDB=xrdb -DXMODMAP=xmodmap \
		-DTWM=twm -DXCLOCK=xclock -DXTERM=${X11BINDIR}/xterm
CPPSCRIPTFLAGS_startx=	-DXINITDIR=${XINITDIR} -DBINDIR=${X11BINDIR} \
		-DSHELL_CMD=/bin/sh -DXAUTH=xauth -DXINIT=xinit \
		-DXSERVER=${X11BINDIR}/X -DXTERM=${X11BINDIR}/xterm \
		-DHAS_COOKIE_MAKER \
		-DMK_COOKIE='dd if=/dev/urandom bs=16 count=1 2>/dev/null | hexdump -e \\"%08x\\"'

SCRIPTS=	startx
CONFIGFILES=	xinitrc
FILESDIR=	${XINITDIR}

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11}

X11EXTRAMANDEFS=-D__xinitdir__=${XINITDIR}
X11MANCPP=	yes

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
