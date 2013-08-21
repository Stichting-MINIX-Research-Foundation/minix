#	$NetBSD: Makefile,v 1.11 2013/05/31 21:40:29 mrg Exp $

.include <bsd.own.mk>

PROG=	xterm
SRCS=	button.c charproc.c charsets.c cursor.c data.c doublechr.c \
	fontutils.c input.c linedata.c menu.c misc.c print.c ptydata.c \
	screen.c scrollback.c scrollbar.c tabs.c util.c xstrings.c \
	TekPrsTbl.c Tekproc.c VTPrsTbl.c main.c charclass.c precompose.c \
	wcwidth.c xutf8.c cachedGCs.c xtermcap.c version.c

CPPFLAGS+=	-I${X11SRCDIR.${PROG}} \
		-I${X11SRCDIR.${PROG}}/../include \
		-I${DESTDIR}${X11INCDIR}/freetype2 \
		-DPROJECTROOT=${X11ROOTDIR} \
		-DHAVE_CONFIG_H \
		-D_REENTRANT \
		${X11FLAGS.VERSION}

CWARNFLAGS.clang+=	-Wno-empty-body

APPDEFS=XTerm.ad XTerm-color.ad

BUILDSYMLINKS=	XTerm-col.ad XTerm-color.ad

#CPPFLAGS+=-DOPT_TRACE
#SRCS+=trace.c

LDADD+=	-lXft -lfontconfig -lfreetype -lexpat -lXrender
DPADD+=	${LIBXFT} ${LIBFONTCONFIG} ${LIBFREETYPE} ${LIBEXPAT} ${LIBXRENDER}
LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11 -lcurses -lterminfo -lutil
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11} ${LIBCURSES} ${LIBTERMINFO} ${LIBUTIL}

SUBDIR=	resize uxterm

.PATH:	${X11SRCDIR.${PROG}}

COPTS.input.c+=	-Wno-error	# uses XKeycodeToKeysym

.include "Makefile.inc"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
