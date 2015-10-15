#	$NetBSD: Makefile,v 1.15 2015/07/23 09:37:59 mrg Exp $

.include <bsd.own.mk>

PROG=	xterm
SRCS=	button.c charproc.c charsets.c cursor.c data.c doublechr.c \
	fontutils.c input.c linedata.c menu.c misc.c print.c ptydata.c \
	screen.c scrollback.c scrollbar.c tabs.c util.c xstrings.c \
	TekPrsTbl.c Tekproc.c VTPrsTbl.c main.c charclass.c precompose.c \
	wcwidth.c xutf8.c cachedGCs.c xtermcap.c version.c

# graphics_regis.c graphics_sixel.c
# xterm.appdata.xml

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
LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXinerama
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXINERAMA}
LDADD+=	-lXpm -lXext -lX11 -lcurses -lterminfo -lutil
DPADD+=	${LIBXPM} ${LIBXEXT} ${LIBX11} ${LIBCURSES} ${LIBTERMINFO} ${LIBUTIL}

.if defined(__MINIX)
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
.endif # defined(__MINIX)

SUBDIR=	resize uxterm

.PATH:	${X11SRCDIR.${PROG}}

COPTS.input.c+=	-Wno-error	# uses XKeycodeToKeysym

# XXXMRG should probably build builtin_icons.h

.include "Makefile.inc"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
