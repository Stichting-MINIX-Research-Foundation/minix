#	$NetBSD: Makefile,v 1.4 2015/06/26 02:43:26 matt Exp $

NOMAN=	1

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.X11}/src/util

PROG?=		makekeys
SRCS=		makekeys.c
.if !make(obj) && !exists(${DESTDIR}${X11INCDIR}/X.h)
CLEANFILES+=	X11
CPPFLAGS+=	-I.
XPINCS=		X.h Xarch.h Xos.h Xosdefs.h keysymdef.h
X11INCS=	Xlib.h Xresource.h
CLEANFILES+=	${XPINCS} ${X11INCS} Xfuncproto.h
.if !make(clean) && !make(cleandir)
.BEGIN:
	@rm -f ${.OBJDIR}/X11 && ln -s . X11
.for i in ${XPINCS}
	@rm -f $i && ln -s ${X11SRCDIR.xproto}/$i .
.endfor
.for i in ${X11INCS}
	@rm -f $i && ln -s ${X11SRCDIR.X11}/include/X11/$i .
.endfor
.endif

Xfuncproto.h: ${X11SRCDIR.xproto}/Xfuncproto.h.in
	${TOOL_SED} -e 's/#undef NARROWPROTO/#define NARROWPROTO/'  \
	    -e 's/#undef FUNCPROTO/#define FUNCPROTO 15/' \
	    < ${.ALLSRC} > ${.TARGET}

makekeys.lo makekeys.o makekeys.d: Xfuncproto.h

.else
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}
.endif

.include <bsd.x11.mk>
.include <bsd.prog.mk>
