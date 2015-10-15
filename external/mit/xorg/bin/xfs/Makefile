#	$NetBSD: Makefile,v 1.12 2015/07/17 17:57:11 mrg Exp $

.include <bsd.own.mk>

PROG=	xfs

XFSDIR= ${X11SRCDIR.${PROG}}

.PATH:	${XFSDIR}/man

.PATH:	${XFSDIR}/difs
SRCS=	atom.c charinfo.c difsutils.c dispatch.c events.c \
	extensions.c fontinfo.c fonts.c globals.c initfonts.c main.c \
	resource.c swaprep.c swapreq.c tables.c

.PATH:	${XFSDIR}/os
SRCS+=	access.c config.c connection.c daemon.c error.c io.c osglue.c \
	osinit.c utils.c waitfor.c xfstrans.c

CPPFLAGS+=		-I${XFSDIR}/include \
			-I${DESTDIR}${X11INCDIR}/X11/FS \
			-I${X11SRCDIR.libXfont}/include \
			-I${X11SRCDIR.libXfont}/bitmap \
			-I${XFSDIR}/../include \
			-DHAVE_DAEMON

CONN_DEFINES=		${X11FLAGS.CONNECTION} -DFONT_t -DTRANS_SERVER \
			-DTRANS_REOPEN -DHAS_FCHOWN -DHAS_STICKY_DIR_BIT

XFSCONFIGDIR=		${X11ETCDIR}/fs

CPPFLAGS.initfonts.c=	-DFONT_PCF -DFONT_FS -DFONT_SPEEDO
CPPFLAGS.connection.c=	${CONN_DEFINES} -DBSD44SOCKETS \
			${X11FLAGS.OS_DEFINES}
CPPFLAGS.xfstrans.c=	${CONN_DEFINES} -DBSD44SOCKETS \
			${X11FLAGS.OS_DEFINES}
CPPFLAGS.utils.c=	-DXFSPIDDIR=\"/var/run\"
CPPFLAGS.io.c=		${CONN_DEFINES}
CPPFLAGS.waitfor.c=	${CONN_DEFINES}
CPPFLAGS.error.c=	-DUSE_SYSLOG
CPPFLAGS.osglue.c=	${CONN_DEFINES} \
			${X11FLAGS.OS_DEFINES}
CPPFLAGS.config.c=	${CONN_DEFINES}
# XXX
CPPFLAGS.xfstrans.c+=	-Wno-error

X11EXTRAMANDEFS+=	-e 's%__configfilepath__%${XFSCONFIGDIR}/config%g' \
			-e 's%__configfiledesc__%the default file,%g'


.PATH:	${XFSDIR}/config
BUILDSYMLINKS=	config.cpp fsconfig.cpp
CPPSCRIPTS=	fsconfig
CPPSCRIPTFLAGS=	-DDEFAULTFONTPATH=${X11FONTDIR}/misc/,${X11FONTDIR}/Type1/,${X11FONTDIR}/75dpi/,${X11FONTDIR}/100dpi/ \
		-DFSERRORS=/var/log/xfs.log -D__appmansuffix__=1

CONFIGFILES=	fsconfig
FILESNAME=	config
FILESDIR=	${XFSCONFIGDIR}


LDADD+=	-lXfont -lfontenc -lXfontcache -lXext -lX11 -lfreetype -lm -lz
DPADD+=	${LIBXFONT} ${LIBFONTENC} ${LIBXFONTCACHE} ${LIBXEXT} ${LIBX11} \
	${LIBFREETYPE} ${LIBM} ${LIBZ}

.PATH:	${XFSDIR}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
