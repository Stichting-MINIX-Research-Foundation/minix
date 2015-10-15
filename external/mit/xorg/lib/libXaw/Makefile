#	$NetBSD: Makefile,v 1.11 2015/07/19 07:55:07 mrg Exp $

.include <bsd.own.mk>

LIB=	Xaw7
LIBOLD=	Xaw

.if ${MKSTATICLIB} != "no"
SYMLINKS=	lib${LIB}.a ${X11USRLIBDIR}/lib${LIBOLD}.a
.endif

.if ${MKPROFILE} != "no"
SYMLINKS+=	lib${LIB}_p.a ${X11USRLIBDIR}/lib${LIBOLD}_p.a
.endif

.if ${MKPIC} != "no"
SYMLINKS+=	lib${LIB}.so ${X11USRLIBDIR}/lib${LIBOLD}.so
.endif

.PATH:	${X11SRCDIR.Xaw}/src
SRCS=	Actions.c AllWidgets.c AsciiSink.c AsciiSrc.c \
	AsciiText.c Box.c Command.c Converters.c Dialog.c \
	DisplayList.c Form.c Grip.c Label.c List.c MenuButton.c \
	MultiSrc.c MultiSink.c OS.c Paned.c Panner.c Pixmap.c \
	Porthole.c Repeater.c Scrollbar.c Simple.c \
	SimpleMenu.c Sme.c SmeBSB.c SmeLine.c StripChart.c \
	Text.c TextSink.c TextSrc.c TextAction.c TextPop.c \
	TextTr.c Tip.c Toggle.c Tree.c Vendor.c Viewport.c \
	XawIm.c XawInit.c XawI18n.c sharedlib.c

.PATH:	${X11SRCDIR.Xaw}/man
MAN=	Xaw.3

.PATH:	${X11SRCDIR.Xaw}/include/X11/Xaw
INCS=	AllWidgets.h AsciiSink.h AsciiSinkP.h AsciiSrc.h \
	AsciiSrcP.h AsciiText.h AsciiTextP.h Box.h BoxP.h \
	Cardinals.h Command.h CommandP.h Dialog.h DialogP.h \
	Form.h FormP.h Grip.h GripP.h Label.h LabelP.h List.h \
	ListP.h MenuButton.h MenuButtoP.h MultiSrc.h MultiSrcP.h \
	MultiSink.h MultiSinkP.h Paned.h PanedP.h Panner.h \
	PannerP.h Porthole.h PortholeP.h \
	Repeater.h RepeaterP.h Reports.h Scrollbar.h ScrollbarP.h \
	Simple.h SimpleP.h SimpleMenu.h SimpleMenP.h Sme.h SmeP.h \
	SmeBSB.h SmeBSBP.h SmeLine.h SmeLineP.h StripChart.h \
	StripCharP.h Template.c Template.h TemplateP.h Text.h \
	TextP.h TextSink.h TextSinkP.h TextSrc.h TextSrcP.h \
	Tip.h TipP.h Toggle.h ToggleP.h Tree.h TreeP.h VendorEP.h \
	Viewport.h ViewportP.h XawImP.h XawInit.h

INCSDIR=${X11INCDIR}/X11/Xaw


CPPFLAGS+=	-DHAVE_WCHAR_H -DHAVE_WCTYPE_H -DNO_WIDEC_H -DHAVE_ISWALNUM
CPPFLAGS+=	-DHAVE_GETPAGESIZE
CPPFLAGS+=	-DHAVE_UNISTD_H
CPPFLAGS+=	-DPROJECT_ROOT=\"${X11ROOTDIR}\" -DXAW7
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/Xaw
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/extensions

LIBDPLIBS=\
	Xmu	${.CURDIR}/../libXmu \
	Xt	${.CURDIR}/../libXt \
	SM	${.CURDIR}/../libSM \
	ICE	${.CURDIR}/../libICE \
	Xpm	${.CURDIR}/../libXpm \
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

PKGCONFIG=	xaw7
PKGDIST=	${LIBOLD}

.include <bsd.x11.mk>
.include <bsd.lib.mk>

# XXX
.if defined(HAVE_GCC)
COPTS.XawIm.c+=	-Wno-error
.endif

CWARNFLAGS.clang+=	-Wno-format
