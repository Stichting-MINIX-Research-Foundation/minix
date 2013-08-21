#	$NetBSD: Makefile,v 1.3 2013/05/31 08:02:00 mrg Exp $

.include <bsd.own.mk>

PROG=	xsm
SRCS=	auth.c choose.c info.c list.c lock.c log.c \
	mainwin.c misc.c popup.c printhex.c prop.c remote.c \
	restart.c save.c saveutil.c signals.c xsm.c xtwatch.c

XSMDIR=	${X11ETCDIR}/xsm

CPPFLAGS+=-DHAVE_MKSTEMP -DHAVE_PUTENV
CPPFLAGS.remote.c=	-DRSHCMD=\"ssh\"
CPPFLAGS.restart.c=	-DSYSTEM_INIT_FILE=\"${XSMDIR}/system.xsm\"

CONFIGFILES=	system.xsm
FILESDIR=	${XSMDIR}

.PATH:	${X11SRCDIR.${PROG}}/app-defaults
APPDEFS=XSm

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
