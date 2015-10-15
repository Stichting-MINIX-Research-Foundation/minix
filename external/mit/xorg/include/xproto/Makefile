#	$NetBSD: Makefile,v 1.3 2014/08/24 16:39:21 christos Exp $

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.xproto}

Xfuncproto.h: Xfuncproto.h.in
	${TOOL_SED} -e 's/#undef NARROWPROTO/#define NARROWPROTO/'  \
	    -e 's/#undef FUNCPROTO/#define FUNCPROTO 15/' \
	    < ${.ALLSRC} > ${.TARGET}

Xpoll.h: Xpoll.h.in
	${TOOL_SED} 's/@USE_FDS_BITS@/fds_bits/' \
	    < ${.ALLSRC} > ${.TARGET}

INCS=	DECkeysym.h HPkeysym.h Sunkeysym.h X.h XF86keysym.h XWDFile.h \
	Xalloca.h Xarch.h Xatom.h Xdefs.h Xfuncproto.h Xfuncs.h Xmd.h \
	Xos.h Xos_r.h Xosdefs.h Xpoll.h Xproto.h Xprotostr.h Xthreads.h \
	Xw32defs.h Xwindows.h Xwinsock.h ap_keysym.h keysym.h keysymdef.h
INCSDIR=${X11INCDIR}/X11

CLEANFILES+=	Xpoll.h Xfuncproto.h

NOOBJ=	# defined

PKGCONFIG=	xproto

.include <bsd.x11.mk>
.include <bsd.prog.mk>
