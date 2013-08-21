#	$NetBSD: Makefile,v 1.3 2011/08/11 23:15:38 joerg Exp $

NOLINT=		1
NOPROFILE=	1
NOSTATICLIB=	1

.include <bsd.own.mk>

.include "../Makefile.libx11"

SRCS+=		XlcDL.c XlcSL.c XDefaultIMIF.c XDefaultOMIF.c

CPPFLAGS.lcInit.c=	$(LC_DYNDEFINES)
CPPFLAGS.lcGeneric.c=	$(LC_DYNDEFINES)
CPPFLAGS.lcPublic.c=	$(LC_DYNDEFINES)
CPPFLAGS.CrCursor.c=	$(XCURSOR_DEFINES)
CPPFLAGS.CrGlCur.c=	$(XCURSOR_DEFINES)
CPPFLAGS.CrPixmap.c=	$(XCURSOR_DEFINES)
CPPFLAGS.PutImage.c=	$(XCURSOR_DEFINES)

LIBDPLIBS+= \
	xcb ${.CURDIR}/../../libxcb/libxcb \
	Xdmcp ${.CURDIR}/../../libXdmcp \
	Xau ${.CURDIR}/../../libXau

CWARNFLAGS.clang+=	-Wno-parentheses

.include <bsd.x11.mk>
.include <bsd.lib.mk>
