#	$NetBSD: Makefile,v 1.3 2011/11/20 21:02:23 dholland Exp $

.include <bsd.own.mk>

PROG=	x11perf
SRCS=	x11perf.c bitmaps.c do_tests.c do_simple.c do_rects.c do_valgc.c \
	do_lines.c do_segs.c do_dots.c do_windows.c do_movewin.c do_text.c \
	do_blt.c do_arcs.c do_tris.c do_complex.c do_traps.c

CPPFLAGS+=	-DMITSHM -DXRENDER
CPPFLAGS+=	-DXFT -DXFREE86_FT2 -I${DESTDIR}${X11INCDIR}/freetype2

MAN=	Xmark.1 x11perf.1 x11perfcomp.1

x11perfcomp: x11perfcomp.in
	${TOOL_SED} \
		-e "s,@x11perfcompdir@,${X11LIBDIR}/x11perfcomp,;" \
		-e "s,@MKTEMP@,/usr/bin/mktemp,;" \
		< ${.ALLSRC} > ${.TARGET}

SCRIPTS=		x11perfcomp Xmark \
			fillblnk perfboth perfratio
SCRIPTSDIR_fillblnk.sh=	${X11LIBDIR}/x11perfcomp
SCRIPTSDIR_perfboth.sh=	${X11LIBDIR}/x11perfcomp
SCRIPTSDIR_perfratio.sh=${X11LIBDIR}/x11perfcomp

CLEANFILES+=	x11perfcomp

LDADD+=	-lXft -lfontconfig -lfreetype -lexpat -lXrender
DPADD+=	${LIBXFT} ${LIBFONTCONFIG} ${LIBFREETYPE} ${LIBEXPAT} ${LIBXRENDER}
LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11 -lm
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
